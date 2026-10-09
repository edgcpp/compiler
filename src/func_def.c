/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

func_def.c -- Processing for function definitions (both user supplied and
              compiler generated).

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
#include "class_decl.h"
#include "exprutil.h"
#if DO_IL_LOWERING
#include "il_walk.h"
#include "lower_il.h"
#endif /* DO_IL_LOWERING */
#if MICROSOFT_EXTENSIONS_ALLOWED
#include "ms_attrib.h"
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#include "statements.h"
#include "layout.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/* Forward declaration: */
static void define_special_member_function(a_routine_ptr rout_ptr);

#if ASM_FUNCTION_ALLOWED

static char *scan_asm_function(void)
/*
Scan the text between the opening and closing brace of an asm
function.  Proceed token by token until the matching right brace is
found.  For each token, copy the source text directly into the asm
function body buffer; when the entire text of the asm function body
has been copied, a string of appropriate size is allocated in the
memory region of the asm function and the buffer is copied to it.
Comments in asm functions are saved along with the normal tokens.
*/
{
  unsigned int     nbrace = 1;
  char             *body;

  db_enter(3, "scan_asm_function");
  /* Initialize variables used for building the string. */
  reset_asm_buffer();
  /* Initialize global variables used by lexical routines. */
  in_asm_function_body = TRUE;
  treat_newline_as_token = TRUE;
  fetch_pp_tokens = TRUE;
  /* Advance past the opening brace. */
  if (curr_token == tok_lbrace) {
    (void)get_token();
  } else {
    pos_error(ec_exp_lbrace, &pos_curr_token);
  }  /* if */
  /* Loop through the tokens and build the string token by token. */
  while (curr_token != tok_end_of_source) {
    /* Stop when a zero-level right brace is reached.
       Keep track of braces. */
    if (curr_token == tok_rbrace && --nbrace == 0) {
      /* This right brace matches the opening left brace, marking the end of
         the asm function body.  Copy white space up to the current token. */
      if (start_of_curr_token != NULL) {
        copy_from_source_to_asm_func_buffer(start_of_curr_token, (char *)NULL);
      } else {
        /* We can get here in severe error situations that caused tokens to be
           prematurely cached. */
        check_assertion(is_at_least_one_error());
      }  /* if */
      break;
    }  /* if */
    /* Special handling for a left brace embedded within the assembler
       code: assume it has a matching right brace. */
    if (curr_token == tok_lbrace) ++nbrace;
    /* Copy characters from the source line to the buffer, from
       last_stop_char through the end of the current token. */
    if (end_of_curr_token != NULL) {
      copy_from_source_to_asm_func_buffer(end_of_curr_token + 1, (char *)NULL);
    } else {
      /* We can get here in severe error situations that caused tokens to be
         prematurely cached. */
      check_assertion(is_at_least_one_error());
    }  /* if */
    /* Advance to the next token. */
    (void)get_token();
  }  /* while */
  fetch_pp_tokens = FALSE;
  in_asm_function_body = FALSE;
  treat_newline_as_token = FALSE;
  /* Allocate a block of the current IL memory region (the one established
     for the asm function) -- the asm buffer will be copied into it, along
     with a trailing null character. */
  body = alloc_asm_function_body(pos_in_asm_func_body_buffer + 1);
  (void)memcpy(body, asm_func_body_buffer,
               size_t_arg(pos_in_asm_func_body_buffer));
  /* Add a null terminator. */
  body[pos_in_asm_func_body_buffer] = '\0';
#if DEBUG
  if (debug_level >= 3 || db_flag_is_set("asm_function")) {
    fprintf(f_debug, "asm block: %s\n", body);
  }  /* if */
#endif /* DEBUG */
  db_exit();
  return body;
}  /* scan_asm_function */

static a_statement_ptr scan_asm_function_body(void)
/*
Scan the body of an asm function.  An stmk_asm_func_body statement is
returned to the caller.
*/
{
  a_statement_ptr    stmt;

  db_enter(3, "scan_asm_function_body");
  stmt = alloc_statement(stmk_asm_func_body, /*compiler_generated=*/FALSE);
  stmt->position = pos_curr_token;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  add_to_source_sequence_list((char *)stmt, (an_il_entry_kind)iek_statement);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  stmt->variant.asm_func_body = scan_asm_function();
  db_exit();
  return stmt;
}  /* scan_asm_function_body */

#endif /* ASM_FUNCTION_ALLOWED */

static void require_definitions_of_virtual_functions_on_routine_list(
                                                         a_type_ptr class_type)
/*
Require definitions for the virtual functions of the indicated class.
*/
{
  /* If this is a template class, instantiate all the virtual member
     functions.  When instantiating extern inline functions in a way similar
     to templates, do this for all classes. */
  if (class_type->variant.class_struct_union.any_virtual_functions) {
    /* Look for virtual functions on the class routines list. */
    a_routine_ptr  rp = class_type_supp(class_type)->assoc_scope->routines;
#if DO_IL_LOWERING && MAINTAIN_NEEDED_FLAGS
    a_boolean      keep_class_def = FALSE;
#endif /* DO_IL_LOWERING && MAINTAIN_NEEDED_FLAGS */
    for (; rp != NULL; rp = rp->next) {
      if (rp->is_virtual && !rp->pure_virtual) {
#if DO_IL_LOWERING && MAINTAIN_NEEDED_FLAGS
        keep_class_def = TRUE;
#endif /* DO_IL_LOWERING && MAINTAIN_NEEDED_FLAGS */
#if IA64_ABI && DO_IL_LOWERING
        /* Secondary entry points of constructors and destructors should
           not get here. */
        check_assertion(rp->primary_ctor_or_dtor == NULL);
#endif /* IA64_ABI && DO_IL_LOWERING */
        /* The function could be called, so mark it to be instantiated.  Note
           that the defer-inline flag is important here to prevent the actual
           instantiation of these functions from occurring earlier than is
           absolutely necessary. */
        if (rp->is_defaulted ||
            (rp->compiler_generated &&
             special_kind_is(rp, sfk_operator) &&
             (opname_kind_is(rp, onk_assign) || opname_kind_is(rp, onk_eq)))) {
          /* Generate bodies for defaulted virtual functions and for
             implicitly-declared assignment operators that override a
             virtual operator= as well as equality operators implied by a
             virtual defaulted three-way comparison operator.  Virtual
             destructors are handled separately, subject to the vtable-decider
             function. */
          force_definition_of_compiler_generated_routine(rp);
        }  /* if */
        if (instantiate_extern_inline ||
            is_unspecialized_template_member_function(rp)) {
          a_symbol_ptr sym = symbol_for(rp);
          /* Set the instantiation_required flag for the virtual function or
             potentially-inline function (functions can be marked inline after
             they have been declared: do not test the is_inline flag). */
          set_instance_required(sym, /*value=*/TRUE, SIR_DEFER_INLINE);
        }  /* if */
      }  /* if */
    }  /* for */
#if DO_IL_LOWERING && MAINTAIN_NEEDED_FLAGS
    if (keep_class_def) {
      /* Force the class definition to be kept, because if it is removed the
         virtual function table variable will be detached, and later the
         instance-required flag will be cleared on the virtual functions of
         the class because there is no virtual function table. */
      set_class_keep_definition_in_il(class_type);
    }  /* if */
#endif /* DO_IL_LOWERING && MAINTAIN_NEEDED_FLAGS */
  }  /* if */
}  /* require_definitions_of_virtual_functions_on_routine_list */


static void r_require_definitions_of_virtual_functions_in_class(
                                                         a_type_ptr class_type)
/*
Helper routine for require_definitions_of_virtual_functions_in_class
to handle the recursive walk through base classes.
*/
{
  if (!class_type->variant.class_struct_union.
                                        virtual_functions_marked_as_required &&
      class_type->variant.class_struct_union.
                             any_virtual_functions_including_in_base_classes) {
    a_base_class_ptr bcp;
    a_class_type_supplement_ptr
                     ctsp = class_type->variant.class_struct_union.extra_info;

    class_type->variant.class_struct_union.
                                   virtual_functions_marked_as_required = TRUE;
    /* Loop through the routines list and check the virtual functions. */
    require_definitions_of_virtual_functions_on_routine_list(class_type);
    /* Do the same for base class virtual functions. */
    for (bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
      /* Handle only direct base classes, because the recursive call
         will handle that class's base classes. */
      if (bcp->direct) {
        r_require_definitions_of_virtual_functions_in_class(bcp->type);
      }  /* if */
    }  /* for */
  }  /* if */
}  /* r_require_definitions_of_virtual_functions_in_class */


