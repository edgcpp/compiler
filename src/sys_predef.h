/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

sys_predef.h -- System dependent predefined macros and assertions.

*/

/* Avoid including these declarations more than once: */
#ifndef SYS_PREDEF_H
#define SYS_PREDEF_H 1

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
The following macro (NS_alias_templ_intrinsics) describes alias templates in
namespace std that the front end recognizes and attempts to substitute
intrinsically.  The macro takes another macro M that should be of the form:

  #define MACRO(ns, name)

Different parts of the front end invoke NS_alias_templ_intrinsics to
  1) define (here, in sys_predef.h) enumerator constants identifying the
     intrinsics by number,
  2) define a table (in symbol_tbl.c) used to (a) mark associated symbol
     headers for efficient identification, and (b) build a Ptr_map to
     associate a "signature" to match function declarations with, and
  3) produce switch cases (in sys_predef.c) to handle substitution dispatch.

To recognize a new alias template intrinsic named xyz in a namespace N:
  a) Ensure there exists a variable of type a_symbol_ptr named
     symbol_for_namespace_NS (for some unique identifier NS) that 
     represents namespace N.  See, e.g., make_symbol_for_namespace_std,
     which initializes symbol_for_namespace_std (NS = std) and
     symbol_for_namespace_std_meta (NS = std_meta).
  b) Add a line
         M(NS, xyz)
     in the macro below.
  c) Add a function named subst_NS_xyz in sys_predef.c that resolves the
     alias (see, e.g., subst_std_remove_cv_t for the required signature and
     an example definition).
*/

#define NS_alias_templ_intrinsics(M) \
  M(std, remove_cv_t) \
  M(std, remove_const_t) \
  M(std, remove_volatile_t) \
  M(std, remove_reference_t) \
  M(std, remove_cvref_t) \
  /* End of NS_alias_templ_intrinsics. */


enum an_alias_templ_intrinsic {
  ati_error,
#define ATI_name(ns, name) ati_##ns##_##name,
  NS_alias_templ_intrinsics(ATI_name)
#undef ATI_name
  ati_last
};

extern a_boolean eval_intrinsic_alias_templ(int             idx,
                                            a_template_arg  *t_args,
                                            a_type_ptr      *substituted_tp);


/*
NS_var_templ_intrinsics is similar to NS_alias_templ_intrinsics (see above)
but for variable templates whose constant integral-type initializer is
intrinsically instantiated.

To recognize a new variable template intrinsic named xyz in a namespace N:
  a) Ensure there exists a variable of type a_symbol_ptr named
     symbol_for_namespace_NS (for some unique identifier NS) that 
     represents namespace N.  See, e.g., make_symbol_for_namespace_std,
     which initializes symbol_for_namespace_std (NS = std) and
     symbol_for_namespace_std_meta (NS = std_meta).
  b) Add a line
         M(NS, xyz)
     in the macro below.
  c) Add a function named value_of_NS_xyz in sys_predef.c that determines the
     initializer value of instances (see, e.g., value_of_std_is_object_v for
     the required signature and an example definition).
*/

#define NS_var_templ_intrinsics(M) \
  M(std, is_integral_v) \
  M(std, is_object_v) \
  /* End of NS_var_templ_intrinsics. */


enum a_var_templ_intrinsic {
  vti_error,
#define VTI_name(ns, name) vti_##ns##_##name,
  NS_var_templ_intrinsics(VTI_name)
#undef VTI_name
  vti_last
};

extern
a_boolean get_intrinsic_var_templ_value(a_symbol              *vsym,
                                        int                   idx,
                                        a_host_large_integer  *p_val);


/*
NS_templ_type_member_intrinsics is similar to NS_alias_templ_intrinsics
(see above), but for class templates used in the form xyz<A...>::name (for
example, std::enable_if<B,T>::type).  When such a use is encountered, the
front end resolves the named member intrinsically, without completing (i.e.,
instantiating) the xyz<A...> instance.  The macro M should be of the form:

  #define MACRO(ns, name, member)

where member is the name accessed via "::" (typically "type").

To recognize a new type-template member intrinsic for template xyz in a
namespace N accessed as xyz<A...>::member:
  a) Ensure there exists a variable of type a_symbol_ptr named
     symbol_for_namespace_NS that represents namespace N (see, e.g.,
     make_symbol_for_namespace_std).
  b) Add a line
         M(NS, xyz, member)
     in the macro below.
  c) Add a function named subst_NS_xyz in sys_predef.c that resolves the
     member (see, e.g., subst_std_remove_cv for the required signature and an
     example definition).
*/

#define NS_templ_type_member_intrinsics(M) \
  M(std, remove_cv, type) \
  M(std, remove_const, type) \
  M(std, remove_volatile, type) \
  M(std, remove_reference, type) \
  M(std, remove_cvref, type) \
  M(std, enable_if, type) \
  /* End of NS_templ_type_member_intrinsics. */


