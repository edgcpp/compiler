/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

decl_spec.c -- Scanning of declaration specifiers.

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
#if MICROSOFT_EXTENSIONS_ALLOWED
#include "ms_attrib.h"
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#include "disambig.h"
#include "folding.h"
#if MAINTAIN_NEEDED_FLAGS
#include "il_walk.h"
#endif /* MAINTAIN_NEEDED_FLAGS */
#include "statements.h"
#include "ifc_modules.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

#if NEAR_AND_FAR_ALLOWED

static void scan_near_or_far(a_type_qualifier_set  *qualifiers)
/*
Scan a "near" or "far" memory attribute, setting a bit in *qualifiers if
there is no error.
*/
{
  a_type_qualifier_set new_qualifier;

  if (curr_token == tok_near) {
    new_qualifier = TQ_NEAR;
  } else {
    check_assertion(curr_token == tok_far);
    new_qualifier = TQ_FAR;
  }  /* if */
  /* Check for incompatibilities. */
  if (*qualifiers & new_qualifier) {
    /* The bit is already set -- this is a duplicate. */
    pos_warning(ec_dupl_mem_attrib, &error_position);
  } else if ((*qualifiers & (TQ_NEAR | TQ_FAR)) != TQ_NONE) {
    /* The other bit has already been set -- error. */
    pos_error(ec_mem_attrib_incompatible, &error_position);
    new_qualifier = TQ_NONE;
  }  /* if */
  *qualifiers |= new_qualifier;
  (void)get_token();        
}  /* scan_near_or_far */

#endif /* NEAR_AND_FAR_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED

a_boolean convert_GUID_string_literal(a_constant_ptr  strcon,
                                      char            **pstr)
/*
strcon represents a string literal.  Return TRUE if its contents are of the
form
 hhhhhhhh-hhhh-hhhh-hhhh-hhhhhhhhhhhh
(where "h" is any hex digit and the hyphens are required), optionally enclosed
in braces.  If TRUE is returned, set *pstr to point to a null-terminated,
normalized (i.e., lower case) copy of the sequence of digits and hyphens;
otherwise, set *pstr to NULL.
*/
{
  a_boolean      result = TRUE;
  a_const_char   *str = strcon->variant.string.value;
  a_targ_size_t  length;

  *pstr = NULL;
  /* Get the string length not including the trailing null character. */
  check_assertion(strcon->variant.string.length > 0);
  length = strcon->variant.string.length-1;
  /* A valid GUID string is 36 or 38 characters (depending on the presence of
     braces).  Check this first to avoid bound error with pointer and length
     adjustments below. */
  if (length < 36) {
    result = FALSE;
    goto done;
  }  /* if */
  /* Check and adjust for surrounding braces. */
  if (*str == '{') {
    /* Check for matching closing brace. */
    if (str[length-1] != '}') {
      result = FALSE;
      goto done;
    }  /* if */
    str++;
    length -= 2;
  }  /* if */
  /* Check the digits and hyphens. */
  if (!is_valid_GUID_string(str, length)) {
    result = FALSE;
  } else {
    /* Copy the string, lower-casing hex letters so that strcmp can be used to
       compare strings. */
    a_const_char *src = str;
    char         *dst = alloc_primary_file_scope_il((sizeof_t)length+1);
    int          count = (int)length;
    *pstr = dst;
    for (; count != 0; count--) {
      char ch = *src++;
      if (isalpha((unsigned char)ch)) ch = (char)tolower((int)ch);
      *dst++ = ch;
    }  /* for */
    *dst = '\0';
  }  /* if */
done:
  return result;
}  /* convert_GUID_string_literal */


char *scan_GUID_string(void)
/*
Scan the string literal token or unquoted UUID token that contains a GUID
string.  Extract and check the format of the string.  Return a pointer to
a primary IL string containing the GUID characters.  If the string is not
of the required form, a diagnostic is issued and a NULL pointer is returned.
For an unquoted UUID token, const_for_curr_token contains a string constant
that is equivalent to the one that would have been created if it were
quoted.

The string optionally begins and ends with braces and is of the form
  hhhhhhhh-hhhh-hhhh-hhhh-hhhhhhhhhhhh
where "h" is any hex digit and the hyphens are required.

Any alphabetic characters in the string are converted to lower case in the
string that is returned.
*/
{
  char  *result = NULL;

  if (curr_token != tok_string_literal && curr_token != tok_uuid) {
    /* Error. */
    syntax_error(ec_bad_uuid_string);
  } else if (is_error_constant(&const_for_curr_token)) {
    /* We encountered a misformed string literal.  An error should
       have been issued already. */
    check_assertion(is_at_least_one_error());
  } else {
    if (!convert_GUID_string_literal(&const_for_curr_token, &result)) {
      pos_error(ec_bad_uuid_string, &error_position);
    }  /* if */
    /* Bypass the string literal token. */
    (void)get_token();
  }  /* if */
  return result;
}  /* scan_GUID_string */


static a_boolean scan_inheritance_kind(an_inheritance_kind  *inheritance_kind,
                                       a_source_position    *pos)
/*
Unless *inheritance_kind is already set, scan for an inheritance kind keyword
-- i.e.,
  __single_inheritance
  __multiple_inheritance
  __virtual_inheritance
(each of which can also be spelled with a single leading underscore) -- and
return the result in *inheritance_kind.  The source position of the specified
inheritance kind is returned in *pos.  Return TRUE if the scan is successful.
*/
{
  a_boolean    found = FALSE;
  a_const_char *name;

  if (*inheritance_kind == (an_inheritance_kind)ihk_none) {
    check_assertion(curr_token == tok_identifier);
    name = locator_for_curr_id.symbol_header->identifier;
    if (*(name++) == '_') {
      if (*name == '_') name++;
      /* Check the name without its leading single or double underscore. */
      if (strcmp(name, "single_inheritance") == 0) {
        *inheritance_kind = (an_inheritance_kind)ihk_single;
        found = TRUE;
      } else if (strcmp(name, "multiple_inheritance") == 0) {
        *inheritance_kind = (an_inheritance_kind)ihk_multiple;
        found = TRUE;
      } else if (strcmp(name, "virtual_inheritance") == 0) {
        *inheritance_kind = (an_inheritance_kind)ihk_virtual;
        found = TRUE;
      }  /* if */
      if (found) {
        /* Remember the source position, in case a diagnostic is required
           later. */
        *pos = pos_curr_token;
        /* Advance past the inheritance-kind keyword. */
        (void)get_token();
      }  /* if */
    }  /* if */
  }  /* if */
  return found;
}  /* scan_inheritance_kind */


void check_inheritance_kind(a_type_ptr           class_type,
                            an_inheritance_kind  inheritance_kind,
                            a_source_position    *err_pos)
/*
Issue an error if the specified inheritance kind (which was either
explicitly specified for the specified class or assigned to it by default)
is insufficient for the actual characteristics of the class.  *err_pos
indicates the source position at which the error should be put out.
*/
{
  if (inheritance_kind != (an_inheritance_kind)ihk_none &&
      inheritance_kind != (an_inheritance_kind)ihk_incomplete) {
    a_boolean  err = FALSE;
    if (class_type->variant.class_struct_union.any_virtual_base_classes) {
      err = inheritance_kind < (an_inheritance_kind)ihk_virtual;
    } else if (any_multiple_inheritance(class_type)) {
      err = inheritance_kind < (an_inheritance_kind)ihk_multiple;
    }  /* if */
    if (err) {
      pos_stsy_error(ec_invalid_inheritance_kind_for_class, err_pos,
                     inheritance_kind_names[(int)inheritance_kind],
                     (a_symbol_ptr)class_type->source_corresp.assoc_info);
    }  /* if */
  }  /* if */
}  /* check_inheritance_kind */


static void apply_microsoft_w64_specifier(a_type_ptr         *type_ptr,
                                          a_source_position  *err_pos)
/*
Replace the given type with a copy that is marked as having been specified
with the __w64 token.  Except for signedness and qualifiers the given type
must be int, long, or a pointer type; if not, an error is issued at the
given position.
*/
{
  a_type_ptr       plain_type = skip_typerefs(*type_ptr);
  an_integer_kind  ikind;

  if (is_integral_type(plain_type)) {
    ikind = plain_type->variant.integer.int_kind;
  } else {
    ikind = (an_integer_kind)ik_none;
  }  /* if */
  if (is_pointer_type(plain_type) ||
      ikind == (an_integer_kind)ik_int ||
      ikind == (an_integer_kind)ik_unsigned_int ||
      ikind == (an_integer_kind)ik_long ||
      ikind == (an_integer_kind)ik_unsigned_long) {
    a_type_qualifier_set  qualifiers = get_type_qualifiers(*type_ptr);
    a_type_ptr            copy = alloc_type(plain_type->kind);
    copy_type(plain_type, copy);
    copy->has_microsoft_w64_specifier = TRUE;
    *type_ptr = make_qualified_type(copy, qualifiers);
  } else {
    pos_error(ec_invalid_type_for_w64, err_pos);
  }  /* if */
}  /* apply_microsoft_w64_specifier */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if DECL_MODIFIERS_IN_USE || NEAR_AND_FAR_ALLOWED

#if MICROSOFT_EXTENSIONS_ALLOWED

void add_flags_from_dll_attributes(a_decl_modifier_set  *p_flags,
                                   an_attribute_ptr     ap)
/*
Set the DM_DLLIMPORT or DM_DLLEXPORT flag if the corresponding attribute is
present in the list pointed to by ap, except if doing so would cause both
flags to be set (in that case, issue a warning).
*/
{
  for (; ap != NULL; ap = ap->next) {
    if (ap->kind == ak_dllimport) {
      if (*p_flags & DM_DLLEXPORT) {
        pos_warning(ec_bad_combination_of_dll_attributes, &ap->position);
      } else {
        *p_flags |= DM_DLLIMPORT;
      }  /* if */
    } else if (ap->kind == ak_dllexport) {
      if (*p_flags & DM_DLLIMPORT) {
        pos_warning(ec_bad_combination_of_dll_attributes, &ap->position);
      } else {
        *p_flags |= DM_DLLEXPORT;
      }  /* if */
    }  /* if */
  }  /* for */
}  /* add_flags_from_dll_attributes */


void update_dll_info_for_class(a_type_ptr           class_type,
                               a_decl_modifier_set  flags,
                               a_boolean            explicit_inst,
                               a_boolean            adjust_template_base,
                               a_source_position    *err_pos)
/*
Update the given class type to reflect any dllimport/dllexport flags recorded
in flags.  If explicit_inst is TRUE, this routine is called for the explicit
instantiation of class_type.  If adjust_template_base is TRUE, class type is a
base class of a class being defined with a DLL interface: If class_type is a
template class, its DLL interface may need to be adjusted implicitly.
*/
{
  a_decl_modifier_set  new_dll_flags = (flags & DM_DLLFLAGS);

  /* dllimport and dllexport should never be set together. */
  check_assertion(new_dll_flags != DM_DLLFLAGS);
  if (new_dll_flags != 0) {
    a_class_type_supplement_ptr  ctsp = class_type_supp(class_type);
    if (!adjust_template_base &&
        class_type->source_corresp.name_linkage !=
                                          (a_name_linkage_kind)nlk_external &&
        is_member_of_unnamed_namespace(&class_type->source_corresp)) {
      /* dllimport/dllexport on an unnamed namespace member is unlikely to
         be useful. */
      pos_warning(ec_dll_interface_in_unnamed_namespace, err_pos);
    }  /* if */
    if (!class_type_has_body(class_type)) {
      /* Apply the new flag values to the class type only. */
      ctsp->decl_modifiers &= ~DM_DLLFLAGS;
      ctsp->decl_modifiers |= new_dll_flags;
    } else if (explicit_inst || adjust_template_base) {
      /* Either an explicit instantiation or a template class used as a base
         class of a derived class with a DLL interface: Update the flags on
         the members if needed. */
      a_decl_modifier_set old_dll_flags = (ctsp->decl_modifiers & DM_DLLFLAGS);
      if (adjust_template_base &&
          (!class_type->variant.class_struct_union.is_template_class ||
           class_type->variant.class_struct_union.is_specialized)) {
        /* The DLL interface of a base class type is only adjusted for base
           class types that are implicit template specializations.  Otherwise,
           we warn about inconsistent DLL interfaces in base class type. */
        if ((ctsp->decl_modifiers & DM_DLLFLAGS) != new_dll_flags) {
          pos_warning(ec_base_class_has_different_dll_interface, err_pos);
        }  /* if */
      } else if (old_dll_flags != 0) {
        /* The class template instantiation already has a DLL interface: Do
           not change it. */
        if (explicit_inst && (old_dll_flags | new_dll_flags) == DM_DLLFLAGS) {
          /* A previous specialization had the opposite DLL interface, which
             is an error for explicit instantiations. */
          pos_error(ec_bad_combination_of_dll_attributes, err_pos);
        }  /* if */
      } else {
        /* Traverse member functions and static data members and apply the
           DLL interface to those too.  Members of nested classes and in-class
           friend definitions are not affected.  Then recursively update any
           base class if appropriate. */
        a_routine_ptr     rp = ctsp->assoc_scope->routines;
        a_variable_ptr    vp = ctsp->assoc_scope->variables;
        a_base_class_ptr  bcp = ctsp->base_classes;
#if DO_IL_LOWERING
        a_boolean         specialized_dtor_or_ctor = FALSE;
#endif /* DO_IL_LOWERING */
        /* First update the DLL interface of the class itself. */
        ctsp->decl_modifiers |= new_dll_flags;
        if (new_dll_flags & DM_DLLIMPORT) {
          /* Members of a dllimport class are not instantiated. */
          update_instantiation_flags_for_class(
             symbol_for(class_type), (a_pragma_kind)pk_do_not_instantiate,
             err_pos, /*is_pragma=*/FALSE, /*top_level=*/TRUE,
             /*is_dll_directive=*/TRUE);
        }  /* if */
        for (; rp != NULL; rp = rp->next) {
          if (!rp->is_specialized && !rp->is_prototype_instantiation) {
            if ((rp->decl_modifiers & DM_DLLFLAGS) != 0) {
              /* This can only happen in situations like the following:
                   template<class T> struct B {
                      __declspec(dllexport) void f();
                   };
                   struct __declspec(dllexport) D: B<int> ();
                 Microsoft compilers raise an error on such constructs and
                 we follow suit. */
              check_assertion(adjust_template_base);
              pos_sy_error(
                    ec_class_and_inherited_member_instance_have_dll_interface,
                    err_pos, symbol_for(rp));
            } else {
              update_dll_info_for_routine(
                        rp, new_dll_flags, (a_boolean)rp->is_inline,
                        /*is_redecl=*/FALSE, /*is_definition=*/FALSE, err_pos);
            }  /* if */
#if DO_IL_LOWERING
          } else if (rp->special_kind ==
                                    (a_special_function_kind)sfk_destructor ||
                     rp->special_kind ==
                                    (a_special_function_kind)sfk_constructor) {
            specialized_dtor_or_ctor = TRUE;
#endif /* DO_IL_LOWERING */
          }  /* if */
        }  /* for */
        if (new_dll_flags & DM_DLLIMPORT) {
          /* Microsoft compilers do not propagate the dllimport flag to
             static data members of template instances. */
        } else {
          for (; vp != NULL; vp = vp->next) {
            if (!vp->is_specialized && !vp->is_prototype_instantiation) {
              if ((vp->decl_modifiers & DM_DLLFLAGS) != 0) {
                /* This can only happen in situations like the following:
                     template<class T> struct B {
                        __declspec(dllexport) static int s;
                     };
                     struct __declspec(dllexport) D: B<int> ();
                   Microsoft compilers raise an error on such constructs and
                   we follow suit. */
                check_assertion(adjust_template_base);
                pos_sy_error(
                    ec_class_and_inherited_member_instance_have_dll_interface,
                    err_pos, symbol_for(vp));
              } else {
                update_dll_info_for_variable(
                                       vp, new_dll_flags, /*is_redecl=*/FALSE,
                                       /*is_definition=*/FALSE, err_pos);
              }  /* if */
            }  /* if */
          }  /* for */
        }  /* if */
#if DO_IL_LOWERING
        if ((new_dll_flags & DM_DLLIMPORT) && !specialized_dtor_or_ctor) {
          /* Microsoft compilers do not propagate the dllimport flag to
             compiler-generated data (like virtual function tables)
             associated with class template instances, except (apparently)
             when such an instance contains an explicitly specialized
             destructor or constructor. */
        } else {
          if (class_type->typeinfo_var != NULL) {
            update_dll_info_for_variable(class_type->typeinfo_var,
                                         new_dll_flags, /*is_redecl=*/FALSE,
                                         /*is_definition=*/FALSE,
                                         (a_source_position*)NULL);
          }  /* if */
          if (ctsp->virtual_function_table_var != NULL) {
            update_dll_info_for_variable(ctsp->virtual_function_table_var,
                                         new_dll_flags, /*is_redecl=*/FALSE,
                                         /*is_definition=*/FALSE,
                                         (a_source_position*)NULL);
          }  /* if */
#if IA64_ABI
          if (ctsp->virtual_table_table_var != NULL) {
            update_dll_info_for_variable(ctsp->virtual_table_table_var,
                                         new_dll_flags, /*is_redecl=*/FALSE,
                                         /*is_definition=*/FALSE,
                                         (a_source_position*)NULL);
          }  /* if */
#endif /* IA64_ABI */
        }  /* if */
#endif /* DO_IL_LOWERING */
        for (; bcp != NULL; bcp = bcp->next) {
          if (bcp->direct) {
            a_type_ptr       base_type = skip_typerefs(bcp->type);
            if ((new_dll_flags & DM_DLLEXPORT) != 0 &&
                base_type->variant.class_struct_union.is_template_class &&
                !base_type->variant.class_struct_union.is_specialized) {
              /* The dllexport attribute causes every template base type to be
                 instantiated "as if" by an explicit template instantiation
                 directive. */
              a_symbol_ptr     type_sym = symbol_for(base_type);
              check_assertion(type_sym != NULL);
              update_instantiation_flags_for_class(
                             type_sym, (a_pragma_kind)pk_instantiate,
                             err_pos, /*is_pragma=*/FALSE, /*top_level=*/TRUE,
                             /*is_dll_directive=*/TRUE);
            }  /* if */
            update_dll_info_for_class(skip_typerefs(bcp->type), new_dll_flags,
                                      /*explicit_inst=*/FALSE,
                                      /*adjust_template_base=*/TRUE, err_pos);
          }  /* if */
        }  /* for */
      }  /* if */
    } else {
      /* We just ignore dllimport/dllexport on class declarations after a
         definition has been seen. */
    }  /* if */
  }  /* if */
}  /* update_dll_info_for_class */


a_boolean record_uuid_for_class(a_type_ptr         class_type,
                                a_const_char       *uuid_string,
                                a_source_position  *err_pos)
/*
Record the given uuid string in the given class type.  If the class type
already had an associated uuid string, do not record the new value but
issue an error at the given source position.
*/
{
  a_boolean                    result = TRUE;  /* Assume. */
  a_class_type_supplement_ptr  ctsp = class_type_supp(class_type);

  if (ctsp->uuid_string != NULL) {
    /* Issue an error if __declspec(uuid(...)) strings are present and they
       aren't identical. */
    if (strcmp(ctsp->uuid_string, uuid_string) != 0) {
      pos_diagnostic(es_discretionary_error,
                     ec_decl_modifiers_incompatible_with_previous_decl,
                     err_pos);
      result = FALSE;
    }  /* if */
  } else {
    ctsp->uuid_string = uuid_string;
  }  /* if */
  return result;
}  /* record_uuid_for_class */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  
void update_extended_decl_info_for_class(
                            a_type_ptr                   class_type,
                            an_extended_decl_info_block  *extended_decl_info,
                            a_boolean                    explicit_inst,
                            ARG_UNUSED a_source_position *err_pos)
/*
Update the specified class type with information based on a previous scan of
extended declaration modifiers, as specified by *extended_decl_info.  If
explicit_inst is TRUE, this routine is called for the explicit instantiation
of class_type.  err_pos is a pointer to a source position used for diagnostics.
*/
{
  if (!C_mode()) {
    a_class_type_supplement_ptr  ctsp = class_type_supp(class_type);
    /* Record any C++-only declaration modifiers in the class type supplement.
       (See also scan_extended_decl_modifiers which rejects C++-only modifiers
       in C mode.) */
#if DECL_MODIFIERS_IN_USE
    a_decl_modifier_set  flags = extended_decl_info->decl_modifiers.flags;
#endif /* DECL_MODIFIERS_IN_USE */
#if NEAR_AND_FAR_ALLOWED
    copy_qualifiers(extended_decl_info->qualifiers, ctsp->qualifiers);
#endif /* NEAR_AND_FAR_ALLOWED */
#if DECL_MODIFIERS_IN_USE
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* First handle the dllimport and dllexport attributes. */
    update_dll_info_for_class(class_type, flags, explicit_inst,
                              /*adjust_template_base=*/FALSE, err_pos);
    flags &= ~DM_DLLFLAGS;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (flags != DM_NONE) {
      /* The following processing is more complicated than it needs to be so as
         to allow for the easy addition of decl-modifiers. */
      a_boolean            invalid_modifier;
      int                  bit_number;
      a_decl_modifier_set  modifier_value;
      for (bit_number = 0; bit_number < (int)dmt_last; ++bit_number) {
        modifier_value = (1 << bit_number);
        if ((flags & modifier_value) != 0) {
          /* This bit is set. */
          invalid_modifier = FALSE;
          switch (bit_number) {
#if MICROSOFT_EXTENSIONS_ALLOWED
            case dmt_novtable:
              break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            default:
              invalid_modifier = TRUE;
              break;
          }  /* switch */ /*lint !e764 */
          /* If this modifier is invalid, reset the bit in the new
             modifiers. */
          if (invalid_modifier) {
            flags &= (~modifier_value);
          }  /* if */
          if (invalid_modifier) {
            pos_st_diagnostic(es_discretionary_error,
                              ec_decl_modifiers_invalid_for_this_decl,
                              err_pos, decl_modifier_names[bit_number]);
          }  /* if */
        }  /* if */
      }  /* for */
      /* Update the routine entry with any valid modifiers that were found.
         (The dllexport/dllimport flags were set separately.) */
      ctsp->decl_modifiers |= flags;
    }  /* if */
#endif /* DECL_MODIFIERS_IN_USE */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (extended_decl_info->inheritance_kind !=
                                              (an_inheritance_kind)ihk_none) {
      /* Set the specified inheritance kind, unless a different inheritance
         kind has already been locked in -- either explicitly through a prior
         declaration or implicitly, based on the setting of global variable
         default_inheritance_kind, if a pointer-to-member declaration has
         been seen. */
      if (ctsp->inheritance_kind == (an_inheritance_kind)ihk_none) {
        ctsp->inheritance_kind = extended_decl_info->inheritance_kind;
      } else if (ctsp->inheritance_kind !=
                                    extended_decl_info->inheritance_kind) {
        /* Inheritance kind has already been set for this class. */
        pos_stsy_error(ec_inheritance_kind_already_set,
                       &extended_decl_info->inheritance_kind_pos,
                       inheritance_kind_names[(int)ctsp->inheritance_kind],
                       (a_symbol_ptr)class_type->source_corresp.assoc_info);
      }  /* if */
      if (ctsp->inheritance_kind == extended_decl_info->inheritance_kind) {
        ctsp->inheritance_kind_is_explicit = TRUE;
      }  /* if */
    }  /* if */
    if (extended_decl_info->decl_modifiers.uuid_string != NULL) {
      (void)record_uuid_for_class(
                               class_type,
                               extended_decl_info->decl_modifiers.uuid_string,
                               err_pos);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
}  /* update_extended_decl_info_for_class */

#endif /* DECL_MODIFIERS_IN_USE || NEAR_AND_FAR_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED

void scan_extended_decl_modifiers(
                          an_extended_decl_info_block      *extended_decl_info,
                          ARG_UNUSED an_attribute_ptr      *p_attr,
                          ARG_UNUSED an_attribute_location syn_loc,
                          a_boolean                        is_enum_decl)
/*
Scan extended declaration modifiers (e.g., Microsoft extensions) and record
them in the specified extended-decl-info block or, in the case of Microsoft
__declspec attributes, append them to list pointed to by *p_attr.  syn_loc
describes the syntactic context of the modifiers.  is_enum_decl is TRUE if
this routine is called while parsing an enum specifier (syn_loc will be
al_tag_name in that case).
*/
{
  for (;;) {
#if NEAR_AND_FAR_ALLOWED
    if (is_near_or_far()) {
      if (syn_loc == al_tag_name && (is_enum_decl || !C_mode())) {
        /* Memory attribute like "near". */
        scan_near_or_far(&extended_decl_info->qualifiers);
        continue;
      }  /* if */
    }  /* if */
#endif /* NEAR_AND_FAR_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (curr_token == tok_declspec) {
      /* __declspec(...) */
      /* There should be no "unscanned" attributes at this point. */
      check_assertion(!unscanned_attributes_pending());
      *last_attribute_link(p_attr) = scan_attributes(syn_loc);
      continue;
    }  /* if */
    if (syn_loc == al_tag_name && (is_enum_decl || !C_mode()) &&
        curr_token == tok_identifier) {
      /* This is a class or enum declaration, so if the next token is an
         identifier it is probably the class/enum name.  But it might also be
         the "inheritance kind". */
      if (!C_mode() &&
          scan_inheritance_kind(&extended_decl_info->inheritance_kind,
                                &extended_decl_info->inheritance_kind_pos)) {
        if (is_enum_decl) {
          pos_warning(ec_inheritance_kind_ignored_on_enum,
                      &extended_decl_info->inheritance_kind_pos);
        }  /* if */
        continue;
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    break;
  }  /* for */
}  /* scan_extended_decl_modifiers */


void scan_and_discard_extended_decl_modifiers(void)
/*
The Microsoft compiler accepts __declspec declarations in certain contexts
in which they appear to have no effect.  This routine scans the modifiers
and issues a warning indicating that they are being ignored.
*/
{
  /* Issue a warning that the modifiers are being ignored. */
  pos_warning(ec_decl_modifiers_ignored, &pos_curr_token);
  skip_over_attributes();
}  /* scan_and_discard_extended_decl_modifiers */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */

using a_c_tagged_type_map = Ptr_map<a_symbol_header_ptr,
                                    a_type_list_entry_ptr>;
			/* The type of a table that maps the symbol header of
			   a tag to the C structure and union types that have
			   been defined with that tag. */

STATIC_THREAD a_c_tagged_type_map
		*c_tagged_type_map;
			/* The types defined with each tag in this translation
			   unit, used only in C23 mode to find the types that
			   a new definition is compatible with. */


static a_type_ptr compatible_c_tagged_type(a_type_ptr  type)
/*
Return a C structure or union type defined earlier in this translation unit
that C23 makes compatible with type, which must be a complete type declared
with a tag, or NULL if there is no such type.  When there is none, record type
so that a later definition of the same tag can be compared against it.
*/
{
  a_type_ptr             result = NULL;
  a_symbol_header_ptr    header = symbol_for(type)->header;
  uintptr_t              hash = hash_ptr(header);
  a_type_list_entry_ptr  types, tlep;

  types = c_tagged_type_map->get_with_hash(header, hash);

  for (tlep = types; tlep != NULL; tlep = tlep->next) {
    if (c_tagged_types_match(tlep->type, type, ttmk_compatibility)) {
      result = tlep->type;
      break;
    }  /* if */
  }  /* for */
  if (result == NULL) {
    tlep = alloc_type_list_entry();
    tlep->type = type;
    tlep->next = types;
    (void)c_tagged_type_map->map_or_replace_with_hash(header, tlep, hash);
  }  /* if */
  return result;
}  /* compatible_c_tagged_type */


static void rebind_pragmas_on_scope(a_scope_ptr  scope,
                                    char         *from_ptr,
                                    char         *to_ptr)
/*
If scope is non-NULL, any pragma on its list whose entity is from_ptr is
rebound to to_ptr.  A NULL to_ptr clears the pragma's entity, which is
used when a leftover C23 redefinition has no corresponding member.
*/
{
  a_pragma_ptr  pp;

  if (scope != NULL) {
    for (pp = scope->pragmas; pp != NULL; pp = pp->next) {
      if (pp->entity.ptr == from_ptr) {
        pp->entity.ptr = to_ptr;
        if (to_ptr == NULL) {
          pp->entity.kind = iek_none;
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
}  /* rebind_pragmas_on_scope */


static void rebind_pragmas_for_entity(char  *from_ptr,
                                      char  *to_ptr)
/*
Rebind pragmas associated with from_ptr so they refer to to_ptr, on the
file-scope pragma list and, when inside a function, on that function's
pragma list.
*/
{
  rebind_pragmas_on_scope(il_header.primary_scope, from_ptr, to_ptr);
  if (innermost_function_scope != NULL &&
      innermost_function_scope != il_header.primary_scope) {
    rebind_pragmas_on_scope(innermost_function_scope, from_ptr, to_ptr);
  }  /* if */
}  /* rebind_pragmas_for_entity */


static void rebind_c23_redefinition_pragmas(a_type_ptr  leftover,
                                            a_type_ptr  canonical)
/*
leftover is a tagged type formed by a C23 redefinition of a tag that
already denotes canonical.  Pragmas bound to leftover or to its members
stay on a scope pragma list after the tag is made to denote canonical, and
those leftover entities are not always reached by the IL walk.  Rebind the
pragmas to the corresponding entities of canonical.
*/
{
  rebind_pragmas_for_entity((char*)leftover, (char*)canonical);
  if (leftover->source_corresp.has_associated_pragma) {
    leftover->source_corresp.has_associated_pragma = FALSE;
    canonical->source_corresp.has_associated_pragma = TRUE;
  }  /* if */
  if (is_immediate_class_type(leftover)) {
    a_field_ptr  from_fp = fields_of(leftover), to_fp = fields_of(canonical);
    while (from_fp != NULL) {
      if (to_fp != NULL) {
        rebind_pragmas_for_entity((char*)from_fp, (char*)to_fp);
        if (from_fp->source_corresp.has_associated_pragma) {
          from_fp->source_corresp.has_associated_pragma = FALSE;
          to_fp->source_corresp.has_associated_pragma = TRUE;
        }  /* if */
        to_fp = to_fp->next;
      } else {
        rebind_pragmas_for_entity((char*)from_fp, NULL);
        from_fp->source_corresp.has_associated_pragma = FALSE;
      }  /* if */
      from_fp = from_fp->next;
    }  /* while */
  } else if (is_immediate_enum_type(leftover)) {
    a_constant_ptr  from_cp = enum_constants(leftover),
                    to_cp = enum_constants(canonical);
    while (from_cp != NULL) {
      if (to_cp != NULL) {
        rebind_pragmas_for_entity((char*)from_cp, (char*)to_cp);
        if (from_cp->source_corresp.has_associated_pragma) {
          from_cp->source_corresp.has_associated_pragma = FALSE;
          to_cp->source_corresp.has_associated_pragma = TRUE;
        }  /* if */
        to_cp = to_cp->next;
      } else {
        rebind_pragmas_for_entity((char*)from_cp, NULL);
        from_cp->source_corresp.has_associated_pragma = FALSE;
      }  /* if */
      from_cp = from_cp->next;
    }  /* while */
  }  /* if */
}  /* rebind_c23_redefinition_pragmas */


static a_boolean tag_currently_being_defined(a_type_ptr tag_type)
/*
Returns TRUE if the class type pointed to by tag_type is in the process of
being defined.  This is determined by examining any class/struct/union scopes
on the scope stack.  A C23 redefinition of the tag is scanned into a type of
its own, listed under the same symbol header as tag_type, and counts as a
definition of the tag as well.  This is only used in C mode.  This function
does not handle enum types.
*/
{
  a_scope_depth	depth;
  a_boolean	result = FALSE;

  for (depth = depth_scope_stack ;depth != DEPTH_OF_FILE_SCOPE; depth--) {
    if (scope_stack[depth].kind == (a_scope_kind)sck_class_struct_union) {
      a_type_ptr  scope_type = scope_stack[depth].assoc_type;
      if (same_entities(scope_type, tag_type) ||
          (scope_type->is_tag_redefinition &&
           symbol_for(scope_type)->header == symbol_for(tag_type)->header)) {
        result = TRUE;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
  return result;
}  /* tag_currently_being_defined */


static void check_qualified_tag_access(a_boolean	allow_access)
/*
Do access and ambiguity checking on a qualified name being processed
by scan_tag_name.  allow_access is TRUE if access errors should be
ignored.  This is the case when the tag is being defined and for
explicit specializations.
*/
{
  a_boolean defer_access_checks;

  /* If we are allowing access (e.g, for a definition) and access checks
     are not already being deferred, defer them now. */
  defer_access_checks = allow_access &&
                        !scope_stack_top().defer_access_checks;
  if (defer_access_checks) {
    begin_deferral_of_access_checks();
  }  /* if */
  /* The Microsoft compiler does check the access of qualified tag
     references. */
  if (microsoft_bugs) {
    check_for_ambiguity(&locator_for_curr_id);
  } else {
    check_ambiguity_and_verify_access(&locator_for_curr_id);
  }  /* if */
  if (allow_access && any_deferred_access_checks()) {
    /* When defining a class member outside of its class definition
       using a qualified name, any access errors that may have been
       detected when scanning the qualified name should be suppressed.
       This context is not really a declarator, but the concept is the
       same as suppressing access errors when scanning the declarator
       of a member function or static data member.  This is also done
       for explicit specializations that name a class. */
    discard_declarator_access_errors();
  }  /* if */
  if (defer_access_checks) {
    end_deferral_of_access_checks();
  }  /* if */
}  /* check_qualified_tag_access */


static a_boolean is_namespace_for_type_info_definition(a_type_info_kind kind)
/*
Returns TRUE if the current namespace is the one in which the indicated
type_info type may be defined.
*/
{
  a_boolean       result = FALSE;
  a_namespace_ptr nsp;

  /* See what namespace we are in now. */
  nsp = scope_stack[depth_innermost_namespace_scope].assoc_namespace;
#if IA64_ABI
  if (kind != tik_user) {
    /* Make sure we are in the abi namespace. */
    if (nsp == symbol_for_namespace_abi->variant.namespace_info.ptr) {
      result = TRUE;
    }  /* if */
  } else 
#endif /* !IA64_ABI */
  /* Do not add code here. */
  if (kind == tik_user) {
    if (type_info_in_namespace_std && !ignore_std_namespace) {
      /* When type_info is required to be defined in the std namespace,
         make sure we are in that namespace now. */
      if (nsp == symbol_for_namespace_std->variant.namespace_info.ptr) {
        result = TRUE;
      }  /* if */
    } else {
      /* When type_info is not in std, it must be in the global namespace.
         This is also the case when using the g++ compatibility feature
         where the std namespace is an alias for the global namespace. */
      result = depth_scope_stack == DEPTH_OF_FILE_SCOPE;
    }  /* if */
  } else {
    unexpected_condition();
  }  /* if */
  return result;
}  /* is_namespace_for_type_info_definition */


void check_for_class_modifiers(a_token_kind  *next_tok,
                               a_token_kind  body_start,
                               a_boolean     tag_name_first)
/*
C++11 permits
    struct F final: B { ... };
and Microsoft compilers accept constructs like:
    struct S sealed abstract: B { ... };
where "final", "sealed", and "abstract" are context-sensitive keywords: They
are scanned as identifiers, but must be treated like keywords in certain
contexts.  Unfortunately, that means we may need to look ahead several tokens
to determine whether or not a definition follows.  (In particular,
    struct S sealed;
should be treated as a declaration of a variable named "sealed", and not a
declaration of a sealed type "S".)
If tag_name_first is TRUE, this routine assumes that the token stream contains
a generalized identifier followed by one or more plain identifiers.  If
tag_name_first is FALSE, this routine assumes that the token stream contains
one or more plain identifiers.  If these plain identifiers turn out to be
context-sensitive keywords, the tokens are transformed accordingly (i.e., they
become "tok_abstract" or "tok_sealed" keywords).  *next_tok is set to the token
kind that follows the tag name and the class modifiers (if any).  body_start is
the token that represents the beginning of a class body: tok_lbrace in the
normal case, and tok_end_of_source during template prescanning.
*/
{
  a_scanning_token_cache      transformed_token_cache;
  a_tiny_scanning_token_cache orig_token_cache(/*reusable=*/TRUE);
  a_token_kind                tok;
  a_token_kind                orig_next_tok;
  a_boolean                   valid = FALSE;
  a_boolean                   identifier_cached = FALSE;

  check_assertion(curr_token == tok_identifier);
  /* Cache the first identifier. */
  cache_curr_token(orig_token_cache.ptr());
  if (!tag_name_first) {
    identifier_cached = TRUE;
  }  /* if */
  tok = orig_next_tok = get_token();
  /* Cache additional identifiers (if any). */
  while (tok == tok_identifier) {
    identifier_cached = TRUE;
    cache_curr_token(orig_token_cache.ptr());
    tok = get_token();
  }  /* while */
  terminate_token_cache(orig_token_cache.ptr());
  if (identifier_cached &&
      (tok == body_start || tok == tok_colon ||
       tok == tok_removed_template_body)) {
    /* A class definition: The cached identifiers should have been
       context-sensitive keywords.  Make an additional pass over the
       cached tokens, turning the identifiers into keywords when
       possible.  Construct a new cache containing the new tokens. */
    rescan_shared_reusable_cache(shared_obj<a_token_cache>(*orig_token_cache));
    *next_tok = tok;
    if (tag_name_first) {
      cache_curr_token(transformed_token_cache.ptr());
      (void)get_token();
    }  /* if */
    for (; curr_token != tok_end_of_source; (void)get_token()) {
      if (check_context_sensitive_keyword(tok_final, "final") ||
          ((gpp_version_is(>= 40700) || clang_mode) &&
           check_context_sensitive_keyword(tok_final, "__final"))) {
        cache_curr_token(transformed_token_cache.ptr());
        /* If we found any context-sensitive keywords, we should use the
           transformed cache. */
        valid = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (ms_extensions &&
                 (microsoft_version >= 1400 || cli_or_cx_enabled) &&
                 (check_context_sensitive_keyword(tok_abstract, "abstract") ||
                  check_context_sensitive_keyword(tok_sealed, "sealed"))) {
        cache_curr_token(transformed_token_cache.ptr());
        /* If we found any context-sensitive keywords, we should use the
           transformed cache. */
        valid = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      }  /* if */
    }  /* for */
    (void)get_token();
  }  /* if */
  /* If we encountered a valid sequence of class modifiers, return the token
     after the modifier list as the next token, and rescan the transformed
     list of  tokens.  Otherwise, return the original next token and continue
     scanning the original token stream. */
  if (valid) {
    *next_tok = tok;
    rescan_cached_tokens(transformed_token_cache.ptr());
  } else {
    *next_tok = orig_next_tok;
    rescan_copy_of_cache(orig_token_cache.ptr());
  }  /* if */
}  /* check_for_class_modifiers */


static a_boolean tag_definition_next(a_token_kind   next_tok,
                                     a_symbol_kind  tag_kind,
                                     a_boolean      is_ref_within_new_expr,
                                     a_boolean      no_definition_allowed)
/*
We've seen the beginning of a class, struct, union, or enum declaration
(tag_kind determines which).  Return TRUE if next_tok introduces a definition
for that type.  (next_tok is the current token or one that follows.)
is_ref_within_new_expr is TRUE if this occurs in a new-expression; in that
case, a colon is assumed to be part of a "?:" operator and not the beginning
of a base type specifier.  If no_definition_allowed is TRUE, always return
FALSE (e.g., in "[]()->struct X {}" the "{}" is assumed to be the body of the
lambda, not the definition of X).
*/
{
  a_boolean  result;

  if (no_definition_allowed) {
    /* Note that FALSE is also returned if next_tok is a colon.  A colon is an
       error either way, but most likely the programmer did not intend a class
       definition in this context. */
    result = FALSE;
  } else if (next_tok == tok_lbrace) {
    result = TRUE;
  } else if (!C_mode() ? (next_tok == tok_colon && !is_ref_within_new_expr)
                       : (next_tok == tok_colon && tag_kind == sk_enum_tag &&
                          explicit_enum_base_enabled)) {
    /* Possibly the beginning of a C++ base class type specifier or an
       explicit underlying type for C++11/C23/Microsoft enum type. */
    if (tag_kind != sk_enum_tag) {
      result = TRUE;
    } else if (!C_mode() &&
               !scope_is(&scope_stack_top(), sck_class_struct_union)) {
      /* In C++ mode, the only valid scenario when "enum X" is followed by a
         colon outside a class definition is the case where an explicit
         underlying type is specified.  (In C mode, it could also be a
         selection in a _Generic(...) construct.) */
      result = TRUE;
    } else if (explicit_enum_base_enabled) {
      /* An enum type with an explicit base, a bit field declaration of enum
         type, or a C99 _Generic selection for an enum type.  More lookahead
         is required to distinguish the first case from the others. */
      a_scanning_token_cache  cache;
      /* Skip past the colon.  (Since next_tok == tok_colon, we know that
         either the current token or one that follows soon after the current
         token is a colon.) */
      for (;;) {
        cache_curr_token(cache.ptr());
        if (curr_token == tok_colon) break;
        check_assertion(curr_token != tok_end_of_source);
        (void)get_token();
      }  /* for */
      (void)get_token();
      /* Check that what follows the colon is not an expression (which would
         indicate a bit field length or _Generic selection). */
      result = is_decl_not_expr(DFS_ABSTRACT_DECLARATOR_ALLOWED |
                                DFS_SINGLE_TYPE_REQUIRED |
                                DFS_POSSIBLE_ENUM_BASE);
      rescan_cached_tokens(cache.ptr());
    } else {
      result = FALSE;
    }  /* if */
  } else {
    result = FALSE;
  }  /* if */
  return result;
}  /* tag_definition_next */


static void check_consistent_tag_kind(a_symbol_kind     tag_kind,
                                      a_symbol_ptr      *p_tag_sym,
                                      a_symbol_locator  *locator,
                                      a_scope_depth     *effective_decl_level,
                                      a_scope_depth     computed_decl_level,
                                      a_boolean         *err)
/*
A tag is being declared with the given tag kind, but a previous declaration of
that tag name exists (an recorded in *p_tag_sym).  Check that the new kind is
consistent with the previous tag and issue a diagnostic at the position
recorded in *locator otherwise.  If this is an error case, set *err to TRUE,
*p_tag_sym to NULL, and (in C++ and Microsoft modes) *effective_decl_level to
computed_decl_level.
This is a helper routine for scan_tag_name.
*/
{
  a_symbol_ptr  tag_sym = *p_tag_sym;

  if (tag_sym->kind != tag_kind) {
    an_error_severity  severity;
    if (any_cfront_mode() && tag_kind != (a_symbol_kind)sk_enum_tag &&
        tag_sym->kind != (a_symbol_kind)sk_enum_tag) {
      /* Allow mixing of struct/class and union in cfront mode. */
      severity = (an_error_severity)es_warning;
    } else {
      severity = (an_error_severity)es_error;
      *err = TRUE;
    }  /* if */
    pos_stsy_diagnostic(severity, ec_tag_kind_incompatible_with_declaration,
                        &locator->source_position,
                        name_of_symbol_kind(tag_kind), tag_sym);
    if (*err) {
      /* Ignore the existing symbol. */
      if (!C_mode() || microsoft_mode) {
        /* Ensure that the caller will create the new type in an appropriate
           scope. */
        *effective_decl_level = computed_decl_level;
      }  /* if */
      *p_tag_sym = NULL;
    }  /* if */
  }  /* if */
}  /* check_consistent_tag_kind */


static a_symbol_ptr scan_tag_name(
                         a_symbol_kind               tag_kind,
                         a_symbol_locator            *locator,
                         a_boolean                   *is_friend_decl,
                         a_boolean                   is_specialization,
                         a_boolean                   *check_for_vacuous_decl,
                         a_boolean                   is_ref_within_new_expr,
                         a_boolean                   no_definition_allowed,
                         a_boolean                   is_event_interface,
                         a_scope_depth               *effective_decl_level,
                         a_boolean                   *tag_resolution,
                         a_boolean                   *tag_redefinition,
                         a_boolean                   *is_predeclared_type_decl,
                         ARG_UNUSED a_decl_pos_block *decl_pos_block)
/*
Scan a tag identifier for a class, struct, union, enum, or interface
declaration.  If a tag symbol already exists for the identifier, return
a pointer to that symbol; otherwise return NULL.  If there is no
identifier or if there is an error, return NULL; otherwise, set *locator
to represent the identifier.  In Microsoft mode, the symbol returned
may represent a class template when parsing a nonstandard friend template
declaration (a declaration of the form "friend class X;" where X is the
name of a template).

*is_friend_decl is TRUE when the declaration appears to be of the form
"friend class X;"; if it turns out that no semicolon follows the identifier,
however, the flag will be reset to FALSE and a normal lookup will be done.
is_specialization is TRUE for calls done while scanning the tag name of a
template explicit specialization.  *check_for_vacuous_decl is TRUE when the
context permits a declaration like "struct x;".  is_ref_within_new_expr is
TRUE when the declaration appears inside a new expression.
no_definition_allowed is TRUE if no definition is considered in this context
(e.g., if the declaration appears in a C++11 trailing return type).
is_event_interface is TRUE if the __event keyword precedes the __interface
keyword meaning that an "__event __interface" is being declared.
*effective_decl_level will have been initialized to decl_scope_level by the
caller; it may be changed in C++ for a forward reference to a tag within a
function prototype or a class definition -- the tag is entered into the
innermost non-class/non-prototype scope, which is returned as its effective
declaration level.  *tag_resolution is returned TRUE if this is the
definition of a previously declared incomplete class or enum.
*tag_redefinition is returned TRUE if this is a second definition of a tag
that C23 allows to be defined more than once in a scope, in which case the
symbol of the earlier declaration is returned and the caller must verify that
the two definitions declare the same type.  An opaque enum declaration is
not a definition for this purpose.  *is_predeclared_type_decl is returned TRUE
if this is the explicit declaration of a predeclared type like type_info in
C++ or _GUID in Microsoft mode.

This routine may look more complicated than is necessary -- it isn't.
This routine can either be matching up a definition with a previous
declaration or may be entering a definition in a new scope.  The lookups
have to be done very carefully to create new entries only when required
and to find existing entries only when appropriate.  Exercise great
caution when modifying this routine.
*/
{
  a_symbol_ptr               tag_sym = NULL, templ_sym = NULL;
  a_token_kind               next_tok;
  a_boolean	             err = FALSE, tag_err = FALSE;
  a_boolean	             is_tag_definition = FALSE, body_removed = FALSE;
  an_identifier_options_set  options = GID_CHECK_TAG_NAME_FLAGS;
  a_scope_depth              computed_decl_level = NO_SCOPE_DEPTH;
  a_boolean                  allow_typedef = FALSE;

  db_enter(3, "scan_tag_name");
  *tag_resolution = FALSE;
  *tag_redefinition = FALSE;
  /* Coalesce the identifier that follows the class, struct, union, or
     enum keyword. */
  if (is_ref_within_new_expr) options |= GID_IS_NEW_TYPE_NAME;
  if (is_generalized_identifier_start(options)) {
    /* Determine whether this is a definition or something else (a
       declaration or an elaborated type specifier). */
    next_tok = next_token();
    if (class_modifiers_allowed() && next_tok == tok_identifier &&
        tag_kind != (a_symbol_kind)sk_enum_tag && !is_ref_within_new_expr) {
      /* The next token is an identifier: It could be a declarator-id, or it
         might be a context-sensitive keyword "final", "sealed", or
         "abstract". */
      check_for_class_modifiers(
                              &next_tok, tok_lbrace, /*tag_name_first=*/TRUE);
    }  /* if */
    if (next_tok == tok_removed_template_body) {
      /* The body of a nested class definition in a class templates was
         replaced by a placeholder token.  If the declaration is autonomous,
         ignore the placeholder token. */
      a_token_kind  token_after_next;
      (void)next_two_tokens(tok_removed_template_body, &token_after_next);
      if (token_after_next == tok_semicolon) next_tok = tok_semicolon;
      body_removed = TRUE;
    }  /* if */
    is_tag_definition = tag_definition_next(next_tok, tag_kind,
                                            is_ref_within_new_expr,
                                            no_definition_allowed);
    if (gpp_mode &&
        tag_kind != (a_symbol_kind)sk_enum_tag &&
        !locator_for_curr_id.is_error &&
        locator_for_curr_id.is_qualified_name) {
      /* Early GNU C++ compilers allow elaborated class names whose identifier
         is a qualified typedef name.  Later versions allow this only when the
         name was specified as dependent name.  In a real instantiation, we
         can't tell if the qualifier was originally specified using a
         dependent name, but if we only allow this in real instantiations,
         an error will be issued on the dependent typedef in the prototype
         instantiation, producing the desired result. */
      if (gnu_version < 30400 || is_real_instantiation_context()) {
        allow_typedef = TRUE;
      }  /* if */
      if (!locator_for_curr_id.is_class_member && gnu_version < 30400) {
        /* GNU C++ compilers sometimes treat elaborated class names qualified
           with the current namespace scope as unqualified names. */
        a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];
        a_namespace_ptr          nsp =
                                  qualifier_namespace_ptr(locator_for_curr_id);
        if (((ssep->kind == (a_scope_kind)sck_namespace ||
              ssep->kind == (a_scope_kind)sck_namespace_extension) &&
             ssep->il_scope->variant.assoc_namespace == nsp) ||
            (ssep->kind == (a_scope_kind)sck_file && nsp == NULL)) {
          a_boolean  clear_qualifier = TRUE;
          if (is_tag_definition || next_tok == tok_semicolon) {
            /* Issue a warning if the tag introduces a class definition or a
               stand-alone declaration. */
            an_error_code  err_code;
            if (ssep->kind == (a_scope_kind)sck_file) {
              err_code = ec_nonstd_qualifier_in_global_scope_decl;
            } else {
              err_code = ec_nonstd_qualifier_in_namespace_member_decl;
            }  /* if */
            pos_warning(err_code, &pos_curr_token);
          } else {
            /* If qualified lookup finds something, don't treat the name as
               unqualified. */
            a_symbol_ptr  qual_lookup_result;
            if (nsp == NULL) {
              qual_lookup_result = file_scope_id_lookup(
                                       il_header.primary_scope,
                                       &locator_for_curr_id, IDL_MUST_BE_TAG);
            } else {
              qual_lookup_result = namespace_qualified_id_lookup(
                                   &locator_for_curr_id, nsp, IDL_MUST_BE_TAG);
            }  /* if */
            clear_qualifier = (qual_lookup_result == NULL);
            clear_specific_symbol(locator_for_curr_id);
          }  /* if */
          if (clear_qualifier) {
            clear_qualifier_from_locator(&locator_for_curr_id);
          }  /* if */
        }  /*  if */
      }  /*  if */
    }  /* if */
  } else {
    /* Identifier is missing. */
    pos_error(ec_exp_identifier, &error_position);
    tag_err = TRUE;
  }  /* if */
  if (!C_mode() || !func_prototype_tags_enabled) {
    /* The effective scope depth for the current declaration may need to be
       reset.  Compute the depth to which it should be reset now, since it's
       used for processing declarations of predeclared types.  The actual
       resetting, if required, will be done later. */
    /* In C++ mode or Microsoft C, don't enter tags in prototype scopes (nor
       in any other C mode with func_prototype_tags_enabled set to FALSE). */
    a_boolean     done = FALSE;
    a_symbol_ptr  instance_sym;
    computed_decl_level = *effective_decl_level;
    do {
      switch (scope_stack[computed_decl_level].kind) {
        case sck_template_instantiation:
          /* We hit a template instantiation scope.  If the instantiation
             scope is for a real instantiation then effective_decl_level
             will be set to file scope.  If it is a prototype or nonreal
             instantiation then it will be left pointing at the instantiation
             scope.  The problem is that a class declared in a prototype
             instantiation may not be a real type, but we don't know yet.
             We want to avoid contaminating the name space, etc., so it gets
             declared in the instantiation scope.  If the elaborated type
             specifier is a simple identifier, it cannot be dependent and it
             should be declared in the innermost enclosing namespace scope. */
          instance_sym = scope_stack[computed_decl_level].instance_sym;
          if ((!locator_for_curr_id.is_qualified_name &&
               !locator_for_curr_id.is_template_id) ||
              instance_sym == NULL ||
              !is_nonreal_instance_class_symbol(instance_sym)) {
            computed_decl_level = depth_innermost_namespace_scope;
          }  /* if */
          FALLTHROUGH
        case sck_file:
        case sck_namespace:
        case sck_namespace_extension:
        case sck_function:
        case sck_block:
          done = TRUE;
          break;
        default:
          computed_decl_level--;
      }  /* if */
    } while (!done);
  }  /* if */
  if (!C_mode() && !tag_err) {
    /* Check for the presence of a qualified name.  If we have a qualified
       name, do the lookup in a manner that will only find tag names. */
    if (coalesce_and_lookup_qualified_name(options, ilm_tag, &err) ||
        (curr_token == tok_identifier &&
         locator_for_curr_id.is_template_id)) {
      if (err) {
        /* An error occurred while scanning or looking up the qualified
           name. */
        tag_err = TRUE;
      } else {
        /* Do access and ambiguity checking on the name. */
        check_qualified_tag_access(is_tag_definition ||
                                   (is_specialization &&
                                                   next_tok == tok_semicolon));
        tag_sym = locator_for_curr_id.specific_symbol;
        if (tag_sym != NULL) {
          reduce_projection_symbol_to_fundamental_symbol(tag_sym);
          /* If we found the injected class name, use the symbol of the
             actual class. */
          if (is_injected_class_symbol(tag_sym)) {
            tag_sym = (a_symbol_ptr)(type_symbol_type(tag_sym)->
                                                    source_corresp.assoc_info);
          } else if (symbol_is(tag_sym, sk_type)) {
            a_type_ptr  typedef_tp = skip_typerefs(tag_sym->variant.type.ptr);
            a_symbol_ptr	sym_of_type = symbol_for(typedef_tp);
            if (allow_typedef) {
              /* A typedef name was scanned.  Work with the underlying class
                 symbol in what follows.  This supports an older g++ feature.
                 See the setting of allow_typedef earlier in this routine. */
              if (is_immediate_class_type(typedef_tp)) {
                tag_sym = sym_of_type;
              }  /* if */
            } else if (gpp_mode && gnu_version < 30400 &&
                       is_enum_type(typedef_tp) &&
                       is_unnamed_tag_symbol(sym_of_type)) {
              /* Older versions of g++ allow "enum" to be followed by a typedef
                 to an unnamed enum. */
              tag_sym = sym_of_type;
            }  /* if */
          }  /* if */
          if (tag_sym->kind != tag_kind) {
            /* A qualified name is being used with a different tag kind than
               that of its declaration.  Issue an error. */
            if (is_type_template_param_symbol(tag_sym)) {
                /* This is a template parameter during a prototype
                   instantiation. Don't issue an error.  This will be checked
                   during real instantiations. */
            } else if (tag_sym->kind == (a_symbol_kind)sk_class_template) {
              templ_sym = tag_sym;
              tag_sym = NULL;
            } else if (is_template_class_symbol(tag_sym) &&
                       tag_kind != (a_symbol_kind)sk_enum_tag &&
                       !symbol_is(tag_sym, sk_enum_tag)) {
              /* Caller will issue the diagnostic. */
            } else if (symbol_is(tag_sym, sk_type)) {
              /* A typedef name.  Issue an error (unless it already is an
                 error symbol). */
              if (!tag_sym->is_error) {
                pos_st_error(ec_typedef_in_elab_type, 
                             &locator_for_curr_id.source_position,
                             tag_sym->header->identifier);
              }  /* if */
              tag_sym = NULL;
              tag_err = TRUE;
            } else {
              pos_stsy_error(ec_tag_kind_incompatible_with_declaration,
                             &locator_for_curr_id.source_position,
                             name_of_symbol_kind(tag_kind), tag_sym);
              tag_sym = NULL;
              tag_err = TRUE;
            }  /* if */
          }  /* if */
          if (!tag_err && tag_kind != (a_symbol_kind)sk_enum_tag &&
              locator_for_curr_id.is_decltype_qualified) {
            /* Something like "struct decltype(x)::Nested {...}:" is not
               permitted by the standard. */
            pos_diagnostic((strict_ansi_mode || clang_mode) ? es_error
                                                            : es_warning,
                           ec_decltype_qualified_declared_name,
                           &locator_for_curr_id.source_position);
          }  /* if */
        }  /* if */
      }  /* if */
    } else if (curr_token == tok_identifier &&
               (decl_scope_level == depth_innermost_namespace_scope ||
                ((microsoft_mode || sun_mode) && *is_friend_decl)) &&
               tag_kind != (a_symbol_kind)sk_enum_tag) {
      /* Look up what may be a class template symbol.  If the name is
         the start of a qualified name (e.g., A::B) or has a template
         argument list (e.g., A<T>) it will have been coalesced by the
         call to coalesce_and_lookup_qualified_name.  If it just a simple
         identifier (e.g., "A") we need look it up and coalesce it here.
         In Microsoft and Sun modes, a simple friend declaration that resolves
         to a template is treated as a friend template declaration.  A linkage
         lookup should not be done for a Microsoft friend declaration as
         such declarations can refer to class members. */
      an_id_lookup_options_set	lookup_options = IDL_NO_OPTIONS;
      if (decl_scope_level == depth_innermost_namespace_scope) {
        lookup_options = IDL_LINKAGE_LOOKUP;
      }  /* if */
      templ_sym = normal_id_lookup(&locator_for_curr_id, lookup_options);
      if ((ms_version_is(<1800) || sun_mode) && *is_friend_decl &&
          templ_sym != NULL && next_token() == tok_semicolon) {
        /* In Sun and older Microsoft C++ modes, simple friend declarations
           may refer to templates: These are treated as friend template
           declarations.  For example:
              template<class T> struct S;
              class C {
                friend struct S; // Same as template<class T> friend struct S;
              };
           In Microsoft mode with microsoft_version >= 1400, this is only
           supported if the name nominated by the friend declaration is an
           injected template name.  I.e., the example above is not accepted,
           but the following one is:
              template<class T> struct S {
                friend class S; // Same as template<class T> friend struct S;
              };
           MSVC 18.00 no longer behaves that way (and instead treats the
           injected class name as a type, as required by the standard). */
        if (!(microsoft_mode && microsoft_version >= 1400 &&
              is_class_template_symbol(templ_sym))) {
          if (is_injected_template_symbol(templ_sym)) {
            /* Resolve the symbol as the template instead of as the current
               instance. */
            templ_sym = class_template_for_injected_template_symbol(templ_sym);
          }  /* if */
          if (is_class_template_symbol(templ_sym)) {
            *locator = locator_for_curr_id;
            tag_sym = templ_sym;
            goto done;
          }  /* if */
        }  /* if */
      } else if (gpp_mode && gnu_version >= 30300 && gnu_version < 40100 &&
                 scope_is(&scope_stack_top(), sck_template_instantiation) &&
                 scope_stack_top().template_sym == templ_sym &&
                 is_class_template_symbol(templ_sym)) {
        /* g++ versions 3.3 through 4.0 treat the elaborated name of a class
           template as an injected class name while scanning base classes.
           For example:
             template<class> struct B {};
             template<class> struct D: B<D> {};  // B<D> same as B<D<T>>
           (We know we are scanning base class specifiers because the
           sck_template_instantiation scope for the class template
           is on top of the stack.  Once we're past the base class specifiers
           a class scope will be pushed.)
        */
        tag_sym = scope_stack_top().instance_sym;
        templ_sym = NULL;
      }  /* if */
    }  /* if */
    if (templ_sym != NULL) {
      /* Check for an identifier that is a class template name.  A class
         template name at file scope must have an argument list.  A use of a
         class template name in another scope is actually a declaration of a
         new class that has nothing to do with the template. */
      /* There are two situations that need to be handled: this could be the
         first time we are scanning this template reference -- in which case
         we must scan the arguments (using coalesce_template_class_reference).
         Alternately, the arguments may have already been coalesced.  If the
         symbol is a class template then we need to scan the arguments.  If
         the symbol is a template class symbol then the arguments have already
         been scanned and we should simply use the symbol returned by
         normal_id_lookup. */
      if (is_class_template_but_not_cli_generic(templ_sym)) {
        a_boolean  implicit_template_allowed =
                     (microsoft_bugs && microsoft_version < 1400) || sun_mode;
        tag_sym = coalesce_template_class_reference(
                     templ_sym,
                     implicit_template_allowed ? GID_TEMPLATE_ARGS_OPTIONAL
                                               : GID_IS_TAG_NAME,
                     &err);
        if (is_class_template_symbol(tag_sym)) {
          if (implicit_template_allowed) {
            /* In Microsoft bugs mode (with microsoft_version < 1400) and in
               Sun mode, the following is accepted:
                  template<class T> struct S;
                  struct S; // ignored
            */
            if (next_token() == tok_semicolon && !is_ref_within_new_expr) {
              tag_sym = tag_sym->variant.template_info
                             ->variant.class_template.prototype_instantiation;
              pos_warning(ec_not_a_class_or_struct_name, &error_position);
              /* Don't attempt to attach pragmas to the prototype
                 instantiation. */
              discard_curr_construct_pragmas();
            } else {
              pos_error(ec_not_a_class_or_struct_name, &error_position);
              err = TRUE;
            }  /* if */
          } else {
            check_assertion(err);
          }  /* if */
        }  /* if */
        /* If an error occurred while scanning the template arguments, set
           tag_sym to NULL.  The caller is not prepared for it to point to
           an error symbol. */
        if (err) {
          tag_sym = NULL;
          tag_err = TRUE;
        }  /* if */
      } else if (is_template_class_symbol(templ_sym)) {
        /* Use the symbol pointer from the locator (returned by
           normal_id_lookup earlier).  The template class reference has
           already been coalesced. */
        tag_sym = templ_sym;
      } else {
        /* Don't prejudice subsequent lookups. */
        clear_specific_symbol(locator_for_curr_id);
      }  /* if */
    }  /* if */
    if (tag_sym == NULL && !tag_err &&
        !locator_for_curr_id.is_qualified_name &&
        computed_decl_level == depth_innermost_namespace_scope &&
        tag_kind != (a_symbol_kind)sk_enum_tag) {
      /* See if this is an explicit declaration of one of the type_info
         types, which was already "predeclared".  If it is, reuse the
         original symbol. */
      a_type_ptr    predeclared_type = NULL;
      a_symbol_ptr  type_info_sym = NULL;
      int           i;

      /* Look for a type_info type with the same name. */
      for (i = 0; i < (int)tik_last; ++i) {
        if (types_of_type_info[i] == NULL) continue;
        type_info_sym = symbol_for(types_of_type_info[i]);
        if (type_info_sym->header == locator_for_curr_id.symbol_header) {
          break;
        }  /* if */
      }  /* for */
      /* Note that we need a match not only on the name but also on the
         namespace.  This depends on whether the implicitly declared type_info
         is expected to be in namespace "std" or in the global namespace. */
      if (i != (int)tik_last) {
        if (is_namespace_for_type_info_definition((a_type_info_kind)i)) {
          /* The identifier does indeed  name a type info type.  Check
             for  the pragma  that specifically  identifies it  as the
             type_info  that  is returned  by  typeid (typically,  the
             type_info defined in <typeinfo>). */
          a_pending_pragma_list ppl = extract_specific_pragmas(
                                          pk_define_type_info,
                                          type_info_sym, (a_statement_ptr)NULL,
                                          /*curr_scope_only=*/TRUE);
          if (!ppl.is_empty()) {
            /* This is the one. */
            tag_sym = type_info_sym;
            /* RTTI is outside the "Embedded C++" subset. */
            feature_is_not_part_of_embedded_cplusplus_subset(
                                                &pos_curr_token,
                                                ec_rtti_in_embedded_cplusplus);
          } else {
            if (!pragma_define_type_info_is_required) {
              /* The pragma is not required (e.g., when the C++ generating
                 back end is in use). */
              tag_sym = type_info_sym;
            } else {
#if ABI_CHANGES_FOR_RTTI
              if (type_info_sym->decl_scope == NO_SCOPE_DEPTH) {
                Small_string<50> buffer;

                /* Not yet explicitly redeclared. */
                /* Run-time support for RTTI declares type_info, so consider
                   the name to be reserved. */
#if IA64_ABI
                if (i != (int)tik_user) {
                  buffer.reset_to("__cxxabiv1::",
                                  type_info_sym->header->identifier);
                } else
#endif /* IA64_ABI */
                /* Do not insert code here. */
                {
                  buffer.reset_to(type_info_in_namespace_std ? "std::" : "",
                                  "type_info");
                }  /* if */
                pos_error(ec_conflicts_with_predeclared_type_info,
                          &locator_for_curr_id.source_position,
                          buffer.as_temp_characters());
              }  /* if */
              tag_sym = type_info_sym;
#endif /* ABI_CHANGES_FOR_RTTI */
            }  /* if */
          }  /* if */
        }  /* if */
        if (tag_sym == type_info_sym) {
          if (tag_sym->decl_scope == NO_SCOPE_NUMBER) {
            predeclared_type = types_of_type_info[i];
          } else if (microsoft_mode && tag_sym->decl_position.seq == 0) {
            /* In Microsoft mode the type_info symbol is usually entered
               at the start of compilation with a NULL source position.
               Update that position now that we have an explicit source
               construct. */
            tag_sym->decl_position = locator_for_curr_id.source_position;
            tag_sym->variant.class_struct_union.type
                   ->source_corresp.decl_position = tag_sym->decl_position;
          }  /* if */
        }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (microsoft_mode && !C_mode() &&
                 computed_decl_level == DEPTH_OF_FILE_SCOPE &&
                 /* FIXME: This should not be NULL while microsoft_mode is
                    enabled.  However, we don't currently have a nice place to
                    factor initialization of this type to make lazy
                    initialization available for IFC modules. */
                 type_of_guid != NULL) {
        /* Similarly, check for predeclared "struct _GUID". */
        a_symbol_ptr  guid_sym;
        check_assertion(type_of_guid != NULL);
        guid_sym = (a_symbol_ptr)type_of_guid->source_corresp.assoc_info;
        if (locator_for_curr_id.symbol_header == guid_sym->header) {
          tag_sym = guid_sym;
          if (tag_sym->decl_scope == NO_SCOPE_NUMBER) {
            predeclared_type = type_of_guid;
          } else if (microsoft_mode && tag_sym->decl_position.seq == 0) {
            /* In Microsoft mode the _GUID symbol is usually entered
               at the start of compilation with a NULL source position.
               Update that position now that we have an explicit source
               construct. */
            tag_sym->decl_position = locator_for_curr_id.source_position;
            tag_sym->variant.class_struct_union.type
                   ->source_corresp.decl_position = tag_sym->decl_position;
          }  /* if */
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      }  /* if */
      if (predeclared_type != NULL) {
        /* If the type_info or _GUID symbol has no scope number, it hasn't
           been added to the symbol table yet.  Use the current source
           position. */
        enter_predeclared_class(predeclared_type, computed_decl_level,
                                &locator_for_curr_id.source_position);
        *is_predeclared_type_decl = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (!tag_err) {
    if (locator_for_curr_id.is_qualified_name) {
      /* A "vacuous declaration" may not involve a qualified name: "struct x;"
         is okay, but "struct A::x;" is not. */
      *check_for_vacuous_decl = FALSE;
    }  /* if */
    if (locator_for_curr_id.is_operator_name ||
        locator_for_curr_id.is_conversion_name ||
        locator_for_curr_id.is_udl_operator_name) {
      /* Issue an error for something like "class operator+" or
         "class operator int". */
      pos_error(ec_operator_name_not_allowed,
                &locator_for_curr_id.source_position);
      tag_err = TRUE;
      tag_sym = NULL;
    }  /* if */
  }  /* if */
  if (tag_sym != NULL) {
    /* Tag symbol is a qualified name or a template class reference. */
    /* Return a copy of the locator to the caller. */
    *locator = locator_for_curr_id;
  } else if (tag_err) {
    /* An error occurred while handling a qualified name or a template
       reference earlier. */
  } else if (is_error_locator(locator_for_curr_id)) {
    /* There was some other error on the lookup -- e.g., maybe this was
       an ambiguous namespace projection. */
    tag_err = TRUE;
  } else {
    a_boolean  is_vacuous_declaration = FALSE;
    /* Save the symbol locator for this identifier. */
    *locator = locator_for_curr_id;
    if (next_tok == tok_semicolon || body_removed) {
      if ((*check_for_vacuous_decl || body_removed) &&
          C_dialect != C_dialect_pcc && !is_ref_within_new_expr) {
        /* This may be a "vacuous declaration" (e.g. "struct S;" or "enum E;").
           The effect of a vacuous declaration (unless we are in pcc mode) is
           to establish the name in the current scope, even if the tag name
           exists in a containing scope or is inherited from a base class.
           The body of a nested class of a class template that is part of
           another declaration is replaced with a "removed template body"
           token.  Consider that a vacuous declaration too. */
        is_vacuous_declaration = TRUE;
      }  /* if */
    } else if (*is_friend_decl) {
      /* In a friend class declaration a semicolon will always follow the
         identifier.  It doesn't here -- maybe it's something like:
           friend class X *f();
         i.e., the "friend" specifier doesn't apply to the class.  Also allow
         for the case where the friend class declaration is a definition
         (which is an error reported elsewhere). */
      if (next_tok != tok_colon && next_tok != tok_lbrace) {
        *is_friend_decl = FALSE;
      }  /* if */
    }  /* if */
    if (is_tag_definition || is_vacuous_declaration) {
      /* Look for a tag symbol in the current scope.  If the tag kind does
         not match the tag being processed, issue an error. */
      /* Note: we only call curr_scope_id_lookup for declarations, not for
         references within the declaration of something else, because of
         cases like this:
           class A;
           namespace { class A; }
           class A *p;                // Error -- ambiguous reference
           class A { };               // Okay -- defines ::A
         curr_scope_id_lookup will return ::A only, whereas normal_id_lookup
         will return a projection symbol that informs of the ambiguity. */
      /* In some GNU modes, the class' tag name may be replaced by a
         typedef name. */
      tag_sym = curr_scope_id_lookup(locator, allow_typedef ? IDL_MUST_BE_CLASS
                                                            : IDL_MUST_BE_TAG);
      if (allow_typedef && tag_sym != NULL && symbol_is(tag_sym, sk_type)) {
        /* A typedef name was scanned.  Work with the underlying class symbol
           in what follows. */
        a_type_ptr  typedef_tp = skip_typerefs(tag_sym->variant.type.ptr);
        tag_sym = symbol_for(typedef_tp);
      }  /* if */
      if (tag_sym != NULL && is_injected_class_symbol(tag_sym)) {
        /* Ignore an injected class symbol, which would be found for this sort
           of case:
             struct A { struct A { ... }; };
        */
        if (any_cfront_mode()) {
          /* In Cfront mode, such a declaration is seen as a declaration of
             the enclosing class. */
          tag_sym = symbol_for(type_symbol_type(tag_sym));
        } else {
          tag_sym = NULL;
        }  /* if */
      }  /* if */
    }  /* if */
    if (is_tag_definition) {
      if (tag_sym != NULL) {
        /* The tag has already appeared in the current scope. */
        if (!tag_sym->defined &&
            (!C_mode() ||
             !tag_currently_being_defined(type_symbol_type(tag_sym)))) {
          /* Resolution of a previous incomplete declaration.  In C mode, make
             sure that an incomplete type is not in the process of being
             defined. */
          *tag_resolution = TRUE;
        } else if (c23_mode && tag_sym->defined && !tag_sym->is_error &&
                   !tag_currently_being_defined(type_symbol_type(tag_sym)) &&
                   (tag_kind != sk_enum_tag || next_tok == tok_lbrace)) {
          /* C23 allows a tag to be defined more than once in a scope, with
             every definition declaring the same type.  Keep the symbol of
             the earlier definition and let the caller check that the two
             definitions agree.  A declaration nested in the definition of
             the type it redeclares is not allowed.  Opaque enum
             declarations are not definitions for this purpose. */
          *tag_redefinition = TRUE;
        } else if (tag_kind != (a_symbol_kind)sk_enum_tag
                   if_microsoft_extensions(
                           && !is_partial_class(type_symbol_type(tag_sym)))) {
          /* Redefinition of a class tag that has already been defined.  Set
             tag_sym to NULL and let enter_symbol issue an error.  Don't do
             this for enum types since we may be dealing with an opaque enum
             declaration.  Also, if we're dealing with C++/CX partial class
             definitions, multiple "definitions" are not actually errors. */
          tag_sym = NULL;
        }  /* if */
      }  /* if */
    } else if (tag_sym == NULL) {
      /* This is the first appearance of the tag in the current scope.  This
         is not its definition, so it is either a reference to an existing
         tag or a declaration of a new (incomplete) tag. */
      /* Check for a cfront bug (violation of ARM 7.1.3, which says a typedef
         name may not appear in an elaborated type specifier) which allows
         a typedef name as long as it refers to a class/struct/union type. */
      if (any_cfront_mode() && tag_kind != (a_symbol_kind)sk_enum_tag) {
        /* Look up the name again in the current scope, but this time don't
           restrict the search to tag names. */
        a_symbol_ptr  sym;
        check_assertion(locator->specific_symbol == NULL);
        sym = curr_scope_id_lookup(locator, IDL_NO_OPTIONS);
        if (sym != NULL) {
          /* Found a symbol of the same name that was declared in the current
             scope. */
          if (symbol_is(sym, sk_type)) {
            /* Name is already declared in the current scope as a typedef. */
            a_type_ptr  tp = skip_typerefs(sym->variant.type.ptr);
            if (is_immediate_class_type(tp) &&
                ((tag_kind == (a_symbol_kind)sk_union_tag) ==
                 (tp->kind == (a_type_kind)tk_union))) {
              /* This is the special case.  Return an sk_type symbol instead
                 of the normally expected sk_class_or_struct_tag. */
              tag_sym = sym;
              goto done;
            }  /* if */
          }  /* if */
          /* Reset the specific_symbol pointer to avoid prejudicing any
             subsequent lookup. */
          clear_specific_symbol(*locator);
        }  /* if */
      }  /* if */
      if (is_vacuous_declaration && !is_event_interface) {
        /* This is a vacuous declaration.  Leave tag_sym set to NULL to force
           creation of a new symbol in the current scope. */
      } else {
        /* This may be a reference to an existing tag, either from the
           current scope or from a containing scope or a base class. */
        tag_sym = curr_tag_symbol(locator, tag_kind, allow_typedef,
                                  *is_friend_decl);
        if (tag_sym == NULL) {
          /* We will need to enter an incomplete tag that may be resolved
             later.  Just leave tag_sym NULL.  In C it will be entered at
             the scope level indicated by decl_scope_level.  In C++ we need
             to pop out to the innermost non-class/non-prototype scope.
             (For example, to introduce class name B in a parameter
             declaration of a member function within the definition of class
             A does not introduce the name of nested class A::B; rather, B
             is entered in the same scope as A.) */
          /* Note: in some C modes (like Microsoft C mode), tags are not
             entered in function prototype scopes. */
          if (!C_mode() || !func_prototype_tags_enabled) {
            *effective_decl_level = computed_decl_level;
          }  /* if */
        } else if (is_injected_class_symbol(tag_sym)) {
          /* A symbol representing an injected class name.  Use the tag symbol
             associated with the class in its place. */
          tag_sym = symbol_for(tag_sym->variant.type.ptr);
        } else if (symbol_is(tag_sym, sk_type)) {
          /* A type symbol.  This can result from the use of a template
             parameter or an elaborated type specifier (in certain modes). */
          if (is_template_param_type_symbol(tag_sym)) {
            /* The tag is a template parameter type.  A diagnostic will have
               been issued in curr_tag_symbol.  This usage is still supported
               in the front end although the feature is no longer permitted by
               the standard.  An error will have been issued in strict mode. */
            goto done;
          } else {
            /* We should only get here in C++ mode. */
            /* A typedef name.  Use the underlying type as the tag symbol. */
            a_type_ptr	underlying_type;
            check_assertion(!C_mode());
            underlying_type = type_symbol_type(tag_sym);
            underlying_type = skip_typerefs(underlying_type);
            tag_sym = symbol_for(underlying_type);
          }  /* if */
        } else if (is_type_symbol(tag_sym)) {
          a_type_ptr  tp = type_symbol_type(tag_sym);
          if (mscpp_version_is(any_version) &&
              is_immediate_enum_type(tp) &&
              tp->variant.integer.originally_unnamed &&
              scope_is(&scope_stack_top(), sck_func_prototype)) {
            /* In a case like:
                 typedef enum { e } E;
                 void f(enum E x) {
                   decltype(e) *p = &x;
                 }
               MSVC appears to enter "enum E" in the prototype scope of f,
               and thus issues a type compatibility error on the initializer
               for p. */
            tag_sym = NULL;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (!tag_err && tag_sym != NULL) {
      check_consistent_tag_kind(tag_kind, &tag_sym, locator,
                                effective_decl_level, computed_decl_level,
                                &tag_err);
    }  /* if */
  }  /* if */
done:
  if (tag_err) {
    /* If an error occurred while scanning the tag, make the locator that
       is returned to the caller an error locator. */
    set_to_error_locator(*locator);
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    /* The end position of the current token is the end of the identifier
       and (as far as we know so far) the end of the specifier to which the
       class or enum declaration may belong. */
    decl_pos_block->identifier_range.end = end_pos_curr_token;
    decl_pos_block->specifiers_range.end = end_pos_curr_token;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Now that we have completed the lookup on the tag identifier we can
     advance past it. */
  (void)get_token();
  db_exit();
  return tag_sym;
}  /* scan_tag_name */


void set_name_linkage_for_type(a_type_ptr  tp)
/*
Set the name_linkage field of the class or enum type pointed to by tp.
*/
{
  a_source_correspondence  *scp = &tp->source_corresp;

  check_assertion(is_immediate_class_type(tp) || is_immediate_enum_type(tp));
  if (!has_name(tp) || scp->is_local_to_function) {
    /* Name linkage requires a nonlocal name.  (Note that this can change if
       the type acquires a name through a typedef declaration.) */
    scp->name_linkage = (a_name_linkage_kind)nlk_none;
  } else if (scp->is_class_member && !gpp_mode) {
    /* A nested class or enum has the same linkage as the class of which it
       is a member.  GNU C++ ignores the name linkage of the enclosing class;
       e.g., a named class nested in an unnamed class has C++ name linkage. */
    scp->name_linkage = scp_parent_class(scp)->source_corresp.name_linkage;
  } else if (any_cfront_mode() &&
             depth_innermost_namespace_scope == DEPTH_OF_FILE_SCOPE) {
    /* In cfront mode -- unless this is a class or enum declared within a
       namespace -- give it internal linkage by default.  It may be promoted
       later, based on how it's used, etc. */
    scp->name_linkage = (a_name_linkage_kind)nlk_internal;
  } else {
    /* Ordinary default for classes and enums is C++ external linkage. */
    scp->name_linkage = (a_name_linkage_kind)nlk_cplusplus_external;
  }  /* if */
}  /* set_name_linkage_for_type */


static a_boolean namespace_scope_should_be_pushed(
				a_symbol_ptr		tag_sym,
				a_source_position	*pos,
				a_boolean		*err,
				a_boolean		inline_namespace)
/*
The class or enum indicated by tag_sym is being defined, having originally
been declared a namespace member.  Determine whether it's legal in this
context (if not, issue a diagnostic and return *err set to TRUE), and if it
is legal, determine whether a scope stack entry needs to be pushed (in which
case return TRUE).  inline_namespace is TRUE if it is okay for tag_sym
to be defined in an inline namespace of the namespace containing tag_sym.
*/
{
  a_boolean    should_be_pushed = FALSE;
  a_scope_ptr  scope = scope_stack[decl_scope_level].il_scope;

  if (inline_namespace &&
      is_symbol_from_inline_namespace(tag_sym)) {
    /* Push a namespace extension scope. */
    should_be_pushed = TRUE;
  } else if (!namespace_is_enclosed_by_curr_scope(tag_sym)) {
    /* This declaration appears within a namespace scope in which the name
       cannot be defined -- it is a member (directly or indirectly) of a
       namespace that is not enclosed by the current namespace scope (see
       WP 7.3.1.4). */
    pos_sy_error(ec_bad_scope_for_definition, pos, tag_sym);
    *err = TRUE;
  } else if (scope->kind != (a_scope_kind)sck_namespace ||
             sym_parent_namespace_or_null(tag_sym) !=
                                 scope->variant.assoc_namespace) {
    /* Push a namespace extension scope. */
    should_be_pushed = TRUE;
  }  /* if */
  return should_be_pushed;
}  /* namespace_scope_should_be_pushed */


static void check_nested_class_redeclaration(
                                 a_decl_parse_state      *dps,
                                 a_symbol_ptr            tag_sym,
                                 a_source_position       *tag_position,
                                 a_boolean               is_class_definition,
                                 a_boolean               is_friend_decl,
                                 a_boolean               is_qualified_name,
                                 a_boolean               *declares_something)
/*
Helper for class_specifier (below) that verifies whether the use of an 
elaborated type-specifier is really a redeclaration, and if so performs
various checks related to access and the use of a qualified name.
This function is called if a class-specifier is seen in the scope of another
class type, and the tag of that specifier was already declared in that scope.
*dps describes the context of the elaborated name.  tag_sym is a pointer to
the symbol associated with the elaborated name.  tag_position is the position
of the elaborated name (tag) that was just scanned.  is_class_definition and
is_friend_decl are set when the tag is used to define the nested type or
introduce a friend declaration.  If the tag-name was qualified,
is_qualified_name is set as well.  *declares_something is set to false if the
elaborated type specifier does not introduce a definition and is not followed
by a semicolon; otherwise it is left unchanged.
*/
{
  a_type_ptr  type = tag_sym->variant.class_struct_union.type;

  if (is_class_definition && is_qualified_name) {
    /* Issue a warning on a case like this:
         class A {
           class N;
           class A::N { ... };    // Qualified name is not allowed
         };
       -- a warning rather than an error for consistency with other
       member declarations (see simplify_curr_class_qualified_name). */
    pos_diagnostic(strict_ansi_mode ?
                       strict_ansi_error_severity : es_warning,
                   ec_qualifier_in_member_declaration, tag_position);
  }  /* if */
  if (!is_class_definition &&
      (curr_token != tok_semicolon || dps->is_type_name)) {
    /* For example:
          struct S { struct N {}; private: struct N* f(); };
       is fine -- there is no redeclaration of struct N here.  Similarly:
          struct T { struct N {}; using R = struct N; }; */
    *declares_something = FALSE;
  } else if (!is_friend_decl) {
    /* Be sure the access is consistent on the redeclaration. */
    a_scope_stack_entry_ptr ssep = &scope_stack[depth_scope_stack];
    if (ssep->current_access != type->source_corresp.access) {
      /* The access specified for the previous declaration does not
         correspond to the access for current declaration. */
      an_error_code      error_code;
      an_error_severity  severity;

      /* If this is a definition, use the current access instead of
         that specified on the original declaration. */
      if (is_class_definition) {
        type->source_corresp.access = ssep->current_access;
        error_code = ec_redecl_changes_access;
      } else {
        error_code = ec_cannot_change_access;
      }  /* if */
      severity = strict_ansi_mode ?
                   strict_ansi_discretionary_severity : es_warning;
      pos_sy_diagnostic(severity, error_code, tag_position, tag_sym);
    }  /* if */
    if ((tag_sym->defined || !is_class_definition) &&
        /* Exclude ordinary (nonprototype) template instantiations: */
        !(type->variant.class_struct_union.is_template_class &&
          !type->variant.class_struct_union.is_prototype_instantiation &&
          !type->variant.class_struct_union.is_specialized)) {
      /* The only standard conforming nested class redeclaration is a
         definition following a nondefining declaration. */
      pos_diagnostic(strict_ansi_mode ?
                       strict_ansi_error_severity : es_warning,
                     ec_invalid_nested_class_redecl, tag_position);

    }  /* if */
  }  /* if */
}  /* check_nested_class_redeclaration */


static a_boolean same_entity_as_a_type_info_type(a_type_ptr type)
/*
Return true if "type" is the same entity as a type_info type.
*/
{
  a_boolean result = FALSE;
  int       i;

  for (i = 0; i != (int)tik_last; ++i) {
    if (same_entities(type, types_of_type_info[i])) {
      result = TRUE;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* same_entity_as_a_type_info_type */


static void duplicate_friend_sym_in_namespace(a_scope_depth  *scope_depth,
                                              a_symbol_ptr   *sym)
/*
The given symbol was created for a friend class declaration during a
prototype instantiation.  It is therefore associated with a template
instantiation scope.  Duplicate this symbol in the surrounding namespace
scope, but keep it hidden in that scope until a visible declaration of
the class is made (unless we are doing friend injection).  Update *sym to
the duplicated symbol value and *scope_depth to the namespace scope in
which it was added.
*/
{
  *scope_depth = depth_innermost_namespace_scope;
  *sym = enter_copy_of_symbol(*sym, *scope_depth, /*suppress_error=*/FALSE);
  if (!friend_class_injection_enabled) (*sym)->is_invisible = TRUE;
}  /* duplicate_friend_sym_in_namespace */

#if MICROSOFT_EXTENSIONS_ALLOWED || SUN_EXTENSIONS_ALLOWED

static void decl_nonstandard_friend_template(a_symbol_ptr  sym)
/*
We've seen a friend declaration of the form "friend class X;" where X is a
class template represented by sym.  In Microsoft and Sun modes, this is
treated as a friend template declaration.  Update the IL as needed and issue
an appropriate diagnostic.
*/
{
  a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];
  a_template_symbol_supplement_ptr
                           tssp = sym->variant.template_info;
  a_template_ptr           tp;

  check_assertion(microsoft_mode || sun_mode);
  /* Pragmas cannot bind to this "implicit template". */
  cannot_bind_to_curr_construct();
  if (ssep->kind != (a_scope_kind)sck_class_struct_union) {
    pos_error(ec_bad_friend_decl, &locator_for_curr_id.source_position);
    goto done;
  }  /* if */
  add_befriending_class_to_class_template(tssp, ssep->assoc_type);
  pos_warning(ec_bad_friend_decl, &locator_for_curr_id.source_position);
  /* Create an IL entry for the template.  This entry is special in the
     sense that its template_decl field is NULL (since there were no
     template parameters in the source). */
  tp = alloc_template();
  tp->kind = (a_template_kind)templk_class;
  tp->source_corresp.assoc_info = (char*)sym;
  set_source_corresp_name(&tp->source_corresp, sym->header);
  tp->source_corresp.decl_position = locator_for_curr_id.source_position;
  tp->source_corresp.name_linkage =
                                  (a_name_linkage_kind)nlk_cplusplus_external;
  tp->source_corresp.access = ssep->current_access;
  tp->canonical_template = tssp->il_template_entry->canonical_template;
  tp->prototype_instantiation.type = tp->canonical_template
                                       ->prototype_instantiation.type;
  add_to_templates_list(tp, depth_innermost_namespace_scope);
#if RECORD_TEMPLATE_STRINGS
  /* This entry has no text representation because its tokens were not
     cached. */
#endif /* RECORD_TEMPLATE_STRINGS */
#if MAINTAIN_CLASS_MEMBER_LIST
  /* The friend declaration is recorded as a template declaration in the body
     of the class that contains it. */
  record_class_member_declaration((char*)tp, iek_template);
#endif /* MAINTAIN_CLASS_MEMBER_LIST */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (!source_sequence_entries_disallowed) {
    a_src_seq_secondary_decl_ptr  sssdp;
    add_to_source_sequence_list((char *)tp, (an_il_entry_kind)iek_template);
    sssdp = secondary_src_seq_for_template(tp);
    sssdp->friend_decl = TRUE;
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
done:;
}  /* decl_nonstandard_friend_template */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED || SUN_EXTENSIONS_ALLOWED */

static void check_friend_class_declaration(a_symbol_locator     *locator,
                                           a_symbol_ptr         *tag_sym,
                                           ARG_UNUSED a_boolean *is_template)
/*
Check that a friend class declaration is well-formed.  locator describes the
source construct to name the class and tag_sym describes the associated class.
For error cases, *tag_sym is set to NULL and locator is turned into an error
locator.  Microsoft compilers allow "friend class X;" where X is a template:
That case is handled entirely by this routine, after which *is_template is set
to TRUE.
*/
{
  if (!(gpp_mode || microsoft_mode) && locator->is_qualified_name &&
      locator->specific_symbol->kind ==
                                     (a_symbol_kind)sk_namespace_projection) {
    /* Except in GNU and Microsoft C++ modes, a qualified friend declaration
       that finds a using-declaration is an error. */
    a_namespace_ptr  nsp = qualifier_namespace_ptr(*locator);
    if (nsp == NULL) {
      /* Must be something like this:
           namespace N { class X; }
           using N::X;
           class Y {
             friend class ::X;      // Error
           };
      */
      check_assertion(locator->is_file_scope_qualified_name);
      pos_st_error(ec_name_not_tag_in_file_scope,
                   &locator->source_position,
                   locator->symbol_header->identifier);
    } else {
      /* Must be something like this:
           namespace N { class X; }
           namespace M { using N::X; }
           class Y {
             friend class M::X;     // Error
           };
      */
      pos_stsy_error(ec_not_an_actual_member, &locator->source_position,
                     locator->symbol_header->identifier,
                     (a_symbol_ptr)nsp->source_corresp.assoc_info);
    }  /* if */
    set_to_named_error_locator(*locator);
    *tag_sym = NULL;
  } else if ((gpp_mode || microsoft_mode) && (*tag_sym)->is_nonreal_member) {
    a_type_ptr  parent_type = (*tag_sym)->parent.class_type;
    /* Microsoft and g++ allows a friend declaration that refers to
       an undeclared member of a prototype instantiation.  Check for
       this case and issue a diagnostic if needed. */
    check_for_invalid_friend_declaration(parent_type, *tag_sym, locator);
#if MICROSOFT_EXTENSIONS_ALLOWED || SUN_EXTENSIONS_ALLOWED
  } else if (is_class_template_symbol(*tag_sym)) {
    /* Microsoft and Sun compilers accept "friend class X;" where X is a class
       template.  It is treated as a friend template declaration. */
    decl_nonstandard_friend_template(*tag_sym);
    *is_template = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || SUN_EXTENSIONS_ALLOWED */
  }  /* if */
}  /* check_friend_class_declaration */


static void check_name_used_for_qualified_class_definition(
                                                       a_symbol_locator  *loc,
                                                       a_symbol_ptr      sym)
/*
sym represents a class being defined in strict mode using a qualified name
represented by loc.  If sym represents a nested class and the class type used
to qualify the nested class name does not directly contain the nested class
(i.e., the nested class is inherited instead), issue an error.  For example:
  struct B { struct N; };
  struct D: B {}:
  struct D::N {};  // Error in strict mode.

Similarly, if sym represents a namespace scope class defined in a namespace
that is different from the namespace indicated by the qualifier (due to a
using-declaration), issue an error.  For example:
  namespace N { struct S; }
  namespace M { using N::S; }
  struct M::S {};  // Error in strict mode.

A symbol from an inline namespace can be defined as if it were a member of
the enclosing namespace:
  namespace N {
    inline namespace I {
      struct A;
    }
  }
  struct N::A {};  // Okay
*/
{
  if (sym->is_class_member) {
    a_type_ptr  qualifier_class = qualifier_class_type(*loc);
    /* A nested class definition. */
    check_assertion(loc->is_class_member);
    if (!same_entities(qualifier_class, sym_parent_class(sym))) {
      pos_ty_diagnostic(strict_ansi_discretionary_severity,
                        ec_bad_qualifier_for_nested_class_decl,
                        &loc->source_position,
                        type_symbol_type(sym));
    }  /* if */
  } else {
    /* A namespace scope class definition. */
    a_namespace_ptr  qualifier_nsp;
    check_assertion(!loc->is_class_member);
    qualifier_nsp = qualifier_namespace_ptr(*loc);
    if (!same_entities(qualifier_nsp, sym_parent_namespace_or_null(sym)) &&
        !is_symbol_from_inline_namespace(sym)) {
      pos_ty_diagnostic(strict_ansi_discretionary_severity,
                        ec_bad_qualifier_for_delayed_class_definition,
                        &loc->source_position,
                        type_symbol_type(sym));
    }  /* if */
  }  /* if */
}  /* check_name_used_for_qualified_class_definition */

#if SUN_EXTENSIONS_ALLOWED

static void scan_link_scope_specifier(a_decl_flag_set         input_flags,
                                      a_decl_modifiers_block  *decl_modifiers)
/*
The current token corresponds to a Sun link scope specifier.  Update
decl_modifiers to reflect the specifier if appropriate.  Issue an error if
there are several such specifiers on the current declaration or if the
specifiers appear on a parameter declaration.  input_flags is the flag set
passed to the call to decl_specifiers.
*/
{
  if (input_flags & DSI_IS_PARAMETER) {
    pos_error(ec_parameter_with_link_scope_specifier, &error_position);
  } else if (decl_modifiers->flags & DM_ANY_SUN_LINK_SCOPE) {
    pos_error(ec_multiple_link_scope_specifiers, &error_position);
  } else {
    switch (curr_token) {
      case tok_global_link_scope:
        decl_modifiers->flags |= DM_GLOBAL_LINK_SCOPE;
        break;
      case tok_symbolic_link_scope:
        decl_modifiers->flags |= DM_SYMBOLIC_LINK_SCOPE;
        break;
      case tok_hidden_link_scope:
        decl_modifiers->flags |= DM_HIDDEN_LINK_SCOPE;
        break;
      default:
        unexpected_condition();
    }  /* if */
  }  /* if */
}  /* scan_link_scope_specifier */


static void record_sun_link_scope_for_class(a_type_ptr              class_type,
                                            a_decl_modifier_set     link_scope,
                                            a_source_position       *err_pos)
/*
The given class type is declared with the given link scope.  Record the new
link scope value, but issue an error at the given position if it loosens a
previous specification.  For example:
	class __hidden X *p;
        class __global X {};  // Error.
*/
{
  a_class_type_supplement_ptr  ctsp = class_type_supp(class_type);

  if (link_scope < (ctsp->decl_modifiers & DM_ANY_SUN_LINK_SCOPE)) {
    pos_error(ec_link_scope_relaxation, err_pos);
  }  /* if */
  ctsp->decl_modifiers &= ~DM_ANY_SUN_LINK_SCOPE;
  ctsp->decl_modifiers |= link_scope;
}  /* record_sun_link_scope_for_class */

#endif /* SUN_EXTENSIONS_ALLOWED */

void scan_class_modifiers(a_type_kind  type_kind,
                          a_boolean    *p_is_final,
                          a_boolean    *p_is_abstract,
                          a_boolean    *p_is_sealed)
/*
Scan the (context-sensitive) keywords "final", "abstract", and "sealed" and
record their presence through the given pointers: *p_is_final is set to TRUE if
"final" or "sealed" is encountered, *p_is_abstract is set to TRUE if "abstract"
is encountered, and *p_is_sealed is set to TRUE if "sealed" is encountered.
Duplicate specifiers are diagnosed as discretionary errors.  type_kind
indicates whether the modifiers are applied to a struct, union, or class type.
An error is issued if the "abstract" or "sealed" appear in a union definition.
*/
{
  a_boolean  union_error_issued = FALSE;
  a_boolean  is_final = FALSE, is_abstract = FALSE, is_sealed = FALSE;

  for (;;) {
    if (curr_token == tok_final) {
      if (is_final) {
        diagnostic(es_discretionary_error, ec_duplicate_class_modifier);
      } else {
        is_final = TRUE;
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (curr_token == tok_abstract) {
      if (is_abstract) {
        diagnostic(es_discretionary_error, ec_duplicate_class_modifier);
      } else {
        is_abstract = TRUE;
      }  /* if */
    }  else if (curr_token == tok_sealed) {
      if (is_sealed) {
        diagnostic(es_discretionary_error, ec_duplicate_class_modifier);
      } else {
        is_sealed = TRUE;
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else {
      break;
    }  /* if */
    if (type_kind == (a_type_kind)tk_union && (is_sealed || is_abstract) &&
        !union_error_issued) {
      pos_error(ec_abstract_or_sealed_on_union, &error_position);
      union_error_issued = TRUE;
    }  /* if */
    (void)get_token();
  }  /* for */
  *p_is_final = is_final || is_sealed;
  *p_is_abstract = is_abstract;
  *p_is_sealed = is_sealed;
}  /* scan_class_modifiers */


void apply_class_modifiers(a_type_ptr           class_type,
                           a_boolean            is_final,
                           ARG_UNUSED a_boolean is_abstract,
                           ARG_UNUSED a_boolean is_sealed)
/*
Update the class type entry to account for any "final"/"sealed" or "abstract"
context-sensitive keywords encountered while scanning the class definition.
*/
{
  check_assertion(is_immediate_class_type(class_type));
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (is_sealed) {
    class_type->variant.class_struct_union.sealed = TRUE;
  }  /* if */
  if (is_abstract) {
    class_type->variant.class_struct_union.abstract = TRUE;
#if BACK_END_IS_CP_GEN_BE
    class_type
      ->variant.class_struct_union.defined_with_abstract_class_modifier = TRUE;
#endif /* BACK_END_IS_CP_GEN_BE */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (is_final) {
    class_type->variant.class_struct_union.final = TRUE;
  }  /* if */
}  /* apply_class_modifiers */

#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED

static a_boolean delayed_nested_class_allowed_in_class(a_symbol_ptr  sym)
/*
Some GNU and Microsoft compilers accept delayed nested class definitions in
certain class scopes.  For example:
    struct A {
      struct B {
        struct C;
      };
      struct B::C {};  // Accepted in some Microsoft and GNU modes.
    };
This routine returns TRUE if the current scope is a class scope and if the
nested class type represented by sym can be defined in that scope.  Otherwise,
it returns FALSE.
*/
{
  a_boolean  result = FALSE;

  if (gpp_mode || microsoft_mode) {
    /* The proxy class test below is to prevent "struct T::X {}" from being
       allowed.  The is_any_template_instance_class_symbol test is used to
       prevent an instantiation from being treated as a delayed nested class
       definition. */
    if (innermost_function_scope == NULL &&
        scope_stack[depth_scope_stack].kind ==
                                       (a_scope_kind)sck_class_struct_union &&
        !is_proxy_class(type_symbol_type(sym)) &&
        !is_any_template_instance_class_symbol(sym)) {
      /* Check that the current scope encloses sym (not required for earlier
         Microsoft versions). */
      if ((microsoft_bugs && microsoft_version < 1400) ||
          (gpp_mode && gnu_version < 30300)) {
        result = TRUE;
      } else {
        a_symbol_ptr  curr_class_sym =
                        symbol_for(scope_stack[depth_scope_stack].assoc_type);
        a_symbol_ptr  parent_sym = sym;
        do {
          parent_sym = symbol_for(sym_parent_class(parent_sym));
          if (parent_sym == curr_class_sym) {
            result = TRUE;
            break;
          }  /* if */
        } while (parent_sym->is_class_member);
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* delayed_nested_class_allowed_in_class */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED

/*
Pointer to a hash table recording unresolved types.
*/
STATIC_THREAD a_hash_table_ptr
		unresolved_type_map;

/*
Key value structure for unresolved_type_map.
*/
typedef struct an_unresolved_type_map_key {
  an_assembly_scope_index
		assembly_scope_index;
  a_cpp_cli_token
		metadata_type_def_token;
} an_unresolved_type_map_key;


a_hash_value hash_unresolved_type_map_key(a_void_ptr  key_ptr)
/*
key_ptr points to an object of type an_unresolved_type_map_key.  Return a
hash value for that object.
*/
{
  an_unresolved_type_map_key  *key = (an_unresolved_type_map_key*)key_ptr;
  /* The following uses Knuth' suggestion of multiplying with the golden
     ratio of 2^32. */
  return (a_hash_value)(key->assembly_scope_index*2654435761UL
                       +key->metadata_type_def_token);
}  /* hash_unresolved_type_map_key */


a_boolean compare_for_unresolved_type_map(a_void_ptr  type_ptr,
                                          a_void_ptr  key_ptr)
/*
Return TRUE if the a_type entry pointed to by type_ptr corresponds to the
unresolved type map key pointed to by key_ptr.
*/
{
  a_type_ptr                  type = (a_type_ptr)type_ptr;
  an_unresolved_type_map_key  *key = (an_unresolved_type_map_key*)key_ptr;
  a_boolean                   result;

  if (type->kind == (a_type_kind)tk_integer) {
    an_integer_type_supplement_ptr  itsp = integer_type_supp(type);
    result = itsp->assembly_scope_index == key->assembly_scope_index &&
             itsp->metadata_type_def_token == key->metadata_type_def_token;
  } else {
    a_class_type_supplement_ptr  ctsp;
    check_assertion(is_immediate_class_type(type));
    ctsp = class_type_supp(type);
    result = ctsp->assembly_scope_index == key->assembly_scope_index &&
             ctsp->metadata_type_def_token == key->metadata_type_def_token;
  }  /* if */
  return result;
}  /* compare_for_unresolved_type_map */


static void record_unresolved_type(an_assembly_scope_index  asm_idx,
                                   a_cpp_cli_token          type_tok,
                                   a_type_ptr               *result)
/*
Return in *result the unresolved type entry associated with the given assembly
scope index and the given metadata type token.  If there is no such type yet,
create one.
*/
{
  an_unresolved_type_map_key  key;
  a_type_ptr                  *p_table_entry;

  key.assembly_scope_index = asm_idx;
  key.metadata_type_def_token = type_tok;
  if (unresolved_type_map == NULL) {
    unresolved_type_map =
           alloc_hash_table(NO_MEMORY_REGION_NUMBER, (a_hash_table_size)1000,
                            fn_for_function(hash_unresolved_type_map_key),
                            fn_for_function(compare_for_unresolved_type_map));
  }  /* if */
  p_table_entry = (a_type_ptr*)
                        hash_find(unresolved_type_map, &key, /*create=*/TRUE);
  if (*p_table_entry == NULL) {
    /* This is the first time we record this unresolved type.  Create the type
       entry and a symbol for it (the latter is not recorded in the symbol
       table). */
    a_type_ptr                   type = alloc_type((a_type_kind)tk_struct);
    a_class_type_supplement_ptr  ctsp = class_type_supp(type);
    a_symbol_ptr                 sym;
    type->incomplete = TRUE;
    ctsp->assembly_scope_index = asm_idx;
    ctsp->metadata_type_def_token = type_tok;
    ctsp->cli_class_type_kind = (a_cli_class_type_kind)cctk_unresolved;
    check_assertion(curr_token == tok_string_literal);
    sym = make_cppcli_unresolved_type_symbol(&const_for_curr_token);
    set_source_corresp(&type->source_corresp, sym);
    add_to_types_list(type, DEPTH_OF_FILE_SCOPE);
    *p_table_entry = type;
  }  /* if */
  *result = *p_table_entry;
}  /* record_unresolved_type */


a_type_ptr scan_unresolved_metadata_type(void)
/*
Scan a construct of the form
    __unresolved_type ( <assembly/scope-index> , <type-token> , <type-name> )
where the first two arguments are integer constants, and the third is a string
literal.  This construct is generated from C++/CLI metadata to denote a
type that is referred to in that metadata, but which is declared in another
assembly that hasn't been loaded (yet).
Return a class type with CLI class type kind cctk_unresolved.
*/
{
  a_type_ptr  result;

  /* Skip over the __unresolved_type token. */
  check_assertion(curr_token == tok_unresolved_type);
  (void)get_token();
  /* A '(' should be next. */
  if (required_token(tok_lparen, ec_exp_lparen)) {
    an_assembly_scope_index  assembly_scope_index = 0;
    a_cpp_cli_token          metadata_type_def_token = 0;
    a_source_position        arg_pos;
    a_constant_ptr           con = local_constant();
    a_boolean                ovflo;
    arg_pos = pos_curr_token;
    add_stop_token(tok_rparen);
    add_stop_token(tok_comma);
    /* Scan the first argument, which should be an integer constant. */
    scan_integral_constant_expression(con);
    if (is_error_constant(con)) {
      expect_error();
    } else if (con->kind != (a_constant_repr_kind)ck_integer) {
      pos_error(ec_exp_int_constant, &arg_pos);
    } else {
      assembly_scope_index = (an_assembly_scope_index)
                             unsigned_value_of_integer_constant(con, &ovflo);
      check_assertion(!ovflo);
    }  /* if */
    (void)required_token(tok_comma, ec_exp_comma);
    /* Scan the second argument, which should also be an integer constant. */
    scan_integral_constant_expression(con);
    if (is_error_constant(con)) {
      expect_error();
    } else if (con->kind != (a_constant_repr_kind)ck_integer) {
      pos_error(ec_exp_int_constant, &arg_pos);
    } else {
      metadata_type_def_token = (a_cpp_cli_token)
                               unsigned_value_of_integer_constant(con, &ovflo);
      check_assertion(!ovflo);
    }  /* if */
    (void)required_token(tok_comma, ec_exp_comma);
    /* A string literal should be next.  If not, zero the metadata type token
       to force the production of an error type below. */
    if (!required_token_no_advance(tok_string_literal,
                                   ec_exp_string_literal)) {
      metadata_type_def_token = 0;
    }  /* if */
    if (metadata_type_def_token == 0 || assembly_scope_index == 0) {
      result = error_type();
    } else {
      record_unresolved_type(assembly_scope_index, metadata_type_def_token,
                             &result);
      (void)get_token();
    }  /* if */
    (void)required_token(tok_rparen, ec_exp_rparen);
    remove_stop_token(tok_comma);
    remove_stop_token(tok_rparen);
    release_local_constant(&con);
  } else {
    /* __unresolved_type not followed by a parenthesis. */
    result = error_type();
  }  /* if */
  return result;
}  /* scan_unresolved_metadata_type */


an_assembly_visibility scan_cli_visibility_specifier_if_any(
                                                       a_source_position  *pos)
/*
If the current token is "public" or "private" scan past it, set *pos to its
position, and return av_public or av_private accordingly.  Otherwise, return
av_none and leave *pos unchanged.  (Called in C++/CLI mode only.)
*/
{
  an_assembly_visibility  result = (an_assembly_visibility)av_none;

  if (is_cli_assembly_visibility_specifier(curr_token)) {
    *pos = pos_curr_token;
    switch (curr_token) {
      case tok_public:   result = (an_assembly_visibility)av_public;   break;
      case tok_private:  result = (an_assembly_visibility)av_private;  break;
      default:           unexpected_condition();
    }  /* switch */
    *pos = pos_curr_token;
    /* Scan past the visibility specifier token. */
    (void)get_token();
    if (curr_token == tok_public || curr_token == tok_private) {
      /* Multiple specifiers are an error. */
      pos_error(ec_multiple_visibility_specifiers, &pos_curr_token);
      do {
        (void)get_token();
      } while (curr_token == tok_public || curr_token == tok_private);
    }  /* if */
  }  /* if */
  return result;
}  /* scan_cli_visibility_specifier_if_any */


void set_cli_visibility(a_type_ptr              type,
                        an_assembly_visibility  declared_visibility,
                        a_source_position_ptr   diag_pos,
                        a_boolean               is_definition)
/*
type represents a class or enum type whose specifier was just scanned.
Determine the C++/CLI assembly visibility of this type.  If a C++/CLI
visibility was explicitly specified, declared_visibility indicates whether it
was "public" or "private".  Otherwise, declared_visibility is av_none.  If the
type declaration is a definition, is_definition is TRUE.  Issue diagnostics as
appropriate at the position indicated by diag_pos.  (Called in C++/CLI mode
only.)
*/
{
  an_assembly_visibility  vis = (an_assembly_visibility)av_private;

  check_assertion(cli_or_cx_enabled);
  if (declared_visibility != (an_assembly_visibility)av_none) {
    /* An explicitly specified visibility: Ensure this is a top-level
       definition. */
    if (type->source_corresp.is_class_member) {
      pos_error(ec_visibility_specifier_on_nested_type, diag_pos);
    } else if (!is_definition) {
      pos_error(ec_visibility_specifier_requires_definition, diag_pos);
    } else {
      vis = declared_visibility;
    }  /* if */
  }  /* if */
  if (is_definition) {
    /* Set the visibility in the IL entry. */
    if (type->source_corresp.is_class_member) {
      /* If the current member assembly access is private, then the nested
         class is assembly-private; otherwise, the visibility of the parent
         applies. */
      if (scope_stack_top().current_assembly_access ==
                                            (an_access_specifier)as_private) {
        vis = (an_assembly_visibility)av_private;
      } else {
        vis = get_assembly_visibility_of(parent_class_of(type));
      }  /* if */
    }  /* if */
    if (is_immediate_class_type(type)) {
      class_type_supp(type)->declared_assembly_visibility =
                                                          declared_visibility;
      class_type_supp(type)->assembly_visibility = vis;
    } else {
      check_assertion(type->kind == (a_type_kind)tk_integer);
      integer_type_supp(type)->declared_assembly_visibility =
                                                          declared_visibility;
      integer_type_supp(type)->assembly_visibility = vis;
    }  /* if */
  }  /* if */
}  /* set_cli_visibility */


static void check_interface_redeclaration(a_symbol_ptr       prev_decl,
                                          a_symbol_kind      tag_kind,
                                          a_boolean          *is_interface,
                                          a_boolean          is_definition,
                                          a_source_position  *tag_pos)
/*
Verify that previous declarations of a class (represented by prev_decl) are
consistent in their use of the Microsoft keyword "__interface".  Issue a
diagnostic otherwise.  tag_kind, *is_interface, is_definition, and *tag_pos
describe properties of the current declaration.  *is_interface is set to FALSE
if the type should not be treated as an interface.
*/
{
  a_type_ptr  class_type = prev_decl->variant.class_struct_union.type;

  if (*is_interface != class_type->variant.class_struct_union.is_interface) {
    /* Only the first declaration and the definition of an interface must use
       the __interface keyword.  Other declarations can use "struct" or "class"
       (but not "union").  Furthermore, if the first declaration uses
       "__interface" and the definition does not, the code is accepted but the
       type is not an interface type. */
    an_error_severity  sev = es_warning;
    a_boolean          no_interface = FALSE;
    if (tag_kind == (a_symbol_kind)sk_union_tag ||
        class_type->kind == (a_type_kind)tk_union) {
      sev = es_error;
      no_interface = TRUE;
    } else if (is_definition) {
      if (*is_interface) {
        /* The current declaration is a definition using the "__interface"
           keyword but the first declaration did not use that keyword:
           Microsoft compilers diagnose this, unless the prior declaration was
           a "struct" (as opposed to a "class" or "union").  We emulate that
           behavior, and proceed with an interface type even in the error
           cases. */
        if (class_type->kind != (a_type_kind)tk_struct) {
          sev = es_error;
        }  /* if */
        class_type->kind = (a_type_kind)tk_struct;
        class_type->variant.class_struct_union.is_interface = TRUE;
        class_type->variant.class_struct_union.abstract = TRUE;
      } else {
        /* The definition does not use the "__interface" keyword: Don't
           treat the type as an interface. */
        no_interface = TRUE;
      }  /* if */
    }  /* if */
    pos_stsy_diagnostic(sev, ec_tag_kind_incompatible_with_declaration,
                        tag_pos, name_of_symbol_kind(tag_kind), prev_decl);
    if (no_interface) {
      class_type->variant.class_struct_union.is_interface = FALSE;
      class_type->variant.class_struct_union.abstract = FALSE;
      *is_interface = FALSE;
    }  /* if */
  }  /* if */
}  /* check_interface_redeclaration */


static void move_declspec_align_attr(an_attribute_ptr  *p_prefix_attributes, 
                                     an_attribute_ptr  *p_tag_attributes)
/*
Move "__declspec(align(...))" attributes on the *p_prefix_attributes to the
head of the list pointed to by *p_tag_attributes (the moved attributes are
reclassified as al_tag_name).
*/
{
  an_attribute_ptr  moved = NULL, *p_end_moved = &moved;

  /* Collect the "__declspec(align(...))" attributes. */
  while (*p_prefix_attributes != NULL) {
    an_attribute_ptr  ap = *p_prefix_attributes;
    if (ap->kind == ak_align && ap->family == af_ms_declspec) {
      ap->syntactic_location = al_tag_name;
      *p_end_moved = ap;
      p_end_moved = &ap->next;
      *p_prefix_attributes = ap->next;
    } else {
      p_prefix_attributes = &ap->next;
    }  /* if */
  }  /* while */
  /* Prepend the collected attributes to the *p_tag_attributes list. */
  *p_end_moved = *p_tag_attributes;
  *p_tag_attributes = moved;
}  /* move_declspec_align_attr */


static void preapply_microsoft_class_align_attribute(
                                     a_decl_parse_state  *dps,
                                     a_boolean            is_class_definition)
/*
The Microsoft __declspec(align(...)) attribute applied to a class type behaves
differently from other __declspec attributes.  If necessary, adjust the
dps->tag_attributes, dps->prefix_attributes, and dps->specifier_attributes
lists to achieve the same effect as Microsoft compilers.  A diagnostic may be
issued in some cases.
*/
{
  if (is_class_definition) {
    /* Microsoft compilers treat
         __declspec(align(N)) struct __declspec(...) X { ... };
       as
         struct __declspec(align(N), ...) X { ... };
       (and this is specific to the "align" attributes).  Emulate this behavior
       by moving "__declspec(align(...))" attributes on dps->prefix_attributes
       and dps->specifier_attributes to the head of the list pointed to by
       dps->tag_attributes (the moved attributes are reclassified as
       al_tag_name).
    */
    if (dps->prefix_attributes != NULL) {
      move_declspec_align_attr(&dps->prefix_attributes, &dps->tag_attributes);
    }
    if (dps->specifier_attributes != NULL) {
      move_declspec_align_attr(&dps->specifier_attributes,
                               &dps->tag_attributes);
    }
  } else {
    an_attribute_ptr  ap = dps->tag_attributes;
    for (; ap != NULL; ap = ap->next) {
      if (ap->kind == ak_align && ap->family == af_ms_declspec) {
        pos_st_warning(ec_attribute_ignored_on_nondefinition, &ap->position,
                       ap->name);
        make_attr_unrecognized(ap);
      }  /* if */
    }  /* for */
  }  /* if */
}  /* preapply_microsoft_class_align_attribute */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static a_type_ptr scan_edg_internal_type(void)
/*
Scan a construct of the form

	__edg_internal_type__(<integral constant N>)

and return internal_type_array[N] if N<n_internal_types or an error type
otherwise.
*/
{
  a_type_ptr             result;
  a_boolean              success = FALSE;
  a_host_large_unsigned  n = 0;

  (void)get_token();
  /* A '(' should be next. */
  if (required_token(tok_lparen, ec_exp_lparen)) {
    add_stop_token(tok_rparen);
    if (curr_token == tok_int_constant) {
      a_constant_ptr  cp = &const_for_curr_token;
      if (sign_of_integer_constant(cp) >= 0) {
        a_boolean             ovflo;
        n = unsigned_value_of_integer_constant(cp, &ovflo);
        if (!ovflo && n < n_internal_types) {
          success = TRUE;
        }  /* if */
      }  /* if */
      if (!success) {
        pos_error(ec_integer_overflow, &pos_curr_token);
      }  /* if */
      (void)get_token();
    } else {
      syntax_error(ec_exp_int_constant);
    }  /* if */
    (void)required_token(tok_rparen, ec_exp_rparen);
    remove_stop_token(tok_rparen);
  }  /* if */
  if (success) {
    result = internal_type_array[n];
  } else {
    result = error_type();
  }  /* if */
  return result;
}  /* scan_edg_internal_type */


static a_type_ptr scan_edg_vector_type(a_vector_kind  kind)
/*
Scan a construct of the form

	__edg_vector_type__(<element type>, <integral constant N>)

and return a tk_vector type representing a vector of N elements of the given
type.
*/
{
  a_type_ptr      vtype, etype = NULL;
  a_boolean       err = FALSE;
  a_targ_size_t   n_elems, esize = 0;
#if GNU_VECTOR_TYPES_ALLOWED
  a_constant_ptr  size_con = NULL;
#endif /* GNU_VECTOR_TYPES_ALLOWED */

  (void)get_token();
  /* A '(' should be next. */
  if (required_token(tok_lparen, ec_exp_lparen)) {
    a_source_position  pos;
    a_constant_ptr     con = local_constant();
    pos = pos_curr_token;
    add_stop_token(tok_rparen);
    add_stop_token(tok_comma);
    type_name(&etype);
    if (is_integral_or_enum_type(etype) 
#if C99_IL_EXTENSIONS_SUPPORTED
        || is_real_floating_type(etype)
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#if GNU_VECTOR_TYPES_ALLOWED
        || type_is(skip_typerefs(etype), tk_mfp8)
#endif /* GNU_VECTOR_TYPES_ALLOWED */
                                                 ) {
      /* The normal case. */
#if GNU_VECTOR_TYPES_ALLOWED
      /* Element types for NEON vectors have additional restrictions. */
      if (kind == vk_neon && !is_valid_neon_vector_element_type(etype)) {
        pos_ty_error(ec_invalid_neon_vector_element_type, &pos, etype);
        err = TRUE;
      } else if (kind == vk_neon_poly &&
                 !is_valid_neon_polyvector_element_type(etype)) {
        pos_ty_error(ec_invalid_neon_polyvector_element_type, &pos, etype);
        err = TRUE;
      }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
      esize = skip_typerefs(etype)->size;
    } else if (is_template_param_type(etype)) {
      /* Use an arbitrary nonzero size. */
      esize = 1;
    } else {
      /* Other type kinds are invalid. */
      if (!is_error_type(etype)) {
        pos_ty_error(ec_invalid_vector_element_type, &pos, etype);
      }  /* if */
      err = TRUE;
    }  /* if */
    (void)required_token(tok_comma, ec_exp_comma);
    pos = pos_curr_token;
    scan_integral_constant_expression(con);
    if (is_error_constant(con)) {
      expect_error();
      err = TRUE;
    } else if (con->kind == (a_constant_repr_kind)ck_template_param) {
      /* Record a dummy (nonzero) size. */
      n_elems = 1;
#if GNU_VECTOR_TYPES_ALLOWED
      size_con = move_local_constant_to_il(&con);
#endif /* GNU_VECTOR_TYPES_ALLOWED */
    } else if (con->kind != (a_constant_repr_kind)ck_integer) {
      pos_error(ec_exp_int_constant, &pos);
      err = TRUE;
    } else if (!err) {
      a_boolean  ovflo;
      n_elems = (a_targ_size_t)unsigned_value_of_integer_constant(con, &ovflo);
      if ((kind == vk_neon || kind == vk_neon_poly) &&
          (ovflo || (n_elems * esize != 8 && n_elems * esize != 16))) {
        pos_error(ec_invalid_neon_vector_size, &pos);
        err = TRUE;
      } else if (ovflo ||
                 (n_elems * esize) >
                                  (a_targ_size_t)targ_maximum_pack_alignment) {
        pos_error(ec_vector_length_too_large, &pos);
        err = TRUE;
      } else if ((n_elems & (n_elems-1)) != 0) {
        pos_error(ec_vector_size_must_be_power_of_two, &pos);
        err = TRUE;
      }  /* if */
    }  /* if */
    (void)required_token(tok_rparen, ec_exp_rparen);
    remove_stop_token(tok_comma);
    remove_stop_token(tok_rparen);
    release_local_constant(&con);
  } else {
    err = TRUE;
  }  /* if */
#if GNU_VECTOR_TYPES_ALLOWED
  if (!err) {
    vtype = make_vector_type(etype, n_elems, kind);
    vtype->variant.vector.size_constant = size_con;
  } else
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  /* Do not insert code here. */
  {
    vtype = error_type();
  }  /* if */
  return vtype;
}  /* scan_edg_vector_type */


static a_type_ptr scan_edg_scalable_vector_type(void)
/*
Scan a construct of the form

        __edg_scalable_vector_type__(<element type>, <integral constant N>)

and return a tk_scalable_vector type representing a vector of N tuple elements
of the given type.  This syntax can be used to create a scalable vector type
irrespective of the configured compatibility mode (e.g., for builtin
declarations that need to work across compatibility modes).
*/
{
  a_type_ptr      etype = NULL, vtype;
  a_boolean       err = FALSE;
#if GNU_VECTOR_TYPES_ALLOWED
  a_targ_size_t   n_tuple_elems = 0;
#endif /* GNU_VECTOR_TYPES_ALLOWED */

  (void)get_token();
  /* A '(' should be next. */
  if (required_token(tok_lparen, ec_exp_lparen)) {
    a_source_position  pos;
    a_type_ptr         utp;
    pos = pos_curr_token;
    add_stop_token(tok_rparen);
    add_stop_token(tok_comma);
    type_name(&etype);
    utp = skip_typerefs(etype);
    if (is_qualified_type(etype)) {
      err = TRUE;
    } else if (is_standard_integer_type(utp)) {
      /* Standard integer types other than (unsigned) long long are allowed. */
#if LONG_LONG_ALLOWED
      if (utp->variant.integer.int_kind == ik_long_long ||
          utp->variant.integer.int_kind == ik_unsigned_long_long) {
        err = TRUE;
      }  /* if */
#endif /* LONG_LONG_ALLOWED */
    } else if (is_real_floating_type(utp)) {
      a_float_kind  float_kind = utp->variant.float_kind;
      err = float_kind != fk_std_bfloat16 && float_kind != fk_fp16 &&
            float_kind != fk_float && float_kind != fk_double;
    } else if (!is_bool_type(utp)
#if GNU_VECTOR_TYPES_ALLOWED
               && !type_is(utp, tk_mfp8)
#endif /* GNU_VECTOR_TYPES_ALLOWED */
                                        ) {
      /* Other type kinds are invalid. */
      err = TRUE;
    }  /* if */
    if (err && !is_error_type(utp)) {
      pos_ty_error(ec_invalid_scalable_vector_element_type, &pos, etype);
    }  /* if */
    (void)required_token(tok_comma, ec_exp_comma);
    if (required_token(tok_int_constant, ec_exp_int_constant)) {
      a_host_large_integer  val;
      conv_integer_value_to_host_large_integer(
                                   &const_for_curr_token.variant.integer_value,
                                   /*is_signed=*/FALSE, &val, &err);
      if (!err) {
        if ((val < 1 || val > 4) || (is_bool_type(etype) && val == 3)) {
          pos_error(ec_invalid_scalable_vector_tuple_elements, &pos);
          err = TRUE;
#if GNU_VECTOR_TYPES_ALLOWED
        } else {
          n_tuple_elems = (a_targ_size_t)val;
#endif /* GNU_VECTOR_TYPES_ALLOWED */
        }  /* if */
      }  /* if */
    }  /* if */
    (void)required_token(tok_rparen, ec_exp_rparen);
    remove_stop_token(tok_comma);
    remove_stop_token(tok_rparen);
  } else {
    err = TRUE;
  }  /* if */
#if GNU_VECTOR_TYPES_ALLOWED
  if (!err) {
    vtype = make_scalable_vector_type(etype, (uint8_t)n_tuple_elems);
  } else
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  /* Do not insert code here. */
  {
    vtype = error_type();
  }  /* if */
  return vtype;
}  /* scan_edg_scalable_vector_type */


void update_membership_of_class(
                              a_symbol_ptr                 tag_sym,
                              a_boolean                    def_or_vacuous_decl,
                              ARG_UNUSED a_boolean         is_event_interface,
                              a_scope_depth                decl_level,
                              ARG_UNUSED a_source_position *diag_pos)
/*
The given class/struct/union symbol has just been created.  Record its class
or namespace membership if appropriate.  In C++ mode, also set its name
linkage if necessary.  A few other related peripheral fields are set by this
routine.  def_or_vacuous_decl is TRUE if this is a definition or a vacuous
declaration.  is_event_interface is TRUE if the symbol represents an "__event
__interface".  decl_level determines the scope in which the declaration
appears.  Diagnostics may be emitted at the given position.
*/
{
  a_boolean  is_local_class = FALSE;
  a_type_ptr  class_type = tag_sym->variant.class_struct_union.type;

  if (depth_innermost_function_scope != NO_SCOPE_DEPTH ||
      inside_local_class) {
    /* This declaration appears within a function or block scope, or else it
       is a nested class declaration within a local class.  In either case,
       it is a local class. */
    a_routine_ptr  rp = NULL;
    is_local_class = TRUE;
    if (depth_innermost_function_scope != NO_SCOPE_DEPTH) {
      rp = innermost_function_scope->variant.routine.ptr;
      rp->contains_local_class_type = TRUE;
    } else {
      a_scope_stack_entry_ptr  ssep = &scope_stack[decl_level];
      if (ssep->depth_innermost_function_scope != NO_SCOPE_DEPTH) {
        rp = scope_stack[decl_level].assoc_routine;
      } else if (scope_is(ssep, sck_class_struct_union) ||
                 scope_is(ssep, sck_class_reactivation)) {
        rp = ssep->assoc_type->source_corresp.enclosing_routine;
      } else {
        expect_error();
        for (; !scope_is(ssep, sck_file); ssep -= 1) {
          if (scope_is(ssep, sck_function)) {
            rp = ssep->assoc_routine;
            break;
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
    class_type->source_corresp.enclosing_routine = rp;
  }  /* if */
  if (!C_mode()) {
    switch (scope_stack[decl_level].kind) {
      case sck_class_struct_union:
        /* A new class name is being declared within a class scope. */
        if (def_or_vacuous_decl) {
          /* Either a definition or a vacuous declaration -- either way, the
             class name is introduced in the current scope. */
          a_type_ptr  parent = scope_stack[decl_level].assoc_type;
          set_class_membership(tag_sym, &class_type->source_corresp, parent);
          class_type->source_corresp.access =
                               scope_stack[depth_scope_stack].current_access;
#if MICROSOFT_EXTENSIONS_ALLOWED
          class_type->source_corresp.assembly_access =
                                    scope_stack_top().current_assembly_access;
          if (microsoft_mode &&
              !class_type_supp(class_type)->is_lambda_closure_class) {
            if (cppcli_enabled &&
                is_immediate_managed_class_type(parent) &&
                is_immediate_standard_class_type(class_type)) {
              /* Standard class types cannot be nested in managed class types
                 (but the opposite is usually okay). */
              if (is_immediate_delegate_type(class_type)) {
                /* A more specific error is issued elsewhere for delegate
                   definitions appearing in standard class types. */
                expect_error();
              } else {
                pos_error(ec_standard_class_nested_in_managed_class, diag_pos);
              }  /* if */
            }  /* if */
            /* coverity[dead_error_condition] */
            if (class_type->variant.class_struct_union.is_interface &&
                !is_event_interface) {
              /* In the __event __interface case, a more specific error is
                 given later. */
              pos_error(ec_interface_cannot_be_nested_class, diag_pos);
            }  /* if */
          }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        }  /* if */
        break;
      case sck_class_reactivation:
        /* A class declared in a class reactivation scope.  This should only
           occur for lambda closure classes, unless we're in a context where
           we are lazy-loading module entities.  Make the class a member of the
           reactivated class. */
        { a_type_ptr  parent = scope_stack[decl_level].assoc_type;
          a_class_type_supplement_ptr
                      ctsp = class_type_supp(class_type);
          check_assertion(ctsp->is_lambda_closure_class ||
                          in_code_from_module());
          set_class_membership(tag_sym, &class_type->source_corresp, parent);
          if (ctsp->is_lambda_closure_class) {
            class_type->source_corresp.access = (an_access_specifier)as_public;
          }  /* if */
        }
        break;
      case sck_namespace:
      case sck_namespace_extension:
        /* A class is being declared within a namespace.  This includes
           friend declarations injected into a namespace from a class
           scope. */
        set_namespace_membership(
                   tag_sym, &class_type->source_corresp,
                   scope_stack[decl_level].il_scope->variant.assoc_namespace);
        break;
      default:;
    }  /* switch */
    /* In C classes have no linkage, as do local classes in C++; otherwise
       classes have "C++-external" name linkage.  (Note: in cfront mode
       classes may also have internal linkage -- see ARM 3.3.)  Note that
       even nameless classes may be marked as having linkage; this is
       useful for dealing with member functions.) */
    if (!is_local_class) {
      /* Nonlocal class. */
      set_name_linkage_for_type(class_type);
    } else {
#if CHECKING
      /* For a local class, save information about the enclosing function. */
      a_scope_stack_entry_ptr  ssep;
      if (depth_innermost_function_scope != NO_SCOPE_DEPTH) {
        ssep = &scope_stack[depth_innermost_function_scope];
      } else {
        /* Depth innermost function scope is not set when this local
           class is declared within another nested class.  Find the
           enclosing function scope. */
        for (ssep = scope_stack_entry_for(depth_scope_stack);
             ssep != NULL; ssep = previous_scope_of(ssep)) {
          if (scope_is(ssep, sck_function) ||
              (scope_is(ssep, sck_template_instantiation) &&
               ssep->assoc_routine != NULL)) {
            break;
          }  /* if */
        }  /* for */
      }  /* if */
      check_assertion(ssep != NULL && ssep->assoc_routine != NULL);
#endif /* CHECKING */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (cppcli_enabled &&
          depth_innermost_function_scope != NO_SCOPE_DEPTH &&
          !is_immediate_managed_class_type(class_type)) {
        /* Local class types are not allowed in members of managed classes.
           (But C++/CX member functions can have local classes and lambdas.) */
        a_routine_ptr  rp = innermost_function_scope->variant.routine.ptr;
        check_assertion(rp != NULL);
        if (rp->source_corresp.is_class_member) {
          a_type_ptr  parent_class = parent_class_of(rp);
          if (is_managed_class_type(parent_class)) {
            pos_error(class_type_supp(class_type)->is_lambda_closure_class ?
                                ec_local_lambda_in_managed_member_function :
                                ec_local_class_in_managed_member_function,
                      diag_pos);
          }  /* if */
        }  /* if */
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
  }  /* if */
}  /* update_membership_of_class */


void attach_tag_attributes(an_attribute_ptr    attributes,
                           a_type_ptr          type,
                           a_decl_parse_state  *dps,
                           a_boolean           is_definition,
                           a_boolean           is_forward_decl,
                           a_boolean           ignore_gnu_attributes)
/*
The given attributes were specified on the given type after a "class", "enum",
"struct", or "union" keyword.  Attach and apply the attributes to the type.
If is_definition is TRUE, the attributes appeared on the type definition.  If
is_forward_decl is TRUE, the attributes appeared on an autonomous forward
declaration; i.e., something like:
  class [[]] X;
rather than e.g.
  class [[]] X *p;
If ignore_gnu_attributes is TRUE, turn any recognized GNU attributes into
ak_unrecognized attributes (which means they will have no further effect), and
issue a warning.
*dps describes the declaration that is being parsed.  dps is NULL if the
attributes are attached as part of the template instantiation process.

Note that this call is typically followed by a call to
record_strongest_alignment_attr to finalize any standard alignment attributes
(but that call isn't performed here because it is also necessary at the end of
a declaration/definition to check for cases where a required attribute is
missing).
*/
{
  a_boolean         gnu_warning_emitted = FALSE, std_error_emitted = FALSE;
  an_attribute_ptr  ap;

  if (is_error_type(type)) {
    /* Nothing to do (i.e., silently ignore the attributes). */
  } else if (is_template_param_type(type)) {
    /* We can get here in some nonstandard cases like "enum [[]] T x;" where T
       is a template parameter.  Ignore the attributes (with a warning). */
    if (attributes != NULL) {
      pos_warning(ec_attributes_ignored, &attributes->position);
    }  /* if */
  } else {
    check_assertion(is_immediate_class_type(type) ||
                    is_immediate_enum_type(type));
    /* Traverse the attribute list and record whether the attribute was on a
       "primary" declaration (i.e., a definition).  Also issue diagnostics for
       invalid combinations. */
    for (ap = attributes; ap != NULL; ap = ap->next) {
      /* If this is a definition, mark the associated attributes as appearing
         on a definition.  If this is the explicit instantiation of a class
         template, also do this since the directive will trigger the
         instantiation of the definition and the tag attributes apply to that
         definition in such cases. */
      ap->on_primary_declaration = is_definition ||
                                   (dps != NULL &&
                                    dps->is_explicit_instantiation &&
                                    is_forward_decl);
      if (is_std_attribute(ap)) {
        /* Standard attributes cannot appear in this syntactic location if no
           class/enum definition follows. */
        if (!is_definition && !is_forward_decl) {
          if (!std_error_emitted) {
            pos_error(ec_invalid_attribute_location, &ap->position);
            std_error_emitted = TRUE;
          }  /* if */
          make_attr_unrecognized(ap);
        }  /* if */
      } else if (is_gcc_attribute(ap)) {
        /* GNU attributes in this syntactic location are ignored in most
           contexts that aren't definitions. */
        if (ignore_gnu_attributes && !is_unapplicable_attr(ap)) {
          if (!gnu_warning_emitted) {
            pos_warning(is_immediate_class_type(type) ?
                          ec_attribute_ignored_on_incomplete_class_decl :
                          ec_enum_attribute_ignored,
                        &ap->group->position);
            gnu_warning_emitted = TRUE;
          }  /* if */
          make_attr_unrecognized(ap);
        }  /* if */
      }  /* if */
    }  /* for */
    /* Temporarily attach dps to the attributes, to the application routines
       can examine the declaration context (e.g., to see if the tag is declared
       as part of a friend declaration). */
    for (ap = attributes; ap != NULL; ap = ap->next) {
      ap->assoc_info = (void*)dps;
    }  /* for */
    attach_attributes(attributes, (char*)type, iek_type);
    /* Detach dps from the attributes. */
    for (ap = attributes; ap != NULL; ap = ap->next) {
      ap->assoc_info = NULL;
    }  /* for */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (is_forward_decl) {
      /* If a source sequence entry was recorded (which would necessarily be a
         secondary entry, since this is not a definition), associate a copy of
         the given attributes with that entry. */
      a_source_sequence_entry_ptr
                      ssep = last_matching_source_sequence_entry((char*)type);
      if (ssep != NULL) {
        check_assertion(ss_entry_kind(ssep) == iek_src_seq_secondary_decl);
        ss_entry_ptr(ssep, a_src_seq_secondary_decl_ptr)->attributes =
                                          copy_of_attributes_list(attributes);
      }  /* if */
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  }  /* if */
}  /* attach_tag_attributes */

#if GNU_EXTENSIONS_ALLOWED

static void attach_postfix_enum_attributes(an_attribute_ptr    attributes,
                                           a_type_ptr          enum_type,
                                           a_decl_parse_state  *dps)
/*
The given GNU attributes were specified immediately after the definition of
enum_type (an enumeration type) in a declaration described by *dps.  Apply
any of the attributes that modify the enum type directly (e.g., "packed")
and move the remaining attributes to dps->specifier_attributes.
*/
{
  an_attribute_ptr  *p_from = &attributes, *p_to;

  p_to = last_attribute_link(&dps->specifier_attributes);
  /* Traverse the attribute list and move those that are ordinary specifier
     attributes to the dps->specifier_attributes list. */
  while (*p_from != NULL) {
    switch ((*p_from)->kind) {
      case ak_packed:
      case ak_mode:
        /* Leave these attributes on the list. */
        p_from = &(*p_from)->next;
        break;
      default:
        /* Move all other attributes to the specifiers list. */
        *p_to = *p_from;
        (*p_from)->syntactic_location = al_specifier;
        *p_from = (*p_from)->next;
        p_to = &(*p_to)->next;
    }  /* switch */
  }  /* while */
  attach_tag_attributes(attributes, enum_type, dps, /*is_definition=*/TRUE,
                        /*is_forward_decl=*/FALSE,
                        /*ignore_gnu_attributes=*/FALSE);
}  /* attach_postfix_enum_attributes */

#endif /* GNU_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS

static void update_sse_for_first_tag_declaration(
                                              a_type_ptr        tag_type,
                                              a_symbol_locator  *locator,
                                              a_boolean         is_definition,
                                              a_boolean         gnu_extension)
/*
tag_type is being declared for the first time (and defined if is_definition
is TRUE).  The name used in the source is described by *locator.  A source
sequence entry was already created for it: Update that source sequence entry
if needed (as well as an associated name reference entry if appropriate).
gnu_extension is TRUE if the class was declared with the GNU keyword
__extension__.
*/
{
  a_name_reference_ptr  name_ref = NULL;

  if (record_name_references_in_context()) {
    name_ref = qualifiable_name_reference(locator, &tag_type->source_corresp);
  }  /* if */
  if (!is_definition) {
    /* Set the first_declaration flag in the associated source-sequence
       secondary declaration entry.  The corresponding field in the class
       symbol supplement will already have been set for definitions, if
       appropriate. */
    an_sssd_flag_set  flags = SSSD_FIRST_DECLARATION;
#if TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
    if (curr_token == tok_removed_template_body) {
      /* In code like "struct S { struct N {} *p; };" N is a nested non-
         autonomous class type.  In the similar "struct S { struct N *p; };"
         it is a namespace-scope class.  When representing template instances
         in source sequence lists, the former can end up as the latter if
         no care is taken.  For example:
           template<class T> struct S { struct N {} *p; };
           S<int> s;
         doesn't instantiate S<int>::N and would therefore result in a
         specialization like:
           template<> struct S { struct N *p; };
         To avoid that, we mark the nested class as autonomous, which is
         equivalent to rewriting the specialization as:
           template<> struct S { struct N; N *p; };
         We also record that this was originally a non-autonomous definition
         to ensure that source sequence entries of a later full instance are
         inserted at the right location. */
      check_assertion(tag_type->source_corresp.is_class_member);
      flags |= SSSD_AUTONOMOUS_TAG_DECL |
               SSSD_ORIGINALLY_NONAUTONOMOUS_DEFINITION;
    }  /* if */
#endif /* TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
    if (gnu_extension) {
      flags |= SSSD_MARKED_AS_GNU_EXTENSION;
    }  /* if */
    (void)set_src_seq_secondary_decl_fields((char*)tag_type, (a_type*)NULL,
                                            name_ref, flags);
#if GNU_EXTENSIONS_ALLOWED
  } else {
    if (name_ref != NULL) {
      name_ref->used_in_primary_declarator = TRUE;
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
    tag_type->source_corresp.marked_as_gnu_extension = gnu_extension;
  }  /* if */
}  /* update_sse_for_first_tag_declaration */

#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

void diagnose_std_attribute_on_explicit_instantiation(an_attribute_ptr  ap)
/*
ap points to a list of attributes applied to an explicit template
instantiation.  If any of these attributes use standard attribute syntax,
issue an error and reclassify those attributes as ak_unrecognized.
*/
{
  a_boolean  diagnostic_issued = FALSE;

  for (; ap != NULL; ap = ap->next) {
    if (is_std_attribute(ap)) {
      if (!diagnostic_issued) {
        pos_error(ec_attribute_on_explicit_instantiation,
                  &ap->group->position);
        diagnostic_issued = TRUE;
      }  /* if */
      make_attr_unrecognized(ap);
    }  /* if */
  }  /* if */
}  /* diagnose_std_attribute_on_explicit_instantiation */


static a_boolean is_allowed_ms_spec_of_base_template(a_symbol_ptr	sym)
/*
We are processing the declaration, in Microsoft mode, of an explicit
specialization of the class specified by sym.  Return TRUE if we are in
the definition of a class and sym is a specialization of a template from
a base class of the current class, and the base class and current class
are both instantiations of the same template.
*/
{
  a_boolean			result = FALSE;
  a_scope_stack_entry_ptr	ssep;

  ssep = scope_stack_entry_for(depth_scope_stack);
  if (sym->is_class_member &&
      ssep->kind == (a_scope_kind)sck_class_struct_union) {
    a_type_ptr	curr_type = ssep->assoc_type;
    /* The check is only needed for template classes. */
    if (curr_type->variant.class_struct_union.is_template_class) {
      a_type_ptr	parent_type = sym_parent_class(sym);
      if (parent_type->variant.class_struct_union.is_template_class) {
        /* Get the primary template from which both types are based. */
        a_symbol_ptr	curr_sym = symbol_for(curr_type);
        a_symbol_ptr	curr_template_sym = template_for_instance(curr_sym);
        a_symbol_ptr	parent_sym = symbol_for(parent_type);
        a_symbol_ptr	parent_template_sym =
                                             template_for_instance(parent_sym);
        curr_template_sym = primary_template_of(curr_template_sym);
        parent_template_sym = primary_template_of(parent_template_sym);
        if (curr_template_sym == parent_template_sym) {
          /* The templates are the same.  Make sure that the class being
             defined is a base class of the parent of the member that is
             being specialized. */
          if (find_base_class_of(ssep->assoc_type, parent_type) != NULL) {
            result = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* is_allowed_ms_spec_of_base_template */


static a_boolean has_declspec_attributes(an_attribute_ptr ap)
/*
Returns TRUE if the specified attribute list (which may be NULL) has at least
one af_ms_declspec attribute.
*/
{
  a_boolean result = FALSE;

  for (; ap != NULL; ap = ap->next) {
    if (ap->family == af_ms_declspec) {
      result = TRUE;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* has_declspec_attributes */

static a_boolean class_specifier(
                           a_decl_parse_state          *dps,
                           a_decl_flag_set             dsi_flags,
                           a_boolean                   vacuous_decl_allowed,
                           a_boolean                   is_friend_decl,
                           ARG_UNUSED a_boolean        marked_as_gnu_extension,
                           a_type_ptr                  *type_ptr,
                           a_boolean                   *declares_something,
                           a_boolean                   *defines_something,
                           ARG_UNUSED a_decl_pos_block *decl_pos_block)
/*
Scan a class-specifier, which declares a class type (class/struct/union).  This
function also handles Microsoft __interface declarations (treated as a special
kind of struct).
*dps tracks information about the current declaration.  dsi_flags is the set
of input flags passed to decl_specifiers.  vacuous_decl_allowed is TRUE if the
class specifier can be immediately followed by a semicolon and not include a
definition.  is_friend_decl is TRUE if the "friend" keyword appeared before the
class specifier.  marked_as_gnu_extension is TRUE if the GNU __extension__
keyword is present.  The type is returned in *type_ptr. *declares_something is
set to indicate whether or not this specifier declares something, and
*defines_something to indicate whether the class/struct/union is actually
defined.  Detailed position information is recorded in *decl_pos_block.
*/
{
  a_symbol_kind           tag_kind = (a_symbol_kind)sk_class_or_struct_tag;
  a_type_kind             type_kind;
  a_symbol_locator        locator;
  a_symbol_ptr            tag_sym, error_tag_sym = NULL;
  a_symbol_ptr            parent_sym;
  a_boolean               tag_id_present = FALSE;
  a_type_ptr              class_type = NULL;
  a_boolean               is_local_class = FALSE, class_key_is_missing = FALSE;
  a_boolean               is_abstract = FALSE, is_final = FALSE,
                          is_sealed = FALSE;
  a_boolean               is_event_interface = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_boolean               is_interface = FALSE;
  an_assembly_visibility  cli_visibility = (an_assembly_visibility)av_none;
  a_cli_class_type_kind   cli_type_kind = (a_cli_class_type_kind)cctk_standard;
  a_source_position       cli_visibility_pos;        
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_boolean               is_template_class_instantiation = FALSE;
  a_boolean               tag_resolution = FALSE;
  a_boolean               tag_redefinition = FALSE;
  a_boolean               tag_is_newly_declared = FALSE;
  a_symbol_ptr            redefined_tag_sym = NULL;
  a_boolean               err = FALSE;
  a_scope_depth           effective_decl_level = decl_scope_level;
  a_boolean               is_class_definition, definition_removed;
  a_source_position       decl_start_pos;
  a_source_position       tag_position;
  a_symbol_reference_kind srk_flags;
  a_boolean               delayed_nested_class_def = FALSE;
  a_boolean               namespace_extension_pushed = FALSE;
  a_boolean               specialization_scope_adjusted = FALSE;
  a_boolean               is_redeclaration = FALSE;
  a_boolean               is_template_specific_decl = FALSE;
  a_boolean               is_predeclared_type_decl = FALSE;
  a_boolean               def_or_vacuous_decl;
  a_decl_pos_block        local_decl_pos_block;
  a_boolean		  previously_invisible = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED || SUN_EXTENSIONS_ALLOWED || \
    NEAR_AND_FAR_ALLOWED
  an_extended_decl_info_block
                          extended_decl_info;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || SUN_EXTENSIONS_ALLOWED || NEAR_... */
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED
  a_boolean               tag_name_access_checks_deferred = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED */
#if SUN_EXTENSIONS_ALLOWED
  a_source_position       pos_link_scope;
#endif /* SUN_EXTENSIONS_ALLOWED */
  a_boolean               is_ref_within_new_expr = 
                                      (dsi_flags & DSI_IS_NEW_TYPE_NAME) != 0;
  a_boolean               no_definition_allowed = 
                                     (dsi_flags & DSI_NO_TAG_DEFINITION) != 0;
  a_boolean               is_explicit_instantiation =
                             (dsi_flags & DSI_IS_EXPLICIT_INSTANTIATION) != 0;
  a_boolean               is_template_specialization =
                                     (dsi_flags & DSI_IS_SPECIALIZATION) != 0;
  /* When is_partial is TRUE, is_class_definition is TRUE to preserve behavior
     associated with parsing class bodies even though a partial class
     declaration is not a definition; however, at certain points, !is_partial
     checks prevent leaking the notion of a "definition" outside of
     class_specifier (e.g., SRK_DEFINITION is not set). */
  a_boolean               is_partial = FALSE;

  db_enter(3, "class_specifier");
  *declares_something = FALSE;
  *defines_something = FALSE;
  decl_start_pos = pos_curr_token;
  clear_decl_pos_block(&local_decl_pos_block);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  local_decl_pos_block.specifiers_range.start = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if MICROSOFT_EXTENSIONS_ALLOWED || SUN_EXTENSIONS_ALLOWED || \
    NEAR_AND_FAR_ALLOWED
  clear_extended_decl_info_block(extended_decl_info);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || SUN_EXTENSIONS_ALLOWED || NEAR_... */
  /* Determine whether this is a template class instantiation or a local
     class (one being declared within a function scope). */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cli_or_cx_enabled) {
    cli_visibility = scan_cli_visibility_specifier_if_any(&cli_visibility_pos);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Skip over "class", "struct", or "union", remembering which appears.
     In Microsoft modes, other possibilities exist (e.g., "__interface" or,
     in C++/CLI, "ref class"). */
  switch (curr_token) {
    case tok_struct:
      type_kind = (a_type_kind)tk_struct;
      break;
    case tok_class:
      type_kind = (a_type_kind)tk_class;
      break;
    case tok_union:
      tag_kind = (a_symbol_kind)sk_union_tag;
      type_kind = (a_type_kind)tk_union;
      break;
#if MICROSOFT_EXTENSIONS_ALLOWED
    case tok_interface:
      /* The Microsoft C++ "__interface" keyword (not to be confused with the
         C++/CLI "interface class" and "interface struct" keywords). */
      type_kind = (a_type_kind)tk_struct;
      is_interface = TRUE;
      is_event_interface = dps->has_event_keyword;
      break;
    case tok_value_struct:
      type_kind     = (a_type_kind)tk_struct;
      cli_type_kind = (a_cli_class_type_kind)cctk_value;
      break;
    case tok_value_class:
      type_kind     = (a_type_kind)tk_class;
      cli_type_kind = (a_cli_class_type_kind)cctk_value;
      break;
    case tok_ref_struct:
      type_kind     = (a_type_kind)tk_struct;
      cli_type_kind = (a_cli_class_type_kind)cctk_ref;
      break;
    case tok_ref_class:
      type_kind     = (a_type_kind)tk_class;
      cli_type_kind = (a_cli_class_type_kind)cctk_ref;
      break;
    case tok_partial_ref_struct:
      type_kind     = (a_type_kind)tk_struct;
      cli_type_kind = (a_cli_class_type_kind)cctk_ref;     
      is_partial = TRUE;
      break;
    case tok_partial_ref_class:
      type_kind     = (a_type_kind)tk_class;
      cli_type_kind = (a_cli_class_type_kind)cctk_ref;
      is_partial = TRUE;
      break;
    case tok_interface_struct:
      type_kind     = (a_type_kind)tk_struct;
      cli_type_kind = (a_cli_class_type_kind)cctk_interface;
      break;        
    case tok_interface_class:
      type_kind     = (a_type_kind)tk_class;
      cli_type_kind = (a_cli_class_type_kind)cctk_interface;
      break;        
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    default:
      /* No "class", "struct", "union", or similar keyword.  This only occurs
         with a friend class declaration like "friend A;". */
      /* class_specifier is called with is_friend_decl TRUE only when the name
         has not yet been declared; this happens in cfront compatibility mode
         only.  Default kind is "class" when a class is introduced by a friend
         declaration.   (In fact, there is a slight incompatibility here, since
         in cfront 2.1 this can also be turned into a union declaration.) */
      class_key_is_missing = TRUE;
      check_assertion(is_friend_decl &&
                      curr_token == tok_identifier &&
                      locator_for_curr_id.has_been_coalesced);
      tag_id_present = TRUE;
      tag_kind = (a_symbol_kind)sk_class_or_struct_tag;
      type_kind = (a_type_kind)tk_class;
  }  /* switch */
  if (!class_key_is_missing) {
    (void)get_token();
    dps->tag_attributes = scan_attributes(al_tag_name);
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
    if (ms_extensions or_near_and_far_enabled()) {
      /* Scan the decl-modifiers that apply to an entire class.  They will be
         passed on to scan_class_definition and applied to each member
         declaration, where appropriate. */
      scan_extended_decl_modifiers(&extended_decl_info, &dps->tag_attributes,
                                   al_tag_name, /*is_enum_decl=*/FALSE);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
#if SUN_EXTENSIONS_ALLOWED
    if (sun_linker_scope_allowed) {
      /* Scan and record any __hidden/__symbolic/__global tokens. */
      pos_link_scope = pos_curr_token;
      while (is_sun_link_scope_specifier()) {
        scan_link_scope_specifier(DSI_NO_INPUT_FLAGS,
                                  &extended_decl_info.decl_modifiers);
        (void)get_token();
      }  /* while */
    }  /* if */
#endif /* SUN_EXTENSIONS_ALLOWED */
    if (curr_token == tok_ellipsis) {
      /* A misplaced "..." -- issue an error and discard the token for better
         error recovery. */
      pos_error(ec_ellipsis_not_allowed, &pos_curr_token);
      (void)get_token();
    }  /* if */
    if (curr_token == tok_decltype) {
      /* A decltype could be decltype(x) or decltype(x)::something.  In the
         latter case we need to coalesce it before deciding what it is.  */
      (void)is_generalized_identifier_start(GID_NO_OPTIONS);
    }  /* if */
    /* If there is an identifier next, it is a tag.  It can be the declaration
       of a new tag or a reference to an existing tag.  Although it is an
       error, also be on the lookout for a qualified name. */
    tag_id_present = curr_token == tok_identifier ||
                     (curr_token == tok_colon_colon &&
                      next_token() == tok_identifier) ||
                     curr_token == tok_operator;        /* Error case. */
  }  /* if */
  if (tag_id_present) {
    /* It seems that appearance of a tag name is a declaration of the
       tag, even if it just repeats a previous name.  At least, there's
       a Plum Hall test that implies that. */
    tag_position = pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    local_decl_pos_block.identifier_range.start = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    *declares_something = TRUE;
    check_assertion(!vacuous_decl_allowed || !is_friend_decl);
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED
    if ((gpp_mode || (ms_extensions && !C_mode())) &&
        innermost_function_scope == NULL && !is_explicit_instantiation &&
        curr_deferred_access_scope != NO_SCOPE_DEPTH &&
        !scope_stack[curr_deferred_access_scope].defer_access_checks) {
      /* In GNU and Microsoft modes, the possibility of delayed nested class
         definitions in class scopes requires us to delay access checking
         until we know whether the tag name is part of such a definition.
         If we are already deferring access checks, do not set the following
         flag since that would cause the deferred checks to be checked too
         early. */
      tag_name_access_checks_deferred = TRUE;
      begin_deferral_of_access_checks();
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED */
    tag_sym = scan_tag_name(tag_kind, &locator, &is_friend_decl,
                            (dsi_flags & DSI_IS_SPECIALIZATION) != 0,
                            &vacuous_decl_allowed, is_ref_within_new_expr,
                            no_definition_allowed,
                            is_event_interface,
                            &effective_decl_level,
                            &tag_resolution, &tag_redefinition,
                            &is_predeclared_type_decl,
                            &local_decl_pos_block);
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED
    if (tag_name_access_checks_deferred) {
      if (microsoft_mode && microsoft_version <= 1310 &&
          is_friend_decl && locator.is_qualified_name) {
        /* Some Microsoft compilers appear not to perform access checks for
           qualified friend declarations.  E.g., the following is accepted:
             class C { class N; };  struct S { friend class C::N; }; */
        discard_deferred_access_checks();
      }  /* if */
      end_deferral_of_access_checks();
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED */
    if (tag_redefinition) {
      /* This declaration redefines a tag that is already defined in this
         scope.  Set the tag symbol aside and proceed as though the tag were
         being introduced here, so that the definition is scanned into a type
         of its own; once the two types have been compared, the tag continues
         to denote the type declared earlier. */
      redefined_tag_sym = tag_sym;
      tag_sym = NULL;
    }  /* if */
    if (tag_sym != NULL) {
      if (is_friend_decl) {
        /* A friend declaration: if the identifier was a qualified name
           (either namespace qualified or globally qualified), be sure the
           lookup did not find a namespace-projection symbol.  If it did,
           issue an error. */
        a_boolean  is_template = FALSE;
        check_friend_class_declaration(&locator, &tag_sym, &is_template);
#if MICROSOFT_EXTENSIONS_ALLOWED || SUN_EXTENSIONS_ALLOWED
        if (is_template) {
          /* In Microsoft and Sun modes a friend class declaration may refer
             to a template.  That case is full handled by the call to
             check_friend_class_declaration. */
          *type_ptr = NULL;
          goto done;
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || SUN_EXTENSIONS_ALLOWED */
      } else if ((is_explicit_instantiation || is_template_specialization) &&
                 !is_declarator_start()) {
        /* This is an explicit instantiation directive or a specialization
           of a class template.  Check for member templates from a base class
           that are specified using the derived class name as qualifier. */
        if (locator.is_class_member && locator.is_qualified_name &&
            locator.specific_symbol != NULL &&
            !gpp_mode && !microsoft_mode &&
            is_template_instance_class_symbol(locator.specific_symbol)) {
          a_type_ptr  qualifier = qualifier_class_type(locator);
          if (!same_entities(sym_parent_class(locator.specific_symbol),
                             qualifier)) {
            /* Specifying an inherited name in an explicit instantiation
               directive or in a template specialization declaration is
               disallowed. */
            pos_ty_error(ec_bad_qualifier_for_nested_class_decl,
                         &locator.source_position,
                         type_symbol_type(locator.specific_symbol));
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (tag_sym != NULL) {
      /* Store the symbol on the decl parse state so that we can retrieve
         the associated IL entity for modules. */
      dps->sym = tag_sym;
      /* Check for tag mismatch.  This can only happen when an instance of a
         class template is being referenced in an elaborated type specifier
         or in some GNU C++ and Cfront cases. */
      if (tag_sym->kind == (a_symbol_kind)sk_type) {
        if (tag_sym->is_template_param ||
            tag_sym->variant.type.ptr->kind ==
                                             (a_type_kind)tk_template_param) {
          /* Template param used in with a class-key -- for instance:
               template <class T> class A {
                 class T x;
               };
             During prototype instantiation we have to assume that T can be a
             valid class name.  Therefore "class T x" is treated as synonymous
             with "T x".  In addition, "friend class T" is also supported. */
          if ((is_friend_decl || tag_sym->is_class_member) &&
              !tag_sym->is_nonreal_nested_type) {
            /* If the form is similar to "struct T::X", we must preserve the
               elaborator (for disambiguation purposes).  So use a proxy
               class instead of the raw template parameter entity.  For a
               case like "friend class T;" we must also ensure that the
               returned symbol is a class type.  Don't do this if the template
               parameter is the nonreal version of a dependent nested class. */
	    a_type_ptr  proxy_type = proxy_class_for_template_param(
                                                   tag_sym->variant.type.ptr);
            proxy_type->kind = type_kind;
            tag_sym = symbol_for(proxy_type);
            tag_sym->kind = tag_kind;
          }  /* if */
#if CHECKING
        } else if (any_cfront_mode() || gpp_mode) {
          /* Cfront bug that allows this:
               typedef class A B;
               class B;
               class B *pa;
             The current declaration must not be a definition and the
             typedef name must refer to a class type.  GNU C++ allows a
             similar construct even in definitions. */
          check_assertion(is_class_struct_union_type(tag_sym->
                                                       variant.type.ptr));
        } else {
          internal_error("class_specifier: invalid sk_type tag_sym");
#endif /* CHECKING */
        }  /* if */
      } else if (tag_sym->kind != tag_kind) {
        /* Union/nonunion mismatch on a redeclaration. */
        if (is_nonreal_instance_class_symbol(tag_sym)) {
          /* Ignore a union/nonunion mismatch on nonreal classes. */
        } else if (is_template_class_symbol(tag_sym)) {
          /* Error -- tag-kind mismatch in a specialization. */
          pos_sy_error(ec_union_nonunion_mismatch, &decl_start_pos,
                       tag_sym->variant.class_struct_union.extra_info->
                                                            class_template);
          set_to_named_error_locator(locator);
          error_tag_sym = tag_sym;
          tag_sym = NULL;
        } else if (same_entity_as_a_type_info_type(type_symbol_type(tag_sym))){
          /* Error -- tag-kind mismatch for a type_info type. */
          pos_sy_error(ec_union_nonunion_mismatch, &decl_start_pos, tag_sym);
          set_to_named_error_locator(locator);
          error_tag_sym = tag_sym;
          tag_sym = NULL;
#if CHECKING
        } else {
          /* Mixing union and nonunion declarations is allowed in cfront
             mode.  A diagnostic will already have been issued. */
          check_assertion(any_cfront_mode());
#endif /* CHECKING */
        }  /* if */
      }  /* if */
    }  /* if */
    if (is_error_locator(locator)) err = TRUE;
  } else {
    /* No tag identifier present. */
    tag_sym = NULL;
    /* Don't leave tag_position undefined. */
    tag_position = decl_start_pos;
    set_to_error_locator(locator);
    if (is_ref_within_new_expr || no_definition_allowed) {
      /* If we are within a new expression and no class name is given following
         the keyword -- e.g., "class A *pa = new class;" -- report the missing
         identifier as a syntax error.  This applies to similar situations in
         trailing return types. */
      syntax_error(ec_exp_identifier);
      err = TRUE;
    } else if (curr_token == tok_lbrace ||
               (C_dialect == C_dialect_cplusplus && curr_token == tok_colon)) {
      /* This is a tagless class definition. */
#if GNU_EXTENSIONS_ALLOWED
      if (gpp_mode && gnu_version < 30300 &&
          type_kind == (a_type_kind)tk_class) {
        /* In some GNU C++ modes, unnamed class types defined with the "class"
           keyword are treated as if they were declared with the "struct"
           keyword.  This is true even if the unnamed type acquires a name for
           linkage purposes through a typedef. */
        type_kind = (a_type_kind)tk_struct;
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (cli_type_kind != (a_cli_class_type_kind)cctk_standard) {
        /* A C++/CLI managed class type must have a name. */
        pos_error(ec_unnamed_cli_managed_class_type, &pos_curr_token);
        err = TRUE;
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else {
      /* Neither the tag id nor the {...} is present.  This is an error. */
      if (scope_stack[depth_scope_stack].kind ==
                        (a_scope_kind)sck_template_declaration) {
        /* Inside a template-declaration scope -- don't issue a diagnostic
           that suggests a definition would be appropriate. */
        syntax_error(ec_exp_identifier);
      } else {
        add_stop_token(tok_lbrace);
        if (C_dialect == C_dialect_cplusplus) add_stop_token(tok_colon);
        syntax_error(ec_exp_definition_of_tag);
        if (C_dialect == C_dialect_cplusplus) remove_stop_token(tok_colon);
        remove_stop_token(tok_lbrace);
      }  /* if */
      err = TRUE;
    }  /* if */
  }  /* if */
  if (class_modifiers_allowed()) {
    /* Record any class modifiers indicated by the context-sensitive keywords
       "final", "abstract", or "sealed". */
#if MICROSOFT_EXTENSIONS_ALLOWED
    a_source_position  pos_after_name = pos_curr_token;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    scan_class_modifiers(type_kind, &is_final, &is_abstract, &is_sealed);
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (cli_type_kind == (a_cli_class_type_kind)cctk_value) {
      /* Value classes are implicitly sealed (i.e., "final"). */
      is_final = is_sealed = TRUE;
    } else if (cli_type_kind == (a_cli_class_type_kind)cctk_interface &&
               is_final) {
      pos_error(ec_sealed_cli_interface, &pos_after_name);
      is_final = is_sealed = FALSE;
    }  /* if */
    if (cli_type_kind != (a_cli_class_type_kind)cctk_standard &&
        is_final && !is_sealed) {
      /* Use of "final" instead of "sealed" on a managed class is an error. */
      pos_error(ec_final_managed_class, &pos_after_name);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  if (curr_token == tok_removed_template_body) {
    /* Presumably a nested class of a class template.  The definition was
       replaced by a placeholder token.  If the declaration is autonomous,
       ignore the placeholder token in what follows.  I.e., code like
         template<class T> struct S { struct N {}; };
       is treated like
         template<class T> struct S { struct N; };
       (during a real instantiation of S). */
    is_class_definition = FALSE;
    definition_removed = TRUE;
    if (next_token() == tok_semicolon) {
      /* We don't do a get_token at this point because that would change
         curr_token_sequence_number, whose precise value is needed later.
         The placeholder token is skipped at the end instead. */
      curr_token = tok_semicolon;
    }  /* if */
  } else {
    /* If the next token is a "{" or, in C++, a ":" (introducing a list of
       base classes) this is probably a class definition (but there are some
       exceptions). */
    is_class_definition = tag_definition_next(
                                 curr_token, tag_kind, is_ref_within_new_expr,
                                 no_definition_allowed);
    definition_removed = FALSE;
    if (is_class_definition && tag_sym != NULL &&
        is_proxy_class(type_symbol_type(tag_sym))) {
      /* A qualified class name could potentially result in an attempt to
         define a proxy class, which would lead to all kinds of surprises
         since lookup in a proxy class creates new members.  Catch this early
         and drop the proxy-class connection. */
      a_type_ptr  parent_type;
      check_assertion(locator.is_class_member);
      parent_type = locator.parent.class_type;
      if (parent_type->kind == (a_type_kind)tk_template_param &&
          parent_type->variant.template_param.extra_info
                     ->orig_nested_type != NULL) {
        /* The indicated nested type exists in the template, but in this
           context it cannot be defined. */
        pos_sy_error(ec_bad_scope_for_definition, &tag_position, tag_sym);
      } else {
        pos_stsy_error(ec_not_a_member_class, &tag_position,
                       locator.symbol_header->identifier,
                       symbol_for(locator.parent.class_type));
      }  /* if */
      tag_sym = NULL;
      set_to_named_error_locator(locator);
    }  /* if */
  }  /* if */
  if ((is_class_definition || definition_removed)) {
    /* Class definitions cannot appear as part of function declarations.
       There is therefore no need to keep caching tokens in case this turns
       out to be an abbreviated function template declaration. */
    end_potential_abbr_func_templ_caching(dps);
    if (constexpr_enabled && !relaxed_constexpr_enabled && !gpp_mode &&
        innermost_function_scope != NULL &&
        innermost_function_scope->variant.routine.ptr->is_constexpr) {
      pos_diagnostic(relaxed_constexpr_allowed() ? es_warning : es_error,
                     ec_tag_defined_in_constexpr_body, &decl_start_pos);
    }  /* if */
  }  /* if */
  if (is_class_definition && is_friend_decl) {
    /* This is an error.  Defer the diagnostic until we have a tag_sym
       to use for the fill-in.  If tag_sym is already non-NULL, we'll create
       another one. */
    if (gpp_mode && gnu_version < 30400 &&
        !locator.is_qualified_name && !locator.is_template_id) {
      /* GNU C++ accepts (and seems to ignore) friend specifiers on nested
         class definitions. */
      pos_warning(ec_friend_specifier_ignored, &decl_start_pos);
      is_friend_decl = FALSE;
    } else {
      set_to_named_error_locator(locator);
      tag_sym = NULL;
    }  /* if */
  }  /* if */
  if (tag_sym != NULL && C_dialect == C_dialect_cplusplus) {
    a_class_symbol_supplement_ptr	cssp;
    cssp = (tag_sym->kind == (a_symbol_kind)sk_type) ?
                        NULL : tag_sym->variant.class_struct_union.extra_info;
    class_type = type_symbol_type(tag_sym);
    if (tag_sym->kind == (a_symbol_kind)sk_type) {
      if (is_class_definition) {
        /* Attempting to redefine a template parameter name.  Let enter_symbol
           issue an error. */
        tag_sym = NULL;
      }  /* if */
    } else if (is_class_definition && tag_sym->defined
               if_microsoft_extensions(&& !is_partial_class(class_type))) {
      /* This class has already been defined.  If this is a template
         specialization declaration (and the entity has not already
         been defined as a specialization), indicate that the entity being
         specialized has already been referenced. */
      if (tag_sym->is_error) {
        /* This is a class generated because of an error situation.  Do not
           add an unhelpful diagnostic. */
        expect_error();
      } else if (is_template_specialization &&
                 !class_type->variant.class_struct_union.is_specialized) {
        pos2_sy_diagnostic(es_error,
                           ec_specialization_of_referenced_entity_pos,
                           &tag_position, &cssp->instantiation_position,
                           tag_sym);
      } else {
        issue_redef_diag(&tag_position, tag_sym);
      }  /* if */
      error_tag_sym = tag_sym;
      tag_sym = NULL;
      set_to_named_error_locator(locator);
      err = TRUE;
    } else {
      a_boolean  class_type_is_complete;
      class_type_is_complete = !is_incomplete_type(class_type);
      if (class_type->variant.class_struct_union.is_template_class) {
        /* A template class or a nested class within a template class. */
        if (is_template_specialization) {
          /* A specialization using the template<> syntax. */
          if (is_class_definition || curr_token == tok_semicolon) {
            check_assertion(!is_ref_within_new_expr);
            is_template_specific_decl = TRUE;
            if (class_type->variant.class_struct_union.is_specialized) {
              /* Redeclaration. */
              *declares_something = FALSE;
            } else {
              if (tag_sym->decl_scope !=
                                        scope_stack[decl_scope_level].number) {
                /* The explicit specialization appears outside the scope of
                   the template.  Check whether that is valid. */ 
                if (microsoft_mode && !tag_sym->is_class_member &&
                    tag_sym->parent.namespace_ptr == NULL &&
                    is_file_or_namespace_scope(&scope_stack_top())) {
                  /* Microsoft compilers allows a class template defined in
                     file scope to be specialized in a namespace.  E.g.:
                       template<class T> struct S {};
                       namespace N { template<> struct S<int> {}; }
                     Oddly, this does not work for a non-file-scope template:
                       namespace M {
                         template<class T> struct S {};
                         namespace N { template<> struct S<int> {}; }
                       }  // Microsoft does not accept this specialization.
                  */
                  effective_decl_level = DEPTH_OF_FILE_SCOPE;
                  specialization_scope_adjusted = TRUE;
                } else if (microsoft_mode &&
                           is_allowed_ms_spec_of_base_template(tag_sym)) {
                  /* In Microsoft mode, certain base class templates can be
                     specialized in a derived class. */
                } else if (sym_is_class_or_namespace_member(tag_sym) &&
                           (namespace_is_enclosed_by_curr_scope(tag_sym) ||
                            is_symbol_from_inline_namespace(tag_sym))) {
                  /* A specialization can always appear in a scope enclosing
                     the namespace scope of the template.  The
                     "is_symbol_from_inline_namespace" test is used to check
                     for specializations allowed in g++ mode when a
                     specializations in one namespace refers to an entity in
                     a namespace made visible by an inline namespace. */
                } else {
                  pos_sy_error(ec_bad_scope_for_specialization,
                               &tag_position, tag_sym);
                  tag_sym = NULL;
                  set_to_named_error_locator(locator);
                  is_template_specialization = FALSE;
                  err = TRUE;
                }  /* if */
              }  /* if */
              if (!class_type->variant.class_struct_union.is_specialized &&
                  tag_sym != NULL) {
                /* A specialization must first be declared in the namespace
                   containing the template. */
                check_specialization_scope(tag_sym, &tag_position);
              }  /* if */
              if (class_type->variant.class_struct_union.is_nonreal_class) {
                /* A specialization of a nonreal class.  This is usually the
                   result of a specialization in an invalid scope, in which
                   case an error will have already been issued.  This can
                   also occur in Microsoft mode where specializations are
                   allowed in class scopes. */
                /* The specialization that is in the prototype instantiation
		   of the enclosing class should itself be treated as a
		   prototype instantiation. */
                class_type->variant.class_struct_union.is_specialized = TRUE;
                if (is_prototype_instantiation_context()) {
                  class_type->variant.class_struct_union.
                                             is_prototype_instantiation = TRUE;
                }  /* if */
              } else if (class_type_is_complete && !err) {
                /* The class has already been instantiated and can't now
                   be specialized. */
                pos2_sy_diagnostic(es_error,
                                   ec_specialization_of_referenced_entity_pos,
                                   &tag_position,
                                   &cssp->instantiation_position, tag_sym);
              } else {
                class_type->variant.class_struct_union.is_specialized = TRUE;
                /* Set the referencing namespace to the namespace containing
                   the class.  This is needed in Microsoft mode when an
                   instantiation scope is pushed for specialized classes. */
                if (tag_sym != NULL) {
                  cssp->referencing_namespace =
                                          parent_namespace_for_symbol(tag_sym);
                }  /* if */
                if (instantiation_mode == tim_local) {
                  /* In tim_local mode generated instances have internal
                     linkage.  For specialized classes, the name linkage must
                     be reset. */
                  set_name_linkage_for_type(class_type);
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* if */
        } else if (is_class_definition &&
                   (tag_sym->is_class_member ||
                    is_nonreal_instance_class_symbol(tag_sym)) &&
                   !is_explicit_instantiation &&
                   cssp->class_template != NULL) {
          /* This is a definition of a member template instance -- apparently
             an attempt at old-style specialization, but only the "template<>"
             syntax is allowed for member template specializations.  We
             also go into this path for attempting to specialize certain kinds
             of nonreal classes. */
          if (depth_innermost_namespace_scope == depth_scope_stack) {
            /* A valid scope in which "template<>" can appear. */
            pos_sy_error(ec_old_specialization_not_allowed, &tag_position,
                         tag_sym);
          } else {
            /* Also an invalid scope. */
            pos_sy_error(ec_bad_scope_for_specialization, &tag_position,
                         tag_sym);
          }  /* if */            
          tag_sym = NULL;
          set_to_named_error_locator(locator);
          err = TRUE;
        } else if (is_class_definition ||
                   ((curr_token == tok_semicolon ||
                     curr_token == tok_removed_template_body) &&
                    !is_ref_within_new_expr && !dps->is_type_name &&
                    !is_friend_decl && !is_explicit_instantiation)) {
          /* We have a specific declaration of a template class. */
          if (tag_sym->decl_scope != scope_stack[depth_scope_stack].number &&
              !(microsoft_mode && !is_class_definition &&
                curr_token == tok_semicolon) &&
              (!sym_is_class_or_namespace_member(tag_sym) ||
               !namespace_is_enclosed_by_curr_scope(tag_sym))) {
            /* Explicit specializations of class templates must appear in the
               file or namespace scope in which the template was originally
               declared or in a scope enclosing the original scope.  In
               Microsoft mode, that restriction is not imposed if the
               specialization is not a definition.  A removed template body
               is treated as a definition. */
            pos_sy_error(ec_bad_scope_for_specialization,
                         &tag_position, tag_sym);
            tag_sym = NULL;
            set_to_named_error_locator(locator);
            err = TRUE;
          } else if (tag_sym->is_class_member &&
                     tag_sym->decl_scope ==
                                       scope_stack[depth_scope_stack].number) {
            /* This is a vacuous declaration of a nested class of a
               class template such as:
	         template <class T> struct A {
		   struct B;
		   struct B;
                 };
               Don't consider this to be a specialization. */
          } else if (class_type_is_complete) {
            /* The class has already been instantiated -- don't consider this
               to be a specialization. */
          } else {
            check_old_specialization_allowed(tag_sym, &tag_position);
            class_type->variant.class_struct_union.is_specialized = TRUE;
            class_type->variant.class_struct_union.
                                      specialized_with_old_syntax = TRUE;
            is_template_specific_decl = TRUE;
            /* Set the referencing namespace to the namespace containing
               the class.  This is needed in Microsoft mode when an
               instantiation scope is pushed for specialized classes. */
            cssp->referencing_namespace = parent_namespace_for_symbol(tag_sym);
            if (instantiation_mode == tim_local) {
              /* In tim_local mode generated instances have internal
                 linkage.  For specialized classes, the name linkage must
                 be reset. */
              set_name_linkage_for_type(class_type);
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (tag_sym != NULL) {
      if (!tag_sym->is_class_member) {
        if (is_class_definition) {
          /* This is a definition and a namespace-qualified name. */
          if (specialization_scope_adjusted) {
            /* This is a specialization whose effective scope has already
               been adjusted above.  Do not check is again. */
          } else if (!sym_is_namespace_member(tag_sym)) {
            if (tag_sym->decl_scope != scope_stack[decl_scope_level].number) {
              /* Unless a class is a namespace member or nested in another
                 class, it cannot be defined other than it the scope to which
                 it belongs. */
              pos_sy_error(ec_bad_scope_for_definition, &tag_position,
                           tag_sym);
              tag_sym = NULL;
              set_to_error_locator(locator);
            }  /* if */
          } else {
            /* The class being defined was originally declared a namespace
               member.  Determine (1) whether it's legal in this context and
               if so, (2) whether a scope stack entry needs to be pushed. */
            a_boolean  scope_err = FALSE;
            if (namespace_scope_should_be_pushed(
                                            tag_sym, &tag_position, &scope_err,
                                            /*inline_namespace=*/TRUE)) {
              /* Push a namespace extension scope. */
              push_namespace_extension_scope(sym_parent_namespace(tag_sym));
              namespace_extension_pushed = TRUE;
              effective_decl_level = depth_scope_stack;
            } else if (scope_err) {
              /* An error was issued by the subroutine. */
              tag_sym = NULL;
              set_to_error_locator(locator);
            }  /* if */
          }  /* if */
        }  /* if */
      } else {
        /* Nested class. */
        a_scope_stack_entry_ptr	ssep = &scope_stack[depth_scope_stack];
        if (ssep->kind == (a_scope_kind)sck_class_struct_union &&
            same_entities(sym_parent_class(tag_sym), ssep->assoc_type)) {
          /* Possible redeclaration of nested class name inside the body of
             the class of which it is a member. Note that if the elaborated
             type-specifier does not introduce a definition and is not
             followed by a semicolon, then it is not a redeclaration. */
          check_nested_class_redeclaration(
             dps, tag_sym, &tag_position, is_class_definition, is_friend_decl,
             (a_boolean)locator.is_qualified_name, declares_something);
        } else if (is_class_definition) {
          /* A definition of a nested class that appears in the scope other
             than that of its parent class. */
          /* Find the outermost enclosing class. */
          parent_sym = symbol_for(sym_parent_class(tag_sym));
          /* Find the outermost enclosing class. */
          while (parent_sym->is_class_member) {
            parent_sym = symbol_for(sym_parent_class(parent_sym));
          }  /* while */
          if (parent_sym->decl_scope == ssep->number) {
            /* Okay to define the nested class in this scope -- it is the
               scope in which the parent was defined. */
            delayed_nested_class_def = TRUE;
          } else if (sym_is_namespace_member(parent_sym) &&
                     namespace_is_enclosed_by_curr_scope(parent_sym)) {
            /* Also okay to define the nested class in this scope -- it is a
               a scope (namespace- or file-scope) enclosing the namespace
               scope in which the parent class was defined.  Handle this like
               the case where the parent class itself is defined in such an
               enclosing scope, so that we get the innermost namespace scope
               and the effective declaration level right.  Here's an example:
                 namespace NS1 {
                   namespace NS2 {
                     class A;
                     class B { class N; };
                   }
                 }
                 class NS1::NS2::A { class N; };   // Okay (handled above)
                 class NS1::NS2::A::N { };         // Okay (handled here)
                 class NS1::NS2::B::N { };         // Okay (handled here)
               Specifically, push the namespace extension scope. */
            push_namespace_extension_scope(sym_parent_namespace(parent_sym));
            namespace_extension_pushed = TRUE;
            effective_decl_level = depth_scope_stack;
            delayed_nested_class_def = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED
          } else if (delayed_nested_class_allowed_in_class(tag_sym)) {
            /* GNU and Microsoft C++ sometimes allow delayed nested class
               definitions in class scopes. */
            delayed_nested_class_def = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || GNU_EXTENSIONS_ALLOWED */
          } else {
            /* Template specializations will have been checked above. */
            if (!is_template_specialization) {
              pos_sy_error(ec_bad_scope_for_definition,
                           &tag_position, tag_sym);
              tag_sym = NULL;
              set_to_error_locator(locator);
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  def_or_vacuous_decl = (is_class_definition ||
                         definition_removed ||
                         (vacuous_decl_allowed &&
                          curr_token == tok_semicolon));
  if (tag_sym == NULL) {
    /* Create a new class, struct, or union type.  All such types are
       allocated in the file scope memory region, though local types will be
       added to the function scope's types list. */
    class_type = alloc_type(type_kind);
    if (depth_innermost_function_scope != NO_SCOPE_NUMBER ||
        inside_local_class) {
      /* This declaration appears within a function or block scope, or else it
         is a nested class declaration within a local class.  In either case,
         it is a local class. */
      is_local_class = TRUE;
      if (depth_innermost_function_scope != NO_SCOPE_NUMBER) {
        innermost_function_scope->variant.routine.ptr
                                ->contains_local_class_type = TRUE;
      }  /* if */
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* Indicate special Microsoft variations as appropriate (including C++/CLI
       managed class types). */
    if (is_interface) {
      if (is_local_class) {
        pos_error(ec_interface_cannot_be_local, &decl_start_pos);
      } else {
        class_type->variant.class_struct_union.is_interface = TRUE;
        class_type->variant.class_struct_union.abstract = TRUE;
      }  /* if */
    } else if (cli_type_kind != (a_cli_class_type_kind)cctk_standard) {
      if (is_local_class) {
        /* A C++/CLI managed class type cannot be local. */
        pos_error(ec_local_cli_managed_class_type, &tag_position);
        err = TRUE;
      }  /* if */
      class_type_supp(class_type)->is_hide_by_sig = TRUE;
      class_type_supp(class_type)->cli_class_type_kind = cli_type_kind;
      if (cli_type_kind == (a_cli_class_type_kind)cctk_interface) {
        class_type->variant.class_struct_union.abstract = TRUE;
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (scope_stack[effective_decl_level].kind ==
                                           (a_scope_kind)sck_func_prototype) {
      /* A type is actually declared in a function prototype scope only in
         C mode.  In C++ the type is injected into a containing scope (except
         in some nonstandard cases involving class definitions in a function
         prototype scope). */
      check_assertion(err || C_mode() || is_class_definition);
      class_type->declared_in_function_prototype = TRUE;
    }  /* if */
    if (C_dialect == C_dialect_cplusplus && error_tag_sym != NULL) {
      class_type->variant.class_struct_union.extra_info->template_arg_list =
             error_tag_sym->variant.class_struct_union.type->
                      variant.class_struct_union.extra_info->template_arg_list;
    }  /* if */
    /* Wait to add the type to the types list; it should not be added
       until the closing brace of the full definition appears, to get the
       IL list in the right order. */
    /* Enter a new tag symbol, if a tag id was specified (a tag is not
       specified in something like "struct {int a; int b;}"). */
    if (redefined_tag_sym != NULL) {
      /* A redefinition of a tag.  Give the type a symbol that carries the
         tag's name for the sake of diagnostics and of the comparison against
         the earlier definition.  Do not enter that symbol in the symbol table:
         The tag continues to denote the type declared earlier. */
      tag_sym = make_unentered_symbol(tag_kind, redefined_tag_sym->header,
                                      &locator.source_position);
      tag_sym->variant.class_struct_union.type = class_type;
      set_source_corresp(&(class_type->source_corresp), tag_sym);
      class_type->is_tag_redefinition = TRUE;
    } else if (tag_id_present) {
      tag_sym = enter_local_symbol(tag_kind, &locator, effective_decl_level,
                                   /*suppress_redecl_error=*/FALSE);
      tag_sym->variant.class_struct_union.type = class_type;
      set_source_corresp(&(class_type->source_corresp), tag_sym);
      tag_is_newly_declared = TRUE;
      if (is_friend_decl) {
        if (!friend_class_injection_enabled) {
          /* The name of a class first declared in a friend declaration is
             entered into the innermost non-class scope, but it's not visible
             to lookup. */
          tag_sym->is_invisible = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else if (microsoft_mode && is_local_class) {
          /* Microsoft compilers appear to make friend declarations invisible
             if they appear in a local class and normal lookup of the name
             finds something. */
          a_symbol_locator  friend_loc;
          friend_loc = locator;
          clear_specific_symbol(friend_loc);
          tag_sym->is_invisible = TRUE;
          if (normal_id_lookup(&friend_loc,
                               IDL_SKIP_CLASS_SCOPES |
                               IDL_DO_NOT_ADD_TO_NONREAL_CLASS |
                               IDL_DO_NOT_CREATE_PROJ_SYM) == NULL) {
            tag_sym->is_invisible = FALSE;
          }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        }  /* if */
      }  /* if */
      if (locator.is_error) {
        /* Some error occurred earlier.  Make sure we don't try to treat this
           as a specialization because the error class that is created does not
           look like a template class. */
        is_template_specialization = FALSE;
        err = TRUE;
      }  /* if */
    } else {
      /* Tagless class, struct, or union.  Create a symbol to represent it;
         though not entered in the symbol table, it is needed to carry
         around some information about classes that is of interest to the
         front end only. */
      tag_sym = make_unnamed_tag_symbol(tag_kind, &pos_curr_token);
      tag_sym->variant.class_struct_union.type = class_type;
      /* Although the symbol header has a name of sorts, it should not appear
         in the type, so NULL it out after the call to set_source_corresp. */
      set_source_corresp(&(class_type->source_corresp), tag_sym);
      clear_source_corresp_name(&class_type->source_corresp);
      class_type->variant.class_struct_union.originally_unnamed = TRUE;
    }  /* if */
#if NEED_NAME_MANGLING
    /* The mangled names of local types and unnamed types in namespace scope
       are distinguished using a unique number ("discriminator").  Compute this
       number now if appropriate.  The notion of "discriminator" here is a
       generalization of the one defined in the IA-64 ABI. */
    compute_name_collision_discriminator(tag_sym, effective_decl_level);
#endif /* NEED_NAME_MANGLING */
    if (is_friend_decl && is_class_definition) {
      /* Issuing the diagnostic was deferred till now. */
      pos_sy_error(ec_bad_scope_for_definition, &tag_position, tag_sym);
      err = TRUE;
    }  /* if */
    /* Set parent class or namespace pointers, if appropriate, and adjust
       related fields (e.g., name linkage). */
    update_membership_of_class(tag_sym, def_or_vacuous_decl,
                               is_event_interface, effective_decl_level,
                               &decl_start_pos);
    if (is_friend_decl && tag_id_present &&
        secondary_translation_unit_seen()) {
      /* This class type entry might have been generated during the
         instantiation of another template.  The correspondence checking
         process must therefore be notified of its existence. */
      establish_friend_type_correspondence(class_type);
    }  /* if */
    if (depth_innermost_instantiation_scope != NO_SCOPE_DEPTH &&
        tag_sym->is_class_member) {
      /* Determine whether this is a reference to a nested class within
         a class template.  If so, set the correspondence with the
         corresponding prototype class. */
      set_nested_template_class_symbol_info(tag_sym, type_kind);
    }  /* if */
    srk_flags = SRK_DECLARATION;
    if (is_class_definition && !is_partial) srk_flags |= SRK_DEFINITION;
    if (is_friend_decl) srk_flags |= SRK_FRIEND;
    if (cpp11_mode && !microsoft_mode &&
        scope_stack[depth_innermost_namespace_scope].within_unnamed_namespace){
      /* Starting with C++11 unnamed namespaces and their members have internal
         linkage. */
      class_type->source_corresp.name_linkage =
                                             (a_name_linkage_kind)nlk_internal;
    }  /* if */
    record_symbol_declaration(srk_flags, tag_sym, &locator.source_position,
                              (a_source_sequence_entry_ptr)NULL);
    /* If this declaration is associated with a declaration statement, update
       the associated stmk_decl statement. */
    record_entity_in_decl_stmt_if_needed(tag_sym);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    update_sse_for_first_tag_declaration(class_type, &locator,
                                         is_class_definition,
                                         marked_as_gnu_extension);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  } else if (tag_sym->kind == (a_symbol_kind)sk_type) {
    if (tag_sym->variant.type.ptr->kind == (a_type_kind)tk_template_param) {
      /* Use of template parameter name as a proxy tag name during a
         prototype instantiation. */
    } else {
      mark_referenced(tag_sym, &locator.source_position);
      *declares_something = FALSE;
    }  /* if */
  } else {
    a_class_type_supplement_ptr  ctsp;
    /* Using an existing type.  Fetch the type pointer from it. */
    class_type = tag_sym->variant.class_struct_union.type;
    ctsp = class_type_supp(class_type);
    is_local_class = class_type->source_corresp.is_local_to_function;
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (ms_extensions) {
      if (is_interface !=
                        class_type->variant.class_struct_union.is_interface) {
        /* Diagnose inconsistent use of the "__interface" keyword. */
        check_interface_redeclaration(tag_sym, tag_kind, &is_interface,
                                      is_class_definition, &tag_position);
      }  /* if */
      if (cli_type_kind != ctsp->cli_class_type_kind) {
        /* Diagnose inconsistent C++/CLI class type kinds, except if the class
           is imported from metadata and the current code is not from
           metadata. */
        if (class_is_from_metadata(class_type) &&
            cli_type_kind == (a_cli_class_type_kind)cctk_standard &&
            !scanning_generated_code_from_metadata) {
          /* The Microsoft compiler accepts e.g. "class System::ValueType". */
        } else {
          pos_sy_error(ec_conflicting_cli_class_type_kinds, &tag_position,
                       tag_sym);
        }  /* if */
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (!is_template_specific_decl || !(*declares_something)) {
      dps->redeclares_tag = TRUE;
      is_redeclaration = TRUE;
    }  /* if */
    if (!friend_class_injection_enabled && !is_friend_decl) {
      /* In case the previous declaration was a friend declaration, ensure
         that the symbol is henceforth visible for lookup. */
      previously_invisible = tag_sym->is_invisible;
      tag_sym->is_invisible = FALSE;
    }  /* if */
    if (is_class_definition) {
      /* Allow for alternating between class and struct, but stay with the
         one associated with the definition.  The difference only affects
         default member access. */
      class_type->kind = type_kind;
      /* If this is a nested class of a class template, update the type kind
         associated with the template. */
      if (depth_innermost_instantiation_scope != NO_SCOPE_DEPTH &&
          !is_template_specialization && !tag_sym->is_error) {
        update_nested_template_class_symbol_info(tag_sym, type_kind);
      }  /* if */
      if (strict_ansi_mode && locator.is_qualified_name &&
          !(is_explicit_instantiation || is_template_specialization)) {
        /* The C++ standard was revised to restrict the kind of qualifiers
           used for class definitions.  Explicit instantiations and
           explicit specializations are diagnosed elsewhere. */
        check_name_used_for_qualified_class_definition(&locator, tag_sym);
      }  /* if */
    }  /* if */
    /* Record cross-reference information. */
    if (!is_friend_decl && !locator.is_template_id &&
        is_file_or_namespace_scope(&scope_stack[depth_scope_stack]) &&
        class_type->variant.class_struct_union.is_prototype_instantiation &&
        ctsp->template_arg_list != NULL &&
        !(!scope_is_null_or_placeholder(ctsp->assoc_scope) &&
          ctsp->assoc_scope->depth_in_scope_stack != NO_SCOPE_DEPTH)) {
      /* A prototype instantiation of a class template (as opposed of that of
         a class nested in a class template).
         This can only happen in the emulation of a peculiar Microsoft and Sun
         bug that causes the following to be accepted:
           template<class T> struct S;
           struct S; // ignored (scan_tag_name returns the prototype
                     //          instantiation)
         Such "redeclarations" are ignored (i.e., not recorded).
      */
      check_assertion(sun_mode ||
                      (microsoft_bugs && microsoft_version < 1400));
    } else if (is_class_definition || is_predeclared_type_decl ||
               ((curr_token == tok_semicolon ||
                 curr_token == tok_removed_template_body) &&
                (vacuous_decl_allowed || is_friend_decl ||
                 is_template_specific_decl || dps->tag_attributes != NULL)) ||
               previously_invisible) {
      /* Vacuous declarations are not usually permitted to use qualified names
         (even so, the declarations are accepted with warning in nonstrict
         modes).  Exceptions are friend declarations and template
         specialization declarations.  We also treat vacuous declarations with
         attributes as actual declarations since the attributes may have a
         significant declarative effect.
         If an elaborated type specifier makes visible a class name that was
         previously invisible (e.g., the first declaration was a friend), we
         we treat the elaborated type specifier as a declaration. */
      srk_flags = SRK_DECLARATION;
      if (is_friend_decl) srk_flags |= SRK_FRIEND;
      if (is_class_definition) {
        if (!is_template_class_instantiation && !is_partial) {
          srk_flags |= SRK_DEFINITION;
        }  /* if */
      } else {
        /* A declaration of the form "class A;", when A has already been
           declared, is treated as a redeclaration (not a reference). */
        if (locator.is_qualified_name && !is_friend_decl &&
            !is_explicit_instantiation && !is_template_specialization &&
            curr_token == tok_semicolon &&
            (dps->tag_attributes == NULL ||
             !has_declspec_attributes(dps->tag_attributes))) {
          /* When an elaborated type specifier is the sole constituent of a
             declaration (except for explicit specializations or explicit
             instantiations, or certain friend declarations), it cannot have a
             qualified name (e.g., "class ::A;"). */
          /* Declarations with Microsoft declspec attributes (e.g.,
             "struct __declspec(dllimport) N::S;") are treated as
             redeclarations (without a diagnostic). */
          pos_diagnostic((strict_ansi_mode || clang_mode) ? es_error :
                                                            es_warning,
                         ec_qualified_name_not_allowed,
                         &locator.source_position);
        }  /* if */
      }  /* if */
      record_symbol_declaration(srk_flags, tag_sym, &locator.source_position,
                                (a_source_sequence_entry_ptr)NULL);
      /* If this declaration is associated with a declaration statement,
         update the associated stmk_decl statement. */
      record_entity_in_decl_stmt_if_needed(tag_sym);
#if GENERATE_SOURCE_SEQUENCE_LISTS
      if ((is_predeclared_type_decl || previously_invisible) &&
          !is_class_definition) {
        /* This is the first explicit declaration of a predeclared type,
           or this is the first visible declaration of a previously
           invisible symbol.  Set the first_declaration flag in the
           associated source-sequence secondary declaration entry. */
        an_sssd_flag_set      flags = SSSD_FIRST_DECLARATION;
        a_name_reference_ptr  name_ref = NULL;
        if (record_name_references_in_context()) {
          name_ref = qualifiable_name_reference(&locator,
                                                &class_type->source_corresp);
        }  /* if */
        if (marked_as_gnu_extension) {
          flags |= SSSD_MARKED_AS_GNU_EXTENSION;
        }  /* if */
        (void)set_src_seq_secondary_decl_fields(
                                         (char *)class_type, (a_type_ptr)NULL,
                                         name_ref, flags);
      }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    } else {
      /* Not a definition, not a vacuous declaration, so presumably a
         reference. */
      mark_referenced(tag_sym, &locator.source_position);
      *declares_something = FALSE;
    }  /* if */
  }  /* if */
  dps->tag_def_or_forward_decl = is_class_definition ||
                                 curr_token == tok_semicolon;
  if (tag_sym->kind != (a_symbol_kind)sk_type &&
      !is_redeclaration && !is_template_specific_decl) {
    /* This is the initial declaration of this class type. */
    if (is_friend_decl && prototype_instantiations_in_il &&
        scope_stack[effective_decl_level].kind ==
                                   (a_scope_kind)sck_template_instantiation &&
        !is_template_dependent_type(class_type)) {
      /* Nondependent types named in friend class declarations during a
         prototype instantiation are associated with two symbols: one in
         the template instantiation scope (visible) and one in the surrounding
         namespace scope (invisible). */
      duplicate_friend_sym_in_namespace(&effective_decl_level, &tag_sym);
    }  /* if */
    if (may_be_added_to_types_list(class_type, effective_decl_level)) {
      add_to_types_list(class_type, effective_decl_level);
    }  /* if */
    if (is_local_class &&
        scope_stack[effective_decl_level].in_prototype_instantiation) {
      /* Mark a local class of a prototype instantiation as nonreal.
         Otherwise, an instantiation over that type can end up on the list of
         file scope types even when prototype instantiations are not recorded
         in the IL. */
      class_type->variant.class_struct_union.is_nonreal_class = TRUE;
    }  /* if */
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cli_or_cx_enabled) {
    set_cli_visibility(class_type, cli_visibility, &cli_visibility_pos,
                       (is_class_definition || definition_removed) &&
                       !is_partial);
    if (cppcx_enabled && (is_class_definition || definition_removed)) {
      error_if_cppcx_public_global_type(class_type, &cli_visibility_pos);
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (err ||
      (depth_template_declaration_scope != NO_SCOPE_DEPTH &&
       scope_stack[depth_scope_stack].kind ==
                                      (a_scope_kind)sck_class_struct_union)) {
    /* Pragma processing may run into invalid scopes.  So discard them. */
    discard_curr_construct_pragmas();
  } else if (is_immediate_class_type(class_type) &&
             class_type->variant.class_struct_union.is_nonreal_class &&
             !prototype_instantiations_in_il &&
             depth_innermost_function_scope == NO_SCOPE_DEPTH) {
    /* A pragma applied to a nonreal class.  This can result from Microsoft
       mode nonreal instantiations.  For things outside of functions, discard
       the pragmas if we are not recording prototype instantiations in IL. */
    discard_curr_construct_pragmas();
  } else if (is_class_definition || curr_token == tok_semicolon) {
    /* Do processing required for any pragmas that are bound to the current
       declaration. */
    process_curr_construct_pragmas(tag_sym, (a_statement_ptr)NULL);
  }  /* if */
  /* Now that we have a type, we can apply any attributes attached to it. */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ms_extensions) {
    preapply_microsoft_class_align_attribute(dps, is_class_definition ||
                                                  definition_removed);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (dps->tag_attributes != NULL) {
    a_boolean         ignore_gnu_attributes = FALSE;
    an_attribute_ptr  attributes_to_attach = dps->tag_attributes;
    if (gnu_version_is(< 40200)) {
      /* In some GNU modes, attributes appearing between the class/struct/union
         keyword and the type name are ignored if the elaborated name specifier
         is not followed by a class type definition and if this is not an
         explicit class template instantiation directive. */
      a_boolean  definition_follows = is_class_definition;
      if (!definition_follows && tag_sym != NULL && tag_sym->is_class_member &&
          !is_explicit_instantiation) {
        /* We may be dealing with a class nested in a class template.  E.g.:
             template<class T> struct S { class __attribute((...)) N {}; };
           The nested declaration will look like "class __attribute((...)) N;"
           in such cases (with the definition being processed after the
           enclosing class is completed). */
        a_symbol_ptr  proto_sym = corresp_prototype_for_class_symbol(tag_sym);
        definition_follows = (proto_sym != NULL && proto_sym->defined);
      }  /* if */
      if (!definition_follows &&
          (!is_explicit_instantiation || is_declarator_start())) {
        ignore_gnu_attributes = TRUE;
      }  /* if */
    }  /* if */
    if (is_explicit_instantiation) {
      /* Make a copy of the attributes recorded in dps->tag_attributes; the
         original entries will be recorded in an instantiation directive entry
         later on. */
      attributes_to_attach = copy_of_attributes_list(dps->tag_attributes);
      if (std_attributes_enabled) {
        diagnose_std_attribute_on_explicit_instantiation(attributes_to_attach);
      }  /* if */
    }  /* if */
    attach_tag_attributes(attributes_to_attach, class_type, dps,
                          (is_class_definition || definition_removed),
                          curr_token == tok_semicolon, ignore_gnu_attributes);
#if MICROSOFT_EXTENSIONS_ALLOWED
    /* The call to attach_tag_attributes does not directly apply DLL
       attributes.  Instead, the call to update_extended_decl_info_for_class
       does that below.  Set the required flags in extended_decl_info. */
    add_flags_from_dll_attributes(&extended_decl_info.decl_modifiers.flags,
                                  attributes_to_attach);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if MAINTAIN_NEEDED_FLAGS
    if (is_class_definition) {
      /* Some attributes are only valid on class definitions.  Therefore, if
         attributes appear on a class definition, do not let elimination of
         unneeded entities remove that definition. */
      set_class_keep_definition_in_il(class_type);
    }  /* if */
#endif /* MAINTAIN_NEEDED_FLAGS */
  }  /* if */
  /* If any declaration or definition of this class contains a standard
     alignment attribute, process it. */
  record_strongest_alignment_attr(dps, iek_type, &class_type->source_corresp,
                                  !is_incomplete_type(class_type),
                                  is_class_definition);
  /* If the current token marks a removed template body, skip past that
     special token. */
  if (definition_removed) (void)get_token();
#if MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED
  if ((ms_extensions or_near_and_far_enabled()) &&
      tag_sym->kind != (a_symbol_kind)sk_type) {
    update_extended_decl_info_for_class(class_type, &extended_decl_info,
                                        is_explicit_instantiation,
                                        &locator.source_position);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || NEAR_AND_FAR_ALLOWED */
  if (is_final || is_abstract) {
    apply_class_modifiers(class_type, is_final, is_abstract, is_sealed);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ms_extensions && is_immediate_class_type(class_type)) {
    if (dps->ms_attributes != NULL && !is_local_class) {
      if (!is_class_definition && curr_token != tok_semicolon) {
        /* This is a non-autonomous declaration of the class: The attributes
           do not apply to the class type, but to the entity associated with
           the declarator. */
      } else {
        /* Check for the presence of the "attribute" attribute and mark the
           class accordingly.  This has to be done early because it causes the
           implicit addition of System::Attribute as a base class if needed. */
        an_ms_attribute_ptr  msap = dps->ms_attributes;
        for (; msap != NULL; msap = msap->next) {
          if (msap->is_attribute_attribute) {
            class_type_supp(class_type)->is_cli_attribute = TRUE;
            break;
          }  /* if */
        }  /* for */
        apply_microsoft_attributes_to_type(&dps->ms_attributes, class_type);
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  if (gnu_mode) {
    if (!C_mode() && is_immediate_class_type(class_type)) {
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
      /* If no ELF visibility was explicitly specified, use that of the
         enclosing class or namespace (if any). */
      a_class_type_supplement_ptr  ctsp = class_type_supp(class_type);
      an_ELF_visibility_kind       visibility =
                       enum_cast<an_ELF_visibility_kind>(ctsp->ELF_visibility);
      update_for_default_ELF_visibility(
                     &visibility, class_type->source_corresp.is_class_member);
      ctsp->ELF_visibility = visibility;
#endif /* GNU_VISIBILITY_ATTRIBUTE_ALLOWED */
      if (!class_type->variant.class_struct_union.is_template_class) {
        /* Record whether or not the class has an inline namespace with an
           abi_tag as a parent (in the template case, this has already been
           set from the prototype template). */
        class_type->in_gnu_abi_tag_namespace =
                                    scope_stack_top().in_gnu_abi_tag_namespace;
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if SUN_EXTENSIONS_ALLOWED
  if (sun_linker_scope_allowed) {
    a_decl_modifier_set link_scope = extended_decl_info.decl_modifiers.flags &
                                     DM_ANY_SUN_LINK_SCOPE;
    if (link_scope != 0) {
      record_sun_link_scope_for_class(class_type, link_scope, &pos_link_scope);
    }  /* if */
  }  /* if */
#endif /* SUN_EXTENSIONS_ALLOWED */
  if (is_class_definition) {
    if (scan_class_definition(class_type, dps, effective_decl_level,
                              is_partial,
                              is_local_class, delayed_nested_class_def,
                              /*is_template_instantiation=*/FALSE,
                              is_template_specialization,
                              (a_template_ptr)NULL,
                              &local_decl_pos_block)) {
      if (!is_partial) *defines_something = TRUE;
    } else {
      err = TRUE;
    }  /* if */
    /* If necessary, pop the namespace extension scope. */
    if (namespace_extension_pushed) pop_namespace_extension_scope();
    /* If there are no longer any classes in the process of being defined
       do any class fixups and template instantiations that have been
       deferred.  (Note that this has to be done after the namespace
       extension scope is popped to handle source-sequence insertion for
       templates correctly.)  At this point we should not be in a template
       declaration scope, unless an earlier syntax error caused us to confuse
       the intended construct; in that case a diagnostic has been or will be
       issued elsewhere.  Normally there should not be a function scope
       nested in a template declaration scope, but this can sometimes occur
       in error cases.   In such cases, do the fixup in case the function
       contained a local class. */
    if ((depth_template_declaration_scope == NO_SCOPE_DEPTH ||
         depth_innermost_function_scope > depth_template_declaration_scope) &&
        !(microsoft_bugs &&
          dps->declared_storage_class == (a_storage_class)sc_typedef)) {
      /* In Microsoft bugs mode, the typedef is processed before member
         function bodies etc. are rescanned.  This makes e.g. the following
         legal:
            typedef struct {
              enum { e };
              void f() { S::e; }
            } S;
         Hence, in that mode the following call will be made after the call
         to decl_typedef.
      */
      process_deferred_class_fixups_and_instantiations(
                                                  /*for_instantiation=*/FALSE);
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS
    check_for_and_remove_redundant_secondary_decl_ss_entry(class_type);
#endif /* CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    if (inside_statement_expression() && !C_mode() &&
        !is_template_class_type(class_type) &&
        is_nonPOD_or_has_nontrivial_copy_semantics(class_type)) {
      /* Non-POD class definitions (classes with non-trivial
         copy/initialization semantics in modern C++) are not allowed
         inside statement expressions.  However, if class_type is a template
         class, this is not a definition in the statement expression but an
         instantiation triggered during the parsing of the statement expression
         (which is fine). */
      pos_error(ec_class_def_in_statement_expr, &decl_start_pos);
    }  /* if */
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ms_extensions && !C_mode() && tag_sym->kind != (a_symbol_kind)sk_type) {
    /* If the class has been defined, issue an error if the inheritance kind
       (if any) for the class is too restrictive. */
    a_class_type_supplement_ptr ctsp = class_type_supp(class_type);
    if (extended_decl_info.inheritance_kind != (an_inheritance_kind)ihk_none) {
      if (extended_decl_info.inheritance_kind != ctsp->inheritance_kind) {
        /* An error will already have been issued -- no need for another. */
      } else if (is_incomplete_type(class_type)) {
        /* The class hasn't been defined yet, so there's no way to check. */
      } else {
        /* Be sure the inheritance kind specified on the current declaration
           is not "too restrictive" for the actual characteristics of the
           class. */
        check_inheritance_kind(class_type, extended_decl_info.inheritance_kind,
                               &extended_decl_info.inheritance_kind_pos);
      }  /* if */
    } else if (is_class_definition) {
      /* There was no explicit specification of an inheritance kind on the
         class, but there may have been on a previous declaration, or the
         default inheritance kind may have been assigned.  In either case,
         now that we have a definition, determine whether the preestablished
         inheritance kind is appropriate. */
      if (ctsp->inheritance_kind != (an_inheritance_kind)ihk_none) {
        check_inheritance_kind(class_type, ctsp->inheritance_kind,
                               &locator.source_position);
      }  /* if */
    }  /* if */
  }  /* if */
  if (cppcx_enabled) {
    if (is_partial &&
        (class_type->source_corresp.is_class_member || !def_or_vacuous_decl)) {
      /* The membership of a nested class has been set and def_or_vacuous_decl
         indicates whether the class_type was just declared (or defined) as
         part of this specifier.  Ensure that a partial class is not nested in
         C++/CX or is part of a class specifier that is not a declaration nor
         definition. */
      pos_error(ec_partial_class_incorrect_type_or_location,
                &decl_start_pos);
    } else if (def_or_vacuous_decl &&
               class_type->source_corresp.is_class_member &&
               is_cppcx_externally_visible_symbol(symbol_for(class_type)) &&
               !in_code_generated_from_metadata()) {
      /* Externally visible nested types are not allowed in C++/CX. */
      /* Platform::String has a native nested type, so don't enforce this
         check in code generated from metadata. */
      pos_error(ec_public_nested_type_in_cppcx_type, &decl_start_pos);
    } else if (def_or_vacuous_decl &&
               !is_managed_class_type(class_type) &&
               is_cppcx_externally_visible_symbol(symbol_for(class_type))) {
      /* Externally visible native types are not allowed in C++/CX. */
      pos_error(ec_cppcx_public_native_type, &decl_start_pos);
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    /* Copy the end specifiers end position into decl_pos_block.  There are
       potentially two declarations here -- e.g.,
         const struct S { ... } *ps;
       where both "S" and "ps" are declared (and where *decl_pos_block
       belongs to the declaration of "ps" and local_decl_pos_block belongs
       to the declaration of "S").  Note that the specifiers ranges start at 
       different positions but end at the same position. */
    decl_pos_block->specifiers_range.end =
                             local_decl_pos_block.specifiers_range.end;
  }  /* if */
  if (is_class_definition ||
      (!is_redeclaration && tag_sym->kind != (a_symbol_kind)sk_type)) {
    /* If this is the initial or defining declaration of the class, update
       the extra source information for the class type. */
    a_decl_position_supplement_ptr  dpsp = class_type->
                                               source_corresp.decl_pos_info;
    if (dpsp != NULL) {
      dpsp->specifiers_range = local_decl_pos_block.specifiers_range;
      if (tag_id_present) {
        dpsp->identifier_range = local_decl_pos_block.identifier_range;
      }  /* if */
#if DEBUG
/* GCOVR_EXCL_START */
      if (delayed_nested_class_def) {
        if (debug_level >= 3 || db_flag_is_set("dump_decl_pos_info")) {
          fprintf(f_debug, "decl-pos info for delayed nested class def\n");
          db_decl_pos_info(tag_sym);
        }  /* if */
      }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
    }  /* if */
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (!is_class_definition && *declares_something &&
      tag_sym->kind != (a_symbol_kind)sk_type) {
    /* Update source range information in the secondary-decl entry. */
    a_source_sequence_entry_ptr     class_ssep;
    a_src_seq_secondary_decl_ptr    sssdp;
    a_decl_position_supplement_ptr  dpsp;

    class_ssep = last_matching_source_sequence_entry((char *)class_type);
    if (class_ssep != NULL &&
        ss_entry_kind(class_ssep) == iek_src_seq_secondary_decl) {
      sssdp = (a_src_seq_secondary_decl_ptr)class_ssep->entity.ptr;
      if (sssdp->decl_pos_info == NULL) {
        dpsp = alloc_decl_position_supplement(in_file_scope(sssdp));
        dpsp->specifiers_range = local_decl_pos_block.specifiers_range;
        if (is_friend_decl) {
          /* If this is a friend declaration, adjust the specifiers range to
             include "friend". */
          check_assertion(decl_pos_block != NULL);
          dpsp->specifiers_range.start =
                                   decl_pos_block->specifiers_range.start;
        }  /* if */
        if (tag_id_present) {
          dpsp->identifier_range = local_decl_pos_block.identifier_range;
        }  /* if */
        sssdp->decl_pos_info = dpsp;
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (redefined_tag_sym != NULL) {
    /* Every definition of a tag must define the same type.  Check that, then
       let the specifier yield the type the tag already denotes.  That ensures
       the rest of the front end sees just one type for the tag. */
    a_type_ptr  prev_class_type = type_symbol_type(redefined_tag_sym);
    if (!err &&
        !c_tagged_types_match(class_type, prev_class_type,
                              ttmk_redeclaration)) {
      pos_diagnostic(es_error, ec_tag_redefined_differently, &tag_position,
                     redefined_tag_sym,
                     &prev_class_type->source_corresp.decl_position);
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    dps->c23_tag_redefinition_type = class_type;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    rebind_c23_redefinition_pragmas(class_type, prev_class_type);
    class_type = prev_class_type;
    tag_sym = redefined_tag_sym;
  } else if (c23_mode && is_class_definition && !err &&
             tag_is_newly_declared && !class_type->incomplete) {
    /* C23 makes this type compatible with any type declared with the same tag
       in another scope of this translation unit whose members correspond to
       this one's.  Compatible types are interchangeable, so let this tag
       denote the type declared earlier: The rest of the front end then sees
       one type for the whole set of compatible declarations.  A definition
       that completes a type declared earlier in the same scope is left
       alone because that earlier type may already have been used in
       declarations that must keep denoting it.  A generating back end must
       still render a local definition when the type it denotes was defined
       in a scope that is not visible here. */
    a_type_ptr  prev_class_type = compatible_c_tagged_type(class_type);
    if (prev_class_type != NULL) {
      a_symbol_ptr  dup_sym = make_unentered_symbol(tag_kind, tag_sym->header,
                                                    &locator.source_position);
      dup_sym->variant.class_struct_union.type = class_type;
      set_source_corresp(&(class_type->source_corresp), dup_sym);
      class_type->is_tag_redefinition = TRUE;
      tag_sym->variant.class_struct_union.type = prev_class_type;
#if GENERATE_SOURCE_SEQUENCE_LISTS
      dps->c23_tag_redefinition_type = class_type;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      rebind_c23_redefinition_pragmas(class_type, prev_class_type);
      class_type = prev_class_type;
    }  /* if */
  }  /* if */
  if (err) {
    *type_ptr = error_type();
  } else if (tag_sym->kind == (a_symbol_kind)sk_type) {
    *type_ptr = tag_sym->variant.type.ptr;
  } else {
    *type_ptr = class_type;
#if DEFAULT_RECORD_FORM_OF_NAME_REFERENCE
    if (record_form_of_name_reference) {
      *type_ptr = make_typeref_with_lexical_information(*type_ptr, &locator);
    }  /* if */
#endif /* DEFAULT_RECORD_FORM_OF_NAME_REFERENCE */
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED || SUN_EXTENSIONS_ALLOWED
done:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || SUN_EXTENSIONS_ALLOWED */
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 3) {
    db_symbol(tag_sym, "tag_sym: ", 4);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  dps->sym = tag_sym;
  db_exit();
  return !err;
}  /* class_specifier */


STATIC_THREAD an_integer_kind
		largest_enum_int_kind;
			/* The largest integer kind that enum type can
			   have.  Ordinarily ik_int in C mode, but in C++
			   mode the integer kind corresponding to the
			   largest integer type supported. */


#if MICROSOFT_EXTENSIONS_ALLOWED

static void validate_event_handler(a_routine_ptr rp)
/*
The routine is being used in the context of an event handler; issue an error
if it doesn't meet the criteria for an event handler.
*/
{
  a_type_ptr return_type, rout_type = skip_typerefs(rp->type);

  check_assertion(is_function_type(rout_type));
  return_type = skip_typerefs(rout_type->variant.routine.return_type);
  if (is_void_type(return_type) || is_integral_type(return_type)) {
    /* Valid return types for an event handler. */
  } else {
    pos_error(ec_invalid_event_handler_type,
              &rp->source_corresp.decl_position);
  }  /* if */
}  /* validate_event_handler */


void scan_and_record_event_interface_declaration(a_decl_parse_state *dps,
                                                 a_type_ptr         class_type)
/*
Handle an "__event __interface" declaration in (a COM) class_type.
*/
{
  a_boolean         declares_something, defines_something;
  a_type_ptr        interface_type;
  a_source_position event_pos = pos_curr_token;

  check_assertion(curr_token == tok_event &&
                  is_immediate_class_type(class_type));
  dps->has_event_keyword = TRUE;
  (void)get_token();
  check_assertion(curr_token == tok_interface);
  if (class_specifier(dps, DSI_NO_INPUT_FLAGS, /*vacuous_decl_allowed=*/TRUE,
                     /*is_friend_decl=*/FALSE,
                     /*marked_as_gnu_extension=*/FALSE, &interface_type,
                     &declares_something, &defines_something,
                     (a_decl_pos_block *)NULL)) {
    if (defines_something) {
      /* An event interface cannot be defined in a class. */
      pos_error(ec_event_interface_cannot_have_definition,
                &interface_type->source_corresp.decl_position);
    } else if (!declares_something) {
      expect_error();
    } else {
      a_class_type_supplement_ptr  ictsp = class_type_supp(interface_type);
      if (scope_is_null_or_placeholder(ictsp->assoc_scope)) {
        /* The interface class must have been previously defined. */
        pos_error(ec_event_interface_must_be_previously_defined,
                  &interface_type->source_corresp.decl_position);
      } else {
        a_routine_ptr rp;
        an_event_interface_ptr eip = alloc_event_interface();
        a_class_type_supplement_ptr  ctsp = class_type_supp(class_type);
        eip->interface_type = interface_type;
        eip->pos = event_pos;
        if (ctsp->event_interfaces == NULL) {
          ctsp->event_interfaces = eip;
        } else {
          an_event_interface_ptr ptr;
          for (ptr = ctsp->event_interfaces;
               ptr->next != NULL;
               ptr = ptr->next) {}
          ptr->next = eip;
        }  /* if */
        /* All user-specified methods of the interface must be valid event
           handlers (note that base classes are not checked since MSVS
           doesn't seem to check them). */
        for (rp = ictsp->assoc_scope->routines; rp != NULL; rp = rp->next) {
          if (rp->special_kind == (a_special_function_kind)sfk_none) {
            validate_event_handler(rp);
          }  /* if */
        }  /* for */
#if GENERATE_SOURCE_SEQUENCE_LISTS
        /* Add an indication that this is an "__event __interface". */
        { a_source_sequence_entry_ptr sse =
                                 scope_stack_top().end_of_source_sequence_list;
          a_src_seq_secondary_decl_ptr sssd;
          if (sse != NULL) {
            check_assertion(sse->entity.kind == iek_src_seq_secondary_decl);
            sssd = (a_src_seq_secondary_decl_ptr)sse->entity.ptr;
            sssd->is_event_interface = TRUE;
          }  /* if */
        }
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* scan_and_record_event_interface_declaration */


static a_boolean validate_cppcli_enum_base_type(
                                              a_type_ptr         *p_base_type,
                                              a_source_position  *pos_type)
/*
*p_base_type was specified as an explicit underlying type for an enum type in
C++/CLI mode.  If that type is valid in that context, return TRUE; otherwise,
issue an error at the given position and return FALSE.  If *p_base_type is a
valid value class type in that context (e.g., System::Boolean), replace
*p_base_type by the associated primitive type.  If TRUE is returned, any
typerefs are dropped from *p_base_type.
*/
{
  a_boolean   valid = FALSE;
  a_type_ptr  base_type = skip_typerefs(*p_base_type);

  if (is_value_class_type(base_type) &&
      class_type_supp(base_type)->corresponding_basic_type != NULL) {
    base_type = class_type_supp(base_type)->corresponding_basic_type;
  }  /* if */
  if (!is_integral_type(base_type)) {
    pos_error(ec_enum_base_type_must_be_integral, pos_type);
  } else if (!scanning_generated_code_from_metadata &&
             system_type_from_fundamental_type(base_type) == NULL) {
    /* A C++/CLI enum base type must have a corresponding System::xxx value
       class type.  (This cannot be tested while loading the System::...
       metadata.) */
    pos_error(cppcx_enabled ? ec_cppcx_enum_base_has_no_platform_counterpart
                            : ec_cli_enum_base_has_no_system_counterpart,
              pos_type);
  } else {
    valid = TRUE;
    *p_base_type = base_type;
  }  /* if */
  return valid;
}  /* validate_cppcli_enum_base_type */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static an_integer_kind kind_of_explicit_enumeration_base(
                                                      a_type_ptr explicit_base,
                                                      a_boolean  *bool_type)
/*
Return the integer kind associated with the given explicit base type of an
enumeration.  If the explicit base is NULL, return ik_none.  *bool_type is
set to TRUE if the underlying type is the bool type (and FALSE otherwise).
*/
{
  an_integer_kind result;

  *bool_type = FALSE;
  if (explicit_base == NULL) {
    result = ik_none;
  } else if (is_template_dependent_type(explicit_base) ||
             is_error_type(explicit_base)) {
    /* Assume largest_enum_int_kind as the underlying integer kind. */
    result = largest_enum_int_kind;
  } else {
    a_type_ptr underlying_type = skip_typerefs(explicit_base);

    /* If this assertion fails, an integer was expected but a non-integer
       explicit base was used. */
    check_assertion(underlying_type->kind == tk_integer);
    result = underlying_type->variant.integer.int_kind;
    *bool_type = underlying_type->variant.integer.bool_type;
  }  /* if */
  return result;
}  /* kind_of_explicit_enumeration_base */


static an_integer_kind scan_explicit_enum_base_type(
                                               a_type_ptr         *p_base_type,
                                               a_source_position  *pos_type,
                                               a_boolean          *bool_type)
/*
In some modes (e.g., C++11), we accept the explicit specification of an
enumeration type's underlying integer type.  For example:
	enum E: short int { a, b };
If such a base type was specified, the current token is the colon, and this
routine scans it along with the specified type.  The integer kind to be used
for the underlying type is returned, and *p_base_type is set to the type as
scanned if it is valid.  If no base type was specified, ik_none is returned
and *p_base_type is left unchanged.  *bool_type is set to TRUE if the
underlying type is bool (and FALSE otherwise).
*/
{
  if (curr_token == tok_colon && explicit_enum_base_enabled) {
    a_type_ptr  base_type = NULL;
    if (report_explicit_enum_base_as_nonstandard) {
      pos_warning(ec_explicit_enum_base_nonstandard_in_current_mode,
                  &pos_curr_token);
    }  /* if */
    (void)get_token();
    *pos_type = pos_curr_token;
    add_stop_token(tok_lbrace);
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode && disable_access_checking_in_microsoft_enum_bases) {
      /* Microsoft compilers accept the following example:
           struct S { private: typedef int I; };
           enum E: S::I { e };
      */
      begin_deferral_of_access_checks();
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    type_name(&base_type);
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (microsoft_mode && disable_access_checking_in_microsoft_enum_bases) {
      discard_deferred_access_checks();
      end_deferral_of_access_checks();
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    remove_stop_token(tok_lbrace);
    if (base_type != NULL) {
      a_type_ptr  orig_base_type = base_type;
      if (is_template_dependent_type(base_type)) {
        /* Return orig_base_type to be recorded in the IL as the explicit base
           type. */
#if MICROSOFT_EXTENSIONS_ALLOWED
      } else if (cli_or_cx_enabled) {
        /* C++/CLI allows a specific list of integral types and is therefore
           handled separately. */
        if (!validate_cppcli_enum_base_type(&base_type, pos_type)) {
          base_type = NULL;
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      } else if (is_error_type(base_type)) {
        /* An error has presumably already been emitted. */
        expect_error();
      } else if (!is_integral_type(base_type)) {
        pos_error(ec_enum_base_type_must_be_integral, pos_type);
        base_type = NULL;
      } else if (is_bit_precise_integer_type(base_type)) {
        pos_error(ec_bitint_enum_base_not_allowed, pos_type);
        base_type = NULL;
      } else if (microsoft_mode && microsoft_version < 1800 && !cpp11_mode &&
                 is_bool_type(base_type)) {
        /* Early Microsoft compilers did not accept bool as the integral type
           underlying an enum type (in non-C++/CLI mode). */
        pos_error(ec_bool_type_not_allowed, pos_type);
        base_type = NULL;
      }  /* if */
      if (base_type != NULL) {
        *p_base_type = orig_base_type;
      }  /* if */
    }  /* if */
  }  /* if */
  return kind_of_explicit_enumeration_base(*p_base_type, bool_type);
}  /* scan_explicit_enum_base_type */


static void set_enum_representation(a_type_ptr         enum_type,
                                    a_source_position  *pos_enum_tag,
                                    a_boolean          diag_range,
                                    an_integer_kind    explicit_base_kind,
                                    a_boolean          bool_type,
                                    a_boolean          min_max_set,
                                    a_constant_ptr     min_value,
                                    a_constant_ptr     max_value)
/*
Determine the representation type for the given enumeration type.  When
enum_types_can_be_smaller_than_int is FALSE (e.g., in pcc mode and Microsoft
modes), it's always "int", and that's already set.  Otherwise, pick the first
of "char", "signed char", "unsigned char", "short", "unsigned short", and
"int" into which the enumeration values will fit.  Only if
enum_types_can_be_larger_than_int (e.g., in strict C++ mode), is there any
point in trying "unsigned int" and larger integer types.
In some C++ modes, the underlying integer type can be specified explicitly:
In that case, explicit_base_kind will indicate the specified integer type (and
bool_type will indicate if the underlying integer type is the bool type).
If the range of constants has been determined, min_max_set will be TRUE, and
the constants *min_value and *max_value will describe that range.
diag_range is FALSE if diagnostics about the representation range are likely
unhelpful due to earlier errors (while parsing the enum definition).
pos_enum_tag is the position used for diagnostics not related to an explicit
base specifier.
*/
{
#if CHECKING
  if (C_dialect == C_dialect_pcc) {
    check_assertion(!enum_types_can_be_smaller_than_int &&
                    !enum_types_can_be_larger_than_int);
  }  /* if */
#endif /* CHECKING */
  if (explicit_base_kind != (an_integer_kind)ik_none) {
    /* The underlying type is already determined: Record it. */
    enum_type->variant.integer.int_kind = explicit_base_kind;
    enum_type->variant.integer.bool_type = bool_type;
  } else if (enum_type->variant.integer.is_scoped_enum) {
    /* The underlying type for a scoped enum is "int" (unless explicitly
       specified). */
  } else if (enum_types_can_be_smaller_than_int 
#if GNU_EXTENSIONS_ALLOWED
             || (enum_type->variant.integer.packed && gnu_version >= 40000)
             || il_header.short_enums
#endif /* GNU_EXTENSIONS_ALLOWED */
                                                                         ) {
    if (!min_max_set || in_range_for_integer_kind(min_value, max_value,
                                                  plain_char_int_kind)) {
      /* "Plain" char. */
      enum_type->variant.integer.int_kind = plain_char_int_kind;
    } else if (in_range_for_integer_kind(min_value, max_value,
                                         (an_integer_kind)ik_signed_char)) {
      /* Signed char. */
      enum_type->variant.integer.int_kind = (an_integer_kind)ik_signed_char;
    } else if (in_range_for_integer_kind(min_value, max_value,
                                        (an_integer_kind)ik_unsigned_char)) {
      /* Unsigned char. */
      enum_type->variant.integer.int_kind =
                                           (an_integer_kind)ik_unsigned_char;
    } else if (in_range_for_integer_kind(min_value, max_value,
                                         (an_integer_kind)ik_short)) {
      /* Short. */
      enum_type->variant.integer.int_kind = (an_integer_kind)ik_short;
    } else if ((targ_sizeof_short < targ_sizeof_int) &&
                in_range_for_integer_kind(min_value, max_value,
                                       (an_integer_kind)ik_unsigned_short)) {
      /* Unsigned short.  Note that we can only get here if
         sizeof(short) < sizeof(int) on the target, for otherwise the
         previous test (for "short") is testing the same range as "int"
         (into which all enumeration values must fall), so "short" would
         have been selected.  This is important, as we would not want
         to pick "unsigned short" if the integral promotions would
         promote it to "unsigned int" rather than "int". */
      enum_type->variant.integer.int_kind =
                                          (an_integer_kind)ik_unsigned_short;
    } else {
      /* Use the default representation type, which is already set. */
    }  /* if */
  }  /* if */
  /* If the underlying integer type of enums can be larger than "int" (as
     is standard in C++) and if the type has not already been adjusted to
     be smaller than int, keep checking. */
  if (explicit_base_kind != (an_integer_kind)ik_none) {
    /* The underlying type is already determined: Nothing to be done. */
  } else if (min_max_set && enum_types_can_be_larger_than_int &&
             enum_type->variant.integer.int_kind == (an_integer_kind)ik_int) {
    if (in_range_for_integer_kind(min_value, max_value,
                                  (an_integer_kind)ik_int)) {
      /* Int. */
      enum_type->variant.integer.int_kind = (an_integer_kind)ik_int;
    } else if (in_range_for_integer_kind(min_value, max_value,
                                        (an_integer_kind)ik_unsigned_int)) {
      /* Unsigned int. */
      enum_type->variant.integer.int_kind = (an_integer_kind)ik_unsigned_int;

    } else if (in_range_for_integer_kind(min_value, max_value,
                                         (an_integer_kind)ik_long)) {
      /* Long. */
      enum_type->variant.integer.int_kind = (an_integer_kind)ik_long;
    } else if (in_range_for_integer_kind(min_value, max_value,
                                        (an_integer_kind)ik_unsigned_long)) {
      /* Unsigned long. */
      enum_type->variant.integer.int_kind = (an_integer_kind)ik_unsigned_long;
#if LONG_LONG_ALLOWED
    } else if ((!strict_ansi_mode || long_long_is_standard) &&
               in_range_for_integer_kind(min_value, max_value,
                                         (an_integer_kind)ik_long_long)) {
      /* Long long. */
      enum_type->variant.integer.int_kind = (an_integer_kind)ik_long_long;
    } else if ((!strict_ansi_mode || long_long_is_standard) &&
               in_range_for_integer_kind(
                                  min_value, max_value,
                                  (an_integer_kind)ik_unsigned_long_long)) {
      /* Unsigned long long. */
      enum_type->variant.integer.int_kind =
                                     (an_integer_kind)ik_unsigned_long_long;
#endif /* LONG_LONG_ALLOWED */
    } else {
      /* No integer type can hold all the values.  We'll use the largest
         available integer type and issue a diagnostic. */
#if INT128_EXTENSIONS_ALLOWED
      /* GNU compilers do not consider 128-bit integer types in this context.
         We emulate that behavior. */
#endif /* INT128_EXTENSIONS_ALLOWED */
      enum_type->variant.integer.int_kind = largest_enum_int_kind;
      if (diag_range && !scope_stack_top().in_prototype_instantiation) {
        pos_diagnostic(strict_ansi_mode ? es_error : es_warning,
                       ec_insufficient_enum_range, pos_enum_tag);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* set_enum_representation */


static void check_enum_value_for_fixed_underlying_type(
                                             a_constant_ptr   constant,
                                             a_type_ptr       underlying_type,
                                             a_boolean        implicit_value,
                                             a_boolean        *err)
/*
Check that an enumerator value fits in the given underlying type (if that type
is NULL, use "int" instead).  The enumerator value is the given constant and
was specified explicitly if implicit_value is FALSE; otherwise, the enumerator
value is obtained implicitly by incrementing the given constant.  If the
enumerator value does not fit, an error is issued and *err is set to TRUE.  If
no error is issued and implicit_value is TRUE, *constant is incremented.  A
bool underlying type (e.g., "enum E: bool") can represent only false and true,
so its values are checked against the range [0, 1].
*/
{
  an_integer_kind  underlying_kind;
  a_boolean        underlying_is_bool;

  if (underlying_type == NULL) {
    underlying_type = integer_type(ik_int);
  }  /* if */
  underlying_kind = skip_typerefs(underlying_type)->variant.integer.int_kind;
  underlying_is_bool = is_bool_type(underlying_type);
  if (implicit_value) {
    a_boolean increment_overflows =
                underlying_is_bool
                  ? eqlit_integer_constant(constant, (a_host_large_integer)1)
                  : is_max_value_for_integer_kind(constant, underlying_kind);
    if (increment_overflows) {
      /* The implicit increment produces a value that cannot be represented
         by the underlying type.  Ordinarily, this is an error, but Microsoft
         compilers just wrap the value around (except for bool). */
      if (microsoft_mode) {
        pos_ty_warning(ec_enum_value_out_of_underlying_range, &error_position,
                       underlying_type);
        if (underlying_is_bool) {
          set_integer_value(&constant->variant.integer_value,
                            (a_host_large_integer)1);
        } else {
          constant->variant.integer_value =
                                   min_integer_value_of_kind[underlying_kind];
        }  /* if */
      } else {
        pos_ty_error(ec_enum_value_out_of_underlying_range, &error_position,
                     underlying_type);
        *err = TRUE;
      }  /* if */
    } else {
      incr_integer_value(&constant->variant.integer_value);
    }  /* if */
  } else {
    a_boolean out_of_range =
                underlying_is_bool
                  ? !(eqlit_integer_constant(constant,
                                             (a_host_large_integer)0) ||
                      eqlit_integer_constant(constant,
                                             (a_host_large_integer)1))
                  : !in_range_for_integer_kind(constant, constant,
                                               underlying_kind);
    if (out_of_range) {
      if (microsoft_mode) {
        pos_ty_warning(ec_enum_value_out_of_underlying_range, &error_position,
                       underlying_type);
        /* Truncate the specified value to the length of the underlying type
           (taking care to extend the sign bit as needed).  E.g., 1000
           truncated to "signed char" becomes -24. */
        if (underlying_is_bool) {
          set_integer_value(&constant->variant.integer_value,
                            (a_host_large_integer)
                                                !is_false_constant(constant));
        } else {
          trim_integer_value_to_type(&constant->variant.integer_value,
                                     underlying_type);
        }  /* if */
      } else {
        pos_ty_error(ec_enum_value_out_of_underlying_range, &error_position,
                     underlying_type);
        *err = TRUE;
      }  /* if */
    } else {
      a_boolean  did_not_fold = FALSE;
      type_change_constant(constant, underlying_type,
                           /*is_implicit_cast=*/TRUE,
                           /*maintain_expression=*/TRUE,
                           &did_not_fold, &error_position);
    }  /* if */
  }  /* if */
}  /* check_enum_value_for_fixed_underlying_type */


static void change_enum_constants_type(a_constant_ptr  constants,
                                       a_type_ptr      new_type)
/*
Change the types of the enumeration constants in the given list to the given
integer type and adjust the associated integer values if needed.
*/
{
  a_constant_ptr   cp;
  an_integer_kind  new_int_kind;

  check_assertion(is_integral_or_enum_type(new_type));
  new_int_kind = skip_typerefs(new_type)->variant.integer.int_kind;
  for (cp = constants; cp != NULL; cp = cp->next) {
    cp->type = new_type;
    if (cp->kind == (a_constant_repr_kind)ck_integer &&
        !in_range_for_integer_kind(cp, cp, new_int_kind)) {
      /* Convert the value of the enumerator constant to fit in its new type.
         Do not use type_change_constant since that would turn it into an
         unnamed constant. */
      an_integer_value  value;
      value = cp->variant.integer_value;
      trunc_and_set_integer(&value, cp, /*check_overflow=*/FALSE,
                            /*saturate_on_overflow=*/FALSE,
                            (an_error_code*)NULL, (an_error_severity*)NULL);
    }  /* if */
  }  /* for */
}  /* change_enum_constants_type */


static an_integer_kind explicit_base_kind_of_enumeration(a_type_ptr enum_type,
                                                         a_boolean  *bool_type)
/*
Return the integer kind associated with the given enumeration's explicit base.
If no explicit base was specified, return ik_none.  *bool_type is set to TRUE
if the type is the bool type (and FALSE otherwise).
*/
{
  a_type_ptr explicit_base = integer_type_supp(enum_type)->base_type;

  return kind_of_explicit_enumeration_base(explicit_base, bool_type);
}  /* explicit_base_kind_of_enumeration */


a_boolean scan_enumerator_constant(
                                 a_constant_ptr            constant,
                                 a_type_ptr                enum_type,
                                 a_boolean                 *is_template_param,
                                 ARG_UNUSED a_source_range *source_range)
/*
Scan the tokens forming an enumerator constant.  The constant will be returned
in *constant, which will be subsequently allocated in the file scope memory
region.  *is_template_param will be set to TRUE if the enumerator constant
refers to a template parameter.  *source_range if set will be updated to mark
the start and end source positions of the enumerator constant.  Return TRUE if
there was an error; otherwise, return FALSE.
*/
{
  a_boolean result = FALSE;
  a_boolean bool_type = FALSE;

#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (source_range != NULL) {
    source_range->start = pos_curr_token;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

  a_boolean       is_scoped_enum = enum_type->variant.integer.is_scoped_enum;
  a_type_ptr      explicit_base = integer_type_supp(enum_type)->base_type;
  an_integer_kind explicit_base_kind =
                      explicit_base_kind_of_enumeration(enum_type, &bool_type);
  a_type_ptr      fixed_type = explicit_base;
  /* The underlying type of a C++11 enumeration type is said to be fixed
     if it is explicitly specified or if the enumeration type is scoped.
     This is used to scan the enumerator constant definitions as "converted
     constant expressions". */
  if (explicit_base == NULL && is_scoped_enum) {
    fixed_type = integer_type(ik_int);
  }  /* if */
  /* Scan the constant expression.  (If fixed_type is non-NULL, scan it as a
     "converted constant expression" for that type in C++11 mode.) */
  scan_fs_integral_constant_expression(fixed_type, /*is_enum=*/TRUE,
                                       constant);
  add_backing_expression_for_named_constant(constant);
  /* Even though the constant may just be "0", that property should
     not be carried into the enumerators derived from it. */
  constant->is_simple_zero = FALSE;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (source_range != NULL) {
    source_range->end = curr_construct_end_position;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  if (is_error_constant(constant)) {
    result = TRUE;
  } else if (constant->kind == ck_template_param) {
    /* We are doing a prototype instantiation and we have a case like this:
         template <int N> class A { enum e { e1 = 2*N }; };
     */
    *is_template_param = TRUE;
  } else if (is_scoped_enum || explicit_base_kind != ik_none) {
    /* The underlying type is fixed.  explicit_base is NULL for a scoped enum
       with no explicit base, in which case the underlying type is "int". */
    check_enum_value_for_fixed_underlying_type(constant, explicit_base,
                                               /*implicit_value=*/FALSE,
                                               &result);
  } else if (enum_types_can_be_larger_than_int) {
    /* No need to check, since the largest integer kind will be used if
       needed. */
  } else {
    check_assertion(constant->kind == ck_integer);
    /* Check the value to see if it is out of range. */
    if (!in_range_for_integer_kind(constant, constant,
                                   largest_enum_int_kind)) {
      a_boolean  conversion_allowed = TRUE;
      if (strict_ansi_mode) {
        conversion_allowed = strict_ansi_error_severity != es_error;
      }  /* if */
      if (conversion_allowed &&
          (f_skip_typerefs(constant->type)->size <= targ_sizeof_int ||
           ms_extensions)) {
        /* In non-strict mode, allow unsigned constants that can be coerced
           into an int.  (Microsoft compilers appear to even permit cases like:
           enum { e = static_cast<unsigned long>(-1) }; with unsigned long a
           larger type than int. */
        a_boolean  did_not_fold = FALSE;
        type_change_constant(constant, integer_type(ik_int),
                             /*is_implicit_cast=*/TRUE,
                             /*maintain_expression=*/TRUE,
                             &did_not_fold,
                             &error_position);
        if (strict_ansi_mode) {
          pos_warning(ec_enum_value_out_of_int_range, &error_position);
        }  /* if */
      } else {
        pos_error(ec_enum_value_out_of_int_range, &error_position);
        result = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* scan_enumerator_constant */


void scan_enumerator_list(
                        a_type_ptr                     enum_type,
                        ARG_UNUSED a_decl_parse_state  *dps,
                        a_decl_flag_set                dsi_flags,
                        ARG_UNUSED an_ms_attribute_ptr *p_ms_attributes,
                        a_type_ptr                     class_of_which_a_member,
                        a_boolean                      *declares_something,
                        ARG_UNUSED a_decl_pos_block    *decl_pos_block)
/*
Scan the list of enumerators in an enum type definition, including its
enclosing braces.  (The current token is the left brace, except perhaps in
error cases.)  enum_type is the type whose definition must be scanned.  *dps
describes the declaration containing the enum definition (which may include
attributes).  dsi_flags is the set of input flags passed to decl_specifiers.
p_ms_attributes describes Microsoft COM-style attributes preceding the enum
specifier (if any).   class_of_which_a_member specifies the parent type of
the enum type.  *declares_something is set to TRUE if an unscoped enum
definition introduces a name in its surrounding scope.  *decl_pos_block
is updated to reflect relevant positions of this definition.
*/
{
  a_symbol_ptr       tag_sym = symbol_for(enum_type), enum_con_sym;
  a_boolean          is_scoped_enum =
                                    enum_type->variant.integer.is_scoped_enum;
  a_boolean          is_dependent_enum = FALSE;
  a_boolean          done, min_max_set, diag_range = TRUE;
  a_boolean          bool_type = FALSE;
  a_constant_ptr     constant_list = NULL, end_of_enum_con_list, enum_con;
  a_constant_ptr     max_value = local_constant();
  a_constant_ptr     min_value = local_constant();
  a_constant_ptr     constant = local_constant();
  a_type_ptr         explicit_base, enum_con_type;
  an_integer_kind    explicit_base_kind;
  a_scope_number     reactivated_class_scope_number = NO_SCOPE_DEPTH;
  a_source_position  definition_pos, end_pos;
  a_memory_region_number
                     region_to_switch_back_to;
  a_template_cache_segment_ptr
                     tcsp = NULL;
  an_enum_symbol_supplement_ptr
                     essp;
  a_boolean          is_enum_template_definition = FALSE;
  a_source_position  lbrace_pos;
  a_token_sequence_number
                     first_tsn = NO_TOKEN_SEQUENCE_NUMBER;

  explicit_base = integer_type_supp(enum_type)->base_type;
  /* Determine whether this is the definition of a member enum that can be
     instantiated.  A scoped enum declared in a class template is such an
     enum (whether defined in the class or later outside of the class).
     An unscoped enum can also be instantiated if it is declared in the
     class as an opaque enumeration and defined later outside of the class. */
  if (class_of_which_a_member != NULL &&
      (is_scoped_enum || explicit_base != NULL) &&
      enum_type->variant.integer.is_template_enum &&
      !enum_type->variant.integer.originally_unnamed &&
      class_of_which_a_member->
                       variant.class_struct_union.is_prototype_instantiation) {
    if (is_scoped_enum && curr_scope_is_class_template_definition()) {
      /* A scoped enum defined in the class template. */
      is_enum_template_definition = TRUE;
    } else if ((dsi_flags & DSI_IS_TEMPLATE_DECLARATION) != 0) {
      /* A scoped or unscoped enum that is being defined outside of the
         class. */
      is_enum_template_definition = TRUE;
    }  /* if */
  }  /* if */
  check_assertion_or_expect_error(curr_token == tok_lbrace);
  essp = tag_sym->variant.enumeration.extra_info;
  explicit_base_kind = explicit_base_kind_of_enumeration(enum_type,
                                                         &bool_type);
  definition_pos = pos_curr_token;
  /* We associate a curr-construct pragma with this enum type only if this
     is a definition.  Otherwise this is assumed to be part of a declaration
     of something else -- to which the pragma should be bound. */
  if (tag_sym != NULL) {
    /* Do processing required for any pragmas that are bound to the current
       declaration. */
    process_curr_construct_pragmas(tag_sym, (a_statement_ptr)NULL);
  } else {
    /* Issue diagnostics on pragmas that are trying to bind to an unnamed
       enum. */
    cannot_bind_to_curr_construct();
  }  /* if */
  if (constexpr_enabled && !relaxed_constexpr_enabled && !gpp_mode &&
      innermost_function_scope != NULL &&
      innermost_function_scope->variant.routine.ptr->is_constexpr) {
    pos_diagnostic(relaxed_constexpr_allowed() ? es_warning : es_error,
                   ec_tag_defined_in_constexpr_body, &definition_pos);
  }  /* if */
  if (scope_is(&scope_stack_top(), sck_class_reactivation)) {
    reactivated_class_scope_number = scope_stack_top().number;
  }  /* if */
  if (is_enum_template_definition) {
    /* This is an enumeration declared in a class template that can be
       separately instantiated.  We need to create a cache for the enum
       definition. */
    first_tsn = curr_token_sequence_number;
    /* Start background caching of the tokens of the definition. */
    begin_caching_fetched_tokens(/*include_curr_token=*/TRUE);
  }  /* if */
  /* Scan the enumeration itself.  Since the enumeration type entry is
     allocated in the file scope memory region, all its components should
     also be.  Switch to the file scope memory region here at the start of
     the definition and switch back when we reach the right brace. */
  lbrace_pos = (curr_token == tok_lbrace) ? pos_curr_token
                                          : null_source_position;
  (void)required_token(tok_lbrace, ec_exp_lbrace);
  if (is_scoped_enum) {
    enum_type->variant.integer.enum_info.assoc_scope = 
              push_scope((a_scope_kind)sck_enum, NO_SCOPE_NUMBER, enum_type,
                         (a_routine_ptr)NULL);
  }  /* if */
  integer_type_supp(enum_type)->enumerator_list_seen = TRUE;
  if (C_dialect == C_dialect_cplusplus || gcc_mode ||
      explicit_base_kind != (an_integer_kind)ik_none) {
    /* In C++ the type of an enumerator is the same as that of its
       enumeration, but that won't actually be known until the definition
       is complete.  Set the types in the enum constants later.
       In GNU C mode, the type of an enumerator is an integer type
       large enough to represent all the enumerator values.  It won't be
       known until the definition is complete. */
    enum_con_type = NULL;
    /* For an enum in a class template the enumerators must be treated as
       dependent. */
    is_dependent_enum = tag_sym->corresp_nonreal_or_nested_type != NULL;
  } else {
    /* In C the type of the constants is always "int", regardless of
       the type of the enumerated type (see 3.5.2.2).  However, it is
       tagged with the enumerated type, so that enum compatibility checking
       can be done later.  (GNU C mode is different: See above.) */
    check_assertion(!enum_types_can_be_larger_than_int);
    enum_con_type = alloc_type((a_type_kind)tk_integer);
    enum_con_type->variant.integer.int_kind = (an_integer_kind)ik_int;
    enum_con_type->variant.integer.enum_type = FALSE;
    enum_con_type->variant.integer.enum_info.affiliated_type = enum_type;
    set_type_size(enum_con_type);
  }  /* if */
  min_max_set = FALSE;
  if (curr_token == tok_rbrace &&
      (C_dialect == C_dialect_cplusplus || microsoft_mode)) {
    /* An enumerator constant list is optional in C++ and Microsoft C. */
  } else {
    a_boolean   cppcli_enum_init_error_issued = FALSE;
    add_stop_token(tok_rbrace);
    end_of_enum_con_list = NULL;
    /* Scan the list of enumerated constants. */
    do {
      a_symbol_locator             locator;
      a_boolean                    template_param = FALSE, err = FALSE;
      a_source_position            enum_con_pos, pos_comma;
      a_source_sequence_entry_ptr  enum_con_ssep = NULL;
      an_attribute_ptr             enumerator_attributes = NULL;
#if MICROSOFT_EXTENSIONS_ALLOWED
      an_ms_attribute_ptr          ms_attributes = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      a_source_range               enum_value_range;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      a_source_range               enum_id_range = null_source_range;
      enum_value_range = null_source_range;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      add_stop_token(tok_comma);
      add_stop_token(tok_assign);
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (cli_or_cx_enabled && microsoft_attribute_tokens_next()) {
        /* In C++/CLI mode, attributes can be applied to individual
           enumeration values. */
        ms_attributes = scan_microsoft_attributes(/*is_param_or_base=*/FALSE);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      enum_con_pos = pos_curr_token;
      if (curr_token != tok_identifier) {
        (void)required_token(tok_identifier, ec_exp_identifier);
        set_to_error_locator(locator);
      } else {
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if DEBUG
/* GCOVR_EXCL_START */
        if (!source_sequence_entries_disallowed &&
            (debug_level >= 4 || db_flag_is_set("dump_ss_full"))) {
          fprintf(f_debug, "enum_specifier: empty ss entry for \"%s\":\n",
                  locator_for_curr_id.symbol_header->identifier);
        }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
        enum_con_ssep = add_empty_source_sequence_entry();
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        locator = locator_for_curr_id;
#if EXTRA_SOURCE_POSITIONS_IN_IL
        enum_id_range.start = pos_curr_token;
        enum_id_range.end = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (cli_or_cx_enabled && curr_token_is_identifier_string("value__")) {
          pos_error(ec_reserved_enumerator_name, &pos_curr_token);
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Advance past the identifier. */
        (void)get_token();
        /* Set the error position to the identifier position. */
        copy_source_position(locator.source_position, error_position);
        if (enumerator_attributes_enabled) {
          /* Scan any standard attributes. */
          enumerator_attributes = scan_attributes(al_enumerator);
        }  /* if */
      }  /* if */
      /* Note that the enumerator symbol is entered a little later, after
         the constant expression (if any) has been scanned.  (C standard,
         3.1.2.1 and 3.5.2.2) */
      remove_stop_token(tok_assign);
      /* Forget about expressions scanned in previous constants. */
      constant->expr = NULL;
      /* See if "= constant-expression" follows. */
      if (curr_token == tok_assign) {
        (void)get_token();
        if (scan_enumerator_constant(constant, enum_type, &template_param,
                                     &enum_value_range)) {
          err = TRUE;
        }  /* if */
      } else {
        /* No explicit value. */
        if (cli_or_cx_enabled && !cppcli_enum_init_error_issued &&
            explicit_base_kind != (an_integer_kind)ik_none &&
            is_bool_type(integer_type_supp(enum_type)->base_type)) {
          /* ECMA-372 (the C++/CLI standard) requires that C++/CLI enum
             types with an explicit boolean underlying type have explicit
             values for each of their enumerator constants. */
          pos_error(ec_cppcli_enumerator_requires_explicit_value,
                    &enum_con_pos);
          cppcli_enum_init_error_issued = TRUE;
        }  /* if */
        if (end_of_enum_con_list == NULL) {
          /* This is the first enumerator.  Start with zero. */
          an_integer_kind  first_kind = (an_integer_kind)ik_int;
          if (explicit_base_kind != (an_integer_kind)ik_none) {
            first_kind = explicit_base_kind;
          }  /* if */
          set_integer_constant(constant, (a_host_large_integer)0,
                               first_kind);
        } else if (is_error_constant(constant)) {
          /* There was a previous error. */
          err = TRUE;
        } else {
          /* Use a value one larger than the previous value. */
          if (constant->kind == (a_constant_repr_kind)ck_template_param) {
            /* The previous value was template-dependent.  So we need to
               create a distinct template-dependent value for this one. */
            increment_template_dependent_enum_constant(constant);
            template_param = TRUE;
          } else if (is_scoped_enum ||
                     explicit_base_kind != (an_integer_kind)ik_none) {
            /* The underlying type is fixed.  explicit_base is NULL for a
               scoped enum with no explicit base, in which case the
               underlying type is "int". */
            check_enum_value_for_fixed_underlying_type(
                                            constant, explicit_base,
                                            /*implicit_value=*/TRUE, &err);
          } else if (is_max_value_for_integer_kind(constant,
                                                   largest_enum_int_kind)) {
            /* The incremented value would be out of range (3.5.2.2,
               constraints). */
            pos_error(ec_enum_value_out_of_int_range, &error_position);
            err = TRUE;
          } else {
            /* If incrementing the current value requires a larger
               integer, just use the integer corresponding to
               largest_enum_int_kind.  It's not specified by the standard
               what larger integer to use, and it doesn't seem to make
               much difference. */
            a_type_ptr  constant_type = skip_typerefs(constant->type);
            check_assertion(constant_type->kind == (a_type_kind)tk_integer);
            if (is_max_value_for_integer_kind(
                                constant,
                                constant_type->variant.integer.int_kind)) {
              constant->type = integer_type(largest_enum_int_kind);
            }  /* if */
            incr_integer_value(&constant->variant.integer_value);
          }  /* if */
        }  /* if */
      }  /* if */
      if (gpp_mode && scope_is(&scope_stack_top(), sck_class_struct_union)) {
        /* In GNU C++ mode, we remove any synthesized projection symbols
           before entering a symbol for an enumeration specifier.  This
           allows examples like
             struct B { enum { e }; };
             struct D: B { enum { d = D::e, e = d + 1 }; };
           (which normally results in an "already declared" error). */
        a_symbol_ptr  sym;
        /* Look for existing symbols that would collide with the current
           name.  The value returned by curr_scope_id_lookup is already
           stripped of projections, but the original symbol is available
           in the specific_symbol field of the symbol locator. */
        (void)curr_scope_id_lookup(&locator, IDL_PROJ_SYMBOL_ALLOWED);
        sym = locator.specific_symbol;
        if (sym != NULL && symbol_is(sym, sk_projection) &&
            !sym->variant.projection.is_using_decl) {
          remove_symbol(sym);
        }  /* if */
      }  /* if */
      /* Enter the enumeration constant identifier. */
      if (enum_type->is_tag_redefinition) {
        /* A redefinition of an enumerated type declares the same enumerators
           as the earlier declaration of the tag, and those are already in
           this scope.  Give this one a symbol that is not entered in the
           symbol table, so that the two declarations can be compared without
           the enumerator names colliding. */
        enum_con_sym = make_unentered_symbol(sk_constant,
                                             locator.symbol_header,
                                             &locator.source_position);
      } else if (reactivated_class_scope_number != NO_SCOPE_DEPTH &&
                 !is_scoped_enum) {
        /* enter_local_symbol cannot be used to add a symbol to a completed
           class scope.  Use enter_enumerator_into_completed_class instead. */
        enum_con_sym = enter_enumerator_into_completed_class(
                                            &locator, class_of_which_a_member,
                                            reactivated_class_scope_number);
      } else {
        enum_con_sym = enter_local_symbol((a_symbol_kind)sk_constant, &locator,
                                          decl_scope_level,
                                          /*suppress_redecl_error=*/FALSE);
      }  /* if */
      if (!is_scoped_enum) {
        /* An unscoped and unnamed enum type definition that introduces
           enumerator constants "declares something", but a scoped unnamed
           enum type does not "declare something" (at least not in its
           surrounding scope). */
        *declares_something = TRUE;
      }  /* if */
      /* Track the highest and lowest values in the enumeration.  These are
         used to determine the appropriate representation type. */
      if (err) {
        /* There was some kind of error in the value for the enumerator. */
        set_error_constant(constant);
        diag_range = FALSE;
      } else if (template_param) {
        /* The expression has a template parameter value, so it has
           no effect on the size of the enumeration. */
      } else if (!min_max_set) {
        *max_value = *constant;
        *min_value = *constant;
        min_max_set = TRUE;
      } else if (cmp_integer_constants(constant, max_value) > 0) {
        *max_value = *constant;
      } else if (cmp_integer_constants(constant, min_value) < 0) {
        *min_value = *constant;
      }  /* if */
      /* Assign the value to the enumeration constant. */
      switch_to_file_scope_region(&region_to_switch_back_to);
      if (constant->kind == (a_constant_repr_kind)ck_template_param ||
          is_dependent_enum) {
        /* Add a do-nothing cast to a ck_template_constant so as
           to avoid problems with using the same constant entry for
           the enumerator and its value (e.g., class membership
           information for the value as a tpck_member constant
           conflicts with the class membership of the enumerator).
           Also do this for enumerators that must be treated as dependent. */
        make_template_param_cast_constant(constant, constant, constant->type,
                                          /*is_explicit=*/FALSE);
      }  /* if */
      enum_con = alloc_unshared_constant(constant);
      enum_con->is_named_constant_definition = TRUE;
      /* Record the parent scope of the enumerator. */
      if (reactivated_class_scope_number != NO_SCOPE_DEPTH &&
          !is_scoped_enum) {
        enum_con->source_corresp.parent_scope =
                        class_type_supp(class_of_which_a_member)->assoc_scope;
      } else {
        if (scope_stack[decl_scope_level].kind == (a_scope_kind)sck_block ||
            scope_stack[decl_scope_level].kind ==
                                           (a_scope_kind)sck_func_prototype) {
          /* Don't call ensure_il_scope_exists for any scope other than block
             and function prototype scopes, because it may fail (abort) in
             some unusual error cases. */
          (void)ensure_il_scope_exists(&scope_stack[decl_scope_level]);
        }  /* if */
        enum_con->source_corresp.parent_scope =
                                       scope_stack[decl_scope_level].il_scope;
        if (parent_scope_of(enum_con) == NULL) {
          /* Only occurs in strange error situations (e.g., an enum defined
             in a template parameter list). */
          expect_error();
        } else if (!in_file_scope(parent_scope_of(enum_con))) {
          /* A memory region constraint violation: Break the link. */
          enum_con->source_corresp.parent_scope = NULL;
        }  /* if */
      }  /* if */
      /* Switch back from the file scope memory region to whatever region
         was current upon entry. */
      switch_back_to_original_region(region_to_switch_back_to);
      set_source_corresp(&(enum_con->source_corresp), enum_con_sym);
      enum_con->source_corresp.name_linkage =
                                     enum_type->source_corresp.name_linkage;
      enum_con_sym->variant.constant = enum_con;
      if (gcc_mode && explicit_base == NULL) {
        /* In GNU C mode (without an explicitly-specified underlying type),
           the type of the constants is determined after all the constants
           have been seen. */
      } else if (C_mode()) {
        /* Pre-C23 the type is an plain integer type (normally "int").  In C23,
           it may be the enumeration type if the underlying type was explicitly
           specified. */
        enum_con->type = explicit_base != NULL ? enum_type : enum_con_type;
      } else {
        /* In C++ mode leave the type of the constant unchanged for now.
           The enumerator constants will get the type of the enumeration,
           but not until after all the constants have been scanned.  (This
           affects cases in which an enum constant expression involves a
           previously declared enum constant from the same enumeration.) */
        if (!is_scoped_enum) {
          /* In C++ specify membership and access.  That doesn't apply to
             scoped enum constants; setting parent_scope (done above) is
             sufficient for those. */
          if (class_of_which_a_member != NULL) {
            /* Set the parent class. */
            set_class_membership(enum_con_sym, &enum_con->source_corresp,
                                 class_of_which_a_member);
            enum_con->source_corresp.access = enum_type->source_corresp.access;
          } else if (sym_is_namespace_member(tag_sym)) {
            /* Set the parent namespace. */
            set_namespace_membership(enum_con_sym, &enum_con->source_corresp,
                                     sym_parent_namespace(tag_sym));
          }  /* if */
        }  /* if */
      }  /* if */
      record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION, enum_con_sym,
                                &locator.source_position, enum_con_ssep);
      /* Add the enumeration constant to the list under the enumerated
         type. */
      if (end_of_enum_con_list == NULL) {
        constant_list = enum_con;
        if (is_scoped_enum) {
          enum_type->variant.integer.enum_info.assoc_scope->constants =
                                                              constant_list;
        } else {
          enum_type->variant.integer.enum_info.constant_list = constant_list;
        }  /* if */
      } else {
        end_of_enum_con_list->next = enum_con;
      }  /* if */
      end_of_enum_con_list = enum_con;
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (ms_attributes != NULL) {
        apply_microsoft_attributes(&ms_attributes, (char*)enum_con,
                                   (an_il_entry_kind)iek_constant,
                                   (an_ms_attribute_target)msat_field,
                                   (an_ms_attribute_target)msat_field);
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      if (enumerator_attributes != NULL) {
        /* Attach any standard attributes to the enumerator. */
        attach_attributes(enumerator_attributes, (char*)enum_con,
                          iek_constant);
      }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
      { a_decl_position_supplement_ptr  dpsp;
        dpsp = enum_con->source_corresp.decl_pos_info;
        if (dpsp != NULL) {
          dpsp->identifier_range = enum_id_range;
          dpsp->variant.enum_value_range = enum_value_range;
        }  /* if */
      }
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      /* Keep looping while there are more enumeration constant
         identifiers. */
      copy_source_position(pos_curr_token, pos_comma);
      done = !loop_token(tok_comma);
      if (!done && curr_token == tok_rbrace) {
         /* In K&R C, C99, and C++11 modes an extra comma is allowed at the
            end of the list.  In other C and C++ modes, we allow it as an
            extension, with a remark or strict ANSI diagnostic, but the
            diagnostic is omitted altogether for C++11-like scoped enum
            types and enum types with explicit underlying types.  The gcc
            compiler source includes cases like this, and that source is
            part of the SPEC benchmark suite. */
        done = TRUE;
        if (C_dialect != C_dialect_pcc && !c99_mode && !cpp11_mode &&
            !(is_scoped_enum ||
              explicit_base_kind != (an_integer_kind)ik_none)) {
          an_error_severity  severity;
          severity = strict_ansi_mode ? strict_ansi_discretionary_severity :
                                        es_remark;
          pos_diagnostic(severity, ec_nonstd_extra_comma, &pos_comma);
        }  /* if */
      }  /* if */
      remove_stop_token(tok_comma);
    } while (!done);
    remove_stop_token(tok_rbrace);
  }  /* if */
  end_pos = pos_curr_token;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  decl_pos_block->specifiers_range.end = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* For an enum in a class template, first_tsn will have been set above.
     Save the ending position of the tokens to be included in the
     definition cache of the template. */
  if (first_tsn != NO_TOKEN_SEQUENCE_NUMBER) {
    a_template_symbol_supplement_ptr tssp;
    a_template_symbol_supplement_ptr class_tssp;
    a_type_ptr                       parent_class;
    check_assertion(tag_sym->is_class_member);
    parent_class = tag_sym->parent.class_type;
    class_tssp = symbol_supplement_for_class(parent_class)->template_info;
    tssp = essp->template_info;
    tcsp = get_template_cache_segment(tag_sym, tssp, first_tsn,
                                      curr_token_sequence_number);
    end_caching_fetched_tokens();
    if (tssp->cache->tokens.ptr() == NULL) {
      tssp->cache->tokens = shared_obj<a_token_cache>(/*is_reusable=*/TRUE);
    }  /* if */
    copy_tokens_from_cache(curr_lexical_state_cache(),
                           tcsp->first_token_number,
                           curr_token_sequence_number,
                           /*include_last_token=*/TRUE,
                           tssp->cache->tokens.ptr());
    if (is_scoped_enum) {
      /* Scoped enums are instantiated when needed.  Unscoped enums are
         instantiated when their declaration is encountered in the
         instantiation of the enclosing class.  For the latter case we
         don't add a cache terminator as the tokens will appear to occur
         as part of the enum declaration in class instantiations. */
      terminate_token_cache(tssp->cache->tokens.ptr());
    }  /* if */
    /* Update the template decl info based on the parent class. */
    set_template_cache_info(tssp->cache, a_reusable_token_cache(),
                            class_tssp->cache->decl_info);
  }  /* if */
  /* Check for and pass over the closing "}". */
  (void)required_token(tok_rbrace, ec_exp_rbrace, ec_matching_lbrace,
                       &lbrace_pos);
  if (is_scoped_enum) pop_scope();
#if GNU_EXTENSIONS_ALLOWED
  if (gnu_mode && curr_token == tok_attribute) {
    /* Check for something like "enum E { e } __attribute((packed));".
       Ordinarily, specifier attributes should be applied after all
       specifiers have been seen, but in this case some may have to be
       applied before the type underlying the enumeration is fixed. */
    an_attribute_ptr  attributes =
                          scan_gnu_attribute_groups(al_post_tag_definition);
    if (attributes != NULL) {
      end_pos = end_position_of_attributes;
#if EXTRA_SOURCE_POSITIONS_IN_IL
      if (decl_pos_block != NULL) {
        /* Update the recorded end position to the end of the attributes
           specifier. */
        decl_pos_block->specifiers_range.end = curr_construct_end_position;
      }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
      attach_postfix_enum_attributes(attributes, enum_type, dps);
    }  /* if */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (p_ms_attributes != NULL && *p_ms_attributes != NULL &&
      depth_innermost_function_scope == NO_SCOPE_NUMBER &&
      !inside_local_class) {
    apply_microsoft_attributes_to_type(p_ms_attributes, enum_type);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Add a source sequence entry marking the end of the enum definition. */
  add_end_of_construct_source_sequence_entry((char *)enum_type, iek_type);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  set_enum_representation(enum_type, &enum_type->source_corresp.decl_position,
                          diag_range, explicit_base_kind, bool_type,
                          min_max_set, min_value, max_value);
#if GNU_EXTENSIONS_ALLOWED
  if (gnu_mode && !is_scoped_enum &&
      explicit_base_kind == (an_integer_kind)ik_none) {
    an_integer_kind  int_kind = enum_type->variant.integer.int_kind;
    if (gcc_mode) {
      /* Unlike standard C, GNU C bases an underlying type on the enumerator
         values that were encountered. */
      /* Create the appropriate type for the enumerator constants. */
      enum_con_type = alloc_type((a_type_kind)tk_integer);
      enum_con_type->variant.integer.int_kind = int_kind;
      enum_con_type->variant.integer.enum_type = FALSE;
      enum_con_type->variant.integer.enum_info.affiliated_type = enum_type;
      set_type_size(enum_con_type);
      /* Apply this type to every enumerator constant. */
      change_enum_constants_type(constant_list, enum_con_type);
    }  /* if */
    if (constant_list == NULL ||
        (min_max_set && unsigned_int_kind_of[int_kind] != int_kind &&
         in_range_for_integer_kind(
                      min_value, max_value, unsigned_int_kind_of[int_kind]))) {
      /* GNU C prefers an unsigned underlying type if none of the
         enumerator constants were negative.  Note that this does not affect
         the type of the enumerator constants themselves.  GNU C++ also
         produces the unsigned type when tested with __underlying_type, but
         does not use that unsigned type for promotion purposes (we cannot
         change the underlying type in the IL representation since that is
         what we use for promotion purposes). */
      if (gcc_mode) {
        enum_type->variant.integer.int_kind = unsigned_int_kind_of[int_kind];
      } else {
        integer_type_supp(enum_type)
                                 ->underlying_type_should_use_unsigned = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  /* Set the type size (based on the integral type it is mapped onto). */
  set_type_size(enum_type);
  enum_type->incomplete = FALSE;
  integer_type_supp(enum_type)->enumerator_list_complete = TRUE;
  if (!C_mode() || explicit_base_kind != ik_none) {
    /* In C++ now that we know the type of the enumeration, we can update
       each constant to share the same type.  This also applies in C modes if
       the enumeration has an explicit underlying type. */
    change_enum_constants_type(constant_list, enum_type);
  }  /* if */
  /* If entities dependent on this enum type were declared before it was
     defined, they will have been recorded on a fixup list.  Go through
     the fixup list and complete the declarations. */
  check_dependent_type_fixup_list(tag_sym);
  /* Issue a warning if the current token is in a file different from the
     last token of the enum definition. */
  check_for_file_with_unterminated_type_definition(&end_pos);
  release_local_constant(&max_value);
  release_local_constant(&min_value);
  release_local_constant(&constant);
}  /* scan_enumerator_list */


void enum_specifier(a_decl_parse_state          *dps,
                    a_decl_flag_set             dsi_flags,
                    a_boolean                   vacuous_decl_allowed,
                    a_boolean                   is_enum_template_definition,
                    a_type_ptr                  *type_ptr,
                    an_ms_attribute_ptr         *p_ms_attributes,
                    a_boolean                   *declares_something,
                    a_boolean                   *defines_something,
                    ARG_UNUSED a_decl_pos_block *decl_pos_block)
/*
Scan an enumeration specifier (i.e., the definition of an enumeration type) or
an elaborated name for an enumeration type (e.g., "enum E").  C++11 scoped
enumerations and similar Microsoft extensions are also scanned here.

The type is returned in *type_ptr.  *declares_something is set to indicate
whether or not this specifier declares something.  If defines_something is
non-NULL, *defines_something is set to indicate whether an enumeration is
actually defined.  p_ms_attributes describes Microsoft attributes preceding
the enum specifier (if any).  is_enum_template_definition is TRUE if this is
called for an out-of-class definition (or opaque declaration) of an enum
template.  dsi_flags is the set of input flags passed to decl_specifiers.
*/
{
  a_symbol_locator             locator;
  a_symbol_ptr                 tag_sym;
  a_boolean                    tag_id_present;
  a_type_ptr                   enum_type = NULL, explicit_base = NULL;
  a_boolean                    err = FALSE;
  a_constant_ptr               max_value = local_constant();
  a_constant_ptr               min_value = local_constant();
  a_type_ptr                   class_of_which_a_member;
  an_access_specifier          access;
#if MICROSOFT_EXTENSIONS_ALLOWED
  an_access_specifier          assembly_access;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_scope_depth                effective_decl_level = decl_scope_level;
  a_boolean                    inside_class_definition;
  a_boolean                    is_redeclaration = FALSE, is_definition = FALSE;
  a_boolean                    tag_redefinition = FALSE;
  a_symbol_ptr                 redefined_tag_sym = NULL;
  a_boolean                    namespace_extension_pushed = FALSE;
  a_boolean                    class_reactivation_pushed = FALSE;
  a_source_position            enum_pos, tag_position;
  a_decl_pos_block             local_decl_pos_block;
  a_boolean                    is_predeclared_type_decl = FALSE;
#if MICROSOFT_EXTENSIONS_ALLOWED
  an_extended_decl_info_block  extended_decl_info;
  an_assembly_visibility       cli_visibility =
                                               (an_assembly_visibility)av_none;
  a_source_position            cli_visibility_pos;        
  a_boolean                    new_type_created = FALSE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  an_integer_kind              explicit_base_kind = (an_integer_kind)ik_none;
  a_source_position            pos_explicit_base;
  a_boolean                    is_scoped_enum = FALSE;
  a_boolean                    is_opaque_enum_decl = FALSE;
  a_token_sequence_number      tsn_for_enum;
  a_boolean                    unnamed = FALSE;
  a_boolean                    bool_type = FALSE;
  a_boolean                    is_template_specialization =
                                     (dsi_flags & DSI_IS_SPECIALIZATION) != 0;

  db_enter(3, "enum_specifier");

  *declares_something = FALSE;
  if (scope_is(&scope_stack[decl_scope_level], sck_class_struct_union)) {
    class_of_which_a_member = scope_stack[decl_scope_level].assoc_type;
    access = enum_cast<an_access_specifier>(
                                 scope_stack[decl_scope_level].current_access);
#if MICROSOFT_EXTENSIONS_ALLOWED
    assembly_access = enum_cast<an_access_specifier>(
                        scope_stack[decl_scope_level].current_assembly_access);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    inside_class_definition = TRUE;
  } else {
    class_of_which_a_member = NULL;
    access = (an_access_specifier)as_public;
#if MICROSOFT_EXTENSIONS_ALLOWED
    assembly_access = (an_access_specifier)as_public;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    inside_class_definition = FALSE;
  }  /* if */
  clear_decl_pos_block(&local_decl_pos_block);
  enum_pos = pos_curr_token;
  tsn_for_enum = curr_token_sequence_number;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  local_decl_pos_block.specifiers_range.start = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cli_or_cx_enabled) {
    cli_visibility = scan_cli_visibility_specifier_if_any(&cli_visibility_pos);
  }  /* if */
  if (curr_token == tok_enum_class || curr_token == tok_enum_struct) {
    /* In C++/CLI mode and in Microsoft modes with microsoft_version >=1700,
       "enum struct" and "enum class" is scanned as a single token with
       embedded white space. */
    check_assertion(cli_or_cx_enabled ||
                    (ms_extensions && microsoft_version >= 1700));
    is_scoped_enum = TRUE;
  } else {
    check_assertion(curr_token == tok_enum);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* Skip over "enum". */
  (void)get_token();
  /* In C++11, scoped enums are declared with "enum class" or "enum struct".
     In a combination of Microsoft and C++11 modes, however, we may already
     have seen a tok_enum_class or tok_enum_struct keyword (with embedded
     space) and we should not accept another "class" or "struct".  GCC 4.4 and
     later also accept the C++11 feature in non-C++11 mode with a warning. */
  if ((cpp11_mode || (gpp_mode && gnu_version >= 40400)) &&
      (curr_token == tok_class || curr_token == tok_struct) &&
      !is_scoped_enum) {
    if (!cpp11_mode) {
      pos_warning(ec_scoped_enum_nonstandard_in_current_mode, &enum_pos);
    }  /* if */
    is_scoped_enum = TRUE;
    (void)get_token();
  }  /* if */
  dps->tag_attributes = scan_attributes(al_tag_name);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (ms_extensions) {
    /* Scan Microsoft-specific modifiers.  Most are invalid or ignored, but
       __declspec(uuid(...)) will be recorded in C++ mode. */
    clear_extended_decl_info_block(extended_decl_info);
    scan_extended_decl_modifiers(&extended_decl_info, &dps->tag_attributes,
                                 al_tag_name, /*is_enum_decl=*/TRUE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  /* If there is an identifier next, it is a tag.  It can be the declaration
     of a new tag or a reference to an existing tag.  The "expr context"
     flag is used to improve error recovery (specifically to avoid a
     diagnostic like "E is not a class template" when an enum type is followed
     by a "<"). */
  { an_identifier_options_set gid_options = GID_IS_EXPR_CONTEXT;
    if (is_enum_template_definition) {
      /* For template declarations, find a member of the prototype
         instantiation for something like A<T>::E. */
      gid_options |= GID_USE_PROTOTYPE_NOT_NONREAL;
    }  /* if */
    tag_id_present = is_generalized_identifier_start(gid_options);
  }
  tag_position = pos_curr_token;
  error_position = tag_position;
  if (tag_id_present) {
    a_boolean   tag_resolution;
    a_boolean   is_friend_decl = extended_friends_enabled &&
                                 (dps->dso_flags & DSO_FRIEND);
    /* It seems that appearance of a tag name is a declaration of the
       tag, even if it just repeats a previous name.  At least, there's
       a Plum Hall test that implies that. */
    *declares_something = TRUE;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    local_decl_pos_block.identifier_range.start = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    clear_locator(&locator, &null_source_position);
    tag_sym = scan_tag_name((a_symbol_kind)sk_enum_tag, &locator,
                            &is_friend_decl, /*is_specialization=*/FALSE,
                            &vacuous_decl_allowed,
                            (dsi_flags & DSI_IS_NEW_TYPE_NAME) != 0,
                            (dsi_flags & DSI_NO_TAG_DEFINITION) != 0,
                            /*is_event_interface=*/FALSE,
                            &effective_decl_level, &tag_resolution,
                            &tag_redefinition, &is_predeclared_type_decl,
                            &local_decl_pos_block);
    if (tag_redefinition) {
      /* This definition redeclares a tag that is already defined in this
         scope.  Set the tag symbol aside and proceed as though the tag were
         being introduced here, so that the definition is scanned into a type
         of its own; once the two types have been compared, the tag continues
         to denote the type declared earlier. */
      redefined_tag_sym = tag_sym;
      tag_sym = NULL;
    }  /* if */
    is_definition = tag_definition_next(
                                 curr_token, (a_symbol_kind)sk_enum_tag,
                                 (dsi_flags & DSI_IS_NEW_TYPE_NAME) != 0,
                                 (dsi_flags & DSI_NO_TAG_DEFINITION) != 0);
    if (tag_sym != NULL && is_enum_template_definition) {
      /* This is the definition of an enum template.   Use the enum symbol,
         not the nonreal one. */
      tag_sym = nested_prototype_type_for_nonreal_type(tag_sym);
    }  /* if */
    if (tag_resolution) {                            
      /* Resolution of a previous incomplete declaration. */
      err = FALSE;
      if (effective_decl_level != decl_scope_level) {
        class_of_which_a_member = NULL;
        access = (an_access_specifier)as_public;
      }  /* if */
      /* Check for cases where the definition appears in an invalid
         namespace. */
      if (!microsoft_mode && !gpp_version_is(any_version) &&
          tag_sym != NULL && sym_is_namespace_member(tag_sym) &&
          namespace_scope_should_be_pushed(tag_sym, &tag_position, &err,
                                           /*inline_namespace=*/FALSE) &&
          err) {
        /* Diagnose a case like:
             namespace N1 { enum class E; }
             namespace N2 {
               using N1::E;
               enum class E { e };  // Invalid scope for definition.
             }
        */
        tag_sym = NULL;
        set_to_error_locator(locator);
      }  /* if */
    } else if (tag_sym != NULL && is_definition) {
      /* This is a definition of an enumeration that has previously been
         declared. */
      if (tag_sym->is_class_member) {
        /* C++11 enabled out-of-class definitions of enum type when it added
           opaque enum declarations.  Microsoft compilers have allowed this for
           a long time. */
        if ((opaque_enum_decls_enabled || ms_extensions) &&
            class_of_which_a_member == NULL) {
          a_type_ptr  qualifier_class = qualifier_class_type(locator);
          /* An out-of-class definition of a class member enum: Reactivate the
             class scope. */
          class_of_which_a_member = sym_parent_class(tag_sym);
          if (!same_entities(qualifier_class, class_of_which_a_member)) {
            /* Something like:
                 struct B { enum class E; }; struct D: B {};
                 enum D::E { e };  // Invalid.
            */
            pos_ty_error(ec_bad_qualifier_for_member_enum_decl,
                         &locator.source_position, type_symbol_type(tag_sym));
          }  /* if */
          push_class_reactivation_scope(class_of_which_a_member,
                                        /*extend_namespace=*/FALSE);
          class_reactivation_pushed = TRUE;
          effective_decl_level = depth_scope_stack;
        } else if (!same_entities(sym_parent_class(tag_sym),
                                  class_of_which_a_member)) {
          /* This is an attempt to define a member enum outside the class of
             which it is a member. */
          pos_sy_error(ec_bad_scope_for_definition, &tag_position, tag_sym);
          tag_sym = NULL;
          set_to_error_locator(locator);
        }  /* if */
      } else if (sym_is_namespace_member(tag_sym)) {
        err = FALSE;
        if (namespace_scope_should_be_pushed(tag_sym, &tag_position, &err,
                                             /*inline_namespace=*/FALSE)) {
          /* Push a namespace extension scope. */
          push_namespace_extension_scope(sym_parent_namespace(tag_sym));
          namespace_extension_pushed = TRUE;
          effective_decl_level = depth_scope_stack;
        } else if (err) {
          /* An error was issued by the subroutine. */
          tag_sym = NULL;
          set_to_error_locator(locator);
        }  /* if */
      } else if (tag_sym->decl_scope == file_scope_number &&
                 scope_stack[effective_decl_level].kind !=
                                                     (a_scope_kind)sck_file) {
        /* An attempt to define a file-scope enum in a scope other than the
           file scope. */
        pos_sy_error(ec_bad_scope_for_definition, &tag_position, tag_sym);
        tag_sym = NULL;
        set_to_error_locator(locator);
      }  /* if */
    } else if (is_error_locator(locator) && !is_definition) {
      /* There was an error is looking up the tag, and this is not a
         definition.  For error recovery, return an error type. */
      *type_ptr = error_type();
      goto return_point;
    } else if (tag_sym == NULL && class_of_which_a_member != NULL &&
               effective_decl_level != decl_scope_level) {
      /* This is a (non-standard) forward declaration of a nonclass that
         appears inside a class definition. */
      class_of_which_a_member = NULL;
      access = (an_access_specifier)as_public;
    }  /* if */
  } else {
    /* No tag identifier present. */
    tag_sym = NULL;
    set_to_error_locator(locator);
    tag_position = pos_curr_token;
    /* A brace or colon at this point usually means this is a definition, but
       in trailing return types, a brace that follows is treated as the
       beginning of the function (or lambda) body that follows (and that's an
       error). */
    is_definition = tag_definition_next(
                                 curr_token, (a_symbol_kind)sk_enum_tag,
                                 (dsi_flags & DSI_IS_NEW_TYPE_NAME) != 0,
                                 (dsi_flags & DSI_NO_TAG_DEFINITION) != 0);
    if (!is_definition) {
      /* Neither the tag id nor the enum definition is present.  This is an
         error. */
      add_stop_token(tok_lbrace);
      syntax_error((dsi_flags & DSI_NO_TAG_DEFINITION) != 0 || is_scoped_enum ?
                                ec_exp_identifier : ec_exp_definition_of_tag);
      remove_stop_token(tok_lbrace);
      /* This statement might have declared something, but since we're
         scanning past the relevant tokens we'll never know.  Set the flag
         to TRUE anyway, to avoid other errors down the line. */
      *declares_something = TRUE;
      /* Use an error type for error recovery. */
      *type_ptr = error_type();
      goto return_point;
    } else if (is_scoped_enum) {
      pos_error(ec_unnamed_scoped_enum, &pos_curr_token);
    }  /* if */
  }  /* if */
  if (is_definition) {
    /* Enum definitions cannot appear as part of function declarations.  There
       is therefore no need to keep caching tokens in case this turns out to
       be an abbreviated function template declaration. */
    end_potential_abbr_func_templ_caching(dps);
    if (explicit_enum_base_enabled) {
      explicit_base_kind = scan_explicit_enum_base_type(&explicit_base,
                                                        &pos_explicit_base,
                                                        &bool_type);
      if (C_mode() && explicit_base_kind != ik_none && tag_sym != NULL &&
          !type_symbol_type(tag_sym)->variant.integer.has_explicit_enum_base) {
        /* Something like "enum E; enum E: int;".  Note that "enum E;" is
           nonstandard, but accepted in some modes. */
        pos_sy_error(ec_enum_previously_declared_without_explicit_base,
                     &pos_explicit_base, tag_sym);
      }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (microsoft_mode && tag_sym != NULL && !tag_sym->defined &&
          symbol_is(tag_sym, sk_enum_tag) &&
          explicit_base_kind != (an_integer_kind)ik_none &&
          explicit_base_kind != (an_integer_kind)ik_int) {
        /* Microsoft compilers allow:
             enum E ee; // E considered complete with underlying type int.
             enum E: char { e };  // New type E (incompatible with previous E).
           Ignore the previous declaration of E if necessary. */
        a_type_ptr  prev_type = type_symbol_type(tag_sym);
        if (is_immediate_enum_type(prev_type) &&
            !prev_type->variant.integer.has_explicit_enum_base &&
            !prev_type->variant.integer.is_scoped_enum) {
          pos_sy_warning(ec_enum_type_replacement, &tag_position, tag_sym);
          tag_sym->variant.enumeration.extra_info->replaced_enum_symbol = TRUE;
          tag_sym->is_invisible = TRUE;
          tag_sym = NULL;
        }  /* if */
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
  }  /* if */
  if (opaque_enum_decls_enabled &&
      ((explicit_base_kind != (an_integer_kind)ik_none &&
        curr_token != tok_lbrace) ||
       (is_scoped_enum &&
        (curr_token == tok_semicolon ||
         curr_token == tok_removed_template_body)))) {
    /* An opaque enum declaration.  This is an enum declaration that fixes the
       size of the type (i.e., it is "complete") without defining the
       associated enumeration constants.  The standard requires this to be
       followed by a semicolon, and in the case of an enum definition whose
       body has been separated for template instantiation purposes, we also
       treat the left-over declaration as being an opaque enum declaration.
       Microsoft and clang, however, allow code like
         enum E: int x;
       (i.e., non-autonomous opaque enum declarations). */
    if (curr_token != tok_semicolon &&
        curr_token != tok_removed_template_body &&
        !(microsoft_mode || clang_mode)) {
      pos_error(ec_non_autonomous_opaque_enum_decl, &tag_position);
    } else if (dps->is_type_name) {
      /* The C++11 standard does not permit an opaque enum declaration in a
         type-id, but most compilers appear to currently accept this. */
      pos_diagnostic(strict_ansi_discretionary_severity,
                     ec_nonstd_opaque_enum_decl_in_type_id, &tag_position);
    }  /* if */
    is_opaque_enum_decl = TRUE;
    is_definition = FALSE;
  }  /* if */
  if (is_definition && tag_sym != NULL && tag_sym->defined) {
    /* Catch errors like "enum A { e }; enum ::A { f };". */
    enum_type = type_symbol_type(tag_sym);
    if (c23_mode && !tag_sym->is_error && curr_token == tok_lbrace) {
      /* C23 allows a second enumerator list for a tag already defined in
         this scope.  scan_tag_name does not set tag_redefinition when an
         explicit underlying type precedes the braces, so handle that
         case here. */
      redefined_tag_sym = tag_sym;
      tag_sym = NULL;
    } else if (c23_mode && !tag_sym->is_error) {
      /* An opaque "enum E : T;" after a definition redeclares the
         existing type; it is not a C23 redefinition. */
      is_definition = FALSE;
      if (explicit_base_kind != (an_integer_kind)ik_none) {
        is_opaque_enum_decl = TRUE;
      }  /* if */
    } else {
      if (is_template_specialization &&
          !enum_type->variant.integer.is_specialized) {
        an_enum_symbol_supplement_ptr	essp;
        essp = tag_sym->variant.enumeration.extra_info;
        if (is_scoped_enum) {
          pos2_sy_diagnostic(es_error,
                             ec_specialization_of_referenced_entity_pos,
                             &locator.source_position,
                             &essp->instantiation_position,
                             tag_sym);
        } else {
          pos_diagnostic(es_error,
                         ec_specialization_of_unscoped_enum,
                         &locator.source_position);
        }  /* if */
      } else {
        issue_redef_diag(&locator.source_position, tag_sym);
      }  /* if */
      set_to_error_locator(locator);
      tag_sym = NULL;
    }  /* if */
  }  /* if */
  dps->tag_def_or_forward_decl = is_definition || curr_token == tok_semicolon;
  if (tag_sym != NULL) {
    /* Using an existing type.  Fetch the enumerated type pointer from it. */
    enum_type = type_symbol_type(tag_sym);
    is_redeclaration = TRUE;
    dps->redeclares_tag = TRUE;
    if (scope_is(&scope_stack_top(), sck_class_struct_union) &&
        locator.is_qualified_name /*lint !e530*/ && ((!gpp_mode &&
        (dps->tag_def_or_forward_decl || is_opaque_enum_decl)) ||
        (gpp_mode && (is_definition || is_opaque_enum_decl))) &&
        !(dps->dso_flags & DSO_FRIEND)){
       /* Qualified enum names within a class are not allowed, unless they
          appear in a friend declaration. For gpp_mode, allow qualified
          useless member declarations. */
       pos_error(ec_qualified_name_not_allowed, &locator.source_position);
       tag_sym = NULL;
       set_to_error_locator(locator);
    } else if (is_immediate_enum_type(enum_type)) {
      /* C++/CLI does not permit a type first declared with "enum class" or
         "enum struct" to later be referred to with just "enum", nor vice
         versa.  C++11 also disallows the mismatch for opaque declarations
         and definitions, but for elaborated enum specifiers only "enum" is
         allowed (the latter rule came along with the introduction of opaque
         enum declarations into the language). */
      if (cli_or_cx_enabled || (opaque_enum_decls_enabled &&
                                (is_definition || is_opaque_enum_decl))) {
        if (is_scoped_enum != integer_type_is_scoped_enum(enum_type)) {
          pos_sy_error(ec_incompatible_enum_kinds, &locator.source_position,
                       tag_sym);
          /* If this is a definition, it determines whether the type is
             scoped for error recovery purposes.  Otherwise, the prior
             declaration prevails. */
          if (is_definition) {
            enum_type->variant.integer.is_scoped_enum = is_scoped_enum;
          } else {
            is_scoped_enum = enum_type->variant.integer.is_scoped_enum;
          }  /* if */
        }  /* if */
      } else if (opaque_enum_decls_enabled && is_scoped_enum) {
        /* "enum struct" or "enum class" cannot be used for an elaborated
           specifier, even if the original declaration is for a scoped enum. */
        an_error_severity  sev = es_error;
        if (enum_type->variant.integer.is_scoped_enum) {
          /* Microsoft and GNU compilers accept the "enum class" and
             "enum struct" forms if the enumeration is known to be scoped. */
          if (microsoft_mode) {
            sev = es_none;
          } else if (gpp_mode && !clang_mode) {
            sev = es_warning;
          } else {
            sev = es_discretionary_error;
          }  /* if */
        }  /* if */
        if (sev != es_none) {
          pos_diagnostic(sev, ec_invalid_scoped_enum_elaboration,
                         &locator.source_position);
        }  /* if */
        is_scoped_enum = FALSE;
      }  /* if */
      /* If explicit base type specifiers are involved, ensure that the
         underlying types are compatible. */
      if (!enum_type->incomplete && (is_definition || is_opaque_enum_decl)) {
        a_type_ptr  old_base_type = integer_type_supp(enum_type)->base_type;
        a_type_ptr  new_base_type = explicit_base;
        /* For scoped enums without an explicit base type use "int".
           (Microsoft compilers seem to also assume "int" for non-scoped
           enums.) */
        if ((is_scoped_enum || microsoft_mode) && new_base_type == NULL) {
          new_base_type = integer_type((an_integer_kind)ik_int);
        }  /* if */
        if ((is_scoped_enum || microsoft_mode) && old_base_type == NULL) {
          old_base_type = integer_type((an_integer_kind)ik_int);
        }  /* if */
        if ((old_base_type != NULL || new_base_type != NULL) &&
            (old_base_type == NULL || new_base_type == NULL ||
             !identical_types_ignoring_qualifiers(old_base_type,
                                                  new_base_type))) {
          pos_sy_error(ec_incompatible_enum_base_types,
                       &locator.source_position, tag_sym);
          /* Continue as if no explicit base was specified for better error
             recovery. */
          explicit_base = NULL;
          explicit_base_kind = (an_integer_kind)ik_none;
        }  /* if */
        if (enum_type == type_of_align_val_t) {
          /* This is the explicit definition of the predeclared
             std::align_val_t.  It was initially marked as invisible so
             that it could not be used in a program before a declaration
             was seen.  Mark it as visible now. */
          tag_sym->is_invisible = FALSE;
        }  /* if */
      }  /* if */
    } else if ((tag_sym->is_template_param ||
                (tag_sym->is_nonreal_member &&
                 is_type_template_param_symbol(tag_sym))) &&
                !is_definition && !is_opaque_enum_decl) {
      /* During prototype instantiations, treat "enum S<T>::E *p;" (with T a
         template parameter) like "typename S<T>::E *p;".  In nonstrict modes,
         "enum T x;" is also accepted and treated like "T x;". */
      mark_referenced(tag_sym, &locator.source_position);
      *declares_something = FALSE;
      *type_ptr = enum_type;
      goto return_point;
    } else if (tag_sym->is_nonreal_nested_type){
      /* Use the enum symbol, not the nonreal one. */
      tag_sym = nested_prototype_type_for_nonreal_type(tag_sym);
      enum_type = type_symbol_type(tag_sym);
    } else {
      pos_sy_error(ec_not_an_enum_type_name, &locator.source_position,
                   tag_sym);
      tag_sym = NULL;
      set_to_error_locator(locator);
    }  /* if */
    /* Record cross-reference information. */
    if (tag_sym == NULL) {
      /* An error occurred. */
    } else if (is_definition) {
      mark_defined(tag_sym, &locator.source_position);
      /* If this declaration is associated with a declaration statement, update
         the associated stmk_decl statement. */
      record_entity_in_decl_stmt_if_needed(tag_sym);
      if (!C_mode() && inside_class_definition) {
        /* enum_type is a class member and is being defined having been
           forward-declared. */
        check_assertion(tag_sym->is_class_member == TRUE);
        if (enum_type->source_corresp.access != access) {
          /* The access specified for the previous declaration does not
             correspond to the access for current declaration. */
          pos_sy_diagnostic(strict_ansi_mode ?
                              strict_ansi_discretionary_severity :
                              es_warning,
                            ec_redecl_changes_access,
                            &locator.source_position, tag_sym);
           /* Since this is a definition, use the current access instead of
             that specified on the original declaration. */
          enum_type->source_corresp.access = access;
        }  /* if */
      }  /* if */
    } else if (is_opaque_enum_decl ||
               (!strict_ansi_mode && curr_token == tok_semicolon)) {
      /* An opaque enum declaration or a (nonstandard and useless)
         redeclaration of an enum tag. */
      mark_declared(tag_sym, &locator.source_position);
      /* If this declaration is associated with a declaration statement, update
         the associated stmk_decl statement. */
      record_entity_in_decl_stmt_if_needed(tag_sym);
    } else {
      mark_referenced(tag_sym, &locator.source_position);
      *declares_something = FALSE;
    }  /* if */
  }  /* if */
  if ((tag_sym == NULL || !tag_sym->is_class_member ||
       !sym_parent_class(tag_sym)->
                     variant.class_struct_union.is_prototype_instantiation) &&
      is_enum_template_definition && !locator.is_error /*lint !e530*/) {
    /* A template declaration must always refer to an enumeration declared
       in a class template. */
    pos_error(ec_nonmember_enum_template, &locator.source_position);
    tag_sym = NULL;
    set_to_error_locator(locator);
  }  /* if */
  if (tag_sym == NULL) {
    an_enum_symbol_supplement_ptr	essp;
    a_scope_ptr				parent_scope;
    parent_scope = scope_stack[effective_decl_level].il_scope;
    /* Create a new enumerated type.  All enumeration type entries are
       allocated in the file scope memory region. */
#if MICROSOFT_EXTENSIONS_ALLOWED
    new_type_created = TRUE;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    enum_type = alloc_type((a_type_kind)tk_integer);
    enum_type->incomplete = TRUE;
    if (scope_stack[effective_decl_level].in_prototype_instantiation ||
        scope_stack[effective_decl_level].in_nonreal_instantiation) {
      enum_type->variant.integer.is_nonreal = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (class_of_which_a_member != NULL &&
          class_of_which_a_member->variant.class_struct_union
                                          .is_ms_instantiated_nonreal_class) {
        enum_type->variant.integer.is_ms_instantiated_nonreal_enum = TRUE;
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    }  /* if */
    if (parent_scope != NULL && in_file_scope(parent_scope)) {
      /* Record the parent_scope if it is nonlocal so parent classes or parent
         namespaces are available.  (This is recorded in all cases when the
         type is added to the types list, but the parent class/namespace is
         sometimes accessed before that.) */
      enum_type->source_corresp.parent_scope = parent_scope;
    }  /* if */
    is_redeclaration = FALSE;
    if (((!C_mode() || strict_ansi_mode) && !microsoft_mode) &&
        !(is_definition || is_scoped_enum || is_opaque_enum_decl) &&
        !is_error_locator(locator)) {
      /* Since tag_sym was not found, this is either a vacuous declaration or a
         reference to an incomplete (because not yet declared) type.  In either
         case this is non-standard for enums.  It is allowed as an extension by
         analogy with classes. */
      an_error_severity severity = es_discretionary_error;
      an_error_code     err_code = ec_nonstd_forward_decl_enum;
      if (C_mode()) {
        /* In C mode this diagnostic is only issued in strict mode.  Use the
           appropriate strict severity. */
        severity = strict_ansi_discretionary_severity;
      } else if (curr_token == tok_colon && explicit_enum_base_enabled &&
                 scope_is(&scope_stack_top(), sck_class_struct_union)) {
        /* Something like:
             struct S { enum X: Y; };
           where Y isn't a type name.  This could be intended as a bit field
           declaration with an incomplete enumeration type (an error) or as a
           malformed opaque-enum-declaration (also an error).  Provide a
           message that explains the possibilities. */
        severity = es_error;
        err_code = ec_incomplete_enum_bit_field_or_bad_opaque_enum;
        enum_type->incomplete = FALSE;
      }  /* if */
      pos_diagnostic(severity, err_code, &locator.source_position);
    }  /* if */
    /* set_type_size is called later, once the final type is known. */
    /* Set a default representation of "int", which may be adjusted later. */
    enum_type->variant.integer.int_kind = (an_integer_kind)ik_int;
    enum_type->variant.integer.enum_type = TRUE;
    enum_type->variant.integer.enum_info.constant_list = NULL;
    if (scope_stack[effective_decl_level].kind ==
                                           (a_scope_kind)sck_func_prototype) {
      enum_type->declared_in_function_prototype = TRUE;
    }  /* if */
    /* Enter a new tag symbol, if a tag id was specified (a tag is not
       specified in something like "enum {a, b, c}"). */
    if (redefined_tag_sym != NULL) {
      /* A redefinition of a tag.  Give the type a symbol that carries the
         tag's name, for the sake of diagnostics and of the comparison against
         the earlier definition, but do not enter that symbol in the symbol
         table: The tag continues to denote the type declared earlier. */
      tag_sym = make_unentered_symbol(sk_enum_tag, redefined_tag_sym->header,
                                      &locator.source_position);
      *declares_something = TRUE;
      set_source_corresp(&(enum_type->source_corresp), tag_sym);
      tag_sym->variant.enumeration.type = enum_type;
      enum_type->is_tag_redefinition = TRUE;
    } else if (tag_id_present) {
      tag_sym = enter_local_symbol((a_symbol_kind)sk_enum_tag, &locator,
                                   effective_decl_level,
                                   /*suppress_redecl_error=*/FALSE);
      *declares_something = TRUE;
      set_source_corresp(&(enum_type->source_corresp), tag_sym);
      tag_sym->variant.enumeration.type = enum_type;
    } else {
      /* Unnamed enum.  Create a symbol to represent it. */
      tag_sym = make_unnamed_tag_symbol((a_symbol_kind)sk_enum_tag,
                                        &pos_curr_token);
      set_source_corresp(&(enum_type->source_corresp), tag_sym);
      clear_source_corresp_name(&enum_type->source_corresp);
      tag_sym->variant.enumeration.type = enum_type;
      /* set_source_corresp and mark_defined are not called, so clear the
         reference flag and copy in the decl position manually. */
      enum_type->source_corresp.referenced = FALSE;
      enum_type->source_corresp.decl_position = locator.source_position;
      enum_type->variant.integer.originally_unnamed = TRUE;
      unnamed = TRUE;
    }  /* if */
    essp = tag_sym->variant.enumeration.extra_info;
#if NEED_NAME_MANGLING
    /* The mangled names of local types and unnamed types in namespace scope
       are distinguished using a unique number ("discriminator").  Compute this
       number now if appropriate.  The notion of "discriminator" here is a
       generalization of the one defined in the IA-64 ABI. */
    compute_name_collision_discriminator(tag_sym, effective_decl_level);
#endif /* NEED_NAME_MANGLING */
    if (!C_mode()) {
      if (class_of_which_a_member != NULL) {
        /* Add a pointer to the parent class in the symbol and the type. */
        set_class_membership(tag_sym, &enum_type->source_corresp,
                             class_of_which_a_member);
        /* If this is an enum nested in a prototype instantiation, create
           the nonreal version of the type. */
        check_for_nested_type_of_prototype_instantiation(tag_sym);
      } else if (scope_stack[effective_decl_level].kind ==
                                     (a_scope_kind)sck_namespace ||
                 scope_stack[effective_decl_level].kind ==
                                     (a_scope_kind)sck_namespace_extension) {
        /* Set the parent namespace. */
        set_namespace_membership(tag_sym, &enum_type->source_corresp,
                                 scope_stack[effective_decl_level].
                                         il_scope->variant.assoc_namespace);
      }  /* if */
      if (depth_innermost_function_scope == NO_SCOPE_NUMBER &&
          !inside_local_class) {
        /* Enum declaration is not local to a function. */
        set_name_linkage_for_type(enum_type);
      }  /* if */
    }  /* if */
    if (tag_sym->is_error) {
      /* Suppress the template processing below in error cases. */
    } else if (class_of_which_a_member == NULL) {
      /* Only member enumerations need special template processing. */
#if MICROSOFT_EXTENSIONS_ALLOWED
    } else if (cli_or_cx_enabled &&
               is_managed_class_type(class_of_which_a_member)) {
      /* Suppress the template processing for C++/CLI generics and classes. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    } else if (!opaque_enum_decls_enabled) {
      /* Suppress template processing if opaque enums are not allowed. */
    } else if (curr_scope_is_class_template_definition()) {
      a_template_symbol_supplement_ptr	tssp;
      tssp = alloc_template_symbol_supplement((a_symbol_kind)sk_enum_tag);
      essp->template_info = tssp;
      enum_type->variant.integer.is_prototype_instantiation = TRUE;
      tssp->token_sequence_number = tsn_for_enum;
      enum_type->variant.integer.is_template_enum = TRUE;
      tssp->variant.class_template.prototype_instantiation = tag_sym;
    } else if (!unnamed && curr_scope_is_class_instantiation()) {
      /* Find the enum declaration from the prototype instantiation
         (if any). */
      a_template_symbol_supplement_ptr	tssp;
      a_symbol_ptr			template_sym;
      enum_type->variant.integer.is_template_enum = TRUE;
      find_enum_member(tag_sym, sym_parent_class(tag_sym), tsn_for_enum);
      template_sym = essp->template_sym;
      tssp = template_sym == NULL
                ? NULL
                : template_sym->variant.enumeration.extra_info->template_info;
      check_assertion_or_expect_error(tssp != NULL);
      if (tssp != NULL && !is_scoped_enum) {
        /* For a non-scoped enum that was defined outside of its class,
           rescan the tokens of the enumerator list where they would appear
           if the enumeration were defined inside the class. */
        a_template_cache_ptr			tcp;
        tcp = cache_for_template(tssp);
        if (!tcp->tokens.is_empty()) {
          rescan_reusable_cache(tcp->tokens);
          is_definition = TRUE;
          /* Save the position of the reference that caused the
             instantiation. */
          essp->instantiation_position = pos_curr_token;
        }  /* if */
      }  /* if */
    }  /* if */
    /* When an enumeration is defined within a class definition, its access
       should be set based on the access recorded in the current scope stack
       entry. */
    enum_type->source_corresp.access = access;
#if MICROSOFT_EXTENSIONS_ALLOWED
    enum_type->source_corresp.assembly_access = assembly_access;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (tag_id_present) {
      /* Note that mark_defined and mark_referenced are called after the
         namespace/class membership has been specified. */
      if (is_definition) {
        mark_defined(tag_sym, &locator.source_position);
      } else {
        mark_declared(tag_sym, &locator.source_position);
      }  /* if */
      /* If this declaration is associated with a declaration statement, update
         the associated stmk_decl statement. */
      record_entity_in_decl_stmt_if_needed(tag_sym);
#if GENERATE_SOURCE_SEQUENCE_LISTS
      update_sse_for_first_tag_declaration(enum_type, &locator,
                                           is_definition,
                                           /*marked_as_gnu_extension=*/FALSE);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    } else if (is_definition) {
      /* An unnamed enum type.  mark_defined can't be called to record the
         declaration, so the subroutines that do so are called directly. */
#if MAINTAIN_CLASS_MEMBER_LIST
      record_class_member_declaration((char*)enum_type, iek_type);
#endif /* MAINTAIN_CLASS_MEMBER_LIST */
#if GENERATE_SOURCE_SEQUENCE_LISTS
      record_entity_in_decl_stmt_if_needed(tag_sym);
      if (!source_sequence_entries_disallowed) {
        f_update_source_sequence_list((char*)enum_type, iek_type,
                                      (a_source_sequence_entry_ptr)NULL);
      }  /* if */
#endif  /* GENERATE_SOURCE_SEQUENCE_LISTS */
    }  /* if */
    /* Wait to add the type to the types list; it should not be added
       until the closing brace of the full definition appears, to get the
       IL list in the right order. */
  }  /* if */
  /* Attach attributes, if any.  One item of note: any attributes that
     affect the alignment of this incomplete type will be handled later
     (once the underlying type has been determined) either in set_type_size
     or explicitly for an opaque enum definition. */
  attach_tag_attributes(dps->tag_attributes, enum_type, dps, is_definition,
                        !is_definition &&
                        (is_opaque_enum_decl ||
                         (curr_token == tok_semicolon && !strict_ansi_mode)),
                        /*ignore_gnu_attributes=*/!is_definition);
  /* If any declaration or definition of this enum contains a standard
     alignment attribute, process it. */
  record_strongest_alignment_attr(dps, iek_type, &enum_type->source_corresp,
                                  is_redeclaration, is_definition);
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (new_type_created && !is_scoped_enum && explicit_base_kind == ik_none &&
      (microsoft_mode || (clang_mode && ms_compat))) {
    /* In Microsoft compatibility mode (unscoped) enum types can be declared
       without being defined and can also be used.  The use requires that
       the size be set. */
    check_assertion(!enum_types_can_be_smaller_than_int);
    set_type_size(enum_type);
    enum_type->incomplete = FALSE;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
  /* Record whether or not the enum has an inline namespace with an abi_tag as
     a parent. */
  enum_type->in_gnu_abi_tag_namespace =
                                    scope_stack_top().in_gnu_abi_tag_namespace;
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (curr_token == tok_removed_template_body) {
    /* A scoped enum defined in a class template has its enumerator list
       replaced with a tok_removed_template_body token (it is thus treated as
       an opaque enumerator declaration).  Skip over the placeholder token. */
    (void)get_token();
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cli_or_cx_enabled) {
    set_cli_visibility(enum_type, cli_visibility, &cli_visibility_pos,
                       is_definition);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  if (is_definition || is_opaque_enum_decl) {
    if (explicit_base != NULL) {
      a_type_ptr  utp = skip_typerefs(explicit_base);
      /* Record the explicit underlying type as it appeared in the source.
         Prefer the spelling from the definition. */
      if (is_definition || integer_type_supp(enum_type)->base_type == NULL) {
        integer_type_supp(enum_type)->base_type = explicit_base;
      }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
      if (!is_definition && is_opaque_enum_decl) {
        /* Record the underlying type as written in this opaque declaration. */
        (void)set_src_seq_secondary_decl_fields((char *)enum_type,
                                                explicit_base,
                                                (a_name_reference_ptr)NULL,
                                                SSSD_NO_FLAGS);
      }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      if (C_mode() && type_is(utp, tk_integer)) {
        /* In C, enumerator constants immediately have the enumeration type
           with the explicitly-specified (unqualified) underlying type (a C23
           feature also support in some pre-C23 modes). */
        enum_type->variant.integer.int_kind = utp->variant.integer.int_kind;
      }  /* if */
      /* Update the position of the base type if this is the first opaque
         declaration or if it is the definition. */
      if (is_definition ||
          (enum_type->incomplete &&
           !integer_type_supp(enum_type)->enumerator_list_seen)) {
        integer_type_supp(enum_type)->base_type_position = pos_explicit_base;
      }  /* if */
      enum_type->variant.integer.has_explicit_enum_base = TRUE;
      /* If an explicit base is specified, the type is complete at this
         point. */
      enum_type->incomplete = FALSE;
      enum_type->size = skip_typerefs(explicit_base)->size;
      enum_type->alignment = check_explicit_enum_alignment(enum_type,
                                             alignment_of_type(explicit_base));
    }  /* if */
  }  /* if */
  if (is_scoped_enum) {
    enum_type->variant.integer.is_scoped_enum = TRUE;
  }  /* if */
  if (is_definition) {
    scan_enumerator_list(enum_type, dps, dsi_flags, p_ms_attributes,
                         class_of_which_a_member,
                         declares_something, &local_decl_pos_block);
  } else {
    /* No brace-enclosed list follows. */
    if (is_opaque_enum_decl) {
      set_enum_representation(enum_type, &tag_position, !err,
                              explicit_base_kind, bool_type,
                              /*min_max_set=*/FALSE, min_value, max_value);
      enum_type->incomplete = FALSE;
      set_type_size(enum_type);
    }  /* if */
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (is_definition) {
    if (cppcx_enabled) {
      error_if_cppcx_public_global_type(enum_type, &cli_visibility_pos);
    }  /* if */
  } else {
    if (cli_or_cx_enabled && is_scoped_enum &&
        class_of_which_a_member != NULL &&
        is_managed_class_type(class_of_which_a_member)) {
      /* Scoped enum types in managed classes cannot be "forward-declared"
         (not even using C++11-style opaque-enum declaration syntax). */
      pos_error(ec_enum_in_managed_class_missing_definition, &pos_curr_token);
    }  /* if */
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    /* Copy the end specifiers end position into decl_pos_block. */
    decl_pos_block->specifiers_range.end =
                             local_decl_pos_block.specifiers_range.end;
  }  /* if */
  if (is_definition || !is_redeclaration) {
    /* This is either the definition of the enumeration or its initial
       declaration.  Update the extra source position information in the
       type entry. */
    a_decl_position_supplement_ptr  dpsp = enum_type->
                                               source_corresp.decl_pos_info;
    if (dpsp != NULL) {
      dpsp->specifiers_range = local_decl_pos_block.specifiers_range;
      if (tag_id_present) {
        dpsp->identifier_range = local_decl_pos_block.identifier_range;
      }  /* if */
    }  /* if */
  }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  if (*declares_something && !is_definition) {
    /* Update source range information in the secondary-decl entry. */
    a_source_sequence_entry_ptr     ssep;
    a_src_seq_secondary_decl_ptr    sssdp;
    a_decl_position_supplement_ptr  dpsp;
    /* Ordinarily secondary declarations of enum types (e.g., forward
       declarations) are not allowed in some modes. */
    check_assertion(is_opaque_enum_decl || !strict_ansi_mode ||
                    !is_redeclaration);
    /* Look for the secondary-decl entry. */
    ssep = last_matching_source_sequence_entry((char *)enum_type);
    if (ssep != NULL && ss_entry_kind(ssep) == iek_src_seq_secondary_decl) {
      sssdp = (a_src_seq_secondary_decl_ptr)ssep->entity.ptr;
      check_assertion(sssdp->decl_pos_info == NULL);
      /* Allocate the supplement, set its fields, and link it to the
         secondary-decl entry that was just located for enum_type. */
      dpsp = alloc_decl_position_supplement(in_file_scope(sssdp));
      dpsp->specifiers_range = local_decl_pos_block.specifiers_range;
      if (tag_id_present) {
        dpsp->identifier_range = local_decl_pos_block.identifier_range;
      }  /* if */
      sssdp->decl_pos_info = dpsp;
    }  /* if */
  }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Add the type to the types list for the current scope.  This is done
     after the closing brace, if any, to get the IL types list in the right
     order.  Incomplete enums are added to the types list even though the
     actual definition has not yet appeared; however, it will be reentered
     on the list if and when the definition appears. */
  if (!(tag_sym != NULL && tag_sym->is_template_param) &&
      may_be_added_to_types_list(enum_type, effective_decl_level)) {
    if (!is_redeclaration) {
      /* This is the initial declaration of this enum type. */
      add_to_types_list(enum_type, effective_decl_level);
    } else if (is_definition) {
      /* This is a redeclaration and also a definition.  Remove the enum type
         from the types list and reenter it at the end. */
      move_to_end_of_types_list(enum_type, effective_decl_level);
    }  /* if */
  }  /* if */
  /* If necessary, pop the namespace extension scope or the class
     reactivation scope. */
  if (namespace_extension_pushed) {
    pop_namespace_extension_scope();
  } else if (class_reactivation_pushed) {
    pop_class_reactivation_scope();
  }  /* if */
  if (redefined_tag_sym != NULL) {
    /* Every definition of a tag must define the same type.  Check that, then
       let the specifier yield the type the tag already denotes.  That ensures
       the rest of the front end sees just one type for the tag. */
    a_type_ptr  prev_enum_type = type_symbol_type(redefined_tag_sym);
    if (!err &&
        !c_tagged_types_match(enum_type, prev_enum_type,
                              ttmk_redeclaration)) {
      pos_diagnostic(es_error, ec_tag_redefined_differently, &tag_position,
                     redefined_tag_sym,
                     &prev_enum_type->source_corresp.decl_position);
    }  /* if */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    dps->c23_tag_redefinition_type = enum_type;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    rebind_c23_redefinition_pragmas(enum_type, prev_enum_type);
    enum_type = prev_enum_type;
    tag_sym = redefined_tag_sym;
  }  /* if */
  *type_ptr = enum_type;
  if (defines_something != NULL) *defines_something = is_definition;
return_point:;
  dps->sym = tag_sym;
  release_local_constant(&max_value);
  release_local_constant(&min_value);
  db_exit();
}  /* enum_specifier */


static void process_class_template_placeholder(a_decl_parse_state    *state,
                                               a_type_ptr            type)
/*
Determine whether type is a class template placeholder used for C++17
class template argument deduction, and if so, update state to record the
placeholder.  type is known to be a tk_template_param type.  *state describes
the declaration that is being parsed.
*/
{
  a_template_param_type_supplement_ptr  tptsp;

  check_assertion(type_is(type, tk_template_param));
  tptsp = type->variant.template_param.extra_info;
  if (tptsp->coordinates.depth == CLASS_TEMPLATE_PLACEHOLDER_NESTING_DEPTH) {
    state->has_deduced_type = TRUE;
    state->has_deducible_class_templ_args = TRUE;
    state->auto_type = type;
    state->auto_pos = pos_curr_token;
  }  /* if */
}  /* process_class_template_placeholder */


static void clear_template_deduction_context_flag(a_decl_parse_state  *dps)
/*
Helper callback for check_for_rescannable_alias to reset the flag indicating
whether the declaration described by *dps occurred in a template deduction
context (where embedded expressions may need to be rescanned).
*/
{
  if (dps->last_declarator) {
    scope_stack_top().in_template_deduction_context = FALSE;
  }  /* if */
}  /* clear_template_deduction_context_flag */


void typename_specifier(a_type_ptr                      *type_ptr,
                        a_symbol_ptr	                *type_sym,
                        a_boolean                       within_using_decl,
                        a_boolean                       is_decl_specifier,
                        a_decl_parse_state              *dps,
                        ARG_UNUSED a_decl_pos_block_ptr decl_pos_block)
/*
Scan a typename-specifier.  The identifier that follows the typename keyword
must be a type name, otherwise a diagnostic is issued.  The type is returned in
*type_ptr.  The type symbol is returned in *type_sym.  On return, the current
token is the one following the final identifier above.

within_using_decl is TRUE in a class member using declaration that starts with
"using typename".  If is_decl_specifier is TRUE, this routine is called from
decl_specifiers; in that case, this routine may return NULL in some Microsoft
modes if the tokens following the keyword "typename" do not actually start a
type name.  decl_pos_block is a possibly NULL pointer to a block of source
position information when the context is a declaration.  dps is the current
declaration parse state if the typename-specifier is from a decl-specifier,
or NULL in other contexts such as using-declarations.
*/
{
  a_type_ptr			tp = NULL;
  an_identifier_options_set	options = GID_IS_TYPENAME;
  a_boolean			class_template_allowed;
  a_decl_parse_state		local_dps;

  *type_sym = NULL;
  check_assertion(curr_token == tok_typename);
  /* Class template argument deduction is not done in using-declarations. */
  class_template_allowed = class_template_arg_deduction_enabled &&
                           !within_using_decl;
  /* If no decl_parse_state was provided, create a local one that can be
     used for class template argument deduction. */
  if (class_template_allowed && dps == NULL) {
    init_decl_parse_state(&local_dps);
    dps = &local_dps;
  }  /* if */
  /* The typename keyword may only be used within a template, including the
     template parameter list. */
  if (!is_template_context() && !cpp11_mode) {
    diagnostic(strict_ansi_mode ? strict_ansi_discretionary_severity
                                : es_remark,
               ec_typename_not_in_template);
  }  /* if */
  if (class_template_allowed) {
    /* When class template argument deduction is being done, a class
       template name without an argument list is allowed. */
    options |= GID_TEMPLATE_ARGS_OPTIONAL;
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    /* Assume the current token is the last decl-specifier. */
    decl_pos_block->specifiers_range.end = end_pos_curr_token;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Skip over "typename". */
  (void)get_token();
  /* The Microsoft compiler allows the typename specifier to be repeated. */
  while (microsoft_bugs && curr_token == tok_typename) (void)get_token();
  (void)is_generalized_identifier_start(options);
  if (microsoft_bugs && is_decl_specifier &&
      (curr_token != tok_identifier ||
       !locator_for_curr_id.is_qualified_name ||
       locator_for_curr_id.is_file_scope_qualified_name)) {
    /* An invalid typename specifier that is accepted in Microsoft mode.
       Simply return and let decl_specifiers process the rest of the type. */
    goto done;
  }  /* if */
  if (curr_token != tok_identifier) {
    syntax_error(ec_exp_identifier);
  } else {
    a_boolean	               err = FALSE;
    an_identifier_lookup_mode  ilm;

    ilm = within_using_decl ? ilm_using_typename : ilm_typename;
    if (!coalesce_and_lookup_qualified_name(options, ilm, &err) ||
        (!cli_or_cx_enabled &&
         (!(locator_for_curr_id.is_qualified_name ||
            locator_for_curr_id.is_implicitly_qualified) ||
          err))) {
      /* The identifier scanned is not a class-qualified name,
         namespace-qualified name, or is a qualified name that refers to a
         nonexistent member. */
      if (!err) {
        pos_error(ec_qualified_name_required, &error_position);
      }  /* if */
    } else {
      a_symbol_ptr	sym = locator_for_curr_id.specific_symbol;
      a_symbol_ptr	fund_sym;
      a_symbol_ptr	orig_fund_sym;
      check_assertion(sym != NULL);
      check_ambiguity_and_verify_access(&locator_for_curr_id);
      fund_sym = fundamental_symbol_of(sym);
      orig_fund_sym = fund_sym;
      if (class_template_allowed &&
          !locator_for_curr_id.is_template_id &&
          (alias_ctad_allowed ? is_class_template_symbol(fund_sym)
                         : is_class_template_but_not_alias_symbol(fund_sym))) {
        /* We found a class or alias template and we are doing class template
           argument deduction.  Create a placeholder type to represent
           the class template reference.  This is not done for a template-id
           because a template argument list already exists in that case. */
        a_type_ptr	placeholder;
        placeholder = make_class_template_placeholder(fund_sym,
                                                      &pos_curr_token);
        fund_sym = symbol_for(placeholder);
        process_class_template_placeholder(dps, placeholder);
      }  /* if */
      if (!is_type_symbol(fund_sym)) {
        /* The symbol is not a type name. */
        sym_error(ec_sym_not_a_type_name, sym);
      } else {
        /* For the class template deduction case, we want to mark the
           original symbol as referenced, not the placeholder. */
        mark_referenced(orig_fund_sym, &locator_for_curr_id.source_position);
        tp = type_symbol_type(fund_sym);
#if DEFAULT_RECORD_FORM_OF_NAME_REFERENCE
        if (record_form_of_name_reference) {
          tp = make_typeref_with_lexical_information(tp, &locator_for_curr_id);
        }  /* if */
#endif /* DEFAULT_RECORD_FORM_OF_NAME_REFERENCE */
        *type_sym = sym;
      }  /* if */
    }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (decl_pos_block != NULL) {
      /* Assume the current token is the last decl-specifier. */
      decl_pos_block->specifiers_range.end = end_pos_curr_token;
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Bypass the identifier token. */
    if (!within_using_decl) (void)get_token();
  }  /* if */
  /* If no type was created, an error must have occurred above.  Return
     an error type. */
  if (tp == NULL) tp = error_type();
done:
  *type_ptr = tp;
}  /* typename_specifier */


static void cache_std_attribute_group(a_token_cache  *cache)
/*
The next tokens in the token stream are two left brackets, which are expected
to introduce a standard attribute group.  Cache the tokens making up that
group in the given cache.
*/
{
  cache_std_attribute(cache, /*add_tokens_to_cache=*/TRUE);
  if (curr_token == tok_rbracket) {
    /* Advance past the final right bracket. */
    cache_curr_token(cache);
    (void)get_token();
  } else {
    expect_error();
  }  /* if */
}  /* cache_std_attribute_group */


static a_symbol_ptr look_up_class_member_decl(
                                      a_type_ptr                    class_type,
                                      ARG_UNUSED a_decl_parse_state *dps)
/*
Look up the current identifier in the given class and return the symbol found.
*dps describes the declaration for which this look-up is done: If its declared
storage class is "static", consider the C++/CLI static constructor (which is
unique); otherwise consider for the ordinary constructor (which might be an
overload set).
*/
{
  an_id_lookup_options_set  idl_options = IDL_DIRECT_CLASS_MEMBERS_ONLY;

#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cli_or_cx_enabled &&
      dps->declared_storage_class == (a_storage_class)sc_static) {
    idl_options |= IDL_IS_STATIC_DECL;
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  (void)class_qualified_id_lookup(&locator_for_curr_id, class_type,
                                  idl_options);
  return locator_for_curr_id.specific_symbol;
}  /* look_up_class_member_decl */


a_boolean is_constructor_decl(a_type_ptr          class_type,
                              a_decl_parse_state  *dps)
/*
dps describes a member declaration being parsed during the definition of the
given class type.  Return TRUE and modify locator_for_curr_id appropriately if
the current declaration is that of a constructor (possibly, a C++/CLI static
constructor).
*/
{
  a_boolean              is_constructor = FALSE;
  a_symbol_ptr           tag_sym, sym;
  a_symbol_ptr           ctor_type_sym;
  a_symbol_ptr           fund_ctor_type_sym = NULL;
  a_source_position      pos;
  a_boolean              name_match = FALSE;
  a_boolean              type_mismatch = FALSE;
  a_scanning_token_cache scanning_cache;

  db_enter(4, "is_constructor_decl");
  if (ms_extensions &&
      (((curr_token == tok_struct || curr_token == tok_class
         or_is_cli_class_type_keyword(curr_token)) &&
        (class_type->kind == (a_type_kind)tk_struct ||
         class_type->kind == (a_type_kind)tk_class)) ||
       (curr_token == tok_union &&
        class_type->kind == (a_type_kind)tk_union))) {
    /* In Microsoft mode it is possible to use an elaborated type name to
       declare a constructor.  E.g. "struct S { struct S(); };". */
    /* Put the current token in the cache. */
    cache_curr_token(scanning_cache.ptr());
    (void)get_token();
  }  /* if */
  /* See whether the name of the current identifier token is the same as
     that of a class being defined.  If so, this declaration is treated
     as a constructor declaration if the next two tokens are a left paren
     and declaration start token.  Use token caching in the look-ahead,
     since the tokens will have to be rescanned no matter what. */
  tag_sym = symbol_for(class_type);
  ctor_type_sym = locator_for_curr_id.specific_symbol;
  if (ctor_type_sym != NULL) {
    fund_ctor_type_sym = fundamental_symbol_of(ctor_type_sym);
  }  /* if */
  if (locator_for_curr_id.symbol_header == tag_sym->header) {
    name_match = TRUE;
    if (ctor_type_sym != NULL) {
      if (ctor_type_sym == tag_sym) {
        /* The type specified matches the class type symbol. */
      } else if (symbol_is(ctor_type_sym, sk_type) &&
                 ctor_type_sym->variant.type.is_injected_class_name &&
                 same_entities(ctor_type_sym->variant.type.ptr, class_type)) {
        /* The type specified is the injected class symbol.  This is okay. */
      } else if (ms_extensions && fund_ctor_type_sym != NULL &&
                 ctor_type_sym != fund_ctor_type_sym &&
                 symbol_is(fund_ctor_type_sym, sk_type) &&
                 fund_ctor_type_sym->variant.type.is_injected_class_name &&
                 is_template_class_and_not_specific_def_symbol(tag_sym)) {
        /* The symbol found is the injected class name from a base class of
           a template class with the same name as the derived class.  Because
           the Microsoft compiler does not create an injected class name for
           nonspecialized template classes, we should ignore the injected class
           name found from the base class. */
      } else if (class_type->variant.class_struct_union
                                    .is_ms_instantiated_nonreal_class &&
                 symbol_is(fund_ctor_type_sym, sk_class_or_struct_tag) &&
                 identical_types(class_type,
                                 fund_ctor_type_sym
                                         ->variant.class_struct_union.type)) {
        /* We're in a Microsoft-mode nonreal instantiation of a class
           (performed to better approximate MSVC behavior).  In such cases,
           explicit template arguments on the constructor name will produce a
           nonreal type that is distinct from the enclosing class type (with
           a distinct symbol).  However, f_identical_types can tell the two
           are the same. */
      } else {
        /* The names match, but the types don't.  This happens in templates
           when the class name is "A" but the constructor was specified as
           A<T>, and A<T> does not refer to the prototype instantiation.
           This can also occur during a real instantiation if the constructor
           was specified as A<int> and we are instantiating A<char>.
           If there is a mismatch, make a note of it now, but don't issue a
           diagnostic until the balance of the "is constructor" tests have
           been done. */
        type_mismatch = TRUE;
      }  /* if */
    } else if (ms_extensions && locator_for_curr_id.is_qualified_name &&
               locator_for_curr_id.is_class_member) {
      a_type_ptr  qualifier = qualifier_class_type(locator_for_curr_id);
      if (!same_entities(qualifier, class_type) &&
          !(class_type->source_corresp.is_class_member &&
            same_entities(qualifier, parent_class_of(class_type)))) {
        /* In Microsoft mode qualified constructor names are accepted, but the
           qualifier should either be the current class or the enclosing class.
        */
        type_mismatch = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (name_match || (ms_extensions && !(microsoft_mode && !ms_permissive))) {
    /* Change "A::A" into "A" if we are processing inside the definition of
       class "A".  This is necessary for curr_token_type_symbol to handle
       this case correctly.  This is also necessary to allow a typedef
       to name a constructor in permissive Microsoft mode. */
    (void)simplify_curr_class_qualified_name();
    if ((!locator_for_curr_id.is_qualified_name || ms_extensions) &&
        !locator_for_curr_id.is_conversion_name &&
        !locator_for_curr_id.is_operator_name) {
      /* Put the current token in the cache. */
      cache_curr_token(scanning_cache.ptr());
      (void)get_token();
      /* If standard attributes are next, cache them. */
      while (std_attribute_tokens_next()) {
        cache_std_attribute_group(scanning_cache.ptr());
      }  /* while */
      /* Skip right parentheses that may enclose the declarator---e.g.,
         "struct S { (((S)))(); };"---and advance to what may be a left
         parenthesis: */
      while (curr_token == tok_rparen) {
        cache_curr_token(scanning_cache.ptr());
        (void)get_token();
      }  /* while */
      /* A left parenthesis presumably starts a parameter declaration list: */
      if (curr_token == tok_lparen) {
        /* Cache the left parenthesis. */
        cache_curr_token(scanning_cache.ptr());
        /* Advance past it.  If the next token is a right paren or
           the start of a parameter declaration, this must be a
           constructor. */
        (void)get_token();
        if (curr_token == tok_rparen || curr_token == tok_ellipsis) {
          /* Constructor. */
          is_constructor = TRUE;
        } else {
          a_pack_expansion_stack_entry_ptr	pesep;
          a_pack_expansion_descr_ptr		pedp;
          a_boolean				any_args;
          /* A constructor could include a pack expansion. */
          if (class_template_arg_deduction_enabled) {
            /* Any expressions scanned as part of a type may need to be
               rescannable. */
            scope_stack_top().in_template_deduction_context = TRUE;
          }  /* if */
          any_args = begin_potential_pack_expansion_context_full(
                                         &pesep, &pedp, /*is_lookahead=*/TRUE,
                                         /*allow_empty_list=*/FALSE,
                                         /*ignore_suppression=*/FALSE,
                                         /*claim_pack_index=*/FALSE);
          if (!any_args ||
              is_decl_start(IDS_REAL_DECLARATOR_ALLOWED |
                            IDS_IMPLICIT_TYPENAME_CONTEXT)) {
            /* An empty pack expansion, or a pack expansion of a parameter. */
            is_constructor = TRUE;
          } else {
            abandon_potential_pack_expansion_context(pesep);
          }  /* if */
          if (class_template_arg_deduction_enabled) {
            scope_stack_top().in_template_deduction_context = FALSE;
          }  /* if */
        }  /* if */
      }  /* if */
      /* Note that rescan_cached_tokens caches the current token as well as
         resetting the current token state to what it was before token caching
         was started.  So the current token should again be the name of the
         class being defined. */
      rescan_cached_tokens(scanning_cache.ptr());
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (!name_match && is_constructor) {
      /* We must be in Microsoft mode.  MSVC++ allows a typedef name that
         refers to the current class to replace the class name in a
         constructor declaration.  The syntax looks like a constructor
         declaration, so do a lookup to see if it's a typedef name for the
         current class. */
      a_type_ptr  sym_type = NULL;
      sym = normal_id_lookup(&locator_for_curr_id,
                             IDL_TENTATIVE_TYPE_LOOKUP |
                             IDL_DO_NOT_ADD_TO_NONREAL_CLASS |
                             IDL_DO_NOT_CREATE_PROJ_SYM);
      if (sym != NULL && symbol_is(sym, sk_type) && !sym->ambiguous) {
        sym_type = skip_typerefs(sym->variant.type.ptr);
      }  /* if */
      if (sym_type != NULL && same_entities(sym_type, class_type)) {
        /* Note that qualifiers on the typedef name are ignored -- this
           corresponds to MSVC++ behavior. */
      } else {
        is_constructor = FALSE;
      }  /* if */
      clear_specific_symbol(locator_for_curr_id);
    }  /* if */
#endif /* if MICROSOFT_EXTENSIONS_ALLOWED */
    if (is_constructor) {
      /* Turn the current locator from a "specific symbol" locator into a
         constructor locator. */
      a_boolean  is_cli_static_ctor = FALSE;
      clear_specific_symbol(locator_for_curr_id);
      locator_for_curr_id.specific_symbol = NULL;
      sym = look_up_class_member_decl(class_type, dps);
      if (sym != tag_sym) {
        /* The symbol one gets by looking up the class name is not the same as
           the class symbol.  This might be okay, but it has to be checked
           carefully. */
        if (sym != NULL) {
          a_type_ptr  sym_type;
          if (is_constructor_symbol(sym)) {
            /* Okay. */
#if MICROSOFT_EXTENSIONS_ALLOWED
          } else if (cli_or_cx_enabled && is_static_constructor_symbol(sym)) {
            /* Okay. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          } else if (symbol_is(sym, sk_type) &&
                     (sym_type = f_skip_typerefs(sym->variant.type.ptr),
                      same_entities(sym_type, class_type))) {
            /* There is a typedef for the class type with the same name as
               the class.  It was found instead of the class on the lookup.
               That's okay. */
          } else if (!symbol_is(sym, sk_projection) ||
                     sym->variant.projection.is_using_decl) {
            /* This can only mean that another member has been declared with
               the class name (usually a field, but it could also be a symbol
               representing a nonreal member).  Issue an error. */
            check_assertion(symbol_is(sym, sk_field) ||
                            is_nontype_template_param_symbol(sym));
            pos_error(ec_field_name_conflicts_with_class, &sym->decl_position);
          }  /* if */
        }  /* if */
        /* Use the class symbol instead of whatever the lookup returned. */
        locator_for_curr_id.specific_symbol = tag_sym;
      }  /* if */
      pos = locator_for_curr_id.source_position;
#if MICROSOFT_EXTENSIONS_ALLOWED
      if (cli_or_cx_enabled &&
          dps->declared_storage_class == (a_storage_class)sc_static) {
        is_cli_static_ctor = TRUE;
      }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      change_class_locator_into_constructor_locator(
                              &locator_for_curr_id, &pos, is_cli_static_ctor);
    }  /* if */
  }  /* if */
  if (!scanning_cache->is_empty()) {
    /* If we haven't done so yet, reset the token stream. */
    rescan_cached_tokens(scanning_cache.ptr());
  }  /* if */
  if (is_constructor && type_mismatch) {
    /* The type used to declare the constructor does not match the type
       of the current class. */
    pos_ty_error(ec_constructor_type_mismatch,
                 &locator_for_curr_id.source_position, class_type);
  }  /* if */
  db_exit();
  return is_constructor;
}  /* is_constructor_decl */


/* Enumerations used by decl_specifiers for its internal processing and
   for calls to its subroutines. */
/* The basic type (without qualifiers or other specifiers). */
enum a_basic_type {
  bt_none,
  bt_void,
  bt_char,
  bt_wchar_t,
  bt_char8_t,
  bt_char16_t,
  bt_char32_t,
  bt_bool,
  bt_int,
  bt_bit_precise_int,
#if FIXED_POINT_ALLOWED
  bt_fract,
  bt_accum,
#endif /* FIXED_POINT_ALLOWED */
  bt_bfloat16,
  bt_float16,
  bt_fp16,
  bt_float,
  bt_float32,
  bt_float32x,
  bt_double,
  bt_float64,
  bt_float64x,
  bt_float80,
  bt_float128,
  bt_std_float128,
  bt_nullptr_t,
  bt_typedef,
  bt_struct_union,
  bt_enum,
  bt_typename,
  bt_auto,
  bt_no_type,
  bt_error
};

/* The sign specifier. */
enum a_type_sign {
  sign_none,
  sign_signed,
  sign_unsigned
};

/* The size specifier. */
enum a_type_size {
  size_none,
  size_short,
  size_long
#if LONG_LONG_ALLOWED
  , size_long_long
#endif /* LONG_LONG_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  , size_int8,
  size_int16,
  size_int32,
  size_int64
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
  , size_int128
#endif /* INT128_EXTENSIONS_ALLOWED */
};

/* C99 complex modifiers. */
enum a_complex_attribute {
  cxa_none,
  cxa_complex,
  cxa_imaginary
};
#if !C99_IL_EXTENSIONS_SUPPORTED
/*lint -esym(749,cxa_complex)*/
/*lint -esym(749,cxa_imaginary)*/
#endif /* !C99_IL_EXTENSIONS_SUPPORTED */

/*
Macro that determines whether the current mode allows certain typedefs to be
modified by sign and/or size specifiers.  E.g.:
  typedef long L;  unsigned L x;  // Allowed in some modes.
*/
#define current_mode_allows_typedef_with_adjectives()                       \
  (C_dialect == C_dialect_pcc || gpp_mode || (gcc_mode && gnu_version < 30400))

static a_basic_type basic_float_type(a_float_kind fkind)
/*
Return the basic type corresponding to the given floating-point kind.
*/
{
  a_basic_type basic_type = bt_double;

  if (fkind == fk_std_bfloat16) {
    basic_type = bt_bfloat16;
  } else if (fkind == fk_float16 || fkind == fk_std_float16) {
    basic_type = bt_float16;
  } else if (fkind == fk_fp16) {
    basic_type = bt_fp16;
  } else if (fkind == fk_float || fkind == fk_std_float32) {
    basic_type = bt_float;
  } else if (fkind == fk_float32x) {
    basic_type = bt_float32x;
  } else if (fkind == fk_double) {
    basic_type = bt_double;
  } else if (fkind == fk_float64x) {
    basic_type = bt_float64x;
  } else if (fkind == fk_float80) {
    basic_type = bt_float80;
  } else if (fkind == fk_float128) {
    basic_type = bt_float128;
  } else if (fkind == fk_std_float128) {
    basic_type = bt_std_float128;
  }  /* if */
  return basic_type;
}  /* basic_float_type */


static a_basic_type basic_type_from_typedef(a_decl_parse_state  *dps,
                                            a_type_sign         *sign,
                                            a_type_size         *size)
/*
dps->specifiers_type represents a typedef used as a specifier in the current
declaration-like construct (described by dps).  Some modes allow certain
typedef types to be modified by sign and size specifiers (short, unsigned,
etc.).  If dps->specifiers_type is such a typedef, return the associated basic
type specifier and set *sign and *size to the sign and size of the type
underlying that typedef.  Also set dps->specifiers_type to NULL in that case.
Otherwise, return bt_typedef and leave dps->specifiers_type, *sign, and *size
unchanged.
*/
{
  a_basic_type     basic_type = bt_typedef;
  a_type_ptr       temp_type = skip_typerefs(dps->specifiers_type);
  an_integer_kind  ikind;

  if (temp_type->kind == (a_type_kind)tk_integer) {
    if (temp_type->variant.integer.enum_type ||
        temp_type->variant.integer.bool_type) {
      /* Don't allow adjectives on enum integers or bool. */
    } else {
      /* Adjectives (size and sign) are only allowed where they
         fill in empty holes -- unspecified attributes -- in the
         following table:

                                    sign      size      base type
           ik_signed_char                    -fixed-     char
           ik_unsigned_char       see note   -fixed-     char
           ik_short                          short       int
           ik_unsigned_short      unsigned   short       int
           ik_int                                        int
           ik_unsigned_int        unsigned               int
           ik_long                           long        int
           ik_unsigned_long       unsigned   long        int
           ik_long_long                      long long   int
           ik_unsigned_long_long  unsigned   long long   int

         In pcc mode, the "signed" keyword does not exist, so something
         that is signed really has unspecified sign.  Note that ik_char
         is not used in pcc mode, so ik_unsigned_char has to be viewed
         as not specifying a sign if it is plain_char_int_kind.
         ik_signed_char always implies an unspecified sign.  A size may
         not be specified for those ("short char" and "long char" don't
         make sense). */
      check_assertion(*sign != sign_none || *size != size_none);
      ikind = temp_type->variant.integer.int_kind;
      switch (ikind) {
        case ik_char:
          basic_type = bt_char;
          break;
        case ik_unsigned_char:
          if (plain_char_int_kind != ikind && *sign != sign_none) break;
          /* Fall into signed char case. */
          FALLTHROUGH
        case ik_signed_char:
          if (*size != size_none
#if MICROSOFT_EXTENSIONS_ALLOWED
              && *size != size_int8
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                                  ) break;
          basic_type = bt_char;
          break;
        case ik_short:
          /* GNU C++ allows many strange things for a type specifier in a new
             expression (e.g., "long" on a typedef for "short").  We only
             emulate cases that are unlikely to be surprising (i.e., extraneous
             modifiers). */
          if (*size == size_none ||
              (gpp_mode && dps->is_new_expr_type && *size == size_short)) {
            basic_type = bt_int;
            *size = size_short;
          }  /* if */
          break;
        case ik_unsigned_short:
          /* No holes to fill in. */
          /* GNU C++ allows many strange things for a type specifier in a new
             expression (e.g., "long" on a typedef for "short").  We only
             emulate cases that are unlikely to be surprising (i.e., extraneous
             modifiers). */
          if (gpp_mode && dps->is_new_expr_type &&
              ((*size == size_short && *sign == sign_unsigned) ||
               (*size == size_none && *sign == sign_unsigned) ||
               (*size == size_short && *sign == sign_none))) {
            basic_type = bt_int;
            *sign = sign_unsigned;
            *size = size_short;
          }  /* if */
          break;
        case ik_unsigned_int:
          /* GNU C++ allows many strange things for a type specifier in a new
             expression (e.g., "signed" on a typedef for "unsigned").  We only
             emulate cases that are unlikely to be surprising (i.e., extraneous
             modifiers). */
          if (*sign == sign_none ||
              (gpp_mode && dps->is_new_expr_type && *sign == sign_unsigned)) {
            *sign = sign_unsigned;
            basic_type = bt_int;
          }  /* if */
          break;
        case ik_int:
          basic_type = bt_int;
          break;
        case ik_long:
          /* GNU C allows the extra "long" (with no effect).
             GNU C++ allows many strange things for a type specifier in a new
             expression (e.g., "short" on a typedef for "long").  We only
             emulate cases that are unlikely to be surprising (i.e., extraneous
             modifiers). */
          if (*size == size_none ||
              (gcc_mode && *size == size_long) ||
              (gpp_mode && dps->is_new_expr_type && *size == size_long)) {
            basic_type = bt_int;
            *size = size_long;
          }  /* if */
          break;
        case ik_unsigned_long:
          /* No holes to fill in. */
          /* GNU C allows the extra "long" and/or "unsigned" (with no effect).
             GNU C++ allows many strange things for a type specifier in a new
             expression (e.g., "short" on a typedef for "unsigned long").  We
             only emulate cases that are unlikely to be surprising (i.e.,
             extraneous modifiers). */
          if ((gcc_mode && *size == size_long) ||
              (gpp_mode && dps->is_new_expr_type &&
               ((*size == size_long && *sign == sign_unsigned) ||
                (*size == size_none && *sign == sign_unsigned) ||
                (*size == size_long && *sign == sign_none)))) {
            basic_type = bt_int;
            *sign = sign_unsigned;
            *size = size_long;
          }  /* if */
          break;
#if LONG_LONG_ALLOWED
        case ik_long_long:
          if (*size == size_none) {
            basic_type = bt_int;
            *size = size_long_long;
          }  /* if */
          break;
        case ik_unsigned_long_long:
          /* No holes to fill in. */
          break;
#endif /* LONG_LONG_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
        case ik_int128:
        case ik_unsigned_int128:
          /* No holes to fill in.  However, GCC accepts superfluous sign
             specifiers in C++ mode. */
          if (gpp_mode && dps->is_type_name) {
            if ((ikind == ik_int128 && *sign == sign_signed) ||
                (ikind == ik_unsigned_int128 && *sign == sign_unsigned)) {
              *sign = sign_none;
            }  /* if */
          }  /* if */
          break;
#endif /* INT128_EXTENSIONS_ALLOWED */
        default:
          unexpected_condition_str(
                              "basic_type_from_typedef: bad typedef int kind");
      }  /* switch */
    }  /* if */
  } else if (temp_type->kind == (a_type_kind)tk_float) {
    basic_type = basic_float_type(temp_type->variant.float_kind);
  }  /* if */
  if (basic_type != bt_typedef) dps->specifiers_type = NULL;
  return basic_type;
}  /* basic_type_from_typedef */


static a_boolean combine_type_specifiers(
                                  a_decl_parse_state             *dps,
                                  a_basic_type                   basic_type,
                                  a_type_sign                    sign,
                                  a_type_size                    size,
                                  a_constant_ptr                 bit_width_con,
                                  ARG_UNUSED a_complex_attribute complex_attr,
                                  ARG_UNUSED a_boolean           saturating_fp)
/*
Given a basic type, a sign specifier, and a size specifier, return a pointer to
a type entry in dps->specifiers_type.  This routine is only called from
decl_specifiers.  bit_width_con is the constant that specifies the bit width of
the bit-precise integer type (if any), and complex_attr indicates the kind of
complex type involved (_Complex or _Imaginary).  saturating_fp is TRUE if the
fixed-point modifier _Sat was specified.
*/
{
  an_integer_kind  ikind = (an_integer_kind)ik_none;
  a_float_kind     fkind;
  a_boolean        bad_combination = FALSE;

  if (current_mode_allows_typedef_with_adjectives() &&
      basic_type == bt_typedef && (sign != sign_none || size != size_none)) {
    /* GNU C/C++ (except GNU C 3.4 and later) and pcc allow unsigned, long,
       and short as adjectives modifying a typedef type.  Turn the typedef
       into a matching basic type, for the cases for which it makes sense.
       For the others, an error will be detected below. */
    basic_type = basic_type_from_typedef(dps, &sign, &size);
    if (gnu_mode && basic_type != bt_typedef) {
      report_gnu_extension_if_needed(&error_position,
                                     ec_typedef_modification_is_nonstandard);
    }  /* if */
  }  /* if */
  /* Now check for the various legal combinations of specifiers.  See 3.5.2
     for list. */
  switch (basic_type) {
    case bt_void:
      if (sign == sign_none && size == size_none) {
        /* void type. */
        dps->specifiers_type = void_type();
      } else {
        bad_combination = TRUE;
      }  /* if */
      break;
    case bt_char:
      if (size != size_none
#if MICROSOFT_EXTENSIONS_ALLOWED
          && size != size_int8
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
                              ) {
        bad_combination = TRUE;
      } else {
        switch (sign) {
          case sign_none:
            /* "plain" char. */
            ikind = plain_char_int_kind;
            break;
          case sign_signed:
            /* signed char. */
            ikind = (an_integer_kind)ik_signed_char;
            break;
          case sign_unsigned:
            /* unsigned char. */
            ikind = (an_integer_kind)ik_unsigned_char;
            break;
          default:
            unexpected_condition_str(
                      "combine_type_specifiers: bad value for a_type_sign");
        }
        /* In Microsoft Visual C++ 6.0 __int8 is a distinct type (not just a
           synonym for a char type). */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (ms_extensions && microsoft_version == 1200 &&
            size == size_int8) {
          if (ikind == (an_integer_kind)ik_unsigned_char) {
            ikind = targ_unsigned_int8_int_kind;
          } else {
            /* signed __int8 is the same as __int8. */
            ikind = targ_int8_int_kind;
          }  /* if */
          dps->specifiers_type =
                         microsoft_sized_integer_type((an_integer_kind)ikind);
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        {
          dps->specifiers_type = integer_type((an_integer_kind)ikind);
        }  /* if */
      }  /* if */
      break;
    case bt_wchar_t:
      if (sign == sign_none && size == size_none) {
        dps->specifiers_type = wchar_t_type();
      } else {
        bad_combination = TRUE;
      }  /* if */
      break;
    case bt_char8_t:
      if (sign == sign_none && size == size_none) {
        dps->specifiers_type = char8_t_type();
      } else {
        bad_combination = TRUE;
      }  /* if */
      break;
    case bt_char16_t:
      if (sign == sign_none && size == size_none) {
        dps->specifiers_type = char16_t_type();
      } else {
        bad_combination = TRUE;
      }  /* if */
      break;
    case bt_char32_t:
      if (sign == sign_none && size == size_none) {
        dps->specifiers_type = char32_t_type();
      } else {
        bad_combination = TRUE;
      }  /* if */
      break;
    case bt_bool:
      if (sign == sign_none && size == size_none) {
        dps->specifiers_type = bool_type();
      } else {
        bad_combination = TRUE;
      }  /* if */
      break;
    case bt_bit_precise_int:
      if (size == size_none && bit_width_con != NULL) {
        if (is_error_constant(bit_width_con)) {
          dps->specifiers_type = error_type();
        } else if (constant_is(bit_width_con, ck_template_param)) {
          dps->specifiers_type = dependent_bit_precise_integer_type(
                                        bit_width_con, sign == sign_unsigned);
        } else {
          a_boolean             err = FALSE;
          a_host_large_integer  width;
          a_source_position     *width_pos =
                                &bit_width_con->source_corresp.decl_position;
          check_assertion(constant_is(bit_width_con, ck_integer));
          conv_integer_value_to_host_large_integer(
                                        &bit_width_con->variant.integer_value,
                                        /*is_signed=*/TRUE, &width, &err);
          if (err || (width > 0 &&
                      (a_targ_size_t)width > bitint_maxwidth_value)) {
            int32_t diagnostic_width =
                          err ? (int32_t)bitint_maxwidth_value + 1 :
                                (int32_t)width;
            pos_num2_diagnostic(es_error, ec_bitint_width_too_large,
                                width_pos, diagnostic_width,
                                (int32_t)bitint_maxwidth_value);
            dps->specifiers_type = error_type();
          } else if (width <= 0 && sign == sign_unsigned) {
            pos_num2_diagnostic(es_error, ec_unsigned_bitint_width_too_small,
                                width_pos, (int32_t)width, 1);
            dps->specifiers_type = error_type();
          } else if (width <= 0 || (width == 1 && sign != sign_unsigned)) {
            pos_num2_diagnostic(es_error, ec_signed_bitint_width_too_small,
                                width_pos, (int32_t)width, 2);
            dps->specifiers_type = error_type();
          } else {
            dps->specifiers_type = bit_precise_integer_type(
                                                        (a_targ_size_t)width,
                                                        sign == sign_unsigned,
                                                        sign == sign_signed);
          }  /* if */
        }  /* if */
      } else {
        bad_combination = TRUE;
      }  /* if */
      break;
    case bt_none:
      /* If there was no explicit basic type, assume an integer type. */
    case bt_int:
      switch (size) {
        case size_short:
          if (sign != sign_unsigned) {
            /* short, signed short, short int, signed short int. */
            ikind = (an_integer_kind)ik_short;
          } else {
            /* unsigned short, unsigned short int. */
            ikind = (an_integer_kind)ik_unsigned_short;
          }  /* if */
          break;
        case size_none:
          if (sign != sign_unsigned) {
            /* int, signed, signed int, or no type specifiers. */
            ikind = (an_integer_kind)ik_int;
          } else {
            /* unsigned, unsigned int. */
            ikind = (an_integer_kind)ik_unsigned_int;
          }  /* if */
          break;
        case size_long:
          if (sign != sign_unsigned) {
            /* long, signed long, long int, signed long int. */
            ikind = (an_integer_kind)ik_long;
          } else {
            /* unsigned long, unsigned long int. */
            ikind = (an_integer_kind)ik_unsigned_long;
          }  /* if */
          break;
#if LONG_LONG_ALLOWED
        case size_long_long:
          if (sign != sign_unsigned) {
            /* long long, signed long long, long long int,
               signed long long int. */
            ikind = (an_integer_kind)ik_long_long;
          } else {
            /* unsigned long long, unsigned long long int. */
            ikind = (an_integer_kind)ik_unsigned_long_long;
          }  /* if */
          break;
#endif /* LONG_LONG_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
        case size_int16:
          if (sign != sign_unsigned) {
            /* __int16, signed __int16. */
            ikind = targ_int16_int_kind;
          } else {
            /* unsigned __int16. */
            ikind = targ_unsigned_int16_int_kind;
          }  /* if */
          break;
        case size_int32:
          if (sign != sign_unsigned) {
            /* __int32, signed __int32. */
            ikind = targ_int32_int_kind;
          } else {
            /* unsigned __int32. */
            ikind = targ_unsigned_int32_int_kind;
          }  /* if */
          break;
        case size_int64:
          if (sign != sign_unsigned) {
            /* __int64, signed __int64. */
            ikind = targ_int64_int_kind;
          } else {
            /* unsigned __int64. */
            ikind = targ_unsigned_int64_int_kind;
          }  /* if */
          break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
        case size_int128:
          if (sign != sign_unsigned) {
            /* __int128, signed __int128. */
            ikind = (an_integer_kind)ik_int128;
          } else {
            /* unsigned __int128. */
            ikind = (an_integer_kind)ik_unsigned_int128;
          }  /* if */
          break;
#endif /* INT128_EXTENSIONS_ALLOWED */
        default:
          unexpected_condition_str(
                                 "combine_type_specifiers: bad size for int");
      }  /* switch */
        /* In Microsoft Visual C++ 6.0 __intN is a distinct type (not just a
           synonym for another integral type). */
      if (sign == sign_signed) {
        /* For an explicitly "signed" int, use a different type entry.
           Plain "int" and "signed int" have to be kept separate because
           they may mean different things as bit-field types.  The same
           applies to explicitly signed short, long, and long long. */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (ms_extensions && microsoft_version == 1200 &&
            (int)size >= (int)size_int8 &&
            (int)size <= (int)size_int64) { /*lint !e587 !e685 !e2650*/
          dps->specifiers_type = microsoft_sized_signed_integer_type(
                                                      (an_integer_kind)ikind);
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        {
          dps->specifiers_type = signed_integer_type((an_integer_kind)ikind);
        }  /* if */
      } else {
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (ms_extensions && microsoft_version == 1200 &&
            (int)size >= (int)size_int8 &&
            (int)size <= (int)size_int64) { /*lint !e587 !e685 !e2650*/
          dps->specifiers_type =
                         microsoft_sized_integer_type((an_integer_kind)ikind);
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        {
          dps->specifiers_type = integer_type((an_integer_kind)ikind);
        }  /* if */
      }  /* if */
      break;
#if FIXED_POINT_ALLOWED
    case bt_fract:
    case bt_accum:
      { /* Create a fixed-point type according to specification. */
        a_fixed_point_precision  precision =
                                          (a_fixed_point_precision)fpp_default;
        switch (size) {
          case size_none:
            /* Default precision. */
            precision = (a_fixed_point_precision)fpp_default;
            break;
          case size_short:
            precision = (a_fixed_point_precision)fpp_short;
            break;
          case size_long:
            precision = (a_fixed_point_precision)fpp_long;
            break;
          default:
            bad_combination = TRUE;
        }  /* switch */
        if (!bad_combination) {
          dps->specifiers_type = fixed_point_type(
                           make_fixed_point_type_descr(
                                       precision, (sign == sign_unsigned),
                                       (basic_type == bt_fract),
                                       saturating_fp));
        }  /* if */
      }  /* if */
      break;
#endif /* FIXED_POINT_ALLOWED */
    case bt_bfloat16:
    case bt_float16:
    case bt_fp16:
    case bt_float:
    case bt_double:
    case bt_float32:
    case bt_float32x:
    case bt_float64:
    case bt_float64x:
    case bt_float80:
    case bt_float128:
    case bt_std_float128:
      if (sign != sign_none || (size != size_none && size != size_long)) {
        bad_combination = TRUE;
      } else {
        if (size == size_none) {
          if (basic_type == bt_bfloat16) {
            fkind = fk_std_bfloat16;
          } else if (basic_type == bt_float16) {
            fkind = fk_float16;
          } else if (basic_type == bt_fp16) {
            fkind = fk_fp16;
          } else if (basic_type == bt_float) {
            /* float. */
            fkind = (a_float_kind)fk_float;
          } else if (basic_type == bt_float32x) {
            fkind = (a_float_kind)fk_float32x;
          } else if (basic_type == bt_double) {
            fkind = (a_float_kind)fk_double;
          } else if (basic_type == bt_float64x) {
            fkind = (a_float_kind)fk_float64x;
          } else if (basic_type == bt_float32) {
            fkind = (a_float_kind)fk_std_float32;
          } else if (basic_type == bt_float64) {
            fkind = (a_float_kind)fk_std_float64;
          } else if (basic_type == bt_float80) {
            fkind = (a_float_kind)fk_float80;
          } else if (basic_type == bt_float128) {
            fkind = (a_float_kind)fk_float128;
          } else if (basic_type == bt_std_float128) {
            fkind = (a_float_kind)fk_std_float128;
          } else {
            unexpected_condition();
          }  /* if */
        } else {
          if (basic_type == bt_float) {
            /* long float, which is double in pcc.  Accepted with a warning
               in some other modes. */
            fkind = (a_float_kind)fk_double;
            if (C_dialect == C_dialect_pcc) {
              /* No diagnostic */
            } else if (strict_ansi_mode || gnu_mode) {
              /* The extension is not accepted in strict mode or in GNU
                 mode. */
              diagnostic(gnu_mode ? es_discretionary_error
                                  : strict_ansi_error_severity,
                         ec_bad_combination_of_type_specifiers);
            } else {
              pos_warning(ec_nonstandard_long_float, &error_position);
            }  /* if */
          } else {
            /* long double. */
            fkind = (a_float_kind)fk_long_double;
          }  /* if */
        }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
        if (complex_attr == cxa_complex) {
          dps->specifiers_type = complex_type((a_float_kind)fkind);
        } else if (complex_attr == cxa_imaginary) {
          dps->specifiers_type = imaginary_type((a_float_kind)fkind);
        } else
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
        /* Do not insert code here. */
        {
          dps->specifiers_type = float_type((a_float_kind)fkind);
        }  /* if */
      }  /* if */
      break;
    case bt_nullptr_t:
      if (sign == sign_none && size == size_none) {
        /* nullptr_t type. */
        dps->specifiers_type = standard_nullptr_type();
      } else {
        bad_combination = TRUE;
      }  /* if */
      break;
    case bt_auto:
    case bt_struct_union:
    case bt_enum:
    case bt_typename:
    case bt_typedef:
      if (sign != sign_none || size != size_none) bad_combination = TRUE;
      check_assertion_str2(dps->specifiers_type != NULL,
                           "combine_type_specifiers: null type ptr for",
                           "class, struct, union, enum, or typedef");
      break;
    case bt_no_type:
      /* No specifiers type declared (constructor, destructor, or conversion
         operator). */
      dps->specifiers_type = unknown_type();
      break;
    case bt_error:
      /* Error, already diagnosed. */
      dps->specifiers_type = error_type();
      break;
    default:
      unexpected_condition_str("combine_type_specifiers: bad basic type");
  }  /* switch */
#if FIXED_POINT_ALLOWED
  if (saturating_fp && (basic_type != bt_fract && basic_type != bt_accum)) {
    /* "_Sat" is only allowed on fixed-point types ("_Fract" and "_Accum"). */
    bad_combination = TRUE;
  }  /* if */
#endif /* FIXED_POINT_ALLOWED */
  if (bad_combination) {
    /* Bad combination of type specifiers.  Issue a diagnostic and set the
       type to an error type. */
    pos_error(ec_bad_combination_of_type_specifiers, &error_position);
    dps->specifiers_type = error_type();
  }  /* if */
  /* Return TRUE if no problems were encountered in combining type
     specifiers. */
  return !bad_combination;
}  /* combine_type_specifiers */


static a_type_ptr make_c11_atomic_type(a_type_ptr         utp,
                                       a_source_position  *diag_pos,
                                       a_boolean          prev_quals_allowed)
/*
Make and return a C11 _Atomic type based on the given underlying type.  Return
an error type if utp is null, an error type, an array type, or a function type;
issue an error for the latter two cases.  If prev_quals_allowed is FALSE and
utp is a qualified type also issue an error and return an error type.
*/
{
  a_type_ptr  result;

  if (utp == NULL || is_error_type(utp)) {
    result = error_type();
  } else if (is_function_type(utp) || is_array_type(utp)) {
    pos_ty_error(ec_c11_atomic_array_or_function_type, diag_pos, utp);
    result = error_type();
  } else if (!prev_quals_allowed && is_qualified_type(utp)) {
    pos_ty_error(ec_c11_atomic_specifier_with_qualified_type, diag_pos, utp);
    result = error_type();
  } else {
    result = make_qualified_type(utp, TQ_C11_ATOMIC);
  }  /* if */
  return result;
}  /* make_c11_atomic_type */


static a_boolean add_type_qualifiers(a_type_ptr            *p_type_ptr,
                                     a_decl_parse_state    *state)
/*
Add the type qualifiers specified by state->qualifiers to the type specified
by *type_ptr.  This function is called from decl_specifiers only.
*/
{
  a_boolean             err = FALSE;
  an_error_severity     severity;
  a_type_qualifier_set  qualifiers = state->qualifiers;

  if (qualifiers != TQ_NONE) {
    a_type_ptr  type_ptr = *p_type_ptr;
#if UPC_EXTENSIONS_ALLOWED
    a_type_qualifier_set  new_upc_access = TQ_NONE, old_upc_access = TQ_NONE;
    if (upc_mode) {
      /* Retrieve the UPC strict/relax qualifiers for possible later
         checking. */
      new_upc_access = qualifiers & (TQ_UPC_RELAXED | TQ_UPC_STRICT);
      old_upc_access = f_get_type_qualifiers(type_ptr, /*top_level=*/FALSE) &
                                              (TQ_UPC_RELAXED | TQ_UPC_STRICT);
    }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
#if NAMED_ADDRESS_SPACES_ALLOWED
    if (named_address_spaces_enabled && qualifiers != TQ_NONE) {
      a_named_address_space_id  new_nas =
                            named_address_space_from_qualifier_set(qualifiers);
      if (new_nas != 0) {
        if (is_function_type(type_ptr)) {
          /* Function types cannot be qualified with named address spaces. */
          err = TRUE;
          pos_error(ec_named_address_space_on_function_type,
                    &state->qualifiers_pos);
          qualifiers = simple_qualifiers(qualifiers);
          copy_qualifiers(qualifiers, state->qualifiers);
        } else {
          a_type_ptr                tp = type_ptr;
          a_type_qualifier_set      old_quals;
          a_named_address_space_id  old_nas;
          if (is_array_type(tp)) {
            /* The qualifiers for an array type are actually applied to the
               underlying element type. */
            tp = underlying_array_element_type(tp);
          }  /* if */
          old_quals = get_type_qualifiers(tp);
          old_nas = named_address_space_from_qualifier_set(old_quals);
          if (old_nas != 0) {
            /* Double qualification with a named address space.  If the address
               space is identical, issue a warning; otherwise, an error. */
            severity = es_warning;
            if (old_nas != new_nas) {
              severity = es_error;
              err = TRUE;
            }  /* if */
            pos_diagnostic(severity, ec_multiple_named_address_spaces,
                           &state->qualifiers_pos);
            qualifiers = simple_qualifiers(qualifiers);
            copy_qualifiers(qualifiers, state->qualifiers);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
    if (type_is(type_ptr, tk_typeref)) {
      if (C_dialect == C_dialect_cplusplus) {
        /* In C++ adding a qualifier to a typedef name that is already
           identically qualified is okay, so don't even bother checking for
           an error.  Note that make_qualified_type will not actually add
           superfluous qualifiers. */
        /* However, adding a qualifier to a typedef for a reference type
           may not have any effect.  More precisely, if the construct is
           followed by a declarator creating a reference, the qualifier
           should be merged with any qualifiers of the type underlying the
           reference. */
        if (is_reference_type(type_ptr)) {
          /* "restrict" applies directly to reference types; other qualifiers
              do not. */
          state->unused_qualifiers = (qualifiers & ~TQ_RESTRICT) != TQ_NONE;
          qualifiers &= TQ_RESTRICT;
        }  /* if */
      } else {
        /* In C we check for duplicate qualifiers on a declaration, even
           if, in the case of an array type, one is a top-level qualifier
           and the other qualifies an element type.  That's why top_level
           is set to FALSE here -- that's normally not the case in C mode. */
        /* According to 3.5.3: "If the specification of an array type
           includes any type qualifiers, the element type is so-qualified,
           not the array type.", and this is interpreted recursively
           for arrays of arrays.  The type qualifiers therefore apply
           to the ultimate element type.  This can only happen with typedefs,
           as in "typedef int A[2][3]; const A a;", which makes "a" an
           array of array of const int. */
        if ((qualifiers &
                 f_get_type_qualifiers(type_ptr, /*top_level=*/FALSE)) != 0) {
          /* Duplication of type qualifier (probably because of a typedef
             that is already qualified).  In strict ANSI C89 mode issue an
             error or warning; otherwise, just issue a remark. */
          if (strict_ansi_mode && !c99_mode) {
            severity = strict_ansi_error_severity;
            if (severity == es_error) err = TRUE;
          } else {
            severity = es_remark;
          }  /* if */
          diagnostic(severity, ec_dupl_type_qualifier);
        }  /* if */
      }  /* if */
    }  /* if */
    /* The restrict qualifier may only be applied to pointer and reference
       types (but not pointer-to-function-type), pointer-to-member types,
       and (in parameter declarations only) array types. */
    if ((qualifiers & TQ_RESTRICT) &&
        !restrict_qualifier_is_allowed(type_ptr, &state->restrict_pos)) {
      /* Diagnostic has already been issued.  Just remove TQ_RESTRICT
         from the qualifier set. */
      qualifiers &= ~TQ_RESTRICT;
      err = TRUE;
    }  /* if */
    /* If Clang nullability qualifiers are invalid, issue a diagnostic and
       ignore them. */
    if ((qualifiers & TQ_NULLABILITY) &&
        !check_nullability_qualifiers(qualifiers, type_ptr,
                                      &state->qualifiers_pos)) {
      qualifiers &= ~TQ_NULLABILITY;
      err = TRUE;
    }  /* if */
    if (qualifiers != TQ_NONE) {
      /* Type qualifiers occurring on function types through typedef or
         template parameter substitutions are ignored.  The C standard (C90
         and C99) specifies that type qualifiers on function types result in
         undefined behavior (except that "restrict" is ill-formed; see above).
         We handle the C case as in C++: We ignore the qualifiers with a
         warning. */
      if (is_function_type(type_ptr)) {
        if (depth_innermost_instantiation_scope == NO_SCOPE_DEPTH ||
            scope_stack[decl_scope_level].in_prototype_instantiation) {
          /* If we're not instantiating a template, applying a cv-qualifier
             to a function type was probably not intended: Issue a warning. */
          a_source_position_ptr  diag_pos =
                          (qualifiers == TQ_RESTRICT) ? &state->restrict_pos
                                                      : &state->qualifiers_pos;
          pos_warning(ec_cv_qualified_function_type, diag_pos);
        }  /* if */
        qualifiers = TQ_NONE;
      } else if (state->decltype_auto_specifier_seen &&
                 !gpp_version_is(<110000)) {
        /* Something like "volatile decltype(auto) x = y;" is invalid, but
           earlier versions of GCC did permit it. */
        pos_error(ec_decltype_auto_cannot_be_qualified, &state->auto_pos);
        qualifiers = TQ_NONE;
      }  /* if */
    }  /* if */
#if UPC_EXTENSIONS_ALLOWED
    if (upc_mode) {
      /* Disallow upc_relaxed or upc_strict if the other is already specified.
         */
      if (!err && new_upc_access != TQ_NONE && old_upc_access != TQ_NONE &&
          (new_upc_access != old_upc_access)) {
        pos_error(ec_dupl_type_qualifier, &error_position);
        err = TRUE;
      }  /* if */
      /* Disallow strict or relaxed without shared. */
      if (new_upc_access != TQ_NONE && !(qualifiers & TQ_UPC_SHARED)) {
        /* Check whether shared was specified in the base type. */
        if ((f_get_type_qualifiers(type_ptr, /*top_level=*/FALSE) &
                                                         TQ_UPC_SHARED) == 0) {
          /* Issue an error and remove the offending qualifiers. */
          pos_error(ec_nonshared_strict_relaxed, &error_position);
          qualifiers &= ~(TQ_UPC_STRICT | TQ_UPC_RELAXED);
          new_upc_access = TQ_NONE;
        }  /* if */
      }  /* if */
      /* Disallow duplicate shared if the block sizes do not match. */
      if ((qualifiers & TQ_UPC_SHARED &
           f_get_type_qualifiers(type_ptr, /*top_level=*/FALSE)) != 0 &&
          state->upc_block_size !=
                        f_get_upc_block_size(type_ptr, /*top_level=*/FALSE)) {
        pos_error(ec_mismatched_shared_block_size, &error_position);
        err = TRUE;
      }  /* if */
    }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
    if (qualifiers != TQ_NONE) {
      if (is_unknown_type(type_ptr) && state->auto_type == NULL) {
        /* An "unknown type" is an indication that no type was specified,
           except that in GNU C mode it can represent __auto_type.  If no
           type was specified, default to "int". */
        type_ptr = integer_type((an_integer_kind)ik_int);
      }  /* if */
      if (qualifiers & TQ_C11_ATOMIC) {
        type_ptr = make_c11_atomic_type(type_ptr, &state->qualifiers_pos,
                                        /*prev_quals_allowed=*/TRUE);
        qualifiers &= ~TQ_C11_ATOMIC;
      }  /* if */
      /* Identify top-level cv-qualifiers that have no effect. */
      a_type_qualifier_set  existing_cv_quals = get_type_qualifiers(type_ptr) &
                                                      (TQ_CONST | TQ_VOLATILE),
                            eff_quals = qualifiers;
      state->eff_top_level_cv_quals = qualifiers & (TQ_CONST | TQ_VOLATILE);
      if (existing_cv_quals != TQ_NONE) {
        copy_qualifiers(state->eff_top_level_cv_quals & ~existing_cv_quals,
                        state->eff_top_level_cv_quals);
        eff_quals &= ~existing_cv_quals;
      }  /* if */
      /* Add the qualifiers if necessary.  make_qualified_type understands
         the strange array case too. */
      type_ptr = f_make_qualified_type(type_ptr, eff_quals,
                                       state->upc_block_size);
    }  /* if */
    if (err) {
      /* Some qualifiers were dropped due to errors.  Ignore those from now
         on. */
      copy_qualifiers(qualifiers, state->qualifiers);
    }  /* if */
    *p_type_ptr = type_ptr;
  }  /* if */
  return !err;
}  /* add_type_qualifiers */


static a_boolean implicit_int_member_with_name_of_type(void)
/*
Helper called from decl_specifiers to determine if the current identifier
might have meant to be a declarator in Cfront or Microsoft mode.  Both those
modes accept:
   struct X; struct Y { X(); };
and take Y::X to be an ordinary member function returning int.
Note that Microsoft will not accept such function declarations if they take
any parameters.  Cfront will, but that behavior is not imitated here.
Recent Microsoft compilers (microsoft_version >= 1310) still accept the code
above, but reject the code if X is neither a class type nor an enum type.
*/
{
  a_boolean    result;
  a_symbol_ptr sym;
  a_token_kind token_after_next;

  check_assertion(curr_token == tok_identifier);
  sym = locator_for_curr_id.symbol_header->symbol;
  if (sym != NULL && is_type_symbol(sym)) {
    (void)next_two_tokens(tok_lparen, &token_after_next);
    result = (token_after_next == tok_rparen);
    if (result && microsoft_bugs && microsoft_version >= 1310) {
      a_type_ptr  type = type_symbol_type(sym);
      result = (is_class_struct_union_type(type) ||
                is_enum_type(type));
    }  /* if */
  } else {
    result = FALSE;
  }  /* if */
  return result;
}  /* implicit_int_member_with_name_of_type */


static a_boolean looks_like_member_function_declarator(void)
/*
We are scanning the specifiers for a class template member.  The current token
is an identifier assumed to refer to a type because of the presence of a
template-dependent base class.  However, it might be that the identifier was
meant to be a member function name declared with the "implicit int return"
assumption.  For example:
    template<typename T> struct B {}
    template<typename T> struct D: B<T> { f(); };
Return TRUE if this looks like it is the case (and the code could not
otherwise be valid).
Recognizing such cases allows for a closer (but not exact) emulation of the
behavior of Microsoft compilers, and also enables better error recovery in
other modes.
*/
{
  a_boolean                   result = FALSE;
  a_tiny_scanning_token_cache cache;

  /* Cache the identifier. */
  check_assertion(curr_token == tok_identifier && !C_mode());
  cache_curr_token(cache.ptr());
  (void)get_token();
  if (curr_token == tok_lparen) {
    cache_curr_token(cache.ptr());
    (void)get_token();
    if (!is_declarator_start() && !is_ptr_to_member_declarator_start()) {
      /* We're not dealing with a construct of the form
             T (<nested-declarator>) ...
         So this must be a member function declarator for a declaration that
         assumes "implicit int return" rules. */
      result = TRUE;
    }  /* if */
  }  /* if */
  /* Restore the token state. */
  rescan_cached_tokens(cache.ptr());
  return result;
}  /* looks_like_member_function_declarator */


/* Define a bit vector to be used within decl_specifiers to track which
   specifiers have been encountered. */
typedef unsigned long a_decl_specifiers_set;
#define DS_NONE (a_decl_specifiers_set)(0x0)
			/* No decl-specifiers have been scanned. */
#define DS_STORAGE_CLASS (a_decl_specifiers_set)(0x1)
			/* A storage class has been scanned (this doesn't
			   include "mutable" and "thread_local/_Thread_local"
			   which are storage-class-specifiers, but have no
			   a_storage_class value). */
#define DS_TYPE_QUALIFIER (a_decl_specifiers_set)(0x2)
			/* A type qualifier (including "restrict" and the
			   Microsoft type qualifiers "near" and "far") has
			   been scanned. */
#define DS_TYPE (a_decl_specifiers_set)(0x4)
			/* A basic type or a size or "signed" or "unsigned"
			   has been scanned. */
#define DS_FRIEND (a_decl_specifiers_set)(0x8)
			/* "friend" has been scanned (C++ only). */
#define DS_VIRTUAL (a_decl_specifiers_set)(0x10)
			/* "virtual" has been scanned (C++ only). */
#define DS_EXPLICIT (a_decl_specifiers_set)(0x20)
			/* "explicit" has been scanned (C++ only). */
#define DS_INLINE (a_decl_specifiers_set)(0x40)
			/* "inline" has been scanned (C++ only). */
#define DS_MUTABLE  (a_decl_specifiers_set)(0x80)
			/* "mutable" has been scanned (C++ only). */
#define DS_MICROSOFT_INLINE (a_decl_specifiers_set)(0x100)
			/* "__inline" has been scanned (Microsoft mode
			   only). */
#define DS_FORCEINLINE (a_decl_specifiers_set)(0x200)
			/* "__forceinline" has been scanned (Microsoft mode
			   only). */
#define DS_OVERLOAD (a_decl_specifiers_set)(0x400)
			/* "overload" has been scanned (a C++ anachronism). */
#define DS_VOID (a_decl_specifiers_set)(0x800)
			/* "void" was scanned as the very first specifier. */
#define DS_CONSTEXPR (a_decl_specifiers_set)(0x1000)
			/* "constexpr" or "consteval" was scanned. */
#define DS_THREAD_LOCAL (a_decl_specifiers_set)(0x2000)
			/* "thread_local/_Thread_local" was scanned. */
#define DS_NORETURN (a_decl_specifiers_set)(0x4000)
			/* "_Noreturn" was scanned. */
#define DS_THIS (a_decl_specifiers_set)(0x8000)
			/* "this" was scanned. */


static void report_bad_type_name(a_decl_flag_set  input_flags)
/*
locator_for_curr_id describes a source name that was expected to name a valid
type, but it does not.  Report different errors depending on whether the name
can be found at all (in which case it presumably does not name a type).
The parameter input_flags is the same value that was passed to decl_specifiers
(from where this routine is called).
*/
{
  if (!is_error_locator(locator_for_curr_id)) {
    a_boolean                  lookup_error;
    a_symbol_ptr               sym = NULL;
    an_identifier_options_set  options;

    options = (input_flags & DSI_IS_NEW_TYPE_NAME) ?
                                        GID_IS_NEW_TYPE_NAME : GID_NO_OPTIONS;
    check_assertion(is_generalized_identifier_start(options));
    sym = coalesce_and_lookup_generalized_identifier(
                                          options, ilm_normal, &lookup_error);
    if (!lookup_error) {
      /* No error message has been issued yet. */
      if (sym != NULL) {
        /* The name refers to something, but not a type. */
        if (locator_for_curr_id_is_member_of_nonreal_template_class() &&
            !is_or_contains_error_type(locator_for_curr_id.parent.class_type)){
          /* If the identifier is a qualified class member of a nonreal,
             non-prototype-instantiation template class, then it's likely that
             the "typename" keyword is needed; give a special error message
             for this case. */
          sym_error(ec_typename_needed, sym);
        } else {
          sym_error(ec_sym_not_a_type_name, sym);
        }  /* if */
      } else {
        str_error(ec_undefined_identifier,
                  locator_for_curr_id.symbol_header->identifier);
      }  /* if */
    }  /* if */
    reference_to_invalid_name(&locator_for_curr_id);
    clear_specific_symbol(locator_for_curr_id);
  }  /* if */
}  /* report_bad_type_name */


a_type_ptr enclosing_class_type(void)
/*
Called while processing a member declaration to determine the type of the
class for which the member is being scanned.  Returns NULL in case of error.
*/
{
  a_scope_stack_entry_ptr  ssep = &scope_stack[depth_scope_stack];
  a_type_ptr               result = NULL;

  if (scope_is(ssep, sck_template_instantiation) ||
      scope_is(ssep, sck_template_declaration)) {
    /* Either this is a member template declaration or a rescan of a member
       template declaration to instantiate it. */
    --ssep;
  }  /* if */
  if (scope_is(ssep, sck_class_struct_union) ||
      scope_is(ssep, sck_class_reactivation) ||
      scope_is(ssep, sck_template_instantiation)) {
    result = ssep->assoc_type;
    check_assertion(result != NULL && is_immediate_class_type(result));
  } else {
    /* Error case of some sort. */
    expect_error();
  }  /* if */
  return result;
}  /* enclosing_class_type */

#if UPC_EXTENSIONS_ALLOWED

static void scan_upc_block_size_if_any(a_decl_parse_state    *state,
                                       a_basic_type          basic_type,
                                       a_decl_pos_block_ptr  decl_pos_block,
                                       a_boolean             *err)
/*
Scan the constant integer block size specified on a UPC shared type qualifier.
This routine also scans the enclosing brackets.  E.g.,
	shared[100] int a[35];  // Block size 100
If a block size was actually specified, the fact is recorded in *state.
The current token must be "shared."  decl_pos_block is updated with the
final position of the construct (whether or not a block size was specified).
*/
{
  /* If not otherwise specified, the block size will be 1. */
  a_upc_block_size  block_size = 1;

#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL) {
    /* If no block size followed, the specifier's end is the last position
       of the "shared" token. */
    decl_pos_block->specifiers_range.end = end_pos_curr_token;
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Skip the "shared" token. */
  check_assertion(curr_token == (a_token_kind)tok_upc_shared);
  (void)get_token();
  if (curr_token == tok_lbracket) {
    /* A shared block specifier. */
    state->dso_flags |= DSO_UPC_SHARED_LAYOUT;
    if (basic_type != bt_none) {
      /* Usually one would write "shared [N] int ...", but "int shared [N] ..."
         is possible too.  In the latter case, the brackets are still treated
         as a block size; not an abstract array declarator. */
      pos_remark(ec_ambiguous_block_size_spec, &error_position);
    }  /* if */
    /* Skip over the left bracket. */
    (void)get_token();
    add_stop_token(tok_rbracket);
    if (curr_token == tok_rbracket) {
      /* Empty brackets indicate an indefinite block size. */
      block_size = UPC_BLOCK_SIZE_INDEFINITE;
    } else if (curr_token == tok_star) {
      /* Pure block allocation requested. */
      block_size = UPC_BLOCK_SIZE_BLOCK;
      /* Skip the asterisk. */
      (void)get_token();
    } else {
      /* Get the integer constant for the block size */
      a_constant_ptr  constant = local_constant();
      scan_integral_constant_expression(constant);
      switch(constant->kind) {
        case ck_integer:
          /* The block size must be greater than or equal to zero, with
             zero indicating an indefinite block size. */
          if (sign_of_integer_constant(constant) < 0) {
            pos_error(ec_shared_block_size_must_be_positive, &error_position);
            *err = TRUE;
          } else {
            a_host_large_unsigned  const_value =
                           unsigned_value_of_integer_constant(constant, err);
            block_size = (a_upc_block_size)const_value;
            if (!*err) {
              if (const_value == 0) {
                block_size = UPC_BLOCK_SIZE_INDEFINITE;
              } else {
                /* An error will be issued if the block size is too large. */
                *err = upc_block_size_too_large(const_value);
              }  /* if */
            }  /* if */
          }  /* if */
          break;
        case ck_upc_threads:
          pos_error(ec_threads_constant_not_allowed, &error_position);
          FALLTHROUGH
        case ck_error:
          *err = TRUE;
          break;
        default:
          unexpected_condition_str("UPC shared block size: bad constant kind");
      }  /* switch */
      release_local_constant(&constant);
    }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (decl_pos_block != NULL) {
      decl_pos_block->specifiers_range.end = end_pos_curr_token;
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    /* Make sure we have a right bracket */
    (void)required_token(tok_rbracket, ec_exp_rbracket);
    remove_stop_token(tok_rbracket);
    if (curr_token == tok_lbracket) {
      /* More than one block size found.  Scan them all
         to avoid misleading error messages. */
      pos_error(ec_multiple_block_sizes, &error_position);
      *err = TRUE;
      while (curr_token == tok_lbracket) {
        a_constant_ptr  dummy_constant = local_constant();
        (void)get_token();
        add_stop_token(tok_rbracket);
        scan_integral_constant_expression(dummy_constant);
        (void)required_token(tok_rbracket, ec_exp_rbracket);
        remove_stop_token(tok_rbracket);
        release_local_constant(&dummy_constant);
      }  /* while */
    }  /* if */
  }  /* if */
  state->upc_block_size = block_size;
}  /* scan_upc_block_size_if_any */

#endif /* UPC_EXTENSIONS_ALLOWED */

static a_boolean unelaborated_cfront_friend_class(void)
/*
Return whether this is a friend declaration of the form "friend X;" where
"X" has not yet been declared.  This is only accepted in Cfront modes.
The case where "X" is already declared is also a Cfront extension, but is
accepted by several other C++ compilers.  It is handled by
check_missing_declarator_in_member_declaration.
*/
{
  a_boolean  result = FALSE;

  check_assertion(curr_token == tok_friend && any_cfront_mode());
  /* Advance to the token following "friend". */
  (void)get_token();
  if (!is_decl_qualified_name_start()) {
    /* Can't be the start of an identifier -- back up to continue
       processing. */
    unget_token();
    curr_token = tok_friend;
  } else {
    /* Advance over the identifier, which may actually be a
       qualified name or even a template class. */
    a_boolean          lookup_err;
    a_symbol_ptr       tag_sym;
    a_source_position  ident_pos;

    ident_pos = pos_curr_token;
    tag_sym = coalesce_and_lookup_generalized_identifier(
                    GID_NO_OPTIONS, ilm_tentative_type, &lookup_err);
    /* Even if the lookup was successful, if the next token is not
       a ";" this is not of the form "friend T;". */
    if (next_token() != tok_semicolon || tag_sym != NULL) {
      /* If there is no semicolon following the class name, this
         is not a friend class declaration.  If tag_sym is not NULL,
         the class was already known and this is more easily handled
         in check_missing_declarator_in_member_declaration.  Either
         way, back up. */
      clear_specific_symbol(locator_for_curr_id);
      unget_token();
      curr_token = tok_friend;
    } else if (tag_sym == NULL) {
      /* This friend declaration introduces a new type -- which is
         okay in cfront compatibility mode.  Still, issue a remark
         on use of a nonstandard feature. */
      pos_st_remark(ec_nonstd_friend_decl, &ident_pos, "class");
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* unelaborated_cfront_friend_class */

#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED

static void scan_thread_local_storage_specifier(
                                       a_decl_flag_set         input_flags,
                                       a_decl_modifiers_block  *decl_modifiers)
/*
The current token ("__thread") indicates thread-local storage.  Update
decl_modifiers to reflect the specifier if appropriate.  Issue an error if
there are several such specifiers on the current declaration or if the
specifiers appear on a parameter declaration.  input_flags is the flag set
passed to the call to decl_specifiers.

Note that the "thread_local/_Thread_local" keyword isn't handled here (it's
handled in process_storage_class_specifier).
*/
{
  if (input_flags & DSI_IS_PARAMETER) {
    pos_error(ec_cannot_use_thread_local_storage, &error_position);
  } else if (decl_modifiers->flags & DM_THREAD) {
    pos_error(ec_multiple_thread_local_storage_specifiers, &error_position);
  } else {
    decl_modifiers->flags |= DM_THREAD;
  }  /* if */
}  /* scan_thread_local_storage_specifier */

#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED

static void scan_microsoft_inline_specifiers(
                                a_decl_flag_set         input_flags,
                                a_decl_flag_set         *output_flags,
                                a_decl_specifiers_set   *decl_specifiers_seen,
                                a_decl_modifiers_block  *decl_modifiers,
                                a_boolean               *err)
/*
Scan certain Microsoft-specific specifiers (__inline, and __forceinline) and
update *output_flags, *decl_specifiers_seen, and *decl_modifiers accordingly.
Set *err in case of an error.
*/
{
  an_extended_decl_info_block
                     extended_decl_info;
  a_source_position  specifier_start_pos;
  a_boolean          is_parameter = ((input_flags & DSI_IS_PARAMETER) != 0);

  specifier_start_pos = pos_curr_token;
  clear_extended_decl_info_block(extended_decl_info);
  /* __inline or __force_inline must be next. */
  switch (curr_token) {
    case tok_microsoft_inline:
	      extended_decl_info.decl_modifiers.flags = DM_MICROSOFT_INLINE;
      *decl_specifiers_seen |= DS_MICROSOFT_INLINE;
      *output_flags |= DSO_INLINE;
      break;
    case tok_forceinline:
	      extended_decl_info.decl_modifiers.flags = DM_FORCEINLINE;
      *decl_specifiers_seen |= DS_FORCEINLINE;
      *output_flags |= DSO_INLINE;
      break;
    default:
      unexpected_condition();
  }  /* switch */
  if (!(input_flags & DSI_STORAGE_CLASS_SPECIFIER_ALLOWED) &&
      !(input_flags & (DSI_IS_EXPLICIT_INSTANTIATION |
                       DSI_IS_SPECIALIZATION))) {
    /* When storage class specifiers are not allowed, a diagnostic is often
       emitted.  Exceptions are explicit instantiations and explicit
       specializations. */
    pos_diagnostic((an_error_severity)(microsoft_bugs ? es_warning
                                                      : es_error),
                   ec_storage_class_not_allowed, &specifier_start_pos);
    *err = TRUE;
  } else if (input_flags & DSI_IS_CONDITION_DECL) {
    pos_error(ec_storage_class_not_allowed, &specifier_start_pos);
    *err = TRUE;
  } else {
    /* There were no errors; update decl_modifiers to reflect this
       specifier. */
    a_decl_modifiers_block_ptr  new_modifiers =
                                           &extended_decl_info.decl_modifiers;
    decl_modifiers->flags |= new_modifiers->flags;
    if (is_parameter) {
      /* For parameters, warn if a storage class modifier is used. */
      pos_warning(ec_bad_param_storage_class, &specifier_start_pos);
    }  /* if */
  }  /* if */
}  /* scan_microsoft_inline_specifiers */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static a_boolean gpp_type_name_matches_class_name(a_symbol_ptr	sym)
/*
This routine is used in g++ mode to determine whether the parent class
in the current symbol locator has the same name as "sym". In g++ mode a
declaration like "A<1>::A<1>() is taken to name the constructor.  This routine
is used to detect this case.  The current token must be the identifier of
the prospective constructor name.  The caller is responsible for checking that
the current identifier is a class member and a template-id.
*/
{
  a_boolean result = FALSE;

  if (is_any_template_instance_class_symbol(sym)) {
    /* The current symbol is a template-id that names a class member.  Note
       that in the g++ case, of "A<1>::A<1>" the second "A<1>" actually names
       an instance of the parent template, and so the symbol is not a class
       member.  */
    if (sym->header ==
              symbol_for(qualifier_class_type(locator_for_curr_id))->header) {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* gpp_type_name_matches_class_name */


static a_boolean process_nontype_identifier(
                      a_decl_parse_state                  *dps,
                      a_decl_specifiers_set               decl_specifiers_seen,
                      a_decl_flag_set                     input_flags,
                      a_basic_type                        *basic_type,
                      ARG_UNUSED a_named_address_space_id *named_address_space,
                      a_boolean                           *err)
/*
The current token is an identifier or (in C++) a global qualification token
("::") followed by an identifier.  If the name introduced by this token is
a type name, it is normally part of the decl-specifier and handled by the
caller.  If the name is not a type name, the work is mostly done in this
routine or in declarator processing.  This includes named memory regions (part
of the specifiers; an Embedded C/TR 18037 extension) and constructors (part of
the declarator).
dps describes the declaration being parsed.  decl_specifiers_seen records some
of the specifiers (virtual, inline, ...) that may have been seen already.
input_flags is the flag set passed to decl_specifiers.  *basic_type represents
the basic type specifier that was seen (if any) and may be set to bt_no_type
if this is a constructor (or in C++/CLI, a static constructor).  If the current
token names an address space, *named_address_space is set to represent it;
otherwise it is set to zero.  Unusual syntax errors (e.g., "::" followed by
something unexpected) cause *err to be set to TRUE.
If the identifier is not part of the decl-specifiers (i.e., either part
of a declarator or a syntax error) return TRUE; otherwise return FALSE.
*/
{
  a_boolean  result = FALSE;
  a_boolean  identifier_names_address_space = FALSE;

  if (!C_mode()) {
    a_boolean  is_member_decl = (input_flags & DSI_IS_MEMBER_DECLARATION) != 0;
    an_identifier_options_set
               options = GID_NO_OPTIONS;
    /* In case the identifier has not yet been coalesced, do it now. */
    if (input_flags & DSI_IS_NEW_TYPE_NAME) {
      options |= GID_IS_NEW_TYPE_NAME;
    }  /* if */
    if (!is_generalized_identifier_start(options)) {
      if (curr_token == tok_decltype_construct) {
        /* We get here for a decltype because it could be followed by
           "::", in which case it is part of a qualified name.  If
           tok_decltype_construct is returned, it was not followed by
           "::", so is a normal decltype case. */
        goto done;
      } else {
        /* This could result from "::" followed by something strange. */
        *err = TRUE;
        result = TRUE;
      }  /* if */
    }  /* if */
    /* Check for a constructor declaration.  The following conditions
       must be satisfied:  (1) we are inside a class definition;
       (2) the current token is the name of the class being defined
       (note that typedef names are not allowed); (3) the declaration
       has no other specifiers besides "inline" and "explicit" (which
       are legal) and "virtual" or "static" (which are not); (4) the
       next token is a left parenthesis; (5) the token following the
       left paren is a right paren or the start of a formal parameter
       declaration. */
    if ((is_member_decl && !result) || dps->has_deducible_class_templ_args) {
      /* Permit specifiers that apply to member functions.  In some Clang and
         Microsoft modes cv-qualifiers are accepted too. */
      a_decl_specifiers_set  accepted_specifiers =
                                 DS_VIRTUAL | DS_STORAGE_CLASS | DS_EXPLICIT |
                                 DS_INLINE | DS_MICROSOFT_INLINE |
                                 DS_FORCEINLINE | DS_CONSTEXPR;
      if (clang_version_is(< 30500) || microsoft_mode) {
        accepted_specifiers |= DS_TYPE_QUALIFIER;
      }  /* if */
      if (!(decl_specifiers_seen & ~accepted_specifiers) &&
          (dps->declared_storage_class == (a_storage_class)sc_unspecified ||
           dps->declared_storage_class == (a_storage_class)sc_static)) {
        a_type_ptr  class_type = enclosing_class_type();
        a_boolean   is_static_ctor = FALSE;
        if (class_type != NULL) {
#if MICROSOFT_EXTENSIONS_ALLOWED
          /* Static constructors are not supported in C++/CX mode. */
          if (cppcli_enabled &&
              is_immediate_managed_class_type(class_type) &&
              dps->declared_storage_class == (a_storage_class)sc_static) {
            is_static_ctor = TRUE;
          }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          /* The following test will not succeed if the constructor
             declaration is parenthesized; in that case, the test is
             repeated in scan_real_declarator_id. */
          if (is_constructor_decl(class_type, dps)) {
            *basic_type = bt_no_type;
            if (is_static_ctor) {
              dps->dso_flags |= DSO_STATIC_CONSTRUCTOR;
            } else {
              dps->dso_flags |= DSO_CONSTRUCTOR | DSO_NO_DECL_SPECIFIERS;
            }  /* if */
            result = TRUE;
          } else if ((microsoft_bugs || any_cfront_mode()) &&
                     !is_error_locator(locator_for_curr_id) &&
                     implicit_int_member_with_name_of_type()) {
            /* Microsoft and Cfront will accept:
                 struct X; struct Y { X(); }; */
            dps->dso_flags |= DSO_NO_DECL_SPECIFIERS;
            result = TRUE;
          }  /* if */              
        }  /* if */              
      } else if ((decl_specifiers_seen & DS_FRIEND) != 0 &&
                 locator_for_curr_id.is_class_member &&
                 scope_stack_top().in_prototype_instantiation) {
        /* Consider a friend declaration of the form
               friend A<T>::A(...);
           When doing dependent name processing, A<T>::A will not be considered
           a type name because it isn't preceded by the keyword "typename".
           However, in default mode, it will be treated as a type name, and if
           it weren't for the processing here, it would unconditionally be
           handled as a specifiers type. */
        a_type_ptr  parent_type = locator_for_curr_id.parent.class_type;
        if (is_immediate_class_type(parent_type) &&
            parent_type->variant.class_struct_union.is_nonreal_class &&
            locator_for_curr_id.symbol_header ==
                                            symbol_for(parent_type)->header &&
            next_token() == tok_lparen) {
          /* The qualifier name matches the qualified name, and this looks like
             a function declarator (i.e., a left parenthesis is next). */
          if (!do_dependent_name_processing ||
              curr_type_symbol((input_flags & DSI_IS_NEW_TYPE_NAME) != 0,
                               /*in_prescan=*/FALSE,
                               /*in_type_check=*/FALSE,
                               /*is_implicit_type_context=*/FALSE,
                               /*is_expr_context=*/FALSE) == NULL) {
            /* The qualified name should not be treated as a type name: Assume
               a constructor is intended. */
            dps->dso_flags |= DSO_CONSTRUCTOR;
            result = TRUE;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
done:
#if NAMED_ADDRESS_SPACES_ALLOWED
  *named_address_space = 0;
  if (!result && named_address_spaces_enabled) {
    /* Check if the identifier corresponds to a named address space
       qualifier.  A tentative type lookup is done to suppress things like
       the out-of-scope declaration check done by normal_id_lookup. */
    a_symbol_ptr  sym = normal_id_lookup(&locator_for_curr_id,
                                         IDL_TENTATIVE_TYPE_LOOKUP);
    if (sym != NULL && sym->kind == (a_symbol_kind)sk_named_address_space) {
      *named_address_space = sym->variant.named_address_space.id;
      identifier_names_address_space = TRUE;
    }  /* if */
  }  /* if */
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
  if (!result && curr_token == tok_identifier &&
      *basic_type != bt_none && !identifier_names_address_space) {
    /* There's already a basic type, so the identifier should be processed as
       a declarator.  If it happens to be a type name, it is better to have an
       invalid-redeclaration error later than a bad-combination-of-types error
       here.  In addition, the following is permitted in C++:
           struct S {...};
           int S;
       since tag names are not in the same name space with other objects. */
    result = TRUE;
  }  /* if */
  return result;
}  /* process_nontype_identifier */


static void check_use_of_thread_local(a_decl_parse_state  *dps)
/*
Callback routine called at the end of processing for a declaration containing
the thread_local/_Thread_local specifier.  Issue an error if the specifier is
not applicable and ensure the IL reflects the presence of the specifier
otherwise.
*/
{
  a_symbol_ptr  sym = dps->sym;

  if ((dps->dso_flags & DSO_THREAD_LOCAL) == 0) {
    /* Nothing to do.  This can occur if previous diagnostics "canceled" the
       thread_local specifier. */
  } else if (sym == NULL || is_class_symbol(sym) || sym->kind == sk_type) {
    if (dps->type != NULL &&
        is_immediate_class_type(dps->type) &&
        dps->type->source_corresp.name == NULL) {
      /* "thread_local/_Thread_local" applied to an unnamed union/struct. */
      if (class_type_supp(dps->type)->anonymous_union_kind ==
                                           (an_anonymous_union_kind)auk_none) {
        if (C_mode()) {
          /* A case like:
               void f() { _Thread_local union { char x; int y; }; }
             This ends up declaring nothing because anonymous unions/structs
             can only appear as members.  Ignore it (an error has already been
             issued in strict mode). */
        } else {
          /* A case like:
               struct A { static thread_local union { char x; int y; }; }; */
          if (microsoft_mode) {
            /* Microsoft seems to ignore thread_local (with a warning) in this
               case. */
            pos_warning(ec_thread_local_ignored, &dps->storage_class_pos);
          } else {
            /* Issue a discretionary error. */
            pos_diagnostic(es_discretionary_error, ec_thread_local_not_allowed,
                           &dps->storage_class_pos);
          }  /* if */
        }  /* if */
      } else if (class_type_supp(dps->type)->anonymous_union_kind ==
                                       (an_anonymous_union_kind)auk_variable) {
        /* Something like:
             static thread_local union { char x; int y; };
           is allowed in all modes. */
      } else {
        /* An auk_field case (which is handled in the sk_field case below). */
        unexpected_condition();
      }  /* if */
    } else {
      /* No declaration is associated with "thread_local/_Thread_local":
         Issue an error. */
      pos_error(ec_thread_local_not_allowed, &dps->storage_class_pos);
    }  /* if */
  } else if (sym->is_error ||
             (dps->type != NULL && is_error_type(dps->type))) {
    /* An error has presumably already been reported for this declaration.
       An additional error is unlikely to be helpful. */
    expect_error();
  } else if (symbol_is(sym, sk_variable) ||
             symbol_is(sym, sk_static_data_member)) {
    /* "thread_local/_Thread_local" is only allowed on variables and static
       data members. */
    a_variable_ptr  vp = variable_for_symbol(sym);
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
    if (dps->decl_modifiers.flags & DM_THREAD) {
      /* Can't combine "__thread" and "thread_local/_Thread_local". */
      pos_error(ec_multiple_thread_local_storage_specifiers,
                &dps->storage_class_pos);
    }  /* if */
#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */
    if (!is_static_or_thread_storage_duration_storage_class(
                                                dps->declared_storage_class)) {
      /* If a storage class was specified with "thread_local/_Thread_local",
         it must be "extern" or "static". */
      pos_error(ec_cannot_use_thread_local_storage, &dps->storage_class_pos);
    }  /* if */
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
    if (vp->source_corresp.attributes != NULL) {
      /* Issue a warning for any attributes that conflict with
         "thread_local/_Thread_local" (this is done here rather than in
         attribute processing because is_thread_local is not set at the time
         attributes are being processed). */
      an_attribute_ptr ap;
      for (ap = vp->source_corresp.attributes; ap != NULL; ap = ap->next) {
        if (ap->kind == ak_init_priority) {
          pos_warning(ec_attribute_ignored_for_thread_local, &ap->position);
        }  /* if */
      }  /* for */
    }  /* if */
#endif /* GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED */
    if (C_mode() &&
        depth_innermost_function_scope != NO_SCOPE_NUMBER &&
        !(dps->declared_storage_class == (a_storage_class)sc_static ||
          dps->declared_storage_class == (a_storage_class)sc_extern)) {
      /* In the declaration of an object with block scope, if the declaration
         specifiers include _Thread_local, they shall also include either
         static or extern. */
      pos_error(ec_thread_local_must_include_static_or_extern,
                &dps->storage_class_pos);
    }  /* if */
    vp->is_thread_local = TRUE;
    if (vp->is_struct_binding_container) {
      /* Variable is the container for struct binding variables - mark all of
         the individual variables as thread local as well. */
      an_il_entity_list_entry_ptr binding_vars = vp->variant.bindings;
      for (; binding_vars != NULL; binding_vars = binding_vars->next) {
        a_variable_ptr binding_var = (a_variable_ptr)binding_vars->entity.ptr;
        binding_var->is_thread_local = TRUE;
      }  /* for */
    }  /* if */
    check_assertion_or_expect_error(var_has_thread_storage_duration(vp));
  } else if (symbol_is(sym, sk_field) &&
             sym->variant.field.ptr->is_anonymous_parent_object) {
    /* An anonymous union in a structure, i.e., something like:
         struct A { thread_local union { char x; int y; }; }; */
    if (strict_ansi_mode) {
      /* Don't allow this in strict mode. */
      pos_error(ec_thread_local_not_allowed, &dps->storage_class_pos);
    } else if (microsoft_mode) {
      /* Microsoft seems to ignore thread_local (with a warning) in this
         case. */
      pos_warning(ec_thread_local_ignored, &dps->storage_class_pos);
    } else {
      /* GNU and clang seem to silently accept this (and ignore it). */
    }  /* if */
  } else {
    pos_error(ec_thread_local_not_allowed, &dps->storage_class_pos);
  }  /* if */
}  /* check_use_of_thread_local */


static void process_storage_class_specifier(
                                  a_token_kind           first_token,
                                  a_decl_flag_set        input_flags,
                                  a_decl_parse_state     *state,
                                  a_decl_pos_block_ptr   decl_pos_block,
                                  a_boolean              first_specifier,
                                  a_decl_specifiers_set  *decl_specifiers_seen,
                                  a_boolean              *err)
/*
This is a helper function for decl_specifiers(...) called when the first_token
is the first token of a storage class specifier (or "mutable", which is
syntactically similar).  (first_token is usually also the only token of the
storage class specifier; the only exception are named-register storage class
specifiers.)  input_flags, state, and decl_pos_block are parameters forwarded
from decl_specifiers.  If first_specifier is TRUE, the storage specifier token
was the first decl-specifier (ignoring "friend", "inline", and
"thread_local/_Thread_local"); a warning may be issued if that is not the
case.  *decl_specifiers_seen is updated with an indication of the specifiers
that were consumed.  *err is set to TRUE if an error is issued.  All the
storage class specifier tokens are consumed by this routine, except for "auto"
which is processed after any other specifiers have also been consumed.
*/
{
  a_boolean          is_parameter = (input_flags & DSI_IS_PARAMETER) != 0;
  a_boolean          is_member_decl =
                               (input_flags & DSI_IS_MEMBER_DECLARATION) != 0;
  a_boolean          is_named_register = FALSE;
  a_source_position  pos_first_token;

  /* For "auto" as a storage class specifier, this routine is called after
     all specifiers have been scanned already.  For all other cases, the
     specifier token is the current token. */
  if (first_token != tok_auto) {
    check_assertion(curr_token == first_token);
    check_for_c23_deprecation("_Thread_local", ec_c23_thread_local_deprecated);
    pos_first_token = pos_curr_token;
    (void)get_token();
  } else {
    pos_first_token = state->auto_pos;
  }  /* if */
  if ((input_flags & DSI_MICROSOFT_SECONDARY_SPECIFIERS)) {
    pos_warning(ec_secondary_specifier_ignored, &pos_first_token);
    goto done;
  }  /* if */
#if NAMED_REGISTERS_ALLOWED
  if (named_registers_enabled && first_token == tok_register &&
      curr_token == tok_identifier) {
    a_symbol_ptr  sym = normal_id_lookup(&locator_for_curr_id, IDL_NO_OPTIONS);
    if (sym != NULL && sym->kind == (a_symbol_kind)sk_named_register) {
      is_named_register = TRUE;
      if (!(input_flags & DSI_REGISTER_ID_ALLOWED) ||
          (input_flags & (DSI_IS_MEMBER_DECLARATION |
                          DSI_IS_PARAMETER |
                          DSI_IS_CONDITION_DECL))) {
        pos_error(ec_named_register_not_allowed, &pos_curr_token);
      } else {
        state->register_id = sym->variant.named_register.id;
      }  /* if */
      (void)get_token();
    }  /* if */
  }  /* if */
#endif /* NAMED_REGISTERS_ALLOWED */
  if (!(input_flags & DSI_STORAGE_CLASS_SPECIFIER_ALLOWED)) {
    pos_error((!C_mode() && first_token == tok_typedef) ?
                       ec_typedef_not_allowed : ec_storage_class_not_allowed,
               &pos_first_token);
    *err = TRUE;
#if ASM_FUNCTION_ALLOWED
  } else if (state->declared_storage_class == (a_storage_class)sc_asm) {
    pos_error(ec_storage_class_not_allowed, &pos_first_token);
    *err = TRUE;
#endif /* ASM_FUNCTION_ALLOWED */
  } else if (*decl_specifiers_seen & (DS_STORAGE_CLASS | DS_MUTABLE) &&
             !(first_token == tok_thread_local ||
               first_token == tok_c11_thread_local)) {
    /* More than one storage class may not be specified (except for the
       "thread_local/_Thread_local" storage class, which may be combined with
       "extern" or "static").  Note that the diagnostic should be issued on
       the second storage class, but since "auto" is processed after all the
       other specifiers, something like "auto register x;" needs special care
       to get the position of the "register" keyword. */
    a_source_position  *diag_pos = &pos_first_token;
    if (first_token == tok_auto) {
      if (cmp_source_positions(state->storage_class_pos,
                               pos_first_token) > 0) {
        diag_pos = &state->storage_class_pos;
      }  /* if */
    }  /* if */
    pos_error(ec_mult_storage_classes, diag_pos);
    *err = TRUE;
  } else if (is_parameter && first_token != tok_register &&
             (C_mode() || first_token != tok_auto)) {
    /* For parameters, the only allowed storage class specifiers are
       "register" and (in C++ only) "auto". */
    if (first_token == tok_typedef) {
      if (input_flags & DSI_IS_OLD_STYLE_PARAM_DECL) {
        /* Error will be handled by caller. */
        state->declared_storage_class = (a_storage_class)sc_typedef;
        *decl_specifiers_seen |= DS_STORAGE_CLASS;
      } else {
        pos_error(ec_typedef_not_allowed, &pos_first_token);
        *err = TRUE;
      }  /* if */
    } else {
      /* "static" and "extern" aren't allowed on parameter declarations,
         but Microsoft compilers ignore them with a warning. */
      pos_diagnostic(ms_extensions ? es_warning : es_error,
                     ec_bad_param_storage_class, &pos_first_token);
      *err = !ms_extensions;
    }  /* if */
  } else if (first_token == tok_mutable) {
    if (!is_member_decl || (*decl_specifiers_seen & DS_FRIEND)) {
      pos_error(ec_mutable_not_allowed, &pos_first_token);
      *err = TRUE;
    } else {
      /* "mutable" is outside the "Embedded C++" subset. */
      feature_is_not_part_of_embedded_cplusplus_subset(
                                       &pos_first_token,
                                       ec_mutable_in_embedded_cplusplus);
      /* Aside from interactions with storage classes, errors cannot
         be issued on mutable until the declarator has been scanned.
         Just return a flag to the caller. */
      *decl_specifiers_seen |= DS_MUTABLE;
      state->dso_flags |= DSO_MUTABLE;
      if ((*decl_specifiers_seen & DS_STORAGE_CLASS) == 0) {
        /* If there's already a "real" storage class, don't overwrite its
           position. */
        state->storage_class_pos = pos_first_token;
      }  /* if */
    }  /* if */
  } else if (first_token == tok_thread_local ||
             first_token == tok_c11_thread_local) {
    /* Do some checking for the "thread_local/_Thread_local" specifier. */
    if (input_flags & DSI_IS_CONDITION_DECL) {
      pos_error(ec_storage_class_not_allowed, &pos_first_token);
      *err = TRUE;
    } else if (*decl_specifiers_seen & DS_FRIEND) {
      pos_error(ec_thread_local_not_allowed, &pos_first_token);
      *err = TRUE;
    } else if (*decl_specifiers_seen & DS_THREAD_LOCAL) {
      /* Duplicate "thread_local/_Thread_local" specifiers. */
      pos_error(ec_dupl_decl_specifier, &error_position);
    } else {
      /* Mark that we've seen "thread_local/_Thread_local" and register a
         callback routine to perform additional checks once the declaration
         has been scanned. */
      *decl_specifiers_seen |= DS_THREAD_LOCAL;
      state->dso_flags |= DSO_THREAD_LOCAL;
      if ((*decl_specifiers_seen & DS_STORAGE_CLASS) == 0) {
        /* If there's already a "real" storage class, don't overwrite its
           position. */
        state->storage_class_pos = pos_first_token;
      }  /* if */
      add_end_of_parse_action(check_use_of_thread_local, state,
                              /*secondary_decls=*/TRUE);
    }  /* if */
  } else if ((*decl_specifiers_seen & DS_FRIEND) &&
             !ms_extensions && !sun_mode) {
    /* Note: in Microsoft and Sun modes a friend function can
       be declared "static" or "extern".  The check is done later. */
    pos_error(ec_storage_class_in_friend_decl, &pos_first_token);
    *err = TRUE;
  } else if ((input_flags & DSI_IS_SPECIALIZATION) &&
             first_token != tok_static && first_token != tok_extern) {
    pos_error(first_token == tok_typedef ?
                ec_typedef_not_allowed : ec_storage_class_not_allowed,
              &pos_first_token);
    *err = TRUE;
  } else if (is_member_decl && !ms_extensions &&
             !(*decl_specifiers_seen & DS_FRIEND) &&
             first_token != tok_static && first_token != tok_typedef) {
    /* A declaration like "extern int i;" in a C++ class definition. */
    check_assertion(!C_mode());
    pos_error(ec_bad_member_storage_class, &pos_first_token);
    *err = TRUE;
  } else if ((input_flags & DSI_IS_LINKAGE_SPEC_DECL) &&
             first_token != tok_typedef &&
             !ms_extensions && !gpp_mode && !sun_mode) {
    /* We disallow
         extern "C" static void f();
       in our default and strict modes, but Microsoft, GNU, and Sun all allow
       it.  In order to support association between a name linkage and a
       function type we do allow
         extern "C" typedef void FT();
    */
    pos_error(ec_storage_class_not_allowed, &pos_first_token);
    *err = TRUE;
  } else if (input_flags & DSI_IS_TEMPLATE_DECLARATION &&
             first_token != tok_extern && first_token != tok_static) {
    pos_error(first_token == tok_typedef ?
                ec_typedef_not_allowed : ec_bad_storage_class_on_template_decl,
              &pos_first_token);
    *err = TRUE;
  } else if (C_mode() && depth_scope_stack == DEPTH_OF_FILE_SCOPE &&
             (first_token == tok_auto ||
              (first_token == tok_register &&
#if GNU_EXTENSIONS_ALLOWED
               /* In GNU C mode, "register" can appear at file
                  scope, as long as an explicit register name is
                  provided. (Not in GNU C++ mode, however.) */
               !gcc_mode &&
#endif /* GNU_EXTENSIONS_ALLOWED */
               !is_named_register))) {
    if (gcc_mode) {
      pos_warning(ec_auto_ignored, &pos_first_token);
    } else {
      pos_error(ec_bad_file_scope_storage_class, &pos_first_token);
      *err = TRUE;
    }  /* if */
  } else if (input_flags & DSI_IS_CONDITION_DECL) {
    /* Issue a diagnostic for the specification of a storage class on
       a condition declaration.  Ignore auto and register except in
       strict mode.  Only set *err if an error is issued.  Do not
       set state->declared_storage_class. */
    an_error_severity  es;
    if (first_token == tok_auto || first_token == tok_register) {
      es = strict_ansi_mode ? strict_ansi_error_severity : es_none;
    } else {
      es = es_error;
    }  /* if */
    /* Put out the diagnostic. */
    if ((int)es != (int)es_none) {
      pos_diagnostic(es,
                     first_token == tok_typedef ? ec_typedef_not_allowed
                                                : ec_storage_class_not_allowed,
                     &pos_first_token);
    }  /* if */
    /* If an error was issued, set the flag; otherwise, set the bit in
       *decl_specifiers_seen, so that the multiple-storage-class
       diagnostic will be put out if another storage class is
       specified. */
    if ((int)es > (int)es_warning) {
      *err = TRUE;
    } else {
      *decl_specifiers_seen |= DS_STORAGE_CLASS;
    }  /* if */
  } else if (c99_mode &&
             !(first_token == tok_auto || first_token == tok_register) &&
             depth_stmt_stack > 0 &&
             struct_stmt_stack[depth_stmt_stack].for_init) {
    /* A for-init declaration in C99 can only have storage class
       auto or register. */
    pos_error(ec_invalid_storage_class_in_for_init, &pos_first_token);
    *err = TRUE;
  } else {
    if (C_dialect != C_dialect_pcc && !*err) {
      if (!first_specifier) {
        /* Issue a diagnostic if the storage class is not the first
           specifier (except for "inline", "friend", "constexpr" or
           "thread_local/_Thread_local"). */
        pos_diagnostic(strict_ansi_mode ? es_warning : es_remark,
                       ec_storage_class_not_first, &pos_first_token);
      }  /* if */
    }  /* if */
    if (first_token == tok_register) {
      /* The use of "register" was deprecated in C++11 and removed from the
         language in C++17.  Issue a warning or error as appropriate, but only
         on the first use outside of a system header. */
      if (!seq_is_in_system_header(pos_first_token.seq)) {
        if (register_is_disallowed) {
          pos_error(ec_register_keyword_disallowed, &pos_first_token);
          (void)set_severity_for_error_number(
                                           (int)ec_register_keyword_disallowed,
                                           es_once, /*make_default=*/FALSE);
          *err = TRUE;
          goto done;
        } else if (register_is_deprecated) {
          pos_warning(ec_register_keyword_deprecated, &pos_first_token);
          (void)set_severity_for_error_number(
                                           (int)ec_register_keyword_deprecated,
                                           es_once, /*make_default=*/FALSE);
        }  /* if */
      }  /* if */
    }  /* if */
    *decl_specifiers_seen |= DS_STORAGE_CLASS;
    state->storage_class_pos = pos_first_token;
    if (decl_pos_block != NULL) {
      /* Set the source position of the storage class for use by the
         caller in issuing diagnostics. */
      decl_pos_block->storage_class_pos = pos_first_token;
    }  /* if */
    switch (first_token) {
      case tok_typedef:
        state->declared_storage_class = (a_storage_class)sc_typedef;  break;
      case tok_extern:
        state->declared_storage_class = (a_storage_class)sc_extern;   break;
      case tok_static:
        state->declared_storage_class = (a_storage_class)sc_static;   break;
      case tok_auto:
        state->declared_storage_class = (a_storage_class)sc_auto;     break;
      case tok_register:
        /* The storage class depends on whether this was a classic (unnamed)
           register storage specifier, or a named-register storage specifier
           (the latter is an Embedded C extension). */
        /* coverity[dead_error_condition] */
        state->declared_storage_class =
                             is_named_register ? (a_storage_class)sc_extern
                                               : (a_storage_class)sc_register;
        break;
      default:
        unexpected_condition_str("decl_specifiers: bad storage class");
    }  /* switch */
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (first_token == tok_typedef && microsoft_attribute_tokens_next()) {
    /* Microsoft attributes can follow the typedef keyword. */
    scan_and_append_microsoft_attributes(&state->ms_attributes,
                                         /*is_param_or_base=*/FALSE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
done:;
}  /* process_storage_class_specifier */


static void check_c_auto_type(a_decl_parse_state  *dps)
/*
A type specifier whose type is deduced from an initializer -- "auto" in C23 or
the GNU C "__auto_type" extension -- was used in C mode.  Make sure that a
diagnostic is issued if it is not valid.
*/
{
  if (dps->auto_type == NULL) {
    expect_error();
  } else {
    a_type_ptr    utp = skip_typerefs(dps->auto_type);
    a_const_char  *spelling = c_auto_specifier_spelling(dps);
    /* Deduction replaces the placeholder type, which is a tk_unknown entry
       for "__auto_type" and an "auto" type entry for the C23 specifier.  If
       one of those is still in place, no deduction was done. */
    if (utp->kind == (a_type_kind)tk_unknown || is_auto_type(utp)) {
      /* The specifier did not appear in a valid context (it was not the type
         specifier for a variable declaration with an initializer). */
      if (dps->sym != NULL && symbol_is(dps->sym, sk_variable)) {
        pos_st_error(ec_gnu_auto_type_without_initializer, &dps->auto_pos,
                     spelling);
      } else {
        pos_st_error(ec_bad_gnu_auto_type, &dps->auto_pos, spelling);
      }  /* if */
      set_type_kind(utp, (a_type_kind)tk_error);
    } else if (dps->secondary_declarator && !is_error_type(utp)) {
      pos_st_error(ec_gnu_auto_type_with_secondary_declarator, &dps->auto_pos,
                   spelling);
      set_type_kind(utp, (a_type_kind)tk_error);
    }  /* if */
  }  /* if */
}  /* check_c_auto_type */


static void process_auto_specifier(
                                 a_boolean              auto_type_allowed,
                                 a_boolean              first_specifier,
                                 a_decl_flag_set        input_flags,
                                 a_decl_parse_state     *state,
                                 a_decl_pos_block_ptr   decl_pos_block,
                                 a_decl_specifiers_set  *decl_specifiers_seen,
                                 a_basic_type           *basic_type,
                                 a_type_ptr             *type_ptr,
                                 a_boolean              *err)
/*
Process the "auto" or "decltype(auto)" specifier.  Depending on the mode,
"auto" can be a storage class specifier, a type specifier, or both.  If it can
be both, we cannot determine which it is until all specifiers have been seen,
and this routine is called late (i.e., after all specifiers have been seen).
If it cannot be both, this routine is called early because in
    typedef int T; T x;
    void f() { auto T(x); }
we must know that a type specifier ("auto") was seen to avoid treating T as a
type specifier (here, it is a declarator-id).
auto_type_allowed is TRUE if the current context allows "auto" as a type
specifier.  first_specifier is TRUE if "auto"/"decltype(auto)" was the first
specifier other than "inline" or "friend".  input_flags are the flags passed
to decl_specifier (for which this is a helper routine).  *state and
*decl_pos_block track various properties of the current declaration parsing
state; they may be updated by this routine.  *decl_specifiers_seen records the
kind of specifiers seen (and may be updated).  *basic_type and *type_ptr
describe the type specified by the specifiers and are updated if
"auto"/"decltype(auto)" is treated as a type specifier.  *err is set to TRUE
if an error is issued.
*/
{
  if ((auto_type_specifier_enabled || clangcpp_version_is(any_version)) &&
      (!auto_storage_class_specifier_enabled ||
       state->decltype_auto_specifier_seen ||
       (!(*decl_specifiers_seen & DS_TYPE) &&
        (input_flags & DSI_TYPE_SPECIFIER_ALLOWED)))) {
    /* Either this is "decltype(auto)" or else it is "auto" in a mode where
       "auto" can be a type specifier.  In the "auto" case, it cannot be a
       storage class specifier because this mode doesn't allow it or because
       no other type specifier was seen. */
    if (!auto_type_allowed ||
        (*decl_specifiers_seen & (DS_TYPE | DS_VOID)) != 0 ||
        ((input_flags & DSI_IS_PARAMETER) != 0 &&
         state->decltype_auto_specifier_seen)) {
      /* If the current mode supports "auto" as a type specifier, but the
         current context does not (e.g., a typedef declaration), issue an
         error that is specific for "auto" but does not imply whether it is a
         type specifier or a storage class.  If "decltype(auto)" is used for a
         parameter type, issue an error specific to "decltype(auto)".
         Otherwise, if a type specifier has already been seen, report a bad
         combination of type specifiers. */
      an_error_code  err_code = state->decltype_auto_specifier_seen ?
                                           ec_decltype_auto_not_allowed_here :
                                           ec_auto_not_allowed_here;
      if (auto_type_allowed &&
          !((input_flags & DSI_IS_PARAMETER) != 0 &&
            state->decltype_auto_specifier_seen)) {
        err_code = ec_bad_combination_of_type_specifiers;
      }  /* if */
      pos_error(err_code, &state->auto_pos);
      *basic_type = bt_error;
      *type_ptr = error_type();
      *err = TRUE;
      discard_placeholder_type(state);
    } else {
      *basic_type = bt_auto;
      state->auto_type = make_auto_type(&state->auto_pos,
                                        state->decltype_auto_specifier_seen);
      state->auto_type->variant.template_param.extra_info
                      ->constraint.type_constraint = state->type_constraint;
      *type_ptr = state->auto_type;
      if (!auto_type_specifier_enabled) {
        pos_warning(ec_auto_type_nonstandard, &state->auto_pos);
      }  /* if */
      if (C_mode()) {
        /* In C the deduction is done as the initializer is scanned, so a
           declaration that provides no initializer, or that is not a variable
           declaration at all, has to be diagnosed once the declaration has
           been parsed. */
        add_end_of_parse_action(check_c_auto_type, state,
                                /*secondary_decls=*/TRUE);
      }  /* if */
    }  /* if */
    *decl_specifiers_seen |= DS_TYPE;
  } else {
    /* "auto" must be a storage class specifier. */
    state->auto_type_specifier_seen = FALSE;
    state->has_deduced_type = FALSE;
    process_storage_class_specifier(
                               tok_auto, input_flags, state, decl_pos_block,
                               first_specifier, decl_specifiers_seen, err);
  }  /* if */
}  /* process_auto_specifier */


void cache_attributes(a_token_cache  *cache)
/*
If the current tokens introduce GNU or standard attributes, cache those
attributes in the given cache.  If the attributes are malformed, an arbitrary
number of tokens might be cached instead.
*/
{
  for (;;) {
    if (curr_token == tok_attribute) {
      /* Skip past the __attribute__ token to the left parenthesis that
         presumably follows. */
      cache_curr_token(cache);
      (void)get_token();
      if (curr_token != tok_lparen) break;
    } else if (std_attribute_tokens_next()) {
    } else {
      break;
    }  /* if */
    (void)cache_token_stream_until_matching_token(cache, CTS_NO_OPTIONS); 
    /* Skip past the closing ')' or ']'. */
    cache_curr_token(cache);
    (void)get_token();
  }  /* for */
}  /* cache_attributes */


static a_boolean auto_for_trailing_return_type(void)
/*
The caller has determined that the current token is "auto".  Return TRUE if it
appears to be the introducer for a trailing return type.
*/
{
  a_boolean               result;
  a_scanning_token_cache  cache;

  /* Cache the "auto" token. */
  cache_curr_token(cache.ptr());
  (void)get_token();
  cache_attributes(cache.ptr());
  if (curr_token == tok_lparen) {
    /* A left parenthesis can be:
         (a) the start of a function declarator,
         (b) the start of a parenthesized initializer, or
         (c) the start of a parenthesized declarator component.
       In case (a), a trailing return type can be recognized by a "->" token
       that immediately follows.  Case (b) cannot immediately be followed by a
       "->", nor can it be followed by cases (a) or (c).  Case (c) can be
       followed by case (a) (and potentially a trailing return type), or by
       case (b) (which cannot have a trailing return type).  So our algorithm
       can be:
         - skip one (case (a)) or two (case (c)+(a)) parenthesized token
           sequences
         - return whether the next token is "->".
       For example:
         void f(auto(*)());       // Return FALSE.  (Abbreviated template.)
         void f(auto(*)()->int);  // Return TRUE.  (Not a template.)
         void f(auto()->int);     // Return TRUE.  (Not a template.)
    */        
    result = FALSE;
    for (int n = 0; n<2; ++n) {
      if (cache_token_stream_until_matching_token(cache.ptr(),
                                                  CTS_NO_OPTIONS)) {
        /* Did not find a matching tok_rparen. */
        break;
      } else  {
        /* Put the current token (tok_rparen) in the cache. */
        cache_curr_token(cache.ptr());
        (void)get_token();
        cache_attributes(cache.ptr());
        if (curr_token == tok_arrow) {
          result = TRUE;
          break;
        } else if (curr_token != tok_lparen) {
          break;
        }  /* if */
      }  /* if */
    }  /* for */
  } else if (curr_token == tok_identifier) {
    /* This identifier could be the name of a function/parameter, followed by
       a parenthesized parameter list, followed by a trailing return type. */
    /* Put the identifier in the cache. */
    cache_curr_token(cache.ptr());
    (void)get_token();
    cache_attributes(cache.ptr());
    if (curr_token == tok_lparen) {
      result = FALSE;
      if (!cache_token_stream_until_matching_token(cache.ptr(),
                                                   CTS_NO_OPTIONS)) {
        /* Found a matching tok_rparen: Cache it to see if it is followed by
           a "->" token. */
        cache_curr_token(cache.ptr());
        (void)get_token();
        cache_attributes(cache.ptr());
        if (curr_token == tok_arrow) {
          result = TRUE;
        }  /* if */
      }  /* if */
    } else {
      /* No function declarator. */
      result = FALSE;
    }  /* if */
  } else {
    result = FALSE;
  }  /* if */
  rescan_cached_tokens(cache.ptr());
  return result;
}  /* auto_for_trailing_return_type */


static a_boolean process_auto_parameter(a_decl_parse_state  *dps,
                                        a_symbol_ptr        concept_sym)
/*
*dps describes the declaration of a parameter and the current token is "auto"
(if concept_sym is NULL) or a type constraint associated with concept_sym
(in which case the type constraint must be followed by an "auto" token).
If we are in a non-template context that might be turned into an implicit
template context because of an "auto" parameter, record the presence of the
"auto" parameter and (optionally) its associated type constraint, set
dps->specifiers_type to an "auto" type, and return TRUE.  If we are in a
template declaration or rescanning context, set dps->specifiers_type to the
template parameter type previously created for this occurrence of "auto"
(determined through its token sequence number) and return TRUE.  Otherwise,
return FALSE.  
*/
{
  a_boolean                result = FALSE;
  a_decl_parse_state       *func_dps = dps->assoc_func_decl_state;
  a_scope_stack_entry_ptr  ssep = &scope_stack_top();

  if (dps->is_top_level_param_decl &&
      (abbr_func_templates_enabled ||
       (func_dps->is_lambda && generic_lambdas_enabled)) &&
      (func_dps->decl_being_cached ||
       func_dps->is_template_declaration ||
       func_dps->is_template_rescan ||
       func_dps->is_abbr_func_template) &&
      !auto_storage_class_specifier_enabled) {
    an_expr_node_ptr  constraint;
    check_assertion(func_dps != NULL && scope_is(ssep, sck_func_prototype) &&
                    (concept_sym != NULL) != (curr_token == tok_auto));
    constraint = concept_sym != NULL ? scan_type_constraint(concept_sym)
                                     : NULL;
    if (func_dps->is_template_rescan || func_dps->is_abbr_func_template) {
      /* An instantiation ("rescan") of a template or the reparsing of a
         declaration that was found to have "auto" parameters and for which
         the corresponding template parameters have now been declared. */
      if (curr_token == tok_auto && !auto_for_trailing_return_type()) {
        /* Search for the corresponding template parameter. */
        a_template_param_ptr  tpp;
        if (func_dps->is_template_rescan) {
          ssep = &scope_stack[depth_innermost_instantiation_scope];
        } else {
          ssep = &scope_stack[depth_template_declaration_scope];
        }  /* if */
        /* Search for the parameter that corresponds to this "auto" token. */
        tpp = ssep->template_decl_info->parameters;
        for (; tpp != NULL; tpp = tpp->next) {
          a_symbol_ptr  sym = tpp->param_symbol;
          if (sym->token_sequence_number == curr_token_sequence_number) {
            dps->specifiers_type = type_symbol_type(sym);
            if (type_is(dps->specifiers_type, tk_template_param)) {
              /* Record the type constraint. */
              dps->specifiers_type->variant.template_param.extra_info
                                  ->constraint.type_constraint = constraint;
            }  /* if */
            result = TRUE;
            break;
          }  /* if */
        }  /* for */
        check_assertion(tpp != NULL);
        if (tpp->is_pack) {
          record_potential_pack_reference(tpp->param_symbol, &pos_curr_token);
        }  /* if */
      }  /* if */
    } else if (func_dps->decl_being_cached ||
               func_dps->is_template_declaration) {
      /* Either a declaration that is being cached even though it didn't start
         with a "template" token, or a template declaration that started with
         "template" but which hasn't been marked as an "abbreviated function
         template" yet.  Record the presence of an "auto" parameter if
         appropriate: When the function parameters have all been parsed,
         template parameters will be declared for each "auto" parameter and
         parsing will be restarted. */
      if (curr_token == tok_auto) {
        if (!auto_for_trailing_return_type()) {
          /* An auto parameter: Record a description of this to declare
             template parameters later on. */
          record_auto_param_descr(func_dps, constraint);
          /* Also point to the description from the parse state of the
             parameter, in case this turns out to be a parameter pack later
             on. */
          dps->variant.auto_params = func_dps->variant.auto_params;
          dps->pack_ellipsis_allowed = TRUE;
          /* Record the type as an ordinary "auto" type specifier.  It will
             eventually be discarded since we will reparse the declaration in
             a context where the "auto" can be mapped to a specific template
             parameter (see above). */
          dps->auto_pos = pos_curr_token;
          dps->auto_type = make_auto_type(&dps->auto_pos,
                                          /*is_decltype_auto=*/FALSE);
          dps->specifiers_type = dps->auto_type;
          result = TRUE;
        } else if (constraint != NULL) {
          /* Something like "void f(Concept auto f()->int);".  The concept is
             meaningless in that context. */
          pos_error(ec_invalid_use_of_concept, &dps->specifiers_pos);
          dps->auto_pos = pos_curr_token;
          dps->auto_type = make_auto_type(&dps->auto_pos,
                                          /*is_decltype_auto=*/FALSE);
          dps->specifiers_type = dps->auto_type;
          dps->auto_type_specifier_seen = TRUE;
        }  /* if */
      } else {
        pos_error(ec_exp_auto, &pos_curr_token);
        dps->specifiers_type = error_type();
      }  /* if */
    } else {
      expect_error();
      dps->specifiers_type = error_type();
    }  /* if */
  }  /* if */
  return result;
}  /* process_auto_parameter */


static void scan_specifier_attributes(a_decl_flag_set     flags,
                                      a_decl_parse_state  *dps,
                                      a_boolean           *std_attr_seen)
/*
Scan specifier attributes and record them in *dps.  flags is the set of input
flags passed to the call of decl_specifiers (which in turn called this
function).  Issue a diagnostic if attributes appear in a context that doesn't
allow for them (as indicated by flags); such attributes are otherwise ignored.
*std_attr_seen is set to TRUE if standard attributes were encountered, and to
FALSE otherwise.

Note that Microsoft appears to have a bug where attributes in this syntactic
location are treated as though they appeared in the prefix position.
*/
{
  a_boolean         treat_as_prefix = (microsoft_mode && microsoft_bugs &&
                                       !dps->is_type_name);
  an_attribute_ptr  ap = scan_attributes(treat_as_prefix ? al_prefix :
                                                           al_specifier);

  *std_attr_seen = FALSE;
  if (ap != NULL) {
    a_boolean         disallow_std = !(flags & DSI_STD_ATTRIBUTES_ALLOWED);
    a_boolean         disallow_gnu = !(flags & DSI_GNU_ATTRIBUTES_ALLOWED);
    a_boolean         diag_emitted = FALSE;
    an_attribute_ptr  *p_ap = &ap;
    /* Traverse the attributes dropping any that aren't allowed and recording
       the presence of a standard attribute (if one is allowed). */
    do {
      a_boolean  drop_attribute = FALSE;
      if (disallow_gnu && is_gcc_attribute(*p_ap)) {
        drop_attribute = TRUE;
      } else if (is_std_attribute(*p_ap) &&
                 !treat_as_prefix &&
                 !(c11_mode && (*p_ap)->family == af_alignas)) {
        if (disallow_std) {
          drop_attribute = TRUE;
        } else {
          *std_attr_seen = TRUE;
        }  /* if */
      }  /* if */
      if (drop_attribute) {
        if (!diag_emitted) {
          pos_diagnostic(gpp_mode ? es_warning : es_error,
                         ec_attribute_not_allowed, &(*p_ap)->position);
          diag_emitted = TRUE;
        }  /* if */
        *p_ap = (*p_ap)->next;
      } else {
        p_ap = &(*p_ap)->next;
      }  /* if */
    } while (*p_ap != NULL);
    if (treat_as_prefix) {
      *last_attribute_link(&dps->prefix_attributes) = ap;
      *std_attr_seen = FALSE;
    } else {
      *last_attribute_link(&dps->specifier_attributes) = ap;
    }  /* if */
  }  /* if */
}  /* scan_specifier_attributes */

#if MICROSOFT_EXTENSIONS_ALLOWED

static void preapply_declspec_attributes(a_decl_parse_state  *dps)
/*
Check for the presence of certain __declspec attributes (which at this point
haven't yet been applied to the entity that will presumably be declared), and
adjust *dps accordingly.
Set dps->is_declspec_property_field if a __declspec(property(...)) attribute
was seen and the current scope is a class scope (issue a diagnostic and
disable the "property" attribute if the current scope is not a class scope).
If the __declspec(dllimport) or __declspec(dllexport) were seen, perform some
early check and adjustments:
  - Check that both are not present simultaneously.
  - Set dps->decl_modifiers accordingly.
  - If __declspec(dllimport) is present without any storage class specifier,
    set the storage class to sc_extern.
(Note: The general attributes application mechanism doesn't actually record
the DLL flags.  That is done elsewhere using dps->decl_modifiers.flags.)
*/
{
  an_attribute_ptr  ap = find_attribute(ak_property, dps->prefix_attributes);

  if (ap != NULL) {
    if (scope_stack_top().kind != (a_scope_kind)sck_class_struct_union) {
      pos_st_error(ec_attr_must_appear_in_class_definition, &ap->position,
                   ap->name);
      make_attr_unrecognized(ap);
    } else {
      dps->is_declspec_property_field = TRUE;
      dps->is_property_or_event_field = TRUE;
    }  /* if */
  }  /* if */
  add_flags_from_dll_attributes(&dps->decl_modifiers.flags,
                                dps->prefix_attributes);
  if (dps->storage_class == (a_storage_class)sc_unspecified &&
      !dps->in_class_scope &&
      (dps->decl_modifiers.flags & DM_DLLIMPORT) != 0) {
    dps->storage_class = (a_storage_class)sc_extern;
  }  /* if */
}  /* preapply_declspec_attributes */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

static void attach_specifier_attributes(a_decl_parse_state  *dps)
/*
Attach "specifier attributes" to the type indicated by the specifiers (which
produces a new type).  For standard C++11 attributes, "specifier attributes"
are in principle those attributes that were scanned after seeing the
decl-specifiers.  E.g., in
  [[noreturn]] int [[XYZ::abc]] f();
[[noreturn]] is a prefix attribute and [[XYZ::abc]] is a specifier attribute.
In GNU mode, however, there is no distinction between prefix and specifier
attributes; instead, each attribute kind either applies to the declaration as
a whole or to the specifiers type.  E.g.:
  __attribute((vector_size(16))) int __attribute((noreturn)) g();
is equivalent to
  __attribute((noreturn)) int __attribute((vector_size(16))) g();
(the vector_size attribute applies to the specifiers type and the noreturn
attribute applies to the declaration as a whole).
Similarly, Microsoft compilers make no distinction between prefix __declspec
attributes and specifier attributes (unlike GNU attributes, there are no
type-modifying __declspec attributes).
This routine therefore moves any entries on the dps->specifier_attributes list
that don't apply to types to the dps->prefix_attributes list.  Conversely, any
entries on the dps->prefix_attributes that do transform types are moved to the
dps->specifier_attributes list.
*/
{
  if (dps->prefix_attributes != NULL || dps->specifier_attributes != NULL) {
    an_attribute_ptr  to_prefix = NULL, to_specifier = NULL;
    an_attribute_ptr  *end_to_prefix, *end_to_specifier, *end_specifier, *p_ap;
    end_to_prefix = &to_prefix;
    end_to_specifier = &to_specifier;
    /* First move any non-type attributes from the specifiers list to the
       prefix list. */
    p_ap  = &dps->specifier_attributes;
    while (*p_ap != NULL) {
      if (!is_type_transforming_attribute(*p_ap) &&
          (!is_unapplicable_attr(*p_ap) ||
           /* In a case like: "__attribute__((X)) auto f() -> int;" ensure that
              the unrecognized attribute isn't attached to the "auto" type.
              Similarly, move the unknown attribute if there was no specified
              type (e.g., on a constructor). */
           (is_gcc_attribute(*p_ap) &&
            (is_auto_type(dps->specifiers_type) ||
             is_unknown_type(dps->specifiers_type))))) {
        /* Move the attribute to the prefix attributes list. */
        an_attribute_ptr  ap = *p_ap;
        if (is_std_attribute(ap) && !ap->is_std_gcc_attribute &&
            !(c11_mode && ap->family == af_alignas)) {
          report_bad_attribute_target(gpp_mode ? es_warning : es_error, ap);
        }  /* if */
        *p_ap = ap->next;
        ap->next = NULL;
        ap->syntactic_location = al_prefix;
        if (ap->kind == ak_enable_if) {
          dps->pending_prefix_enable_if_attr = TRUE;
        }  /* if */
        *end_to_prefix = ap;
        end_to_prefix = &ap->next;
      } else {
        /* Proceed to the next attribute. */
        p_ap = &(*p_ap)->next;
      }  /* if */
    }  /* while */
    end_specifier = p_ap;
    /* Now move any type attributes from the prefix list to the specifiers
       list. */
    p_ap  = &dps->prefix_attributes;
    while (*p_ap != NULL) {
      if (is_type_transforming_attribute(*p_ap) &&
          !is_unapplicable_attr(*p_ap)) {
        /* Move the attribute to the specifier attributes list. */
        an_attribute_ptr  ap = *p_ap;
        if (is_std_attribute(ap) && !ap->is_std_gcc_attribute) {
          report_bad_attribute_target(es_error, ap);
        }  /* if */
        *p_ap = ap->next;
        ap->next = NULL;
        ap->syntactic_location = al_specifier;
        *end_to_specifier = ap;
        end_to_specifier = &ap->next;
      } else {
        if ((*p_ap)->kind == ak_enable_if) {
          dps->pending_prefix_enable_if_attr = TRUE;
        }  /* if */
        /* Proceed to the next attribute. */
        p_ap = &(*p_ap)->next;
      }  /* if */
    }  /* while */
    *p_ap = to_prefix;
    *end_specifier = to_specifier;
    if (dps->specifier_attributes != NULL) {
      if (type_is(dps->specifiers_type, tk_routine)) {
        /* A specifiers type can be a tk_routine type only if the type
           specifier was a template type parameter substituted with a routine
           type.  Do not apply attributes directly to that routine type,
           because the type might be shared.  For example (in Clang mode):
             template<typename T> struct S {
               using X = T;
               using Y = T [[nodebug]];
             };
             S<int(int)> sf;
           We should not modify the routine type int(int) directly with the
           attribute [[nodebug]] since that would, e.g., also (erroneously)
           modify the type S<int(int)>::X.  Instead, we add a typeref layer
           to which the attributes will be attached. */
        a_type_ptr  trtp = alloc_type((a_type_kind)tk_typeref);
        trtp->variant.typeref.type = dps->specifiers_type;
        trtp->variant.typeref.kind = trk_for_type_attributes;
        dps->specifiers_type = trtp;
      }  /* if */
      attach_type_attributes(&dps->specifiers_type, dps->specifier_attributes,
                             (void*)dps);
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (ms_extensions && dps->prefix_attributes != NULL) {
      /* Perform some early checking for dllimport/dllexport attributes. */
      preapply_declspec_attributes(dps);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
}  /* attach_specifier_attributes */


static void check_use_of_constexpr(a_decl_parse_state  *dps)
/*
Callback routine called at the end of processing for a declaration containing
the constexpr specifier.  Issue an error if the specifier is not applicable.
*/
{
  a_symbol_ptr  sym = dps->sym;

  if (sym == NULL || is_class_symbol(sym) || sym->kind == sk_type) {
    /* No declaration is associated with "constexpr" or "constexpr" has been
       applied to a type declaration: Issue an error. */
    pos_error(ec_invalid_constexpr, &dps->constexpr_pos);
  } else if (sym->is_error ||
             (dps->type != NULL && is_error_type(dps->type))) {
    /* An error has presumably already been reported for this declaration.
       An additional error is unlikely to be helpful. */
    expect_error();
    if (is_simple_function_symbol(sym)) {
      /* Ensure the is_constexpr flag is FALSE to avoid confusing later
         processing. */
      sym->variant.routine.ptr->is_constexpr = FALSE;
    }  /* if */
  } else if (symbol_is(sym, sk_member_function)) {
    a_routine_ptr  rp = sym->variant.routine.ptr;
    if (special_kind_is(rp, sfk_destructor) &&
        !constexpr_dynamic_alloc_enabled) {
      if (!rout_is_real_template_instance(rp)) {
        pos_error(ec_constexpr_destructor, &dps->constexpr_pos);
      }  /* if */
      rp->is_constexpr = FALSE;
    } else if (special_kind_is(rp, sfk_constructor)) {
      a_type_ptr  class_type = parent_class_of(rp);
      if (class_type->variant.class_struct_union.any_virtual_base_classes) {
        pos_error(ec_constexpr_ctor_with_virtual_base, &dps->constexpr_pos);
        rp->is_constexpr = FALSE;
      }  /* if */
    }  /* if */
  } else if (symbol_is(sym, sk_function_template)) {
    /* The restriction on constructors applies also to constructor
       templates. */
    a_routine_ptr  rp = sym->variant.template_info->variant.function.routine;
    if (special_kind_is(rp, sfk_constructor)) {
      a_type_ptr  class_type = parent_class_of(rp);
      if (class_type->variant.class_struct_union.any_virtual_base_classes) {
        pos_error(ec_constexpr_ctor_with_virtual_base, &dps->constexpr_pos);
        rp->is_constexpr = FALSE;
      }  /* if */
    }  /* if */
  } else if (symbol_is(sym, sk_variable) ||
             symbol_is(sym, sk_static_data_member) ||
             symbol_is(sym, sk_variable_template)) {
    /* Check that a constexpr variable, variable template, or static data
       member has a reference type or a literal type. */
    a_variable_ptr  vp = variable_for_symbol(sym);
    a_type_ptr      vtp = skip_array_types(vp->type);
    vtp = skip_typerefs(vtp);
    if (vp->is_handler_param) {
      pos_error(ec_invalid_constexpr, &dps->constexpr_pos);
      vp->is_constexpr = FALSE;
    } else if (vtp->incomplete) {
      /* We can get here in error situations, but we cannot test an incomplete
         class type with is_literal_type (it can trigger an internal error). */
      if (dps->is_definition) {
        expect_error();
        vp->is_constexpr = FALSE;
      }  /* if */
    } else if (!is_literal_type(vtp) &&
               !is_template_dependent_type(vtp) &&
               !is_error_type(vtp)) {
      pos_error(ec_constexpr_variable_must_have_literal_type,
                &dps->constexpr_pos);
      vp->is_constexpr = FALSE;
    }  /* if */
  } else if (symbol_is(sym, sk_routine)) {
    /* constexpr is potentially valid for non-member functions: No diagnostic
       is needed here. */
  } else {
    pos_error(ec_invalid_constexpr, &dps->constexpr_pos);
  }  /* if */
}  /* check_use_of_constexpr */


static void check_use_of_consteval(a_decl_parse_state  *dps)
/*
Callback routine called at the end of processing for a declaration containing
the consteval specifier.  Issue an error if the specifier is not applicable.
*/
{
  a_symbol_ptr   sym = dps->sym;
  a_routine_ptr  rp = NULL;

  if (sym == NULL) {
    /* No declaration is associated with "consteval": Issue an error. */
    pos_error(ec_invalid_consteval, &dps->constexpr_pos);
  } else if (sym->is_error ||
             (dps->type != NULL && is_error_type(dps->type))) {
    /* An error has presumably already been reported for this declaration.
       An additional error is unlikely to be helpful. */
    expect_error();
    if (is_simple_function_symbol(sym)) {
      /* Ensure the is_constexpr and is_consteval flags are FALSE to avoid
         confusing later processing. */
      sym->variant.routine.ptr->is_constexpr = FALSE;
      sym->variant.routine.ptr->is_consteval = FALSE;
    }  /* if */
  } else if (symbol_is(sym, sk_member_function)) {
    rp = sym->variant.routine.ptr;
    if (special_kind_is(rp, sfk_destructor)) {
      if (!rout_is_real_template_instance(rp)) {
        pos_error(ec_consteval_destructor, &dps->constexpr_pos);
      }  /* if */
      rp->is_constexpr = FALSE;
      rp->is_consteval = FALSE;
    } else if (special_kind_is(rp, sfk_constructor)) {
      a_type_ptr  class_type = parent_class_of(rp);
      if (class_type->variant.class_struct_union.any_virtual_base_classes) {
        pos_error(ec_consteval_ctor_with_virtual_base, &dps->constexpr_pos);
        rp->is_constexpr = FALSE;
        rp->is_consteval = FALSE;
      }  /* if */
    }  /* if */
  } else if (symbol_is(sym, sk_function_template)) {
    /* The restriction on constructors applies also to constructor
       templates. */
    rp = sym->variant.template_info->variant.function.routine;
    if (special_kind_is(rp, sfk_constructor)) {
      a_type_ptr  class_type = parent_class_of(rp);
      if (class_type->variant.class_struct_union.any_virtual_base_classes) {
        pos_error(ec_consteval_ctor_with_virtual_base, &dps->constexpr_pos);
        rp->is_constexpr = FALSE;
        rp->is_consteval = FALSE;
      }  /* if */
    }  /* if */
  } else if (symbol_is(sym, sk_variable) ||
             symbol_is(sym, sk_static_data_member) ||
             symbol_is(sym, sk_variable_template)) {
    /* Unlike "constexpr", "consteval" is not permitted on variable-like
       declarations. */
    pos_error(ec_consteval_variable, &dps->constexpr_pos);
  } else if (symbol_is(sym, sk_routine)) {
    /* consteval is potentially valid for non-member functions: No diagnostic
       is needed here. */
    rp = sym->variant.routine.ptr;
  } else {
    pos_error(ec_invalid_consteval, &dps->constexpr_pos);
  }  /* if */
  if (rp != NULL && rp->is_consteval) {
    /* No error was issued earlier. */
    if (special_kind_is(rp, sfk_operator) &&
        (opname_kind_is(rp, onk_new) ||
         opname_kind_is(rp, onk_array_new) ||
         opname_kind_is(rp, onk_delete) ||
         opname_kind_is(rp, onk_array_delete))) {
      /* Allocation and deallocation functions cannot be declared consteval. */
      pos_error(ec_consteval_new_or_delete_operator, &dps->constexpr_pos);
    }  /* if */
  }  /* if */
}  /* check_use_of_consteval */


static void check_use_of_constinit(a_decl_parse_state  *dps)
/*
Callback routine called at the end of processing for a declaration containing
the constinit specifier.  Issue an error if the specifier is not applicable.
Otherwise, mark the associated variable as having been declared with
"constinit".
*/
{
  a_symbol_ptr   sym = dps->sym;

  if (sym == NULL) {
    /* No declaration is associated with "constinit": Issue an error. */
    pos_error(ec_invalid_constinit, &dps->constexpr_pos);
  } else if (sym->is_error ||
             (dps->type != NULL && is_error_type(dps->type))) {
    /* An error has presumably already been reported for this declaration.
       An additional error is unlikely to be helpful. */
    expect_error();
  } else {
    a_variable_ptr  vp = variable_for_symbol(sym);
    if (vp == NULL) {
      /* Only variables and static data members can be declared "constinit". */
      pos_error(ec_invalid_constinit, &dps->constexpr_pos);
    } else if (!var_has_static_or_thread_storage_duration(vp)) {
      /* constinit can only be applied to variables with static (or "thread")
         storage duration. */
      pos_error(ec_constinit_variable_storage, &dps->constexpr_pos);
      vp->declared_constinit = FALSE;
    } else if (!vp->is_nonreal) {
      /* Unless the variable is nonreal, a constinit variable cannot have
         a dynamic initializer. */
      an_init_kind        init_kind;
      an_initializer_ptr  initializer;
      get_variable_initializer(vp, (a_scope*)NULL, &init_kind, &initializer);
      if (init_kind == (an_init_kind)initk_dynamic &&
          !dyn_init_is(initializer->dynamic, dik_constant) &&
          !dyn_init_is(initializer->dynamic, dik_zero) &&
          !dyn_init_is(initializer->dynamic, dik_none)) {
        pos_error(ec_constinit_variable_has_dynamic_init, &dps->constexpr_pos);
        vp->declared_constinit = FALSE;
      } else {
        vp->declared_constinit = TRUE;
      }  /* if  */
    }  /* if  */
  }  /* if */
}  /* check_use_of_constinit */


static void apply_c11_noreturn(a_decl_parse_state  *dps)
/*
Callback routine called at the end of processing for a declaration containing
the C11 _Noreturn specifier.  Record the _Noreturn property if needed.
*/
{
  an_element_position_ptr  *eppp = &dps->extra_positions, epp;
  a_boolean                err = FALSE;

  while ((*eppp)->kind != (an_element_position_kind)epk_noreturn) {
    eppp = &(*eppp)->next;
  }  /* if */
  epp = *eppp;
  *eppp = epp->next;
  if (dps->sym == NULL || !symbol_is(dps->sym, sk_routine)) {
    pos_error(ec_bad_c11_noreturn, &epp->position);
    err = TRUE;
  } else if (is_main_function(dps->sym->variant.routine.ptr)) {
    if (gnu_mode) {
      /* GCC (and clang) allow _Noreturn on main (with a warning). */
      pos_warning(ec_bad_c11_noreturn, &epp->position);
    } else {
      pos_error(ec_bad_c11_noreturn, &epp->position);
      err = TRUE;
    }  /* if */
  }  /* if */
  if (!err) {
    a_routine_ptr  rp = dps->sym->variant.routine.ptr;
    if (rp->type->kind == (a_type_kind)tk_routine) {
      rp->type->variant.routine.extra_info->does_not_return = TRUE;
    }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    /* If appropriate, record the _Noreturn position in the IL. */
    if (dps->is_definition) {
      prepend_element_positions(epp, &rp->source_corresp.decl_pos_info
                                        ->extra_positions);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    } else if (!source_sequence_entries_disallowed) {
      /* If a source sequence entry was recorded (which would necessarily be a
         secondary entry, since this is not a definition), record the position
         in that entry. */
      a_source_sequence_entry_ptr
                        ssep = last_matching_source_sequence_entry((char*)rp);
      if (ssep != NULL) {
        a_src_seq_secondary_decl_ptr  sssdp;
        check_assertion(ss_entry_kind(ssep) == iek_src_seq_secondary_decl);
        sssdp = ss_entry_ptr(ssep, a_src_seq_secondary_decl_ptr);
        prepend_element_positions(epp, &sssdp->decl_pos_info->extra_positions);
      }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  }  /* if */
}  /* apply_c11_noreturn */


static a_type_ptr scan_c11_atomic_type_specifier(void)
/*
Scan a type specifier of the form

	_Atomic ( type-name )

and return a representation of it (an error type in some error cases).

The caller must make sure the current token is _Atomic followed by a left
parenthesis; on return, the current token is the right parenthesis.
*/
{
  a_type_ptr  result;

  check_assertion(curr_token == tok_c11_atomic);
  (void)get_token();
  check_assertion(curr_token == tok_lparen);
  (void)get_token();
  add_stop_token(tok_rparen);
  type_name(&result);
  if (curr_token != tok_rparen) {
    syntax_error(ec_exp_rparen);
  }  /* if */
  remove_stop_token(tok_rparen);
  result = make_c11_atomic_type(result, &error_position,
                                /*prev_quals_allowed=*/FALSE);
  return result;
}  /* scan_c11_atomic_type_specifier */


static void check_explicit_specifier(a_decl_parse_state  *dps)
/*
Check that the explicit specifier was permitted on this declaration.  Issue
an error if appropriate.
*/
{
  a_symbol_ptr  sym = dps->sym;

  if (sym != NULL && symbol_is(sym, sk_function_template)) {
    sym = symbol_for(sym->variant.template_info->variant.function.routine);
  }  /* if */
  if (sym != NULL && is_simple_function_symbol(sym) &&
      !(dps->dso_flags & DSO_FRIEND) &&
      (special_kind_is(sym->variant.routine.ptr, sfk_deduction_guide) ||
       (special_kind_is(sym->variant.routine.ptr, sfk_constructor) &&
        dps->in_class_scope) ||
       (special_kind_is(sym->variant.routine.ptr, sfk_conversion) &&
        explicit_conversion_functions_enabled && dps->in_class_scope))) {
    /* A valid use of "explicit". */
  } else if (microsoft_mode && dps->in_class_scope &&
             (dps->declarator_pos.seq == 0 ||
              scope_stack_top().in_nonreal_instantiation)) {
    /* Microsoft compilers appear to ignore "explicit" if no declarator
       appeared in a class scope declaration.  We also ignore it in nonreal
       instantiations (in-class specializations during nonreal instantiations
       have no associated symbol). */
  } else if (sym != NULL && (sym->is_error || dps->sym->is_error)) {
    /* An error occurred.  An additional diagnostic is unlikely to be
       helpful. */
    expect_error();
  } else {
    pos_error(ec_explicit_not_allowed, &dps->specifiers_pos);
  }  /* if */
}  /* check_explicit_specifier */


void check_for_rescannable_alias(a_decl_parse_state  *dps)
/*
If we're parsing a member type alias (including a typedef) in a prototype
instantiation, set the flag ensuring that any expressions in the type alias
can be rescanned later on.  For example, consider:

  template<int> struct M;
  template<typename... Ts> struct S {
    using SType = M<sizeof...(Ts)>;
    S();
    template<typename...> S() noexcept(SType::X);
  };
  int main() { S<>(); }

Here, SType::X will be recorded as M<sizeof...(Ts)>::X and then substituted
(which in this case will fail since M<0> is incomplete).  Make sure the flag
is reset when the member type alias has been scanned.
*/
{
  a_scope_stack_entry_ptr  ssep = &scope_stack_top();

  if (scope_is(ssep, sck_class_struct_union) &&
      ssep->in_prototype_instantiation &&
      !ssep->in_template_deduction_context) {
    ssep->in_template_deduction_context = TRUE;
    add_end_of_parse_action(clear_template_deduction_context_flag, dps,
                            /*secondary_decls=*/TRUE);
  }  /* if */
}  /* check_for_rescannable_alias */


static void conditional_explicit_specifier(a_decl_parse_state  *dps)
/*
The caller has determined that the current and next tokens are "explicit (".
Scan the parenthesized constant-expression and record it as an internal
attribute.
*/
{
  a_scope_stack_entry_ptr  ssep = &scope_stack_top();
  a_boolean                rescannable = FALSE;
  an_attribute_ptr         ap = alloc_attribute();
  an_attribute_arg_ptr     aap = alloc_attribute_arg();
  a_constant_ptr           bool_val =
                                  fs_constant((a_constant_repr_kind)ck_error);

  /* Skip over "explicit". */
  check_assertion(curr_token == tok_explicit);
  (void)get_token();
  check_assertion(curr_token == tok_lparen);
  /* Scan a parenthesized boolean constant-expression.  The parenthesized value
     is represented as an internal attribute on the (eventual) routine type. */
  ap->kind = ak_conditional_explicit;
  ap->name = copy_string_to_region(file_scope_region_number, "explicit");
  ap->position = pos_curr_token;
  ap->arguments = aap;
  /* Skip over the left parenthesis. */
  (void)get_token();
  add_stop_token(tok_rparen);
  if (is_prototype_instantiation_context() &&
      !ssep->in_template_deduction_context) {
    /* This is a member template or a member of a class template: The
       expression may therefore need to be rescanned during deduction.
       Set a flag to ensure that rescan information is recorded. */
    ssep->in_template_deduction_context = TRUE;
    rescannable = TRUE;
  }  /* if */
  scan_bool_constant_expression(bool_val);
  if (rescannable) {
    ssep->in_template_deduction_context = FALSE;
  }  /* if */
  aap->kind = (an_attribute_arg_kind)aak_constant;
  aap->position = pos_curr_token;
  aap->variant.constant = bool_val;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  aap->end_position = curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  (void)required_token_no_advance(tok_rparen, ec_exp_rparen);
  remove_stop_token(tok_rparen);
  dps->conditional_explicit_attr = TRUE;
  ap->next = dps->prefix_attributes;
  dps->prefix_attributes = ap;
}  /* conditional_explicit_specifier */


static a_boolean potential_type_ahead_heuristic(void)
/*
The current token is "auto" in a mode where it could either be a type specifier
or a storage class specifier.  Return TRUE if the tokens ahead might include a
type name (if not, we can conclude that "auto" is a type specifier).
*/
{
  a_boolean              result = TRUE;
  a_scanning_token_cache cache;

  for (;;) {
    cache_curr_token(cache.ptr());
    (void)get_token();
    if (curr_token == tok_lparen || curr_token == tok_lbrace ||
        curr_token == tok_assign || curr_token == tok_star ||
        curr_token == tok_ampersand || curr_token == tok_and_and) {
      /* An initializer, declarator, or function parameter list is
         next.  So there are no more decl-specifiers. */
      result = FALSE;
      break;
    } else if (curr_token == tok_identifier) {
      /* Check if this identifier might name a type. */
      a_symbol  *sym = locator_for_curr_id.symbol_header->symbol;
      for (; sym != NULL; sym = sym->next) {
        if (is_type_symbol(sym) ||
            (symbol_is(sym, sk_class_template) && next_token() == tok_lt)) {
          goto done;
        } else if (symbol_is(sym, sk_namespace)) {
          /* Likely a qualified name next.  Assume it might name a type. */
          goto done;
        }  /* if */
      }  /* for */
      result = FALSE;
      break;
    } else if (curr_token != tok_const && curr_token != tok_volatile &&
               curr_token != tok_static && curr_token != tok_constexpr) {
      /* Assume this is a token specifying a type. */
      break;
    }  /* if */
  }  /* for */
done:
  rescan_cached_tokens(cache.ptr());
  return result;
}  /* potential_type_ahead_heuristic */


void decl_specifiers(a_decl_flag_set       input_flags,
                     a_decl_parse_state    *state,
                     a_decl_pos_block_ptr  decl_pos_block)
/*
Scan a list of declaration specifiers (e.g., the nonterminal grammar token
declaration-specifiers in the C standard or the similar decl-specifier-seq in
the C++ standard) according to options passed through input_flags.  The
results of the scan are summarized in *state, with some additional position
information stored in *decl_pos_block.

The concept of "declaration specifiers" includes standard constructs such as
those corresponding to the nonterminal grammar token declaration-specifiers
in the C standard or the similar decl-specifier-seq in the C++ standard, but
also various nonstandard constructs (e.g., the Microsoft __declspec(...)
mechanism).  For GNU and Microsoft extensions, we generally implement actual
behavior rather than documented behavior whenever the two differ.

Descriptions of the various input_flag options follow the definition of the
macro DSI_NO_INPUT_FLAGS.

A change in this function that affects C++ syntax almost always requires a
corresponding change in prescan_decl_specifiers (in disambig.c).
*/
{
  a_symbol_ptr               curr_token_type_symbol;
  a_boolean                  err = FALSE;
  a_boolean                  bad_combination_of_type_specifiers = FALSE;
  a_boolean                  is_parameter =
                                        (input_flags & DSI_IS_PARAMETER) != 0;
  a_boolean                  is_member_decl =
                               (input_flags & DSI_IS_MEMBER_DECLARATION) != 0;
  a_boolean                  vacuous_decl_allowed;
  a_boolean                  specifier_allows_vacuous_decl;
  a_boolean                  declares_something = FALSE;
  a_boolean                  defines_something = FALSE;
  a_boolean                  type_specifier_allowed, auto_type_allowed;
  a_boolean                  dangling_type_specifier = FALSE;
  a_boolean                  is_elaborated_type_specifier = FALSE;
  an_error_severity          es = es_none;
  a_basic_type               basic_type = bt_none;
#if GNU_EXTENSIONS_ALLOWED
  an_error_code              delayed_error = ec_no_error;
  a_source_position          pos_delayed_error;
#endif /* GNU_EXTENSIONS_ALLOWED */
  a_type_sign                sign = sign_none;
  a_type_size                size = size_none;
  a_constant_ptr             bit_precise_width_con = NULL;
  a_complex_attribute        complex_attr = cxa_none;
  a_boolean                  saturating_fixed_point = FALSE;
  a_boolean                  bad_type_name_error;
  a_decl_specifiers_set      decl_specifiers_seen;
  a_boolean                  any_decl_specifiers_seen = FALSE;
  a_boolean                  marked_as_gnu_extension =
                              (input_flags & DSI_MARKED_AS_GNU_EXTENSION) != 0;
#if UPC_EXTENSIONS_ALLOWED
  a_upc_block_size           saved_block_size = UPC_BLOCK_SIZE_INDEFINITE;
  a_boolean                  multiple_shared_seen = FALSE;
#endif /* UPC_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
  a_boolean                  context_specific_keyword_expected =
                                            state->has_cli_initonly_keyword ||
                                            state->has_cli_literal_keyword;
  a_boolean                  microsoft_w64_seen = FALSE;
  a_source_position          microsoft_w64_pos;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  a_named_address_space_id   named_address_space;
  a_type_qualifier_set       qualifiers = TQ_NONE;
  a_storage_class            *storage_class = &state->declared_storage_class;
  a_type_ptr                 *type_ptr = &state->specifiers_type;
  a_decl_flag_set            *output_flags = &state->dso_flags;
  a_boolean                  auto_is_first = FALSE;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position          id_start_pos = null_source_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
 
  db_enter(3, "decl_specifiers");
  if (marked_as_gnu_extension) {
    state->decl_modifiers.marked_as_gnu_extension = TRUE;
  }  /* if */
  decl_specifiers_seen = DS_NONE;
  type_specifier_allowed = (input_flags & DSI_TYPE_SPECIFIER_ALLOWED) != 0;
  vacuous_decl_allowed = (input_flags & DSI_VACUOUS_TAG_DECL_ALLOWED) != 0;
  auto_type_allowed = state->auto_type_allowed ||
                      state->trailing_return_type_allowed;
  set_err_pos_to_curr_token();
  copy_source_position(pos_curr_token, state->specifiers_pos);
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL && !state->is_linkage_spec_decl) {
    /* Assume the current source position is the starting position of the
       decl-specifiers.   If it turns out there are no decl-specifiers, the
       field will be reset to null_source_position.  If this declaration has
       an attached linkage specifier (like extern "C"), the position was
       already set to that of the initial keyword "extern".  If prefix
       attributes preceded the decl-specifiers, record their position (which
       is the same as state->start_pos) as the actual starting position of the
       specifiers. */
    if (state->prefix_attributes != NULL) {
      decl_pos_block->specifiers_range.start = state->start_pos;
    } else {
      decl_pos_block->specifiers_range.start = state->specifiers_pos;
    }  /* if */
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  /* Helper macro to record the position of the first qualifier (other than
     "restrict", which has its own position record). */
#define record_qualifiers_pos()                                              \
  if (state->qualifiers_pos.seq == 0) state->qualifiers_pos = pos_curr_token
  /* Loop for each declaration specifier. */
  for (;;) {
    /* Some specifiers are discarded.  This flag indicates whether the
       current specifier should count toward any specifiers being seen. */
    a_boolean  count_as_specifier_seen = TRUE;
    /* Most specifiers cannot be part of a vacuous class or enum declaration,
       so we start with that assumption.  The flag will be set to TRUE in the
       exceptional cases. */
    specifier_allows_vacuous_decl = FALSE;
    switch (curr_token) {
      case tok_auto:
        if (state->auto_type_specifier_seen ||
            state->decltype_auto_specifier_seen) {
          pos_error(auto_type_allowed ? ec_bad_combination_of_type_specifiers :
                                        ec_mult_storage_classes,
                                        &error_position);
        } else if (is_parameter && auto_type_allowed &&
                   process_auto_parameter(state, /*concept_sym=*/NULL)) {
          /* "auto" as a parameter type specifier in what is presumably a
             (generic) lambda parameter or a parameter of a C++20 abbreviated
             function template.  state->specifiers_type points to the
             corresponding type entry. */
          state->auto_pos = pos_curr_token;
          state->has_deduced_type = TRUE;
          state->auto_type_specifier_seen = TRUE;
          basic_type = bt_typedef;
          decl_specifiers_seen |= DS_TYPE;
        } else {
          state->auto_pos = pos_curr_token;
          state->has_deduced_type = TRUE;
          state->auto_type_specifier_seen = TRUE;
          /* Remember whether "auto" was the first specifier (ignoring inline
             and friend). */
          auto_is_first = !(decl_specifiers_seen & ~(DS_INLINE | DS_FRIEND));
          if (auto_storage_class_specifier_enabled &&
              (auto_type_specifier_enabled ||
               clangcpp_version_is(any_version)) &&
              ((decl_specifiers_seen & DS_TYPE) != 0 ||
               potential_type_ahead_heuristic())) {
            /* Whether "auto" is a type specifier or a storage class specifier
               cannot be decided until all decl-specifiers have been seen. */
          } else {
            /* Process the auto specifier now.  If "auto" can only be a type
               specifier, this cannot be delayed until later, because in
                 typedef int T; T x;
                 void f() { auto T(x); }
               we must know that a type specifier ("auto") was seen to avoid
               treating T as a type specifier (here, it is a declarator-id). */
            process_auto_specifier(
                    auto_type_allowed, auto_is_first, input_flags, state,
                    decl_pos_block, &decl_specifiers_seen, &basic_type,
                    type_ptr, &err);
          }  /* if */
        }  /* if */
        break;
      case tok_auto_type:
        /* GCC 4.9 and later accept "__auto_type x = 3;" in C mode.  It
           determines the type of a variable declaration from its initializer
           (much like the C++11 "auto" type specifier, but with more
           restrictions). */
        if (!type_specifier_allowed) {
          pos_error(ec_type_specifier_not_allowed, &error_position);
          err = TRUE;
        } else if (basic_type != bt_none) {
          bad_combination_of_type_specifiers = TRUE;
          pos_error(ec_bad_combination_of_type_specifiers, &error_position);
        } else {
          basic_type = bt_auto;
          decl_specifiers_seen |= DS_TYPE;
          state->auto_pos = pos_curr_token;
          state->has_deduced_type = TRUE;
          state->auto_type_specifier_seen = TRUE;
          state->gnu_auto_type_specifier_seen = TRUE;
          /* Allocate a separate tk_unknown entry, so it can be changed to
             another type (e.g., an error type) later on. */
          state->auto_type = alloc_type((a_type_kind)tk_unknown);
          state->specifiers_type = state->auto_type;
          add_end_of_parse_action(check_c_auto_type, state,
                                  /*secondary_decls=*/TRUE);
        }  /* if */
        break;
      case tok_typedef:
        specifier_allows_vacuous_decl = !strict_ansi_mode;
        /* Reset the local indication of seeing a GNU "__extension__" keyword
           (so it won't apply to the type underlying the typedef). */
        marked_as_gnu_extension = FALSE;
        check_for_rescannable_alias(state);
        goto storage_class_specifier;
      case tok_ellipsis:
        if (variadic_templates_enabled &&
            (decl_specifiers_seen & (DS_TYPE | DS_VOID)) == 0) {
          /* A "..." before a type specifier, so this can't be part of the
             declarator. */
          pos_error(ec_parameter_pack_decl_not_allowed, &pos_curr_token);
          err = TRUE;
        } else {
          goto something_unexpected;
        }  /* if */
        break;
      case tok_extern:
        if (!C_mode() && next_token() == tok_string_literal) {
          /* This is a C++ linkage specification, which is an error in this
             context. */
          pos_error(ec_linkage_specifier_not_allowed, &error_position);
          err = TRUE;
          /* Consume "extern".  We don't bother validating the string since an
             error has already been issued. */
          (void)get_token();
          break;
        }  /* if */
        /* Otherwise drop through for normal storage class processing. */
        FALLTHROUGH
      case tok_static:
      case tok_register:
      case tok_mutable:
      case tok_thread_local:
      case tok_c11_thread_local:
        /* A storage class specifier (3.5.1). */
storage_class_specifier:
        process_storage_class_specifier(
                            curr_token, input_flags, state, decl_pos_block, 
                            !(decl_specifiers_seen &
                              ~(DS_INLINE | DS_FRIEND | DS_THREAD_LOCAL |
                                DS_CONSTEXPR)),
                            &decl_specifiers_seen, &err);
        goto no_get_token;
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
      case tok_thread:
        /* A "__thread" storage specifier allowed in certain modes
           (can be combined with "extern" or "static").  Note that this does
           not process "thread_local/_Thread_local". */
        scan_thread_local_storage_specifier(input_flags,
                                            &state->decl_modifiers);
        break;
#endif /* THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */
#if ASM_FUNCTION_ALLOWED
      case tok_asm:
        /* Specifier indicating an asm function declaration.  It is
           inconsistent with an explicit storage class declaration or
           "inline". */
        if (*storage_class == (a_storage_class)sc_asm) {
          pos_error(ec_dupl_decl_specifier, &error_position);
          err = TRUE;
        } else if (!(input_flags & DSI_ASM_ALLOWED) ||
                   (decl_specifiers_seen & (DS_INLINE | DS_STORAGE_CLASS))) {
          /* asm is not allowed if we've already seen a storage class or
             inline. */
          pos_error(ec_asm_not_allowed, &error_position);
          err = TRUE;
        } else {
          /* The asm specifier is represented as a storage class (even though
             strictly speaking it's more like "inline" as an attribute of a
             routine declaration). */
          *storage_class = (a_storage_class)sc_asm;
          decl_specifiers_seen |= DS_STORAGE_CLASS;
        }  /* if */
        break;
#endif /* ASM_FUNCTION_ALLOWED */
#if SUN_EXTENSIONS_ALLOWED
      case tok_global_link_scope:
      case tok_symbolic_link_scope:
      case tok_hidden_link_scope:
        /* A Sun-specific storage class allowed only on function and variable
           declarations with external linkage. */
        scan_link_scope_specifier(input_flags, &state->decl_modifiers);
        break;
#endif /* SUN_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
      case tok_microsoft_inline:
      case tok_forceinline:
        /* Microsoft-specific specifiers: __inline and __forceinline. */
        scan_microsoft_inline_specifiers(input_flags, output_flags,
                                         &decl_specifiers_seen,
                                         &state->decl_modifiers, &err);
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case tok_lsplice:
        if (state->is_implicit_type_context) {
          if (spliced_name_qualifier_next()) {
            /* The splice is a name qualifier. */
            goto general_identifier_case;
          } else {
            *type_ptr = scan_type_splicer((a_rescan_control_block *)NULL);
            basic_type = bt_typedef;
            decl_specifiers_seen |= DS_TYPE;
            goto no_get_token;
          }  /* if */
        } else {
          goto something_unexpected;
        }  /* if */
      case tok_lbracket:
        /* A Microsoft or C++11 attribute (presumably). */
        { a_token_kind  next_tok = next_token();
          if (next_tok == tok_lbracket) {
            /* A C++11 standard attribute. */
            if (!std_attributes_enabled) goto something_unexpected;
          } else if (!ms_extensions ||
                     (microsoft_version < 1700 ?
                              any_decl_specifiers_seen :
                              (decl_specifiers_seen & (DS_VOID | DS_TYPE))) || 
                     (C_mode() && microsoft_version < 1400) ||
                     (input_flags & DSI_MICROSOFT_ATTRIBUTES_ALLOWED) == 0) {
            /* Microsoft attributes are only recognized in Microsoft C++ mode
               and, when microsoft_version is at least 1400, in Microsoft C
               mode.  When microsoft_version < 1700 they must precede any
               specifiers; otherwise, they must precede type specifiers. */
            goto something_unexpected;
#if MICROSOFT_EXTENSIONS_ALLOWED
          } else {
            /* Microsoft attributes are valid here.  Append them to any
               attributes that we might have seen before. */
            scan_and_append_microsoft_attributes(
                 &state->ms_attributes, (input_flags & DSI_IS_PARAMETER) != 0);
            goto no_get_token;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          }  /* if */
        }
        FALLTHROUGH
      case tok_alignas:
#if GNU_EXTENSIONS_ALLOWED
      case tok_attribute:
#endif /* GNU_EXTENSIONS_ALLOWED */
#if MICROSOFT_EXTENSIONS_ALLOWED
      case tok_declspec:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        if (input_flags & DSI_MICROSOFT_SECONDARY_SPECIFIERS) {
          /* These specifiers are ignored by the caller when scanning
             secondary specifiers; e.g., "int i, __declspec(thread) j;" */
          pos_warning(ec_secondary_specifier_ignored, &pos_curr_token);
          skip_over_attributes();
        } else {
          a_boolean         std_attr_seen;
          scan_specifier_attributes(input_flags, state, &std_attr_seen);
#if EXTRA_SOURCE_POSITIONS_IN_IL
          if (decl_pos_block != NULL) {
            decl_pos_block->specifiers_range.end = curr_construct_end_position;
          }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
          if (std_attr_seen) {
            if ((decl_specifiers_seen & (DS_TYPE | DS_VOID)) == 0) {
              /* If no type specifier has been seen yet, ignore the attributes
                 with an error. */
              pos_error(ec_unattached_attribute,
                        &state->specifier_attributes->group->position);
              state->specifier_attributes = NULL;
            } else {
              /* Standard attributes must be the last item in a decl-specifier
                 sequence.  (We do accept other forms of attributes after them,
                 but no other specifier kinds.) */
              goto something_unexpected;
            }  /* if */
          } else {
            specifier_allows_vacuous_decl = TRUE;
          }  /* if */
        }
        goto no_get_token;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case tok_microsoft_w64:
        /* __w64 (or _w64).  Syntactically, this is like a cv-qualifier
           except that it does not really modify the type (and Microsoft
           allows the keyword to be repeated). */
        microsoft_w64_seen = TRUE;
        microsoft_w64_pos = pos_curr_token;
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case tok_const:
        /* const type qualifier (3.5.3). */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (input_flags & DSI_MICROSOFT_SECONDARY_SPECIFIERS) {
          /* E.g., "int i, double const j;". */
          pos_warning(ec_type_qualifier_ignored, &error_position);
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        if (qualifiers & TQ_CONST) {
          /* In most modes, const may appear more than once (this is a
             standard C99 feature, and an extension in other dialects) and we
             just issue a warning in those cases.  In strict non-C99 modes, a
             discretionary error is issued instead. */
          if (c99_mode || !strict_ansi_mode) {
            es = es_warning;
          } else {
            es = strict_ansi_discretionary_severity;
          }  /* if */
          diagnostic(es, ec_dupl_type_qualifier);
        } else {
          record_qualifiers_pos();
          qualifiers |= TQ_CONST;
          decl_specifiers_seen |= DS_TYPE_QUALIFIER;
        }  /* if */
        break;
      case tok_volatile:
        /* volatile type qualifier (3.5.3). */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (input_flags & DSI_MICROSOFT_SECONDARY_SPECIFIERS) {
          /* E.g., "int i, double volatile j;". */
          pos_warning(ec_type_qualifier_ignored, &error_position);
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        if (qualifiers & TQ_VOLATILE) {
          /* In most modes, volatile may appear more than once (this is a
             standard C99 feature, and an extension in other dialects) and we
             just issue a warning in those cases.  In strict non-C99 modes, a
             discretionary error is issued instead. */
          if (c99_mode || !strict_ansi_mode) {
            es = es_warning;
          } else {
            es = strict_ansi_discretionary_severity;
          }  /* if */
          diagnostic(es, ec_dupl_type_qualifier);
        } else {
          record_qualifiers_pos();
          qualifiers |= TQ_VOLATILE;
          decl_specifiers_seen |= DS_TYPE_QUALIFIER;
        }  /* if */
        break;
      case tok_c11_atomic:
        if (next_token() == tok_lparen) {
          /* A type specifier of the form "_Atomic ( type-name )". */
          *type_ptr = scan_c11_atomic_type_specifier();
          if (basic_type != bt_none) {
            bad_combination_of_type_specifiers = TRUE;
            pos_error(ec_bad_combination_of_type_specifiers, &error_position);
          }  /* if */
          basic_type = bt_typedef;
          decl_specifiers_seen |= DS_TYPE;
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else if (input_flags & DSI_MICROSOFT_SECONDARY_SPECIFIERS) {
          /* E.g., "int i, double _Atomic j;". */
          pos_warning(ec_type_qualifier_ignored, &error_position);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        } else if (qualifiers & TQ_C11_ATOMIC) {
          pos_warning(ec_dupl_type_qualifier, &error_position);
        } else {
          /* A type qualifier. */
          record_qualifiers_pos();
          qualifiers |= TQ_C11_ATOMIC;
          decl_specifiers_seen |= DS_TYPE_QUALIFIER;
        }  /* if */
        break;
      case tok_nullable:
        /* Clang _Nullable qualifier. */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (input_flags & DSI_MICROSOFT_SECONDARY_SPECIFIERS) {
          /* E.g., "int i, double _Nullable j;". */
          pos_warning(ec_type_qualifier_ignored, &error_position);
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        if (qualifiers & TQ_NULLABLE) {
          pos_warning(ec_dupl_type_qualifier, &pos_curr_token);
        } else if (qualifiers & TQ_NULLABILITY) {
          pos_error(ec_conflicting_nullability, &pos_curr_token);
        } else {
          record_qualifiers_pos();
          qualifiers |= TQ_NULLABLE;
          decl_specifiers_seen |= DS_TYPE_QUALIFIER;
        }  /* if */
        break;
      case tok_nonnull:
        /* Clang _Nonnull qualifier. */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (input_flags & DSI_MICROSOFT_SECONDARY_SPECIFIERS) {
          /* E.g., "int i, double _Nonnull j;". */
          pos_warning(ec_type_qualifier_ignored, &error_position);
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        if (qualifiers & TQ_NONNULL) {
          pos_warning(ec_dupl_type_qualifier, &pos_curr_token);
        } else if (qualifiers & TQ_NULLABILITY) {
          pos_error(ec_conflicting_nullability, &pos_curr_token);
        } else {
          record_qualifiers_pos();
          qualifiers |= TQ_NONNULL;
          decl_specifiers_seen |= DS_TYPE_QUALIFIER;
        }  /* if */
        break;
      case tok_null_unspecified:
        /* Clang _Null_unspecified qualifier. */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (input_flags & DSI_MICROSOFT_SECONDARY_SPECIFIERS) {
          /* E.g., "int i, double _Null_unspecified j;". */
          pos_warning(ec_type_qualifier_ignored, &error_position);
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        if (qualifiers & TQ_NULL_UNSPECIFIED) {
          pos_warning(ec_dupl_type_qualifier, &pos_curr_token);
        } else if (qualifiers & TQ_NULLABILITY) {
          pos_error(ec_conflicting_nullability, &pos_curr_token);
        } else {
          record_qualifiers_pos();
          qualifiers |= TQ_NULL_UNSPECIFIED;
          decl_specifiers_seen |= DS_TYPE_QUALIFIER;
        }  /* if */
        break;
#if UPC_EXTENSIONS_ALLOWED
      case tok_upc_strict:
        /* UPC strict type qualifier. */
        check_assertion(C_mode() && upc_mode);
        if (qualifiers & (TQ_UPC_STRICT | TQ_UPC_RELAXED)) {
          /* Duplicate qualifiers are allowed in C99 mode (with a warning). */
          es = c99_mode ? es_warning : es_error;
          if (qualifiers & TQ_UPC_RELAXED) {
            /* It is an error to have both strict and relaxed. */
            es = es_error;
          }  /* if */
          diagnostic(es, ec_dupl_type_qualifier);
          if (es == es_error) err = TRUE;
        } else {
          record_qualifiers_pos();
          qualifiers |= TQ_UPC_STRICT;
          decl_specifiers_seen |= DS_TYPE_QUALIFIER;
        }  /* if */
        break;
      case tok_upc_relaxed:
        /* UPC relaxed type qualifier. */
        check_assertion(C_mode() && upc_mode);
        if (qualifiers & (TQ_UPC_STRICT | TQ_UPC_RELAXED)) {
          /* Duplicate qualifiers are allowed in C99 mode (with a warning). */
          es = c99_mode ? es_warning : es_error;
          if (qualifiers & TQ_UPC_STRICT) {
            /* It is an error to have both strict and relaxed. */
            es = es_error;
          }  /* if */
          diagnostic(es, ec_dupl_type_qualifier);
          if (es == es_error) err = TRUE;
        } else {
          record_qualifiers_pos();
          qualifiers |= TQ_UPC_RELAXED;
          decl_specifiers_seen |= DS_TYPE_QUALIFIER;
        }  /* if */
        break;
      case tok_upc_shared:
        /* UPC shared type qualifier. */
        if (qualifiers & TQ_UPC_SHARED) {
          /* Duplicate qualifiers are allowed in C99 mode (with a warning). */
          es = c99_mode ? es_warning : es_error;
          /* Save information to compare the block sizes.  If the block
             sizes do not match, the type will be rejected. */
          multiple_shared_seen = TRUE;
          saved_block_size = state->upc_block_size;
        } else {
          record_qualifiers_pos();
          qualifiers |= TQ_UPC_SHARED;
          decl_specifiers_seen |= DS_TYPE_QUALIFIER;
        }  /* if */
        /* Go past "shared" to see if a block size is specified. */
        scan_upc_block_size_if_any(state, basic_type, decl_pos_block, &err);
        if (multiple_shared_seen) {
          /* We've seen multiple UPC shared qualifiers.  Sometimes this
             is accepted with a warning, but if the block sizes are
             different, an error must be issued (no matter what mode). */
          if (es == es_warning && state->upc_block_size != saved_block_size) {
            pos_error(ec_mismatched_shared_block_size, &error_position);
            err = TRUE;
          } else {
            diagnostic(es, ec_dupl_type_qualifier);
            if (es == es_error) err = TRUE;
          }  /* if */
        }  /* if */
        goto no_get_token;
#endif /* UPC_EXTENSIONS_ALLOWED */
      case tok_gnu_restrict:
      case tok_restrict:
        /* restrict type qualifier. */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (input_flags & DSI_MICROSOFT_SECONDARY_SPECIFIERS) {
          /* E.g., "int i, Ptr restrict j;". */
          pos_warning(ec_type_qualifier_ignored, &error_position);
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        if (qualifiers & TQ_RESTRICT) {
          /* In most modes, restrict may appear more than once (this is a
             standard C99 feature, and an extension in other dialects) and we
             just issue a warning in those cases.  In strict non-C99 modes, a
             discretionary error is issued instead. */
          if (c99_mode || !strict_ansi_mode) {
            es = es_warning;
          } else {
            es = strict_ansi_discretionary_severity;
          }  /* if */
          diagnostic(es, ec_dupl_type_qualifier);
        } else {
          qualifiers |= TQ_RESTRICT;
          state->restrict_pos = pos_curr_token;
          decl_specifiers_seen |= DS_TYPE_QUALIFIER;
          if (gnu_mode && report_gnu_extensions) {
            if (curr_token == tok_gnu_restrict) {
              report_gnu_extension_if_needed(&pos_curr_token,
                                             ec_gnu_restrict_is_nonstandard);
            } else if (!c99_mode && curr_token == tok_restrict) {
              /* GNU compilers don't accept the "restrict" form in non-C99
                 modes. */
              unexpected_condition();
            }  /* if */
          }  /* if */
        }  /* if */
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case tok_unaligned:
        /* Microsoft __unaligned type qualifier. */
        if (input_flags & DSI_MICROSOFT_SECONDARY_SPECIFIERS) {
          /* E.g., "int i, double __unaligned *j;". */
          pos_warning(ec_type_qualifier_ignored, &error_position);
        } else if (qualifiers & TQ_UNALIGNED) {
          /* __unaligned may not appear more than once. */
          pos_warning(ec_dupl_type_qualifier, &error_position);
        } else {
          record_qualifiers_pos();
          qualifiers |= TQ_UNALIGNED;
          decl_specifiers_seen |= DS_TYPE_QUALIFIER;
        }  /* if */
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if NEAR_AND_FAR_ALLOWED
      case tok_near:
        /* "near" memory attribute, usually treated as a type qualifier. */
        /* This qualifier applies only on pointer declarators and not in
           normal type specifiers. */
        if ((input_flags & DSI_COLLECT_DECLARATOR_TYPE_QUALIFIERS) == 0) {
          goto something_unexpected;
        }  /* if */
        if (qualifiers & TQ_NEAR) {
          /* near may not appear more than once. */
          pos_warning(ec_dupl_mem_attrib, &error_position);
        } else if (qualifiers & TQ_FAR) {
          /* near and far are incompatible. */
          pos_error(ec_mem_attrib_incompatible, &error_position);
        } else {
          record_qualifiers_pos();
          qualifiers |= TQ_NEAR;
          decl_specifiers_seen |= DS_TYPE_QUALIFIER;
        }  /* if */
        break;
      case tok_far:
        /* "far" memory attribute, usually treated as a type qualifier. */
        /* This qualifier applies only on pointer declarators and not in
           normal type specifiers. */
        if ((input_flags & DSI_COLLECT_DECLARATOR_TYPE_QUALIFIERS) == 0) {
          goto something_unexpected;
        }  /* if */
        if (qualifiers & TQ_FAR) {
          /* far may not appear more than once. */
          pos_warning(ec_dupl_mem_attrib, &error_position);
        } else if (qualifiers & TQ_NEAR) {
          /* near and far are incompatible. */
          pos_error(ec_mem_attrib_incompatible, &error_position);
        } else {
          record_qualifiers_pos();
          qualifiers |= TQ_FAR;
          decl_specifiers_seen |= DS_TYPE_QUALIFIER;
        }  /* if */
        break;
#endif /* NEAR_AND_FAR_ALLOWED */
      case tok_friend:
	/* "friend" specifier is allowed only in a C++ class declaration.
	   This also excludes its appearing in a function parameter
	   specification. */
	if (is_parameter) {
	  /* "friend" may not appear in a function parameter specification. */
	  pos_error(ec_bad_param_specifier, &error_position);
	  err = TRUE;
	} else if (!is_member_decl) {
	  /* In fact, it may only appear in a C++ class (or struct or union)
	     declaration. */
	  pos_error(ec_bad_specifier_outside_class_decl, &error_position);
	  err = TRUE;
	} else if (decl_specifiers_seen & DS_FRIEND) {
	  /* Only one "friend" specifier at at time. */
	  pos_error(ec_dupl_decl_specifier, &error_position);
	  err = TRUE;
	} else {
          decl_specifiers_seen |= DS_FRIEND;
          *output_flags |= DSO_FRIEND;
          if (decl_specifiers_seen != DS_FRIEND) {
            if ((decl_specifiers_seen & DS_STORAGE_CLASS) &&
                !microsoft_mode &&
                !(sun_mode && *storage_class == (a_storage_class)sc_static)) {
              pos_error(ec_storage_class_in_friend_decl, &error_position);
              err = TRUE;
              *storage_class = (a_storage_class)sc_unspecified;
              decl_specifiers_seen &= ~(DS_STORAGE_CLASS);
            } else if (decl_specifiers_seen & DS_MUTABLE) {
              pos_error(ec_mutable_not_allowed, &state->storage_class_pos);
              err = TRUE;
              decl_specifiers_seen &= ~(DS_MUTABLE);
              *output_flags &= ~DSO_MUTABLE;
            }  /* if */
          } else if (any_cfront_mode()) {
            /* Check for a special case -- a friend declaration of the form
               "friend T;" which is taken to mean the same as "friend class T;"
               by cfront.  This only handles the case where T has not yet been
               declared.  If T is already declared, the construct is accepted
               in other nonstrict modes through the processing in
               check_missing_declarator_in_member_declaration. */
            if (unelaborated_cfront_friend_class()) {
              vacuous_decl_allowed = FALSE;
              goto process_class_specifier;
            }  /* if */
          }  /* if */
	}  /* if */
	break;
      case tok_virtual:
        if (is_parameter) {
          /* "virtual" may not appear in a function parameter specification. */
          pos_error(ec_bad_param_specifier, &pos_curr_token);
          err = TRUE;
        } else if ((decl_specifiers_seen & DS_FRIEND) && !microsoft_mode) {
          pos_error(ec_virtual_not_allowed, &pos_curr_token);
          err = TRUE;
        } else if (!is_member_decl &&
                   !(microsoft_mode &&
                     (input_flags & DSI_IS_SPECIALIZATION) != 0)) {
          /* In fact, it may only appear in a C++ class (or struct or union)
             declaration.  However, Microsoft compilers allow this on explicit
             specializations of class template member functions. */
          pos_error(ec_bad_specifier_outside_class_decl, &pos_curr_token);
          err = TRUE;
        } else if ((input_flags & DSI_IS_TEMPLATE_DECLARATION) &&
                   !state->is_generic_declaration) {
          /* Must be a member function template -- virtual is not allowed.
             (Virtual C++/CLI member generics are okay.) */
          pos_error(ec_virtual_function_template, &pos_curr_token);
          err = TRUE;
        } else if (decl_specifiers_seen & DS_VIRTUAL) {
          /* Only one "virtual" specifier at a time. */
          pos_diagnostic(microsoft_mode ? es_warning : es_error,
                         ec_dupl_decl_specifier, &pos_curr_token);
          if (!microsoft_mode) {
            err = TRUE;
          }  /* if */
        } else {
          decl_specifiers_seen |= DS_VIRTUAL;
          *output_flags |= DSO_VIRTUAL;
          copy_source_position(pos_curr_token, state->virtual_pos);
          if ((decl_specifiers_seen & DS_CONSTEXPR) &&
              !constexpr_virtual_enabled) {
            pos_error((*output_flags & DSO_CONSTEXPR) ?
                                            ec_constexpr_virtual_combination :
                                            ec_consteval_virtual_combination,
                      &pos_curr_token);
            *output_flags &= ~(a_decl_flag_set)DSO_CONSTEXPR;
          }  /* if */
        }  /* if */
        break;
      case tok_this:
        if (!explicit_this_param_enabled && !gpp_version_is(>= 140000)) {
          /* If explicit "this" parameters are not enabled, this is not a
             decl-specifier.  GCC 14 (and later) accepts "this" parameters
             (with a warning) in all C++ modes. */
          goto something_unexpected;
        } else if (!is_parameter) {
          /* The decl-specifier "this" may only appear in a function parameter
             declaration. */
          pos_error(ec_bad_this, &error_position);
          err = TRUE;
        } else if (decl_specifiers_seen & DS_THIS) {
          pos_warning(ec_dupl_decl_specifier, &error_position);
        } else {
          a_routine_type_supplement_ptr
                          rtsp = rout_type_supp(scope_stack_top().assoc_type);
          /* "this" may only appear on the first parameter. */
          if (rtsp->param_type_list != nullptr) {
            pos_error(ec_explicit_this_param_must_be_first, &error_position);
            err = TRUE;
          } else {
            if (!explicit_this_param_enabled) {
              pos_warning(ec_explicit_this_is_cpp23, &error_position);
            }  /* if */
            decl_specifiers_seen |= DS_THIS;
            state->is_explicit_this = TRUE;
          }  /* if */
        }  /* if */
        break;
      case tok_constexpr:
        if (is_parameter) {
          /* "constexpr" may not appear in a function parameter declaration. */
          pos_error(ec_bad_param_specifier, &error_position);
          err = TRUE;
        } else if ((decl_specifiers_seen & DS_VIRTUAL) &&
                   !constexpr_virtual_enabled) {
          pos_error(ec_constexpr_virtual_combination, &pos_curr_token);
        } else if ((input_flags & DSI_IS_EXPLICIT_INSTANTIATION) != 0) {
          pos_error(ec_constexpr_explicit_instantiation, &pos_curr_token);
        } else if (decl_specifiers_seen & DS_CONSTEXPR) {
          pos_error((*output_flags & DSO_CONSTEXPR) ?
                                        ec_dupl_decl_specifier :
                                        ec_constexpr_and_consteval_specifiers,
                    &error_position);
        } else {
          decl_specifiers_seen |= DS_CONSTEXPR;
          *output_flags |= DSO_CONSTEXPR;
          state->constexpr_pos = pos_curr_token;
          add_end_of_parse_action(check_use_of_constexpr, state,
                                  /*secondary_decls=*/TRUE);
        }  /* if */
        break;
      case tok_consteval:
        if (is_parameter) {
          /* "consteval" may not appear in a function parameter declaration. */
          pos_error(ec_bad_param_specifier, &error_position);
          err = TRUE;
        } else if ((decl_specifiers_seen & DS_VIRTUAL) &&
                   !constexpr_virtual_enabled) {
          pos_error(ec_consteval_virtual_combination, &pos_curr_token);
        } else if ((input_flags & DSI_IS_EXPLICIT_INSTANTIATION) != 0) {
          pos_error(ec_consteval_explicit_instantiation, &pos_curr_token);
        } else if (decl_specifiers_seen & DS_CONSTEXPR) {
          pos_error((*output_flags & DSO_CONSTEVAL) ?
                                        ec_dupl_decl_specifier :
                                        ec_constexpr_and_consteval_specifiers,
                    &error_position);
        } else {
          decl_specifiers_seen |= DS_CONSTEXPR;
          *output_flags |= DSO_CONSTEVAL;
          state->constexpr_pos = pos_curr_token;
          add_end_of_parse_action(check_use_of_consteval, state,
                                  /*secondary_decls=*/TRUE);
        }  /* if */
        break;
      case tok_constinit:
        if (is_parameter) {
          /* "constinit" may not appear in a function parameter declaration. */
          pos_error(ec_bad_param_specifier, &error_position);
          err = TRUE;
        } else if (decl_specifiers_seen & DS_CONSTEXPR) {
          pos_error((*output_flags & DSO_CONSTINIT) ?
                                        ec_dupl_decl_specifier :
                                        ec_constexpr_and_consteval_specifiers,
                    &error_position);
        } else {
          decl_specifiers_seen |= DS_CONSTEXPR;
          *output_flags |= DSO_CONSTINIT;
          state->constexpr_pos = pos_curr_token;
          add_end_of_parse_action(check_use_of_constinit, state,
                                  /*secondary_decls=*/TRUE);
        }  /* if */
        break;
      case tok_noreturn:
        check_for_c23_deprecation("_Noreturn", ec_c23_noreturn_deprecated);
        if (decl_specifiers_seen & DS_NORETURN) {
          pos_warning(ec_dupl_decl_specifier, &pos_curr_token);
        } else if (is_parameter) {
          /* "_Noreturn" may not appear in a function parameter declaration. */
          pos_error(ec_bad_param_specifier, &error_position);
          err = TRUE;
        } else {
          add_element_position(epk_noreturn, &pos_curr_token,
                                             &state->extra_positions);
          add_end_of_parse_action(apply_c11_noreturn, state,
                                  /*secondary_decls=*/TRUE);
        }  /* if */
        decl_specifiers_seen |= DS_NORETURN;
        break;
      case tok_inline:
        if (is_parameter) {
          /* "inline" may not appear in a function parameter specification. */
          pos_error(ec_bad_param_specifier, &error_position);
          err = TRUE;
        } else if (!(input_flags & DSI_INLINE_ALLOWED) ||
                   (C_dialect == C_dialect_cplusplus &&
                    !extern_inline_allowed &&
                    *storage_class == (a_storage_class)sc_extern)) {
          /* "inline" allowed on certain function declarations only. */
          diagnostic(gnu_mode ? es_warning : es_error, ec_inline_not_allowed);
          err = !gnu_mode;
        } else if (decl_specifiers_seen & DS_INLINE) {
          /* Only one "inline" specifier at a time.  C99 and Microsoft C++
             allow multiple "inline" specifiers, but that is unlikely the
             intent of a programmer. */
          diagnostic((c99_mode || ms_extensions) ? es_warning : es_error,
                     ec_dupl_decl_specifier);
          if (!(c99_mode || ms_extensions)) {
            err = TRUE;
          }  /* if */
        } else if (input_flags & DSI_COLLECT_DECLARATOR_TYPE_QUALIFIERS) {
          /* The keyword "inline" was seen as a qualifier.  This is only
             possible in Microsoft mode and that qualifier is ignored. */
          check_assertion(ms_extensions);
          pos_warning(ec_inline_qualifier_ignored, &error_position);
        } else {
          decl_specifiers_seen |= DS_INLINE;
          *output_flags |= DSO_INLINE;
          copy_source_position(pos_curr_token, state->inline_pos);
        }  /* if */
        break;
      case tok_explicit:
        auto_type_allowed = FALSE;
        if (is_parameter) {
          /* "explicit" may not appear in a function parameter
              specification. */
          pos_error(ec_bad_param_specifier, &error_position);
          err = TRUE;
        } else if ((decl_specifiers_seen & DS_EXPLICIT) != 0 ||
                   state->conditional_explicit_attr) {
          /* Disallow duplicates. */
          pos_error(ec_dupl_decl_specifier, &error_position);
          err = TRUE;
        } else {
          a_boolean explicit_bool_enabled = conditional_explicit_enabled ||
                                            gpp_version_is(>=90000) ||
                                            clang_version_is(>=100000);
          if (explicit_bool_enabled && next_token() == tok_lparen) {
            if (!conditional_explicit_enabled) {
              /* Warn on use of C++20 explicit(bool) feature in GNU and clang
                 modes when C++20 is not enabled. */
              pos_warning(ec_nonstandard_explicit_bool, &pos_curr_token);
            }  /* if */
            conditional_explicit_specifier(state);
          } else {
            decl_specifiers_seen |= DS_EXPLICIT;
            *output_flags |= DSO_EXPLICIT;
          }  /* if */
          /* An "explicit" specifier is only permitted on some kinds of
             declarations (constructors, conversion functions, and deduction
             guides).  Record an end-of-parse action to check this later. */
          add_end_of_parse_action(check_explicit_specifier, state,
                                  /*secondary_decls=*/FALSE);
        }  /* if */
        break;
      case tok_void:
        if (C_dialect == C_dialect_pcc) {
          /* To allow "typedef <something> void;" to be ignored in pcc mode,
             stop scanning on "void" when a basic type has already been scanned
             in a typedef. */
          if (basic_type != bt_none &&
              *storage_class == (a_storage_class)sc_typedef) {
            goto exit_loop;
          }  /* if */
        }  /* if */
        FALLTHROUGH
      case tok_char:
      case tok_wchar_t:
      case tok_char8_t:
      case tok_char16_t:
      case tok_char32_t:
      case tok_c99_bool:
      case tok_bool:
      case tok_int:
      case tok_bit_precise_int:
      case tok_float:
      case tok_double:
#if FIXED_POINT_ALLOWED
      case tok_fract:
      case tok_accum:
#endif /* FIXED_POINT_ALLOWED */
      case tok_float32:
      case tok_float32x:
      case tok_float64:
      case tok_float64x:
      case tok_float128:
      case tok_nullptr_t:
        /* A type specifier (3.5.2) that indicates a basic type. */
        if (!type_specifier_allowed) {
          pos_error(ec_type_specifier_not_allowed, &error_position);
          err = TRUE;
        } else if (basic_type != bt_none) {
          /* Basic type has already been specified in some way. */
#if GNU_EXTENSIONS_ALLOWED
          if (gcc_version_is(< 40000) ||
              (gnu_version_is(any_version) &&
               (curr_token == tok_wchar_t || curr_token == tok_bool ||
                curr_token == tok_float32 || curr_token == tok_float32x ||
                curr_token == tok_float64 || curr_token == tok_float64x ||
                curr_token == tok_float128) &&
               seq_is_in_system_header(pos_curr_token.seq)) ||
              ((gnu_version_is(any_version) ||
                clang_version_is(any_version)) &&
               curr_token == tok_nullptr_t)) {
            /* Early GNU C allows multiple basic type specifiers, but they
               must be part of a typedef declaration that doesn't include a
               declarator (and therefore it doesn't really declare
               anything).  For example, "typedef int int;".  In system
               headers, a typedef for wchar_t, bool, or one of the _Float*
               types is discarded like this by all current GNU C++
               compilers.  Both gcc and clang accept typedefs of the C23
               nullptr_t keyword in any file, not just system headers. */
            delayed_error = ec_bad_combination_of_type_specifiers;
            copy_source_position(pos_curr_token, pos_delayed_error);
          } else
#endif /* GNU_EXTENSIONS_ALLOWED */
          /* Do not insert code here. */
          {
            bad_combination_of_type_specifiers = TRUE;
            pos_error(ec_bad_combination_of_type_specifiers, &error_position);
          }  /* if */
        } else {
          switch (curr_token) {
            case tok_void:     basic_type = bt_void;    break;
            case tok_char:     basic_type = bt_char;    break;
            case tok_wchar_t:  basic_type = bt_wchar_t; break;
            case tok_char8_t:  basic_type = bt_char8_t; break;
            case tok_char16_t: basic_type = bt_char16_t; break;
            case tok_char32_t: basic_type = bt_char32_t; break;
            case tok_c99_bool:
              check_for_c23_deprecation("_Bool", ec_c23_bool_deprecated);
              FALLTHROUGH
            case tok_bool:     basic_type = bt_bool;    break;
            case tok_int:      basic_type = bt_int;     break;
            case tok_bit_precise_int:
              { a_boolean          lparen_found;
                a_source_position  wpos;
                basic_type = bt_bit_precise_int;
                bit_precise_width_con = fs_constant(ck_error);
                /* Skip over _BitInt. */
                (void)get_token();
                lparen_found = required_token(tok_lparen, ec_exp_lparen);
                wpos = pos_curr_token;
                if (lparen_found) {
                  add_stop_token(tok_rparen);
                  scan_integral_constant_expression(bit_precise_width_con);
                  (void)required_token_no_advance(tok_rparen, ec_exp_rparen);
                  remove_stop_token(tok_rparen);
                }  /* if */
                bit_precise_width_con->source_corresp.decl_position = wpos;
              }
              break;
            case tok_float:    basic_type = bt_float;   break;
            case tok_double:   basic_type = bt_double;  break;
#if FIXED_POINT_ALLOWED
            case tok_fract:    basic_type = bt_fract;   break;
            case tok_accum:    basic_type = bt_accum;   break;
#endif /* FIXED_POINT_ALLOWED */
            case tok_float32:  basic_type = bt_float32; break;
            case tok_float32x: basic_type = bt_float32x; break;
            case tok_float64:  basic_type = bt_float64; break;
            case tok_float64x: basic_type = bt_float64x; break;
            case tok_float128: basic_type = bt_std_float128; break;
            case tok_nullptr_t: basic_type = bt_nullptr_t; break;
            default:
              unexpected_condition_str("decl_specifiers: bad type specifier");
          }  /* switch */
          if (curr_token == tok_void && !any_decl_specifiers_seen) {
            decl_specifiers_seen = DS_VOID;
          } else {
            decl_specifiers_seen |= DS_TYPE;
          }  /* if */
        }  /* if */
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case tok_int8:
      case tok_int16:
      case tok_int32:
      case tok_int64:
        /* The Microsoft keywords __int8, __int16, __int32, and __int64
           represent a basic type and a size in combination.  In other words,
           an explicit size may not be specified in conjunction with either. */
        if (!type_specifier_allowed) {
          pos_error(ec_type_specifier_not_allowed, &error_position);
          err = TRUE;
        } else if (basic_type != bt_none || size != size_none) {
          /* Basic type or size has already been specified in some way. */
          bad_combination_of_type_specifiers = TRUE;
          pos_error(ec_bad_combination_of_type_specifiers, &error_position);
        } else {
          if (curr_token == tok_int8) {
            /* __int8 is treated as a plain char. */
            basic_type = bt_char;
            size = size_int8;
          } else {
            /* Set both basic type and size. */
            basic_type = bt_int;
            switch (curr_token) {
              case tok_int16:  size = size_int16; break;
              case tok_int32:  size = size_int32; break;
              case tok_int64:  size = size_int64; break;
              default:;
            }  /* switch */
          }  /* if */
          decl_specifiers_seen |= DS_TYPE;
        }  /* if */
        break;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
      case tok_int128:
        /* The GNU keyword __int128 represents a basic type and a size in
           combination.  In other words, an explicit size may not be specified
           in conjunction with it. */
        if (!type_specifier_allowed) {
          pos_error(ec_type_specifier_not_allowed, &error_position);
          err = TRUE;
        } else if (basic_type != bt_none || size != size_none) {
          /* Basic type or size has already been specified in some way. */
          bad_combination_of_type_specifiers = TRUE;
          pos_error(ec_bad_combination_of_type_specifiers, &error_position);
        } else {
          /* Set both basic type and size. */
          basic_type = bt_int;
          size = size_int128;
          decl_specifiers_seen |= DS_TYPE;
        }  /* if */
        break;
#endif /* INT128_EXTENSIONS_ALLOWED */
      case tok_short:
      case tok_long:
        /* A type specifier (3.5.2) that modifies the length of a basic
           type. */
        if (!type_specifier_allowed) {
          pos_error(ec_type_specifier_not_allowed, &error_position);
          err = TRUE;
        } else if (size != size_none) {
          /* Size has already been specified in some way. */
          if (size == size_long && curr_token == tok_long) {
            /* long long.  This is an extension. */
#if LONG_LONG_ALLOWED
            size = size_long_long;
            if (strict_ansi_mode && !long_long_is_standard) {
              diagnostic(strict_ansi_discretionary_severity,
                         ec_nonstd_long_long);
            }  /* if */
#else /* !LONG_LONG_ALLOWED */
            if (any_cfront_mode()) {
              /* Cfront warns about "long long" and treats it as "long". */
              pos_warning(ec_dupl_decl_specifier, &error_position);
            } else {
              pos_error(ec_nonstd_long_long, &error_position);
            }  /* if */
#endif /* LONG_LONG_ALLOWED */
#if GNU_EXTENSIONS_ALLOWED
          } else if (gcc_version_is(< 40000)) {
            /* GNU C allows multiple type size specifiers, but they must be
               part of a typedef declaration that doesn't include a declarator
               (and therefore it doesn't really declare anything). */
            delayed_error = ec_bad_combination_of_type_specifiers;
            copy_source_position(pos_curr_token, pos_delayed_error);
#endif /* GNU_EXTENSIONS_ALLOWED */
          } else if (size == size_short && curr_token == tok_short) {
            /* "short short".  Issue an error, except in cfront mode, which is
               silent about "short short".  (GNU C issues a warning, but that
               is handled earlier.) */
            diagnostic(any_cfront_mode() ? es_warning : es_error,
                       ec_dupl_decl_specifier);
          } else {
            /* Some other bad combination. */
            bad_combination_of_type_specifiers = TRUE;
            pos_error(ec_bad_combination_of_type_specifiers, &error_position);
          }  /* if */
        } else {
          /* First specification of size. */
          if (curr_token == tok_short) {
            size = size_short;
          } else {
            size = size_long;
          }  /* if */
          decl_specifiers_seen |= DS_TYPE;
        }  /* if */
        break;
#if C99_IL_EXTENSIONS_SUPPORTED
      case tok_c99_complex:
        check_assertion(c99_mode || gnu_mode);
        if (complex_attr == cxa_complex) {
          /* E.g. "_Complex float _Complex". */
          pos_error(ec_dupl_decl_specifier, &error_position);
        } else if (complex_attr == cxa_imaginary) {
          /* E.g. "_Imaginary float _Complex". */
          pos_error(ec_bad_combination_of_type_specifiers, &error_position);
          bad_combination_of_type_specifiers = TRUE;
        } else {
          complex_attr = cxa_complex;
        }  /* if */
        break;
      case tok_c99_imaginary:
        check_assertion(c99_mode);
        if (complex_attr == cxa_imaginary) {
          /* E.g. "_Imaginary float _Imaginary". */
          pos_error(ec_dupl_decl_specifier, &error_position);
        } else if (complex_attr == cxa_complex) {
          /* E.g. "_Complex float _Imaginary". */
          pos_error(ec_bad_combination_of_type_specifiers, &error_position);
          bad_combination_of_type_specifiers = TRUE;
        } else {
          complex_attr = cxa_imaginary;
        }  /* if */
        break;
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#if FIXED_POINT_ALLOWED
      case tok_sat:
        /* The _Sat specifier for fixed-point types. */
        if (saturating_fixed_point) {
          pos_error(ec_dupl_decl_specifier, &error_position);
        } else {
          saturating_fixed_point = TRUE;
        }  /* if */
        break;
#endif /* FIXED_POINT_ALLOWED */
      case tok_signed:
      case tok_unsigned:
        /* A type specifier (3.5.2) that modifies the signedness of a
           basic type. */
        if (!type_specifier_allowed) {
          pos_error(ec_type_specifier_not_allowed, &error_position);
          err = TRUE;
        } else if (sign != sign_none) {
          /* Sign has already been specified in some way. */
          if ((sign == sign_signed) == (curr_token == tok_signed)) {
            /* Either "signed signed" or "unsigned unsigned".  Issue an error,
               except in Microsoft, cfront, and early GNU C modes (GNU C++
               issues an error; GCC 3.3.x and 3.4.x only permits the
               duplication in typedef declarations). */
            an_error_severity  sev = es_error;
            if (any_cfront_mode() || microsoft_mode ||
                (gcc_mode && !clang_mode &&
                 (gnu_version < 30300 ||
                  (state->declared_storage_class ==
                                                (a_storage_class)sc_typedef &&
                   gnu_version < 40000)))) {
              sev = es_warning;
            }  /* if */
            pos_diagnostic(sev, ec_dupl_decl_specifier, &pos_curr_token);
          } else {
            /* Mixing signs. */
            bad_combination_of_type_specifiers = TRUE;
            pos_error(ec_bad_combination_of_type_specifiers, &error_position);
          }  /* if */
        } else {
          /* First specification of sign. */
          sign = (curr_token == tok_signed) ? sign_signed : sign_unsigned;
          decl_specifiers_seen |= DS_TYPE;
        }  /* if */
        break;
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
      case tok_partial_ref_struct:
      case tok_partial_ref_class:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
process_class_specifier:
        /* A struct or union specifier (3.5.2.1). */
        if (!type_specifier_allowed) {
          pos_error(ec_type_specifier_not_allowed, &error_position);
          err = TRUE;
        } else {
          if (basic_type == bt_none) {
#if MICROSOFT_EXTENSIONS_ALLOWED
            if (ms_extensions && !C_mode() && is_member_decl && !err &&
                !(decl_specifiers_seen & DS_FRIEND)) {
              /* In Microsoft mode, "struct S { struct S(); }; is accepted.
                 Access checks are disabled during this processing. */
              a_boolean   is_elaborated_ctor = FALSE;
              a_type_ptr  class_type = enclosing_class_type();
              if (class_type != NULL) {
                begin_deferral_of_access_checks();
                is_elaborated_ctor = is_constructor_decl(class_type, state);
                discard_deferred_access_checks();
                end_deferral_of_access_checks();
                if (is_elaborated_ctor) {
                  basic_type = bt_no_type;
                  if (*storage_class == (a_storage_class)sc_static) {
                    *output_flags |= DSO_STATIC_CONSTRUCTOR;
                  } else {
                    *output_flags |= DSO_CONSTRUCTOR | DSO_NO_DECL_SPECIFIERS;
                  }  /* if */
                  /* Skip "class" or "struct". */
                  (void)get_token();
                  goto exit_loop;
                }  /* if */
              }  /* if */
            }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
            if (!class_specifier(state, input_flags, vacuous_decl_allowed,
                                 (decl_specifiers_seen & DS_FRIEND) != 0,
                                 marked_as_gnu_extension, type_ptr,
                                 &declares_something, &defines_something,
                                 decl_pos_block)) {
              err = TRUE;
            }  /* if */
            basic_type = bt_struct_union;
            is_elaborated_type_specifier = TRUE;
          } else {
            a_boolean  dummy_flag;
            a_type_ptr dummy_type;
            /* Basic type has already been specified in some way. */
            bad_combination_of_type_specifiers = TRUE;
            pos_error(ec_bad_combination_of_type_specifiers, &error_position);
            /* Scan the specifier anyway, but throw it away. */
            (void)class_specifier(
                       state, input_flags, /*vacuous_decl_allowed=*/FALSE,
                       /*is_friend_decl=*/FALSE, marked_as_gnu_extension,
                       &dummy_type, &dummy_flag, &dummy_flag, decl_pos_block);
          }  /* if */
          decl_specifiers_seen |= DS_TYPE;
          goto no_get_token;
        }  /* if */
        break;
      case tok_enum:
#if MICROSOFT_EXTENSIONS_ALLOWED
      case tok_enum_class:
      case tok_enum_struct:
process_enum_specifier:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* An enumeration specifier (3.5.2.2). */
        if (!type_specifier_allowed) {
          pos_error(ec_type_specifier_not_allowed, &error_position);
          err = TRUE;
        } else {
          if (basic_type == bt_none) {
            enum_specifier(state, input_flags, vacuous_decl_allowed,
                           /*is_enum_template_definition=*/FALSE, type_ptr,
                           &state->ms_attributes,
                           &declares_something, &defines_something,
                           decl_pos_block);
            if (is_error_type(*type_ptr)) {
              /* An error was detected in enum_specifier -- typically, an
                 ill-formed tag name. */
              err = TRUE;
              basic_type = bt_error;
            } else {
              basic_type = bt_enum;
              is_elaborated_type_specifier = TRUE;
            }  /* if */
          } else {
            a_boolean  dummy_flag;
            a_type_ptr dummy_type;
            /* Basic type has already been specified in some way. */
            bad_combination_of_type_specifiers = TRUE;
            pos_error(ec_bad_combination_of_type_specifiers, &error_position);
            /* Scan the specifier anyway, but throw it away. */
            enum_specifier(state, input_flags, /*vacuous_decl_allowed=*/FALSE,
                           /*is_enum_template_definition=*/FALSE,
                           &dummy_type, (an_ms_attribute_ptr*)NULL,
                           &dummy_flag, /*defines_something=*/NULL,
                           decl_pos_block);
          }  /* if */
          decl_specifiers_seen |= DS_TYPE;
          goto no_get_token;
        }  /* if */
        break;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case tok_unresolved_type:
        check_assertion(cli_or_cx_enabled);
        *type_ptr = scan_unresolved_metadata_type();
        if (!is_error_type(*type_ptr)) {
          check_assertion(is_immediate_class_type(*type_ptr));
          basic_type = bt_struct_union;
        } else {
          basic_type = bt_error;
          err = TRUE;
        }  /* if */
        decl_specifiers_seen |= DS_TYPE;
        goto no_get_token;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case tok_edg_internal_type:
        *type_ptr = scan_edg_internal_type();
        decl_specifiers_seen |= DS_TYPE;
        basic_type = bt_typedef;
        goto no_get_token;
      case tok_edg_size_type:
        /* An EDG-specific way to specify size_t. */
        *type_ptr = integer_type(targ_size_t_int_kind);
        decl_specifiers_seen |= DS_TYPE;
        basic_type = bt_typedef;
        break;
      case tok_edg_ptrdiff_type:
        /* An EDG-specific way to specify ptrdiff_t. */
        *type_ptr = integer_type(targ_ptrdiff_t_int_kind);
        decl_specifiers_seen |= DS_TYPE;
        basic_type = bt_typedef;
        break;
      case tok_edg_bool_type:
        /* An EDG-specific way to specify a boolean type. */
        *type_ptr = bool_type();
        decl_specifiers_seen |= DS_TYPE;
        basic_type = bt_typedef;
        break;
      case tok_edg_wchar_type:
        /* An EDG-specific way to specify a wchar_t type. */
        *type_ptr = eff_wchar_t_type();
        decl_specifiers_seen |= DS_TYPE;
        basic_type = bt_typedef;
        break;
      case tok_edg_vector_type:
        *type_ptr = scan_edg_vector_type(vk_gnu);
        decl_specifiers_seen |= DS_TYPE;
        basic_type = bt_typedef;
        goto no_get_token;
      case tok_edg_neon_vector_type:
        *type_ptr = scan_edg_vector_type(vk_neon);
        decl_specifiers_seen |= DS_TYPE;
        basic_type = bt_typedef;
        goto no_get_token;
      case tok_edg_neon_polyvector_type:
        *type_ptr = scan_edg_vector_type(vk_neon_poly);
        decl_specifiers_seen |= DS_TYPE;
        basic_type = bt_typedef;
        goto no_get_token;
      case tok_edg_scalable_vector_type:
        *type_ptr = scan_edg_scalable_vector_type();
        decl_specifiers_seen |= DS_TYPE;
        basic_type = bt_typedef;
        goto no_get_token;
      case tok_ifc_type_ref:
        *type_ptr = load_tok_ifc_type_ref();
        decl_specifiers_seen |= DS_TYPE;
        basic_type = bt_typedef;
        break;
      case tok_typename:
        /* A typename specifier.  The typename keyword is used to
	   specify that the qualified name that follows the keyword is
	   a type.  This is used to parse template definitions (as
           opposed to parsing a template instantiation when the values of
           the template parameters are known. */
        if (!type_specifier_allowed) {
          pos_error(ec_type_specifier_not_allowed, &error_position);
          err = TRUE;
        } else if (reflection_enabled && next_token() == tok_lsplice) {
          /* When reflection features are enabled typename[:expr:] is a
             valid simple-type-specifier. */
          (void)get_token();
          if (spliced_name_qualifier_next()) {
            /* The splice is a name qualifier (the "typename" keyword refers
               to the qualified name in that case). */
            goto general_identifier_case;
          } else {
            *type_ptr = scan_type_splicer((a_rescan_control_block *)NULL);
            if (type_is(*type_ptr, tk_typeref) &&
                is_typeref_kind(*type_ptr, trk_is_splice)) {
              (*type_ptr)->variant.typeref.has_typename_prefix = TRUE;
            }  /* if */
            basic_type = bt_typedef;
            decl_specifiers_seen |= DS_TYPE;
            goto no_get_token;
          }  /* if */
        } else if (sun_mode && use_implicit_typename()) {
          /* typename is ignored in Sun mode.  Simply discard the token
             unless the user has disabled implicit typename mode. */
        } else if (basic_type != bt_none) {
          /* Ignore a typename specifier if a basic type has already been
             seen (with either a warning or a discretionary error).  This
             allows typename to be used in some invalid locations.  In
             particular this allows typename before a declarator, which
             the Microsoft compiler allows. */
          diagnostic(microsoft_bugs ? es_warning : es_discretionary_error,
                     ec_invalid_typename_specifier);
        } else {
          a_symbol_ptr	type_sym;
          typename_specifier(type_ptr, &type_sym, /*within_using_decl=*/FALSE,
                             /*is_decl_specifier=*/TRUE, state,
                             decl_pos_block);
          if (*type_ptr == NULL) {
            /* In Microsoft mode a NULL type is returned for a nonstandard
               typename specifier in which the typename keyword is followed by
               something other than a qualified name. */
            check_assertion(ms_extensions);
            if (is_template_context()) {
              /* The Microsoft compiler ignores certain typename specifiers in
                 instantiations, so forget that we have seen a specifier. */
              count_as_specifier_seen = FALSE;
              specifier_allows_vacuous_decl = TRUE;
            }  /* if */
            goto no_get_token;
          }  /* if */
          basic_type = bt_typename;
          is_elaborated_type_specifier = TRUE;
          decl_specifiers_seen |= DS_TYPE;
          goto no_get_token;
        }  /* if */
        break;
      case tok_overload:
        /* Special case -- the "overload" keyword (which shows up in
           cfront compatibility mode only).  Ignore it and advance to the
           next token. */
        auto_type_allowed = FALSE;
        diagnostic(anachronism_error_severity, ec_overload_anachronism);
        decl_specifiers_seen |= DS_OVERLOAD;
        break;
      case tok_underlying_type:
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
type_transform_case:
        /* A "type-returning type trait" (e.g., __underlying_type). */
        { a_source_position  decltype_pos = pos_curr_token;
          *type_ptr = scan_type_returning_type_trait_operator();
          if (!is_error_type(*type_ptr) &&
              (basic_type != bt_none || sign != sign_none ||
               size != size_none)) {
            /* We've already seen specifiers that cannot be combined with
               this type trait: Ignore them and issue an error. */
            pos_error(ec_bad_combination_of_type_specifiers, &decltype_pos);
            *type_ptr = error_type();
            sign = sign_none;
            size = size_none;
          }  /* if */
          basic_type = bt_typedef;
          decl_specifiers_seen |= DS_TYPE;
          goto no_get_token;
        }
#if GNU_EXTENSIONS_ALLOWED
      case tok_bases:
      case tok_direct_bases:
        /* The g++ __bases or __direct_bases operators. */
        { a_source_position  bases_pos = pos_curr_token;
          *type_ptr = scan_bases_operator();
          if (!is_error_type(*type_ptr) &&
              (basic_type != bt_none || sign != sign_none ||
               size != size_none)) {
            /* We've already seen specifiers that cannot be combined with
               this operator: Ignore them and issue an error. */
            pos_error(ec_bad_combination_of_type_specifiers, &bases_pos);
            *type_ptr = error_type();
            sign = sign_none;
            size = size_none;
          }  /* if */
          basic_type = bt_typedef;
          decl_specifiers_seen |= DS_TYPE;
          goto no_get_token;
        }
#endif /* GNU_EXTENSIONS_ALLOWED */
      case tok_typeof:
      case tok_typeof_unqual:
        { a_source_position  typeof_pos = pos_curr_token;
          *type_ptr = scan_typeof_operator((a_rescan_control_block *)NULL,
                                           decl_pos_block);
          if (!is_error_type(*type_ptr) &&
              (basic_type != bt_none || sign != sign_none ||
               size != size_none)) {
            /* We've already seen specifiers that cannot be combined with
               __typeof__: Ignore them and issue an error. */
            pos_error(ec_bad_combination_of_type_specifiers, &typeof_pos);
            *type_ptr = error_type();
            sign = sign_none;
            size = size_none;
          }  /* if */
          basic_type = bt_typedef;
          decl_specifiers_seen |= DS_TYPE;
          goto no_get_token;
        }
      case tok_decltype:
        /* This could be decltype(auto), decltype(...)::... or just a plain
           decltype specifier.  The first case is handled here, and the others
           in the fall-through path. */
        if (decltype_auto_enabled && decltype_auto_tokens_next()) {
          check_assertion(auto_type_specifier_enabled);
          if (state->auto_type_specifier_seen ||
              state->decltype_auto_specifier_seen) {
            pos_error(ec_bad_combination_of_type_specifiers, &pos_curr_token);
          } else {
            auto_is_first = !(decl_specifiers_seen & ~(DS_INLINE | DS_FRIEND));
            state->auto_pos = pos_curr_token;
            state->has_deduced_type = TRUE;
            state->decltype_auto_specifier_seen = TRUE;
            process_auto_specifier(
                    decltype_auto_enabled, auto_is_first, input_flags, state,
                    decl_pos_block, &decl_specifiers_seen, &basic_type,
                    type_ptr, &err);
          }  /* if */
          /* Consume the four tokens of "decltype(auto)". */
          (void)get_token();
          (void)get_token();
          (void)get_token();
          (void)get_token();
          goto no_get_token;
        }  /* if */
        goto general_identifier_case;
      case tok_identifier:  /* Identifier or "::". */
        if (locator_for_curr_id.symbol_header != NULL &&
            locator_for_curr_id.symbol_header->has_intrinsic_name &&
            check_type_transform_name() != tok_error) {
          goto type_transform_case;
        }  /* if */
        FALLTHROUGH
      case tok_colon_colon:
#if MICROSOFT_EXTENSIONS_ALLOWED
      case tok_super:
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case tok_ifc_entity_ref:
      case tok_ifc_decl_ref:
general_identifier_case:
        /* Identifier. */
#if EXTRA_SOURCE_POSITIONS_IN_IL
        id_start_pos = pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
        /* The appearance of an identifier may mean that the specifiers
           are complete (the identifier is a declarator) or it may be another
           specifier.  First we look for conditions that will cause us to
           exit the loop -- generally because the identifier is clearly not
           a specifier.  Note that when we get here with a decltype token
           this could be a simple "decltype(x)" or the start of a qualified
           name (e.g., "decltype(x)::something").  In the former case,
           the call to process_nontype_identifier will return FALSE and
           the current token will be a tok_decltype_construct, which is
           handled below. */
        /* To be more specific: in ANSI C, an identifier that appears to be
           a typedef name is not recognized as such if the specifiers list
           already includes a basic type or sign.  This is so that the
           following (from the standard, 3.5.6) will work:
               typedef signed int t;
               main () {
                 long t;    <-- This declares a new identifier t.
               }
           K&R (first edition, Appendix A, section 11.1) also includes the
           following:
               typedef float distance;
               {
                 auto int distance;
               }
           but pcc (at least on a Sun 3) doesn't accept this.  It always
           seems to scan a typedef name as a typedef name.  To conform to
           K&R, we consider an identifier to be a typedef when there is
           just a sign or size (since these are "adjectives" to pcc), but
           not when there is a type specifier. */
#if MICROSOFT_EXTENSIONS_ALLOWED
        if (context_specific_keyword_expected) {
          /* A "pre-scan" has already determined that the first identifier
             among the specifiers is a C++/CLI context-sensitive keyword.
             Since the state already reflects this specifier, it can now be
             skipped. */
          check_assertion(cli_or_cx_enabled && curr_token == tok_identifier);
          context_specific_keyword_expected = FALSE;
          break;
        } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Do not insert code here. */
        {
          a_boolean  unexpected_identifier = FALSE;
          if (process_nontype_identifier(state, decl_specifiers_seen,
                                         input_flags, &basic_type,
                                         &named_address_space,
                                         &unexpected_identifier)) {
            if (unexpected_identifier) {
              goto something_unexpected;
            } else {
              if (state->dso_flags & DSO_CONSTRUCTOR &&
                  qualifiers != TQ_NONE) {
                /* In some modes, qualifiers are accepted on a constructor
                   declaration.  Ignore them. */
                pos_warning(ec_type_qualifier_ignored, &state->qualifiers_pos);
                qualifiers = TQ_NONE;
                decl_specifiers_seen &= ~(DS_TYPE_QUALIFIER);
              }  /* if */
              /* Note that with a branch to exit_loop the get_token call is
                 bypassed.  In the case of a constructor declaration, this
                 means curr_token will still represent the constructor name
                 (= class name) upon return to the caller. */
              goto exit_loop;
            }  /* if */
#if NAMED_ADDRESS_SPACES_ALLOWED
          } else if (named_address_space != 0) {
            if (named_address_space_from_qualifier_set(qualifiers) != 0) {
              pos_error(ec_multiple_named_address_spaces, &error_position);
            } else {
              record_qualifiers_pos();
              set_named_address_space_in_qualifier_set(qualifiers,
                                                       named_address_space);
            }  /* if */
            break;
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
          }  /* if */
        }
        FALLTHROUGH
      case tok_decltype_construct:
        /* When a decltype is encountered by is_identifier_start, it must
           be scanned to see if it is followed by "::".  When it is not
           followed by "::", a tok_decltype_construct token is created,
           and the type from the decltype is stored in the locator. */
        if (curr_token == tok_decltype_construct) {
          a_source_position  decltype_pos = pos_curr_token;
          *type_ptr = locator_for_curr_id.variant.decltype_type;
          if (!is_error_type(*type_ptr) &&
              (basic_type != bt_none || sign != sign_none ||
               size != size_none)) {
            /* We've already seen specifiers that cannot be combined with
               decltype: Ignore them and issue an error. */
            pos_error(ec_bad_combination_of_type_specifiers, &decltype_pos);
            *type_ptr = error_type();
            sign = sign_none;
            size = size_none;
          }  /* if */
          basic_type = bt_typedef;
          decl_specifiers_seen |= DS_TYPE;
          break;
        }  /* if */
        if (sign != sign_none || size != size_none) {
          /* There is an indication of sign and/or size (but no indication
             of a basic type).  In ANSI C and C++, assume we're dealing with
             a declarator.  In pcc mode and in some GNU modes, adjectival
             modification of a typedef is allowed in certain circumstances, so
             keep going till we know if the identifier is a typedef.  (GNU
             compilers behave differently depending on whether the sign or
             size modifier appears before or after the typedef name.  This
             code handles the usual case where the modifier appears before the
             typedef name: That is only accepted by g++ 3.4 and later.) */
          if (!(C_dialect == C_dialect_pcc ||
                (gpp_version_is(>= 30400) &&
                 (input_flags & DSI_NO_REAL_DECLARATOR) != 0))) {
            goto exit_loop;
          }  /* if */
        }  /* if */
        /* Look up the identifier as a type symbol, if it has not already been
           looked up. */
        if (microsoft_mode && state->declared_storage_class == sc_typedef &&
            basic_type == bt_none) {
          /* MSVC appears not to require "typename" after a typedef.  E.g.:
               template<typename T> auto f(T x) {
                 typedef T::X X;  // Okay in Microsoft mode.
                 return X(x);
               }
          */
          state->is_implicit_type_context = TRUE;
        }  /* if */
        { a_boolean  implicit_typename = state->is_implicit_type_context &&
                                         relaxed_typename_enabled,
                     retried = FALSE,
                     concept_okay = concepts_enabled &&
                                    (is_parameter ||
                                     state->auto_type_allowed ||
                                     state->is_trailing_return_type),
                     pack_index_follows =
                                      !locator_for_curr_id.is_qualified_name &&
                                      pack_index_next();
retry_type_name_determination:
          /* If a type pack-index-specifier (T...[N]) follows, the identifier
             must be classified with curr_type_symbol(..., in_type_check=TRUE)
             on this pass so the lookup does not record variadic pack
             references that belong only to the real pack-index parse in
             scan_pack_index_type_specifier. */
          curr_token_type_symbol =
                    curr_type_symbol((input_flags & DSI_IS_NEW_TYPE_NAME) != 0,
                                     /*in_prescan=*/FALSE,
                                     /*in_type_check=*/pack_index_follows,
                                     implicit_typename,
                                     /*is_sizeof_context=*/FALSE,
                                     concept_okay);
          if (clangcpp_version_is(>= 160000)) {
            /* Clang 16 (and later) appears to accept the implicit typename
               contexts of C++20 in pre-C++20 modes, but with a warning.  We
               emulate this by retrying a curr_type_symbol call that failed
               in an implicit type context without relaxed_typename_enabled
               as if relaxed_typename_enabled is TRUE.  If it succeeds the
               second time around, we issue a warning. */ 
            if (!retried && curr_token_type_symbol == NULL &&
                state->is_implicit_type_context) {
              implicit_typename = TRUE;
              retried = TRUE;
              goto retry_type_name_determination;
            } else if (retried && curr_token_type_symbol != NULL) {
              pos_warning(ec_missing_typename, &pos_curr_token);
            }  /* if */
          }  /* if */
          if (curr_token_type_symbol != NULL &&
              is_type_symbol(curr_token_type_symbol) && pack_index_follows) {
            /* This is a C++26 type pack-index-specifier. */
            *type_ptr = scan_pack_index_type_specifier(
                                     (input_flags & DSI_IS_NEW_TYPE_NAME) != 0,
                                     implicit_typename, concept_okay,
                                     /*might_be_id_start=*/FALSE);
            basic_type = bt_typedef;
            decl_specifiers_seen |= DS_TYPE;
            state->type_is_injected_class_name = FALSE;
            goto no_get_token;
          }  /* if */
        }
        if (curr_token_type_symbol != NULL &&
            symbol_is(curr_token_type_symbol, sk_concept_template)) {
          if (is_parameter) {
            /* An abbreviated function template parameter. */
            if (process_auto_parameter(state, curr_token_type_symbol)) {
              state->auto_pos = pos_curr_token;
              state->has_deduced_type = TRUE;
              state->auto_type_specifier_seen = TRUE;
            } else {
              expect_error();
            }  /* if */
            basic_type = bt_typedef;
            decl_specifiers_seen |= DS_TYPE;
          } else {
            /* Presumably something like "C<8> auto x = 42;". */
            state->type_constraint =
                                 scan_type_constraint(curr_token_type_symbol);
            if (curr_token != tok_auto &&
                !(curr_token == tok_decltype && decltype_auto_tokens_next())) {
              pos_error(ec_exp_auto, &pos_curr_token);
              state->type_constraint = NULL;
            }  /* if */
            goto no_get_token;
          }  /* if */
          break;
        }  /* if */
        if (!C_mode() && is_member_decl &&
            (decl_specifiers_seen & DS_TYPE) == 0 &&
            curr_token_type_symbol != NULL &&
            !locator_for_curr_id.is_qualified_name &&
            is_template_param_type_symbol(curr_token_type_symbol) &&
            skip_typerefs(curr_token_type_symbol->variant.type.ptr)
              ->variant.template_param.kind ==
                                    (a_template_param_type_kind)tptk_member) {
          /* This is the first type specifier we see, but it is only a
             placeholder that assumes the symbol will be found as a type in
             a dependent base class.  If it looks like the start of a member
             function declarator with an implicit int return type, discard
             the result of the lookup.  This is to allow code like:
               template<class T> struct B {};
               template<class T> struct D: B { f(const int); };  */
          if (looks_like_member_function_declarator()) {
            curr_token_type_symbol = NULL;
          }  /* if */
        }  /* if */
        if (curr_token_type_symbol != NULL) {
          if (locator_for_curr_id.is_class_member &&
              ((curr_token_type_symbol->kind == (a_symbol_kind)sk_type &&
                curr_token_type_symbol->variant.type.is_injected_class_name) ||
	       (gpp_mode && locator_for_curr_id.is_template_id &&
		gpp_type_name_matches_class_name(curr_token_type_symbol))) && 
	      (!(decl_specifiers_seen &
                 ~(DS_FRIEND | DS_INLINE | DS_CONSTEXPR |
                   DS_MICROSOFT_INLINE | DS_FORCEINLINE))) &&
              /* g++ allows X::X to be used in most places as a type name.
                 A left parenthesis seems to be used to detect the constructor
                 case (but this must be suppressed for new type names). */
              (!gpp_mode ||
               ((input_flags & DSI_IS_NEW_TYPE_NAME) == 0 &&
                next_token() == tok_lparen))) {
            /* This identifier appears to specify a constructor. */
            a_type_ptr    tp = type_symbol_type(curr_token_type_symbol);
            a_symbol_ptr  sym = symbol_supplement_for_class(tp)->constructor;
            if (sym != NULL) {
              a_type_ptr  curr_id_parent =
                                    qualifier_class_type(locator_for_curr_id);
              curr_id_parent = skip_typerefs(curr_id_parent);
              if (same_entities(curr_id_parent, tp)) {
                *output_flags |= DSO_CONSTRUCTOR;
                if (!any_decl_specifiers_seen) {
                  *output_flags |= DSO_NO_DECL_SPECIFIERS;
                }  /* if */
                basic_type = bt_no_type;
                locator_for_curr_id.specific_symbol = sym;
                locator_for_curr_id.template_arg_list = NULL;
                locator_for_curr_id.symbol_header = sym->header;
                goto exit_loop;
              }  /* if */
            }  /* if */
          }  /* if */
          if (sign != sign_none || size != size_none) {
            /* We are in pcc mode, in which adjectival modification of a
               typedef is allowed -- but with restrictions.  For integral
               types, any size/sign is allowed. For floating types, only
               "long" is allowed. */
            a_type_ptr  tp = type_symbol_type(curr_token_type_symbol);
            if (is_integral_or_enum_type(tp) ||
                (is_floating_type(tp) &&
                 sign == sign_none && size == size_long)) {
              /* Adjectives okay. */
            } else {
              goto exit_loop;
            }  /* if */
          }  /* if */
          /* Do ambiguity and access control checking. */
          if ((input_flags & DSI_IS_NEW_TYPE_NAME) && microsoft_bugs &&
              (microsoft_version < 1310 || microsoft_version == 1400)) {
            /* Various versions of MSVC seem not to check access to the type
               specified in a "new".  Access to the constructor, if any,
               will be checked later.  Do ambiguity checking anyway. */
            (void)f_check_for_ambiguity(&locator_for_curr_id,
                                        /*is_templ_context=*/FALSE,
                                        /*is_qualifier=*/FALSE,
                                        /*issue_diagnostics=*/TRUE);
          } else {
            check_ambiguity_and_verify_access(&locator_for_curr_id);
          }  /* if */
          /* The identifier is a type name and should be treated as a
             type specifier. */
          if (is_error_locator(locator_for_curr_id)) {
            /* Ambiguity error was issued. */
            err = TRUE;
            basic_type = bt_typedef;
            decl_specifiers_seen |= DS_TYPE;
            *type_ptr = error_type();
          } else {
            a_type_ptr  tp = type_symbol_type(curr_token_type_symbol);
#if DEFAULT_RECORD_FORM_OF_NAME_REFERENCE
            if (record_form_of_name_reference) {
              tp = make_typeref_with_lexical_information(tp,
                                                         &locator_for_curr_id);
            }  /* if */
#endif /* DEFAULT_RECORD_FORM_OF_NAME_REFERENCE */
            if (locator_for_curr_id.is_semivisible_nested_type) {
              /* The symbol in the locator is a nested class that is not
                 visible according to the ARM lookup rules but is returned
                 in support of the nested class anachronism (ARM 18.3.5).
                 Issue an anachronism diagnostic. */
              sym_diagnostic(anachronism_error_severity, 
                             ec_nested_class_anachronism,
                             locator_for_curr_id.specific_symbol);
            }  /* if */
            /* If the symbol is a projection symbol, get the fundamental
               symbol. */
            reduce_projection_symbol_to_fundamental_symbol(
                                                      curr_token_type_symbol);
            mark_referenced(curr_token_type_symbol,
                            &locator_for_curr_id.source_position);
            if (!type_specifier_allowed) {
              pos_error(ec_type_specifier_not_allowed, &error_position);
              err = TRUE;
            } else {
              /* Save the type. */
#if MICROSOFT_EXTENSIONS_ALLOWED
              if (microsoft_mode && !any_decl_specifiers_seen &&
                  curr_token_type_symbol->is_template_param &&
                  type_is(curr_token_type_symbol->variant.type.ptr, tk_void)) {
                basic_type = bt_void;
                decl_specifiers_seen = DS_VOID;
                state->template_void_specifier = TRUE;
              } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
              /* Do not insert code here. */
              { basic_type = bt_typedef;
                *type_ptr = tp;
                decl_specifiers_seen |= DS_TYPE;
                state->type_is_injected_class_name = 
                              is_injected_class_symbol(curr_token_type_symbol);
                if (class_template_arg_deduction_enabled &&
                    type_is(skip_lexical_typerefs(tp), tk_template_param) &&
                    !locator_for_curr_id.is_template_id) {
                  /* Check for class template argument deduction.  If the
                     identifier is something like "T::template X<Y>" it
                     will have been coalesced, so suppress this processing
                     if we have a template-id. */
                  process_class_template_placeholder(
                                             state, skip_lexical_typerefs(tp));
                }  /* if */
              }  /* if */
            }  /* if */
          }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
          if (decl_pos_block != NULL) {
            /* Record the position of this type name.  The identifier_range may
               later be overwritten when scanning the identifier in a
               declarator.  So the caller should save these positions if they
               are of interest and decl_pos_block is passed in a call to
               declarator. */
            decl_pos_block->identifier_range.start = id_start_pos;
            decl_pos_block->identifier_range.end = end_pos_curr_token;
          }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
          break;
        }  /* if */
        /* Getting to this point means the identifier is not a type name.
           However, the lookup may still have found something -- see
           locator_for_curr_id.specific_symbol. */
        if (input_flags & DSI_IS_NEW_TYPE_NAME) {
          /* This is an identifier in a "new" expression so it was
             probably intended to be a type name.  Issue an error and
             pretend that's what it is. */
          pos_error(ec_exp_type_specifier, &error_position);
          err = TRUE;
          basic_type = bt_typedef;
          *type_ptr = error_type();
          decl_specifiers_seen |= DS_TYPE;
          break;
        }  /* if */
        if (locator_for_curr_id.is_operator_name ||
            locator_for_curr_id.is_conversion_name) {
          /* This identifier represents something like "A::operator+" or
             "A::operator int"." */
          goto operator_or_conversion_name;
        }  /* if */
        if (is_dtor_like_locator(locator_for_curr_id) &&
            !(decl_specifiers_seen & DS_FRIEND) &&
	    (simplify_curr_class_qualified_name() ||
	     !locator_for_curr_id.is_qualified_name)) {
          /* This identifier represents something like "A::~A".  This
             case is handled one way if we are currently processing the
             definition of class A and another way if we are not.  If we
	     are processing the definition of class A, "A::~A" will have
	     already been coalesced and the qualifier information
             will have been discarded by simplify_curr_class_qualified_name.
             This test identifies this case and transfers control to the code
             that would have been executed if the program simply said "~A"
             instead of "A::~A".  If we are not processing the definition of
             class A, we simply fall through this test. */
          /* C++/CLI finalizer syntax is handled similarly. */
#if MICROSOFT_EXTENSIONS_ALLOWED
          if (locator_for_curr_id.is_finalizer_name) {
            goto finalizer_name;
          } else
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
          /* Do not insert code here. */
          {
            goto destructor_name;
          }  /* if */
        }  /* if */
        bad_type_name_error = FALSE;
        if (is_error_locator(locator_for_curr_id) &&
            locator_for_curr_id.is_template_id) {
          /* An error was detected in scanning a class template id.  Since
             a template id can only be a type, treat it as an error type. */
          bad_type_name_error = TRUE;
        } else if (!any_decl_specifiers_seen &&
                   !(input_flags & DSI_EMPTY_DECL_SPECIFIERS_ALLOWED)) {
          /* If this is the first specifier, and this identifier is undefined,
             assume that we are dealing with a name that was supposed to be
             declared as a typedef.  Note that we do not get here on
             declarations, so this bit of error recovery tweaking applies
             only to things like prototyped parameter declarations and
             members of structs/unions. */
          bad_type_name_error = TRUE;
        } else if (!any_decl_specifiers_seen &&
                   is_error_locator(locator_for_curr_id) &&
                   locator_for_curr_id.is_global_qualified_name) {
          /* An error was detected in scanning a qualified name that started
             with "::".  Since declarators may not start with "::" we assume
             this to be like an unidentified type name. */
          bad_type_name_error = TRUE;
        } else if (type_specifier_allowed && basic_type == bt_none &&
                   sign == sign_none && size == size_none) {
          /* This is an error recovery optimization.  The current identifier
             is not a type name, but if it is undefined and the next token
             is the start of a declarator, we may plausibly have something
             like "extern x y" or static x *z", where x can be interpreted
             as a type name. */
          a_tiny_scanning_token_cache  cache;

          /* Put the current token in the cache. */
          cache_curr_token(cache.ptr());
          /* Advance to next token. */
          (void)get_token();
          /* Check next token for start of a declarator.  "(" may be part
             of a function declaration, so it doesn't count. */
          if (is_declarator_start() && curr_token != tok_lparen) {
            /* Assume that the undefined identifier that is apparently
               followed by a declarator was intended to be a type name. */
            bad_type_name_error = TRUE;
          }  /* if */
          /* Restore the token state. */
          rescan_cached_tokens(cache.ptr());
        }  /* if */
        if (bad_type_name_error) {
          report_bad_type_name(input_flags);
          err = TRUE;
          basic_type = bt_typedef;
          *type_ptr = error_type();
          decl_specifiers_seen |= DS_TYPE;
          break;
        }  /* if */
        if (!(decl_specifiers_seen &
              ~(DS_FRIEND | DS_INLINE |
                DS_MICROSOFT_INLINE | DS_FORCEINLINE))) {
          /* A function declaration without declaration specifiers is
             permitted. */
          if (!any_decl_specifiers_seen) {
            *output_flags |= DSO_NO_DECL_SPECIFIERS;
          }  /* if */
          /* Set the type appropriately if this is the name of a constructor
             member function. */
          if (locator_for_curr_id.is_class_member) {
            a_type_ptr    qual_tp = qualifier_class_type(locator_for_curr_id);
            a_symbol_ptr  sym;
            if (is_enum_type(qual_tp)) {
              sym = enum_qualified_id_lookup(&locator_for_curr_id, qual_tp);
            } else {
              sym = look_up_class_member_decl(qual_tp, state);
            }  /* if */
            if (sym != NULL) {
              if (is_constructor_symbol(sym)) {
                *output_flags |= DSO_CONSTRUCTOR;
                basic_type = bt_no_type;
              } else if (is_destructor_symbol(sym)) {
                *output_flags |= DSO_DESTRUCTOR;
                basic_type = bt_no_type;
#if MICROSOFT_EXTENSIONS_ALLOWED
              } else if (is_static_constructor_symbol(sym)) {
                *output_flags |= DSO_STATIC_CONSTRUCTOR;
                basic_type = bt_no_type;
              } else if (is_finalizer_symbol(sym)) {
                *output_flags |= DSO_FINALIZER;
                basic_type = bt_no_type;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
              } else {
                clear_specific_symbol(locator_for_curr_id);
              }  /* if */
            }  /* if */
          }  /* if */
          goto exit_loop;
        } else if (decl_specifiers_seen & DS_FRIEND) {
          /* Clear the specific symbol pointer in the locator so that
             subsequent lookups will be done correctly. */
          clear_specific_symbol(locator_for_curr_id);
          goto exit_loop;
        }  /* if */
        /* For non-typedef identifiers, branch to the default case. */
        goto something_unexpected;
      case tok_operator:
        /* Coalesce the operator name. */
        (void)is_generalized_identifier_start(GID_NO_OPTIONS);
operator_or_conversion_name:
        if ((input_flags & DSI_NO_REAL_DECLARATOR) != 0) {
          /* This is not a context where a declarator was expected. */
          syntax_error(ec_operator_name_not_allowed);
          err = TRUE;
          basic_type = bt_error;
          goto exit_loop;
        }  /* if */
        if (locator_for_curr_id.is_conversion_name) {
          if (basic_type == bt_none && sign == sign_none &&
              size == size_none) {
            basic_type = bt_no_type;
          } else {
            /* A type has already been specified, but the error is issued
               later in order to unify error processing for both of the
               following (only the first of which is detected here):
                 class A {
                   char operator char();       // Error detectable here
                   char* operator char*();     // Error not detectable here
                 };
               Errors for both are issued in declarator. */
          }  /* if */
        }  /* if */
        if (!err && !decl_specifiers_seen) {
          *output_flags |= DSO_NO_DECL_SPECIFIERS;
        }  /* if */
        goto exit_loop;
      case tok_template:
        /* "template" cannot appear in decl-specifiers. */
        set_to_error_locator(locator_for_curr_id);
        locator_for_curr_id.source_position = pos_curr_token;
        pos_error(ec_template_not_allowed, &pos_curr_token);
        if (next_token() == tok_lt) {
          flush_tokens();
        } else {
          (void)get_token();
        }  /* if */
        err = TRUE;
        if (basic_type == bt_typedef) {
          *type_ptr = error_type();
        } else {
          basic_type = bt_error;
        }  /* if */
        goto no_get_token;
#if MICROSOFT_EXTENSIONS_ALLOWED
      case tok_public:
      case tok_private:
        if (cli_or_cx_enabled) {
          /* C++/CLI allows something like "public class X {};" or
             "private enum E: int {};". */
          a_token_kind  next_tok = next_token();
          if (is_class_type_keyword(next_tok)) {
            goto process_class_specifier;
          } else if (is_enum_type_keyword(next_tok)) {
            goto process_enum_specifier;
          }  /* if */
        }  /* if */
        FALLTHROUGH
      case tok_protected:
        if (microsoft_bugs && microsoft_version >= 1300 &&
            *storage_class == (a_storage_class)sc_typedef) {
          /* Microsoft C++ compilers ignore access specifiers in ill-formed
             typedef declarations like "typedef int private I;". */
          pos_warning(ec_invalid_access_specifier, &error_position);
          break;
        } else {
          goto something_unexpected;
        }  /* if */
      case tok_not:
finalizer_name:
        /* Finalizers are not allowed in C++/CX. */
        if (cppcli_enabled && is_member_decl) {
          auto_type_allowed = FALSE;
          if (!any_decl_specifiers_seen) {
            *output_flags |= DSO_NO_DECL_SPECIFIERS;
          }  /* if */
          *output_flags |= DSO_FINALIZER;
          if (basic_type == bt_none && sign == sign_none &&
              size == size_none) {
            basic_type = bt_no_type;
          } else {
            /* It is an error to specify the type on a finalizer, but it will
               be reported later. */
          }  /* if */
          goto exit_loop;
        }  /* if */
        /* A finalizer is not expected. */
        goto something_unexpected;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      case tok_export:
        pos_error(ec_export_not_allowed, &pos_curr_token);
        break;
      case tok_compl:
destructor_name:
        if (!C_mode() && is_member_decl) {
          auto_type_allowed = FALSE;
          if (!any_decl_specifiers_seen) {
            *output_flags |= DSO_NO_DECL_SPECIFIERS;
          }  /* if */
          *output_flags |= DSO_DESTRUCTOR;
          if (basic_type == bt_none && sign == sign_none &&
              size == size_none) {
            basic_type = bt_no_type;
          } else {
            /* It is an error to specify the type on a destructor, but it
               will be reported later. */
          }  /* if */
          goto exit_loop;
        }  /* if */
        /* If destructors aren't expected, fall through into the default
           case. */
        FALLTHROUGH
      default:
        /* Something unexpected.  After the first time, we can just exit
           the loop (we've taken all we're supposed to).  The first time,
           this is usually an error. */
something_unexpected:
        if (!any_decl_specifiers_seen) {
          if (!(input_flags & DSI_EMPTY_DECL_SPECIFIERS_ALLOWED)) {
            syntax_error(ec_exp_type_specifier);
            err = TRUE;
            basic_type = bt_error;
          } else {
            *output_flags |= DSO_NO_DECL_SPECIFIERS;
          }  /* if */
        }  /* if */
        goto exit_loop;
    }  /* switch */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (decl_pos_block != NULL) {
      /* Each time through the loop assume the current token is the last. */
      decl_pos_block->specifiers_range.end = end_pos_curr_token;
    }  /* if */
    curr_construct_end_position = end_pos_curr_token;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    (void)get_token();
no_get_token:
    /* If this specifier should count as one that has been "seen" set the
       any specifier seen flag. */
    if (count_as_specifier_seen) any_decl_specifiers_seen = TRUE;
    /* Vacuous declarations (like "class C;") cannot be combined with most
       other specifiers, but there are a few exceptions (like __declspec). */
    if (!specifier_allows_vacuous_decl) {
      vacuous_decl_allowed = FALSE;
    }  /* if */
    /* Check for special conditions that will cause this loop to terminate. */
    if (input_flags & DSI_COLLECT_DECLARATOR_TYPE_QUALIFIERS) {
      /* We are only interested in scanning type qualifiers in a
         pointer declarator. */
      if (is_type_qualifier() or_is_near_or_far() ||
          (microsoft_mode && curr_token == tok_inline)) {
        /* Keep looping. */
      } else {
        /* Did we see tok_inline used as a qualifier? */
        goto exit_loop;
      }  /* if */
    } else if (defines_something &&
               !(gcc_mode && *storage_class == (a_storage_class)sc_typedef) &&
               input_flags & DSI_CHECK_FOR_DANGLING_TYPE_SPECIFIER) {
      /* The basic type is a class, struct, union, or enum that actually
         defines a type.  We are especially interested in cases like this:
           class A {...}          <== Note the missing semicolon.
           class B {...};
         where we'd rather report a missing semicolon than a conflict of
         types.  This is referred to as a "dangling type specifier".  Look
         for a type-specifier keyword or a type name.  E.g.,
           class A {...} int...                 <== Dangling type specifier
         Note that this logic works for both C++ and standard C.  We do not
         perform this diagnostic improvement for typedef declarations in GNU C
         mode, because it interferes with the emulation of a GNU bug in that
         case. */
      if (is_type_specifier()) {
        /* The current token is a type keyword; treat it as the start
           of a new declaration.  The error on missing punctuation will be
           handled by the caller. */
        dangling_type_specifier = TRUE;
        goto exit_loop;
      }  /* if */
    }  /* if */
  }  /* for */
#undef record_qualifiers_pos
exit_loop:
  if (!C_mode() &&
      ((decl_specifiers_seen & (DS_INLINE | DS_STORAGE_CLASS)) == 
                               (DS_INLINE | DS_STORAGE_CLASS)) &&
      state->declared_storage_class != (a_storage_class)sc_static &&
      (!extern_inline_allowed ||
       state->declared_storage_class != (a_storage_class)sc_extern)) {
    /* A storage class specifier and "inline" was specified.  In C++, "inline
       static" is allowed; if extern_inline_allowed is TRUE, so is "inline
       extern"; otherwise, we issue an error.  (In C99 mode "inline" can appear
       with both "static" and "extern".) */
    pos_error(ec_bad_storage_class_with_inline, &state->storage_class_pos);
    err = TRUE;
  }  /* if */
  if (state->auto_type_specifier_seen && state->auto_type == NULL &&
      auto_storage_class_specifier_enabled &&
      (auto_type_specifier_enabled || clangcpp_version_is(any_version))) {
    /* The "auto" token was seen among the specifiers, but we could not decide
       if it is a storage class specifier or a type specifier until now.  We do
       not check for decltype(auto) as that would have been processed when it
       was seen. */
    process_auto_specifier((auto_type_allowed && !is_parameter), auto_is_first,
                           input_flags, state, decl_pos_block,
                           &decl_specifiers_seen, &basic_type, type_ptr, &err);
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  /* coverity[dead_error_condition] */
  if (delayed_error != ec_no_error) {
    /* Some GNU C compilers do not diagnose certain invalid specifier
       combinations in typedef declarations that do not include a
       declarator. */
    an_error_severity  sev = (an_error_severity)es_warning;
    if (*storage_class != (a_storage_class)sc_typedef ||
        curr_token != tok_semicolon) {
      /* Either this not a typedef declaration or it's a typedef declaration
         that does include a declarator: Issue an error rather than a
         warning. */
      sev = (an_error_severity)es_error;
      bad_combination_of_type_specifiers = TRUE;
    } else if (c23_mode && is_nullptr_type(state->specifiers_type)) {
      /* Benign redefinition of C23 nullptr_t. */
      sev = es_none;
    }  /* if */
    pos_diagnostic(sev, delayed_error, &pos_delayed_error);
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
  copy_qualifiers(qualifiers, state->qualifiers);
  if (!C_mode() && (ms_extensions || sun_mode) &&
      (decl_specifiers_seen & DS_STORAGE_CLASS)) {
    /* Certain Microsoft-mode diagnostics involving storage class specifiers
       are put off until all the specifiers have been collected.  The same is
       done in Sun mode. */
    if (decl_specifiers_seen & DS_FRIEND) {
      /* "extern" and "static" are permitted on a friend declaration in
         Microsoft and Sun modes -- other storage classes are ignored,
         with a warning. */
      if (*storage_class != (a_storage_class)sc_extern &&
          *storage_class != (a_storage_class)sc_static) {
        pos_warning(ec_storage_class_in_friend_decl,
                    &state->storage_class_pos);
        *storage_class = (a_storage_class)sc_unspecified;
        decl_specifiers_seen &= ~DS_STORAGE_CLASS;
      }  /* if */
    } else if (is_member_decl) {
      /* The diagnostics on invalid storage class were deferred, in case
         this turned out to be a friend declaration instead of a member
         declaration. */
      if (*storage_class != (a_storage_class)sc_typedef &&
          *storage_class != (a_storage_class)sc_static) {
        pos_error(ec_bad_member_storage_class, &state->storage_class_pos);
        *storage_class = (a_storage_class)sc_unspecified;
        decl_specifiers_seen &= ~DS_STORAGE_CLASS;
      }  /* if */
    }  /* if */
  }  /* if */
  if (decl_specifiers_seen == DS_VOID) {
    /* Set the output_flags bit to indicate that the sequence of specifiers
       had just one specifier, and it was "void". */
    *output_flags |= DSO_JUST_VOID;
  } else if (is_elaborated_type_specifier) {
    /* Set the output flag bit indicating that the type specifier is an
       elaborated form (i.e., with the "class", "struct", "union", or
       "enum" keyword).  For friend class declarations this must be done
       even if other specifiers or qualifiers are present. */
    if (((decl_specifiers_seen & DS_FRIEND) && curr_token == tok_semicolon) ||
        (!err && !defines_something &&
         !(decl_specifiers_seen & (DS_STORAGE_CLASS | DS_INLINE |
                                   DS_VIRTUAL | DS_TYPE_QUALIFIER)))) {
      *output_flags |= DSO_ELABORATED_TYPE_SPECIFIER;
      if (basic_type == bt_typename) { *output_flags |= DSO_TYPENAME; }
    }  /* if */
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  if (decl_pos_block != NULL && !any_decl_specifiers_seen &&
     !state->is_linkage_spec_decl) {
    /* No decl-specifiers were seen, so clear the starting position.  If
       there were prefix attributes their starting position is already
       recorded and we keep that, but also record their ending position. */
    an_attribute_ptr  ap = state->prefix_attributes;
    if (ap == NULL) {
      decl_pos_block->specifiers_range.start = null_source_position;
    } else {
      while (ap->next != NULL) ap = ap->next;
      decl_pos_block->specifiers_range.end =
        ap->group != NULL ? ap->group->end_position : ap->end_position;
    }  /* if */
  }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */

  /* Return the position of the first specifier as the position of the
     overall list of specifiers for error purposes. */
  copy_source_position(state->specifiers_pos, error_position);
  if (type_specifier_allowed) {
    if ((basic_type != bt_none && basic_type != bt_no_type) ||
        sign != sign_none || size != size_none) {
      /* Set the flag indicating an explicit type specifier.  Adjectival
         type modifiers (size, sign) are okay, since this is for detecting
         implicit void function return types in pre-ANSI C. */
      *output_flags |= DSO_HAS_EXPLICIT_TYPE_SPECIFIER;
      /* Note that is certain cases a diagnostic is issued to warn the
         user about a missing type specifier.  This is handled by the
         caller. */
    }  /* if */
#if C99_IL_EXTENSIONS_SUPPORTED
    if (complex_attr != cxa_none &&
        basic_type != bt_float && basic_type != bt_double &&
        basic_type != bt_float32 && basic_type != bt_float32x &&
        basic_type != bt_float64 && basic_type != bt_float64x &&
        basic_type != bt_float80 && basic_type != bt_float128 &&
        basic_type != bt_std_float128 && basic_type != bt_bfloat16) {
      /* _Complex and _Imaginary usually require "float" or "double".  GNU
         C mode is an exception: If no type specifier is mentioned,
         "double" is implied.  In some GNU and clang versions, "_Float*"
         types are also accepted. */
      a_boolean  bad_complex_combination = TRUE;
      if (basic_type == bt_none) {
        /* No basic type was specified: Okay in GNU C mode, an error
           otherwise. */
        if (gcc_mode) {
          basic_type = bt_double;
          bad_complex_combination = FALSE;
        } else {
          pos_error(ec_missing_floating_point_type, &error_position);
        }  /* if */
      } else if (basic_type == bt_typedef &&
                 *type_ptr != NULL && type_is(*type_ptr, tk_typeref) &&
                 ((typeref_is_type_operator(*type_ptr) &&
                   gpp_version_is(any_version)) ||
                  (*type_ptr)->variant.typeref.predeclared) &&
                 type_is(skip_typerefs(*type_ptr), tk_float)) {
        basic_type = basic_float_type(skip_typerefs(*type_ptr)->
                                                           variant.float_kind);
        bad_complex_combination = FALSE;
      } else {
        /* An invalid type was specified as the basic type for an
           "_Imaginary" or "_Complex". */
        str_error(ec_only_applies_to_float_types,
                  (char *)((complex_attr == cxa_complex) ? "_Complex"
                                                         : "_Imaginary"));
      }  /* if */
      if (bad_complex_combination) {
        bad_combination_of_type_specifiers = TRUE;
      }  /* if */
      *output_flags |= DSO_HAS_EXPLICIT_TYPE_SPECIFIER;
    }  /* if */
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
    if (dangling_type_specifier) {
      /* Set the bit to mark a malformed type specification, typically
         caused by a missing semicolon following an class, struct, union,
	 or enum declaration.  Error reporting is left to the caller in
         such cases. */
      *output_flags |= DSO_DANGLING_TYPE_SPECIFIER;
    }  /* if */
    if (bad_combination_of_type_specifiers) {
      /* Error has already been diagnosed. */
      *type_ptr = error_type();
      err = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED || SUN_EXTENSIONS_ALLOWED
    } else if (*type_ptr == NULL && basic_type == bt_struct_union) {
      /* A friend declaration of the form "friend class X;" where "X" is a
         class template. */
      check_assertion((ms_extensions || sun_mode) &&
                      (decl_specifiers_seen & DS_FRIEND));
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || SUN_EXTENSIONS_ALLOWED */
#if VLA_ALLOWED
    } else if (vla_enabled && inside_local_class && *type_ptr != NULL &&
               is_nonlocal_variably_modified_type(*type_ptr)) {
      /* In C++ mode with VLAs enabled, a local class could contain a reference
         to a variably-modified type in the enclosing function.  We cannot
         accept this since it implies that the local class accesses local
         storage in the enclosing function scope: Issue an error. */
      pos_error(ec_nonlocal_vla_not_allowed, &error_position);
      *type_ptr = error_type();
      err = TRUE;
#endif /* VLA_ALLOWED */
    } else {
      /* Combine the type specifiers (except for the type qualifiers) into a
         type.  *type_ptr is updated, based on the basic type, sign, and size
         specified. */
      if (!combine_type_specifiers(state, basic_type, sign, size,
                                   bit_precise_width_con, complex_attr,
                                   saturating_fixed_point)) {
        err = TRUE;
      } else {
#if MICROSOFT_EXTENSIONS_ALLOWED
        a_type_ptr  tp = *type_ptr;
        if (tp != NULL) tp = skip_lexical_typerefs(tp);
        if (cli_or_cx_enabled && tp != NULL && is_immediate_class_type(tp) &&
            cli_class_type_kind_is(tp, cctk_value)) {
          /* Naming a special value class like System::Int32 is equivalent to
             denoting the corresponding standard type (e.g., int).  It may
             later be switched back to the System value type, e.g. if a handle
             to an int is formed (but not e.g. if a tracking reference to an
             int is formed). */
          a_type_ptr  standard_type = fundamental_type_from_system_type(tp);
          if (standard_type != NULL) *type_ptr = standard_type;
        }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        /* Add any type qualifiers (const, volatile, etc.) to the type. */
        if (!add_type_qualifiers(type_ptr, state)) {
          err = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else if (microsoft_w64_seen) {
          check_assertion(type_ptr != NULL && *type_ptr != NULL);
          apply_microsoft_w64_specifier(type_ptr, &microsoft_w64_pos);
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        }  /* if */
      }  /* if */
    }  /* if */
#if UPC_EXTENSIONS_ALLOWED
  } else if (upc_mode && !err) {
    /* The UPC strict and relaxed qualifiers can only appear combined with
       the shared qualifier. */
    if ((qualifiers & (TQ_UPC_STRICT | TQ_UPC_RELAXED)) != 0 &&
        (qualifiers & TQ_UPC_SHARED) == 0) {
      pos_error(ec_nonshared_strict_relaxed, &error_position);
      err = TRUE;
    }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
  }  /* if */
  /* If there was an error, assume something was declared.  Who knows what the
     correct code should have done. */
  if (err || declares_something) *output_flags |= DSO_DECLARES_SOMETHING;
  if (defines_something) *output_flags |= DSO_DEFINES_SOMETHING;
#if DEBUG
/* GCOVR_EXCL_START */
  if (debug_level >= 3) {
    fputs("type_ptr: ", f_debug);
    if (*type_ptr == NULL) {
      fputs("<null>", f_debug);
    } else {
      db_type(*type_ptr);
    }  /* if */
    (void)fputc('\n', f_debug);
  }  /* if */
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */
  state->storage_class = state->declared_storage_class;
  state->decl_specifiers_error = err;
  attach_specifier_attributes(state);
  /* state->type and state->declared_type may get updated by a subsequent call
     to declarator(...) or by other adjustments (e.g., decay of array types to
     pointer types). */
  state->type = state->declared_type = state->specifiers_type;
  if ((*output_flags & DSO_NO_DECL_SPECIFIERS) &&
      !state->is_linkage_spec_decl &&
      state->prefix_attributes == NULL &&
      state->specifier_attributes == NULL) {
    /* Note that for the purposes of diagnostic, something like
       ``extern "C" f();'' is treated as having a decl-specifier (hence the
       test for !state->is_linkage_spec_decl). */
    state->decl_specifiers_omitted = TRUE;
  }  /* if */
  db_exit();
}  /* decl_specifiers */

#if MICROSOFT_EXTENSIONS_ALLOWED

void scan_microsoft_secondary_decl_specifiers(
                                         a_decl_flag_set       input_flags,
                                         a_decl_parse_state    *state,
                                         a_decl_pos_block_ptr  decl_pos_block)
/*
This is a wrapper function for decl_specifiers(...), to handle the scanning
of Microsoft C++ mode decl-specifiers appearing after a comma separating
multiple declarators (we call these "secondary specifiers").  Many specifiers
result in an error in this context, and many others (including cv-qualifiers)
are ignored with a warning.  "Primary cv-qualifiers" are preserved, however.
For example:
    int i1, char const* s1;       // s1 has type char*
    int const i2, char* s2;       // s2 has type char const*
    int i3, extern char* s3;      // "extern" is ignored
    int i4, extern "C" char* s4;  // syntax error
This Microsoft extension/bug is sometimes used in "for" statements:
    for (char *p = s, int k = 0; ...

See decl_specifiers(...) for the meaning of the parameters.
*/
{
  a_type_qualifier_set    saved_qualifiers = state->qualifiers;
  a_source_position       pos;

  check_assertion(ms_extensions && !C_mode());
  pos = pos_curr_token;
  input_flags &= ~(DSI_INLINE_ALLOWED | DSI_ASM_ALLOWED |
                   DSI_EMPTY_DECL_SPECIFIERS_ALLOWED);
  input_flags |= DSI_MICROSOFT_SECONDARY_SPECIFIERS;
  decl_specifiers(input_flags, state, decl_pos_block);
  /* Restore the primary cv-qualifiers: */
  copy_qualifiers(saved_qualifiers, state->qualifiers);
  (void)add_type_qualifiers(&state->specifiers_type, state);
  state->type = state->specifiers_type;
  /* Issue a warning in most cases, but if a class or enumeration type was
     defined make it an error. */
  pos_diagnostic((state->dso_flags & DSO_DEFINES_SOMETHING) ? es_error
                                                            : es_warning,
                 ec_nonstandard_secondary_decl_specifiers, &pos);
}  /* scan_microsoft_secondary_decl_specifiers */

#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

void decl_spec_init(void)
/*
Initialize variables related to the processing of decl-specifiers that must be
initialized for each compilation.
*/
{
#if !STANDALONE_UTILITY_PROGRAM
  c_tagged_type_map = alloc_fe_of_type(a_c_tagged_type_map);
  construct(c_tagged_type_map, /*mask_width=*/6u);
#endif /* STANDALONE_UTILITY_PROGRAM */
}  /* decl_spec_init */


void decl_spec_one_time_init(void)
/*
Do one-time initialization of variables related to the processing of
decl-specifiers.
*/
{
  if (!enum_types_can_be_larger_than_int) {
    largest_enum_int_kind = (an_integer_kind)ik_int;
  } else {
#if LONG_LONG_ALLOWED
    if (!strict_ansi_mode || long_long_is_standard) {
      largest_enum_int_kind = (an_integer_kind)ik_unsigned_long_long;
    } else
#endif /* LONG_LONG_ALLOWED */
    /* Do not insert code here. */
    {
      largest_enum_int_kind = (an_integer_kind)ik_unsigned_long;
    }  /* if */
  }  /* if */

  /* Save variables from decl_spec.c that are needed for precompiled
     headers */
  if (precompiled_header_processing_required) {
    STATIC_THREAD a_pch_saved_variable saved_vars[] = {
#if MICROSOFT_EXTENSIONS_ALLOWED
      pch_saved_var_array_elem(unresolved_type_map),
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
      pch_saved_var_array_elem(c_tagged_type_map),
      pch_saved_var_array_elem(largest_enum_int_kind),
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  /* Verify that tok_auto has at least one meaning. */
  check_assertion(auto_storage_class_specifier_enabled ||
                  auto_type_specifier_enabled);
}  /* decl_spec_one_time_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