static void define_virtual_generated_dtor_if_needed(a_type_ptr  class_type)
/*
If the given class has a virtual compiler-generated destructor, force the
definition of the latter if needed (i.e., if no decider function is known to
trigger its definition elsewhere).
*/
{
  a_symbol_ptr  dtor_sym =
                        class_symbol_supp(symbol_for(class_type))->destructor;

  if (dtor_sym != NULL) {
    a_routine_ptr  dtor = dtor_sym->variant.routine.ptr;
    if ((dtor->compiler_generated || dtor->is_defaulted) && dtor->is_virtual &&
        !routine_has_been_defined(dtor)) {
      /* A virtual generated destructor that hasn't been defined yet. */
      a_boolean  generate = FALSE;
#if IA64_ABI && ABI_COMPATIBILITY_VERSION >= 410
      if (class_type->variant.class_struct_union.is_template_class &&
          !class_type->variant.class_struct_union.is_specialized) {
        /* For instantiated classes, we cannot rely on a definition of a
           "decider" function since it would itself require instantiation.
           So generate the destructor unconditionally. */
        generate = TRUE;
      } else
#endif /* IA64_ABI && ABI_COMPATIBILITY_VERSION >= 410 */
      /* Do not insert code here. */
      {
        /* Check the decider function (if any). */
        a_routine_ptr decider = vtbl_decider_function_for_class(
                                                          class_type,
                                                          (a_boolean *)NULL);
        if (decider != NULL && !routine_has_been_defined(decider)) {
          /* The vtable is not being put out in this compilation, so don't
             force the definition of the destructor here.  If the decider
             function gets defined later, we'll get back to this code and
             decide at that point to put out the destructor definition. */
        } else {
          generate = TRUE;
        }  /* if */
      }  /* if */
      if (generate) {
        define_special_member_function(dtor);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* define_virtual_generated_dtor_if_needed */


void require_definitions_of_virtual_functions_in_class(a_type_ptr class_type)
/*
Require definitions for all virtual functions in class_type (including
those from its base classes that are not overridden).  This includes
virtual destructors and instantiatable functions.  The definitions
are required in the overall program, not necessarily in the current
compilation.
*/
{
  class_type = skip_typerefs(class_type);
  if (class_type->variant.class_struct_union
                         .any_virtual_functions_including_in_base_classes) {
    /* If this class has a virtual implicitly-generated destructor its body
       may have to be generated, too.  This is typically done only at the
       top level because virtual destructors in base classes would be
       overridden and therefore would not be pointed to from the virtual
       function table in the derived class.  However, Clang and early
       versions of GCC also have construction vtables point to the
       destructors between the most derived class and any virtual base
       classes (in the IA-64 ABI). */
    define_virtual_generated_dtor_if_needed(class_type);
#if IA64_ABI
    if (((gpp_mode && gnu_version < 40900) || clang_mode) &&
        class_type->variant.class_struct_union.any_virtual_base_classes) {
      /* Look for virtual base classes and define generated virtual destructors
         on its (possibly multiple) derivation paths (not including the virtual
         base itself).  For example:
           struct A { virtual ~A(); };
           struct B: virtual A {};
           struct C: B { virtual ~C() ;  };
           struct D: public C { virtual ~D(); };
           D::~D() {}  // Decider function triggers vtable generation.
         Here, the body of B::~B() is generated by Clang and some versions of
         GCC. */
      a_base_class_ptr  bcp;
      for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
        if (bcp->is_virtual) {
          a_base_class_derivation_ptr  der;
          for (der = bcp->derivation; der != NULL; der = der->next) {
            a_derivation_step_ptr  step = der->path;
            for (; !step->base_class->is_virtual; step = step->next) {
              define_virtual_generated_dtor_if_needed(step->base_class->type);
            }  /* for */
          }  /* for */
        }  /* if */
      }  /* for */
    }  /* if */
#endif /* IA64_ABI */
    r_require_definitions_of_virtual_functions_in_class(class_type);
  }  /* if */
}  /* require_definitions_of_virtual_functions_in_class */


static a_boolean is_explicit_instantiation_to_be_ignored(
                                              ARG_UNUSED a_routine_ptr routine)
/*
Return TRUE if routine was explicitly instantiated and we are in an ABI
where that should suppress the generation of the vtable.

Other compilers (g++, clang, Microsoft) do not emit vtables if the decider
function is explicitly instantiated, so we do likewise in the IA-64 ABI.
When the cfront ABI is used, we have to emit the vtable because it won't
be emitted elsewhere.
*/
{
#if IA64_ABI
  return routine->explicit_instantiation;
#else /* !IA64_ABI */
  return FALSE;
#endif /* IA64_ABI */
}  /* is_explicit_instantiation_to_be_ignored */


static a_boolean virtual_functions_needed_due_to_definition_of(
                                                         a_routine_ptr routine)
/*
Return TRUE if definitions of virtual functions of the class of which the
indicated routine is a member are needed (somewhere in the program, but
not necessarily in the current compilation).  The definition of the
indicated routine has just been processed.
*/
{
  a_boolean  needed = FALSE;
  a_type_ptr class_type = parent_class_of(routine);

  if (class_type->variant.class_struct_union.
                             any_virtual_functions_including_in_base_classes) {
    if (routine->special_kind == (a_special_function_kind)sfk_constructor ||
        routine->special_kind == (a_special_function_kind)sfk_destructor) {
      /* Constructor and destructor wrappers refer to the virtual function
         table and therefore the virtual functions are needed. */
      needed = TRUE;
    } else if (routine->is_virtual &&
               !is_explicit_instantiation_to_be_ignored(routine)) {
      a_routine_ptr decider = vtbl_decider_function_for_class(
                                                            class_type,
                                                            (a_boolean *)NULL);
      if (decider != NULL ?
                    (decider == routine ||
                     (routine_has_been_defined(decider) &&
                      !is_explicit_instantiation_to_be_ignored(decider))) :
                    routine->considered_decider_function_at_some_point) {
        /* This routine is the decider function for definition of the
           virtual function table.  Since it's defined, the virtual function
           table definition will be put out in this compilation and therefore
           the virtual functions are needed. */
        /* Also mark the virtual functions as needed if the decider function
           was previously defined at a point when it wasn't known to be
           the decider function, or was thought to be the decider function
           but is now known not to be.  This can happen only for ABIs (like
           the ARM EABI; see TARG_IA64_ABI_VARIANT_KEY_FUNCTION) where the
           decider function can be altered by an out-of-class definition
           that specifies "inline" for a function that otherwise would have
           been considered to be the decider function. */
        needed = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  return needed;
}  /* virtual_functions_needed_due_to_definition_of */


static void require_definitions_of_virtual_functions_due_to_definition_of(
                                                         a_routine_ptr routine)
/*
The indicated routine (a member function) has just been defined.  If that
implies that definitions of virtual functions of the routine's class are
needed, go through and process the virtual functions accordingly.  Note that
the definitions are required in the overall program, not necessarily in the
current compilation.
*/
{
  if (virtual_functions_needed_due_to_definition_of(routine)) {
    a_type_ptr class_type = parent_class_of(routine);
    require_definitions_of_virtual_functions_in_class(class_type);
    if (routine->considered_decider_function_at_some_point &&
        class_type->used_in_exception_or_rtti) {
      force_definition_of_typeinfo_for(class_type);
    }  /* if */
  }  /* if */
}  /* require_definitions_of_virtual_functions_due_to_definition_of */


a_boolean check_function_return_type(a_type_ptr         rout_type,
                                     a_source_position  *diag_pos,
                                     a_boolean          is_expr_use,
                                     a_boolean          evaluated,
                                     a_boolean          incomplete_return_okay,
                                     a_routine_ptr      rout_ptr)
/*
Given a routine type, return TRUE if the return type is valid and FALSE
otherwise (add_to_derived_type_list already performed checks not repeated
here).  If diag_pos is non-NULL, issue diagnostics at that position.
is_expr_use is TRUE if the function is being called or its address is being
taken; otherwise, the function is being defined (nondefining declarations are
checked by add_to_derived_type_list).  When is_expr_use is TRUE, evaluated is
TRUE if the expression is in an evaluated context.  incomplete_return_okay is
TRUE if no error should be issued for an incomplete (non-void) return type.
rout_ptr is a pointer to the routine that is being defined or called; may be
NULL.
*/
{
  a_type_ptr  return_type;
  a_boolean   err = FALSE;
  a_boolean   issue_incomplete_type_error = FALSE;
  a_type_ptr  orig_return_type;

  rout_type = skip_typerefs(rout_type);
  orig_return_type = rout_type->variant.routine.return_type;
  return_type = skip_typerefs(orig_return_type);
  /* 3.7.1, constraints: The return type of a function shall be void
     or an object type other than array.  See also the constraints of
     3.5.4.3 on function declarators, enforced previously by
     add_to_derived_type_list.  In addition, a reference type (including a
     reference to an array or function) may also be returned (ARM 8.2.5). */
  if (is_void_type(return_type)) {
    if (is_qualified_type(orig_return_type) && !is_expr_use &&
        C_mode() && strict_ansi_mode) {
      /* In strict C mode a void return type on a function definition cannot
         have a qualifier. */
      err = (strict_ansi_error_severity == es_error);
      if (diag_pos != NULL) {
        diagnostic(strict_ansi_error_severity,
                   ec_type_qualifier_on_void_return_type);
      }  /* if */
    } else {
      /* Okay. */
    }  /* if */
  } else if (is_error_type(return_type)) {
    /* No diagnostic this time. */
  } else if (!incomplete_return_okay) {
    /* If return_type is an uninstantiated template class, force its
       instantiation. */
    complete_type_is_needed(return_type);
    if (is_expr_use) {
      /* The type check is simpler on function calls, because function and
         array types have already been filtered out. */
      check_assertion(!is_array_type(return_type) &&
                      !is_function_type(return_type));
      if (is_incomplete_type(return_type)) {
        if ((microsoft_bugs && !evaluated &&
             is_immediate_class_type(return_type)) ||
            (!strict_ansi_mode &&
             ((rout_ptr != NULL && rout_ptr->is_prototype_instantiation) ||
              in_generic_lambda_in_prototype_instantiation()))) {
          /* MSVC++ allows a function call returning an incomplete class type
             in a not-evaluated context.  Also, it is common practice not to
             diagnose calls to functions with incomplete return types if the
             called function is a prototype instantiation (verified with GCC
             and Clang) or if the call appears in a generic-lambda prototype
             instantiation (verified with GCC); in nonstrict modes, we
             therefore just issue a warning as well. */
          if (diag_pos != NULL) {
            pos_ty_warning(ec_incomplete_class_return_type, diag_pos,
                           orig_return_type);
          }  /* if */
        } else {
          if (diag_pos != NULL) {
            a_routine_type_supplement_ptr  rtsp = rout_type->
                                                 variant.routine.extra_info;
            if (!rtsp->suppress_diagnostic_on_incomplete_return_type) {
              /* If a diagnostic has already been issued on calling (or taking
                 the address of) this routine, there is no need to do it
                 again. */
              issue_incomplete_type_error = TRUE;
            }  /* if */
            rtsp->suppress_diagnostic_on_incomplete_return_type = TRUE;
          }  /* if */
          /* Note that err is set (for the return value) even if no diagnostic
             is actually issued. */
          err = TRUE;
        }  /* if */
      } else if ((relaxed_abstract_checking || gpp_version_is(< 50000)) &&
                 is_immediate_class_type(return_type) &&
                 return_type->variant.class_struct_union.abstract) {
        /* In relaxed abstract checking (P0929R2) mode, as well as in older
           g++ modes, we do not check for abstract return types on
           non-defining declarations, so we must do so on calls. */
        if (diag_pos != NULL) {
          abstract_class_diagnostic(
                             es_error, ec_function_returning_abstract_class,
                             orig_return_type, diag_pos);
        }  /* if */
        err = TRUE;
      }  /* if */
    } else {
      /* Declaration case. */
      if ((is_complete_object_type(return_type) &&
           !is_array_type(return_type)) ||
          is_any_reference_type(return_type)) {
        /* err = FALSE; */
        if (relaxed_abstract_checking &&
            is_immediate_class_type(return_type) &&
            return_type->variant.class_struct_union.abstract) {
          /* In relaxed abstract checking (P0929R2) mode, we only check for
             abstract return types on function definitions and calls.  In
             other C++ modes, this is done whenever a function type is
             created and thus should not be repeated here. */
          if (diag_pos != NULL) {
            abstract_class_diagnostic(
                               es_error, ec_function_returning_abstract_class,
                               orig_return_type, diag_pos);
          }  /* if */
          err = TRUE;
        }  /* if */
      } else {
        err = TRUE;
        if (diag_pos != NULL) {
          if (is_immediate_class_type(return_type) &&
              is_incomplete_type(return_type)) {
            issue_incomplete_type_error = TRUE;
          } else {
            pos_error(ec_bad_function_return_type, diag_pos);
            rout_type->variant.routine.return_type = error_type();
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    if (issue_incomplete_type_error) {
      report_incomplete_function_return_type(orig_return_type, diag_pos,
                                             rout_ptr);
    }  /* if */
  }  /* if */
  return !err;
}  /* check_function_return_type */


static a_variable_ptr make_param_variable(a_type_ptr       type_ptr,
                                          a_storage_class  storage_class)
/*
Allocate a variable entry with type type_ptr, set some of its fields, and
return a pointer to it.
*/
{
  a_variable_ptr vp;

  check_assertion(type_ptr != NULL);
  vp = make_variable(type_ptr, storage_class, NO_SCOPE_DEPTH);
#if GENERATE_SOURCE_SEQUENCE_LISTS
  vp->declared_type = type_ptr;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  vp->is_parameter = TRUE;
  /* Parameter variables are always "local". */
  vp->source_corresp.is_local_to_function = TRUE;
  return(vp);
}  /* make_param_variable */


static a_variable_ptr make_implicit_this_param_variable(a_type_ptr  rout_type)
/*
Create a variable entry for an implicit-this parameter, using the indicated
routine type, and return a pointer to it.
*/
{
  a_variable_ptr vp;
  a_type_ptr     this_type = f_implicit_this_param_type_of(rout_type);

  this_type = make_qualified_type(this_type, TQ_CONST);
  vp = make_param_variable(this_type, (a_storage_class)sc_auto);
  vp->is_this_parameter = TRUE;
  set_parent_scope(&vp->source_corresp, iek_variable,
                   innermost_function_scope);
  return vp;
}  /* make_implicit_this_param_variable */


static void attach_param_variable_attributes(a_variable_ptr  vp)
/*
The given variable is a parameter variable.  If any attributes specified on
the parameter really apply to the underlying variable, apply them to the
variable.
*/
{
  a_param_type_ptr  ptp = vp->variant.assoc_param_type;

  if (ptp->attributes != NULL) {
    an_attribute_ptr  vap = get_param_variable_attr_copies(ptp);
    attach_attributes(vap, (char*)vp, iek_variable);
  }  /*  */
}  /* attach_param_variable_attributes */


static void decl_parameter(a_param_id_ptr        param_id,
                           ARG_UNUSED a_type_ptr declared_type,
                           a_param_type_ptr      ptp,
                           a_boolean             function_instantiation)
/*
Enter the declaration of an identifier for a parameter.  The param_id
points to an sk_parameter symbol, which under ordinary circumstances, is
turned into an sk_variable symbol; but if function_instantiation is TRUE,
a new symbol is created and entered in the symbol table.  When declared
types are recorded, declared_type points to the type of this parameter as
it was originally declared (before any transformations such as array-to-
pointer decay).
*/
{
  a_symbol_ptr      sym;
  a_variable_ptr    vp;
  a_type_ptr        tp, utp;
  a_symbol_locator  locator;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_boolean         is_real_instantiation = function_instantiation &&
                   !scope_stack[depth_scope_stack].in_prototype_instantiation;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

  db_enter(3, "decl_parameter");
  /* Choose the type to use, the one in the param-type entry or the one in
     the param-id entry.  Usually, they will be the same. */
  if (function_instantiation) {
    /* For template functions being instantiated the type pointed to by the
       param-id may include a template parameter, so use the type in
       param-type entry, which will be the result of the template arg
       substitution. */
    tp = ptp->type;
    if (remove_qualifiers_from_param_types) {
      /* If top-level qualifiers were stripped off the param-type type,
         restore them in the parameter variable's type (unless the top-level
         qualifiers were on an array type, because in that case the qualifier
         is no longer "top level" after array type decay). */
      if (param_id->eff_top_level_cv_quals != TQ_NONE &&
          !is_array_type(ptp->declared_type)) {
        ptp->qualifiers |= param_id->eff_top_level_cv_quals;
      }  /* if */
      if (ptp->qualifiers != TQ_NONE) {
        tp = make_qualified_type(tp, ptp->qualifiers);
      }  /* if */
    }  /* if */
  } else {
    /* In cases other than template instantiations, use the param-id type,
       since it will be the one actually used in the function definition,
       whereas the type in the param-type entry may be a composite type, as
       in the following example:
         void f(int a[3]);
         void f(a) int a[]; { ... }
       In the second declaration the param-id type is int[], but the composite
       type produced for the routine's interface is int[3]. */
    tp = param_id->type;
#if CHECKING
    if (remove_qualifiers_from_param_types) {
      /* A top-level type qualifier may have been stripped off.  The type
         qualifier has been recorded in the param type entry; it should
         correspond to the parameter variable's type qualifier.  It is also
         possible that top-level qualifiers were present on a guiding
         declaration, but not on the corresponding template declaration. */
      check_assertion(guiding_decls_allowed ||
                      ptp->qualifiers == get_type_qualifiers(param_id->type));
    }  /* if */
#endif /* CHECKING */
  }  /* if */
  utp = skip_typerefs(tp);
  if (is_immediate_class_type(utp)) {
    complete_type_is_needed(utp);
#if REF_DESTRUCTORS_FOR_PARAMETER_VARIABLES
    if (class_symbol_supp(symbol_for(utp))->destructor != NULL) {
      a_symbol_ptr   dtor_sym = class_symbol_supp(symbol_for(utp))->destructor;
      a_routine_ptr  dtor = dtor_sym->variant.routine.ptr;
      mark_routine_referenced(dtor);
    }  /* if */
#endif /* REF_DESTRUCTORS_FOR_PARAMETER_VARIABLES */
  }  /* if */
  if (utp->incomplete) {
    /* Incomplete type is not allowed. */
    issue_incomplete_type_diag(&param_id->type_pos, utp);
    tp = ptp->type = error_type();
  }  /* if */
  /* Create the parameter variable. */
  if (param_id->is_parameter_pack && ptp->param_num == 0) {
    /* This is a "dummy" parameter produced for an empty parameter pack
       expansion. */
    vp = make_variable(type_of_unknown_templ_param_nontype, sc_static,
                       depth_scope_stack);
    vp->source_corresp.is_local_to_function = TRUE;
    vp->compiler_generated = TRUE;
  } else {
    vp = make_param_variable(tp, param_id->storage_class);
    add_to_parameters_list(vp);
  }  /* if */
  vp->is_pack = ptp->is_parameter_pack;
  vp->is_pack_element = ptp->is_pack_element;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  /* Record the type exactly as it was declared (before array-to-pointer
     decay, etc.). */
  vp->declared_type = declared_type;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if VLA_ALLOWED
  vp->has_variably_modified_type = (vla_enabled &&
                                    is_variably_modified_type(tp));
#endif /* VLA_ALLOWED */
  sym = param_id->symbol;
  if (gnu_mode && sym != NULL && sym->ambiguous) {
    /* In GNU C and C++ mode, a duplicate parameter name is only diagnosed in
       function definitions.  Any duplicate parameters were marked "ambiguous"
       when parsing the function declarator.  Issue an error and treat them
       as unnamed parameters in what follows. */
    pos_error(ec_dupl_param_name, &sym->decl_position);
    sym = NULL;
  }  /* if */
  if (sym == NULL) {
    /* This param_id entry represents an unnamed parameter (which is legal
       in function definitions in C++). */
    /* Clear the referenced flag, which is set by set_default_source_corresp
       when the variable is allocated. */
    vp->source_corresp.referenced = FALSE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
#if CHECKING
    if (!source_sequence_entries_disallowed &&
        depth_innermost_instantiation_scope == NO_SCOPE_DEPTH &&
        depth_template_declaration_scope == NO_SCOPE_DEPTH) {
      /* In nontemplate contexts, a source sequence entry should have been
         generated, but some template-related syntax errors may cause us to
         lose track of the fact that we're in a template context. */
      check_assertion(param_id->source_sequence_entry != NULL ||
                      is_at_least_one_error());
    }  /* if */
#endif /* CHECKING */
    update_source_sequence_list((char *)vp, (an_il_entry_kind)iek_variable,
                                param_id->source_sequence_entry);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (param_id->type_pos.seq != 0) {
      /* Since set_source_corresp is not called for unnamed entities, create
         the associated decl-pos supplement directly. */
      vp->source_corresp.decl_pos_info =
                     alloc_decl_position_supplement(in_file_scope(vp));
    }  /* if */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  } else {
    make_locator_for_symbol(sym, &locator);
    if (function_instantiation) {
      sym = enter_local_symbol((a_symbol_kind)sk_variable, &locator,
                               decl_scope_level,
                               /*suppress_redecl_error=*/FALSE);
      if (param_id->uses_only_enclosing_pack) {
        /* A parameter that uses an enclosing pack is a pack expansion. */
        sym->is_pack_expansion = TRUE;
      }  /* if */
    } else {
      set_symbol_kind(sym, (a_symbol_kind)sk_variable);
      /* In some modes, the parameter symbols (in the prototype scopes) are
         invisible.  Ensure that they will be visible when copied to the
         function scope.  In GNU modes, a parameter with a name that is a
         duplicate of an earlier parameter is both "ambiguous" and "invisible":
         That case is not made visible here. */
      if (!sym->ambiguous) {
        sym->is_invisible = FALSE;
      }  /* if */
    }  /* if */
    if (ptp->is_pack_element) sym->is_pack_element = TRUE;
    sym->variant.variable.ptr = vp;
    set_source_corresp(&(vp->source_corresp), sym);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Since real instantiations of a same template share the same param_id
       list, new source sequence entries should be created for the
       corresponding parameters (if source sequence entries are at all
       generated for instantiations). */
    record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION, sym,
                              &sym->decl_position,
                              is_real_instantiation ?
                                      NULL : param_id->source_sequence_entry);
    if (param_id->is_decl_after_first_in_comma_list) {
      /* An old-style parameter definition that appears after the first in a
         comma-separated list. */
      vp->source_corresp.is_decl_after_first_in_comma_list = TRUE;
    }  /* if */
#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
    mark_defined(sym, &sym->decl_position);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    mark_variable_value_set(sym);
#if DEBUG
    if (debug_level >= 3) {
      db_symbol(sym, "Changed from parameter symbol: ", 4);
    }  /* if */
#endif /* DEBUG */
  }  /* if */
  if (vp->is_pack_element) {
    /* The symbol for a non-initial variadic pack element is not in the symbol
       table and therefore has no associated scope depth.  As a result, the
       call to set_source_corresp may have produced invalid values for
       is_local_to_function and scope_depth.  Produce the correct values
       now. */
    vp->source_corresp.is_local_to_function = TRUE;
#if RECORD_SCOPE_DEPTH_IN_IL
    vp->source_corresp.scope_depth = decl_scope_level;
  } else if (sym == NULL) {
    /* scope_depth is normally recorded by a call to set_source_corresp, but
       that call didn't occur for unnamed parameters.  Record the depth now. */
    vp->source_corresp.scope_depth = decl_scope_level;
#endif /* RECORD_SCOPE_DEPTH_IN_IL */
  }  /* if */
  if (param_id->is_parameter_pack && ptp->param_num == 0) {
    /* This is a dummy parameter variable for an empty parameter expansion.
       The param-type entry is just a placeholder, and should not be recorded
       in the variable. */
  } else {
    vp->variant.assoc_param_type = ptp;
    /* Make sure the parameter recorded in the function type reflects the
       name and position as it appeared in the definition of the function. */
    ptp->name = vp->source_corresp.name;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    {
      a_decl_position_supplement_ptr  dpsp = ptp->decl_pos_info;
      if (dpsp != NULL) {
        dpsp->identifier_range = param_id->identifier_range;
        dpsp->specifiers_range = param_id->specifiers_range;
        dpsp->variant.declarator_range = param_id->declarator_range;
      }  /* if */
    }
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
    attach_param_variable_attributes(vp);
  }  /* if */
#if EXTRA_SOURCE_POSITIONS_IN_IL
  {
    a_decl_position_supplement_ptr  dpsp = vp->source_corresp.decl_pos_info;
    if (dpsp != NULL) {
      dpsp->identifier_range = param_id->identifier_range;
      dpsp->specifiers_range = param_id->specifiers_range;
      dpsp->variant.declarator_range = param_id->declarator_range;
    }  /* if */
  }
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  db_exit();
}  /* decl_parameter */


static void process_vla_parameters(a_func_info_block              *func_info,
                                   a_routine_type_supplement_ptr  rtsp)
/*
A function declared with the properties described by *func_info and *rtsp is
being defined in C mode.  If the function prototype contains references to
variable length arrays (VLAs), the associated VLA entries are created (we
couldn't do so earlier because the function memory region was not yet
available).  Furthermore, the VLA dimensions must be associated with the
parameter variables (which are internal to the function), but not with the
the a_param_type entries of the function: In the function type itself, all
VLA types are therefore transformed into [*] VLAs.  Finally, explicit [*]
declarators are diagnosed (since they can only appear in function prototypes
that are not followed by a definition).
This function should not be called in C++ mode since VLAs are not permitted
in parameter types of C++ mode functions.
*/
{
  a_vla_fixup_ptr   vfp;
  a_param_id_ptr    param_id;
  a_param_type_ptr  ptp;

  check_assertion(C_mode());
  /* Do fixups on VLA declarations that appeared in the function prototype
     scopes.  They are required because the function memory region was not
     not yet available when the function prototype was scanned. */
  /* On the first pass over the fixup list, adjust parameter references
     in VLA dimension expressions.  Replace references to a dummy
     param variable with the references to the real param variable. */
  for (vfp = func_info->vla_fixup_list; vfp != NULL; vfp = vfp->next) {
    if (vfp->array_type == NULL) {
      /* This entry represents a parameter variable fixup. */
      check_assertion(vfp->param_sym != NULL &&
                      vfp->param_sym->kind == (a_symbol_kind)sk_variable);
      node_variable(vfp->expr) = vfp->param_sym->variant.variable.ptr;
    }  /* if */
  }  /* for */
  /* On the second pass over the fixup list, create the VLA dimension
     entries and add them to the vla_dimensions list for the routine's IL
     scope. */
  for (vfp = func_info->vla_fixup_list; vfp != NULL; vfp = vfp->next) {
    if (vfp->array_type != NULL) {
      /* This entry represents a dimension expression fixup.  Copy the
         expression list to the function memory region and then create
         the vla_dimension entry. */
      (void)make_vla_dimension(vfp->array_type,
                               copy_expr_tree(vfp->expr, CE_NO_OPTIONS),
                               /*in_prototype_scope=*/TRUE,
                               &vfp->position);
    }  /* if */
  }  /* for */
  free_vla_fixup_list(func_info->vla_fixup_list);
  func_info->vla_fixup_list = NULL;
  /* Check for VLA errors. */
  param_id = func_info->param_id_list;
  ptp = rtsp->param_type_list;
  for (; param_id != NULL; param_id = param_id->next, ptp = ptp->next) {
    check_assertion_str(param_id->declared_type != NULL,
                        "process_vla_parameters: NULL declared_type");
    if (is_or_contains_vla_type_with_unspecified_bound(
                                           param_id->declared_type)) {
      /* The [*] syntax for VLAs is not allowed for a parameter in a
         function definition.  When parsing a function declarator the [*]
         syntax is allowed because it is impossible to distinguish a
         function prototype and a function definition at that point.  Now
         that the opening brace has been seen, the presence of [*] can be
         detected as an error. */
      pos_error(ec_vla_with_unspecified_bound_not_allowed,
                &param_id->type_pos);
      param_id->type = ptp->type = error_type();
    } else if (is_variably_modified_type(ptp->type)) {
      /* The param-type entry describes the public interface of the
         routine, whereas the parameter variable contains its internal
         representation.  VLA dimensions expressions, which have already
         been recorded in the types of the parameter variables, cannot
         be part of the public interface (like top-level const qualifiers
         in C++), so remove them now.  This transformation has the effect
         of replacing the dimension expression with "*". */
      ptp->type = remove_assoc_vla_dimensions(ptp->type);
    }  /* if */
  }  /* for */
}  /* process_vla_parameters */


static void advance_param_id_and_param_type(
				a_param_id_ptr		*param_id,
				a_param_type_ptr	*ptp,
				a_routine_ptr		rout_ptr)
/*
Advance *param_id and *ptp to the next element in their lists.   If *ptp is
an element of a variadic parameter pack, don't advance *param_id until we
advance past the pack.  This special processing is suppressed for
specializations and lambdas.  rout_ptr is the routine being defined.
*/
{
  a_param_type_ptr	next_ptp = (*ptp)->next;

  if (rout_ptr->is_template_function && !rout_ptr->is_specialized &&
      next_ptp != NULL && next_ptp->param_num == (*ptp)->param_num &&
      next_ptp->param_num != 0 && !(*param_id)->is_pack_element) {
    /* The next parameter type entry is for the same variadic parameter pack.
       Don't advance the param_id. */
  } else {
    *param_id = (*param_id)->next;
  }  /* if */
  *ptp = next_ptp;
}  /* advance_param_id_and_param_type */


static void check_deduced_return_type(a_routine_ptr      rp,
                                      a_source_position  *diag_pos)
/*
The given routine did not include an explicitly specified return type.  If a
value-returning statement was encountered, the return type was set accordingly;
otherwise, this routine will set it to void.  If the return type is non-void
and this is a C++11-style lambda body, check that the body had the simple form
      { return <expression> ; }
and issue a diagnostic if that was not the case.
*/
{
  if (!rp->has_deduced_return_type) {
    /* No return type was specified, and no return type was deduced from a
       return statement.  Determine the return type as if "return (void)0;"
       had appeared, except in dependent contexts, where we want to keep
       the original form of the return type. */
    if (is_template_dependent_context()) {
      rp->has_deduced_return_type = TRUE;
    } else {
      deduce_return_type_from_void_operand(rp, !rp->is_lambda_body, diag_pos);
    }  /* if */
  } else {
    a_type_ptr  rtp = skip_typerefs(rp->type);
    if (!could_be_literal_type(rtp->variant.routine.return_type)) {
      if (!rout_is_real_template_instance(rp) && rp->is_declared_constexpr) {
        pos_ty_error(ec_nonliteral_return_type_in_constexpr_function, diag_pos,
                     rtp->variant.routine.return_type);
      }  /* if */
      rp->is_constexpr = FALSE;
    }  /* if */
  }  /* if */
}  /* check_deduced_return_type */


static void set_routine_constexpr_info(a_scope_ptr scope,
                                       a_boolean   constexpr_ruled_out)
/*
scope is the function scope for a constexpr function or constructor.
Check to see if it is valid and record information used later when doing
a constexpr expansion of the routine.  When constexpr_ruled_out is TRUE,
the routine's body failed the criteria for a constexpr function or
constructor.
*/
{
  a_routine_ptr   routine;

  if (!constexpr_ruled_out) {
    check_assertion(scope->kind == (a_scope_kind)sck_function);
    routine = scope->variant.routine.ptr;
    check_assertion(routine->is_constexpr);
    if (relaxed_constexpr_allowed()) {
      /* C++14 doesn't impose the constraints checked for below. */
      scope->is_constexpr_routine = TRUE;
    } else if (special_kind_is(routine, sfk_constructor)) {
      /* Constructor.  Must have an empty statement as the body, i.e.,
         an implicit return. */
      scope->is_constexpr_routine = TRUE;
    } else {
      /* constexpr function. */
      scope->is_constexpr_routine = TRUE;
    }  /* if */
  }  /* if */
}  /* set_routine_constexpr_info */


static void create_coroutine_parameter_copy(a_variable_ptr    param_var,
                                            a_variable_ptr    copy_var,
                                            a_source_position *pos)
/*
Overwrite the provided copy_var with a variable that is direct-initialized with
the provided param_var.  Use pos as the position of this generated variable.
*/
{
  an_expr_stack_entry expr_stack_entry, *saved_expr_stack = expr_stack;
  a_type_ptr          ctype = param_var->type;
  a_decl_parse_state  dps;

  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  init_decl_parse_state(&dps);
  { /* Create the copy variable and a matching symbol. */
    a_symbol_locator loc;
    a_symbol_ptr     copy_sym;

    clear_locator(&loc, pos);
    copy_sym = make_symbol((a_symbol_kind)sk_variable, &loc);
    clear_variable(copy_var);
    copy_var->storage_class = (a_storage_class)sc_auto;
    copy_var->type = ctype;
    /* Use the same name as the original parameter to allow the C++ generating
       back end to produce valid code without needing to perform hijinks to
       get back to the original parameter name. */
    copy_var->source_corresp.name = param_var->source_corresp.name;
    copy_var->source_corresp.assoc_info = (char*)copy_sym;
    copy_var->source_corresp.enclosing_routine =
                                   param_var->source_corresp.enclosing_routine;
    copy_var->source_corresp.referenced = param_var->source_corresp.referenced;
    copy_var->source_corresp.is_local_to_function = TRUE;
    copy_var->is_this_parameter = param_var->is_this_parameter;
    copy_var->is_parameter = param_var->is_parameter;
    copy_var->next = param_var->next;
    copy_sym->variant.variable.ptr = copy_var;
    dps.sym = copy_sym;
  }
  { /* Create the initializer operand for the copy variable. */
    an_arg_list_elem_ptr var;
    an_expr_node_ptr     expr;
    an_operand           var_operand;

    expr = var_rvalue_expr(param_var);
    if (is_any_reference_type(ctype)) {
      expr = add_ref_indirection_to_node(expr);
      expr->is_lvalue = FALSE;
    }  /* if */
    if (is_lvalue_reference_type(ctype)) {
      expr->is_lvalue = TRUE;
    } else {
      expr->is_xvalue = TRUE;
    }  /* if */
    make_glvalue_expression_operand(expr, &var_operand);
    var = alloc_arg_list_elem_for_operand(&var_operand);
    add_init_component_to_initializer_cache(var, /*to_front=*/TRUE,
                                            &dps.prescanned_initializer_cache);
  }
  { /* Create the initializer for the copy variable.  Note that this frees
       the init component allocated above. */
    a_boolean incomplete_type_err = FALSE;
    initializer(&dps, pos, idl_none, /*parenthesized_initializer=*/TRUE,
                &incomplete_type_err, /*decl_pos_block=*/NULL);
  }
  pop_expr_stack();
  expr_stack = saved_expr_stack;
}  /* create_coroutine_parameter_copy */


static void copy_coroutine_parameters(a_routine_ptr         coroutine,
                                      a_coroutine_descr_ptr cr_desc)
/*
A coroutine requires that copies be made of all parameters to the coroutine,
and that all uses of these parameters be updated to use the copies.  Duplicate
the parameter variables and overwrite the original variables with these copies
(this will cause all references to the parameter variables to now refer to the
copies).  Update the scope entry for the routine to refer to the duplicates,
as this should not reference the copies.  Record the parameter copies in the
coroutine descriptor block.  Note that this should not be done until after the
coroutine body has finished processing, as this could otherwise cause later
parameter references to refer to the parameter duplicates instead of the
created copies.
*/
{
  a_source_position      *pos = &coroutine->source_corresp.decl_position;
  a_scope_ptr            sp = scope_for_routine(coroutine);
  a_variable_ptr         rout_param_var, relocated_var;
  a_variable_ptr         *orig_param = &sp->variant.routine.parameters;
  an_object_lifetime_ptr saved_curr_object_lifetime = curr_object_lifetime;
  an_expr_stack_entry    expr_stack_entry, *saved_expr_stack = expr_stack;

  push_expr_stack((an_expression_kind)ek_normal, &expr_stack_entry,
                  /*force_object_lifetime=*/FALSE,
                  /*suppress_object_lifetime=*/FALSE);
  expr_stack->in_coroutine_desc_init = TRUE;
  curr_object_lifetime = expr_stack->lifetime = sp->lifetime;
  cr_desc->this_param_copy = rout_param_var =
                                       sp->variant.routine.this_param_variable;
  cr_desc->parameter_copies = sp->variant.routine.parameters;
  if (rout_param_var != NULL) {
    relocated_var = alloc_variable(rout_param_var->storage_class);
    *relocated_var = *rout_param_var;
    create_coroutine_parameter_copy(relocated_var, rout_param_var, pos);
    sp->variant.routine.this_param_variable = relocated_var;
  }  /* if */
  for (rout_param_var = sp->variant.routine.parameters;
       rout_param_var != NULL; rout_param_var = rout_param_var->next) {
    relocated_var = alloc_variable(rout_param_var->storage_class);
    *relocated_var = *rout_param_var;
    create_coroutine_parameter_copy(relocated_var, rout_param_var, pos);
    *orig_param = relocated_var;
    orig_param = &(*orig_param)->next;
  }  /* for */
  pop_expr_stack();
  expr_stack = saved_expr_stack;
  curr_object_lifetime = saved_curr_object_lifetime;
}  /* copy_coroutine_parameters */


static a_statement_ptr add_coroutine_decl_statement(a_statement_ptr stmt,
                                                    a_variable_ptr  var,
                                                    a_scope_ptr     decl_scope)
/*
Add a statement declaring and initializing var to stmt and return that
statement.  If var is NULL, do nothing and return stmt.  decl_scope is the
scope that will contain the variable declaration.
*/
{
  a_scope_stack_entry_ptr ssep = &scope_stack_top();

  if (var != NULL) {
    check_assertion(var->init_kind == (an_init_kind)initk_dynamic);
    /* Add the variable to the scope's nonstatic variable list. */
    if (decl_scope->nonstatic_variables == NULL) {
      decl_scope->nonstatic_variables = var;
    } else {
      ssep->last_nonstatic_variable->next = var;
    }  /* if */
    ssep->last_nonstatic_variable = var;
    set_parent_scope(&var->source_corresp, iek_variable, decl_scope);
    /* Allocate the declaration statement. */
    stmt->next = alloc_statement(stmk_decl, /*compiler_generated=*/TRUE);
    stmt->next->parent = stmt->parent;
    stmt = stmt->next;
    stmt->variant.decl.entities = alloc_il_entity_list_entry();
    stmt->variant.decl.entities->entity.kind = iek_variable;
    stmt->variant.decl.entities->entity.ptr = (char*)var;
    /* Allocate the initializer statement for the variable. */
    stmt->next = alloc_statement(stmk_init, /*compiler_generated=*/TRUE);
    stmt->next->parent = stmt->parent;
    stmt = stmt->next;
    stmt->variant.dynamic_init = var->initializer.dynamic;
    /* Record the destruction at the appropriate place. */
    record_end_of_lifetime_destruction(var->initializer.dynamic,
                                       /*static_lifetime=*/FALSE,
                                       /*block_lifetime=*/TRUE);
  }  /* if */
  return stmt;
}  /* add_coroutine_decl_statement */


static
a_statement_ptr add_coroutine_variable_decls(a_statement_ptr       stmt,
                                             a_coroutine_descr_ptr cr_desc,
                                             a_scope_ptr           decl_scope)
/*
Append the coroutine variable decls (if any) to stmt, and ensure their
destructions (if needed) are recorded.  Return the last statement added, or
stmt if there's nothing to add.  decl_scope is the scope that will contain the
variable declarations.
*/
{
  a_variable_ptr param = cr_desc->parameter_copies;

  stmt = add_coroutine_decl_statement(stmt, cr_desc->this_param_copy,
                                      decl_scope);
  for (; param != NULL; param = param->next) {
    stmt = add_coroutine_decl_statement(stmt, param, decl_scope);
  }  /* for */
  stmt = add_coroutine_decl_statement(stmt, cr_desc->promise, decl_scope);
  return stmt;
}  /* add_coroutine_variable_decls */


static a_statement_ptr add_coroutine_expr_statement(a_statement_ptr  stmt,
                                                    an_expr_node_ptr expr)
/*
Add a statement for expr to stmt and return that statement.  If expr is NULL,
do nothing and return stmt.
*/
{
  if (expr != NULL) {
    stmt->next = alloc_statement(stmk_expr, /*compiler_generated=*/TRUE);
    stmt->next->parent = stmt->parent;
    stmt = stmt->next;
    stmt->expr = expr;
  }  /* if */
  return stmt;
}  /* add_coroutine_expr_statement */


static a_statement_ptr add_coroutine_label(a_statement_ptr        stmt,
                                           a_label_ptr            label,
                                           an_object_lifetime_ptr lifetime)
/*
Add a label statement for label to stmt, set its lifetime to the provided
lifetime, and return that stmt.  If label is NULL, do nothing and return stmt.
*/
{
  if (label != NULL) {
    stmt->next = alloc_statement(stmk_label, /*compiler_generated=*/TRUE);
    stmt->next->parent = stmt->parent;
    stmt = stmt->next;
    stmt->variant.label.ptr = label;
    stmt->variant.label.lifetime = lifetime;
    label->exec_stmt = stmt;
  }  /* if */
  return stmt;
}  /* add_coroutine_label */


static void generate_coroutine_body(a_routine_ptr coroutine)
/*
Given a coroutine where P is the promise type and F is the function body, the
coroutine behaves as if its body were:
  {
    <parameter copies>
    P p(<constructor args>);
    try {
      co_await p.initial_suspend();
      F
    } catch(...) {
      if (!initial-await-resume-called) throw;
      p.unhandled_exception();
    }
  final_suspend:
    co_await p.final_suspend();
  }
The pseudo-variable "initial-await-resume-called" is initially false and set to
true immediately before the evaluation of await-resume for the initial suspend
point.

The coroutine descriptor block contains expressions for the various calls and
the needed variables, but hasn't constructed any of the statements.  Generate
the necessary statements for the coroutine and insert the variable destructors
in the correct place.
*/
{
  a_scope_ptr            sp = scope_for_routine(coroutine);
  a_statement_ptr        stmt = sp->assoc_block;
  a_statement_ptr        func_body;
  an_object_lifetime_ptr func_lifetime;
  a_coroutine_descr_ptr  cr_desc;

  check_assertion(stmt != NULL);
  if (stmt->kind == (a_statement_kind)stmk_try_block) {
    stmt = stmt->variant.try_block->statement;
  }  /* if */
  check_assertion(stmt->kind == (a_statement_kind)stmk_block);
  stmt = stmt->variant.block.statements;
  /* stmt should now be pointing to the first statement of the function, which
     should be the generated stmk_coroutine that contains the coroutine
     descriptor.  It should be followed by at least one statement that
     represents the actual function body. */
  check_assertion(stmt != NULL && stmt->next != NULL &&
                  stmt->kind == (a_statement_kind)stmk_coroutine);
  func_body = stmt->next;
  stmt->next = NULL;
  cr_desc = stmt->variant.coroutine.descr;
  /* Create the statement for the function body try block first, as we need to
     transfer the function body's lifetime into that block. */
  func_body = wrap_coroutine_body_in_try_block(coroutine, func_body, cr_desc,
                                               cr_desc->initial_suspend_call);
  func_lifetime = sp->lifetime->child_lifetime;
  /* Now that we have the function body wrapped and saved off, generate the
     variable decls and initial statements as if they preceded the function
     try/catch block. */
  sp->lifetime->child_lifetime = NULL;
  copy_coroutine_parameters(coroutine, cr_desc);
  stmt = add_coroutine_variable_decls(stmt, cr_desc, sp);
  /* Add back in the function body. */
  stmt = stmt->next = func_body;
  if (func_lifetime != NULL) {
    /* Add the original function's lifetime back into the appropriate location
       of the modified function's lifetime. */
    func_lifetime->next = sp->lifetime->child_lifetime;
    func_lifetime->parent_destruction_sublist = sp->lifetime->destructions;
    sp->lifetime->child_lifetime = func_lifetime;
  }  /* if */
  /* Finally, add the final_suspend label and call to p.final_suspend() */
  stmt = add_coroutine_label(stmt, cr_desc->final_suspend_label, sp->lifetime);
  stmt = add_coroutine_expr_statement(stmt, cr_desc->final_suspend_call);
  cr_desc->body_generated = TRUE;
}  /* generate_coroutine_body */


static void wrap_up_coroutine(a_routine_ptr  rp)
/*
Given a coroutine whose body has finished being scanned, perform any last
changes required that couldn't be done until the body was complete.  Diagnose
certain constraint violations if needed (e.g., a coroutine cannot have an
ellipsis parameter).
*/
{
  a_coroutine_descr_ptr  cdp;

  check_assertion(rp->is_coroutine);
  cdp = get_coroutine_descr(rp);
  if (!cdp->error_descr) {
    a_scope_ptr  decl_scope = scope_for_routine(rp);
    if (!rp->is_prototype_instantiation) {
      generate_coroutine_body(rp);
    } else {
      set_parent_scope(&cdp->promise->source_corresp, iek_variable,
                       decl_scope);
    }  /* if */
    set_parent_scope(&cdp->handle->source_corresp, iek_variable, decl_scope);
    set_parent_scope(&cdp->init_await_resume->source_corresp, iek_variable,
                     decl_scope);
  }  /* if */
  if (skip_typerefs(rp->type)->variant.routine.extra_info->has_ellipsis) {
    pos_error(ec_coroutine_with_ellipsis_parameter,
              &rp->source_corresp.decl_position);
  }  /* if */
  if (rp->is_declared_constexpr || rp->is_consteval) {
    pos_error(ec_no_constexpr_coroutine, &rp->source_corresp.decl_position);
  }  /* if */
}  /* wrap_up_coroutine */


void scan_function_body(a_routine_ptr     rout_ptr,
                        a_func_info_block *func_info,
                        a_decl_flag_set   flags)
/*
Scan the function body of the routine pointed to rout_ptr.  *func_info
contains information accumulated during the declaration.  The flags control
specific requirements of the scan, since this routine is called not only
for normal function definitions but also (in C++ mode only, of course) for the
delayed scan of cached tokens of member functions defined in a class
definition, for the instantiation of template functions, and for the bodies
of lambda expressions.
*/
{
  a_type_ptr                     class_type, rout_type;
  a_routine_type_supplement_ptr  rtsp;
  a_scope_number                 scope_number;
  a_param_id_ptr                 param_id;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_param_id_ptr                 orig_param_id = NULL;
  a_boolean                      is_real_instantiation;
  a_param_type_ptr               orig_ptp = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  a_scope_ptr                    scope_ptr;
  a_struct_stmt_stack_state      saved_sss_state;
  a_boolean                      is_instantiation;
  a_param_type_ptr               ptp;
  a_namespace_ptr                nsp = NULL;
  a_boolean                      is_function_try_block = FALSE;
  a_pack_alignment_state         saved_pack_alignment_state;
  a_source_position              body_pos;
  a_source_position              lbrace_pos;

  db_enter(3, "scan_function_body");
  body_pos = pos_curr_token;
  lbrace_pos = (curr_token == tok_lbrace) ? pos_curr_token
                                          : null_source_position;
  if (rout_ptr->source_corresp.is_class_member) {
    class_type = parent_class_of(rout_ptr);
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (cli_or_cx_enabled) {
      /* Issue errors when attempting to define a abstract member of a C++/CLI
         managed class (a more specific message is used for a member of an
         interface class). */
      if (cli_class_type_kind_is(class_type, cctk_interface) &&
          routine_type_is_nonstatic_member_function(rout_ptr->type)) {
        pos_error(ec_cli_interface_member_function_definition,
                  &rout_ptr->source_corresp.decl_position);
      } else if (is_immediate_managed_class_type(class_type) &&
                 rout_ptr->pure_virtual) {
        pos_error(ec_cli_abstract_member_function_definition,
                  &rout_ptr->source_corresp.decl_position);
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  } else {
    class_type = NULL;
  }  /* if */
  /* Change the storage class to indicate that the function has a definition
     now.  An sc_static storage class is left alone. */
  if (rout_ptr->storage_class == (a_storage_class)sc_extern) {
    rout_ptr->storage_class = (a_storage_class)sc_unspecified;
  }  /* if */
  /* Instantiate any delayed exception specification arguments. */
  if (rout_ptr->is_template_function) {
    instantiate_exception_spec_if_needed(symbol_for(rout_ptr));
  }  /* if */
  rout_type = skip_typerefs(rout_ptr->type);
  /* Issue an error if this is an invalid return type. */
  (void)check_function_return_type(rout_type,
                                   &rout_ptr->source_corresp.decl_position,
                                   /*is_expr_use=*/FALSE, /*evaluated=*/FALSE,
                                   /*incomplete_return_okay=*/FALSE, rout_ptr);
  /* In certain very obscure cases, the routine type associated with
     rout_ptr may be replaced by an equivalent type entry.  Refetch the type,
     just in case. */
  rout_type = skip_typerefs(rout_ptr->type);
  rtsp = rout_type->variant.routine.extra_info;
  is_instantiation = (flags & SFB_IS_INSTANTIATION) != 0;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  is_real_instantiation = is_instantiation &&
                   !scope_stack[depth_scope_stack].in_prototype_instantiation;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  if (!C_mode() && !is_instantiation) {
    /* Reactivate the class and/or namespace of which the function body is
       a member.  For template instantiations this is done when the
       instantiation scope is pushed, so it should not be done here. */
    if (class_type != NULL) {
      /* Member function -- either an inline or "out-of-line" definition. */
      if (flags & SFB_NO_CLASS_REACTIVATION) {
        /* Inline.  Class has already been reactivated. */
      } else {
        /* Push a class symbol reactivation scope, to make class member names
           visible for processing the function definition.  If the class is a
           member of a namespace, reactivating the namespace is treated as a
           namespace extension. */
        a_boolean  reactivate_template_params = FALSE,
                   saved_use_microsoft_specialization_scope =
                                           use_microsoft_specialization_scope;
        if (microsoft_mode && rout_ptr->specialized_with_old_syntax) {
          /* In Microsoft mode, old-style specializations (those without the
             "template <>") can make use of the template parameters of the
             enclosing class template. */
          reactivate_template_params = TRUE;
          use_microsoft_specialization_scope = TRUE;
        }  /* if */
        push_class_and_template_reactivation_scope_full(
                               class_type,
                               reactivate_template_params,
                               rout_ptr->is_specialized,
                               /*extend_namespace=*/TRUE,
                               /*force_new_context=*/TRUE,
                               PS_NO_OPTIONS);
        use_microsoft_specialization_scope =
                                     saved_use_microsoft_specialization_scope;
      }  /* if */
    } else {
      nsp = parent_namespace_or_null(rout_ptr);
      if (nsp != NULL) {
        a_scope_stack_entry_ptr  decl_ssep = &scope_stack[decl_scope_level];
        a_scope_stack_entry_ptr  curr_ssep = &scope_stack[depth_scope_stack];
        if (curr_ssep->kind == (a_scope_kind)sck_class_reactivation) {
          /* If the current scope is a class reactivation, use the previous
             scope for the following check for a template instantiation
             scope. */
          curr_ssep--;
        }  /* if */
        if (curr_ssep->kind == (a_scope_kind)sck_template_instantiation) {
          /* The namespace is pushed when the template instantiation
             scope is pushed.  Don't do it again now. */
          nsp = NULL;
        } else if (clang_mode && gpp_mode &&
                   (flags & SFB_INLINE_NAMESPACE_SPECIALIZATION) != 0) {
          /* An explicit specialization of an inline namespace member does
             not extend the namespace in clang mode. */
          nsp = NULL;
        } else if ((decl_ssep->kind != (a_scope_kind)sck_namespace &&
                    decl_ssep->kind !=
                                     (a_scope_kind)sck_namespace_extension) ||
                   nsp != decl_ssep->il_scope->variant.assoc_namespace) {
          /* Push a namespace extension scope. */
          push_namespace_extension_scope(nsp);
        } else {
          /* Set the pointer to NULL to indicate there's no stack entry to
             pop. */
          nsp = NULL;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
  scope_number = (is_instantiation) ?
                        NO_SCOPE_NUMBER : func_info->scope_number;
  /* Push the name scope for the routine body. */
  scope_ptr = push_scope((a_scope_kind)sck_function, scope_number,
                         (a_type_ptr)NULL, rout_ptr);
  if (rout_ptr->is_consteval) {
    scope_stack_top().in_consteval_context = TRUE;
  }  /* if */
  if (func_info->lambda != NULL) {
    a_symbol_ptr  call_op_sym;
    /* Make the lambda call operator invisible to unqualified lookup inside
       its own definition. */
    if (rout_ptr->is_template_function) {
      call_op_sym = symbol_for(rout_ptr->assoc_template);
    } else {
      call_op_sym = symbol_for(rout_ptr);
    }  /* if */
    call_op_sym->is_invisible = TRUE;
    call_op_sym->qualified_lookup = TRUE;
  }  /* if */
  /* Make sure the implicit_typename flag is FALSE during prototype
     instantiations.  It could be set if we are in a mode where
     nonclass prototype instantiations are not normally done, but we
     are processing a variadic template. */
  if (rout_ptr->is_prototype_instantiation && !force_implicit_typename) {
    scope_stack_top().implicit_typename = FALSE;
  }  /* if */
  check_assertion(rout_ptr->function_def_number != NULL_function_def_number);
  /* Associate the routine entry to its type entry. */
  rtsp->assoc_routine = rout_ptr;
  /* If return value optimization may be possible (i.e., if the routine
     returns a class value via a copy constructor) set the flag to TRUE.
     (It is also required that all the return statements return a single local
     variable -- if that turns out not to be the case, the flag will be
     cleared again.) */
  if (rtsp->value_returned_by_cctor) {
    scope_stack[depth_scope_stack].return_value_optimization_possible = TRUE;
  }  /* if */
  if (class_type != NULL && rtsp->this_class != NULL) {
    scope_ptr->variant.routine.this_param_variable =
                                 make_implicit_this_param_variable(rout_type);
  }  /* if */
  if (microsoft_bugs && microsoft_version == 1200 &&
      rout_ptr->defined && rout_ptr->is_specialized) {
    /* This is a duplicate definition of a template specialization.
       Microsoft Visual C++ 6.0 just discards these. */
    scope_stack[depth_scope_stack].discard_when_popped = TRUE;
  }  /* if */
  if (func_info->function_type_from_typedef) {
    /* An error was already issued on this.  Now, since no parameters were
       specified, skip the processing for parameter names. */
    check_assertion(func_info->param_id_list == NULL);
  } else {
    /* Correctly declared function type. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (func_info->prototype_scope_ss_list != NULL) {
      /* Step through the segment of file-scope source sequence entries
         generated when the parameter list of the function was scanned.
         Do necessary fixups for parameter entries, and build function-scope
         proxies where necessary. */
      a_scope_stack_entry_ptr      stack_ptr;
      a_source_sequence_entry_ptr  ssep, next_ssep;

      stack_ptr = &scope_stack[DEPTH_OF_FILE_SCOPE];
      ssep = func_info->prototype_scope_ss_list;
#if DEBUG
      if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
        if (ssep != NULL) {
          fputs("scan_function_body: fixing up func prototype ss list:\n",
                f_debug);
          db_ss_list(ssep);
        }  /* if */
      }  /* if */
#endif /* DEBUG */
      for (; ssep != NULL; ssep = next_ssep) {
        next_ssep = ssep->next;
        ssep->prev = ssep->next = NULL;
        switch (ss_entry_kind(ssep)) {
          case iek_none:
            /* An empty entry should be for a parameter (the entry could
               not be filled in when the parameter identifier appeared, because
               the variable entry does not get built at that time).  Find
               the corresponding parameter (the lists are not necessarily in
               the same order). */
            param_id = func_info->param_id_list;
            for (; param_id != NULL; param_id = param_id->next) {
              if (param_id->source_sequence_entry == ssep) break;
            }  /* for */
#if DEBUG
            if (debug_level >= 4 || db_flag_is_set("dump_ss_full")) {
              fprintf(f_debug, "%sparam_id match for ",
                               param_id == NULL ? "no " : "");
              db_source_sequence_entry(ssep);
            }  /* if */
#endif /* DEBUG */
            if (param_id == NULL) {
              /* This source sequence entry is not associated with one of the
                 parameters.  For instance:
                   void f(a) int a(int); { ... } 
                 for which an empty source sequence entry will have been
                 created for the omitted parameter of function a. */
            } else {
              /* Take the entry off the file-scope list and add one (also
                 empty so far) to the function-scope list. */
              ssep->next = stack_ptr->source_sequence_avail_list;
              stack_ptr->source_sequence_avail_list = ssep;
              param_id->source_sequence_entry = NULL;
              if (!is_real_instantiation) {
                /* Instantiations share the same param_id list; so one should
                   not override the source sequence entry of another.  (Only
                   significant when source sequence entries are recorded for
                   instantiations.) */
                param_id->source_sequence_entry =
                                            add_empty_source_sequence_entry();
              }  /* if */
            }  /* if */
            break;
          default:
            /* For types (as well as other miscellany, such as fields in a
               C struct definition), add the entries to a sublist of the
               function scope list. */
            add_source_sequence_entry_to_list(ssep);
            break;
            /* No action. */
        }  /* switch */
      }  /* for */
      /* Just to be neat. */
      func_info->prototype_scope_ss_list = NULL;
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    if (func_info->any_prototype_names_omitted) {
      /* New-style (function prototype) for which at least one of the param
         names was omitted in the prototype.  In C this is not valid prior to
         C23 on a function definition; in C++ it's okay. */
      if (C_mode() && !c23_mode) {
        /* In anticipation of C23, some versions of GCC and Clang accept such
           unnamed parameters in all C modes. */
        an_error_severity  sev = es_discretionary_error;
        if (gcc_version_is(>= 110000) || clangc_version_is(>= 110000)) {
          sev = es_warning;
        }  /* if */
        diagnostic(sev, ec_all_proto_params_must_be_named);
      }  /* if */
    }  /* if */
    if (f_xref_info != NULL) {
      /* Cross reference info is being put out. */
      param_id = func_info->param_id_list;
      if (param_id != NULL &&
          param_id->old_style_id_pos.seq != 0) {
        /* Update the cross-reference output with entries for the comma-list
           of parameter names in the old-style parameter declaration format. */
        for (; param_id != NULL; param_id = param_id->next) {
          if (param_id->implicitly_declared) {
            /* Implicitly declared old-style parameters are recorded as
               definitions -- mark_defined is called in decl_parameter. */
          } else {
            /* The symbol pointed to by param_id is still an sk_parameter
               symbol.  However, its source position will have been modified,
               so use the original source position. */
            mark_declared(param_id->symbol, &param_id->old_style_id_pos);
          }  /* if */
        }  /* for */
      }  /* if */
    }  /* if */
    param_id = func_info->param_id_list;
    ptp = rtsp->param_type_list;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* The declared type of parameters is recorded in a different locations
       depending on the nature of the function (template or nontemplate) and
       the way it was declared (through a typedef, using old-style C syntax,
       etc.).  Depending on the situation we may have to iterate over an
       a_param_type list or an a_param_id list. */
    if (param_id != NULL) {
      if (is_real_instantiation) {
        /* An instantiation of a function template does not always involve
           rescanning its declaration (only its body).  That means that the
           declared type is not recorded in *func_info; instead, it may have
           been reconstructed in the associated a_template_instance entity. */
        a_symbol_ptr             rout_sym = symbol_for(rout_ptr);
        a_template_instance_ptr  tip = rout_sym->variant.routine.instance_ptr;
        check_assertion(tip != NULL);
        if (tip->declared_type != NULL) {
          orig_ptp = skip_typerefs(tip->declared_type)
                                ->variant.routine.extra_info->param_type_list;
        }  /* if */
      } else if (func_info->declared_type != NULL) {
        orig_ptp = skip_typerefs(func_info->declared_type)
                                ->variant.routine.extra_info->param_type_list;
      }  /* if */
      if (orig_ptp == NULL) {
        /* The declared parameter types are not available from the routine's
           declared_type entry (e.g., when dealing with old-style C function
           definitions). */
        orig_param_id = func_info->param_id_list;
        check_assertion(orig_param_id != NULL);
      }  /* if */
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    /* Be sure param-id and param-type lists are in sync. */
    if ((param_id == NULL) != (ptp == NULL) &&
        !(param_id != NULL && param_id->is_parameter_pack)) {
      /* Getting here is slightly unusual.  In most modes, it is the result of
         the param_id list being discarded because severe syntax errors made it
         look like the declarator did not appear at the top level (param_id is
         NULL in such cases).  In Microsoft mode, however, it can also occur
         when a single template-dependent parameter became "void" after
         instantiation (ptp is NULL in that case).  Note that the case where
         there is a param_id representing an empty parameter pack expansion is
         handled below (in which case ptp might be NULL if there are no actual
         parameters that follow). */
#if CHECKING
      if (param_id == NULL) {
        check_assertion(is_at_least_one_error());
      } else {
        check_assertion_or_expect_error(
                                  microsoft_mode && param_id->next == NULL &&
                                  param_id->type != NULL &&
                                  is_template_dependent_type(param_id->type));
      }  /* if */
#endif /* CHECKING */
      param_id = NULL;
      ptp = NULL;
    }  /* if */
    for (; param_id != NULL;
         advance_param_id_and_param_type(&param_id, &ptp, rout_ptr)) {
#if GENERATE_SOURCE_SEQUENCE_LISTS
      a_type_ptr  declared_param_type;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      /* In some cases with empty pack expansions there can be no param
         type entry for a given parameter.   Skip over those param_ids. */
      while (param_id != NULL && param_id->is_parameter_pack &&
             (ptp != NULL ?  param_id->param_num < ptp->param_num : TRUE)) {
#if GENERATE_SOURCE_SEQUENCE_LISTS
        Value_saver<a_boolean>  saver(&source_sequence_entries_disallowed,
                                      TRUE);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
        a_param_type_ptr        empty_ptp = alloc_param_type(param_id->type);
        empty_ptp->type = type_of_unknown_templ_param_nontype;
        empty_ptp->declared_type = type_of_unknown_templ_param_nontype;
        decl_parameter(param_id, param_id->type, empty_ptp,
                       is_instantiation);
        free_param_type_list(empty_ptp);
        param_id = param_id->next;
      }  /* while */
      if (ptp == NULL) break;
      if (param_id == NULL) {
        /* This can happen with severe errors (particularly with variadic
           template instantiations). */
        expect_error();
        break;
      }  /* if */
      /* Declare each parameter identifier to have the associated type
         from the parameter type list. */
#if GENERATE_SOURCE_SEQUENCE_LISTS
      if (orig_ptp != NULL) {
        declared_param_type = orig_ptp->declared_type;
        orig_ptp = orig_ptp->next;
      } else {
        /* coverity[var_deref_op] - orig_param_id can't be NULL. */
        declared_param_type = orig_param_id->declared_type;
        orig_param_id = orig_param_id->next;
      }  /* if */
      decl_parameter(param_id, declared_param_type, ptp, is_instantiation);
#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
      decl_parameter(param_id, (a_type_ptr)NULL, ptp, is_instantiation);
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    }  /* for */
    if ((param_id == NULL) != (ptp == NULL) &&
        param_id != NULL && !param_id->is_parameter_pack) {
      /* Something went wrong while parsing the template, which caused us to
         miscount the number of parameters. */
      check_assertion(is_at_least_one_error());
    }  /* if */
    if (vla_enabled && C_mode()) {
      /* Some additional transformations and checks may be needed for
         parameters with variably-modified types.  (In C++ mode, such
         parameters are not allowed.) */
      process_vla_parameters(func_info, rtsp);
    }  /* if */
    if (rout_ptr->is_constexpr &&
        !check_constexpr_routine_def_type(
                         rout_ptr, &rout_ptr->source_corresp.decl_position)) {
      rout_ptr->is_constexpr = FALSE;
    }  /* if */
    if (!is_instantiation) {
      /* Parameter symbols that were created in the prototype scope (and then
         removed in pop_scope) have to be reentered in the function scope;
         they will be transformed into variable symbols.  Also, in C mode,
         types that were defined in the prototype scope are reactivated now
         so that they will be available in the current scope. */
      if (func_info->prototype_scope_symbols != NULL) {
        reactivate_prototype_scope_symbols(func_info->prototype_scope_symbols);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Change the defaults for packing class members in a struct definition.
     This is especially important when the definition is encountered "out
     of sequence" relative to the rest of the program (e.g., delayed
     processing of inline-defined member functions and instantiations of
     function templates), since the defaults at the point of definition
     (textual) may be different from the defaults at the point where the
     body is actually scanned (now). */
  if (flags & SFB_PRAGMA_PACK_IS_LOCAL) {
    reset_pack_alignment_state(func_info->max_member_alignment,
                               &saved_pack_alignment_state);
    scope_stack[depth_scope_stack].pragma_pack_is_local = TRUE;
  }  /* if */
  if (flags & SFB_NEW_STRUCT_STMT_STACK_REQUIRED) {
    /* Save structured statement stack state before calling compound_statement
       (so that it can be restored upon return) and create a new structured
       statement stack.  This is required for function definitions in classes
       defined within a function definition.  An indefinite nesting depth is
       supported */
    new_struct_stmt_stack(&saved_sss_state);
  }  /* if */
  /* Start a new stop tokens set because the inside of a function is rather
     different than the outside. */
  push_stop_token_stack();
#if ASM_FUNCTION_ALLOWED
  if (rout_ptr->storage_class == (a_storage_class)sc_asm) {
    scope_ptr->assoc_block = scan_asm_function_body();
#if ONE_INSTANTIATION_PER_OBJECT
#if DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES
    if (one_instantiation_per_object) {
      /* In one-instantiation-per-object mode, asm functions should be
         duplicated in each slice. */
      rout_ptr->source_corresp.duplicate_static_in_instantiation_slices = TRUE;
    }  /* if */
#endif /* DUPLICATE_SPECIAL_STATICS_IN_INSTANTIATION_SLICES */
#endif /* ONE_INSTANTIATION_PER_OBJECT */
  } else
#endif /* ASM_FUNCTION_ALLOWED */
  /* Do not insert code here. */
  {
    a_boolean  explicit_return_type =
                         ((flags & SFB_IMPLICITLY_DECLARED_RETURN_TYPE) == 0);

    if (curr_token == tok_try && func_info->lambda == NULL) {
      /* This must be a function-try-block.  Do some initialization that has
         to be done before ctor-initializers are processed and bypass "try". */
      start_of_function_try_block();
      is_function_try_block = TRUE;
    }  /* if */
#if CHECKING
    if (!is_at_least_one_error()) {
      /* Except where there are invalid declarations, the flags in the types
         should be consistent with the special function kinds. */
      check_assertion((a_boolean)rtsp->assoc_routine_is_ctor ==
                    (rout_ptr->special_kind ==
                                   (a_special_function_kind)sfk_constructor));
      check_assertion((a_boolean)rtsp->assoc_routine_is_dtor ==
                    (rout_ptr->special_kind ==
                                   (a_special_function_kind)sfk_destructor));
    }  /* if */
#endif /* CHECKING */
    /* Enter the constructor initializers.  If the current token is a ":",
       explicit initialization for the constructor follows, but even without
       an explicit initializer, any implicit initializers should be
       recorded. */
    if (rout_ptr->special_kind == (a_special_function_kind)sfk_constructor) {
      scope_ptr->variant.routine.constructor_inits =
                                      ctor_initializer(rout_ptr,
                                                       /*user_defined=*/TRUE,
                                                       /*fields_only=*/FALSE);
    } else if (rout_ptr->special_kind ==
                                   (a_special_function_kind)sfk_destructor) {
      scope_ptr->variant.routine.constructor_inits =
                                      dtor_initializer(rout_ptr);
    }  /* if */
    if (is_function_try_block) {
      /* Scan the function try block.  This includes scanning the catch
         clauses that follow the function body. */
      scope_ptr->assoc_block = function_try_block(explicit_return_type);
    } else {
      /* Scan the compound statement defining the function.  The closing "}"
         is not swallowed by compound_statement, so that the pop_scope call
         can be done to get any errors out right on the "}". */
      scope_ptr->assoc_block = compound_statement(/*at_function_level=*/TRUE,
                                                  explicit_return_type,
                                                  /*is_catch_clause=*/FALSE,
                                                  /*is_statement_expr=*/FALSE);
    }  /* if */
    if (rout_ptr->is_constexpr) {
      set_routine_constexpr_info(
             scope_ptr,
             scope_stack[depth_innermost_function_scope].constexpr_ruled_out);
    }  /* if */
  }  /* if */
  /* Restore defaults for packing class members in a struct definition to
     what it was before the routine body was entered. */
  if (flags & SFB_PRAGMA_PACK_IS_LOCAL) {
    restore_pack_alignment_state(&saved_pack_alignment_state);
  }  /* if */
  if (rout_ptr->is_coroutine) {
    wrap_up_coroutine(rout_ptr);
  }  /* if */
  if (rout_ptr->has_deducible_return_type) {
    /* We're completing the body of a function with a deducible return type.
       Ensure that a type is established at this point. */
    check_deduced_return_type(rout_ptr, &body_pos);
  }  /* if */
  if (func_info->lambda != NULL) {
    /* Remove unneeded captures. */
    a_lambda_capture_ptr  *p_lcp = &func_info->lambda->capture_list;
    while (*p_lcp != NULL) {
      if ((*p_lcp)->field_pending) {
        /* No capture was actually required.  Drop this capture. */
        *p_lcp = (*p_lcp)->next;
      } else {
        p_lcp = &(*p_lcp)->next;
      }  /* if */
    }  /* while */
    /* The lambda call operator was made invisible above; make it visible
       again. */
    if (rout_ptr->is_template_function) {
      symbol_for(rout_ptr->assoc_template)->is_invisible = FALSE;
    } else {
      symbol_for(rout_ptr)->is_invisible = FALSE;
    }  /* if */
  }  /* if */
  /* Pop the function scope. */
  pop_scope();
  if (flags & SFB_NEW_STRUCT_STMT_STACK_REQUIRED) {
    /* Restore the original structured statement stack.  This is done
       after popping the function scope because pop_scope calls
       wrapup_control_flow_processing. */
    restore_struct_stmt_stack(&saved_sss_state);
  }  /* if */
  if (class_type != NULL) {
    /* This is a member function.  See if the fact that it is defined
       forces definition of virtual functions of the class. */
    require_definitions_of_virtual_functions_due_to_definition_of(rout_ptr);
  }  /* if */
  if (!is_instantiation) {
    /* For templates, the class and/or namespace scopes are pushed and
       popped when the instantiation scope is pushed/popped. */
    if (class_type != NULL) {
      if (!(flags & SFB_NO_CLASS_REACTIVATION)) {
        /* Pop the class symbol reactivation scope. */
        pop_class_reactivation_scope();
      }  /* if */
    } else if (nsp != NULL) {
      if (gpp_mode) {
        pop_namespace_reactivation_scope();
      } else {
        pop_namespace_extension_scope();
      }
    }  /* if */  
  }  /* if */
  if (!is_function_try_block) {
    /* Check for the closing "}", not done in compound_statement.  Note that
       required_token is not called; if compound_statement returned on
       anything other than a right brace, it's because we should start parsing
       on this token. */
    if (curr_token != tok_rbrace) {
      error_position = pos_curr_token;
      report_missing_closing_delimiter(ec_exp_rbrace, ec_matching_lbrace,
                                       &lbrace_pos);
    }  /* if */
  }  /* if */
  pop_stop_token_stack();
  if (instantiate_extern_inline && rout_ptr->is_inline &&
      !rout_ptr->is_consteval &&
      !rout_ptr->is_prototype_instantiation &&
      !rout_ptr->on_inline_function_list) {
    /* When inline functions are instantiated like templates, add the function
       to the list of inline functions if it is inline.  In some cases the
       function may already have been added to the list. */
    add_to_inline_function_list(rout_ptr);
  }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
  if (cli_or_cx_enabled && special_kind_is(rout_ptr, sfk_static_constructor)) {
    /* If this is the static constructor definition, ensure that the initonly
       static members have been initialized. */
    check_initonly_members(class_type, /*static_ctor_def_seen=*/TRUE);
  }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
#if DEBUG
  if (debug_level >= 4) {
    a_symbol_ptr  sym = (a_symbol_ptr)rout_ptr->source_corresp.assoc_info;
    if (sym != NULL) {
      db_symbol(sym, "finished scanning body for ", 2);
    }  /* if */
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* scan_function_body */


void adjust_member_routine_type(a_type_ptr	rout_type,
				a_type_ptr	prev_type)
/*
rout_type is the type of a member function or member function template
that is being defined.  prev_type is the type from the declaration in
the class.  rout_type is missing certain information, such as the
this_class information.  Update rout_type with information from prev_type.
*/
{
  a_routine_type_supplement_ptr  rtsp, prev_rtsp;

  rtsp = rout_type_supp(skip_typerefs(rout_type));
  prev_rtsp = rout_type_supp(skip_typerefs(prev_type));
  rtsp->this_class = prev_rtsp->this_class;
  rtsp->has_this_param = prev_rtsp->has_this_param;
  rtsp->qualifiers = prev_rtsp->qualifiers;
  rtsp->routine_name_linkage = prev_rtsp->routine_name_linkage;
}  /* adjust_member_routine_type */


static void define_member_function(
                                a_symbol_locator                *locator,
                                a_decl_parse_state              *dps,
                                a_func_info_block               *func_info,
                                an_id_linkage_kind              *linkage_ptr,
                                a_type_ptr                      *old_type,
                                a_symbol_ptr                    *ext_sym,
                                ARG_UNUSED a_decl_pos_block_ptr decl_pos_block)
/*
This routine is called in the case of a member function definition.  Its
function is similar to that of decl_routine, which is called for
the definitions of ordinary functions.  After doing some error checking,
it calls reconcile_routine_types to merge the current type with the type
on a prior declaration.
The function's declaration is described by locator, dps, and func_info.
Existing type information (from the matching in-class declaration) is returned
through *old_type.  Extended position information is recorded in
*decl_pos_block.  *linkage_ptr is set to idl_external, and *ext_sym is set to
NULL.
This function is also called in the case of a nondefining out-of-class
member declaration (allowed in some Microsoft modes only).
*/
{
  a_type_ptr           type_ptr = dps->type;
  a_type_ptr           rout_type = skip_typerefs(type_ptr);
  a_routine_type_supplement_ptr
                       rtsp = rout_type->variant.routine.extra_info;
  a_symbol_ptr         sym = locator->specific_symbol;
  a_type_ptr           class_type = sym_parent_class(sym);
  a_routine_ptr        rp;
  a_scope_stack_entry  *ssep = &scope_stack[decl_scope_level];
  a_boolean            microsoft_out_of_class_redecl = ms_extensions &&
                                                  locator->is_class_member &&
                                                  curr_token == tok_semicolon;
  a_source_position    orig_pos = null_source_position, saved_pos;

  db_enter(3, "define_member_function");
  if (!is_member_function_symbol(sym)) {
    /* A nonfunction class member.  This is an error, so set sym to NULL to
       force the creation of a fake member function symbol. */
    if (sym->kind == (a_symbol_kind)sk_projection) {
      /* A member of a base class. */
      pos_error(ec_inherited_member_not_allowed, &locator->source_position);
    } else {
      pos_sy_error(ec_not_compatible_with_previous_decl,
                   &locator->source_position, sym);
    }  /* if */
    sym = NULL;
  } else if (!namespace_is_enclosed_by_scope(sym, ssep) &&
             !gpp_mode && !microsoft_out_of_class_redecl) {
    /* This member function is being defined in a scope that does not enclose
       the scope in which the parent class was defined.  (This is not required
       in GNU C++ mode.  Neither is it required for Microsoft mode out-of-class
       member function redeclarations, which can appear in function scope.) */
    sym_error(ec_bad_scope_for_definition, sym);
    sym = NULL;
  } else {
    a_symbol_ptr  orig_sym = sym, other_match;
    /* Update the implicit exception specification if needed. */
    /* Look for a member function symbol of this type in the symbol table.
       It is an error if it is  not already there. */
    sym = member_function_redecl_sym(orig_sym, dps, (a_template_param_ptr)NULL,
                                     &other_match);
    if (sym == NULL && any_cfront_mode()) {
      /* In cfront it's okay to put a function qualifier on a member function
         definition.  If it's inappropriate, it's just ignored.  Do the same
         in cfront mode -- but issue a diagnostic. */
      if (rtsp->this_class != NULL) {
        rtsp->this_class = NULL;
        rtsp->has_this_param = FALSE;
        sym = member_function_redecl_sym(
                                    locator->specific_symbol, dps,
                                    (a_template_param_ptr)NULL, &other_match);
        /* The qualifiers are cleared only after looking for a redeclaration
           symbol.  This ensures that we find the same declaration Cfront
           would find. */
        rtsp->qualifiers = TQ_NONE;
        if (sym != NULL) {
          pos_sy_warning(ec_not_compatible_with_previous_decl,
                         &locator->source_position, locator->specific_symbol);
        }  /* if */
      }  /* if */
    }  /* if */
    if (sym != NULL && sym->kind == (a_symbol_kind)sk_projection) {
      /* The matching declaration is a using-declaration: An error. */
      check_assertion(is_class_member_using_decl_symbol(sym));
      pos_error(ec_inherited_member_not_allowed, &locator->source_position);
      reduce_projection_symbol_to_fundamental_symbol(sym);
    }  /* if */
    if (sym == NULL) {
      /* No member function with a matching type was found.  Issue an error.
         If the type matches an instance of a member function template,
         then this is probably an attempt to define a function using
         the old specialization syntax.  Issue an error to that effect. */
      if (has_matching_template_instance(orig_sym, dps->type,
                                         locator->template_arg_list)) {
        pos_sy_error(ec_old_specialization_not_allowed,
                     &locator->source_position, orig_sym);
      } else {
        pos_sy_error(locator->specific_symbol->kind ==
                                     (a_symbol_kind)sk_overloaded_function ?
                        ec_no_match_for_type_of_overloaded_function :
                        ec_not_compatible_with_previous_decl,
                   &locator->source_position, locator->specific_symbol);
      }  /* if */
      if (dps->has_deducible_return_type) {
        /* The mismatch was possibly due to this declaration having an "auto"
           or "decltype(auto)" return type.  For error recovery purposes,
           proceed with an error return type. */
        rout_type->variant.routine.return_type = error_type();
      }  /* if */
    } else if (sym->kind == (a_symbol_kind)sk_function_template) {
      /* A case like this:
           class A { template <class T> void f(int); };
           void A::f(int) { }
      */
      pos_sy_error(ec_old_specialization_not_allowed,
                   &locator->source_position, sym);
      sym = NULL;
    } else if (sym->variant.routine.ptr->compiler_generated) {
      /* Attempting to give a definition for a function that was implicitly
         declared. */
      pos_error(ec_definition_of_implicitly_declared_function,
                &locator->source_position);
      /* Unless a definition has already been generated, reset some flags
         so that this routine will be treated as user-declared from
         now on. */
      if (!sym->defined) {
        sym->variant.routine.ptr->compiler_generated = FALSE;
        set_inline_flag(sym->variant.routine.ptr, FALSE);
      }  /* if */
    } else if (other_match != NULL) {
      /* Multiple matches were found.  E.g.,
           __interface I1 { int f(); };  __interface I2 { int f(); };
           struct D: I1, I2 { int I1::f(); int I2::f(); };
           int D::f() { return 0; }  // Ambiguous.
      */
      pos_sy_error(ec_ambiguous_name, &locator->source_position, sym);
      check_assertion(ms_extensions);
      /* Proceed as if no match had been found at all. */
      sym = NULL;
    }  /* if */
  }  /* if */
  if (gpp_mode && gnu_version < 40700 && defaulted_special_members_enabled &&
      sym != NULL) {
    /* Some versions of GCC accept an explicit out-of-class definition after a
       special member has been defaulted in the class.  If that's the case,
       discard the prior "definition". */
    rp = sym->variant.routine.ptr;
    if (rp->is_defaulted && rp->function_def_number ==
                                                    NULL_function_def_number) {
#if GENERATE_SOURCE_SEQUENCE_LISTS
      a_name_reference_ptr  name_ref = rp->source_corresp.name_references;
      turn_routine_primary_sse_into_secondary_sse(rp);
      rp->declared_type = NULL;
      for (; name_ref != NULL; name_ref = name_ref->next) {
        if (!name_ref->used_in_primary_declarator) {
          name_ref->used_in_primary_declarator = FALSE;
          break;
        }  /* if */
      }  /* for */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      sym->defined = FALSE;
      rp->defined = FALSE;
      rp->is_defaulted = FALSE;
#if IA64_ABI
      rp->inline_in_class_definition = FALSE;
#endif /* IA64_ABI */
      skip_typerefs(rp->type)
         ->variant.routine.extra_info->exception_specification =
                                                rtsp->exception_specification;
      issue_redef_diag(&locator->source_position, sym, es_warning);
    }  /* if */
  }  /* if */
  if (sym == NULL || (sym->defined && !microsoft_out_of_class_redecl)) {
    /* Error case. */
    a_routine_ptr        other_rp = NULL;
    a_symbol_header_ptr  hdr = locator->symbol_header;
    if (sym != NULL) {
      /* Type was okay, but this member function has a body. */
      a_routine_type_supplement_ptr  other_rtsp;
      issue_redef_diag(&locator->source_position, sym);
      other_rp = sym->variant.routine.ptr;
      other_rtsp = rout_type_supp(other_rp->type);
      rtsp->this_class = other_rtsp->this_class;
      rtsp->has_this_param = other_rtsp->has_this_param;
      rtsp->qualifiers = other_rtsp->qualifiers;
      rtsp->this_qualifiers = other_rtsp->this_qualifiers;
    } else {
      /* In the error case assume the member function is nonstatic and give
         it an implicit this parameter type.  This will prevent an error from
         being issued on a direct reference to a nonstatic data member in the
         function body. */
      rtsp->this_class = class_type;
      rtsp->has_this_param = TRUE;
    }  /* if */
    /* An error has been detected.  Make a "fake" symbol and routine entry so
       that the routine definition can proceed. */
    /* "Enter" the symbol using an error locator -- this means a symbol
       entry will be created but it will not be added to any lists.  Then
       we'll restore the header to the new symbol, so that the correct name
       will be available in diagnostics. */
    set_to_error_locator(*locator);
    sym = enter_local_symbol((a_symbol_kind)sk_routine, locator,
                             DEPTH_OF_FILE_SCOPE,
                             /*suppress_redecl_error=*/TRUE);
    sym->header = hdr;
    rp = make_routine(type_ptr, (a_storage_class)sc_unspecified,
                      NO_SCOPE_DEPTH);
    sym->variant.routine.ptr = rp;
    set_source_corresp(&(rp->source_corresp), sym);
    set_class_membership(sym, &rp->source_corresp, class_type);
    if (other_rp != NULL) {
      set_routine_special_kind(rp, other_rp->special_kind);
      rp->variant = other_rp->variant;
    }  /* if */
    dps->prev_type = *old_type = type_ptr;
  } else {
    /* A member function symbol with a compatible type was found. */
    rp = sym->variant.routine.ptr;
    orig_pos = sym->decl_position;
    dps->prev_type = *old_type = routine_symbol_type(sym);
#if GNU_EXTENSIONS_ALLOWED
    if (gcc_pragma_options_stack != NULL) {
      /* Attach a synthesized "target" attribute from a "#pragma GCC target"
         if applicable. */
      attach_target_pragma_attribute(&dps->prefix_attributes);
    }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GNU_FUNCTION_MULTIVERSIONING
    if (gpp_mode && gnu_version >= 40800 &&
        is_multiversion_representative(rp)) {
      /* A GNU function multiversion representative function was found;
         this definition must have a "target" attribute that matches a
         previously declared member function. */
      an_attribute_ptr  target_ap = NULL;
      if (dps->prefix_attributes != NULL) {
        target_ap = find_last_target_attribute(dps->prefix_attributes);
      }  /* if */
      if (target_ap == NULL) {
        /* Missing "target" attributes. */
        pos_sy_error(ec_missing_target_attribute, &locator->source_position,
                     sym);
      } else {
        /* Apply the "target" attribute(s). */
        a_boolean     found_existing;
        a_routine_ptr target = NULL;
        if (process_multiversion_function(target_ap, NO_SCOPE_DEPTH, rp,
                                          &target, &found_existing)) {
          if (found_existing) {
            sym = symbol_for(target);
            if (!sym->defined) {
              /* Use the target-specific version routine and symbol. */
              rp = target;
            } else {
              /* Member function already has a definition. */
              issue_redef_diag(&locator->source_position, sym);
            }  /* if */
          } else {
            /* No declared member function has the same "target" attributes. */
            pos_sy_error(ec_no_matching_target_attribute, &target_ap->position,
                         sym);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
#endif /* GNU_FUNCTION_MULTIVERSIONING */
    if (rp->is_declared_constexpr != ((dps->dso_flags & DSO_CONSTEXPR) != 0) ||
        rp->is_consteval != ((dps->dso_flags & DSO_CONSTEVAL) != 0)) {
      /* The previous declaration doesn't match the current one wrt. the
         "constexpr" specifier.  Issue an error. */
      an_error_code  ec;
      if (rp->is_consteval) {
        ec = ec_previous_consteval_decl_conflict;
      } else if (rp->is_constexpr) {
        ec = ec_previous_constexpr_decl_conflict;
      } else if ((dps->dso_flags & DSO_CONSTEVAL) != 0) {
        ec = ec_previous_nonconsteval_decl_conflict;
      } else {
        ec = ec_previous_nonconstexpr_decl_conflict;
      }  /* if */
      pos_sy_error(ec, rp->is_declared_constexpr ? &dps->declarator_pos
                                                 : &dps->constexpr_pos,
                   sym);
      if (!rp->is_constexpr) {
        if (dps->dso_flags & DSO_CONSTEXPR) {
          rp->is_declared_constexpr = TRUE;
        } else {
          rp->is_consteval = TRUE;
        }  /* if */
        rp->is_constexpr = TRUE;
      }  /* if */
    }  /* if */
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (ms_extensions) {
      if (microsoft_out_of_class_redecl && microsoft_version >= 1310 &&
          !in_microsoft_implementation_key_mapping_region &&
          (!is_member_function_symbol(sym) || !rp->is_template_function)) {
        /* Recent microsoft compilers only accept the out-of-class
           redeclaration syntax for template specializations, or inside a
           region of code delimited by #pragma start_map_region and
           #pragma stop_map_region. */
        pos_sy_error(ec_member_function_redecl_outside_class,
                     &locator->source_position, sym);
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* The types may be compatible but not identical.  Create (in rout_type)
       a composite type.  First copy the implicit this param type pointer
       into rout_type:  it is always wrong for nonstatic member functions.
       Also be sure the routine name linkage for the type is right. */
    adjust_member_routine_type(rout_type, *old_type);
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (ms_extensions && rtsp->calling_convention != cc_default) {
      /* A calling convention was specified on the out-of-class definition.
         Note this must be checked after the function is adjusted, this ensures
         calling_conventions_are_compatible has its preconditions satisfied. */
      if (rtsp->calling_convention == cc_thiscall &&
          !routine_type_is_nonstatic_member_function(*old_type)) {
        /* The "__thiscall" calling convention can only be applied to nonstatic
           member functions. */
        pos_error(ec_thiscall_requires_nonstatic_member,
                  &locator->source_position);
      } else if (!calling_conventions_are_compatible(*old_type, rout_type)) {
        /* An out-of-class definition should not change the calling convention
           declared in the class definition (not specifying a calling
           convention never amounts to a change).  (A similar GNU-mode test is
           delayed until attributes are applied.) */
        pos_error(ec_conflicting_calling_conventions,
                  &locator->source_position);
      }  /* if */
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    if (noexcept_enabled && rtsp->exception_specification == NULL) {
      if (special_kind_is(rp, sfk_destructor)) {
        /* [except.spec]/p8 (N4762) "The exception specification for [...] a
           destructor without a noexcept-specifier, is potentially-throwing
           if and only if any of the destructors for any of its potentially
           constructed subobjects is potentially throwing." */
        update_routine_type_exception_specification_if_needed(rp, &rout_type);
      } else if (special_kind_is(rp, sfk_operator) &&
            is_delete_operator(rp->variant.opname_kind)) {
        /* For an "operator delete" an exception specification may be
           generated.  Use the specification used in the in-class
           declaration. */
        rtsp->exception_specification =
                          skip_typerefs(*old_type)->variant.routine.extra_info
                                                  ->exception_specification;
      }  /* if */
    }  /* if */
    /* Do compatibility checking on the throw specification. */
    (void)check_exception_specification(rout_type, sym,
                                        &func_info->throw_position,
                                        /*is_redecl=*/TRUE);
#if GENERATE_SOURCE_SEQUENCE_LISTS
    /* Record the default arguments of the current declaration before
       reconcile_routine_types is called. */
    if (func_info->declared_type != NULL) {
      copy_routine_type_default_args(type_ptr, func_info->declared_type);
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    /* Note that type_ptr is passed to reconcile_routine_types instead of
       rout_type.  This is intended.  type_ptr should differ from rout_type
       only by the presence of a top-level type qualifiers.  These will only
       appear on a function type when support for near and far is enabled. */
#if CHECKING
    if (same_entities(rout_type, type_ptr)) {
      /* Okay. */
#if NEAR_AND_FAR_ALLOWED
    } else if (near_and_far_enabled()) {
      a_type_qualifier_set  qual = get_top_level_type_qualifiers(type_ptr);
      check_assertion(qual == TQ_NEAR || qual == TQ_FAR);
#endif /* NEAR_AND_FAR_ALLOWED */
    } else {
      unexpected_condition();
    }  /* if */
#endif /* CHECKING */
    rp = sym->variant.routine.ptr;
    /* Ordinarily, the current declaration is the definition, and
       reconcile_routine_types should therefore preserve the new type.
       However, for Microsoft out-of-class redeclarations this isn't an actual
       definition.  Furthermore, such out-of-class redeclarations may appear
       in local scopes, including the scope of the function itself:
            struct S { void f(int i); };
            void S::f(int i) { void S::f(int i); }
       Changing the type of the function while it is being defined would lead
       to subtle errors later on.  So the original type is preserved in that
       case. */
    reconcile_routine_types(
                         rp, type_ptr,
                         /*preserve_rout_type=*/microsoft_out_of_class_redecl,
                         /*preserve_type_ptr=*/!microsoft_out_of_class_redecl,
                         dps);
    if (special_kind_is(rp, sfk_constructor)) {
      /* If the routine is a default constructor or a copy constructor, it may
         be that this has not yet been recorded in the symbol.  (This becomes
         possible if there are default arguments in the definition.) */
      a_class_symbol_supplement_ptr  cssp;
      cssp = class_symbol_supp(symbol_for(class_type));
      if (!cssp->has_nontrivial_default_constructor &&
          is_default_constructor(rp, /*is_declarative_context=*/TRUE)) {
        /* This is a default constructor, so set the flag. */
        cssp->has_nontrivial_default_constructor = TRUE;
        if (cpp14_mode) {
          /* The resolution of Core issue 1344 makes it invalid to produce a
             special member by adding default arguments to an out-of-class
             definition. */
          pos_error(ec_member_special_after_class_definition,
                    &locator->source_position);
        }  /* if */
      }  /* if */
      if (!cssp->has_copy_constructor_for_const_object ||
          cssp->construction_by_bitwise_copy_allowed) {
        /* There are three flags associated with copy constructors. */
        a_type_qualifier_set  qualifiers;
        if (is_copy_constructor(rp, class_type, &qualifiers,
                                /*include_move_ctors=*/TRUE,
                                /*is_declarative_context=*/FALSE)) {
          /* This is a copy constructor.  Note that the presence of a user-
             defined copy constructor means that construction by bitwise
             copying is not done. */
          cssp->has_copy_constructor = TRUE;
          cssp->has_copy_constructor_for_const_object =
                                               ((qualifiers & TQ_CONST) != 0);
          cssp->construction_by_bitwise_copy_allowed = FALSE;
          if (cpp14_mode &&
              !is_copy_constructor_type(*old_type, class_type, &qualifiers,
                                        /*include_move_ctors=*/TRUE,
                                        /*is_declarative_context=*/FALSE)) {
            /* The resolution of Core issue 1344 makes it invalid to produce
               a special member by adding default arguments to an
               out-of-class definition. */
            pos_error(ec_member_special_after_class_definition,
                      &locator->source_position);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
    /* If this is a member function of an instantiation of a class
       template, mark this as a specialization.  However, since the newer
       template<> syntax was not used, mark it as using the old syntax. */
    if (sym->variant.routine.instance_ptr != NULL) {
      check_old_specialization_allowed(sym, &locator->source_position);
      sym->variant.routine.ptr->is_specialized = TRUE;
      sym->variant.routine.ptr->specialized_with_old_syntax = TRUE;
      sym->variant.routine.instance_ptr->instantiation_required = FALSE;
    }  /* if */
    /* Mark the routine to indicate that, though really belonging to the
       scope of its parent class, it is defined elsewhere. */
    if (!microsoft_out_of_class_redecl) rp->defined_outside_of_parent = TRUE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
    if (!microsoft_out_of_class_redecl) {
      record_symbol_declaration(SRK_DECLARATION | SRK_DEFINITION, sym,
                                &locator->source_position,
                                func_info->declarator_ssep);
      set_routine_declared_type(rp, func_info->declared_type);
    } else {
      /* This is just a redeclaration, and hence the declared type is attached
         to a secondary source sequence entry (if one was created). */
      record_symbol_declaration(SRK_DECLARATION, sym,
                                &locator->source_position,
                                func_info->declarator_ssep);
      if (!source_sequence_entries_disallowed) {
        a_source_sequence_entry_ptr  decl_ssep = NULL;
        a_src_seq_secondary_decl_ptr  sssdp;
        decl_ssep = last_matching_source_sequence_entry((char *)rp);
        check_assertion(decl_ssep != NULL);
        sssdp = (a_src_seq_secondary_decl_ptr)decl_ssep->entity.ptr;
        sssdp->declared_type = func_info->declared_type;
      }  /* if */
    }  /* if */
    /* Set the declared-type in the routine entry. */
#else /* !GENERATE_SOURCE_SEQUENCE_LISTS */
    if (!microsoft_out_of_class_redecl) {
      mark_defined(sym, &locator->source_position);
    } else {
      mark_declared(sym, &locator->source_position);
    }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
    copy_source_position(locator->source_position,
                         rp->source_corresp.decl_position);
#if EXTRA_SOURCE_POSITIONS_IN_IL
    update_decl_pos_info(&rp->source_corresp, decl_pos_block);
#if DEBUG
    if (decl_pos_block != NULL) {
      if (debug_level >= 3 || db_flag_is_set("dump_decl_pos_info")) {
        fprintf(f_debug, "decl-pos info for member function def\n");
        db_decl_pos_info(sym);
      }  /* if */
    }  /* if */
#endif /* DEBUG */
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  }  /* if */
  if (!exceptions_enabled) {
    /* Check whether a diagnostic should be issued on this exception
       specification, and issue one if needed. */
    issue_no_exception_support_diag_on_throw_spec(func_info);
  }  /* if */
  /* Record whether the routine was inline before this declaration (used when
     applying the GNU "noinline" attribute). */
  dps->inline_on_earlier_decl = rp->is_inline;
  if (func_info->is_inline) {
    if (!rp->is_inline) {
      set_inline_flag(rp, TRUE);
      if (rp->called) {
        /* In the ARM, member functions could not be redeclared inline after
           being called.  This restriction has been eliminated in the
           standard. */
        pos_sy_remark(ec_called_function_redeclared_inline,
                      &locator->source_position, sym);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Reset the storage class and name linkage. */
  if (!extern_inline_allowed && rp->is_inline) {
    rp->storage_class = (a_storage_class)sc_static;
    rp->source_corresp.name_linkage = (a_name_linkage_kind)nlk_internal;
  } else if (rp->source_corresp.name_linkage ==
                           (a_name_linkage_kind)nlk_cplusplus_external &&
             !microsoft_out_of_class_redecl) {
    /* The routine will have been given a storage class of sc_extern when it
       was originally declared; change it to sc_unspecified now that the
       definition has been seen. */
    rp->storage_class = (a_storage_class)sc_unspecified;
    if (!rp->is_inline) {
      /* Also set the referenced flag, assuming a reference from another
         translation unit. */
      rp->source_corresp.referenced = TRUE;
    }  /* if */
  }  /* if */
#if ASM_FUNCTION_ALLOWED
  if (func_info->is_asm_function &&
      rp->storage_class == (a_storage_class)sc_unspecified) {
    rp->storage_class = (a_storage_class)sc_asm;
  }  /* if */
#endif /* ASM_FUNCTION_ALLOWED */
  dps->sym = sym;
#if GNU_EXTENSIONS_ALLOWED
  report_gnu_postfix_attributes_on_function_definition(dps);
#endif /* GNU_EXTENSIONS_ALLOWED */
  if (!is_error_locator(*locator)) {
    /* Apply attributes. */
    attach_decl_attributes(dps, func_info->is_definition);
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (dps->ms_attributes != NULL) {
      apply_microsoft_attributes_to_routine(&dps->ms_attributes, rp);
    }  /* if */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
    /* Temporarily restore the position of the original declaration in the
       associated symbol so that diagnostics come out right. */
    saved_pos = sym->decl_position;
    sym->decl_position = orig_pos;
    update_routine_decl_modifiers(rp, &dps->decl_modifiers,
                                  &locator->source_position,
                                  /*is_redecl=*/TRUE,
                                  !microsoft_out_of_class_redecl,
                                  (a_boolean)func_info->is_inline);
    sym->decl_position = saved_pos;
#if MICROSOFT_EXTENSIONS_ALLOWED
  } else {
    /* Clear the attributes pointer to avoid cascading errors. */
    dps->ms_attributes = NULL;
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
  }  /* if */
  if (any_deferred_access_checks()) {
    /* Now that we know which function has been declared, recheck any
       access errors that occurred while scanning the declaration. */
    check_assertion(rp != NULL);
    perform_deferred_access_checks_for_function(rp);
  }  /* if */
  if (func_info->any_default_args) {
    set_parent_routine_for_closure_types_in_default_args(type_ptr, sym);
  }  /* if */
  /* If a lint-style "argsused" or "varargs" comment appeared, record that in
     the function type.  That will suppress any warnings about unused
     parameters or variable arguments.  Note that this is done before calling
     process_curr_construct_pragmas; otherwise the pragmas we're interested
     in would have been disposed of. */
  record_lint_argsused_and_varargs_state(sym);
  /* Do processing required for the rest of the pragmas, if any, that are
     bound to the current declaration. */
  process_curr_construct_pragmas(sym, (a_statement_ptr)NULL);
  *ext_sym = NULL;
  *linkage_ptr = idl_external;
#if DEBUG
  if (debug_level >= 3) {
    db_symbol(sym, "", 2);
  }  /* if */
#endif /* DEBUG */
  db_exit();
}  /* define_member_function */


static a_routine_ptr get_lambda_static_entry_point(a_routine_ptr  conv_op)
/*
conv_op is a lambda conversion function (an instance of a template in the case
of a generic lambda) whose definition is needed.  Return a routine entry for a
corresponding static entry point to the lambda's call operator, and, in the
case of a generic lambda, ensure that the call operator will be fully
instantiated.
*/
{
  a_type_ptr     closure_type = parent_class_of(conv_op), return_type;
  a_routine_ptr  static_entry_pt = NULL, lambda_body;
  
  lambda_body = lambda_body_for_closure(closure_type);
  check_assertion(lambda_body != NULL);
  if (conv_op->assoc_template == NULL ||
      conv_op->assoc_template->kind == templk_member_function) {
    /* For ordinary (i.e., non-generic) lambdas, the alternate (static) entry
       point for the call operator is the routine entry following the entry
       for the conversion function on the closure type's routines list. */
    if (routine_type_is_nonstatic_member_function(lambda_body->type)) {
      static_entry_pt = conv_op->next;
      check_assertion(special_kind_is(static_entry_pt,
                                      sfk_lambda_entry_point));
    } else {
      static_entry_pt = lambda_body;
    }  /* if */
  } else {
    /* For generic lambdas, the alternative entry for the call operator has to
       be partially instantiated at this point, and the corresponding call
       operator has to be fully instantiated. */
    a_template_ptr      entry_pt_templ, call_op_templ;
    a_symbol_ptr        entry_pt_templ_sym, call_op_templ_sym, instance_sym;
    a_routine_ptr       call_op, generic_call_op = lambda_body;
    a_template_arg_ptr  templ_arg_list;
    /* Get the call operator template and the static entry point template.
       We count on the fact that the template's list for the closure type has
       in order: (1) the call operator template, (2) the conversion function
       template, and (3) the static entry point template.  However, in
       Microsoft mode, there may be multiple conversion/entry-point pairs to
       account for different calling conventions. */
    if (generic_call_op == NULL) {
      /* This can happen with severe errors. */
      expect_error();
      return_type = error_type();
      call_op = NULL;
    } else {
      call_op_templ = generic_call_op->assoc_template;
      check_assertion(call_op_templ != NULL);
      call_op_templ_sym = symbol_for(call_op_templ);
      templ_arg_list = copy_template_arg_list(conv_op->template_arg_list);
      instance_sym = find_template_function(
                                          call_op_templ_sym, &templ_arg_list,
                                          /*explicit_arg_list_present=*/FALSE,
                                          &error_position);
      free_template_arg_list(templ_arg_list);
      check_assertion(instance_sym != NULL &&
                      symbol_is(instance_sym, sk_member_function));
      call_op = instance_sym->variant.routine.ptr;
      check_assertion(type_is(call_op->type, tk_routine));
      return_type = call_op->type->variant.routine.return_type;
      /* Ensure that the call operator will be fully instantiated so that the
         alternate entry point does not dangle. */
      set_instance_required(instance_sym, TRUE, SIR_DEFER_INLINE);
      if (!routine_type_is_nonstatic_member_function(call_op->type)) {
        /* For static lambda call operators, no separate entry point is
           needed. */
        static_entry_pt = call_op;
      }  /* if */
    }  /* if */
    if (static_entry_pt == NULL) {
      entry_pt_templ = conv_op->assoc_template->next;
      check_assertion(entry_pt_templ != NULL);
      entry_pt_templ_sym = symbol_for(entry_pt_templ);
      templ_arg_list = copy_template_arg_list(conv_op->template_arg_list);
      instance_sym = find_template_function(
                                          entry_pt_templ_sym, &templ_arg_list,
                                          /*explicit_arg_list_present=*/FALSE,
                                          &error_position);
      free_template_arg_list(templ_arg_list);
      check_assertion(instance_sym != NULL &&
                      symbol_is(instance_sym, sk_member_function));
      static_entry_pt = instance_sym->variant.routine.ptr;
      check_assertion(type_is(static_entry_pt->type, tk_routine) &&
                      special_kind_is(static_entry_pt,
                                      sfk_lambda_entry_point));
      /* Update the entry point's function type, including its return type
         (which may be deduced from the lambda's instantiated definition). */
      static_entry_pt->type->variant.routine.return_type = return_type;
      static_entry_pt->variant.lambda_call_operator = call_op;
      if (call_op != NULL) {
        static_entry_pt->storage_class = call_op->storage_class;
      }  /* if */
    }  /* if */
  }  /* if */
  return static_entry_pt;
}  /* get_lambda_static_entry_point */


void define_lambda_conversion_function(a_routine_ptr  conv_op)
/*
conv_op points to the entry for a conversion function of a closure type
(generated for a lambda expression).  conv_op->next points to a static member
that represents an alternate entry point for the closure's call operator.
Add a definition to conv_op that returns the address of the conv_op->next
routine.
*/
{
  a_routine_ptr    static_entry_pt;
  a_statement_ptr  block_stmt, return_stmt;
  a_scope_ptr      fn_scope;

  conv_op->storage_class = (a_storage_class)sc_unspecified;
  fn_scope = push_scope((a_scope_kind)sck_function, NO_SCOPE_NUMBER,
                        (a_type_ptr)NULL, conv_op);
  fn_scope->variant.routine.this_param_variable =
                             make_implicit_this_param_variable(conv_op->type); 
  conv_op->type->variant.routine.extra_info->assoc_routine = conv_op;
  static_entry_pt = get_lambda_static_entry_point(conv_op);
  return_stmt = alloc_statement(stmk_return, /*compiler_generated=*/TRUE);
  return_stmt->expr = function_addr_expr(static_entry_pt);
  mark_routine_referenced(static_entry_pt);
  block_stmt = alloc_statement(stmk_block, /*compiler_generated=*/TRUE);
  block_stmt->variant.block.statements = return_stmt;
  block_stmt->variant.block.extra_info->end_of_block_reachable = FALSE;
  fn_scope->assoc_block = block_stmt;
  pop_scope();
  check_assertion(conv_op->is_inline);
  if (instantiate_extern_inline && !conv_op->is_consteval) {
    add_to_inline_function_list(conv_op);
  }  /* if */
  if (constexpr_lambdas_enabled) {
    /* The conversion operator is constexpr in C++17. */
    conv_op->is_constexpr = TRUE;
    fn_scope->is_constexpr_routine = TRUE;
    static_entry_pt->is_constexpr = TRUE;
  }  /* if */
}  /* define_lambda_conversion_function */


void scan_defaulted_or_deleted_definition(a_decl_parse_state    *dps,
                                          a_func_info_block     *func_info)
/*
A "= default;" or "= delete;" definition is next (although the semicolon may
be missing) and func_info reflects this already.  Update the associated IL
entry and scan past the first two tokens.  This is called for non-member
functions and for out-of-class definitions of member functions, but not for
in-class definitions of member functions.
*/
{
  a_routine_ptr  routine_ptr = dps->sym->variant.routine.ptr;

  check_assertion(curr_token == tok_assign);
  (void)get_token();
  check_defaulted_or_deleted_function(dps, func_info, &pos_curr_token);
  if (routine_ptr->is_defaulted && routine_ptr->is_declared_constexpr &&
      special_kind_is(routine_ptr, sfk_constructor) &&
      is_default_constructor(routine_ptr, /*is_declarative_context=*/TRUE)) {
    (void)check_if_constexpr_generated_default_constructor(
                                                 parent_class_of(routine_ptr));
  }  /* if */
  force_definition_of_compiler_generated_routine(routine_ptr);
  check_assertion(curr_token == tok_delete || curr_token == tok_default ||
                  (ms_extensions && microsoft_version >= 1400 &&
                   check_context_sensitive_keyword(tok_default, "default")));
  (void)get_token();
}  /* scan_defaulted_or_deleted_definition */


void function_definition(a_symbol_locator      *locator,
                         a_decl_parse_state    *dps,
                         a_func_info_block     *func_info,
                         a_decl_pos_block_ptr  decl_pos_block)
/*
Scan a function definition.  The declarator has already been scanned; the
old-style parameter declarations and the compound statement for the body are
still to come.  *locator is the locator to be used to enter the function
symbol; *dps describes various properties of the declaration, including the
type for the function; *func_info contains information about parameters, as
well as field function_type_from_typedef (when it is FALSE, the function type
came from the declarator; when it is TRUE an error is reported).
This function is also called in the case of a nondefining out-of-class
member declaration (allowed in Microsoft mode only).
*/
{
  a_symbol_ptr                   sym = locator->specific_symbol;
  a_symbol_ptr                   ext_sym;
  a_routine_ptr                  routine_ptr;
  a_param_id_ptr                 param_id;
  an_id_linkage_kind             linkage;
  a_type_ptr                     old_type, unqualified_rout_type;
  a_routine_type_supplement_ptr  extra_info;
  a_boolean                      prototyped;
  a_param_type_ptr               ptp;
  a_decl_flag_set                flags;

  db_enter(3, "function_definition");
  /* The top type (function) must have come from a declarator, not from a
     typedef (see constraints section of 3.7.1, and associated footnote). */
  if (func_info->function_type_from_typedef) {
    pos_error(ec_function_type_must_come_from_declarator, &error_position);
    /* Build a copy of the routine type that can be used below, to avoid
       further error recovery problems, and because we need a non-shared
       routine type entry that we can modify. */
    dps->type = copy_routine_type_with_param_types(dps->type,
                                                   /*copy_default_args=*/TRUE);
    unqualified_rout_type = skip_typerefs(dps->type);
  } else {
    unqualified_rout_type = skip_typerefs(dps->type);
    check_assertion(unqualified_rout_type->kind == (a_type_kind)tk_routine);
  }  /* if */
  extra_info = unqualified_rout_type->variant.routine.extra_info;
  prototyped = extra_info->prototyped;
  if (sym != NULL && sym->is_class_member) {
    /* This is the definition of a member function. */
    check_assertion(prototyped);
    if (func_info->is_defaulted &&
        exceptions_enabled && implicit_noexcept_enabled &&
        extra_info->exception_specification == NULL) {
      /* In the case of a destructor or an operator delete, use the implicit
         exception specification.  See also
         update_routine_type_exception_specification_if_needed. */
      if (locator->is_destructor_name) {
        if (symbol_is(sym, sk_member_function)) {
          a_type_ptr  rtp = routine_symbol_type(sym);
          extra_info->exception_specification =
                                skip_typerefs(rtp)->variant.routine.extra_info
                                                  ->exception_specification;
        }  /* if */
      } else if (locator->is_operator_name &&
                 is_delete_operator(locator->variant.opname)) {
        add_noexcept_specification(extra_info);
      }  /* if */
    }  /* if */
    define_member_function(locator, dps, func_info, &linkage, &old_type,
                           &ext_sym, decl_pos_block);
  } else {
    if (!prototyped) {
      /* Old-style id list.  Before calling decl_routine scan the
         parameter declarations.  It is important for the routine type to
         include all the parameter information in order to do overloading
         involving both prototyped and old-style functions. */
      a_param_type_ptr   old_style_param_types = NULL;
      a_param_type_ptr   end_old_style_param_types = NULL;
      /* Push the name scope for the parameter declarations. */
      (void)push_scope((a_scope_kind)sck_func_prototype,
                       func_info->scope_number, dps->type,
                       (a_routine_ptr)NULL);
      /* Remember the scope number for later use when the body is scanned. */
      func_info->scope_number = scope_stack[depth_scope_stack].number;
#if ASM_FUNCTION_ALLOWED
      if (func_info->is_asm_function && curr_token != tok_lbrace) {
        /* If an asm function is not prototyped, all its old-style params
           have to be implicitly declared. */
        pos_error(ec_asm_func_must_be_prototyped, &pos_curr_token);
      }  /* if */
#endif /* ASM_FUNCTION_ALLOWED */
      while (curr_token == tok_identifier ||
             is_decl_start(IDS_REAL_DECLARATOR_ALLOWED)) {
        /* This declaration is checked to make sure the identifier is on the
           param_id_list. */
        declaration(/*function_definition_allowed=*/FALSE, 
                    /*is_old_style_param_decl=*/TRUE,
                    /*is_top_level_declaration=*/FALSE, 
                    /*marked_as_gnu_extension=*/FALSE, 
                    func_info->param_id_list, (a_source_range *)NULL);
      }  /* while */
#if GNU_EXTENSIONS_ALLOWED
      if (gcc_mode && curr_token == tok_ellipsis) {
        /* GNU C allows an old-style parameter list to be followed by
           an ellipsis ("...") to indicate varargs parameters. */
        extra_info->has_ellipsis = TRUE;
        (void)get_token();
      }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if GENERATE_SOURCE_SEQUENCE_LISTS
      /* Transfer the source sequence list in the function prototype scope
         over to the func_info block. */
      func_info->prototype_scope_ss_list =
                         scope_stack[depth_scope_stack].source_sequence_list;
      scope_stack[depth_scope_stack].source_sequence_list = NULL;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      /* Scan the list of identifiers, assigning types to any that remain
         undeclared, and create the param type entries. */
      for (param_id = func_info->param_id_list;
           param_id != NULL;
           param_id = param_id->next) {
        if (param_id->type == NULL) {
          a_symbol_ptr  param_sym = param_id->symbol;
          /* Enter any undeclared parameters with a type of int. */
          param_id->type = integer_type((an_integer_kind)ik_int);
          param_id->declared_type = param_id->type;
          param_id->storage_class = (a_storage_class)sc_auto;
          param_id->implicitly_declared = TRUE;
          copy_source_position(param_sym->decl_position, param_id->type_pos);
          /* Symbols for explicitly declared parameters will already have been
             entered into the symbol table; so the same for parameters that
             are implicitly declared.  In GNU modes, parameters may be marked
             "ambiguous" if they have a duplicate name.  Since such GNU cases
             are diagnosed elsewhere (in decl_parameter), we inhibit the
             redeclaration diagnostic here. */
          reenter_symbol(param_sym, decl_scope_level,
                         (a_boolean)param_sym->ambiguous);
          if (c99_mode) {
            /* In C99, implicit declarations are no longer allowed. */
            pos_sy_diagnostic(strict_ansi_mode ?
                               strict_ansi_discretionary_severity : es_warning,
                              ec_undeclared_parameter,
                              &param_sym->decl_position, param_sym);
          }  /* if */
        }  /* if */
        /* The param_type entry must be allocated in the file-scope
           region. */
        ptp = make_param_type(param_id->type, &param_id->type_pos);
        ptp->declared_type = param_id->declared_type;
        update_param_top_level_qualifiers(ptp);
        /* Now build the list of parameter types that is attached to the 
           routine type (needed for checking type compatibility -- see
           types_are_compatible). */
        if (old_style_param_types == NULL) {
          old_style_param_types = ptp;
        } else {
          check_assertion(end_old_style_param_types != NULL);
          end_old_style_param_types->next = ptp;
        }  /* if */
        end_old_style_param_types = ptp;
      }  /* for */
      /* Set the type to the new type information from the old-style
         parameters just scanned. */
      extra_info->param_type_list = old_style_param_types;
      if (C_mode()) {
        /* Set a flag indicating that old style params were scanned.  This
           is done in case, when this declaration is reconciled with other
           declarations, the prototyped flag is changed -- e.g.,
             void f(int,int);
             void f(i,j) int i; int j { ... }
           where the type associated with the routine entry is marked as
           prototyped but the defining declaration is old-style. */
        extra_info->old_style_params_scanned = TRUE;
      } else {
        /* In C++ mode old style parameter declarations are permitted as
           an anachronism.  However, the internal representation should be
           the same as for a prototyped param list. */
        extra_info->prototyped = TRUE;
#if GENERATE_SOURCE_SEQUENCE_LISTS
        if (func_info->declared_type != NULL) {
          /* Replace the declared_type that was recorded in the func-info
             block with one that records the param-type entries. */
          /* It doesn't make any difference how copy_default_args is set;
             there shouldn't be any on an old-style declaration. */
          func_info->declared_type =
              copy_routine_type_with_param_types(dps->type,
                                                 /*copy_default_args=*/FALSE);
        }  /* if */
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
      }  /* if */
      /* Parameter symbols are not actually entered in the function
         prototype scope, but other symbols (in consequence of an error or
         a type declaration) may be.  Record them so that they can be
         transferred to the function scope later. */
      func_info->prototype_scope_symbols =
            assoc_pointers_block_of(&scope_stack[depth_scope_stack])->symbols;
      /* Process pragmas associated with the opening brace before the current
         scope is popped.  This means, for old-style param lists, a pragma
         immediately preceding the left brace is interpreted as belonging to
         the function prototype scope; it's different for prototyped
         param lists. */
      process_curr_token_pragmas();
      /* Before popping the scope, move the vla_fixup_list from the
         scope_stack to func_info. */
      func_info->vla_fixup_list =
                         scope_stack[depth_scope_stack].vla_fixup_list;
      scope_stack[depth_scope_stack].vla_fixup_list = NULL;
      /* Pop the function prototype scope. */
      pop_scope();
    } else {
      /* Prototyped. */
      /* Process pragmas associated with the opening brace before pushing
         the function scope.  This means, for prototyped param lists, a pragma
         immediately preceding the left brace is interpreted as belonging to
         the file; it's different for old-style param lists. */
      process_curr_token_pragmas();
    }  /* if */
    /* Create the symbol entry and routine entry for the routine. */
    decl_routine(locator, dps, func_info, (SRK_DECLARATION | SRK_DEFINITION),
                 &linkage, &old_type, &ext_sym, decl_pos_block);
  }  /* if */
  if (relaxed_abstract_checking && !func_info->is_deleted) {
    /* With relaxed abstract checking (P0929R2), parameter types are only
       checked in definitions and calls, not in non-defining declarations. */
    for (param_id = func_info->param_id_list; param_id != NULL;
         param_id = param_id->next) {
      a_type_ptr tp = skip_typerefs(param_id->type);
      if (is_immediate_class_type(tp) &&
          tp->variant.class_struct_union.abstract) {
        abstract_class_diagnostic(es_error, ec_abstract_class_param_type,
                                  param_id->type, &param_id->type_pos);
      }  /* if */
    }  /* for */
  }  /* if */
  routine_ptr = dps->sym->variant.routine.ptr;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  wrapup_sse_for_simple_decl(dps);
  routine_ptr->declared_storage_class = dps->declared_storage_class;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  /* Run end-of-declaration-parsing actions prior to scanning the function
     body. */
  run_end_of_parse_actions(dps, /*more_declarators=*/FALSE);
  /* Now scan the function body, except if we're dealing with the special
     Microsoft and GNU extension case that allows a nondefining out-of-class
     member declaration. */
  if (curr_token == tok_semicolon &&
      (ms_extensions || (gpp_mode && gnu_version < 30400)) &&
      (locator->is_class_member || locator->is_error)) {
    /* There is no definition. */
    check_assertion(!gpp_mode || routine_ptr->is_specialized ||
                    locator->is_error);
  } else if (func_info->is_deleted || func_info->is_defaulted) {
    scan_defaulted_or_deleted_definition(dps, func_info);
    (void)required_token(tok_semicolon, ec_exp_semicolon);
  } else {
    flags = SFB_NO_FLAGS;
    if ((dps->dso_flags & DSO_HAS_EXPLICIT_TYPE_SPECIFIER) == 0) {
      flags |= SFB_IMPLICITLY_DECLARED_RETURN_TYPE;
    }  /* if */
#if MODULE_ID_NEEDED
    if (!locator->is_error) {
      /* If a module id for the current translation unit has not yet been
         created, see if the name of this routine is suitable.  Do this before
         scanning the function body to increase the chance that a module id
         is in place early (as lowering is delayed until a module id has
         been selected). */
      use_variable_or_routine_for_module_id_if_needed(
                                                  &routine_ptr->source_corresp,
                                                  iek_routine);
    }  /* if */
#endif /* MODULE_ID_NEEDED */
    a_boolean need_func_tokens_for_module =
                          (create_module_unit &&
                           is_routine_definition_exported_inline(routine_ptr));
    if (need_func_tokens_for_module) {
      push_lexical_state_stack();
      begin_caching_fetched_tokens(/*include_curr_token=*/TRUE);
    }  /* if */
    scan_function_body(routine_ptr, func_info, flags);
    if (need_func_tokens_for_module) {
      end_caching_fetched_tokens();
      save_function_definition_for_module_write(
                       routine_ptr,
                       shared_obj<a_token_cache>(*curr_lexical_state_cache()));
      pop_lexical_state_stack();
    }  /* if */
#if CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
    /* Save the symbol associated with the most recent constructor or
       destructor for which a definition was supplied outside of the
       class definition.  Clear this value when any other member function
       is processed.  This is used to emulate a cfront name lookup bug.
       See check_for_cfront_name_lookup_bug in symbol_tbl.c for more
       information. */
    if (routine_ptr->source_corresp.is_class_member) {
      if (cfront_2_1_mode) {
        if (special_kind_is(routine_ptr, sfk_constructor) ||
            special_kind_is(routine_ptr, sfk_destructor)) {
          last_ctor_or_dtor_sym = dps->sym;
        } else {
          last_ctor_or_dtor_sym = NULL;
        }  /* if */
      }  /* if */
    }  /* if */
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */
  }  /* if */

  db_exit();
}  /* function_definition */


static a_variable_ptr implicitly_generated_param_variable(a_param_type  *ptp)
/*
Allocate a parameter variable corresponding to the given param-type entry and
return a pointer to it.
*/
{
  a_variable_ptr vp;

  vp = make_param_variable(ptp->type, sc_auto);
  vp->variant.assoc_param_type = ptp;
  add_to_parameters_list(vp);
  return(vp);
}  /* implicitly_generated_param_variable */


static void make_generated_constructor_body(a_scope_ptr  scope)
/*
Create the body for a generated constructor (this could be a default
constructor, a default copy constructor, or an inheriting constructor).
The heavy lifting for this function is mostly in ctor_initializer, which
generates the implicit mem-initializer constructs that do the actual member
construction (if any is needed).
*/
{
  a_routine_ptr                  rp;
  a_routine_type_supplement_ptr  rtsp;
  a_param_type_ptr               ptp;
  a_constructor_init_ptr         cip;

  db_enter(4, "make_generated_constructor_body");
  rp = scope->variant.routine.ptr;
  /* Create the parameter variable -- needed for copy constructors only. */
  rtsp = (skip_typerefs(rp->type))->variant.routine.extra_info;
  for (ptp = rtsp->param_type_list; ptp != NULL; ptp = ptp->next) {
    (void)implicitly_generated_param_variable(ptp);
  }  /* if */    
  /* Create entries describing constructions to be done in the wrapper code. */
  if (rp->is_inheriting_ctor) {
    cip = ctor_inits_for_inheriting_ctor(rp);
  } else {
    cip = ctor_initializer(rp, /*user_defined=*/FALSE, /*fields_only=*/FALSE);
  }  /* if */
  scope->variant.routine.constructor_inits = cip;
  /* Create a statement block that is empty except for the return statement. */
  scope->assoc_block = alloc_statement(stmk_block,/*compiler_generated=*/TRUE);
  scope->assoc_block->variant.block.statements =
                     alloc_statement(stmk_return, /*compiler_generated=*/TRUE);
  /* See if the fact that this constructor is defined forces definition
     of virtual functions of the class. */
  require_definitions_of_virtual_functions_due_to_definition_of(rp);
  db_exit();
}  /* make_generated_constructor_body */


static void make_default_destructor_body(a_scope_ptr  scope)
/*
Create the body for a default destructor.  It will return no value.
*/
{
  a_routine_ptr  rp;
  a_type_ptr     class_type;

  db_enter(4, "make_default_destructor_body");
  rp = scope->variant.routine.ptr;
  class_type = parent_class_of(rp);
  if (!type_is(class_type, tk_union)) {
    /* Create entries describing destructions to be done in the wrapper
       code.  Do not do this for a union (the only union types that should
       get here are those created by std::meta::define_aggregate). */
    scope->variant.routine.constructor_inits = dtor_initializer(rp);
  }  /* if */
  /* Create a statement block that is empty except for the return
     statement. */
  scope->assoc_block = alloc_statement(stmk_block,/*compiler_generated=*/TRUE);
  scope->assoc_block->variant.block.statements =
                     alloc_statement(stmk_return, /*compiler_generated=*/TRUE);
  /* See if the fact that this destructor is defined forces definition
     of virtual functions of the class. */
  require_definitions_of_virtual_functions_due_to_definition_of(rp);
  db_exit();
}  /* make_default_destructor_body */


static a_statement_ptr make_assignment_call(an_expr_node_ptr  source_expr,
                                            an_expr_node_ptr  dest_expr,
                                            a_routine_ptr     rp,
                                            a_source_position *err_pos)
/*
Return a statement pointer that represents a call to an assignment operator.
source_expr is an lvalue for the source of the assignment; dest_expr is an
lvalue for the destination.  rp is the pointer to the routine entry for
the operator= function.  source_expr and dest_expr will be converted as
necessary for use as the argument and the "this" parameter of the call.
*err_pos is the source position for diagnostics.
*/
{
  a_param_type_ptr  ptp;
  a_statement_ptr   sp;
  a_routine_type_supplement_ptr
                    rtsp = rout_type_supp(skip_typerefs(rp->type));
  a_type_ptr        param_class, arg_class;

  /* Get the first parameter of the assignment operator, which represents the
     source type. */
  ptp = rtsp->param_type_list;
  /* Convert the source for use as the argument, e.g., cast it to a base
     class or add a copy constructor call. */
  source_expr = prep_generated_arg_expr(source_expr, ptp, err_pos);
  /* Convert the lvalue for the destination into a pointer for the "this"
     argument. */
  arg_class = skip_typerefs(dest_expr->type);
  param_class = rtsp->this_class;
  dest_expr = add_address_of_to_node(dest_expr);
  if (param_class != arg_class) {
    /* Presumably a base-class operator= made accessible via a member
       using-declaration. */
    a_base_class_ptr  bcp = find_base_class_of(arg_class, param_class);
    if (bcp != NULL) {
      dest_expr = base_class_selection_expr(dest_expr, bcp);
    } else {
      expect_error();
    }  /* if */
  }  /* if */
  /* Calls generated are non-virtual; see [class.copy]/13 in the C++
     standard. */
  sp = make_call_assignment_statement(rp, /*suppress_virtual=*/TRUE,
                                      dest_expr, source_expr, err_pos);
  return sp;
}  /* make_assignment_call */


static an_expr_node_ptr conv_array_expr_to_underlying_ptr(
                                                       an_expr_node_ptr  expr)
/*
expr is an expression for an lvalue or rvalue array.  Do the array-to-pointer
decay on it, and return a pointer to the decayed expression.  For a multi-
dimensional array, return a pointer to the first non-array element.
*/
{
  for (;;) {
    expr = conv_array_expr_to_pointer(expr);
    if (!(is_pointer_type(expr->type) &&
          is_array_type(type_pointed_to(expr->type)))) {
      break;
    }  /* if */
    expr = add_indirection_to_node(expr);
  }  /* for */
  return expr;
}  /* conv_array_expr_to_underlying_ptr */


static an_expr_node_ptr add_subscript_to_ptr_expr(an_expr_node_ptr  ptr_expr,
                                                  a_variable_ptr    idx_vp)
/*
Return an expression "ptr[idx]" with ptr and idx as described by ptr_expr and
idx_vp, respectively.
*/
{
  an_expr_node_ptr  result_expr;

  ptr_expr->next = var_rvalue_expr(idx_vp);
  result_expr = make_operator_node((an_expr_operator_kind)eok_subscript,
                                   type_pointed_to(ptr_expr->type), ptr_expr);
  result_expr->is_lvalue = TRUE;
  return result_expr;
}  /* add_subscript_to_ptr_expr */


static an_expr_node_ptr lvalue_for_source_param(a_variable_ptr source_var)
/*
source_var is the variable for the source parameter of an operator=
assignment function.  Create and return an lvalue that refers to the source
object, including for the case where the parameter has a reference type.
*/
{
  an_expr_node_ptr  source_expr;

  /* A compiler-generated operator= function always has a reference-typed
     source parameter, but that is not necessarily the case for a defaulted
     operator=. */
  if (is_any_reference_type(source_var->type)) {
    source_expr = var_rvalue_expr(source_var);
    source_expr->position = error_position;
    source_expr = add_ref_indirection_to_node(source_expr);
  } else {
    source_expr = var_lvalue_expr(source_var);
  }  /* if */
  source_expr->position = error_position;
  return source_expr;
}  /* lvalue_for_source_param */


static an_expr_node_ptr assignment_dest_ptr_expr(a_variable_ptr dest_var)
/*
Return a pointer expression for the object being assigned to in a defaulted
assignment operator.  dest_var is the explicit object parameter variable
when the operator has an explicit "this" parameter; otherwise dest_var is
NULL and the implicit "this" parameter is used.
*/
{
  an_expr_node_ptr expr;

  if (dest_var != NULL) {
    expr = add_address_of_to_node(lvalue_for_source_param(dest_var));
  } else {
    expr = this_param_value_expr();
  }  /* if */
  return expr;
}  /* assignment_dest_ptr_expr */


static void make_default_assignment_body(a_scope_ptr  scope)
/*
Create the body for a default assignment operator.  Typically it will
entail a series of member-wise and base-class-wise assignment operations:
based on the properties of the subobject, it will either call an assignment
operator routine or do bitwise assignment.
*/
{
  a_type_ptr                     class_type, tp, array_type;
  a_routine_ptr                  rout;
  a_routine_type_supplement_ptr  rtsp;
  a_statement_ptr                sp, top_block;
  a_statement                    head_of_statement_list;
  a_variable_ptr                 source_var, dest_var;
  an_expr_node_ptr               source_expr, dest_expr;
  a_base_class_ptr               bcp;
  a_field_ptr                    fp;
  a_routine_ptr                  rp;
  a_symbol_ptr                   sym;
  a_param_type_ptr               ptp;
  a_boolean                      bitwise_assign, move_assign;
  a_source_position              *err_pos;
  a_source_position              saved_error_position = error_position;

  db_enter(4, "make_default_assignment_body");
  /* The source is the non-object parameter.  When there is an explicit
     "this" parameter, that parameter is the destination; otherwise, the
     implicit "this" parameter is used. */
  rout = scope->variant.routine.ptr;
  rtsp = rout_type_supp(skip_typerefs(rout->type));
  ptp = rtsp->param_type_list;
  dest_var = NULL;
  if (ptp->is_explicit_this) {
    dest_var = implicitly_generated_param_variable(ptp);
    ptp = ptp->next;
  }  /* if */
  move_assign = is_rvalue_reference_type(ptp->type);
  source_var = implicitly_generated_param_variable(ptp);
  if (scope->variant.routine.this_param_variable != NULL) {
    class_type =
          type_pointed_to(scope->variant.routine.this_param_variable->type);
  } else {
    class_type = parent_class_of(rout);
  }  /* if */
  err_pos = &class_type->source_corresp.decl_position;
  error_position = *err_pos;
  /* Create the top-level block statement for the function. */
  top_block = alloc_statement(stmk_block, /*compiler_generated=*/TRUE);
  scope->assoc_block = top_block;
  /* "head_of_statement_list" is a local statement variable whose only
      interesting property is its "next" field, from which a linked list of
      allocated statement entries will be hung.  That list will eventually be
      transferred to the block statement that is created. */
  head_of_statement_list.next = NULL;
  sp = &head_of_statement_list;
  /* See if a bitwise copy is all that is called for. */
  if (rout->is_trivial_copy_function) {
    /* Yes.  (Then why are we defining a routine?  Probably because the
       address of the default assignment operator was taken, forcing the
       actual creation of the routine.) */
    /* Get the source and destination expressions to use as operands for an
       assignment statement. */
    source_expr = lvalue_for_source_param(source_var);
    source_expr = rvalue_expr_for_lvalue(source_expr);
    dest_expr = add_indirection_to_node(assignment_dest_ptr_expr(dest_var));
    sp = sp->next = make_assignment_statement(dest_expr, source_expr);
    sp->parent = top_block;
  } else {
    /* Memberwise copy is required.  That is, first do the appropriate
       operation on each direct base class (direct assignment or calling
       the base class's assignment function), and then do the appropriate
       copy of each member. */
    for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
      if (bcp->direct) {
        /* We are only interested in direct base classes. */
        if (bcp->is_virtual &&
            virtual_base_class_is_indirect(bcp, class_type)) {
          /* If it is also an indirect base class, it will be handled by
             the assignment function of some other base class. */
          continue;
        }  /* if */
        /* The destination is the object being assigned to, cast to the
           appropriate base class. */
        dest_expr = base_class_selection_expr(
                                    assignment_dest_ptr_expr(dest_var), bcp);
        dest_expr = add_indirection_to_node(dest_expr);
        /* The source is the non-object parameter cast to the same base
           class. */
        source_expr = lvalue_for_source_param(source_var);
        source_expr = add_address_of_to_node(source_expr);
        source_expr = base_class_selection_expr(source_expr, bcp);
        source_expr = add_indirection_to_node(source_expr);
        /* Determine which assignment operator applies. */
        if (move_assign) {
          /* For move assignment, do the assignment from an xvalue. */
          source_expr = xvalue_expr_for_lvalue(source_expr);
        }
        rp = find_assignment_operator_for_memberwise_copy(bcp->type,
                                                          source_expr,
                                                          dest_expr,
                                                          &bcp->decl_position);
        if (rp == NULL ||
            (rp->is_trivial_copy_function &&
             rp->is_consteval == rout->is_consteval)) {
          /* A bitwise copy may be performed. */
          source_expr = rvalue_expr_for_lvalue(source_expr);
          /* Create the assignment statement.  The appropriate operator
             will be selected by the function. */
          sp = sp->next = make_assignment_statement(dest_expr, source_expr);
          sp->parent = top_block;
        } else {
          sp = sp->next = make_assignment_call(source_expr, dest_expr, rp,
                                               err_pos);
        }  /* if */
        sp->parent = top_block;
      }  /* if */
      /* Advance to the next base class. */
    }  /* for */
    /* Now go through all the fields, copying them one at a time.  Use the
       symbol list rather than the field list to be sure we adhere to
       declaration order and to be sure only user defined fields are
       copied. */
    sym = class_symbol_supp(symbol_for(class_type))->symbols;
    for (; sym != NULL; sym = sym->next_in_scope) {
      if (symbol_is(sym, sk_field)) {
        /* A field. */
        fp = sym->variant.field.ptr;
        tp = skip_typerefs(fp->type);
        if (field_is_nontrivial_property_or_event(fp)) {
          /* Property and event fields aren't really data members and thus are
             not copied.  (C++/CLI "trivial" properties and events do have
             associated storage represented by the field and hence are copied
             here.)*/
          continue;
        }  /* if */
        if (is_const_qualified_type(tp) || is_any_reference_type(tp)) {
          /* The error has already been issued for const and ref members.
             Don't bother trying to do the copy. */
          check_assertion(is_at_least_one_error());
          continue;
        }  /* if */
        /* If this is an array, we need the element type. */
        if (is_array_type(tp)) {
          array_type = tp;
          tp = f_skip_typerefs(underlying_array_element_type(tp));
        } else {
          array_type = NULL;
        }  /* if */
        /* The destination is the appropriate field of the destination
           object, as an lvalue. */
        dest_expr = fe_field_lvalue_selection_expr(
                                    assignment_dest_ptr_expr(dest_var),
                                    fp);
        /* The source will be the corresponding field of the class pointed to
           by the source parameter. */
        source_expr = lvalue_for_source_param(source_var);
        source_expr = fe_field_lvalue_selection_expr(source_expr, fp);
        if (is_immediate_class_type(tp)) {
          /* It's a class type, so we may have to call an assignment operator
             function. */
          if (move_assign ?
                !class_symbol_supp(symbol_for(tp))
                                         ->makes_move_assignment_nontrivial :
                !class_symbol_supp(symbol_for(tp))
                                         ->makes_copy_assignment_nontrivial) {
            /* A bitwise copy can be performed. */
            bitwise_assign = TRUE;
          } else {
            a_statement_ptr call_stmt;
            /* A bitwise copy cannot be done.  Find the default assignment
               operator and put out a call to it. */
            bitwise_assign = FALSE;
            if (array_type != NULL) {
              /* Copying an array of classes.  Generate a loop around the
                 call of the assignment routine, like
                   tmp = 0;
                   do {
                     assignfunc(&dest[tmp], &src[tmp]);
                   } while (++tmp < num_elements);
                 Note that copying of a multidimensional array is done
                 as a single loop for all the elements, treating the array
                 as a single-dimensional array of the ultimate underlying
                 element type. */
              a_variable_ptr   temp_var;
              an_expr_node_ptr temp_node, temp_incr_node, compare_node;
              a_type_ptr       size_t_type;
              a_targ_size_t    num_elems;

              size_t_type = integer_type(targ_size_t_int_kind);
              temp_var = alloc_temporary_variable(size_t_type,
                                                  /*force_static=*/FALSE);
              /* Make "tmp = 0;" */
              temp_node = var_lvalue_expr(temp_var);
              sp = sp->next =
                make_assignment_statement(temp_node,
                                          node_for_integer_constant(
                                                    0L, targ_size_t_int_kind));
              sp->parent = top_block;
              /* Make "++tmp < num_elements". */
              temp_node = var_lvalue_expr(temp_var);
              temp_incr_node = make_operator_node(
                                          (an_expr_operator_kind)eok_pre_incr,
                                          size_t_type, temp_node);
              num_elems = skip_typerefs(array_type)->size / tp->size;
              temp_incr_node->next = node_for_host_large_integer(
                        (a_host_large_integer)num_elems, targ_size_t_int_kind);
              compare_node =
                      make_operator_node((an_expr_operator_kind)eok_lt,
                                         boolean_result_type(),
                                         temp_incr_node);
              /* Make the do-while statement. */
              sp = sp->next = alloc_statement(stmk_end_test_while,
                                              /*compiler_generated=*/TRUE);
              sp->parent = top_block;
              sp->expr = compare_node;
              /* Convert the two operands to pointers to the underlying
                 non-array elements and apply a subscript to those pointer
                 values. */
              source_expr = conv_array_expr_to_underlying_ptr(source_expr);
              source_expr = add_subscript_to_ptr_expr(source_expr, temp_var);
              dest_expr = conv_array_expr_to_underlying_ptr(dest_expr);
              dest_expr = add_subscript_to_ptr_expr(dest_expr, temp_var);
              /* Now that we have element lvalues, we can find the right
                 assignment operator. */
            }  /* if */
            if (move_assign) {
              /* For move assignment, do the assignment from an xvalue. */
              source_expr = xvalue_expr_for_lvalue(source_expr);
            }
            /* Find the assignment operator to do the copy. */
            rp = find_assignment_operator_for_memberwise_copy(
                                            tp, source_expr, dest_expr,
                                            &fp->source_corresp.decl_position);
            if (rp == NULL) {
              /* Error has already been issued in the subroutine. */
              continue;
            }  /* if */
            call_stmt = make_assignment_call(source_expr, dest_expr, rp,
                                             err_pos);
            if (array_type != NULL) {
              /* Array case; the call goes under the do-while. */
              sp->variant.loop_statement = call_stmt;
              call_stmt->parent = sp;
            } else {
              /* Non-array case; the call goes at the end of the statement
                 sequence. */
              sp = sp->next = call_stmt;
              call_stmt->parent = top_block;
            }  /* if */
          }  /* if */
        } else {
          /* Not a class type.  Just do a bitwise copy. */
          bitwise_assign = TRUE;
        }  /* if */
        if (bitwise_assign) {
          /* Do a bitwise assignment. */
          if (array_type != NULL) {
            /* Array type.  Do a special assignment (source operand is an
               lvalue). */
            sp = sp->next =
                       make_array_assignment_statement(dest_expr, source_expr);
            sp->parent = top_block;
          } else {
            /* Not an array.  The appropriate IL operator will be selected
               by make_assignment_statement. */
            source_expr = rvalue_expr_for_lvalue(source_expr);
            sp = sp->next = make_assignment_statement(dest_expr, source_expr);
            sp->parent = top_block;
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
  }  /* if */
  /* Make the return statement.  A pointer to the variable assigned to is
     the return value. */
  sp = sp->next = alloc_statement(stmk_return, /*compiler_generated=*/TRUE);
  sp->parent = top_block;
  sp->expr = add_reference_to_to_node(
                  add_indirection_to_node(
                                    assignment_dest_ptr_expr(dest_var)));
  /* We now have a list of one or more statements hanging off the local
     variable head_of_statement_list.  The start of the list is pointed to
     by the next field.  Attach the list to the top-level block. */
  top_block->variant.block.statements = head_of_statement_list.next;
  error_position = saved_error_position;
  db_exit();
  return;
}  /* make_default_assignment_body */


static void check_default_assignment_operator(a_type_ptr  class_type)
/*
Issue an error if a compiler-generated assignment operator is not allowed
because the class has a const or ref member (ARM 12.8).  The case of a
member or a base class with a nonpublic operator=() is handled elsewhere.
*/
{
  a_boolean        err, is_ref, is_const;
  a_symbol_ptr     sym;
  a_type_ptr       tp;
  a_diagnostic_ptr dp = NULL;

  db_enter(4, "check_default_assignment_operator");
  if (class_type->variant.class_struct_union.any_const_member ||
      symbol_supplement_for_class(class_type)->any_ref_member) {
    /* An error is issued only if a immediate member of the class is const or
       ref.  Those in base classes or embedded within members are diagnosed
       elsewhere. */
    err = FALSE;
    /* Go through all the fields, using the symbol list rather than the field
       list to be sure only user defined fields are checked and to be sure
       anonymous union fields are picked up. */
    sym = ((a_symbol_ptr)class_type->source_corresp.assoc_info)->
                           variant.class_struct_union.extra_info->symbols;
    for (; sym != NULL; sym = sym->next_in_scope) {
      if (sym->kind == (a_symbol_kind)sk_field) {
        tp = sym->variant.field.ptr->type;
        is_ref = is_const = FALSE;
        if (field_is_nontrivial_property_or_event(sym->variant.field.ptr)) {
          /* Property and event fields are not copied by the default
             assignment operator (C++/CLI "trivial" properties and events are
             an exception because the field also represents associated
             storage). */
        } else if (is_any_reference_type(tp)) {
          /* An assignment operator should not be generated if a member has a
             ref type. */
          is_ref = TRUE;
        } else if (is_const_qualified_type(tp)) {
          /* An assignment operator should not be generated if a member has a
             const type. */
          is_const = TRUE;
        }  /* if */
        if (is_ref || is_const) {
          if (!err) {
            /* Multi-line diagnostic has not been started yet. */
            dp = pos_start_error(ec_bad_default_assignment,
                                 &class_type->source_corresp.decl_position);
          }  /* if */
          sym_add_diag_info(dp,
                            is_ref ? ec_reference_member : ec_const_member,
                            sym);
          err = TRUE;
        }  /* if */
      }  /* if */
    }  /* for */
    if (err) end_diagnostic(dp);
  }  /* if */
  db_exit();
}  /* check_default_assignment_operator */


/*
Structure that maintains some information about the context in which a
generated function definition is created.
*/
typedef struct a_generated_func_def_context {
  a_boolean
		trans_unit_pushed;
			/* A flag indicating whether a translation unit was
			   pushed prior to creating the definition scope for a
			   generated function. */
  a_scope_depth
		saved_innermost_scope_that_affects_access,
		saved_depth_template_declaration_scope;
			/* Some scope stack depths that must be temporarily
			   adjusted while creating the function definition. */
} a_generated_func_def_context ;


static a_scope_ptr begin_definition_of_generated_function(
                                     a_routine_ptr                 rout_ptr,
                                     a_type_ptr                    rtp,
                                     a_type_ptr                    class_type,
                                     a_generated_func_def_context  *context)
/*
rout_ptr represent a generated function (e.g., a special member function) of
type rtp (no typerefs) associated with the given class type.  Start the
definition scope for the function and record some information about the
original context in *context to be able to restore that context later on using
end_definition_of_generated_function.  Return an IL entry for the definition
scope.
*/
{
  a_symbol_ptr  rout_sym = symbol_for(rout_ptr);
  a_scope_ptr   scope;
  a_routine_type_supplement_ptr
                rtsp;

  check_assertion(!scope_is_null_or_placeholder(class_type_supp(class_type)
                                                               ->assoc_scope));
  /* Switch translation units if necessary. */
  context->trans_unit_pushed = push_translation_unit_if_needed(rout_sym);
  /* Reset the innermost scope that affects access control so that any existing
     context on the scope stack does not affect the generation of the function.
     Similarly, reset the depth of the current template declaration scope. */
  context->saved_innermost_scope_that_affects_access =
                         depth_of_innermost_scope_that_affects_access_control;
  depth_of_innermost_scope_that_affects_access_control = NO_SCOPE_DEPTH;
  context->saved_depth_template_declaration_scope =
                                             depth_template_declaration_scope;
  depth_template_declaration_scope = NO_SCOPE_DEPTH;
  /* Push a class symbol reactivation scope, to make class member names
     visible for processing the function definition. */
  push_class_and_template_reactivation_scope_full(
                                           class_type,
                                           /*reactivate_template_params=*/TRUE,
                                           /*is_specialized=*/FALSE,
                                           /*extend_namespace=*/TRUE,
                                           /*force_new_context=*/TRUE,
                                           PS_NO_OPTIONS);
  /* Push the scope for the new function itself. */
  scope = push_scope((a_scope_kind)sck_function, NO_SCOPE_NUMBER,
                     (a_type_ptr)NULL, rout_ptr);
  /* If this is an "extern inline" function, change its storage class. */
  if (rout_ptr->storage_class == (a_storage_class)sc_extern) {
    rout_ptr->storage_class = (a_storage_class)sc_unspecified;
  }  /* if */
  rtsp = rtp->variant.routine.extra_info;
  rtsp->assoc_routine = rout_ptr;
  if (rtsp->this_class != NULL) {
    scope->variant.routine.this_param_variable =
                             make_implicit_this_param_variable(rout_ptr->type);
  }  /* if */
  return scope;
}  /* begin_definition_of_generated_function */


static void end_definition_of_generated_function(
                                     a_routine_ptr                 rout_ptr,
                                     a_scope_ptr                   scope,
                                     a_generated_func_def_context  *context)
/*
rout_ptr represents a generated function (e.g., a special member function) for
which begin_definition_of_generated_function returned scope and *context.
Complete the definition and restore the original context.
*/
{
  /* End of statement block is unreachable because of the return statement. */
  check_assertion(scope->assoc_block->kind == (a_statement_kind)stmk_block);
  scope->assoc_block->variant.block.extra_info->end_of_block_reachable = FALSE;
  if (rout_ptr->is_constexpr &&
      check_constexpr_routine_def_type(
                         rout_ptr, &rout_ptr->source_corresp.decl_position)) {
    /* A generated special member satisfies the rules for the body of a
       constexpr constructor or function. */
    set_routine_constexpr_info(scope, /*constexpr_ruled_out=*/FALSE);
  }  /* if */
  /* Terminate the function scope. */
  pop_scope();
  /* Terminate the class reactivation scope. */
  pop_class_reactivation_scope();
  depth_template_declaration_scope =
                              context->saved_depth_template_declaration_scope;
  depth_of_innermost_scope_that_affects_access_control =
                           context->saved_innermost_scope_that_affects_access;
  /* Mark the symbol for this routine "defined". */
  symbol_for(rout_ptr)->defined = TRUE;
  /* Notify the correspondence routines that a definition of this function
     is now present.  Note that this is done for both template classes
     and normal classes. */
  establish_function_instantiation_corresp(rout_ptr);
  /* If the translation unit stack was pushed above, pop it now. */
  if (context->trans_unit_pushed) pop_translation_unit_stack();
}  /* end_definition_of_generated_function */


static void define_special_member_function(a_routine_ptr  rout_ptr)
/*
Define a compiler generated routine for a member function (constructor or
destructor).  This entails creating a new memory region, a scope, and an
empty statement block.
*/
{
  db_enter(4, "define_special_member_function");

  /* Push the routine's module entity to the top of the module entity stack so
     that any generated definition has access to the correct entities. */
  a_module_entity_stack_state tmp_mod(symbol_for(rout_ptr)->module_entity);
  a_type_ptr                  class_type = parent_class_of(rout_ptr);
  if (class_type->variant.class_struct_union.is_nonreal_class) {
    /* Don't bother generating the definition for a member of an nonreal
       instantiation of a template class. */
  } else {
    a_scope_ptr  scope;
    a_generated_func_def_context
                 context;
    a_type_ptr   rtp = skip_typerefs(rout_ptr->type);
    scope = begin_definition_of_generated_function(rout_ptr, rtp, class_type,
                                                   &context);
    if (rout_ptr->is_inheriting_ctor && rout_ptr->is_constexpr &&
        !default_ctor_can_be_constexpr(rout_ptr, class_type,
                                      /*check_bases=*/FALSE)) {
      /* class_type was incomplete at the time the inheriting constructor was
         generated.  The inheriting constructor also needs to behave as if it
         were a default constructor for the purposes of initializing the most-
         derived subobject.  Determine if it can still be a constexpr
         constructor and if not, clear those flags. */
      rout_ptr->is_declared_constexpr = rout_ptr->is_constexpr = FALSE;
    }  /* if */
    if (has_indeterminate_exception_spec(rout_ptr)) {
      /* A default constructor whose exception specification hasn't been
         determined yet because it depended on field initializers.  In GNU C++
         mode, all special member functions have their exception specification
         delayed this way. */
      resolve_indeterminate_exception_specification(rout_ptr);
    }  /* if */
    /* Enter the constructor and destructor initializers, to record possible
       implicit initializers. */
    if (rout_ptr->special_kind == (a_special_function_kind)sfk_constructor) {
      make_generated_constructor_body(scope);
    } else if (rout_ptr->special_kind ==
                                  (a_special_function_kind)sfk_destructor) {
      make_default_destructor_body(scope);
    } else {
      /* Assignment operator case. */
      check_assertion(rout_ptr->special_kind ==
                                   (a_special_function_kind)sfk_operator &&
                      rout_ptr->variant.opname_kind == 
		                   (an_opname_kind)onk_assign);
      check_default_assignment_operator(class_type);
      make_default_assignment_body(scope);
    }  /* if */
    end_definition_of_generated_function(rout_ptr, scope, &context);
  }  /* if */
  db_exit();
}  /* define_special_member_function */


static a_statement_ptr make_return_false_stmt_if_false_expr(
                                                       an_expr_node_ptr  cond)
/*
Return a statement of the form:

	if (!cond) return false;

where cond is the given expression.
*/
{
  a_statement_ptr  if_stmt, return_stmt;

  return_stmt = alloc_statement(stmk_return, /*compiler_generated=*/TRUE);
  return_stmt->position = error_position;
  return_stmt->expr = make_zero_expr(boolean_result_type());
  if_stmt = alloc_statement(stmk_if, /*compiler_generated=*/TRUE);
  if_stmt->position = error_position;
  if_stmt->expr = make_operator_node((an_expr_operator_kind)eok_not,
                                     boolean_result_type(), cond);
  if_stmt->variant.if_stmt.then_statement = return_stmt;
  return_stmt->parent = if_stmt;
  return if_stmt;
}  /* make_return_false_stmt_if_false_expr */


static void make_comparison_args(an_expr_node_ptr  *arg1,
                                 an_expr_node_ptr  *arg2)
/*
We are currently in the scope of a defaulted comparison operator.  Return in
*arg1 and *arg2 expressions denoting pointers to the two objects being
compared.
*/
{
  a_variable_ptr  vp = innermost_function_scope->variant.routine.parameters;

  if (vp->next == NULL) {
    *arg1 = this_param_value_expr();
  } else {
    *arg1 = add_address_of_to_node(lvalue_for_source_param(vp));
    vp = vp->next;
  }  /* if */
  *arg2 = add_address_of_to_node(lvalue_for_source_param(vp));
}  /* make_comparison_args */


static void make_default_eq_body(a_scope_ptr  scope,
                                 a_type_ptr   class_type)
/*
Create the body for a defaulted operator== for the given class type.  The
definition of the operator has been started and its associated scope is also
given.
*/
{
  a_statement_ptr    sp, top_block;
  a_statement        head_of_statement_list;
  a_base_class_ptr   bcp;
  a_symbol_ptr       sym;
  a_source_position  saved_error_position = error_position;

  error_position = class_type->source_corresp.decl_position;
  /* Create the top-level block statement for the function. */
  top_block = alloc_statement(stmk_block, /*compiler_generated=*/TRUE);
  scope->assoc_block = top_block;
  /* "head_of_statement_list" is a local statement variable whose only
      interesting property is its "next" field, from which a linked list of
      allocated statement entries will be hung.  That list will eventually be
      transferred to the block statement that is created. */
  head_of_statement_list.next = NULL;
  sp = &head_of_statement_list;
  /* Perform memberwise comparisons.  Start with the direct base classes in
     declaration order (if any) and then the nonstatic data members in
     declaration order (if any). */
  for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
    an_expr_node_ptr  arg1, arg2;
    /* We are only interested in direct base classes. */
    if (!bcp->direct) continue;
    if (bcp->is_virtual && virtual_base_class_is_indirect(bcp, class_type)) {
      /* If bcp is also an indirect virtual base class, it will be handled by
         the assignment function of some other base class. */
      continue;
    }  /* if */
    make_comparison_args(&arg1, &arg2);
    arg1 = add_indirection_to_node(base_class_selection_expr(arg1, bcp));
    arg2 = add_indirection_to_node(base_class_selection_expr(arg2, bcp));
    sp->next = make_return_false_stmt_if_false_expr(
                                              make_eq_comparison(arg1, arg2));
    sp = sp->next;
    sp->parent = top_block;
  }  /* for */
  /* For the data members, use the symbol list rather than the field list to
     be sure we adhere to declaration order and to be sure only user-defined
     members are compared. */
  sym = class_symbol_supp(symbol_for(class_type))->symbols;
  for (; sym != NULL; sym = sym->next_in_scope) {
    a_field_ptr       fp;
    a_type_ptr        ftp, array_type;
    an_expr_node_ptr  arg1, arg2;
    a_statement_ptr   cmp_stmt;
    if (!symbol_is(sym, sk_field)) continue;
    fp = sym->variant.field.ptr;
    if (field_is_nontrivial_property_or_event(fp)) {
      /* Generated comparisons are currently not supported for nontrivial
         property/event fields. */
      pos_error(ec_defaulted_comparison_for_property, &error_position);
      continue;
    }  /* if */
    ftp = skip_typerefs(fp->type);
    /* If this is an array, we need the underlying element type. */
    if (is_array_type(ftp)) {
      array_type = ftp;
      ftp = f_skip_typerefs(underlying_array_element_type(ftp));
    } else {
      array_type = NULL;
    }  /* if */
    make_comparison_args(&arg1, &arg2);
    arg1 = fe_field_lvalue_selection_expr(arg1, fp);
    arg2 = fe_field_lvalue_selection_expr(arg2, fp);
    if (array_type != NULL) {
      /* Compare an array of elements.  Generate a loop around the comparison,
         like this:
           tmp = 0;
           do {
             if (!(arg1[tmp] == &src[tmp]))
               return false;
           } while (++tmp < num_elements);
         Note that comparing multidimensional arrays is done as a single loop
         for all the elements, treating the array as a single-dimensional array
         of the ultimate underlying element type. */
      a_variable_ptr    temp_var;
      an_expr_node_ptr  temp_node, temp_incr_node, compare_node;
      a_type_ptr        size_t_type = integer_type(targ_size_t_int_kind);
      a_targ_size_t     num_elems;
      temp_var = alloc_temporary_variable(size_t_type,
                                          /*force_static=*/FALSE);
      /* Make "tmp = 0;" */
      temp_node = var_lvalue_expr(temp_var);
      sp->next =
        make_assignment_statement(temp_node, node_for_integer_constant(
                                                   0L, targ_size_t_int_kind));
      sp = sp->next;
      sp->parent = top_block;
      /* Make "++tmp < num_elements". */
      temp_node = var_lvalue_expr(temp_var);
      temp_incr_node = make_operator_node((an_expr_operator_kind)eok_pre_incr,
                                          size_t_type, temp_node);
      num_elems = skip_typerefs(array_type)->size / ftp->size;
      temp_incr_node->next = node_for_host_large_integer(
                                              (a_host_large_integer)num_elems,
                                              targ_size_t_int_kind);
      compare_node = make_operator_node((an_expr_operator_kind)eok_lt,
                                        boolean_result_type(),
                                        temp_incr_node);
      /* Make the do-while statement. */
      sp->next = alloc_statement(stmk_end_test_while,
                                 /*compiler_generated=*/TRUE);
      sp = sp->next;
      sp->parent = top_block;
      sp->expr = compare_node;
      /* Convert the two operands to pointers to the underlying non-array
         elements and apply a subscript to those pointer values. */
      arg1 = conv_array_expr_to_underlying_ptr(arg1);
      arg1 = add_subscript_to_ptr_expr(arg1, temp_var);
      arg2 = conv_array_expr_to_underlying_ptr(arg2);
      arg2 = add_subscript_to_ptr_expr(arg2, temp_var);
    }  /* if */
    cmp_stmt = make_return_false_stmt_if_false_expr(
                                              make_eq_comparison(arg1, arg2));
    if (array_type != NULL) {
      /* Array case; the comparison goes under the do-while. */
      sp->variant.loop_statement = cmp_stmt;
      cmp_stmt->parent = sp;
    } else {
      /* Non-array case; the comparison goes at the end of the statement
         sequence. */
      sp->next = cmp_stmt;
      sp = sp->next;
      cmp_stmt->parent = top_block;
    }  /* if */
  }  /* for */
  /* Make the final "return true;" statement. */
  sp->next = alloc_statement(stmk_return, /*compiler_generated=*/TRUE);
  sp = sp->next;
  sp->parent = top_block;
  sp->expr = make_one_expr(boolean_result_type());
  /* We now have a list of one or more statements hanging off the local
     variable head_of_statement_list.  The start of the list is pointed to
     by the next field.  Attach the list to the top-level block. */
  top_block->variant.block.statements = head_of_statement_list.next;
  error_position = saved_error_position;
}  /* make_default_eq_body */


static void make_default_ne_body(a_scope_ptr  scope)
/*
Create the body for a defaulted operator!=.  scope is the definition scope of
the operator (which has just been started).
*/
{
  an_expr_node_ptr  arg1, arg2, cmp;
  a_statement_ptr   top_block, return_stmt;

  top_block = alloc_statement(stmk_block, /*compiler_generated=*/TRUE);
  scope->assoc_block = top_block;
  make_comparison_args(&arg1, &arg2);
  arg1 = add_indirection_to_node(arg1);
  arg2 = add_indirection_to_node(arg2);
  cmp = make_eq_comparison(arg1, arg2);
  return_stmt = alloc_statement(stmk_return, /*compiler_generated=*/TRUE);
  return_stmt->position = error_position;
  return_stmt->expr = make_operator_node((an_expr_operator_kind)eok_not,
                                         boolean_result_type(), cmp);
  return_stmt->parent = top_block;
  top_block->variant.block.statements = return_stmt;
}  /* make_default_ne_body */


static a_statement_ptr make_spaceship_element_comparison(
                                                       an_expr_node_ptr arg1,
                                                       an_expr_node_ptr arg2,
                                                       a_type_ptr       tp,
                                                       a_statement_ptr  block)
/*
Create a pair of statements:

        R v{arg1 <=> arg2};
        if (v != 0) return v;

with R the given type, and return a pointer to the first statement.  block is
the parent statement for the new statements.  In error cases, return NULL.
*/
{
  a_variable_ptr    vp;
  a_statement_ptr   init_stmt = NULL, if_stmt, return_stmt;
  an_expr_node_ptr  return_cond;

  /* Create the variable, its initializer, and the v != 0 expression. */
  vp = make_spaceship_cmp_variable(arg1, arg2, tp, &return_cond);
  if (vp->init_kind != (an_init_kind)initk_dynamic) {
    expect_error();
    goto done;
  }  /* if */
  /* Allocate the initializer statement for the variable. */
  init_stmt = alloc_statement(stmk_init, /*compiler_generated=*/TRUE);
  init_stmt->parent = block;
  init_stmt->variant.dynamic_init = vp->initializer.dynamic;
  /* Create the return statement. */
  return_stmt = alloc_statement(stmk_return, /*compiler_generated=*/TRUE);
  return_stmt->position = error_position;
  return_stmt->expr = var_rvalue_expr(vp);
  /* Place the return statement under an if-statement. */
  if_stmt = alloc_statement(stmk_if, /*compiler_generated=*/TRUE);
  if_stmt->parent = block;
  if_stmt->position = error_position;
  if_stmt->expr = return_cond;
  if_stmt->variant.if_stmt.then_statement = return_stmt;
  return_stmt->parent = if_stmt;
  init_stmt->next = if_stmt;
done:
  return init_stmt;
}  /* make_spaceship_element_comparison */


static void make_default_spaceship_body(a_scope_ptr  scope,
                                        a_type_ptr   rtp,
                                        a_type_ptr   class_type)
/*
Create the body for a defaulted operator<=> (of type rtp, with no typerefs)
for the given class type.  The definition of the operator has been started and
its associated scope is also given.
*/
{
  a_statement_ptr    sp, top_block;
  a_statement        head_of_statement_list;
  a_base_class_ptr   bcp;
  a_symbol_ptr       sym;
  a_source_position  saved_error_position = error_position;
  a_type_ptr         return_type = rtp->variant.routine.return_type;

  error_position = class_type->source_corresp.decl_position;
  /* Create the top-level block statement for the function. */
  top_block = alloc_statement(stmk_block, /*compiler_generated=*/TRUE);
  scope->assoc_block = top_block;
  /* "head_of_statement_list" is a local statement variable whose only
      interesting property is its "next" field, from which a linked list of
      allocated statement entries will be hung.  That list will eventually be
      transferred to the block statement that is created. */
  head_of_statement_list.next = NULL;
  sp = &head_of_statement_list;
  /* Perform memberwise comparisons.  Start with the direct base classes in
     declaration order (if any) and then the nonstatic data members in
     declaration order (if any). */
  for (bcp = base_classes_of(class_type); bcp != NULL; bcp = bcp->next) {
    an_expr_node_ptr  arg1, arg2;
    /* We are only interested in direct base classes. */
    if (!bcp->direct) continue;
    if (bcp->is_virtual && virtual_base_class_is_indirect(bcp, class_type)) {
      /* If bcp is also an indirect virtual base class, it will be handled by
         the assignment function of some other base class. */
      continue;
    }  /* if */
    make_comparison_args(&arg1, &arg2);
    arg1 = add_indirection_to_node(base_class_selection_expr(arg1, bcp));
    arg2 = add_indirection_to_node(base_class_selection_expr(arg2, bcp));
    sp->next = make_spaceship_element_comparison(arg1, arg2, return_type,
                                                 top_block);
    /* Two statements should have been returned, or none in error cases. */
    if (sp->next != NULL) {
      sp = sp->next->next;
      check_assertion(sp->next == NULL);
    }  /* if */
  }  /* for */
  /* For the data members, use the symbol list rather than the field list to
     be sure we adhere to declaration order and to be sure only user-defined
     members are compared. */
  sym = class_symbol_supp(symbol_for(class_type))->symbols;
  for (; sym != NULL; sym = sym->next_in_scope) {
    a_field_ptr       fp;
    a_type_ptr        ftp, array_type;
    an_expr_node_ptr  arg1, arg2;
    if (!symbol_is(sym, sk_field)) continue;
    fp = sym->variant.field.ptr;
    if (field_is_nontrivial_property_or_event(fp)) {
      /* Generated comparisons are currently not supported for nontrivial
         property/event fields. */
      pos_error(ec_defaulted_comparison_for_property, &error_position);
      continue;
    }  /* if */
    ftp = skip_typerefs(fp->type);
    /* If this is an array, we need the underlying element type. */
    if (is_array_type(ftp)) {
      array_type = ftp;
      ftp = f_skip_typerefs(underlying_array_element_type(ftp));
    } else {
      array_type = NULL;
    }  /* if */
    make_comparison_args(&arg1, &arg2);
    arg1 = fe_field_lvalue_selection_expr(arg1, fp);
    arg2 = fe_field_lvalue_selection_expr(arg2, fp);
    if (array_type != NULL) {
      /* Compare an array of elements.  Generate a loop around the comparison,
         like this:
           tmp = 0;
           do {
             R v{arg1[tmp] <=> arg2[tmp]};
             if (v != 0) return v;
           } while (++tmp < num_elements);
         Note that comparing multidimensional arrays is done as a single loop
         for all the elements, treating the array as a single-dimensional array
         of the ultimate underlying element type.  Also, in some cases, the
         <=> operator above may be implemented in terms of == and <. */
      a_variable_ptr    temp_var;
      an_expr_node_ptr  temp_node, temp_incr_node, compare_node;
      a_type_ptr        size_t_type = integer_type(targ_size_t_int_kind);
      a_targ_size_t     num_elems;
      temp_var = alloc_temporary_variable(size_t_type,
                                          /*force_static=*/FALSE);
      /* Make "tmp = 0;" */
      temp_node = var_lvalue_expr(temp_var);
      sp->next =
        make_assignment_statement(temp_node, node_for_integer_constant(
                                                   0L, targ_size_t_int_kind));
      sp = sp->next;
      sp->parent = top_block;
      /* Make "++tmp < num_elements". */
      temp_node = var_lvalue_expr(temp_var);
      temp_incr_node = make_operator_node((an_expr_operator_kind)eok_pre_incr,
                                          size_t_type, temp_node);
      num_elems = skip_typerefs(array_type)->size / ftp->size;
      temp_incr_node->next = node_for_host_large_integer(
                                              (a_host_large_integer)num_elems,
                                              targ_size_t_int_kind);
      compare_node = make_operator_node((an_expr_operator_kind)eok_lt,
                                        boolean_result_type(),
                                        temp_incr_node);
      /* Make the do-while statement. */
      sp->next = alloc_statement(stmk_end_test_while,
                                 /*compiler_generated=*/TRUE);
      sp = sp->next;
      sp->parent = top_block;
      sp->expr = compare_node;
      /* Convert the two operands to pointers to the underlying non-array
         elements and apply a subscript to those pointer values. */
      arg1 = conv_array_expr_to_underlying_ptr(arg1);
      arg1 = add_subscript_to_ptr_expr(arg1, temp_var);
      arg2 = conv_array_expr_to_underlying_ptr(arg2);
      arg2 = add_subscript_to_ptr_expr(arg2, temp_var);
    }  /* if */
    if (array_type != NULL) {
      /* Array case; the comparison goes under the do-while. */
      sp->variant.loop_statement = make_spaceship_element_comparison(
                                                 arg1, arg2, return_type, sp);
    } else {
      /* Non-array case; the comparison goes at the end of the statement
         sequence. */
      sp->next = make_spaceship_element_comparison(
                                          arg1, arg2, return_type, top_block);
      if (sp->next != NULL) {
        sp = sp->next->next;
      }  /* if */
    }  /* if */
  }  /* for */
  /* Make the final return statement, which returns "equal" or "equivalent". */
  sp->next = alloc_statement(stmk_return, /*compiler_generated=*/TRUE);
  sp = sp->next;
  sp->parent = top_block;
  make_defaulted_final_spaceship_return(rtp, sp);
  /* We now have a list of one or more statements hanging off the local
     variable head_of_statement_list.  The start of the list is pointed to
     by the next field.  Attach the list to the top-level block. */
  top_block->variant.block.statements = head_of_statement_list.next;
  error_position = saved_error_position;
}  /* make_default_spaceship_body */


static void make_default_rel_op_body(an_opname_kind  onk,
                                     a_scope_ptr     scope)
/*
Create the body for a defaulted relational operator described by onk.  scope is
the definition scope of the operator (which has just been started).
*/
{
  an_expr_node_ptr  arg1, arg2, cmp;
  a_statement_ptr   top_block, return_stmt;

  top_block = alloc_statement(stmk_block, /*compiler_generated=*/TRUE);
  scope->assoc_block = top_block;
  make_comparison_args(&arg1, &arg2);
  arg1 = add_indirection_to_node(arg1);
  arg2 = add_indirection_to_node(arg2);
  cmp = make_synthesized_rel_op(token_for_rel_op(onk), arg1, arg2);
  return_stmt = alloc_statement(stmk_return, /*compiler_generated=*/TRUE);
  return_stmt->position = error_position;
  return_stmt->expr = cmp;
  return_stmt->parent = top_block;
  top_block->variant.block.statements = return_stmt;
}  /* make_default_rel_op_body */


static void define_default_comparison_operator(a_routine_ptr  rp)
/*
The given routine is a defaulted comparison operator.  Generate an actual
definition for it.
*/
{
  a_type_ptr        rtp = skip_typerefs(rp->type), class_type;
  a_param_type_ptr  ptp = function_type_params(rtp);

  /* Identify the associated class C from the first parameter type (which
     should be "C const&" whether the function is declared as a member or as a
     friend). */
  check_assertion(ptp != NULL);
  if (is_reference_type(ptp->type)) {
    class_type = type_pointed_to(ptp->type);
    class_type = skip_typerefs(class_type);
  } else {
    class_type = skip_typerefs(ptp->type);
  }  /* if */
  check_assertion(is_immediate_class_type(class_type));
  if (class_type->variant.class_struct_union.is_nonreal_class) {
    /* Don't bother generating the definition for a member of a nonreal
       instantiation of a template class. */
  } else {
    a_scope_ptr  scope;
    a_generated_func_def_context
                 context;
    scope = begin_definition_of_generated_function(rp, rtp, class_type,
                                                   &context);
    for (; ptp != NULL; ptp = ptp->next) {
      (void)implicitly_generated_param_variable(ptp);
    }  /* for */
    if (opname_kind_is(rp, onk_eq)) {
      make_default_eq_body(scope, class_type);
    } else if (opname_kind_is(rp, onk_ne)) {
      make_default_ne_body(scope);
    } else if (opname_kind_is(rp, onk_spaceship)) {
      make_default_spaceship_body(scope, rtp, class_type);
    } else if (opname_kind_is_rel_op(rp)) {
      make_default_rel_op_body(rp->variant.opname_kind, scope);
    } else {
      unexpected_condition();
    }  /* if */
    end_definition_of_generated_function(rp, scope, &context);
  }  /* if */
}  /* define_default_comparison_operator */


void force_definition_of_compiler_generated_routine(a_routine_ptr  rp)
/*
If rp points to a compiler-generated routine that is being referenced and
whose definition has not yet been generated (if it's marked as "deleted", it
is considered already defined), force the definition now.
*/
{

  if ((rp->compiler_generated || rp->is_defaulted) && !rp->is_deleted) {
    if (!routine_has_been_defined(rp)) {
      /* Only force a definition for constructors, destructors, and operator=
         functions, as well as generic lambda conversion functions.  In
         particular, do not try to define operator new and delete functions. */
      if (special_kind_is(rp, sfk_constructor) ||
          special_kind_is(rp, sfk_destructor) ||
          (special_kind_is(rp, sfk_operator) &&
           rp->variant.opname_kind == (an_opname_kind)onk_assign)) {
        a_type_ptr  parent_type = parent_class_of(rp);
        a_class_symbol_supplement_ptr
                    cssp = symbol_supplement_for_class(parent_type);
        if (rp->is_defaulted && !rp->is_deleted &&
            !is_move_function_with_explicit_exc_spec(rp) &&
            !(gpp_version_is(< 100000) || clang_version_is(< 90000) ||
              ms_version_is(< 1928))) {
          /* For class template instances, we complete ("instantiate") the
             exception specification if one is explicitly specified to ensure
             that the explicit and implicit versions are equivalent.
             This is now done at the point of use, but earlier compilers did
             it at the point of instantiation. */
          complete_defaulted_exc_spec_if_explicit(rp);
        }  /* if */
        if (special_kind_is(rp, sfk_constructor) &&
            cssp->has_initializer_fixups &&
            /*lint -e(506)*/!is_immediate_managed_class_type(parent_type) &&
            is_default_constructor(rp, /*is_declarative_context=*/TRUE)) {
          /* Don't generate the default constructor body at this time because
             required field initializers haven't been parsed yet.  Instead set
             a flag indicating that the definition has been delayed so that it
             can be generated once the field initializers have been parsed. */
          cssp->default_ctor_body_delayed = TRUE;
#if MICROSOFT_EXTENSIONS_ALLOWED
        } else if (cli_or_cx_enabled &&
                   is_immediate_delegate_type(parent_type) &&
                   special_kind_is(rp, sfk_constructor)) {
          /* The generated constructor declaration of a delegate class type
             is not one whose body can be generated. */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        } else {
          define_special_member_function(rp);
        }  /* if */
      } else if (special_kind_is(rp, sfk_conversion) &&
                 rp->is_template_function) {
        /* A compiler-generated conversion function is presumably a closure
           conversion function. */
        check_assertion(
               class_type_supp(parent_class_of(rp))->is_lambda_closure_class);
        define_lambda_conversion_function(rp);
      } else if (special_kind_is(rp, sfk_operator) &&
                 opname_is_comparison(rp->variant.opname_kind)) {
        define_default_comparison_operator(rp);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* force_definition_of_compiler_generated_routine */


void generate_required_virtual_destructor_bodies(ARG_UNUSED a_scope_ptr  scope)
/*
Go through the classes on the types list of the indicated scope and generate
bodies for virtual destructors, as required.  Then (if it is a file or
namespace scope) check the scopes for each namespace defined in the
indicated scope.  This is mostly vestigial, but it's been kept as a hook
in case it's useful.
*/
{
#if DO_IL_LOWERING && ABI_COMPATIBILITY_VERSION < 238
  /* The only case left is generation of a destructor if needed because
     a typeinfo variable points to it. */
  a_namespace_ptr                nsp;
  a_type_ptr                     tp;
  a_routine_ptr                  rp;
  a_class_symbol_supplement_ptr  cssp;
  a_class_type_supplement_ptr    ctsp;

  db_enter(3, "generate_required_virtual_destructor_bodies");
  /* First go through all the classes declared in the indicated scope. */
  for (tp = scope->types; tp != NULL; tp = tp->next) {
    if (is_immediate_class_type(tp)) {
      ctsp = tp->variant.class_struct_union.extra_info;
      if (ctsp->assoc_scope == NULL) {
        /* Class has no definition. */
      } else if (tp->source_corresp.assoc_info == NULL) {
        /* Class has no tag symbol.  This serves to eliminate types
           generated by IL lowering. */
      } else {
        cssp = symbol_supplement_for_class(tp);
        if (has_nontrivial_destructor(cssp)) {
          rp = cssp->destructor->variant.routine.ptr;
          if (external_typeinfo_will_be_defined_for_class(tp)) {
            /* The destructor for the current class is needed because it is
               referenced from the typeinfo variable.  Create a definition
               if needed, and force its instantiation if a template. */
            mark_routine_referenced(rp);
          }  /* if */
        }  /* if */
        /* Do the same check for nested classes, if any. */
        generate_required_virtual_destructor_bodies(ctsp->assoc_scope);
      }  /* if */
    }  /* if */
  }  /* for */
  if (scope->kind == (a_scope_kind)sck_file ||
      scope->kind == (a_scope_kind)sck_namespace) {
    /* Next go though all the namespaces, calling this routine recursively
       for each (excluding namespace aliases). */
    for (nsp = scope->namespaces; nsp != NULL; nsp = nsp->next) {
      if (!nsp->is_namespace_alias) {
        generate_required_virtual_destructor_bodies(nsp->variant.assoc_scope);
      }  /* if */
    }  /* for */
  }  /* if */
  db_exit();
#endif /* DO_IL_LOWERING && ABI_COMPATIBILITY_VERSION < 238 */
}  /* generate_required_virtual_destructor_bodies */


a_coroutine_descr_ptr get_coroutine_descr(a_routine_ptr rp)
/*
Return a coroutine description for the given routine (which must have a
definition).  If no description was allocated for this routine yet, one is
allocated at this time (associated with the routine position), recorded in the
definition through a leading stmk_coroutine statement, and rp->is_coroutine
is set to TRUE.
*/
{
  a_coroutine_descr_ptr   cdp;
  a_scope_ptr             func_scope;
  a_statement_ptr         body_stmt;
  a_struct_stmt_stack_entry_ptr
                          root_sssep = &struct_stmt_stack[0];

  func_scope = scope_for_routine(rp);
  body_stmt = func_scope->assoc_block;
  if (body_stmt == NULL) {
    /* The top-level block is still being parsed and has therefore not been
       associated with the function scope yet.  Use the structured statement
       stack to get the outer block instead. */
    body_stmt = root_sssep->statement;
  }  /* if */
  if (body_stmt->kind == (a_statement_kind)stmk_try_block) {
    /* For a function-try-block, use the associated dependent block. */
    body_stmt = body_stmt->variant.try_block->statement;
    root_sssep += 1;
  }  /* if */
  if (rp->is_coroutine) {
    /* This is not the first time we're calling this function for this routine
       entry.  Retrieve the previously allocated description. */
    a_statement_ptr  csp = body_stmt->variant.block.statements;
    check_assertion(csp != NULL &&
                    csp->kind == (a_statement_kind)stmk_coroutine);
    cdp = csp->variant.coroutine.descr;
  } else {
    /* Allocate a new description and associated stmk_coroutine entry. */
    a_statement_ptr  csp = alloc_statement(stmk_coroutine,
                                           /*compiler_generated=*/TRUE);
    cdp = alloc_coroutine_descr();
    cdp->position = rp->source_corresp.decl_position;
    csp->variant.coroutine.descr = cdp;
    csp->parent = body_stmt;
    csp->next = body_stmt->variant.block.statements;
    body_stmt->variant.block.statements = csp;
    if (csp->next == NULL) {
      /* There are no statements recorded in the top-level block yet (because
         we're still parsing the first statement; there must be one since
         coroutine definitions are the result of specific constructs like
         "co_yield" and "co_await" expressions).  Record the stmk_coroutine
         entry as the last statement on the list for now. */
      root_sssep->last_dep_statement = csp;
    }  /* if */
    rp->is_coroutine = TRUE;
    if (!rp->is_declared_constexpr && !rp->is_consteval) {
      rp->is_constexpr = FALSE;
    }
    if (special_kind_is(rp, sfk_constructor) ||
#if MICROSOFT_EXTENSIONS_ALLOWED
        special_kind_is(rp, sfk_static_constructor) ||
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */
        special_kind_is(rp, sfk_destructor)) {
      pos_error(ec_special_member_coroutine, &cdp->position);
      cdp->error_descr = TRUE; 
    } else if (rp == il_header.main_routine) {
      pos_sy_error(ec_main_coroutine, &cdp->position, symbol_for(rp));
      cdp->error_descr = TRUE; 
    }  /* if */
    init_coroutine_descr(rp, cdp);
  }  /* if */
  return cdp;
}  /* get_coroutine_descr */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

