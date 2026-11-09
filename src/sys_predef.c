/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

sys_predef.c -- System dependent predefined macros and assertions.

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
#include "macro.h"
#include "sys_predef.h"
#if BUILTIN_FUNCTIONS_ENABLED
#include "builtin_defs.h"
#endif /* BUILTIN_FUNCTIONS_ENABLED */
#if USE_X86_FUNCTION_MULTIVERSIONING
#include "exprutil.h"
#endif /* USE_X86_FUNCTION_MULTIVERSIONING */
#include "pch.h"

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Functions implementing intrinsic alias templates.  (See the description of
NS_alias_templ_intrinsic.)
*/

static a_boolean subst_std_remove_cv_t(a_template_arg  *t_args,
                                       a_type          **p_tp)
/*
t_args represents the single template argument passed to std::remove_cv_t<T>.
Place in *p_tp the resulting type.  Return TRUE.
*/
{
  check_assertion(t_args != NULL && t_args->kind == tak_type);
  *p_tp = remove_qualifiers(t_args->variant.type, TQ_CONST | TQ_VOLATILE);
  return TRUE;
}  /* subst_std_remove_cv_t */


static a_boolean subst_std_remove_const_t(a_template_arg  *t_args,
                                          a_type          **p_tp)
/*
t_args represents the single template argument passed to
std::remove_const_t<T>.  Place in *p_tp the resulting type.  Return TRUE.
*/
{
  check_assertion(t_args != NULL && t_args->kind == tak_type);
  *p_tp = remove_qualifiers(t_args->variant.type, TQ_CONST);
  return TRUE;
}  /* subst_std_remove_const_t */


static a_boolean subst_std_remove_volatile_t(a_template_arg  *t_args,
                                             a_type          **p_tp)
/*
t_args represents the single template argument passed to
std::remove_volatile_t<T>.  Place in *p_tp the resulting type.  Return TRUE.
*/
{
  check_assertion(t_args != NULL && t_args->kind == tak_type);
  *p_tp = remove_qualifiers(t_args->variant.type, TQ_VOLATILE);
  return TRUE;
}  /* subst_std_remove_volatile_t */


static a_boolean subst_std_remove_reference_t(a_template_arg  *t_args,
                                              a_type          **p_tp)
/*
t_args represents the single template argument passed to
std::remove_reference_t<T>.  Place in *p_tp the resulting type.  Return TRUE.
*/
{
  a_type  *tp;

  check_assertion(t_args != NULL && t_args->kind == tak_type);
  tp = t_args->variant.type;
  if (is_reference_type(tp)) tp = skip_typedefs(type_pointed_to(tp));
  *p_tp = tp;
  return TRUE;
}  /* subst_std_remove_reference_t */


static a_boolean subst_std_remove_cvref_t(a_template_arg  *t_args,
                                          a_type          **p_tp)
/*
t_args represents the single template argument passed to
std::remove_cvref_t<T>.  Place in *p_tp the resulting type.  Return TRUE.
*/
{
  a_type  *tp;

  check_assertion(t_args != NULL && t_args->kind == tak_type);
  tp = t_args->variant.type;
  if (is_reference_type(tp)) tp = skip_typedefs(type_pointed_to(tp));
  *p_tp = remove_qualifiers(tp, TQ_CONST | TQ_VOLATILE);
  return TRUE;
}  /* subst_std_remove_cvref_t */


a_boolean eval_intrinsic_alias_templ(int             idx,
                                     a_template_arg  *t_args,
                                     a_type_ptr      *substituted_tp)
/*
Substitute the intrinsic alias template with the given index idx (corresponding
to the enumerators of an_alias_templ_intrinsic) with the template arguments
described by t_args.  If successful, return TRUE and set *substituted_tp to
the resulting type.  Otherwise, return FALSE.
*/
{
  a_boolean  result = FALSE;

  switch (idx) {
    case ati_error:
      break;
#define ATI_dispatch(ns, name) \
    case ati_##ns##_##name: \
      result = subst_##ns##_##name(t_args, substituted_tp); \
      break;
    NS_alias_templ_intrinsics(ATI_dispatch)
#undef ATI_dispatch
    default:
      unexpected_condition();
  }  /* switch */
  return result;
}  /* eval_intrinsic_alias_templ */


/*
Functions implementing intrinsic class-template members of the form
xyz<A...>::name.  (See the description of NS_templ_type_member_intrinsics.)
The std::remove_* members share the underlying type transform with the
corresponding alias templates, so they simply delegate to the alias subst
functions (the argument list shape, <T>, is identical) and report that the
member resolved.
*/

static a_templ_type_member_result subst_std_remove_cv(a_template_arg  *t_args,
                                                      a_type          **p_tp)
/*
Resolve std::remove_cv<T>::type.  See subst_std_remove_cv_t.
*/
{
  (void)subst_std_remove_cv_t(t_args, p_tp);
  return ttmr_resolved;
}  /* subst_std_remove_cv */


static a_templ_type_member_result subst_std_remove_const(
                                                  a_template_arg  *t_args,
                                                  a_type          **p_tp)
/*
Resolve std::remove_const<T>::type.  See subst_std_remove_const_t.
*/
{
  (void)subst_std_remove_const_t(t_args, p_tp);
  return ttmr_resolved;
}  /* subst_std_remove_const */


static a_templ_type_member_result subst_std_remove_volatile(
                                                  a_template_arg  *t_args,
                                                  a_type          **p_tp)
/*
Resolve std::remove_volatile<T>::type.  See subst_std_remove_volatile_t.
*/
{
  (void)subst_std_remove_volatile_t(t_args, p_tp);
  return ttmr_resolved;
}  /* subst_std_remove_volatile */


static a_templ_type_member_result subst_std_remove_reference(
                                                  a_template_arg  *t_args,
                                                  a_type          **p_tp)
/*
Resolve std::remove_reference<T>::type.  See subst_std_remove_reference_t.
*/
{
  (void)subst_std_remove_reference_t(t_args, p_tp);
  return ttmr_resolved;
}  /* subst_std_remove_reference */


static a_templ_type_member_result subst_std_remove_cvref(
                                                  a_template_arg  *t_args,
                                                  a_type          **p_tp)
/*
Resolve std::remove_cvref<T>::type.  See subst_std_remove_cvref_t.
*/
{
  (void)subst_std_remove_cvref_t(t_args, p_tp);
  return ttmr_resolved;
}  /* subst_std_remove_cvref */


static a_templ_type_member_result subst_std_enable_if(a_template_arg  *t_args,
                                                      a_type          **p_tp)
