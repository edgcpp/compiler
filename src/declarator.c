/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

declarator.c -- Scanning of declarators.

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
#include "exprutil.h"
#include "folding.h"
#include "statements.h"
#if MICROSOFT_EXTENSIONS_ALLOWED
#include "ms_attrib.h"
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

static void scan_declarator_attributes(a_decl_parse_state  *dps,
                                       a_type_ptr          *p_type)
/*
Scan attributes that appear after a declarator component other than the
declarator-id.  *p_type is the type formed by the particular declarator
component being parsed.  (Because of the way types are built up through
declarators, **p_type may not be fully formed yet.  For example, the type
pointed to by a pointer type may still be NULL.)
GCC allows attributes after array and function declarators only for non-nested
declarators.  Such attributes are placed on the state->id_attributes list
(i.e., they are treated as appertaining to the declared entity, rather than to
the type formed by the declarator component as would be the case with
standard-attribute syntax).
*/
{
  a_type_ptr                      type = *p_type;
  an_attribute_location           syn_loc;
  an_attribute_ptr                attributes;
  a_boolean                       error_issued = FALSE;

  /* Determine the syntactic location from the type. */
  switch (skip_typerefs_not_typedefs(type)->kind) {
    case tk_error:
      expect_error();
      /* No logical attribute location can be derived for an error type.
         Ignore any attributes that come up.  (A location is still assigned
         to avoid diagnostics from compilers and tools.) */
      skip_over_attributes();
      syn_loc = al_implicit;
      break;
    case tk_pointer:
    case tk_ptr_to_member:
      syn_loc = al_post_ptr_or_ref;
      break;
    case tk_array:
      syn_loc = al_post_array;
      break;
    case tk_routine:
      syn_loc = al_post_func;
      break;
    default:
      /* This can only validly happen with GNU attributes specified as the
         first construct in a nested declarator or after an outermost
         parenthesized declarator.  In the first case, they are treated like
         specifier attributes (e.g., "int (__attribute((mode(DI))) x);" is
         treated like "int __attribute((mode(DI))) x;").  In the second case
         they are treated as declarator-id attributes. */
      if (dps->in_nested_declarator) {
        syn_loc = al_specifier;
      } else {
        syn_loc = al_postfix;
      }  /* if */
  }  /* switch */
  /* Scan the attributes. */
  attributes = scan_attributes(syn_loc);
  /* Reclassify attributes if necessary. */
  if (attributes != NULL) {
    /* Move any non-type-transforming GNU attributes to the dps->id_declarator
       list, and change their syntactic location to al_postfix or
       al_id_equivalent.  Similarly, treat an al_post_func __declspec attribute
       on a lambda as al_id_equivalent. */
    an_attribute_ptr  ap, *p_from = &attributes, *p_to;
    p_to = last_attribute_link(&dps->id_attributes);
    do {
      ap = *p_from;
      if ((is_gcc_attribute(ap) ||
           ((gnu_mode || ms_extensions) && ap->family == af_alignas)) &&
          !is_type_transforming_attribute(ap) &&
          ap->kind != ak_enable_if) {
        /* GNU attributes following nested array or function declarators elicit
           an error in GCC. */
        if (dps->in_nested_declarator && !error_issued &&
            is_gcc_attribute(ap) && 
            (syn_loc == al_post_func || syn_loc == al_post_array)) {
          pos_error(ec_invalid_attribute_location, &ap->position);
          error_issued = TRUE;
        }  /* if */
        /* The GNU "aligned" attribute is treated as a type transforming
           attribute in some pointer/reference declarator contexts, but it
           doesn't actually modify the underlying type entry. */
        if (ap->kind == ak_align && is_gcc_attribute(ap) &&
            syn_loc == al_post_ptr_or_ref && !dps->in_class_scope) {
          make_attr_unrecognized(ap);
          ap->transforms_type_specifier = TRUE;
          p_from = &ap->next;
          continue;
        }  /* if */
        *p_from = ap->next;
        /* Non-nested postfix attributes are recorded as al_postfix.  Others
           are recorded as al_id_equivalent. */
        if (!dps->in_nested_declarator &&
            (syn_loc == al_post_func || syn_loc == al_post_array)) {
          ap->syntactic_location = al_postfix;
        } else {
          ap->syntactic_location = al_id_equivalent;
        }  /* if */
        *p_to = ap;
        p_to = &ap->next;
        ap->next = NULL;
      } else if (ap->family == af_ms_declspec && dps->is_lambda) {
        /* The only kind of __declspec attributes allowed on declarators are
           al_post_func attributes on lambdas. */
        check_assertion(ap->syntactic_location == al_post_func);
        ap->syntactic_location = al_id_equivalent;
        *p_from = ap->next;
        *p_to = ap;
        p_to = &ap->next;
      } else {
        if (dps->in_nested_declarator &&
            ap->syntactic_location == al_postfix) {
          /* A non-GNU attribute after a nested declarator is not valid. */
          if (!error_issued) {
            pos_error(ec_invalid_attribute_location, &ap->position);
            error_issued = TRUE;
          }  /* if */
          make_attr_unrecognized(ap);
        }  /* if */
        p_from = &ap->next;
      }  /* if */
    } while (*p_from != NULL);
  }  /* if */
  if (dps->pending_prefix_enable_if_attr && !dps->in_nested_declarator &&
      syn_loc == al_post_func) {
    /* One or more prefix "enable_if" or "unavailable" attributes should be
       handled at this point.  Move them from the "prefix_attributes" list, to
       the list pointed to by "attributes" (and about to be applied). */
    an_attribute_ptr  *p_ap = &dps->prefix_attributes, to_move;
    for (;;) {
      if ((*p_ap)->kind == ak_enable_if) {
        to_move = *p_ap;
        *p_ap = to_move->next;
        to_move->next = attributes;
        attributes = to_move;
      } else {
        p_ap = &(*p_ap)->next;
      }  /* if */
      if (*p_ap == NULL) break;
    }  /* for */
  }  /* if */
  if (attributes != NULL) {
    /* Microsoft __declspec attributes cannot appear in declarators (with an
       exception for lambda declarators): Disable any that were scanned (and
       issue an error in that case).  Similarly handle standard attributes
       that appear as the first construct in a nested declarator. */
    an_attribute_ptr  ap = attributes;
    for (; ap != NULL; ap = ap->next) {
      if ((ap->family == af_ms_declspec &&
           !(syn_loc == al_post_func && dps->is_lambda)) ||
          (is_std_attribute(ap) && syn_loc == al_specifier)) {
        if (!error_issued) {
          pos_error(ec_invalid_attribute_location, &ap->position);
          error_issued = TRUE;
        }  /* if */
        make_attr_unrecognized(ap);
      }  /* if */
    }  /* if */
    /* Apply the remaining attributes to the type. */
    attach_type_attributes(p_type, attributes, (void*)dps);
  }  /* if */
}  /* scan_declarator_attributes */

#if GNU_EXTENSIONS_ALLOWED

static an_attribute_ptr scan_predeclarator_attributes(void)
/*
Scan and return any attributes appearing as the first construct in a nested
declarator.  Issue an error if non-GNU attributes are scanned.
*/
{
  a_boolean         error_issued = FALSE;
  an_attribute_ptr  attributes, ap;

  attributes = scan_attributes(al_predeclarator);
  if (attributes != NULL) {
    if (gnu_mode) {
      /* Check that any scanned attributes are GNU attributes.  (Other
         attribute kinds are not permitted in this syntactic context.) */
      for (ap = attributes; ap != NULL; ap = ap->next) {
        if (!is_gcc_attribute(ap) && !error_issued) {
          pos_error(ec_only_gnu_attributes_here, &ap->position);
          error_issued = TRUE;
        }  /* if */
      }  /* for */
    } else {
      pos_error(ec_attribute_not_allowed, &attributes->position);
      attributes = NULL;
    }  /* if */
  }  /* if */
  return attributes;
}  /* scan_predeclarator_attributes */

#endif /* GNU_EXTENSIONS_ALLOWED */

static a_boolean check_pm_member_type(a_type_ptr  member_type)
/*
member_type is to be used in a pointer-to-member type.  Check its validity
and return TRUE if it's okay; otherwise issue a diagnostic and return FALSE.
*/
{
  an_error_code  err_code = ec_no_error;

  /* Note that member_type may not be fully assembled yet (i.e., it may
     contain NULL underlying types).  The diagnostic should therefore not
     attempt to quote the type in full. */
  if (is_void_type(member_type)) {
    err_code = ec_ptr_to_member_of_type_void;
  } else if (is_any_reference_type(member_type)) {
    err_code = ec_ptr_to_member_of_reference_type;
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (cli_or_cx_enabled && is_handle_type(member_type)) {
    err_code = ec_ptr_to_member_of_handle_type;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  if (err_code != ec_no_error) {
    pos_error(err_code, &error_position);
  }  /* if */
  return err_code == ec_no_error;
}  /* check_pm_member_type */


a_boolean is_cfront_member_function_typedef(a_type_ptr   type_ptr,
                                            a_type_ptr   *rout_type,
                                            a_type_ptr   *class_type,
                                            a_symbol_ptr *sym)
/*
We are checking for a type entry produced by a typedef declaration like
this:

        typedef void A::T(int);  // Nonstandard typedef

(meaning "T" names a routine type for a member function of A that takes an
int argument and returning void.  Its tie to class A is indicated by having
an implicit this-param type of const-ptr-to-A).  Cfront treats "T*" as though
it had been a ptr-to-member declaration -- e.g.,

        T* pm = &A::f(int);      // Nonstd ptr-to-member decl

and

        void (A::*pm)(int) = &A::f(int);

have the very same meaning for cfront.  Although this is not part of the
language defined in the ARM, it is supported for cfront compatibility.

Return TRUE if this is a member function typedef.  Also return a pointer to
the function type and the class type if this is the case -- and a pointer to
the type symbol for the typedef, for use in diagnostics.
*/
{
  a_type_ptr  tp;
  a_boolean   is_member_function_typedef = FALSE;

  *class_type = NULL;
  *rout_type = NULL;
  *sym = NULL;
  if (type_ptr->kind == (a_type_kind)tk_typeref &&
      is_function_type(type_ptr)) {
    *rout_type = skip_typerefs(type_ptr);
    tp = (*rout_type)->variant.routine.extra_info->this_class;
    if (tp != NULL) {
      is_member_function_typedef = TRUE;
      *class_type = tp;
      *sym = (a_symbol_ptr)type_ptr->source_corresp.assoc_info;
    }  /* if */
  }  /* if */
  return is_member_function_typedef;
}  /* is_cfront_member_function_typedef */


a_type_qualifier_set collect_type_qualifiers(
                               ARG_UNUSED a_decl_pos_block_ptr decl_pos_block,
                               ARG_UNUSED a_upc_block_size     *upc_block_size)
/*
Call decl_specifiers to scan one or more declarator qualifiers, and return
a bit vector describing what was found.  At least one qualifier must be
present (i.e., the caller must have already checked that the current
token is a qualifier).  If a UPC shared qualifier is seen, the associated
block size is returned through upc_block_size (when non-NULL).
*/
{
  a_decl_flag_set         dsi_flags;
  a_decl_parse_state      state;
  a_decl_pos_block        local_decl_pos_block;

  init_decl_parse_state(&state);
  clear_decl_pos_block(&local_decl_pos_block);
  dsi_flags = DSI_COLLECT_DECLARATOR_TYPE_QUALIFIERS;
  if (ms_extensions) { dsi_flags |= DSI_INLINE_ALLOWED; }
  decl_specifiers(dsi_flags, &state, &local_decl_pos_block);
#if UPC_EXTENSIONS_ALLOWED
  if (upc_block_size != NULL) {
    *upc_block_size = state.upc_block_size;
  }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    check_assertion(local_decl_pos_block.specifiers_range.end.seq != 0 ||
                    scanning_generated_code || in_code_from_module());
    decl_pos_block->declarator_range.end =
                       local_decl_pos_block.specifiers_range.end;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  return state.qualifiers;
}  /* collect_type_qualifiers */


a_boolean restrict_qualifier_is_allowed(a_type_ptr         type,
                                        a_source_position  *error_pos)
/*
Return TRUE if type may be qualified by the "restrict" qualifier.  It may be
applied to pointer and reference types (but not pointer-to-function-type),
pointer-to-member types (but not pointers to member functions), and (in
parameter declarations only) array types.  If a restrict qualifier is not
allowed, issue a diagnostic and return FALSE.
*/
{
  a_type_ptr         tp;
  an_error_severity  sev = es_error;
  an_error_code      error_code = ec_no_error;
  
  if (!is_error_type(type)) {
    if (is_ptr_or_ref_type(type)) {
      /* Pointer types and references may be restrict qualified unless they
         point to function types.  (Microsoft compilers do not support the
         "restrict" qualifiers; we therefore do not accept the qualifier on
         C++/CLI handles or tracking references.) */
      tp = type_pointed_to(type);
      if (tp != NULL && is_function_type(tp)) {
        error_code = ec_restrict_pointer_to_function;
      }  /* if */
    } else if (is_ptr_to_member_type(type)) {
      /* Pointer-to-member types may be restrict qualified unless they point
         to function types. */
      tp = pm_member_type(type);
      if (tp != NULL && is_function_type(tp)) {
        error_code = ec_restrict_pointer_to_function;
      }  /* if */
    } else if (is_template_param_type(type)) {
      /* A template parameter type can be instantiated for a pointer type
         later on.  So we must assume it is valid. */
    } else if (gpp_mode &&
               ((is_nonspecialized_instantiation_context() &&
                 !scope_stack[decl_scope_level].in_prototype_instantiation) ||
                is_possibly_qualified_typedef(type))) {
      sev = es_remark;
      error_code = ec_restrict_qualifier_ignored;
    } else {
      /* Anything else is disallowed. */
      error_code = ec_restrict_not_allowed;
    }  /* if */
    if (error_code != ec_no_error) {
      pos_diagnostic(sev, error_code, error_pos);
    }  /* if */
  }  /* if */
  return (error_code == ec_no_error);
}  /* restrict_qualifier_is_allowed */


static void check_for_restrict_qualifier_on_derived_type(
                                              a_type_ptr  new_type_ptr,
                                              a_type_ptr  *derived_type,
                                              a_type_ptr  *bottom_derived_type)
/*
*derived_type is the top of a derived type that is being constructed, and
*bottom_derived_type is the bottom, which is about to be updated to link to
new_type_ptr.  Check for the presence of improperly applied restrict
qualifier: if *bottom_derived_type is some kind of pointer or reference type
and is about to be updated to point to a function type, then the restrict
qualifier, if there is one, is invalid.  Issue a diagnostic and rewrite the
derived type to remove the restrict qualifier.
*/
{
  a_type_ptr            tp, prev_tp, new_tp;
  a_type_qualifier_set  qualifiers;

  if (is_function_type(new_type_ptr)) {
    check_assertion(is_any_ptr_or_ref_type(*bottom_derived_type) ||
                    is_ptr_to_member_type(*bottom_derived_type));
    /* We are about to form a derived type that is pointer-to-function-type,
       reference-to-function-type, or ptr-to-member-function.  Such pointer
       types, unlike other pointer types, may not be restrict qualified.
       Go through the derived type list looking for restrict qualifier that
       applies to the pointer type. */
    for (tp = *derived_type, prev_tp = NULL;
         !same_entities(tp, *bottom_derived_type);
         prev_tp = tp, tp = underlying_type_of_derived_type(tp)) {
      if (tp->kind == (a_type_kind)tk_typeref) {
        /* Check for qualifiers on a typeref. */
        qualifiers = get_top_level_type_qualifiers(tp);
        tp = skip_typerefs(tp);
        if (same_entities(tp, *bottom_derived_type)) {
          if (qualifiers & TQ_RESTRICT) {
            /* A restrict qualifier was found and it applies to the pointer
               type that is going to be set to point to the function type.
               Issue a diagnostic and remove the restrict qualifier. */
            pos_error(ec_restrict_pointer_to_function, &error_position);
            if (qualifiers == TQ_RESTRICT) {
              new_tp = *bottom_derived_type;
            } else {
              new_tp = make_qualified_type(*bottom_derived_type,
                                           (qualifiers & ~TQ_RESTRICT));
              *bottom_derived_type = skip_typerefs(new_tp);
            }  /* if */
            if (prev_tp == NULL) {
              *derived_type = new_tp;
            } else {
              switch (prev_tp->kind) {
                case tk_pointer:  /* Includes C++ reference too. */
                  prev_tp->variant.pointer.type = new_tp;
                  break;
                case tk_ptr_to_member:
                  prev_tp->variant.ptr_to_member.type = new_tp;
                  break;
                case tk_array:
                  prev_tp->variant.array.element_type = new_tp;
                  break;
                case tk_routine:
                  prev_tp->variant.routine.return_type = new_tp;
                  break;
                default:
                  unexpected_condition_str(
                                       "check_for_restrict...: bad type kind");
              }  /* switch */
            }  /* if */
            *bottom_derived_type = skip_typerefs(new_tp);
          }  /* if */
          break;
        }  /* if */
      }  /* if */
      /* Continue to the next type in the derived type sequence. */
    }  /* for */
  }  /* if */  
}  /* check_for_restrict_qualifier_on_derived_type */


a_boolean check_nullability_qualifiers(a_type_qualifier_set  nullability,
                                       a_type_ptr            type,
                                       a_source_position     *diag_pos)
/*
Check that the given nullability qualifiers can validly be applied to the
given type.  If not, issue an error at the given position.
*/
{
  a_boolean   result = TRUE;
  a_type_ptr  utp = skip_typerefs(type);

  if (is_pointer_type(utp) ||
      utp->kind == (a_type_kind)tk_ptr_to_member ||
      utp->kind == (a_type_kind)tk_template_param ||
      utp->kind == (a_type_kind)tk_error) {
    a_type_qualifier_set  tqs = get_top_level_type_qualifiers(type);
    if ((tqs & TQ_NULLABILITY) != TQ_NONE &&
        (tqs & TQ_NULLABILITY) != (nullability & TQ_NULLABILITY)) {
      pos_error(ec_conflicting_nullability, diag_pos);
      result = FALSE;
    }  /* if */
  } else {
    pos_warning(ec_invalid_type_for_nullability, diag_pos);
    result = FALSE;
  }  /* if */
  return result;
}  /* check_nullability_qualifiers */

#if GENERATE_SOURCE_SEQUENCE_LISTS

a_type_ptr form_declared_type(a_type_ptr             type_ptr,
                              a_func_info_block_ptr  func_info)
/*
If type_ptr is a function type, return a copy of type_ptr that incorporates
the parameter types as actually declared in the source program; that is,
preserve the parameter type as it was before any adjustment was done (e.g.,
array-to-pointer decay).
*/
{
  a_type_ptr        declared_type;

  db_enter(4, "form_declared_type");
  if (type_ptr->kind == (a_type_kind)tk_typeref) {
    /* Leave the declared type the same as the routine type. */
    declared_type = type_ptr;
  } else if (func_info->declared_type != NULL) {
    declared_type = func_info->declared_type;
  } else {
    /* Make a copy of the type.  Note that default arg expressions, if any,
       will be copied later. */
    a_routine_type_supplement_ptr  copied_rtsp;
    a_param_type_ptr               ptp;
    declared_type =
               copy_routine_type_with_param_types(type_ptr,
                                                  /*copy_default_args=*/FALSE);
    copied_rtsp = skip_typerefs(declared_type)->variant.routine.extra_info;
    if (!exceptions_enabled && copied_rtsp->exception_specification != NULL) {
      /* When exceptions are disabled, no exception specification should be
         recorded.  However, with noexcept an entry may have been created to
         enable later instantiation.  Discard that entry in the declared
         type. */
      copied_rtsp->exception_specification = NULL;
    }  /* if */
    for (ptp = copied_rtsp->param_type_list; ptp != NULL; ptp = ptp->next) {
      if (ptp->declared_type != NULL) {
        ptp->type = ptp->declared_type;
      }  /* if */
    }  /* for */
    if (copied_rtsp->prototype_scope != NULL) {
      /* The copy shares the prototype scope of type_ptr.  The scope's
         associated type is this copy, which is the type recorded in
         source sequence entries. */
      copied_rtsp->prototype_scope->variant.assoc_type = declared_type;
    }  /* if */
  }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 3) {
    fputs("declared type: ", f_debug);
    db_type(declared_type);
    fputc('\n', f_debug);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  db_exit();
  return declared_type;
}  /* form_declared_type */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

static a_boolean is_partial_type(a_type_ptr  type)
/*
Return TRUE if and only if the given type is not (yet) fully assembled because
it only incorporates some components of the declarator and not those of the
decl-specifier (e.g., "array [1] of NULL").
*/
{
  a_boolean  result = FALSE;

  if (type->size == 0) {
    /* If the size is zero, the type may not yet be fully constructed. */
    a_type_ptr  underlying_type = type;
    a_boolean   is_derived_type;
    do {
      underlying_type = f_underlying_type_of_derived_type(underlying_type,
                                                          &is_derived_type);
    } while (underlying_type != NULL);
    result = (is_derived_type && underlying_type == NULL);
  }  /* if */
  return result;
}  /* is_partial_type */

#if MICROSOFT_EXTENSIONS_ALLOWED

a_boolean f_check_cli_or_cx_type_pointed_to(a_type_ptr         tp,
                                            a_boolean          is_ref,
                                            a_boolean          is_handle,
                                            a_source_position  *pos)
/*
A construct attempting to form a tk_pointer type with tp as the underlying
type has been encountered.  If is_ref is TRUE, the resulting type would be a
reference or tracking reference type.  If is_handle is TRUE, the resulting
type would be a handle or tracking reference type.  If such a type would be
invalid for a C++/CLI- or C++/CX-specific reason (e.g., a handle or a managed
class type is involved), issue a diagnostic at the given position (when it is
non-NULL) and return FALSE.  Otherwise, return TRUE.
*/
{
  an_error_code  err_code = ec_no_error;

  tp = skip_typerefs(tp);
  if (is_handle) {
    if (is_void_type(tp)) {
      /* A handle or tracking reference to void is invalid. */
      err_code = is_ref ? ec_reference_to_void
                        : ec_handle_to_void;
    } else if (is_function_type(tp)) {
      /* A handle or tracking reference to a function is invalid. */
      err_code = is_ref ? ec_tracking_reference_to_function
                        : ec_handle_to_function;
    } else if (cppcx_enabled &&
               ((is_immediate_class_type(tp) &&
                 cli_class_type_kind_is(tp, cctk_value)) ||
                system_type_from_fundamental_type(tp) != NULL)) {
      /* In C++/CX mode, a handle or tracking reference to a value class is
         invalid. */
      err_code = is_ref ? ec_tracking_reference_to_value_class
                        : ec_handle_to_value_class;
    } else if (cppcx_enabled && is_immediate_enum_type(tp)) {
      /* In C++/CX mode, a handle or tracking reference to an enum is
         invalid. */
      err_code = is_ref ? ec_tracking_reference_to_enum
                        : ec_handle_to_enum;
    } else if (cppcx_enabled &&
               is_class_struct_union_type(tp) &&
               !is_managed_class_type(tp) && is_ref) {
      err_code = ec_cppcx_tracking_reference_on_standard_class_type;
    } else if (is_interior_ptr_type(tp) || is_pin_ptr_type(tp)) {
      /* Interior pointers and pin pointers are handled below. */
    } else if (is_ref) {
      /* Checks applicable to tracking references but not handles. */
      if (is_immediate_managed_class_type(tp)) {
        if (is_immediate_delegate_type(tp)) {
          /* A tracking reference to a delegate is invalid. */
          err_code = ec_tracking_reference_to_delegate;
        } else if (is_cli_system_string_type(tp)) {
          /* A tracking reference to a System::String is invalid. */
          err_code = ec_tracking_reference_to_system_string;
        }  /* if */
      }  /* if */
    } else {
      /* Checks applicable to handles but not tracking references. */
      if (is_array_type(tp)) {
        /* A handle to an array is invalid.  (Strangely, Microsoft compilers
           allow tracking references to arrays.) */
        err_code = ec_handle_to_array;
      } else if (is_any_ptr_or_ref_type(tp)) {
        /* Handles to handles, pointers or references are not allowed.  If tp
           is a handle, it may actually represent a generic parameter: Use a
           more direct diagnostic for that case. */
        if (is_cli_generic_definition_argument_type(tp)) {
          err_code = ec_ptr_handle_or_ref_to_generic_param;
        } else {
          err_code = ec_handle_to_address_type;
        }  /* if */
      } else if (is_immediate_class_type(tp) &&
                 cli_class_type_kind_is(tp, cctk_standard) &&
                 !tp->variant.class_struct_union.is_nonreal_class) {
        /* A handle to a non-managed class type is invalid.  (However, the
           Microsoft compiler does not impose this constraint on nonreal class
           types.) */
        err_code = ec_handle_to_standard_class_type;
      } else if (is_immediate_enum_type(tp) &&
                 !integer_type_is_scoped_enum(tp)) {
        /* A handle to an unscoped enum type is invalid. */
        err_code = ec_handle_to_unscoped_enum_type;
      }  /* if */
    }  /* if */
  } else {
    /* Ordinary pointers and references to ref class and interface class types
       are not allowed. */
    if (is_cli_ref_or_interface_class_type(tp)) {
      err_code = is_ref ? ec_reference_to_ref_or_interface_class
                        : ec_pointer_to_ref_or_interface_class;
    }  /* if */
  }  /* if */
  if (err_code == ec_no_error) {
    /* A pointer, handle, or reference type to an interior/pin pointer or to a
       generic parameter may not be formed.  Similarly, an ordinary pointer or
       a reference to a C++/CLI array is invalid  (a handle is okay). */
    if (is_interior_ptr_type(tp)) {
      err_code = ec_ptr_handle_or_ref_to_interior_ptr;
    } else if (is_pin_ptr_type(tp)) {
      err_code = ec_ptr_handle_or_ref_to_pin_ptr;
    } else if (is_cli_array_type(tp) && (is_ref || !is_handle)) {
      err_code = ec_ptr_or_ref_to_cli_array;
    } else if (!(is_ref && is_handle)) {
      /* Check for a pointer, handle, or ordinary reference to a generic
         parameter (a tracking reference is okay). */
      if (is_cli_generic_definition_argument_type(tp)) {
        if (cppcx_enabled) {
          /* In C++/CX mode, ordinary pointers to generic parameters are
             allowed. */
          err_code = (!is_ref && !is_handle) ?
                        ec_no_error : ec_cppcx_handle_or_ref_to_generic_param;
        } else {
          err_code = ec_ptr_handle_or_ref_to_generic_param;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (err_code != ec_no_error && pos != NULL) {
    pos_error(err_code, pos);
  }  /* if */
  return err_code == ec_no_error;
}  /* f_check_cli_or_cx_type_pointed_to */


static
a_boolean check_invalid_use_of_special_cli_class_type(a_type_ptr         tp,
                                                      a_source_position  *pos)
/*
Some special C++/CLI class types (notably, delegate types and C++/CLI array
types) can only be used in a few ways:
  (a) to form a handle
  (b) as the underlying type of a typedef or template argument
  (c) as an argument to gcnew
(This doesn't apply to C++/CX arrays.)
This routine is called in other contexts where the appearance of such a type
should be diagnosed.
If the given type is one of the special types above, return FALSE and if pos
is non-NULL, issue an error at that position.  Otherwise, return TRUE.
*/
{
  an_error_code  err_code = ec_no_error;

  tp = skip_typerefs(tp);
  if (is_immediate_class_type(tp)) {
    if (is_immediate_delegate_type(tp)) {
      err_code = ec_bad_use_of_delegate_type;
    } else if (!cppcx_enabled && is_cli_array_type(tp)) {
      err_code = ec_bad_use_of_cli_array_type;
    }  /* if */
  }  /* if */
  if (err_code != ec_no_error && pos != NULL) {
    pos_error(err_code, pos);
  }  /* if */
  return err_code == ec_no_error;
}  /* check_invalid_use_of_special_cli_class_type */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

void report_bad_return_type_qualifier(a_type_ptr           type,
                                      a_decl_parse_state   *dps,
                                      a_source_position    *diag_pos,
                                      ARG_UNUSED a_boolean *err)
/*
The given type is a qualified type used as a function return type.  Issue an
error if the qualification is invalid or a warning or remark if it is not
meaningful (e.g., the "const" in "int const f()" has no effect).  If an error
is issued, *err is set to TRUE.  For explicit return type declarations, *dps
carries information about the way the type was formed (e.g., whether qualifiers
appeared explicitly; meaningless qualification acquired through a typedef are
not diagnosed); for the implicit function return type resulting from certain
lambda constructs, dps is NULL.  Diagnostics are issued at the position given
by *diag_pos or at a position recorded in *dps (depending on the diagnostic).
*/
{
  if (!C_mode() &&
      (is_class_struct_union_type(type) || is_template_param_type(type))) {
    /* In C++ mode class rvalues can have type qualifiers, so allow a function
       returning a qualified class type or a qualified template param type
       (the latter because a function template could end up being instantiated
       with a class type). */
  } else if (get_type_qualifiers(type) == TQ_RESTRICT) {
    /* Exactly one type qualifier -- "restrict".  No warning. */
#if NAMED_ADDRESS_SPACES_ALLOWED
  } else if (type_qualified_with_named_address_space(type)) {
    /* Functions cannot return a value in a named address space. */
    pos_error(ec_function_returning_named_address_space, diag_pos);
    *err = TRUE;
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if UPC_EXTENSIONS_ALLOWED
  } else if (is_shared_qualified_type(type)) {
    /* Functions cannot return a shared type. */
    pos_error(ec_function_returning_shared, diag_pos);
    *err = TRUE;
#endif /* UPC_EXTENSIONS_ALLOWED */
  } else if (is_any_reference_type(type)) {
    /* A diagnostic will already have been issued. */
    expect_error();
  } else if (dps != NULL && dps->qualifiers != TQ_NONE) {
    an_error_severity  severity = es_none;
    /* Type qualifiers were explicitly specified on the return type, but they
       have no effect.  Issue a diagnostic in most cases.  Note, however, that
       the qualifiers are left as part of the type. */
    if (C_mode() && is_void_type(skip_typerefs(type)) &&
        get_type_qualifiers(type) == TQ_VOLATILE) {
      /* Issue just a remark for "volatile void" -- gcc uses that to
         indicate a function (like exit()) that does not return. */
      severity = es_remark;
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (cli_or_cx_enabled && dps->in_class_scope &&
               in_cli_property_or_event_definition() && is_void_type(type)) {
      /* Presumably a property accessor: Any diagnostics will be issued by the
         code that checks the accessor type. */
      severity = es_none;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else if (is_nonspecialized_instantiation_context() &&
               !scope_stack[decl_scope_level].in_prototype_instantiation) {
      /* Inside a template instantiation it is sometimes the case
         that the type qualifier is "useless" for some instantiations
         but not in general -- e.g.,
           template <class T> struct A {
             const T f();
           };
           struct X { };
           A<int> aint;     // A<int>::f returns const int (useless)
           A<X> ax;         // A<X>::f returns const X (okay)
         Do not issue a remark in this case to eliminate annoying
         warnings the user can't do anything about. */
      /* Note that this solution fails to warn on cases that are
         *always* useless, too.  If A<T>::f returned "T * const" a
         warning would always be appropriate, whatever T was replaced
         by in the instantiation.  But the representation of types
         based on template arguments will have to be improved to
         make this distinction.  When performing prototype
         instantiations, however, most such cases are in fact
         diagnosed. */
    } else {
      severity = es_warning;
    }  /* if */
    if (severity != es_none) {
      pos_diagnostic(severity, ec_useless_type_qualifier_on_return_type,
                     &dps->qualifiers_pos);
    }  /* if */
  }  /* if */
  if (!C_mode() && is_volatile_qualified_type(type) &&
      !(is_nonspecialized_instantiation_context() &&
        !scope_stack[decl_scope_level].in_prototype_instantiation)) {
    an_error_severity sev = cpp20_mode ? es_warning : es_remark;
    pos_diagnostic(sev, ec_volatile_return_type_deprecated, diag_pos);
  }  /* if */
}  /* report_bad_return_type_qualifier */


a_boolean check_return_type(a_type_ptr          type,
                            a_decl_parse_state  *dps,
                            a_source_position   *diag_pos)
/*
type is used as a function return type (in a declarative context described by
*dps; dps is NULL when the return type is determined implicitly in some lambda
constructs).  Issue diagnostics as appropriate, and return TRUE if no error is
issued.  Diagnostics are issued at the position given by *diag_pos or at a
position recorded in *dps (depending on the diagnostic).
*/
{
  a_boolean  err = FALSE;

  if (is_function_type(type)) {
    pos_error(ec_function_returning_function, diag_pos);
    err = TRUE;
  } else if (is_array_type(type)) {
    pos_error(ec_function_returning_array, diag_pos);
    err = TRUE;
#if VLA_ALLOWED
  } else if (!C_mode() && vla_enabled &&
             is_variably_modified_type(type)) {
    /* We do not accept variably-modified return types in C++. */
    pos_error(ec_vla_in_return_type, diag_pos);
    err = TRUE;
#endif /* VLA_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else if (cli_or_cx_enabled) {
    /* If a return type was explicitly specified, use its position for
       diagnostic purposes. */
    if (dps != NULL) diag_pos = &dps->return_type_pos;
    if (is_pin_ptr_type(type)) {
      /* A pin pointer cannot be used as a return type. */
      pos_error(ec_pin_ptr_return_type_not_allowed, diag_pos);
      err = TRUE;
    } else if (is_cli_interface_type(type)) {
      pos_error(ec_return_type_is_interface, diag_pos);
    } else {
      err = !check_invalid_use_of_special_cli_class_type(type, diag_pos);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  if (is_qualified_type(type)) {
    /* A qualified return type. */
    report_bad_return_type_qualifier(type, dps, diag_pos, &err);
  }  /* if */
  return !err;
}  /* check_return_type */


void add_to_derived_type_list(a_type_ptr          new_type_ptr,
                              a_type_ptr          *derived_type,
                              a_type_ptr          *bottom_derived_type,
                              a_decl_parse_state  *dps,
                              a_boolean           parameter_type)
/*
Add the type entry pointed to by new_type_ptr to the list of derived-type
entries pointed to by *derived_type (and whose end is pointed to by
*bottom_derived_type).  Aside from the purely mechanical issues of linking the
entries, this routine also checks to see if the resulting type is legal.  If
the type is for a parameter declaration, parameter_type is TRUE (in Sun and
GNU C++ modes this relaxes the array of abstract class check).  *dps describes
the specifiers and declarator that formed the new type.
*/
{
  a_type_ptr              temp_type, prev_temp_type, tp;
  a_boolean               err = FALSE;
  a_type_kind             tkind;
  a_boolean               array_of_incomp_class_or_enum = FALSE;
  a_boolean               is_member_function_typedef = FALSE;
  a_type_ptr              mft_class_type = NULL, mft_rout_type = NULL;
  a_symbol_ptr            mft_sym = NULL;

  db_enter(3, "add_to_derived_type_list");
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 4 || db_flag_is_set("declarators")) {
    fprintf(f_debug, "At start of add_to_derived_type_list:\n");
    fprintf(f_debug, "  new_type_ptr = ");
    db_type(new_type_ptr);
    fprintf(f_debug, "\n");
    fprintf(f_debug, "  *derived_type = ");
    db_type(*derived_type);
    fprintf(f_debug, "\n");
    fprintf(f_debug, "  *bottom_derived_type = ");
    db_type(*bottom_derived_type);
    fprintf(f_debug, "\n");
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  /* Note that while derived types are being built up, the derived-type
     entries are connected to one another from the top down, which
     means that the bottom-most derived-type entry temporarily points
     to nothing (a "partial type").  Each derived type is checked as the
     type below it is attached.  This must be done carefully, because the
     type being attached may look incomplete (its size may be zero). */
  if (*bottom_derived_type == NULL) {
    /* This is the first entry on the list.  No checking can be done yet. */
    *derived_type = new_type_ptr;
    /* We'll find the real bottom of the type below. */
    *bottom_derived_type = new_type_ptr;
  } else {
    /* The derived-type list is non-empty, so we need to check to see if
       the bottom derived type can legally be connected to the new type
       (e.g., if it's an array, can it have elements of the indicated
       type, and if it's a function, can it have a result of the indicated
       type). */
    tkind = (*bottom_derived_type)->kind;
    if (tkind == (a_type_kind)tk_error) {
      /* The bottom derived type is an error, and nothing can be attached
         to it.  Therefore, the new type is thrown away. */
    } else {
      if (any_cfront_mode()) {
        /* Check for a "member function typedef" type -- it can only be used
           to form pointer-to-member types. */
        if (is_cfront_member_function_typedef(new_type_ptr, &mft_rout_type,
                                              &mft_class_type, &mft_sym)) {
          /* The validity of this use in the current context is checked
             later. */
          is_member_function_typedef = TRUE;
        }  /* if */
      }  /* if */
      if (tkind == (a_type_kind)tk_array) {
        /* Array.  See if the element type is proper.  3.1.2.5: the 
           elements must have an object type.  If the element type is
           a partial array or pointer type (see comment above), let it
           by as long as it looks okay otherwise. */
        temp_type = skip_typerefs(new_type_ptr);
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (cppcli_enabled && is_handle_type(temp_type)) {
          /* A native array of handles is invalid in C++/CLI (but not in
             C++/CX). */
          if (is_cli_generic_definition_argument_type(temp_type)) {
            /* The handle type is really a generic parameter. */
            pos_error(ec_array_of_generic_param, &error_position);
          } else {
            pos_error(ec_array_of_handle, &error_position);
          }  /* if */
          err = TRUE;
        } else if (cppcli_enabled && 
                   is_immediate_managed_class_type(temp_type)) {
          /* A native array of managed classes is invalid. */
          pos_error(ec_array_of_managed_class, &error_position);
          err = TRUE;
        } else if (cli_or_cx_enabled && is_cli_generic_param_type(temp_type)) {
          /* A native array of a generic parameter is invalid. */
          pos_error(ec_array_of_generic_param, &error_position);
          err = TRUE;
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        if (is_complete_object_type(temp_type) &&
            !is_partial_type(temp_type)) {
          /* Usually okay. */
          if (flexible_array_members_allowed) {
            /* A struct or union containing a flexible array member is usually
               not allowed to be an array element type.  An exception is made
               GNU modes. */
            if (is_class_struct_union_type(temp_type) &&
                temp_type->variant.class_struct_union.
                                contains_flexible_array_member) {
              if (gnu_mode) {
                pos_warning(ec_nonstandard_array_with_flexible_array_element,
                            &error_position);
              } else {
                pos_error(ec_flexible_array_member_not_allowed,
                          &error_position);
                err = TRUE;
              }  /* if */
            }  /* if */
          }  /* if */
          if (!(parameter_type && (sun_mode || gpp_mode)) &&
              is_abstract_class_type(temp_type)) {
            /* An array type cannot have its element type be an abstract class
               type.  An exception in some modes are parameter types (since
               they are always transformed into pointer types). */
            abstract_class_diagnostic(es_error, ec_array_of_abstract_class,
                                      temp_type, &error_position);
          } else if (is_sizeless_type(temp_type)) {
            pos_ty_error(ec_sizeless_type_not_allowed, &error_position,
                         temp_type);
            err = TRUE;
          }  /* if */
        } else if (is_pointer_type(temp_type)) {
          /* Partial pointer type: Okay. */
        } else if (temp_type->kind == (a_type_kind)tk_array &&
                   (has_unknown_specified_bound(temp_type) ||
                    temp_type->
                           variant.array.variant.number_of_elements != 0 ||
                    gcc_mode ||
                    temp_type->variant.array.bound_is_zero)) {
          /* Okay.  Note that in GNU C mode, parameters can have type X[][]. */
          tp = underlying_array_element_type(temp_type);
          if (tp != NULL) {
            tp = skip_typerefs(tp);
            if (is_incomplete_type(tp) &&
                (is_immediate_class_type(tp) || is_immediate_enum_type(tp))) {
              /* This is an array of array ... of incomplete class or enum
                 type.  A diagnostic may be issued (see below), but this is
                 done only when the class is the immediate element type. */
              array_of_incomp_class_or_enum = TRUE;
            }  /* if */
          }  /* if */
        } else if (is_ptr_to_member_type(temp_type) &&
                   pm_member_type(temp_type) == NULL) {
          /* This is an incomplete ptr-to-member type, presumably a
             pointer to member function.  Okay. */
        } else if (is_template_param_type(temp_type)) {
          /* This is a declaration in the midst of a template declaration.
             Okay. */
        } else if (is_immediate_class_type(temp_type)) {
          /* In C++ mode and as an extension in C mode, allow an array of
             incomplete class type.  Obviously, the element type has to be
             completed before the array is actually used. (The array type
             will be added to a list of types to be fixed up when the
             class/struct/union declaration is completed.) */
          a_boolean  complete_type_required = (C_mode() && strict_ansi_mode);
          if (complete_type_required) {
            complete_class_type_is_needed(temp_type);
          }  /* if */
          if (is_incomplete_type(temp_type)) {
            array_of_incomp_class_or_enum = TRUE;
            temp_type->
                variant.class_struct_union.inc_class_used_in_array_type = TRUE;
            if (complete_type_required) {
              diagnostic(strict_ansi_error_severity,
                         ec_array_of_incomplete_type);
              if (strict_ansi_error_severity == es_error) err = TRUE;
            } else if (!(parameter_type && (sun_mode || gpp_mode))) {
              /* The type may still end up being abstract.  Add it to a fixup
                 list to verify the constraint when the class is complete. */
              add_to_dependent_type_fixup_list(
                                     temp_type,
                                     (a_dependent_type_fixup_kind)
                                            dtfk_array_of_abstract_class_check,
                                     (char*)(*bottom_derived_type),
                                     iek_type,
                                     &error_position);
            }  /* if */
          }  /* if */
        } else if (is_immediate_enum_type(temp_type)) {
          if (is_incomplete_type(temp_type)) {
            /* In C++ mode and as an extension in C mode, allow an array of
               incomplete enum type.  Obviously, the element type has to be
               completed before the array is actually used. (The enum type
               will be added to a list of types to be fixed up when the
               class/struct/union declaration is completed.) */
            array_of_incomp_class_or_enum = TRUE;
            if (C_mode() && strict_ansi_mode) {
              diagnostic(strict_ansi_error_severity,
                         ec_array_of_incomplete_type);
              if (strict_ansi_error_severity == es_error) err = TRUE;
            }  /* if */
          }  /* if */
        } else {
          /* Element type is not okay.  Select a specific error message. */
          if (is_member_function_typedef) {
            /* A cfront member function typedef type can only be used in
               forming a pointer-to-member type. */
            sym_error(ec_bad_use_of_member_function_typedef, mft_sym);
            err = TRUE;
          } else if (is_function_type(temp_type)) {
            pos_error(ec_array_of_function, &error_position);
            err = TRUE;
          } else if (is_void_type(temp_type)) {
            pos_error(ec_array_of_void, &error_position);
            err = TRUE;
          } else if (is_any_reference_type(temp_type)) {
            pos_error(ec_array_of_reference, &error_position);
            err = TRUE;
          } else if (temp_type->kind == (a_type_kind)tk_error) {
            /* Error already put out. */
            expect_error();
            err = TRUE;
          } else if (!dps->is_declspec_property_field) {
            pos_error(ec_bad_array_element_type, &error_position);
            err = TRUE;
          }  /* if */
        }  /* if */
#if UPC_EXTENSIONS_ALLOWED
        /* If the numbers of threads is static (i.e., specified on the
           command line), we should not see a THREADS-dependent dimension
           here. */
        check_assertion_str(
           (err || upc_dynamic_threads() ||
            !is_shared_qualified_type(new_type_ptr) ||
            !(*bottom_derived_type)->variant.array.is_threads_dimension),
           "add_to_derived_type_list: unexpected upc_threads dimension");
#endif /* UPC_EXTENSIONS_ALLOWED */
        if (err) new_type_ptr = error_type();
        (*bottom_derived_type)->variant.array.element_type = new_type_ptr;
      } else if (is_pointer_or_handle_type(*bottom_derived_type)) {
        /* Pointer or handle (C++/CLI) type. */
        a_boolean  is_handle = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
        is_handle = cli_or_cx_enabled && is_handle_type(*bottom_derived_type);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        if (is_member_function_typedef && !is_handle) {
          /* The code contains "T*" where "T" names a member function typedef.
             It points to a routine type in which the implicit this-param
             type pointer identifies the parent class, say "S".  Then "T*" is
             equivalent to a pointer-to-member declaration, say int S::*(),
             where the return type and argument types are read from the
             routine type pointed to by "T".  What we need is not to add
             something to *bottom_derived_type (as in other cases) but rather
             to change it from a "pointer-to-?" type to a "ptr-to-member"
             type pointing the class and routine type. */
          tp = ptr_to_member_type(mft_rout_type, mft_class_type,
                                  mft_class_type);
          copy_type(tp, *bottom_derived_type);
          /* Change new_type_ptr and tkind to make it seem as if this were
             an ordinary ptr-to-member declaration. */
          new_type_ptr = mft_rout_type;
          tkind = (a_type_kind)tk_ptr_to_member;
        } else {
          if (/*lint -e(506)*/!check_cli_or_cx_type_pointed_to(new_type_ptr,
                                                            /*is_ref=*/FALSE,
                                                            is_handle,
                                                            &error_position)) {
            new_type_ptr = error_type();
          } else if (is_any_reference_type(new_type_ptr)) {
            /* A pointer-to-reference type is invalid (a handle-to-reference
               would be diagnosed by check_cli_or_cx_type_pointed_to). */
            pos_error(ec_pointer_to_reference, &error_position);
            new_type_ptr = error_type();
          }  /* if */
          check_for_restrict_qualifier_on_derived_type(new_type_ptr,
                                                       derived_type,
                                                       bottom_derived_type);
          (*bottom_derived_type)->variant.pointer.type = new_type_ptr;
        }  /* if */
      } else if (is_any_reference_type(*bottom_derived_type)) {
        /* Reference type. */
        temp_type = skip_typerefs(new_type_ptr);
        if (is_void_type(temp_type)) {
	  /* Reference to void is illegal. */
          pos_error(ec_reference_to_void, &error_position);
	  err = TRUE;
        } else if (is_member_function_typedef) {
          /* A cfront member function typedef type can only be used in
             forming a pointer-to-member type. */
          sym_error(ec_bad_use_of_member_function_typedef, mft_sym);
          err = TRUE;
        } else if (/*lint -e(506)*/!check_cli_or_cx_type_pointed_to(
                             temp_type, /*is_ref=*/TRUE,
                             is_tracking_reference_type(*bottom_derived_type),
                             &error_position)) {
          err = TRUE;
        }  /* if */
        if (err) new_type_ptr = error_type();
        check_for_restrict_qualifier_on_derived_type(new_type_ptr,
                                                     derived_type,
                                                     bottom_derived_type);
        (*bottom_derived_type)->variant.pointer.type = new_type_ptr;
      } else if (is_ptr_to_member_type(*bottom_derived_type)) {
        /* Pointer-to-member type. */
        if (is_member_function_typedef) {
          /* A cfront member function typedef type can only be used in
             forming a pointer-to-member type. */
          sym_error(ec_bad_use_of_member_function_typedef, mft_sym);
          err = TRUE;
        } else if (!check_pm_member_type(new_type_ptr)) {
          err = TRUE;
        }  /* if */
        if (err) new_type_ptr = error_type();
        check_for_restrict_qualifier_on_derived_type(new_type_ptr,
                                                     derived_type,
                                                     bottom_derived_type);
        update_ptr_to_member_type(*bottom_derived_type, new_type_ptr);
      } else {
        /* Function type. */
        check_assertion(tkind == (a_type_kind)tk_routine);
        /* 3.5.4.3, constraints: A function declarator shall not specify
           a return type that is a function type or an array type.
           Footnote to 3.5.2.3 also says it is legal to have an incomplete
           struct or union type, as long as it is complete before the 
           function is called or defined.  These are the constraints on
           a declarator; there are additional constraints (3.7.1) on function
           definitions -- see function_definition. */
        if (is_member_function_typedef) {
          /* A cfront member function typedef type can only be used in
             forming a pointer-to-member type. */
          sym_error(ec_bad_use_of_member_function_typedef, mft_sym);
          err = TRUE;
        } else if (!check_return_type(new_type_ptr, dps,
                                      dps->has_trailing_return_type ?
                                        &dps->return_type_pos :
                                        &error_position)) {
          err = TRUE;
        }  /* if */
        if (err) {
          new_type_ptr = error_type();
        } else {
          if (C_dialect == C_dialect_pcc) {
            /* In pcc mode, promote float functions to double functions.
               Any type qualifiers or typedef information on the new type
               are discarded. */
            promote_float_to_double(new_type_ptr);
          }  /* if */
          if (gcc_mode) {
            /* GCC essentially ignores the "volatile" qualifier on C-mode
               return types.  We also drop it, to avoid redeclaration errors
               in cases like the following:
                  int volatile f();
                  int f();  // Not an error in GNU C mode.
               Note that a warning is likely to have been issued by the call to
               check_return_type. */
            a_type_qualifier_set  tqs = get_type_qualifiers(new_type_ptr);
            if ((tqs & TQ_VOLATILE) != TQ_NONE &&
                !is_void_type(new_type_ptr)) {
              tqs &= ~(a_type_qualifier_set)tqs;
              new_type_ptr = make_qualified_type(
                               skip_typerefs_not_typedefs(new_type_ptr), tqs);
            }  /* if */
          }  /* if */
        }  /* if */
        check_assertion((*bottom_derived_type)->kind ==
                                                    (a_type_kind)tk_routine);
        (*bottom_derived_type)->variant.routine.return_type = new_type_ptr;
        /* Check whether the routine needs special support for returning a
           class object by value. */
        set_routine_calling_method_flag(*bottom_derived_type, &error_position);
        set_clrcall_convention_if_needed(*bottom_derived_type);
      }  /* if */
      temp_type = *bottom_derived_type;
      *bottom_derived_type = new_type_ptr;
      /* If the former bottom entry (temp_type) has no size, and the
         new bottom entry (new_type_ptr) has a size, loop from the bottom
         up computing sizes of types on the list.  This is necessary to
         finish off array and pointer type entries which had dependent
         types with unknown sizes up until now.  This happens because
         the string of array/pointer/function types has to be built up
         in a strange order, and sometimes a list of derived types has
         no type at the bottom (just a NULL pointer waiting to be filled
         in). */
      /* Note that the size of a pointer pointing to an incomplete type
         can be determined, so do that even if the new type is incomplete. */
      if (tkind != (a_type_kind)tk_routine /* For speed. */ &&
          !dps->is_declspec_property_field &&
          (tkind == (a_type_kind)tk_pointer ||
           tkind == (a_type_kind)tk_ptr_to_member ||
           array_of_incomp_class_or_enum ||
           is_function_type(new_type_ptr) ||
           !is_partial_type(new_type_ptr) ||
           is_error_type(new_type_ptr))) {
        while (tkind == (a_type_kind)tk_array ||
               tkind == (a_type_kind)tk_pointer ||
               tkind == (a_type_kind)tk_typeref ||
               tkind == (a_type_kind)tk_ptr_to_member) {
          /* Determine the type size.  For the "array of incomplete struct or
             union" case, this will put the type entry on a list for later
             fixup. */
          set_type_size(temp_type);
          /* set_type_size may have returned an error type. */
          if (is_error_type(temp_type)) *bottom_derived_type = temp_type;
          /* Find the derived type above this one, and see if it needs to
             have its size computed.  If so, continue looping. */
          if (same_entities(temp_type, *derived_type)) {
            /* We have reached the top of the derived type list; stop. */
            break;
          } else {
            /* Find the derived type entry above this one by searching down
               from the top of the list. */
            prev_temp_type = *derived_type;
            for (;;) {
              tp = underlying_type_of_derived_type(prev_temp_type);
              check_assertion_str(tp != NULL,
                                 "add_to_derived_type_list: bad type in list");
              if (same_entities(tp, temp_type)) break;
              prev_temp_type = tp;
            }  /* for */
            /* Found the previous type entry.  Keep looping. */
            temp_type = prev_temp_type;
            tkind = temp_type->kind;
          }  /* if */
        }  /* while */
      }  /* if */
    }  /* if */
  }  /* if */
  /* Make sure that the new bottom derived type is really the bottom
     and not a node above some other kind of type. */
  *bottom_derived_type = find_bottom_of_type(*bottom_derived_type);

#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 4) {
    fprintf(f_debug, "At end of add_to_derived_type_list:\n");
    fprintf(f_debug, "  new_type_ptr = ");
    if (new_type_ptr != NULL) db_type(new_type_ptr);
    fprintf(f_debug, "\n");
    fprintf(f_debug, "  derived_type = ");
    if (*derived_type != NULL) db_type(*derived_type);
    fprintf(f_debug, "\n");
    fprintf(f_debug, "  *bottom_derived_type = ");
    if (*bottom_derived_type != NULL) db_type(*bottom_derived_type);
    fprintf(f_debug, "\n");
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  db_exit();
}  /* add_to_derived_type_list */


static void scan_eh_spec_type(an_exception_specification_type_ptr
                                                 estp,
                              a_func_info_block  *func_info,
                              a_boolean          ignoring_exception_spec,
                              a_boolean          is_top_level_declarator,
                              a_source_position  *diag_pos)
/*
Scan and check for validity a single type of an exception specification, and
record it in *estp.  *func_info holds information about the function
declarator being parsed.  If ignoring_exception_spec is TRUE, exception
specifications are scanned and discarded.  is_top_level_declarator is TRUE if
we are not parsing a nested declarator.  Diagnostics may be issued at the
given position.
*/
{
  type_name(&estp->type);
  /* Decay array and function types, and drop top-level const/volatile type
     qualifiers (as clarified by the resolution for Core issue 973). */
  adjust_parameter_type(&estp->type);
  estp->type = make_unqualified_type(estp->type);
  if (is_error_type(estp->type)) {
    /* Nothing to be done. */
  } else if (strict_ansi_mode && is_rvalue_reference_type(estp->type) &&
             !is_template_param_type(type_pointed_to(estp->type))) {
    /* A C++11 defect resolution (for Core issue 1267) made rvalue reference
       types invalid in exception specifications, but other compilers appear
       not to enforce this. */
    pos_diagnostic(strict_ansi_discretionary_severity,
                   ec_rvalue_reference_in_exception_specification, diag_pos);
  }  else if (exceptions_enabled && !ms_extensions &&
              !ignoring_exception_spec) {
    /* Check the type to be sure it's not an incomplete type or a pointer
       to an incomplete type.  Microsoft compilers do not use the type
       information at all: We perform no type checking in that case. */
    a_type_ptr     tp = estp->type;
    an_error_code  error_code = ec_no_error;
    /* Issue a diagnostic if an incomplete type is indicated in the exception
       specification.  According to the standard, this is always an error
       (except that in a class definition, the class being defined is
       considered complete for this purpose), but it really only makes a
       difference on a function definition.  We don't know at this point
       whether a top-level declarator belongs to a function definition or not,
       so we defer issuing the diagnostic in that case. */
    /* Force instantiation of template class. */
    complete_type_is_needed(tp);
    if (is_incomplete_type(tp) && !in_definition_of_class(tp)) {
      /* Incomplete type (including possibly void type). */
      error_code = incomplete_type_error_code(tp);
    } else if (is_any_ptr_or_ref_type(tp)) {
      tp = type_pointed_to(tp);
      if (is_void_type(tp)) {
        /* Pointer to cv-qualified void is okay. */
      } else {
        /* Force instantiation of template class. */
        complete_type_is_needed(tp);
        if (is_incomplete_type(tp) && !in_definition_of_class(tp)) {
          error_code = ec_ptr_or_ref_to_incomplete_type;
        }  /* if */
      }  /* if */
    }  /* if */
    if (error_code != ec_no_error) {
      /* Defer a diagnostic if this is a top-level declarator and the type is
         something other than "void"; in strict mode or if the type is "void",
         issue a diagnostic.  Otherwise, suppress the diagnostic -- that is,
         silently allow a non-top-level declaration that throws an incomplete
         type (or pointer thereto) */
      if (is_top_level_declarator && !is_void_type(tp)) {
        defer_exception_spec_error(func_info, error_code, diag_pos, tp);
      } else if (strict_ansi_mode) {
        issue_incomplete_type_diag(error_code, diag_pos, tp,
                                   strict_ansi_discretionary_severity);
      } else if (is_void_type(tp)) {
        issue_incomplete_type_diag(error_code, diag_pos, tp,
                                   /*severity=*/es_error);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* scan_eh_spec_type */

namespace {

/*
Structure recording information to establish the context of a noexcept
operand.  This is currently used to handle the delayed instantiation of the
noexcept specifier appearing on a friend function declaration in a class
template.
*/
struct a_noexcept_arg_descr {
  a_type_ptr	class_type;
			/* The innermost class type in which the noexcept
			   specifier appeared. */
  a_symbol_ptr	prototype_scope_symbols;
			/* The list of prototype scope symbols that were
			   active when the noexcept operand tokens were
			   cached. */
  a_scope_depth	prototype_scope_number;
			/* The scope number to use for the function prototype
			   scope. */
};

#if EXPENSIVE_CHECKING

inline a_boolean operator==(a_noexcept_arg_descr const &nad1,
                            a_noexcept_arg_descr const &nad2)
/*
Return TRUE if and only if the given descriptors are equivalent.
*/
{
  return nad1.class_type == nad2.class_type &&
         nad1.prototype_scope_symbols == nad2.prototype_scope_symbols &&
         nad1.prototype_scope_number == nad2.prototype_scope_number;
}  /* operator== */


inline a_boolean operator!=(a_noexcept_arg_descr const &nad1,
                            a_noexcept_arg_descr const &nad2)
/*
Return FALSE if the given descriptors are not equivalent.
*/
{
  return !(nad1 == nad2);
}  /* operator!= */

#endif /* EXPENSIVE_CHECKING */

}  /* namespace */

typedef Ptr_map<an_exception_specification_ptr, a_noexcept_arg_descr>
		a_noexcept_arg_map;
			/* The type of a hash table mapping exception
			   specification entries to a description of the
			   context in which they appeared. */

STATIC_THREAD a_noexcept_arg_map
		*noexcept_args;
			/* A pointer to a hash table mapping exception
			   specification entries to a description of the
			   context in which they appeared. */


static void mark_mapped_exc_spec(a_decl_parse_state_ptr  dps)
/*
If dps represents the declaration of a function, mark it as having an
exception specification that is mapped to context information through the
noexcept_args table.  This function is called through the end-of-parse-actions
mechanism after the exception specification entry has been mapped (see
scan_noexcept_arg).
*/
{
  if (dps->sym != NULL && is_simple_function_symbol(dps->sym)) {
    dps->sym->variant.routine.pending_mapped_exc_spec = TRUE;
  }  /* if */
}  /* mark_mapped_exc_spec */


void resolve_pending_mapped_exc_spec(a_symbol_ptr                sym,
                                     an_exception_specification  *esp)
/*
Instantiate a noexcept operand whose context is mapped through the
noexcept_args table.  sym is a symbol for the associated routine and esp is
the associated noexcept specifier that needs instantiation.
*/
{
  a_noexcept_arg_descr  nad = noexcept_args->get(esp);
  a_token_cache         *cache = esp->variant.token_cache;

  if (nad.class_type == NULL || cache == NULL) {
    expect_error();
  } else {
    a_routine_ptr  rp = sym->variant.routine.ptr;
    push_class_and_template_reactivation_scope(
                                          nad.class_type,
                                          /*reactivate_template_params=*/TRUE,
                                          /*extend_namespace=*/FALSE);
     /* Recreate a function prototype scope equivalent to the original. */
    (void)push_scope((a_scope_kind)sck_func_prototype,
                     nad.prototype_scope_number, rp->type,
                     (a_routine_ptr)NULL);
    scope_stack_top().outside_parameter_list = TRUE;
    if (nad.prototype_scope_symbols != NULL) {
      reactivate_prototype_scope_symbols(nad.prototype_scope_symbols);
    }  /* if */
    esp->arg_cached = FALSE;
    esp->variant.token_cache = NULL;
    if (rout_type_supp(rp->type)->exception_specification == esp) {
      /* Only clear the flag on the symbol if the exception specification is
         associated with that symbol (which would not be the case when matching
         declarations). */
      sym->variant.routine.pending_mapped_exc_spec = FALSE;
    }  /* if */

    a_shared_token_cache shared_cache =
                                   shared_obj<a_token_cache>(move_from(cache));
    delayed_scan_of_exception_spec(rp, a_reusable_token_cache(shared_cache),
                                   esp);
    delete_fe(&cache);
    noexcept_args->unmap(esp);
    /* Pop the reactivated function prototype scope off the stack. */
    pop_scope();
    pop_class_reactivation_scope();
  }  /* if */
}  /* resolve_pending_mapped_exc_spec */


static void scan_noexcept_arg(an_exception_specification  *esp,
                              a_boolean                   may_cache,
                              a_decl_parse_state          *dps,
                              a_func_info_block           *func_info = NULL)
/*
The noexcept token of a noexcept-specification has just been scanned.  Scan a
noexcept argument if any, and update *esp as appropriate.  If may_cache
is TRUE, cache the argument tokens if appropriate (i.e., if this is a
template-dependent context or a member of a class).  dps describes the
declaration on which the exception specification appears.
*/
{
  a_boolean  is_inclass_member_function_decl = FALSE,
             is_local_decl = FALSE;

  if (scope_is(&scope_stack_top(), sck_func_prototype)) {
    if (dps->is_inclass_member_function_decl) {
      is_inclass_member_function_decl = TRUE;
    } else if (is_local_scope_kind(
                       scope_stack[scope_stack_top().decl_scope_level].kind)) {
      is_local_decl = TRUE;
    }  /* if */
  }  /* if */
  if (curr_token == tok_removed_expr) {
    /* An exception specification in a template context that has been
       removed and replaced with a placeholder.  Just ignore the
       placeholder.  Set the arg_cached field to indicate that the
       exception specification needs to be instantiated. */
    (void)get_token();
    if (esp != NULL) {
      esp->arg_cached = TRUE;
    } else {
      /* We may end up here if a noexcept specifier appear on a non-function
         declaration in a template context. */
      expect_error();
    }  /* if */
  } else if (may_cache &&
             (is_inclass_member_function_decl ||
              is_template_dependent_context() ||
              is_nonspecialized_instantiation_context()) &&
             !is_in_class_specialization_context() &&
             !is_local_decl) {
    /* For top-level declarators in template-dependent contexts, just cache
       the specifier argument for now.  Also create a corresponding template
       cache segment to extract the tokens later on.  In-class specializations
       are handled differently than other template members, so a template
       cache segment should not be created.  A friend template function
       declaration should not have a template cache segment created because
       for that case we want to rescan the noexcept for the individual friend
       declarations. */
    a_token_set_array             stop_tokens;
    a_token_sequence_number       first_tsn, last_tsn;
    /* The caller ensured that an exception specification entry was
       allocated. */
    first_tsn = curr_token_sequence_number;
    clear_token_set_array(stop_tokens);
    incr_token_set_array_element(stop_tokens, tok_rparen);
    incr_token_set_array_element(stop_tokens, tok_semicolon);
    if (esp != NULL &&
        !(dps->variant.auto_params != NULL && !dps->is_abbr_func_template)) {
      /* Do not bother caching the argument if we are going to re-parse the
         declarator because abbreviated function template syntax was
         encountered.  (This avoids having duplicate template cache segment
         entries associated with the current token.) */
      esp->arg_cached = TRUE;
      esp->variant.token_cache = new_fe<a_token_cache>(/*reusable=*/TRUE);
      cache_token_stream(esp->variant.token_cache, stop_tokens);
      if (is_template_dependent_context()) {
        a_template_cache_segment_ptr  tcsp;
        last_tsn = curr_token_sequence_number - 1;
        /* When there is no argument, the computed last token number could be
           less that the first.  In that case, use the first token number as
           the last. */
        last_tsn = last_tsn < first_tsn ? first_tsn : last_tsn;
        tcsp = get_template_cache_segment(
                  (a_symbol_ptr)NULL, (a_template_symbol_supplement_ptr)NULL,
                  first_tsn, last_tsn);
        tcsp->is_exception_specification_arg = TRUE;
        /* Check for the case where the cache is empty. */
        tcsp->expression_missing = esp->variant.token_cache->is_empty();
        tcsp->exception_spec_on_templ_friend = is_template_friend_decl();
      }  /* if */
      terminate_token_cache(esp->variant.token_cache);
    } else {
      flush_tokens_with_stop_tokens_and_warning_flag(
                                      stop_tokens, /*suppress_warning=*/TRUE);
    }  /* if */
  } else {
    a_memory_region_number   region_to_switch_back_to;
    a_scope_stack_entry      *ssep = &scope_stack_top();
    a_boolean                saved_in_template_deduction_context = FALSE;
    a_boolean                saved_in_noexcept_spec;
    check_assertion(scope_is(ssep, sck_func_prototype));
    saved_in_noexcept_spec = ssep->in_noexcept_spec;
    ssep->in_noexcept_spec = TRUE;
    if (exc_spec_in_func_type) {
      saved_in_template_deduction_context =
                                          ssep->in_template_deduction_context;
      ssep->in_template_deduction_context = TRUE;
    }  /* if */
    switch_to_file_scope_region(&region_to_switch_back_to);
    if (scope_is(ssep, sck_func_prototype) &&
        scope_is((ssep-1), sck_class_struct_union) &&
        (dps->dso_flags & DSO_FRIEND) != 0 &&
        dps->sym == NULL) {
      /* A friend function. */
      a_token_set_array     stop_tokens;
      a_noexcept_arg_descr  nad;
      check_assertion(func_info != NULL);
      clear_token_set_array(stop_tokens);
      incr_token_set_array_element(stop_tokens, tok_rparen);
      incr_token_set_array_element(stop_tokens, tok_semicolon);
      if (esp != NULL) {
        esp->arg_cached = TRUE;
        esp->variant.token_cache = new_fe<a_token_cache>(/*reusable=*/TRUE);
        cache_token_stream(esp->variant.token_cache, stop_tokens);
        terminate_token_cache(esp->variant.token_cache);
        nad.class_type = (ssep-1)->assoc_type;
        nad.prototype_scope_symbols = func_info->prototype_scope_symbols;
        nad.prototype_scope_number = ssep->number;
        noexcept_args->map(esp, nad);
        add_end_of_parse_action(mark_mapped_exc_spec, dps,
                                /*secondary_decls=*/TRUE);
      } else {
        flush_tokens_with_stop_tokens_and_warning_flag(
                                                    stop_tokens,
                                                    /*suppress_warning=*/TRUE);
      }  /* if */
    } else {
      /* Scan the argument for the noexcept-specifier, which must be a
         constant-expression convertible to bool. */
      a_source_position        constant_pos = pos_curr_token;
      a_constant_ptr           noexcept_con = local_constant();
      scan_bool_constant_expression(noexcept_con);
      if (esp != NULL) {
        if (constant_is(noexcept_con, ck_template_param) ||
            is_error_constant(noexcept_con) ||
            is_false_constant(noexcept_con)) {
          esp->throw_any = TRUE;
        }  /* if */
        esp->variant.noexcept_arg = move_local_constant_to_il(&noexcept_con);
        esp->variant.noexcept_arg->source_corresp.decl_position = constant_pos;
      } else {
        release_local_constant(&noexcept_con);
      }  /* if */
    }  /* if */
    switch_back_to_original_region(region_to_switch_back_to);
    if (exc_spec_in_func_type) {
      scope_stack_top().in_template_deduction_context =
                                          saved_in_template_deduction_context;
    }  /* if */
    scope_stack_top().in_noexcept_spec = saved_in_noexcept_spec;
  }  /* if */
}  /* scan_noexcept_arg */


void delayed_scan_of_exception_spec(a_routine_ptr              rp,
                                    a_reusable_token_cache     tokens,
                  /* Defaulted: */  an_exception_specification *esp)
/*
The given routine has an exception specification with an operand that hasn't
been parsed yet.  The tokens of the operand are described by the given cache.
Parse the operand now.  If esp is non-NULL, use that as the exception
specification to parse; otherwise, use the specification recorded in rp->type.
*/
{
  a_scope_stack_entry_ptr  ssep = &scope_stack_top();
  a_decl_parse_state       dps;
  a_symbol_ptr             lookup_sym = NULL;
  a_boolean                saved_is_invisible = FALSE;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position        saved_curr_construct_end_position =
                                                  curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

  check_assertion(rp->type->kind == (a_type_kind)tk_routine &&
                  scope_is(ssep, sck_func_prototype));
  /* Recreate a declaration parse state for the routine. */
  init_decl_parse_state(&dps);
  dps.sym = symbol_for(rp);
  if (rp->is_prototype_instantiation && !rp->source_corresp.is_class_member &&
      rp->assoc_template != NULL) {
    /* For non-member function template prototype instantiations, the template
       itself should not be visible in its own noexcept-specifier because the
       function declarator is not complete at that point yet.  (For class
       members, the noexcept-specifier is a complete-class context.) */
    lookup_sym = symbol_for(rp->assoc_template);
    saved_is_invisible = lookup_sym->is_invisible;
    lookup_sym->is_invisible = TRUE;
  }  /* if */
  dps.type = rp->type;
  if (rp->source_corresp.is_class_member) {
    a_type_ptr  parent_class = parent_class_of(rp);
    a_class_symbol_supplement_ptr
                cssp = class_symbol_supp(symbol_for(parent_class));
    if (cssp->routine_fixup_list != NULL ||
        (rp->is_template_function && !rp->is_prototype_instantiation &&
         !rp->is_specialized)) {
      /* If the routine fixup list for this class is still present this must
         be an exception specification appearing in the class definition.
         For real instantiations of class members, the in-class exception
         specification is used too (even though by then the routine fixup list
         is likely processed already). */
      dps.is_inclass_member_function_decl = TRUE;
    } else {
      dps.is_out_of_class_member_function_decl = TRUE;
    }  /* if */
  }  /* if */
  ssep->decl_parse_state = &dps;
  ssep->outside_parameter_list = TRUE;
  if (esp == NULL) {
    esp = rout_type_supp(rp->type)->exception_specification;
  }  /* if */
  rescan_reusable_cache(tokens);
  begin_deferral_of_access_checks();
  if (esp->is_noexcept) {
    Value_saver<a_symbol_locator>  saved_locator_for_curr_id(
                                                         &locator_for_curr_id);
    scan_noexcept_arg(esp, /*may_cache=*/FALSE, &dps);
  } else {
    /* Delayed instantiation of dynamic exception specifications (which are no
       longer part of the language as of C++17) is not implemented.  (So we
       should never get here.) */
    unexpected_condition();
  }  /* if */
  perform_deferred_access_checks_for_function(rp);
  end_deferral_of_access_checks();
  if (lookup_sym != NULL) {
    lookup_sym->is_invisible = saved_is_invisible;
  }  /* if */
  if (curr_token != tok_end_of_source) {
    /* Tokens remain in the cache: Issue an error. */
    pos_error(ec_exp_rparen, &pos_curr_token);
    /* Flush to the end of the cache. */
    while (curr_token != tok_end_of_source) (void)get_token();
  }  /* if */
  /* Skip past the tok_end_of_source. */
  (void)get_token();
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = saved_curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
}  /* delayed_scan_of_exception_spec */


static an_exception_specification_ptr scan_exception_specification(
                                  a_decl_parse_state  *dps,
                                  a_func_info_block   *func_info,
                                  a_boolean           exception_spec_allowed,
                                  a_boolean           is_top_level_declarator)
/*
Scan an exception specification, which may be empty or take one of the
following forms:

  noexcept
  noexcept( <expression> )
  throw ()
  throw ( <type-name> [, <type-name> ]... )
  throw (...)

(The last variant, "throw (...)", is a Microsoft feature used to indicate
explicitly that a function can throw any exception.  This is necessary for
Microsoft compilers because they assume by default that extern "C" functions
do not throw exceptions.)

Return a (possibly NULL) pointer to the appropriate kind of exception
specification entry and update *func_info (describing the current function
declarator) as needed.

Diagnostics are issued on redundant types on a list, but if this is a
redeclaration of a routine, reconciliation with the previous throw
specification is handled later (see check_exception_specification).

exception_spec_allowed is FALSE if this routine is called for a function
declarator that doesn't permit exception specifications (e.g., the function
declarator in a pointer-to-pointer-to-function declaration).
is_top_level_declarator is TRUE if this is function is called for a function
declarator that is the top-level declarator of a declaration (i.e., it
actually declares a function, member function, or function template).
*/
{
  an_exception_specification_ptr       esp = NULL;
  an_exception_specification_type_ptr  estp, other_estp, end_of_list = NULL;
  a_source_position                    type_pos;
  a_boolean                            ignoring_exception_spec = FALSE;
  a_boolean                            is_noexcept, is_edg_throw = FALSE;

  db_enter(4, "scan_exception_specification");
  is_noexcept = curr_token == tok_noexcept;
  if (exceptions_enabled || is_noexcept || curr_token == tok_throw ||
      curr_token == tok_edg_throw) {
    /* Update the source position for the "throw".  Even if there is no
       "throw" this is where it would appear in the source.  If exception
       support is not enabled but a "throw" appears, we may want to issue
       a diagnostic, so save the source position for that case, too. */
    func_info->throw_position = pos_curr_token;
  }  /* if */
  if (curr_token != tok_throw && curr_token != tok_edg_throw && !is_noexcept) {
    /* No explicit throw specification, meaning anything may be thrown. */
    goto done;
  }  /* if */
  if (!exception_spec_allowed ||
      (!exceptions_enabled && !exc_spec_in_func_type) ||
      (microsoft_bugs && microsoft_version <= 1200 && !is_noexcept)) {
    /* If this is a context in which an exception specification is not allowed,
       exception-handling support is not enabled and is not required to be part
       of the function type, or if (in some Microsoft-compatibility modes)
       exception specifications are recognized but ignored, set a flag to
       control the diagnostics that are put out. */
    ignoring_exception_spec = TRUE;
  }  /* if */
  if (!ignoring_exception_spec ||
      (is_noexcept && is_top_level_declarator && next_token() == tok_lparen &&
       (is_template_dependent_context() ||
        is_nonspecialized_instantiation_context()))) {
    /* An exception specification must be recorded if it is not ignored later
       on, but, in the case of noexcept we also want to record it so that it
       can be instantiated later on (even though it will be ignored after that
       instantiation). */
    esp = alloc_exception_specification();
    esp->is_noexcept = is_noexcept;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    esp->source_range.start = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  }  /* if */
  if (!ignoring_exception_spec) {
    /* Exceptions are outside the "Embedded C++" subset. */
    feature_is_not_part_of_embedded_cplusplus_subset(
                                          &pos_curr_token,
                                          ec_exceptions_in_embedded_cplusplus);
  } else if (!exception_spec_allowed) {
    /* This is a declaration on which an exception specification is not
       allowed. */
    pos_diagnostic((!exceptions_enabled || microsoft_bugs) ?
                       es_warning : es_discretionary_error,
                   ec_exception_specification_not_allowed, &pos_curr_token);
  } else if (microsoft_bugs && microsoft_version <= 1200) {
    /* Issue a remark: Exception specifications are parsed and discarded. */
    pos_remark(ec_exception_specification_ignored, &pos_curr_token);
  }  /* if */
  if (is_noexcept) {
    if (next_token() != tok_lparen) {
      /* "noexcept" without arguments. */
      if (esp != NULL) {
        esp->variant.noexcept_arg = NULL;
#if EXTRA_SOURCE_POSITIONS_IN_IL
        esp->source_range.end = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      }  /* if */
      /* Scan past the "noexcept" token. */
      (void)get_token();
      goto done;
    }  /* if */
  } else if (curr_token == tok_edg_throw) {
    /* Recognize the "__edg_throw__" throw token, which is equivalent to
       "throw" but should not elicit certain diagnostics. */
    is_edg_throw = TRUE;
  }  /* if */
  /* Bypass "throw" or "noexcept". */
  (void)get_token();
  /* Start a new stop token state. */
  push_stop_token_stack();
  add_stop_token(tok_semicolon);
  add_stop_token(tok_lbrace);
  add_stop_token(tok_rparen);
  /* Next token should be a left paren. */
  if (curr_token == tok_lparen) {
    (void)get_token();
    if (is_noexcept) {
      a_boolean  may_cache = FALSE;
      if (is_top_level_declarator &&
          !((dps->dso_flags & DSO_FRIEND) != 0 &&
            !is_template_friend_decl()) &&
          (!dps->is_lambda ||
           (dps->is_abbr_func_template &&
            !scope_stack[depth_scope_stack-3].in_prototype_instantiation))) {
        /* A noexcept argument should generally be cached for later
           instantiation if we are in a template or class definition.  However,
           that's not the case if we're in an ordinary friend function
           declaration (not a friend template declaration), nor for nongeneric
           lambdas (which aren't "members" of any enclosing templates), nor
           even for generic lambdas that appear in a template (if this is a
           generic lambda, scope_stack[depth_scope_stack-1] is its template
           declaration scope and scope_stack[depth_scope_stack-2] is its
           closure class scope).  It's also not the case for pointers to
           functions and the like. */
        may_cache = TRUE;
      }  /* if */
      scan_noexcept_arg(esp, may_cache, dps, func_info);
      goto finish_list;
    } else if (curr_token == tok_rparen) {
      /* Case is "throw ()" -- which means "no exception will be thrown by
         this routine." */
        if (cpp20_mode && !is_noexcept && !is_edg_throw &&
            exc_spec_in_func_type) {
          /* The exception-specification "throw()" was completely dropped in
             C++20, but current practice is to accept it. */
          pos_diagnostic(strict_ansi_mode ? es_discretionary_error
                                          : es_warning,
                         ec_empty_throw_specification_not_cpp20,
                         &func_info->throw_position);
        }  /* if */
        goto finish_list;
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (ms_extensions && microsoft_version >= 1300 &&
               curr_token == tok_ellipsis) {
      /* Some microsoft compilers treat function with "C" linkage as having an
         implicit "throw()" specification.  For those functions with "C"
         linkage that can throw an exception, an explicit "throw(...)" must
         be specified. */
      /* Bypass the ellipsis. */
      (void)get_token();
      if (esp == NULL && exception_spec_allowed) {
        esp = alloc_exception_specification();
#if EXTRA_SOURCE_POSITIONS_IN_IL
        esp->source_range.start = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      }  /* if */
      if (esp != NULL) {
        esp->throw_any = TRUE;
      }  /* if */
      goto finish_list;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
  } else {
    /* Syntax error -- left paren is missing.  We don't actually call
       syntax_error or required_token for this, however, since writing
       "throw int" instead of "throw (int)" might be a common mistake. */
    pos_error(ec_exp_lparen, &error_position);
  }  /* if */
  /* Loop through the types. */
  do {
    a_pack_expansion_stack_entry_ptr	pesep;
    a_boolean				any_types;
    add_stop_token(tok_comma);
    /* An exception specification is a potential variadic pack expansion
       context. */
    any_types = begin_potential_pack_expansion_context(&pesep);
    while (any_types) {
      /* Allocate the throw spec type entry. */
      estp = alloc_exception_specification_type();
#if EXTRA_SOURCE_POSITIONS_IN_IL
      estp->source_position = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      type_pos = pos_curr_token;
      scan_eh_spec_type(estp, func_info, ignoring_exception_spec,
                        is_top_level_declarator, &type_pos);
      if (esp != NULL) {
        /* Add estp to the list. */
        if (end_of_list == NULL) {
          esp->variant.exception_specification_type_list = estp;
        } else {
          if (!is_error_type(estp->type)) {
            /* Examine other entries already on the list to see if the current
               one is redundant. */
            other_estp = esp->variant.exception_specification_type_list;
            for (; other_estp != NULL; other_estp = other_estp->next) {
              if (!other_estp->redundant &&
                  identical_types(estp->type, other_estp->type)) {
                pos_remark(ec_redundant_exception_specification_type,
                           &type_pos);
                estp->redundant = TRUE;
                break;
              }  /* if */
            }  /* for */
          }  /* if */
          /* Add it to the end of the list. */
          end_of_list->next = estp;
        }  /* if */
        end_of_list = estp;
        if (!estp->redundant && !is_error_type(estp->type)) {
          /* Mark the type as having been used in an exception.  (Also, if it
             "contains" any classes, they are marked as requiring external
             linkage.) */
          set_used_in_exception_or_rtti_flag(estp->type);
        }  /* if */
      }  /* if */
      { a_pack_expansion_descr_ptr pedep =
            end_potential_pack_expansion_context(pesep,
                                                 /*is_declarator=*/FALSE);
        if (pedep != NULL) {
          estp->is_pack_expansion = TRUE;
        }  /* if */
      }
      any_types = advance_to_next_pack_element(pesep);
    }  /* while */
    pesep = NULL;
    remove_stop_token(tok_comma);
    /* If the next token is not a comma, it should be a right paren -- but
       check for a few other tokens that (in error cases) should also force
       the loop to terminate. */
    if (curr_token == tok_rparen || curr_token == tok_end_of_source ||
        curr_token == tok_semicolon || curr_token == tok_lbrace) {
      break;
    }  /* if */
  } while (loop_token(tok_comma));
  if (esp != NULL) {
    check_assertion(!esp->indeterminate);
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (ms_extensions && microsoft_version >= 1300 && esp != NULL &&
        !is_noexcept &&
        esp->variant.exception_specification_type_list != NULL) {
      /* Some versions of Microsoft C++ treat any non-empty exception
         specification as "throw (...)". */
      esp->variant.exception_specification_type_list = NULL;
      esp->throw_any = TRUE;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (esp->variant.exception_specification_type_list != NULL ||
        (esp->throw_any && !esp->is_noexcept)) {
      /* A dynamic exception specification other than "throw()".  C++11
         deprecated them and C++17 removed them altogether because they do not
         interact well with exception specifications becoming part of function
         types. */
      a_source_position  *pos = &func_info->throw_position;
      if (is_edg_throw) {
        /* Nothing to report. */
      } else if (exc_spec_in_func_type) {
        pos_diagnostic(microsoft_mode ? es_warning : es_discretionary_error,
                       ec_dynamic_exc_spec_not_permitted, pos);
        esp = NULL;
      } else if (cpp11_mode && !ignoring_exception_spec) {
        pos_diagnostic(cpp14_mode ? es_warning : es_remark,
                       ec_dynamic_exception_specifications_deprecated, pos);
      }  /* if */
    }  /* if */
  }  /* if */
finish_list:;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (esp != NULL) {
    esp->source_range.end = pos_curr_token;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* List should be terminated by a right paren. */
  remove_stop_token(tok_rparen);
  (void)required_token(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_lbrace);
  remove_stop_token(tok_semicolon);
  /* Restore the stop token state. */
  pop_stop_token_stack();
done:;
  db_exit();
  return esp;
}  /* scan_exception_specification */


static a_boolean is_prototyped_parameter_list_start(void)
/*
Return TRUE if the current token is the start of a prototyped parameter list,
FALSE if it is the start of an old-style identifier list.  The current token
is the first token after the opening parenthesis.  Note that the case of
an empty parameter list, as in "int f();", is handled by the caller and
need not be addressed here.
*/
{
  a_boolean    prototyped;
  a_token_kind next_tok;

  /* The ambiguous cases start with an identifier. */
  if (curr_token == tok_identifier) {
    if (curr_id_is_type_name(GID_NO_OPTIONS, IDS_NO_OPTIONS)) {
      /* The identifier is a typedef symbol. */
      if (C_dialect != C_dialect_pcc) {
        /* In ANSI and C++ mode, this must be a prototyped parameter list. */
        prototyped = TRUE;
      } else {
        /* In pcc mode, it's an old-style identifier list if the
           identifier is followed by "," or ")", and a prototyped parameter
           list otherwise.  pcc would, of course, consider anything to
           be an old-style identifier list. */
        next_tok = next_token();
        if (next_tok == tok_comma || next_tok == tok_rparen) {
          /* A case like 
               int f(t )     or
               int f(t , u)
                       ^---- must be old-style.
          */
          prototyped = FALSE;
        } else {
          /* A case like
               int f(t x)
                       ^---- must be prototyped.
          */
          prototyped = TRUE;
        }  /* if */
      }  /* if */
    } else {
      /* The identifier is not a typedef.  This suggests the start of an
         old-style identifier list. */
      if (C_dialect == C_dialect_pcc) {
        /* In pcc mode, this must be an old-style identifier list. */
        prototyped = FALSE;
      } else {
        /* To improve error recovery in ANSI mode for
             int f(tt x);
           where tt was supposed to be a typedef identifier but was not
           declared (maybe tt is misspelled), assume a prototyped parameter
           list if the next token is not a "," or ")". */
        next_tok = next_token();
        if (next_tok == tok_comma || next_tok == tok_rparen) {
          /* A case like 
               int f(tt )     or
               int f(tt , u )
                        ^---- must be old-style.
          */
          prototyped = FALSE;
        } else {
          /* A case like
               int f(tt x)
                        ^---- must be prototyped.
          */
          prototyped = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (is_decl_start(IDS_REAL_DECLARATOR_ALLOWED)) {
    /* The parameter list starts with something that looks like the start of
       a declaration (but not a typedef identifier): for example, "int".
       This must be a prototyped parameter list. */
    prototyped = TRUE;
  } else {
    /* The parameter list starts with something else, meaning there's an
       error.  In ANSI mode, assume a prototyped parameter list; in pcc
       mode, assume an old-style identifier list.  The error will be
       issued below. */
    prototyped = (C_dialect != C_dialect_pcc);
  }  /* if */
  return prototyped;
}  /* is_prototyped_parameter_list_start */


static void scan_member_function_modifiers(
                                      ARG_UNUSED a_symbol_locator   *locator,
                                      ARG_UNUSED a_decl_parse_state *dps,
                                      a_func_info_block             *func_info)
/*
Scan for member function modifiers and record their presence in *func_info.
*dps describes some syntactic properties of the current declaration.
("sealed", "abstract", and "override" are an ECMA C++/CLI extension also
accepted by some Microsoft compilers in their non-CLI modes; "new" is only
accepted in C++/CLI mode.)  "final" is accepted in later Microsoft modes.
*/
{
  a_boolean  accept_ms_modifiers = ms_extensions &&
                                   (cli_or_cx_enabled ||
                                    microsoft_version >= 1400);
  a_boolean  accept_ms_final_modifier = (microsoft_mode &&
                                         microsoft_version >= 1700);

  if (std_override_modifiers_enabled || accept_ms_modifiers) {
    a_boolean  err = FALSE;
    for (;;) {
      if ((std_override_modifiers_enabled || accept_ms_modifiers) &&
          check_context_sensitive_keyword(tok_override, "override")) {
        if (func_info->override) {
          pos_error(ec_duplicate_function_modifier, &dps->declarator_pos);
          err = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else if (microsoft_mode &&
                   ((locator->is_destructor_name &&
                     microsoft_version < 1700) ||
                    locator->is_finalizer_name)) {
          pos_error(locator->is_destructor_name ?
                                        ec_modifier_not_allowed_on_destructor
                                      : ec_modifier_not_allowed_on_finalizer,
                    &pos_curr_token);
          err = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        } else {
          if (gpp_mode && !cpp11_mode) {
            pos_warning(ec_override_and_final_is_cpp11, &dps->declarator_pos);
          }  /* if */
          func_info->override = TRUE;
        }  /* if */
      } else if ((std_override_modifiers_enabled ||
                  accept_ms_final_modifier) &&
                 (check_context_sensitive_keyword(tok_final, "final") ||
                  ((gnu_version_is(>= 40700) || clang_mode) &&
                   check_context_sensitive_keyword(tok_final, "__final")))) {
        if (func_info->final) {
          pos_error(ec_duplicate_function_modifier, &dps->declarator_pos);
          err = TRUE;
        } else {
          if (gpp_mode && !cpp11_mode) {
            pos_warning(ec_override_and_final_is_cpp11, &dps->declarator_pos);
          }  /* if */
          func_info->final = TRUE;
        }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (accept_ms_modifiers &&
                 check_context_sensitive_keyword(tok_abstract, "abstract")) {
        if (func_info->abstract) {
          pos_error(ec_duplicate_function_modifier, &dps->declarator_pos);
          err = TRUE;
        } else if (func_info->sealed) {
          pos_error(ec_function_modifiers_abstract_and_sealed,
                    &dps->declarator_pos);
          err = TRUE;
        } else if (locator->is_destructor_name || locator->is_finalizer_name) {
          pos_error(locator->is_destructor_name ?
                                        ec_modifier_not_allowed_on_destructor
                                      : ec_modifier_not_allowed_on_finalizer,
                    &pos_curr_token);
          err = TRUE;
        } else {
          func_info->abstract = TRUE;
        }  /* if */
      } else if (accept_ms_modifiers &&
                 check_context_sensitive_keyword(tok_sealed, "sealed")) {
        if (func_info->sealed) {
          pos_error(ec_duplicate_function_modifier, &dps->declarator_pos);
          err = TRUE;
        } else if (func_info->abstract) {
          pos_error(ec_function_modifiers_abstract_and_sealed,
                    &dps->declarator_pos);
          err = TRUE;
        } else if (locator->is_destructor_name || locator->is_finalizer_name) {
          pos_error(locator->is_destructor_name ?
                                        ec_modifier_not_allowed_on_destructor
                                      : ec_modifier_not_allowed_on_finalizer,
                    &pos_curr_token);
          err = TRUE;
        } else {
          func_info->sealed = TRUE;
        }  /* if */
      } else if (cli_or_cx_enabled && curr_token == tok_new) {
        if (func_info->new_member) {
          pos_error(ec_duplicate_function_modifier, &dps->declarator_pos);
          err = TRUE;
        } else if (locator->is_destructor_name || locator->is_finalizer_name) {
          pos_error(locator->is_destructor_name ?
                                        ec_modifier_not_allowed_on_destructor
                                      : ec_modifier_not_allowed_on_finalizer,
                    &pos_curr_token);
          err = TRUE;
        } else {
          func_info->new_member = TRUE;
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else {
        break;
      }  /* if */
      if (!err &&
          (!dps->is_nonstatic_member_function_decl ||
           (dps->is_template_declaration && !dps->is_generic_declaration))) {
        pos_error(!dps->is_nonstatic_member_function_decl ?
                                ec_member_function_modifier_on_static_member :
                                ec_member_function_modifier_on_template,
                  &pos_curr_token);
#if MICROSOFT_EXTENSIONS_ALLOWED
        func_info->new_member = FALSE;
        func_info->sealed = FALSE;
        func_info->abstract = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        func_info->final = FALSE;
        func_info->override = FALSE;
        err = TRUE;
      }  /* if */
      (void)get_token();
    }  /* for */
  }  /* if */
}  /* scan_member_function_modifiers */

#if MICROSOFT_EXTENSIONS_ALLOWED

a_boolean check_param_array_type(a_param_type_ptr   ptp,
                                 a_source_position  *diag_pos)
/*
Return TRUE if the given C++/CLI parameter array is a handle to a
one-dimensional CLI array; if not and diag_pos is non-NULL, issue a diagnostic
at *diag_pos.
*/
{
  a_boolean  err = FALSE;
  a_type_ptr param_type = ptp->type;

  if (!is_handle_type(param_type)) {
    err = !is_error_type(param_type) && !is_template_param_type(param_type);
  } else {
    /* Check that the handle "points to" a C++/CLI array type. */
    a_type_ptr  tp = type_pointed_to(param_type);
    tp = skip_typerefs(tp);
    if (!is_cli_array_type(tp)) {
      err = !is_error_type(tp) && !is_template_param_type(tp);
    } else {
      /* Check that the C++/CLI array type is one-dimensional.  Template-
         dependent dimensions are not acceptable. */
      a_boolean unknown;
      if (cli_array_rank(tp, &unknown) != 1) {
        if (is_error_constant(cli_array_rank_constant(tp))) {
          /* Do not issue another diagnostic but turn the parameter type into
             an error type to avoid surprises downstream. */
          expect_error();
          ptp->type = error_type();
        } else {
          err = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (err) {
    if (diag_pos != NULL) {
      pos_error(ec_invalid_param_array_type, diag_pos);
    }  /* if */
    ptp->type = error_type();
  }  /* if */
  return !err;
}  /* check_param_array_type */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

void make_param_syms_invisible(a_boolean  is_invisible)
/*
The current scope is a function prototype scope.  Set the is_invisible flag of
the parameter symbols in that scope to is_invisible.
*/
{
  a_symbol_ptr  sym = scope_stack_top().pointers_block.symbols;

  for (; sym != NULL; sym = sym->next_in_scope) {
    if (sym->kind == (a_symbol_kind)sk_parameter) {
      sym->is_invisible = is_invisible;
    }  /* if */
  }  /* for */
}  /* make_param_syms_invisible */


void diagnose_invalid_class_templ_arg_deduction(a_decl_parse_state  *dps)
/*
*dps represents a declaration that uses a class template name as a type
specifier, but class template argument deduction is not valid in this context.
Issue an error suggesting explicit template arguments.
*/
{
  a_type_ptr  ptp = dps->auto_type;
  a_template_param_type_supplement_ptr
              tptsp;

  check_assertion(type_is(ptp, tk_template_param));
  tptsp = ptp->variant.template_param.extra_info;
  pos_sy_error(ec_missing_template_arg_list, &dps->auto_pos,
               tptsp->constraint.class_template_symbol);
}  /* diagnose_invalid_class_templ_arg_deduction */


void check_type_with_placeholder_specifier(a_decl_parse_state  *state)
/*
*state describes a declaration parsing state involving a complete declarator
that builds a type on top of an "auto" or "decltype(auto)" type specifier, or
a class template name used as a placeholder type.  Check that the resulting
type is not an array type, and, if C++14-style deduced return types are not
enabled, that it is not a function type without a trailing return type.  Also,
if this is a secondary declarator, diagnose cases such as
    auto f()->int, x = 0;
where "auto" is used both to deduce a type from an initializer and to announce
a trailing return type.
*/
{
  a_boolean  err = FALSE, ctad_case = state->has_deducible_class_templ_args;

  check_assertion(state->has_deduced_type);
  if (is_array_type(state->declared_type)) {
    if (state->is_param_decl && 
        (state->assoc_func_decl_state != NULL &&
         (state->assoc_func_decl_state->is_lambda ||
          abbr_func_templates_enabled))) {
      /* In the context of generic lambda parameters, "auto" can be used to
         create an array type.  For example: 
           int x = [](auto [3]) { return 42; }((int*)nullptr); 
      */
    } else {
      if (!ctad_case) {
        pos_error(ec_auto_type_in_array_type, &state->auto_pos);
      }  /* if */
      err = TRUE;
    }  /* if */
  } else {
    a_boolean  is_function_declarator =
                        state->declared_type->kind == (a_type_kind)tk_routine;
    /* Check that if "decltype(auto)" or a class template name is used, it has
       no declarator operator on top.   E.g., "decltype(auto)& g();" is
       invalid. */
    if ((state->decltype_auto_specifier_seen || ctad_case) &&
        ((is_function_declarator && !state->has_trailing_return_type) ||
         state->is_trailing_return_type || state->is_conversion_type_id)) {
      a_type_kind  ret_kind;
      if (is_function_declarator) {
        ret_kind = state->declared_type->variant.routine.return_type->kind;
      } else {
        ret_kind = state->declared_type->kind;
      }  /* if */
      if (ret_kind == (a_type_kind)tk_pointer ||
          ret_kind == (a_type_kind)tk_ptr_to_member) {
        if (!ctad_case) {
          pos_error(ec_decltype_auto_return_must_be_standalone,
                    &state->auto_pos);
        }  /* if */
        err = TRUE;
      }  /* if */
    }  /* if */
    /* Check whether a trailing return type is missing. */
    if (is_function_declarator && !state->has_trailing_return_type) {
      if (deduced_return_types_enabled && !state->is_param_decl &&
          !state->is_type_name && !ctad_case) {
        /* Something like "auto g() { return 0; }", which is permitted in
           C++14. */
        state->has_deducible_return_type = TRUE;
        if (warn_on_deduced_return_types) {
          pos_warning(ec_deduced_return_types_is_cpp14, &state->auto_pos);
        }  /* if */
      } else {
        if (is_error_type(state->specifiers_type)) {
          /* A diagnostic has been issued already. */
          expect_error();
        } else if (!ctad_case) {
          pos_error(trailing_return_types_enabled ?
                      ec_missing_trailing_return_type :
                      ec_auto_type_in_function_type,
                    &state->auto_pos);
        }  /* if */
        err = TRUE;
      }  /* if */
    }  /* if */
    /* Check that a class template name placeholder didn't appear in an
       invalid context. */
    if (ctad_case && (state->is_conversion_type_id ||
                      (state->is_nontype_template_param && !cpp20_mode))) {
      err = TRUE;
    }  /* if */
  }  /* if */
  if (err) {
    if (ctad_case) {
      /* For class template argument deduction cases, we always issue an error
         suggesting explicit template arguments. */
      diagnose_invalid_class_templ_arg_deduction(state);
    }  /* if */
    discard_placeholder_type(state);
  } else if (state->secondary_declarator) {
    /* Check that "auto" is not used both to announce a trailing return type
       and as a deducible type specifier. */
    if ((state->deduced_auto_type == NULL) !=
                                            state->has_trailing_return_type) {
      pos_diagnostic(strict_ansi_mode ? strict_ansi_discretionary_severity
                                      : es_warning,
                     ec_auto_used_two_ways, &state->auto_pos);
    }  /* if */
  }  /* if */
}  /* check_type_with_placeholder_specifier */


static void scan_trailing_return_type(a_decl_parse_state  *dps,
                                      a_type_ptr          rout_type)
/*
A function declarator has just been parsed (resulting in the entry rout_type)
and the current token is a "->".  Scan a trailing return type and update
*rout_type and *dps accordingly (*dps describes the current declaration).
Diagnostics are issued if the function declarator does not in fact allow for a
trailing return type (e.g., because the type specifier was not "auto").  This
routine is also called for the trailing return type of a lambda declarator.
*/
{
  a_decl_parse_state             trt_dps;
  a_boolean                      err = FALSE;

  check_assertion(curr_token == tok_arrow);
  if (dps->is_lambda || dps->is_deduction_guide) {
    /* No special syntax checks are needed. */
  } else if (!dps->auto_type_specifier_seen ||
             dps->has_deducible_class_templ_args) {
    /* Something like "int ()->int" or "decltype(auto) f()->void". */
    pos_error(ec_trailing_return_type_requires_auto, &error_position);
    err = TRUE;
  } else if (dps->in_nested_declarator) {
    /* Something like "auto (()->int)". */
    pos_error(ec_trailing_return_type_in_nested_declarator, &error_position);
    err = TRUE;
  } else if (gpp_version_is(any_version) ?
                 skip_typerefs_not_typedefs(dps->type) != dps->auto_type :
                 dps->type != dps->auto_type) {
    /* Something like "auto *()->int".  GCC appears to accept something like
       "auto const ()->int". */
    pos_error(ec_trailing_return_type_function_without_simple_auto,
              &dps->declarator_start_pos);
    err = TRUE;
  } else if (dps->type != dps->auto_type) {
    pos_warning(ec_trailing_return_type_function_without_simple_auto,
                &dps->declarator_start_pos);
    dps->type = dps->auto_type;
    dps->declared_type = dps->auto_type;
    dps->specifiers_type = dps->auto_type;
  }  /* if */
  /* Any leading "auto" did not represent a deduced type after all. */
  if (dps->secondary_declarator) {
    /* Check that "auto" is not used both to announce a trailing return type
       and as a deducible type specifier. */
    if (dps->deduced_auto_type != NULL) {
      pos_diagnostic(strict_ansi_mode ? strict_ansi_discretionary_severity
                                      : es_warning,
                     ec_auto_used_two_ways, &dps->auto_pos);
    }  /* if */
  }  /* if */
  dps->has_deduced_type = FALSE;
  /* Skip over the "->" token. */
  (void)get_token();
  dps->return_type_pos = pos_curr_token;
  init_decl_parse_state(&trt_dps);
  trt_dps.is_trailing_return_type = TRUE;
  trt_dps.trailing_return_type_allowed = trailing_return_types_enabled;
  if (deduced_return_types_enabled && dps->auto_type_allowed) {
    trt_dps.auto_type_allowed = TRUE;
  }  /* if */
  if (parameters_visible_late) {
  /* In some GNU C++ modes, parameter symbols are marked invisible until a
     definition (if any) is seen.  The corresponding GCC compilers do not
     support trailing return types, but that model severely limits the
     usefulness of trailing return types.  In such modes, we therefore
     make the parameters visible while scanning the trailing return type. */
    make_param_syms_invisible(FALSE);
  }  /* if */
  /* Parse the trailing return type.  While doing this, set
     dps->has_trailing_return_type to TRUE.  This is sufficient for some of
     the expression routines to know that we are parsing a trailing return
     type. */
  dps->has_trailing_return_type = TRUE;
  type_name_full(&trt_dps);
  if (parameters_visible_late) {
    make_param_syms_invisible(TRUE);
  }  /* if */
  if (err) {
    dps->specifiers_type = dps->declared_type = dps->type = error_type();
    dps->auto_type_specifier_seen = FALSE;
    dps->has_deduced_type = FALSE;
    dps->decltype_auto_specifier_seen = FALSE;
    dps->has_trailing_return_type = FALSE;
  } else {
    /* Replace the specifiers type (which was auto) and the type assembled
       so far (which should be the same as the specifiers type)  by the
       actual return type. */
    dps->specifiers_type = dps->declared_type = dps->type = trt_dps.type;
    rout_type->variant.routine.extra_info->trailing_return_type = TRUE;
    if (trt_dps.has_deduced_type &&
        (!trt_dps.has_trailing_return_type ||
         trt_dps.has_deducible_return_type)) {
      dps->has_deducible_return_type = TRUE;
    }  /* if */
  }  /* if */
}  /* scan_trailing_return_type */


static void cplusplus_function_declarator_trailer(
                         ARG_UNUSED a_decl_parse_state *state,
                         a_type_ptr                    rout_type,
                         a_func_info_block             *func_info,
                         a_symbol_locator              *locator,
                         a_type_ptr                    parent_type,
                         a_boolean                     top_level,
                         a_boolean                     is_nonstatic_member,
                         a_boolean                     is_constructor,
                         a_boolean                     is_static_constructor,
                         a_boolean                     is_destructor,
                         a_boolean                     is_finalizer,
                         a_boolean                     disallow_exception_spec,
                         a_boolean                     is_typedef_decl,
                         a_decl_pos_block              *decl_pos_block)
/*
Parse any C++-specific additions to a function declarator that follow its
closing right parenthesis (cv-qualifiers and/or exception specifications),
and update the given routine type supplement accordingly.  top_level is
TRUE if we're parsing a top-level declarator.  rout_type is the new routine
type.  For the other parameters, see function_declarator (below) for which
this is a helper function.
*/
{
  a_routine_type_supplement_ptr   rtsp = rout_type->variant.routine.extra_info;
  a_type_ptr                      this_class = NULL;
  a_type_qualifier_set            qualifiers = TQ_NONE;
  a_ref_qualifier_kind            ref_qualifiers =
                                             (a_ref_qualifier_kind)rqk_default;
  a_boolean                       qualifier_err = FALSE;
  a_boolean                       cv_qualifier_with_no_this_class_okay = FALSE;
  an_exception_specification_ptr  esp;
  an_attribute_ptr                attributes = NULL;

  /* Create a pointer to the implicit "this" parameter.  This can be done
     for nonstatic function declarations within a class definition or
     for member function declarations outside a class definition when
     a function qualifier is present.  If there is a function qualifier,
     it is applied to the type pointed to by the this param type. */
  if (is_typedef_decl || state->is_template_type_argument || state->is_alias ||
      state->is_reflected_type) {
    /* Typedefs and alias declarations for function types can have a
       cv-qualifier even though there is no "parent class" in those cases.  The
       same is true for type-ids denoting function type arguments for template
       type parameters or the type-id following a reflection operator.  (This
       is only possible when the declarator type is a function type; if, e.g.,
       a pointer-to-function type is being formed, a diagnostic will be issued
       later.) */
    cv_qualifier_with_no_this_class_okay = TRUE;
  }  /* if */
  if (state->is_lambda) {
    /* Lambdas don't allow a cv-qualifier here, but they are "const" by
       default.  "mutable", however, is allowed here (unless there is an
       explicit "this" parameter), and means the lambda is non-const.
       "constexpr" and "consteval" are also permitted and apply to the call
       operator. */
    a_boolean  done_with_quals, mutable_seen = FALSE;
    do {
      done_with_quals = TRUE;
      if (curr_token == tok_mutable) {
        if (has_explicit_this_parameter(rout_type)) {
          /* Lambdas with an explicit "this" parameter cannot be "mutable". */
          pos_error(ec_mutable_qualifier_on_explicit_this_lambda,
                    &error_position);
        } else if (state->storage_class == sc_static) {
          pos_error(ec_lambda_mutable_and_static, &pos_curr_token);
        } else if (mutable_seen) {
          pos_error(ec_dupl_decl_specifier, &pos_curr_token);
        } else if (func_info->lambda != NULL) {
          func_info->lambda->is_mutable = TRUE;
#if EXTRA_SOURCE_POSITIONS_IN_IL
          func_info->lambda->mutable_position = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        } else {
          /* func_info->lambda may be NULL during real instantiations of
             generic lambdas. */
          check_assertion(scope_is(&scope_stack_top()-1,
                                   sck_template_instantiation));
        }  /* if */
        mutable_seen = TRUE;
        (void)get_token();
        done_with_quals = FALSE;
      } else if (curr_token == tok_constexpr || curr_token == tok_consteval) {
        if (!constexpr_lambdas_enabled) {
          pos_error(ec_constexpr_lambdas_not_enabled, &pos_curr_token);
        } else if (func_info->lambda != NULL) {
          if (func_info->lambda->constexpr_specified ||
              func_info->lambda->consteval_specified) {
            a_boolean  duplicate = func_info->lambda->consteval_specified ==
                                                 (curr_token == tok_consteval);
            pos_error(duplicate ? ec_dupl_decl_specifier :
                                  ec_constexpr_and_consteval_specifiers,
                      &pos_curr_token);
          }  /* if */
          if (curr_token == tok_constexpr) {
            func_info->lambda->constexpr_specified = TRUE;
          } else {
            func_info->lambda->consteval_specified = TRUE;
          }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
          func_info->lambda->constexpr_position = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        } else {
          /* func_info->lambda may be NULL during real instantiations of
             generic lambdas. */
          check_assertion(scope_is(&scope_stack_top()-1,
                                   sck_template_instantiation));
        }  /* if */
        (void)get_token();
        done_with_quals = FALSE;
      } else if (curr_token == tok_static) {
        /* C++23 allows "[]() static { return 42; }". */
        if (state->storage_class == sc_static) {
          pos_error(ec_dupl_decl_specifier, &pos_curr_token);
        } else if (mutable_seen) {
          pos_error(ec_lambda_mutable_and_static, &pos_curr_token);
        } else if (func_info->lambda != NULL &&
                   (func_info->lambda->has_capture_default ||
                    func_info->lambda->capture_list != NULL)) {
          pos_error(ec_static_lambda_with_capture, &pos_curr_token);
        } else {
          state->storage_class = sc_static;
          if (!static_call_operator_enabled) {
            an_error_severity  sev = es_discretionary_error;
            if (gpp_version_is(>=130000) || clang_version_is(>=160000) ||
                ms_version_is(>=1944)) {
              /* Recent versions of GCC, Clang, and MSVC accept the syntax in
                 pre-C++23 modes. */
              sev = es_warning;
            }  /* if */
            pos_diagnostic(sev, ec_static_lambda_nonstandard, &pos_curr_token);
          }  /* if */
        } 
        (void)get_token();
        done_with_quals = FALSE;
      } else if (is_type_qualifier()) {
        /* Type qualifiers are not allowed on lambdas.  If there are, scan
           them and issue a lambda-specific diagnostic. */
        pos_error(ec_type_qualifier_on_lambda, &error_position);
        (void)collect_type_qualifiers(decl_pos_block,
                                      (a_upc_block_size*)NULL);
        done_with_quals = FALSE;
      }  /* if */
    } while (!done_with_quals);
    if (state->storage_class == sc_static) {
      is_nonstatic_member = FALSE;
    } else {
      this_class = parent_type;
      if (!mutable_seen) {
        qualifiers = TQ_CONST;
      }  /* if */
    }  /* if */
  } else if ((is_type_qualifier() or_is_near_or_far() ||
              (microsoft_mode && curr_token == tok_inline)) &&
             rtsp->prototyped) {
    /* In C++ the type of certain member functions may be qualified.  Scan
       for a const or volatile qualifier. */
    a_source_position  qualifier_pos;
    an_error_code      err_code = ec_no_error;
    copy_source_position(pos_curr_token, qualifier_pos);
    qualifiers = collect_type_qualifiers(decl_pos_block,
                                         (a_upc_block_size *)NULL);
    /* When a member function is declared with the restrict qualifier, the
       qualifier attaches to the this pointer, not to *this (as with const
       and volatile). */
    /* If this is not a member function or it is but it is a static member
       function declared within a class definition, a qualifier on the
       function is illegal (ARM 8.2.5).  However, qualifiers on a pointer
       to member function are permitted.  Also, in microsoft mode the
       keyword "inline" is always accepted as a qualifier (a warning that
       it is ignored will have been issued earlier). */
    if (microsoft_mode && qualifiers == TQ_NONE) {
      /* No diagnostic and no need to adjust the type of this or *this. */
    } else if (locator != NULL && locator->is_operator_name &&
               (is_new_operator(locator->variant.opname) ||
                is_delete_operator(locator->variant.opname))) {
      /* Operator new and delete can never be qualified. */
      err_code = ec_function_qualifier_on_new_or_delete;
      qualifier_err = TRUE;
    } else if (parent_type == NULL && !cv_qualifier_with_no_this_class_okay) {
      /* Cv-qualifier is allowed on a member function only. */
      err_code = ec_function_qualifier_on_nonmember;
      qualifier_err = TRUE;
    } else if (!is_nonstatic_member &&
               !cv_qualifier_with_no_this_class_okay &&
               (state->is_inclass_member_function_decl ||
                is_static_constructor)) {
      /* This must be the declaration of a static member function inside its
         class definition (or, possibly, a C++/CLI static constructor outside
         its class).  "const" and "volatile" are not allowed, but with Cfront
         it's sometimes okay (depending on the return type!) so just put out a
         warning in cfront mode. */
      err_code = ec_function_qualifier_on_static_member;
      if (any_cfront_mode() && parent_type != NULL) {
        pos_warning(err_code, &qualifier_pos);
      } else {
        qualifier_err = TRUE;
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (cppcli_enabled && is_nonstatic_member && parent_type != NULL &&
               is_managed_class_type(parent_type)) {
      err_code = ec_qualifier_not_allowed_on_managed_member_function;
      qualifier_err = TRUE;
      this_class = parent_type;
      qualifiers = TQ_NONE;
    } else if (microsoft_mode && is_destructor && qualifiers == TQ_RESTRICT) {
      /* Microsoft compilers allow destructors to be qualified with
         "__restrict".  This affects the signature (i.e., mangling) of the
         destructor. */
      this_class = parent_type;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else if (is_constructor || is_destructor || is_finalizer) {
      /* A qualifier appearing on a constructor, destructor, or finalizer is
         not allowed (ARM 9.3.1 and ECMA-372 19.13.2). */
      err_code = is_finalizer ? ec_function_qualifier_on_finalizer
                              : ec_function_qualifier_on_ctor_or_dtor;
      if (microsoft_mode && is_constructor &&
          !state->is_inclass_member_function_decl) {
        /* Microsoft compilers ignore "__restrict" on out-of-class
           constructors. */
        pos_warning(ec_type_qualifier_ignored_on_constructor, &qualifier_pos);
      } else if (cfront_2_1_mode) {
        /* Cfront 2.1 issues no diagnostic for a qualifier on a constructor
           or destructor. */
        pos_warning(err_code, &qualifier_pos);
      } else {
        qualifier_err = TRUE;
      }  /* if */
      this_class = parent_type;
      qualifiers = TQ_NONE;
    } else {
      this_class = parent_type;
    }  /* if */
    if (qualifier_err && 
        scope_stack[depth_scope_stack].kind !=
                                 (a_scope_kind)sck_template_instantiation) {
      /* The qualifier was not allowed here, but if we're parsing an
         instantiation, the error was already emitted when parsing the
         template declaration. */
      check_assertion(err_code != ec_no_error);
      pos_error(err_code, &qualifier_pos);
    }  /* if */
  }  /* if */
  if (curr_token == tok_ampersand || curr_token == tok_and_and) {
    /* This looks like a ref-qualifier (e.g., "struct S { int f() &; };"). */
    if ((is_nonstatic_member ||
         (!state->is_inclass_member_function_decl && parent_type != NULL) ||
         cv_qualifier_with_no_this_class_okay) &&
        !(is_constructor || is_destructor || is_finalizer) &&
        !(locator != NULL && locator->is_operator_name &&
          (is_new_operator(locator->variant.opname) ||
           is_delete_operator(locator->variant.opname)))) {
      /* Ref-qualifiers are permitted on nonstatic member function declarations
         an on certain type name declarations, but never on declarations of
         constructors, destructors, finalizers, or new/delete operators. */
      if (!ref_qualifiers_enabled) {
        /* Clang accepts ref-qualifiers in all modes, but issues a warning
           in pre-C++11 modes. */
        an_error_severity  sev = clang_mode ? es_warning : es_error;
        pos_diagnostic(sev, ec_ref_qualifier_nonstandard, &pos_curr_token);
      }  /* if */
      ref_qualifiers = (a_ref_qualifier_kind)
                                   (curr_token == tok_ampersand ? rqk_lvalue
                                                                : rqk_rvalue);
    } else if (gpp_mode && state->is_param_decl) {
      /* GCC accepts "void f(int () &&);". */
      pos_warning(ec_ref_qualifier_ignored, &pos_curr_token);
    } else {
      pos_error(ec_ref_qualifier_not_allowed, &pos_curr_token);
      qualifier_err = TRUE;
    }  /* if */
    (void)get_token();
  }  /* if */
  if (!is_nonstatic_member && locator != NULL && locator->is_error &&
      locator->is_class_member) {
    /* Severe errors like
           struct A {};
           A::() const { ... };
       can get us here.  Don't record a "this" type since the resulting
       function isn't really a member of the indicated class type. */
    expect_error();
    this_class = NULL;
    qualifiers = TQ_NONE;
  } else if (is_nonstatic_member && qualifiers == TQ_NONE && !qualifier_err) {
    /* This is a nonstatic member function declared within the definition
       of the class indicated, but without significant qualifiers, or a
       pointer-to-member-function declarator. */
    this_class = parent_type;
  }  /* if */
  if (this_class != NULL) {
    if (this_class->kind == (a_type_kind)tk_template_param) {
      /* Ensure that "this_class" points to a class type. */
      this_class = proxy_class_for_template_param(this_class);
    }  /* if */
    if ((state->dso_flags & DSO_CONSTEXPR) != 0 && top_level &&
        !(is_constructor || is_destructor || is_finalizer)) {
      if (constexpr_implies_const) {
        /* constexpr nonstatic member functions are implicitly "const" in
           C++11 but not C++14 (this does not apply to constructors,
           destructors, and finalizers). */
        if ((qualifiers & TQ_CONST) == 0 && !cpp14_mode) {
          /* Issue a warning in C++11 mode to indicate that the behavior will
             be different in C++14 mode. */
          pos_warning(ec_constexpr_not_const, &state->constexpr_pos);
        }  /* if */
        qualifiers |= TQ_CONST;
      } else if ((qualifiers & TQ_CONST) == 0) {
        /* This member function had been implicitly "const" in C++11 mode
           but is no longer; mark it as having a mangled name that is affected
           by this change. */
        rtsp->had_been_implicitly_const = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  /* The implicit "this" param type will be either "pointer to class-type"
     or, if there was a const qualifier on the function, "pointer to const
     class-type".  However, it is possible to have a cv-qualified function
     type in a typedef declaration or a type-id.  So the qualifiers and the
     class type are encoded separately.  E.g. in
        typedef void CF() const;
     this_class == NULL but qualifiers != TQ_NONE. */
  if (this_class != NULL && !has_explicit_this_parameter(rout_type)) {
    rtsp->this_class = skip_typerefs(this_class);
    rtsp->has_this_param = TRUE;
  }  /* if */
  if (!qualifier_err) {
    /* The traditional "const" and "volatile" function qualifiers really apply
       to the object pointed to by the implied "this" parameter.  The
       "restrict" qualifier, on the other hand, applies to the "this" pointer
       itself, and should therefore not affect the type of the function (just
       as would be the case with a top-level "const" or "volatile" qualifier
       on a parameter). */
    copy_qualifiers(qualifiers & ~TQ_RESTRICT, rtsp->qualifiers);
    copy_qualifiers(qualifiers & TQ_RESTRICT, rtsp->this_qualifiers);
    rtsp->ref_qualifiers = ref_qualifiers;
  }  /* if */
  if (state->is_lambda) {
    /* GCC allows all forms of attributes here; clang allows only GNU
       attributes.  Scan any that exist, but see below for notes on attaching
       them. */
    if (gpp_mode) {
      if (clang_mode) {
        attributes = scan_gnu_attribute_groups(al_post_func);
      } else {
        attributes = scan_attributes(al_post_func);
      }  /* if */
    }  /* if */
  }  /* if */
  esp = scan_exception_specification(state, func_info,
                                     !disallow_exception_spec, top_level);
  check_assertion(esp == NULL || !esp->indeterminate);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cli_or_cx_enabled && esp != NULL && parent_type != NULL &&
      is_managed_class_type(parent_type)) {
    /* Exception specifications are not allowed on members of C++/CLI managed
       class types. */
    pos_error(ec_managed_member_exception_spec, &func_info->throw_position);
    esp = NULL;
  } else if ((microsoft_bugs && microsoft_version < 1300) ||
             (ms_extensions && esp != NULL &&
              !esp->is_noexcept && !esp->throw_any &&
              esp->variant.exception_specification_type_list != NULL)) {
    /* Early Microsoft compilers ignored exception specifications entirely.
       Newer versions ignore all exception specifications except "throw()"
       (which is treated in a nonstandard way), noexcept, and "throw(...)" (a
       nonstandard extension). */
  } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Do not insert code here. */
  {
    rtsp->exception_specification = esp;
  }  /* if */
  /* Scan any attributes now, but delay attaching them until we know if a
     trailing return type follows. */
  attributes = composite_attributes(attributes, scan_attributes(al_post_func));
  if (curr_token == tok_arrow &&
      (trailing_return_types_enabled || state->is_lambda ||
       state->is_deduction_guide)) {
    /* A trailing return type. */
    scan_trailing_return_type(state, rout_type);
  } else {
    state->return_type_pos = state->specifiers_pos;
  }  /* if */
  if (attributes != NULL) {
    /* Make the scanned attributes available to the next call of
       scan_attributes, which will occur in scan_declarator_attributes. */
    check_assertion(!unscanned_attributes_pending());
    unscan_attributes(attributes);
  }  /* if */
}  /* cplusplus_function_declarator_trailer */


static void set_early_member_function_decl_flags(a_decl_parse_state  *dps)
/*
This routine is called when parsing a top-level function declarator (before
the associated function prototype scope is pushed).  Determine whether this
declarator is for the declaration (in-class or out-of-class) of a member
function and update the corresponding flags in *dps.
*/
{
  a_scope_stack_entry  *ssep = &scope_stack_top();
  a_boolean            instance = scope_is(ssep, sck_template_instantiation);

  if (ssep->kind == (a_scope_kind)sck_template_declaration || instance) {
    --ssep;
  }  /* if */
  if (scope_is(ssep, sck_class_struct_union) ||
      /* If an instantiation scope (for a function declaration) sits on top of
         a class reactivation scope, we are presumably rescanning a function
         member declared inside a class. */
      (instance && scope_is(ssep, sck_class_reactivation)) ||
      /* In Microsoft mode, a selective overrider will have added a class
         reactivation scope. */
      (microsoft_mode && scope_is(ssep, sck_class_reactivation) &&
       scope_is(ssep-1, sck_class_struct_union))) {
    dps->is_inclass_member_function_decl = (dps->dso_flags & DSO_FRIEND) == 0;
  } else if (scope_is(ssep, sck_class_reactivation)) {
    /* In some error cases the flag here is set to TRUE even though there
       is no member function corresponding to the signature that is being
       parsed. */
    dps->is_out_of_class_member_function_decl = TRUE;
  }  /* if */
}  /* set_early_member_function_decl_flags */


void report_incomplete_function_return_type(a_type_ptr         return_type,
                                            a_source_position  *pos,
                                            a_routine_ptr      rp)
/*
Report an error at the given position for a function being called or defined
with the given incomplete return type.  rp is the called function if it is
known; otherwise it is NULL.
*/
{
  if (rp != NULL) {
    /* We know the routine that is being defined or called. */
    pos_syty_error(ec_incomplete_function_return_type, pos, symbol_for(rp),
                   return_type);
  } else {
    /* The actual function being called is not available, presumably because
       it is called through a pointer-to-function variable. */
    pos_ty_error(ec_incomplete_return_type, pos, return_type);
  }  /* if */
}  /* report_incomplete_function_return_type */


static void check_c_mode_ellipsis(a_decl_parse_state_ptr  dps)
/*
dps describes a declaration with a function declarator that starts with an
ellipsis in C mode, which is ordinarily invalid.  However, in Clang C mode,
this should be accepted if the "overloadable" attribute is specified for a
corresponding function declaration.  We therefore delayed the check until
now (that the attributes have been applied): Issue an error unless this is a
declaration of a function with the "overloadable" attribute.
*/
{
  a_boolean attribute_found = FALSE;

  if (dps->sym != NULL && is_simple_function_symbol(dps->sym)) {
     a_routine_ptr    rp = func_sym_routine(dps->sym);
     an_attribute_ptr ap = find_attribute(ak_overloadable,
                                          rp->source_corresp.attributes);
     if (ap != NULL) {
       attribute_found = TRUE;
     }  /* if */
  }  /* if */
  if (!attribute_found) {
    pos_error(ec_nonstd_ellipsis_only_param, &(dps->declarator_pos));
  }  /* if */
}  /* check_c_mode_ellipsis */


static void diag_unprototyped_func_declarator(a_decl_parse_state_ptr  dps)
/*
dps is associated with a non-prototyped function declarator.  Issue a remark
about it, unless this is for a function definition that was previously declared
with a prototype.
*/
{
  a_boolean  issue_remark = TRUE;

  if (dps->sym != NULL && is_simple_function_symbol(dps->sym) &&
      dps->is_definition) {
    a_type_ptr  rtp = routine_symbol_type(dps->sym);
    if (rout_type_supp(rtp)->prototyped) {
      issue_remark = FALSE;
    }  /* if */
  }  /* if */
  if (issue_remark) {
    pos_remark(ec_use_of_non_prototype_func_declarator, &dps->declarator_pos);
  }  /* if */
}  /* diag_unprototyped_func_declarator */


void function_declarator(a_decl_parse_state  *state,
                         a_decl_flag_set     di_flags,
                         a_type_ptr          *new_type_ptr,
                         a_func_info_block   *func_info,
                         a_symbol_locator    *locator,
                         a_type_ptr          parent_type,
                         a_boolean           is_nonstatic_member,
                         a_boolean           is_constructor,
                         a_boolean           is_static_constructor,
                         a_boolean           is_destructor,
                         a_boolean           is_finalizer,
                         a_boolean           disallow_default_args,
                         a_boolean           disallow_exception_spec,
                         a_decl_pos_block    *decl_pos_block)
/*
Scan a function declarator, or an array declarator in an abstract declarator.
Allocate and return in *new_type_ptr an appropriate function type.  The
initial opening parenthesis has already been checked and passed over (which is
unusual; that's necessary because of the syntactic strangeness of abstract
declarators).  *state and di_flags describe the declaration being parsed.
If func_info is NULL, then the function declarator is not for the top-level
type or this is an abstract declarator (and therefore certain forms are
disallowed); otherwise, extra information about the function declarator is
returned in *func_info.  For member functions, parent_type is a pointer to the
class (or struct or union) type of which it is a member; otherwise it is NULL.
When parent_type is non-NULL, is_nonstatic_member distinguishes static from
nonstatic member functions when the current scope is that of a class
definition.  is_constructor, is_static_constructor, is_destructor, and
is_finalizer indicate that previous processing determined that this is a
constructor, a C++/CLI static constructor, a destructor, or a C++/CLI
finalizer declaration, respectively.  If disallow_default_args is TRUE issue
an error if a default argument expression is encountered.
*/
{
  a_storage_class         param_storage_class;
  a_type_ptr              tp;
  a_decl_flag_set         dso_flags;
  a_param_type_ptr        last_param_type;
  a_param_id_ptr          last_param_id;
  a_symbol_locator        param_locator;
  a_boolean               done = FALSE, any_params;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_boolean               param_array_next = FALSE;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_boolean               param_array_ellipsis = FALSE;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_source_position       start_pos, param_type_pos, ellipsis_pos;
  a_routine_type_supplement_ptr
                          extra_info;
  a_boolean               dangling_type_specifier = FALSE;
  a_boolean               defines_something;
  a_boolean               declarator_allows_default_args = FALSE;
  a_boolean               ignore_disallowed_default_arg = FALSE;
  a_boolean               any_default_args = FALSE;
  a_source_position       last_default_arg_pos;
  a_boolean               may_be_copy_constructor = FALSE;
  a_boolean               bad_first_param_for_copy_constructor = FALSE;
  a_source_position       pos_of_first_param_type;
  a_func_info_block       local_func_info_block;
  a_boolean               is_top_level_declarator = TRUE;
  a_boolean               microsoft_C_leading_ellipsis = FALSE;
  a_boolean               must_pop_function_prototype_scope = FALSE;
  a_boolean               is_typedef_decl =
                                  (di_flags & DI_IS_TYPEDEF_DECLARATION) != 0;
  a_boolean               is_friend_decl = (di_flags & DI_IS_FRIEND_DECL) != 0;
  a_boolean               must_adjust_param_type_qualifiers = FALSE;

  db_enter(3, "function_declarator");
  copy_source_position(pos_curr_token, start_pos);
  set_err_pos_to_curr_token();
  add_stop_token(tok_rparen);
  /* If the caller passed in a func_info pointer, this is the declarator of
     a "top-level" function declaration.  Use the storage passed in by the
     caller.  But if func_info is NULL, use a local func info block.  This
     is mainly useful for managing param_id entries properly. */
  if (func_info == NULL) {
    clear_func_info(&local_func_info_block);
    func_info = &local_func_info_block;
    is_top_level_declarator = FALSE;
  } else {
    /* A top-level declarator. */
    if (!C_mode()) {
      /* Set some flags in the declaration parsing state block indicating
         whether this is the declaration of a member function. */
      set_early_member_function_decl_flags(state);
    }  /* if */
  }  /* if */
  last_param_id = NULL;
  *new_type_ptr = alloc_type((a_type_kind)tk_routine);
  extra_info = (*new_type_ptr)->variant.routine.extra_info;
  if (is_constructor) extra_info->assoc_routine_is_ctor = TRUE;
  if (is_destructor) extra_info->assoc_routine_is_dtor = TRUE;
  extra_info->param_type_list = NULL;
  /* Copy the current name linkage into the routine type.  It will not
     necessarily correspond to the name linkage of the routine (if any) with
     which this type is associated. */
  extra_info->routine_name_linkage =
                          scope_stack[depth_scope_stack].default_name_linkage;
  if (!is_name_linkage_kind_for_rout_type(extra_info->routine_name_linkage)) {
    /* Custom name linkage kinds may presumably not affect routine types
       (i.e., calling conventions). */
    /*lint -e{587,650,685}*/
#if CHECKING
    if (extra_info->routine_name_linkage == nlk_none) {
      expect_error();
    } else {
      check_assertion(!C_mode() &&
                      extra_info->routine_name_linkage > nlk_last_standard);
    }  /* if */
#endif /* CHECKING */
    extra_info->routine_name_linkage =
                                  (a_name_linkage_kind)nlk_cplusplus_external;
  }  /* if */
  if (scope_stack[depth_scope_stack].name_linkage_is_explicit) {
    extra_info->routine_name_linkage_is_explicit = TRUE;
  }  /* if */
#if CHECKING
  if (extra_info->routine_name_linkage == (a_name_linkage_kind)nlk_none ||
      extra_info->routine_name_linkage == (a_name_linkage_kind)nlk_internal) {
    unexpected_condition_str2("function_declarator:",
                              "bad default name linkage kind");
  }  /* if */
#endif /* CHECKING */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_mode && C_mode() && curr_token == tok_comma &&
      next_token() == tok_ellipsis) {
    /* Microsoft C accepts "(,...)" as a way to request an arbitrary
       set of call arguments.  Skip the comma so the ellipsis is seen
       below. */
    (void)get_token();
    microsoft_C_leading_ellipsis = TRUE;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (curr_token == tok_rparen) {
    if (require_func_prototypes) {
      /* In C++ f() is equivalent to f(void).  Leave param_type_list empty.
         C23 (and later) behaves the same way by default. */
      extra_info->prototyped = TRUE;
    } else {
      /* In earlier versions of C, f() is an old-style empty parameter list. */
      extra_info->prototyped = FALSE;
    }  /* if */
    any_params = FALSE;
  } else if (curr_token == tok_ellipsis &&
             (!C_mode() || allow_ellipsis_only_param_in_C_mode ||
              microsoft_C_leading_ellipsis || clang_mode)) {
    /* The first thing in the parameter list is an ellipsis. */
    ellipsis_pos = pos_curr_token;
    /* Advance past the ellipsis. */
    (void)get_token();
    if (is_destructor) {
      /* Destructors are allowed no arguments. */
      pos_error(ec_too_many_params_for_destructor, &ellipsis_pos);
      any_params = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (is_finalizer) {
      /* Finalizers are allowed no arguments. */
      pos_error(ec_too_many_params_for_finalizer, &error_position);
      any_params = FALSE;
    } else if (cppcli_enabled && is_type_start(/*is_expr_context=*/FALSE)) {
      /* Presumably a C++/CLI parameter array (not allowed in C++/CX). */
      param_array_next = TRUE;
      any_params = TRUE;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      param_array_ellipsis = TRUE;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else {
      /* In C++ f(...) is standard.  In C it's supported (as an extension
         or in standard C23) when allow_ellipsis_only_param_in_C_mode is
         TRUE. */
      extra_info->has_ellipsis = TRUE;
      any_params = FALSE;
#if ASM_FUNCTION_ALLOWED
      if (func_info->is_asm_function) {
        pos_error(ec_bad_asm_func_ellipsis, &ellipsis_pos);
      } else
#endif /* ASM_FUNCTION_ALLOWED */
      /* Do not insert code here. */
      if (C_mode()) {
        if (clang_mode && !allow_ellipsis_only_param_in_C_mode) {
          /* Clang allows ellipsis and no named parameters in an overloadable
             function.  Register an end of parse action to ensure this is
             an overloadable function. */
          add_end_of_parse_action(check_c_mode_ellipsis, state,
                                      /*secondary_decls=*/TRUE);
        } else if (strict_ansi_mode && !c23_mode) {
          /* Issue a diagnostic on use of a nonstandard feature. */
          pos_diagnostic(strict_ansi_error_severity,
                         ec_nonstd_ellipsis_only_param, &ellipsis_pos);
        }  /* if */
      }  /* if */
    }  /* if */
    /* An ellipsis only occurs in prototyped param lists. */
    extra_info->prototyped = TRUE;
  } else {
    /* Determine whether this is an old-style list of identifiers or
       a prototyped parameter list. */
    if (!C_mode() && (!allow_anachronisms || parent_type != NULL)) {
      /* If this is a C++ member function, it must be prototyped.  If
         anachronism support is not the default or was not explicitly
         requested, always parse the declaration as a prototyped param list
         -- this will produce better error messages in certain cases (even
         though it will produce poor error recovery if it actually *is* an
         old-style list). */
      extra_info->prototyped = TRUE;
    } else {
      /* Not a member function -- examine the first token. */
      extra_info->prototyped = is_prototyped_parameter_list_start();
    }  /* if */
    any_params = TRUE;
  }  /* if */
  if (extra_info->prototyped) {
    /* ANSI function prototype, as in "int f(int a, char *b)" or
       "int f(int, char *)". */
    a_pack_expansion_stack_entry_ptr  pesep = NULL;
    a_pack_expansion_descr_ptr        pedp = NULL;
    a_boolean                         any_variadic_params = FALSE;
    uint32_t                          param_number = 0;
    if (any_params && !disallow_default_args) {
      /* In C++ mode a default argument may be declared with the parameter
         unless the function is a user-defined overloaded operator or a
         user-defined conversion.  The exceptions are the call operator and
         the C++23 subscript operator (both tested with opname_is_call_like).
         Note that locator may be NULL (e.g., with abstract declarators).
         Deduction guides can also have default arguments. */
      /* operator new(), new[](), delete(), and delete[]() can also take
         default arguments in the second and successive arguments -- this
         is implied by ARM 13.4, which excludes those operators from the
         restrictions that are listed for overloaded operators in general.
         We don't set the flag till after the first parameter has been seen,
         however; see below.  The test for template-ids is used to disallow
         default arguments on friend declarations that refer to an explicit
         template instance through the use of an explicit template argument
         list. */
      if (locator != NULL && !locator->is_conversion_name &&
          (!locator->is_template_id ||
           (di_flags & DI_IS_EXPLICIT_INSTANTIATION) != 0) &&
          (!locator->is_operator_name ||
           opname_is_call_like(locator->variant.opname))) {
        declarator_allows_default_args = TRUE;
      } else if ((di_flags & DI_IS_DEDUCTION_GUIDE) != 0) {
        declarator_allows_default_args = TRUE;
      }  /* if */
    }  /* if */
    /* Push a function prototype scope for the parameters. */
    (void)push_scope((a_scope_kind)sck_func_prototype, NO_SCOPE_NUMBER,
                     *new_type_ptr, (a_routine_ptr)NULL);
    scope_stack_top().decl_parse_state = state;
    must_pop_function_prototype_scope = !state->for_requires_expr_params;
    if (is_constructor && is_template_dependent_context()) {
      /* A constructor in a template-dependent context may be used for
         deduction even if it isn't a template itself, because of C++17
         template argument deduction. */
      scope_stack_top().in_template_deduction_context = TRUE;
    }  /* if */
    /* Remember the scope number for later use if and when a body appears. */
    func_info->scope_number = scope_stack[depth_scope_stack].number;
    if (any_params) {
      /* If there appear to be parameters, check for empty function parameter
         packs. */
      for (;;) {
        a_symbol_locator  loc;
        any_params = begin_potential_pack_expansion_context_full(
                                                &pesep, &pedp,
                                                /*is_lookahead=*/FALSE,
                                                /*allow_empty_list=*/FALSE,
                                                /*ignore_suppression=*/FALSE,
                                                /*claim_pack_index=*/FALSE);
        if (any_params) break;
        /* An empty pack expansions: Create a dummy parameter id entry in case
           nested prototype instantiations refer to it. */
        if (pedp != NULL && pedp->param_symbol_header != NULL) {
          clear_locator(&loc, &pos_curr_token);
          loc.symbol_header = pedp->param_symbol_header;
          add_to_param_id_list(&loc, type_of_unknown_templ_param_nontype,
                               &pos_curr_token, sc_auto, func_info,
                               (a_source_sequence_entry*)NULL,
                               &last_param_id, /*is_pack_element=*/FALSE);
          last_param_id->is_empty_pack_parameter = TRUE;
          last_param_id->declared_type = type_of_unknown_templ_param_nontype;
          last_param_id->param_num = param_number;
        }  /* if */
        param_number += 1;
        if (curr_token == tok_ellipsis) {
          /* An empty pack expansion immediately followed by an ellipsis: This
             is similar to "(...)". */
          (void)get_token();
          extra_info->has_ellipsis = TRUE;
          break;
        } else if (curr_token == tok_rparen) {
          /* Nothing follows the empty pack expansion. */
          break;
        } else {
          /* Continue checking for additional empty expansions. */
        }  /* if */
      }  /* for */
      any_variadic_params = any_params;
    }  /* if */
    if (any_params) {
      last_param_type = NULL;
      do {
        a_decl_parse_state   param_state;
        a_decl_pos_block     local_decl_pos_block;
        a_decl_flag_set      dsi_flags = DSI_STORAGE_CLASS_SPECIFIER_ALLOWED |
                                         DSI_TYPE_SPECIFIER_ALLOWED |
                                         DSI_IS_PARAMETER |
                                         DSI_CHECK_FOR_DANGLING_TYPE_SPECIFIER;
        a_param_type_ptr     ptp;
        a_type_qualifier_set param_qualifiers = TQ_NONE;
        a_boolean            is_pack_element;
        a_boolean	     is_non_initial_pack_element;
        a_boolean            default_arg_allowed_on_curr_param =
                                                declarator_allows_default_args;
        a_boolean            check_no_default_arg_okay = FALSE;
        /* Mark the start of the parameter declaration as the start of a
           potential variadic pack expansion. */
        is_pack_element = pesep != NULL && pesep->instantiation_descr != NULL;
        is_non_initial_pack_element = is_pack_element &&
                                      is_non_initial_variadic_element();
        /* Start the following flag with value TRUE.  It will be set to FALSE
           if the declaration includes a reference to a local (non-enclosing)
           template pack. */
        state->param_with_only_enclosing_pack_refs = TRUE;
        /* Count the number of parameters encountered.  All elements of a given
           parameter pack are given the same parameter number. */
        if (!is_non_initial_pack_element) param_number++;
        /* In a real instantiation, the tokens of the parameter will have been
           skipped. */
        if (pesep != NULL && !any_variadic_params) continue;
        if (std_attributes_enabled)  dsi_flags |= DSI_STD_ATTRIBUTES_ALLOWED;
        if (gnu_attributes_enabled) dsi_flags |= DSI_GNU_ATTRIBUTES_ALLOWED;
        if (ms_extensions) dsi_flags |= DSI_MICROSOFT_ATTRIBUTES_ALLOWED;
        add_stop_token(tok_comma);
        init_decl_parse_state(&param_state);
        param_state.is_param_decl = TRUE;
        param_state.is_top_level_param_decl = is_top_level_declarator;
        param_state.is_pack_element = is_pack_element;
        param_state.assoc_func_decl_state = state;
        param_state.auto_type_allowed = is_top_level_declarator &&
                                        !state->is_explicit_specialization &&
                                        !state->is_explicit_instantiation &&
                                        !state->for_requires_expr_params &&
                                        (abbr_func_templates_enabled ||
                                         (generic_lambdas_enabled &&
                                          state->is_lambda));
        param_state.trailing_return_type_allowed =
                                                trailing_return_types_enabled;
        param_state.pack_ellipsis_allowed = is_variadic_template_context();
        param_state.is_implicit_type_context =
                             (!is_top_level_declarator &&
                              state->is_implicit_type_context) ||
                              parent_type != NULL || is_friend_decl ||
                              state->for_requires_expr_params ||
                              (locator != NULL && locator->is_qualified_name);
        copy_source_position(pos_curr_token, param_type_pos);
        clear_decl_pos_block(&local_decl_pos_block);
        /* Scan prefix attributes. */
        param_state.prefix_attributes = scan_attributes(al_prefix);
        /* Scan the specifiers of a parameter-declaration. */
        decl_specifiers(dsi_flags, &param_state, &local_decl_pos_block);
        dso_flags = param_state.dso_flags;
        if (param_state.is_explicit_this) {
          /* An explicit "this" parameter makes a member function "static". */
          is_nonstatic_member = FALSE;
        }  /* if */
        param_storage_class = param_state.declared_storage_class;
        dangling_type_specifier =
                               (dso_flags & DSO_DANGLING_TYPE_SPECIFIER) != 0;
        defines_something = (dso_flags & DSO_DEFINES_SOMETHING) != 0;
        if (param_state.variant.auto_params != NULL && pesep != NULL) {
          /* If we encountered an "auto" parameter not yet associated with a
             synthesized template parameter, do not issue an error about a
             function parameter pack declaration not referencing a template
             parameter pack. */
          pesep->expansion_with_no_packs_diagnostic_issued = TRUE;
        }  /* if */
        if (last_param_type == NULL && curr_token == tok_rparen) {
          if (dso_flags & DSO_JUST_VOID) {
            /* The first and only parameter-declaration is just "void", which
               has a special meaning (no parameters).  */
            if (param_state.template_void_specifier) {
              /* In Microsoft mode, (T) can be a valid (void). */
              pos_warning(ec_nonstd_template_void_param_list, &param_type_pos);
              check_assertion(microsoft_mode);
            }  /* if */
            remove_stop_token(tok_comma);
            abandon_potential_pack_expansion_context(pesep);
            pesep = NULL;
            if (param_state.prefix_attributes != NULL) {
              /* There is no parameter to apply the attributes to.  Create a
                 dummy parameter and attempt to apply the attribute to that.
                 This will give a diagnostic if one is necessary (but won't
                 for cases where the syntax allows the attribute even if it
                 has no effect, e.g., "[[deprecated]] void"). */
              ptp = make_param_type(error_type(), &param_type_pos);
              attach_param_attributes(&param_state, ptp);
            }  /* if */
            break;
          } else if (is_void_type(param_state.type) &&
                     (c99_mode || cpp11_mode ||
                      !is_qualified_type(param_state.type)) &&
                     param_storage_class == (a_storage_class)sc_unspecified) {
            /* A type name is bound to void type -- this construct is treated
               as a (possibly nonstandard) way of signifying an empty param
               list.  In C99 and C++11 modes, this is a standard form and no
               diagnostic is needed (unless the void type is qualified; in
               that case an error is issued below).  Otherwise, issue an error
               (in strict mode) or a warning. */
            if ((is_template_dependent_context() ||
                 is_nonspecialized_instantiation_context()) &&
                !microsoft_mode) {
              /* We usually don't accept such constructs in template contexts,
                 because it could cause the number of parameters seen in the
                 template to differ from the number seen during instantiation.
                 An exception occurs in Microsoft mode, where a function
                 declarator with a single parameter type that instantiates to
                 "void" is treated as a function taking no parameters.  In the
                 usual case of a template parameter instantiated with "void"
                 the call to decl_specifiers produced the DSO_JUST_VOID flag,
                 which is handled above.  However, cases involving a typedef
                 top of the template parameter are handled below. */
              pos_error(ec_void_param_not_allowed, &param_type_pos);
              invalidate_type(&param_state);
            } else {
              a_boolean  qualified_void = is_qualified_type(param_state.type);
              if (param_state.decl_specifiers_error) {
                /* An error already occurred during the call to
                   decl_specifiers.  Don't issue another one. */
                check_assertion(is_at_least_one_error());
              } else if (!c99_mode && !cpp11_mode) {
                an_error_severity  sev = es_warning;
                if (qualified_void) {
                  sev = es_error;
                } else if (strict_ansi_mode) {
                  sev = strict_ansi_discretionary_severity;
                }  /* if */
                pos_diagnostic(sev, ec_nonstd_void_param_list,
                               &param_type_pos);
              } else if (qualified_void) {
                pos_error(ec_nonstd_qualified_void_param_list,
                          &param_type_pos);
              }  /* if */
              remove_stop_token(tok_comma);
              abandon_potential_pack_expansion_context(pesep);
              pesep = NULL;
              break;
            }  /* if */
          }  /* if */
        }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (cli_or_cx_enabled) {
          /* Check for the presence of a [ParamArray] attribute. */
          an_ms_attribute_ptr  msap = param_state.ms_attributes;
          for (; msap != NULL; msap = msap->next) {
            if (msap->kind == (an_ms_attribute_kind)msak_custom &&
                is_cli_type_of_kind(msap->variant.custom_info.type,
                                    csk_system_param_array_attribute)) {
              param_array_next = TRUE;
            }  /* if */
          }  /* for */
          param_state.no_special_cli_class_type_check = param_array_next;
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        if (is_destructor && last_param_type == NULL) {
          /* Destructors are allowed no arguments.  Issue an error on the
             first parameter. */
          pos_error(ec_too_many_params_for_destructor, &error_position);
        }  else if (is_finalizer && last_param_type == NULL) {
          /* Finalizers are allowed no arguments.  Issue an error on the
             first parameter. */
          pos_error(ec_too_many_params_for_finalizer, &error_position);
        }  /* if */
        if (defines_something && C_dialect == C_dialect_cplusplus) {
          pos_error(ec_type_definition_not_allowed, &param_type_pos);
          invalidate_type(&param_state);
        } else if (!(dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
          /* No type specifier (aside from const or volatile) appeared among
             the declaration specifiers.  Issue a diagnostic. */
          report_implicit_int(&pos_curr_token, param_state.type);
        } else {
          /* Mark the type as referenced.  This is important for a
             parameter declaration like "struct s {int a;} p;" --
             the structure is referenced (because it's the type of "p")
             even through the struct is not referenced by name.  This is
             usually redundant, since the type is also marked as
             referenced in declarator.  But if declarator is not called,
             we still consider this use of the type as a reference, since
             it is incorporated into the definition of the function. */
          (skip_typerefs(param_state.type))->source_corresp.referenced = TRUE;
        }  /* if */
        /* Scan an optional declarator or abstract declarator.  Don't bother
           looking for a declarator when decl_specifiers has found a badly
           formed type specifier.  If an error is to be put out, that's done
           later. */
        if ((!dangling_type_specifier &&
             is_abstract_or_real_declarator_start()) ||
             /* Check for a parameter pack declaration like "P ...". This is
                not handled by the is_abstract_or_real_declarator_start macro
                because the specifiers type may be required for
                disambiguation: If the next two tokens are "... )", the
                ellipsis declares a parameter pack only if the specifiers type
                is a pattern type (i.e., contains an unexpanded template
                parameter pack) or if we are in an instantiation of a pack
                element.  During the tentative parsing of an abbreviated
                function template (or generic lambda), an "auto" specifier also
                causes the ellipsis to be treated as introducing a pack. */
             (variadic_templates_enabled &&
              ((curr_token == tok_ellipsis &&
                (next_token() != tok_rparen || any_packs_referenced() ||
                 param_state.variant.auto_params != NULL)) ||
               is_pack_element))) {
          a_decl_flag_set  param_di_flags = DI_IS_PARAMETER_DECL |
                                            DI_REAL_DECLARATOR_ALLOWED |
                                            DI_ABSTRACT_DECLARATOR_ALLOWED;
          if (is_typedef_decl) {
            /* At the top level this is a typedef declaration. */
            param_di_flags |= DI_IS_TYPEDEF_DECLARATION;
          }  /* if */
          if (vla_enabled && C_mode()) {
            /* Permit a variable length array declaration in C modes that
               alloc VLAs (e.g., C99 mode).  While we can accept VLAs in some
               C++ modes, we cannot accept them in parameters: It causes
               problems wrt. name mangling, for example.  This restriction is
               also imposed by GNU C++ compilers (the other known C++ compiler
               accepting VLAs). */
            param_di_flags |= DI_VLA_ALLOWED | DI_VLA_ASTERISK_ALLOWED;
          }  /* if */
          declarator(param_di_flags, &param_state, 
                     /*member_parent_type=*/(a_type_ptr)NULL, &param_locator,
                     (a_func_info_block_ptr)NULL, &local_decl_pos_block);
#if RECORD_HIDDEN_NAMES_IN_IL
          if (!C_mode() && param_locator.symbol_header != NULL) {
            /* In C++, parameter names may hide names from surrounding
               scopes used in subsequent parameter declarations. */
            check_name_hiding_by_parameter(&param_locator);
          }  /* if */
#endif /* RECORD_HIDDEN_NAMES_IN_IL */
        } else {
          /* No declarator. */
          param_state.declarator_pos = pos_curr_token;
          set_to_error_locator(param_locator);
          check_pending_qualifiers_used(&param_state);
#if MICROSOFT_EXTENSIONS_ALLOWED
          if (cli_or_cx_enabled &&
              !param_state.no_special_cli_class_type_check &&
              !check_invalid_use_of_special_cli_class_type(
                              param_state.type, &param_state.specifiers_pos)) {
            /* Some special C++/CLI class types (e.g., delegates) are invalid
               at this point. */
            param_state.type = error_type();
          }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        }  /* if */
        if (!C_mode()) {
          /* Check that the type is legal, and do required adjustments. */
          check_use_of_placeholder_type(&param_state);
        }  /* if */
        check_and_adjust_parameter_type(&param_state, &param_type_pos);
        /* Standardize the storage class: unspecified becomes auto. */
        if (param_storage_class == (a_storage_class)sc_unspecified) {
          param_storage_class = (a_storage_class)sc_auto;
        }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
        /* Make adjustments on the param source sequence entry before it is
           bound to the param_id entry. */
        if (!is_top_level_declarator || is_template_dependent_context()) {
          /* If a parameter id was specified in a non-top-level function
             declarator, a source sequence entry created for it is useless.
             In certain configurations source sequence entries are put out
             during prototype instantiation of class templates -- but since
             the function body won't be scanned at this time, the source
             sequence entry for the param id should be eliminated in that
             case, too. */
          remove_declarator_sse(&param_state, depth_scope_stack);
        } else if (param_state.source_sequence_entry == NULL) {
          /* Declarator was not called or a source sequence entry was not
             created for some some other reason.  Still, if this turns out to
             be a function definition, it will be needed (in C++ unnamed
             parameters are allowed). */
#if DEBUG
/* GCOVR_EXCL_START */
          if (!source_sequence_entries_disallowed &&
              (debug_level >= 4 || db_flag_is_set("dump_ss_full"))) {
            if (!is_error_locator(param_locator)) {
              fprintf(f_debug,
                     "function_declarator: empty ss entry for param \"%s\":\n",
                      param_locator.symbol_header->identifier);
            }  /* if */
          }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
          param_state.source_sequence_entry =
                                            add_empty_source_sequence_entry();
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        if (is_error_locator(param_locator)) {
          /* There was no declarator. */
          func_info->any_prototype_names_omitted = TRUE;
        }  /* if */
        if (remove_qualifiers_from_param_types) {
          /* Move top-level type qualifiers to a separate field of *ptp.  They
             are not part of the type signature of a C++ function.  However,
             because they do belong to the type of the parameter variable,
             they will be added back before calling add_to_param_id_list. */
          /* Note: whether to remove top-level qualifiers is sensitive to the
             ABI version because qualifiers are reflected in mangled names. */
          check_assertion(!C_mode());
          param_qualifiers = get_top_level_type_qualifiers(param_state.type);
          param_state.type = make_unqualified_type(param_state.type);
          if (param_qualifiers & TQ_C11_ATOMIC) {
            param_state.type = make_qualified_type(param_state.type,
                                                   TQ_C11_ATOMIC);
          }  /* if */
          if ((param_qualifiers & TQ_RESTRICT) != 0 &&
              keep_restrict_in_signatures) {
            param_state.type = make_qualified_type(param_state.type,
                                                   TQ_RESTRICT);
          }  /* if */
        }  /* if */
        /* Create a param-type entry and add it to the list of param-types
           associated with the routine type. */
        ptp = make_param_type(param_state.type, &param_type_pos);
        ptp->declared_type = param_state.declared_type;
        copy_qualifiers(param_qualifiers, ptp->qualifiers);
        if (param_state.is_explicit_this) {
          /* If the parameter is the explicit object ("this") parameter, mark
             it as such. */
          ptp->is_explicit_this = TRUE;
        }  /* if */
        if (param_state.has_pack_ellipsis && is_template_dependent_context() &&
            !is_pack_element) {
          /* This looks like the declaration of a function parameter pack.
             If it is a real pack (for a function template), default arguments
             are not permitted.  However, if it is a pack in an ordinary member
             of a class template, then default arguments are okay. */
          if (scope_is(&scope_stack_top()-1, sck_template_declaration) ||
              state->is_lambda) {
            default_arg_allowed_on_curr_param = FALSE;
          }  /* if */
        } else {
          if (is_non_initial_pack_element) {
            ptp->duplicate_name = TRUE;
          } else if (param_state.has_pack_ellipsis &&
                     !(di_flags & DI_ABSTRACT_DECLARATOR_ALLOWED) &&
                     is_template_dependent_context()) {
            /* Consider a case like the following:
                 template<class ... Ts> struct S {
                   template<class F> auto m(F f, Ts... p)->decltype(f(p...));
                 };
               During a real instantiation of S, any number of parameters p
               may be created, each marked as a "pack element".  However, when
               "p..." is rescanned in the return type, we'll need to know that
               p was a parameter pack (to validate the ellipsis and set up
               another context for expansion).  We therefore mark the first
               parameter of the expansion as a "parameter pack".  Don't do this
               if the function declarator could be an abstract declarator
               (e.g., a type-id or parameter declaration) and therefore not a
               top-level declaration. */
            ptp->is_parameter_pack =
                                   !state->param_with_only_enclosing_pack_refs;
          } else if (param_state.has_pack_ellipsis &&
                     scope_stack_top().in_nonreal_instantiation &&
                     !scope_stack_top().is_rescan) {
            /* In nonreal instantiations (other than rescan contexts), we also
               may need to record that a pack element is a pack instead.
               For example:
                 template<typename> struct B;
                 template<typename R, typename ... Ps> struct X {
                   template<typename> using A = B<R(Ps...)>;
                   template<typename F>
                     X(F&&) noexcept(A<F>::template f<F>()) {}
                 };
               Here, scanning the noexcept argument will substitute A<F>, which
               must produce B<R(Ps...)> and not just B<R(Ps)>.
            */
            a_boolean  is_pack = FALSE;
            a_pack_instantiation_descr_ptr
                       pidp = pack_expansion_stack->instantiation_descr;
            for (; !is_pack && pidp != NULL; pidp = pidp->next) {
              is_pack = symbol_is_pack(pidp->pack_status->symbol);
            }  /* for */
            ptp->is_parameter_pack = is_pack;
            is_pack_element = !is_pack;
          }  /* if */
          ptp->is_pack_element = is_pack_element;
          if (is_pack_element &&
              scope_is(&scope_stack_top()-1, sck_template_instantiation)) {
            default_arg_allowed_on_curr_param = FALSE;
          }  /* if */
        }  /* if */
        if (param_state.variant.auto_params != NULL &&
            param_state.has_pack_ellipsis) {
          /* A variadic "auto" parameter in the initial scan. */
          param_state.variant.auto_params->is_parameter_pack = TRUE;
        }  /* if */
        if (param_state.has_deduced_type) {
          ptp->is_auto_param = TRUE;
        }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (param_state.ms_attributes != NULL) {
          apply_microsoft_attributes(&param_state.ms_attributes, (char*)ptp,
                                     iek_param_type,
                                     (an_ms_attribute_target)msat_parameter,
                                     (an_ms_attribute_target)msat_parameter);
        }  /* if */
        if (param_array_next) {
          ptp->is_cli_param_array = TRUE;
          (void)check_param_array_type(ptp, &param_state.specifiers_pos);
#if EXTRA_SOURCE_POSITIONS_IN_IL
          if (param_array_ellipsis) {
            local_decl_pos_block.specifiers_range.start = ellipsis_pos;
          }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        if (!is_error_locator(param_locator)) {
          ptp->name = param_locator.symbol_header->identifier;
        } else if (state->for_requires_expr_params) {
          pos_warning(ec_unnamed_require_expr_param,
                      &param_state.declarator_pos);
        }  /* if */
        attach_param_attributes(&param_state, ptp);
#if EXTRA_SOURCE_POSITIONS_IN_IL
        {
        /* Update source range information in the param-type entry. */
        a_decl_position_supplement_ptr  dpsp;
        dpsp = alloc_decl_position_supplement(in_file_scope(ptp));
        dpsp->identifier_range = local_decl_pos_block.identifier_range;
        dpsp->specifiers_range = local_decl_pos_block.specifiers_range;
        dpsp->variant.declarator_range = local_decl_pos_block.declarator_range;
        ptp->decl_pos_info = dpsp;
        }
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        if (last_param_type == NULL) {
          extra_info->param_type_list = ptp;
        } else {
          last_param_type->next = ptp;
        }  /* if */
        last_param_type = ptp;
        { /* Add an entry to record the parameter name and other information
             associated with the parameter declaration.  These go on to the
             param-id list. */
          a_type_ptr  param_id_type = param_state.type;
          if (param_qualifiers != TQ_NONE) {
            /* If qualifiers were stripped from the parameter type earlier on,
               add them back to the param-id type since that will be used for
               the associated variable if this declarator is for a function
               definition. */
            param_id_type = make_qualified_type(param_id_type,
                                                param_qualifiers);
          }  /* if */
          add_to_param_id_list(&param_locator, param_id_type, &param_type_pos,
                               param_storage_class, func_info,
                               param_state.source_sequence_entry,
                               &last_param_id, is_pack_element);
          last_param_id->declared_type = param_state.declared_type;
#if EXTRA_SOURCE_POSITIONS_IN_IL
          last_param_id->specifiers_range =
                              local_decl_pos_block.specifiers_range;
          last_param_id->declarator_range =
                              local_decl_pos_block.declarator_range;
          last_param_id->identifier_range =
                              local_decl_pos_block.identifier_range;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
          last_param_id->param_num = param_number;
          ptp->param_num = param_number;
          if (param_state.eff_top_level_cv_quals != TQ_NONE) {
            /* Record qualifiers that are not necessarily part of the function
               type in template rescan cases. */
            last_param_id->eff_top_level_cv_quals =
                                           param_state.eff_top_level_cv_quals;
            must_adjust_param_type_qualifiers = state->is_template_rescan;
          }  /* if */
          if (ptp->is_pack_element) {
            last_param_id->is_pack_element = TRUE;
            if (ptp->is_parameter_pack) {
              last_param_id->is_parameter_pack = TRUE;
            }  /* if */
          }  /* if */
          if (ptp->is_parameter_pack &&
              pesep != NULL && pesep->instantiation_descr == NULL &&
              pedp != NULL && last_param_id->symbol != NULL) {
            /* Record the name of the parameter in the prototype instantiation.
               That allows access to the name even for empty expansions. */
            pedp->param_symbol_header = last_param_id->symbol->header;
          }  /* if */
#if GNU_EXTENSIONS_ALLOWED
          if (last_param_id->symbol != NULL) {
            /* Record whether the name of this parameter is a duplicate of
               an earlier one. */
            ptp->duplicate_name = last_param_id->symbol->ambiguous;
          }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
        }
        if (C_mode()) {
          /* Default argument processing not needed in C mode. */
        } else if (curr_token != tok_assign) {
          check_no_default_arg_okay = TRUE;
        } else {
          a_scope_kind      parent_scope_kind;
          a_scope_stack_entry_ptr
                            parent_ssep;
          a_boolean         is_member_or_friend_function;
          a_boolean         cache_default_arg = FALSE;
          a_boolean         ignore_default_arg_expr;
          a_boolean         invalid_default_arg = FALSE;
          a_boolean         nontemplate_function_outside_of_class = FALSE;
          a_param_type_ptr  ptp_for_scan;
          if (!default_arg_allowed_on_curr_param) {
            /* Argument expressions is not allowed.  Issue an error, but go
               ahead and scan the expression. */
            pos_error(ec_default_arg_expr_not_allowed, &pos_curr_token);
          } else if (!is_top_level_declarator) {
            /* Default arguments are normally only allowed on top-level
               function declarations (i.e., not on typedef declarations,
               pointer-to-function or pointer-to-member-function declarations,
               param type declarations, etc.).  Cfront and early Microsoft and
               GNU compilers do accept them on other declarators. */
            an_error_severity  sev = es_discretionary_error;
            if ((microsoft_mode && microsoft_version <= 1300) ||
                (gpp_mode && gnu_version < 30400) ||
                any_cfront_mode()) {
              sev = es_remark;
            }  /* if */
            pos_diagnostic(sev, ec_nonstd_default_arg, &pos_curr_token);
          }  /* if */
          any_default_args = TRUE;
          /* Advance past the equal sign. */
          (void)get_token();
          last_default_arg_pos = pos_curr_token;
          /* Check the scope immediately containing the current scope, which is
             a function prototype scope.  We may have to cache the default
             argument tokens and rescan them later. */
          if (pesep != NULL && pesep->instantiation_descr != NULL) {
            /* If we are in a pack where default arguments are not allowed,
               cache them instead of evaluating them, which could lead to
               errors (don't do this inside prototype instantiations, since
               there the representation should be recorded (e.g., for rendering
               by the C++-generating back end). */
            cache_default_arg = TRUE;
          }  /* if */
          is_member_or_friend_function = FALSE;
          /* Default argument expressions are scanned, but not converted to
             their destination type if (a) they're not permitted on the current
             parameter (because additional errors about a failing conversion
             would not be helpful), or (b) if we've seen an "auto" parameter
             (because the destination type is unknown and the work is not
             needed). */
          ignore_default_arg_expr = !default_arg_allowed_on_curr_param ||
                                    state->variant.auto_params != NULL;
          parent_ssep = &scope_stack[depth_scope_stack-1];
          parent_scope_kind = parent_ssep->kind;
          if (default_arg_allowed_on_curr_param) {
            if (parent_scope_kind == (a_scope_kind)sck_class_struct_union &&
                !type_is_lambda_closure(parent_ssep->assoc_type)) {
              /* A member function of a class (normal or template) inside
                 a class declaration.  The is_top_level_declarator test is
                 used to ignore default arguments in things like
                 pointer-to-member declarations.  Such default arguments
                 are cached in non-template contexts only. */
              if (is_top_level_declarator ||
                  !is_template_dependent_context()) {
                cache_default_arg = TRUE;
                is_member_or_friend_function = TRUE;
              }  /* if */
            } else if (parent_scope_kind ==
                                   (a_scope_kind)sck_template_declaration) {
              /* A function template declaration.  Note that all default
                 arguments are cached. */
              cache_default_arg = TRUE;
            } else if (parent_scope_kind ==
                                   (a_scope_kind)sck_template_instantiation) {
              /* A template instantiation -- the function declarator tokens are
                 being rescanned.  All the default arguments are scanned from
                 caches during a later fixup, so ignore the expression now. */
              cache_default_arg = TRUE;
              ignore_default_arg_expr = TRUE;
            } else if (parent_scope_kind ==
                                   (a_scope_kind)sck_class_reactivation ||
                       parent_scope_kind ==
                                   (a_scope_kind)sck_namespace_reactivation) {
              /* First skip surrounding reactivation scopes.  While doing so
                 skip any template instantiation scopes pushed as Microsoft
                 mode specialization scopes.  These should be considered
                 to be part of the associated class reactivation. */
              int  template_scope = depth_scope_stack-1;
              a_scope_stack_entry_ptr	ssep = &scope_stack[template_scope];
              while (ssep->kind == (a_scope_kind)sck_class_reactivation ||
                     ssep->kind == (a_scope_kind)sck_namespace_reactivation) {
                if (ssep->kind == (a_scope_kind)sck_class_reactivation &&
                    ssep->microsoft_specialization_scope_pushed) ssep--;
                ssep--;
              }  /* while */
              if (ssep->kind == (a_scope_kind)sck_template_declaration) {
                /* A member function declaration of a template class outside
                   of the class declaration.  This is nonstandard but is
		   allowed in certain modes. */
                if (allow_default_arg_on_template_member_definition) {
                  /* This is a template case, so the default should be
                     cached. */
                  cache_default_arg = TRUE;
                } else {
                  an_error_severity  sev = es_error;
                  if (ms_extensions &&
                      (microsoft_version < 1911 || ms_permissive)) {
                    /* MSVC accepts this, except for recent versions in
                       non-permissive modes. */
                    sev = es_warning;
                  }  /* if */
                  pos_diagnostic(sev, ec_default_arg_expr_not_allowed,
                                 &pos_curr_token);
                  ignore_default_arg_expr = TRUE;
                  /* Set a flag that indicates that this default argument
                     should not actually be applied to the function.  This is
                     done instead of clearing default_arg_allowed_on_curr_param
                     because that would cause errors if subsequent parameters
                     have ignored defaults. */
                  ignore_disallowed_default_arg = TRUE;
                }  /* if */
              }  /* if */
            } else if (is_top_level_declarator && !is_typedef_decl &&
                       !(di_flags & DI_ABSTRACT_DECLARATOR_ALLOWED) &&
                       !state->is_lambda && !state->nested_ptr_or_ref_seen &&
                       !ms_extensions) {
              /* Even for a non-template function appearing outside of a class
                 definition we have to cache the default argument until we
                 know which function is being declared, because only then can
                 we be certain that we correctly establish friendship.
                 Microsoft compilers, however, don't do this.   This doesn't
                 apply to contexts where abstract declarators may be used
                 (e.g., type-ids or parameter declarations), to typedef
                 declarations, or to lambdas. */
              cache_default_arg = TRUE;
              nontemplate_function_outside_of_class = TRUE;
            }  /* if */
          }  /* if */
          if (curr_token == tok_comma || curr_token == tok_rparen ||
              curr_token == tok_semicolon || 
              (!list_init_enabled &&
               (curr_token == tok_rbrace || curr_token == tok_lbrace))) {
            /* There was an "=" sign, but the following token is not one
               that can begin a default argument. */
            invalid_default_arg = TRUE;
          }  /* if */
          /* A NULL param type pointer is used as a signal to the caching and
             scanning routines that the default argument should be ignored. */
          ptp_for_scan = ignore_default_arg_expr ? NULL : ptp;
          if (cache_default_arg) {
            /* The default argument should be cached.  There are three cases:
                 (1) function templates: Their default arguments aren't
                     scanned until the template is actually used.
                 (2) member function and friend declarations in classes: The
                     default argument should be scanned when the enclosing
                     class(es) are completed.
                 (3) functions and member functions declared outside any class
                     definition: The default argument can be scanned as soon as
                     the function/member function is known. */
            if (invalid_default_arg) {
              /* During a prototype instantiation default arguments are
                 cached but not rescanned.  Issue the syntax error here. */
              pos_error(ec_exp_primary_expr, &pos_curr_token);
              if (curr_token == tok_lbrace) {
                /* Cache and discard a brace initializer when one is not
                   allowed. */
                prescan_default_function_arg_expr(
                                              (a_param_type_ptr)NULL,
                                              (a_def_arg_expr_fixup_ptr*)NULL,
                                              /*is_template_function=*/FALSE,
                                              /*is_friend_decl=*/FALSE, 0);
              }  /* if */
            } else if (is_member_or_friend_function) {
              /* Cache a default argument for a member or friend function. */
              prescan_member_function_default_arg_expr(ptp_for_scan,
						       is_friend_decl,
                                                       param_number);
            } else if (nontemplate_function_outside_of_class) {
              /* Cache a default argument for an ordinary function or member
                 function declared outside any class definition. */
              prescan_function_default_arg_expr(state, ptp_for_scan);
            } else {
              /* Cache a default argument for a function template. */
              prescan_function_template_default_arg_expr(ptp_for_scan,
                                                         param_number);
            }  /* if */
          } else {
            /* Not a case in which the default argument should be cached (e.g.,
               because this is a typedef declaration in a mode that allows
               default arguments in that context) -- or else a syntax error.
               Scan the expression and convert it to the required type. */
            scan_default_arg_expr(ptp_for_scan, is_member_or_friend_function,
                                  (state->dso_flags & DSO_CONSTEVAL) != 0);
          }  /* if */
          if (default_arg_allowed_on_curr_param &&
              !ignore_disallowed_default_arg) {
            ptp->has_default_arg = TRUE;
            if (is_member_or_friend_function) {
              /* Record that the default argument appeared in a class
                 definition.  This matters because such default arguments are
                 subject to different "one-definition-rule" constraints. */
              ptp->default_arg_appeared_in_class_definition = TRUE;
            }  /* if */
            func_info->any_default_args = TRUE;
          }  /* if */
        }  /* if */
        if (!disallow_default_args && !default_arg_allowed_on_curr_param) {
          if (last_param_type == extra_info->param_type_list) {
            /* The first parameter on the list has just been processed. */
            if (locator != NULL && locator->is_operator_name &&
                (is_new_operator(locator->variant.opname) ||
                 is_delete_operator(locator->variant.opname))) {
              /* Default argument expressions are permitted on the second and
                 subsequent parameters of an operator new and delete
                 declarations. */
              declarator_allows_default_args = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
        /* Keep scanning parameter-declarations if there is a comma.
           However, also check for an ellipsis ("...") following the comma,
           which ends the prototype list in a different way. */
        /* Note that a comma preceding the ellipsis is optional in C++. */
        if (dangling_type_specifier ||
            C_mode() ? curr_token == tok_ellipsis :
                       (is_error_locator(param_locator) &&
                        identifier_is_template_id())) {
          /* A dangling type specifier is detected by decl_specifiers
             when a comma is omitted between the end of a type specifier
             and the start of the next.  This is pretty unlikely, but the
             mechanism was added for class declarations, where it is more
             useful. */
          /* Another unlikely case is the identifier-but-not-declarator-id
             case -- which occurs when a template-id appears where a
             declarator was expected. */
          pos_error(ec_exp_comma, &pos_curr_token);
          done = FALSE;
        } else {
          done = !loop_token(tok_comma);
        }  /* if */
        remove_stop_token(tok_comma);
        /* If this was actually a pack declaration in a template definition
           context, a pack expansion descriptor will be returned that can be
           used to create a substituted function type. */
        ptp->pack_expansion_descr =
           end_potential_pack_expansion_context(pesep, /*is_declarator=*/TRUE);
        if (ptp->pack_expansion_descr != NULL &&
            ptp->pack_expansion_descr->is_pack_index) {
          /* A pack-index specifier is a single-element parameter
             declaration, not a pack expansion. */
          ptp->pack_expansion_descr = NULL;
        }  /* if */
        if (ptp->pack_expansion_descr != NULL) {
          ptp->is_parameter_pack = TRUE;
          last_param_id->is_parameter_pack = TRUE;
          if (state->param_with_only_enclosing_pack_refs) {
            /* In something like:
                 template<class ... Ts> struct S {
                   template<class F> auto m(F f, Ts... p)->decltype(f(p...));
                 };
               a pack reference entry will be created for the reference to p
               in the return type.  That entry must indicate that it really is
               expanded only through the enclosing template's pack (Ts, here);
               otherwise, the empty expansion case will not be handled
               correctly.  By recording this property at this time (when p is
               declared), we ensure that record_potential_pack_reference_full
               will have the needed information to record the property for the
               later reference. */
            last_param_id->uses_only_enclosing_pack = TRUE;
          }  /* if */
        }  /* if */
        if (!skip_pack_index_iteration(&pesep,
                                       (a_pack_expansion_descr_ptr)NULL,
                                       &any_variadic_params)) {
          any_variadic_params = advance_to_next_pack_element(pesep);
        }  /* if */
        if (!any_variadic_params) {
          /* The previous variadic expansion is now complete.  Prepare for the
             possibility that the next parameter will come from a variadic
             expansion.  We may have to skip one or more empty expansions
             as part of this process. */
          pesep = NULL;
          while (!done && !any_variadic_params) {
            any_variadic_params =
                               begin_potential_pack_expansion_context(&pesep);
            if (!any_variadic_params) {
              /* We ran into an empty expansion, which caused us to skip the
                 pattern tokens.  Update "done" accordingly. */
              done = curr_token == tok_rparen;
              /* Keep the parameter numbering of the instantiation in sync
                 with the generic numbering. */
              ++param_number;
            }  /* if */
          }  /* while */
        }  /* if */
        if (check_no_default_arg_okay && !ptp->is_parameter_pack) {
          /* For in-class member template declarations, make sure that all
             parameters after the first one with a default argument also have
             default arguments (or are parameter packs).  Similar checks for
             other functions are done elsewhere.  This had to wait until now,
             because we need to check the ptp->is_parameter_pack flag. */
          if (any_default_args && parent_type != NULL &&
              depth_template_declaration_scope == depth_scope_stack - 1) {
            pos_error(ec_default_arg_not_at_end, &last_default_arg_pos);
            /* Reset the flag that indicates that default arguments have
               been seen to suppress subsequent errors. */
            any_default_args = FALSE;
          }  /* if */
        }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (param_array_next) {
          if (!done) {
            /* A C++/CLI parameter array was just scanned, but there are more
               parameters: Issue an error. */
            pos_error(ec_cli_param_array_must_be_last_parameter,
                      &param_state.declarator_pos);
          } else if (curr_token == tok_ellipsis) {
            /* A C++/CLI parameter array was followed by an ellipsis indicating
               a C-style variadic function: Issue an error. */
            pos_error(ec_ellipsis_after_param_array, &pos_curr_token);
            /* Skip over the ellipsis for error recovery. */
            (void)get_token();
          }  /* if */
          param_array_next = FALSE;
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not add code here. */
        if (curr_token == tok_ellipsis) {
          /* The parameter list ends with an ellipsis or the next parameter is
             a C++/CLI parameter array. */
          ellipsis_pos = pos_curr_token;
          (void)get_token();
#if ASM_FUNCTION_ALLOWED
          if (func_info->is_asm_function) {
            pos_error(ec_bad_asm_func_ellipsis, &ellipsis_pos);
          }  /* if */
#endif /* ASM_FUNCTION_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
          if (cppcli_enabled && !done &&
              is_type_start(/*is_expr_context=*/FALSE)) {
            /* The ellipsis follows a comma and precedes a type specifier:
               A C++/CLI parameter array should be next (not allowed in
               C++/CX, however). */
            param_array_next = TRUE;
          } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          /* Do not insert code here. */
          { /* Set the ellipsis flag on the parameter type list, and exit the
               loop. */
            extra_info->has_ellipsis = TRUE;
            done = TRUE;
            /* The call to begin_potential_pack_expansion_context above may
               have mistakenly concluded that more parameters are ahead, but
               we now know that the ellipsis terminates the parameter list.
               E.g., template<class ...T> void f(int, T..., ...) {} */
            any_variadic_params = FALSE;
            if (pesep != NULL) {
              abandon_potential_pack_expansion_context(pesep);
              pesep = NULL;
            }  /* if */
          }  /* if */
        }  /* if */
        if (is_constructor && parent_type != NULL
#if MICROSOFT_EXTENSIONS_ALLOWED
            && !(cli_class_type_kind_is(parent_type, cctk_value) &&
                 scanning_generated_code_from_metadata)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                                       ) {
          /* In case this is an ill-formed copy constructor, we need to do
             some additional error checking.  We're looking for cases like
               A::A(A);                // case 1
               A::A(A, T=x);           // case 2
               A::A(A&, A=y);          // case 3
               A::A(A&, T=x, A=y);     // case 4
             In C++/CLI mode, additional variants are of interest; e.g.
               A::A(A%);               // case 5
               A::A(A%, T=x, A=y);     // case 6
             It's not actually possible to know whether a constructor is a
             (valid or invalid) copy constructor without looking past the
             first parameter.  That's part of what makes this check a little
             complicated.  In modes with guaranteed copy elision, cases 3 and 4
             are not diagnosed here since copy elision may be performed on the
             default argument.  An error will be reported later if the copy
             cannot be elided. */
          /* Note that this check doesn't apply to C++/CLI value classes read
             from metadata.  There is no danger of unbounded recursion since
             C++/CLI value types are always bit-copied rather than copied
             through a copy constructor.  (This allowance could be made for
             value classes not loaded from metadata, but the Microsoft compiler
             does diagnose those.) */
          if (extra_info->param_type_list->next == NULL) {
            /* This is the first item on the list. */
            tp = skip_typerefs(param_state.type);
            if (identical_types(parent_type, tp)) {
              /* Type of the first parameter is identical to the type of the
                 parent class. */
              if (done && !any_variadic_params) {
                /* This is like case 1 above. */
                pos_ty_error(ec_bad_constructor_param, &param_type_pos,
                             parent_type);
                ptp->type = error_type();
                ptp->passed_via_copy_constructor = FALSE;
              } else if (scope_stack[depth_scope_stack-1].kind !=
                                   (a_scope_kind)sck_template_instantiation) {
                /* Depending on whether the next parameter has a default
                   argument (see case 2 above), this may be an (illegal)
                   copy constructor.  (If this is the instantiation of a
                   member template, it cannot be a copy constructor.) */
                may_be_copy_constructor = TRUE;
                /* Record information to assure that an error will be issued
                   if this does turn out to be an error case. */
                bad_first_param_for_copy_constructor = TRUE;
                pos_of_first_param_type = param_type_pos;
              }  /* if */
            } else if (!done) {
              /* We're looking at the first parameter.  See if this may be a
                 copy constructor.  This will help find cases 3 through 6. */
              if (is_any_lvalue_reference_type(param_state.type)) {
                tp = type_pointed_to(param_state.type);
                tp = skip_typerefs(tp);
                if (identical_types(parent_type, tp)) {
                  /* Depending on whether the next parameter has a default
                     argument, this may be a copy constructor. */
                  may_be_copy_constructor = TRUE;
                }  /* if */
              }  /* if */
            }  /* if */
          } else if (may_be_copy_constructor) {
            /* We are beyond the first parameter on the list of what may
               be a copy constructor. */
            if (ptp == extra_info->param_type_list->next) {
              /* This is the second parameter in the list. */
              if (!ptp->has_default_arg) {
                /* The second parameter lacks a default argument so this must
                   not be a copy constructor. */
                may_be_copy_constructor = FALSE;
              } else if (bad_first_param_for_copy_constructor) {
                /* The second parameter does have a default argument and the
                   first argument is not a ref.  This is like case 2.  Issue
                   the error using the source position of the first param
                   type. */
                pos_ty_error(ec_bad_constructor_param,
                             &pos_of_first_param_type, parent_type);
                extra_info->param_type_list->type = error_type();
                extra_info->param_type_list->passed_via_copy_constructor=FALSE;
                may_be_copy_constructor = FALSE;
              }  /* if */
            }  /* if */
            if (may_be_copy_constructor) {
              tp = skip_typerefs(ptp->type);
              if (!mandatory_copy_elision &&
                  identical_types(parent_type, tp)) {
                /* Type of this parameter is identical to the type of the
                   parent class (see cases 3 and 4 above).  If guaranteed copy
                   elision may apply, this is permissible.  If not, since this
                   is a copy constructor, an error is in order. */
                pos_ty_error(ec_bad_constructor_param, &param_type_pos,
                             parent_type);
                ptp->type = error_type();
                may_be_copy_constructor = FALSE;
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* if */
        run_end_of_parse_actions(&param_state, /*more_declarators=*/FALSE);
      } while (!done || any_variadic_params);
    }  /* if */
    state->param_with_only_enclosing_pack_refs = FALSE;
    /* Save the list of symbols for the prototype scope (usually NULL, but
       can have symbols for named types declared within the prototype). */
    if (is_top_level_declarator) {
      /* Note that a pointer to the current entry of scope_stack is not saved
         from earlier in this routine because scope_stack might have been
         reallocated in the interim. */
      func_info->prototype_scope_symbols =
             assoc_pointers_block_of(&scope_stack[depth_scope_stack])->symbols;
#if GENERATE_SOURCE_SEQUENCE_LISTS
      /* Transfer the source sequence list in the function prototype scope
         over to the func_info block. */
      func_info->prototype_scope_ss_list =
                         scope_stack[depth_scope_stack].source_sequence_list;
      scope_stack[depth_scope_stack].source_sequence_list = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    }  /* if */
    /* Process pragmas associated with the closing paren before the current
       scope is popped. */
    process_curr_token_pragmas();
    /* Before popping the scope, move the vla_fixup_list from the scope_stack
       to func_info. */
    func_info->vla_fixup_list = scope_stack[depth_scope_stack].vla_fixup_list;
    scope_stack[depth_scope_stack].vla_fixup_list = NULL;
  } else {
    /* Non-prototyped parameter list.  Issue a remark in most cases because
       non-prototyped function declarators have been obsolescent in C since
       C99 (and are nonstandard in C++).  An exception is made for definitions
       of functions that were previously declared with a prototype.  Since we
       cannot identify such cases at this time, top-level function declarators
       are handled when parsing is complete. */
    if (!is_top_level_declarator) {
      pos_remark(ec_use_of_non_prototype_func_declarator, &start_pos);
    } else {
      add_end_of_parse_action(diag_unprototyped_func_declarator, state,
                              /*secondary_decls=*/FALSE);
    }  /* if */
    if (any_params) {
      /* Old-style list of identifiers. */
      if (!is_top_level_declarator) {
        /* This type of parameter list is not valid in abstract declarators
           and non-top-level function declarators. */
        if (microsoft_mode && C_mode()) {
          /* No diagnostic in Microsoft C mode. */
        } else {
          pos_error(ec_param_id_list_needs_function_def, &error_position);
        }  /* if */
      } else if (C_dialect == C_dialect_cplusplus) {
        /* This type of parameter list is an anachronism in C++. */
        diagnostic(anachronism_error_severity, ec_old_style_parameter_list);
      }  /* if */
      do {
        add_stop_token(tok_comma);
        /* Scan the list of identifiers. */
        if (curr_token != tok_identifier) {
          if (curr_token == tok_ellipsis && next_token() == tok_rparen) {
            /* In Microsoft C an ellipsis is permitted (and ignored) on an
               old-style param list. */
            diagnostic(microsoft_mode ? es_warning : es_error,
                       ec_ellipsis_not_allowed);
            /* Advance past the ellipsis. */
            (void)get_token();
          } else {
            /* Error, expected identifier. */
            (void)required_token(tok_identifier, ec_exp_identifier);
          }  /* if */
        } else {
          /* See if the identifier is also a typedef name.  Such a name is
             not allowed (3.7.1, constraints).  In pcc mode, however, this
             is allowed. */
          if (C_dialect != C_dialect_pcc && !microsoft_bugs &&
              curr_id_is_type_name(GID_NO_OPTIONS, IDS_NO_OPTIONS)) {
            pos_error(C_mode() ? ec_typedef_cannot_be_param_name :
                                 ec_type_cannot_be_param_name,
                      &error_position);
            /* Enter the parameter anyway, for best error recovery. */
          }  /* if */
          /* Add the identifier to the parameter id list. */
          if (c23_mode && last_param_id == NULL) {
            /* C23 no longer permits old-style parameter lists. */
            an_error_severity  sev = gnu_version_is(any_version) ?
                                          es_warning : es_discretionary_error;
            pos_diagnostic(sev, ec_c23_old_style_param_id,
                           &locator_for_curr_id.source_position);
          }  /* if */
          add_to_param_id_list(&locator_for_curr_id, (a_type_ptr)NULL,
                               (a_source_position*)NULL,
                               (a_storage_class)sc_unspecified, 
                               func_info, (a_source_sequence_entry_ptr)NULL,
                               &last_param_id, /*is_pack_element=*/FALSE);
          /* Update the param-id entry just created with the source position
             of the identifier. */
          last_param_id->old_style_id_pos =
                                          locator_for_curr_id.source_position;
          /* Advance past the identifier. */
          (void)get_token();
        }  /* if */
        remove_stop_token(tok_comma);
        /* Keep looping on a comma, stop otherwise. */
      } while (loop_token(tok_comma));
    }  /* if */
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    decl_pos_block->declarator_range.end = pos_curr_token;
  }  /* if */
  curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Check for closing right parenthesis.  We temporarily clear the stop
     token array values for tok_comma and tok_assign, in order to flush past
     either to the right paren. */
  { a_token_set_array_element t1;
    a_token_set_array_element t2;
    t1 = curr_stop_token_stack_entry->stop_tokens[(int)tok_comma];
    t2 = curr_stop_token_stack_entry->stop_tokens[(int)tok_assign];
    curr_stop_token_stack_entry->stop_tokens[(int)tok_comma] = 0;
    curr_stop_token_stack_entry->stop_tokens[(int)tok_assign] = 0;
    (void)required_token(tok_rparen, ec_exp_rparen);
    curr_stop_token_stack_entry->stop_tokens[(int)tok_comma] = t1;
    curr_stop_token_stack_entry->stop_tokens[(int)tok_assign] = t2;
  }
  remove_stop_token(tok_rparen);
  scope_stack_top().outside_parameter_list = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ms_extensions) {
    a_boolean  managed_member = cli_or_cx_enabled && parent_type != NULL &&
                                is_managed_class_type(parent_type);
    if (extra_info->has_ellipsis) {
      /* If this function type was declared with an ellipsis, its calling
         convention is required to be __cdecl, unless it is a member of a
         managed class.  If this isn't already the default for the
         compilation, set it in the type.  (Ordinarily, the setting in the
         type reflects an explicit specification of the calling convention.) */
      if (managed_member) {
        if (!scanning_generated_code_from_metadata) {
          /* Microsoft compilers don't accept managed member definitions with
             an ellipsis parameter (they accept such declarations that aren't
             definitions, but that is a useless caveat we do not emulate).
             However, the C++/CLI core library does have members with an
             ellipsis parameter (notably: Console::WriteLine). */
          pos_error(ec_managed_member_function_cannot_have_ellipsis_parameter,
                    &ellipsis_pos);
        }  /* if */
      } else if (default_calling_convention !=
                                             (a_calling_convention)cc_cdecl) {
        extra_info->calling_convention = (a_calling_convention)cc_cdecl;
      }  /* if */
    }  /* if */
    if (managed_member && !cppcx_enabled) {
      extra_info->calling_convention = (a_calling_convention)cc_clrcall;
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (C_dialect == C_dialect_cplusplus) {
    if (state->decl_being_cached && state->variant.auto_params != NULL) {
      /* If we ran into "auto" parameters, do not complete parsing of the
         function declarator because trailing components might refer to those
         "auto" parameters in ways that cannot be resolved at this time.  E.g.:
             void f(auto ... ps)->decltype((0 + ... + ps));
         Here the pack expansion in the trailing return type cannot be
         performed until the template declaration context is set up. */
      goto done;
    } else if (state->for_requires_expr_params && extra_info->has_ellipsis) {
      pos_error(ec_requires_expr_ellipsis_param, &ellipsis_pos);
    }  /* if */
    cplusplus_function_declarator_trailer(state, *new_type_ptr, func_info,
                                          locator, parent_type,
                                          is_top_level_declarator,
                                          is_nonstatic_member, is_constructor,
                                          is_static_constructor, is_destructor,
                                          is_finalizer,
                                          disallow_exception_spec,
                                          is_typedef_decl, decl_pos_block);
  } else {
    if (curr_token == tok_edg_throw) {
      /* In C mode, the __edg_throw__ keyword introduces an exception
         specification that is discarded here.  This is used during the
         processing of builtin function signatures that are used in both
         C and C++ modes. */
      (void)get_token();
      if (curr_token == tok_lparen) {
        flush_until_matching_token_full(/*limit_flush=*/FALSE);
        if (curr_token == tok_rparen) {
          (void)get_token();
        }  /* if */
      } else {
        pos_error(ec_exp_lparen, &pos_curr_token);
      }  /* if */
    }  /* if */
  }  /* if */
  scan_declarator_attributes(state, new_type_ptr);
done:
  /* Pop the function prototype scope if needed. */
  if (must_pop_function_prototype_scope) pop_scope();
  if (!is_top_level_declarator) {
    done_with_func_info(local_func_info_block); /*lint !e530*/
  } else if (must_adjust_param_type_qualifiers) {
    /* We are rescanning a function template declaration for substitution
       purposes and we encountered a parameter with a top-level qualifier.
       Now consider:
         template<typename T> void f(T const);  // (1)
         template<typename T> void f(T) {}
         template void f(int);
       Here, the rescan is based on (1), but we do not want to carry the
       "const" into the function definition.  The qualifiers to drop are
       recorded in a_param_id::eff_top_level_cv_quals.  Note that if the
       explicit instantiation is
         template void f<int const>(int);
       a_param_id::eff_top_level_cv_quals will be TQ_NONE and the const will
       be preserved.  This adjustment has to happen after trailing return
       types and exception-specifications are scanned, because in those
       contexts the cv-qualifiers do apply. */
    a_param_id_ptr    pip = func_info->param_id_list;
    a_param_type_ptr  ptp = extra_info->param_type_list;
    for (; ptp != NULL; ptp = ptp->next) {
      while (pip != NULL && pip->param_num < ptp->param_num) {
        pip = pip->next;
      }  /* while */
      if (pip != NULL && pip->param_num == ptp->param_num) {
        copy_qualifiers((~pip->eff_top_level_cv_quals & ptp->qualifiers),
                        ptp->qualifiers);
      }  /* if */
    }  /* for */
  }  /* if */
  copy_source_position(start_pos, error_position);
  state->function_declarator_seen = TRUE;
  if (!state->is_old_style_param_decl && !state->is_param_decl &&
      state->variant.auto_params != NULL) {
    /* The "auto" parameters were recorded by inserting elements at the front
       of the list.  Reverse the list so they appear in lexical order.  (A
       parameter can also point to its corresponding "auto" param description,
       but even if that parameter involves a function declarator, it shouldn't
       treat that pointer as a pointer to a list.) */
    state->variant.auto_params =
                              reverse_simple_list(state->variant.auto_params);
  }  /* if */
  db_exit();
}  /* function_declarator */


a_param_type_ptr scan_requires_expr_parameters(a_decl_parse_state  *dps)
/*
Scan a sequence of parameters for a requires-expression and return a pointer to
the corresponding param-type entries.  This establishes a function prototype
scope that persists after the call.  dps is a pointer to the state representing
the parse state of the parameters (initialized by this function, and pointed to
by the scope stack entry created by this function).
*/
{
  a_type_ptr          func_type = void_type();
  a_func_info_block   func_info;
  a_decl_pos_block    decl_pos_block;
  a_param_type_ptr    result = NULL;
  a_symbol_locator    loc;

  clear_decl_pos_block(&decl_pos_block);
  check_assertion(curr_token == tok_lparen);
  make_opname_locator((an_opname_kind)onk_function_call, &loc,
                      &pos_curr_token);
  add_stop_token(tok_rparen);
  (void)get_token();
  clear_func_info(&func_info);
  func_info.keep_param_id_list = TRUE;
  function_declarator(dps, DI_NO_INPUT_FLAGS, &func_type, &func_info, &loc,
                      /*parent_type=*/(a_type_ptr)NULL,
                      /*is_nonstatic_member=*/FALSE, /*is_constructor=*/FALSE, 
                      /*is_static_constructor=*/FALSE, /*is_destructor=*/FALSE,
                      /*is_finalizer=*/FALSE, /*disallow_default_args=*/TRUE,
                      /*disallow_exception_spec=*/TRUE, &decl_pos_block);
  done_with_func_info(func_info);
  if (func_type != NULL && type_is(func_type, tk_routine)) {
    a_param_type_ptr  ptp;
    result = function_type_params(func_type);
    for (ptp = result; ptp != NULL; ptp = ptp->next) {
      ptp->is_requires_expr_param = TRUE;
    }  /* for */
  }  /* if */
  remove_stop_token(tok_rparen);
  return result;
}  /* scan_requires_expr_parameters */


static void make_bound_expr_referenceable_from_file_scope(
                                                        an_expr_node_ptr *expr,
                                                        a_type_ptr       type,
                                                        a_boolean        dep)
/*
Process an expression used as an array bound (may be NULL) so that it can
be referred to by the given type entry.  Because types appear in file scope
and file scope entries cannot refer to local scope entries, either copy the
expression tree to the current memory region (assumed to be file scope upon
entry) or create a local expr_node reference to it and mark the type entry
accordingly.  If the expression tree was copied, the original expression
pointer is updated to point to the copy; if a local expr_node reference was
created, the original expression pointer is set to NULL.  dep is TRUE for
the subexpressions of the ck_template_param for the template-dependent
bound case, FALSE for the "expr" field of the constant itself.
*/
{
  if (*expr != NULL) {
    if (expr_has_reference_to_local_entity(*expr)) {
      /* The expression refers to a local entity that can't be referenced from
         file scope.  Create a local expr node reference to it instead.  Use
         get_innermost_function_scope because we could be in a local class
         here. */
      a_scope_ptr function_scope = get_innermost_function_scope();
      check_assertion(function_scope != NULL);
      if (in_file_scope(*expr)) {
        /* Even though there is a reference to a local entity somewhere
           in the expression tree, the top-level node is in file-scope
           memory.  Make a copy in the innermost function scope and use
           that for the local expr node reference.  This copy is necessary
           because of code in i_copy_constant_full that copies the expression
           under tpck_expression constants unconditionally, which might mean
           a copy from the function scope to the file scope there that would
           have to be reversed here.  That's wrong, but for the reasons given
           there it can't be fixed immediately, and this undoes most of the
           damage.*/
        check_assertion(function_scope != NULL &&
                        curr_il_region_number == file_scope_region_number);
        switch_il_region(mem_region_for_routine(
                                         function_scope->variant.routine.ptr));
        *expr = copy_expr_tree(*expr, CE_COPYING_FOR_LOCAL_EXPR_NODE_REF);
        switch_il_region(file_scope_region_number);
      }  /* if */
      make_local_expr_node_ref(
                      *expr,
                      (dep ? (a_local_expr_node_ref_kind)lerk_dep_array_bound :
                             (a_local_expr_node_ref_kind)lerk_array_bound),
                      (char *)type, function_scope);
      *expr = NULL;
    } else if (!in_file_scope(*expr)) {
      /* Copy the expression to file-scope memory. */
      *expr = copy_expr_tree(*expr, CE_ALWAYS_COPY_BACKING_EXPRESSIONS);
    }  /* if */
  }  /* if */
}  /* make_bound_expr_referenceable_from_file_scope */


static a_boolean in_class_definition(void)
/*
Return TRUE if the current scope is a class scope or a function prototype
scope in a class scope.
*/
{
  a_scope_stack_entry_ptr  ssep = &scope_stack_top();

  while (ssep->kind == (a_scope_kind)sck_func_prototype) ssep -= 1;
  return ssep->kind == (a_scope_kind)sck_class_struct_union;
}  /* in_class_definition */


void array_declarator(a_decl_parse_state    *dps,
                      a_type_ptr            *new_type_ptr,
                      a_boolean             nonconstant_dimension_allowed,
                      a_boolean             vla_allowed,
                      a_boolean             vla_asterisk_allowed,
                      ARG_UNUSED a_boolean  threads_dimension_allowed,
                      a_boolean             top_level_field_decl,
                      a_boolean             top_level_param_decl,
                      a_decl_pos_block_ptr  decl_pos_block)
/*
Scan an array declarator, or an array declarator in an abstract declarator.
*dps describes the context of the call (e.g., if this declarator appears in an
evaluated sizeof expression).  Allocate and return in *new_type_ptr an
appropriate array type.  The initial opening bracket is the current token.
In C++ the dimension may sometimes be a nonconstant expression (e.g., with a
new type name); that case is indicated by nonconstant_dimension_allowed.
When vla_enabled is TRUE, the dimension may be a nonconstant expression; and
when vla_asterisk_allowed is TRUE, a VLA of unknown size can be indicated with
the "[*]" syntax in a function prototype.  top_level_field_decl is TRUE to
indicate that this is the declaration of a nonstatic data member of a class.
top_level_param_decl is TRUE to indicate that this is a a top-level declarator
in a function parameter declaration.  threads_dimension_allowed indicates
whether the dimension expression can be a multiple of the special UPC THREADS
constant.
*/
{
  a_targ_size_t           num_of_elements = 0;
  a_constant_ptr          constant = local_constant();
  a_boolean               is_constant_bound = FALSE;
  a_boolean               err = FALSE;
  a_boolean               has_vla_asterisk = FALSE;
  a_boolean               template_dependent_bound = FALSE;
  a_source_position       start_pos, size_pos;
  an_expr_node_ptr        dim_expr = NULL;
  a_boolean               static_seen = FALSE;
  a_type_qualifier_set    qualifiers = TQ_NONE;
#if UPC_EXTENSIONS_ALLOWED
  a_boolean               upc_threads_dimension = FALSE;
#endif /* UPC_EXTENSIONS_ALLOWED */

  db_enter(3, "array_declarator");
  copy_source_position(pos_curr_token, start_pos);
  /* Pass over the initial left bracket. */
  (void)get_token();
  if (curr_token == tok_lbracket && std_attributes_enabled) {
    pos_diagnostic(es_discretionary_error, ec_must_introduce_attribute,
                   &start_pos);
  }  /* if */
  add_stop_token(tok_rbracket);
  if ((c99_mode || gcc_mode) && top_level_param_decl &&
      curr_token == tok_static) {
    /* In C99, "static" in an array declarator in a parameter declaration
       indicates that the actual argument must have at least as many elements
       as the declared size of the array. */
    if (!c99_mode) {
      pos_warning(ec_static_dimension_nonstandard, &pos_curr_token);
    }  /* if */
    static_seen = TRUE;
    (void)get_token();
  } else if (!C_mode() && (decl_scope_level == NO_SCOPE_DEPTH ||
                           scope_stack[decl_scope_level].kind ==
                                          (a_scope_kind)sck_func_prototype)) {
    /* In non-C modes, VLAs are never allowed in function prototypes. */
    vla_allowed = vla_asterisk_allowed = FALSE;
  }  /* if */
  /* In some modes, "restrict" is allowed inside the brackets:
       int x[restrict 5]
     or
       int y[restrict]
     This is allowed only for formal parameter declarations, and
     indicates that the pointer type to which the array type decays
     is restrict-qualified (e.g., "restrict pointer to int" in
     the first example above.  In C99, cv-qualifiers are also allowed
     inside the brackets. */
  if (is_type_qualifier()) {
    a_source_position     qualifier_pos;

    qualifier_pos = pos_curr_token;
    qualifiers = collect_type_qualifiers(decl_pos_block,
                                         (a_upc_block_size *)NULL);
    if (top_level_param_decl) {
      /* This is a top-level declaration of a function parameter type. */
      /* Only C99 mode allows cv-qualifiers.  _Atomic, restrict, and Clang
         nullability qualifiers are allowed in any mode where they are enabled.
         Named-address space qualifiers are not allowed in any mode. */
      a_type_qualifier_set  mask = TQ_RESTRICT | TQ_NULLABILITY
                                               | TQ_C11_ATOMIC;
      if (c99_mode) {
        mask |= TQ_CONST | TQ_VOLATILE;
      }  /* if */
#if NAMED_ADDRESS_SPACES_ALLOWED
      if (named_address_space_from_qualifier_set(qualifiers) != 0) {
        pos_error(ec_named_address_space_not_allowed, &qualifier_pos);
        qualifiers = simple_qualifiers(qualifiers);
      }  /* if */
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
      if ((qualifiers & mask) != qualifiers) {
        pos_error(ec_type_qualifier_not_allowed, &qualifier_pos);
        qualifiers &= mask;
      }  /* if */
    } else {
      /* This is not a top-level declarator for a parameter, so "restrict"
         and cv-qualifiers are not allowed.  Issue an error. */
      pos_error((qualifiers == TQ_RESTRICT) ?
                   ec_restrict_not_allowed : ec_type_qualifier_not_allowed,
                &qualifier_pos);
      qualifiers = TQ_NONE;
    }  /* if */
    if ((c99_mode || gcc_mode) && top_level_param_decl &&
        curr_token == tok_static && !static_seen) {
      /* In C99, "static" can appear after cv-qualifiers as well. */
      if (!c99_mode) {
        pos_warning(ec_static_dimension_nonstandard, &pos_curr_token);
      }  /* if */
      static_seen = TRUE;
      (void)get_token();
    }  /* if */
  }  /* if */
  size_pos = pos_curr_token;
  if (curr_token == tok_rbracket && !static_seen) {
    /* Empty brackets, indicating an incomplete array type. */
    num_of_elements = 0;
  } else if (vla_enabled && curr_token == tok_star &&
             next_token() == tok_rbracket) {
    /* [*] syntax for a VLA (should only appear in a prototype). */
    if (vla_asterisk_allowed && !static_seen &&
        (decl_scope_level != NO_SCOPE_DEPTH &&
         scope_stack[decl_scope_level].kind ==
                                          (a_scope_kind)sck_func_prototype)) {
      has_vla_asterisk = TRUE;
    } else {
      pos_error(ec_vla_with_unspecified_bound_not_allowed, &error_position);
      err = TRUE;
    }  /* if */
    /* Pass over the asterisk. */
    (void)get_token();
  } else {
    /* Scan the array size. */
    if (nonconstant_dimension_allowed || vla_allowed) {
      a_boolean  top_level_vla = vla_allowed && !dps->nested_ptr_or_ref_seen;
      a_boolean  for_new_expr = !vla_allowed;
      scan_nonconstant_dimension_expression(
              for_new_expr, top_level_vla, dps->is_evaluated_sizeof_type_arg,
              &is_constant_bound, &dim_expr, constant);
      check_assertion(is_constant_bound == (dim_expr == NULL));
#if GNU_EXTENSIONS_ALLOWED
      if (gcc_mode && dim_expr != NULL && top_level_field_decl &&
          depth_innermost_function_scope != NO_SCOPE_DEPTH) {
        /* GNU C allows variable-length array fields in local classes.
           Supporting that requires operations like sizeof and field offset
           to be computed at run time.  However, this extension is often
           only used in situations where the size does not matter.
           As a compatibility work-around, we therefore warn about the
           construct and treat the resulting array as having bound zero. */
        pos_warning(ec_vla_size_ignored, &error_position);
        set_integer_constant(constant, (a_host_large_integer)0,
                             (an_integer_kind)ik_int);
        dim_expr = NULL;
        is_constant_bound = TRUE;
        dps->vla_field_treated_as_zero_length_array = TRUE;
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    } else {
      /* Scan a bound expression expected to be a constant. */
      scan_constant_dimension_expression(constant);
      is_constant_bound = TRUE;
    }  /* if */
    if (dim_expr == NULL) {
      switch (constant->kind) {
#if UPC_EXTENSIONS_ALLOWED
        case ck_upc_mythread:
          /* MYTHREAD (and multiples thereof) is not a valid array
             dimension. */
          pos_error(ec_expr_not_constant, &error_position);
          err = TRUE;
          break;
        case ck_upc_threads:
          /* The array is dimensioned to a multiple of THREADS.  Set the
             flag and fall through to the integer case. */
          upc_threads_dimension = TRUE;
          if (!threads_dimension_allowed) {
            pos_error(ec_expr_not_constant, &error_position);
            err = TRUE;
            break;
          }  /* if */
          FALLTHROUGH
#endif /* UPC_EXTENSIONS_ALLOWED */
        case ck_integer:
          /* Array size must be greater than zero. */
          if (sign_of_integer_constant(constant) > 0) {
            num_of_elements =
                        unsigned_value_of_integer_constant(constant, &err);
            if (err) pos_error(ec_array_size_too_large, &error_position);
          } else if (((ms_extensions &&
                       (top_level_field_decl || in_class_definition())) ||
                      gnu_mode) &&
                     sign_of_integer_constant(constant) == 0) {
            /* In Microsoft C mode a field may be a zero-sized array type if
               it is the last field of the struct.  Thus
                 struct S { int a,b,c[0]; }
               is allowed, and "c[0]" has the same semantics as "c[]".  Also
               allowed in Microsoft C++ mode, as long as the class is an
               "aggregate".  Note: last-field restriction and the aggregate
               restriction in C++ are enforced in scan_class_definition.
               GNU C and C++ also allow zero-sized array types: they are
               considered complete types of zero size.  They can be used to
               achieve the same effect as flexible array members, but are
               more general. */
            num_of_elements = 0;
          } else {
            pos_error(ec_array_size_must_be_positive, &error_position);
            err = TRUE;
          }  /* if */
          break;
        case ck_template_param:
          /* Template-dependent bound.  Handled below. */
          template_dependent_bound = TRUE;
          break;
        case ck_error:
          err = TRUE;
          break;
        default:
          unexpected_condition_str("array declarator: bad constant kind");
      }  /* switch */
    }  /* if */
  }  /* if */
  if (err) {
    *new_type_ptr = error_type();
  } else {
    *new_type_ptr = alloc_type((a_type_kind)tk_array);
    (*new_type_ptr)->source_corresp.decl_position = start_pos;
    (*new_type_ptr)->variant.array.is_static = static_seen;
    copy_qualifiers(qualifiers, (*new_type_ptr)->variant.array.qualifiers);
    /* Store the array size. */
    if (has_vla_asterisk) {
      /* [*] case (C only).  Since the size of the VLA is not specified,
         there is no need to allocate a_vla_dimension. */
      (*new_type_ptr)->variant.array.is_vla = TRUE;
      (*new_type_ptr)->variant.array.is_variable_size_array = TRUE;
      il_header.vla_used = TRUE;
    } else if (dim_expr != NULL) {
      /* Expression case. */
      (*new_type_ptr)->variant.array.is_variable_size_array = TRUE;
      if (vla_allowed) {
        /* VLA case. */
        (*new_type_ptr)->variant.array.is_vla = TRUE;
        il_header.vla_used = TRUE;
        /* A VLA dimension entry will be created to record the array
           dimension expression. */
        if (scope_stack[decl_scope_level].kind ==
                                           (a_scope_kind)sck_func_prototype) {
          /* For a VLA in a function parameter declaration, generation of the
             stmk_set_vla_size statement is delayed until it is determined
             that the parameter is part of a function definition, not a
             declaration.  */
          add_vla_fixup_entry(*new_type_ptr, dim_expr, (a_symbol_ptr)NULL,
                              &size_pos);
        } else {
          /* Create a VLA dimension entry to record the expression. */
          a_vla_dimension_ptr  vdp;

          vdp = make_vla_dimension(*new_type_ptr, dim_expr,
                                   /*in_prototype_scope=*/FALSE,
                                   &size_pos);
          if (in_expression_context()) {
            /* Don't put out an stmk_set_vla_size statement if this is an
               expression context (e.g., a sizeof or cast). */
          } else {
            /* Generate an stmk_set_vla_size statement for the VLA to indicate
               when (at runtime) the VLA dimension expression is to be
               evaluated to fix the size of the array. */
            set_vla_size_statement(vdp, &start_pos);
          }  /* if */
        }  /* if */
      } else {
        (*new_type_ptr)->variant.array.variant.element_count_expr = dim_expr;
      }  /* if */
    } else {
      /* Not an expression or VLA bound, so either [] or a constant bound. */
      a_constant_ptr         il_constant = NULL;
      a_memory_region_number region_to_switch_back_to;

      /* Make sure any constants are allocated in the file scope memory
         region, because they will be pointed to by the array type, which
         is in the file scope memory region. */
      switch_to_file_scope_region(&region_to_switch_back_to);
      if (template_dependent_bound) {
        /* Template-dependent bound (constant but not a known value). */
        check_assertion(constant_is(constant, ck_template_param));
        if (constant_is_shareable(constant)) {
          il_constant = alloc_shareable_constant(constant);
        } else {
          a_template_param_constant_kind tkind =
                                         constant->variant.template_param.kind;
          a_boolean expr_case = (tkind == tpck_expression);
          a_boolean sizeof_case = (tkind == tpck_sizeof ||
                                   tkind == tpck_datasizeof ||
                                   tkind == tpck_alignof ||
                                   tkind == tpck_uuidof ||
                                   tkind == tpck_typeid ||
                                   tkind == tpck_noexcept);
          il_constant = alloc_unshared_constant_full(constant,
                                                     /*source_in_il=*/FALSE,
                                                     /*suppress_copy=*/
                                                     (expr_case||sizeof_case));
          do_fs_constant_fixup(il_constant);
          /* In a couple of cases, we can record a local expr ref and
             keep a function-scope expression. */
          if (expr_case) {
            make_bound_expr_referenceable_from_file_scope(
                             &il_constant->variant.template_param.variant.expr,
                             *new_type_ptr,
                             /*dep=*/TRUE);
          } else if (sizeof_case) {
            make_bound_expr_referenceable_from_file_scope(
                &il_constant->variant.template_param.variant.templ_sizeof.expr,
                *new_type_ptr,
                /*dep=*/TRUE);
          }  /* if */
        }  /* if */
        (*new_type_ptr)->variant.array.variant.element_count_constant =
                                                                   il_constant;
        (*new_type_ptr)->variant.array.is_template_dependent_size_array = TRUE;
      } else {
        /* Non-dependent constant bound or []. */
        if (is_constant_bound) {
          /* Save the constant for the bound.  If it has an attached expression
             it may need to be referred to indirectly if the expression is
             allocated in function scope memory. */
          il_constant = alloc_shareable_constant(constant);
          if (il_constant->is_named_constant_definition) {
            /* We must not copy the backing expression from the bound
               constant if il_constant is the definition of a named
               constant.  In such a case, the backing expression will refer
               to the shared constant, and copying it into il_constant
               would create a loop in the IL. */
          } else if (constant->expr != NULL &&
                     is_operation_node(constant->expr) &&
                     node_operator_is(constant->expr, eok_dot_member_call) &&
                     constant->expr->variant.operation.operands->next->kind ==
                                               (an_expr_node_kind)enk_lambda) {
            /* Do not copy the backing expression from the bound constant
               if it results from invoking a lambda expression, as the
               lambda may have references to local variables. */
          } else if (depth_innermost_function_scope != NO_SCOPE_DEPTH) {
            /* Preserve the bound expression, which will not have been
               copied if it refers to local variables, and ensure that it
               can be referenced from the file-scope type entry.  Note that
               il_constant will be unshared in this case because of the
               non-NULL backing expression in the source constant, even
               though the backing expression was cleared in il_constant. */
            il_constant->expr = constant->expr;
            make_bound_expr_referenceable_from_file_scope(&il_constant->expr,
                                                          *new_type_ptr,
                                                          /*dep=*/FALSE);
          }  /* if */
          (*new_type_ptr)->variant.array.bound_constant = il_constant;
        }  /* if */
        (*new_type_ptr)->variant.array.variant.number_of_elements =
                                                              num_of_elements;
        if ((((gnu_mode || ms_version_is(>=1900)) && is_constant_bound) ||
             (gpp_version_is(<60000) && top_level_field_decl)) &&
             num_of_elements == 0) {
          /* Record the fact that we saw a GNU C zero-length array.  GNU C++
             compilers treat flexible array members ([]) as zero-length arrays
             ([0]) until GCC 6.x.  Recent versions of MSVC also accept zero-
             length arrays. */
          (*new_type_ptr)->variant.array.bound_is_zero = TRUE;
          if (is_constant_bound) {
            report_gnu_extension_if_needed(
                             &size_pos, ec_zero_length_array_is_gnu_extension);
          }  /* if */
        }  /* if */
      }  /* if */
      switch_back_to_original_region(region_to_switch_back_to);
    }  /* if */
    if (gnu_mode && !c99_mode && (*new_type_ptr)->variant.array.is_vla) {
      report_gnu_extension_if_needed(&size_pos, ec_vla_is_nonstandard);
    }  /* if */
#if UPC_EXTENSIONS_ALLOWED
    /* Record whether the dimension is a multiple of THREADS. */
    (*new_type_ptr)->variant.array.is_threads_dimension =
                                                        upc_threads_dimension;
#endif /* UPC_EXTENSIONS_ALLOWED */
    /* The size of the array (in bytes) is updated in 
       add_to_derived_type_list. */
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    decl_pos_block->declarator_range.end = end_pos_curr_token;
  }  /* if */
  curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Check for closing right bracket. */
  (void)required_token(tok_rbracket, ec_exp_rbracket);
  remove_stop_token(tok_rbracket);
  copy_source_position(start_pos, error_position);
  scan_declarator_attributes(dps, new_type_ptr);
  release_local_constant(&constant);
  db_exit();
}  /* array_declarator */


/*
A structure to track various modifications that might have been applied to a
pointer declarator.  This includes not only the standard const/volatile
qualifiers, but also a variety of Microsoft extensions (e.g., __ptr64/__ptr32
and calling conventions).
*/
typedef struct a_pointer_modifier_state {
  a_type_qualifier_set
		qualifiers;
			/* Standard and nonstandard qualifiers. */
  a_source_position
		qualifiers_pos;
			/* The position of the first qualifier seen. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_call_conv_descr
		cc_descr;
			/* Microsoft calling convention. */
  a_variable_ptr
		based_var;
			/* The variable specified by a __based modifier (if
			   any). */
  a_source_position
		based_pos;
			/* The source position of the __based modifier (if
			   any). */
  a_boolean
		microsoft_w64;
			/* TRUE if the __w64 token was seen. */
  a_source_position
		microsoft_w64_pos;
			/* The source position of the __w64 token (if any). */
  a_pointer_modifier_set
		modifiers;
			/* A bit set describing the presence of certain
			   additional modifiers (like __ptr32). */
			   
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
} a_pointer_modifier_state;


static void clear_pointer_modifier_state(a_pointer_modifier_state  *ptr_mods)
/*
Clear the pointer modifiers structure (except for some position information
that isn't accessed unless the associated flag has been set).
*/
{
  ptr_mods->qualifiers = TQ_NONE;
  ptr_mods->qualifiers_pos = null_source_position;
#if MICROSOFT_EXTENSIONS_ALLOWED
  clear_call_conv_descr(&ptr_mods->cc_descr);
  ptr_mods->based_var = NULL;
  ptr_mods->microsoft_w64 = FALSE;
  ptr_mods->modifiers = PM_NONE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
}  /* clear_pointer_modifier_state */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void scan_microsoft_calling_convention(a_calling_convention *call_conv)
/*
Scan a list of Microsoft calling conventions (__cdecl, __fastcall, __stdcall,
__thiscall, __clrcall).  Only one calling convention may be specified, but the
same specifier may appear more than once.  It can be assumed that
is_microsoft_calling_convention is TRUE on entry.  *call_conv on entry
has any calling convention previously scanned, or is cc_default if there
was no previous calling convention.  On exit it is set to the calling
convention scanned on this call.
*/
{
  set_err_pos_to_curr_token();
  do {
    a_calling_convention new_call_conv = cc_default;
    switch (curr_token) {
      case tok_cdecl:
        new_call_conv = cc_cdecl;
        break;
      case tok_vectorcall:
        new_call_conv = cc_vectorcall;
        break;
      case tok_fastcall:
        new_call_conv = cc_fastcall;
        break;
      case tok_stdcall:
        new_call_conv = cc_stdcall;
        break;
      case tok_thiscall:
        new_call_conv = cc_thiscall;
        break;
      case tok_clrcall:
        if (cppcli_enabled) {
          new_call_conv = cc_clrcall;
        } else {
          pos_error(ec_clrcall_requires_cppcli, &error_position);
          goto skip_token;
        }  /* if */
        break;
      default: unexpected_condition();
    }  /* switch */
    if (*call_conv != cc_default) {
      /* A calling convention was specified. */
      if (normalized_calling_conv(*call_conv) !=
                                     normalized_calling_conv(new_call_conv)) {
        /* The new calling convention does not agree with the old one. */
        pos_error(ec_conflicting_calling_conventions, &error_position);
      } else {
        /* The new and old calling conventions are the same. */
        pos_warning(ec_dupl_calling_convention, &error_position);
      }  /* if */
    }  /* if */
    *call_conv = new_call_conv;
skip_token:
    (void)get_token();
  } while (is_microsoft_calling_convention(curr_token));
}  /* scan_microsoft_calling_convention */


static
void update_calling_convention(a_type_ptr	      *type,
			       a_call_conv_descr_ptr  p_calling_convention,
                               a_decl_parse_state     *dps,
                               a_source_position      *decl_pos)
/*
Determine the validity of the given calling convention specified for the
given type in a declaration described by *dps.  If appropriate, issue a
diagnostic at the given position.
*/
{
  a_calling_convention           calling_convention;
  a_boolean                      discard = FALSE;
  an_error_severity              discard_sev = (an_error_severity)es_remark;
  a_routine_type_supplement_ptr  rtsp;

  calling_convention = p_calling_convention->call_conv;
  if (*type == NULL) {
    /* Null type -- the calling convention will be discarded. */
    discard = TRUE;
  } else if (calling_convention != (a_calling_convention)cc_default) {
    if (!is_function_type(*type)) {
      /* A calling convention on a non-function type is ignored. */
      pos_remark(ec_calling_convention_ignored_for_type, decl_pos);
    } else {
      /* A calling convention applied to a function type. */
      if (is_qualified_type(*type)) {
        /* The type involves qualifiers on top of a routine type.  This is an
           unusual situation that can only occur when a type qualifier is
           applied to a typedef that points to a routine type.  If a calling
           convention were allowed to be declared on top of that, it would
           cause the underlying routine type to be modified; but that would
           affect the meaning of the typedef that points to it.  Another
           approach would be to copy the routine type, add the calling
           convention to the copy, and then reapply the qualifier directly to
           the copy, but this has implementation difficulties: among other
           things, without a typedef in the resulting type tree the type can't
           be represented outside the IL (e.g., in diagnostics).  Here's an
           example of what is disallowed:
             typedef void F(int);
             typedef const F CF;
             extern CF __stdcall f;     // __stdcall is not allowed here
           This should be a very rarely encountered limitation, since type
           qualifiers are uncommon on routine types to begin with. */
        /* An error is issued, since to just to ignore the declaration (even
           with a warning) could give the user a false impression. */
        pos_error(ec_calling_convention_not_allowed, decl_pos);
      } else {
        a_type_ptr  tp = *type;
        a_boolean   any_typedefs = FALSE;

        /* Skip past any typerefs.  See if any of them are typedefs. */
        while (tp->kind == (a_type_kind)tk_typeref) {
          any_typedefs |= (int)(typeref_is_typedef(tp));
          tp = tp->variant.typeref.type;
        }  /* while */
        check_assertion(tp->kind == (a_type_kind)tk_routine);
        rtsp = tp->variant.routine.extra_info;
        if (rtsp->has_ellipsis) {
          /* Calling convention for functions with variable argument lists
             is always __cdecl.  Whether or not __cdecl was explicitly
             specified, add it to the type. */
          rtsp->calling_convention = (a_calling_convention)cc_cdecl;
          rtsp->explicit_calling_convention = TRUE;
          if (calling_convention != (a_calling_convention)cc_cdecl) {
            /* For __thiscall or __clrcall, an error should be issued.  The
               other cases only elicit a remark (issued below). */
            if (calling_convention == (a_calling_convention)cc_thiscall) {
              pos_error(ec_vararg_thiscall, &error_position);
            } else if (calling_convention ==
                                           (a_calling_convention)cc_clrcall) {
              pos_error(ec_vararg_clrcall, &error_position);
            } else {
              discard = TRUE;
            }  /* if */
          }  /* if */
        } else if (rtsp->calling_convention ==
                                           (a_calling_convention)cc_clrcall &&
                   calling_convention != (a_calling_convention)cc_clrcall &&
                   !dps->is_explicit_instantiation) {
          /* A calling convention of __clrcall cannot be "overridden" by a
             different convention.  (For explicit instantiations, do not
             discard the explicit calling convention: It affects deduction
             and will likely trigger an error later on because the template
             presumably doesn't have the same convention.) */
          discard = TRUE;
          discard_sev = (an_error_severity)es_warning;
        } else if ((rtsp->assoc_routine_is_ctor ||
                    rtsp->assoc_routine_is_dtor) &&
                   calling_convention != (a_calling_convention)cc_thiscall &&
                   calling_convention != (a_calling_convention)cc_clrcall) {
          /* Microsoft compilers ignore __cdecl and __fastcall specifiers on
             constructors with a warning.  They silently also ignore __stdcall.
             All these cases are treated as __thiscall instead.  We discard all
             calling conventions on constructors and destructors with a
             warning, except __thiscall and __clrcall. */
          discard = TRUE;
          discard_sev = (an_error_severity)es_warning;
        } else if (rtsp->calling_convention != calling_convention) {
          /* The underlying routine type needs to be updated. */
          if (any_typedefs) {
            /* Copy the routine type, since it's about to be modified and we
               don't want to change the meaning of the typedef.  But that
               means *type has to be adjusted. */
            tp = copy_routine_type_with_param_types(tp,
                                                   /*copy_default_args=*/TRUE);
            *type = tp;
            rtsp = tp->variant.routine.extra_info;
          }  /* if */
          rtsp->calling_convention = calling_convention;
          rtsp->explicit_calling_convention = TRUE;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  if (discard) {
    /* Issue a diagnostic (remark or warning) indicating that the calling
       convention has no effect. */
    pos_diagnostic(discard_sev, ec_calling_convention_ignored,
                   &p_calling_convention->position);
  }  /* if */
  /* Whether or not we were able to apply the calling convention,
     reset it so that the caller does not attempt to reuse it later. */
  clear_call_conv_descr(p_calling_convention);
}  /* update_calling_convention */


static a_variable_ptr scan_based_modifier(void)
/*
Scan the Microsoft __based modifier.  The syntax is

	__based(identifier)

The identifier must name a variable with pointer type.  Return a
pointer to the variable.  If the identifier is undefined, or
is not a variable with pointer type, return NULL.
*/
{
  a_variable_ptr	var = NULL;

  check_assertion(curr_token == tok_based);
  /* Bypass the __based token. */
  (void)get_token();
  if (required_token(tok_lparen, ec_exp_lparen)) {
    add_stop_token(tok_rparen);
    if (!is_generalized_identifier_start(GID_NO_OPTIONS)) {
      syntax_error(ec_exp_identifier);
      /* Flush tokens to the right paren. */
      flush_tokens();
    } else {
      /* Call an expression routine to scan the identifier.  The routine
         will return NULL if an error occurred while scanning the variable. */
      var = based_variable();
    }  /* if */
    remove_stop_token(tok_rparen);
    /* Bypass the closing parenthesis. */
    (void)required_token(tok_rparen, ec_exp_rparen);
  }  /* if */
  return var;
}  /* scan_based_modifier */


static void issue_invalid_based_error(a_source_position *pos)
/*
Issue an error that a __based modifier is not allowed in the
indicated position.
*/
{
  pos_error(ec_based_not_allowed_here, pos);
}  /* issue_invalid_based_error */

/*
Macro that tests whether a based symbol is present and, if so, issues
an error and resets the symbol.  This macro expands to nothing when
Microsoft extensions are not allowed.
*/
#define based_not_allowed_here(var, pos)			      \
  { if ((var) != NULL) issue_invalid_based_error(&pos); var = NULL; }

#else  /* !MICROSOFT_EXTENSIONS_ALLOWED */

/*
Expands to nothing when Microsoft extensions are not being used.
*/
#define based_not_allowed_here(var, pos)  /* Nothing */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED

static void collect_pointer_declarator_extended_qualifiers(
                                  ARG_UNUSED a_boolean      plain_ptr_seen,
                                  ARG_UNUSED a_boolean      ptr_to_member_seen,
                                  a_pointer_modifier_state  *ptr_mods,
                                  a_decl_pos_block_ptr      decl_pos_block)
/*
Collect a set of pointer declarator qualifiers provided as an extension
(e.g., for Microsoft compatibility).  Aside from the standard const/volatile,
support for near and far may be enabled (e.g., in Microsoft 16-bit mode),
and Microsoft mode also allows other modifiers, notably __based and calling
conventions like __cdecl.  Scan all of those, and return information about what
was scanned in *ptr_mods (the caller need not initialize this structure).
It's permissible for the input to contain no qualifiers. If Microsoft extended
decl specifiers, introduced by __declspec, are encountered, they are
scanned and thrown away with a warning.  plain_ptr_seen is TRUE when scanning
extended qualifiers following a asterisk ("*") indicating a plain pointer.
ptr_to_member_seen is TRUE when scanning extended qualifiers following a
pointer-to-member operator (of the form X::* for some class type X).
Additional position information is recorded in *decl_pos_block.
*/
{
  a_type_qualifier_set new_qualifiers, duplicates;

  clear_pointer_modifier_state(ptr_mods);
  for (;;) {
    if (is_type_qualifier() or_is_near_or_far()) {
      /* Normal qualifiers like const, and declarator-only qualifiers like
         near. */
      ptr_mods->qualifiers_pos = pos_curr_token;
      new_qualifiers = collect_type_qualifiers(decl_pos_block,
                                               (a_upc_block_size *)NULL);
      duplicates = (new_qualifiers & ptr_mods->qualifiers);
#if NEAR_AND_FAR_ALLOWED
      if (near_and_far_enabled()) {
        if ((new_qualifiers & TQ_NEAR) && (ptr_mods->qualifiers & TQ_FAR )) {
          /* Incompatible near and far specifications. */
          pos_error(ec_mem_attrib_incompatible, &error_position);
          new_qualifiers &= ~TQ_NEAR;
          duplicates &= ~TQ_NEAR;
        }  /* if */
        if ((new_qualifiers & TQ_FAR ) && (ptr_mods->qualifiers & TQ_NEAR)) {
          /* Incompatible near and far specifications. */
          pos_error(ec_mem_attrib_incompatible, &error_position);
          new_qualifiers &= ~TQ_FAR;
          duplicates &= ~TQ_FAR;
        }  /* if */
        /* Check for repetition of "near" or "far".  The Microsoft compiler
           gives only a warning for these cases, so we do too. */
        if (duplicates & (TQ_NEAR | TQ_FAR)) {
          pos_warning(ec_dupl_mem_attrib, &error_position);
          duplicates &= ~(TQ_NEAR | TQ_FAR);
        }  /* if */
      }  /* if */
#endif /* NEAR_AND_FAR_ALLOWED */
      /* Check for duplicates other than "near" and "far". */
      if (duplicates != TQ_NONE) {
        /* The Microsoft compiler gives only a warning for duplicates, so
           we do too. */
        pos_warning(ec_dupl_type_qualifier, &error_position);
      }  /* if */
      ptr_mods->qualifiers |= new_qualifiers;
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (ms_extensions) {
      if (is_microsoft_calling_convention(curr_token)) {
        /* Calling conventions like __cdecl. */
        ptr_mods->cc_descr.position = pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
        if (decl_pos_block != NULL) {
          decl_pos_block->declarator_range.end = end_pos_curr_token;
        }  /* if */
        curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        scan_microsoft_calling_convention(&ptr_mods->cc_descr.call_conv);
      } else if (curr_token == tok_based) {
        /* __based. */
        if (ptr_mods->based_var != NULL) {
          /* __based appears more than once. */
          pos_error(ec_dupl_type_qualifier, &error_position);
        }  /* if */
        ptr_mods->based_pos = pos_curr_token;
        ptr_mods->based_var = scan_based_modifier();
      } else if (curr_token == tok_declspec) {
        /* Scan the decl-modifiers.  The Microsoft compiler appears to accept
           and ignore __declspec declarations that appear during declarator
           processing -- there is no evidence that the decl-modifiers are ever
           actually applied to the function or variable being declared. */
        scan_and_discard_extended_decl_modifiers();
      } else if (curr_token == tok_mutable) {
        /* The Microsoft compiler appears to accept and ignore "mutable"
           during declarator processing.  Issue a warning and continue. */
        pos_warning(ec_mutable_not_allowed, &error_position);
#if EXTRA_SOURCE_POSITIONS_IN_IL
        if (decl_pos_block != NULL) {
          decl_pos_block->declarator_range.end = end_pos_curr_token;
        }  /* if */
        curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        (void)get_token();
      } else if (curr_token == tok_microsoft_w64) {
        /* Microsoft compilers accept the __w64 keyword on pointer
           declarators. */
        if (plain_ptr_seen) {
          ptr_mods->microsoft_w64 = TRUE;
          ptr_mods->microsoft_w64_pos = pos_curr_token;
        } else {
          /* The "__w64" token was not expected. */
          pos_error(ec_invalid_type_for_w64, &error_position);
        }  /* if */
        (void)get_token();
      } else if (curr_token == tok_microsoft_ptr32) {
        if (!plain_ptr_seen && !ptr_to_member_seen) {
          pos_error(ec_microsoft_ptr_width_must_follow_star, &error_position);
        } else if ((ptr_mods->modifiers & PM_PTR64) != 0) {
          pos_error(ec_microsoft_ptr_width_conflict, &error_position);
        } else if ((ptr_mods->modifiers & PM_PTR32) != 0) {
          pos_warning(ec_dupl_type_qualifier, &error_position);
        } else {
          ptr_mods->modifiers |= PM_PTR32;
        }  /* if */
        (void)get_token();
      } else if (curr_token == tok_microsoft_ptr64) {
        if (!plain_ptr_seen && !ptr_to_member_seen) {
          pos_error(ec_microsoft_ptr_width_must_follow_star, &error_position);
        } else if ((ptr_mods->modifiers & PM_PTR32) != 0) {
          pos_error(ec_microsoft_ptr_width_conflict, &error_position);
        } else if ((ptr_mods->modifiers & PM_PTR64) != 0) {
          pos_warning(ec_dupl_type_qualifier, &error_position);
        } else {
          ptr_mods->modifiers |= PM_PTR64;
        }  /* if */
        (void)get_token();
      } else if (curr_token == tok_microsoft_sptr) {
        if (ptr_to_member_seen) {
          pos_error(ec_microsoft_ptr_signedness_on_ptr_to_member,
                    &error_position);
        } else if (!plain_ptr_seen) {
          pos_error(ec_microsoft_ptr_signedness_must_follow_star,
                    &error_position);
        } else if ((ptr_mods->modifiers & PM_UPTR) != 0) {
          pos_error(ec_microsoft_ptr_signedness_conflict, &error_position);
        } else if ((ptr_mods->modifiers & PM_SPTR) != 0) {
          pos_warning(ec_dupl_type_qualifier, &error_position);
        } else {
          ptr_mods->modifiers |= PM_SPTR;
        }  /* if */
        (void)get_token();
      } else if (curr_token == tok_microsoft_uptr) {
        if (ptr_to_member_seen) {
          pos_error(ec_microsoft_ptr_signedness_on_ptr_to_member,
                    &error_position);
        } else if (!plain_ptr_seen) {
          pos_error(ec_microsoft_ptr_signedness_must_follow_star,
                    &error_position);
        } else if ((ptr_mods->modifiers & PM_SPTR) != 0) {
          pos_error(ec_microsoft_ptr_signedness_conflict, &error_position);
        } else if ((ptr_mods->modifiers & PM_UPTR) != 0) {
          pos_warning(ec_dupl_type_qualifier, &error_position);
        } else {
          ptr_mods->modifiers |= PM_UPTR;
        }  /* if */
        (void)get_token();
      } else {
        /* Something else; exit the loop. */
        break;
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else {
      break;
    }  /* if */
  }  /* for */
}  /* collect_pointer_declarator_extended_qualifiers */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
#if NEAR_AND_FAR_ALLOWED

static void check_for_addition_of_incompatible_qualifiers(
                                              a_type_ptr           type,
                                              a_type_qualifier_set *qualifiers,
                                              a_source_position    *pos)
/*
The memory attribute qualifiers in the set *qualifiers are about to be added
to the indicated type.  If there is some conflict between the new qualifiers
and the existing ones (explicit and implied), issue an error (at position
*pos) and remove the incompatible qualifiers from *qualifiers.
*/
{
  a_type_qualifier_set new_qualifiers = *qualifiers;

  if (new_qualifiers & (TQ_NEAR | TQ_FAR)) {
    /* Adding a memory attribute.  See if there is a conflicting one
       already. */
    a_type_qualifier_set old_qualifiers = get_original_type_qualifiers(type);
    if (((old_qualifiers & TQ_NEAR) && (new_qualifiers & TQ_FAR)) ||
        ((new_qualifiers & TQ_NEAR) && (old_qualifiers & TQ_FAR))) {
      /* Incompatible memory attributes. */
      pos_error(ec_mem_attrib_incompatible, pos);
      new_qualifiers &= ~(TQ_NEAR | TQ_FAR);
      *qualifiers = new_qualifiers;
    }  /* if */
  }  /* if */
}  /* check_for_addition_of_incompatible_qualifiers */

#endif /* NEAR_AND_FAR_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED

static void apply_microsoft_ptr_modifiers(
                                          a_type_ptr                *type,
                                          a_pointer_modifier_state  *ptr_mods)
/*
Replace *type by a similar type with added microsoft-specific modifiers as
described by *ptr_mods (including __ptr32/__ptr64/__sptr/__uptr modifiers,
__w64 annotation, and __based variable specifiers).  The given type must be a
(possibly qualified) pointer or pointer-to-member type.
*/
{
  a_type_qualifier_set  qualifiers = get_type_qualifiers(*type);
  a_type_ptr            plain_type = skip_typerefs(*type), copy;
  a_boolean             copy_needed = ptr_mods->microsoft_w64 ||
                                      ptr_mods->based_var != NULL;

  if (ptr_mods->modifiers != PM_NONE || copy_needed) {
    /* Create a modified copy of the unqualified type and then reapply the
       qualifiers (if any). */
    if (plain_type->kind == (a_type_kind)tk_pointer) {
      /* A plain pointer type: Modify it according to *ptr_mods.  If __w64 or
         __based were applied, disable reusing an entry from the "based" list
         by calling make_pointer_type_full with a NULL underlying type. */
      a_type_ptr  tpt = copy_needed ? NULL : plain_type->variant.pointer.type;
      copy = make_pointer_type_full(tpt, ptr_mods->modifiers);
      copy->variant.pointer.type = plain_type->variant.pointer.type;
      copy->has_microsoft_w64_specifier = ptr_mods->microsoft_w64;
      copy->variant.pointer.base_variable = ptr_mods->based_var;
      /* Mark the base variable as consumed. */
      ptr_mods->based_var = NULL;
    } else {
      check_assertion(plain_type->kind == (a_type_kind)tk_ptr_to_member);
      if (pm_member_type(plain_type) == NULL) {
        /* The type is under construction.  We can therefore just modify it
           "in place". */
        copy = plain_type;
        copy->variant.ptr_to_member.modifiers = ptr_mods->modifiers;
      } else {
        copy = ptr_to_member_type_full(pm_member_type(plain_type),
                                       pm_class_type(plain_type),
                                       pm_orig_class_type(plain_type),
                                       ptr_mods->modifiers);
      }  /* if */
      /* Issue an error if this was preceded by __based. */
      based_not_allowed_here(ptr_mods->based_var, ptr_mods->based_pos);
      if (ptr_mods->microsoft_w64) {
        pos_error(ec_invalid_type_for_w64, &ptr_mods->microsoft_w64_pos);
      }  /* if */
    }  /* if */
    *type = make_qualified_type(copy, qualifiers);
  }  /* if */
}  /* apply_microsoft_ptr_modifiers */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

a_type_ptr pointer_declarator(
                   a_type_ptr                       specifiers_type,
                   a_decl_parse_state               *state,
                   a_boolean                        reference_allowed,
                   ARG_UNUSED a_call_conv_descr_ptr left_calling_convention,
                   ARG_UNUSED a_call_conv_descr_ptr unbound_calling_convention,
                   ARG_UNUSED a_type_qualifier_set  *left_qualifiers,
                   ARG_UNUSED a_type_qualifier_set  *unbound_qualifiers,
                   a_boolean                        *ptr_to_member_scanned,
                   a_decl_pos_block_ptr             decl_pos_block)
/*
Scan the pointer component of a declarator.  This is "*", "&", "&&", or "C::*"
(where C is a class type) optionally followed by "const" and/or "volatile".
"&" (lvalue reference) and "C::*" (pointer-to-member) are C++ features; "&&"
is a C++11 extension to declare "rvalue references".  In C++/CLI mode the
declarator operators "^" (handle) and "%" (tracking reference) are also
possible.

This routine actually scans a sequence of pointer declarators.
The pointer type modifiers are placed on top of the type passed in as
specifiers_type, and a pointer to the complete type is returned.
specifiers_type is NULL for a nested declarator (one enclosed in
parentheses); in that case the pointer type modifiers are built up
but nothing is attached to the bottom-most modifier.  In either case,
ptr_to_member_scanned is set to TRUE if a pointer-to-member declarator
was scanned, and to FALSE otherwise.

*state describes some state information about the current declaration.
reference_allowed is FALSE if reference declarators ("&" and "&&") should be
disallowed (e.g., in a new-expression).

In Microsoft mode, the Microsoft __cdecl, __stdcall, __fastcall, __thiscall,
and __clrcall are recognized as calling conventions.  The handling of calling
conventions is intended to match the behavior of the Microsoft 32-bit C/C++
compiler.  Calling conventions are allowed on function types and pointer to
function types.  They are permitted on object declarations, but have no
meaning.  They are not allowed on pointers to objects or on references.

left_calling_convention and unbound_calling_convention are pointers
to calling conventions.  The values of these calling conventions
are returned by this routine.  If the pointer declarator looks like

	__cdecl * __cdecl * __cdecl

the first calling convention is returned in *left_calling_convention
(but only if specifiers_type is NULL; otherwise, it is applied directly
to the specifiers type); the middle one is discarded (by applying it
to the pointer type); and the last one (not followed by a pointer
operator) is returned in *unbound_calling_convention.  If
unbound_calling_convention is NULL, an unbound calling convention
is just thrown away.

Also in Microsoft mode, type qualifiers can appear at the beginning of
the declarator, e.g.,

  int i, const j, const *k;  // Declares j as "const int", k as "int *"

This routine will see them only at the beginning of a declarator that
is not immediately next to its specifiers list, as above, because otherwise
the qualifiers are processed as part of the specifiers.

Type qualifiers get processing similar to that for calling conventions.
If a pointer declarator looks like

	far * far * far

*left_qualifiers is used to return the first qualifier (but only if
specifiers_type is NULL; otherwise, the qualifier is applied directly
to the specifiers type); the middle qualifier is handled internally
by applying it to the pointer type; and the last (unbound) qualifier
is returned in *unbound_qualifiers.  If unbound_qualifiers is NULL,
unbound qualifiers are just thrown away.

The pointer modifiers __ptr32 and __ptr64 (yet another Microsoft extension)
are also accepted by this routine: They directly affect the type entry for the
pointer they qualify (i.e., these are not modeled as "tk_typeref" entries).

Microsoft extended decl modifiers are also scanned, but they are ignored
(which is what the Microsoft compiler itself appears to do).
*/
{
  a_type_ptr                complete_type = specifiers_type;
  a_boolean                 err = FALSE;
  a_pointer_modifier_state  ptr_mods;
  a_type_ptr                class_type = NULL;
  a_type_ptr                rout_type = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
  a_pointer_modifier_state  pending_ptr_mods;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
  a_upc_block_size          upc_block_size = UPC_BLOCK_SIZE_NONE;
  a_boolean                 ref_to_ref_allowed = TRUE;

  db_enter(3, "pointer_declarator");
  *ptr_to_member_scanned = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
  if (ms_extensions or_near_and_far_enabled()) {
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* Clear parameters used to return left/unbound qualifiers. */
    if (left_calling_convention != NULL) {
      clear_call_conv_descr(left_calling_convention);
    }  /* if */
    if (unbound_calling_convention != NULL) {
      clear_call_conv_descr(unbound_calling_convention);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (left_qualifiers != NULL) *left_qualifiers = TQ_NONE;
    if (unbound_qualifiers != NULL) *unbound_qualifiers = TQ_NONE;
    /* Scan qualifiers that precede the first pointer or reference, e.g.,
         int far *p;
    */
    collect_pointer_declarator_extended_qualifiers(
                           /*ptr_op_seen=*/FALSE, /*ptr_to_member_seen=*/FALSE,
                           &pending_ptr_mods, decl_pos_block);
  } else {
    clear_pointer_modifier_state(&pending_ptr_mods);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
  clear_pointer_modifier_state(&ptr_mods);
  /* Loop while there are pointer declarators. */
  for (;;) {
    /* See if there is a pointer declarator. */
    a_boolean  another_pointer_declarator = FALSE;
    a_boolean  ptr_to_member_case = FALSE, rvalue_ref_case = FALSE;
    a_boolean  plain_ptr = (curr_token == tok_star), managed_type = FALSE;
    if ((plain_ptr ||
         (reference_allowed && (curr_token == tok_ampersand ||
                                (rvalue_references_enabled &&
                                 curr_token == tok_and_and))))) {
      /* A pointer "*", ordinary ("lvalue") reference "&", or rvalue
         reference "&&". */
      another_pointer_declarator = TRUE;
      if (curr_token == tok_and_and) {
        rvalue_ref_case = TRUE;
        report_gnu_cpp11_extension_if_needed(
                              &pos_curr_token, ec_rvalue_references_is_cpp11);
      }  /* if */
    } else if (!C_mode() && is_ptr_to_member_declarator_start()) {
      /* A pointer-to-member "Name::*". */
      another_pointer_declarator = TRUE;
      ptr_to_member_case = TRUE;
      *ptr_to_member_scanned = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (cli_or_cx_enabled &&
               (curr_token == tok_excl_or ||  curr_token == tok_caret_caret ||
                (reference_allowed && curr_token == tok_remainder))) {
      /* C++/CLI declarator operators "^" (handle) or "%" (tracking
         reference). */
      another_pointer_declarator = TRUE;
      managed_type = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
    /* Exit the loop if there is not another pointer declarator. */
    if (!another_pointer_declarator) break;
    /* Any qualifiers that were previously considered "top level" turned out
       not to be at the top level after all. */
    state->eff_top_level_cv_quals = TQ_NONE;
    set_err_pos_to_curr_token();
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
    if (pending_ptr_mods.qualifiers != TQ_NONE) {
      /* Apply pending qualifiers to the complete type being built up, now
         that we know those are not unbound qualifiers. */
#if NEAR_AND_FAR_ALLOWED
      if ((pending_ptr_mods.qualifiers & ~(TQ_NEAR | TQ_FAR)) != TQ_NONE) {
        /* Drop qualifiers like const/volatile because Microsoft drops
           them:
             int p, const *q;
           q has type "int *", not "const int *".  The only qualifiers
           like this that can be dropped are from the qualifiers collected
           above before the first iteration of the loop. */
        pos_warning(ec_type_qualifier_ignored,
                    &pending_ptr_mods.qualifiers_pos);
        pending_ptr_mods.qualifiers &= (TQ_NEAR | TQ_FAR);
      }  /* if */
      if (pending_ptr_mods.qualifiers != TQ_NONE) {
        /* Some qualifiers like near were specified.  Apply them to
           the complete type or (at the beginning of a nested declarator)
           return them to the caller.  Note that qualifiers like const
           do not get here (they're handled at the end of the loop). */
        if (complete_type != NULL) {
          check_for_addition_of_incompatible_qualifiers(
                                  complete_type, &pending_ptr_mods.qualifiers,
                                  &pending_ptr_mods.qualifiers_pos);
          complete_type = make_qualified_type(complete_type,
                                              pending_ptr_mods.qualifiers);
        } else if (left_qualifiers != NULL) {
          /* Return left-most qualifiers to the caller. */
          *left_qualifiers = pending_ptr_mods.qualifiers;
        }  /* if */
        pending_ptr_mods.qualifiers = TQ_NONE;
      }  /* if */
#else /* !NEAR_AND_FAR_ALLOWED */
      pos_warning(ec_type_qualifier_ignored, &pending_ptr_mods.qualifiers_pos);
      pending_ptr_mods.qualifiers = TQ_NONE;
#endif /* NEAR_AND_FAR_ALLOWED */
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (ms_extensions) {
      if (pending_ptr_mods.cc_descr.call_conv !=
                                           (a_calling_convention)cc_default) {
        /* A calling convention was specified.  Apply it to the complete
           type or (at the beginning of a nested declaration) return it to
           the caller. */
        if (complete_type != NULL) {
          update_calling_convention(
                                  &complete_type, &pending_ptr_mods.cc_descr,
                                  state, &pending_ptr_mods.cc_descr.position);
        } else {
          /* Return left-most calling convention to the caller. */
          check_assertion(left_calling_convention != NULL);
          *left_calling_convention = pending_ptr_mods.cc_descr;
          clear_call_conv_descr(&pending_ptr_mods.cc_descr);
        }  /* if */
      }  /* if */
      /* __based is handled when building the pointer type. */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
    if (!ptr_to_member_case) {
      /* Pointer ("*"), reference ("&" or "&&"), handle ("^"), or tracking
         reference ("%"). */
      /* Add a pointer type to the top of the existing type.  Note that this
         works out right.  For example, if one has

           int * const * volatile i;

         the proper type for i is "volatile pointer to const pointer to int".
         In the loop, the type will be built up from "int" to
         "const pointer to int" to "volatile pointer to const pointer to int"
         on successive iterations. */
      if (complete_type != NULL) {
        /* Normal case -- the specifiers type is given, and the pointer or
           reference type can be attached directly to it.  (Or, this is a
           pointer to a pointer type or a reference to a pointer type). */
        a_type_ptr    temp_type;
        a_symbol_ptr  sym = NULL;
        a_boolean     is_member_function_typedef = FALSE;
        temp_type = skip_typerefs_not_parameterized_decltypes(complete_type);
        if (!same_entities(temp_type, complete_type)) {
          if (any_cfront_mode()) {
            /* Check for a special form of member function typedef that is
               an extension in cfront mode. */
            is_member_function_typedef =
                  is_cfront_member_function_typedef(complete_type, &rout_type,
                                                    &class_type, &sym);
          } else if (typeref_is_typedef(complete_type) &&
                     is_function_type(temp_type)) {
            a_routine_type_supplement_ptr  rtsp =
                                        temp_type->variant.routine.extra_info;
            if (rtsp->this_class == NULL && 
                (rtsp->qualifiers | rtsp->this_qualifiers) != TQ_NONE) {
              /* Catch the following:
                    typedef void f() const;  typedef F *PF;
                 Qualified function types are only allowed to declare members,
                 pointer-to-members and synonym typedefs. */
              pos_error(ec_ptr_or_ref_to_qualified_function_type,
                        &error_position);
            }  /* if */
          }  /* if */
        }  /* if */
        if (plain_ptr) {
          /* "*" for pointer. */
          if (is_member_function_typedef) {
            /* This is a proper use of a cfront member function typedef type
               -- to form a pointer-to-member type.  Do the transformation. */
            complete_type = ptr_to_member_type(rout_type, class_type,
                                               class_type);
          } else {
            if (is_any_reference_type(temp_type)) {
              /* Type "pointer to reference to anything" is illegal. */
              pos_error(ec_pointer_to_reference, &error_position);
              err = TRUE;
            } else if (/*lint -e(506)*/!check_cli_or_cx_type_pointed_to(
                                                        temp_type,
                                                        /*is_ref=*/FALSE,
                                                        /*is_handle=*/FALSE,
                                                        &error_position)) {
              err = TRUE;
            }  /* if */
            /* Make the pointer type. */
            complete_type = make_pointer_type(err ? error_type()
                                                  : complete_type);
          }  /* if */
        } else if (!managed_type) {
          /* "&" or "&&" for reference. */
          /* Make sure this was not preceded by __based. */
          based_not_allowed_here(pending_ptr_mods.based_var,
                                 pending_ptr_mods.based_pos);
          /* Check for the reference-to-reference case, but beware of a
             reference that would result from a dependent decltype (typerefs
             other than dependent decltypes were stripped above). */
          if (temp_type->kind != (a_type_kind)tk_typeref &&
              is_any_reference_type(temp_type)) {
            if (ref_to_ref_allowed) {
              a_source_position_ptr  qual_pos =
                   (state->qualifiers == TQ_RESTRICT) ? &state->restrict_pos
                                                      : &state->qualifiers_pos;
              complete_type = make_reference_to_reference(
                                complete_type, rvalue_ref_case,
                                /*tracking_ref=*/FALSE,
                                state->qualifiers, qual_pos, (a_boolean*)NULL);
              state->unused_qualifiers = FALSE;
            } else {
              /* Type "reference to reference" is illegal. */
              pos_error(ec_reference_to_reference, &error_position);
              err = TRUE;
            }  /* if */
          } else if (is_void_type(temp_type)) {
            /* Type "reference to void" is illegal. */
            pos_error(ec_reference_to_void, &error_position);
            err = TRUE;
          } else if (is_member_function_typedef) {
            /* A cfront member function typedef type can only be used in
               forming a pointer-to-member type. */
            sym_error(ec_bad_use_of_member_function_typedef, sym);
            err = TRUE;
          } else if (/*lint -e(506)*/!check_cli_or_cx_type_pointed_to(
                                                      temp_type,
                                                      /*is_ref=*/TRUE,
                                                      /*is_handle=*/FALSE,
                                                      &error_position)) {
            err = TRUE;
          } else {
            /* Make the reference type. */
            complete_type =
                   rvalue_ref_case ? make_rvalue_reference_type(complete_type)
                                   : make_reference_type(complete_type);
          }  /* if */
          if (err) {
            complete_type = error_type();
          }  /* if */
        } else {
#if MICROSOFT_EXTENSIONS_ALLOWED
          /* A handle ("^") or tracking-reference ("%") type. */
          /* Make sure this was not preceded by __based. */
          based_not_allowed_here(ptr_mods.based_var, ptr_mods.based_pos);
          if (curr_token == tok_caret_caret) {
            /* Replace a single "^^" token (accepted when reflection is
               enabled) by two "^" tokens. */
            a_token_cache  two_tokens;

            cache_token(&two_tokens, tok_excl_or, &pos_curr_token);
            cache_token(&two_tokens, tok_excl_or, &pos_curr_token);
            rescan_cached_tokens(&two_tokens, /*discard_curr_token=*/TRUE);
          }  /* if */
          if (!check_cli_or_cx_type_pointed_to(temp_type,
                                               curr_token == tok_remainder,
                                               /*is_handle=*/TRUE,
                                               &pos_curr_token)) {
            complete_type = error_type();
          } else if (curr_token == tok_excl_or) {
            /* "^" for handle. */
            complete_type = make_handle_type(complete_type);
          } else {
            /* "%" for tracking reference. */
            check_assertion(curr_token == tok_remainder);
            if (is_any_reference_type(temp_type)) {
              /* A "reference to reference" case. */
              complete_type = 
                  make_reference_to_reference(
                                    complete_type, /*is_rvalue_ref=*/FALSE,
                                    /*tracking_ref=*/TRUE, state->qualifiers,
                                    &state->qualifiers_pos, (a_boolean*)NULL);
            } else {
              /* Make the tracking reference type. */
              complete_type = make_tracking_reference_type(complete_type);
            }  /* if */
          }  /* if */
#else /* !MICROSOFT_EXTENSIONS_ALLOWED */
          unexpected_condition();
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        }  /* if */
      } else {
        /* The specifiers type is not known, so the bottom-most pointer type
           modifier is built but not attached to anything.  It will be
           connected later.  Note that the size is not set (set_type_size is
           not called) at this point; that's done in add_to_derived_type_list,
           once the type pointed to is known, in case pointers to different
           types have different sizes. */
        a_type_ptr new_type_ptr = alloc_type((a_type_kind)tk_pointer);
        new_type_ptr->variant.pointer.type = complete_type;
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (managed_type) {
          new_type_ptr->variant.pointer.is_handle = TRUE;
          new_type_ptr->variant.pointer.is_reference =
                                                (curr_token == tok_remainder);
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        if (!plain_ptr) {
          new_type_ptr->variant.pointer.is_reference = TRUE;
          new_type_ptr->variant.pointer.is_rvalue_reference = rvalue_ref_case;
        }  /* if */
        complete_type = new_type_ptr;
      }  /* if */
    } else {
      /* Pointer-to-member declarator. */
      /* Upon return from is_ptr_to_member_declarator_start the current
         token is tok_ptr_to_member. */
      class_type = qualifier_class_type(locator_for_curr_id);
      if (class_type == NULL) {
        /* It looks like a pointer-to-member declarator, but there was some
           error in the class qualifier (e.g., nonclassname::*).  We don't
           want a pointer-to-member type pointing at anything but a
           valid class type, so make it an error type instead. */
        complete_type = error_type();
        err = TRUE;
      } else if (is_enum_type(class_type)) {
        /* A pointer-to-member declarator with a nonclass type.  This can
           occur when enumeration types are allowed in qualified names. */
        type_error(ec_class_type_required, class_type);
        complete_type = error_type();
        err = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (cppcli_enabled && is_managed_class_type(class_type)) {
        /* Pointer-to-member-of-managed-class types are not allowed in
           C++/CLI mode. */
        pos_error(ec_ptr_to_member_of_managed_class, &pos_curr_token);
        complete_type = error_type();
        err = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else {
        a_type_ptr  lexical_class_type = class_type;
        /* A valid pointer-to-member declarator. */
        if (complete_type != NULL && !check_pm_member_type(complete_type)) {
          complete_type = error_type();
        }  /* if */
        if (is_class_struct_union_type(class_type)) {
          class_type = skip_typerefs(class_type);
        }  /* if */
        if (gpp_mode ||
            (microsoft_mode && scope_is(&scope_stack_top(),
                                        sck_template_instantiation))) {
          /* In Microsoft and GNU modes, qualifiers on the class type that
             appear through template rescanning (and, in GNU mode, through
             substitution) are transferred to the member type if that member
             type is a function type.  I.e., "R (T::*)(X)" rescanned with
             T replaced by "C const" results in a type "R (C::*)(X) const". */
          a_symbol_ptr  type_sym = locator_for_curr_id.specific_symbol;
          if (type_sym != NULL && type_sym->is_template_param) {
            /* Retrieve the type in its original form (possibly with type
               qualifiers). */
            a_type_ptr  orig_type = type_symbol_type(type_sym);
            if (orig_type != class_type) {
              check_assertion(class_type == skip_typerefs(orig_type));
              class_type = orig_type;
            }  /* if */
          }  /* if */
        } else {
          class_type = skip_typerefs_not_dependent_decltypes(class_type);
        }  /* if */
        if (complete_type == NULL) {
          /* We cannot create a valid pointer-to-member type yet: Create a
             partially-filled-in entry that will be completed by calling
             update_ptr_to_member_type later on. */
          complete_type = make_partial_ptr_to_member_type(class_type,
                                                          lexical_class_type);
        } else {
          complete_type = ptr_to_member_type(complete_type, class_type,
                                             lexical_class_type);
        }  /* if */
      }  /* if */
    }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (decl_pos_block != NULL) {
      decl_pos_block->declarator_range.end = end_pos_curr_token;
    }  /* if */
    curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Advance past the "*", "&", or "Name::*". */
    (void)get_token();
    consume_any_stray_microsoft_rparen();
    /* Scan any qualifiers following the pointer declarator, e.g.,
         int * const x;
    */
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
    if (ms_extensions or_near_and_far_enabled()) {
#if MICROSOFT_EXTENSIONS_ALLOWED
      a_variable_ptr     based_var = pending_ptr_mods.based_var;
      a_source_position  based_pos = pending_ptr_mods.based_pos;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      /* Microsoft mode allows several kinds of qualifiers/modifiers. */
      collect_pointer_declarator_extended_qualifiers(
                plain_ptr, ptr_to_member_case, &ptr_mods, decl_pos_block);
      /* Break up the qualifiers and modifiers into those like "const" and
         "__ptr64" that are handled immediately and those like "near" and
         "__based" that stay pending into the next iteration of the loop. */
#if NEAR_AND_FAR_ALLOWED
      pending_ptr_mods.qualifiers = (ptr_mods.qualifiers & (TQ_NEAR | TQ_FAR));
      ptr_mods.qualifiers -= pending_ptr_mods.qualifiers;
#endif /* NEAR_AND_FAR_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
      pending_ptr_mods.based_var = ptr_mods.based_var;
      pending_ptr_mods.based_pos = ptr_mods.based_pos;
      ptr_mods.based_var = based_var;
      ptr_mods.based_pos = based_pos;
      apply_microsoft_ptr_modifiers(&complete_type, &ptr_mods);
      pending_ptr_mods.cc_descr = ptr_mods.cc_descr;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
    /* Do not add code here. */
    { /* Just look for type qualifiers. */
      ptr_mods.qualifiers = TQ_NONE;
      if (is_type_qualifier()) {
        ptr_mods.qualifiers_pos = pos_curr_token;
        ptr_mods.qualifiers = collect_type_qualifiers(decl_pos_block,
                                                      &upc_block_size);
      }  /* if */
    }
    if (ptr_mods.qualifiers != TQ_NONE) {
      /* Some qualifiers were specified. */
      /* Check for invalid use of the restrict qualifier. */
      a_type_qualifier_set restrict_bit = (ptr_mods.qualifiers & TQ_RESTRICT);

      copy_qualifiers(ptr_mods.qualifiers, state->qualifiers);
      state->eff_top_level_cv_quals =
                               ptr_mods.qualifiers & (TQ_CONST | TQ_VOLATILE);
      if (ptr_mods.qualifiers != restrict_bit) {
        state->qualifiers_pos = ptr_mods.qualifiers_pos;
      }  /* if */
      if (restrict_bit) {
        /* Remove the restrict bit from the set to allow easier testing
           of qualifiers on references below (restrict is allowed). */
        ptr_mods.qualifiers &= ~TQ_RESTRICT;
        if (!restrict_qualifier_is_allowed(complete_type, &error_position)) {
          /* Restrict is not allowed here.  The diagnostic has been issued
             already.  Turn off the restrict bit. */
          restrict_bit = 0;
        }  /* if */
      }  /* if */
      /* Check for using qualifiers on a reference type.  The restrict
         bit has been removed if it was set, which is good because it is okay
         to put restrict on a reference. */
      if (ptr_mods.qualifiers != TQ_NONE &&
          is_any_reference_type(complete_type)) {
        diagnostic(strict_ansi_mode ? strict_ansi_error_severity : es_warning,
                   ec_qualified_reference_type);
        ptr_mods.qualifiers = TQ_NONE;
      }  /* if */
      /* Restore the restrict bit if it was on. */
      ptr_mods.qualifiers |= restrict_bit;
      /* Add the qualifiers to the complete type being built up. */
      complete_type = f_make_qualified_type(complete_type, ptr_mods.qualifiers,
                                            upc_block_size);
      consume_any_stray_microsoft_rparen();
      if (state->is_new_expr_type) {
        /* In a "new" expression, an attribute cannot appear after any
           cv-qualifiers (e.g., "new int * volatile [[]];" is invalid). */
        an_error_severity severity = (gnu_mode && !clang_mode) ? es_remark :
                                                        es_discretionary_error;
        scan_and_discard_attributes(severity, ec_attribute_not_allowed);
      }  /* if */
    }  /* if */
    scan_declarator_attributes(state, &complete_type);
    ref_to_ref_allowed = FALSE;
    /* Keep looping as long as there are pointer declarators. */
  }  /* for */
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 4) {
    if (!same_entities(complete_type, specifiers_type)) {
      fputs("pointer/reference type: ", f_debug);
      db_type(complete_type);
      (void)fputc('\n', f_debug);
    }  /* if */
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
  /* Return unbound qualifiers to the caller. */
  if (pending_ptr_mods.qualifiers != TQ_NONE) {
    if (microsoft_mode && microsoft_version < 1000) {
      /* Case like
           int i, const j;
         The type qualifiers are ignored in later versions of the Microsoft
         compiler, but were applied in MSVC++ 2.0. */
#if NEAR_AND_FAR_ALLOWED
    } else if ((pending_ptr_mods.qualifiers & ~(TQ_NEAR | TQ_FAR))
                                                                 != TQ_NONE) {
      /* A qualifier other than near/far.  It's ignored -- strip it out of
         the bit vector and issue a warning. */
      pos_warning(ec_type_qualifier_ignored, &pending_ptr_mods.qualifiers_pos);
      pending_ptr_mods.qualifiers &= (TQ_NEAR | TQ_FAR);
#else /* !NEAR_AND_FAR_ALLOWED */
    } else {
      pos_warning(ec_type_qualifier_ignored, &pending_ptr_mods.qualifiers_pos);
      pending_ptr_mods.qualifiers = TQ_NONE;
#endif /* NEAR_AND_FAR_ALLOWED */
    }  /* if */
    if (pending_ptr_mods.qualifiers != TQ_NONE) {
      /* If there are still qualifiers (because of MSVC++ 2.0 compatibility
         and/or because near/far is present), return them to the caller if
         appropriate, or else issue a warning that they're being ignored. */
      if (unbound_qualifiers != NULL) {
        *unbound_qualifiers = pending_ptr_mods.qualifiers;
      } else {
        /* Can't be returned to the caller, so put out a warning. */
        pos_warning(near_and_far_enabled() ?
                      ec_mem_attrib_ignored : ec_type_qualifier_ignored,
                    &pending_ptr_mods.qualifiers_pos);
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ms_extensions) {
    if (pending_ptr_mods.cc_descr.call_conv !=
                                           (a_calling_convention)cc_default) {
      /* Calling convention like __cdecl. */
      if (unbound_calling_convention != NULL) {
        *unbound_calling_convention = pending_ptr_mods.cc_descr;
      } else {
        /* Discard an unbound calling convention. */
        pos_warning(ec_calling_convention_ignored,
                    &pending_ptr_mods.cc_descr.position);
      }  /* if */
    }  /* if */
    if (pending_ptr_mods.based_var != NULL) {
      /* A __based modifier was present that was not followed by a
         pointer operator.  Issue a warning that the modifier is
         being discarded. */
      pos_warning(ec_based_not_followed_by_star, &pending_ptr_mods.based_pos);
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  db_exit();
  return complete_type;
}  /* pointer_declarator */


static a_boolean is_microsoft_static_operator(
                                        ARG_UNUSED an_opname_kind  opname,
                                        ARG_UNUSED a_type_ptr      parent_type)
/*
Microsoft compilers allow most operators to be declared as static member
functions.  In C++/CLI mode, this is true only if parent_type (the class type
of which the operator is a member) is a managed type.  This function returns
FALSE if and only if the operator kind opname is an exception to that rule (or
if microsoft bugs mode and C++/CLI modes are disabled).
*/
{
  a_boolean  result = FALSE;

#if MICROSOFT_EXTENSIONS_ALLOWED
  if (microsoft_bugs || cli_or_cx_enabled) {
    if (cli_or_cx_enabled && !is_managed_class_type(parent_type)) {
      result = FALSE;
    } else {
      result = opname != (an_opname_kind)onk_assign &&
               opname != (an_opname_kind)onk_function_call &&
               opname != (an_opname_kind)onk_subscript &&
               opname != (an_opname_kind)onk_arrow;
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  return result;
}  /* is_microsoft_static_operator */


static void scan_id_attributes(a_decl_parse_state  *dps)
/*
Scan attributes following a declarator-id.  If this includes GNU attributes,
reclassify them as appropriate (e.g., in some GNU modes, type-transforming
attributes are applied to the underlying type).
*/
{
  an_attribute_ptr  attributes = scan_attributes(al_declarator_id);

  if (attributes != NULL) {
    if (gnu_mode &&
        dps->declared_storage_class != (a_storage_class)sc_typedef) {
      /* Type transforming attributes are reclassified as specifier attributes
         or post-pointer-or-reference attributes (except on typedef
         declarations, where the attribute is associated with the typedef
         entry itself). */
      an_attribute_ptr  ap, tt_attributes = NULL;
      an_attribute_ptr  *p_from = &attributes, *p_to = &tt_attributes;
      an_attribute_location
                        to_syn_loc;
      switch (skip_typerefs_not_typedefs(dps->declared_type)->kind) {
        case tk_pointer:
        case tk_ptr_to_member:
          to_syn_loc = al_post_ptr_or_ref;
          break;
        default:
          to_syn_loc = al_specifier;
      }  /* switch */
      /* Extract (and reclassify) type transforming GNU attributes. */
      do {
        ap = *p_from;
        if (is_gcc_attribute(ap) &&
            is_type_transforming_attribute(ap)) {
          *p_from = ap->next;
          ap->syntactic_location = to_syn_loc;
          *p_to = ap;
          p_to = &ap->next;
        } else {
          p_from = &ap->next;
        }  /* if */
      } while (*p_from != NULL);
      /* Apply any extracted attributes to dps->declared_type. */
      if (tt_attributes != NULL) {
        attach_type_attributes(&dps->declared_type, tt_attributes, (void*)dps);
      }  /* if */
    }  /* if */
    /* Append the remaining attributes (if any) to the list pointed to by
       dps->id_attributes. */
    *last_attribute_link(&dps->id_attributes) = attributes;
  }  /* if */
}  /* scan_id_attributes */


static a_boolean check_static_call_operator(a_symbol_locator  *loc)
/*
If loc represents a function call operator, return TRUE.  This operator is
being declared "static".  In non-C++23 mode issue a diagnostic (warning or
discretionary error, depending on the mode).
*/
{
  a_boolean  result = FALSE;

  if (loc->is_operator_name && loc->variant.opname == onk_function_call) {
    result = TRUE;
    if (!static_call_operator_enabled) {
      an_error_severity  sev = es_discretionary_error;
      if (gpp_version_is(>=130000) || clang_version_is(>=160000) ||
          ms_version_is(>=1944)) {
        /* Recent versions of GCC, Clang, and MSVC accept the syntax in
           pre-C++23 modes. */
        sev = es_warning;
      }  /* if */
      pos_diagnostic(sev, ec_static_member_operator_not_allowed,
                     &loc->source_position);
    }  /* if */
  }  /* if */
  return result;
}  /* check_static_call_operator */


static a_boolean check_static_subscript_operator(a_symbol_locator  *loc)
/*
If loc represents a subscript operator, return TRUE.  This operator is being
declared "static".  In non-C++23 mode issue a diagnostic (warning or
discretionary error, depending on the mode).
*/
{
  a_boolean  result = FALSE;

  if (loc->is_operator_name && loc->variant.opname == onk_subscript) {
    result = TRUE;
    if (!multi_subscript_enabled) {
      an_error_severity  sev = es_discretionary_error;
      if (gpp_version_is(>=130000) || clang_version_is(>=160000) ||
          ms_version_is(>=1939)) {
        /* Recent versions of GCC, Clang, and MSVC accept the syntax in
           pre-C++23 modes. */
        sev = es_warning;
      }  /* if */
      pos_diagnostic(sev, ec_static_member_operator_not_allowed,
                     &loc->source_position);
    }  /* if */
  }  /* if */
  return result;
}  /* check_static_subscript_operator */


static void scan_real_declarator_id(
                a_decl_parse_state          *dps,
                a_decl_flag_set             input_flags,
                a_decl_flag_set             *output_flags,
                a_symbol_locator            *locator,
                a_boolean                   *is_constructor,
                ARG_UNUSED a_boolean        *is_static_constructor,
                a_boolean                   *is_destructor,
                ARG_UNUSED a_boolean        *is_finalizer,
                a_boolean                   *parenthesized_initializer_allowed,
                a_boolean                   *not_a_function_declarator,
                a_type_ptr                  *p_member_parent_type,
                ARG_UNUSED a_decl_pos_block *decl_pos_block)
/*
This routine is called by declarator for real declarators; it scans the name
that is specified (and any following attributes).  The current token is the
beginning of the name (usually but not always an identifier).  dps points to a
structure describing various properties of the current declaration.
input_flags is the set of flags passed in to declarator, and *output_flags is
the set of flags that will be returned to declarator's caller.  *locator is
returned with the locator for the name.  *p_member_parent_type is the class
type when this is a qualified name.  *is_constructor, *is_static_constructor,
*is_destructor, or *is_finalizer is set to TRUE if the name is for a
constructor, a C++/CLI static constructor, a destructor, or a finalizer
respectively.
*parenthesized_initializer_allowed is set to FALSE if the entity being
declared is not initializable, and *not_a_function_declarator is set if the
declared entity is known to not be a function.
*/
{
  a_source_position         declarator_pos;
  a_boolean                 err;
  an_identifier_options_set options;
  a_symbol_ptr              sym;
  a_namespace_ptr           nsp;
  a_boolean		    is_in_class_specialization = FALSE;
  a_boolean		    is_specialization_or_instantiation;
  a_boolean		    is_specialization = FALSE;
  a_boolean		    explicit_template_args_allowed = FALSE;
  a_boolean		    template_args_allowed_only_on_func_decl = FALSE;
  a_boolean		    ignore_explicit_template_args = FALSE;

  db_enter(3, "scan_real_declarator_id");
  declarator_pos = pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    decl_pos_block->identifier_range.start = pos_curr_token;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Process the identifier.  This is done if we are at the beginning of a
     qualified name.  A special test is done to exclude a destructor name
     that is not part of a qualified name -- this case is handled separately
     below.  A destructor that is not part of a qualified name (according to
     the locator) can appear to be an identifier under some circumstances.
     For example, inside the definition of class A, the destructor "A::~A"
     will be coalesced by is_generalized_identifier_start.  The qualifier
     will then be discarded by simplify_curr_class_qualified_name resulting
     in an unqualified destructor that has already been coalesced. */
  is_specialization_or_instantiation =
    (input_flags & (DI_IS_SPECIALIZATION | DI_IS_EXPLICIT_INSTANTIATION)) != 0;
  options = GID_DTOR_RECOGNIZED;
  if (!(input_flags & DI_QUALIFIED_NAME_ALLOWED)) {
    options |= GID_DISALLOW_QUALIFIED_NAME | GID_DISALLOW_GLOBAL_QUALIFIER;
  }  /* if */
  if (input_flags & DI_IS_TEMPLATE_DECLARATION) {
    options |= GID_IS_TEMPLATE_DECLARATION;
    if (input_flags & DI_IS_SPECIALIZATION) {
      options |= GID_IS_TEMPLATE_SPECIALIZATION;
      is_specialization = TRUE;
    }  /* if */
  }  /* if */
  if (input_flags & DI_IS_FRIEND_DECL) {
    options |= GID_IS_FRIEND_DECL;
  }  /* if */
  if (variable_templates_may_be_enabled &&
      (options & GID_IS_TEMPLATE_DECLARATION) != 0) {
    /* Explicit template arguments are not allowed on most declarators in
       template declarations, but are allowed for variable template partial
       specializations.  We don't know if that is what we have now, but
       we need to allow for that case. */
    explicit_template_args_allowed = TRUE;
  } else if (is_specialization_or_instantiation ||
             (input_flags & DI_EXPLICIT_TEMPLATE_ARGS_ALLOWED) ||
             ((input_flags & DI_IS_FRIEND_DECL) &&
             !(options & GID_IS_TEMPLATE_DECLARATION))) {
    explicit_template_args_allowed = TRUE;
  } else if (microsoft_mode &&
             depth_innermost_function_scope == NO_SCOPE_DEPTH) {
    /* In Microsoft mode a function declarator can take explicit template
       argument syntax.  If the declaration is not a template, it is taken to
       be a specialization.  For example,
         template <class T> void f(T) { ... }
         void f<int>(int) { ... }
       is allowed in Microsoft mode -- the second line is equivalent to
         template <> void f<int>(int) { ... }
       (In more recent versions of the Microsoft compiler, this is actually
       context-dependent.  That dependency is handled elsewhere.)
       If the declaration is a non-member template redeclaration, the template
       argument list may simply be ignored (with a warning).  For example:
         template <class T> void f(T);
         template <class T> void f<float>(T) { ... }  // <float> ignored
       Such cases are handled in higher-level declaration processing.
    */
    if (!(options & GID_IS_TEMPLATE_DECLARATION)) {
      explicit_template_args_allowed = TRUE;
      template_args_allowed_only_on_func_decl = TRUE;
    } else if (!locator_for_curr_id.is_class_member) {
      ignore_explicit_template_args = TRUE;
    }  /* if */
  }  /* if */
  if (*p_member_parent_type != NULL && (input_flags & DI_IS_SPECIALIZATION)) {
    /* When a member parent type is provided and the specialization flag is
       set, this must be an in-class specialization. */
    is_in_class_specialization = TRUE;
  }  /* if */
  if (is_generalized_identifier_start(GID_DTOR_RECOGNIZED) &&
      (!is_dtor_like_locator(locator_for_curr_id) ||
       locator_for_curr_id.is_qualified_name)) {
    an_identifier_lookup_mode  lookup_mode = ilm_declarator;
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (cli_or_cx_enabled &&
        dps->declared_storage_class == (a_storage_class)sc_static) {
      lookup_mode = ilm_static_declarator;
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (any_cfront_mode()) {
      /* Provide support for an exploitable cfront bug. */
      if (locator_for_curr_id.is_qualified_name &&
          qualifier_class_type(locator_for_curr_id) != NULL &&
          input_flags & DI_IS_TYPEDEF_DECLARATION) {
        /* We have a typedef declaration involving what appears to be a
           qualified name, but cfront interprets it as a kind of member
           routine type, e.g.,
               typedef void A::t(int);
                               ^---------We're here now.
           The type "t" is construed as a routine type taking an int argument
           and returning void and having an implicit this-param type of
           const-ptr-to-A.  Note that this syntax and interpretation are not
           supported in the ARM.  We allow it under cfront compatibility mode
           only. */
        check_assertion(locator_for_curr_id.specific_symbol == NULL);
        /* Force function_declarator to add an implicit-this-param pointer
           to the routine type. */
        *p_member_parent_type = qualifier_class_type(locator_for_curr_id);
        *output_flags |= DO_CFRONT_MEMBER_FUNCTION_TYPEDEF;
        /* Clear the is_qualified_name flag in the locator, but keep the
           qualifer_class_type around, in case this is a recursive declarator
           call and the function_declarator is called at another level. */
        locator_for_curr_id.is_qualified_name = FALSE;
        /* Note that the diagnostic on this nonstandard construct is issued
           by the caller. */
      }  /* if */
    }  /* if */
    if (locator_for_curr_id.is_qualified_name) {
      if (locator_for_curr_id.is_class_member) {
        if (!(input_flags & DI_IS_FRIEND_DECL) || microsoft_bugs) {
          /* If this declaration appears in the immediate context of a class
             definition and the current token is an identifier representing
             the name of the current class, see if this is a qualified name
             and if so change it into a simple name (e.g., A::x becomes x,
             its equivalent in A's scope).  This needs to be done after the
             check for the cfront member typedef processing that is done
             above.  Ordinarily, this does not apply to the declarator of a
             friend declaration (i.e., "friend void A::f();" is an error in
             class A if f had not been declared yet), but Microsoft compilers
             perform the transformation anyway (thereby creating ::f instead
             of A::f(!!)). */
          if (simplify_curr_class_qualified_name() /* side effects */ &&
              (input_flags & DI_IS_FRIEND_DECL)) {
            /* We're emulating a Microsoft bug by ignoring the qualification
               on a friend function (possibly injecting it in the surrounding
               namespace scope).  This is likely unintended. */
            pos_warning(ec_friend_qualification_ignored, &error_position);
          }  /* if */
        }  /* if */
        if (locator_for_curr_id.is_decltype_qualified) {
          /* Something like "int decltype(x)::y z;" is not permitted by the
             standard. */
          pos_diagnostic((strict_ansi_mode || clang_mode) ? es_error
                                                          : es_warning,
                         ec_decltype_qualified_declared_name,
                         &locator_for_curr_id.source_position);
        }  /* if */
      } else {
        /* This must be a namespace-qualified name.  This is used when a
           namespace member is redeclared (defined) outside its namespace.
             namespace N { void f(); }
             void N::f() { ... }
           Strictly speaking, when the qualifier is used on a declarator that
           appears within that namespace, it is an error -- though usually a
           benign error:
             namespace N { void N::f(); }       // error
           (It is not benign, however, when the qualifier appears on the
           declarator of a template declaration, because the entire declarator
           is cached and rescanned during instantiations, at which point it is
           possible that the qualifier's meaning will have changed.  The case
           of a simple globally-qualified identifier (e.g., ::f) presents no
           such problem.) */
        a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];
        a_boolean                is_template_decl = FALSE;

        if (ssep->kind == (a_scope_kind)sck_template_declaration) {
          /* This is a template declaration. */
          is_template_decl = TRUE;
          ssep--;
        } else if (gpp_mode && !clang_mode &&
                   ((input_flags & DI_IS_FRIEND_DECL) ||
                    (gnu_version < 40101 &&
                     depth_innermost_function_scope != NO_SCOPE_DEPTH)) &&
                   !locator_for_curr_id.is_template_id) {
          /* An unqualified friend declarator would declare a member of the
             nearest enclosing namespace scope.  GNU C++ compilers therefore
             ignore qualifiers that refer to that scope.  If a friend
             declaration is a qualified template-id, however, the template
             must already have been declared in that namespace, so this
             processing is not needed.  GCC up to version 4.1.0 also ignores
             such qualifiers on local declarations.  For example:
                 namespace N {
                   void g() {
                     double N::x;  // GCC treats this as "double x;".
                   }
                 }
          */
          ssep = &scope_stack[depth_innermost_namespace_scope];
        }  /* if */
        if (locator_for_curr_id.is_error) {
          /* We can get here with a declaration like "int X::f();" where X
             is not found or ambiguous.  An error will already have been
             issued, so no additional diagnostic should be emitted here. */
          check_assertion(is_at_least_one_error());
        } else if (((ssep->kind == (a_scope_kind)sck_namespace ||
                     ssep->kind == (a_scope_kind)sck_namespace_extension) &&
                    ssep->il_scope->variant.assoc_namespace ==
                              qualifier_namespace_ptr(locator_for_curr_id)) ||
                   (ssep->kind == (a_scope_kind)sck_file &&
                    qualifier_namespace_ptr(locator_for_curr_id) == NULL)) {
          an_error_severity  severity = es_discretionary_error;
          an_error_code      err_code;
          a_boolean          keep_qualifier = FALSE;
          /* The declarator name is qualified by the current namespace. */
          if (is_template_decl && !do_dependent_name_processing &&
              !(gpp_mode ||
                (ms_version_is(<=1910) && ms_version_is(>=1900))) &&
              qualifier_namespace_ptr(locator_for_curr_id) != NULL) {
            severity = es_error;
          } else if (is_specialization_or_instantiation && !strict_ansi_mode) {
            severity = es_remark;
          } else if (!strict_ansi_mode) {
            /* In nonstrict modes, the extraneous qualifier is accepted
               provided it is for a redeclaration.  In general, we cannot
               verify that this is a redeclaration at this point; so it has to
               be checked later (e.g., in decl_variable or decl_routine). */
            /* Save the symbol pointed to by the current locator so we can
               undo the side-effect of curr_scope_id_lookup. */
            a_symbol_ptr  prev_locator_sym =
                                           locator_for_curr_id.specific_symbol;
            a_symbol_ptr  prev_sym =
                    curr_scope_id_lookup(&locator_for_curr_id, IDL_NO_OPTIONS);
            locator_for_curr_id.specific_symbol = prev_locator_sym;
            if (prev_sym != NULL && !is_tag_symbol(prev_sym) &&
                ssep == &scope_stack[depth_scope_stack]) {
              severity = strict_ansi_mode ? es_discretionary_error : es_remark;
              keep_qualifier = TRUE;
            } else if (gpp_mode || sun_mode ||
                       (is_template_decl &&
                        (ms_version_is(<=1910) && ms_version_is(>=1900)))) {
              /* GNU and Sun compilers accept the superfluous qualifier and
                 ignore it.  As explained above, we cannot always emulate this
                 behavior for templates (unless dependent name processing is
                 enabled, which is the default for gnu_version >= 30400).
                 Some versions of the Microsoft compiler similarly ignore the
                 superfluous qualifier, but only for the template case. */
              severity = es_warning;
            }  /* if */
          }  /* if */
          if (!keep_qualifier) {
            /* Reset the fields in the locator to make it appear as if the
               qualifier were not present. */
            clear_qualifier_from_locator(&locator_for_curr_id);
          }  /* if */
          if (severity == es_error) {
            /* This is not a benign error in a template declaration, so
               make this an error locator.  Otherwise, there are name-binding
               bugs in this sort of case:
                 namespace N {
                   template <class T> void N::f(T);
                   class N { ... };
                   void g() {
                     f(0);   // Problems with this instantiation because
                   }         // rescanning N::f changes its meaning.
                 }
            */
            set_to_named_error_locator(locator_for_curr_id);
          }  /* if */
          if (depth_innermost_function_scope != NO_SCOPE_DEPTH &&
              (int)severity <= (int)es_warning) {
            check_assertion(gpp_mode);
            err_code = ec_qualifier_ignored_on_local_declaration;
          } else if (gpp_mode && (input_flags & DI_IS_FRIEND_DECL) &&
              (int)severity <= (int)es_warning) {
            err_code = ec_friend_qualification_ignored;
          } else if (ssep->kind == (a_scope_kind)sck_file) {
            err_code = ec_nonstd_qualifier_in_global_scope_decl;
          } else if ((int)severity <= (int)es_warning) {
            err_code = ec_nonstd_qualifier_in_namespace_member_decl;
          } else {
            err_code = ec_qualifier_in_namespace_member_decl;
          }  /* if */
          pos_diagnostic(severity, err_code, &pos_curr_token);
        } else if (ssep->kind == (a_scope_kind)sck_template_instantiation) {
          /* Error has already been issued on the template declaration.
             Just skip over it here on the instantiation. */
          clear_qualifier_from_locator(&locator_for_curr_id);
        }  /* if */
      }  /* if */
    }  /* if */
    /* The declarator may be a qualified name or a normal name. */
    if (coalesce_and_lookup_qualified_name(options, lookup_mode, &err)) {
      /* See if the name is a qualified name, like "A::x" or "::j". */
      if (err) {
        /* The locator will be set to an error locator below. */
        check_assertion(is_at_least_one_error());
      } else if (locator_for_curr_id.is_qualified_name) {
        a_type_ptr	tp;
        tp = qualifier_class_type(locator_for_curr_id);
        if (tp != NULL) {
          tp = skip_typerefs(tp);
          /* If this is a template parameter that represents a nested class of
             a class template, use the original nested type in its place. */
          tp = orig_nested_type_if_nonreal_nested_type(tp);
        }  /* if */
        *p_member_parent_type = tp;
        if (*p_member_parent_type != NULL) {
          a_boolean  reactivate_scope = FALSE;
          sym = locator_for_curr_id.specific_symbol;
          if (microsoft_mode && sym->kind == (a_symbol_kind)sk_projection) {
            /* Microsoft compilers allow static member declarators using a
               derived class qualifier. */
            a_symbol_ptr  fund_sym = fundamental_symbol_of(sym);
            if (fund_sym->kind == (a_symbol_kind)sk_static_data_member) {
              sym = fund_sym;
            }  /* if */
          }  /* if */
          /* See if the name is the name of a member function. */
          if (sym->kind == (a_symbol_kind)sk_member_function ||
              sym->kind == (a_symbol_kind)sk_overloaded_function ||
              sym->kind == (a_symbol_kind)sk_function_template) {
            /* It is a member function.  Its parameters should be scanned
               with the original class reactivated. */
            reactivate_scope = TRUE;
            *parenthesized_initializer_allowed = FALSE;
            if (is_constructor_symbol(sym)) {
              *is_constructor = TRUE;
            } else if (is_destructor_symbol(sym)) {
              *is_destructor = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
            } else if (cli_or_cx_enabled && is_finalizer_symbol(sym)) {
              *is_finalizer = TRUE;
            } else if (cli_or_cx_enabled &&
                       is_static_constructor_symbol(sym)) {
              *is_static_constructor = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            }  /* if */
          } else if ((sym->kind == (a_symbol_kind)sk_static_data_member ||
                      sym->kind == (a_symbol_kind)sk_variable) &&
                      sym->is_class_member) {
            /* The dimensions (if any) of variable template instances and
               static data members are scanned with the original class
               reactivated. */
            reactivate_scope = TRUE;
            *not_a_function_declarator = TRUE;
            if (is_nonspecialized_instantiation_context()) {
              /* When instantiating a template static data member, we need
                 to update scopes pushed for instantiation purposes so that
                 the class definition context will be visible from this point
                 on in the declaration. */
              make_class_definition_context_visible();
	    }  /* if */
          } else if (dps->dso_flags & DSO_FRIEND &&
                     dps->in_nested_declarator &&
                     locator_for_curr_id.is_class_member &&
                     !locator_for_curr_id.is_template_id &&
                     scope_stack_top().in_prototype_instantiation &&
                     locator_for_curr_id.parent.class_type
                              ->variant.class_struct_union.is_nonreal_class &&
                     locator_for_curr_id.symbol_header ==
                                  symbol_for(*p_member_parent_type)->header &&
                     next_token() == tok_rparen) {
            /* A declaration like "friend (A<T>::A)();" in a prototype
               instantiation.  is_constructor_symbol(sym) does not return TRUE
               in such cases because lookup in the nonreal A<T> does not yield
               declared members. */
            *is_constructor = TRUE;
          }  /* if */
          if (reactivate_scope) {
            /* Reactivate the scope of the parent class.  It will be
               deactivated once the entire declarator has been scanned. */
            push_class_and_template_reactivation_scope_full(
                            *p_member_parent_type,
                            /*reactivate_template_params=*/FALSE,
                            is_specialization,
                            /*extend_namespace=*/FALSE,
                            /*force_new_context=*/FALSE,
                            PS_NO_OPTIONS);
            *output_flags |= DO_SCOPE_DEACTIVATION_REQUIRED;
            if (any_deferred_access_checks()) {
              /* Discard any access errors that occurred while scanning
                 the name of the thing being defined. */
              discard_declarator_access_errors();
              /* Recheck any access errors that occurred while scanning
                 the specifiers or the beginning of the declarator
                 now that we know the class of the thing being declared. */
              perform_deferred_access_checks();
            }  /* if */
          }  /* if */
        } else {
          /* This must be a namespace-qualified name. */
          nsp = qualifier_namespace_ptr(locator_for_curr_id);
          if (nsp != NULL) {
            /* Push the namespace extension scope.  It will be popped when
               scanning the declarator has been completed. */
            push_namespace_reactivation_scope(nsp);
            *output_flags |= DO_SCOPE_DEACTIVATION_REQUIRED;
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      /* The declarator id is not qualified. */
      if (!C_mode()) {
        a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];
        while (ssep->kind == (a_scope_kind)sck_template_declaration ||
               ssep->kind == (a_scope_kind)sck_template_instantiation) {
          /* Take into account the possibility that the constructor is a
             member template (or an instance thereof).  (This loop only
             iterates more than once in error situations like the following:
               struct S { template<class> template<class> (S)(); };
             Skipping the template declaration scopes allows for better error
             recovery in such cases.) */
          -- ssep;
        }  /* while */
        if (ssep->kind == (a_scope_kind)sck_class_struct_union ||
            ssep->kind == (a_scope_kind)sck_class_reactivation) {
          /* Check if we have a constructor. Trying to find it out while
             scanning the specifiers might have failed because the scanning
             had to stop at an opening parenthesis or a Microsoft calling
             convention. However, we might have  e.g. "struct S { (S)(); }".
             Note that destructors aren't a problem because of the distinctive
             leading tilde. */
          if (!err && (input_flags & DI_NO_TYPE_SPECIFIERS) != 0 &&
              is_constructor_decl(ssep->assoc_type, dps)) {
#if MICROSOFT_EXTENSIONS_ALLOWED
            if (cli_or_cx_enabled &&
                dps->declared_storage_class == (a_storage_class)sc_static &&
                is_managed_class_type(ssep->assoc_type)) {
              *is_static_constructor = TRUE;
            } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            /* Do not insert code here. */
            {
              *is_constructor = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (err) {
      /* An error occurred while scanning the identifier -- use an error
         locator. */
      if (!(input_flags & DI_QUALIFIED_NAME_ALLOWED) &&
          (locator_for_curr_id.is_class_member ||
           locator_for_curr_id.parent.namespace_ptr != NULL)) {
        /* If the declarator-id was qualified in a context that doesn't
           allow qualified names, do not try to preserve the name or its
           qualifier. */
        set_to_error_locator(locator_for_curr_id);
      } else {
        set_to_named_error_locator(locator_for_curr_id);
      }  /* if */
    }  /* if */
    /* Save information on the identifier to be declared. */
    *locator = locator_for_curr_id;
    dps->declarator_name_tsn = curr_token_sequence_number;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (decl_pos_block != NULL) {
      decl_pos_block->identifier_range.end = end_pos_curr_token;
      decl_pos_block->declarator_range.end = end_pos_curr_token;
    }  /* if */
    curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    (void)get_token();
  } else {
    if (!(input_flags & DI_IS_FRIEND_DECL)) {
      /* The call to simplify_curr_class_qualified_name is placed here so
         that it will be done at this point for all cases other than the
         normal identifier case handled above. */
      (void)simplify_curr_class_qualified_name();
    }  /* if */
    if (curr_token == tok_identifier &&
        is_dtor_like_locator(locator_for_curr_id)) {
      an_error_code error_name_must_be_qualified;
      an_error_code error_bad_decl;
      /* A destructor or C++/CLI finalizer name, like "~A" or "!A".  It must
         have the same name as the class currently being defined, it must be
         followed by a left paren, and the specifiers must include no type. */
      if (locator_for_curr_id.is_destructor_name) {
        *is_destructor = TRUE;
        error_name_must_be_qualified = ec_destructor_name_must_be_qualified;
        error_bad_decl = ec_bad_destructor_decl;
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else {
        *is_finalizer = TRUE;
        error_name_must_be_qualified = ec_finalizer_name_must_be_qualified;
        error_bad_decl = ec_bad_finalizer_decl;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      }  /* if */
      *parenthesized_initializer_allowed = FALSE;
      if (is_error_locator(locator_for_curr_id)) {
        /* There is some error in the destructor or finalizer name. */
        set_to_error_locator(*locator);
      } else if ((input_flags & DI_IS_FRIEND_DECL) &&
                 !locator_for_curr_id.is_qualified_name) {
        pos_error(error_name_must_be_qualified, &error_position);
        set_to_error_locator(*locator);
      } else if (input_flags & DI_IS_TYPEDEF_DECLARATION) {
        /* "typedef ~X();" and "typedef !X();" are not acceptable. */
        pos_error(error_bad_decl, &error_position);
        set_to_error_locator(*locator);
      } else {
        a_scope_stack_entry_ptr ssep = &scope_stack[decl_scope_level];

        if (ssep->kind != (a_scope_kind)sck_class_struct_union) {
          /* Not inside a class; destructor or finalizer is not allowed. */
          pos_error(error_bad_decl, &error_position);
          set_to_error_locator(*locator);
        } else {
          sym = (a_symbol_ptr)ssep->assoc_type->source_corresp.assoc_info;
          if (!destructor_name_matches_class_name(sym)) {
            /* The name on the destructor or finalizer is not the name of the
               class. */
            pos_error(error_bad_decl, &error_position);
            set_to_error_locator(*locator);
          } else {
            *locator = locator_for_curr_id;
            dps->declarator_name_tsn = curr_token_sequence_number;
            *p_member_parent_type = ssep->il_scope->variant.assoc_type;
          }  /* if */
        }  /* if */
      }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
      if (decl_pos_block != NULL) {
        decl_pos_block->identifier_range.end = end_pos_curr_token;
        decl_pos_block->declarator_range.end = end_pos_curr_token;
      }  /* if */
      curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      /* Advance past the destructor. */
      (void)get_token();
    } else {
      an_error_code error_code;
      if (curr_token == tok_using && dps->is_explicit_specialization) {
        /* Give a more descriptive message for an attempt to explicitly
           specialize an alias template. */
        error_code = ec_expl_spec_alias_template;
        error_position = dps->start_pos;
      } else {
        error_code = ec_exp_identifier;
      }  /* if */
      add_stop_token(tok_lparen);
      add_stop_token(tok_lbracket);
      set_to_error_locator(*locator);
      copy_source_position(pos_curr_token, locator->source_position);
      syntax_error(error_code);
      remove_stop_token(tok_lparen);
      remove_stop_token(tok_lbracket);
      *parenthesized_initializer_allowed = FALSE;
    }  /* if */
  }  /* if */
  if (locator->specific_symbol != NULL &&
      locator->specific_symbol->kind == (a_symbol_kind)sk_namespace) {
    /* A namespace name cannot be a declarator. */
    pos_error(ec_namespace_name_not_allowed, &declarator_pos);
    set_to_error_locator(*locator);
  }  /* if */
  if (template_args_allowed_only_on_func_decl && locator->is_template_id &&
      locator->specific_symbol != NULL &&
      !is_function_symbol(locator->specific_symbol)) {
    /* In Microsoft mode, explicit template arguments may be used to indicate
       an explicit function template specialization without a "template<>"
       prefix (see above), but in this case the resulting template-id does not
       resolve to an instance of a function template: Disallow the explicit
       arguments in what follows. */
    check_assertion(microsoft_mode);
    explicit_template_args_allowed = FALSE;
  }  /* if */
  if (!explicit_template_args_allowed && locator->is_template_id &&
      !is_error_locator(*locator)) {
    /* An explicit template argument list is only permitted on explicit
       specializations, explicit instantiations, and friend declarations.
       Other declarations that appear to include an explicit argument list,
       such as a destructor declaration of the form ~A<T>(), will have
       already been transformed to a form where they are no longer considered
       to be template-ids.  An exception exists in Microsoft mode (see
       above). */
    if (!ignore_explicit_template_args) {
      pos_error(ec_explicit_template_args_not_allowed, &declarator_pos);
      set_to_error_locator(*locator);
    } else {
      /* We ignore the template arguments for now, but higher-level
         declaration processing will issue a warning or error depending on
         the context. */
      locator->is_template_id = FALSE;
    }  /* if */
  }  /* if */
  if (locator->is_operator_name) {
    /* Enforce some restrictions on the declarations of overloaded
       operator functions. */
    *parenthesized_initializer_allowed = FALSE;
    if (input_flags & DI_IS_TYPEDEF_DECLARATION) {
      /* "typedef int operator+" is not allowed. */
      pos_error(ec_operator_name_not_allowed,
                &locator->source_position);
      set_to_error_locator(*locator);
    } else if (*p_member_parent_type != NULL) {
      if (locator->specific_symbol != NULL) {
        /* This must be a redeclaration. */
      } else if (is_in_class_specialization) {
        /* A specialization declared within the class.  Suppress the following
           test for this case.  It is a kind of redeclaration. */
      } else if (!(input_flags & DI_NONSTATIC_MEMBER) &&
                 !is_new_operator(locator->variant.opname) &&
                 !is_delete_operator(locator->variant.opname) &&
                 !check_static_call_operator(locator) &&
                 !check_static_subscript_operator(locator) &&
                 !is_microsoft_static_operator(locator->variant.opname,
                                               *p_member_parent_type)) {
        /* Most operators cannot be declared to be static members (except in
           Microsoft mode, but except for new and delete those static member
           operators can only be invoked with qualified notation). */
        pos_error(ec_static_member_operator_not_allowed,
                  &locator->source_position);
        set_to_error_locator(*locator);
      }  /* if */
    } else {
      a_const_char *s = NULL;
      switch (locator->variant.opname) {
        case onk_assign:         s = "=";       break;
        case onk_function_call:  s = "()";      break;
        case onk_subscript:      s = "[]";      break;
        case onk_arrow:          s = "->";      break;
        default:;  /* No error. */
      }  /* switch */
      if (s != NULL) {
        if (locator->variant.opname == (an_opname_kind)onk_assign &&
            cfront_2_1_mode) {
          pos_st_warning(ec_nonmember_operator_not_allowed,
                         &locator->source_position, s);
        } else {
          pos_st_error(ec_nonmember_operator_not_allowed,
                       &locator->source_position, s);
          set_to_error_locator(*locator);
        }  /* if */
      }  /* if */
    }  /* if */
  } else if (locator->is_conversion_name) {
    /* A conversion function must be a nonstatic member function (except in
       C++/CLI mode, where it can also be a static member function).  Allow
       a Microsoft in-class specialization as this should be treated
       as a redeclaration. */
    if (*p_member_parent_type == NULL ||
        (locator->specific_symbol == NULL &&
         !(input_flags & DI_NONSTATIC_MEMBER) &&
#if MICROSOFT_EXTENSIONS_ALLOWED
         !(cli_or_cx_enabled &&
           is_managed_class_type(*p_member_parent_type)) &&
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
         !is_in_class_specialization)) {
      pos_error(ec_bad_conversion_function_decl,
                &locator->source_position);
      set_to_error_locator(*locator);
      /* Avoid error recovery problems later. */
      locator->is_conversion_name = TRUE;
    }  /* if */
  } else if (locator->is_udl_operator_name) {
    if (input_flags & DI_IS_TYPEDEF_DECLARATION) {
      /* "typedef int operator ""X(char*);" is not allowed either. */
      pos_error(ec_operator_name_not_allowed, &locator->source_position);
      set_to_error_locator(*locator);
    }  /* if */
  }  /* if */
  scan_id_attributes(dps);
  db_exit();
}  /* scan_real_declarator_id */

#if UPC_EXTENSIONS_ALLOWED

static void check_and_update_upc_type(a_type_ptr       type,
                                      a_decl_flag_set  input_flags)
/*
If the given type is an array, check whether it satisfies the UPC
constraints regarding THREADS-dependent dimensions.  If necessary
also resolve any UPC block sizes.  (input_flags is the set of flags
passed to r_declarator.)
*/
{
  if (is_array_type(type)) {
    if (!(input_flags & (DI_IS_TYPEDEF_DECLARATION | DI_IS_PARAMETER_DECL)) &&
        upc_dynamic_threads() &&
        !is_underlying_threads_dimensioned_array_type(type) &&
        get_underlying_upc_block_size(type) != UPC_BLOCK_SIZE_INDEFINITE &&
        is_underlying_shared_qualified_type(type) &&
        skip_typerefs(type)->variant.array.variant.number_of_elements != 0) {
      /* Shared data must be THREADS-dimensioned (except for parameters, but
         they decay to pointers). */
      pos_error(ec_shared_nonthreads_dim, &error_position);
    }  /* if */
  }  /* if */
  /* Resolve any pure block or automatic block sizes. */
  fixup_upc_block_size(type);
}  /* check_and_update_upc_type */

#endif /* UPC_EXTENSIONS_ALLOWED */

static void process_conversion_function_declarator(
                                             a_symbol_locator    *locator,
                                             a_decl_parse_state  *state, 
                                             a_decl_flag_set     input_flags,
                                             a_type_ptr          derived_type,
                                             a_type_ptr          *return_type)
/*
This is a helper function for r_declarator: locator, state, and input_flags
are the corresponding parameters of that function.  locator represents a
conversion-function-id in a declarator and the type of that declarator (as
known so far) is derived_type.  Record the return type of the function in
*return_type, and issue an error if type specifiers appeared prior to the
conversion-function-id.  If locator->specific_symbol is an ambiguous symbol
but the completed declarator can remove the ambiguity, update
locator->specific_symbol to point to the correct symbol entry.
*/
{
  if (is_error_locator(*locator)) {
    *return_type = error_type();
  } else {
    a_symbol_ptr  sym = locator->specific_symbol;
    if (!is_unknown_type(state->specifiers_type) &&
        !(input_flags & DI_NO_TYPE_SPECIFIERS)) {
      pos_error(ec_return_type_on_conversion_function, &state->specifiers_pos);
    }  /* if */
    if (sym != NULL && sym->ambiguous &&
        sym->kind == (a_symbol_kind)sk_member_function &&
        sym->variant.routine.instance_ptr != NULL &&
        sym->variant.routine.instance_ptr->template_sym->kind
                                     == (a_symbol_kind)sk_function_template &&
        derived_type != NULL && is_function_type(derived_type)) {
      /* When the conversion operator name was coalesced, there may have been
         multiple matching conversion operator templates but with different
         cv-qualifiers.  E.g.:
           struct S {
             template<class T> operator T();
             template<class T> operator T() const;
           };
           template<> S::operator int() const;
         Now that the qualifiers are known, we may be able to disambiguate
         such cases. */
      a_type_ptr  class_type = sym_parent_class(locator->specific_symbol);
      a_class_symbol_supplement_ptr
                  cssp = symbol_supplement_for_class(class_type);
      a_type_ptr  func_type = skip_typerefs(derived_type);
      a_symbol_ptr  conv_op;
      /* Managed class types don't allow cv-qualifiers on member functions:
         So we shouldn't get here with managed class types (and hence static
         conversion functions do not get here either). */
      check_assertion(!is_immediate_managed_class_type(class_type));
      conv_op = find_conversion_template_instance(
                       locator, cssp->conversion_template_list,
                       /*match_fn_qualifiers=*/TRUE,
                       func_type->variant.routine.extra_info->qualifiers);
      locator->specific_symbol = conv_op;
    }  /* if */
    *return_type = locator->variant.conversion_result_type;
  }  /* if */
}  /* process_conversion_function_declarator */


static a_boolean must_be_function_declarator(a_decl_flag_set input_flags)
/*
Return TRUE if, from the input_flags and the current context, we can
determine that a declaration must be a function declaration and, as a
result, disambiguation is not necessary.
*/
{
  a_boolean	result = FALSE;

  if ((input_flags & DI_IS_TEMPLATE_DECLARATION) != 0) {
    a_scope_stack_entry_ptr	ssep;
    check_assertion(depth_template_declaration_scope != NO_SCOPE_DEPTH);
    ssep = &scope_stack[depth_template_declaration_scope];
    if (ssep->templ_member_class_sym == NULL) {
      /* A template declaration that is not a qualified name.  This can
         only be a function declaration or a variable template, if they
         are enabled. */
      result = !variable_templates_may_be_enabled;
    }  /* if */
  } else if (is_real_instantiation_context()) {
    a_scope_stack_entry_ptr	ssep;
    ssep = &scope_stack[depth_innermost_instantiation_scope];
    if (ssep->function_partial_instantiation) {
      /* We are rescanning a function declarator to create the partial
         instantiation of a function template. */
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* must_be_function_declarator */


static a_boolean type_is_derived_from_function_declarator(a_type_ptr  tp)
/*
tp is the derived type of a declarator.  Return TRUE if that type is
constructed from a function declarator (e.g., a function type or an array of
pointer to function type, without intervening typedef/decltype constructors).
*/
{
  a_boolean  result = FALSE;

  while (tp != NULL) {
    switch (tp->kind) {
      case tk_routine:
        result = TRUE;
        goto done;
      case tk_pointer:
        tp = tp->variant.pointer.type;
        break;
      case tk_ptr_to_member:
        tp = tp->variant.ptr_to_member.type;
        break;
      case tk_array:
        tp = tp->variant.array.element_type;
        break;
      case tk_typeref:
        if (typeref_is_typedef(tp) || typeref_is_type_operator(tp)) {
          /* The type from a typedef name or decltype-like construct isn't the
             result of the current declarator. */
          goto done;
        } else {
          tp = tp->variant.typeref.type;
        }  /* if */
        break;
      default:
        goto done;
    }  /* switch */
  }  /* for */
done:
  return result;
}  /* type_is_derived_from_function_declarator */


static void cache_struct_bindings_list(a_decl_parse_state    *dps,
                                       a_decl_pos_block_ptr  decl_pos_block)
/*
The current token is a left bracket that looks like the beginning of a
structured bindings list.  Cache the list (including the delimiting brackets)
and record it in *dps.  Also update positions in decl_pos_block.
*/
{
  a_boolean err = FALSE;

  check_assertion(curr_token == tok_lbracket);
  decl_pos_block->decl_pos = pos_curr_token;
  dps->declarator_pos = pos_curr_token;
  if (!dps->auto_type_specifier_seen) {
    if (dps->secondary_declarator) {
      expect_error();
    } else {
      pos_error(ec_invalid_struct_binding_specifier, &dps->specifiers_pos);
      dps->specifiers_type = dps->declared_type = dps->type = error_type();
      err = TRUE;
    }  /* if */
  } else {
    if (!struct_bindings_enabled) {
      pos_warning(ec_struct_bindings_is_cpp17, &pos_curr_token);
    }  /* if */
  }  /* if */
  /* Check some constraints early.  In C++20 mode, structured bindings can have
     the static or thread_local storage classes. */
  if (dps->dso_flags & DSO_INLINE) {
    pos_error(ec_struct_binding_inline, &dps->inline_pos);
  } else if ((dps->dso_flags & DSO_CONSTEXPR) != 0 && !cpp26_mode) {
    pos_error(ec_struct_binding_constexpr, &dps->constexpr_pos);
    dps->dso_flags &= ~DSO_CONSTEXPR;
  } else if ((dps->dso_flags & DSO_CONSTEVAL) != 0) {
    pos_error(ec_struct_binding_consteval, &dps->constexpr_pos);
  } else if ((dps->declared_storage_class != sc_unspecified &&
              (!cpp20_mode ||
               dps->declared_storage_class != sc_static)) ||
             (!cpp20_mode && dps->dso_flags & DSO_THREAD_LOCAL)) {
    if (!cpp20_mode) {
      pos_error(ec_struct_binding_storage_class, &dps->storage_class_pos);
    } else {
      pos_error(ec_struct_binding_restricted_storage_class,
                &dps->storage_class_pos);
    }  /* if */
    dps->storage_class = sc_unspecified;
  }  /* if */
  dps->dso_flags &= ~(DSO_INLINE | DSO_CONSTEVAL);
  if (!cpp20_mode) {
    dps->dso_flags &= ~DSO_THREAD_LOCAL;
  }  /* if */
  if (type_is(dps->type, tk_pointer) &&
      !dps->type->variant.pointer.is_reference) {
    pos_error(ec_invalid_struct_binding_syntax, &dps->declarator_start_pos);
  }  /* if */
  if (dps->qualifiers & TQ_VOLATILE) {
    an_error_severity sev = cpp20_mode ? es_warning : es_remark;
    pos_diagnostic(sev, ec_volatile_str_bind_deprecated, &dps->qualifiers_pos);
  }  /* if */

  a_scanning_token_cache cache;
  if (cache_token_stream_until_matching_token(cache.ptr(), CTS_NO_OPTIONS)) {
    /* A syntax error.  We'll run into it again when we parse the cache
       later on. */
    expect_error();
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  decl_pos_block->declarator_range.end = end_pos_curr_token;
  curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  cache_curr_token(cache.ptr());
  (void)get_token();
  terminate_token_cache(cache.ptr());
  if (!err) {
    dps->is_struct_binding_decl = TRUE;
    dps->variant.struct_bindings_cache = new_fe<a_token_cache>(*cache);
  }  /* if */
  if (!dps->range_based_for) {
    /* An initializer should be next. */
    if (curr_token != tok_assign && curr_token != tok_lbrace &&
        curr_token != tok_lparen) {
      pos_error(ec_missing_initializer, &pos_curr_token);
    }  /* if */
  }  /* if */
}  /* cache_struct_bindings_list */


static void r_declarator(
                  a_decl_flag_set                  input_flags,
                  a_decl_flag_set                  *output_flags,
                  a_decl_parse_state               *state,
                  a_type_ptr                       specifiers_type,
                  a_type_ptr                       member_parent_type,
                  a_symbol_locator                 *locator,
                  a_type_ptr                       *p_complete_type,
                  a_type_ptr                       *p_bottom_derived_type,
                  a_boolean                        *is_constructor,
                  a_boolean                        *is_static_constructor,
                  a_boolean                        *is_destructor,
                  a_boolean                        *is_finalizer,
                  ARG_UNUSED a_call_conv_descr_ptr p_left_call_conv,
                  ARG_UNUSED a_call_conv_descr_ptr p_unbound_call_conv,
                  ARG_UNUSED a_type_qualifier_set  *p_left_qualifiers,
                  ARG_UNUSED a_type_qualifier_set  *p_unbound_qualifiers,
                  an_attribute_ptr                 *p_predeclarator_attributes,
                  a_source_sequence_entry_ptr      *declarator_ssep,
                  a_func_info_block                *func_info,
                  a_decl_pos_block_ptr             decl_pos_block)
/*
Scan a declarator or an abstract declarator, depending on the values of the
DI_REAL_DECLARATOR_ALLOWED and DI_ABSTRACT_DECLARATOR_ALLOWED flags in
input_flags.  input_flags also indicates various other options for parsing
(e.g., whether variable-length array declarators should be allowed)
and *output_flags returns some properties about the scanned declarator to the
caller.  *state contains state information about the declaration being parsed.
specifiers_type points to the type scanned in a preceding specifiers list, or
is NULL when this routine calls itself to scan a nested declarator.  Return in
*locator the symbol table locator and source position for the identifier in
the declarator (if only an abstract declarator is allowed, locator is not
used; if both real and abstract declarators are allowed, and an abstract
declarator is scanned, *locator is set to a null declarator).  Return
the final type (declarator derived types, if any, combined with the
specifiers type) in *p_complete_type.  If specifiers_type was NULL on
entry, *p_complete_type points to just the declarator derived type list
(with nothing attached to the bottom), or is NULL if there is no derived
type list.  *p_bottom_derived_type is set to point to the bottom type in
the declarator derived type list, or NULL if there is no derived type
list.  If source sequence entries are enabled, *declarator_ssep is set
to point to a source sequence entry for the declaration.  If the top
type in the declarator derived type list is a function, *func_info is
filled with extra information about the parameter list, for use if a
function body follows.  For declarators that may turn out to be member
functions, member_parent_type is a pointer to the class (or struct or
union) type of which it is a member; otherwise it is NULL.

The routine "declarator" is called at the top level, and it calls
this routine to do the actual work.  This routine can call itself
recursively to handle nested declarators.

If left-side or unbound qualifiers are detected, they are returned
in *p_left_call_conv, *p_unbound_call_conv, *left_qualifiers, and
*unbound_qualifiers.  See pointer_declarator for more information
on those.

For a nested declarator, *p_predeclarator_attributes is set to any
attributes scanned as the first construct in the nested declarator.

The original C89 syntax for declarators is:

       declarator:
		pointer    direct-declarator
		       opt

       direct-declarator:
		identifier
		( declarator )
		direct-declarator [ constant-expression    ]
						       opt
                direct-declarator ( parameter-type-list )
		direct-declarator ( identifier-list    )
						   opt
       pointer:
		* type-qualifier-list
				     opt
		* type-qualifier-list    pointer
				     opt

       abstract-declarator:
		pointer
		pointer    direct-abstract-declarator
                       opt

       direct-abstract-declarator:
		( abstract-declarator )
		direct-abstract-declarator    [ constant-expression    ]
					  opt                      opt
		direct-abstract-declarator    ( parameter-type-list    )
					  opt                      opt

C++, C99, and dialects thereof add various extensions to this (including
reference types, variable-length arrays, calling conventions, attributes,
etc.).
*/
{
  a_type_ptr            complete_type;
  a_type_ptr            derived_type;
  a_type_ptr            bottom_derived_type;
  a_type_ptr            new_type_ptr;
  a_source_position     declarator_pos;
  a_boolean             real_declarator_allowed;
  a_boolean             abstract_declarator_allowed;
  a_boolean             is_name_start;
  a_boolean             is_nonstatic_member_function = FALSE;
  a_boolean             nonconstant_dimension_allowed = FALSE;
  a_boolean             vla_allowed;
  a_boolean             vla_asterisk_allowed;
  a_boolean             parenthesized_initializer_allowed;
  a_boolean		not_a_function_declarator = FALSE;
  a_call_conv_descr     left_call_conv, inner_left_call_conv;
  a_call_conv_descr     unbound_call_conv;
  a_type_qualifier_set  left_qualifiers, inner_left_qualifiers;
  a_type_qualifier_set  unbound_qualifiers;
  a_boolean             disallow_default_args, disallow_exception_spec;
  a_func_info_block     *local_func_info;
  a_boolean             threads_dimension_allowed = FALSE;
  a_boolean             pointer_to_member_scanned;
  a_boolean             parenthesized_new_declarator = FALSE;
  a_boolean             allow_one_more_array_dimension = FALSE;
#if GNU_EXTENSIONS_ALLOWED
  an_attribute_ptr      predeclarator_attributes = NULL;
#endif /* GNU_EXTENSIONS_ALLOWED */

  db_enter(3, "r_declarator");
  set_err_pos_to_curr_token();
  /* Set declarator_pos to the start of the declarator (which may not be the
     position of the declarator-id).  It will be changed later if required. */
  copy_source_position(pos_curr_token, declarator_pos);
  *output_flags = DO_NO_OUTPUT_FLAGS;
  real_declarator_allowed = (input_flags & DI_REAL_DECLARATOR_ALLOWED) != 0;
  abstract_declarator_allowed =
                          (input_flags & DI_ABSTRACT_DECLARATOR_ALLOWED) != 0;
  parenthesized_initializer_allowed =
                    (input_flags & DI_PARENTHESIZED_INITIALIZER_ALLOWED) != 0;
  if (input_flags & DI_DIMENSION_EXPRESSION_ALLOWED) {
    /* A (direct or indirect) call from "new_type_name" to scan a declarator
       that is part of a new-expression with a parenthesized type name (in
       some GNU modes, part of the declarator may be outside the parentheses).
       A top-level array declarator may have a nonconstant dimension in that
       case. */
    nonconstant_dimension_allowed = TRUE;
    if (specifiers_type != NULL) {
      /* The top-level declarator. */
      parenthesized_new_declarator = TRUE;
    }  /* if */
  }  /* if */
  vla_allowed = (input_flags & DI_VLA_ALLOWED) != 0;
  vla_asterisk_allowed = (input_flags & DI_VLA_ASTERISK_ALLOWED) != 0;
  if (!real_declarator_allowed) {
    func_info = NULL;
    locator = NULL;
  } else if (input_flags & DI_IS_TYPEDEF_DECLARATION) {
    /* Avoid confusing a typedef declaration of a function type with a
       function declaration. */
    func_info = NULL;
  }  /* if */
  /* Set the locator to indicate there is no identifier. */
  if (locator != NULL) set_to_error_locator(*locator);
  /* Scan a list of pointer, reference and pointer-to-member declarators. */
  complete_type = pointer_declarator(specifiers_type, state,
                                     /*reference_allowed=*/!C_mode(),
                                     &left_call_conv, &unbound_call_conv,
                                     &left_qualifiers, &unbound_qualifiers,
                                     &pointer_to_member_scanned,
                                     decl_pos_block);
  if (complete_type != NULL && complete_type != specifiers_type) {
    /* We scanned a pointer or reference component. */
    *output_flags |= DO_HAS_PTR_OR_REF_COMPONENT;
    if (pointer_to_member_scanned) {
      /* We scanned a pointer-to-member declarator. */
      *output_flags |= DO_HAS_PTR_TO_MEMBER_COMPONENT;
    }  /* if */
    if (specifiers_type != NULL) {
      /* E.g., in "int *(*f)()" set state->type and state->declared_type to
         "int*" just before processing the nested declarator. */
      state->type = state->declared_type = complete_type;
    }  /* if */
  }  /* if */
#if UPC_EXTENSIONS_ALLOWED
  if (upc_mode && complete_type != NULL &&
      is_underlying_shared_qualified_type(complete_type)) {
    /* VLAs of UPC shared types are not allowed. */
    vla_allowed = FALSE;
    threads_dimension_allowed = TRUE;
  }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
  derived_type = NULL;
  bottom_derived_type = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ms_extensions) clear_call_conv_descr(&inner_left_call_conv);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
  if (near_and_far_enabled()) inner_left_qualifiers = TQ_NONE;
#endif /* NEAR_AND_FAR_ALLOWED */
  /* The next thing is an identifier, or a parenthesis that begins a
     nested declarator.  For the abstract declarator case, the
     identifier is omitted. */
  set_err_pos_to_curr_token();
  if (curr_token == tok_lparen) {
    /* Left parenthesis indicating nested declarator.  For the abstract
       declarator case, this might be a parenthesis indicating a function.
       We can differentiate the two cases because in the case of a nested
       declarator the next tokens must be "*", "(", "[", or "... <id>" whereas
       the function case is followed by ")", "...)", or a declaration
       specifier. */
    a_decl_flag_set       local_do_flags;
    a_type_qualifier_set  saved_qualifiers = state->qualifiers;
    a_source_position     saved_qualifiers_pos;
    a_boolean             saved_in_nested_declarator =
                                                  state->in_nested_declarator;
    (void)get_token();
    { an_attribute_ptr  prescanned_attributes;
      /* Scan any attributes, but "unscan" them right away.  We cannot decide
         their syntactic location at this time, so we'll "rescan" them once we
         know whether we are dealing with a nested declarator or a function
         declarator. */ 
      prescanned_attributes = scan_attributes(al_prefix);
      if (prescanned_attributes != NULL) {
        unscan_attributes(prescanned_attributes);
      }  /* if */
    }
    if (abstract_declarator_allowed) {
      a_pack_expansion_stack_entry_ptr	pesep = NULL;
      a_pack_expansion_descr_ptr	pedp;
      a_boolean				any_args = FALSE;
      if (curr_token != tok_rparen) {
        /* We could be at the start of a function declarator or a parenthesized
           initializer.  Determining which case we have may involve
           coalescing an identifier that could be part of a pack expansion.
           As a result, we need to push the pack expansion context now.
           During a real instantiation of a variadic template with an empty
           pack we do further processing and possibly determine whether
           this is a function further below. */
        any_args = begin_potential_pack_expansion_context_full(
                                         &pesep, &pedp, /*is_lookahead=*/TRUE,
                                         /*allow_empty_list=*/FALSE,
                                         /*ignore_suppression=*/FALSE,
                                         /*claim_pack_index=*/FALSE);
      }  /* if */
      if (curr_token == tok_rparen || !any_args ||
          is_decl_start(IDS_REAL_DECLARATOR_ALLOWED |
                        IDS_MS_ATTRIB_NOT_ALLOWED) ||
          (curr_token == tok_ellipsis && next_token() == tok_rparen)) {
        /* Function declarator rather than a nested declarator. */
        if (curr_token == tok_ellipsis && pesep != NULL) {
          /* Beware of (...) in variadic contexts.  Abandon the pack
             started above. */
          abandon_potential_pack_expansion_context(pesep);
        }  /* if */
        goto function_lparen;
      }  /* if */
      if (pesep != NULL) {
        /* Abandon this expansion.  A new one may be started later by the
           begin_... call later in this routine. */
        abandon_potential_pack_expansion_context(pesep);
      }  /* if */
    }  /* if */
    /* This parenthesis begins a nested declarator. */
    state->in_nested_declarator = TRUE;
#if GNU_EXTENSIONS_ALLOWED
    predeclarator_attributes = scan_predeclarator_attributes();
    if (gnu_mode) {
      /* GCC accepts declarations like:
             void (__attribute((cdecl)) **p)();
         The attributes in that position appertain to the type underlying the
         declarator (here, the function type).  Currently, however, we only
         take the attributes into account if no nested pointer operators have
         been seen.  I.e., we accept:
             int *((__attribute((stdcall)) **p2))();
         but not:
             int (*(__attribute((stdcall)) **p3))();
         This is a limitation of how we currently build up types.  E.g., in
         the case of p3, the attribute would be applied to a type "pointer
         to <null>", which is too little information to determine if the
         attribute is valid. */
      if (specifiers_type != NULL) {
        /* The first level of declarator nesting. */
        p_predeclarator_attributes = &predeclarator_attributes;
      } else if (complete_type == NULL && p_predeclarator_attributes != NULL) {
        /* A more deeply nested level of declarator nesting that hasn't
           introduced pointer operators. */
        *p_predeclarator_attributes = predeclarator_attributes;
        p_predeclarator_attributes = &predeclarator_attributes;
      } else {
        /* Some nested pointer operators were seen: Don't accept more
           predeclarator attributes. */
        if (predeclarator_attributes != NULL) {
          pos_error(ec_attribute_not_allowed,
                    &predeclarator_attributes->position);
        }  /* if */
        predeclarator_attributes = NULL;
        p_predeclarator_attributes = NULL;
      }  /* if */
      p_predeclarator_attributes =
                              last_attribute_link(p_predeclarator_attributes);
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (ms_extensions) {
      if (unbound_call_conv.call_conv != (a_calling_convention)cc_default) {
        /* Constructs such as
             int __cdecl (*fp)();
           are not permitted. */
        pos_error(ec_calling_convention_may_not_precede_nested_declarator,
                  &declarator_pos);
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
    if (ms_extensions or_near_and_far_enabled()) {
      if (unbound_qualifiers != TQ_NONE) {
        /* Constructs such as
             int far (*p);
           are not permitted. */
        pos_error(ec_mem_attrib_may_not_precede_nested_declarator,
                  &declarator_pos);
      }  /* if */
    }  /* if */
#endif  /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
    add_stop_token(tok_rparen);
    /* Call r_declarator recursively to parse the nested declarator.  Some
       state must be saved and restored for this call.  For example,
       "int const (* const g())()" has two meaningless const qualifiers
       that are diagnosed, but when declarator processing is completed,
       *state->qualifiers should reflect the outermost qualifier (i.e., the
       first one in this example). */
    saved_qualifiers = state->qualifiers;
    saved_qualifiers_pos = state->qualifiers_pos;
    /* Get the nested declarator, removing the flag allowing parenthesized
       initializers from the input_flags bit vector.  (The other flags are
       passed on in the recursive call.) */
    r_declarator((input_flags & ~DI_PARENTHESIZED_INITIALIZER_ALLOWED),
                 &local_do_flags, state, /*specifiers_type=*/(a_type_ptr)NULL,
                 member_parent_type, locator, &derived_type,
                 &bottom_derived_type, is_constructor, is_static_constructor, 
                 is_destructor, is_finalizer, &inner_left_call_conv,
                 &unbound_call_conv, &inner_left_qualifiers,
                 &unbound_qualifiers, p_predeclarator_attributes,
                 declarator_ssep, func_info, decl_pos_block);
    state->in_nested_declarator = saved_in_nested_declarator;
    copy_qualifiers(saved_qualifiers, state->qualifiers);
    state->qualifiers_pos = saved_qualifiers_pos;
    if (local_do_flags & DO_HAS_PTR_OR_REF_COMPONENT) {
      /* A nested declarator that contained a pointer, pointer-to-member, or
         reference component.  If we were to scan an array bound next, the
         end result would not be a VLA type (instead it would e.g. be a
         "pointer to a VLA type" or perhaps something more complicated). */
      state->nested_ptr_or_ref_seen = TRUE;
      if (input_flags & DI_VARIABLY_MODIFIED_DECL_ALLOWED) {
        vla_allowed = TRUE;
      }  /* if */
      /* Propagate the flag up. */
      *output_flags |= DO_HAS_PTR_OR_REF_COMPONENT;
      if (local_do_flags & DO_HAS_PTR_TO_MEMBER_COMPONENT) {
        *output_flags |= DO_HAS_PTR_TO_MEMBER_COMPONENT;
      }  /* if */
    }  /* if */
    if (local_do_flags & DO_REAL_DECLARATOR_SCANNED) {
      *output_flags |= DO_REAL_DECLARATOR_SCANNED;
      /* Copy the position of the declarator-id into declarator_pos. */
      declarator_pos = error_position;
    } else {
      parenthesized_initializer_allowed = FALSE;
    }  /* if */
    if (local_do_flags & DO_CFRONT_MEMBER_FUNCTION_TYPEDEF) {
      *output_flags |= DO_CFRONT_MEMBER_FUNCTION_TYPEDEF;
      /* Force function_declarator to add an implicit-this-param pointer
         to the routine type. */
      check_assertion(locator != NULL);
      member_parent_type = qualifier_class_type(*locator);
      check_assertion(member_parent_type != NULL);
    } else if (member_parent_type == NULL && locator != NULL) {
      /* In certain error cases (involving parenthesized declarators) the
         declaration can be marked as a constructor or destructor but
         member_parent_type will be NULL.  In other cases of parenthesized
         declarators member_parent_type must be updated from the locator. */
      if (is_error_locator(*locator) &&
          (*is_constructor || *is_static_constructor ||
           *is_destructor || *is_finalizer)) {
        *is_constructor = *is_static_constructor = FALSE;
        *is_destructor = *is_finalizer = FALSE;
      } else {
        member_parent_type = qualifier_class_type(*locator);
      }  /* if */
    }  /* if */
    if (local_do_flags & DO_SCOPE_DEACTIVATION_REQUIRED) {
      /* A class scope was reactivated to scan a static data member or a
         member function.  It will have to be deactivated when the scanning
         of the top-level declarator is complete. */
      *output_flags |= DO_SCOPE_DEACTIVATION_REQUIRED;
    }  /* if */
    /* A nonconstant dimension, if allowed at all, is allowed only on the
       topmost type.  Set the flag to FALSE for subsequent processing. */
    nonconstant_dimension_allowed = FALSE;
    /* Check for and get the closing parenthesis. */
    (void)required_token(tok_rparen, ec_exp_rparen);
    remove_stop_token(tok_rparen);
#if GNU_EXTENSIONS_ALLOWED
    if (curr_token == tok_attribute && !state->in_nested_declarator) {
      /* GNU attributes may appear after the outermost nested declarator.
         They are treated as declarator-id attributes. */
      scan_gnu_declarator_attributes(state);
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  } else {
    /* Not a nested declarator. */
    a_source_position  ellipsis_pos;
    /* Check for a declarator-id or abstract declarator that starts with an
       ellipsis indicating a parameter pack.  There is an ambiguity in this
       context where the ellipsis can either indicate a parameter pack for an
       abstract declarator, or a classic vararg function: It is the former
       only if the specifiers type is a pattern.  E.g.:
         template<typename ... T> void f(T ...);  // Parameter pack.
         template<typename T>     void f(T ...);  // Classic vararg function.
       During a tentative scan of an abbreviated function template or generic
       lambda, an "auto" parameter is treated as a pattern. */
    if (variadic_templates_enabled && curr_token == tok_ellipsis &&
        (next_token() != tok_rparen || any_packs_referenced() ||
         state->variant.auto_params != NULL)) {
      /* An ellipsis at this point can indicate a parameter pack. */
      if (state->pack_ellipsis_allowed) {
        state->has_pack_ellipsis = TRUE;
        ellipsis_pos = pos_curr_token;
        /* For function parameter pack declarations, the ellipsis is a sort of
           expansion (the type of the parameter must be a "pattern"), but for
           template parameter declarations that is sometimes not the case.
           Something like a template parameter "int ...N" is a pack declaration
           and not an expansion.  But for a template parameter that expands
           an enclosing pack (e.g., "Enclosing_T ... N"), it is a pack
           expansion.  Also, when scanning an "auto" parameter declaration for
           the first time, there is no associated template parameter yet to
           record the ellipsis. */
        if (((input_flags & DI_IS_TEMPLATE_PARAM_DECL) != 0 &&
             (input_flags & DI_IS_TEMPLATE_PARAM_PACK_EXPANSION) == 0) ||
            (state->variant.auto_params != NULL)) {
          (void)get_token();
        } else {
          record_pack_expansion_ellipsis();
        }  /* if */
      } else {
        /* An error will be issued elsewhere if it turns out we are not
           in an expansion. */
      }  /* if */
    }  /* if */
    /* An identifier is expected next, but is omitted in the 
       abstract declarator. */
    is_name_start = (is_decl_qualified_name_start() ||
                     curr_token == tok_operator || curr_token == tok_compl
#if MICROSOFT_EXTENSIONS_ALLOWED
                     || (cli_or_cx_enabled && curr_token == tok_not)
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                                                          );
    if (!real_declarator_allowed ||
        (abstract_declarator_allowed && !is_name_start)) {
      /* Identifier is omitted in an abstract declarator.  Be sure it is not a
         tk_unknown type. */
      if (state->has_pack_ellipsis) {
        /* Disallow a pack ellipsis in a nested abstract declarator. */
        if (specifiers_type == NULL) {
          pos_error(ec_abstract_declarator_pack_is_nested, &ellipsis_pos);
        }  /* if */
      }  /* if */
      check_assertion(specifiers_type == NULL ||
                      !is_unknown_type(specifiers_type) ||
                      (complete_type != NULL &&
                       is_or_contains_error_type(complete_type)));
      parenthesized_initializer_allowed = FALSE;
    } else if (curr_token == tok_lbracket &&
               (struct_bindings_enabled ||
                (cpp11_mode && gpp_version_is(>= 70000)) ||
                clangcpp_version_is(>= 40000)) &&
               specifiers_type != NULL && !abstract_declarator_allowed &&
               !state->in_class_scope) {
      /* This looks like the bracket introducing a list of structured
         bindings. */
      declarator_pos = pos_curr_token;
      make_struct_binding_container_locator(locator, &declarator_pos);
      cache_struct_bindings_list(state, decl_pos_block);
      goto past_postfix_declarator_operators;
    } else {
      /* Real (non-abstract) declarator. */
      check_assertion(locator != NULL);
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if DEBUG
/* GCOVR_EXCL_START */
      if (!source_sequence_entries_disallowed &&
          (debug_level >= 4 || db_flag_is_set("dump_ss_full"))) {
        fprintf(f_debug, "declarator: empty ss entry for \"%s\":\n",
                curr_token == tok_identifier ?
                  locator_for_curr_id.symbol_header->identifier :
                  token_names[(int)curr_token]);
      }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
      *declarator_ssep = add_empty_source_sequence_entry();
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      if (state->p_postfix_entities != NULL) {
        /* If we're in a declaration statement, record the end of the
           associated entities list. */
        while (*state->p_postfix_entities != NULL) {
          state->p_postfix_entities = &(*state->p_postfix_entities)->next;
        }  /* while */
      }  /* if */
      *output_flags |= DO_REAL_DECLARATOR_SCANNED;
      /* Process the name declared here. */
      scan_real_declarator_id(state, input_flags, output_flags, locator,
                              is_constructor, is_static_constructor,
                              is_destructor, is_finalizer,
                              &parenthesized_initializer_allowed,
                              &not_a_function_declarator,
                              &member_parent_type, decl_pos_block);
      /* Type attributes may have changed state->declared_type, which should
         stay in sync with complete_type in non-nested contexts. */
      if (!state->in_nested_declarator) {
        complete_type = state->declared_type;
      }  /* if */
      /* Reset declarator_pos to correspond to the position of the
         declarator-id.  Doing this after the call to scan_real_declarator_id
         ensures the position is that of the main identifier (and not e.g.
         a qualifier). */
      declarator_pos = locator->source_position;
      decl_pos_block->decl_pos = declarator_pos;
    }  /* if */
  }  /* if */
  consume_any_stray_microsoft_rparen();
  /* The declarator can end at this point, or an array or function
     specification (or a series of them) can follow.  The additional
     specifications, if they appear, are parsed in their order of 
     appearance.  However, the resulting type has to be built from
     the top down, which makes the logic a bit convoluted.  For example,

     int a[3][4];

     is a 3-element array, each of whose elements is a four-element
     int array.  The type "array[3] of" is built on the first iteration;
     on the second iteration the type "array[4] of" is built, and
     the new type is added to the end of the existing list, giving
     "array[3] of array[4] of".  Then the loop ends, and the array
     type is attached to the original type "int" (from complete_type).

     If a nested declarator was scanned above, there may already be
     a derived type list, and the new entries are added to its bottom.
  */
  while (curr_token == tok_lparen || curr_token == tok_lbracket ||
         curr_token == tok_rparen) {
    if (curr_token == tok_rparen) {
      /* Normally, a right parenthesis at this point is not part of the
         declarator.  An exception occurs when emulating the new-expression
         syntax of early GNU C++ compilers.  For example, "new (int[n])[3]"
         treats the "[3]" as part of the type being allocated.  Only one
         (array) declarator level is allowed after the right parentheses in
         those cases. */
      if (parenthesized_new_declarator && gpp_mode && gnu_version < 30400 &&
          next_token() == tok_lbracket) {
        (void)get_token();
        remove_stop_token(tok_rparen);
        *output_flags |= DO_RPAREN_IN_NEW_DECLARATOR;
        allow_one_more_array_dimension = TRUE;
      } else {
        break;
      }  /* if */
    }  /* if */
    if (curr_token == tok_lparen) {
      /* Appears to be a function declarator.  But be sure it's not the
         start of a parenthesized initializer (C++ only). */
      a_source_position  lparen_pos;
      lparen_pos = pos_curr_token;
      /* Advance past the left parenthesis. */
      (void)get_token();
      if (parenthesized_initializer_allowed &&
          curr_token != tok_rparen && curr_token != tok_ellipsis) {
        /* The context and other information we have about the declarator do
           not preclude a parenthesized initializer, nor does the token that
           follows the left paren.  If the construct inside the parentheses
           could be interpreted as a declaration, then do so.  Otherwise,
           treat this as a parenthesized initializer.  The
           not_a_function_declarator flag is set when the declarator-id is
           scanned if the name found is a qualified name that names a static
           data member.  This is used in template cases when implicit typename
           is enabled because is_decl_not_expr can return an incorrect result
           in such cases.  In other cases, we still use is_decl_not_expr
           because it provides for better diagnostics later on. */
        a_pack_expansion_stack_entry_ptr	pesep;
        a_pack_expansion_descr_ptr		pedp;
        a_boolean				any_args;
        /* We could be at the start of a function declarator or a parenthesized
           initializer.  Determining which case we have may involve
           coalescing an identifier that could be part of a pack expansion.
           As a result, we need to push the pack expansion context now.
           During a real instantiation of a variadic template with an empty
           pack, we should rely on the value of is_function_declarator saved
           during the prototype instantiation to determine whether or not
           this is a function. */
        any_args = begin_potential_pack_expansion_context_full(
                                         &pesep, &pedp, /*is_lookahead=*/TRUE,
                                         /*allow_empty_list=*/FALSE,
                                         /*ignore_suppression=*/FALSE,
                                         /*claim_pack_index=*/FALSE);
        if (!is_template_dependent_context() && pedp != NULL) {
          /* This is a real instantiation.  If we found a function declarator
             in the prototype instantiation, treat this as one now. */
          if (pedp->is_function_declarator) goto function_lparen;
        }  /* if */
        if (locator->is_qualified_name && locator->specific_symbol != NULL &&
            is_function_or_template_symbol(locator->specific_symbol)) {
          /* This must be a function declarator re-declaring a prior function
             or member function (or function template).  No need to rely on
             is_decl_not_expr for that case. */
        } else if ((!any_args && pedp != NULL &&
                    !pedp->is_function_declarator) ||
                   (not_a_function_declarator && use_implicit_typename() &&
                    is_template_dependent_context()) ||
                   (!must_be_function_declarator(input_flags) &&
                    !is_decl_not_expr(DFS_ABSTRACT_DECLARATOR_ALLOWED |
                                      DFS_REAL_DECLARATOR_ALLOWED))) {
          a_boolean  is_function_decl = FALSE;
          /* This appears to be a parenthesized initializer.  However, it
             might also be a function definition with an old-style parameter
             list.  If, starting with the current token, we have a comma-
             separated list of identifiers followed by a right paren followed
             by a left brace or the start of a declaration, then this can
             only be a function definition.  Otherwise assume it to be a
             parenthesized initializer.  GNU C++ does not accept old-style
             parameter lists. */
          if (!any_args && pedp != NULL && !pedp->is_function_declarator) {
            /* We know from the prototype instantiation that this is a
               parenthesized initializer. */
          } else if (curr_token == tok_identifier && !gpp_mode) {
            a_scanning_token_cache cache;

            cache_curr_token(cache.ptr());
            /* Advance past all comma-identifier pairs till what should be
               the closing paren. */
            while (get_token() == tok_comma) {
              cache_curr_token(cache.ptr());
              if (get_token() == tok_identifier) {
                cache_curr_token(cache.ptr());
              } else {
                break;
              }  /* if */
            }  /* while */
            /* If there is a closing paren, see if the next token is the
               start of a declaration or else a left brace introducing the
               function body. */
            if (curr_token == tok_rparen) {
              cache_curr_token(cache.ptr());
              if (get_token() == tok_lbrace ||
                  is_decl_start(IDS_REAL_DECLARATOR_ALLOWED)) {
                /* This looks exactly like a function declaration with an
                   old style parameter list. */
                is_function_decl = TRUE;
              }  /* if */
            }  /* if */
            rescan_cached_tokens(cache.ptr());
          }  /* if */
          if (!is_function_decl) {
            if (pesep != NULL) {
              /* We are committing to a parenthesized initializer, so this
                 disambiguation-only pack context must not remain on the
                 stack. */
              abandon_potential_pack_expansion_context(pesep);
              pesep = NULL;
            }  /* if */
            *output_flags |= DO_PARENTHESIZED_INITIALIZER;
            if (decl_pos_block != NULL) {
              decl_pos_block->var_init_range.start = lparen_pos;
            }  /* if */
            /* Function_declarator should not be called, so exit the loop. */
            break;
          }  /* if */
        }  /* if */
        /* Save the knowledge that this should be processed as a function
           declarator (note that we only get here with pedp non-NULL in
           template dependent contexts). */
        if (pedp != NULL) pedp->is_function_declarator = TRUE;
      }  /* if */
function_lparen:
      /* For function types as the top type, fetch the extra function info
         as well.  For non-top types, do not. */
      local_func_info = func_info;
      if (C_dialect == C_dialect_cplusplus) {
        if (derived_type != NULL) {
          /* If the function is pointed to by a pointer-to-member type, we need
             to pass the class-of-which-a-member to function_declarator. */
          a_type_ptr tp = bottom_derived_type;
          if (tp != NULL && is_ptr_to_member_type(tp)) {
            /* Declaration of a pointer to member function. */
            member_parent_type = pm_class_type(tp);
            is_nonstatic_member_function = TRUE;
          } else {
            member_parent_type = NULL;
          }  /* if */
          local_func_info = NULL;
          *is_constructor = *is_static_constructor = FALSE;
          *is_destructor = *is_finalizer = FALSE;
        } else if (*output_flags & DO_CFRONT_MEMBER_FUNCTION_TYPEDEF) {
          check_assertion(func_info == NULL);
          is_nonstatic_member_function = TRUE;
          *is_constructor = *is_static_constructor = FALSE;
          *is_destructor = *is_finalizer = FALSE;
        } else if (func_info == NULL) {
          is_nonstatic_member_function = FALSE;
          *is_constructor = *is_static_constructor = FALSE;
          *is_destructor = *is_finalizer = FALSE;
          member_parent_type = NULL;
        } else if (*is_constructor || *is_destructor || *is_finalizer) {
          is_nonstatic_member_function = TRUE;
          if ((*output_flags & DO_HAS_PTR_OR_REF_COMPONENT) &&
              !(state->dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER)) {
            /* Something like "*~D()". */
            pos_error(*is_constructor ? ec_bad_constructor_decl :
                      (an_error_code)
                      (*is_destructor ? ec_bad_destructor_decl
                                      : ec_bad_finalizer_decl),
                      &state->declarator_start_pos);
          }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else if (*is_static_constructor) {
          is_nonstatic_member_function = FALSE;
        } else if (cli_or_cx_enabled &&
                   in_static_cli_property_or_event_definition()) {
          /* Accessor functions for static C++/CLI properties and events are
             always static member functions. */
          is_nonstatic_member_function = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        } else {
          if (input_flags & DI_NONSTATIC_MEMBER) {
            if (locator->is_operator_name &&
                (is_new_operator(locator->variant.opname) ||
                 is_delete_operator(locator->variant.opname))) {
              /* operator new and operator delete are always static, even
                 if "static" was not specified in the declaration. */
            } else {
              is_nonstatic_member_function = TRUE;
            }  /* if */
          }  /* if */
        }  /* if */
        if (is_nonstatic_member_function) {
          state->is_nonstatic_member_function_decl = TRUE;
        }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (microsoft_mode && is_nonstatic_member_function &&
            member_parent_type != NULL && derived_type == NULL &&
            !(input_flags & DI_IS_TYPEDEF_DECLARATION) &&
            scope_stack[decl_scope_level].kind ==
                                       (a_scope_kind)sck_class_struct_union &&
            depth_innermost_instantiation_scope < decl_scope_level) {
          /* Microsoft mode allows for "selective virtual overriders" in which
             a qualified name is used for a member function declaration.
             However, in that case the member_parent_type is not the type
             used as a qualifier, but the enclosing class.  This is only an
             issue for top-level type declarators scanned in class scope. */
          member_parent_type = scope_stack[decl_scope_level].assoc_type;
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else {
        /* Normal C case.  If the derived type is nonnull this is not the
           top-most type, so we don't want to fetch the extra function info. */
        if (derived_type != NULL) local_func_info = NULL;
      }  /* if */
      /* Pass in a flag to indicate whether default arguments are allowed at
         all.  They should be disallowed on top-level function declarations
         for explicit template instantiations and template specializations --
         for instance:
           template <class T> void f(T) { ... }
           template<> void f(int=0);
           template void f(char=0);
         (However, Microsoft and GNU compilers allow the default arguments on
         explicit instantiations.)
         In addition, default arguments are disallowed in template parameter
         declarations. */
      disallow_default_args = C_mode() ||
                              (input_flags & DI_IS_TEMPLATE_PARAM_DECL);
      if (local_func_info != NULL) {
        if ((input_flags & DI_IS_SPECIALIZATION) != 0) {
          disallow_default_args = TRUE;
        } else if ((input_flags & DI_IS_EXPLICIT_INSTANTIATION) != 0 &&
                   !(gpp_mode || microsoft_mode)) {
          disallow_default_args = TRUE;
        }  /* if */
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
      /* Default arguments are also disallowed for generic functions and for
         members of managed class types. */
      if (cppcx_enabled && member_parent_type != NULL &&
          is_ref_class_type(member_parent_type)) {
        /* Non-public C++/CX ref class methods do permit default arguments. */
      } else if (cli_or_cx_enabled &&
                 (state->is_generic_declaration ||
                  (is_nonstatic_member_function &&
                   member_parent_type != NULL &&
                   is_immediate_managed_class_type(member_parent_type)))) {
        disallow_default_args = TRUE;
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      /* Pass in a flag to indicate whether exception specifications are
         allowed.  C++17 made a change that allows them on all function
         declarators (enabled when exc_spec_in_func_type is TRUE).  Otherwise,
         they are allowed on a declaration of a function, a pointer or
         reference to function, or a pointer to member function.  The
         declaration must be a top-level declaration, a parameter declaration,
         or a return type; it cannot be a typedef declaration.  That turns out
         to correspond to places where real declarators are allowed.  GNU and
         Microsoft compilers also allow exception specifications in other
         places (e.g., types in casts) and we also allow it as an extension
         in other nonstrict modes. */
      if (C_mode()) {
        disallow_exception_spec = TRUE;
      } else if (exc_spec_in_func_type) {
        disallow_exception_spec = FALSE;
      } else {
        disallow_exception_spec = TRUE;
        if (!(input_flags & DI_IS_TYPEDEF_DECLARATION) &&
            ((input_flags & DI_REAL_DECLARATOR_ALLOWED) ||
             !strict_ansi_mode)) {
          if (derived_type == NULL ||
              type_is_derived_from_function_declarator(derived_type)) {
            /* Top level function declaration, or return type of function
               type. */
            disallow_exception_spec = FALSE;
          } else if (is_any_ptr_or_ref_type(derived_type)) {
            /* If derived_type is a pointer or reference type that currently
               points to NULL, this can be assumed to be a top-level pointer
               or reference declaration, and an exception specification is
               permitted:
                 void (*pf)() throw();    // Okay
                 void (**ppf)() throw();  // Error
               (Handle and tracking-reference types cannot refer to function
               types, but there is no need to produce an additional diagnostic
               for the exception specification.) */
            disallow_exception_spec = (type_pointed_to(derived_type) != NULL);
          } else if (is_ptr_to_member_type(derived_type)) {
            /* Similarly if derived_type is a pointer-to-member type whose
               member pointer is NULL:
                 void (A::*pmf)() throw ();  // Okay
                 void (A::**ppmf)() throw(); // Error
                 void (* A::*pm)() throw();  // Error
            */
            disallow_exception_spec = (pm_member_type(derived_type) != NULL);
          }  /* if */
        }  /* if */
      }  /* if */
      function_declarator(state, input_flags, &new_type_ptr, local_func_info,
                          locator, member_parent_type,
                          is_nonstatic_member_function,
                          *is_constructor, *is_static_constructor,
                          *is_destructor, *is_finalizer,
                          disallow_default_args, disallow_exception_spec,
                          decl_pos_block);
      if (state->has_trailing_return_type) {
        /* function_declarator encountered a trailing return type (which means
           that "complete_type" corresponded to a simple "auto", except
           possibly in GNU C++ mode).  Replace the "auto" placeholder type by
           the actual return type (which was recorded as the specifiers_type
           in *state). */
        check_assertion(complete_type == state->auto_type ||
                        (gpp_version_is(any_version) &&
                         skip_typerefs_not_typedefs(complete_type) ==
                                                           state->auto_type));
        complete_type = state->specifiers_type;
      }  /* if */
      if (local_func_info != NULL &&
          (input_flags & DI_IS_EXPLICIT_INSTANTIATION) != 0 &&
          local_func_info->any_default_args) {
        pos_warning(ec_nonstd_default_arg, &locator->source_position);
      }  /* if */
      /* Check that we do not create a typedef for a pointer or reference to a
         qualified function type.  (We don't check C++/CLI handles and tracking
         references here because they cannot refer to function types;
         additional diagnostics would not be helpful.) */
      if (new_type_ptr->kind == (a_type_kind)tk_routine &&
          derived_type != NULL && is_ptr_or_ref_type(derived_type)) {
        a_routine_type_supplement_ptr  rtsp =
                                     new_type_ptr->variant.routine.extra_info;
        if (rtsp->this_class == NULL &&
            (rtsp->qualifiers | rtsp->this_qualifiers) != TQ_NONE) {
          /* Catch the following:
                typedef void (*PF)() const;
             Qualified function types are only allowed to declare members,
             pointer-to-members and synonym typedefs. */
          pos_error(ec_ptr_or_ref_to_qualified_function_type,
                    locator != NULL ? &locator->source_position
                                    : &lparen_pos);
        }  /* if */
      }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
      if (func_info != NULL) {
        /* Record the source sequence entry in func_info even if there was
           an error in the declarator (i.e., even if the locator is an error
           locator).  This could mean an empty source sequence entry is in
           the list when there's an error, but that should be okay. */
        func_info->declarator_ssep = *declarator_ssep;
      }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    } else {
      /* Left bracket, indicating array declarator. */
      a_boolean  top_level_field_decl, top_level_param_decl;
      /* This is a top-level declarator if derived_type is NULL; it's a field
         declaration only if the nonstatic member flag is set.  (Note: it
         will be set for fields in C mode as well as in C++ mode.) */
      top_level_field_decl = derived_type == NULL ?
                                       (input_flags & DI_NONSTATIC_MEMBER) :
                                       derived_type->kind == tk_ptr_to_member;
      /* See whether this is a top-level declarator in a parameter
         declaration. */
      top_level_param_decl = (input_flags & DI_IS_PARAMETER_DECL) &&
                             derived_type == NULL;
      array_declarator(state, &new_type_ptr, nonconstant_dimension_allowed,
                       vla_allowed, vla_asterisk_allowed,
                       threads_dimension_allowed, top_level_field_decl,
                       top_level_param_decl, decl_pos_block);
      if (nonconstant_dimension_allowed) {
        /* In C++ a array declarator that appears in an operator new()
           expression may have a nonconstant expression in the first
           dimension (ARM 5.3.3).  Subsequent dimensions must be constants. */
        nonconstant_dimension_allowed = FALSE;
      }  /* if */
    }  /* if */
#if GNU_EXTENSIONS_ALLOWED
    if (specifiers_type != NULL && predeclarator_attributes != NULL) {
      /* Apply the nested predeclarator attributes to the topmost array or
         function type.  Note that this means attribute application routines
         must deal with partially assembled types. */
      attach_type_attributes(&new_type_ptr, predeclarator_attributes,
                             (void*)state);
      predeclarator_attributes = NULL;
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (ms_extensions) {
      /* Apply left-side qualifiers that were hanging:
           int (__cdecl *f)();
                             ^ We're here now.
                ^ inner_left_call_conv indicates this.
         The __cdecl calling convention was returned from the nested
         declarator scan and goes on top of the function type. */
      if (inner_left_call_conv.call_conv != (a_calling_convention)cc_default) {
        update_calling_convention(&new_type_ptr, &inner_left_call_conv, state,
                                  locator != NULL ? &locator->source_position
                                                  : &declarator_pos);
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
    if (near_and_far_enabled()) {
      if (inner_left_qualifiers != TQ_NONE) {
        if (new_type_ptr->kind == (a_type_kind)tk_array) {
          /* A case like
               int (__near *p)[5];
             The qualifiers cannot be applied to the array now because they
             go on the element type and the element type is not attached yet.
             Leave them in inner_left_qualifiers for the next time around the
             loop. */
        } else {
          /* Function case.  The qualifiers can be added now. */
          new_type_ptr = make_qualified_type(new_type_ptr,
                                             inner_left_qualifiers);
          inner_left_qualifiers = TQ_NONE;
        }  /* if */
      }  /* if */
    }  /* if */
#endif  /* NEAR_AND_FAR_ALLOWED */
    /* Type attributes may have changed state->declared_type, which should
       stay in sync with complete_type in non-nested contexts. */
    if (!state->in_nested_declarator) {
      complete_type = state->declared_type;
    }  /* if */
    /* Add the new type to the bottom of the existing derived type list.
       Note that this involves error checking. */
    add_to_derived_type_list(new_type_ptr, &derived_type, &bottom_derived_type,
                             state, (input_flags & DI_IS_PARAMETER_DECL) != 0);
    consume_any_stray_microsoft_rparen();
    if (allow_one_more_array_dimension) {
      break;
    }  /* if */
  }  /* while */
past_postfix_declarator_operators:
  /* Set the referenced flag on the specifiers type if this is the top-level
     scan of the declarator (i.e., if specifiers_type is non-NULL) -- but
     do this only if a real declarator was scanned.  This enables us to
     properly handle tags whose definitions appear in the declaration of
     another entity -- e.g., "struct S {int a;} x", where there is no reference
     to S by tag name (and so the symbol's referenced flag is not set) but
     there IS a use of the IL entity (and therefore the reference flag in the
     type entry is set).  (Compare this to "struct S {int a;}; struct S x",
     where the reference to (use of) the type appears with the reference
     to the tag name.)  As written, this sets the referenced flag for all
     types, not just tags, which is harmless. */
  if (specifiers_type != NULL) {
    (skip_typerefs(specifiers_type))->source_corresp.referenced = TRUE;
  }  /* if */
  /* Use the position of the identifier as the position of this declarator
     for error purposes.  If this was an abstract declarator, declarator_pos
     has been set to the beginning of the declarator.  The error position
     is set a bit early here so that any errors below from combining the
     two lists will have the right position. */
  copy_source_position(declarator_pos, error_position);
#if GNU_EXTENSIONS_ALLOWED
  if (specifiers_type != NULL && predeclarator_attributes != NULL) {
    /* Apply the nested predeclarator attributes to the type specified
       before the nested declarator (if there were postfix declarator
       operators, the attributes were already consumed above). */
    attach_type_attributes(&complete_type, predeclarator_attributes,
                           (void*)state);
    predeclarator_attributes = NULL;
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ms_extensions) {
    /* Apply left-side qualifiers that were hanging, for the case where
       there were no function or array declarators:
         typedef void F(int);
         F (__cdecl *f);
                       ^ We're here now.
            ^ inner_left_call_conv indicates this.
       The __cdecl calling convention was returned from the nested
       declarator scan and goes on top of the complete type.  If there
       is no complete type, move these hanging left-side qualifiers to
       the variables that will apply them to the specifiers type. */
    if (inner_left_call_conv.call_conv != (a_calling_convention)cc_default) {
      if (complete_type != NULL) {
        update_calling_convention(&complete_type, &inner_left_call_conv,
                                  state, &declarator_pos);
      } else {
        check_assertion(left_call_conv.call_conv ==
                        (a_calling_convention)cc_default);
        left_call_conv = inner_left_call_conv;
      }  /* if */
    }  /* if */
    /* Return the left calling convention to the caller if it can't be
       handled at this level (i.e., in a nested declarator). */
    if (specifiers_type == NULL) {
      check_assertion(p_left_call_conv != NULL);
      *p_left_call_conv = left_call_conv;
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
  if (near_and_far_enabled()) {
    if (inner_left_qualifiers != TQ_NONE) {
      if (complete_type != NULL) {
        check_for_addition_of_incompatible_qualifiers(complete_type,
                                                      &inner_left_qualifiers,
                                                      &declarator_pos);
        complete_type = make_qualified_type(complete_type,
                                            inner_left_qualifiers);
      } else {
        check_assertion(left_qualifiers == TQ_NONE);
        left_qualifiers = inner_left_qualifiers;
      }  /* if */
    }  /* if */
    /* Return the left qualifiers to the caller if they can't be handled
       at this level by applying them to the specifiers type (i.e., in a
       nested declarator). */
    if (specifiers_type == NULL) {
      check_assertion(p_left_qualifiers != NULL);
      *p_left_qualifiers = left_qualifiers;
    }  /* if */
  }  /* if */
#endif /* NEAR_AND_FAR_ALLOWED */
  if (specifiers_type != NULL) {
    /* This is a top-level call to declarator.  Do some checks for special
       member functions and set complete_type appropriately, so that it can
       be added as return type to the associated routine type. */
    if (!(input_flags & DI_OPERATOR_NAME_ALLOWED) && locator != NULL &&
        (locator->is_operator_name || locator->is_conversion_name ||
         locator->is_udl_operator_name)) {
      pos_error(ec_operator_name_not_allowed, &locator->source_position);
      set_to_error_locator(*locator);
      complete_type = error_type();
    } else if (locator != NULL && locator->is_conversion_name) {
      /* Do error checking on the conversion function declaration. */
      process_conversion_function_declarator(locator, state, input_flags,
                                             derived_type, &complete_type);
    } else if (*is_constructor || *is_static_constructor) {
      /* Return type should be "unknown" at this point, unless the declarator
         was parenthesized in which case decl_specifiers will have thought we
         are in an "implicit int" case.  Change it to void. */
      if (!is_unknown_type(specifiers_type) &&
          !(input_flags & DI_NO_TYPE_SPECIFIERS)) {
        pos_error(ec_return_type_on_constructor, &state->specifiers_pos);
      }  /* if */
      complete_type = void_type();
    } else if (*is_destructor) {
      /* Make the destructor return "void". */
      if (is_error_locator(*locator)) {
        complete_type = error_type();
      } else {
        if (!(input_flags & DI_NO_TYPE_SPECIFIERS)) {
          pos_error(ec_return_type_on_destructor, &state->specifiers_pos);
        } else if (derived_type == NULL || !is_function_type(derived_type)) {
          pos_error(ec_bad_destructor_decl, &state->start_pos);
        }  /* if */
        complete_type = void_type();
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (*is_finalizer) {
      /* Make the finalizer return "void". */
      if (is_error_locator(*locator)) {
        complete_type = error_type();
      } else {
        if (!(input_flags & DI_NO_TYPE_SPECIFIERS)) {
          pos_error(ec_return_type_on_finalizer, &state->specifiers_pos);
        } else if (derived_type == NULL || !is_function_type(derived_type)) {
          pos_error(ec_bad_finalizer_decl, &state->start_pos);
        }  /* if */
        complete_type = void_type();
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else {
      /* Not a conversion function, constructor, or destructor, therefore
         something where a specifier is expected.  If we inherited a
         specifier type of unknown from the first declarator in a
         declaration, e.g., in a case like
           operator int(), j;
         issue an error. */
      if (is_unknown_type(specifiers_type) &&
          !(gcc_mode && state->has_deduced_type)) {
        pos_error(ec_missing_decl_specifiers, &state->declarator_start_pos);
        complete_type = error_type();
      }  /* if */
    }  /* if */
  }  /* if */
  if (derived_type != NULL && complete_type != NULL &&
      bottom_derived_type != NULL) {
    if (bottom_derived_type->kind == (a_type_kind)tk_pointer &&
        bottom_derived_type->variant.pointer.is_reference &&
        is_any_reference_type(complete_type)) {
      /* If we are creating a reference (bottom_derived_type) to a reference
         (complete_type), complete_type must be a reference as a consequence
         of specifiers_type being a reference (i.e., the specifiers contained
         a typedef or template parameter referring to a reference).  Otherwise,
         an error should be issued. */
      a_boolean  is_rvalue_ref =
                      bottom_derived_type->variant.pointer.is_rvalue_reference;
      a_source_position_ptr
                 qual_pos = (state->qualifiers == TQ_RESTRICT) ?
                                 &state->restrict_pos : &state->qualifiers_pos;
      if (specifiers_type == NULL || !is_any_reference_type(specifiers_type)) {
        pos_error(ec_reference_to_reference, &error_position);
        derived_type = error_type();
      } else {
        a_boolean  tracking_ref = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
        tracking_ref = bottom_derived_type->variant.pointer.is_handle;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        derived_type =
                  make_reference_to_reference(complete_type, is_rvalue_ref,
                                              tracking_ref, state->qualifiers,
                                              qual_pos, (a_boolean*)NULL);
      }  /* if */
      state->unused_qualifiers = FALSE;
      /* The second reference component is essentially ignored.  We do not
         need to call add_to_derived_type_list in this case. */
      bottom_derived_type = derived_type;
    } else {
      /* Combine the derived type list with the earlier complete type
         (pointer derived type list plus specifiers_list), making
         the full type.  Note that this involves error checking. */
      add_to_derived_type_list(complete_type,
                               &derived_type, &bottom_derived_type, state,
                               (input_flags & DI_IS_PARAMETER_DECL) != 0);
    }  /* if */
    complete_type = derived_type;
  } else {
    if (derived_type != NULL) complete_type = derived_type;
    /* Find the bottom of the type to be returned. */
    if (complete_type != NULL && !is_error_type(complete_type)) {
      bottom_derived_type = find_bottom_of_type(complete_type);
    } else {
      bottom_derived_type = NULL;
    }  /* if */
  }  /* if */
#if UPC_EXTENSIONS_ALLOWED
  if (upc_mode && complete_type != NULL) {
    check_and_update_upc_type(complete_type, input_flags);
  }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ms_extensions) {
    if (unbound_call_conv.call_conv != (a_calling_convention)cc_default) {
      /* If there is an unbound calling convention, attempt to apply it to
         the complete type (if one exists).  If none exists, return the unbound
         type to the caller. */
      if (complete_type != NULL) {
        update_calling_convention(&complete_type, &unbound_call_conv, state,
                                  &declarator_pos);
      } else {
        check_assertion(p_unbound_call_conv != NULL);
        *p_unbound_call_conv = unbound_call_conv;
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
  if (ms_extensions or_near_and_far_enabled()) {
    if (unbound_qualifiers != TQ_NONE) {
      /* If there are unbound type qualifiers, apply them to the complete
         type (if it exists).  If it does not exist, return the unbound
         type qualifiers to the caller. */
      if (complete_type != NULL) {
#if NEAR_AND_FAR_ALLOWED
        check_for_addition_of_incompatible_qualifiers(complete_type,
                                                      &unbound_qualifiers,
                                                      &declarator_pos);
#endif  /* NEAR_AND_FAR_ALLOWED */
        complete_type = make_qualified_type(complete_type, unbound_qualifiers);
      } else {
        check_assertion(p_unbound_qualifiers != NULL);
        *p_unbound_qualifiers = unbound_qualifiers;
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
  if (specifiers_type != NULL) {
    /* This is a top-level call to declarator. */
    check_assertion(complete_type != NULL);
    if (!is_function_type(complete_type)) {
      if (locator != NULL &&
          (locator->is_operator_name || locator->is_conversion_name ||
           locator->is_udl_operator_name)) {
        /* A declaration of an operator must have a function type. */
        pos_error(ec_function_type_required, &locator->source_position);
        set_to_error_locator(*locator);
        complete_type = bottom_derived_type = error_type();
      }  /* if */
#if UPC_EXTENSIONS_ALLOWED
      if (is_underlying_shared_qualified_type(complete_type) &&
          (input_flags & DI_NONSTATIC_MEMBER) != 0) {
        /* Shared types cannot be allocated inside structs or unions. */
        pos_error(ec_shared_inside_struct, &error_position);
        complete_type = bottom_derived_type = error_type();
      }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
    } else if (func_info != NULL) {
      /* Note the presence of "explicit( <bool-expr> )" (before we potentially
         copy the type to create a "declared type"). */
      if (state->conditional_explicit_attr) {
        if (!type_is(complete_type, tk_routine)) {
          /* "explicit" is only permitted on constructors and on conversion
             functions, neither of which can be declared with a typedef or
             decltype construct for the top-level function type. */
          expect_error();
        } else {
          complete_type->variant.routine.extra_info
                       ->is_conditionally_explicit = TRUE;
        }  /* if */
      }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
      { /* Set the declared type in the func_info block.  Note that further
           fixup may be required for member functions, since default argument
           expressions will not have been scanned yet. */
        a_type  *dtype = form_declared_type(complete_type, func_info);
        if (state->conditional_explicit_attr) {
          /* The copied type needs an associated ak_conditional_explicit
             attribute. */
          an_attribute  *ap2, *ap1 = find_attribute(ak_conditional_explicit,
                                                    state->prefix_attributes);
          check_assertion(ap1 != NULL);
          copy_attribute(ap1, ap2);
          *last_attribute_link(&dtype->source_corresp.attributes) = ap2;
        }  /* if */
        func_info->declared_type = dtype;
      }
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    }  /* if */
  }  /* if */
  if (*output_flags & DO_SCOPE_DEACTIVATION_REQUIRED) {
    /* A class scope was reactivated when a qualified name was seen. */
    if (specifiers_type != NULL) {
      /* This is a top-level call to declarator, so the class scope can now
         be deactivated. */
      if (scope_stack[depth_scope_stack].kind ==
                          (a_scope_kind)sck_class_reactivation) {
        pop_class_reactivation_scope();
      } else {
        /* Must be a namespace reactivation. */
        pop_namespace_reactivation_scope();
      }  /* if */
      /* Clear the flag, just to be neat. */
      *output_flags &= ~(a_decl_flag_set)DO_SCOPE_DEACTIVATION_REQUIRED;
    } else {
      /* Just pass the information up to the caller. */
    }  /* if */
  }  /* if */
  *p_complete_type = complete_type;
  *p_bottom_derived_type = bottom_derived_type;
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 3) {
    fputs("complete_type: ", f_debug);
    if (complete_type == NULL) {
      fputs("<null>", f_debug);
    } else {
      db_type(complete_type);
    }  /* if */
    (void)fputc('\n', f_debug);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  db_exit();
}  /* r_declarator */


static void scan_trailing_requires_clause(a_decl_parse_state  *dps,
                                          a_func_info_block   *func_info,
                                          a_symbol_locator    *loc)
/*
The current token is "requires" following an otherwise-complete declarator
described by dps, func_info, and loc.  Parse or skip the requires-clause that
presumably follows, as appropriate, and update dps->trailing_requires_clause
as needed.  (Note that N4885 [temp.inst]/17 makes it clear that (trailing)
requires clauses aren't instantiated when a member function of a template
class is partially instantiated.
*/
{
  a_boolean  discard_clause = dps->is_template_rescan,
             is_ordinary_member_instantiation = FALSE,
             pop_func_prototype_scope = FALSE,
             in_member_template_decl = FALSE;
  a_scope_stack_entry_ptr
             ssep = &scope_stack_top();

  if (scope_is(ssep, sck_template_declaration) &&
      !(dps->dso_flags & DSO_FRIEND)) {
    in_member_template_decl = TRUE;
    ssep = &scope_stack[ssep->previous_scope];
  }  /* if */
  if (scope_is(ssep, sck_class_struct_union)) {
    a_type_ptr  class_type = ssep->assoc_type;
    if (is_unspecialized_template_class(class_type) &&
        !class_type->variant.class_struct_union.is_prototype_instantiation) {
      /* When instantiating members of class templates, ignore the
         requires-clause tokens.  The requires-clause recorded for the
         prototype instantiation will be recorded instead, and will be
         substituted at the first point of reference. */
      discard_clause = TRUE;
      is_ordinary_member_instantiation = !in_member_template_decl;
    }  /* if */
  }  /* if */
  if (!type_is(dps->type, tk_routine) ||
      (!(is_template_dependent_context() &&
         dps->function_definition_allowed) &&
       !discard_clause)) {
    pos_error(ec_trailing_requires_clause_not_on_template, &pos_curr_token);
  }  /* if */
  /* Reactivate any parent scope and the function parameter scope. */
  if (loc == NULL) {
    /* Presumably an abstract declarator, which should not permit a requires-
       clause in the first place.  Push a dummy instantiation scope to ensure
       memory regions are set up correctly during error recovery. */
    expect_error();
    push_instantiation_scope_for_constraint_type();
  } else if (loc->is_class_member) {
    if (is_immediate_class_type(loc->parent.class_type)) {
      push_class_reactivation_scope(loc->parent.class_type,
                                    /*extend_namespace=*/FALSE);
    }  /* if */
  } else if (loc->parent.namespace_ptr != NULL) {
    push_namespace_reactivation_scope(loc->parent.namespace_ptr);
  }  /* if */
  if (type_is(dps->type, tk_routine) && func_info != NULL) {
    (void)push_scope((a_scope_kind)sck_func_prototype, func_info->scope_number,
                     dps->type, (a_routine_ptr)NULL);
    scope_stack_top().decl_parse_state = dps;
    scope_stack_top().outside_parameter_list = TRUE;
    reactivate_prototype_scope_symbols(func_info->prototype_scope_symbols);
    pop_func_prototype_scope = TRUE;
  }  /* if */
  dps->trailing_requires_clause = scan_requires_clause(discard_clause);
  if (pop_func_prototype_scope) {
    pop_scope();
  }  /* if */
  if (loc == NULL) {
    /* This can happen in error cases (see above for the expect_error
       check). */
    pop_instantiation_scope_for_constraint_type();
  } else if (loc->is_class_member) {
    if (is_immediate_class_type(loc->parent.class_type)) {
      pop_class_reactivation_scope();
    }  /* if */
  } else if (loc->parent.namespace_ptr != NULL) {
    pop_namespace_reactivation_scope();
  }  /* if */
  if (is_ordinary_member_instantiation) {
    dps->pending_trailing_requires_clause = TRUE;
  }  /* if */
}  /* scan_trailing_requires_clause */


static void use_nonreal_type_for_nested_prototype_type(
						a_decl_parse_state	*state)
/*
In a class prototype instantiation context for a member declaration like:

  class X {} x;

The type used for the declaration of "x" must be the associated nonreal type.
If we are in such a context, update the types in the decl_parse_state.
*/
{
  if (scope_stack_top().kind == (a_scope_kind)sck_class_struct_union) {
    a_type_ptr  tp = state->type;
    if (tp->source_corresp.is_class_member) {
      a_symbol_ptr  sym = symbol_for(tp);
      a_symbol_ptr  nonreal_sym = nonreal_type_if_nested_prototype_type(sym);
      if (sym != nonreal_sym) {
        state->specifiers_type = state->declared_type = state->type =
                                                nonreal_sym->variant.type.ptr;
      }  /* if */
    }  /* if */
  }  /* if */
}  /* use_nonreal_type_for_nested_prototype_type */


typedef Ptr_map<a_token_sequence_number, an_auto_param_descr*> 
		an_abbr_lambda_descr_map;

STATIC_THREAD an_abbr_lambda_descr_map
		*abbr_lambda_descrs;
			/* Map from token sequence numbers to "auto" parameter
			   lists for lambdas that appear in templates (i.e.,
			   in prototype instantiations).  The lists can then be
			   reused in real instantiations.  That is not only a
			   performance optimization, but it also avoids issues
			   with "auto..." parameter packs that would otherwise
			   accidentally be expanded to empty lists because of
			   the missing template declaration context. */

a_symbol_header* sym_hdr_for_capture(a_lambda_capture  *lcp)
/*
Return the symbol header associated with the given capture.  If the capture is
for "this", return NULL.
*/
{
  a_symbol_header  *hdr = NULL;

retry:
  if (lcp->is_init_capture) {
    a_decl_parse_state  *dps = lcp->capture_info.init_capture_dps;
    if (dps != NULL) {
      hdr = dps->sym->header;
    } else {
      hdr = symbol_for(lcp->closure_field)->header;
    }  /* if */
  } else if (!lcp->is_indirect_init_capture) {
    if (lcp->captured.variable != NULL &&
        symbol_for(lcp->captured.variable) != NULL) {
      hdr = symbol_for(lcp->captured.variable)->header;
    }  /* if */
  } else if (lcp->field_pending) {
    lcp = lcp->capture_info.source_capture;
    goto retry;
  } else {
    hdr = symbol_for(lcp->captured.init_capture_field)->header;
  }  /* if */
  return hdr;
}  /* sym_hdr_for_capture */


void scan_lambda_declarator(a_decl_parse_state  *dps,
                            a_func_info_block   *func_info,
                            a_tmpl_decl_state   *templ_state,
                            a_decl_pos_block    *decl_pos_block)
/*
Scan the "declarator" part of a C++ lambda construct. That includes the
parameter list, optionally followed by "mutable", an exception specification,
and/or a lambda return type.  The caller must ensure that the current token is
the left parenthesis introducing the declarator-like construct.
dps, func_info, templ_state, and decl_pos_block describe the lambda declarator
(templ_state is provided in case this is a generic lambda).
*/
{
  a_type_ptr               func_type = void_type(), closure_class;
  a_decl_flag_set          di_flags = DI_NONSTATIC_MEMBER;
  a_symbol_locator         loc;
  a_scope_stack_entry_ptr  ssep = &scope_stack_top();
  a_token_sequence_number  reparse_tsn = curr_token_sequence_number;
  a_decl_parse_callback    *reparse_actions = dps->end_of_parse_actions;
  a_boolean                already_template = FALSE,
                           in_prototype_instantiation =
                                  scope_stack_top().in_prototype_instantiation;

  if (scope_is(ssep, sck_class_struct_union)) {
    /* We haven't determined yet whether this is a generic lambda. */
    closure_class = ssep->assoc_type;
    dps->variant.auto_params = abbr_lambda_descrs->get(reparse_tsn);
    if (dps->variant.auto_params != NULL) {
      /* This is a lambda declarator that was previously encountered in a
         prototype instantiation and found to have "auto" parameters.  Set up
         a template declaration context up front. */
      set_up_generic_lambda_declarator_scan(dps, templ_state);
      dps->is_abbr_func_template = TRUE;
      dps->variant.auto_params = NULL;
      dps->start_tsn = reparse_tsn;
      begin_caching_fetched_tokens(/*include_curr_token=*/TRUE);
    } else if (generic_lambdas_enabled) {
      begin_potential_abbr_func_templ_caching(dps);
    }  /* if */
  } else if ((scope_is(ssep, sck_template_declaration) &&
              scope_is(ssep-1, sck_class_struct_union)) ||
             (scope_is(ssep, sck_template_instantiation) &&
              scope_is(ssep-1, sck_class_reactivation))) {
    /* A generic lambda (first scan, or instantiation). */
    if (scope_is(ssep, sck_template_declaration) && generic_lambdas_enabled) {
      /* Presumably a lambda with C++20-style explicit template parameters.
         We may still encounter additional "auto" parameters, which would
         require re-parsing the declarator. */
      begin_potential_abbr_func_templ_caching(dps);
      already_template = TRUE;
    }  /* if */
    closure_class = (ssep-1)->assoc_type;
  } else {
    expect_error();
    closure_class = error_type();
  }  /* if */
reparse_declarator:
  check_assertion(curr_token == tok_lparen);
  make_opname_locator((an_opname_kind)onk_function_call, &loc,
                      &pos_curr_token);
  add_stop_token(tok_rparen);
  (void)get_token();
  function_declarator(dps, di_flags, &func_type, func_info, &loc,
                      closure_class,
                      /*is_nonstatic_member=*/TRUE, /*is_constructor=*/FALSE, 
                      /*is_static_constructor=*/FALSE, /*is_destructor=*/FALSE,
                      /*is_finalizer=*/FALSE, !lambda_default_args_enabled,
                      /*disallow_exception_spec=*/FALSE, decl_pos_block);
  remove_stop_token(tok_rparen);
  if (dps->decl_being_cached) {
    /* We called begin_potential_abbr_func_templ_caching in case "auto"
       parameters would be encountered. */
    if (dps->variant.auto_params != NULL) {
      /* At least one "auto" parameter was encountered: Reparse the declarator
         in the corresponding template declaration context. */
      a_lambda_ptr lambda = func_info->lambda;

      { a_scanning_token_cache  reparse_cache;

        /* Create a cache with the declarator tokens. */
        copy_tokens_from_cache(curr_lexical_state_cache(),
                               reparse_tsn, curr_token_sequence_number,
                               /*include_last_token=*/FALSE,
                               reparse_cache.ptr());
        end_potential_abbr_func_templ_caching(
                                            dps,
                                            /*remove_pack_descriptors=*/TRUE);
        rescan_cached_tokens(reparse_cache.ptr());
      }
      set_up_generic_lambda_declarator_scan(dps, templ_state);
      /* Clear the declaration parse state associated with the declarator.
         This is most easily done by pretending we are about to scan a
         secondary declarator. */
      discard_end_of_parse_actions(dps, /*until_action=*/reparse_actions);
      start_secondary_declarator(dps);
      dps->secondary_declarator = FALSE;
      dps->is_abbr_func_template = TRUE;
      dps->declarator_start_pos = pos_curr_token;
      dps->declarator_pos = pos_curr_token;
      clear_func_info(func_info);
      func_info->lambda = lambda;
      if (!already_template) {
        begin_caching_fetched_tokens(/*include_curr_token=*/TRUE);
      }  /* if */
      if (in_prototype_instantiation) {
        /* We encountered "auto" parameters in a lambda inside a template.
           Save the associated "auto" parameter descriptions so we can use them
           to create the needed template declaration up front, without a
           tentative parse.  This is not just a performance improvement: It
           ensures that "auto ..." parameter packs aren't mistakenly skipped as
           empty packs during real instantiations of the enclosing template. */
        abbr_lambda_descrs->map(reparse_tsn, dps->variant.auto_params);
        dps->variant.auto_params = NULL;
      } else {
        free_auto_param_descriptions(dps);
      }  /* if */
      goto reparse_declarator;
    } else {
      end_potential_abbr_func_templ_caching(dps);
    }  /* if */
  }  /* if */
  /* Check that the parameters do not conflict with the captures.  Note that
     func_info->lambda is NULL when rescanning generic lambdas, but we do not
     need to recheck conflicts in that case. */
  if (func_info->lambda != NULL &&
      !ms_version_is(any_version) && !clang_version_is(<80000) &&
      !gnu_version_is(<90000)) {
    a_lambda          *lambda = func_info->lambda;
    a_lambda_capture  *lcp = lambda->capture_list;
    for (; lcp != NULL; lcp = lcp->next) {
      a_symbol_header  *hdr = sym_hdr_for_capture(lcp);
      if (hdr != NULL) {
        a_symbol  *sym = func_info->prototype_scope_symbols;
        for (; sym != NULL; sym = sym->next_in_scope) {
          if (sym->header == hdr) {
            pos_diagnostic(es_discretionary_error,
                           ec_parameter_capture_conflict, &sym->decl_position);
            goto next_capture;
          }  /* if */
        }  /* for */
      }  /* if */
next_capture:;
    }  /* for */
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  func_info->declared_type = func_type;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Record whether an explicit return type was specified. */
  if (dps->has_trailing_return_type) {
    if (!is_error_type(func_type)) {
      /* function_declarator doesn't itself "connect" the function type to its
         return type because of the possibility of complex nested-declarator
         situations (instead that connection is usually done in r_declarator
         for non-lambda declarators).  So we do this manually here. */
      a_type_ptr  bottom_derived_type = func_type;
      add_to_derived_type_list(dps->type, &func_type, &bottom_derived_type,
                               dps, /*parameter_type=*/FALSE);
      check_assertion(is_function_type(func_type));
    }  /* if */
  } else if (!is_error_type(func_type)) {
    check_assertion(is_function_type(func_type));
    func_type->variant.routine.return_type =
                                   make_auto_type(&null_source_position,
                                                  /*is_decltype_auto=*/FALSE);
    dps->has_deducible_return_type = TRUE;
  }  /* if */
  dps->type = func_type;
  if (curr_token == tok_requires && !dps->is_trailing_return_type) {
    if (dps->lambda_with_omitted_parameters) {
      pos_error(ec_lambda_without_parameters_requires_clause, &pos_curr_token);
    }  /* if */
    scan_trailing_requires_clause(dps, func_info, &loc);
  }  /* if */
}  /* scan_lambda_declarator */


void declarator(a_decl_flag_set             input_flags,
                a_decl_parse_state          *state,
                a_type_ptr                  member_parent_type,
                a_symbol_locator            *locator,
                a_func_info_block           *func_info,
                a_decl_pos_block_ptr        decl_pos_block)
/*
Scan a declarator.  This is an interface routine for r_declarator, provided
so that parameters needed only on recursive calls for nested declarators
need not be supplied on other calls.  See r_declarator for the meaning of
the parameters.
*/
{
  a_type_ptr         bottom_derived_type = NULL;
  a_boolean          is_constructor = FALSE, is_destructor = FALSE,
                     is_static_constructor = FALSE, is_finalizer = FALSE;

  is_constructor = (input_flags & DI_IS_CONSTRUCTOR) != 0;
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cli_or_cx_enabled && (state->dso_flags & DSO_STATIC_CONSTRUCTOR) != 0) {
    is_static_constructor = TRUE;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* If DI_IS_CONSTRUCTOR is set, the parent class should be provided. */
  check_assertion_str(!is_constructor || member_parent_type != NULL ||
                      (input_flags & (DI_IS_FRIEND_DECL |
                                      DI_IS_DEDUCTION_GUIDE)),
                      "declarator: parent class is NULL for ctor");
  state->declarator_start_pos = pos_curr_token;
  if (is_prototype_instantiation_context()) {
    /* See if the type being used is a type nested in a prototype
       instantiation. */
    use_nonreal_type_for_nested_prototype_type(state);
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    decl_pos_block->declarator_range.start = pos_curr_token;
    decl_pos_block->declarator_range.end = end_pos_curr_token;
  }  /* if */
  curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (state->declared_storage_class == (a_storage_class)sc_typedef) {
    input_flags |= DI_IS_TYPEDEF_DECLARATION;
  } else if (member_parent_type != NULL) {
    /* The declarator appears in a class definition. */
    if ((state->dso_flags & DSO_FRIEND) != 0) {
      /* Friend declarations aren't really member declarations. */
      member_parent_type = NULL;
      input_flags |= DI_IS_FRIEND_DECL;
    } else if (!C_mode() &&
               state->storage_class != (a_storage_class)sc_static) {
      /* The storage class "static" was not specified and this is a member
         declaration that is not a friend declaration, therefore, if this
         is a member function declaration, it will be a nonstatic member
         function.  This is important because when the routine type
         is created, function_declarator needs to know whether to
         add a this class to the type. */
      input_flags |= DI_NONSTATIC_MEMBER;
    }  /* if */
  }  /* if */
  r_declarator(input_flags, &state->do_flags, state, state->type,
               member_parent_type, locator, &state->declared_type,
               &bottom_derived_type, &is_constructor, &is_static_constructor, 
               &is_destructor, &is_finalizer, (a_call_conv_descr_ptr)NULL, 
               (a_call_conv_descr_ptr)NULL, (a_type_qualifier_set *)NULL, 
               (a_type_qualifier_set *)NULL, (an_attribute_ptr*)NULL,
               &state->source_sequence_entry, func_info, decl_pos_block);
  /* r_declarator will have set error_position to the position of the
     declarator-id if this is a real declarator and the first token of the
     whole declarator if it is an abstract declarator. */
  state->declarator_pos = error_position;
  if (state->is_inclass_member_function_decl) {
    scan_member_function_modifiers(locator, state, func_info);
  }  /* if */
  if (is_constructor) {
    state->do_flags |= DO_IS_CONSTRUCTOR;
  }  /* if */
  if (is_destructor) {
    state->do_flags |= DO_IS_DESTRUCTOR;
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cli_or_cx_enabled) {
    /* is_constructor and is_static_constructor cannot both be TRUE. */
    check_assertion(!(is_constructor && is_static_constructor));
    /* is_destructor and is_finalizer cannot both be TRUE either. */
    check_assertion(!(is_destructor && is_finalizer));
    if (is_static_constructor) {
      state->do_flags |= DO_IS_STATIC_CONSTRUCTOR;
    } else if (is_finalizer) {
      state->do_flags |= DO_IS_FINALIZER;
    }  /* if */
  } else {
    /* Static constructors only occur in C++/CLI mode. */
    check_assertion(!is_static_constructor && !is_finalizer);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (state->do_flags & DO_HAS_PTR_TO_MEMBER_COMPONENT) {
    (void)check_for_vla_in_pointer_to_member(state->declared_type,
                                             &state->declarator_start_pos);
  }  /* if */
  if (curr_token == tok_lparen &&
      (input_flags & DI_PARENTHESIZED_INITIALIZER_ALLOWED) != 0 &&
      (state->do_flags & DO_PARENTHESIZED_INITIALIZER) == 0) {
    /* A call to declarator consumes the left parenthesis introducing a
       parenthesized initializer.  This is usually done by the call to
       r_declarator (where at first the parenthesis could still introduce
       a function parameter list), but in some cases (e.g. after a GNU
       attribute) the parenthesis might not have been considered yet. */
    state->do_flags |= DO_PARENTHESIZED_INITIALIZER;
    if (decl_pos_block != NULL) {
      decl_pos_block->var_init_range.start = pos_curr_token;
    }  /* if */
    (void)get_token();
  }  /* if */
  check_pending_qualifiers_used(state);
  if (state->has_deduced_type) {
    check_type_with_placeholder_specifier(state);
  } else if (locator != NULL && locator->is_conversion_name) {
    /* Check if the conversion type involves "auto" or "decltype(auto)", and
       if so update *state to reflect this. */
    if (deduced_return_types_enabled && is_auto_type(bottom_derived_type)) {
      state->has_deducible_return_type = TRUE;
    }  /* if */
  }  /* if */
  state->type = state->declared_type;
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cli_or_cx_enabled &&
      state->declared_storage_class != (a_storage_class)sc_typedef &&
      !state->no_special_cli_class_type_check &&
      !check_invalid_use_of_special_cli_class_type(
                                       state->type, &state->specifiers_pos)) {
    /* Some special C++/CLI class types (e.g., delegates) are invalid at this
       point. */
    state->type = error_type();
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (state->type == NULL) {
    expect_error();
    state->type = error_type();
  }  /* if */
  if (curr_token == tok_requires && !state->is_trailing_return_type) {
    if (type_is(state->type, tk_routine) &&
        state->variant.auto_params != NULL &&
        !state->is_abbr_func_template) {
      /* Presumably this is the first pass scanning an abbreviated template.
         Ignore the requires-clause this time: It will be reparsed in a
         template context.  Note that this should be the last "declaration"
         component before reparsing, and thus we can simply not consume it. */
    } else {
      scan_trailing_requires_clause(state, func_info, locator);
    }  /* if */
  }  /* if */
}  /* declarator */

void declarator_one_time_init(void)
/*
Do one-time initialization of static variables defined in this file.
*/
{
  if (precompiled_header_processing_required) {
    STATIC_THREAD a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(abbr_lambda_descrs),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
}  /* declarator_one_time_init */


void declarator_init(void)
/*
Do initialization of static variables defined in this file that require
initialization for each compilation.
*/
{
  abbr_lambda_descrs = alloc_fe_of_type(an_abbr_lambda_descr_map);
  construct(abbr_lambda_descrs, /*mask_width=*/10u);
  noexcept_args = alloc_fe_of_type(a_noexcept_arg_map);
  construct(noexcept_args, /*mask_width=*/10u);
}  /* declarator_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