enum a_templ_type_member_intrinsic {
  ttmi_error,
#define TTMI_name(ns, name, member) ttmi_##ns##_##name,
  NS_templ_type_member_intrinsics(TTMI_name)
#undef TTMI_name
  ttmi_last
};

/*
The tri-state result of attempting to resolve xyz<A...>::name intrinsically.
*/
enum a_templ_type_member_result {
  ttmr_not_applicable,  /* The case could not be decided intrinsically;
                           the caller should fall back to ordinary
                           processing. */
  ttmr_resolved,        /* The member resolves to a type (returned to the
                           caller). */
  ttmr_no_such_member   /* The member provably does not exist (e.g.,
                           std::enable_if<false,T>::type). */
};

extern a_templ_type_member_result eval_intrinsic_templ_type_member(
                                       int             idx,
                                       a_template_arg  *t_args,
                                       a_type_ptr      *substituted_tp);


#if BUILTIN_FUNCTIONS_ENABLED

/*
To add a user-defined builtin function, follow these steps:

  - Add a new enumeration value to a_builtin_user_function_kind below.
  - Add a new entry in builtin_user_table below.  See the description for
    a_builtin_user_descr which details the values of each of the fields.
*/

/*
This typedef is used for character strings used to specify a compact encoding
of a condition under which the builtin function should be enabled.  See the
description of the "cond" field in a_builtin_user_descr below for more
information.
*/
typedef a_const_char *a_builtin_condition_string;

/*
This typedef is used for character strings that contain a C representation
of a builtin function's type.  The string is parsed by the front end if the
builtin function is referenced.
*/
typedef a_const_char *a_builtin_type_string;

/*
Describes a user-defined builtin function.
*/
typedef struct a_builtin_user_descr *a_builtin_user_descr_ptr;
typedef struct a_builtin_user_descr {
  a_const_char  *name;
                        /* The name of the builtin function. */
  a_builtin_condition_string
                cond;
                        /* A compact encoding of the condition(s) in which this
                           builtin function is enabled.  The encoding consists
                           of a sequence of conditions; each condition has
                           seven potential parts (in the following order):

                             - prefix ('S') [optional]
                             - emulation ('L', 'g', or 'm') or
                               "standard" ('s')
                             - mode ('c', '+', or 'x')
                             - arch ('A', 'R', or 'X') [optional]
                             - bits ('4' or '8') [optional]
                             - version (version range in parens) [optional]
                             - restrictions ['v', 'i', 'f', 'c'][optional]

                           A prefix of 'S' indicates that the name in the
                           entry (which must start with "__builtin_") also
                           has a secondary declaration with the same type
                           but without the "__builtin_" prefix.  It appears
                           that secondary declarations are only used by GCC
                           to give warnings on a redeclaration of the function
                           (typically a library function) in C mode, so these
                           secondary declarations are not entered into the
                           symbol table in C++ mode.

                           The 'L' emulation mode indicates that the function
                           applies to clang mode; 'g' indicates GNU mode, and
                           'm' is for Microsoft emulation mode.

                           A mode of 'c' indicates C mode, '+' indicates
                           C++ mode, and 'x' indicates both C and C++ modes.

                           An 'A', 'R', or 'X' indicates the function applies
                           only to architectures where target_is_arm_based,
                           target_is_riscv_based, or target_is_x86_based is
                           TRUE, respectively.

                           A '4' indicates the function applies only to
                           architectures where target_is_64_bits is FALSE
                           and an '8' indicates the function applies only to
                           architectures where target_is_64_bits is TRUE.

                           If a parenthesized range of applicable versions is
                           given, the function is only enabled when the version
                           is within that range.  Either end of the range can
                           be dropped; e.g., "gc(40800-)" means the attribute
                           is valid in GNU C mode with gnu_version >= 40800.

                           If restrictions exist, they are a non-empty
                           sequence of the following characters:
                             'c' - indicates that the signature depends on
                                   the char8_t type
                             'f' - indicates that the signature depends on
                                   128-bit floating-point types
                             'i' - indicates that the signature depends on
                                   128-bit integer types
                             'v' - the signature depends on vector types
                           If a function with restrictions is referenced, a
                           check is made to ensure that all restrictions are
                           satisfied (otherwise an error is given). */
  a_builtin_type_string
                type_string;
                        /* The type of the builtin function(s).  This takes
                           the form of a C declaration, e.g., a function
                           taking an int argument and returning a float would
                           be: "float (int)". */
  a_builtin_function_kind
                kind;
                        /* A unique identifier for this builtin function.  It
                           should be an enumeration value from
                           a_builtin_function_kind_tag or
                           a_builtin_user_function_kind. */
} a_builtin_user_descr;


using a_builtin_type_index = unsigned short;
                        /* The type representing an index of a builtin type.
                           The indexed value is the same index in
                           builtin_type_strings. */

using a_builtin_type_map = Ptr_map<a_builtin_type_index, a_type_ptr>;
                        /* The type used for mapping between the string
                           representation of a builtin function type and its
                           internal type representation. */

EXTERN_THREAD a_builtin_type_map
                *builtin_type_table;
                        /* A dynamically-allocated mapping of builtin type
                           indexes to type IL entries.

                           Entries containing the C representations of each
                           builtin function's type are automatically generated
                           by an external tool, then, if referenced, the
                           internal type is generated by invoking the parser on
                           the string.

                           If the associated type is non-NULL, the type
                           represents the internal representation of the
                           routine type specified by the corresponding entry in
                           builtin_type_strings.  In cases where the function
                           type has attributes, this can be a tk_typeref (which
                           records those attributes).  */

/*
Mapping between the string representation of a condition string and its
internal representation.  Entries containing the C representations of each
builtin function's type are automatically generated by an external tool, then,
if referenced, the internal type is generated by invoking the parser on the
string.  Corresponds to the same index in builtin_condition_strings.
*/
typedef struct a_builtin_function_condition {
  a_const_char  *restrictions;
                        /* If non-NULL, points to any "restrictions" for this
                           builtin function.  See the description of "cond" in
                           a_builtin_user_descr for more information. */
  a_boolean     evaluated;
                        /* If TRUE, this condition string has been evaluated
                           (and hence the flags that follow have been set
                           appropriately). */
  a_boolean     primary_enabled;
                        /* If TRUE, this condition string indicates that any
                           primary builtin declarations that use it are enabled
                           in the current configuration. */
  a_boolean     secondary_enabled;
                        /* If TRUE, this condition string indicates that any
                           secondary builtin declarations that use it are
                           enabled in the current configuration. */
} a_builtin_function_condition;

EXTERN_THREAD a_builtin_function_condition
                *builtin_condition_table;
                        /* A dynamically-allocated array of
                           a_builtin_function_condition entries that
                           corresponds to the condition strings in
                           builtin_condition_strings. */

/*
Data structure describing the name and signature of a builtin function, as well
as the modes in which the builtin function should be recognized.  These entries
are automatically generated by an external tool.
*/
typedef struct a_builtin_descr {
  a_const_char  *name;
                        /* The name of the builtin function. */
  unsigned short
                cond_index;
                        /* An index into builtin_condition_table that gives the
                           condition string for the builtin function.  See the
                           comment for the "cond" field in a_builtin_user_descr
                           for a full explanation. */
  unsigned short
                type_index;
                        /* An index into builtin_type_table that specifies
                           the type of the builtin function(s) for this
                           entry. */
  a_builtin_function_kind
                kind;
                        /* An indicator of which builtin function (for use
                           within the front end).  Note that the same value is
                           used for cases where an entry refers to two symbols
                           (i.e., because an 'S' is present in the condition
                           string indicating that there are primary and
                           secondary versions of the builtin function). */
} a_builtin_descr;

/*
Include the GCC/clang/Microsoft builtins that have been automatically
generated by an external tool.
*/
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include "builtin_kinds.h"
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */

/*
An enumeration of unique user-defined builtin functions.  This is effectively
a continuation of the automatically-generated a_builtin_function_kind_tag
enumeration.
*/
enum a_builtin_user_function_kind {
  bufk_is_constant_evaluated = (int)bfk_is_constant_evaluated,
                                  /* __builtin_is_constant_evaluated --
                                     for backward compatibility only. */
  bufk_first = (int)bfk_last,     /* initial entry */
  bufk_choose_expr,               /* __builtin_choose_expr */
  bufk_launder,                   /* __builtin_launder */
  bufk_u8memchr,                  /* __builtin_u8memchr */
  bufk_u8memcmp,                  /* __builtin_u8memcmp */
  bufk_u8strlen,                  /* __builtin_u8strlen */
  bufk_FUNCSIG,                   /* __builtin_FUNCSIG */
  bufk_FILE_NAME,                 /* __builtin_FILE_NAME */
  bufk_eh_ptr_adjust_ref,         /* __builtin_eh_ptr_adjust_ref */
  bufk_current_exception,         /* __builtin_current_exception */
  bufk_uncaught_exceptions,       /* __builtin_uncaught_exceptions */
  bufk_last                       /* final entry */
};

/*
This table contains entries for "manually added" builtin functions.  The
builtin functions described in builtin_table are automatically generated by
an external tool; this table should be used for builtin functions that are
not captured by the external tool, or are added by the customer.

This table may also be used to "override" signatures and/or condition entries
for builtins whose entries in the automatically-generated table are found
to be incorrect (i.e., a matching entry here will prevent the loading of the
same entry in builtin_table).

Note that any entries in this table are lazily loaded, i.e., they are
loaded into the symbol table for each translation unit, but a routine entry
(and associated symbol) are not created until the builtin function is
referenced.  If the builtin function must be defined each time, a different
mechanism (e.g., calling enter_builtin_function directly) should be used.

Note also that the ordering of this table is arbitrary (i.e., it does not need
to be kept sorted).
*/
/*lint -e641 */ /* Suppress lint messages about converting enums to int. */
EXTERN a_builtin_user_descr builtin_user_table[]
#if VAR_INITIALIZERS
= {
  /* libstdc++ implements std::source_location using an intrinsic function
     "__builtin_source_location". */
  { "__builtin_source_location", "L+(150000-)g+(100000-)s+(202002-)",
    "const void* () __edg_throw__()", bfk_source_location },

  /* GCC 9.x implements std::is_constexpr_evaluated using an intrinsic function
     __builtin_is_constexpr_evaluated.  We accept it in all modes, but the
     front end also recognizes std::is_constexpr_evaluated directly. */
  { "__builtin_is_constant_evaluated",
    "g+(90000-)L+(90000-)s+(202002-)m+(1925-)",
    "bool () __edg_throw__()", bfk_is_constant_evaluated },

  /* __builtin_launder is "magical" in that it implicitly produces a return
     type matching the argument type.  Note that this version supersedes
     the version defined in builtin_defs.h (if any). */
  { "__builtin_launder", "g+(70100-)mx(1914-)L+(80000-)", "void* (void*)",
    bufk_launder },

  /* __builtin_choose_expr is available in all gcc modes. */
  { "__builtin_choose_expr", "gcLc", "int (...)", bufk_choose_expr },

  /* __builtin_va_arg is not picked up by the automatic tools because it
     is implemented as a keyword by GCC and clang. */
  { "__builtin_va_arg", "gx(40500-)Lx", "void (...)", bfk_va_arg },

  /* __builtin_stdarg_start is also not picked up for GCC. */
  { "__builtin_stdarg_start", "gx(40500-)", "void (...)", bfk_stdarg_start },

  /* Manually add the size-specific versions of the __atomic builtins for
     clang.  These entries were copied from the corresponding automatically-
     generated GCC entries. */
  { "__atomic_add_fetch_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_add_fetch_1 },
  { "__atomic_add_fetch_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_add_fetch_16 },
  { "__atomic_add_fetch_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_add_fetch_2 },
  { "__atomic_add_fetch_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_add_fetch_4 },
  { "__atomic_add_fetch_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_add_fetch_8 },
  { "__atomic_add_fetch_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_add_fetch_8 },
  { "__atomic_and_fetch_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_and_fetch_1 },
  { "__atomic_and_fetch_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_and_fetch_16 },
  { "__atomic_and_fetch_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_and_fetch_2 },
  { "__atomic_and_fetch_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_and_fetch_4 },
  { "__atomic_and_fetch_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_and_fetch_8 },
  { "__atomic_and_fetch_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_and_fetch_8 },
  { "__atomic_compare_exchange_1", "Lx", "__edg_bool_type__ (volatile void*,void*,unsigned char,__edg_bool_type__,int,int)", bfk_atomic_compare_exchange_1 },
  { "__atomic_compare_exchange_16", "Lx[i]", "__edg_bool_type__ (volatile void*,void*,__uint128_t,__edg_bool_type__,int,int)", bfk_atomic_compare_exchange_16 },
  { "__atomic_compare_exchange_2", "Lx", "__edg_bool_type__ (volatile void*,void*,unsigned short,__edg_bool_type__,int,int)", bfk_atomic_compare_exchange_2 },
  { "__atomic_compare_exchange_4", "Lx", "__edg_bool_type__ (volatile void*,void*,unsigned,__edg_bool_type__,int,int)", bfk_atomic_compare_exchange_4 },
  { "__atomic_compare_exchange_8", "Lx4", "__edg_bool_type__ (volatile void*,void*,unsigned long long,__edg_bool_type__,int,int)", bfk_atomic_compare_exchange_8 },
  { "__atomic_compare_exchange_8", "Lx8", "__edg_bool_type__ (volatile void*,void*,unsigned long,__edg_bool_type__,int,int)", bfk_atomic_compare_exchange_8 },
  { "__atomic_exchange_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_exchange_1 },
  { "__atomic_exchange_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_exchange_16 },
  { "__atomic_exchange_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_exchange_2 },
  { "__atomic_exchange_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_exchange_4 },
  { "__atomic_exchange_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_exchange_8 },
  { "__atomic_exchange_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_exchange_8 },
  { "__atomic_fetch_add_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_fetch_add_1 },
  { "__atomic_fetch_add_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_fetch_add_16 },
  { "__atomic_fetch_add_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_fetch_add_2 },
  { "__atomic_fetch_add_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_fetch_add_4 },
  { "__atomic_fetch_add_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_fetch_add_8 },
  { "__atomic_fetch_add_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_fetch_add_8 },
  { "__atomic_fetch_and_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_fetch_and_1 },
  { "__atomic_fetch_and_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_fetch_and_16 },
  { "__atomic_fetch_and_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_fetch_and_2 },
  { "__atomic_fetch_and_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_fetch_and_4 },
  { "__atomic_fetch_and_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_fetch_and_8 },
  { "__atomic_fetch_and_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_fetch_and_8 },
  { "__atomic_fetch_nand_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_fetch_nand_1 },
  { "__atomic_fetch_nand_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_fetch_nand_16 },
  { "__atomic_fetch_nand_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_fetch_nand_2 },
  { "__atomic_fetch_nand_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_fetch_nand_4 },
  { "__atomic_fetch_nand_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_fetch_nand_8 },
  { "__atomic_fetch_nand_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_fetch_nand_8 },
  { "__atomic_fetch_or_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_fetch_or_1 },
  { "__atomic_fetch_or_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_fetch_or_16 },
  { "__atomic_fetch_or_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_fetch_or_2 },
  { "__atomic_fetch_or_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_fetch_or_4 },
  { "__atomic_fetch_or_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_fetch_or_8 },
  { "__atomic_fetch_or_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_fetch_or_8 },
  { "__atomic_fetch_sub_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_fetch_sub_1 },
  { "__atomic_fetch_sub_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_fetch_sub_16 },
  { "__atomic_fetch_sub_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_fetch_sub_2 },
  { "__atomic_fetch_sub_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_fetch_sub_4 },
  { "__atomic_fetch_sub_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_fetch_sub_8 },
  { "__atomic_fetch_sub_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_fetch_sub_8 },
  { "__atomic_fetch_xor_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_fetch_xor_1 },
  { "__atomic_fetch_xor_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_fetch_xor_16 },
  { "__atomic_fetch_xor_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_fetch_xor_2 },
  { "__atomic_fetch_xor_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_fetch_xor_4 },
  { "__atomic_fetch_xor_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_fetch_xor_8 },
  { "__atomic_fetch_xor_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_fetch_xor_8 },
  { "__atomic_load_1", "Lx", "unsigned char (const volatile void*,int)", bfk_atomic_load_1 },
  { "__atomic_load_16", "Lx[i]", "__uint128_t (const volatile void*,int)", bfk_atomic_load_16 },
  { "__atomic_load_2", "Lx", "unsigned short (const volatile void*,int)", bfk_atomic_load_2 },
  { "__atomic_load_4", "Lx", "unsigned (const volatile void*,int)", bfk_atomic_load_4 },
  { "__atomic_load_8", "Lx4", "unsigned long long (const volatile void*,int)", bfk_atomic_load_8 },
  { "__atomic_load_8", "Lx8", "unsigned long (const volatile void*,int)", bfk_atomic_load_8 },
  { "__atomic_nand_fetch_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_nand_fetch_1 },
  { "__atomic_nand_fetch_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_nand_fetch_16 },
  { "__atomic_nand_fetch_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_nand_fetch_2 },
  { "__atomic_nand_fetch_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_nand_fetch_4 },
  { "__atomic_nand_fetch_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_nand_fetch_8 },
  { "__atomic_nand_fetch_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_nand_fetch_8 },
  { "__atomic_or_fetch_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_or_fetch_1 },
  { "__atomic_or_fetch_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_or_fetch_16 },
  { "__atomic_or_fetch_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_or_fetch_2 },
  { "__atomic_or_fetch_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_or_fetch_4 },
  { "__atomic_or_fetch_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_or_fetch_8 },
  { "__atomic_or_fetch_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_or_fetch_8 },
  { "__atomic_store_1", "Lx", "void (volatile void*,unsigned char,int)", bfk_atomic_store_1 },
  { "__atomic_store_16", "Lx[i]", "void (volatile void*,__uint128_t,int)", bfk_atomic_store_16 },
  { "__atomic_store_2", "Lx", "void (volatile void*,unsigned short,int)", bfk_atomic_store_2 },
  { "__atomic_store_4", "Lx", "void (volatile void*,unsigned,int)", bfk_atomic_store_4 },
  { "__atomic_store_8", "Lx4", "void (volatile void*,unsigned long long,int)", bfk_atomic_store_8 },
  { "__atomic_store_8", "Lx8", "void (volatile void*,unsigned long,int)", bfk_atomic_store_8 },
  { "__atomic_sub_fetch_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_sub_fetch_1 },
  { "__atomic_sub_fetch_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_sub_fetch_16 },
  { "__atomic_sub_fetch_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_sub_fetch_2 },
  { "__atomic_sub_fetch_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_sub_fetch_4 },
  { "__atomic_sub_fetch_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_sub_fetch_8 },
  { "__atomic_sub_fetch_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_sub_fetch_8 },
  { "__atomic_xor_fetch_1", "Lx", "unsigned char (volatile void*,unsigned char,int)", bfk_atomic_xor_fetch_1 },
  { "__atomic_xor_fetch_16", "Lx[i]", "__uint128_t (volatile void*,__uint128_t,int)", bfk_atomic_xor_fetch_16 },
  { "__atomic_xor_fetch_2", "Lx", "unsigned short (volatile void*,unsigned short,int)", bfk_atomic_xor_fetch_2 },
  { "__atomic_xor_fetch_4", "Lx", "unsigned (volatile void*,unsigned,int)", bfk_atomic_xor_fetch_4 },
  { "__atomic_xor_fetch_8", "Lx4", "unsigned long long (volatile void*,unsigned long long,int)", bfk_atomic_xor_fetch_8 },
  { "__atomic_xor_fetch_8", "Lx8", "unsigned long (volatile void*,unsigned long,int)", bfk_atomic_xor_fetch_8 },

#if USE_X86_FUNCTION_MULTIVERSIONING && DO_IL_LOWERING
  /* Early versions of clang don't define these.  When performing function
     multiversioning lowering, they are required (and must therefore be
     implemented by a back end in clang mode). */
  { "__builtin_cpu_init", "Lx(-59999)", "int (void)", bfk_cpu_init },
  { "__builtin_cpu_is", "Lx(-59999)", "int (const char*)", bfk_cpu_is },
  { "__builtin_cpu_supports", "Lx(-30699)", "__edg_bool_type__ (const char*)",
    bfk_cpu_supports },
#endif /* USE_X86_FUNCTION_MULTIVERSIONING && DO_IL_LOWERING */

  /* Builtins used by Microsoft for char_traits<char8_t> intrinsics. */
  { "__builtin_u8memchr", "m+(1922-)[c]",
    "char8_t* (const char8_t*,int,__edg_size_type__)", bufk_u8memchr },
  { "__builtin_u8memcmp", "m+(1922-)[c]",
    "int (const char8_t*,const char8_t*,__edg_size_type__)", bufk_u8memcmp },
  { "__builtin_u8strlen", "m+(1922-)[c]",
    "__edg_size_type__ (const char8_t*)", bufk_u8strlen },

  /* Clang defines these generic builtins as having void return type but
     their return type is always bool. */
  { "__builtin_add_overflow", "Lx", "__edg_bool_type__ (...)",
    bfk_add_overflow },
  { "__builtin_mul_overflow", "Lx", "__edg_bool_type__ (...)",
    bfk_mul_overflow },
  { "__builtin_sub_overflow", "Lx", "__edg_bool_type__ (...)",
    bfk_sub_overflow },

  /* Clang and GCC 15+ support at least some of the overloaded versions of
     __builtin_operator_new and __builtin_operator_delete, but the signatures
     for these are missing in GCC and contain only a single operator in Clang.
     Add an ellipsis to allow parsing of additional arguments.  Note that there
     is special processing for these builtins -- they are essentially processed
     as their corresponding global operator delete/new operators (so they can
     be overloaded). */
  { "__builtin_operator_delete", "L+(-39999)",
    "void (void*,...)", bfk_operator_delete },
  { "__builtin_operator_delete", "g+(150000-)L+(40000-)",
    "void (void*,...) __edg_throw__()", bfk_operator_delete },
  { "__builtin_operator_new", "g+(150000-)L+", "void* (__edg_size_type__,...)",
    bfk_operator_new },

  /* Clang supports these builtin functions (but they are not reported as
     typical builtins, so they're manually added here). */
  { "__builtin_COLUMN",    "Lx(90000-)s+(202002-)m+(1927-)",
    "int (void) __edg_throw__()", bfk_COLUMN},
  { "__builtin_LINE",      "Lx(90000-)s+(202002-)m+(1927-)",
    "int (void) __edg_throw__()", bfk_LINE },
  { "__builtin_FILE",      "Lx(90000-)s+(202002-)m+(1927-)",
    "const char*(void) __edg_throw__()", bfk_FILE },
  { "__builtin_FILE_NAME", "Lx(170000-)",
    "const char*(void) __edg_throw__()", bufk_FILE_NAME },
  { "__builtin_FUNCTION",  "Lx(90000-)s+(202002-)m+(1927-)",
    "const char*(void) __edg_throw__()", bfk_FUNCTION },

  /* __builtin_vectorelements is not picked up for all Clang versions. */
  { "__builtin_vectorelements", "Lx(180000-)",
    "__edg_size_type__ (...) __edg_throw__()", bfk_vectorelements },

  /* Microsoft supports __builtin_FUNCSIG (which returns the same as their
     __FUNCSIG__ macro) beginning with version 19.35. */
  { "__builtin_FUNCSIG", "m+(1935-)",
    "const char*(void) __edg_throw__()", bufk_FUNCSIG },

  /* The GCC C++ standard library uses these builtin functions to implement
     std::exception_ptr during constant evaluation.  A call of
     __builtin_current_exception or __builtin_uncaught_exceptions is replaced
     by a call of std::current_exception or std::uncaught_exceptions,
     respectively, when that function is declared, but a back end still
     generates the builtin (see builtin_call_needs_adjustment). */
  { "__builtin_eh_ptr_adjust_ref", "g+(160000-)",
    "void (void*,int) __edg_throw__()", bufk_eh_ptr_adjust_ref },
  { "__builtin_current_exception", "g+(160000-)",
    "void* (void) __edg_throw__()", bufk_current_exception },
  { "__builtin_uncaught_exceptions", "g+(160000-)",
    "int (void) __edg_throw__()", bufk_uncaught_exceptions },

  { NULL, NULL, 0, bfk_none }   /* end of table marker */
}
#endif /* VAR_INITIALIZERS */
;
/*lint +e641 */ /* Re-enable lint messages about converting enums to int. */
extern a_symbol_ptr load_matching_builtin_function(a_symbol_header *sym_hdr);

extern void load_matching_builtin_function_by_name(a_const_char *name);

extern a_boolean builtin_function_or_keyword_is_enabled(a_const_char *name);

extern a_boolean builtin_needs_to_be_loaded_in_secondary_translation_unit(
                                                     a_symbol_header *sym_hdr);

extern void mark_builtin_loaded_in_secondary_translation_unit(
                                                     a_symbol_header *sym_hdr);

extern void load_overloadable_builtin_symbols(a_builtin_function_category bfc);

#endif /* BUILTIN_FUNCTIONS_ENABLED */

extern a_type_ptr get_default_va_list_type(void);

extern void enter_system_specific_predeclared_symbols(void);

extern void enter_system_specific_predefined_macros_and_assertions(void);

#if GNU_EXTENSIONS_ALLOWED
#if USE_X86_FUNCTION_MULTIVERSIONING
/*
This enumeration lists the valid CPU and Instruction Set Architectures (ISAs)
for the Intel/AMD line of processors and is used for the GNU function
multiversioning feature (as specified by the "target" attribute).  The ordering
of the list is important: CPU architectures are first, then "default", then the
ISA architectures (in the order specified in the GCC Function Multiversioning
Wiki).  When an entry is added here, the target_distinction table must also be
updated.  If an ISA entry is added here, an entry must also be added to
isa_alphabetic_order.

The "target" attributes listed below correspond to those that GCC appears
to use in its resolver functions.  All other "target" attributes are mapped
to mvak_unknown and a warning is issued (which is suppressed in system
headers).  An error is issued if an mvak_unknown routine is needed by a
lowering-created resolver routine.
*/
enum a_multiversion_arch_kind : signed char {
  mvak_invalid = -1,                /* An invalid entry. */
  mvak_unknown = 0,                 /* Not otherwise on this list. */
  mvak_lowest_cpu,                  /* Lowest CPU architecture entry. */
  /* CPU architectures: */
  mvak_cpu_bdver1 = mvak_lowest_cpu,
  mvak_cpu_bdver2,
  mvak_cpu_corei7,
  mvak_cpu_amdfam10h,
  mvak_cpu_core2,
  mvak_cpu_atom,
  mvak_highest_cpu = mvak_cpu_atom, /* Highest CPU architecture entry. */
  mvak_default_target,              /* Default entry.*/
  mvak_lowest_isa,                  /* Marks first ISA entry. */
  /* ISA architectures: */
  mvak_isa_mmx = mvak_lowest_isa,
  mvak_isa_sse,
  mvak_isa_sse2,
  mvak_isa_sse3,
  mvak_isa_ssse3,
  mvak_isa_sse4,
  mvak_isa_sse4a,
  mvak_isa_sse4_1,
  mvak_isa_sse4_2,
  mvak_isa_popcnt,
  mvak_isa_aes,
  mvak_isa_pclmul,
  mvak_isa_avx,
  mvak_isa_bmi,
  mvak_isa_fma4,
  mvak_isa_xop,
  mvak_isa_fma,
  mvak_isa_bmi2,
  mvak_isa_avx2,
  mvak_isa_avx512f,
  mvak_highest_isa = mvak_isa_avx512f, /* Marks last ISA entry. */
  mvak_last                         /* Must be last. */
};

/*
Macro that returns TRUE if the specified architecture corresponds to a
CPU architecture.
*/
#define is_mv_cpu_arch(t)                                                     \
  ((t) >= (a_multiversion_arch_kind)mvak_lowest_cpu &&                        \
   (t) <= (a_multiversion_arch_kind)mvak_highest_cpu)

/*
Macro that returns TRUE if a CPU architecture is specified in a bitset.
*/
#define is_any_mv_arch_bit_set(bs)                                            \
  (((bs) & (((a_mv_target_bitset)1 <<                                         \
             ((a_multiversion_arch_kind)mvak_highest_cpu + 1)) - 1)) != 0)

/*
Macro that returns TRUE if the specified bitset indicates the "default"
routine.
*/
#define is_default_targ_bitset(bs)                                            \
  ((bs) == (a_mv_target_bitset)1 <<                                           \
           (a_multiversion_arch_kind)mvak_default_target)

/*
Macro that returns TRUE if the specified bitset indicates an unknown target
attribute.
*/
#define is_unknown_targ_bitset(bs)                                            \
  ((bs) & (a_mv_target_bitset)1 <<                                           \
          (a_multiversion_arch_kind)mvak_unknown)

/*
Macro that returns TRUE if the specific-target routine is the "default"
routine.
*/
#define is_mv_default_routine(rp)                                             \
 (has_gnu_routine_supp(rp) &&                                                 \
  is_default_targ_bitset(                                                     \
                 (rp)->gnu_extra_info->mv_info.targeted_version.target_bitset))

/*
Macro that takes a representative routine and returns TRUE in the special
case when there is exactly one target-specific routine on the list.
*/
#define has_exactly_one_target_specific_routine(rp)                           \
  (has_gnu_routine_supp(rp) &&                                                \
   (rp)->gnu_extra_info->mv_info.representative.targeted_versions != NULL &&  \
   (rp)->gnu_extra_info->mv_info.representative.targeted_versions->next== NULL)

/*
Macro that takes a representative routine and returns TRUE if there is
a "default" routine on the list (which will be the first routine on the list).
*/
#define has_mv_default_routine(rp)                                            \
  (has_gnu_routine_supp(rp) &&                                                \
   (rp)->gnu_extra_info->mv_info.representative.targeted_versions != NULL &&  \
   is_mv_default_routine((rp)->gnu_extra_info->                               \
                            mv_info.representative.targeted_versions->routine))

#if DO_IL_LOWERING
extern a_const_char *target_name_for_builtin(a_multiversion_arch_kind arch);
#endif /* DO_IL_LOWERING */

extern void reference_to_mv_routine(a_routine_ptr      routine,
                                    a_source_position  *error_pos);

extern a_routine_ptr find_mv_target_specific_routine(
                                            a_routine_ptr routine,
                                            a_routine_ptr surrounding_routine);

#endif /* USE_X86_FUNCTION_MULTIVERSIONING */

#if GNU_FUNCTION_MULTIVERSIONING
extern a_routine_ptr find_existing_mv_routine(
                                       a_routine_ptr representative_routine,
                                       a_routine_ptr candidate,
                                       an_attribute_arg_ptr aap);

extern void add_to_specific_version_list(a_routine_ptr representative_routine,
                                         a_routine_ptr target_routine);

extern a_const_char *target_specific_distinction(a_routine_ptr routine);
#endif /* GNU_FUNCTION_MULTIVERSIONING */

extern void validate_target_argument(a_const_char         *str,
                                     size_t               str_len,
                                     an_attribute_arg_ptr aap,
                                     a_routine_ptr        routine,
                                     a_boolean            *error_issued);

#if GNU_VECTOR_TYPES_ALLOWED
extern void enter_arm_32_mve_predeclared_types(a_source_position *decl_pos);
extern void enter_arm_64_acle_predeclared_types(a_source_position *decl_pos);
extern void enter_arm_64_neon_predeclared_types(a_source_position *decl_pos);
extern void enter_arm_64_sve_predeclared_types(a_source_position *decl_pos);
extern void enter_riscv_vector_predeclared_types(a_source_position *decl_pos);

extern a_const_char *get_predefined_name_for_neon_vector_type(
                                                a_type_ptr     element_type,
                                                a_targ_size_t  vector_elements,
                                                a_vector_kind  vector_kind);
extern a_const_char *get_predefined_name_for_builtin_neon_vector_type(
                                               a_type_ptr     element_type,
                                               a_targ_size_t  vector_elements);
#endif /* GNU_VECTOR_TYPES_ALLOWED */

#endif /* GNU_EXTENSIONS_ALLOWED */

extern a_boolean check_availability_attr(an_attribute_ptr ap);

extern void sys_predef_trans_unit_init(void);

extern void sys_predef_one_time_init(void);

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef SYS_PREDEF_H */