/*
Resolve std::enable_if<B,T>::type.  t_args is the argument list <B, T> where B
is a nontype (bool) argument and T is a type.  If B is true, set *p_tp to T and
return ttmr_resolved.  If B is false, the member does not exist; return
ttmr_no_such_member.  If the condition cannot be evaluated to an integral
constant here, return ttmr_not_applicable so the caller can fall back.
*/
{
  a_templ_type_member_result  result;
  a_template_arg              *cond_arg = t_args;
  a_template_arg              *type_arg = (t_args != NULL) ? t_args->next
                                                           : NULL;
  a_constant_ptr              con;

  if (cond_arg == NULL || !is_nontype_templ_arg(cond_arg) ||
      type_arg == NULL || type_arg->kind != tak_type) {
    result = ttmr_not_applicable;
  } else {
    con = cond_arg->variant.constant;
    if (con == NULL || !constant_is(con, ck_integer)) {
      /* The condition is dependent, an error, or otherwise not a plain
         integral constant; let the caller handle it. */
      result = ttmr_not_applicable;
    } else {
      a_boolean             ovflo;
      a_host_large_integer  val = value_of_integer_constant(con, &ovflo);
      if (val != 0) {
        *p_tp = type_arg->variant.type;
        result = ttmr_resolved;
      } else {
        result = ttmr_no_such_member;
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /* subst_std_enable_if */


a_templ_type_member_result eval_intrinsic_templ_type_member(
                                              int             idx,
                                              a_template_arg  *t_args,
                                              a_type_ptr      *substituted_tp)
/*
Resolve the intrinsic class-template member with the given index idx
(corresponding to the enumerators of a_templ_type_member_intrinsic) for an
instance with the template arguments described by t_args.  Return one of the
a_templ_type_member_result values; on ttmr_resolved, set *substituted_tp to
the resulting type.
*/
{
  a_templ_type_member_result  result = ttmr_not_applicable;

  switch (idx) {
    case ttmi_error:
      break;
#define TTMI_dispatch(ns, name, member) \
    case ttmi_##ns##_##name: \
      result = subst_##ns##_##name(t_args, substituted_tp); \
      break;
    NS_templ_type_member_intrinsics(TTMI_dispatch)
#undef TTMI_dispatch
    default:
      unexpected_condition();
  }  /* switch */
  return result;
}  /* eval_intrinsic_templ_type_member */


a_boolean value_of_std_is_integral_v(a_template_arg        *t_args,
                                     ARG_UNUSED a_boolean  *okay)
/*
Set *okay to FALSE if *t_args represents a 128-bit integer type because
std::is_integral_v<int128> has different values depending on the standard
library implementing the trait (we fall back to the definition provided by
the library and skip intrinsic processing).  Otherwise, return TRUE if
*t_args represents a C++ integral type.  
*/
{
  a_boolean   result = FALSE;
  a_type_ptr  tp;

  check_assertion(t_args != NULL && t_args->kind == tak_type);
  tp = skip_typerefs(t_args->variant.type);
  if (type_is(tp, tk_integer) && !tp->variant.integer.enum_type) {
#if INT128_EXTENSIONS_ALLOWED
    if (tp->variant.integer.int_kind == ik_int128 ||
        tp->variant.integer.int_kind == ik_unsigned_int128) {
      /* Whether __int128 (and related types) produces a TRUE value appears
         to be dependent on the library version.  So don't attempt to handle
         that case intrinsically. */
      *okay = FALSE;
    } else
#endif /* INT128_EXTENSIONS_ALLOWED */
    /* Do not insert code here. */
    {
      result = TRUE;
    }  /* if */
  }  /* if */
  return result;
}  /* value_of_std_is_integral_v */


a_boolean value_of_std_is_object_v(a_template_arg        *t_args,
                                   ARG_UNUSED a_boolean  *okay)
/*
Return TRUE if *t_args represents an object type.  (*okay is unused.  It would
be set to FALSE if there were a case where this determination can fail.)
*/
{
  check_assertion(t_args != NULL && t_args->kind == tak_type);
  return is_object_type(t_args->variant.type);
}  /* value_of_std_is_object_v */


a_boolean get_intrinsic_var_templ_value(a_symbol              *vsym,
                                        int                   idx,
                                        a_host_large_integer  *p_val)
/*
vsym represents an instance of a variable template whose instantiated constant
initializer value (of integral type) can be determined intrinsically.  idx
identifies the template (it corresponds to the enumerators of enum type
a_var_templ_intrinsic).  Determine that initial value and return it through
*p_val if possible.  Return TRUE if successful, FALSE otherwise.
*/
{
  a_boolean       okay = TRUE;
  a_template_arg  *t_args = vsym->variant.variable.ptr->template_info
                                                      ->template_arg_list;

  switch (idx) {
#define VTI_dispatch(ns, name) \
    case vti_##ns##_##name: \
      *p_val = value_of_##ns##_##name(t_args, &okay); \
      break;
    NS_var_templ_intrinsics(VTI_dispatch)
#undef VTI_dispatch
    case vti_error:
    default:
      unexpected_condition();
  }  /* switch */
  return okay;
}  /* get_intrinsic_var_templ_value */


#ifdef __linux__

static a_const_char *int_kind_name_for_macro(an_integer_kind kind)
/*
Return a string for the name of certain integer kinds.  This is a simplified
version of the more general int_kind_name routine that is used when providing
strings for various predefined macros where differences between, e.g., "long"
and "long int" are important.  The make_predef_macro_table script can be used
to create a predefined_macros.txt that may have these macros defined with
strings generated by other compilers (e.g., gcc), so the strings generated
here are intended to match those.  Only integer kinds that are likely to appear
in macros are currently implemented.
*/
{
  a_const_char *result = "";

  switch (kind) {
    case ik_char:               result = "char";                   break;
    case ik_signed_char:        result = "signed char";            break;
    case ik_unsigned_char:      result = "unsigned char";          break;
    case ik_short:              result = "short";                  break;
    case ik_unsigned_short:     result = "unsigned short";         break;
    case ik_int:                result = "int";                    break;
    case ik_unsigned_int:       result = "unsigned int";           break;
    case ik_long:               result = "long int";               break;
    case ik_unsigned_long:      result = "long unsigned int";      break;
#if LONG_LONG_ALLOWED
    case ik_long_long:          result = "long long int";          break;
    case ik_unsigned_long_long: result = "long long unsigned int"; break;
#endif /* LONG_LONG_ALLOWED */
    default:
      unexpected_condition_str("unexpected integer kind");
  }  /* switch */
  return result;
}  /* int_kind_name_for_macro */


static void enter_linux_predefined_macros(void)
/*
Enter the macros that are needed when running the front end on
Linux using the gcc/g++ header files.
*/
{
  if (!strict_ansi_mode) {
    (void)enter_predef_macro("1", "unix", /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
  (void)enter_predef_macro("1", "__unix__", /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  /* Note that int_kind_name isn't used in the code below because it would
     give slightly different type names (e.g., "long" instead of "long int")
     and that might cause problems (e.g., for legacy predefined_macros.txt). */
  (void)enter_predef_macro(int_kind_name_for_macro(targ_ptrdiff_t_int_kind),
                           "__PTRDIFF_TYPE__",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro(int_kind_name_for_macro(targ_size_t_int_kind),
                           "__SIZE_TYPE__",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro(int_kind_name_for_macro(targ_wchar_t_int_kind),
                           "__WCHAR_TYPE__",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  if (targ_supports_arm64) {
    /* Macro definitions for the 64-bit version of the ARM architecture. */
    (void)enter_predef_macro("1", "__aarch64__", /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
    (void)enter_predef_macro("1", "__ARM_64BIT_STATE",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
  } else if (targ_supports_arm32) {
    /* Macro definitions for the 32-bit version of the ARM architecture. */
    (void)enter_predef_macro("1", "__arm__", /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
    (void)enter_predef_macro("1", "__ARM_32BIT_STATE",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
  } else if (targ_supports_riscv32 || targ_supports_riscv64) {
    /* Macro definitions for the RISC-V architecture. */
    (void)enter_predef_macro("1", "__riscv__", /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
  } else if (targ_supports_x86_64) {
    /* Macro definitions for the 64-bit version of the x86 architecture. */
    (void)enter_predef_macro("1", "__x86_64", /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
    (void)enter_predef_macro("1", "__x86_64__", /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
  } else {
    /* No macro definitions for the default (32-bit version of the x86
       architecture). */
    check_assertion(target_is_32_bit_x86_based());
  }  /* if */
  (void)enter_predef_macro("1", "__linux__", /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#if defined(__i386) || defined(__i386__)
  /* Define __i386__ and __i386 if the compiler being used to build the
     front end has either defined. */
  (void)enter_predef_macro("1", "__i386__", /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro("1", "__i386", /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* defined(__i386) || defined(__i386__) */
#ifdef __i486__
  /* Define __i486__ if the compiler being used to build the front end
     has it defined. */
  (void)enter_predef_macro("1", "__i486__", /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* ifdef __i486__ */
  if (!gnu_mode || strict_gnu) {
    /* The following macros enable the use of some Linux system header
       files (like stdio.h) when not in GNU C mode. */
    /* Setting __STRICT_ANSI__ disables parts of Linux headers that rely on
       GNU C extensions. */
    /* GNU and Clang define __STRICT_ANSI__ when -std=c* is used, but not
       when -std=gnu* is used; emulate that with strict_gnu. */
    (void)enter_predef_macro("1", "__STRICT_ANSI__",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
#if INT128_EXTENSIONS_ALLOWED
  if (int128_extensions_enabled && !C_mode() &&
      !strict_gnu && (gnu_version_is(>=50000) || clang_mode)) {
    /* With -std=gnu++* (but not -std=c++*), define macros indicating that
       128-bit integers are available. */
    (void)enter_predef_macro("128", "__GLIBCXX_BITSIZE_INT_N_0",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
    (void)enter_predef_macro("__int128", "__GLIBCXX_TYPE_INT_N_0",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
#endif /* INT128_EXTENSIONS_ALLOWED */
  if (!gnu_mode) {
    if (pass_stdarg_references_to_generated_code) {
      /* <stdio.h> refers to __gnuc_va_list which is declared in the Linux
         version of <stdarg.h>.  Since we're in a mode that bypasses the actual
         inclusion of <stdarg.h>, we must define __gnu_va_list separately. */
      (void)enter_predef_macro("va_list", "__gnuc_va_list",
                               /*cannot_be_redefined=*/FALSE,
                               /*ref_suppresses_pch_file=*/FALSE);
    }  /* if */
  } else if (gpp_mode) {
    /* In GNU C++ mode (but not in GNU C mode), _GNU_SOURCE is predefined
       on Linux systems.  This macro guards GNU extensions in GNU operating
       system header files. */
    (void)enter_predef_macro("1", "_GNU_SOURCE",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
}  /* enter_linux_predefined_macros */

#endif /* ifdef __linux__ */

#ifdef __sparc

static void enter_sparc_predefined_macros(void)
/*
Enter the standard predefined macros for a SPARC system.
*/
{
  if (!strict_ansi_mode) {
    (void)enter_predef_macro("1", "unix", /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
    (void)enter_predef_macro("1", "sun", /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
    (void)enter_predef_macro("1", "sparc", /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
  (void)enter_predef_macro("1", "__unix", /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro("1", "__sun", /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro("1", "__sparc", /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
}  /* enter_sparc_predefined_macros */

#endif /* ifdef __sparc */

#if defined(__APPLE__) && defined(__MACH__)

static void enter_macosx_predefined_macros(void)
/*
Enter some predefined macros for a MacOS X (Apple) system.
*/
{
  (void)enter_predef_macro("1", "__APPLE__", /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro("1", "__MACH__", /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
#if defined(__BIG_ENDIAN__)
  (void)enter_predef_macro("1", "_BIG_ENDIAN", /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro("1", "__BIG_ENDIAN__",
                           /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* defined(__BIG_ENDIAN__) */
#if defined(__ppc__)
  (void)enter_predef_macro("1", "__ppc__", /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* defined(__ppc__) */
#if defined(__POWERPC__)
  (void)enter_predef_macro("1", "__POWERPC__", /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* defined(__POWERPC__) */
#if defined(_ARCH_PPC)
  (void)enter_predef_macro("1", "_ARCH_PPC", /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* defined(_ARCH_PPC) */
  if (!C_mode()) {
    /* Some older MacOS X headers included insufficient guards for the C mode
       typedef of wchar_t.  It appears to be fixed in the more recent headers,
       but to enable earlier versions we nevertheless explicitly disable the
       typedef in C++ modes by defining the _BSD_WCHAR_T_DEFINED macro. */
    (void)enter_predef_macro("1", "_BSD_WCHAR_T_DEFINED",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
}  /* enter_macosx_predefined_macros */

#endif /* defined(__APPLE__) && defined(__MACH__) */
#if BUILTIN_FUNCTIONS_ENABLED

static a_boolean is_consteval_builtin(a_builtin_function_kind kind)
/*
Given a builtin function kind, return TRUE if the builtin function is a
consteval builtin.
*/
{
  a_boolean result;

  switch (kind) {
    case bfk_source_location:
    case bfk_COLUMN:
    case bfk_LINE:
    case bfk_FILE:
    case bufk_FILE_NAME:
    case bfk_FUNCTION:
    case bufk_FUNCSIG:
      result = TRUE;
      break;
    default:
      result = FALSE;
      break;
  }  /* switch */
  return result;
}  /* is_consteval_builtin */


static a_symbol_ptr enter_builtin_function(a_const_char            *name,
                                           a_type_ptr              rout_type,
                                           a_builtin_function_kind kind,
                                           a_symbol_locator        *loc)
/*
Enter a builtin function with the given name and type (which must be a
tk_routine type -- possibly with a typeref that describes attributes).  The
builtin function corresponds to the (a_builtin_function_kind_tag or
a_builtin_user_function_kind) kind.  If non-NULL, loc specifies the symbol
locator for name.  The routine is given C name linkage (and the routine type is
updated accordingly).  Return the newly-created symbol for the builtin
function.
*/
{
  a_symbol_ptr        sym;
  a_symbol_locator    local_loc;
  a_name_linkage_kind saved_name_linkage = enum_cast<a_name_linkage_kind>(
                           scope_stack[decl_scope_level].default_name_linkage);

  /* In cases where attributes are part of the function type, a typeref
     may be present here; skip it (the attributes are already reflected in
     the underlying type). */
  rout_type = skip_typerefs(rout_type);
  check_assertion(rout_type->kind == (a_type_kind)tk_routine);
  if (loc == NULL) {
    /* Find the symbol header if not specified by the caller. */
    clear_locator(&local_loc, &null_source_position);
    (void)find_symbol(name, (sizeof_t)strlen(name), &local_loc);
    loc = &local_loc;
  }  /* if */
  if (!loc->symbol_header->is_builtin_overloadable) {
    /* Non-overloadable builtin functions have extern "C" name linkage by
       default. */
    scope_stack[decl_scope_level].default_name_linkage = nlk_external;
  }  /* if */
  sym = make_predeclared_function_symbol(loc, rout_type);
  if (!loc->symbol_header->is_builtin_overloadable) {
    check_assertion(sym->variant.routine.ptr->source_corresp.name_linkage
                                                              == nlk_external);
    check_assertion(rout_type->variant.routine.extra_info->routine_name_linkage
                                                              == nlk_external);
  }  /* if */
  /* Restore the previous default name linkage. */
  scope_stack[decl_scope_level].default_name_linkage = saved_name_linkage;
  sym->explicit_linkage_specifier = !C_mode();
  mark_builtin_loaded(sym->header);
  sym->variant.routine.ptr->variant.builtin_function_kind = kind;
  sym->variant.routine.ptr->is_consteval = is_consteval_builtin(kind);
#if DEBUG
  if (db_flag_is_set("dump_builtins")) {
    /* Dump builtin function declarations. */
    an_il_to_str_output_control_block octl;
    fprintf(f_debug, "/* %s */ ", sym->header->identifier);
    clear_il_to_str_output_control_block(&octl);
    octl.output_str = put_str_to_f_debug;
    form_type_first_part(rout_type, /*under_lhs_declarator=*/FALSE,
                         /*need_trailing_space=*/FALSE, TQ_NONE,
                         FTO_NO_OPTIONS, &octl);
    fprintf(f_debug, "%s", sym->header->identifier);
    form_type_second_part(rout_type, /*under_lhs_declarator=*/FALSE,
                          FTO_NO_OPTIONS, &octl);
    fprintf(f_debug, ";\n");
  }  /* if */
#endif /* DEBUG */
  return sym;
}  /* enter_builtin_function */


static a_boolean builtin_matches_version_range(unsigned long version,
                                               a_const_char  **cond_range)
/*
Returns TRUE if "version" falls within the range of versions specified by
cond_range (which is part of a a_builtin_condition_string).
*/
{
  unsigned long  min_version = 0, max_version = (unsigned long)-1;
  a_const_char   *str = *cond_range;

  check_assertion_str(str[0] == '(', "invalid version range configuration");
  str += 1;
  if (str[0] != '-') {
    check_assertion_str(str[0] >= '0' && str[0] <= '9',
                        "invalid version range configuration");
    min_version = strtoul(str, (char **)&str, 10);
  }  /* if */
  if (str[0] == '-') {
    str += 1;
    if (str[0] >= '0' && str[0] <= '9') {
      max_version = strtoul(str, (char **)&str, 10);
    }  /* if */
  } else {
    /* Not a range, but a single version number. */
    max_version = min_version;
  }  /* if */
  check_assertion_str(str[0] == ')', "invalid version range configuration");
  *cond_range = str+1;
  return version >= min_version && version <= max_version;
}  /* builtin_matches_version_range */


static void builtin_condition_enabled(
                                 a_builtin_condition_string condition,
                                 a_boolean                  *primary_enabled,
                                 a_boolean                  *secondary_enabled,
                                 a_const_char               **restrictions)
/*
For the given builtin condition string, sets *primary_enabled to TRUE if
the condition string causes a "primary" declaration to be enabled in the
current configuration and sets *secondary_enabled to TRUE if a "secondary"
declaration is enabled by the string.  Also sets *restrictions to point to
a list of characters representing restrictions (or NULL if there are no
restrictions).
*/
{
  a_boolean     result, has_secondary;
  a_const_char  *p = condition, *res_ptr;
  unsigned long version;

  check_assertion(p != NULL);
  while (*p != '\0') {
    result = TRUE;
    res_ptr = NULL;
    if (*p == 'S') {
      has_secondary = TRUE;
      p++;
    } else {
      has_secondary = FALSE;
    }  /* if */
    if (*p == 'g' || *p == 'L' || *p == 'm' || *p == 's') {
      if (*p == 'g') {
        result = result && (gnu_mode && !clang_mode);
        version = gnu_version;
      } else if (*p == 'L') {
        result = result && (gnu_mode && clang_mode);
        version = clang_version;
      } else if (*p == 'm') {
        result = result && ms_extensions;
        version = microsoft_version;
      } else {
        check_assertion(*p == 's');
        version = std_version;
      }  /* if */
      p++;
      check_assertion(*p == 'x' || *p == 'c' || *p == '+');
      result = result && ((*p == 'x') ||
                          (*p == 'c' && C_mode()) ||
                          (*p == '+' && !C_mode()));
      p++;
      if (*p == 'A') {
        result = result && target_is_arm_based();
        p++;
      } else if (*p == 'R') {
        result = result && target_is_riscv_based();
        p++;
      } else if (*p == 'X') {
        result = result && target_is_x86_based();
        p++;
      }  /* if */
      if (*p == '4') {
        result = result && !target_is_64_bits();
        p++;
      } else if (*p == '8') {
        result = result && target_is_64_bits();
        p++;
      }  /* if */
      if (*p == '(') {
        /* A range specification follows (note that the version range is
           inspected even if result is FALSE because the pointer needs to
           be updated to point past the version range). */
        result = builtin_matches_version_range(version, &p) && result;
      }  /* if */
      if (*p == '[') {
        /* This string has restrictions; save a pointer for later. */
        p++;
        res_ptr = p;
        p = strchr(p, ']');
        check_assertion(p != NULL);
        p++;
      }  /* if */
      if (result) {
        *primary_enabled = TRUE;
        *restrictions = res_ptr;
        if (!*secondary_enabled) {
          *secondary_enabled = has_secondary;
          if (has_secondary) {
            /* Both primary and secondary are enabled; no need to look any
               further. */
            break;
          }  /* if */
        }  /* if */
      }  /* if */
    } else {
      unexpected_condition();
    }  /* if */
  }  /* while */
}  /* builtin_condition_enabled */


static a_boolean builtin_enabled(unsigned short             cond_index,
                                 a_builtin_condition_string condition,
                                 a_boolean                  is_secondary)
/*
Returns TRUE if the builtin condition is satisfied in the current emulation
mode.  The condition is given by "condition" if it is non-NULL, otherwise
cond_index is assumed to be an index into builtin_condition_table where the
condition string is specified by the "condition_string" field of that entry.
If is_secondary is TRUE, this declaration is for the "secondary" declaration
(i.e., one without the "__builtin_" prefix).  In that case, make sure an 'S' is
present in the condition (indicating that a secondary declaration is allowed).
*/
{
  a_boolean result;

  if (condition != NULL) {
    a_boolean primary_enabled = FALSE, secondary_enabled = FALSE;
    a_const_char    *restrictions;
    builtin_condition_enabled(condition, &primary_enabled, &secondary_enabled,
                              &restrictions);
    result = (is_secondary ? secondary_enabled : primary_enabled);
  } else {
    a_builtin_function_condition *bfcp = &builtin_condition_table[cond_index];
    check_assertion(cond_index < (unsigned short)bfci_last);
    if (!bfcp->evaluated) {
      builtin_condition_enabled(builtin_condition_strings[cond_index],
                                &bfcp->primary_enabled,
                                &bfcp->secondary_enabled,
                                &bfcp->restrictions);
      bfcp->evaluated = TRUE;
    }  /* if */
    result = (is_secondary ? bfcp->secondary_enabled : bfcp->primary_enabled);
  }  /* if */
  return result;
}  /* builtin_enabled */


static a_boolean check_restrictions_met(a_const_char  *restrictions,
                                        a_boolean     issue_error)
/*
Returns TRUE if the specified restrictions are met in the current configuration
(or restrictions is NULL).  If FALSE is returned an error is issued (only if
issue_error is TRUE).
*/
{
  a_boolean  result = TRUE;

  if (restrictions != NULL) {
    while (*restrictions != ']' && *restrictions != '\0') {
      switch (*restrictions) {
        case 'c':
          /* char8_t enabled? */
          if (char8_t_enabled) {
            /* Okay. */
          } else {
            if (issue_error) {
              pos_error(ec_builtin_needs_char8_t, &pos_curr_token);
            }  /* if */
            result = FALSE;
          }  /* if */
          break;
        case 'f':
          /* Ensure that 128-bit floating-point types are configured and
             enabled. */
#if FLOAT128_ENABLING_POSSIBLE
          if (float128_enabled) {
            /* Okay. */
          } else
#endif /* FLOAT128_ENABLING_POSSIBLE */
          {
            if (issue_error) {
              pos_error(ec_builtin_needs_128_bit_floats, &pos_curr_token);
            }  /* if */
            result = FALSE;
          }  /* if */
          break;
        case 'i':
          /* Ensure that 128-bit integers are configured and enabled. */
#if INT128_EXTENSIONS_ALLOWED
          if (int128_extensions_enabled) {
            /* Okay. */
          } else
#endif /* INT128_EXTENSIONS_ALLOWED */
          {
            if (issue_error) {
              pos_error(ec_builtin_needs_128_bit_integers, &pos_curr_token);
            }  /* if */
            result = FALSE;
          }  /* if */
          break;
        case 'v':
          /* GNU vector types must be configured. */
#if !GNU_VECTOR_TYPES_ALLOWED
          if (issue_error) {
            pos_error(ec_builtin_needs_vector_types, &pos_curr_token);
          }  /* if */
          result = FALSE;
#endif /* !GNU_VECTOR_TYPES_ALLOWED */
          break;
        default:
          unexpected_condition();
      }  /* switch */
      restrictions++;
    }  /* while */
  }  /* if */
  return result;
}  /* check_restrictions_met */


static a_boolean builtin_restrictions_met(a_symbol_header *sym_hdr,
                                          a_boolean       issue_error)
/*
Returns TRUE if the builtin function referred to by sym_hdr has no restrictions
or those restrictions are met in the current configuration.  If FALSE is
returned an error is issued (only if issue_error is TRUE).
*/
{
  a_boolean     result = TRUE;
  a_const_char  *restrictions = NULL;

  if (sym_hdr->builtin_function_category == bfc_user) {
    /* For a user-defined builtin function, re-parse the condition string to
       see if there are any restrictions. */
    a_boolean primary_enabled = FALSE, secondary_enabled = FALSE;
    a_builtin_user_descr_ptr budp =
                          &builtin_user_table[sym_hdr->builtin_function_index];
    builtin_condition_enabled(budp->cond, &primary_enabled, &secondary_enabled,
                              &restrictions);
  } else {
    /* The restriction string (if any) has already been found for non-user
       defined builtins. */
    const a_builtin_descr *bdp;
    bdp = builtin_tables[sym_hdr->builtin_function_category] +
                                               sym_hdr->builtin_function_index;
    restrictions = builtin_condition_table[bdp->cond_index].restrictions;
  }  /* if */
  result = check_restrictions_met(restrictions, issue_error);
  if (!result && issue_error) {
    /* Prevent cascading errors for this builtin function. */
    check_assertion(locator_for_curr_id.symbol_header == sym_hdr);
    curr_token = tok_identifier;
    make_specific_symbol_error_locator(&locator_for_curr_id);
  }  /* if */
  return result;
}  /* builtin_restrictions_met */


static a_type_ptr builtin_function_type(a_builtin_type_string type_string,
                                        a_source_position     *err_source_pos)
/*
Parse the builtin function type specified by type_string and return the
resulting type.  See also scan_top_level_generated_code (which is similar).
*/
{
  a_type_ptr        result;
  a_token_cache     cache;
  a_boolean         saved_scanning_generated_code = scanning_generated_code;
  a_boolean         saved_next_token_is_top_level_decl_start =
                                            next_token_is_top_level_decl_start;
  a_boolean         saved_allow_ellipsis_only_param_in_C_mode =
                                         allow_ellipsis_only_param_in_C_mode;
  a_const_char      *saved_start_of_curr_token = start_of_curr_token;
  a_const_char      *saved_end_of_curr_token = end_of_curr_token;
  a_symbol_locator  saved_locator_for_curr_id = locator_for_curr_id;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  a_source_position saved_curr_construct_end_position =
                                                   curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
#if GENERATE_SOURCE_SEQUENCE_LISTS
  a_boolean         saved_source_sequence_entries_disallowed =
                                            source_sequence_entries_disallowed;

  /* Don't generate source sequence entries for builtins. */
  source_sequence_entries_disallowed = TRUE;
  scope_stack_top().source_sequence_entries_disallowed = TRUE;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  scanning_generated_code = TRUE;
  allow_ellipsis_only_param_in_C_mode = TRUE;
  check_assertion(depth_innermost_namespace_scope == DEPTH_OF_FILE_SCOPE);
  /* Save the lexical state. */
  push_lexical_state_stack();
  /* Inject an end-of-source token into the token stream to prevent
     reading past the end. */
  terminate_token_cache(&cache);
  rescan_cached_tokens(&cache);
  /* Insert the builtin function type into the token stream. */
  insert_string_into_token_stream(type_string, /*insert_after=*/FALSE,
                                  /*p_expand_macros=*/FALSE,
                                  /*suspend_caching=*/FALSE,
                                  *err_source_pos);
  /* Scan the type. */
  type_name(&result);
  /* Get the injected end of source token. */
  check_assertion(curr_token == tok_end_of_source);
  (void)get_token();
  /* Restore the lexical state. */
  pop_lexical_state_stack();
  /* Restore the flags. */
  allow_ellipsis_only_param_in_C_mode =
                                     saved_allow_ellipsis_only_param_in_C_mode;
  scanning_generated_code = saved_scanning_generated_code;
  next_token_is_top_level_decl_start =
                                      saved_next_token_is_top_level_decl_start;
#if GENERATE_SOURCE_SEQUENCE_LISTS
  source_sequence_entries_disallowed =
                                      saved_source_sequence_entries_disallowed;
  scope_stack_top().source_sequence_entries_disallowed =
                                      saved_source_sequence_entries_disallowed;
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */
  start_of_curr_token = saved_start_of_curr_token;
  end_of_curr_token = saved_end_of_curr_token;
  locator_for_curr_id = saved_locator_for_curr_id;
#if EXTRA_SOURCE_POSITIONS_IN_IL
  curr_construct_end_position = saved_curr_construct_end_position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
  return result;
}  /* builtin_function_type */


static a_type_ptr builtin_function_type_for_index(
                                               a_builtin_type_index type_index)
/*
Return the type associated with the specified builtin function type index.
Parse the specified type if it has not been parsed yet.
*/
{
  a_type_ptr tp = builtin_type_table->get(type_index + 1);

  check_assertion(type_index < (unsigned short)bfti_last);
  if (tp == NULL) {
    tp = builtin_function_type(builtin_type_strings[type_index],
                               &pos_curr_token);
    builtin_type_table->map(type_index + 1, tp);
  }  /* if */
  check_assertion(tp != NULL && !is_error_type(tp));
  return tp;
}  /* builtin_function_type_for_index */


static void get_builtin_table_entry_name(const a_builtin_descr  *bdp,
                                         Small_string<64>       *name,
                                         a_const_char           **ovl_info)
/*
Set *name to the name of the given builtin function table entry.  A name
starting with a '#' character, followed by a digit, indicates that the name is
formed from the name of the previous table entry (which must then be in *name
on entry) by stripping off the specified number of '_'-separated components and
then appending the new suffix; otherwise, the entry is stored with its full
name.  If the entry name contains an '@' character (overload information),
*ovl_info is set to point at the character following the '@'; otherwise, it is
set to NULL.
*/
{
  a_const_char  *p = bdp->name;

  if (*p != '#') {
    while (*p != '\0' && *p != '@') {
      ++p;
    }  /* while */
    *name = a_string_view(bdp->name, (size_t)(p - bdp->name));
  } else {
    /* The new name shares a common prefix with the previous one. */
    unsigned  nr_to_remove;
    size_t    len = name->length();
    ++p;
    check_assertion(*p >= '0' && *p <= '9');
    nr_to_remove = (unsigned)((*p) - '0');
    ++p;
    /* Skip over the specified number of '_'-separated components from the
       end. */
    while (nr_to_remove != 0) {
      while ((*name)[len - 1] != '_') --len;
      --len;
      --nr_to_remove;
    }  /* while */
    name->truncate_to(len);
    /* Append the new suffix. */
    for (len = 0; *p && *p != '@'; ++p) ++len;
    name->append(a_string_view(bdp->name + 2, len));
  }  /* if */
  *ovl_info = *p == '@' ? p + 1 : NULL;
}  /* get_builtin_table_entry_name */


static void builtin_overload_base_name(const Small_string<64>  *name,
                                       a_const_char            *ovl_info,
                                       Small_string<64>        *ovl_name)
/*
Set *ovl_name to the name of the additional overload described by the '@'
overload information of a builtin function table entry whose name is given by
name, where ovl_info points at the digits following the '@' (any
compiler-restricting letter must have been skipped by the caller): for the form
"n", the trailing n '_'-separated components of the name are removed, and for
the form "nm", only the m components starting at the nth component from the end
are removed.
*/
{
  size_t        pos = name->length();
  unsigned      nr_to_skip;
  a_const_char  *p = ovl_info;

  check_assertion(*p >= '0' && *p <= '9');
  nr_to_skip = (unsigned)(*p - '0');
  ++p;
  /* Skip over the specified number of '_'-separated components from the
     end. */
  while (nr_to_skip != 0) {
    --pos;
    while ((*name)[pos - 1] != '_') --pos;
    --nr_to_skip;
  }  /* while */
  *ovl_name = a_string_view(name->as_temp_characters(), pos - 1);
  if (*p != '\0') {
    check_assertion(*p >= '0' && *p <= '9');
    /* Skip over the specified number of '_'-separated components towards the
       end. */
    nr_to_skip = (unsigned)(*p - '0');
    while (nr_to_skip != 0) {
      while ((*name)[pos] != '_') ++pos;
      ++pos;
      --nr_to_skip;
    }  /* while */
    --pos;
    /* Append the suffix to the overload name. */
    ovl_name->append(a_string_view(name->as_temp_characters() + pos,
                                   name->length() - pos));
  }  /* if */
}  /* builtin_overload_base_name */


static a_const_char *builtin_overload_info_for_current_mode(
                                                       a_const_char  *ovl_info)
/*
The given '@' overload information of a builtin function table entry (ovl_info
points at the character following the '@', or is NULL if the entry has no such
information) may be restricted to one compiler by a leading letter: 'L' for
Clang and 'g' for GCC.  Return NULL if the overload is not declared in the
current emulation mode; otherwise, return ovl_info advanced past any compiler
letter.
*/
{
  if (ovl_info != NULL && (*ovl_info == 'L' || *ovl_info == 'g')) {
    if ((*ovl_info == 'L' && clang_mode) ||
        (*ovl_info == 'g' && gnu_mode && !clang_mode)) {
      ++ovl_info;
    } else {
      ovl_info = NULL;
    }  /* if */
  }  /* if */
  return ovl_info;
}  /* builtin_overload_info_for_current_mode */


/* Type used for an index into builtin_overload_set_members.  Values are
   stored as one plus the index, so that zero denotes "no member". */
typedef unsigned int a_builtin_overload_set_member_index;

typedef struct a_builtin_overload_set_member {
  a_builtin_function_index
                table_index;
                        /* The index of the entry in the builtin table. */
  a_builtin_function_category
                category;
                        /* The builtin function category selecting the
                           builtin table. */
  a_builtin_overload_set_member_index
                next;   /* One plus the index, in builtin_overload_set_members,
                           of the node for the next member of the overload set,
                           or zero if this is the last recorded member. */
} a_builtin_overload_set_member;

typedef struct a_builtin_overload_set_member_list {
  a_builtin_overload_set_member_index
                head;   /* One plus the index, in builtin_overload_set_members,
                           of the node for the first member of the overload
                           set. */
  a_builtin_overload_set_member_index
                tail;   /* One plus the index, in builtin_overload_set_members,
                           of the node for the last member of the overload
                           set. */
} a_builtin_overload_set_member_list;

namespace detail {

template<>
struct Is_trivially_copyable_edg_impl<a_builtin_overload_set_member> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

template<>
struct Is_trivially_destructible_edg_impl<a_builtin_overload_set_member> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

template<>
struct Is_trivially_copyable_edg_impl<a_builtin_overload_set_member_list> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_copyable_edg_impl */

template<>
struct Is_trivially_destructible_edg_impl<a_builtin_overload_set_member_list> :
                                                Integral_constant<bool, true> {
};  /* Is_trivially_destructible_edg_impl */

}  /* namespace detail */

#if EXPENSIVE_CHECKING

static inline a_boolean operator==(a_builtin_overload_set_member_list  list_1,
                                   a_builtin_overload_set_member_list  list_2)
/*
Return TRUE if the two given overload set member lists are equal.
*/
{
  return list_1.head == list_2.head && list_1.tail == list_2.tail;
}  /* operator== */


static inline a_boolean operator!=(a_builtin_overload_set_member_list  list_1,
                                   a_builtin_overload_set_member_list  list_2)
/*
Return TRUE if the two given overload set member lists are not equal.
*/
{
  return !(list_1 == list_2);
}  /* operator!= */

#endif /* EXPENSIVE_CHECKING */

STATIC_THREAD Dyn_array<a_builtin_overload_set_member>
                *builtin_overload_set_members;
                        /* The nodes of the overload set member lists of the
                           current translation unit. */

using a_builtin_overload_set_map = Ptr_map<a_symbol_header*,
                                           a_builtin_overload_set_member_list>;
                        /* The type of a map that associates the symbol header
                           of a builtin function overload set name with the
                           member list of the set. */

STATIC_THREAD a_builtin_overload_set_map
                *builtin_overload_set_map;
                        /* Maps the symbol header of a builtin function
                           overload set name to the member list of the set. */

STATIC_THREAD unsigned int
                builtin_overload_set_categories;
                        /* A bit mask of the builtin function categories whose
                           tables have been loaded by
                           load_overloadable_builtin_symbols (in any
                           translation unit of the compilation) and may contain
                           entries of builtin function overload sets.  The mask
                           is reset when a primary translation unit starts and
                           is saved and restored with precompiled headers;
                           otherwise it is shared by all translation units of a
                           compilation. */

STATIC_THREAD unsigned int
                mapped_overload_set_categories;
                        /* A bit mask of the builtin function categories whose
                           tables have been scanned for overload set members in
                           the current translation unit. */

/* builtin_overload_set_categories and mapped_overload_set_categories have
   one bit per builtin function category. */
static_assert(bfc_last <= 8*sizeof(unsigned int),
              "too many builtin function categories for the category bit "
              "masks");

static void add_builtin_overload_set_member(
                                      a_symbol_header              *sym_hdr,
                                      a_builtin_function_category  bfc,
                                      a_builtin_function_index     table_index)
/*
Record the builtin function table entry given by bfc and table_index as a
member of the overload set named by sym_hdr.
*/
{
  a_builtin_overload_set_member        member = { table_index, bfc, 0 };
  a_builtin_overload_set_member_list   list;
  a_builtin_overload_set_member_index  node_index;

  node_index = (a_builtin_overload_set_member_index)
                                        builtin_overload_set_members->length();
  builtin_overload_set_members->push_back(member);
  list = builtin_overload_set_map->get(sym_hdr);
  if (list.head == 0) {
    /* This is the first recorded member of the set. */
    list.head = node_index + 1;
    list.tail = node_index + 1;
    builtin_overload_set_map->map(sym_hdr, list);
  } else {
    /* Append the new member at the end of the list. */
    (*builtin_overload_set_members)[list.tail - 1].next = node_index + 1;
    list.tail = node_index + 1;
    builtin_overload_set_map->replace(sym_hdr, list);
  }  /* if */
}  /* add_builtin_overload_set_member */


static a_symbol_ptr enter_builtin_overload_set_members(
                                                     a_symbol_header  *sym_hdr)
/*
Enter a routine for every recorded member entry of the overload set named by
sym_hdr.  Return the symbol for the last routine entered, or NULL if no
routines were entered.
*/
{
  a_symbol_ptr  result = NULL;
  a_builtin_overload_set_member_index
                node_index = builtin_overload_set_map->get(sym_hdr).head;

  if (node_index != 0) {
    while (node_index != 0) {
      a_builtin_overload_set_member
                  *member = &(*builtin_overload_set_members)[node_index - 1];
      const a_builtin_descr
                  *bdp = builtin_tables[member->category]+member->table_index;
      a_type_ptr  builtin_type =
                              builtin_function_type_for_index(bdp->type_index);
      result = enter_builtin_function(sym_hdr->identifier, builtin_type,
                                      bdp->kind, (a_symbol_locator *)NULL);
      node_index = member->next;
    }  /* while */
  }  /* if */
  return result;
}  /* enter_builtin_overload_set_members */


static void map_builtin_overload_sets(a_builtin_function_category  bfc)
/*
Scan the builtin table for the given builtin function category and record the
overload set memberships of its entries.  This is used when the overload set
members of the table were not recorded in the current translation unit when the
table was loaded.  Only entries that are enabled in the current emulation mode
and whose name (or abbreviated overload name) denotes a symbol header that is
already marked as an overload set are recorded.
*/
{
  const a_builtin_descr *bdp;
  Small_string<64>      name;

  for (bdp = builtin_tables[bfc]; bdp->name != NULL; bdp++) {
    a_const_char  *ovl_info;
    get_builtin_table_entry_name(bdp, &name, &ovl_info);
    ovl_info = builtin_overload_info_for_current_mode(ovl_info);
    if (builtin_enabled(bdp->cond_index, NULL, /*is_secondary=*/FALSE)) {
      a_const_char  *restrictions =
                         builtin_condition_table[bdp->cond_index].restrictions;
      if (check_restrictions_met(restrictions, /*issue_error=*/FALSE)) {
        a_symbol_locator          loc;
        a_builtin_function_index  table_index =
                         (a_builtin_function_index)(bdp - builtin_tables[bfc]);
        clear_locator(&loc, &null_source_position);
        (void)find_symbol(name.as_temp_characters(), name.length(), &loc);
        if (loc.symbol_header->is_builtin_overload_set) {
          add_builtin_overload_set_member(loc.symbol_header, bfc,
                                          table_index);
        }  /* if */
        if (ovl_info != NULL) {
          Small_string<64>  ovl_name;
          builtin_overload_base_name(&name, ovl_info, &ovl_name);
          clear_locator(&loc, &null_source_position);
          (void)find_symbol(ovl_name.as_temp_characters(), ovl_name.length(),
                            &loc);
          if (loc.symbol_header->is_builtin_overload_set) {
            add_builtin_overload_set_member(loc.symbol_header, bfc,
                                            table_index);
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* for */
}  /* map_builtin_overload_sets */


a_symbol_ptr load_matching_builtin_function(a_symbol_header *sym_hdr)
/*
The builtin function referred to by sym_hdr has not yet been loaded and a
reference has been made to it, so create the routine entry now.  If the name
denotes an overload set, the routines for the member entries of the overload
set with that name are created; otherwise, the single routine recorded in the
symbol header is created.  Note that this may be called at various points
during the translation, so care must be taken to save and restore the state of
the compilation while a file-scope routine is created (and potentially a
routine type is parsed).  Return the newly-created symbol for the builtin
function.
*/
{
  a_symbol_ptr     result = NULL;
  a_scope_depth    saved_decl_scope_level = decl_scope_level;
  a_boolean        name_linkage_pushed = FALSE;
  a_type_ptr       builtin_type;
  a_builtin_function_kind builtin_kind;

  check_assertion(sym_hdr->is_builtin_function);
  mark_builtin_loaded(sym_hdr);
  if (sym_hdr->is_builtin_overload_set ||
      (sym_hdr->builtin_function_category != bfc_keyword &&
       builtin_restrictions_met(sym_hdr, /*issue_error=*/TRUE))) {
    /* Push a scope suitable for a new top-level declaration. */
    push_new_top_level_declaration();
    decl_scope_level = DEPTH_OF_FILE_SCOPE;
    /* Builtins are always have C linkage. */
    if (scope_stack[depth_scope_stack].default_name_linkage !=
                                           (a_name_linkage_kind)nlk_external) {
      push_name_linkage((a_name_linkage_kind)nlk_external);
      name_linkage_pushed = TRUE;
    }  /* if */
    if (sym_hdr->is_builtin_overload_set) {
      /* Enter the routines of the overload set with this name.  The member
         entries of the set are recorded when the builtin tables are loaded; if
         that recording is not available in this translation unit for a loaded
         category, scan the table now. */
      unsigned int  unmapped = builtin_overload_set_categories &
                                               ~mapped_overload_set_categories;
      for (unsigned  bfc = 0; bfc < (unsigned)bfc_last; ++bfc) {
        if ((unmapped & (1U << bfc)) != 0) {
          map_builtin_overload_sets((a_builtin_function_category)bfc);
          mapped_overload_set_categories |= (1U << bfc);
        }  /* if */
      }  /* for */
      result = enter_builtin_overload_set_members(sym_hdr);
    } else {
      if (sym_hdr->builtin_function_category == bfc_user) {
        a_builtin_user_descr_ptr budp =
                          &builtin_user_table[sym_hdr->builtin_function_index];
        builtin_type = builtin_function_type(budp->type_string,
                                             &pos_curr_token);
        builtin_kind = budp->kind;
      } else {
        const a_builtin_descr *bdp;
        bdp = builtin_tables[sym_hdr->builtin_function_category] +
                                               sym_hdr->builtin_function_index;
        builtin_type = builtin_function_type_for_index(bdp->type_index);
        builtin_kind = bdp->kind;
      }  /* if */
      result = enter_builtin_function(sym_hdr->identifier, builtin_type,
                                      builtin_kind, (a_symbol_locator *)NULL);
    }  /* if */
    /* Restore name linkage and scope. */
    if (name_linkage_pushed) {
      pop_name_linkage();
    }  /* if */
    decl_scope_level = saved_decl_scope_level;
    pop_scope();
  }  /* if */
  return result;
}  /* load_matching_builtin_function */


void load_matching_builtin_function_by_name(a_const_char *name)
/*
Loads the builtin function whose name is specified.
*/
{
  a_symbol_locator  loc;
  
  clear_locator(&loc, &null_source_position);
  (void)load_matching_builtin_function(find_symbol_header(name, strlen(name),
                                                          &loc));
}  /* load_matching_builtin_function_by_name */


a_boolean builtin_function_or_keyword_is_enabled(a_const_char *name)
/*
Returns TRUE if a builtin function or keyword with the specified name is
enabled in the current emulation mode.  Many type intrinsics are implemented
as keywords and __has_builtin returns TRUE for these.
*/
{
  a_boolean         result = FALSE;
  a_symbol_locator  loc;
  
  clear_locator(&loc, &null_source_position);
  (void)find_symbol(name, (sizeof_t)strlen(name), &loc);
  if (loc.symbol_header != NULL &&
      (loc.symbol_header->is_builtin_function ||
       (loc.symbol_header->symbol != NULL &&
        symbol_is(loc.symbol_header->symbol, sk_keyword)) ||
       (loc.symbol_header->has_intrinsic_name &&
        is_intrinsic_type_transform_name(loc.symbol_header)))) {
    result = TRUE;
  }  /* if */
  return result;
}  /* builtin_function_or_keyword_is_enabled */


static void preload_builtin_symbol(
                           a_const_char                 *builtin_name,
                           unsigned short               cond_index,
                           a_builtin_condition_string   condition,
                           a_builtin_function_index     idx,
                           a_builtin_function_category  function_category,
                           a_builtin_function_kind      kind,
                           unsigned short               type_index,
                           a_builtin_type_string        type_string)
/*
Create a symbol header for the builtin function named by builtin_name and mark
that it is associated with a builtin function.  If the builtin has a
"secondary" declaration (i.e., one without the __builtin prefix), that will be
entered as well, but only in C mode.  condition is a string that describes the
conditions in which the builtin is applicable.  If it is NULL, cond_index is
used in its place and specifies an index into builtin_condition_table.  idx is
the array index (into either a system builtin table or the builtin_user_table
depending on the value of function_category) for this builtin function.  kind
is the a_builtin_function_kind or a_builtin_user_function_kind enum value that
corresponds to this builtin function.  If type_string is non-NULL, it is a
string that gives the builtin function's type; otherwise, type_index is an
index into builtin_type_table for the builtin function's type.
*/
{
  a_symbol_locator loc;
  a_type_ptr       builtin_type = NULL;
  a_const_char     *name = builtin_name;

  clear_locator(&loc, &null_source_position);
  (void)find_symbol(name, (sizeof_t)strlen(name), &loc);
  if (loc.symbol_header->is_builtin_function &&
      loc.symbol_header->builtin_function_category == bfc_user &&
      function_category != bfc_user) {
    /* A user builtin function has already been loaded (and that takes
       precedence over a non-user builtin function). */
    goto done;
  }  /* if */
  loc.symbol_header->is_builtin_function = TRUE;
  loc.symbol_header->builtin_function_index = idx;
  loc.symbol_header->builtin_function_category = function_category;
  if (preload_builtin_functions &&
      builtin_restrictions_met(loc.symbol_header, /*issue_error=*/FALSE)) {
    if (type_string == NULL) {
      builtin_type = builtin_function_type_for_index(type_index);
    } else {
      builtin_type = builtin_function_type(type_string, &null_source_position);
    }  /* if */
    (void)enter_builtin_function(name, builtin_type, kind, &loc);
  }  /* if */
  /* Also see if there's a non-prefixed version that should be added.  These
     seem to only be used by GCC in C mode to give diagnostics when
     redeclaring a library function. */
  if (C_mode() && strncmp(name, "__builtin_", 10) == 0) {
    name = &builtin_name[10];
    if ((function_category == bfc_user || name[0] == '_') &&
        builtin_enabled(cond_index, condition, /*is_secondary=*/TRUE)) {
      clear_locator(&loc, &null_source_position);
      (void)find_symbol(name, (sizeof_t)strlen(name), &loc);
      loc.symbol_header->is_builtin_function = TRUE;
      loc.symbol_header->builtin_function_index = idx;
      loc.symbol_header->builtin_function_category = function_category;
      if (preload_builtin_functions &&
          builtin_restrictions_met(loc.symbol_header, /*issue_error=*/FALSE)) {
        check_assertion(builtin_type != NULL);
        (void)enter_builtin_function(name, builtin_type, kind, &loc);
      }  /* if */
    }  /* if */
  }  /* if */
done:;
}  /* preload_builtin_symbol */


static void preload_builtin_symbols(void)
/*
Loop through each builtin declaration (including user-defined builtins) and
create a symbol header entry for any builtin function that is enabled in the
current emulation mode.
*/
{
  const a_builtin_descr        *bdp;
  const a_builtin_user_descr   *budp;
  a_builtin_function_index     i;
  a_builtin_function_category  function_category;

  /* Load user builtin functions first (they override any non-user builtin
     functions with the same name). */
  for (budp = builtin_user_table, i = 0; budp->name != NULL; budp++, i++) {
    if (builtin_enabled(0, budp->cond, /*is_secondary=*/FALSE)) {
      preload_builtin_symbol(budp->name, 0, budp->cond, i, bfc_user,
                             budp->kind, 0, budp->type_string);
    }  /* if */
  }  /* for */
  for (bdp = builtin_common_table, i = 0; bdp->name != NULL; bdp++, i++) {
    if (builtin_enabled(bdp->cond_index, NULL, /*is_secondary=*/FALSE)) {
      preload_builtin_symbol(bdp->name, bdp->cond_index, NULL, i, bfc_common,
                             bdp->kind, bdp->type_index, NULL);
    }  /* if */
  }  /* for */
  if (target_is_arm_based()) {
    for (bdp = builtin_arm_table, i = 0; bdp->name != NULL; bdp++, i++) {
      if (builtin_enabled(bdp->cond_index, NULL, /*is_secondary=*/FALSE)) {
        preload_builtin_symbol(bdp->name, bdp->cond_index, NULL, i, bfc_arm,
                               bdp->kind, bdp->type_index, NULL);
      }  /* if */
    }  /* for */
    function_category = target_is_64_bits() ? bfc_arm_64 : bfc_arm_32;
    bdp = builtin_tables[function_category];
    for (i = 0; bdp->name != NULL; bdp++, i++) {
      if (builtin_enabled(bdp->cond_index, NULL, /*is_secondary=*/FALSE)) {
        preload_builtin_symbol(bdp->name, bdp->cond_index, NULL, i,
                               function_category, bdp->kind,
                               bdp->type_index, NULL);
      }  /* if */
    }  /* for */
  } else if (target_is_riscv_based()) {
    for (bdp = builtin_riscv_table, i = 0; bdp->name != NULL; bdp++, i++) {
      if (builtin_enabled(bdp->cond_index, NULL, /*is_secondary=*/FALSE)) {
        preload_builtin_symbol(bdp->name, bdp->cond_index, NULL, i, bfc_riscv,
                               bdp->kind, bdp->type_index, NULL);
      }  /* if */
    }  /* for */
    function_category = target_is_64_bits() ? bfc_riscv_64 : bfc_riscv_32;
    bdp = builtin_tables[function_category];
    for (i = 0; bdp->name != NULL; bdp++, i++) {
      if (builtin_enabled(bdp->cond_index, NULL, /*is_secondary=*/FALSE)) {
        preload_builtin_symbol(bdp->name, bdp->cond_index, NULL, i,
                               function_category, bdp->kind,
                               bdp->type_index, NULL);
        }  /* if */
    }  /* for */
  } else if (target_is_x86_based()) {
    for (bdp = builtin_x86_table, i = 0; bdp->name != NULL; bdp++, i++) {
      if (builtin_enabled(bdp->cond_index, NULL, /*is_secondary=*/FALSE)) {
        preload_builtin_symbol(bdp->name, bdp->cond_index, NULL, i, bfc_x86,
                               bdp->kind, bdp->type_index, NULL);
      }  /* if */
    }  /* for */
    function_category = target_is_64_bits() ? bfc_x86_64 : bfc_x86_32;
    bdp = builtin_tables[function_category];
    for (i = 0; bdp->name != NULL; bdp++, i++) {
      if (builtin_enabled(bdp->cond_index, NULL, /*is_secondary=*/FALSE)) {
        preload_builtin_symbol(bdp->name, bdp->cond_index, NULL, i,
                               function_category, bdp->kind,
                               bdp->type_index, NULL);
      }  /* if */
    }  /* for */
  }  /* if */
  builtin_functions_enabled = TRUE;
}  /* preload_builtin_symbols */


void load_overloadable_builtin_symbols(a_builtin_function_category  bfc)
/*
Loop through each builtin declaration for the specified function category and
declare each function that is enabled in the current emulation mode as an
overloadable builtin function.

Names starting with a '#' character, followed by a digit, indicate that the new
name is formed from the previous name by stripping off the specified number of
'_'-separated components and then appending the new suffix.

An '@' character indicates that, in addition to the name specified, another
overload is declared with a number of '_'-separated components removed from the
name as follows: for the form "@n", the trailing n components are removed, and
for the form "@nm", only the m components starting from the nth component from
the end are removed.  An optional compiler-restricting letter may appear
between the '@' and the digits: 'L' indicates that the overload is only
declared in Clang emulation mode and 'g' only in GCC emulation mode; without a
letter, the overload is valid for every compiler in the entry's condition.

To avoid the considerable expense of entering a routine for each of the
(potentially many thousands of) entries in these tables, routine entries are
created lazily.
*/
{
  const a_builtin_descr *bdp;
  Small_string<64>      name;
  a_boolean             lazy_sets = !preload_builtin_functions;
  a_boolean             already_loaded = lazy_sets &&
                                         (builtin_overload_set_categories &
                                                ((unsigned int)1 << bfc)) != 0;

  if (!already_loaded) {
    for (bdp = builtin_tables[bfc]; bdp->name != NULL; bdp++) {
      a_const_char  *ovl_info;
      get_builtin_table_entry_name(bdp, &name, &ovl_info);
      ovl_info = builtin_overload_info_for_current_mode(ovl_info);
      if (builtin_enabled(bdp->cond_index, NULL, /*is_secondary=*/FALSE)) {
        a_symbol_locator  loc;
        a_const_char      *restrictions;
        a_boolean         new_builtin;
        clear_locator(&loc, &null_source_position);
        (void)find_symbol(name.as_temp_characters(), name.length(), &loc);
        new_builtin = !loc.symbol_header->is_builtin_function;
        loc.symbol_header->is_builtin_function = TRUE;
        loc.symbol_header->is_builtin_overloadable = TRUE;
        restrictions = builtin_condition_table[bdp->cond_index].restrictions;
        if (check_restrictions_met(restrictions, /*issue_error=*/FALSE)) {
          a_type_ptr                builtin_type = NULL;
          a_builtin_function_index  table_index =
                         (a_builtin_function_index)(bdp - builtin_tables[bfc]);
          if (lazy_sets && loc.symbol_header->is_builtin_overload_set) {
            /* The name denotes an overload set: record the entry as a member
               of the set so that its routine is entered lazily when the name
               is referenced. */
            add_builtin_overload_set_member(loc.symbol_header, bfc,
                                            table_index);
            if (!builtin_needs_to_be_loaded(loc.symbol_header)) {
              builtin_type = builtin_function_type_for_index(bdp->type_index);
              (void)enter_builtin_function(name.as_temp_characters(),
                                           builtin_type, bdp->kind, &loc);
            }  /* if */
          } else if (lazy_sets && loc.symbol_header->is_builtin_deferred) {
            /* The creation of a routine for an earlier entry with this name
               was deferred, but the name denotes an overload set after all:
               mark the header accordingly and record both entries as members
               of the set. */
            loc.symbol_header->is_builtin_overload_set = TRUE;
            add_builtin_overload_set_member(
                                  loc.symbol_header,
                                  loc.symbol_header->builtin_function_category,
                                  loc.symbol_header->builtin_function_index);
            add_builtin_overload_set_member(loc.symbol_header, bfc,
                                            table_index);
            loc.symbol_header->is_builtin_deferred = FALSE;
            if (!builtin_needs_to_be_loaded(loc.symbol_header)) {
              builtin_type = builtin_function_type_for_index(bdp->type_index);
              (void)enter_builtin_function(name.as_temp_characters(),
                                           builtin_type, bdp->kind, &loc);
            }  /* if */
          } else if (new_builtin && !preload_builtin_functions) {
            /* This builtin function is the only one with this name, so don't
               create a routine entry for it now; instead, record where its
               description can be found so that the routine can be created
               lazily if the name is referenced.  If another entry for the same
               name is encountered later, the name is converted to an overload
               set at that point. */
            loc.symbol_header->builtin_function_index = table_index;
            loc.symbol_header->builtin_function_category = bfc;
            loc.symbol_header->is_builtin_deferred = TRUE;
          } else {
            /* This name is already associated with a builtin function or lazy
               loading is disabled: enter the routine for this entry
               eagerly. */
            builtin_type = builtin_function_type_for_index(bdp->type_index);
            (void)enter_builtin_function(name.as_temp_characters(),
                                         builtin_type, bdp->kind, &loc);
          }  /* if */
          if (ovl_info != NULL) {
            /* This builtin should also be added as an overload. */
            Small_string<64>  ovl_name;
            builtin_overload_base_name(&name, ovl_info, &ovl_name);
            clear_locator(&loc, &null_source_position);
            (void)find_symbol(ovl_name.as_temp_characters(),
                              ovl_name.length(), &loc);
            loc.symbol_header->is_builtin_function = TRUE;
            loc.symbol_header->is_builtin_overloadable = TRUE;
            if (lazy_sets) {
              /* Record the entry as a member of the overload set denoted by
                 the abbreviated name; the routine is entered lazily when the
                 name is referenced. */
              if (!loc.symbol_header->is_builtin_overload_set) {
                loc.symbol_header->is_builtin_overload_set = TRUE;
                if (loc.symbol_header->is_builtin_deferred) {
                  /* The creation of a routine for an entry with the
                     abbreviated name was deferred; make that entry a member
                     of the set as well. */
                  add_builtin_overload_set_member(loc.symbol_header,
                              loc.symbol_header->builtin_function_category,
                              loc.symbol_header->builtin_function_index);
                  loc.symbol_header->is_builtin_deferred = FALSE;
                }  /* if */
              }  /* if */
              add_builtin_overload_set_member(loc.symbol_header, bfc,
                                              table_index);
              if (!builtin_needs_to_be_loaded(loc.symbol_header)) {
                /* The routines of the set were already entered, so enter the
                   routine for this entry now. */
                if (builtin_type == NULL) {
                  /* The type of this entry has not been retrieved yet (e.g.,
                     because the routine for the full name was deferred). */
                  builtin_type =
                              builtin_function_type_for_index(bdp->type_index);
                }  /* if */
                (void)enter_builtin_function(ovl_name.as_temp_characters(),
                                             builtin_type, bdp->kind, &loc);
              }  /* if */
            } else {
              /* Lazy loading is disabled, so the routine for the full name
                 was entered above and its type is in builtin_type. */
              (void)enter_builtin_function(ovl_name.as_temp_characters(),
                                           builtin_type, bdp->kind, &loc);
            }  /* if */
          }  /* if */
        }  /* if */
      }  /* if */
    }  /* for */
    if (lazy_sets) {
      /* Record that the entries of this table have been scanned for overload
         set members. */
      builtin_overload_set_categories |= ((unsigned int)1 << bfc);
      mapped_overload_set_categories |= ((unsigned int)1 << bfc);
    }  /* if */
  }  /* if */
}  /* load_overloadable_builtin_symbols */


using a_builtin_func_load_set = Ptr_set<a_symbol_header*>;
                        /* The type of a set that contains the symbols
                           of all loaded builtin functions. */

STATIC_THREAD a_builtin_func_load_set
                *loaded_builtin_set;
                        /* The set of currently loaded builtin function symbols
                           for the current translation unit.  Note that for the
                           primary translation unit this variable is always
                           NULL, and the symbol header's
                           builtin_has_been_loaded field should instead be
                           consulted. */


a_boolean builtin_needs_to_be_loaded_in_secondary_translation_unit(
                                                      a_symbol_header *sym_hdr)
/*
Returns TRUE if the builtin function specified by the symbol header needs to be
loaded.  This function is only used in a secondary translation unit (as
builtin_has_been_loaded has this information for the primary translation unit).
*/
{
  check_assertion(!is_primary_translation_unit);
  return !loaded_builtin_set->contains(sym_hdr);
}  /* builtin_needs_to_be_loaded_in_secondary_translation_unit */


void mark_builtin_loaded(a_symbol_header *sym_hdr)
/*
Mark the given builtin as loaded in the current translation unit.
*/
{
  check_assertion(sym_hdr->is_builtin_function);
  if (is_primary_translation_unit) {
    sym_hdr->builtin_has_been_loaded = TRUE;
  } else if (!loaded_builtin_set->contains(sym_hdr)) {
    loaded_builtin_set->add(sym_hdr);
  }  /* if */
}  /* mark_builtin_loaded */

#endif /* BUILTIN_FUNCTIONS_ENABLED */

static a_type_ptr enter_typedef(a_const_char *name,
                                a_type_ptr   type,
                                a_boolean    is_predeclared)
/*
Create a type entry and associated symbol for a typedef of the given name with
the given underlying type.  If is_predeclared is TRUE, mark the type as being a
predeclared typedef.  Enter these in the file scope and return the type entry.
*/
{
  a_type_ptr        result = alloc_type(tk_typeref);
  a_symbol_locator  location;
  a_symbol_ptr      sym_ptr;

  result->variant.typeref.type = type;
  result->variant.typeref.predeclared = is_predeclared;
  add_to_types_list(result, DEPTH_OF_FILE_SCOPE);
  clear_locator(&location, &null_source_position);
  (void)find_symbol(name, (sizeof_t)(strlen(name)), &location);
  sym_ptr = enter_symbol(sk_type, &location, DEPTH_OF_FILE_SCOPE,
                         !is_predeclared);
  sym_ptr->variant.type.ptr = result;
  set_source_corresp(&result->source_corresp, sym_ptr);
  return result;
}  /* enter_typedef */


static a_type_ptr enter_predefined_typedef(a_const_char *name,
                                           a_type_ptr   type)
/*
Create a type entry and associated symbol for a typedef of the given name with
the given underlying type.  Mark the type as being a predeclared typedef.
Enter these in the file scope and return the type entry.
*/
{
  return enter_typedef(name, type, /*is_predeclared=*/TRUE);
}  /* enter_predefined_typedef */

#if UPC_EXTENSIONS_ALLOWED

static void enter_upc_predefined_macros(void)
/*
Enter macros as requires by the UPC specification.  Called in UPC modes only.
*/
{
  a_number_buffer num_buff;

  (void)enter_predef_macro("1", "__UPC__",
                           /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
  (void)enter_predef_macro("200310L", "__UPC_VERSION__", 
                           /*cannot_be_redefined=*/FALSE,
                           /*ref_suppresses_pch_file=*/FALSE);
  if (upc_dynamic_threads()) {
    (void)enter_predef_macro("1", "__UPC_DYNAMIC_THREADS__",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);
  } else {
    (void)enter_predef_macro("1", "__UPC_STATIC_THREADS__",
                             /*cannot_be_redefined=*/FALSE,
                             /*ref_suppresses_pch_file=*/FALSE);

    num_buff.reset_to(upc_num_threads);
    (void)enter_predef_macro(num_buff.as_temp_characters(), "THREADS",
                             /*cannot_be_redefined=*/TRUE,
                             /*ref_suppresses_pch_file=*/FALSE);
  }  /* if */
  num_buff.reset_to(max_upc_block_size);
  (void)enter_predef_macro(num_buff.as_temp_characters(), "UPC_MAX_BLOCK_SIZE",
                           /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
}  /* enter_upc_predefined_macros */

#endif /* UPC_EXTENSIONS_ALLOWED */
#if NAMED_ADDRESS_SPACES_ALLOWED

static void enter_predefined_named_address_spaces(void)
/*
Enter any predefined named address spaces.  TR 18037 ("Embedded C") requires
that such memory regions have names in the implementation namespace.  The
predefined named address spaces are configured through the initializer of the
global array named_address_spaces (see targ_def.h).
*/
{
  const a_named_address_space_descr  *nas = &named_address_spaces[1];

  for (;nas->name != NULL; ++nas) {
#if CHECKING
    a_symbol_ptr  sym = enter_named_address_space(nas->name);
    check_assertion(sym->variant.named_address_space.id == 
                                                (nas - named_address_spaces));
#else /* !CHECKING */
    (void)enter_named_address_space(nas->name);
#endif /* CHECKING */
  }  /* while */
}  /* enter_predefined_named_address_spaces */

#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if NAMED_REGISTERS_ALLOWED

static void enter_predefined_named_registers(void)
/*
Enter any predefined named address spaces.  TR 18037 ("Embedded C") requires
that such memory regions have names in the implementation namespace.  The
predefined named registers are configured through the initializer of the
global array named_register_storage_classes (see targ_def.h).
*/
{
  const a_named_register_storage_class_descr  *nr =
                                          &named_register_storage_classes[1];

  for (;nr->name != NULL; ++nr) {
#if CHECKING
    a_symbol_ptr  sym = enter_named_register(nr->name);
    check_assertion(sym->variant.named_register.id ==
                                       (nr - named_register_storage_classes));
#else /* !CHECKING */
    (void)enter_named_register(nr->name);
#endif /* CHECKING */
  }  /* while */
}  /* enter_predefined_named_registers */

#endif /* NAMED_REGISTERS_ALLOWED */

a_type_ptr get_default_va_list_type(void)
/*
Return the default type to use for va_list or __builtin_va_list.  In GNU modes,
this is the type underlying __builtin_va_list.  In other modes, this is the
type generated for va_list when the standard header <stdarg.h> or <cstdarg> is
handled internally (i.e., pass_stdarg_references_to_generated_code is TRUE)
instead of being mapped on an actual header file.
*/
{
  a_type_ptr  tp;

  if (type_underlying_va_list != NULL) {
    /* Use type_underlying_va_list if it has been configured. */
    tp = type_underlying_va_list;
  } else {
    if (target_is_arm_based() && !ms_compat) {
      /* The ARM __builtin_va_list type is struct __va_list. */
      tp = make_va_list_tag_type();
    } else if (target_is_x86_based() && target_is_64_bits() && !ms_compat) {
      /* The x86-64 __builtin_va_list type is defined as follows:
           struct __va_list_tag {
             unsigned int  gp_offset;
             unsigned int  fp_offset;
             void          *overflow_arg_area;
             void          *reg_save_area;
           };
           typedef struct __va_list_tag __builtin_va_list[1];
      */    
      tp = alloc_type((a_type_kind)tk_array);
      tp->variant.array.element_type = make_va_list_tag_type();
      tp->variant.array.variant.number_of_elements = 1;
      set_type_size(tp);
    } else {
      /* Use char* in Microsoft and GNU modes (other than for RISC-V), and
         void* otherwise. */
      if ((ms_compat || gnu_mode) && !target_is_riscv_based()) {
        tp = make_pointer_type(integer_type((an_integer_kind)ik_char));
      } else {
        tp = make_pointer_type(void_type());
      }  /* if */
    }  /* if */
  }  /* if */
  return tp;
}  /* get_default_va_list_type */

#if GNU_EXTENSIONS_ALLOWED && GCC_BUILTIN_VARARGS

static void enter_builtin_va_list_type(void)
/*
Enter a predefined type __builtin_va_list.
*/
{
  /* On 32-bit x86 GCC implementations, __builtin_va_list is a type compatible
     with char*.  On x86-64 (at least on Linux), __builtin_va_list is an array
     of one element of struct type, on ARM32/ARM64, __builtin_va_list is a
     struct type, and on RISCV32/RISCV64, it is void*. */
  builtin_va_list_type = enter_predefined_typedef("__builtin_va_list",
                                                  get_default_va_list_type());
  builtin_va_list_type->is_builtin_va_list = TRUE;
}  /* enter_builtin_va_list_type */

#endif /* GNU_EXTENSIONS_ALLOWED && GCC_BUILTIN_VARARGS */
#if GNU_EXTENSIONS_ALLOWED && INT128_EXTENSIONS_ALLOWED

static void enter_128bit_integer_typedefs(void)
/*
Enter typedefs "__int128_t" and "__uint128_t" corresponding to signed and
unsigned 128-bit integer types, respectively.
*/
{
  (void)enter_predefined_typedef(
                      "__int128_t", integer_type((an_integer_kind)ik_int128));
  (void)enter_predefined_typedef(
            "__uint128_t", integer_type((an_integer_kind)ik_unsigned_int128));
}  /* enter_128bit_integer_typedefs */

#endif /* GNU_EXTENSIONS_ALLOWED && INT128_EXTENSIONS_ALLOWED */

#if GNU_EXTENSIONS_ALLOWED && GNU_VECTOR_TYPES_ALLOWED

/*
Descriptor for a floating-point NEON vector element type.
*/
typedef struct a_float_vector_type_descr const *a_float_vector_type_descr_ptr;
typedef struct a_float_vector_type_descr {
  a_float_kind	float_kind;
			/* Float kind of the vector element. */
  a_targ_size_t	elements;
			/* Number of elements for a 64-bit wide vector of that
			   element kind. */
  a_const_char	*names[2];
			/* Array of names for 64-bit and 128-bit wide
			   vectors. */
} a_float_vector_type_descr;

/*
Table of all floating-point NEON vector element types for 64-bit platforms.
*/
static constexpr a_float_vector_type_descr
		float_neon_vector_types_64bit[] = {
  {fk_std_bfloat16, 4, {"__Bfloat16x4_t", "__Bfloat16x8_t"}},
  {fk_fp16,         4, {"__Float16x4_t",  "__Float16x8_t"}},
  {fk_float,        2, {"__Float32x2_t",  "__Float32x4_t"}},
  {fk_double,       1, {"__Float64x1_t",  "__Float64x2_t"}},
  {fk_last,         0, {NULL,             NULL}}
};

/*
Table of all floating-point NEON vector element types for 32-bit platforms.
*/
static constexpr a_float_vector_type_descr
		float_neon_vector_types_32bit[] = {
  {fk_std_bfloat16, 4, {"__simd64_bfloat16_t", "__simd128_bfloat16_t"}},
  {fk_fp16,         4, {"__simd64_float16_t",  "__simd128_float16_t"}},
  {fk_float,        2, {"__simd64_float32_t",  "__simd128_float32_t"}},
  {fk_last,         0, {NULL,                  NULL}}
};

/*
Descriptor for an integer NEON vector or polyvector element type.
*/
typedef struct an_integer_vector_type_descr const *
                                              an_integer_vector_type_descr_ptr;
typedef struct an_integer_vector_type_descr {
  an_integer_kind
		int_kind;
			/* Integer kind of the vector element. */
  a_targ_size_t	elements;
			/* Number of elements for a 64-bit wide vector of that
			   element type. */
  a_const_char	*names[2];
			/* Array of names for 64-bit and 128-bit wide
			   vectors. */
} an_integer_vector_type_descr;

/*
Table of all integer NEON vector element types for 64-bit platforms.
*/
static constexpr an_integer_vector_type_descr
		integer_neon_vector_types_64bit[] = {
  {ik_signed_char,    8, {"__Int8x8_t",   "__Int8x16_t"}},
  {ik_unsigned_char,  8, {"__Uint8x8_t",  "__Uint8x16_t"}},
  {ik_short,          4, {"__Int16x4_t",  "__Int16x8_t"}},
  {ik_unsigned_short, 4, {"__Uint16x4_t", "__Uint16x8_t"}},
  {ik_int,            2, {"__Int32x2_t",  "__Int32x4_t"}},
  {ik_unsigned_int,   2, {"__Uint32x2_t", "__Uint32x4_t"}},
  {ik_long,           1, {"__Int64x1_t",  "__Int64x2_t"}},
  {ik_unsigned_long,  1, {"__Uint64x1_t", "__Uint64x2_t"}},
  {ik_none,           0, {NULL,           NULL}}
};

/*
Table of all integer NEON vector element types for 32-bit platforms.
*/
static constexpr an_integer_vector_type_descr
		integer_neon_vector_types_32bit[] = {
  {ik_signed_char,         8, {"__simd64_int8_t",   "__simd128_int8_t"}},
  {ik_unsigned_char,       8, {"__simd64_uint8_t",  "__simd128_uint8_t"}},
  {ik_short,               4, {"__simd64_int16_t",  "__simd128_int16_t"}},
  {ik_unsigned_short,      4, {"__simd64_uint16_t", "__simd128_uint16_t"}},
  {ik_int,                 2, {"__simd64_int32_t",  "__simd128_int32_t"}},
  {ik_unsigned_int,        2, {"__simd64_uint32_t", "__simd128_uint32_t"}},
  {ik_long_long,           1, {"__simd64_int64_t",  "__simd128_int64_t"}},
  {ik_unsigned_long_long,  1, {"__simd64_uint64_t", "__simd128_uint64_t"}},
  {ik_none,                0, {NULL,                NULL}}
};

/*
Table of all integer NEON polyvector element types for 64-bit platforms.
*/
static constexpr an_integer_vector_type_descr
		integer_neon_polyvector_types_64bit[] = {
  {ik_unsigned_char,  8, {"__Poly8x8_t",  "__Poly8x16_t"}},
  {ik_unsigned_short, 4, {"__Poly16x4_t", "__Poly16x8_t"}},
  {ik_unsigned_long,  1, {"__Poly64x1_t", "__Poly64x2_t"}},
  {ik_none,           0, {NULL,           NULL}},
};

/*
Table of all integer NEON polyvector element types for 32-bit platforms.
*/
static constexpr an_integer_vector_type_descr
		integer_neon_polyvector_types_32bit[] = {
  {ik_signed_char, 8, {"__simd64_poly8_t",  "__simd128_poly8_t"}},
  {ik_short,       4, {"__simd64_poly16_t", "__simd128_poly16_t"}},
  {ik_long_long,   1, {"__simd64_poly64_t", "__simd128_poly64_t"}},
  {ik_none,        0, {NULL,                NULL}},
};

/*
Descriptor for a NEON builtin vector type.
*/
typedef struct a_builtin_vector_type_descr const *
                                               a_builtin_vector_type_descr_ptr;
typedef struct a_builtin_vector_type_descr {
  an_integer_kind
		int_kind;
			/* Integer kind of the vector element. */
  a_targ_size_t	elements;
			/* Number of vector elements. */
  a_const_char	*name;	/* Name of the vector type. */
} a_builtin_vector_type_descr;

/*
Table of all NEON builtin vector types for 32-bit platforms.
*/
static constexpr a_builtin_vector_type_descr
		integer_builtin_neon_vector_types_32bit[] = {
  {ik_long_long,          2, "__builtin_neon_ti"},
  {ik_long_long,          3, "__builtin_neon_ei"},
  {ik_long_long,          4, "__builtin_neon_oi"},
  {ik_long_long,          6, "__builtin_neon_ci"},
  {ik_long_long,          8, "__builtin_neon_xi"},
  {ik_none,               0, NULL}
};

/*
Table of all NEON builtin vector types for 64-bit platforms.
*/
static constexpr a_builtin_vector_type_descr
		integer_builtin_neon_vector_types_64bit[] = {
#if INT128_EXTENSIONS_ALLOWED
  {ik_int128, 2, "__builtin_aarch64_simd_oi"},
  {ik_int128, 3, "__builtin_aarch64_simd_ci"},
  {ik_int128, 4, "__builtin_aarch64_simd_xi"},
#endif /* INT128_EXTENSIONS_ALLOWED */
  {ik_none,   0, NULL}
};


a_const_char *get_predefined_name_for_neon_vector_type(
                                                a_type_ptr     element_type,
                                                a_targ_size_t  vector_elements,
                                                a_vector_kind  vector_kind)
/*
Get the predefined name for a NEON vector or polyvector of the specified
element type and number of vector elements.  vector_kind is the kind of the
vector (either vk_neon or vk_neon_poly).
*/
{
  a_const_char  *result = NULL;

  element_type = skip_typerefs(element_type);
  if (is_real_floating_type(element_type)) {
    a_float_kind  float_kind = element_type->variant.float_kind;
    a_float_vector_type_descr_ptr
                  float_types = target_is_64_bits() ?
                                                float_neon_vector_types_64bit :
                                                float_neon_vector_types_32bit;
    check_assertion(vector_kind == vk_neon);
    while (result == NULL && float_types->float_kind != fk_last) {
      if (float_types->float_kind == float_kind) {
        result = float_types->elements == vector_elements ?
                                 float_types->names[0] : float_types->names[1];
      }  /* if */
      ++float_types;
    }  /* while */
  } else if (is_integral_type(element_type)) {
    a_boolean  is_signed = is_signed_integral_type(element_type);
    an_integer_vector_type_descr_ptr
               integer_types;
    if (vector_kind == vk_neon_poly) {
      integer_types = target_is_64_bits() ?
                                          integer_neon_polyvector_types_64bit :
                                          integer_neon_polyvector_types_32bit;
    } else {
      check_assertion(vector_kind == vk_neon);
      integer_types = target_is_64_bits() ? integer_neon_vector_types_64bit :
                                            integer_neon_vector_types_32bit;
    }  /* if */
    while (result == NULL && integer_types->int_kind != ik_last) {
      /* The name is determined by the type's size and signedness, not by the
         exact integer type kind. */
      if (int_kind_is_signed[integer_types->int_kind] == is_signed &&
          element_type->size == 8 / integer_types->elements) {
        result = integer_types->elements == vector_elements ?
                             integer_types->names[0] : integer_types->names[1];
      }  /* if */
      ++integer_types;
    }  /* while */
  } else if (type_is(element_type, tk_mfp8)) {
    switch (vector_elements) {
      case 8:  result = "__Mfloat8x8_t";  break;
      case 16: result = "__Mfloat8x16_t"; break;
      default: unexpected_condition();
    }  /* switch */
  } else {
    unexpected_condition();
  }  /* if */
  return result;
}  /* get_predefined_name_for_neon_vector_type */


static void enter_neon_vector_types(
                              a_type_ptr                       element_type,
                              a_targ_size_t                    vector_elements,
                              const a_const_char_ptr_array<2>  &names,
                              a_vector_kind                    vector_kind)
/*
Enter predefined typedefs for 64-bit and 128-bit wide NEON vectors of the
specified element type.  vector_elements specifies the number of elements for a
64-bit wide vector.  The typedef name is supplied in the array names for 64-bit
and 128-bit wide vectors.  vector_kind is the kind of the vector (either
vk_neon or vk_neon_poly).
*/
{
  check_assertion(vector_kind == vk_neon || vector_kind == vk_neon_poly);
  (void)enter_predefined_typedef(names[0],
                                 make_vector_type(element_type,
                                                  vector_elements,
                                                  vector_kind));
  (void)enter_predefined_typedef(names[1],
                                 make_vector_type(element_type,
                                                  2*vector_elements,
                                                  vector_kind));
}  /* enter_neon_vector_types */


static void enter_integer_neon_vector_types(
                                 an_integer_vector_type_descr_ptr  types,
                                 a_vector_kind                     vector_kind)
/*
Enter predefined typedefs for 64-bit and 128-bit NEON vectors or polyvectors of
integer type specified in the table types (terminated by an entry with
ik_none).  vector_kind is the kind of the vector (either vk_neon or
vk_neon_poly).
*/
{
  while (types->int_kind != ik_none) {
    enter_neon_vector_types(integer_type(types->int_kind),
                            types->elements, types->names, vector_kind);
    ++types;
  }  /* while */
}  /* enter_integer_neon_vector_types */


static void enter_float_neon_vector_types(a_float_vector_type_descr_ptr  types)
/*
Enter predefined typedefs for 64-bit and 128-bit NEON vectors of float type
specified in the table types (terminated by an entry with fk_last).
*/
{
  while (types->float_kind != fk_last) {
    enter_neon_vector_types(float_type(types->float_kind),
                            types->elements, types->names, vk_neon);
    ++types;
  }  /* while */
}  /* enter_float_neon_vector_types */


a_const_char *get_predefined_name_for_builtin_neon_vector_type(
                                                a_type_ptr     element_type,
                                                a_targ_size_t  vector_elements)
/*
Get the predefined name for a NEON builtin vector of the specified element type
and number of vector elements.
*/
{
  a_const_char  *result = NULL;
  a_builtin_vector_type_descr_ptr
                builtin_types = target_is_64_bits() ?
                                      integer_builtin_neon_vector_types_64bit :
                                      integer_builtin_neon_vector_types_32bit;

  element_type = skip_typerefs(element_type);
  check_assertion(is_integral_type(element_type));
  while (result == NULL && builtin_types->int_kind != ik_none) {
    if (builtin_types->int_kind == element_type->variant.integer.int_kind &&
        builtin_types->elements == vector_elements) {
      result = builtin_types->name;
    }  /* if */
    ++builtin_types;
  }  /* while */
  return result;
}  /* get_predefined_name_for_builtin_neon_vector_type */


static void enter_builtin_integer_neon_vector_types(
                                        a_builtin_vector_type_descr_ptr  types)
/*
Enter predefined typedefs for NEON builtin vectors specified in the table types
(terminated by an entry with ik_none).
*/
{
  while (types->int_kind != ik_none) {
    a_type_ptr  tp = make_vector_type(integer_type(types->int_kind),
                                      types->elements, vk_neon_builtin);
    (void)enter_predefined_typedef(types->name, tp);
    ++types;
  }  /* while */
}  /* enter_builtin_integer_neon_vector_types */


static void enter_scalable_vector_types(
                a_type_ptr                       element_type,
                const a_const_char_ptr_array<4>  &names,
                a_boolean                        enter_single_tuple_element,
                a_boolean                        enter_multiple_tuple_elements,
                a_boolean                        strip_clang_prefix)
/*
Enter predefined typedefs for scalable vector types of the specified element
type.  If enter_single_tuple_element is TRUE, a typedef for tuple size 1 is
entered.  If enter_multiple_tuple_elements is TRUE, typedefs for tuple sizes
between 2 and 4 are entered.  The typedef name is supplied for each tuple size
in the array names.  If strip_clang_prefix is TRUE, a "__clang_" prefix is
stripped from the typedef name.
*/
{
  if (enter_single_tuple_element && names[0] != NULL) {
    (void)enter_predefined_typedef(names[0],
                                   make_scalable_vector_type(element_type, 1));
  }  /* if */
  if (enter_multiple_tuple_elements) {
    for (uint8_t  tuple_elements = 2; tuple_elements <= 4; ++tuple_elements) {
      a_const_char  *name = names[tuple_elements - 1];
      if (name != NULL) {
        a_type_ptr  vector_type = make_scalable_vector_type(element_type,
                                                            tuple_elements);
        if (strip_clang_prefix) {
          const char    clang_prefix[] = "__clang_";
          const size_t  prefix_len = sizeof(clang_prefix) - 1;
          check_assertion(strncmp(name, clang_prefix, prefix_len) == 0);
          name = name + prefix_len;
        }  /* if */
        (void)enter_predefined_typedef(name, vector_type);
      }  /* if */
    }  /* for */
  }  /* if */
}  /* enter_scalable_vector_types */


static void enter_all_scalable_vector_types(
                                      a_boolean  enter_single_tuple_element,
                                      a_boolean  enter_multiple_tuple_elements,
                                      a_boolean  strip_name_prefix)
/*
Enter predefined typedefs for all scalable vector types.  See
enter_scalable_vector_types for a description of the flags.
*/
{
  auto enter_types = [=] (a_type_ptr                       element_type,
                          const a_const_char_ptr_array<4>  &names) {
                            enter_scalable_vector_types(
                                                 element_type, names,
                                                 enter_single_tuple_element,
                                                 enter_multiple_tuple_elements,
                                                 strip_name_prefix);
                          };

  enter_types(integer_type(ik_signed_char),
              {"__SVInt8_t",         "__clang_svint8x2_t",
               "__clang_svint8x3_t", "__clang_svint8x4_t"});
  enter_types(integer_type(ik_unsigned_char),
              {"__SVUint8_t",         "__clang_svuint8x2_t",
               "__clang_svuint8x3_t", "__clang_svuint8x4_t"});
  enter_types(integer_type(ik_short),
              {"__SVInt16_t",         "__clang_svint16x2_t",
               "__clang_svint16x3_t", "__clang_svint16x4_t"});
  enter_types(integer_type(ik_unsigned_short),
              {"__SVUint16_t",         "__clang_svuint16x2_t",
               "__clang_svuint16x3_t", "__clang_svuint16x4_t"});
  enter_types(integer_type(ik_int),
              {"__SVInt32_t",         "__clang_svint32x2_t",
               "__clang_svint32x3_t", "__clang_svint32x4_t"});
  enter_types(integer_type(ik_unsigned_int),
              {"__SVUint32_t",         "__clang_svuint32x2_t",
               "__clang_svuint32x3_t", "__clang_svuint32x4_t"});
  enter_types(integer_type(ik_long),
              {"__SVInt64_t",         "__clang_svint64x2_t",
               "__clang_svint64x3_t", "__clang_svint64x4_t"});
  enter_types(integer_type(ik_unsigned_long),
              {"__SVUint64_t",         "__clang_svuint64x2_t",
               "__clang_svuint64x3_t", "__clang_svuint64x4_t"});
  enter_types(float_type(fk_fp16),
              {"__SVFloat16_t",         "__clang_svfloat16x2_t",
               "__clang_svfloat16x3_t", "__clang_svfloat16x4_t"});
  enter_types(float_type(fk_std_bfloat16),
              {clang_version_is(<180000) ? "__SVBFloat16_t" : "__SVBfloat16_t",
               "__clang_svbfloat16x2_t",
               "__clang_svbfloat16x3_t", "__clang_svbfloat16x4_t"});
  enter_types(float_type(fk_float),
              {"__SVFloat32_t",         "__clang_svfloat32x2_t",
               "__clang_svfloat32x3_t", "__clang_svfloat32x4_t"});
  enter_types(float_type(fk_double),
              {"__SVFloat64_t",         "__clang_svfloat64x2_t",
               "__clang_svfloat64x3_t", "__clang_svfloat64x4_t"});

  if (enter_single_tuple_element) {
    (void)enter_predefined_typedef("__SVBool_t",
                                   make_scalable_vector_type(bool_type(),
                                                             1));
  }  /* if */
  if (clang_version_is(>=170000) || gnu_version_is(>=150000)) {
    enter_types(bool_type(),
                {NULL, "__clang_svboolx2_t",
                 NULL, "__clang_svboolx4_t"});
  }  /* if */
  if (clang_version_is(>=200000) || gnu_version_is(>=150000)) {
    enter_types(modal_8bit_floating_point_type(),
                {"__SVMfloat8_t",         "__clang_svmfloat8x2_t",
                 "__clang_svmfloat8x3_t", "__clang_svmfloat8x4_t"});
  }  /* if */
}  /* enter_scalable_vector_types */


static void enter_riscv_vector_types_for_element_type(
                                               a_const_char  *name_prefix,
                                               a_type_ptr    element_type,
                                               a_boolean     enter_tuple_types)
/*
Enter predefined typedefs for all RISC-V vector types for the given element
type.  If enter_tuple_types is TRUE, additionally create vector types for
multiple tuple elements.
*/
{
  uint8_t  max_tuple_elements = enter_tuple_types ? 8 : 1;

  for (int8_t multiplier = 1; multiplier <= 8; ++multiplier) {
    for (uint8_t tuple_elements = 1;
         tuple_elements <= max_tuple_elements;
         ++tuple_elements) {
      a_type_ptr  vector_type;

      if (multiplier*tuple_elements <= 8) {
        vector_type = make_riscv_vector_type(element_type, multiplier,
                                             tuple_elements);
        (void)enter_predefined_typedef(
              get_name_for_riscv_vector_type(name_prefix,
                                             vector_type).as_temp_characters(),
              vector_type);
      }  /* if */
      if (multiplier > 1 && ((uint8_t)multiplier*element_type->size <= 8)) {
        vector_type = make_riscv_vector_type(element_type, (int8_t)-multiplier,
                                             tuple_elements);
        (void)enter_predefined_typedef(
              get_name_for_riscv_vector_type(name_prefix,
                                             vector_type).as_temp_characters(),
              vector_type);
      }  /* if */
    }  /* for */
  }  /* for */
}  /* enter_riscv_vector_types_for_element_type */


static void enter_all_riscv_vector_types(
                                     a_const_char  *name_prefix,
                                     a_boolean     enter_bfloat16_vector_types,
                                     a_boolean     enter_ofp8_vector_types,
                                     a_boolean     enter_tuple_types)
/*
Enter predefined typedefs for all RISC-V vector types.  If
enter_bfloat16_vector_types is TRUE, include vector types for the bfloat16
floating-point type.  If enter_ofp8_vector_types is TRUE, include vector
types for the OFP8 E4M3 and E5M2 8-bit floating-point types.  If
enter_tuple_types is TRUE, additionally create vector types for multiple
tuple elements.
*/
{
  a_type_ptr          element_type;
  const a_float_kind  float_kinds[] = {fk_float16, fk_float, fk_double,
                                       fk_last};

  for (unsigned bits = 1; bits <= 64; bits *= 2) {
    a_type_ptr  vector_type = make_riscv_vector_type(bool_type(), (int8_t)bits,
                                                     1);
    (void)enter_predefined_typedef(
              get_name_for_riscv_vector_type(name_prefix,
                                             vector_type).as_temp_characters(),
              vector_type);
  }  /* for */
  for (unsigned bits = 8; bits <= 64; bits *= 2) {
    unsigned         size_in_bytes = bits / 8;
    an_integer_kind  int_kind;

    int_kind = int_kind_for_size_and_alignment(size_in_bytes,
                                               (a_targ_alignment)size_in_bytes,
                                               /*is_signed=*/TRUE);
    check_assertion(int_kind < ik_last);
    element_type = integer_type(int_kind);
    enter_riscv_vector_types_for_element_type(name_prefix, element_type,
                                              enter_tuple_types);
    int_kind = int_kind_for_size_and_alignment(size_in_bytes,
                                               (a_targ_alignment)size_in_bytes,
                                               /*is_signed=*/FALSE);
    check_assertion(int_kind < ik_last);
    element_type = integer_type(int_kind);
    enter_riscv_vector_types_for_element_type(name_prefix, element_type,
                                              enter_tuple_types);
  }  /* for */
  for (const a_float_kind *kind = float_kinds; *kind != fk_last; ++kind) {
    element_type = float_type(*kind);
    enter_riscv_vector_types_for_element_type(name_prefix, float_type(*kind),
                                              enter_tuple_types);
  }  /* for */
  if (enter_bfloat16_vector_types) {
    enter_riscv_vector_types_for_element_type(name_prefix,
                                              float_type(fk_std_bfloat16),
                                              enter_tuple_types);
  }  /* if */
  if (enter_ofp8_vector_types) {
    enter_riscv_vector_types_for_element_type(name_prefix, float8e4m3_type(),
                                              /*enter_tuple_types=*/FALSE);
    enter_riscv_vector_types_for_element_type(name_prefix, float8e5m2_type(),
                                              /*enter_tuple_types=*/FALSE);
  }  /* if */
}  /* enter_all_riscv_vector_types */


using a_build_array_type_name_fn = a_boolean(char *, sizeof_t, a_const_char *,
                                             unsigned);
			/* Type of a function to build the name of an array
			   type. */


static void enter_struct_array_types(
                            a_const_char                *base_name,
                            a_type_ptr                  base_type,
                            a_build_array_type_name_fn  *build_array_type_name,
                            a_source_position           *decl_pos)
/*
Create a number of struct types with a single field "val" of array type of the
specified base type.  The type name is generated by build_array_type_name from
from the specified base_name and the array bound.  This is done for array bound
from 2 to 4.  decl_pos is the declaration position to be used for the
declarations.
*/
{
  char  name_buf[16];

  for (unsigned array_elements = 2; array_elements <= 4; ++array_elements) {
    if (build_array_type_name(name_buf, sizeof(name_buf), base_name,
                              array_elements)) {
      a_type_ptr  struct_type, typedef_type;
      a_type_ptr  field_type = alloc_type(tk_array);

      field_type->variant.array.element_type = base_type;
      field_type->variant.array.variant.number_of_elements = array_elements;
      set_type_size(field_type);
      struct_type = make_single_field_struct_type(name_buf, field_type, "val",
                                                  decl_pos);
      typedef_type = enter_typedef(name_buf, struct_type,
                                   /*is_predeclared=*/FALSE);
      typedef_type->source_corresp.decl_position = *decl_pos;
    }  /* if */
  }  /* for */
}  /* enter_struct_array_types */


static a_type_ptr enter_unscoped_enum(a_const_char       *name,
                                      a_source_position  *decl_pos)
/*
Create an unscoped enumeration type and enter it with the specified name in
file scope.  Also enter a typedef to that enumeration type with the same name.
Return a pointer to the enumeration type.  decl_pos is the declaration position
to be used for the declarations.
*/
{
  a_symbol_ptr      sym;
  a_type_ptr        type, typedef_type;
  a_symbol_locator  loc;

  type = alloc_type(tk_enum);
  type->variant.integer.enum_type = TRUE;
  if (!C_mode()) {
    type->source_corresp.name_linkage = nlk_cplusplus_external;
  }  /* if */
  clear_locator(&loc, &null_source_position);
  (void)find_symbol(name, (sizeof_t)strlen(name), &loc);
  sym = alloc_symbol(sk_enum_tag, loc.symbol_header, decl_pos);
  sym->variant.enumeration.type = type;
  sym->decl_position = *decl_pos;
  reenter_symbol(sym, DEPTH_OF_FILE_SCOPE, /*suppress_error=*/FALSE);
  set_source_corresp(&(type->source_corresp), sym);
  set_namespace_membership(sym, &(type->source_corresp), NULL);
  /* The referenced flag may have been reset by set_source_corresp. */
  type->source_corresp.referenced = sym->referenced;
  add_to_types_list(type, DEPTH_OF_FILE_SCOPE);
  typedef_type = enter_typedef(name, type, /*is_predeclared=*/FALSE);
  typedef_type->source_corresp.decl_position = *decl_pos;
  return type;
}  /* enter_unscoped_enum */


/*
Descriptor for enumerator constants.
*/
typedef struct an_enumerator_descr {
  const char	*name;
			/* Name of the enumerator. */
  int		value;
			/* Integer value of the enumerator. */
} an_enumerator_descr;

static void enter_unscoped_enumerators(a_type_ptr                 enum_type,
                                       const an_enumerator_descr  *enumerators,
                                       a_source_position          *decl_pos)
/*
Enter a list of enumerators (terminated by an entry with a NULL name) for the
specified enum type.  decl_pos is the declaration position to be used for the
declarations.
*/
{
  a_type_ptr      enum_con_type;
  a_symbol_ptr    enum_con_sym;
  a_constant_ptr  constant = local_constant();
  a_constant_ptr  constant_list = NULL;
  a_constant_ptr  end_of_constant_list = NULL;
  a_scope_ptr     parent_scope = scope_stack[decl_scope_level].il_scope;

  if (C_mode()) {
    enum_con_type = alloc_type(tk_integer);
    enum_con_type->variant.integer.int_kind = ik_int;
    enum_con_type->variant.integer.enum_type = FALSE;
    enum_con_type->variant.integer.enum_info.affiliated_type = enum_type;
    set_type_size(enum_con_type);
  } else {
    enum_con_type = enum_type;
  }  /* if */
  for (const an_enumerator_descr *iter = enumerators;
       iter->name != NULL;
       ++iter) {
    a_symbol_locator  loc;
    a_constant_ptr    enum_con;

    clear_locator(&loc, &null_source_position);
    (void)find_symbol(iter->name, (sizeof_t)strlen(iter->name), &loc);
    enum_con_sym = alloc_symbol(sk_constant, loc.symbol_header, decl_pos);
    reenter_symbol(enum_con_sym, DEPTH_OF_FILE_SCOPE,
                   /*suppress_error=*/FALSE);
    set_integer_constant(constant, (a_host_large_integer)iter->value, ik_int);
    enum_con = alloc_unshared_constant(constant);
    enum_con->is_named_constant_definition = TRUE;
    set_source_corresp(&(enum_con->source_corresp), enum_con_sym);
    enum_con->source_corresp.parent_scope = parent_scope;
    enum_con->source_corresp.name_linkage =
                                        enum_type->source_corresp.name_linkage;
    enum_con->type = enum_con_type;
    enum_con_sym->variant.constant = enum_con;
    set_namespace_membership(enum_con_sym, &enum_con->source_corresp, NULL);
    if (constant_list == NULL) {
      constant_list = enum_con;
    } else {
      end_of_constant_list->next = enum_con;
    }  /* if */
    end_of_constant_list = enum_con;
  }  /* for */
  enum_type->variant.integer.enum_info.constant_list = constant_list;
  integer_type_supp(enum_type)->enumerator_list_seen = TRUE;
  integer_type_supp(enum_type)->enumerator_list_complete = TRUE;
  integer_type_supp(enum_type)->underlying_type_should_use_unsigned = TRUE;
  release_local_constant(&constant);
}  /* enter_unscoped_enumerators */


static void build_arm_32_neon_vector_type_name(char           *buf,
                                               size_t         buf_size,
                                               a_const_char   *elem_name,
                                               a_targ_size_t  vector_elements)
/*
Build the name for the user-visible vector type from the builtin type name
passed in elem_name with the specified number of vector elements.  *buf points
to the output buffer of size buf_size.
*/
{
  a_const_char  *base_name;
  size_t        len;

  check_assertion(elem_name[0] == '_' && elem_name[1] == '_');
  /* Skip the leading "__" and look for the next '_'. */
  base_name = strchr(elem_name + 2, '_');
  check_assertion(base_name != NULL);
  ++base_name;
  len = strlen(base_name);
  check_assertion(buf_size >= len + 3);
  (void)strcpy(buf, base_name);
  /* Overwrite the trailing "_t" with an indicator for the number of vector
     elements and array elements. */
  buf += len - 2;
  buf_size -= len - 2;
  (void)snprintf(buf, buf_size, "x%u_t", (unsigned)vector_elements);
}  /* build_arm_32_neon_vector_type_name */


static a_boolean build_arm_32_neon_array_type_name(char         *buf,
                                                   size_t       buf_size,
                                                   a_const_char *elem_name,
                                                   unsigned     array_elements)
/*
Build the name for an array type from the builtin type name passed in elem_name
with the specified number of array elements.  *buf points to the output buffer
of size buf_size.  Return TRUE if the number of array elements is supported.
*/
{
  a_boolean  result = TRUE;
  if (array_elements != 3) {
    size_t  len = strlen(elem_name);
    check_assertion(buf_size >= len + 3);
    (void)strcpy(buf, elem_name);
    /* Overwrite the trailing "_t" with an indicator for the number of array
       elements. */
    buf += len - 2;
    buf_size -= len - 2;
    (void)snprintf(buf, buf_size, "x%u_t", array_elements);
  } else {
    /* ARM 32-bit doesn't define array types with 3 elements. */
    result = FALSE;
  }  /* if */
  return result;
}  /* build_arm_32_neon_array_type_name */


void enter_arm_32_mve_predeclared_types(a_source_position *decl_pos)
/*
Enter predeclared types for the ARM 32-bit "arm_mve.h" header file.  decl_pos
is the declaration position to be used for the declarations.
*/
{
  char          name_buf[16];
  a_type_ptr    tp;
  an_integer_vector_type_descr_ptr
                int_types;
  a_float_vector_type_descr_ptr
                float_types;

  tp = enter_typedef("mve_pred16_t", integer_type(ik_unsigned_short), FALSE);
  tp->source_corresp.decl_position = *decl_pos;
  for (int_types = integer_neon_vector_types_32bit;
       int_types->int_kind != ik_none;
       ++int_types) {
    a_type_ptr  base_type = integer_type(int_types->int_kind);
    for (unsigned i = 0; i < 2; ++i) {
      a_type_ptr  vector_type = make_vector_type(base_type,
                                                 (i + 1)*int_types->elements,
                                                 vk_neon);
      build_arm_32_neon_vector_type_name(name_buf, sizeof(name_buf),
                                         int_types->names[i],
                                         (i + 1)*int_types->elements);
      (void)enter_predefined_typedef(name_buf, vector_type);
      enter_struct_array_types(name_buf, vector_type,
                               build_arm_32_neon_array_type_name, decl_pos);
    }  /* for */
  }  /* for */
  for (float_types = float_neon_vector_types_32bit;
       float_types->float_kind != fk_last;
       ++float_types) {
    /* There are no corresponding array types for __bf16. */
    if (float_types->float_kind == fk_std_bfloat16) continue;
    a_type_ptr  base_type = float_type(float_types->float_kind);
    for (unsigned i = 0; i < 2; ++i) {
      a_type_ptr  vector_type = make_vector_type(base_type,
                                                 (i + 1)*float_types->elements,
                                                 vk_neon);
      build_arm_32_neon_vector_type_name(name_buf, sizeof(name_buf),
                                         float_types->names[i],
                                         (i + 1)*float_types->elements);
      (void)enter_predefined_typedef(name_buf, vector_type);
      enter_struct_array_types(name_buf, vector_type,
                               build_arm_32_neon_array_type_name, decl_pos);
    }  /* for */
  }  /* for */
}  /* enter_arm_32_mve_predeclared_types */


void enter_arm_64_acle_predeclared_types(a_source_position  *decl_pos)
/*
Enter predeclared types for the ARM 64-bit "arm_acle.h" header file.  decl_pos
is the declaration position to be used for the declarations.
*/
{
  a_const_char  *name = "__arm_data512_t";
  a_type_ptr    typedef_type, struct_type;
  a_type_ptr    field_type = alloc_type(tk_array);

  field_type->variant.array.element_type = integer_type(ik_unsigned_long);
  field_type->variant.array.variant.number_of_elements = 8;
  set_type_size(field_type);
  struct_type = make_single_field_struct_type(name, field_type, "val",
                                              decl_pos);
  typedef_type = enter_typedef(name, struct_type, /*is_predeclared=*/FALSE);
  typedef_type->source_corresp.decl_position = *decl_pos;
}  /* enter_arm_64_acle_predeclared_types */


static a_boolean build_arm_64_neon_array_type_name(char         *buf,
                                                   size_t       buf_size,
                                                   a_const_char *elem_name,
                                                   unsigned     array_elements)
/*
Build the name for an array type from the builtin type name passed in elem_name
with the specified number of array elements.  *buf points to the output buffer
of size buf_size.  Return TRUE if the number of array elements is supported.
*/
{
  a_const_char  *base_name;
  size_t        len;

  check_assertion(elem_name[0] == '_' && elem_name[1] == '_');
  /* Skip the leading "__" and convert the first character to lowercase. */
  base_name = elem_name + 2;
  len = strlen(base_name);
  check_assertion(buf_size >= len + 3);
  (void)strcpy(buf, base_name);
  buf[0] = (char)tolower(buf[0]);
  /* Overwrite trailing "_t" with indication for the number of array
     elements. */
  buf += len - 2;
  buf_size -= len - 2;
  (void)snprintf(buf, buf_size, "x%u_t", array_elements);
  return TRUE;
}  /* build_arm_64_neon_array_type_name */


void enter_arm_64_neon_predeclared_types(a_source_position  *decl_pos)
/*
Enter predeclared types for the ARM 64-bit "arm_neon.h" header file.  decl_pos
is the declaration position to be used for the declarations.
*/
{
  const an_integer_vector_type_descr  *int_types;
  const a_float_vector_type_descr     *float_types;

  for (int_types = integer_neon_vector_types_64bit;
       int_types->int_kind != ik_none;
       ++int_types) {
    a_type_ptr  base_type = integer_type(int_types->int_kind);
    for (unsigned i = 0; i < 2; ++i) {
      a_type_ptr  vector_type = make_vector_type(base_type,
                                                 (i + 1)*int_types->elements,
                                                 vk_neon);
      enter_struct_array_types(int_types->names[i], vector_type,
                               build_arm_64_neon_array_type_name, decl_pos);
    }  /* for */
  }  /* for */
  for (int_types = integer_neon_polyvector_types_64bit;
       int_types->int_kind != ik_none;
       ++int_types) {
    a_type_ptr  base_type = integer_type(int_types->int_kind);
    for (unsigned i = 0; i < 2; ++i) {
      a_type_ptr  vector_type = make_vector_type(base_type,
                                                 (i + 1)*int_types->elements,
                                                 vk_neon_poly);
      enter_struct_array_types(int_types->names[i], vector_type,
                               build_arm_64_neon_array_type_name, decl_pos);
    }  /* for */
  }  /* for */
  for (float_types = float_neon_vector_types_64bit;
       float_types->float_kind != fk_last;
       ++float_types) {
    a_type_ptr  base_type = float_type(float_types->float_kind);
    for (unsigned i = 0; i < 2; ++i) {
      a_type_ptr  vector_type = make_vector_type(base_type,
                                                 (i + 1)*float_types->elements,
                                                 vk_neon);
      enter_struct_array_types(float_types->names[i], vector_type,
                               build_arm_64_neon_array_type_name, decl_pos);
    }  /* for */
  }  /* for */
  enter_struct_array_types("__Mfloat8x8_t",
                           make_vector_type(modal_8bit_floating_point_type(),
                                            8, vk_neon),
                           build_arm_64_neon_array_type_name, decl_pos);
  enter_struct_array_types("__Mfloat8x16_t",
                           make_vector_type(modal_8bit_floating_point_type(),
                                            16, vk_neon),
                           build_arm_64_neon_array_type_name, decl_pos);
}  /* enter_arm_64_neon_predeclared_types */


void enter_arm_64_sve_predeclared_types(a_source_position *decl_pos)
/*
Enter predeclared types for the ARM 64-bit "arm_sve.h" header file.  decl_pos
is the declaration position to be used for the declarations.
*/
{
  a_type_ptr  svpattern_type;
  static constexpr an_enumerator_descr
              svpattern_enumerators[] = {
                {"SV_POW2",   0},
                {"SV_VL1",    1},
                {"SV_VL2",    2},
                {"SV_VL3",    3},
                {"SV_VL4",    4},
                {"SV_VL5",    5},
                {"SV_VL6",    6},
                {"SV_VL7",    7},
                {"SV_VL8",    8},
                {"SV_VL16",   9},
                {"SV_VL32",  10},
                {"SV_VL64",  11},
                {"SV_VL128", 12},
                {"SV_VL256", 13},
                {"SV_MUL4",  29},
                {"SV_MUL3",  30},
                {"SV_ALL",   31},
                {NULL,       -1}
  };
  a_type_ptr  svprfop_type;
  static constexpr an_enumerator_descr
              svprfop_enumerators[] = {
                {"SV_PLDL1KEEP",  0},
                {"SV_PLDL1STRM",  1},
                {"SV_PLDL2KEEP",  2},
                {"SV_PLDL2STRM",  3},
                {"SV_PLDL3KEEP",  4},
                {"SV_PLDL3STRM",  5},
                {"SV_PSTL1KEEP",  8},
                {"SV_PSTL1STRM",  9},
                {"SV_PSTL2KEEP", 10},
                {"SV_PSTL2STRM", 11},
                {"SV_PSTL3KEEP", 12},
                {"SV_PSTL3STRM", 13},
                {NULL,           -1}
  };

  svpattern_type = enter_unscoped_enum("svpattern", decl_pos);
  enter_unscoped_enumerators(svpattern_type, svpattern_enumerators, decl_pos);
  svprfop_type = enter_unscoped_enum("svprfop", decl_pos);
  enter_unscoped_enumerators(svprfop_type, svprfop_enumerators, decl_pos);
  enter_all_scalable_vector_types(/*enter_single_tuple_element=*/FALSE,
                                  /*enter_multiple_tuple_elements=*/TRUE,
                                  /*strip_name_prefix=*/TRUE);
}  /* enter_arm_64_sve_predeclared_types */


void enter_riscv_vector_predeclared_types(a_source_position *decl_pos)
/*
Enter predeclared types for the RISC-V "riscv_vector.h" header file.  decl_pos
is the declaration position to be used for the declarations.
*/
{
  a_boolean   tuple_types_supported = gnu_version_is(>=140000);
  a_boolean   bfloat16_supported = gnu_version_is(>=150000);
  a_type_ptr  riscv_frm_type;
  static constexpr an_enumerator_descr
              riscv_frm_enumerators[] = {
                {"__RISCV_FRM_RNE",  0},
                {"__RISCV_FRM_RTZ",  1},
                {"__RISCV_FRM_RDN",  2},
                {"__RISCV_FRM_RUP",  3},
                {"__RISCV_FRM_RMM",  4},
                {NULL,              -1}
  };
  a_type_ptr  riscv_vxrm_type;
  static constexpr an_enumerator_descr
              riscv_vxrm_enumerators[] = {
                {"__RISCV_VXRM_RNU",  0},
                {"__RISCV_VXRM_RNE",  1},
                {"__RISCV_VXRM_RDN",  2},
                {"__RISCV_VXRM_ROD",  3},
                {NULL,               -1}
  };
  riscv_frm_type = enter_unscoped_enum("__RISCV_FRM", decl_pos);
  enter_unscoped_enumerators(riscv_frm_type, riscv_frm_enumerators, decl_pos);
  riscv_vxrm_type = enter_unscoped_enum("__RISCV_VXRM", decl_pos);
  enter_unscoped_enumerators(riscv_vxrm_type, riscv_vxrm_enumerators,
                             decl_pos);
  enter_all_riscv_vector_types("v", bfloat16_supported,
                               /*enter_ofp8_vector_types=*/FALSE,
                               tuple_types_supported);
}  /* enter_riscv_vector_predeclared_types */

#endif /* GNU_EXTENSIONS_ALLOWED && GNU_VECTOR_TYPES_ALLOWED */

void enter_system_specific_predeclared_symbols(void)
/*
Enter predeclared symbols as required by the implementation.
*/
{
#if 0
  /* The following is presented as a sort of template for entering
     predeclared functions.  The example causes the symbol to be added to
     the scope of predeclared namespace "std" -- remove the push_scope and
     pop_scope calls to enter the symbols in the file scope. */
  if (!C_mode()) {
    a_symbol_locator  loc;
    a_type_ptr        return_type, param1_type, param2_type, param3_type;
    a_type_ptr        rout_type;
  
    if (namespaces_enabled) {
      /* This routine should not be called before
         make_symbol_for_namespace_std is called to predeclare namespace
         "std" (see fe_init.c). */
      check_assertion(symbol_for_namespace_std != NULL);
      /* First push the scope for namespace std.  (This is done on the
         assumption that the current scope is the file scope.) */
      check_assertion(depth_scope_stack == DEPTH_OF_FILE_SCOPE);
      (void)push_namespace_scope((a_scope_kind)sck_namespace_extension,
                                 symbol_for_namespace_std->
                                               variant.namespace_info.ptr);
    }  /* if */
    /* For each function to be entered, clear the locator, call find_symbol
       to create the symbol header, create a routine type, and then call
       make_predeclared_function_symbol to do the rest of the work.
       The following creates a routine entry and a symbol for std::memcpy,
       adds the routine to the routines list of namespace std, and adds the
       symbol to the symbol table. */
    /* Note: even though std::memcpy is added to the symbol table, it cannot
       be called directly in user code with that name until namespace std is
       explicitly declared, because the latter was predeclared without
       actually being added to the symbol table
       (see enter_symbol_for_namespace_std). */
  
    /* Create a symbol header with the required name. */
    clear_locator(&loc, &null_source_position);
    (void)find_symbol("memcpy", (sizeof_t)6, &loc);
    /* Create the routine type. */
    return_type = void_type();
    param1_type = param2_type =
                    make_pointer_type(integer_type((an_integer_kind)ik_char));
    param3_type = integer_type((an_integer_kind)ik_int);
    rout_type = make_routine_type(return_type, param1_type, param2_type,
                                  param3_type, (a_type_ptr)NULL);
    /* Create the routine entry and the symbol. */
    (void)make_predeclared_function_symbol(&loc, rout_type);
    /* Repeat these steps for additional predeclared functions. */
    if (namespaces_enabled) {
      /* After all the functions have been entered, pop the scope for
         namespace std. */
      (void)pop_scope();
    }  /* if */
  
    /* An example of entering a predefined type: */
    enter_predefined_typedef(
                  "__long_long", integer_type((an_integer_kind)ik_long_long));
  }  /* if */
#endif /* 0 */
  if (float16_enabled) {
    (void)enter_predefined_typedef(
                    "_Float16",
                    float_type(float16_kind()));
  }  /* if */
  if (float80_enabled) {
    (void)enter_predefined_typedef(
               "__float80", float_type((a_float_kind)float_kind_for_float80));
  }  /* if */
  if (float128_enabled) {
    (void)enter_predefined_typedef(
             "__float128", float_type((a_float_kind)float_kind_for_float128));
  }  /* if */
#if GNU_EXTENSIONS_ALLOWED
  if (gnu_mode) {
#if GCC_BUILTIN_VARARGS
    enter_builtin_va_list_type();
#endif /* GCC_BUILTIN_VARARGS */
#if INT128_EXTENSIONS_ALLOWED
    if (int128_extensions_enabled) {
      enter_128bit_integer_typedefs();
    }  /* if */
#endif /* INT128_EXTENSIONS_ALLOWED */
    if (gnu_version >= 40000 && !clang_mode) {
      a_type_ptr file_star_type = init_predeclared_class(
                                                        (a_type_kind)tk_struct,
                                                        "_IO_FILE");
      enter_predeclared_class(file_star_type, DEPTH_OF_FILE_SCOPE,
                              &null_source_position);
    }  /* if */
    if (clang_mode || gnu_version_is(>=60000)) {
      /* Clang has always had support for __fp16 and GNU had initial support
         for __fp16 on ARM targets with support for x86 targets later.  The
         type is available in both C and C++ modes. */
      (void)enter_predefined_typedef("__fp16", float_type(fk_fp16));
    }  /* if */
    if (clang_version_is(>=110000) || gnu_version_is(>=100000)) {
      /* Both Clang and GNU had initial support for __bf16 on ARM targets
         with support for x86 targets later.  The type is enabled here based
         on the earliest version it occurred in (regardless of architecture).
         The type is available in both C and C++ modes. */
      (void)enter_predefined_typedef("__bf16", float_type(fk_std_bfloat16));
    }  /* if */
#if GNU_VECTOR_TYPES_ALLOWED
    if (gnu_version_is(>=50000)) {
      /* GNU makes ARM NEON vector and polyvector types available as
         predefined typedefs; Clang supports them via the "neon_vector_type"
         and "neon_polyvector_type" attributes. */
      if (targ_supports_arm64) {
        enter_float_neon_vector_types(float_neon_vector_types_64bit);
        enter_integer_neon_vector_types(integer_neon_vector_types_64bit,
                                        vk_neon);
        enter_integer_neon_vector_types(integer_neon_polyvector_types_64bit,
                                        vk_neon_poly);
        (void)enter_predefined_typedef("__Poly8_t",
                                       integer_type(ik_unsigned_char));
        (void)enter_predefined_typedef("__Poly16_t",
                                       integer_type(ik_unsigned_short));
        (void)enter_predefined_typedef("__Poly64_t",
                                       integer_type(ik_unsigned_long));
#if INT128_EXTENSIONS_ALLOWED
        (void)enter_predefined_typedef("__Poly128_t",
                                       integer_type(ik_unsigned_int128));
#endif /* INT128_EXTENSIONS_ALLOWED */
      } else if (targ_supports_arm32) {
        enter_float_neon_vector_types(float_neon_vector_types_32bit);
        enter_integer_neon_vector_types(integer_neon_vector_types_32bit,
                                        vk_neon);
        enter_integer_neon_vector_types(integer_neon_polyvector_types_32bit,
                                        vk_neon_poly);
      }  /* if */
    }  /* if */
    if (clang_version_is(>=100000) || gnu_version_is(>=100000)) {
      if (targ_supports_arm64) {
        /* Both Clang and GNU have added support for scalable vector types on
           ARM64 starting with version 10.x.  Clang 11.x and later also
           predefine scalable vector types with 2, 3, and 4 tuple elements. */
        enter_all_scalable_vector_types(/*enter_single_tuple_element=*/TRUE,
                                        clang_version_is(>=110000),
                                        /*strip_name_prefix=*/FALSE);
        if (clang_version_is(>=170000) || gnu_version_is(>=140000)) {
          (void)enter_predefined_typedef("__SVCount_t",
                                         scalable_vector_count_type());
        }  /* if */
        if (clang_version_is(>=200000) || gnu_version_is(>=150000)) {
          (void)enter_predefined_typedef("__mfp8",
                                         modal_8bit_floating_point_type());
        }  /* if */
      }  /* if */
    }  /* if */
    if (target_is_riscv_based() &&
        (clang_version_is(>=140000) || gnu_version_is(>=130000))) {
      a_boolean  tuple_types_supported = clang_version_is(>=170000) ||
                                         gnu_version_is(>=140000);
      a_boolean  bfloat16_supported = clang_version_is(>=190000) ||
                                      gnu_version_is(>=150000);
      a_boolean  ofp8_supported = clang_version_is(>=230000);

      enter_all_riscv_vector_types("__rvv_", bfloat16_supported,
                                   ofp8_supported, tuple_types_supported);
    }  /* if */
    if (gnu_version_is(>=40800)) {
      /* GCC also predefines additional types for NEON builtins, starting with
         version 4.8. */
      if (targ_supports_arm32) {
        if (gnu_version_is(>=100000)) {
          (void)enter_predefined_typedef("__builtin_neon_bf",
                                         float_type(fk_std_bfloat16));
        } else if (gnu_version_is(<60000)) {
          (void)enter_predefined_typedef("__builtin_neon_hf",
                                         float_type(fk_fp16));
        }  /* if */
        (void)enter_predefined_typedef("__builtin_neon_sf",
                                       float_type(fk_float));
        (void)enter_predefined_typedef("__builtin_neon_df",
                                       float_type(fk_double));
        (void)enter_predefined_typedef("__builtin_neon_qi",
                                       integer_type(ik_signed_char));
        (void)enter_predefined_typedef("__builtin_neon_uqi",
                                       integer_type(ik_unsigned_char));
        (void)enter_predefined_typedef("__builtin_neon_hi",
                                       integer_type(ik_short));
        (void)enter_predefined_typedef("__builtin_neon_uhi",
                                       integer_type(ik_unsigned_short));
        (void)enter_predefined_typedef("__builtin_neon_si",
                                       integer_type(ik_int));
        (void)enter_predefined_typedef("__builtin_neon_usi",
                                       integer_type(ik_unsigned_int));
        (void)enter_predefined_typedef("__builtin_neon_di",
                                       integer_type(ik_long_long));
        (void)enter_predefined_typedef("__builtin_neon_udi",
                                       integer_type(ik_unsigned_long_long));
        (void)enter_predefined_typedef("__builtin_neon_poly8",
                                       integer_type(ik_signed_char));
        (void)enter_predefined_typedef("__builtin_neon_poly16",
                                       integer_type(ik_short));
        (void)enter_predefined_typedef("__builtin_neon_poly64",
                                       integer_type(ik_unsigned_long_long));
#if INT128_EXTENSIONS_ALLOWED
        (void)enter_predefined_typedef("__builtin_neon_poly128",
                                       integer_type(ik_unsigned_int128));
        (void)enter_predefined_typedef("__builtin_neon_uti",
                                       integer_type(ik_unsigned_int128));
#endif /* INT128_EXTENSIONS_ALLOWED */
        enter_builtin_integer_neon_vector_types(
                                      integer_builtin_neon_vector_types_32bit);
      } else if (targ_supports_arm64) {
        if (gnu_version_is(>=150000)) {
          enter_neon_vector_types(modal_8bit_floating_point_type(), 8,
                                  {"__Mfloat8x8_t", "__Mfloat8x16_t"},
                                  vk_neon);
        }  /* if */
        if (gnu_version_is(>=100000)) {
          (void)enter_predefined_typedef("__builtin_aarch64_simd_bf",
                                         float_type(fk_std_bfloat16));
        }  /* if */
        if (gnu_version_is(>=60000)) {
          (void)enter_predefined_typedef("__builtin_aarch64_simd_hf",
                                         float_type(fk_fp16));
        }  /* if */
        (void)enter_predefined_typedef("__builtin_aarch64_simd_sf",
                                       float_type(fk_float));
        (void)enter_predefined_typedef("__builtin_aarch64_simd_df",
                                       float_type(fk_double));
        (void)enter_predefined_typedef("__builtin_aarch64_simd_qi",
                                       integer_type(ik_signed_char));
        (void)enter_predefined_typedef("__builtin_aarch64_simd_uqi",
                                       integer_type(ik_unsigned_char));
        (void)enter_predefined_typedef("__builtin_aarch64_simd_hi",
                                       integer_type(ik_short));
        (void)enter_predefined_typedef("__builtin_aarch64_simd_uhi",
                                       integer_type(ik_unsigned_short));
        (void)enter_predefined_typedef("__builtin_aarch64_simd_si",
                                       integer_type(ik_int));
        (void)enter_predefined_typedef("__builtin_aarch64_simd_usi",
                                       integer_type(ik_unsigned_int));
        (void)enter_predefined_typedef("__builtin_aarch64_simd_di",
                                       integer_type(ik_long));
        (void)enter_predefined_typedef("__builtin_aarch64_simd_udi",
                                       integer_type(ik_unsigned_long));
        (void)enter_predefined_typedef("__builtin_aarch64_simd_poly8",
                                       integer_type(ik_unsigned_char));
        (void)enter_predefined_typedef("__builtin_aarch64_simd_poly16",
                                       integer_type(ik_unsigned_short));
        (void)enter_predefined_typedef("__builtin_aarch64_simd_poly64",
                                       integer_type(ik_unsigned_long));
#if INT128_EXTENSIONS_ALLOWED
        (void)enter_predefined_typedef("__builtin_aarch64_simd_poly128",
                                       integer_type(ik_unsigned_int128));
        (void)enter_predefined_typedef("__builtin_aarch64_simd_ti",
                                       integer_type(ik_int128));
#endif /* INT128_EXTENSIONS_ALLOWED */
        enter_builtin_integer_neon_vector_types(
                                      integer_builtin_neon_vector_types_64bit);
      }  /* if */
    }  /* if */
#endif /* GNU_VECTOR_TYPES_ALLOWED */
  }  /* if */
#endif /* GNU_EXTENSIONS_ALLOWED */
#if BUILTIN_FUNCTIONS_ENABLED
  if (is_primary_translation_unit) {
    /* Enter symbol headers for any applicable builtin functions.  The
       routines themselves will be lazily loaded as needed. */
    preload_builtin_symbols();
  }  /* if */
#endif /* BUILTIN_FUNCTIONS_ENABLED */
  if (variadic_templates_enabled) {
    if ((microsoft_mode && microsoft_version >= 1900) ||
         clang_mode) {
      /* Create an alias template for "__make_integer_seq". */
      make_make_integer_seq_internal_template();
    }  /* if */
    if (clangcpp_version_is(>=30900) || gnu_version_is(>=140000)) {
      /* Create an alias template for "__type_pack_element". */
      make_type_pack_element_internal_template();
    }  /* if */
    if (clangcpp_version_is(>=200000)) {
      /* Create class and alias templates for "__builtin_common_type". */
      make_builtin_common_type_internal_templates();
    }  /* if */
    if (clangcpp_version_is(>=220000)) {
      /* Create a class template for "__builtin_dedup_pack". */
      make_builtin_dedup_pack_internal_template();
    }  /* if */
  }  /* if */
#if UPC_EXTENSIONS_ALLOWED
  if (upc_mode) {
    enter_upc_predefined_macros();
  }  /* if */
#endif /* UPC_EXTENSIONS_ALLOWED */
#if NAMED_ADDRESS_SPACES_ALLOWED
  if (named_address_spaces_enabled) {
    enter_predefined_named_address_spaces();
  }  /* if */
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */
#if NAMED_REGISTERS_ALLOWED
  if (named_registers_enabled) {
    enter_predefined_named_registers();
  }  /* if */
#endif /* NAMED_REGISTERS_ALLOWED */
}  /* enter_system_specific_predeclared_symbols */


void enter_system_specific_predefined_macros_and_assertions(void)
/*
Define system-specific predefined macros and builtin #assert predicates
*/
{
  /* System-specific macros: */
#if 0
  /* For example: */
  (void)enter_predef_macro("1", "unix", /*cannot_be_redefined=*/TRUE,
                           /*ref_suppresses_pch_file=*/FALSE);
#endif /* 0 */
#if ATT_PREPROCESSING_EXTENSIONS_ALLOWED
  /* Define predefined #assert predicates: */
  /* CAREFUL:  The value string must have an extra blank at the end. */
  /* For example:
  enter_assert_predicate("m68k ", "machine");
  */
#ifdef __sparc
  enter_assert_predicate("sparc ", "machine");
#endif /* ifdef __sparc */
#endif /* ATT_PREPROCESSING_EXTENSIONS_ALLOWED */
#ifdef __linux__
  enter_linux_predefined_macros();
#else /* !defined(__linux__) */
#ifdef __sparc
  enter_sparc_predefined_macros();
#else /* ifndef __sparc */
#if defined(__APPLE__) && defined(__MACH__)
  enter_macosx_predefined_macros();
#endif /* defined(__APPLE__) && defined(__MACH__) */
#endif /* ifdef __sparc */
#endif /* ifdef __linux__ */
}  /* enter_system_specific_predefined_macros_and_assertions */

#if GNU_EXTENSIONS_ALLOWED
#if USE_X86_FUNCTION_MULTIVERSIONING

/*
Table of valid "target" attributes for GNU function multiversioning.
This table is used in three different ways: to map a "target" attribute
to a specific architecture (see find_target_attribute), to map an architecture
to a string to be used by the GNU __builtin_cpu_is/__builtin_cpu_supports
functions (see target_name_for_builtin), and to generate the target-specific
portion of a mangled name (see target_distinction).  Note that these aren't
all one-to-one mappings: the names in the latter two cases are massaged as
necessary to match GNU's behavior.
*/
static constexpr a_const_char *target_attributes[] = {
  NULL,               /* mvak_unknown */
  "arch=bdver1",      /* mvak_cpu_bdver1 */
  "arch=bdver2",      /* mvak_cpu_bdver2 */
  "arch=corei7",      /* mvak_cpu_corei7 */
  "arch=amdfam10",    /* mvak_cpu_amdfam10h */
  "arch=core2",       /* mvak_cpu_core2 */
  "arch=atom",        /* mvak_cpu_atom */
  "default",          /* mvak_default_target */
  "mmx",              /* mvak_isa_mmx */
  "sse",              /* mvak_isa_sse */
  "sse2",             /* mvak_isa_sse2 */
  "sse3",             /* mvak_isa_sse3 */
  "ssse3",            /* mvak_isa_ssse3 */
  "sse4",             /* mvak_isa_sse4 */
  "sse4a",            /* mvak_isa_sse4a */
  "sse4.1",           /* mvak_isa_sse4_1 */
  "sse4.2",           /* mvak_isa_sse4_2 */
  "popcnt",           /* mvak_isa_popcnt */
  "aes",              /* mvak_isa_aes */
  "pclmul",           /* mvak_isa_pclmul */
  "avx",              /* mvak_isa_avx */
  "bmi",              /* mvak_isa_bmi */
  "fma4",             /* mvak_isa_fma4 */
  "xop",              /* mvak_isa_xop */
  "fma",              /* mvak_isa_fma */
  "bmi2",             /* mvak_isa_bmi2 */
  "avx2",             /* mvak_isa_avx2 */
  "avx512f",          /* mvak_isa_avx512f */
};

/*
GNU's mangled names for ISA architectures are emitted in alphabetical order
so this table lists the ISA architectures in that order.
*/
static constexpr a_multiversion_arch_kind isa_alphabetic_order[] = {
  (a_multiversion_arch_kind)mvak_isa_aes,
  (a_multiversion_arch_kind)mvak_isa_avx,
  (a_multiversion_arch_kind)mvak_isa_avx2,
  (a_multiversion_arch_kind)mvak_isa_avx512f,
  (a_multiversion_arch_kind)mvak_isa_bmi,
  (a_multiversion_arch_kind)mvak_isa_bmi2,
  (a_multiversion_arch_kind)mvak_isa_fma,
  (a_multiversion_arch_kind)mvak_isa_fma4,
  (a_multiversion_arch_kind)mvak_isa_mmx,
  (a_multiversion_arch_kind)mvak_isa_pclmul,
  (a_multiversion_arch_kind)mvak_isa_popcnt,
  (a_multiversion_arch_kind)mvak_isa_sse,
  (a_multiversion_arch_kind)mvak_isa_sse2,
  (a_multiversion_arch_kind)mvak_isa_sse3,
  (a_multiversion_arch_kind)mvak_isa_sse4,
  (a_multiversion_arch_kind)mvak_isa_sse4_1,
  (a_multiversion_arch_kind)mvak_isa_sse4_2,
  (a_multiversion_arch_kind)mvak_isa_sse4a,
  (a_multiversion_arch_kind)mvak_isa_ssse3,
  (a_multiversion_arch_kind)mvak_isa_xop
};


static a_multiversion_arch_kind find_target_attribute(a_const_char *str,
                                                      size_t       str_len)
/*
Return the a_multiversion_arch_kind for the "target" attribute pointed to by
str whose length is strlen (str may not be NULL terminated).  If no attribute
is found, mvak_unknown is returned.
*/
{
  a_multiversion_arch_kind result = mvak_unknown;
  signed char              arch;

  for (arch = mvak_lowest_cpu; arch < mvak_last; arch++) {
    if (strlen(target_attributes[arch]) == str_len &&
        strncmp(str, target_attributes[arch], str_len) == 0) {
      result = (a_multiversion_arch_kind)arch;
      break;
    }  /* if */
  }  /* for */
  return result;
}  /* find_target_attribute */

#if DO_IL_LOWERING

a_const_char *target_name_for_builtin(a_multiversion_arch_kind arch)
/*
Return a string that identifies the specified CPU or ISA architecture to
be used as an argument for the GNU __builtin_cpu_is/__builtin_cpu_supports
calls.
*/
{
  a_const_char *result = target_attributes[arch];

  if (arch == (a_multiversion_arch_kind)mvak_cpu_amdfam10h) {
    /* Special case: "target" attribute is "amdfam10", but "amdfam10h" is
       required for the builtin calls. */
    result = "amdfam10h";
  } else if (is_mv_cpu_arch(arch)) {
    /* Strip the "arch=" from CPU architecture cases. */
    check_assertion(strncmp(result, "arch=", 5) == 0);
    result += 5;
  }  /* if */
  return result;
}  /* target_name_for_builtin */

#endif /* DO_IL_LOWERING */

static a_const_char *target_distinction(a_multiversion_arch_kind arch)
/*
Return a string that identifies the specified CPU or ISA architecture to
be used as part of a "mangled" name.  The strings returned here match those
generated by GNU.  The returned value may point to a static buffer, so the
value must be copied before a second call is made.
*/
{
  a_const_char *result = target_attributes[arch];
  STATIC_THREAD char  buffer[20];

  if (is_mv_cpu_arch(arch)) {
    /* Replace "arch=" with "arch_" in CPU architecture cases. */
    check_assertion(strncmp(result, "arch=", 5) == 0 &&
                    strlen(result) < sizeof(buffer));
    (void)strcpy(buffer, result);
    buffer[4] = '_';
    result = buffer;
#if REPLACE_SPECIAL_CHARACTERS_IN_MANGLED_NAMES
  } else if (strchr(result, '.') != NULL) {
    /* For C-generating back ends, mangled names can't have periods, so replace
       those with underscores. */
    char *ptr;
    check_assertion(strlen(result) + 1 < sizeof(buffer));
    (void)strcpy(buffer, result);
    for (ptr = strchr(buffer, '.'); ptr != NULL; ptr = strchr(ptr, '.')) {
      *ptr = '_';
    }  /* if */
    result = buffer;
#endif /* REPLACE_SPECIAL_CHARACTERS_IN_MANGLED_NAMES */
  }  /* if */
  return result;
}  /* target_distinction */


static a_multiversion_arch_kind highest_isa(a_mv_target_bitset       bitset,
                                            a_multiversion_arch_kind *cpu_arch)
/*
Return the highest (i.e., most capable) Instruction Set Architecture
capability of the specified bitset.  If a CPU architecture is specified
in the bitset, then map that to the corresponding ISA architecture before
determining the highest.  Set *cpu_arch to the CPU architecture (there can
be at most one) if one is found (and to mvak_invalid otherwise).
*/
{
  signed char              arch;
  a_multiversion_arch_kind result_isa = mvak_lowest_isa;

  *cpu_arch = (a_multiversion_arch_kind)mvak_invalid;
  /* First, check if there's a CPU architecture specified in the bitset.
     If there is, get the highest architecture supported by the arch. */
  for (arch = mvak_lowest_cpu; arch <= mvak_highest_cpu; arch++) {
    a_multiversion_arch_kind arch_isa = (a_multiversion_arch_kind)mvak_invalid;
    switch (arch) {
      case mvak_cpu_bdver1:
      case mvak_cpu_bdver2:
        arch_isa = (a_multiversion_arch_kind)mvak_isa_avx2;
        break;
      case mvak_cpu_corei7:
        arch_isa = (a_multiversion_arch_kind)mvak_isa_popcnt;
        break;
      case mvak_cpu_amdfam10h:
        arch_isa = (a_multiversion_arch_kind)mvak_isa_ssse3;
        break;
      case mvak_cpu_core2:
      case mvak_cpu_atom:
        arch_isa = (a_multiversion_arch_kind)mvak_isa_ssse3;
        break;
      default_is_unexpected();
    }  /* switch */
    if (bitset & ((a_mv_target_bitset)1<<arch)) {
      result_isa = arch_isa;
      *cpu_arch = (a_multiversion_arch_kind)arch;
      break;
    }  /* if */
  }  /* for */
  /* Check all the ISAs specified in the bitset, and choose the highest. */
  for (arch = mvak_lowest_isa; arch <= mvak_highest_isa; arch++) {
    if ((bitset & ((a_mv_target_bitset)1<<arch)) && result_isa < arch) {
      result_isa = (a_multiversion_arch_kind)arch;
    }  /* if */
  }  /* for */
  return result_isa;
}  /* highest_isa */


static int compare_target_priority(a_mv_target_bitset left,
                                   a_mv_target_bitset right)
/*
Compares the target information in left and right using the rules
in the GNU Function MultiVersioning wiki and returns the usual -1, 0, or 1
for less, equal, or greater.  "default" always compares "less than" (which
keeps it at the head of a sorted list).
*/
{
  int                      result;
  a_multiversion_arch_kind left_isa, right_isa;
  a_multiversion_arch_kind left_cpu_arch, right_cpu_arch;

  if (is_default_targ_bitset(left) && is_default_targ_bitset(right)) {
    result = 0;
  } else if (is_default_targ_bitset(left)) {
    result = -1;
  } else if (is_default_targ_bitset(right)) {
    result = 1;
  } else {
    left_isa = highest_isa(left, &left_cpu_arch);
    right_isa = highest_isa(right, &right_cpu_arch);
    if (left_isa < right_isa) result = 1;
    else if (left_isa > right_isa) result = -1;
    else {
      if (left_cpu_arch != (a_multiversion_arch_kind)mvak_invalid &&
          right_cpu_arch != (a_multiversion_arch_kind)mvak_invalid) {
        if (left_cpu_arch == right_cpu_arch) result = 0;
        else if (left_cpu_arch == (a_multiversion_arch_kind)mvak_cpu_bdver1 &&
                 right_cpu_arch == (a_multiversion_arch_kind)mvak_cpu_bdver2)
          result = 1;
        else if (left_cpu_arch == (a_multiversion_arch_kind)mvak_cpu_bdver2 &&
                 right_cpu_arch == (a_multiversion_arch_kind)mvak_cpu_bdver1)
          result = -1;
        else
          result = 0;
      } else if (left_cpu_arch == right_cpu_arch) result = 0;
      else if (right_cpu_arch != (a_multiversion_arch_kind)mvak_invalid) {
        result = 1;
      }  /* if */
      else result = -1;
    }  /* if */
  }  /* if */
  return result;
}  /* compare_target_priority */


a_routine_ptr find_mv_target_specific_routine(
                                             a_routine_ptr routine,
                                             a_routine_ptr surrounding_routine)
/*
Returns a target-specific version (of the set of routines represented by
"routine") that can be substituted for "routine", in the context of
surrounding_routine, if one exists (otherwise returns NULL).  This is basically
an optimization to circumvent the use of a resolver routine when possible.
If the routine is referenced in a function, surrounding_routine points to
that function (and is NULL otherwise).
*/
{
  a_routine_list_entry_ptr rlep;
  a_mv_target_bitset       surrounding_bitset, bs;
  a_routine_ptr            result = NULL;

  check_assertion(is_multiversion_representative(routine));
  if (has_exactly_one_target_specific_routine(routine)) {
    /* There's only one target-specific routine.  Return that routine (no
       resolver function is needed). */
    result = gnu_routine_supp(routine)->
                             mv_info.representative.targeted_versions->routine;
  } else if (surrounding_routine != NULL &&
             has_gnu_routine_supp(surrounding_routine) &&
             gnu_routine_supp(surrounding_routine)->
                                                  is_target_specific_version) {
    /* If the surrounding routine is target-specific, see if we can find
       a match on the list of target-specific routines for that target. */
    surrounding_bitset = gnu_routine_supp(surrounding_routine)->
                                        mv_info.targeted_version.target_bitset;
    for (rlep =
           gnu_routine_supp(routine)->mv_info.representative.targeted_versions;
         rlep != NULL;
         rlep = rlep->next) {
      bs = gnu_routine_supp(rlep->routine)->
                                        mv_info.targeted_version.target_bitset;
      if ((bs & surrounding_bitset) != 0) {
        result = rlep->routine;
        /* Note that it is possible for more than one target-specific version
           to match, but GNU seems to use the first. */
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  return result;
}  /* find_mv_target_specific_routine */


void reference_to_mv_routine(a_routine_ptr      routine,
                             a_source_position  *error_pos)
/*
A reference to routine (a GNU function multiversion representative routine) is
being made.  Two things are done here: a decision is made as to whether or not
a resolver routine will be needed, and an error is given (at *error_pos) if a
resolver routine is needed and no "default" routine is provided.
*/
{
  a_routine_ptr   surrounding_routine = NULL;
  a_gnu_routine_supplement_ptr
                  grsp = gnu_routine_supp(routine);

  check_assertion(is_multiversion_representative(routine));
  if (depth_innermost_function_scope != NO_SCOPE_DEPTH) {
    surrounding_routine =
                     scope_stack[depth_innermost_function_scope].assoc_routine;
  }  /* if */
  if (grsp->mv_resolver_required) {
    /* It has previously been determined that a resolver is required. */
  } else if (find_mv_target_specific_routine(routine, surrounding_routine)
                                                                     != NULL) {
    /* This routine can be replaced by a reference to a target-specific
       version routine: no resolver is needed. */
  } else if (has_mv_default_routine(routine)) {
    /* Normal case: a resolver routine is required.  Record the fact that
       a resolver is needed. */
    grsp->mv_resolver_required = TRUE;
  } else if (error_pos != NULL) {
    /* A "default" version is needed but not provided. */
    expr_pos_error(ec_gnu_mv_default_missing, error_pos);
  }  /* if */
  return;
}  /* reference_to_mv_routine */

#endif /* USE_X86_FUNCTION_MULTIVERSIONING */
#if GNU_FUNCTION_MULTIVERSIONING

void add_to_specific_version_list(a_routine_ptr representative,
                                  a_routine_ptr target_routine)
/*
This function inserts target_routine into the list of specific-target routines
that are pointed to by representative.
*/
{
  a_routine_list_entry_ptr new_rlep, *headp =
   &gnu_routine_supp(representative)->mv_info.representative.targeted_versions;

  new_rlep = alloc_list_entry_for_routine();
  new_rlep->routine = target_routine;
  ensure_gnu_routine_supp(target_routine)->
                      mv_info.targeted_version.representative = representative;
#if USE_X86_FUNCTION_MULTIVERSIONING
  {
    /* The list of target-specific version functions is kept in priority
       order -- highest priority first -- which makes generating the resolver
       function easier (among other things).  The one exception is that the
       "default" priority routine is always at a special location at the head
       of the list. */
    a_routine_list_entry_ptr head = *headp;
    if (head == NULL) {
      *headp = new_rlep;
    } else {
      a_routine_list_entry_ptr previous = NULL;
      a_routine_list_entry_ptr rlep;
      a_mv_target_bitset target_bs = gnu_routine_supp(target_routine)->
                                        mv_info.targeted_version.target_bitset;
      for (rlep = head; rlep != NULL; rlep = rlep->next) {
        a_mv_target_bitset rlep_bs = gnu_routine_supp(rlep->routine)->
                                        mv_info.targeted_version.target_bitset;
        check_assertion(target_bs != rlep_bs ||
                        is_unknown_targ_bitset(target_bs));
        if (compare_target_priority(target_bs, rlep_bs) < 0) {
          /* Found the insertion point. */
          break;
        }  /* if */
        previous = rlep;
      }  /* for */
      if (previous == NULL) {
        /* Insert at head of list. */
        new_rlep->next = head;
        *headp = new_rlep;
      } else {
        new_rlep->next = previous->next;
        previous->next = new_rlep;
      }  /* if */
    }  /* if */
  }
#else /* !USE_X86_FUNCTION_MULTIVERSIONING */
  /* Ordering doesn't matter; add it to the head. */
  new_rlep->next = *headp;
  *headp = new_rlep;
#endif /* USE_X86_FUNCTION_MULTIVERSIONING */
}  /* add_to_specific_version_list */


a_const_char *target_specific_distinction(a_routine_ptr routine)
/*
Return a string that is used in the mangled name for routine to differentiate
this target-specific routine from other target-specific routines.  The
pointer that is returned is to a static buffer so the caller should copy the
result to an allocated area.
*/
{
#define STATIC_BUFFER_SIZE 256
  STATIC_THREAD char        buffer[STATIC_BUFFER_SIZE];
  size_t             buff_idx = 0;
#if USE_X86_FUNCTION_MULTIVERSIONING
  size_t             i;
  a_boolean          is_first = TRUE, too_long;
  a_const_char       *arch_name;
  a_mv_target_bitset bs =
             gnu_routine_supp(routine)->mv_info.targeted_version.target_bitset;
  signed char        arch;

  check_assertion(gnu_routine_supp(routine)->is_target_specific_version);
  if (is_unknown_targ_bitset(bs)) {
    /* An unknown target attribute; just copy it to the mangled name.  The
       attribute is in "raw token" form, so it has quotation marks.  Note that
       the string is not sorted; there may be problems if the string contains
       multiple target attributes. */
    a_const_char     *start, *end;
    size_t           len;
    an_attribute_ptr ap = find_attribute(ak_target,
                                         routine->source_corresp.attributes);
    check_assertion(ap != NULL && ap->arguments != NULL &&
                    ap->arguments->kind ==
                                        (an_attribute_arg_kind)aak_raw_token &&
                    *ap->arguments->variant.token == '"');
    start = ap->arguments->variant.token + 1;
    end = strchr(start, '"');
    check_assertion(end != NULL);
    len = (size_t)(end - start);
    if (len >= STATIC_BUFFER_SIZE) goto done;
    memcpy(&buffer[0], start, len);
    buff_idx = len;
  } else {
    /* This loop adds the CPU architecture name (if any). */
    for (arch = mvak_lowest_cpu; arch <= mvak_highest_cpu; arch++) {
      if (bs & ((a_mv_target_bitset)1<<arch)) {
        arch_name = target_distinction((a_multiversion_arch_kind)arch);
        is_first = FALSE;
        check_assertion(buff_idx == 0);
        if (strlen(arch_name) >= STATIC_BUFFER_SIZE) goto done;
        (void)strcpy(&buffer[0], arch_name);
        buff_idx = strlen(arch_name);
        break;
      }  /* if */
    }  /* for */
    /* This loop adds the ISA architecture name(s), if any, in alphabetical
       order. */
    for (i = 0;
         i < sizeof(isa_alphabetic_order)/sizeof(isa_alphabetic_order[0]);
         i++) {
      arch = isa_alphabetic_order[i];
      if (bs & ((a_mv_target_bitset)1<<arch)) {
        arch_name = target_distinction((a_multiversion_arch_kind)arch);
        if (is_first) {
          is_first = FALSE;
        } else {
          too_long = buff_idx + 1 >= STATIC_BUFFER_SIZE;
          check_assertion(!too_long);
          if (too_long) goto done;
          buffer[buff_idx++] = '_';
        }  /* if */
        too_long = buff_idx + strlen(arch_name) >= STATIC_BUFFER_SIZE;
        check_assertion(!too_long);
        if (too_long) goto done;
        (void)strcpy(&buffer[buff_idx], arch_name);
        buff_idx += strlen(arch_name);
      }  /* if */
    }  /* for */
  }  /* if */
done:
  /* Make sure the string is NULL terminated (only an issue if we've run out of
     buffer space). */
  if (buff_idx < STATIC_BUFFER_SIZE) buffer[buff_idx] = '\0';
#endif /* USE_X86_FUNCTION_MULTIVERSIONING */
  if (buff_idx == 0) buffer[0] = '\0';
  return buffer;
#undef STATIC_BUFFER_SIZE
}  /* target_specific_distinction */


a_routine_ptr find_existing_mv_routine(
                                ARG_UNUSED a_routine_ptr        representative,
                                ARG_UNUSED a_routine_ptr        candidate,
                                ARG_UNUSED an_attribute_arg_ptr aap)
/*
Returns a pointer to a target-specific version routine with the same
"target" attributes as "candidate" or NULL if none is found.
representative is the representative routine for the specific group of
multiversion functions.  aap is a pointer to the "target" attributes for
candidate (they haven't been applied to the routine yet).  Called during
attribute processing to check for re-declarations.
*/
{
  a_routine_ptr            result = NULL;
#if USE_X86_FUNCTION_MULTIVERSIONING
  a_routine_list_entry_ptr rlep;

  /* Two target-specific routines are deemed equivalent if their
     mv_target_bitset values are the same. */
  for (rlep = gnu_routine_supp(representative)->
                                      mv_info.representative.targeted_versions;
       rlep != NULL;
       rlep = rlep->next) {
    a_routine_ptr rp = rlep->routine;
    if (gnu_routine_supp(candidate)->mv_info.targeted_version.target_bitset ==
                gnu_routine_supp(rp)->mv_info.targeted_version.target_bitset) {
      if (is_unknown_targ_bitset(
               gnu_routine_supp(rp)->mv_info.targeted_version.target_bitset)) {
        /* Both routines have unknown target attributes; they're the same
           only if the target attributes are also the same. */
        an_attribute_ptr ap;
        ap = find_attribute(ak_target, rp->source_corresp.attributes);
        check_assertion(ap != NULL && aap != NULL &&
                        ap->arguments != NULL &&
                        aap->kind == (an_attribute_arg_kind)aak_raw_token &&
                        ap->arguments->kind ==
                                         (an_attribute_arg_kind)aak_raw_token);
        if (strcmp(aap->variant.token, ap->arguments->variant.token) == 0) {
          result = rp;
          break;
        }  /* if */
      } else {
        result = rp;
        break;
      }  /* if */
    }  /* if */
  }  /* for */
#endif /* USE_X86_FUNCTION_MULTIVERSIONING */
  return result;
}  /* find_existing_mv_routine */

#endif /* GNU_FUNCTION_MULTIVERSIONING */

void validate_target_argument(ARG_UNUSED a_const_char         *str,
                              ARG_UNUSED size_t               str_len,
                              ARG_UNUSED an_attribute_arg_ptr aap,
                              ARG_UNUSED a_routine_ptr        routine,
                              ARG_UNUSED a_boolean            *error_issued)
/*
Validates the "target" attribute pointed to by str whose length is
str_len.  The attribute argument is pointed to by aap and is being
applied to routine.  If any errors are issued, *error_issued is set to TRUE.
str may not be NULL terminated (e.g., it may have a trailing comma), so
str_len should be used to determine the end of the argument.
*/
{
#if USE_X86_FUNCTION_MULTIVERSIONING
  /* When using x86 function multiversioning, additional checking is
     performed to ensure that only one CPU architecture is specified and
     the mv_target_bitset for the routine is updated to reflect the
     target argument. */
  a_multiversion_arch_kind arch = find_target_attribute(str, str_len);
  a_boolean                err = FALSE;

  if (arch == (a_multiversion_arch_kind)mvak_unknown) {
    /* An unknown target attribute.  The list of "target" attributes used in
       system headers is continually growing and the front end only recognizes
       those needed to create a resolver routine, so accept unknown attributes
       in system headers (the back end may know what to do with these).  Issue
       a discretionary error otherwise. */
    if (!seq_is_in_system_header(aap->position.seq)) {
      an_error_severity es = C_mode() ? es_warning : es_discretionary_error;
      pos_diagnostic(es, ec_unrecognized_target_attribute,
                     &aap->position);
      if (is_effective_error(ec_unrecognized_target_attribute, es,
                             &aap->position)) {
        err = TRUE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (err) {
    *error_issued = TRUE;
  } else if (C_mode()) {
    /* The presence of the argument is sufficient. */
  } else if (skip_typerefs(routine->type)->
        variant.routine.extra_info->routine_name_linkage ==
                                           (a_name_linkage_kind)nlk_external) {
    /* An extern "C" routine; silently accept the argument. */
  } else {
    a_gnu_routine_supplement_ptr grsp = gnu_routine_supp(routine);
    check_assertion(grsp->is_target_specific_version);
    if (is_mv_cpu_arch(arch) &&
        is_any_mv_arch_bit_set(grsp->mv_info.targeted_version.target_bitset)) {
      /* Can't specify more than one CPU architecture. */
      pos_error(ec_gnu_mv_only_one_arch, &aap->position);
      *error_issued = TRUE;
    } else {
      /* Add this CPU/ISA architecture to the list of target-specific
         versions that this routine supports. */
      grsp->mv_info.targeted_version.target_bitset |=
                                                 (a_mv_target_bitset)1 << arch;
    }  /* if */
  }  /* if */
#else /* !USE_X86_FUNCTION_MULTIVERSIONING */
  /* Issue a warning that we're not doing anything with the attribute in
     this configuration. */
  pos_warning(ec_unrecognized_target_attribute, &aap->position);
#endif /* USE_X86_FUNCTION_MULTIVERSIONING */
}  /* validate_target_argument */

#endif /* GNU_EXTENSIONS_ALLOWED */

a_boolean check_availability_attr(an_attribute_ptr ap)
/*
Clang's "availability" attribute can be placed on declarations to describe the
lifecycle of that declaration relative to operating system versions.  The
attribute syntax is Clang-specific and describes the various operating systems
and versions that the declaration applies to.  See
https://clang.llvm.org/docs/AttributeReference.html#availability.  The front
end has no explicit knowledge of the operating system or version for the
compilation so a customer will likely need to replace this function with an
implementation that is appropriate for their system.

The "availability" attribute is passed in, and the routine should return TRUE
if the attached declaration should be enabled in the current mode.  A return of
FALSE will make the attached declaration "invisible".  If a syntax (or other)
error is found, a diagnostic should be issued here and the attribute should be
changed to ak_unrecognized.  Due to the nature of the argument syntax, the
argument parameters are parsed as a sequence of tokens (i.e., aak_raw_token).

The default version of this routine does not parse the arguments and issues a
warning that the attribute is effectively being ignored.
*/
{
  check_assertion(ap->kind == ak_availability);
  pos_warning(ec_availability_attribute_ignored, &ap->position);
  return TRUE;
}  /* check_availability_attr */


void sys_predef_trans_unit_init(void)
/*
Do initialization for each source file.
*/
{
#if BUILTIN_FUNCTIONS_ENABLED
  builtin_type_table = new_fe<a_builtin_type_map>(/*mask_width=*/10u);
  loaded_builtin_set = new_fe<a_builtin_func_load_set>(/*mask_width=*/10u);
  builtin_overload_set_map =
                  new_fe<a_builtin_overload_set_map>(/*mask_width=*/10u);
  builtin_overload_set_members =
                            new_fe<Dyn_array<a_builtin_overload_set_member>>();
  if (is_primary_translation_unit) {
    builtin_overload_set_categories = 0;
  }  /* if */
  mapped_overload_set_categories = 0;
#endif /* BUILTIN_FUNCTIONS_ENABLED */
}  /* sys_predef_trans_unit_init */


void sys_predef_one_time_init(void)
/*
Do one-time initialization for data structures used in this file.
*/
{
  /* Save variables that are needed for precompiled headers. */
  if (precompiled_header_processing_required) {
    STATIC_THREAD a_pch_saved_variable saved_vars[] = {
#if BUILTIN_FUNCTIONS_ENABLED
      pch_saved_var_array_elem(builtin_type_table),
      pch_saved_var_array_elem(builtin_overload_set_categories),
#endif /* BUILTIN_FUNCTIONS_ENABLED */
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
  /* Register variables that must be saved and restored when switching
     between translation units. */
#if BUILTIN_FUNCTIONS_ENABLED
  register_trans_unit_variable(builtin_type_table);
  register_trans_unit_variable(loaded_builtin_set);
  register_trans_unit_variable(builtin_overload_set_map);
  register_trans_unit_variable(builtin_overload_set_members);
  register_trans_unit_variable(mapped_overload_set_categories);
#endif /* BUILTIN_FUNCTIONS_ENABLED */
#if CHECKING && USE_X86_FUNCTION_MULTIVERSIONING
  /* Perform some configuration checks. */
  if (sizeof(a_mv_target_bitset)*8 < (size_t)mvak_last) { /*lint !e506*/
    internal_error("undersized a_mv_target_bitset");
  }  /* if */
  check_assertion_str((sizeof(target_attributes)/sizeof(target_attributes[0]))
                                        == (a_multiversion_arch_kind)mvak_last,
                      "target_attributes table must have mvak_last elements");
  check_assertion_str((sizeof(isa_alphabetic_order)/
                       sizeof(isa_alphabetic_order[0])) ==
                       (size_t)((a_multiversion_arch_kind)mvak_highest_isa -
                                (a_multiversion_arch_kind)mvak_lowest_isa + 1),
                      "wrong number of elements in isa_alphabetic_order");
#endif /* CHECKING && USE_X86_FUNCTION_MULTIVERSIONING */
#if BUILTIN_FUNCTIONS_ENABLED
  builtin_type_table = NULL;
  loaded_builtin_set = NULL;
  builtin_overload_set_map = NULL;
  builtin_overload_set_members = NULL;
  builtin_overload_set_categories = 0;
  mapped_overload_set_categories = 0;
  builtin_condition_table = (a_builtin_function_condition*)alloc_general(
         num_builtin_condition_entries * sizeof(a_builtin_function_condition));
  memzero((char *)builtin_condition_table,
         num_builtin_condition_entries * sizeof(a_builtin_function_condition));
#endif /* BUILTIN_FUNCTIONS_ENABLED */
}  /* sys_predef_one_time_init */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE


