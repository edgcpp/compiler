/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

targ_def.h -- Definition of target machine characteristics.  See <limits.h>
              and standard, sec. 2.2.4.2, for related information.

Note that targ_def.h contains configuration values that are incorporated
when the compiler is built (#define values, typedefs), whereas target.h
declares configuration variables that, in principle, can be reset whenever
the compiler is invoked.

*/

/* Avoid including these declarations more than once: */
#ifndef TARG_DEF_H
#define TARG_DEF_H 1

#if __ANSIC__
/* Include float.h to get the definition of things like FLT_MANT_DIG, etc. */
#include <float.h>
#endif /* __ANSIC__ */

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* !defined(HOST_ENVIR_H) */
#ifndef LANG_FEAT_H
#include "lang_feat.h"
#endif /* !defined(LANG_FEAT_H) */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Flag used to retain ABI (Application Binary Interface, i.e., runtime layout
and calling sequence) compatibility with older versions.  The value is the
version number of the EDG C++ front end, e.g., 227 for version 2.27, for
which compatibility should be maintained.  ABI changes made after that
version will be suppressed.  Of course, that may suppress certain language
features that cannot be implemented without the corresponding ABI changes.
Note that even if the ABI version is set to newer version numbers,
if CFRONT_OBJECT_CODE_COMPATIBILITY is TRUE certain language features
will be turned off.  Those features (e.g., RTTI) can be turned on
explicitly, and the front end will work, but you will have a version
with an ABI that is only cfront-like, not cfront-compatible.
*/
#ifndef ABI_COMPATIBILITY_VERSION
#define ABI_COMPATIBILITY_VERSION 9999 /* Latest version. */
#endif /* ifndef ABI_COMPATIBILITY_VERSION */

/* See host_envir.h for
     CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
     CFRONT_3_0_OBJECT_CODE_COMPATIBILITY
     CFRONT_OBJECT_CODE_COMPATIBILITY
     IA64_ABI
*/

#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY && \
    CFRONT_3_0_OBJECT_CODE_COMPATIBILITY
 #error -- Must select either 2.1 compatibility or 3.0 compatibility.
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY ... */

/*
TRUE if code that exploits a cfront 2.1 bug that causes a global name to be
used by a member function when a base class has an entity with the same name.
The conditions under which this bug occurs are quite complicated.  The
full description can be found in lookup.c in the description of
check_for_cfront_name_lookup_bug.  The flag
CFRONT_2_1_OBJECT_CODE_COMPATIBILITY in targ_def.h must be TRUE when this
feature is used.  This is really a language feature and therefore would
be expected to be in lang_feat.h, but if it were there it couldn't
choose a default based on CFRONT_2_1_OBJECT_CODE_COMPATIBILITY.
*/
#ifndef CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
#if CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
#define CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG TRUE
#else /* !CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
#define CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG FALSE
#endif /* CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
#endif /* ifndef CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */

/* The code that implements the cfront name lookup bug makes use of the
   information recorded for semivisible nested type handling.  Consequently,
   2.1 compatibility mode is required to use the name lookup bug. */
#if !CFRONT_2_1_OBJECT_CODE_COMPATIBILITY && \
    CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG
 #error -- cfront name lookup bug support requires cfront 2.1 compatibility
#endif /* CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG */

#if IA64_ABI
/*
The name of the macro to be defined when IA-64 ABI is used on the target.
This is only used when IA64_ABI is TRUE.
*/
#ifndef MACRO_DEFINED_WHEN_IA64_ABI
#define MACRO_DEFINED_WHEN_IA64_ABI "__EDG_IA64_ABI"
#endif /* ifndef MACRO_DEFINED_WHEN_IA64_ABI */
#endif /* IA64_ABI */

/*
TRUE if code should be generated to call the runtime guard
acquire/release/abort routines in initializations of local
static variables.  If the flag is FALSE, the guard variables
are tested/set by inline code.  TRUE allows a thread-safe
solution in the runtime.
*/
#ifndef TARG_IA64_ABI_USE_GUARD_ACQUIRE_RELEASE
#ifdef IA64_ABI_USE_GUARD_ACQUIRE_RELEASE
#define TARG_IA64_ABI_USE_GUARD_ACQUIRE_RELEASE \
        IA64_ABI_USE_GUARD_ACQUIRE_RELEASE
#else /* !defined(IA64_ABI_USE_GUARD_ACQUIRE_RELEASE) */
#define TARG_IA64_ABI_USE_GUARD_ACQUIRE_RELEASE TRUE
#endif /* ifndef IA64_ABI_USE_GUARD_ACQUIRE_RELEASE */
#endif /* ifndef TARG_IA64_ABI_USE_GUARD_ACQUIRE_RELEASE */

/*
If a specific target architecture has not been explicitly selected, select one
that looks like the host compiler.  This is somewhat arbitrary (but can be
overridden by explicitly selecting a target architecture).  If none of these
macros have been set, the implicit assumption is that the target is a 32-bit
x86 target.
*/
#if !defined(TARG_SUPPORTS_X86_64) && !defined(TARG_SUPPORTS_ARM64) && \
    !defined(TARG_SUPPORTS_ARM32) && !defined(TARG_SUPPORTS_RISCV64) && \
    !defined(TARG_SUPPORTS_RISCV32)
#if defined(__x86_64) || defined(__x86_64__)
#define TARG_SUPPORTS_X86_64 TRUE
#elif defined(__aarch64__)
#define TARG_SUPPORTS_ARM64 TRUE
#elif defined(__arm__)
#define TARG_SUPPORTS_ARM32 TRUE
#elif defined(__riscv)
#if defined(__LP64)
#define TARG_SUPPORTS_RISCV64 TRUE
#else /* !defined(__LP64__) */
#define TARG_SUPPORTS_RISCV32 TRUE
#endif /* defined(__LP64__) */
#elif defined(_WIN64)
#define TARG_SUPPORTS_X86_64 TRUE
#endif /* defined(__x86_64) || defined(__x86_64__) */
#endif /* !defined(TARG_SUPPORTS_X86_64) && !defined(TARG_SUPPORTS_ARM32)...*/

/*
Flag that is TRUE if the target is a 64-bit x86 platform.  Provides the initial
setting for targ_supports_x86_64.
*/
#ifndef TARG_SUPPORTS_X86_64
#define TARG_SUPPORTS_X86_64 FALSE
#endif /* ifndef TARG_SUPPORTS_X86_64 */

/*
Flag that is TRUE if the target is a 64-bit ARM platform.  Provides the initial
setting for targ_supports_arm64.
*/
#ifndef TARG_SUPPORTS_ARM64
#define TARG_SUPPORTS_ARM64 FALSE
#endif /* ifndef TARG_SUPPORTS_ARM64 */

/*
Flag that is TRUE if the target is a 32-bit ARM platform.  Provides the initial
setting for targ_supports_arm32.
*/
#ifndef TARG_SUPPORTS_ARM32
#define TARG_SUPPORTS_ARM32 FALSE
#endif /* ifndef TARG_SUPPORTS_ARM32 */

/*
Flag that is TRUE if the target is a 64-bit RISC-V platform.  Provides the
initial setting for targ_supports_riscv64.
*/
#ifndef TARG_SUPPORTS_RISCV64
#define TARG_SUPPORTS_RISCV64 FALSE
#endif /* ifndef TARG_SUPPORTS_RISCV64 */

/*
Flag that is TRUE if the target is a 32-bit RISC-V platform.  Provides the
initial setting for targ_supports_riscv32.
*/
#ifndef TARG_SUPPORTS_RISCV32
#define TARG_SUPPORTS_RISCV32 FALSE
#endif /* ifndef TARG_SUPPORTS_RISCV32 */

#if (TARG_SUPPORTS_X86_64 + TARG_SUPPORTS_ARM64 + TARG_SUPPORTS_ARM32 + \
     TARG_SUPPORTS_RISCV64 + TARG_SUPPORTS_RISCV32) > 1
 #error Only one TARG_SUPPORTS_* configuration macro can be TRUE
#endif /* (TARG_SUPPORTS_X86_64 + TARG_SUPPORTS_ARM64 + TARG_SUPPORTS_ARM32 + \
           TARG_SUPPORTS_RISCV64 + TARG_SUPPORTS_RISCV32) */

#ifdef USE_X86_64
/* USE_X86_64 is now deprecated; set TARG_SUPPORTS_X86_64 appropriately and
   use the targ_supports_x86_64 global variable to check at run-time. */
 #error Use of USE_X86_64 is deprecated; use targ_supports_x86_64 global \
        variable
#endif /* defined(USE_X86_64) */

/*
TRUE to use ARM EABI semantics for static initialization guard variables
(see section 4.4.2 of version 2.02 of the ARM EABI).  The two differences
from the standard IA-64 ABI are: the guard variable is "int"-sized,
and the least significant bit of the guard variable (rather than
the first byte) is used for the guard test.
*/
#ifndef TARG_IA64_ABI_USE_INT_STATIC_INIT_GUARD
#ifdef IA64_ABI_USE_INT_STATIC_INIT_GUARD
#define TARG_IA64_ABI_USE_INT_STATIC_INIT_GUARD \
        IA64_ABI_USE_INT_STATIC_INIT_GUARD
#else /* !defined(IA64_ABI_USE_INT_STATIC_INIT_GUARD) */
#if TARG_SUPPORTS_ARM32 || TARG_SUPPORTS_ARM64
#define TARG_IA64_ABI_USE_INT_STATIC_INIT_GUARD TRUE
#else /* !(TARG_SUPPORTS_ARM32 || TARG_SUPPORTS_ARM64) */
#define TARG_IA64_ABI_USE_INT_STATIC_INIT_GUARD FALSE
#endif /* TARG_SUPPORTS_ARM32 || TARG_SUPPORTS_ARM64 */
#endif /* ifdef IA64_ABI_USE_INT_STATIC_INIT_GUARD */
#endif /* ifndef TARG_IA64_ABI_USE_INT_STATIC_INIT_GUARD */

/*
The name of the macro to be defined when IA-64 guard variables are
"int"-sized instead of "long long"-sized.  Used only when
targ_ia64_abi_use_int_static_init_guard is TRUE.
*/
#ifndef MACRO_DEFINED_WHEN_IA64_USE_INT_STATIC_INIT_GUARD
#define MACRO_DEFINED_WHEN_IA64_USE_INT_STATIC_INIT_GUARD \
			"__EDG_IA64_ABI_USE_INT_STATIC_INIT_GUARD"
#endif /* ifndef MACRO_DEFINED_WHEN_IA64_USE_INT_STATIC_INIT_GUARD */

/*
TRUE to use the variant representation of pointers to member
functions with the IA-64 ABI.  The normal representation
requires an architecture where the address of a function can
never have the low-order bit set (that bit is used to indicate
the virtual function case).  The variant representation moves
the virtual-function bit to the low-order bit of the other
field in the pointer-to-member-function representation.
This is needed, for example, for the ARM architecture.
*/
#ifndef TARG_IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR
#ifdef IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR
#define TARG_IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR \
  IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR
#else /* !defined(IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR) */
#if TARG_SUPPORTS_ARM32 || TARG_SUPPORTS_ARM64
#define TARG_IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR TRUE
#else /* !(TARG_SUPPORTS_ARM32 || TARG_SUPPORTS_ARM64) */
#define TARG_IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR FALSE
#endif /* TARG_SUPPORTS_ARM32 || TARG_SUPPORTS_ARM64 */
#endif /* ifdef IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR */
#endif /* ifndef TARG_IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR */

/*
TRUE to use the variant representation of array cookies with
the IA-64 ABI.  The variant form uses a struct as follows
for the array allocation cookie:
  struct array_cookie {
    std::size_t element_size; // element_size != 0
    std::size_t element_count;
  };
rather than the simple size_t value of the standard IA-64 ABI.
This variant version is used for the ARM architecture.  See
3.2.2.1 in the ARM EABI document.
*/
#ifndef TARG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES
#ifdef IA64_ABI_USE_VARIANT_ARRAY_COOKIES
#define TARG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES \
        IA64_ABI_USE_VARIANT_ARRAY_COOKIES
#else /* !defined(IA64_ABI_USE_VARIANT_ARRAY_COOKIES) */
#if TARG_SUPPORTS_ARM32 || TARG_SUPPORTS_ARM64
#define TARG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES TRUE
#else /* !(TARG_SUPPORTS_ARM32 || TARG_SUPPORTS_ARM64) */
#define TARG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES FALSE
#endif /* TARG_SUPPORTS_ARM32 || TARG_SUPPORTS_ARM64 */
#endif /* ifdef IA64_ABI_USE_VARIANT_ARRAY_COOKIES */
#endif /* ifndef TARG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES */

/*
TRUE to make constructors and destructors return the "this" value
in a variant of the IA-64 ABI.  This is used by the ARM EABI.
Constructors return "pointer to class", and destructors return
"void *", except deleting destructors, which return the standard
"void".
*/
#ifndef TARG_IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS
#ifdef IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS
#define TARG_IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS \
        IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS
#else /* !defined(IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS) */
#if TARG_SUPPORTS_ARM32 || TARG_SUPPORTS_ARM64
#define TARG_IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS TRUE
#else /* !(TARG_SUPPORTS_ARM32 || TARG_SUPPORTS_ARM64) */
#define TARG_IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS FALSE
#endif /* TARG_SUPPORTS_ARM32 || TARG_SUPPORTS_ARM64 */
#endif /* ifdef IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS */
#endif /* ifndef TARG_IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS */

/*
The name of the macro to be defined when constructors and destructors
return "this".  Used only when
targ_ia64_abi_variant_ctors_and_dtors_return_this is TRUE.
*/
#ifndef MACRO_DEFINED_WHEN_IA64_CTORS_DTORS_RETURN_THIS
#define MACRO_DEFINED_WHEN_IA64_CTORS_DTORS_RETURN_THIS \
			"__EDG_IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS"
#endif /* ifndef MACRO_DEFINED_WHEN_IA64_CTORS_DTORS_RETURN_THIS */

/*
TRUE to select the variant rule for determining the key function
(decider function) for virtual function tables in the IA-64 ABI.
See 3.1 in the ARM EABI document.
*/
#ifndef TARG_IA64_ABI_VARIANT_KEY_FUNCTION
#ifdef IA64_ABI_VARIANT_KEY_FUNCTION
#define TARG_IA64_ABI_VARIANT_KEY_FUNCTION IA64_ABI_VARIANT_KEY_FUNCTION
#else /* !defined(IA64_ABI_VARIANT_KEY_FUNCTION) */
#if TARG_SUPPORTS_ARM32 || TARG_SUPPORTS_ARM64
#define TARG_IA64_ABI_VARIANT_KEY_FUNCTION TRUE
#else /* !(TARG_SUPPORTS_ARM32 || TARG_SUPPORTS_ARM64) */
#define TARG_IA64_ABI_VARIANT_KEY_FUNCTION FALSE
#endif /* TARG_SUPPORTS_ARM32 || TARG_SUPPORTS_ARM64 */
#endif /* ifndef IA64_ABI_VARIANT_KEY_FUNCTION */
#endif /* ifndef TARG_IA64_ABI_VARIANT_KEY_FUNCTION */

/*
The early GNU implementations of the IA-64 ABI (e.g., versions 3.2 and 3.3)
had several bugs.  Set the following FLAG to TRUE if those bugs should be
emulated by this implementation.  This is the initial value of the global
variable emulate_gnu_abi_bugs.
*/
#ifndef DEFAULT_EMULATE_GNU_ABI_BUGS
#define DEFAULT_EMULATE_GNU_ABI_BUGS FALSE
#endif /* ifndef DEFAULT_EMULATE_GNU_ABI_BUGS */

/*
Flag that is TRUE if the default GNU ABI version to emulate should be tied to
the version of the GNU C/C++ dialect (indicated by the global variable
gnu_version) being emulated.  If the GNU ABI is emulated but not the GNU C/C++
dialect (i.e., gnu_mode is FALSE), the default GNU ABI version is not affected
by the setting of this macro.  If this macro is set to TRUE, MIN_GNU_VERSION
cannot be smaller than 30200 (since we don't emulate earlier GNU ABIs).  This
option is only available for the IA-64-based ABI.
*/
#ifndef TIE_DEFAULT_GNU_ABI_VERSION_TO_GNU_VERSION
#define TIE_DEFAULT_GNU_ABI_VERSION_TO_GNU_VERSION FALSE
#endif /* ifndef TIE_DEFAULT_GNU_ABI_VERSION_TO_GNU_VERSION */

#if TIE_DEFAULT_GNU_ABI_VERSION_TO_GNU_VERSION
#if (MIN_GNU_VERSION) < 30200
 #error -- MIN_GNU_VERSION must be at least 30200 when \
           TIE_DEFAULT_GNU_ABI_VERSION_TO_GNU_VERSION is TRUE
#endif /* (MIN_GNU_VERSION) < 30200 */
#endif /* TIE_DEFAULT_GNU_ABI_VERSION_TO_GNU_VERSION */

#if TIE_DEFAULT_GNU_ABI_VERSION_TO_GNU_VERSION && !IA64_ABI
 #error -- TIE_DEFAULT_GNU_ABI_VERSION_TO_GNU_VERSION requires that IA64_ABI \
           be TRUE
#endif /* TIE_DEFAULT_GNU_ABI_VERSION_TO_GNU_VERSION && !IA64_ABI */

/*
The GNU C++ version whose IA-64 ABI should be emulated.  This is the initial
value of the global variable gnu_abi_version.  The number is of the form
MMmmss which corresponds to GNU C++ version MM.mm.ss. For example, GNU C++
version 3.3 is 30300 and version 3.2.2 is 30202.  The configured value should
never be less than 30200.  This flag was formerly spelled
DEFAULT_GNU_ABI_BUGS_VERSION: The name was changed because it also affects
the behavior of alignment/packing directives in GNU modes (even when no ABI
bugs are emulated).
*/
#ifndef DEFAULT_GNU_ABI_VERSION
#ifdef DEFAULT_GNU_ABI_BUGS_VERSION
#define DEFAULT_GNU_ABI_VERSION DEFAULT_GNU_ABI_BUGS_VERSION
#else /* !defined(DEFAULT_GNU_ABI_BUGS_VERSION) */
#define DEFAULT_GNU_ABI_VERSION 30200
#endif /* ifdef DEFAULT_GNU_ABI_BUGS_VERSION */
#else /* defined(DEFAULT_GNU_ABI_VERSION) */
#ifdef DEFAULT_GNU_ABI_BUGS_VERSION
#if DEFAULT_GNU_ABI_BUGS_VERSION != DEFAULT_GNU_ABI_VERSION
 #error -- DEFAULT_GNU_ABI_BUGS_VERSION != DEFAULT_GNU_ABI_VERSION \
           DEFAULT_GNU_ABI_BUGS_VERSION is no longer used
#endif /* DEFAULT_GNU_ABI_BUGS_VERSION != DEFAULT_GNU_ABI_VERSION */
#endif /* DEFAULT_GNU_ABI_BUGS_VERSION */
#endif /* ifndef DEFAULT_GNU_ABI_VERSION */

#if DEFAULT_GNU_ABI_VERSION < 30200
 #error -- DEFAULT_GNU_ABI_VERSION must be at least 30200
#endif /* DEFAULT_GNU_ABI_VERSION < 30200 */

/*
Flag that is TRUE if declaring a deleted copy constructor or copy assignment
operator causes the front end to set the corresponding "bitwise copy" flags
(construction_by_bitwise_copy_allowed and assignment_by_bitwise_copy_allowed
in a class symbol supplement) to FALSE.  This can change the argument transfer
method of a function with a parameter of a class type with a deleted copy
constructor (although such a function is not callable using standard code).
*/
#ifndef DELETED_COPY_FUNCTION_CLEARS_BITWISE_COPY_FLAG
#define DELETED_COPY_FUNCTION_CLEARS_BITWISE_COPY_FLAG FALSE
#endif /* defined(DELETED_COPY_FUNCTION_CLEARS_BITWISE_COPY_FLAG) */


/*
Flag that is TRUE if support for exported templates can be enabled.
*/
#ifndef EXPORT_ENABLING_POSSIBLE
#define EXPORT_ENABLING_POSSIBLE FALSE
#endif /* ifndef EXPORT_ENABLING_POSSIBLE */

/*
Export support requires some name mangling features not present in
ABIs older that 2.32.
*/
#if ABI_COMPATIBILITY_VERSION < 232 && EXPORT_ENABLING_POSSIBLE
 #error -- EXPORT_ENABLING_POSSIBLE requires ABI_COMPATIBILITY_VERSION >= 232
#endif /* ABI_COMPATIBILITY_VERSION < 232 && EXPORT_ENABLING_POSSIBLE */

/*
Export cannot be enabled by default if enabling is turned off.
*/
#if DEFAULT_EXPORT_TEMPLATE_ALLOWED && !EXPORT_ENABLING_POSSIBLE
 #error -- DEFAULT_EXPORT_TEMPLATE_ALLOWED requires EXPORT_ENABLING_POSSIBLE 
#endif /* DEFAULT_EXPORT_TEMPLATE_ALLOWED && !EXPORT_ENABLING_POSSIBLE */


/*
Certain C99 and GNU C features require IL constructs not otherwise present.
Because certain back ends may not support the new constructs, a mechanism
is provided to disable the C99 features that require back end support.
The C99_IL_EXTENSIONS_SUPPORTED flag should be TRUE if a back end
is prepared to accept all of the C99 IL extensions (or if C99 IL lowering
will be used; see below).
*/
#ifndef C99_IL_EXTENSIONS_SUPPORTED
#define C99_IL_EXTENSIONS_SUPPORTED TRUE
#endif /* ifndef C99_IL_EXTENSIONS_SUPPORTED */

/*
Flag that is TRUE if compound literals, which look vaguely like a cast
whose source expression is a brace-enclosed initializer (e.g.,
(int []){1, 2, 3}) should be accepted in expressions.  It is the
initial value of the global variable compound_literals_allowed.
*/
#ifndef DEFAULT_COMPOUND_LITERALS_ALLOWED
#define DEFAULT_COMPOUND_LITERALS_ALLOWED FALSE
#endif /* DEFAULT_COMPOUND_LITERALS_ALLOWED */

/*
This switch controls whether support for compound literals (a C99 feature)
can be enabled.  Having this TRUE means the back end is prepared to accept
compound literals, which are represented as enk_temp_init nodes (or that
C99 IL lowering is enabled).  The C-generating and C++-generating back ends
can handle compound literals (but that's useful only if the downstream
compiler also handles them).
*/
#ifndef COMPOUND_LITERAL_ENABLING_POSSIBLE
#if C99_IL_EXTENSIONS_SUPPORTED
#define COMPOUND_LITERAL_ENABLING_POSSIBLE TRUE
#else /* !C99_IL_EXTENSIONS_SUPPORTED */
#define COMPOUND_LITERAL_ENABLING_POSSIBLE FALSE
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#endif /* ifndef COMPOUND_LITERAL_ENABLING_POSSIBLE */
#if !COMPOUND_LITERAL_ENABLING_POSSIBLE && DEFAULT_COMPOUND_LITERALS_ALLOWED
 #error -- compound literal enabling not allowed
#endif /* !COMPOUND_LITERAL_ENABLING_POSSIBLE && ... */

/*
Flag that is TRUE if variable length arrays (VLAs) are allowed.  A VLA is
an array whose size is known only at execution time.  If VLA_ALLOWED is
TRUE, support is enabled and disabled based on DEFAULT_VLA_ENABLED and
command-line options --[no_]vla, which control the global variable
vla_enabled.
*/
#ifndef VLA_ALLOWED
#if C99_IL_EXTENSIONS_SUPPORTED
#define VLA_ALLOWED TRUE
#else /* !C99_IL_EXTENSIONS_SUPPORTED */
#define VLA_ALLOWED FALSE
#endif /* C99_IL_EXTENSIONS_SUPPORTED */
#endif /* ifndef VLA_ALLOWED */

/*
Flag that is TRUE if designators of the form 'x:' and '[expr ... expr]'
should be accepted in aggregate initializers.  This also makes the '='
following an array element designation optional.  It should not be TRUE
if DEFAULT_DESIGNATORS_ALLOWED is FALSE.  It is the initial value
of the global variable extended_designators_allowed.
*/
#ifndef DEFAULT_EXTENDED_DESIGNATORS_ALLOWED
#define DEFAULT_EXTENDED_DESIGNATORS_ALLOWED FALSE
#endif /* DEFAULT_EXTENDED_DESIGNATORS_ALLOWED */

/*
Flag that is TRUE if designators of the form '.x' and '[expr]' should be
accepted in aggregate initializers.  It is the initial value of the global
variable designators_allowed.
*/
#ifndef DEFAULT_DESIGNATORS_ALLOWED
#define DEFAULT_DESIGNATORS_ALLOWED FALSE
#endif /* DEFAULT_DESIGNATORS_ALLOWED */


#ifdef DESIGNATED_INITIALIZER_ENABLING_POSSIBLE
#if !DESIGNATED_INITIALIZER_ENABLING_POSSIBLE
 #error -- the DESIGNATED_INITIALIZER_ENABLING_POSSIBLE macro has been \
 					 eliminated
#endif /* !DESIGNATED_INITIALIZER_ENABLING_POSSIBLE */
#endif /* ifdef DESIGNATED_INITIALIZER_ENABLING_POSSIBLE */

/*
The DO_C99_IL_LOWERING configuration macro is no longer supported.  If set,
verify that it has the same value as DO_IL_LOWERING.
*/
#ifdef DO_C99_IL_LOWERING
#if DO_IL_LOWERING != DO_C99_IL_LOWERING
 #error -- DO_C99_IL_LOWERING must have the same value as DO_IL_LOWERING
#endif /* DO_IL_LOWERING != DO_C99_IL_LOWERING */
#endif /* defined(DO_C99_IL_LOWERING) */

/*
This flag controls whether variable-length arrays (a C99 feature also
available in other modes) are lowered to standard C.  The lowering relies
on facilities in the run-time support library.
*/
#ifndef LOWER_VARIABLE_LENGTH_ARRAYS
#if VLA_ALLOWED && BACK_END_IS_C_GEN_BE
#define LOWER_VARIABLE_LENGTH_ARRAYS TRUE
#else /* !(VLA_ALLOWED && BACK_END_IS_C_GEN_BE) */
#define LOWER_VARIABLE_LENGTH_ARRAYS FALSE
#endif /* VLA_ALLOWED && BACK_END_IS_C_GEN_BE */
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */

#if LOWER_VARIABLE_LENGTH_ARRAYS && !VLA_ALLOWED
 #error -- Lowering of VLAs requires VLA_ALLOWED to be TRUE
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS && !VLA_ALLOWED */
#if LOWER_VARIABLE_LENGTH_ARRAYS && !DO_IL_LOWERING
 #error -- VLAs cannot be lowered without doing C99 or C++ IL lowering
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS && !DO_IL_LOWERING */

/*
This switch controls whether complex and imaginary types and operations
(a C99 and GNU C/C++ feature) are lowered to C89 form.  The lowered form uses
calls to runtime routines to implement complex operations and conversions.
*/
#ifndef LOWER_COMPLEX
#if DO_IL_LOWERING && C99_IL_EXTENSIONS_SUPPORTED
#define LOWER_COMPLEX TRUE
#else /* !(DO_IL_LOWERING && C99_IL_EXTENSIONS_SUPPORTED) */
#define LOWER_COMPLEX FALSE
#endif /* DO_IL_LOWERING && C99_IL_EXTENSIONS_SUPPORTED */
#endif /* ifndef LOWER_COMPLEX */
#if LOWER_COMPLEX && !C99_IL_EXTENSIONS_SUPPORTED
 #error -- LOWER_COMPLEX requires C99_IL_EXTENSIONS_SUPPORTED
#endif /* LOWER_COMPLEX && !C99_IL_EXTENSIONS_SUPPORTED */
#if LOWER_COMPLEX && !DO_IL_LOWERING
 #error -- Complex cannot be lowered without IL lowering
#endif /* LOWER_COMPLEX && !DO_IL_LOWERING */

/*
This switch controls whether fixed-point arithmetic types and operations
are lowered to standard C.  The lowered form uses calls to runtime routines
provided by Dinkumware Ltd. (EDG does not provide them), so the feature
is off by default.
*/
#ifndef LOWER_FIXED_POINT
#define LOWER_FIXED_POINT FALSE
#endif /* ifndef LOWER_FIXED_POINT */
#if LOWER_FIXED_POINT && !DO_IL_LOWERING
 #error -- Fixed point cannot be lowered without doing IL lowering
#endif /* LOWER_FIXED_POINT && !DO_IL_LOWERING */
#if LOWER_FIXED_POINT && !FIXED_POINT_ALLOWED
 #error -- Fixed point cannot be lowered unless fixed point is enabled
#endif /* LOWER_FIXED_POINT && !FIXED_POINT_ALLOWED */

/*
Flag that is TRUE if the eok_class_rvalue_adjust operator should be
rewritten and eliminated by IL lowering.  The operator adjusts the
cv-qualifiers on a class rvalue (e.g., as part of binding a reference).
*/
#ifndef LOWER_CLASS_RVALUE_ADJUST
#if BACK_END_IS_C_GEN_BE
#define LOWER_CLASS_RVALUE_ADJUST TRUE
#else /* !BACK_END_IF_C_GEN_BE */
#define LOWER_CLASS_RVALUE_ADJUST FALSE
#endif /* BACK_END_IS_C_GEN_BE */
#endif /* LOWER_CLASS_RVALUE_ADJUST */
#if LOWER_CLASS_RVALUE_ADJUST && !DO_IL_LOWERING
 #error -- LOWER_CLASS_RVALUE_ADJUST cannot be TRUE if IL lowering is not done
#endif /* LOWER_CLASS_RVALUE_ADJUST && !DO_IL_LOWERING */

/*
In the absence of OS support for dynamic initialization of thread_local
variables, lowering provides such support using "lazy initialization", that is,
each access to a potentially-dynamically-initialized thread_local variable is
surrounded by a guard variable to ensure that the initialization is performed
only once per thread.  For the case of thread_local variables that are local to
a function, a simple thread_local guard variable suffices.  In cases where the
variable is static or external, lowering replaces the reference to the variable
with a call to a wrapper routine that properly initializes the variable and
returns the address of the variable itself.  Re-writing variables in this way
has an obvious performance effect, so if customers have a way to invoke all
thread_local dynamic initializations once when a thread is initially created,
they should use that mechanism and set
USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES to FALSE.
The IA-64 ABI requires USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
be set to TRUE (as do configurations that use the C-generating back end).
See also IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS (if that configuration
macro is FALSE, lowering of thread_local is moot).

The GNU implementation only emits wrapper routines for dynamically-initialized
thread_local variables, but that leads to a violation of the C++11 standard if
a statically-initialized thread_local variable is odr-used before a
dynamically-initialized thread_local variable (because all thread_local
initialization must be performed before any thread_local variable is odr-used,
and the statically-initialized thread_local doesn't have a wrapper which is
used to trigger the initialization of all threads in the translation unit).
See the definition of all_thread_locals_have_wrappers which determines at
run time whether to use the GNU behavior or the standard-compliant behavior.
*/
#ifndef USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
#define USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES \
                   (DO_IL_LOWERING && IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS)
#endif /* USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES */

#if !USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES && DO_IL_LOWERING && \
    IA64_ABI && IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS
 #error -- IA64_ABI requires \
           USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES to be TRUE
#endif /* !USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES && IA64_ABI */
#if !USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES && \
    IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS && BACK_END_IS_C_GEN_BE
 #error -- BACK_END_IS_C_GEN_BE requires \
           USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES to be TRUE
#endif /* !USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES && BACK_... */
#if USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES && \
    !IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS
 #error -- USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES cannot be TRUE \
           if IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS is FALSE
#endif /* USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES && ... */

/*
The IA-64 ABI implementation of lazy initialization for thread_local
variables works best when the back end supports weak references.  For back ends
that don't support weak references, this configuration macro can be
set to TRUE and a do-nothing initialization routine will be emitted for every
thread_local with external linkage.
*/
#ifndef LAZY_INITIALIZATION_USES_WEAK_REFERENCES
#define LAZY_INITIALIZATION_USES_WEAK_REFERENCES GNU_EXTENSIONS_ALLOWED
#endif /* LAZY_INITIALIZATION_USES_WEAK_REFERENCES */

#if DO_IL_LOWERING && USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES
#if LAZY_INITIALIZATION_USES_WEAK_REFERENCES && !GNU_EXTENSIONS_ALLOWED
 #error -- LAZY_INITIALIZATION_USES_WEAK_REFERENCES requires \
           GNU_EXTENSIONS_ALLOWED to be TRUE
#endif /* LAZY_INITIALIZATION_USES_WEAK_REFERENCES && !GNU_EXTENSIONS_ALLOWED*/
#if !LAZY_INITIALIZATION_USES_WEAK_REFERENCES && IA64_ABI
 #error -- IA64_ABI requires LAZY_INITIALIZATION_USES_WEAK_REFERENCES \
           to be TRUE or IMPLEMENTATION_SUPPORTS_MULTIPLE_THREADS to be FALSE
#endif /* !LAZY_INITIALIZATION_USES_WEAK_REFERENCES && IA64_ABI && ... */
#endif /* DO_IL_LOWERING && USE_LAZY_... */

/*
This flag indicates whether the back end is capable of handling C++11
features.  When set to FALSE, the front end is configured such that
C++11 mode, as well as any C++11 feature that requires back end support, cannot
be enabled (therefore back ends and the runtime library do not need to
implement C++11 features).  When TRUE, back ends and the runtime library must
be prepared to deal with any enabled C++11 feature.  The value of this macro is
passed to the runtime library (when --building_runtime is specified) as
__EDG_CPP11_IL_EXTENSIONS_SUPPORTED.  C++11 features that have no back end
(or runtime library) impact are not affected by the setting of this macro.
*/
#ifndef CPP11_IL_EXTENSIONS_SUPPORTED
#define CPP11_IL_EXTENSIONS_SUPPORTED TRUE
#endif /* ifndef CPP11_IL_EXTENSIONS_SUPPORTED */

/*
Flag that is TRUE if the "long long" data type and the associated language
features (e.g., suffixes for constants) are allowed.  "long long" is
standard in C99, and Microsoft mode needs the IL support for __int64.
"long long" is also accepted by GNU C and C++ compilers.

This is really a language feature configuration macro, and as such should
be in lang_feat.h.  However, it affects the IL and requires support
from a back end, and the most sensible default takes into account
whether C99 IL extensions are supported, and that is only known here.
*/
#ifndef LONG_LONG_ALLOWED
#if MICROSOFT_EXTENSIONS_ALLOWED || C99_IL_EXTENSIONS_SUPPORTED || \
    GNU_EXTENSIONS_ALLOWED || CPP11_IL_EXTENSIONS_SUPPORTED
#define LONG_LONG_ALLOWED TRUE
#else /* !(MICROSOFT_EXTENSIONS_ALLOWED || ...) */
#define LONG_LONG_ALLOWED FALSE
#endif /* MICROSOFT_EXTENSIONS_ALLOWED || C99_IL_EXTENSIONS_SUPPORTED || ... */
#endif /* ifndef LONG_LONG_ALLOWED */

#if BUILTIN_FUNCTIONS_ENABLED && !LONG_LONG_ALLOWED
 #error -- BUILTIN_FUNCTIONS_ENABLED requires LONG_LONG_ALLOWED
#endif /* BUILTIN_FUNCTIONS_ENABLED && !LONG_LONG_ALLOWED */

#if MICROSOFT_EXTENSIONS_ALLOWED && !LONG_LONG_ALLOWED
 #error -- MICROSOFT_EXTENSIONS_ALLOWED requires LONG_LONG_ALLOWED
#endif /* MICROSOFT_EXTENSIONS_ALLOWED && !LONG_LONG_ALLOWED */

/*
Flag that is TRUE if signed and unsigned 128-bit integer types can be
enabled in the front end.  Currently, only GNU modes provide a way to refer
to these types in source code (via typedefs __int128_t and __uint128_t,
respectively).
*/
#ifndef INT128_EXTENSIONS_ALLOWED
#define INT128_EXTENSIONS_ALLOWED FALSE
#endif /* ifndef INT128_EXTENSIONS_ALLOWED */

#if INT128_EXTENSIONS_ALLOWED && !LONG_LONG_ALLOWED
 #error -- INT128_EXTENSIONS_ALLOWED requires LONG_LONG_ALLOWED
#endif /* INT128_EXTENSIONS_ALLOWED && !LONG_LONG_ALLOWED */


/*
Flag that is TRUE if C++11 lambdas should be enabled in other C++ modes by
default (they are, of course, always enabled in C++11 mode).  This macro is
used for the initialization of the global variable lambdas_enabled.
*/
#ifndef DEFAULT_LAMBDAS_ENABLED
#if CPP11_IL_EXTENSIONS_SUPPORTED
#define DEFAULT_LAMBDAS_ENABLED FALSE /* Okay to change. */
#else /* !CPP11_IL_EXTENSIONS_SUPPORTED */
#define DEFAULT_LAMBDAS_ENABLED FALSE /* Do not change. */
#endif /* CPP11_IL_EXTENSIONS_SUPPORTED */
#endif /* DEFAULT_LAMBDAS_ENABLED */
#if DEFAULT_LAMBDAS_ENABLED && !CPP11_IL_EXTENSIONS_SUPPORTED
 #error -- Cannot set DEFAULT_LAMBDAS_ENABLED to TRUE when \
           CPP11_IL_EXTENSIONS_SUPPORTED is FALSE
#endif /* DEFAULT_LAMBDAS_ENABLED && !CPP11_IL_EXTENSIONS_SUPPORTED */

/*
Flag that is TRUE if C++11 rvalue references should be enabled in other C++
modes by default (they are, of course, always enabled in C++11 mode).  This
macro is used for the initialization of the global variable
rvalue_references_enabled.
*/
#ifndef DEFAULT_RVALUE_REFERENCES_ENABLED
#if CPP11_IL_EXTENSIONS_SUPPORTED
#define DEFAULT_RVALUE_REFERENCES_ENABLED FALSE /* Okay to change. */
#else /* !CPP11_IL_EXTENSIONS_SUPPORTED */
#define DEFAULT_RVALUE_REFERENCES_ENABLED FALSE /* Do not change. */
#endif /* CPP11_IL_EXTENSIONS_SUPPORTED */
#endif /* DEFAULT_RVALUE_REFERENCES_ENABLED */
#if DEFAULT_RVALUE_REFERENCES_ENABLED && !CPP11_IL_EXTENSIONS_SUPPORTED
 #error -- Cannot set DEFAULT_RVALUE_REFERENCES_ENABLED to TRUE when \
           CPP11_IL_EXTENSIONS_SUPPORTED is FALSE
#endif /* DEFAULT_RVALUE_REFERENCES_ENABLED && ... */

/*
Flag that is TRUE if the C++11 nullptr keyword should be enabled in other
C++ modes by default (it is, of course, always enabled in C++11 mode).
This macro is used for the initialization of the global variable
nullptr_enabled.
*/
#ifndef DEFAULT_NULLPTR_ENABLED
#if CPP11_IL_EXTENSIONS_SUPPORTED
#define DEFAULT_NULLPTR_ENABLED FALSE /* Okay to change. */
#else /* !CPP11_IL_EXTENSIONS_SUPPORTED */
#define DEFAULT_NULLPTR_ENABLED FALSE /* Do not change. */
#endif /* CPP11_IL_EXTENSIONS_SUPPORTED */
#endif /* DEFAULT_NULLPTR_ENABLED */
#if DEFAULT_NULLPTR_ENABLED && !CPP11_IL_EXTENSIONS_SUPPORTED
/* The nullptr feature requires support from the runtime library which is
   only included when CPP11_IL_EXTENSIONS_SUPPORTED is TRUE. */
 #error -- Cannot set DEFAULT_NULLPTR_ENABLED to TRUE when \
           CPP11_IL_EXTENSIONS_SUPPORTED is FALSE
#endif /* DEFAULT_NULLPTR_ENABLED && !CPP11_IL_EXTENSIONS_SUPPORTED */

/*
Flag that is TRUE if the C++11 range-based-for statement should be enabled in
other C++ modes by default (it is, of course, always enabled in C++11 mode).
This macro is used for the initialization of the global variable
range_based_for_enabled.
*/
#ifndef DEFAULT_RANGE_BASED_FOR_ENABLED
#if CPP11_IL_EXTENSIONS_SUPPORTED
#define DEFAULT_RANGE_BASED_FOR_ENABLED FALSE /* Okay to change. */
#else /* !CPP11_IL_EXTENSIONS_SUPPORTED */
#define DEFAULT_RANGE_BASED_FOR_ENABLED FALSE /* Do not change. */
#endif /* CPP11_IL_EXTENSIONS_SUPPORTED */
#endif /* DEFAULT_RANGE_BASED_FOR_ENABLED */
#if DEFAULT_RANGE_BASED_FOR_ENABLED && !CPP11_IL_EXTENSIONS_SUPPORTED && \
    !DO_IL_LOWERING
/* The range-based-for feature requires support from a back end (unless
   lowering is enabled). */
 #error -- Cannot set DEFAULT_RANGE_BASED_FOR_ENABLED to TRUE when \
           CPP11_IL_EXTENSIONS_SUPPORTED is FALSE and DO_IL_LOWERING is FALSE
#endif /* DEFAULT_RANGE_BASED_FOR_ENABLED && !CPP11_IL_EXTENSIONS_... */

/*
The default C++ standard to enable.  The value for DEFAULT_CPP_MODE is the
value of the __cplusplus predefined macro for the desired standard, e.g.,
201103 for C++11, and becomes the default value of the global variable
std_version.
*/
#if defined(DEFAULT_CPP11_MODE) && DEFAULT_CPP11_MODE
/* DEFAULT_CPP11_MODE is no longer used. */
#define DEFAULT_CPP_MODE 201103
#endif /* if defined(DEFAULT_CPP11_MODE) && DEFAULT_CPP11_MODE */
#ifndef DEFAULT_CPP_MODE
#define DEFAULT_CPP_MODE 199711
#endif /* ifndef DEFAULT_CPP_MODE */
#if (DEFAULT_CPP_MODE >= 201103) && !CPP11_IL_EXTENSIONS_SUPPORTED
 #error -- Invalid value for DEFAULT_CPP_MODE when \
           CPP11_IL_EXTENSIONS_SUPPORTED is FALSE
#endif /* (DEFAULT_CPP_MODE >= 201103) && !CPP11_IL_EXTENSIONS_SUPPORTED */
#if (DEFAULT_CPP_MODE != 199711) && \
    (DEFAULT_CPP_MODE != 201103) && \
    (DEFAULT_CPP_MODE != 201402) && \
    (DEFAULT_CPP_MODE != 201703) && \
    (DEFAULT_CPP_MODE != 202002) && \
    (DEFAULT_CPP_MODE != 202302)
 #error -- Invalid value for DEFAULT_CPP_MODE
#endif /* (DEFAULT_CPP_MODE != 199711) && ... */

/*
Flag that is TRUE if the front end should assign sequence numbers to
string literals in routines that might exist in multiple copies in a
program (this is true of inline functions and may also be true of
template instantiations in certain configurations).  This causes the
assignment of a sequence number that can be used to uniquely identify
a given string literal within a function.  The flag is TRUE by default in
the IA-64 ABI.  Sequence numbers are not used by default in the extended
cfront ABI, and should only be used there with caution as they make the use
of string literals in inline functions much more expensive at runtime.
This expense can be avoided in implementations with a true back end,
but only with modifications to IL lowering and support in the back end.
*/
#ifndef ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS
#if IA64_ABI
#define ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS TRUE
#else /* !IA64_ABI */
#define ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS FALSE
#endif /* IA64_ABI */
#endif /* ASSIGN_STRING_LITERAL_SEQUENCE_NUMBERS */

/*
For each floating point data type, specify the number of bits used to
represent the mantissa, and the minimum and maximum exponent values.

In each case, use a previously defined TARG_ value if one is specified.
If none is specified, use the one defined (if any) by the float.h header.
Otherwise, supply a reasonable default value.
*/
#ifndef TARG_FLT_MANT_DIG
#ifdef FLT_MANT_DIG
#define TARG_FLT_MANT_DIG FLT_MANT_DIG
#else /* ifndef FLT_MANT_DIG */
#define TARG_FLT_MANT_DIG 24
#endif /* ifdef FLT_MANT_DIG */
#endif /* ifndef TARG_FLT_MANT_DIG */

#ifndef TARG_FLT_MIN_EXP
#ifdef FLT_MIN_EXP
#define TARG_FLT_MIN_EXP FLT_MIN_EXP
#else /* ifndef FLT_MIN_EXP */
#define TARG_FLT_MIN_EXP (-125)
#endif /* ifdef FLT_MIN_EXP */
#endif /* ifndef TARG_FLT_MIN_EXP */

#ifndef TARG_FLT_MAX_EXP
#ifdef FLT_MAX_EXP
#define TARG_FLT_MAX_EXP FLT_MAX_EXP
#else /* ifndef FLT_MAX_EXP */
#define TARG_FLT_MAX_EXP (128)
#endif /* ifdef FLT_MAX_EXP */
#endif /* ifndef TARG_FLT_MAX_EXP */

#ifndef TARG_DBL_MANT_DIG
#ifdef DBL_MANT_DIG
#define TARG_DBL_MANT_DIG DBL_MANT_DIG
#else /* ifndef DBL_MANT_DIG */
#define TARG_DBL_MANT_DIG 53
#endif /* ifdef DBL_MANT_DIG */
#endif /* ifndef TARG_DBL_MANT_DIG */

#ifndef TARG_DBL_MIN_EXP
#ifdef DBL_MIN_EXP
#define TARG_DBL_MIN_EXP DBL_MIN_EXP
#else /* ifndef DBL_MIN_EXP */
#define TARG_DBL_MIN_EXP (-1021)
#endif /* ifdef DBL_MIN_EXP */
#endif /* ifndef TARG_DBL_MIN_EXP */

#ifndef TARG_DBL_MAX_EXP
#ifdef DBL_MAX_EXP
#define TARG_DBL_MAX_EXP DBL_MAX_EXP
#else /* ifndef DBL_MAX_EXP */
#define TARG_DBL_MAX_EXP (1024)
#endif /* ifdef DBL_MAX_EXP */
#endif /* ifndef TARG_DBL_MAX_EXP */

#ifndef TARG_LDBL_MANT_DIG
#if defined(FP_LONG_DOUBLE_IS_BINARY64) && FP_LONG_DOUBLE_IS_BINARY64
#define TARG_LDBL_MANT_DIG 53
#else /* !(defined(FP_LONG_DOUBLE_IS_BINARY_64) && ...) */
#if defined(FP_LONG_DOUBLE_IS_80BIT_EXTENDED) && \
    FP_LONG_DOUBLE_IS_80BIT_EXTENDED
#define TARG_LDBL_MANT_DIG 64
#else /* !(defined(FP_LONG_DOUBLE_IS_80BIT_EXTENDED) && ...) */
#if defined(FP_LONG_DOUBLE_IS_BINARY128) && FP_LONG_DOUBLE_IS_BINARY128
#define TARG_LDBL_MANT_DIG 113
#else /* !(defined(FP_LONG_DOUBLE_IS_BINARY128) && ...) */
#ifdef LDBL_MANT_DIG
#define TARG_LDBL_MANT_DIG LDBL_MANT_DIG
#else /* ifndef LDBL_MANT_DIG */
#define TARG_LDBL_MANT_DIG 64
#endif /* ifdef LDBL_MANT_DIG */
#endif /* defined(FP_LONG_DOUBLE_IS_BINARY128) && ... */
#endif /* defined(FP_LONG_DOUBLE_IS_80BIT_EXTENDED) && ... */
#endif /* defined(FP_LONG_DOUBLE_IS_BINARY64) && FP_LONG_DOUBLE_IS_BINARY64 */
#endif /* ifndef TARG_LDBL_MANT_DIG */

#ifndef TARG_LDBL_MIN_EXP
#if defined(FP_LONG_DOUBLE_IS_BINARY64) && FP_LONG_DOUBLE_IS_BINARY64
#define TARG_LDBL_MIN_EXP -1021
#else /* !(defined(FP_LONG_DOUBLE_IS_BINARY_64) && ...) */
#if defined(FP_LONG_DOUBLE_IS_80BIT_EXTENDED) && \
    FP_LONG_DOUBLE_IS_80BIT_EXTENDED
#define TARG_LDBL_MIN_EXP -16381
#else /* !(defined(FP_LONG_DOUBLE_IS_80BIT_EXTENDED) && ...) */
#if defined(FP_LONG_DOUBLE_IS_BINARY128) && FP_LONG_DOUBLE_IS_BINARY128
#define TARG_LDBL_MIN_EXP -16381
#else /* !(defined(FP_LONG_DOUBLE_IS_BINARY128) && ...) */
#ifdef LDBL_MIN_EXP
#define TARG_LDBL_MIN_EXP LDBL_MIN_EXP
#else /* ifndef LDBL_MIN_EXP */
#define TARG_LDBL_MIN_EXP (-16381)
#endif /* ifdef LDBL_MIN_EXP */
#endif /* defined(FP_LONG_DOUBLE_IS_BINARY128) && ... */
#endif /* defined(FP_LONG_DOUBLE_IS_80BIT_EXTENDED) && ... */
#endif /* defined(FP_LONG_DOUBLE_IS_BINARY64) && FP_LONG_DOUBLE_IS_BINARY64 */
#endif /* ifndef TARG_LDBL_MIN_EXP */

#ifndef TARG_LDBL_MAX_EXP
#if defined(FP_LONG_DOUBLE_IS_BINARY64) && FP_LONG_DOUBLE_IS_BINARY64
#define TARG_LDBL_MAX_EXP 1024
#else /* !(defined(FP_LONG_DOUBLE_IS_BINARY_64) && ...) */
#if defined(FP_LONG_DOUBLE_IS_80BIT_EXTENDED) && \
    FP_LONG_DOUBLE_IS_80BIT_EXTENDED
#define TARG_LDBL_MAX_EXP 16384
#else /* !(defined(FP_LONG_DOUBLE_IS_80BIT_EXTENDED) && ...) */
#if defined(FP_LONG_DOUBLE_IS_BINARY128) && FP_LONG_DOUBLE_IS_BINARY128
#define TARG_LDBL_MAX_EXP 16384
#else /* !(defined(FP_LONG_DOUBLE_IS_BINARY128) && ...) */
#ifdef LDBL_MAX_EXP
#define TARG_LDBL_MAX_EXP LDBL_MAX_EXP
#else /* ifndef LDBL_MAX_EXP */
#define TARG_LDBL_MAX_EXP (16384)
#endif /* ifdef LDBL_MAX_EXP */
#endif /* defined(FP_LONG_DOUBLE_IS_BINARY128) && ... */
#endif /* defined(FP_LONG_DOUBLE_IS_80BIT_EXTENDED) && ... */
#endif /* defined(FP_LONG_DOUBLE_IS_BINARY64) && FP_LONG_DOUBLE_IS_BINARY64 */
#endif /* ifndef TARG_LDBL_MAX_EXP */

#if defined(FP_LONG_DOUBLE_IS_BINARY64) && FP_LONG_DOUBLE_IS_BINARY64 && \
    (TARG_LDBL_MANT_DIG != 53 || \
     TARG_LDBL_MAX_EXP != 1024 || \
     TARG_LDBL_MIN_EXP != -1021)
#error FP_LONG_DOUBLE_IS_BINARY64 is TRUE but TARG_LDBL_* values do not match.
#endif /* FP_LONG_DOUBLE_IS_BINARY64 && ... */

#if defined(FP_LONG_DOUBLE_IS_80BIT_EXTENDED) && \
    FP_LONG_DOUBLE_IS_80BIT_EXTENDED && \
    (TARG_LDBL_MANT_DIG != 64 || \
     TARG_LDBL_MAX_EXP != 16384 || \
     TARG_LDBL_MIN_EXP != -16381)
#error FP_LONG_DOUBLE_IS_80BIT_EXTENDED is TRUE but TARG_LDBL_* values do not \
       match.
#endif /* FP_LONG_DOUBLE_IS_80BIT_EXTENDED && ... */

#if defined(FP_LONG_DOUBLE_IS_BINARY128) && FP_LONG_DOUBLE_IS_BINARY128 && \
    (TARG_LDBL_MANT_DIG != 113 || \
     TARG_LDBL_MAX_EXP != 16384 || \
     TARG_LDBL_MIN_EXP != -16381)
#error FP_LONG_DOUBLE_IS_BINARY128 is TRUE but TARG_LDBL_* values do not match.
#endif /* FP_LONG_DOUBLE_IS_BINARY128 && ... */

#ifndef TARG_FLT80_MANT_DIG
#define TARG_FLT80_MANT_DIG 64
#endif /* ifndef TARG_FLT80_MANT_DIG */

#ifndef TARG_FLT80_MIN_EXP
#define TARG_FLT80_MIN_EXP (-16381)
#endif /* ifndef TARG_FLT80_MIN_EXP */

#ifndef TARG_FLT80_MAX_EXP
#define TARG_FLT80_MAX_EXP (16384)
#endif /* ifndef TARG_FLT80_MAX_EXP */

#ifndef TARG_FLT128_MANT_DIG
#define TARG_FLT128_MANT_DIG 113
#endif /* ifndef TARG_FLT128_MANT_DIG */

#ifndef TARG_FLT128_MIN_EXP
#define TARG_FLT128_MIN_EXP (-16381)
#endif /* ifndef TARG_FLT128_MIN_EXP */

#ifndef TARG_FLT128_MAX_EXP
#define TARG_FLT128_MAX_EXP (16384)
#endif /* ifndef TARG_FLT128_MAX_EXP */


/*
Maximum floating-point values.  If a TARG_ macro has been defined, we use that.
Otherwise, if the corresponding standard C macros are defined, we use those.
Otherwise, we leave these undefined.
*/
#ifndef TARG_FLT_MAX
#ifdef FLT_MAX
#define TARG_FLT_MAX FLT_MAX
#endif /* ifdef FLT_MAX */
#endif /* ifndef TARG_FLT_MAX */

#ifndef TARG_DBL_MAX
#ifdef DBL_MAX
#define TARG_DBL_MAX DBL_MAX
#endif /* ifdef DBL_MAX */
#endif /* ifndef TARG_DBL_MAX */

#ifndef TARG_LDBL_MAX
#ifdef LDBL_MAX
#define TARG_LDBL_MAX LDBL_MAX
#endif /* ifdef LDBL_MAX */
#endif /* ifndef TARG_LDBL_MAX */

/*
Target byte order.  Little-endian means the least-significant part of a
multi-byte integer is at the lowest memory address.
*/
#ifndef TARG_LITTLE_ENDIAN
#define TARG_LITTLE_ENDIAN TRUE
			/* Default value, used to initialize global variable
			   targ_little_endian. */
#endif /* !defined(TARG_LITTLE_ENDIAN) */

/*
When the default value of TARG_LITTLE_ENDIAN changed from FALSE to TRUE
(because the majority of target architectures are little endian), the
HOST_TARGET_ENDIAN_MISMATCH_OKAY configuration macro was added to cause
an internal error to be diagnosed when the host and target machine endian
values differ (i.e., because a configuration that had relied on the default
value for TARG_LITTLE_ENDIAN may have had its value unwittingly changed).
For configurations where the host and target endian values do indeed differ
(i.e., cross-compilers), this value should be set to TRUE.
*/
#ifndef HOST_TARGET_ENDIAN_MISMATCH_OKAY
#define HOST_TARGET_ENDIAN_MISMATCH_OKAY FALSE
#endif /* HOST_TARGET_ENDIAN_MISMATCH_OKAY */

/*
Char types:
*/
#ifndef TARG_CHAR_BIT
#define TARG_CHAR_BIT 8
			/* Number of bits in a target char.  Default value,
			   used to initialize global variable targ_char_bit. */
#endif /* !defined(TARG_CHAR_BIT) */

/* TARG_HOST_STRING_CHAR_BIT is the number of data bits per character used
   when representing target characters as a string on the host.  One is
   allowed to make the target char larger than the host char, but individual
   characters in string literals will be limited by what is representable in
   a host char. */
#ifndef TARG_HOST_STRING_CHAR_BIT
#if TARG_CHAR_BIT > CHAR_BIT
#define TARG_HOST_STRING_CHAR_BIT CHAR_BIT
#else /* TARG_CHAR_BIT <= CHAR_BIT */
#define TARG_HOST_STRING_CHAR_BIT TARG_CHAR_BIT
#endif /* TARG_CHAR_BIT > CHAR_BIT */
#endif /* !defined(TARG_HOST_STRING_CHAR_BIT) */
			/* Default value, used to initialize global variable
			   targ_host_string_char_bit. */

/* Make the default for character signedness on the target the same as
   for the host.  That's not required; it's just the most common case,
   and doing it this way makes it less likely that this configuration
   will be done wrong. */
#ifndef TARG_HAS_SIGNED_CHARS
#if CHAR_MIN == 0
#define TARG_HAS_SIGNED_CHARS FALSE
#else /* CHAR_MIN != 0 */
#define TARG_HAS_SIGNED_CHARS TRUE
#endif /* CHAR_MIN == 0 */
			/* Default value, used to initialize global variable
			   targ_has_signed_chars. */
#endif /* !defined(TARG_HAS_SIGNED_CHARS) */

/*
Special characters:
*/
#define TARG_BACKSPACE_CHAR   '\b'
#define TARG_FORM_FEED_CHAR   '\f'
#define TARG_NEWLINE_CHAR     '\n'
#define TARG_CARR_RETURN_CHAR '\r'
#define TARG_HORIZ_TAB_CHAR   '\t'

#ifndef TARG_ALERT_CHAR
#define TARG_ALERT_CHAR       '\007'
#endif /* ifndef TARG_ALERT_CHAR */
#ifndef TARG_VERT_TAB_CHAR
#define TARG_VERT_TAB_CHAR    '\013'
#endif /* ifndef TARG_VERT_TAB_CHAR */
#ifndef TARG_ESC_CHAR
#define TARG_ESC_CHAR         '\033'
#endif /* ifndef TARG_ESC_CHAR */

/*
Ordering of bytes in char constants:
*/
#ifndef TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT 1
			/* if 1, 'ab' == 0x6162. */
			/* if 0, 'ab' == 0x6261. */
			/* Default value, used to initialize global variable
			   targ_char_constant_first_char_most_significant. */
#endif /* !defined(TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT) */

/*
Integer types:
*/
/* Remember that the size of a type must be a multiple of the alignment. */
#ifndef TARG_SIZEOF_SHORT
#define TARG_SIZEOF_SHORT 2
			/* Default value, used to initialize global variable
			   targ_sizeof_short. */
#endif /* !defined(TARG_SIZEOF_SHORT) */
#ifndef TARG_ALIGNOF_SHORT
#define TARG_ALIGNOF_SHORT 2
			/* Default value, used to initialize global variable
			   targ_alignof_short. */
#endif /* !defined(TARG_ALIGNOF_SHORT) */
#ifndef TARG_SIZEOF_INT
#define TARG_SIZEOF_INT 4
			/* Default value, used to initialize global variable
			   targ_sizeof_int. */
#endif /* !defined(TARG_SIZEOF_INT) */
#ifndef TARG_ALIGNOF_INT
#define TARG_ALIGNOF_INT 4
			/* Default value, used to initialize global variable
			   targ_alignof_int. */
#endif /* !defined(TARG_ALIGNOF_INT) */
#ifndef TARG_SIZEOF_LONG
#define TARG_SIZEOF_LONG 4
			/* Default value, used to initialize global variable
			   targ_sizeof_long. */
#endif /* !defined(TARG_SIZEOF_LONG) */
#ifndef TARG_ALIGNOF_LONG
#define TARG_ALIGNOF_LONG 4
			/* Default value, used to initialize global variable
			   targ_alignof_long. */
#endif /* !defined(TARG_ALIGNOF_LONG) */
#if LONG_LONG_ALLOWED
#ifndef TARG_SIZEOF_LONG_LONG
#define TARG_SIZEOF_LONG_LONG 8
			/* Default value, used to initialize global variable
			   targ_sizeof_long_long. */
#endif /* !defined(TARG_SIZEOF_LONG_LONG) */
#ifndef TARG_ALIGNOF_LONG_LONG
#define TARG_ALIGNOF_LONG_LONG 8
			/* Default value, used to initialize global variable
			   targ_alignof_long_long. */
#endif /* !defined(TARG_ALIGNOF_LONG_LONG) */
#endif /* LONG_LONG_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
#ifndef TARG_SIZEOF_INT128
#define TARG_SIZEOF_INT128 16
			/* Default value, used to initialize global variable
			   targ_sizeof_int128. */
#endif /* !defined(TARG_SIZEOF_INT128) */
#ifndef TARG_ALIGNOF_INT128
#define TARG_ALIGNOF_INT128 16
			/* Default value, used to initialize global variable
			   targ_alignof_int128. */
#endif /* !defined(TARG_ALIGNOF_INT128) */
#endif /* INT128_EXTENSIONS_ALLOWED */
#ifndef DEFAULT_BITINT_MAXWIDTH_VALUE
#define DEFAULT_BITINT_MAXWIDTH_VALUE 65535
			/* Default value, used to initialize global variable
			   bitint_maxwidth_value. */
#endif /* !defined(DEFAULT_BITINT_MAXWIDTH_VALUE) */

/* Specify the size of the largest integer.  Note that this will constrain
   how targ_sizeof_long, targ_sizeof_long_long, and targ_sizeof_largest_integer
   are configured at runtime.   TARG_SIZEOF_LARGEST_INTEGER represents the
   size of the largest integer in the legacy configuration;
   MAX_SIZEOF_LARGEST_INTEGER represents the size of the largest target
   integer that can be accommodated across all target configurations (and
   therefore must be at least as large as every TARG_SIZEOF_LARGEST_INTEGER).
   MAX_SIZEOF_LARGEST_INTEGER is required to be a compile-time
   constant and so cannot be adjusted at run time the way some other target
   configuration values are.  Therefore, it should be set to the largest
   value a "long int" (or a "long long int") is allowed to have in any
   supported target configuration. */
#ifndef TARG_SIZEOF_LARGEST_INTEGER
/* By default, a minimum largest value is supplied, and it is expected to be
   one of 1, 4, 8, or 16.  This can be changed either here or in defines.h,
   if required.  However, it is necessary to use a literal instead of the
   more obvious named value, since TARG_SIZEOF_LARGEST_INTEGER is used later
   in defining other macros, but TARG_SIZEOF_LONG, etc., do not persist once
   the variables they correspond to have been defined. */
#if LONG_LONG_ALLOWED
/* A "long long int" is the largest integer. */
#define TARG_SIZEOF_LARGEST_INTEGER TARG_SIZEOF_LONG_LONG
#else /* !LONG_LONG_ALLOWED */
/* A "long int" is the largest integer. */
#define TARG_SIZEOF_LARGEST_INTEGER TARG_SIZEOF_LONG
#endif /* !LONG_LONG_ALLOWED */
#if INT128_EXTENSIONS_ALLOWED
#if TARG_SIZEOF_INT128 > TARG_SIZEOF_LARGEST_INTEGER 
#undef TARG_SIZEOF_LARGEST_INTEGER
#define TARG_SIZEOF_LARGEST_INTEGER TARG_SIZEOF_INT128
#endif /* TARG_SIZEOF_INT128 > TARG_SIZEOF_LARGEST_INTEGER */
#endif /* INT128_EXTENSIONS_ALLOWED */
#if TARG_SIZEOF_LARGEST_INTEGER == 4
#undef TARG_SIZEOF_LARGEST_INTEGER
#define TARG_SIZEOF_LARGEST_INTEGER 4
#else /* TARG_SIZEOF_LARGEST_INTEGER != 4 */
#if TARG_SIZEOF_LARGEST_INTEGER == 8
#undef TARG_SIZEOF_LARGEST_INTEGER
#define TARG_SIZEOF_LARGEST_INTEGER 8
#else /* TARG_SIZEOF_LARGEST_INTEGER != 8 */
#if TARG_SIZEOF_LARGEST_INTEGER == 16
#undef TARG_SIZEOF_LARGEST_INTEGER
#define TARG_SIZEOF_LARGEST_INTEGER 16
#else /* TARG_SIZEOF_LARGEST_INTEGER != 16 */
#if TARG_SIZEOF_LARGEST_INTEGER == 1
#undef TARG_SIZEOF_LARGEST_INTEGER
#define TARG_SIZEOF_LARGEST_INTEGER 1
#else /* TARG_SIZEOF_LARGEST_INTEGER != 1 */
 #error -- do not know how to set TARG_SIZEOF_LARGEST_INTEGER
#endif /* TARG_SIZEOF_LARGEST_INTEGER == 1 */
#endif /* TARG_SIZEOF_LARGEST_INTEGER == 16 */
#endif /* TARG_SIZEOF_LARGEST_INTEGER == 8 */
#endif /* TARG_SIZEOF_LARGEST_INTEGER == 4 */
#endif /* !defined(TARG_SIZEOF_LARGEST_INTEGER) */
/* Check the value.  It must be large enough to accommodate the largest
   integer we know about at this point.  An additional check is made after
   command line processing in case the variables corresponding to
   targ_sizeof_long and targ_sizeof_long_long get different values. */
#if LONG_LONG_ALLOWED
#if TARG_SIZEOF_LARGEST_INTEGER < TARG_SIZEOF_LONG_LONG
 #error -- TARG_SIZEOF_LARGEST_INTEGER too small for TARG_SIZEOF_LONG_LONG
#endif /* TARG_SIZEOF_LARGEST_INTEGER < TARG_SIZEOF_LONG_LONG */
#else /* !LONG_LONG_ALLOWED */
#if TARG_SIZEOF_LARGEST_INTEGER < TARG_SIZEOF_LONG
 #error -- TARG_SIZEOF_LARGEST_INTEGER too small for TARG_SIZEOF_LONG
#endif /* TARG_SIZEOF_LARGEST_INTEGER < TARG_SIZEOF_LONG */
#endif /* LONG_LONG_ALLOWED */

#ifndef MAX_SIZEOF_LARGEST_INTEGER
#if INT128_EXTENSIONS_ALLOWED && \
    TARG_SIZEOF_INT128 > TARG_SIZEOF_LARGEST_INTEGER
#define MAX_SIZEOF_LARGEST_INTEGER TARG_SIZEOF_INT128
#else /* !(INT128_EXTENSIONS_ALLOWED && TARG_SIZEOF_INT128 > TARG_SIZEOF...) */
#define MAX_SIZEOF_LARGEST_INTEGER TARG_SIZEOF_LARGEST_INTEGER
#endif /* INT128_EXTENSIONS_ALLOWED && TARG_SIZEOF_INT128 > TARG_SIZEOF... */
#endif /* defined(MAX_SIZEOF_LARGEST_INTEGER) */
#if TARG_SIZEOF_LARGEST_INTEGER > MAX_SIZEOF_LARGEST_INTEGER
 #error -- MAX_SIZEOF_LARGEST_INTEGER too small for TARG_SIZEOF_LARGEST_INTEGER
#endif /* TARG_SIZEOF_LARGEST_INTEGER > MAX_SIZEOF_LARGEST_INTEGER */

/*
Fixed-point type configuration.

Any configuration that sets FIXED_POINT_ALLOWED to TRUE will normally also
define all the quantities defining the size, alignment, etc. of these types.
If a quantity is not configured, we set it to a "typical desktop CPU" value.
Six _Fract types can be configured independently: signed short _Fract,
unsigned short _Fract, signed _Fract, unsigned _Fract, long signed _Fract, and
long unsigned _Fract.  Similarly, there are six _Accum types that can also be
configured independently.
These configuration macros are used to initialize the targ_sizeof_fixed_point,
targ_alignof_fixed_point, and targ_fractional_bits_for_fixed_point arrays.
*/
#if FIXED_POINT_ALLOWED

#ifndef TARG_SIZEOF_SIGNED_SHORT_ACCUM
#define TARG_SIZEOF_SIGNED_SHORT_ACCUM 2
#endif /* ifndef TARG_SIZEOF_SIGNED_SHORT_ACCUM */

#ifndef TARG_ALIGNOF_SIGNED_SHORT_ACCUM
#define TARG_ALIGNOF_SIGNED_SHORT_ACCUM 2
#endif /* ifndef TARG_ALIGNOF_SIGNED_SHORT_ACCUM */

#ifndef TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM 7
#endif /* ifndef TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM */

#ifndef TARG_SIZEOF_UNSIGNED_SHORT_ACCUM
#define TARG_SIZEOF_UNSIGNED_SHORT_ACCUM 2
#endif /* ifndef TARG_SIZEOF_UNSIGNED_SHORT_ACCUM */

#ifndef TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM
#define TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM 2
#endif /* ifndef TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM */

#ifndef TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM 8
#endif /* ifndef TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM */

#ifndef TARG_SIZEOF_SIGNED_ACCUM
#define TARG_SIZEOF_SIGNED_ACCUM 4
#endif /* ifndef TARG_SIZEOF_SIGNED_ACCUM */

#ifndef TARG_ALIGNOF_SIGNED_ACCUM
#define TARG_ALIGNOF_SIGNED_ACCUM 4
#endif /* ifndef TARG_ALIGNOF_SHORT_ACCUM */

#ifndef TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM 15
#endif /* ifndef TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM */

#ifndef TARG_SIZEOF_UNSIGNED_ACCUM
#define TARG_SIZEOF_UNSIGNED_ACCUM 4
#endif /* ifndef TARG_SIZEOF_UNSIGNED_ACCUM */

#ifndef TARG_ALIGNOF_UNSIGNED_ACCUM
#define TARG_ALIGNOF_UNSIGNED_ACCUM 4
#endif /* ifndef TARG_ALIGNOF_UNSIGNED_ACCUM */

#ifndef TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM 16
#endif /* ifndef TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM */

#ifndef TARG_SIZEOF_SIGNED_LONG_ACCUM
#define TARG_SIZEOF_SIGNED_LONG_ACCUM 8
#endif /* ifndef TARG_SIZEOF_SIGNED_LONG_ACCUM */

#ifndef TARG_ALIGNOF_SIGNED_LONG_ACCUM
#define TARG_ALIGNOF_SIGNED_LONG_ACCUM 8
#endif /* ifndef TARG_ALIGNOF_SIGNED_LONG_ACCUM */

#ifndef TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM 31
#endif /* ifndef TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM */

#ifndef TARG_SIZEOF_UNSIGNED_LONG_ACCUM
#define TARG_SIZEOF_UNSIGNED_LONG_ACCUM 8
#endif /* ifndef TARG_SIZEOF_UNSIGNED_LONG_ACCUM */

#ifndef TARG_ALIGNOF_UNSIGNED_LONG_ACCUM
#define TARG_ALIGNOF_UNSIGNED_LONG_ACCUM 8
#endif /* ifndef TARG_ALIGNOF_UNSIGNED_LONG_ACCUM */

#ifndef TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM 32
#endif /* ifndef TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM */

#ifndef TARG_SIZEOF_SIGNED_SHORT_FRACT
#define TARG_SIZEOF_SIGNED_SHORT_FRACT 1
#endif /* ifndef TARG_SIZEOF_SIGNED_SHORT_FRACT */

#ifndef TARG_ALIGNOF_SIGNED_SHORT_FRACT
#define TARG_ALIGNOF_SIGNED_SHORT_FRACT 1
#endif /* ifndef TARG_ALIGNOF_SIGNED_SHORT_FRACT */

#ifndef TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT 7
#endif /* ifndef TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT */

#ifndef TARG_SIZEOF_UNSIGNED_SHORT_FRACT
#define TARG_SIZEOF_UNSIGNED_SHORT_FRACT 1
#endif /* ifndef TARG_SIZEOF_UNSIGNED_SHORT_FRACT */

#ifndef TARG_ALIGNOF_UNSIGNED_SHORT_FRACT
#define TARG_ALIGNOF_UNSIGNED_SHORT_FRACT 1
#endif /* ifndef TARG_ALIGNOF_UNSIGNED_SHORT_FRACT */

#ifndef TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT 8
#endif /* ifndef TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT */

#ifndef TARG_SIZEOF_SIGNED_FRACT
#define TARG_SIZEOF_SIGNED_FRACT 2
#endif /* ifndef TARG_SIZEOF_SIGNED_FRACT */

#ifndef TARG_ALIGNOF_SIGNED_FRACT
#define TARG_ALIGNOF_SIGNED_FRACT 2
#endif /* ifndef TARG_ALIGNOF_SHORT_FRACT */

#ifndef TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT 15
#endif /* ifndef TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT */

#ifndef TARG_SIZEOF_UNSIGNED_FRACT
#define TARG_SIZEOF_UNSIGNED_FRACT 2
#endif /* ifndef TARG_SIZEOF_UNSIGNED_FRACT */

#ifndef TARG_ALIGNOF_UNSIGNED_FRACT
#define TARG_ALIGNOF_UNSIGNED_FRACT 2
#endif /* ifndef TARG_ALIGNOF_UNSIGNED_FRACT */

#ifndef TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT 16
#endif /* ifndef TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT */

#ifndef TARG_SIZEOF_SIGNED_LONG_FRACT
#define TARG_SIZEOF_SIGNED_LONG_FRACT 4
#endif /* ifndef TARG_SIZEOF_SIGNED_LONG_FRACT */

#ifndef TARG_ALIGNOF_SIGNED_LONG_FRACT
#define TARG_ALIGNOF_SIGNED_LONG_FRACT 4
#endif /* ifndef TARG_ALIGNOF_SIGNED_LONG_FRACT */

#ifndef TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT 31
#endif /* ifndef TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT */

#ifndef TARG_SIZEOF_UNSIGNED_LONG_FRACT
#define TARG_SIZEOF_UNSIGNED_LONG_FRACT 4
#endif /* ifndef TARG_SIZEOF_UNSIGNED_LONG_FRACT */

#ifndef TARG_ALIGNOF_UNSIGNED_LONG_FRACT
#define TARG_ALIGNOF_UNSIGNED_LONG_FRACT 4
#endif /* ifndef TARG_ALIGNOF_UNSIGNED_LONG_FRACT */

#ifndef TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT 32
#endif /* ifndef TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT */

/*
Determine the largest fixed-point size (if not already provided).
We assume it is one of the "long" precision variants.
*/
#ifndef TARG_SIZEOF_LARGEST_FIXED_POINT
#define TARG_SIZEOF_LARGEST_FIXED_POINT TARG_SIZEOF_UNSIGNED_LONG_ACCUM
#if TARG_SIZEOF_LARGEST_FIXED_POINT < TARG_SIZEOF_SIGNED_LONG_ACCUM
#undef TARG_SIZEOF_LARGEST_FIXED_POINT
#define TARG_SIZEOF_LARGEST_FIXED_POINT TARG_SIZEOF_SIGNED_LONG_ACCUM
#endif /* TARG_SIZEOF_LARGEST_FIXED_POINT < TARG_SIZEOF_SIGNED_LONG_ACCUM */
#if TARG_SIZEOF_LARGEST_FIXED_POINT < TARG_SIZEOF_UNSIGNED_LONG_FRACT
#undef TARG_SIZEOF_LARGEST_FIXED_POINT
#define TARG_SIZEOF_LARGEST_FIXED_POINT TARG_SIZEOF_UNSIGNED_LONG_FRACT
#endif /* TARG_SIZEOF_LARGEST_FIXED_POINT < TARG_SIZEOF_UNSIGNED_LONG_FRACT */
#if TARG_SIZEOF_LARGEST_FIXED_POINT < TARG_SIZEOF_SIGNED_LONG_FRACT
#undef TARG_SIZEOF_LARGEST_FIXED_POINT
#define TARG_SIZEOF_LARGEST_FIXED_POINT TARG_SIZEOF_SIGNED_LONG_FRACT
#endif /* TARG_SIZEOF_LARGEST_FIXED_POINT < TARG_SIZEOF_SIGNED_LONG_FRACT */
#endif /* ifndef TARG_SIZEOF_LARGEST_FIXED_POINT */

/*
MAX_SIZEOF_LARGEST_FIXED_POINT represents the size of the largest fixed point
type that is supported across all target configurations and must be a
compile-time constant.
*/
#ifndef MAX_SIZEOF_LARGEST_FIXED_POINT
#define MAX_SIZEOF_LARGEST_FIXED_POINT TARG_SIZEOF_LARGEST_FIXED_POINT
#endif /* defined(MAX_SIZEOF_LARGEST_FIXED_POINT) */
#if TARG_SIZEOF_LARGEST_FIXED_POINT > MAX_SIZEOF_LARGEST_FIXED_POINT
 #error -- MAX_SIZEOF_LARGEST_FIXED_POINT too small for \
           TARG_SIZEOF_LARGEST_FIXED_POINT
#endif /* TARG_SIZEOF_LARGEST_FIXED_POINT > MAX_SIZEOF_LARGEST_FIXED_POINT */

#endif /* FIXED_POINT_ALLOWED */

#if GNU_EXTENSIONS_ALLOWED
/*
The size of the largest atomic type natively supported by the target.  In Clang
mode, smaller atomic-qualified types get additional padding to make their size
a power of 2.
*/
#ifndef TARG_SIZEOF_LARGEST_ATOMIC
#define TARG_SIZEOF_LARGEST_ATOMIC (TARG_SIZEOF_LONG+TARG_SIZEOF_LONG)
#endif /* TARG_SIZEOF_LARGEST_ATOMIC */
#endif /* GNU_EXTENSIONS_ALLOWED */

/*
Type used as the representation of an integer value.  More precisely,
this is the form used on the host to represent a target integer.
*/
/*
At the first level, one must choose between a representation using
a single value of some host integer type and one using an array of
smaller host integers.  The latter form is necessary when the front
end is used as part of a cross-compiler where the target has larger
integers than the host.
*/
#ifndef INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
#define INTEGER_VALUE_REPR_IS_A_HOST_INTEGER TRUE
#endif /* !defined(INTEGER_VALUE_REPR_IS_A_HOST_INTEGER) */

#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER

/*
If using a host integer as the integer value representation and 128-bit integer
extensions are allowed, ensure that the host has 128-bit integer extensions.
*/
#if INT128_EXTENSIONS_ALLOWED
#if !HOST_HAS_INT128_EXTENSIONS
 #error -- HOST_HAS_INT128_EXTENSIONS cannot be FALSE when \
           INTEGER_VALUE_REPR_IS_A_HOST_INTEGER is TRUE and \
           INT128_EXTENSIONS_ALLOWED is TRUE
#endif /* !HOST_HAS_INT128_EXTENSIONS */
#endif /* INT128_EXTENSIONS_ALLOWED */

/*
There is a host integer type that is large enough to hold all target integers,
so the integer representation is just some host integral type.
This type must be unsigned; a_signed_integer_value is the signed version.
If LONG_LONG_ALLOWED is TRUE, the host "long long" and "unsigned long long"
are used by default.
*/
#ifndef TYPE_FOR_AN_INTEGER_VALUE
#if INT128_EXTENSIONS_ALLOWED
#define TYPE_FOR_AN_INTEGER_VALUE __uint128_t
#else /* !INT128_EXTENSIONS_ALLOWED */
#if LONG_LONG_ALLOWED
#define TYPE_FOR_AN_INTEGER_VALUE unsigned long long
#else /* !LONG_LONG_ALLOWED */
#define TYPE_FOR_AN_INTEGER_VALUE unsigned long
#endif /* LONG_LONG_ALLOWED */
#endif /* INT128_EXTENSIONS_ALLOWED */
#endif /* ifndef TYPE_FOR_AN_INTEGER_VALUE */
typedef TYPE_FOR_AN_INTEGER_VALUE an_integer_value;

#ifndef TYPE_FOR_A_SIGNED_INTEGER_VALUE
#if INT128_EXTENSIONS_ALLOWED
#define TYPE_FOR_A_SIGNED_INTEGER_VALUE __int128_t
#else /* !INT128_EXTENSIONS_ALLOWED */
#if LONG_LONG_ALLOWED
#define TYPE_FOR_A_SIGNED_INTEGER_VALUE long long
#else /* !LONG_LONG_ALLOWED */
#define TYPE_FOR_A_SIGNED_INTEGER_VALUE long
#endif /* LONG_LONG_ALLOWED */
#endif /* INT128_EXTENSIONS_ALLOWED */
#endif /* ifndef TYPE_FOR_A_SIGNED_INTEGER_VALUE */
typedef TYPE_FOR_A_SIGNED_INTEGER_VALUE a_signed_integer_value;

/* Minimum and maximum values that can be represented in an_integer_value. */
#ifndef MAX_INTEGER_VALUE
#if INT128_EXTENSIONS_ALLOWED
#define MAX_INTEGER_VALUE ((__int128_t)(~(__uint128_t)0 >> 1))
#else /* !INT128_EXTENSIONS_ALLOWED */
#if LONG_LONG_ALLOWED
#ifdef LLONG_MAX
#define MAX_INTEGER_VALUE LLONG_MAX
#else /* !defined(LLONG_MAX) */
#define MAX_INTEGER_VALUE 9223372036854775807LL /* 64-bit */
#endif /* ifdef LLONG_MAX */
#else /* !LONG_LONG_ALLOWED */
#define MAX_INTEGER_VALUE LONG_MAX
#endif /* LONG_LONG_ALLOWED */
#endif /* INT128_EXTENSIONS_ALLOWED */
#endif /* ifndef MAX_INTEGER_VALUE */
#ifndef MIN_INTEGER_VALUE
#if INT128_EXTENSIONS_ALLOWED
#define MIN_INTEGER_VALUE (-MAX_INTEGER_VALUE-1)
#else /* !INT128_EXTENSIONS_ALLOWED */
#if LONG_LONG_ALLOWED
#if defined(LLONG_MIN) && LLONG_MIN < LLONG_MAX /*lint !e30*/
/* The preceding condition detects an incorrect definition of LLONG_MIN
   in MSVC 7.1. */
#define MIN_INTEGER_VALUE LLONG_MIN
#else /* !(defined(LLONG_MIN) && LLONG_MIN < LLONG_MAX) */
#define MIN_INTEGER_VALUE (-MAX_INTEGER_VALUE-1)
#endif /* defined(LLONG_MIN) && LLONG_MIN < LLONG_MAX */
#else /* !LONG_LONG_ALLOWED */
#define MIN_INTEGER_VALUE LONG_MIN
#endif /* LONG_LONG_ALLOWED */
#endif /* INT128_EXTENSIONS_ALLOWED */
#endif /* ifndef MIN_INTEGER_VALUE */
#ifndef MAX_UNSIGNED_INTEGER_VALUE
#if INT128_EXTENSIONS_ALLOWED
#define MAX_UNSIGNED_INTEGER_VALUE (~(__uint128_t)0)
#else /* !INT128_EXTENSIONS_ALLOWED */
#if LONG_LONG_ALLOWED
#ifdef ULLONG_MAX
#define MAX_UNSIGNED_INTEGER_VALUE ULLONG_MAX
#else /* !defined(ULLONG_MAX) */
#define MAX_UNSIGNED_INTEGER_VALUE 18446744073709551615ULL /* 64-bit */
#endif /* ifdef ULLONG_MAX */
#else /* !LONG_LONG_ALLOWED */
#define MAX_UNSIGNED_INTEGER_VALUE ULONG_MAX
#endif /* LONG_LONG_ALLOWED */
#endif /* INT128_EXTENSIONS_ALLOWED */
#endif /* ifndef MAX_UNSIGNED_INTEGER_VALUE */
#ifndef BITS_IN_AN_INTEGER_VALUE
#define BITS_IN_AN_INTEGER_VALUE (sizeof(an_integer_value) * CHAR_BIT)
#endif /* ifndef BITS_IN_AN_INTEGER_VALUE */

/*
Host types used to manipulate integer values.
*/
typedef a_signed_integer_value a_host_large_integer;
typedef an_integer_value a_host_large_unsigned;

static_assert(sizeof(a_host_large_integer) >= sizeof(a_ptrdiff),
              "a_host_large_integer too small");
static_assert(sizeof(a_host_large_unsigned) >= sizeof(sizeof_t),
              "a_host_large_unsigned too small");

/*
Minimum and maximum values that can be represented in a_host_large_integer
and a_host_large_unsigned.
*/
#define MAX_HOST_LARGE_INTEGER MAX_INTEGER_VALUE
#define MIN_HOST_LARGE_INTEGER MIN_INTEGER_VALUE
#define MAX_HOST_LARGE_UNSIGNED MAX_UNSIGNED_INTEGER_VALUE

#else /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

/*
There is no host integer that is large enough, so use an array to represent
the target integers.
*/
/* Type of the elements of the array.  These must be at most half the
   size of a_host_large_integer (some large and efficient integer type on
   the host), and (for space reasons) preferably exactly half.
   Typically, this is a 16-bit value.  The bit size and
   maximum values indicate the range of values to be used, which may
   be smaller than the range actually available.  That is, one could
   use a type that is larger than half of a_host_large_integer but
   restrict the values to be used to just half of a_host_large_integer
   if no suitable smaller type is available. */
typedef unsigned short an_int_value_part;
#define MAX_UINT_VALUE_PART 0xffff
#define SIGN_BIT_INT_VALUE_PART 0x8000
#define SIZEOF_INT_VALUE_PART (sizeof(an_int_value_part)) /* Okay to change. */
#define BITS_IN_INT_VALUE_PART (SIZEOF_INT_VALUE_PART*CHAR_BIT)
/* Large and efficient host integer types, at least twice the size of
   an_int_value_part, used in doing computations on integer values.
   The idea is that any operation involving two an_int_value_part
   values can be done in a_host_large_integer without special coding
   to deal with overflows.  These types are also used to manipulate integer
   values that are a subset of the values that can be represented by
   an_integer_value when INTEGER_VALUE_REPR_IS_A_HOST_INTEGER is
   FALSE.  Many operations can be done using these types, because
   most constant values are small.  When the values are too large,
   alternate routines are used.  These types should also be at least
   as large as the host ptrdiff_t type. */
typedef a_ptrdiff a_host_large_integer;
typedef sizeof_t a_host_large_unsigned;
#define MAX_HOST_LARGE_INTEGER PTRDIFF_MAX
#define MIN_HOST_LARGE_INTEGER PTRDIFF_MIN
#define MAX_HOST_LARGE_UNSIGNED SIZE_MAX

/* Define a macro that has the same value as TARG_CHAR_BIT.  This is done
   because TARG_CHAR_BIT cannot be used outside of targ_def.h (it gets
   undefined below).  The simulated integer routines do not support 
   implementations on which targ_char_bit can be changed. */
#ifndef INTERNAL_TARG_CHAR_BIT
#if TARG_CHAR_BIT == 8
#define INTERNAL_TARG_CHAR_BIT 8
#else /* TARG_CHAR_BIT != 8 */
#if TARG_CHAR_BIT == 16
#define INTERNAL_TARG_CHAR_BIT 16
#else /* TARG_CHAR_BIT != 16 */
#if TARG_CHAR_BIT == 24
#define INTERNAL_TARG_CHAR_BIT 24
#else /* TARG_CHAR_BIT != 24 */
#if TARG_CHAR_BIT == 32
#define INTERNAL_TARG_CHAR_BIT 32
#else /* TARG_CHAR_BIT != 32 */
 #error -- do not know how to set INTERNAL_TARG_CHAR_BIT
#endif /* TARG_CHAR_BIT == 32 */
#endif /* TARG_CHAR_BIT == 24 */
#endif /* TARG_CHAR_BIT == 16 */
#endif /* TARG_CHAR_BIT == 8 */
#endif /* ifndef INTERNAL_TARG_CHAR_BIT */

#define BITS_IN_HOST_LARGE_INTEGER (sizeof(a_host_large_integer)*CHAR_BIT)
/* The array is made up of elements of type an_int_value_part.
   Figure out how many.  Note that this representation may also be
   used for fixed-point values. */
#define INTEGER_VALUE_REPRESENTATION_SIZE MAX_SIZEOF_LARGEST_INTEGER
#if FIXED_POINT_ALLOWED
#if MAX_SIZEOF_LARGEST_FIXED_POINT > INTEGER_VALUE_REPRESENTATION_SIZE
#undef INTEGER_VALUE_REPRESENTATION_SIZE
#define INTEGER_VALUE_REPRESENTATION_SIZE MAX_SIZEOF_LARGEST_FIXED_POINT
#endif /* MAX_SIZEOF_LARGEST_FIXED_POINT > INTEGER_VALUE_REPR... */
#endif /* FIXED_POINT_ALLOWED */
#define INT_VALUE_PARTS_PER_INTEGER_VALUE                             \
  ((INTEGER_VALUE_REPRESENTATION_SIZE*INTERNAL_TARG_CHAR_BIT)/	      \
   (SIZEOF_INT_VALUE_PART*CHAR_BIT))
/* This is an array inside a struct instead of just an array so that
   its address behaves in a predictable way. */
typedef struct an_integer_value {
  an_int_value_part part[INT_VALUE_PARTS_PER_INTEGER_VALUE];
} an_integer_value;
#define BITS_IN_AN_INTEGER_VALUE (BITS_IN_INT_VALUE_PART *	      \
				  INT_VALUE_PARTS_PER_INTEGER_VALUE)

#endif /* INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

#if FIXED_POINT_ALLOWED
/*
By default the front end represents fixed-point values as implicitly scaled
integer values.
*/
#ifndef TYPE_FOR_A_FIXED_POINT_VALUE
#ifdef TYPE_FOR_AN_INTEGER_VALUE
#define TYPE_FOR_A_FIXED_POINT_VALUE TYPE_FOR_AN_INTEGER_VALUE
#else /* !defined(TYPE_FOR_AN_INTEGER_VALUE) */
#define TYPE_FOR_A_FIXED_POINT_VALUE an_integer_value
#endif /* ifdef TYPE_FOR_AN_INTEGER_VALUE */
#endif /* ifndef TYPE_FOR_A_FIXED_POINT_VALUE */
typedef TYPE_FOR_A_FIXED_POINT_VALUE a_fixed_point_value;

#endif /* FIXED_POINT_ALLOWED */

/*
If this flag is TRUE, overflows on signed integer operations do
not cause errors (only warnings).  Usually this would be set to
match the target machine behavior on integer operations in C.
*/
#ifndef TARG_NO_ERROR_ON_INTEGER_OVERFLOW
#define TARG_NO_ERROR_ON_INTEGER_OVERFLOW TRUE
#endif /* ifndef TARG_NO_ERROR_ON_INTEGER_OVERFLOW */

/* If this flag is TRUE, bit-field allocation follows the conventions of
   Microsoft C/C++.  Note that TARG_MICROSOFT_BIT_FIELD_ALLOCATION is set
   independently of MICROSOFT_EXTENSIONS_ALLOWED -- the former has more to
   do with ABI compatibility, the latter with language features that are
   accepted. */
/* When TARG_MICROSOFT_BIT_FIELD_ALLOCATION is TRUE, the setting of
   TARG_BIT_FIELD_CONTAINER_SIZE is required to be -1 and there is a
   two-stage allocation: first, a bit-field container based on the bit-field
   type is allocated (as though it were a field in its own right), and then
   bit fields are allocated within it. When the bit-field type changes or
   the container fills up, a new container is allocated. */
#ifndef TARG_MICROSOFT_BIT_FIELD_ALLOCATION
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION FALSE
			/* Default value, used to initialize global variable
			   targ_microsoft_bit_field_allocation. */
#endif /* ifndef TARG_MICROSOFT_BIT_FIELD_ALLOCATION */

/*
Flag that is TRUE if, when targ_microsoft_bit_field_allocation is TRUE, the
byte offset of a bit field within its container should be recorded in the
bit field's a_field entry.
*/
#ifndef RECORD_BIT_FIELD_CONTAINER_OFFSETS_IN_IL
#if TARG_MICROSOFT_BIT_FIELD_ALLOCATION
#define RECORD_BIT_FIELD_CONTAINER_OFFSETS_IN_IL TRUE
#else /* !TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
#define RECORD_BIT_FIELD_CONTAINER_OFFSETS_IN_IL FALSE
#endif /* TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
#endif /* ifndef RECORD_BIT_FIELD_CONTAINER_OFFSETS_IN_IL */




/* Container size to be used for bit-fields.  If > 0, indicates the
   size in bytes of one of the integral types.  0 means "use the smallest
   integral type into which the field will fit".  < 0 means "use the
   base type given in the declaration". */
#ifndef TARG_BIT_FIELD_CONTAINER_SIZE
#if TARG_MICROSOFT_BIT_FIELD_ALLOCATION
#define TARG_BIT_FIELD_CONTAINER_SIZE (-1)
#else /* !TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
#if CFRONT_OBJECT_CODE_COMPATIBILITY && ABI_COMPATIBILITY_VERSION >= 232
/* In the C code it generates, cfront changes the underlying types of all
   bit-fields to int or unsigned int. */
#define TARG_BIT_FIELD_CONTAINER_SIZE TARG_SIZEOF_INT
#else /* !CFRONT_OBJECT_CODE_COMPATIBILITY... */
#if IA64_ABI
#define TARG_BIT_FIELD_CONTAINER_SIZE (-1)
#else /* !IA64_ABI */
#define TARG_BIT_FIELD_CONTAINER_SIZE 0
#endif /* IA64_ABI */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY... */
#endif /* TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
			/* Default value, used to initialize global variable
			   targ_bit_field_container_size. */
#endif /* ifndef TARG_BIT_FIELD_CONTAINER_SIZE */
#if TARG_MICROSOFT_BIT_FIELD_ALLOCATION && TARG_BIT_FIELD_CONTAINER_SIZE != -1
 #error -- TARG_BIT_FIELD_CONTAINER_SIZE must be -1 when \
           TARG_MICROSOFT_BIT_FIELD_ALLOCATION is TRUE
#endif /* TARG_MICROSOFT_BIT_FIELD_ALLOCATION && ... */

/* How plain "int" bit fields are to be treated (signed or unsigned).
   This flag also controls how plain "short", "long", and "long long"
   are treated as bit field types.  Initial value of
   targ_plain_int_bit_field_is_unsigned.  Note that the signedness of 1-bit
   fields is controlled by TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED. */
#ifndef TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED
/*lint -emacro(506,TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED)*/
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED (!TARG_HAS_SIGNED_CHARS)
			/* Default value, used to initialize global variable
			   targ_plain_int_bit_field_is_unsigned. */
#endif /* ifndef TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED */

/*
Flag that indicates whether a one-bit bit field declared with a
base type that is not explicitly signed or unsigned should be
forced to be unsigned even if TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED
is FALSE, with the rationale that a bit field consisting of only
a sign is not very useful.  Initial value of
targ_force_one_bit_bit_field_to_be_unsigned.

Note that cfront mode forces all bit fields to be unsigned so this
setting is irrelevant in that case.
*/
#ifndef TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED
#if IA64_ABI
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED FALSE
#else /* !IA64_ABI */
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED TRUE
#endif /* IA64_ABI */
#endif /* TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED */

/* Flags controlling the signedness for enum bit fields (an extension in C).
   If TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED is TRUE, enum bit fields
   are always unsigned.  If FALSE, the rules are: (a) if the enum contains
   any negative values, the field is signed; otherwise (b) if the enum
   contains values large enough that they won't fit if one bit is allocated
   for a sign, the field is unsigned; otherwise (c) the signedness is as
   indicated by TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED. */
/* When TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED is TRUE, declaring a bit
   field with an enumeration type that includes negative enum constants will
   elicit a warning; moreover, the value extracted from the bit field will
   always be treated as an unsigned quantity (i.e., sign extension will not
   be done when extracting the value).  On the other hand, when the flag is
   FALSE, the value extracted from the bit field may be treated as a signed
   quantity (and sign extension may be done unexpectedly). */
#ifndef TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED
/* The Microsoft compiler treats enum bit fields as signed or unsigned. */
#if TARG_MICROSOFT_BIT_FIELD_ALLOCATION
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED FALSE
#else /* !TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
#if CFRONT_OBJECT_CODE_COMPATIBILITY
/* Cfront treats enum bit fields as unsigned. */
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED TRUE
#else /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED FALSE
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
#endif /* TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
			/* Default value, used to initialize global variable
			   targ_enum_bit_fields_are_always_unsigned. */
#endif /* ifndef TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED */

/*
Flag that determines the signedness of an enum bit field when both the "signed"
and "unsigned" variants can represent all the associated enumerator constants.
(See above for a detailed description of how the signedness of enum bit fields
is determined.) To emulate the GNU IA-64 ABI,
TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED should be set to TRUE while
TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED should be FALSE.  This is our default
configuration when the IA-64 ABI is selected (except when
ABI_COMPATIBILITY_VERSION < 307, to maintain backward compatibility).
Otherwise, TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED is equal to
TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED by default to keep compatibility with
earlier versions of the front end that did not support this flag.  This is
the default value of the global variable
targ_nonnegative_enum_bit_field_is_unsigned.
*/
#ifndef TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED
#if TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED ||                              \
    (IA64_ABI && ABI_COMPATIBILITY_VERSION >= 307)
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED  TRUE
#else /* !(TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED || (IA64_ABI  && ... )) */
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED  FALSE
#endif /* TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED */
#endif /* TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED || (IA64_ABI  && ... ) */

/* Alignment adjustment to be made when a zero-width (unnamed) bit field is
   declared.  If > 0 it is the alignment to be used (typically the alignment
   of one of the integral types).  A value of zero means "use the minimal
   alignment", which is single-byte alignment.  Any value less than zero
   means "use the alignment of the base type given in the declaration". */
#ifndef TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT
#if TARG_MICROSOFT_BIT_FIELD_ALLOCATION
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT (-1)
#else /* !TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
#if CFRONT_OBJECT_CODE_COMPATIBILITY
/* cfront changes all bit fields to int or unsigned int in the generated
   C code. */
/* This feature CAN be changed when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT TARG_ALIGNOF_INT
#else /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
#if IA64_ABI
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT (-1)
#else /* !IA64_ABI */
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT 0
#endif /* IA64_ABI */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
#endif /* TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
			/* Default value, used to initialize global variable
			   targ_zero_width_bit_field_alignment. */
#endif /* ifndef TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT */

/* TRUE when a zero-width bit field, typically used to control the alignment
   of the next field, thereby also affects how the alignment of the struct
   as a whole is determined.  Should be TRUE for cfront and Microsoft ABI
   compatibility. */
#ifndef TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT
#if TARG_MICROSOFT_BIT_FIELD_ALLOCATION
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT TRUE
#else /* !TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
#if CFRONT_OBJECT_CODE_COMPATIBILITY && ABI_COMPATIBILITY_VERSION >= 232
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT TRUE
#else /* !CFRONT_OBJECT_CODE_COMPATIBILITY... */
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT FALSE
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY... */
#endif /* TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
			/* Default value, used to initialize global variable
			 targ_zero_width_bit_field_affects_struct_alignment. */
#endif /* ifndef TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT */

/* TRUE when an unnamed bit field, typically used to control the alignment
   of the next field, thereby also affects how the alignment of the struct
   as a whole is determined. */
#ifndef TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT
#if TARG_MICROSOFT_BIT_FIELD_ALLOCATION
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT TRUE
#else /* !TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
#if ABI_COMPATIBILITY_VERSION <= 231
/* Setting it to FALSE corresponds to hard-coded behavior prior to 2.32. */
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT FALSE
#else /* !(ABI_COMPATIBILITY_VERSION <= 231) */
#if ABI_COMPATIBILITY_VERSION >= 235
#if CFRONT_OBJECT_CODE_COMPATIBILITY
/* This can be changed. */
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT TRUE
#else /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
/* This can be changed. */
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT FALSE
#endif /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
#else /* !(ABI_COMPATIBILITY_VERSION >= 235) */
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT TRUE
#endif /* ABI_COMPATIBILITY_VERSION >= 235 */
#endif /* ABI_COMPATIBILITY_VERSION <= 231 */
#endif /* TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
			/* Default value, used to initialize global variable
			   targ_unnamed_bit_field_affects_struct_alignment. */
#endif /* ifndef TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT */

/*
Flag that is TRUE if bit fields (specifically, their container types) can
affect the alignment of a union type.  When this FLAG is FALSE, the container
type still affects the union's size, but not its alignment (this reflects the
behavior of Microsoft compilers).  This is the initial value of
targ_bit_field_affects_union_alignment.
*/
#ifndef TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT
#if TARG_MICROSOFT_BIT_FIELD_ALLOCATION
#define TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT FALSE
#else /* !TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
#define TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT TRUE
#endif /* TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
#endif /* ifndef TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT */

/*
Flag that is TRUE if "#pragma pack(n)" and the command-line option
"--pack_alignment=n", when supported, affect the container boundary/alignment
of bit fields.  FALSE indicates that TARG_BIT_FIELD_CONTAINER_SIZE controls
the container boundary/alignment at all times.
*/
#ifndef TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS TRUE
#endif /* ifndef TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS */

/*
Flag that is TRUE if "#pragma pack(n)" and the command-line option
"--pack_alignment=n", when supported, affects the alignment of base classes.
(In versions prior to 4.2, base class subobjects were not affected.)  This is
the initial value of the global variable packing_applies_to_base_classes.
*/
#ifndef TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES
#if ABI_COMPATIBILITY_VERSION >= 402
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES TRUE
#else /* !(ABI_COMPATIBILITY_VERSION >= 402) */
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES FALSE
#endif /* ABI_COMPATIBILITY_VERSION >= 402 */
#endif /* ifndef TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES */

/*
Flag that is TRUE if bit fields longer than their base types are
padded out to the full declared length.  FALSE means allocate only as
many bits as are in the base type.  In either case, the bit field
itself has the same number of bits; the extra bits are padding bits.

cfront did not allow such long bit fields, so the setting here is
irrelevant for cfront object code compatibility.
*/
#ifndef TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE
#if ABI_COMPATIBILITY_VERSION < 301
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE FALSE
#else /* ABI_COMPATIBILITY_VERSION >= 301 */
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE TRUE
#endif /* ABI_COMPATIBILITY_VERSION < 301 */
#endif /* ifndef TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE */

/*
Wide character constant type (wchar_t, see stddef.h and stdlib.h).
*/
#ifndef TARG_WCHAR_T_INT_KIND
#define TARG_WCHAR_T_INT_KIND ((an_integer_kind)ik_unsigned_short)
			/* Default value, used to initialize global variable
			   targ_wchar_t_int_kind. */
#endif /* !defined(TARG_WCHAR_T_INT_KIND) */

/*
wint_t is a standard C99 typedef describing an integer type that can represent
all the members of the extended character set, plus at least one extra value
(for the WEOF macro).  See subsection 7.24.1 in the C99 standard (ISO/IEC
9899:1999).
*/
#ifndef TARG_WINT_T_INT_KIND
#define TARG_WINT_T_INT_KIND TARG_WCHAR_T_INT_KIND
			/* Default value, used to initialize global variable
			   targ_wint_t_int_kind. */
#endif /* !defined(TARG_WINT_T_INT_KIND) */

/*
Configuration of char16_t and char32_t (C extensions introduced by TR 19769).
*/
#ifndef TARG_CHAR16_T_INT_KIND
#define TARG_CHAR16_T_INT_KIND  ((an_integer_kind)ik_unsigned_short)
#endif /* !defined(TARG_CHAR16_T_INT_KIND) */

#ifndef TARG_CHAR32_T_INT_KIND
#define TARG_CHAR32_T_INT_KIND  ((an_integer_kind)ik_unsigned_int)
#endif /* !defined(TARG_CHAR32_T_INT_KIND) */

/*
Macro that determines how to encode a 32-bit character code (e.g., from a
\Uxxxxxxxx universal character form) to char16_t character values.
Invoking the macro should produce the number of values needed by the encoding
or zero if the encoding failed.  The default is UTF-16 encoding.
*/
#ifndef encode_in_char16_t
#ifdef ENCODE_IN_CHAR16_T
/* The old spelling of this macro used all capital letters, and there is
   a definition with that spelling; just map the macro to that definition. */
#define encode_in_char16_t ENCODE_IN_CHAR16_T
#else /* !defined(ENCODE_IN_CHAR16_T) */
#define encode_in_char16_t(code, p_char16_t_vals) \
  ucn_to_utf16((code), (p_char16_t_vals))
#endif /* defined(ENCODE_IN_CHAR16_T) */
#endif /* !defined(encode_in_char16_t) */


/*
The maximum number of char16_t characters required to encode a 32-bit
character code.
*/
#ifndef MAX_CHAR16_T_ENCODING_LENGTH
#define MAX_CHAR16_T_ENCODING_LENGTH 2
#endif /* !defined(MAX_CHAR16_T_ENCODING_LENGTH) */

/*
Integral kind to be used for the bool type in C++.
*/
#ifndef TARG_BOOL_INT_KIND
#define TARG_BOOL_INT_KIND ((an_integer_kind)ik_char)
			/* Default value, used to initialize global variable
			   targ_bool_int_kind.  See BOOL_INT_KIND. */
#endif /* !defined(TARG_BOOL_INT_KIND) */

/*
Integral kind to be used for the _Bool type in C (the C99 and C11 standard
require this to be an unsigned type).
*/
#ifndef TARG_C_BOOL_INT_KIND
#define TARG_C_BOOL_INT_KIND ((an_integer_kind)ik_unsigned_char)
			/* Default value, used to initialize global variable
			   targ_c_bool_int_kind.  See BOOL_INT_KIND. */
#endif /* !defined(TARG_C_BOOL_INT_KIND) */

/*
Pointer types:
*/
#if NEAR_AND_FAR_ALLOWED
/*
Sizes of near/far pointers (e.g., in 16-bit Microsoft mode; note that these
values are not used in 32-bit Microsoft mode).
*/
#ifndef TARG_SIZEOF_FAR_POINTER
#define TARG_SIZEOF_FAR_POINTER 4
#endif /* ifndef TARG_SIZEOF_FAR_POINTER */
#ifndef TARG_ALIGNOF_FAR_POINTER
#define TARG_ALIGNOF_FAR_POINTER 4
#endif /* ifndef TARG_ALIGNOF_FAR_POINTER */
#ifndef TARG_SIZEOF_NEAR_POINTER
#define TARG_SIZEOF_NEAR_POINTER 2
#endif /* ifndef TARG_SIZEOF_NEAR_POINTER */
#ifndef TARG_ALIGNOF_NEAR_POINTER
#define TARG_ALIGNOF_NEAR_POINTER 2
#endif /* ifndef TARG_ALIGNOF_NEAR_POINTER */
#endif /* NEAR_AND_FAR_ALLOWED */

/*
Are all pointers the same size?

If not, see also size_of_pointer_to, and you may want to define
pointer_types_have_same_repr if there's some aspect of pointer representation
that is not completely determined by the type pointed to (e.g., you have
both 32-bit and 64-bit pointers, for any underlying type, and you can choose
between them with some language extension).

Note that TARG_ALL_POINTERS_SAME_SIZE is not consulted when near and far
pointers exist (e.g., in 16-bit Microsoft mode).  Similarly, it does not
affect the size of pointers explicitly sized with the Microsoft __ptr32 and
__ptr64 modifiers.  So this really means, "ignoring near/far mode and
__ptr32/__ptr64 modifiers, are all pointers the same size?"
*/
#ifndef TARG_ALL_POINTERS_SAME_SIZE
#define TARG_ALL_POINTERS_SAME_SIZE TRUE
#endif /* !defined(TARG_ALL_POINTERS_SAME_SIZE) */

#if TARG_ALL_POINTERS_SAME_SIZE
/*
All pointers have the same size and alignment, so TARG_SIZEOF_POINTER and
TARG_ALIGNOF_POINTER should be defined.
*/
#ifndef TARG_SIZEOF_POINTER
#if NEAR_AND_FAR_ALLOWED
#define TARG_SIZEOF_POINTER TARG_SIZEOF_FAR_POINTER
#else /* !NEAR_AND_FAR_ALLOWED */
#define TARG_SIZEOF_POINTER 4
#endif /* NEAR_AND_FAR_ALLOWED */
			/* Default value, used to initialize global variable
			   targ_sizeof_pointer. */
#endif /* !defined(TARG_SIZEOF_POINTER) */
#ifndef TARG_ALIGNOF_POINTER
#if NEAR_AND_FAR_ALLOWED
#define TARG_ALIGNOF_POINTER TARG_ALIGNOF_FAR_POINTER
#else /* !NEAR_AND_FAR_ALLOWED */
#define TARG_ALIGNOF_POINTER 4
#endif /* NEAR_AND_FAR_ALLOWED */
			/* Default value, used to initialize global variable
			   targ_alignof_pointer. */
#endif /* !defined(TARG_ALIGNOF_POINTER) */
#else /* !TARG_ALL_POINTERS_SAME_SIZE */
/*
Pointers may have different sizes and alignments, so TARG_SIZEOF_POINTER and
TARG_ALIGNOF_POINTER are meaningless.  Consequently, all other definitions
that depend on TARG_SIZEOF_POINTER and TARG_ALIGNOF_POINTER need to be
configured in other terms, and it also means that the values of global
variables targ_sizeof_pointer and targ_alignof_pointer are meaningless.
*/
#ifndef TARG_SIZEOF_POINTER
#define TARG_SIZEOF_POINTER 0 /* Arbitrary -- should not be used. */
#endif /* !defined(TARG_SIZEOF_POINTER) */
#ifndef TARG_ALIGNOF_POINTER
#define TARG_ALIGNOF_POINTER 0 /* Arbitrary -- should not be used. */
#endif /* !defined(TARG_ALIGNOF_POINTER) */
#endif /* TARG_ALL_POINTERS_SAME_SIZE */

#if UPC_EXTENSIONS_ALLOWED
/*
The maximum UPC block size (possibly limited by the target representation of a
shared pointer).
*/
#ifndef MAX_UPC_BLOCK_SIZE
#define MAX_UPC_BLOCK_SIZE ((a_upc_block_size)(INT_MAX/2))
#endif /* defined(MAX_UPC_BLOCK_SIZE) */
#endif /* UPC_EXTENSIONS_ALLOWED */

/* Indication of whether NULL pointer is like integer zero. */
#ifndef TARG_NULL_IS_ALL_BITS_ZERO
#define TARG_NULL_IS_ALL_BITS_ZERO TRUE
#endif /* ifndef TARG_NULL_IS_ALL_BITS_ZERO */

/* Integer type for the difference of two pointer types (ptrdiff_t).
   This type must be signed.  See 3.3.6 in the standard and the header
   file <stddef.h>. */
typedef a_host_large_integer a_targ_ptrdiff_t;  /* Must be
                                                   a_host_large_integer. */

/* Pick a typical representation for ptrdiff_t: the smaller of int or long
   that can hold a pointer value. */
#ifndef TARG_PTRDIFF_T_INT_KIND
#if TARG_ALL_POINTERS_SAME_SIZE
/* Pointers all have the same size. */
#if TARG_SIZEOF_POINTER <= TARG_SIZEOF_INT
#define TARG_PTRDIFF_T_INT_KIND ((an_integer_kind)ik_int)
#else /* TARG_SIZEOF_POINTER > TARG_SIZEOF_INT */
#define TARG_PTRDIFF_T_INT_KIND ((an_integer_kind)ik_long)
#endif /* TARG_SIZEOF_POINTER <= TARG_SIZEOF_INT */
#else /* !TARG_ALL_POINTERS_SAME_SIZE */
/* Pointers have different sizes -- use of long is arbitrary. */
#define TARG_PTRDIFF_T_INT_KIND ((an_integer_kind)ik_long)
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
			/* Default value, used to initialize global variable
			   targ_ptrdiff_t_int_kind. */
#endif /* ifndef TARG_PTRDIFF_T_INT_KIND */

/* size_t, used for size of arrays, offsets in fields, type of sizeof, etc.
   This type must be unsigned.  See 3.3.3.4 in the standard and the header
   file <stddef.h>. */
/* a_targ_size_t is the container used to hold size_t values on the host.
   It must be large enough to hold all the target size_t values, but can
   be larger. */
typedef a_host_large_unsigned a_targ_size_t;  /* Must be
						 a_host_large_unsigned. */
#ifndef TARG_SIZE_T_INT_KIND
/* Pick a typical representation for size_t: the smaller of unsigned int or
   unsigned long that can hold a pointer value. */
#if TARG_ALL_POINTERS_SAME_SIZE
/* Pointers all have the same size. */
#if TARG_SIZEOF_POINTER <= TARG_SIZEOF_INT
#define TARG_SIZE_T_INT_KIND ((an_integer_kind)ik_unsigned_int)
#ifndef TARG_SIZE_T_MAX
#define TARG_SIZE_T_MAX ((a_targ_size_t)UINT_MAX)
#endif /* ifndef TARG_SIZE_T_MAX */
#else /* TARG_SIZEOF_POINTER > TARG_SIZEOF_INT */
#define TARG_SIZE_T_INT_KIND ((an_integer_kind)ik_unsigned_long)
#ifndef TARG_SIZE_T_MAX
#define TARG_SIZE_T_MAX ((a_targ_size_t)ULONG_MAX)
#endif /* ifndef TARG_SIZE_T_MAX */
#endif /* TARG_SIZEOF_POINTER <= TARG_SIZEOF_INT */
#else /* !TARG_ALL_POINTERS_SAME_SIZE */
/* Pointers have different sizes -- use of unsigned long is arbitrary. */
#define TARG_SIZE_T_INT_KIND ((an_integer_kind)ik_unsigned_long)
#ifndef TARG_SIZE_T_MAX
#define TARG_SIZE_T_MAX ((a_targ_size_t)ULONG_MAX)
#endif /* ifndef TARG_SIZE_T_MAX */
#endif /* TARG_ALL_POINTERS_SAME_SIZE */
			/* Default value, used to initialize global variable
			   targ_size_t_int_kind. */
#endif /* ifndef TARG_SIZE_T_INT_KIND */
/* TARG_SIZE_T_MAX defines the limit of the host representation
   of size_t constants; the range it defines can be equal to or smaller
   than the integer size implied by TARG_SIZE_T_INT_KIND. */
#ifndef TARG_SIZE_T_MAX
#define TARG_SIZE_T_MAX ((a_targ_size_t)ULONG_MAX)
			/* Default value, used to initialize global variable
			   targ_size_t_max. */
#endif /* ifndef TARG_SIZE_T_MAX */

#if GNU_EXTENSIONS_ALLOWED
/* TARG_SSIZE_T_INT_KIND determines the representation of the POSIX ssize_t
   type (a signed type used to count bytes for certain I/O functions).  It is
   typically identical to ptrdiff_t.  Currently only used to predeclare
   certain GNU __builtin_xyz functions. */
#ifndef TARG_SSIZE_T_INT_KIND
#define TARG_SSIZE_T_INT_KIND TARG_PTRDIFF_T_INT_KIND
#endif /* ifndef TARG_SSIZE_T_INT_KIND */
#endif /* GNU_EXTENSIONS_ALLOWED */

/* Specification of a target alignment requirement.  1 means no alignment
   requirement.  (TARG_MAXIMUM_PACK_ALIGNMENT must fit in this type.)
   Making the size of TYPE_FOR_TARG_ALIGNMENT larger than one may cause an
   increase of the sizes of some IL entries (like a_type and a_field). */
#ifndef TYPE_FOR_TARG_ALIGNMENT
#define TYPE_FOR_TARG_ALIGNMENT a_byte
#endif /* ifndef TYPE_FOR_TARG_ALIGNMENT */

typedef TYPE_FOR_TARG_ALIGNMENT a_targ_alignment;

/*
Float types:
*/
/* Remember that the size of a type must be a multiple of the alignment. */
#ifndef TARG_SIZEOF_FLOAT
#define TARG_SIZEOF_FLOAT 4
			/* Default value, used to initialize global variable
			   targ_sizeof_float. */
#endif /* !defined(TARG_SIZEOF_FLOAT) */
#ifndef TARG_ALIGNOF_FLOAT
#define TARG_ALIGNOF_FLOAT 4
			/* Default value, used to initialize global variable
			   targ_alignof_float. */
#endif /* !defined(TARG_ALIGNOF_FLOAT) */
#ifndef TARG_SIZEOF_DOUBLE
#define TARG_SIZEOF_DOUBLE 8
			/* Default value, used to initialize global variable
			   targ_sizeof_double. */
#endif /* !defined(TARG_SIZEOF_DOUBLE) */
#ifndef TARG_ALIGNOF_DOUBLE
#define TARG_ALIGNOF_DOUBLE 8
			/* Default value, used to initialize global variable
			   targ_alignof_double. */
#endif /* !defined(TARG_ALIGNOF_DOUBLE) */

#ifndef TARG_SIZEOF_LONG_DOUBLE
/* Determine the size of long double based on the number of mantissa digits.
   This is used to initialize global variable targ_sizeof_long_double. */
#if TARG_LDBL_MANT_DIG == 64
#define TARG_SIZEOF_LONG_DOUBLE 12
#else /* !(TARG_LDBL_MANT_DIG == 64) */
#if TARG_LDBL_MANT_DIG == 113
#define TARG_SIZEOF_LONG_DOUBLE 16
#else /* !(TARG_LDBL_MANT_DIG == 113) */
#if TARG_LDBL_MANT_DIG == 53
#define TARG_SIZEOF_LONG_DOUBLE 8
#else /* !(TARG_LDBL_MANT_DIG == 53) */
 #error Cannot determine default long double size based on TARG_LDBL_MANT_DIG
#endif /* TARG_LDBL_MANT_DIG == 53 */
#endif /* TARG_LDBL_MANT_DIG == 113 */
#endif /* TARG_LDBL_MANT_DIG == 64 */
#endif /* !defined(TARG_SIZEOF_LONG_DOUBLE) */

#ifndef TARG_ALIGNOF_LONG_DOUBLE
#define TARG_ALIGNOF_LONG_DOUBLE 8
			/* Default value, used to initialize global variable
			   targ_alignof_long_double. */
#endif /* !defined(TARG_ALIGNOF_LONG_DOUBLE) */

#ifndef TARG_SIZEOF_FLOAT80
#define TARG_SIZEOF_FLOAT80 16
			/* Default value, used to initialize global variable
			   targ_sizeof_float80. */
#endif /* !defined(TARG_SIZEOF_FLOAT80) */
#ifndef TARG_ALIGNOF_FLOAT80
#define TARG_ALIGNOF_FLOAT80 16
			/* Default value, used to initialize global variable
			   targ_alignof_float80. */
#endif /* !defined(TARG_ALIGNOF_FLOAT80) */

#ifndef TARG_SIZEOF_FLOAT128
#define TARG_SIZEOF_FLOAT128 16
			/* Default value, used to initialize global variable
			   targ_sizeof_float128. */
#endif /* !defined(TARG_SIZEOF_FLOAT128) */
#ifndef TARG_ALIGNOF_FLOAT128
#define TARG_ALIGNOF_FLOAT128 16
			/* Default value, used to initialize global variable
			   targ_alignof_float128. */
#endif /* !defined(TARG_ALIGNOF_FLOAT128) */

/*
Historically, the front end has relied on the floating-point capability of
the host to do floating-point arithmetic.  That doesn't work well if, e.g.,
the target supports 128-bit floating-point and the host does not.  In cases
like this, the Berkley SoftFloat library
(http://www.jhauser.us/arithmetic/SoftFloat-3/doc/SoftFloat.html) can be
used to perform 128-bit floating-point operations in software.  When
USE_SOFTFLOAT is TRUE, the SoftFloat library (release 3e or later) is used
for floating-point operations.  The SoftFloat package must be obtained
and compiled separately and provisions made in the build process to make the
SoftFloat include files and library available to the front end.

Note that the SoftFloat floating-point to integer conversion routines can
handle at most 64-bit integers, so if the host integer is larger than that,
unnecessary truncation may occur.
*/
#ifndef USE_SOFTFLOAT
#define USE_SOFTFLOAT FALSE
#endif /* ifndef USE_SOFTFLOAT */

/*
This macro controls whether the front end uses the floating-point conversion
routines provided by the host compiler's standard library (e.g., strtod and
snprintf/sprintf_s) or whether the front end's internal routines should be
used.  The internal routines provide correctly-rounded decimal-to-binary and
binary-to-decimal conversions.  Using the internal routines is typically slower
as the conversion process is performed in software with integer arithmetic (but
see FP_USE_EMULATION in floating.h).  Setting USE_HOST_FP_CONVERSION_ROUTINES
to TRUE uses the host library routines; a setting of FALSE uses the internal
routines.  When setting this macro to FALSE, make sure the configuration macros
for FP_LONG_DOUBLE_IS_* are set properly (see floating.h).  The internal
routines will be used for converting floating point values to and from string
form when USE_FLOAT128_FOR_HOST_FP_VALUE is TRUE and USE_QUADMATH_LIBRARY is
FALSE, regardless of the setting of USE_HOST_FP_CONVERSION_ROUTINES, because
there are no portable host routines to perform those conversions with full
precision.
*/
#ifndef USE_HOST_FP_CONVERSION_ROUTINES
#if USE_SOFTFLOAT
#define USE_HOST_FP_CONVERSION_ROUTINES FALSE
#else /* !USE_SOFTFLOAT */
#define USE_HOST_FP_CONVERSION_ROUTINES TRUE
#endif /* USE_SOFTFLOAT */
#endif /* ifndef USE_HOST_FP_CONVERSION_ROUTINES */

#if USE_SOFTFLOAT && USE_HOST_FP_CONVERSION_ROUTINES
 #error -- must use internal floating-point conversion routines with SoftFloat
#endif /* USE_SOFTFLOAT && USE_HOST_FP_CONVERSION_ROUTINES */

#if USE_SOFTFLOAT
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
extern "C" {
#include "softfloat.h"
}  /* extern "C" */
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */

/*
Use SoftFloat's float128_t type as the host's floating-point internal
representation (it is available on all platforms and is at least as big as
the largest target floating-point representation currently supported).
*/
typedef float128_t a_host_fp_value;

#undef USE_DOUBLE_FOR_HOST_FP_VALUE
#define USE_DOUBLE_FOR_HOST_FP_VALUE 0
#undef USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
#define USE_LONG_DOUBLE_FOR_HOST_FP_VALUE 0
#undef USE_FLOAT128_FOR_HOST_FP_VALUE
#define USE_FLOAT128_FOR_HOST_FP_VALUE 0

#else /* !USE_SOFTFLOAT */
/* Host floating-point routines are being used. */

/*
Configure the type used to perform host floating point computations.  To
support __float128, that type should be __float128.  Otherwise, long double
should be used if possible.  On hosts where long double and double have the
same precision (and __float128 is not available), type double can be used
instead.

Ideally, a configuration should set one of USE_FLOAT128_FOR_HOST_FP_VALUE,
USE_LONG_DOUBLE_FOR_HOST_FP_VALUE, or USE_DOUBLE_FOR_HOST_FP_VALUE to TRUE.
However, older versions of the front end did not support __float128 and the
single macro USE_LONG_DOUBLE_FOR_HOST_FP_VALUE decided whether to use double
(when the macro was FALSE) or long double (when it was TRUE).  To maintain
backward compatibility with older settings, USE_DOUBLE_FOR_HOST_FP_VALUE is
set implicitly to TRUE if no other type is selected and
USE_LONG_DOUBLE_FOR_HOST_FP_VALUE is configured to FALSE.
*/
#undef HOST_FP_TYPE_SELECTED

#if defined(USE_FLOAT128_FOR_HOST_FP_VALUE)
#if USE_FLOAT128_FOR_HOST_FP_VALUE
/* Use either __float128 or _Float128, depending on the platform. */
#if defined(__x86_64) || defined(__i386)
typedef __float128 a_host_fp_value;
#else /* !(defined(__x86_64) || defined(__i386)) */
typedef _Float128 a_host_fp_value;
#endif /* defined(__x86_64) || defined(__i386) */
#define HOST_FP_TYPE_SELECTED TRUE
#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE */
#else /* !defined(USE_FLOAT128_FOR_HOST_FP_VALUE) */
#define USE_FLOAT128_FOR_HOST_FP_VALUE FALSE
#endif /* defined(USE_FLOAT128_FOR_HOST_FP_VALUE) */

#if defined(USE_DOUBLE_FOR_HOST_FP_VALUE)
#if USE_DOUBLE_FOR_HOST_FP_VALUE
#if defined(HOST_FP_TYPE_SELECTED)
 #error -- more than one host floating-point value type selected
#else /* !defined(HOST_FP_TYPE_SELECTED) */
typedef double a_host_fp_value;
#define HOST_FP_TYPE_SELECTED TRUE
#endif /* defined(HOST_FP_TYPE_SELECTED) */
#endif /* USE_DOUBLE_FOR_HOST_FP_VALUE */
#else /* !defined(USE_DOUBLE_FOR_HOST_FP_VALUE) */
#define USE_DOUBLE_FOR_HOST_FP_VALUE FALSE
#endif /* defined(USE_DOUBLE_FOR_HOST_FP_VALUE) */


#if defined(USE_LONG_DOUBLE_FOR_HOST_FP_VALUE)
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
#if defined(HOST_FP_TYPE_SELECTED)
 #error -- more than one host floating-point value type selected
#else /* !defined(HOST_FP_TYPE_SELECTED) */
typedef long double a_host_fp_value;
#define HOST_FP_TYPE_SELECTED TRUE
#endif /* defined(HOST_FP_TYPE_SELECTED) */
#else /* !USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
#if !defined(HOST_FP_TYPE_SELECTED)
/* Implicitly select "double". */
#undef USE_DOUBLE_FOR_HOST_FP_VALUE
#define USE_DOUBLE_FOR_HOST_FP_VALUE TRUE
typedef double a_host_fp_value;
#define HOST_FP_TYPE_SELECTED FALSE
#endif /* !defined(HOST_FP_TYPE_SELECTED) */
#endif /* USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
#else /* !defined(USE_LONG_DOUBLE_FOR_HOST_FP_VALUE) */
#define USE_LONG_DOUBLE_FOR_HOST_FP_VALUE FALSE
#endif /* defined(USE_LONG_DOUBLE_FOR_HOST_FP_VALUE) */

/*
If no host floating-point type is configured use "long double" with ISO C
compilers, and "double" with pre-ISO compilers.
*/
#if !defined(HOST_FP_TYPE_SELECTED)
#if USING_ISO_C
#undef USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
#define USE_LONG_DOUBLE_FOR_HOST_FP_VALUE TRUE
typedef long double a_host_fp_value;
#else /* !USING_ISO_C */
#undef USE_DOUBLE_FOR_HOST_FP_VALUE
#define USE_DOUBLE_FOR_HOST_FP_VALUE TRUE
typedef double a_host_fp_value;
#endif /* !USING_ISO_C */
#define HOST_FP_TYPE_SELECTED FALSE
#endif /* !defined(HOST_FP_TYPE_SELECTED) */


#undef HOST_FP_TYPE_SELECTED
#endif /* USE_SOFTFLOAT */

/*
By default, all floating-point configurations have at least a "float" and
"double" type.  Additional types can be configured as needed.  This macro
controls whether or not a "long double" type is used.
*/

#ifndef FP_HAS_LONG_DOUBLE
#define FP_HAS_LONG_DOUBLE !USE_DOUBLE_FOR_HOST_FP_VALUE
#endif /* ifndef FP_HAS_LONG_DOUBLE */

/*
An internal macro used to signify that the size of a_host_fp_value is 128 bits.
*/
#ifndef HOST_FP_VALUE_IS_128BIT
#if USE_FLOAT128_FOR_HOST_FP_VALUE || USE_SOFTFLOAT
#define HOST_FP_VALUE_IS_128BIT TRUE
#else /* !(USE_FLOAT128_FOR_HOST_FP_VALUE || USE_SOFTFLOAT) */
#define HOST_FP_VALUE_IS_128BIT FALSE
#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE || USE_SOFTFLOAT */
#endif /* defined(HOST_FP_VALUE_IS_128BIT) */

/*
TRUE if the front end can use the GNU QuadMath library to support operations
on __float128 (in configurations where a_host_hp_value is __float128).
*/
#ifndef USE_QUADMATH_LIBRARY
#define USE_QUADMATH_LIBRARY FALSE
#endif /* USE_QUADMATH_LIBRARY */

/*
TRUE if the front end can approximate some conversions involving __float128
with long double conversions instead (in configurations where a_host_fp_value
is __float128).
*/
#ifndef APPROXIMATE_QUADMATH
#define APPROXIMATE_QUADMATH FALSE
#endif /* APPROXIMATE_QUADMATH */

#if USE_FLOAT128_FOR_HOST_FP_VALUE
#if !USE_QUADMATH_LIBRARY && !APPROXIMATE_QUADMATH
 #error -- either USE_QUADMATH_LIBRARY or APPROXIMATE_QUADMATH must be TRUE
#endif /* !USE_QUADMATH_LIBRARY && !APPROXIMATE_QUADMATH */
#if USE_QUADMATH_LIBRARY && APPROXIMATE_QUADMATH
 #error -- USE_QUADMATH_LIBRARY and APPROXIMATE_QUADMATH cannot both be TRUE
#endif /* USE_QUADMATH_LIBRARY && APPROXIMATE_QUADMATH */
#else /* !USE_FLOAT_128_FOR_HOST_FP_VALUE */
#if APPROXIMATE_QUADMATH
  #error -- APPROXIMATE_QUADMATH can only be TRUE if \
            USE_FLOAT128_FOR_HOST_FP_VALUE is TRUE.
#endif /* APPROXIMATE_QUADMATH */
#if USE_QUADMATH_LIBRARY
  #error -- USE_QUADMATH_LIBRARY can only be TRUE if \
            USE_FLOAT128_FOR_HOST_FP_VALUE is TRUE.
#endif /* USE_QUADMATH_LIBRARY */
#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE */


/*
TRUE if the target supports IEEE floating point, i.e., it has NaNs
and Infinities.  Note that unless float_pt.c is rewritten this also
implies that the host supports IEEE floating point, because the
default float_pt.c support uses the host floating point.
*/
#ifndef TARG_HAS_IEEE_FLOATING_POINT
#if defined(__sparc) || defined(__linux__) || EDG_WIN32 || defined(__i386)
/* SPARC supports IEEE floating point. */
/* Linux (X86, Alpha, PowerPC, SPARC) supports IEEE floating point. */
/* Windows X86 supports IEEE floating point. */
/* Other Unix systems on X86 architecture, which define __i386, support
   IEEE floating point. */
#define TARG_HAS_IEEE_FLOATING_POINT TRUE
#else /* !defined(__sparc) || ... */
/* Include <math.h> to see if the C99 NAN macro is defined. */
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include <math.h>
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
#ifdef NAN
/* Systems with NAN defined support IEEE floating point. */
#define TARG_HAS_IEEE_FLOATING_POINT TRUE
#else /* !defined(NAN) */
#define TARG_HAS_IEEE_FLOATING_POINT FALSE
#endif /* ifdef NAN */
#endif /* defined(__sparc) || ... */
#endif /* ifndef TARG_HAS_IEEE_FLOATING_POINT */

/*
Flags that control whether the type specifiers __float80 and/or __float128 can
be enabled.  (If true, they will, e.g., be enabled in GNU modes.)  Actually
enabling these extensions is achieved by setting the global variables
float80_enabled and float128_enabled to TRUE.
*/
#ifndef FLOAT80_ENABLING_POSSIBLE
#define FLOAT80_ENABLING_POSSIBLE FALSE
#endif /* ifndef FLOAT80_ENABLING_POSSIBLE */

#ifndef FLOAT128_ENABLING_POSSIBLE
#define FLOAT128_ENABLING_POSSIBLE FALSE
#endif /* ifndef FLOAT128_ENABLING_POSSIBLE */

#if FLOAT128_ENABLING_POSSIBLE && !HOST_FP_VALUE_IS_128BIT
 #error -- __float128 support requires 128-bit host floating-point type
#endif /* FLOAT128_ENABLING_POSSIBLE && !HOST_FP_VALUE_IS_128BIT */


/*
Default floating-point representations for __float80 and __float128.  Some
compilers make __float80 equivalent to "long double" on some common platforms,
for example (made the default here by setting DEFAULT_FLOAT_KIND_FOR_FLOAT80
to fk_long_double rather than fk_float80).  Used as default initializers for
the global variables float_kind_for_float80 and float_kind_for_float128.
*/
#ifndef DEFAULT_FLOAT_KIND_FOR_FLOAT80
#define DEFAULT_FLOAT_KIND_FOR_FLOAT80 fk_long_double
#endif /* ifndef DEFAULT_FLOAT_KIND_FOR_FLOAT80 */

#ifndef DEFAULT_FLOAT_KIND_FOR_FLOAT128
#define DEFAULT_FLOAT_KIND_FOR_FLOAT128 fk_float128
#endif /* ifndef DEFAULT_FLOAT_KIND_FOR_FLOAT128 */


/*
In a configuration that uses the SoftFloat package, _Float16 values will be
represented internally using the SoftFloat float16_t type.  Otherwise, if
the host compiler supports the _Float16 type, that type will be used for
the internal representation of _Float16 values.  If neither SoftFloat nor
the _Float16 type is available, _Float16 values will be represented
internally using the host "float" type.
*/
#ifndef HOST_HAS_FLOAT16_TYPE
#if defined(__FLT16_MANT_DIG__)
/* Both gcc and clang define this macro if _Float16 is available and not
   otherwise. */
#define HOST_HAS_FLOAT16_TYPE TRUE
#else /* !defined(__FLT16_MANT_DIG__) */
#define HOST_HAS_FLOAT16_TYPE FALSE
#endif /* defined(__FLT16_MANT_DIG__) */
#endif /* ifndef HOST_HAS_FLOAT16_TYPE */


/*
Type used to represent float quantities internally:

(This should not be an array, so that it's always known whether one gets the
address or the value of the item.)
*/
typedef struct an_internal_float_value {
  /* The type here must match the code in float_pt.c.  Enough storage is
     allocated for the largest floating-point type, though the floating-point
     value may take less than that (e.g., a value with "float" type will use
     only 32 bits of the storage). */
  a_byte bytes[sizeof(a_host_fp_value)];
  /*lint -esym(768,an_internal_float_value::bytes)*/
} an_internal_float_value;


/*
The floating-point manipulation routines require an unsigned 32-bit
type.  The macro TYPE_FOR_AN_FP_VALUE_PART can be used to specify the
type to be used.
*/
#ifndef TYPE_FOR_AN_FP_VALUE_PART
#define TYPE_FOR_AN_FP_VALUE_PART uint32_t
#endif /* ifndef TYPE_FOR_AN_FP_VALUE_PART */

typedef	TYPE_FOR_AN_FP_VALUE_PART an_fp_value_part;

#if FIXED_POINT_ALLOWED

/*
Flag that is TRUE if it is okay to use a host floating-point value that
is not large enough to represent all of the bits of the largest fixed-point
type.  Note that if you set this flag (to suppress the #error directive
below) you will get imprecise conversion of some fixed-point constants and
imprecise results of some fixed-point folding operations.  This flag must
be set when building the front end with a compiler on which double and long
double have the same size, and that size is a normal 8 byte double (for
example, the Microsoft compiler).
*/
#ifndef ALLOW_HOST_FP_TOO_SMALL_FOR_LARGEST_FIXED_POINT_TYPE
#define ALLOW_HOST_FP_TOO_SMALL_FOR_LARGEST_FIXED_POINT_TYPE FALSE
#endif /* ifndef ALLOW_HOST_FP_TOO_SMALL_FOR_LARGEST_FIXED_POINT_TYPE */

/*
When fixed-point is enabled, make sure a host floating-point value has enough
mantissa digits to represent all of the bits of the largest fixed-point type.
*/
#if !ALLOW_HOST_FP_TOO_SMALL_FOR_LARGEST_FIXED_POINT_TYPE
#if USE_DOUBLE_FOR_HOST_FP_VALUE
#if DBL_MANT_DIG < (TARG_SIZEOF_UNSIGNED_LONG_ACCUM * CHAR_BIT)
 #error -- double is not large enough to represent a long _Accum value
#endif /* DBL_MANT_DIG < (TARG_SIZEOF_UNSIGNED_LONG_ACCUM * CHAR_BIT) */
#endif /* USE_DOUBLE_FOR_HOST_FP_VALUE */
#if USE_LONG_DOUBLE_FOR_HOST_FP_VALUE
#if LDBL_MANT_DIG < (TARG_SIZEOF_UNSIGNED_LONG_ACCUM * CHAR_BIT)
 #error -- long double is not large enough to represent a long _Accum value
#endif /* LDBL_MANT_DIG < (TARG_SIZEOF_UNSIGNED_LONG_ACCUM * CHAR_BIT) */
#endif /*  USE_LONG_DOUBLE_FOR_HOST_FP_VALUE */
#if USE_FLOAT128_FOR_HOST_FP_VALUE
#if TARG_FLT128_MANT_DIG < (TARG_SIZEOF_UNSIGNED_LONG_ACCUM * CHAR_BIT)
 #error -- __float128 is not large enough to represent a long _Accum value
#endif /* TARG_FLT128_MANT_DIG < ... */
#endif /*  USE_FLOAT128_FOR_HOST_FP_VALUE */
#endif /* !ALLOW_HOST_FP_TOO_SMALL_FOR_LARGEST_FIXED_POINT_TYPE */
#endif /* FIXED_POINT_ALLOWED */

/*
C++ pointer-to-member type.
(The formulas here are for a typical implementation, but are not required.)
Note that cfront uses "int *" for pointers to data members; we use an
integer the same size as a pointer.
*/
#if TARG_ALL_POINTERS_SAME_SIZE
/* Pointers all have the same size and alignment. */
#ifndef TARG_SIZEOF_PTR_TO_DATA_MEMBER
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER TARG_SIZEOF_POINTER
			/* Default value, used to initialize global variable
			   targ_sizeof_ptr_to_data_member. */
#endif /* !defined(TARG_SIZEOF_PTR_TO_DATA_MEMBER) */
#ifndef TARG_ALIGNOF_PTR_TO_DATA_MEMBER
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER TARG_ALIGNOF_POINTER
			/* Default value, used to initialize global variable
			   targ_alignof_ptr_to_data_member. */
#endif /* !defined(TARG_ALIGNOF_PTR_TO_DATA_MEMBER) */
#ifndef TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION
#if IA64_ABI
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION                            \
 (TARG_SIZEOF_POINTER+TARG_SIZEOF_POINTER)
#else /* !IA64_ABI */
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION                            \
  ((((2*TARG_SIZEOF_SHORT+TARG_SIZEOF_POINTER-1)/TARG_ALIGNOF_POINTER)+1)* \
    TARG_ALIGNOF_POINTER)
			/* Default value, used to initialize global variable
			   targ_sizeof_ptr_to_member_function. */
#endif /* IA64_ABI */
#endif /* !defined(TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION) */
#ifndef TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION TARG_ALIGNOF_POINTER
			/* Default value, used to initialize global variable
			   targ_alignof_ptr_to_member_function. */
#endif /* !defined(TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION) */
#else /* !TARG_ALL_POINTERS_SAME_SIZE */
/* Pointers have different sizes -- use of long is arbitrary. */
#ifndef TARG_SIZEOF_PTR_TO_DATA_MEMBER
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER TARG_SIZEOF_LONG
			/* Default value, used to initialize global variable
			   targ_sizeof_ptr_to_data_member. */
#endif /* !defined(TARG_SIZEOF_PTR_TO_DATA_MEMBER) */
#ifndef TARG_ALIGNOF_PTR_TO_DATA_MEMBER
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER TARG_ALIGNOF_LONG
			/* Default value, used to initialize global variable
			   targ_alignof_ptr_to_data_member. */
#endif /* !defined(TARG_ALIGNOF_PTR_TO_DATA_MEMBER) */
#ifndef TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION
#if IA64_ABI
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION                            \
          (TARG_SIZEOF_LONG+TARG_SIZEOF_LONG)
#else /* !IA64_ABI */
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION                            \
           (2*TARG_SIZEOF_SHORT+TARG_SIZEOF_LONG)
#endif /* IA64_ABI */
			/* Default value, used to initialize global variable
			   targ_sizeof_ptr_to_member_function. */
#endif /* !defined(TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION) */
#ifndef TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION TARG_ALIGNOF_LONG
			/* Default value, used to initialize global variable
			   targ_alignof_ptr_to_member_function. */
#endif /* !defined(TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION) */
#endif /* TARG_ALL_POINTERS_SAME_SIZE */

/*
Flag that is TRUE if the pointer-to-member representation should be assumed
to match that of the Microsoft compiler.  Microsoft makes the implementation
of a pointer-to-member dependent on the inheritance characteristics of the
parent class.  Specifically, the representation contains the following
  - For classes whose base classes are not yet known:
      pointer-to-data member: 3 int values
      pointer-to-function member: 1 function pointer + 3 int values
  - For classes with virtual bases:
      pointer-to-data member: 2 int values
      pointer-to-function member: 1 function pointer + 2 int values
  - For classes with multiple inheritance at any level:
      pointer-to-data member: 1 int value
      pointer-to-function member: 1 function pointer + 1 int value
  - For other classes:
      pointer-to-data member: 1 int value
      pointer-to-function member: 1 function pointer
When this flag is TRUE, pointer-to-member size configuration values like
TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION are ignored.  This macro is the initial
value for the global variable targ_microsoft_ptr_to_member_sizing.
*/
#ifndef TARG_MICROSOFT_PTR_TO_MEMBER_SIZING
#define TARG_MICROSOFT_PTR_TO_MEMBER_SIZING FALSE
#endif /* ifndef TARG_MICROSOFT_PTR_TO_MEMBER_SIZING */

/*
Currently, the Microsoft pointer-to-member representation is incompatible
with the ABIs supported by IL lowering.
*/
#if TARG_MICROSOFT_PTR_TO_MEMBER_SIZING && DO_IL_LOWERING
 #error -- TARG_MICROSOFT_PTR_TO_MEMBER_SIZING and DO_IL_LOWERING cannot both \
           be TRUE
#endif /* TARG_MICROSOFT_PTR_TO_MEMBER_SIZING && DO_IL_LOWERING */


/*
The standard disallows a cast of a pointer to member from a virtual base to
a derived base.  That is because such a cast requires extra information
in the runtime representation that's not present in some ABIs.  When this
flag is TRUE, we are told the runtime representation supports such casts
(or we're doing source analysis, and we don't care about the runtime
representation) and we allow the casts in Microsoft mode (the Microsoft
compiler allows these casts).
*/
#ifndef PTR_TO_MEMBER_REPR_SUPPORTS_CAST_FROM_VIRTUAL_BASE
#if DOING_SOURCE_ANALYSIS && MICROSOFT_EXTENSIONS_ALLOWED
#define PTR_TO_MEMBER_REPR_SUPPORTS_CAST_FROM_VIRTUAL_BASE TRUE
#else /* !(DOING_SOURCE_ANALYSIS && MICROSOFT_EXTENSIONS_ALLOWED) */
#define PTR_TO_MEMBER_REPR_SUPPORTS_CAST_FROM_VIRTUAL_BASE FALSE
#endif /* DOING_SOURCE_ANALYSIS && MICROSOFT_EXTENSIONS_ALLOWED */
#endif /* ifndef PTR_TO_MEMBER_REPR_SUPPORTS_CAST_FROM_VIRTUAL_BASE */

/* 
In C++ classes with virtual functions provide a special mechanism for
dynamic function binding.  Typically, this is a virtual function table,
and each object of the class contains a pointer to the table.  For each
class with virtual functions the front end allocates a field to contain
such a pointer -- or other data as required by a given implementation.
The size and alignment of such a field are defined by the following.
*/
#if TARG_ALL_POINTERS_SAME_SIZE
/* Pointers all have the same size and alignment. */
#ifndef TARG_SIZEOF_VIRTUAL_FUNCTION_INFO
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO TARG_SIZEOF_POINTER
			/* Default value, used to initialize global variable
			   targ_sizeof_virtual_function_info. */
#endif /* !defined(TARG_SIZEOF_VIRTUAL_FUNCTION_INFO) */
#ifndef TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO TARG_ALIGNOF_POINTER
			/* Default value, used to initialize global variable
			   targ_alignof_virtual_function_info. */
#endif /* !defined(TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO) */
#else /* !TARG_ALL_POINTERS_SAME_SIZE */
/* Pointers have different sizes -- use of long is arbitrary. */
#ifndef TARG_SIZEOF_VIRTUAL_FUNCTION_INFO
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO TARG_SIZEOF_LONG
			/* Default value, used to initialize global variable
			   targ_sizeof_virtual_function_info. */
#endif /* !defined(TARG_SIZEOF_VIRTUAL_FUNCTION_INFO) */
#ifndef TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO TARG_ALIGNOF_LONG
			/* Default value, used to initialize global variable
			   targ_alignof_virtual_function_info. */
#endif /* !defined(TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO) */
#endif /* TARG_ALL_POINTERS_SAME_SIZE */

/*
Size and alignment of a pointer to virtual base class.  Despite the name, a
"pointer-to-virtual-base-class" member may or may not actually be a "pointer".
The default implementation (namely, IL lowering) does treat it as a pointer,
but implementations are free to do otherwise.
*/
#if TARG_ALL_POINTERS_SAME_SIZE
/* Pointers all have the same size and alignment. */
#ifndef TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS TARG_SIZEOF_POINTER
			/* Default value, used to initialize global variable
			   targ_sizeof_ptr_to_virtual_base_class. */
#endif /* !defined(TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS) */
#ifndef TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS TARG_ALIGNOF_POINTER
			/* Default value, used to initialize global variable
			   targ_alignof_ptr_to_virtual_base_class. */
#endif /* !defined(TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS) */
#else /* !TARG_ALL_POINTERS_SAME_SIZE */
/* Pointers have different sizes -- use of long is arbitrary. */
#ifndef TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS TARG_SIZEOF_LONG
			/* Default value, used to initialize global variable
			   targ_sizeof_ptr_to_virtual_base_class. */
#endif /* !defined(TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS) */
#ifndef TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS TARG_ALIGNOF_LONG
			/* Default value, used to initialize global variable
			   targ_alignof_ptr_to_virtual_base_class. */
#endif /* !defined(TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS) */
#endif /* TARG_ALL_POINTERS_SAME_SIZE */

#if GNU_EXTENSIONS_ALLOWED

/* The default value used to initialize targ_word_mode.  (On Unix-like systems,
   at least, this appears to correspond to an integral type of size equal to
   the size of "long int".) */
#ifndef TARG_WORD_MODE
#if TARG_SIZEOF_LONG == 1
#define TARG_WORD_MODE ((a_type_mode_kind)tmk_QI)
#else /* TARG_SIZEOF_LONG != 1 */
#if TARG_SIZEOF_LONG == 2
#define TARG_WORD_MODE ((a_type_mode_kind)tmk_HI)
#else /* TARG_SIZEOF_LONG != 2 */
#if TARG_SIZEOF_LONG == 4
#define TARG_WORD_MODE ((a_type_mode_kind)tmk_SI)
#else /* TARG_SIZEOF_LONG != 4 */
#if TARG_SIZEOF_LONG == 8
#define TARG_WORD_MODE ((a_type_mode_kind)tmk_DI)
#else /* TARG_SIZEOF_LONG != 8 */
#if TARG_SIZEOF_LONG == 16
#define TARG_WORD_MODE ((a_type_mode_kind)tmk_TI)
#else /* TARG_SIZEOF_LONG != 16 */
 #error -- do not know how to set TARG_WORD_MODE
#endif /* TARG_SIZEOF_LONG != 16 */
#endif /* TARG_SIZEOF_LONG != 8 */
#endif /* TARG_SIZEOF_LONG != 4 */
#endif /* TARG_SIZEOF_LONG != 2 */
#endif /* TARG_SIZEOF_LONG != 1 */
#endif /* !defined(TARG_WORD_MODE) */

/* The default value used to initialize targ_unwind_word_mode.  By default
   this is the same as TARG_WORD_MODE. */
#ifndef TARG_UNWIND_WORD_MODE
#define TARG_UNWIND_WORD_MODE TARG_WORD_MODE
#endif /* !defined(TARG_UNWIND_WORD_MODE) */

/* The default value used to initialize targ_libgcc_cmp_return_mode.  By
   default this is the same as TARG_WORD_MODE. */
#ifndef TARG_LIBGCC_CMP_RETURN_MODE
#define TARG_LIBGCC_CMP_RETURN_MODE TARG_WORD_MODE
#endif /* !defined(TARG_LIBGCC_CMP_RETURN_MODE) */

/* The default value used to initialize targ_libgcc_shift_count_mode.  By
   default this is the same as TARG_WORD_MODE. */
#ifndef TARG_LIBGCC_SHIFT_COUNT_MODE
#define TARG_LIBGCC_SHIFT_COUNT_MODE TARG_WORD_MODE
#endif /* !defined(TARG_LIBGCC_SHIFT_COUNT_MODE) */

/* The default value used to initialize targ_pointer_mode. */
#if TARG_ALL_POINTERS_SAME_SIZE
#ifndef TARG_POINTER_MODE
#if TARG_SIZEOF_POINTER == 1
#define TARG_POINTER_MODE ((a_type_mode_kind)tmk_QI)
#else /* TARG_SIZEOF_POINTER != 1 */
#if TARG_SIZEOF_POINTER == 2
#define TARG_POINTER_MODE ((a_type_mode_kind)tmk_HI)
#else /* TARG_SIZEOF_POINTER != 2 */
#if TARG_SIZEOF_POINTER == 4
#define TARG_POINTER_MODE ((a_type_mode_kind)tmk_SI)
#else /* TARG_SIZEOF_POINTER != 4 */
#if TARG_SIZEOF_POINTER == 8
#define TARG_POINTER_MODE ((a_type_mode_kind)tmk_DI)
#else /* TARG_SIZEOF_POINTER != 8 */
#if TARG_SIZEOF_POINTER == 16
#define TARG_POINTER_MODE ((a_type_mode_kind)tmk_TI)
#else /* TARG_SIZEOF_POINTER != 16 */
 #error -- do not know how to set TARG_POINTER_MODE
#endif /* TARG_SIZEOF_POINTER != 16 */
#endif /* TARG_SIZEOF_POINTER != 8 */
#endif /* TARG_SIZEOF_POINTER != 4 */
#endif /* TARG_SIZEOF_POINTER != 2 */
#endif /* TARG_SIZEOF_POINTER != 1 */
#endif /* !defined(TARG_POINTER_MODE) */

#else /* !TARG_ALL_POINTERS_SAME_SIZE */
/* GCC does not support architectures where all pointers are not the
   same size.  It does not make sense to talk about a "pointer mode"
   on such an architecture. */
#endif /* !TARG_ALL_POINTERS_SAME_SIZE */

#endif /* GNU_EXTENSIONS_ALLOWED */

/* 
Numbering for virtual functions.  Each virtual member function in a given
class is assigned a unique number which can (for instance) be used to
define a virtual function table index value.
*/
typedef unsigned short a_virtual_function_number;
#ifndef MAX_VIRTUAL_FUNCTION_NUMBER
#define MAX_VIRTUAL_FUNCTION_NUMBER USHRT_MAX
#endif /* ifndef MAX_VIRTUAL_FUNCTION_NUMBER */

/* The value of a_virtual_function_number that indicates that there is
   no virtual function number, in the same way that NULL is the value
   of a pointer that does not point anywhere.  */
#if IA64_ABI
#define VIRTUAL_FUNCTION_NUMBER_NONE \
   MAX_VIRTUAL_FUNCTION_NUMBER /* Do not change this. */
#else /* !IA64_ABI */
#define VIRTUAL_FUNCTION_NUMBER_NONE 0 /* Do not change this. */
#endif /* !IA64_ABI */

/* The value of a_virtual_function_number assigned to the first
   virtual function. */
#if IA64_ABI
#define FIRST_VIRTUAL_FUNCTION_NUMBER 0 /* Do not change this. */
#else /* !IA64_ABI */
#define FIRST_VIRTUAL_FUNCTION_NUMBER 1 /* Do not change this. */
#endif /* !IA64_ABI */

/* The largest virtual function number that can be assigned to a
   virtual member function.  */
#ifndef MAX_VIRTUAL_FUNCTIONS_PER_CLASS
#if IA64_ABI
#define MAX_VIRTUAL_FUNCTIONS_PER_CLASS (MAX_VIRTUAL_FUNCTION_NUMBER - 1)
#else /* !IA64_ABI */
#define MAX_VIRTUAL_FUNCTIONS_PER_CLASS MAX_VIRTUAL_FUNCTION_NUMBER
#endif /* !IA64_ABI */
#else /* ifndef MAX_VIRTUAL_FUNCTIONS_PER_CLASS */
/* In previous versions of the front end, MAX_VIRTUAL_FUNCTIONS_PER_CLASS 
   was a parameter that could be set by users.  Now,
   MAX_VIRTUAL_FUNCTION_NUMBER should be set instead.  */
 #error -- Must set MAX_VIRTUAL_FUNCTION_NUMBER instead.
#endif /* ifndef MAX_VIRTUAL_FUNCTIONS_PER_CLASS */

/* The type of an index into the virtual table. */
#if !IA64_ABI
typedef a_virtual_function_number a_virtual_table_index;
#else /* IA64_ABI */
/* In the IA64 ABI, there are entries at negative vtable indices, so this type
   must be signed. */
typedef a_targ_ptrdiff_t a_virtual_table_index;
#endif /* IA64_ABI */

/*
Flag that is TRUE if assignment to "this" (a C++ anachronism) should
be allowed.  This affects the source language accepted.  If assignment
to "this" is allowed, the interface to and wrapper code within constructors
and destructors may have to be changed.  Initial value of
assignment_to_this_allowed global variable.  Lowering of delegating
constructors is incompatible with ASSIGNMENT_TO_THIS_ALLOWED being TRUE,
as is the lowering of exception handling (IL lowering does not know how to
build the right region table if there are several assignments to "this" in
one constructor) consequently the global variable is silently set to FALSE in
C++11 mode or when exceptions are enabled.
*/
#ifndef ASSIGNMENT_TO_THIS_ALLOWED
#if IA64_ABI && DO_IL_LOWERING
/* In the IA64 ABI, new cannot be folded into constructors, so assignment to
   "this" is not supported. */
#define ASSIGNMENT_TO_THIS_ALLOWED FALSE
#else /* !(IA64_ABI && DO_IL_LOWERING) */
#define ASSIGNMENT_TO_THIS_ALLOWED TRUE
#endif /* IA64_ABI && DO_IL_LOWERING */
#endif /* ifndef ASSIGNMENT_TO_THIS_ALLOWED */
#if ASSIGNMENT_TO_THIS_ALLOWED && IA64_ABI && DO_IL_LOWERING
 #error -- ASSIGNMENT_TO_THIS_ALLOWED and IA64_ABI cannot both be TRUE \
           if IL lowering is done
#endif /* ASSIGNMENT_TO_THIS_ALLOWED && IA64_ABI && DO_IL_LOWERING */

/*
Control over whether or not C++ "new" and "delete" operations are allowed
to be folded into the constructor or destructor if possible.
*/
/* IL lowering requires that the delete be folded into the destructor.
   Otherwise the size is not available for the two-argument delete case.
   There is a consistency check in lower_init.c */
#ifndef NEW_CAN_BE_FOLDED_INTO_CTOR
#if CFRONT_OBJECT_CODE_COMPATIBILITY
#define NEW_CAN_BE_FOLDED_INTO_CTOR TRUE  /* cfront compatibility setting. */
#else /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
#if IA64_ABI && DO_IL_LOWERING
#define NEW_CAN_BE_FOLDED_INTO_CTOR FALSE /* Do not change this. */
#else /* !(IA64_ABI && DO_IL_LOWERING) */
#define NEW_CAN_BE_FOLDED_INTO_CTOR TRUE  /* Can be changed. */
#endif /* !(IA64_ABI && DO_IL_LOWERING) */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
#endif /* !defined(NEW_CAN_BE_FOLDED_INTO_CTOR) */
#ifndef DELETE_CAN_BE_FOLDED_INTO_DTOR
#if CFRONT_OBJECT_CODE_COMPATIBILITY
#define DELETE_CAN_BE_FOLDED_INTO_DTOR TRUE /* cfront compatibility setting. */
#else /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
#define DELETE_CAN_BE_FOLDED_INTO_DTOR TRUE  /* Can be changed. */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
#endif /* !defined(DELETE_CAN_BE_FOLDED_INTO_DTOR) */
/* If assignment to "this" is allowed, the folding must be done. */
#if ASSIGNMENT_TO_THIS_ALLOWED && !NEW_CAN_BE_FOLDED_INTO_CTOR
 #error -- NEW_CAN_BE_FOLDED_INTO_CTOR may not be FALSE if \
           ASSIGNMENT_TO_THIS_ALLOWED is TRUE
#endif /* ASSIGNMENT_TO_THIS_ALLOWED ... */
#if ASSIGNMENT_TO_THIS_ALLOWED && !DELETE_CAN_BE_FOLDED_INTO_DTOR
 #error -- DELETE_CAN_BE_FOLDED_INTO_DTOR may not be FALSE if \
           ASSIGNMENT_TO_THIS_ALLOWED is TRUE
#endif /* ASSIGNMENT_TO_THIS_ALLOWED ... */
#if IA64_ABI && NEW_CAN_BE_FOLDED_INTO_CTOR && DO_IL_LOWERING
 #error -- NEW_CAN_BE_FOLDED_INTO_CTOR may not be TRUE if \
           IA64_ABI is TRUE
#endif /* IA64_ABI ... */

/*
Control over whether or not C++ "new" and "delete" operations for an array
whose elements are classes with a constructor or destructor can be folded
into the runtime routine to process those.
*/
#ifndef NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE
#if CFRONT_OBJECT_CODE_COMPATIBILITY
#define NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE \
  TRUE  /* cfront compatibility setting. */
#else /* CFRONT_OBJECT_CODE_COMPATIBILITY */
#define NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_RUNTIME_ROUTINE \
  TRUE  /* Can be changed. */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
#endif /* !defined(NEW_AND_DELETE_FOR_ARRAY_CAN_BE_FOLDED_INTO_...) */
/* This must be TRUE for IL lowering.  There's a consistency check there. */

/*
Enumerated types:  Default setting for targ_enum_types_can_be_smaller_than_int.
If TRUE, enumerated types can be allocated in integral types smaller than int.
*/
#ifndef TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT
#if CFRONT_OBJECT_CODE_COMPATIBILITY
/* This feature CAN be changed when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT FALSE /* cfront compat. */
#else /* !CFRONT_OBJECT_CODE_COMPATIBILITY */
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT FALSE  /* Can be changed. */
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY */
			/* Default value, used to initialize global variable
			   targ_enum_types_can_be_smaller_than_int. */
#endif /* !defined(TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT) */

/*
Definition of shift operations:
*/
#ifndef TARG_RIGHT_SHIFT_IS_ARITHMETIC
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC TRUE
			/* Right shift on a signed quantity does sign
			   extension.  Default value, used to initialize
			   global variable targ_right_shift_is_arithmetic. */
#endif /* !defined(TARG_RIGHT_SHIFT_IS_ARITHMETIC) */

/*
Flag that indicates whether in a shift with a too-large shift count the count
is reduced modulo the bit size of the object being shifted.  FALSE means the
shift is done as if we really shift as many bits as indicated.  Used to
initialize targ_too_large_shift_count_is_taken_modulo_size.  Note that this is
really relevant only for compile-time folding, and probably only in certain
permissive modes (in most modes, too-large shift counts are errors).  Also note
that almost all compilers seem to fold at compile time in a way that matches
the FALSE setting in spite of the fact that when the same operation is done at
runtime the result matches the TRUE setting.
*/
#ifndef TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE FALSE
#endif /* ifndef TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE */

/*
Number of significant characters in an external name (names will be truncated
to this length if necessary).  If there is no limit, this value should be set
to zero.  It should in any case be set to a reasonably small value (not, for
instance, to the maximum integer size) since an array of this many
characters may be allocated (see find_external_symbol in symbol_tbl.c).
*/
#ifndef TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME
#define TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME 0
#endif /* ifndef TARG_SIGNIF_CHARS_IN_EXTERNAL_NAME */

/*
Flag that is TRUE if external names are case sensitive (in which case, e.g.,
the object language would distinguish routines XXX and xxx).
*/
#ifndef TARG_CASE_SENSITIVE_EXTERNAL_NAMES
#define TARG_CASE_SENSITIVE_EXTERNAL_NAMES TRUE
#endif /* ifndef TARG_CASE_SENSITIVE_EXTERNAL_NAMES */

/*
Flag that is TRUE if external names begin with an added underscore.
This is used by some utility programs (e.g., edg_munch) that deal with
names.  The front end doesn't add the underscore.
*/
#ifndef TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED
#define TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED TRUE
#endif /* !defined(TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED) */

/*
Flag that is TRUE if class and struct fields are allocated in the same order
as they are declared, regardless of access specification.  When it is FALSE,
fields are grouped by access (private first, followed by protected and then
public) and offsets are assigned within each group in declaration order.
(Constraints on allocation are discussed in ARM 9.2 and 11.1.  In particular,
both approaches described in the embedded annotation in section 11.1 are
supported.)
*/
#ifndef TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE
#define TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE TRUE
#endif /* ifndef TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE */

#if IA64_ABI && !TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE
 #error -- TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE FALSE is \
           incompatible with IA64_ABI
#endif /* IA64_ABI && ... */

/*
The minimum alignment required for class/struct/union objects in the target
environment.  If C code is being generated, this may be dictated by the
characteristics of the C compiler that will be used for subsequent
processing.
*/
#ifndef TARG_MINIMUM_STRUCT_ALIGNMENT
#define TARG_MINIMUM_STRUCT_ALIGNMENT 1
			/* Default value, used to initialize global variable
			   targ_minimum_struct_alignment. */
#endif /* !defined(TARG_MINIMUM_STRUCT_ALIGNMENT) */

/*
Set the minimum and maximum values which a "pack alignment" value may have.
This is an alignment that is the maximum alignment for a nonstatic data
member of a class; it can force a member to be aligned at a lesser alignment
than its type would normally require.
Microsoft and GNU compilers allow large "pack alignment" values (such as
32768), but the maximum value accepted by the front end must fit in the type
TYPE_FOR_TARG_ALIGNMENT which is a_byte by default.  We therefore limit the
maximum pack alignment to 128 by default.
*/
#ifndef TARG_MINIMUM_PACK_ALIGNMENT
#define TARG_MINIMUM_PACK_ALIGNMENT 1
			/* Default value, used to initialize global variable
			   targ_minimum_pack_alignment. */
#endif /* !defined(TARG_MINIMUM_PACK_ALIGNMENT) */

#ifndef TARG_MAXIMUM_PACK_ALIGNMENT
#define TARG_MAXIMUM_PACK_ALIGNMENT 128
			/* Default value, used to initialize global variable
			   targ_maximum_pack_alignment. */
#endif /* !defined(TARG_MAXIMUM_PACK_ALIGNMENT) */

/*
The maximum alignment the target can take advantage of.  (On some platforms
very high pack alignments are allowed, but they do not provide any advantage
in terms of memory bandwidth.)  This value is used in GNU mode to determine
the alignment of entities with an "aligned" attribute without arguments.
This is the default value used to initialize global variable
targ_maximum_intrinsic_alignment.
*/
#ifndef TARG_MAXIMUM_INTRINSIC_ALIGNMENT
#define TARG_MAXIMUM_INTRINSIC_ALIGNMENT 16
#endif /* !defined(TARG_MAXIMUM_INTRINSIC_ALIGNMENT) */

/*
The alignment beyond which, in C++17 mode, new and delete expressions will
use the versions of operator new and operator delete that have an alignment
parameter.  This is the default value used to initialize global variable
targ_default_new_alignment.
*/
#ifndef TARG_DEFAULT_NEW_ALIGNMENT
#define TARG_DEFAULT_NEW_ALIGNMENT TARG_MAXIMUM_INTRINSIC_ALIGNMENT
#endif /* !defined(TARG_DEFAULT_NEW_ALIGNMENT) */

/*
The maximum size a class object may have.  If TARG_MAX_CLASS_OBJECT_SIZE
is nonzero, then it is the value to which targ_max_class_object_size is set.
If it is zero, then targ_max_class_object_size is set to targ_size_t_max.
*/
#ifndef TARG_MAX_CLASS_OBJECT_SIZE
#define TARG_MAX_CLASS_OBJECT_SIZE 0
#endif /* ifndef TARG_MAX_CLASS_OBJECT_SIZE */

/*
The maximum offset a base class may have.  If TARG_MAX_BASE_CLASS_OFFSET
is nonzero, then it is the value to which targ_max_base_class_offset is
set -- except that, when DO_IL_LOWERING is TRUE, a smaller value will be
used, if necessary, to accommodate the maximum offset value (as implied by
TARG_DELTA_INT_KIND) that may be stored in a virtual function table.  If
TARG_MAX_BASE_CLASS_OFFSET is zero, then targ_max_base_class_offset is set
to targ_size_t_max.
*/
#ifndef TARG_MAX_BASE_CLASS_OFFSET
#define TARG_MAX_BASE_CLASS_OFFSET 0
#endif /* ifndef TARG_MAX_BASE_CLASS_OFFSET */

/*
A flag that is TRUE if the layout mechanism should attempt to allocate empty
base classes at the same offset as other subobjects.
*/
#ifndef TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT
/* This feature CAN be turned on when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#if ABI_COMPATIBILITY_VERSION <= 241 || CFRONT_OBJECT_CODE_COMPATIBILITY
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT FALSE
                                                    /* Versions up to 2.41. */
#else /* ABI_COMPATIBILITY_VERSION > 241 && !CFRONT_... */
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT TRUE
                                                    /* Versions after 2.41. */
#endif /* ABI_COMPATIBILITY_VERSION <= 241 || CFRONT_... */
#endif /* ifndef TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT */
#if TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT
#if ABI_COMPATIBILITY_VERSION <= 241
 #error -- TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT TRUE is incompatible \
           with ABI_COMPATIBILITY_VERSION <= 241
#endif /* ABI_COMPATIBILITY_VERSION <= 242 */
#else /* !TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT */
#if IA64_ABI
 #error -- TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT FALSE is incompatible \
           with IA64_ABI
#endif /* IA64_ABI */
#endif /* !TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT */

/*
A flag that is TRUE if an empty base that does not share its offset with
another subobject (i.e., an "allocated base") should be padded according to
its alignment instead of allocating just one byte for it.  This flag should be
TRUE if the C-generating back end is used, because a C compiler will pad the
fields of struct type representing allocated empty base subobjects if
TARG_MINIMUM_STRUCT_ALIGNMENT is larger than one.
*/
#ifndef TARG_PAD_ALLOCATED_EMPTY_BASE
#define TARG_PAD_ALLOCATED_EMPTY_BASE BACK_END_IS_C_GEN_BE
#endif /* ifndef TARG_PAD_ALLOCATED_EMPTY_BASE */

/*
A flag that is TRUE when tail-padding from base classes should be reused
for other purposes in the derived class.  The IA64 ABI requires that
tail-padding be reused, and the C-generating back end only supports reuse
of tail-padding with the IA64 ABI.  This is the initial value of the global
variable targ_reuse_tail_padding.
*/
#ifndef TARG_REUSE_TAIL_PADDING
#define TARG_REUSE_TAIL_PADDING IA64_ABI
#endif /* TARG_REUSE_TAIL_PADDING */

/*
When a class with a copy constructor is passed to an ellipsis, does the
copy constructor get called?  If this is TRUE, what is passed as the argument
is the address of a temporary into which the class object has been copied.
This falls under undefined behavior.
*/
#ifndef USE_CCTOR_TO_PASS_CLASS_TO_ELLIPSIS
#define USE_CCTOR_TO_PASS_CLASS_TO_ELLIPSIS FALSE
#endif /* ifndef USE_CCTOR_TO_PASS_CLASS_TO_ELLIPSIS */

/*
If this is TRUE, the front end will attempt to optimize virtual
function calls into non-virtual calls when it knows the complete
object type in a call.  For many source-analysis applications
this is best left turned off.
*/
#ifndef OPTIMIZE_VIRTUAL_FUNCTION_CALLS
#if DO_IL_LOWERING
#define OPTIMIZE_VIRTUAL_FUNCTION_CALLS TRUE
#else /* !DO_IL_LOWERING */
#define OPTIMIZE_VIRTUAL_FUNCTION_CALLS FALSE
#endif /* DO_IL_LOWERING */
#endif /* OPTIMIZE_VIRTUAL_FUNCTION_CALLS */

/*
Switch that controls whether top-level casts to void, for example
  (void)f(x);
are removed by IL lowering.  (They are always present in the unlowered IL.)
*/
#ifndef PRESERVE_TOP_LEVEL_CASTS_TO_VOID_IN_IL
#define PRESERVE_TOP_LEVEL_CASTS_TO_VOID_IN_IL FALSE
#endif /* ifndef PRESERVE_TOP_LEVEL_CASTS_TO_VOID_IN_IL */

/*
This switch controls whether or not type qualifiers are removed from
parameter types (e.g., a "const int" parameter is seen simply as "int").
This may seem like a language feature, but it's an ABI issue, because the
parameter type ends up in the mangled name of the function.  This value is
the default for global variable remove_qualifiers_from_param_types.
*/
#ifndef DEFAULT_REMOVE_QUALIFIERS_FROM_PARAM_TYPES
/* This feature CAN be turned on when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#if ABI_COMPATIBILITY_VERSION <= 228 || CFRONT_OBJECT_CODE_COMPATIBILITY
#define DEFAULT_REMOVE_QUALIFIERS_FROM_PARAM_TYPES FALSE
#else /* ABI_COMPATIBILITY_VERSION > 228 && !CFRONT_... */
#define DEFAULT_REMOVE_QUALIFIERS_FROM_PARAM_TYPES TRUE
#endif /* ABI_COMPATIBILITY_VERSION <= 228 || CFRONT_... */
#endif /* ifndef DEFAULT_REMOVE_QUALIFIERS_FROM_PARAM_TYPES */

/*
Flag that is TRUE if, by default, function types are considered distinct
when their only difference is that one has extern "C" routine linkage and
the other has extern "C++" routine linkage.  It is the initial value of
global variable c_and_cpp_function_types_are_distinct.  How to set this
flag is both a language issue (overloading, type conversions) and an ABI
issue (name mangling).  For example:
  typedef void (*PF)();             // Pointer to an extern "C++" function
  extern "C" typedef void (*PCF)(); // Pointer to an extern "C" function
  void f(PF);
  void f(PCF);
When the flag is TRUE, "void f(PCF)" introduces a new function, which is
consistent with the Working Paper; when it is FALSE, "void f(PCF)" is a
compatible redeclaration of "void f(PF)" -- cfront's behavior.  (Note: when
this flag is FALSE, a strictly conforming implementation is not possible;
when it is TRUE, running in cfront compatibility mode is compromised -- but
only rarely as long as if impl_conv_between_c_and_cpp_function_ptrs_allowed
is TRUE.)  This is also an ABI issue, because it affects the representation
of pointer-to-function types in a mangled name.  When the flag is TRUE, the
name-mangling of "void f(PCF)" is distinct from that of "void f(PF)"; if it
is FALSE, the two are mangled identically.
*/
#ifndef DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT
/* This feature CAN be turned on when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#if ABI_COMPATIBILITY_VERSION < 233 || CFRONT_OBJECT_CODE_COMPATIBILITY
#define DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT FALSE
#else /* !(ABI_COMPATIBILITY_VERSION < 233 || ...) */
#define DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT TRUE
#endif /* ABI_COMPATIBILITY_VERSION < 233 || ... */
#endif /* ifndef DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT */

/*
Flag that is TRUE if the runtime library uses namespaces.  This
causes the runtime library to define the library classes (e.g., type_info)
in the "std" namespace.  It is also used by the standard header files
for the same purpose.
*/
#ifndef RUNTIME_USES_NAMESPACES
/* This feature CAN be turned on when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#if ABI_COMPATIBILITY_VERSION < 230 || CFRONT_OBJECT_CODE_COMPATIBILITY
#define RUNTIME_USES_NAMESPACES FALSE
#else /* !(ABI_COMPATIBILITY_VERSION < 230 || CFRONT_...) */
#define RUNTIME_USES_NAMESPACES TRUE
#endif /* ABI_COMPATIBILITY_VERSION < 230 || CFRONT_... */
#endif /* ifndef RUNTIME_USES_NAMESPACES */

/*
Flag that is TRUE if the runtime library defines class type_info in the
"std" namespace.  Usually this should be set to the same value as
RUNTIME_USES_NAMESPACES.  This flag is used to initialize global variable
type_info_in_namespace_std in non-Microsoft modes.  (See also the description
of MICROSOFT_MODE_TYPE_INFO_IN_NAMESPACE_STD below.)
*/
#ifndef DEFAULT_TYPE_INFO_IN_NAMESPACE_STD
#define DEFAULT_TYPE_INFO_IN_NAMESPACE_STD RUNTIME_USES_NAMESPACES
#endif /* ifndef DEFAULT_TYPE_INFO_IN_NAMESPACE_STD */

/*
Flag that is FALSE if in Microsoft mode type_info should be predeclared in
the global namespace.  This is usually desirable since it is what Microsoft
compilers (and their associated run-time support library) do.  However, if
the front end is paired with a run-time support library that places type_info
in namespace std (as per the C++ standard), linker errors will ensue.  This
macro is the initial value of type_info_in_namespace_std in Microsoft mode.
(See also DEFAULT_TYPE_INFO_IN_NAMESPACE_STD above.)
*/
#ifndef MICROSOFT_MODE_TYPE_INFO_IN_NAMESPACE_STD
#define MICROSOFT_MODE_TYPE_INFO_IN_NAMESPACE_STD FALSE
#endif /* ifndef MICROSOFT_MODE_TYPE_INFO_IN_NAMESPACE_STD */



#if RUNTIME_USES_NAMESPACES
/*
The name of the macro to be defined when the runtime uses namespaces.
This is only used when RUNTIME_USES_NAMESPACES is TRUE.
*/
#ifndef MACRO_DEFINED_WHEN_RUNTIME_USES_NAMESPACES
#define MACRO_DEFINED_WHEN_RUNTIME_USES_NAMESPACES \
  "__EDG_RUNTIME_USES_NAMESPACES"
#endif /* ifndef MACRO_DEFINED_WHEN_RUNTIME_USES_NAMESPACES */

/*
The name of the macro to be defined when the runtime should implicitly
do a "using namespace std".  This is only used when RUNTIME_USES_NAMESPACES
is TRUE.
*/
#ifndef MACRO_DEFINED_WHEN_IMPLICITLY_USING_STD
#define MACRO_DEFINED_WHEN_IMPLICITLY_USING_STD "__EDG_IMPLICIT_USING_STD"
#endif /* ifndef MACRO_DEFINED_WHEN_IMPLICITLY_USING_STD */
#endif /* RUNTIME_USES_NAMESPACES */

/*
Flag that is TRUE if the runtime library and/or system header files
use typename.
*/
#ifndef RUNTIME_USES_TYPENAME
#define RUNTIME_USES_TYPENAME FALSE
#endif /* ifndef RUNTIME_USES_TYPENAME */

/*
Flag that is TRUE if the runtime library has a routine that can be used to
throw a std::bad_array_new_length exception to diagnose cases like
"auto a = new int[n] {1,2,3,4};" where n is less than four.  When this
flag is FALSE, no run-time check is inserted into the code (which may
result in undefined behavior in generated code).
*/
#ifndef RUNTIME_SUPPORTS_ARRAY_LENGTH_CHECK
#define RUNTIME_SUPPORTS_ARRAY_LENGTH_CHECK TRUE
#endif /* ifndef RUNTIME_SUPPORTS_ARRAY_LENGTH_CHECK */

/*
Flag that is TRUE if the runtime library supports global sized deallocation
functions, i.e.,

  void operator delete(void *ptr, size_t size);
  void operator delete[](void *ptr, size_t size);
*/
#ifndef RUNTIME_SUPPORTS_SIZED_DEALLOCATION
#if ABI_COMPATIBILITY_VERSION >= 411
#define RUNTIME_SUPPORTS_SIZED_DEALLOCATION TRUE
#else /* ABI_COMPATIBILITY_VERSION < 411 */
#define RUNTIME_SUPPORTS_SIZED_DEALLOCATION FALSE
#endif /* ABI_COMPATIBILITY_VERSION >= 411 */
#endif /* RUNTIME_SUPPORTS_SIZED_DEALLOCATION */

/*
Switch that is TRUE if the C-generating or C++-generating back end should
generate code for a GNU compiler (gcc or g++).
*/
/* The old name of this macro was GCC_IS_C_GEN_BE_TARGET; it that's set,
   transfer its value to the new macro. */
#ifndef GCC_IS_GENERATED_CODE_TARGET
#if defined(GCC_IS_C_GEN_BE_TARGET) && BACK_END_IS_C_GEN_BE
#if GCC_IS_C_GEN_BE_TARGET
#define GCC_IS_GENERATED_CODE_TARGET TRUE
#else /* !GCC_IS_C_GEN_BE_TARGET */
#define GCC_IS_GENERATED_CODE_TARGET FALSE
#endif /* GCC_IS_C_GEN_BE_TARGET */
#endif /* defined(GCC_IS_C_GEN_BE_TARGET) && BACK_END_IS_C_GEN_BE */
#endif /* ifndef GCC_IS_GENERATED_CODE_TARGET */

/*
If this configuration will use either the C- or the C++-generating back end
and no target compiler has been specified, provide a default target.
*/
#if !defined(GCC_IS_GENERATED_CODE_TARGET) &&               \
    !defined(CLANG_IS_GENERATED_CODE_TARGET) &&             \
    !defined(SUN_IS_GENERATED_CODE_TARGET) &&               \
    !defined(MSVC_IS_GENERATED_CODE_TARGET) &&              \
    !defined(MICROSOFT_DIALECT_IS_GENERATED_CODE_TARGET) && \
    !(BACK_END_IS_CP_GEN_BE &&                              \
      defined(CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT))
#if BACK_END_IS_C_GEN_BE
/* Base the default target setting on the compiler being used to compile
   the front end. */
#if defined(__clang__)
#define CLANG_IS_GENERATED_CODE_TARGET TRUE
#define GCC_IS_GENERATED_CODE_TARGET FALSE
#define SUN_IS_GENERATED_CODE_TARGET FALSE
#define MSVC_IS_GENERATED_CODE_TARGET FALSE
#define MICROSOFT_DIALECT_IS_GENERATED_CODE_TARGET FALSE
#else /* !defined(__clang__) */
#if defined(__GNUC__)
#define GCC_IS_GENERATED_CODE_TARGET TRUE
#define CLANG_IS_GENERATED_CODE_TARGET FALSE
#define SUN_IS_GENERATED_CODE_TARGET FALSE
#define MSVC_IS_GENERATED_CODE_TARGET FALSE
#define MICROSOFT_DIALECT_IS_GENERATED_CODE_TARGET FALSE
#else /* !defined(__GNUC__) */
#if defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#define SUN_IS_GENERATED_CODE_TARGET TRUE
#define GCC_IS_GENERATED_CODE_TARGET FALSE
#define CLANG_IS_GENERATED_CODE_TARGET FALSE
#define MSVC_IS_GENERATED_CODE_TARGET FALSE
#define MICROSOFT_DIALECT_IS_GENERATED_CODE_TARGET FALSE
#else /* !(defined(__SUNPRO_C) || defined(__SUNPRO_CC) */
#if EDG_WIN32
#define MSVC_IS_GENERATED_CODE_TARGET TRUE
/* MICROSOFT_DIALECT_IS_GENERATED_CODE_TARGET will be defined below. */
#define GCC_IS_GENERATED_CODE_TARGET FALSE
#define CLANG_IS_GENERATED_CODE_TARGET FALSE
#define SUN_IS_GENERATED_CODE_TARGET FALSE
#else /* !EDG_WIN32 */
/* The compiler is one for which we have no special handling.  Assume it at
   least supports C89. */
#define C_GEN_BE_GENERATES_ANSI_C TRUE
#define GCC_IS_GENERATED_CODE_TARGET FALSE
#define CLANG_IS_GENERATED_CODE_TARGET FALSE
#define SUN_IS_GENERATED_CODE_TARGET FALSE
#define MSVC_IS_GENERATED_CODE_TARGET FALSE
#define MICROSOFT_DIALECT_IS_GENERATED_CODE_TARGET FALSE
#endif /* EDG_WIN32 */
#endif /* defined(__SUNPRO_C) || defined(__SUNPRO_CC) */
#endif /* defined(__GNUC__) */
#endif /* defined(__clang__) */
#if !defined(LOWER_IFUNC) && !GCC_IS_GENERATED_CODE_TARGET && \
    GNU_EXTENSIONS_ALLOWED
#define LOWER_IFUNC TRUE
#endif /* !defined(LOWER_IFUNC) && ... */
#else /* !BACK_END_IS_C_GEN_BE */
#if BACK_END_IS_CP_GEN_BE
/* Make the output reflect how the source was written. */
#define CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT TRUE
#define GCC_IS_GENERATED_CODE_TARGET FALSE
#define CLANG_IS_GENERATED_CODE_TARGET FALSE
#define SUN_IS_GENERATED_CODE_TARGET FALSE
#define MSVC_IS_GENERATED_CODE_TARGET FALSE
#define MICROSOFT_DIALECT_IS_GENERATED_CODE_TARGET FALSE
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* BACK_END_IS_C_GEN_BE */
#endif /* !defined(GCC_IS_GENERATED_CODE_TARGET) && ... */

#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
/*
Make sure undefined targets are defined to FALSE.
*/
#if !defined(GCC_IS_GENERATED_CODE_TARGET)
#define GCC_IS_GENERATED_CODE_TARGET FALSE
#endif /* !defined(GCC_IS_GENERATED_CODE_TARGET) */
#if !defined(CLANG_IS_GENERATED_CODE_TARGET)
#define CLANG_IS_GENERATED_CODE_TARGET FALSE
#endif /* !defined(CLANG_IS_GENERATED_CODE_TARGET) */
#if !defined(SUN_IS_GENERATED_CODE_TARGET)
#define SUN_IS_GENERATED_CODE_TARGET FALSE
#endif /* !defined(SUN_IS_GENERATED_CODE_TARGET) */
#if !defined(MSVC_IS_GENERATED_CODE_TARGET)
#define MSVC_IS_GENERATED_CODE_TARGET FALSE
#endif /* !defined(MSVC_IS_GENERATED_CODE_TARGET) */
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */

/*
Flag that is TRUE if the GNU "ifunc" dispatch mechanism is to be lowered.
The "ifunc" attribute maps to the STT_GNU_IFUNC symbol type in the ELF
standard and is not available on many architectures.  The STT_GNU_IFUNC allows
a routine's symbol to be determined dynamically at load time.  At load time,
the routine specified as the resolver (in the ifunc attribute) is invoked
(once) and returns a pointer to the routine that will be used to resolve all
instances of that symbol for that execution of the executable.  This provides
a very low overhead mechanism to select one of a family of functions
(typically based on the underlying CPU architecture).  Here's an example:

  int printf(const char *,...);
  void target() {
    printf("Here\n");
  }
  static void (*resolver(void))(void) {
    return (void(*)(void))&target;
  }
  void source() __attribute__ ((ifunc ("resolver")));
  int main() {
    source();   // Prints "Here"
  }

For back ends that don't support the STT_GNU_IFUNC symbol, an approximation
of the run-time behavior can be achieved by setting LOWER_IFUNC to TRUE.
In that case, lowering will convert the routine with the ifunc attribute
into a "wrapper" routine as such:

  decltype(source) resolver_result = source;
  source(args...) {
    if (resolver_result == source) {
      resolver_result = (decltype(source))resolver();
    }
    return *resolver_result(args...);
  }

This has the effect of invoking the resolver only once (though at run time
rather than at load time).  Invocations of "source" incur an extra function
call and indirection, but lowering re-writes references to "source" so
they are dispatched through the resolver variable whenever possible (in which
case the only overhead is an extra indirection).

Note that there are a few caveats:

- The re-writing of calls to avoid the wrapper routine only occurs once the
declaration with the ifunc attribute has been seen; any reference prior
to that point will be unlowered (and will hence invoke the wrapper).  That
can be an issue if, for example, the declaration with the ifunc attribute is
only in a library's implementation (and not in the shared header file).  In
that case, code that is compiled with the shared header file will contain
references to the wrapper routine.

- Constants (i.e., ck_address/abk_routine) that refer to the ifunc are not
re-written in lowering, so they'll always point to the wrapper routine.

*/
#ifndef LOWER_IFUNC
#if DO_IL_LOWERING && GNU_EXTENSIONS_ALLOWED
#if BACK_END_IS_C_GEN_BE && GCC_IS_GENERATED_CODE_TARGET
#define LOWER_IFUNC TRUE  /* Can be changed (requires 4.6.0 or later). */
#else /* !(BACK_END_IS_C_GEN_BE && GCC_IS_GENERATED_CODE_TARGET) */
#define LOWER_IFUNC TRUE
#endif /* BACK_END_IS_C_GEN_BE && GCC_IS_GENERATED_CODE_TARGET */
#else /* !(DO_IL_LOWERING && GNU_EXTENSIONS_ALLOWED) */
#define LOWER_IFUNC FALSE
#endif /* DO_IL_LOWERING && GNU_EXTENSIONS_ALLOWED */
#endif /* defined(LOWER_IFUNC) */

#if BACK_END_IS_C_GEN_BE && !GCC_IS_GENERATED_CODE_TARGET && \
    GNU_EXTENSIONS_ALLOWED && !LOWER_IFUNC
 #error -- LOWER_IFUNC must be TRUE if using BACK_END_IS_C_GEN_BE with a \
           non-gcc back end
#endif /* BACK_END_IS_C_GEN_BE && !GCC_IS_GENERATED_CODE_TARGET && ... */
#if LOWER_IFUNC && !(DO_IL_LOWERING && GNU_EXTENSIONS_ALLOWED)
 #error -- LOWER_IFUNC can only be TRUE when both DO_IL_LOWERING and \
           GNU_EXTENSIONS_ALLOWED are TRUE
#endif /* LOWER_IFUNC && !(DO_IL_LOWERING && GNU_EXTENSIONS_ALLOWED) */
    
/*
Flag that is TRUE if uses of __builtin_constant_p should always be folded in
the front end.  In GNU compilers this is not always the case: Instead some
calls are folded by the back end (and the result may depend on the
optimization level).  If FALSE, a back end should be prepared to recognize
calls to the (pseudo-)function.  This is the initial value of the global
variable always_fold_calls_to_builtin_constant_p.
*/
#ifndef DEFAULT_ALWAYS_FOLD_CALLS_TO_BUILTIN_CONSTANT_P
#if (GCC_IS_GENERATED_CODE_TARGET || CLANG_IS_GENERATED_CODE_TARGET) && \
    (BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE)
#define DEFAULT_ALWAYS_FOLD_CALLS_TO_BUILTIN_CONSTANT_P FALSE
#else /* !(GCC_IS_GENERATED_CODE_TARGET && ...) */
#define DEFAULT_ALWAYS_FOLD_CALLS_TO_BUILTIN_CONSTANT_P TRUE
#endif /* GCC_IS_GENERATED_CODE_TARGET && ... */
#endif /* ifndef DEFAULT_ALWAYS_FOLD_CALLS_TO_BUILTIN_CONSTANT_P */

#if BACK_END_IS_CP_GEN_BE
/*
If CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT is TRUE, the C++-generating back
end should generate code that matches the dialect selected for the front end.
(E.g., if the front end is set to parse GNU code, the back end can generate
GNU __attribute__ constructs, whereas if the front end is set to accept
Microsoft extensions, the back end might generate __declspec specifiers.)
*/
#ifndef CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT
#define CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT FALSE
#endif /* !defined(CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT) */
#endif /* BACK_END_IS_CP_GEN_BE */

/*
Macro representing the version of GNU C or C++ for which the C- and C++-
generating back ends should produce code.  For version x.y.z of a GNU
compiler, the macro should equal x*10000+y*100+z.  If this file is
compiled using a GNU compiler (or a GNU-like compiler), then the macro
defaults to the version of that compiler; otherwise, an arbitrary default
is provided.  This macro is the default value of the global variable
gnu_target_version_number.
*/
#ifndef GNU_TARGET_VERSION_NUMBER
#if defined(__GNUC__) && defined(__GNUC_MINOR__) && \
    defined(__GNUC_PATCHLEVEL__)
#define GNU_TARGET_VERSION_NUMBER  ((__GNUC__)*10000 +                    \
                                    (__GNUC_MINOR__)*100 +                \
                                    (__GNUC_PATCHLEVEL__))
#else /* !(defined(__GNUC__) && defined(__GNUC_MINOR__) && ...) */
/* Set an arbitrary default. */
#define GNU_TARGET_VERSION_NUMBER 30200
#endif /* defined(__GNUC__) && defined(__GNUC_MINOR__) && ... */
#endif /* GNU_TARGET_VERSION_NUMBER */

/*
Macro representing the version of clang C or C++ for which the C- and C++-
generating back ends should produce code.  For version x.y.z of a clang
compiler, the macro should equal x*10000+y*100+z.  If this file is
compiled using a clang compiler (or a clang-like compiler), then the macro
defaults to the version of that compiler; otherwise, no default is
provided.  This macro is the default value of the global variable
clang_target_version_number.
*/
#ifndef CLANG_TARGET_VERSION_NUMBER
#if CLANG_IS_GENERATED_CODE_TARGET
#if defined(__clang_major__) && defined(__clang_minor__) && \
    defined(__clang_patchlevel__)
#define CLANG_TARGET_VERSION_NUMBER ((__clang_major__)*10000 +            \
                                     (__clang_minor__)*100 +              \
                                     (__clang_patchlevel__))
#else /* !(defined(__clang_major__) && defined(__clang_minor__) && ...) */
/* A target version number is needed, but none could be determined from the
   host compiler: use the "latest" version. */
#define CLANG_TARGET_VERSION_NUMBER 99999
#endif /* defined(__clang_major__) && defined(__clang_minor__) && ... */
#endif /* CLANG_IS_GENERATED_CODE_TARGET */
#endif /* CLANG_TARGET_VERSION_NUMBER */

/*
Switch that is TRUE if the C-generating or C++-generating back end should
generate code that uses vararg primitives that are predefined by some GNU
and Clang compilers (e.g., __builtin_va_list).
*/
#ifndef GCC_BUILTIN_VARARGS_IN_GENERATED_CODE
#if (GCC_IS_GENERATED_CODE_TARGET || CLANG_IS_GENERATED_CODE_TARGET) && \
    GCC_BUILTIN_VARARGS
#define GCC_BUILTIN_VARARGS_IN_GENERATED_CODE TRUE
#else /* !(GCC_IS_GENERATED_CODE_TARGET || CLANG_IS_GENERATED_CODE_TARGET)...*/
#define GCC_BUILTIN_VARARGS_IN_GENERATED_CODE FALSE
#endif /* (GCC_IS_GENERATED_CODE_TARGET || CLANG_IS_GENERATED_CODE_TARGET)...*/
#endif /* ifndef GCC_BUILTIN_VARARGS_IN_GENERATED_CODE */

/*
Macro representing the version of Sun C or C++ for which the C- and C++-
generating back ends should produce code.  For version x.y of the Sun
compiler, the macro should equal x*0x100+y*0x10 (e.g., version 5.3 would be
0x530).  If this file is compiled using a Sun compiler, then the macro
defaults to the version of that compiler; otherwise, no default is provided.
This macro is the default value of the global variable
sun_target_version_number.
*/
#ifndef SUN_TARGET_VERSION_NUMBER
#if SUN_IS_GENERATED_CODE_TARGET ||             \
    (BACK_END_IS_CP_GEN_BE &&                   \
     CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT && \
     SUN_EXTENSIONS_ALLOWED)
#if defined(__SUNPRO_C)
#define SUN_TARGET_VERSION_NUMBER  (__SUNPRO_C)
#else /* !defined(__SUNPRO_C) */
#if defined(__SUNPRO_CC)
#define SUN_TARGET_VERSION_NUMBER  (__SUNPRO_CC)
#endif /* defined(__SUNPRO_CC) */
#endif /* defined(_SUNPRO_C) */
#ifndef SUN_TARGET_VERSION_NUMBER
/* A target version number is needed, but none could be determined from the
   host compiler: Force a preprocessing error. */
 #error -- SUN_IS_GENERATED_CODE_TARGET, or SUN_EXTENSIONS_ALLOWED with  \
           CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT, require              \
           SUN_TARGET_VERSION_NUMBER to be defined
#endif /* ifndef SUN_TARGET_VERSION_NUMBER */
#endif /* SUN_IS_GENERATED_CODE_TARGET || ... */
#endif /* SUN_TARGET_VERSION_NUMBER */

/*
Macro that is TRUE if a function definition with a parameter of a class type
with a destructor should cause that destructor to be marked as referenced.
This can affect whether the destructor definition is instantiated, and reflects
certain ABIs (e.g., by Microsoft) that have the callee destroy parameter
variables.
*/
#ifndef REF_DESTRUCTORS_FOR_PARAMETER_VARIABLES
#define REF_DESTRUCTORS_FOR_PARAMETER_VARIABLES FALSE
#endif /* REF_DESTRUCTORS_FOR_PARAMETER_VARIABLES */

/*
Switch that is TRUE if bugs in some versions of MSVC++ regarding
value-initialization should be emulated.  This is desirable in products
that are trying to detect uninitialized values, but not in general.
*/
#ifndef DEFAULT_EMULATE_MSVC_VALUE_INITIALIZATION_BUGS
#define DEFAULT_EMULATE_MSVC_VALUE_INITIALIZATION_BUGS FALSE
#endif /* DEFAULT_EMULATE_MSVC_VALUE_INITIALIZATION_BUGS */

/*
Switch that is TRUE if bugs in some versions of g++ regarding
value-initialization should be emulated.  This is desirable in products
that are trying to detect uninitialized values, but not in general.
*/
#ifndef DEFAULT_EMULATE_GNU_VALUE_INITIALIZATION_BUGS
#define DEFAULT_EMULATE_GNU_VALUE_INITIALIZATION_BUGS FALSE
#endif /* DEFAULT_EMULATE_GNU_VALUE_INITIALIZATION_BUGS */

#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
/*
When generating code to be compiled with the Microsoft compiler, this macro
specifies the version of the compiler being used.  This affects, for example,
the static initialization method used by the generated code.  The number is the
Microsoft version number of a Microsoft C/C++ compiler release (e.g., for MSVC
version 7 this is 1300 -- the value of the _MSC_VER preprocessor macro in that
version).
*/
#ifndef MSVC_TARGET_VERSION_NUMBER
#define MSVC_TARGET_VERSION_NUMBER DEFAULT_MICROSOFT_VERSION
#endif /* MSVC_TARGET_VERSION_NUMBER */

/*
Switch that is TRUE if the C-generating or C++-generating back end should
generate code taking advantage of Microsoft extensions.  This does not by
itself cause the back ends to compensate for bugs in the Microsoft compiler.
This is the initial value of the global variable
microsoft_dialect_is_generated_code_target.
*/
#ifndef MICROSOFT_DIALECT_IS_GENERATED_CODE_TARGET
#if MSVC_IS_GENERATED_CODE_TARGET && \
    !(BACK_END_IS_CP_GEN_BE && CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT)
#define MICROSOFT_DIALECT_IS_GENERATED_CODE_TARGET TRUE
#else /* !(MSVC_IS_GENERATED_CODE_TARGET && ...) */
#define MICROSOFT_DIALECT_IS_GENERATED_CODE_TARGET FALSE
#endif /* MSVC_IS_GENERATED_CODE_TARGET */
#endif /* MICROSOFT_DIALECT_IS_GENERATED_CODE_TARGET */

/*
Compensating for Microsoft compiler bugs without actually generating
Microsoft extensions is not likely an intentional configuration option.
(It's allowed with CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT because
MSVC_IS_GENERATED_CODE_TARGET is ignored in non-Microsoft modes.)
*/
#if MSVC_IS_GENERATED_CODE_TARGET &&                \
    !(MICROSOFT_DIALECT_IS_GENERATED_CODE_TARGET || \
      (BACK_END_IS_CP_GEN_BE &&                     \
       CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT))
 #error -- MSVC_IS_GENERATED_CODE_TARGET requires        \
           MICROSOFT_DIALECT_IS_GENERATED_CODE_TARGET or \
           CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT
#endif /* MSVC_IS_GENERATED_CODE_TARGET && !MICROSOFT_DIALECT_... */
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */

#if BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE
/*
The C-generating and C++-generating back ends should never have to generate
code for more than one specific target dialect (Microsoft, GNU, Sun, or Clang).
*/
#if MICROSOFT_DIALECT_IS_GENERATED_CODE_TARGET
#ifndef SPECIFIC_TARGET_DIALECT_SET
#define SPECIFIC_TARGET_DIALECT_SET TRUE
#else /* !defined(SPECIFIC_TARGET_DIALECT_SET) */
#define MULTIPLE_TARGET_DIALECTS_SET TRUE
#endif /* ifndef SPECIFIC_TARGET_DIALECT_SET */
#endif /* MICROSOFT_DIALECT_IS_GENERATED_CODE_TARGET */

#if GCC_IS_GENERATED_CODE_TARGET
#ifndef SPECIFIC_TARGET_DIALECT_SET
#define SPECIFIC_TARGET_DIALECT_SET TRUE
#else /* !defined(SPECIFIC_TARGET_DIALECT_SET) */
#define MULTIPLE_TARGET_DIALECTS_SET TRUE
#endif /* ifndef SPECIFIC_TARGET_DIALECT_SET */
#endif /* GCC_IS_GENERATED_CODE_TARGET */

#if SUN_IS_GENERATED_CODE_TARGET
#ifndef SPECIFIC_TARGET_DIALECT_SET
#define SPECIFIC_TARGET_DIALECT_SET TRUE
#else /* !defined(SPECIFIC_TARGET_DIALECT_SET) */
#define MULTIPLE_TARGET_DIALECTS_SET TRUE
#endif /* ifndef SPECIFIC_TARGET_DIALECT_SET */
#endif /* SUN_IS_GENERATED_CODE_TARGET */

#if CLANG_IS_GENERATED_CODE_TARGET
#ifndef SPECIFIC_TARGET_DIALECT_SET
#define SPECIFIC_TARGET_DIALECT_SET TRUE
#else /* !defined(SPECIFIC_TARGET_DIALECT_SET) */
#define MULTIPLE_TARGET_DIALECTS_SET TRUE
#endif /* ifndef SPECIFIC_TARGET_DIALECT_SET */
#endif /* CLANG_IS_GENERATED_CODE_TARGET */

#ifdef MULTIPLE_TARGET_DIALECTS_SET
 #error -- Multiple target dialects selected (GCC_IS_GENERATED_CODE_TARGET, \
           SUN_IS_GENERATED_CODE_TARGET, CLANG_IS_GENERATED_CODE_TARGET, or \
           MICROSOFT_DIALECT_IS_GENERATED_CODE_TARGET)
#endif /* ifdef MULTIPLE_TARGET_DIALECTS_SET */
#endif /* BACK_END_IS_C_GEN_BE || BACK_END_IS_CP_GEN_BE */

#if BACK_END_IS_C_GEN_BE
/*
Switch that is TRUE if the C-generating back end should generate ANSI C
instead of K&R C.
*/
#ifndef C_GEN_BE_GENERATES_ANSI_C
#if GCC_IS_GENERATED_CODE_TARGET || USING_ISO_C
#define C_GEN_BE_GENERATES_ANSI_C TRUE
#else /* !(GCC_IS_GENERATED_CODE_TARGET || USING_ISO_C) */
#define C_GEN_BE_GENERATES_ANSI_C FALSE
#endif /* GCC_IS_GENERATED_CODE_TARGET || USING_ISO_C */
#endif /* !defined(C_GEN_BE_GENERATES_ANSI_C) */

/*
TRUE if the C-generating back end should generate code that is compatible
with the C23 Standard.
*/
#ifndef C_GEN_BE_GENERATES_C23
#if GCC_IS_GENERATED_CODE_TARGET && GNU_TARGET_VERSION_NUMBER >= 150000
#define C_GEN_BE_GENERATES_C23 TRUE
#else /* !(GCC_IS_GENERATED_CODE_TARGET && ...) */
#define C_GEN_BE_GENERATES_C23 FALSE
#endif /* GCC_IS_GENERATED_CODE_TARGET && ... */
#endif /* !defined(C_GEN_BE_GENERATES_C23 */

#if C_GEN_BE_GENERATES_C23 && !C_GEN_BE_GENERATES_ANSI_C
#error C_GEN_BE_GENERATES_C23 requires C_GEN_BE_GENERATES_ANSI_C
#endif /* C_GEN_BE_GENERATES_C23 */

/*
If SUPPRESS_CONST_IN_GENERATED_C is TRUE, "const" will not be put out when
the C-generating back end generates ANSI C.  (const is never put out when
generating K&R C.)
*/
#ifndef SUPPRESS_CONST_IN_GENERATED_C
#define SUPPRESS_CONST_IN_GENERATED_C FALSE
#endif /* !defined(SUPPRESS_CONST_IN_GENERATED_C) */

/*
Control whether "long double" is put out as "long double" or as "double"
in generated C code.
*/
#ifndef LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C
#if C_GEN_BE_GENERATES_ANSI_C
/* Generating ANSI C.  If "long double" or "__float128" is used as the host
floating point representation or if double and long double have different
representations, put out "long double" in the generated C. */
#if USE_DOUBLE_FOR_HOST_FP_VALUE && \
    TARG_SIZEOF_DOUBLE == TARG_SIZEOF_LONG_DOUBLE
#define LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C TRUE
#else /* !(USE_DOUBLE_FOR_HOST_FP_VALUE && ...) */
#define LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C FALSE
#endif /* USE_DOUBLE_FOR_HOST_FP_VALUE && ... */
#else /* !C_GEN_BE_GENERATES_ANSI_C */
/* Generating K&R C. */
#if FP_LONG_DOUBLE_IS_80BIT_EXTENDED || FP_LONG_DOUBLE_IS_BINARY128
/* long double is configured to be larger than double, so we cannot use
   double for long double. */
#define LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C FALSE
#else /* !(FP_LONG_DOUBLE_IS_80BIT_EXTENDED || ...) */
#define LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C TRUE
#endif /* FP_LONG_DOUBLE_IS_80BIT_EXTENDED || FP_LONG_DOUBLE_IS_BINARY128 */
#endif /* C_GEN_BE_GENERATES_ANSI_C */
#endif /* ifndef LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C */

#if LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C
#if TARG_SIZEOF_DOUBLE != TARG_SIZEOF_LONG_DOUBLE
 #error -- TARG_SIZEOF_DOUBLE must equal TARG_SIZEOF_LONG_DOUBLE when \
           LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C is TRUE
#endif /* TARG_SIZEOF_DOUBLE != TARG_SIZEOF_LONG_DOUBLE */
#if TARG_ALIGNOF_DOUBLE != TARG_ALIGNOF_LONG_DOUBLE
 #error -- TARG_ALIGNOF_DOUBLE must equal TARG_ALIGNOF_LONG_DOUBLE when \
           LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C is TRUE
#endif /* TARG_ALIGNOF_DOUBLE != TARG_ALIGNOF_LONG_DOUBLE */
#endif /* LONG_DOUBLE_AS_DOUBLE_IN_GENERATED_C */

/*
Control whether a warning is put out when "long double" is put out
as "double."
*/
#ifndef ISSUE_WARNING_ON_LONG_DOUBLE_AS_DOUBLE
#define ISSUE_WARNING_ON_LONG_DOUBLE_AS_DOUBLE FALSE
#endif /* ifndef ISSUE_WARNING_ON_LONG_DOUBLE_AS_DOUBLE */

/*
If ALLOW_ADDR_OF_REGISTER_IN_GENERATED_C is TRUE, "register" will be put
out for register variables whose address is taken.  This can occur when
compiling ANSI C code in SVR4 C compatibility mode.
*/
#ifndef ALLOW_ADDR_OF_REGISTER_IN_GENERATED_C
#define ALLOW_ADDR_OF_REGISTER_IN_GENERATED_C FALSE
#endif /* !defined(ALLOW_ADDR_OF_REGISTER_IN_GENERATED_C) */

/*
If the C-generating back end is being used, are bit fields in the
generated C allowed to have base types other than the standard
"int" and "unsigned int"?
*/
#ifndef ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C
#if TARG_BIT_FIELD_CONTAINER_SIZE < 0 || GCC_IS_GENERATED_CODE_TARGET
/* See comment below regarding TARG_BIT_FIELD_CONTAINER_SIZE.  When
   generating code for gcc, we know that non-int bit field types are
   allowed. */
#define ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C TRUE
#else /* !(TARG_BIT_FIELD_CONTAINER_SIZE < 0 || ... ) */
#if CFRONT_OBJECT_CODE_COMPATIBILITY || ABI_COMPATIBILITY_VERSION < 235
/* In the C code it generates, cfront changes the underlying types of all
   bit-fields to int or unsigned int. */
#define ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C FALSE
#else /* !(CFRONT_OBJECT_CODE_COMPATIBILITY || ...) */
#define ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C TRUE
#endif /* CFRONT_OBJECT_CODE_COMPATIBILITY || ... */
#endif /* TARG_BIT_FIELD_CONTAINER_SIZE < 0 || ... */
#endif /* ifndef ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C */
#if !ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C && \
    TARG_BIT_FIELD_CONTAINER_SIZE < 0
/* When TARG_BIT_FIELD_CONTAINER_SIZE is less than 0, meaning "use the
   base type given in the declaration", we have to use the original
   bit field type or we won't get the right layout. */
 #error -- ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C must be TRUE \
           when TARG_BIT_FIELD_CONTAINER_SIZE < 0
#endif /* !ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C && ... */

/*
If the C-generating back end is being used, are "?" operators allowed
to have operands of void type?  pcc, for example, does not allow them.
If they are not allowed, they are rewritten as "(operand, 0)".
*/
#ifndef ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C
#if C_GEN_BE_GENERATES_ANSI_C
#define ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C TRUE
#else /* !C_GEN_BE_GENERATES_ANSI_C */
#define ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C FALSE
#endif /* C_GEN_BE_GENERATES_ANSI_C */
#endif /* ifndef ALLOW_VOID_QUESTION_OPERAND_IN_GENERATED_C */

/*
If the C-generating back end is being used, and the target environment
has .init sections (e.g., SVR4), this flag is TRUE to enable generation of
asm directives to get startup routines called (thus eliminating the need
for patch or munch).  Note that gcc has a better of way of doing this,
so it's not necessarily helpful to set this to TRUE when using gcc as the
target C compiler.  Likewise for the Sunpro C compiler and MSVC++.
*/
#ifndef USE_INIT_SECTION_IN_GENERATED_C
#define USE_INIT_SECTION_IN_GENERATED_C FALSE
#endif /* ifndef USE_INIT_SECTION_IN_GENERATED_C */

/*
This switch controls how the C-generating back represents an empty C++
class.  The C++ Standard requires that the size of an empty class object
that is not a base class subobject be one byte, so the natural
representation of such a class in C is a struct with one byte of padding.
However, when passing an empty class object by value, g++ on 64-bit
architectures uses the calling sequence gcc uses for an empty struct, which
is different from that used for a one-byte struct; in such cases, an empty
struct should be generated (and padding and initialization modified
accordingly).  (On 32-bit architectures it uses the calling sequence for a
one-byte struct, so the natural representation presents no problems in this
regard.)  This is the initial value of the use_empty_struct_in_generated_c
global variable.  Clang appears to use an alternate calling sequence for
empty structs in both 32-bit and 64-bit architectures.
*/
#ifndef USE_EMPTY_STRUCT_IN_GENERATED_C
#if (GCC_IS_GENERATED_CODE_TARGET && TARG_SIZEOF_POINTER == 8) || \
     CLANG_IS_GENERATED_CODE_TARGET
#define USE_EMPTY_STRUCT_IN_GENERATED_C TRUE
#else /* !(GCC_IS_GENERATED_CODE_TARGET && TARG_SIZEOF_POINTER == 8) || ... */
#define USE_EMPTY_STRUCT_IN_GENERATED_C FALSE
#endif /* GCC_IS_GENERATED_CODE_TARGET && TARG_SIZEOF_POINTER == 8 || ... */
#endif /* USE_EMPTY_STRUCT_IN_GENERATED_C */

/*
If ALLOW_ELLIPSIS_ONLY_PARAM_IN_GENERATED_C is TRUE, "(...)" will be put
out by the C-generating back end as the parameter list for a routine with
no parameters and has_ellipsis set to TRUE.  The setting of this switch is
irrelevant in C mode if the construct is not allowed in the source (see
allow_ellipsis_only_param_in_C_mode).  However, the source construct is
always allowed in C++ mode, and this switch also controls the form of the
generated C for that case.  (Note that it does not affect the output of the
C++-generating back end and form_function_declarator in il_to_str.c, where
the output should reflect the actual source form of the declaration.)
*/
#ifndef ALLOW_ELLIPSIS_ONLY_PARAM_IN_GENERATED_C
#if C_GEN_BE_GENERATES_C23
#define ALLOW_ELLIPSIS_ONLY_PARAM_IN_GENERATED_C TRUE
#else /* !C_GEN_BE_GENERATES_C23 */
#define ALLOW_ELLIPSIS_ONLY_PARAM_IN_GENERATED_C FALSE
#endif /* C_GEN_BE_GENERATES_C23 */
#endif /* ifndef ALLOW_ELLIPSIS_ONLY_PARAM_IN_GENERATED_C */
#endif /* BACK_END_IS_C_GEN_BE */

/*
When generating C or C++ code, add extra braces around "if" statements
without an "else" to avoid the "dangling else" problem.  This is necessary
only if customer code modifies the IL statement tree.
*/
#ifndef ADD_BRACES_TO_AVOID_DANGLING_ELSE_IN_GENERATED_C
#define ADD_BRACES_TO_AVOID_DANGLING_ELSE_IN_GENERATED_C FALSE
#endif /* ifndef ADD_BRACES_TO_AVOID_DANGLING_ELSE_IN_GENERATED_C */

#ifdef USE_PRAGMA_IDENT_IN_GENERATED_CODE
/* Up until version 4.10, USE_PRAGMA_IDENT_IN_GENERATED_CODE could be used to
   determine how #ident and #pragma ident directives would be emitted in
   C- and C++-generating back ends.  They are now emitted using the same
   format that appears in the source.  Give an error if the macro was defined
   and has a value other than the previous default (which was FALSE). */
#if USE_PRAGMA_IDENT_IN_GENERATED_CODE
 #error -- the USE_PRAGMA_IDENT_IN_GENERATED_CODE macro has been eliminated
#endif /* USE_PRAGMA_IDENT_IN_GENERATED_CODE */
#endif /* ifdef USE_PRAGMA_IDENT_IN_GENERATED_CODE */

/*
Flag that is TRUE if, when the C-generating back end (c_gen_be) or
C++/C-generating back end (cp_gen_be) is run, the "restrict" keyword should
be suppressed in the output.
*/
#ifndef SUPPRESS_RESTRICT_IN_GENERATED_CODE
#define SUPPRESS_RESTRICT_IN_GENERATED_CODE TRUE
#endif /* SUPPRESS_RESTRICT_IN_GENERATED_CODE */

/*
Flag that is TRUE if, when the C-generating back end (c_gen_be) or
C++/C-generating back end (cp_gen_be) is run, the "static" keyword in
array declarators (a C99 feature) should be suppressed in the output.
*/
#ifndef SUPPRESS_ARRAY_STATIC_IN_GENERATED_CODE
#define SUPPRESS_ARRAY_STATIC_IN_GENERATED_CODE TRUE
#endif /* SUPPRESS_ARRAY_STATIC_IN_GENERATED_CODE */

/*
Flag that is TRUE if the C++-generating back end should generate code for a
compiler that does not visibly inject friend function declarations in the
surrounding namespace scope.  This flag is only applicable if
MICROSOFT_EXTENSIONS_ALLOWED is TRUE.
*/
#if MICROSOFT_EXTENSIONS_ALLOWED && BACK_END_IS_CP_GEN_BE
#ifndef TARG_CPP_COMPILER_DOES_NOT_VISIBLY_INJECT_FRIEND_NAMES
#define TARG_CPP_COMPILER_DOES_NOT_VISIBLY_INJECT_FRIEND_NAMES FALSE
#endif /* ifndef TARG_CPP_COMPILER_DOES_NOT_VISIBLY_INJECT_FRIEND_NAMES */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED && BACK_END_IS_CP_GEN_BE */

/*
Flag that is TRUE if, when the C-generating back end (c_gen_be) or
C++/C-generating back end (cp_gen_be) is run, near and far should be
suppressed in the output.  This flag is only applicable if
NEAR_AND_FAR_ALLOWED is TRUE.
*/
#ifndef SUPPRESS_NEAR_AND_FAR_IN_GENERATED_CODE
#define SUPPRESS_NEAR_AND_FAR_IN_GENERATED_CODE FALSE
#endif /* SUPPRESS_NEAR_AND_FAR_IN_GENERATED_CODE */

/*
Flag that is TRUE if the C++/C-generating back end should issue class member
using-declarations instead of access declarations.
*/
#ifndef USING_DECLARATIONS_IN_GENERATED_CODE
#define USING_DECLARATIONS_IN_GENERATED_CODE TRUE
#endif /* USING_DECLARATIONS_IN_GENERATED_CODE */

/*
Flag that is TRUE if, when the C++/C-generating back end (cp_gen_be)
is run, "specializations" for generated template instances should use
the old syntax instead of the modern "template <>" prefix form.  This is
the initial value of old_specializations_for_generated_instances.
*/
#ifndef DEFAULT_OLD_SPECIALIZATIONS_FOR_GENERATED_INSTANCES
#define DEFAULT_OLD_SPECIALIZATIONS_FOR_GENERATED_INSTANCES FALSE
#endif /* DEFAULT_OLD_SPECIALIZATIONS_FOR_GENERATED_INSTANCES */

/*
When generating floating-point constants in generated code, if
USE_HEX_FP_CONSTANTS_IN_GENERATED_CODE is TRUE, hexadecimal floating-point
constants (e.g., 0x1.6666666666666p+2) will be emitted in the generated code
rather than the traditional decimal floating-point constants.  This has the
advantage of requiring one less floating-point to decimal conversion in the
front end and one less decimal to floating-point conversion in the back end.
Hexadecimal floating-point constants are standard in C99 and are also supported
in some C++ compilers (e.g., g++).  The compiler that is used to compile the
front end must support the "%a" C formatting directive when
USE_HEX_FP_CONSTANTS_IN_GENERATED_CODE is TRUE.
*/
#ifndef USE_HEX_FP_CONSTANTS_IN_GENERATED_CODE
#define USE_HEX_FP_CONSTANTS_IN_GENERATED_CODE FALSE
#endif /* ifndef USE_HEX_FP_CONSTANTS_IN_GENERATED_CODE */

/*
Flag that is TRUE if a field of some built-in type requires a different
alignment than a variable of that same type.  Some GNU compilers exhibit
this behavior on Intel x86-based platforms.  Initial value of the
targ_dual_alignments_for_builtin_types global variable.
*/
#ifndef TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES
#if GCC_IS_GENERATED_CODE_TARGET
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES TRUE
#else /* !GCC_IS_GENERATED_CODE_TARGET */
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES FALSE
#endif /* GCC_IS_GENERATED_CODE_TARGET */
#endif /* ifndef TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES */

/*
The default field alignments for built-in types.  This is different from the
intrinsic alignment of the type for some GNU compilers.  By default however,
we define these equal to the corresponding intrinsic alignments.
*/
#ifndef TARG_SHORT_FIELD_ALIGNMENT
#define TARG_SHORT_FIELD_ALIGNMENT TARG_ALIGNOF_SHORT
#endif /* TARG_SHORT_FIELD_ALIGNMENT */

#ifndef TARG_INT_FIELD_ALIGNMENT
#define TARG_INT_FIELD_ALIGNMENT TARG_ALIGNOF_INT
#endif /* TARG_INT_FIELD_ALIGNMENT */

#ifndef TARG_LONG_FIELD_ALIGNMENT
#define TARG_LONG_FIELD_ALIGNMENT TARG_ALIGNOF_LONG
#endif /* TARG_LONG_FIELD_ALIGNMENT */

#if LONG_LONG_ALLOWED
#ifndef TARG_LONG_LONG_FIELD_ALIGNMENT
#define TARG_LONG_LONG_FIELD_ALIGNMENT TARG_ALIGNOF_LONG_LONG
#endif /* TARG_LONG_LONG_FIELD_ALIGNMENT */
#endif /* LONG_LONG_ALLOWED */

#if INT128_EXTENSIONS_ALLOWED
#ifndef TARG_INT128_FIELD_ALIGNMENT
#define TARG_INT128_FIELD_ALIGNMENT TARG_ALIGNOF_INT128
#endif /* TARG_INT128_FIELD_ALIGNMENT */
#endif /* INT128_EXTENSIONS_ALLOWED */

#ifndef TARG_FLOAT_FIELD_ALIGNMENT
#define TARG_FLOAT_FIELD_ALIGNMENT TARG_ALIGNOF_FLOAT
#endif /* TARG_FLOAT_FIELD_ALIGNMENT */

#ifndef TARG_DOUBLE_FIELD_ALIGNMENT
#define TARG_DOUBLE_FIELD_ALIGNMENT TARG_ALIGNOF_DOUBLE
#endif /* TARG_DOUBLE_FIELD_ALIGNMENT */

#ifndef TARG_LONG_DOUBLE_FIELD_ALIGNMENT
#define TARG_LONG_DOUBLE_FIELD_ALIGNMENT TARG_ALIGNOF_LONG_DOUBLE
#endif /* TARG_LONG_DOUBLE_FIELD_ALIGNMENT */

#ifndef TARG_FLOAT80_FIELD_ALIGNMENT
#define TARG_FLOAT80_FIELD_ALIGNMENT TARG_ALIGNOF_FLOAT80
#endif /* TARG_FLOAT80_FIELD_ALIGNMENT */

#ifndef TARG_FLOAT128_FIELD_ALIGNMENT
#define TARG_FLOAT128_FIELD_ALIGNMENT TARG_ALIGNOF_FLOAT128
#endif /* TARG_FLOAT128_FIELD_ALIGNMENT */

/*
Flag that is TRUE if, when the C-generating back end (c_gen_be) or
C++/C-generating back end (cp_gen_be) is run, references to the
<stdarg.h> macros should be scanned specially and output in the
original form.  This avoids problems with language extensions
used to implement those.  This is the initial value of
pass_stdarg_references_to_generated_code.

Note that GUARD_MACRO_FOR_VA_LIST can be defined to be a quoted string
that is the name of a macro to be defined when the built-in va_list
is defined.  If it is not set, no macro is defined.  A second macro
can be specified with GUARD_MACRO2_FOR_VA_LIST.
*/
#ifndef DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE
#if BACK_END_IS_C_GEN_BE
#if C_GEN_BE_GENERATES_ANSI_C
/* The C-generating back end is being used, and is generating ANSI/ISO C. */
#define DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE TRUE
#else /* !C_GEN_BE_GENERATES_ANSI_C */
/* The C-generating back end is being used, and is generating old-style C. */
#define DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE FALSE
#endif /* C_GEN_BE_GENERATES_ANSI_C */
#else /* !BACK_END_IS_C_GEN_BE */
#if BACK_END_IS_CP_GEN_BE
/* The C++-generating back end is being used. */
#define DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE TRUE
#else /* !BACK_END_IS_CP_GEN_BE */
/* Neither the C-generating back end nor the C++-generating back end is being
   used. */
#define DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE FALSE
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* BACK_END_IS_C_GEN_BE */
#endif /* ifndef DEFAULT_PASS_STDARG_REFERENCES_TO_GENERATED_CODE */

/*
Name of a global type which, if defined when <stdarg.h> is included,
indicates the type to be used for the built-in va_list.
*/
#ifndef BUILTIN_VA_LIST_OVERRIDE_TYPE_NAME
#define BUILTIN_VA_LIST_OVERRIDE_TYPE_NAME "__edg_va_list"
#endif /* ifndef BUILTIN_VA_LIST_OVERRIDE_TYPE_NAME */

/*
Flag that is TRUE if the built-in va_start operation maps on a target
operation that takes the address of its second (variable) operand.
*/
#ifndef BUILTIN_VA_START_TAKES_ADDRESS_OF_VARIABLE
#define BUILTIN_VA_START_TAKES_ADDRESS_OF_VARIABLE TRUE
#endif /* ifndef BUILTIN_VA_START_TAKES_ADDRESS_OF_VARIABLE */

/*
Flag that is TRUE if, by default, the va_list type should be in namespace
std when passing stdarg references to the generated code.
*/
#ifndef DEFAULT_VA_LIST_IN_STD_NAMESPACE
#define DEFAULT_VA_LIST_IN_STD_NAMESPACE TRUE
#endif /* ifndef DEFAULT_VA_LIST_IN_STD_NAMESPACE */

/*
If this flag is TRUE, integer types with the same representation
(same size, alignment, and signedness) are considered to be
identical in the IL.  This requires back end support, i.e., the back
end must be comfortable with the fact that these types will be used
interchangeably without casts between them.  In pcc mode, such types
will be considered identical, which will typically make "int" and
"long" interchangeable, and likewise "unsigned int" and "unsigned
long".  ANSI mode IL is affected in that casts between such types
will not be generated.  However, the language accepted in ANSI mode
is not affected; such types are not considered to be identical, and
errors are still generated for type mismatches.
*/
#ifndef SAME_REPR_INTS_INTERCHANGEABLE_IN_IL
#if BACK_END_IS_C_GEN_BE && !C_GEN_BE_GENERATES_ANSI_C
#define SAME_REPR_INTS_INTERCHANGEABLE_IN_IL TRUE
#else /* !(BACK_END_IS_C_GEN_BE && !C_GEN_BE_GENERATES_ANSI_C) */
#define SAME_REPR_INTS_INTERCHANGEABLE_IN_IL FALSE
#endif /* BACK_END_IS_C_GEN_BE && !C_GEN_BE_GENERATES_ANSI_C */
#endif /* !defined(SAME_REPR_INTS_INTERCHANGEABLE_IN_IL) */

/*
Default value for distinct_template_signatures.  Controls whether the
signatures for template functions can match those for non-template
functions across separate compilation units.  In the modern C++
language, a normal function cannot be used to satisfy the need for a
template instance.  For example, a function "void f(int)" could not be
used to satisfy the need for an instantiation of a template "void
f(T)" with T set to int.  In older versions of the language, the name
mangling for templates was the same as for nontemplates, and a
nontemplate function could satisfy the need for a template function.
Distinct template signatures must be enabled in order to use function
template parameters that are not part of the signature of the function
template.
*/
#ifndef DEFAULT_DISTINCT_TEMPLATE_SIGNATURES

/* This configuration flag was renamed.  If there is no definition for
   the new name, but there is one for the old name, use the value specified
   for the old name. */
#ifdef DEFAULT_DISTINCT_MANGLING_FOR_TEMPLATES
#define DEFAULT_DISTINCT_TEMPLATE_SIGNATURES \
        DEFAULT_DISTINCT_MANGLING_FOR_TEMPLATES
#else /* ifndef DEFAULT_DISTINCT_MANGLING_FOR_TEMPLATES */

/* This feature CAN be turned on when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#if ABI_COMPATIBILITY_VERSION < 232 || CFRONT_OBJECT_CODE_COMPATIBILITY
#define DEFAULT_DISTINCT_TEMPLATE_SIGNATURES FALSE
#else /* !(ABI_COMPATIBILITY_VERSION < 232 || ...) */
#define DEFAULT_DISTINCT_TEMPLATE_SIGNATURES TRUE
#endif /* ABI_COMPATIBILITY_VERSION < 232 || ... */

#endif /* ifdef DEFAULT_DISTINCT_MANGLING_FOR_TEMPLATES */
#endif /* ifndef DEFAULT_DISTINCT_TEMPLATE_SIGNATURES */

#if !DEFAULT_DISTINCT_TEMPLATE_SIGNATURES && EXPORT_ENABLING_POSSIBLE
 #error DEFAULT_DISTINCT_TEMPLATE_SIGNATURES FALSE incompatible with \
        EXPORT_ENABLING_POSSIBLE
#endif /* !DEFAULT_DISTINCT_TEMPLATE_SIGNATURES && EXPORT_ENABLING_POSSIBLE */

#if NEED_NAME_MANGLING && !IA64_ABI
/*
Default value for compress_mangled_names, which controls whether compression
is done on mangled names.  The IA-64 ABI doesn't use compression.
*/
#ifndef DEFAULT_COMPRESS_MANGLED_NAMES
/* This feature CAN be turned on when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#if ABI_COMPATIBILITY_VERSION < 241 || CFRONT_OBJECT_CODE_COMPATIBILITY
#define DEFAULT_COMPRESS_MANGLED_NAMES FALSE
#else /* !(ABI_COMPATIBILITY_VERSION < 241 || ... ) */
#define DEFAULT_COMPRESS_MANGLED_NAMES TRUE
#endif /* ABI_COMPATIBILITY_VERSION < 241 || ... */
#endif /* ifndef DEFAULT_COMPRESS_MANGLED_NAMES */
#endif /* NEED_NAME_MANGLING && !IA64_ABI */

#if NEED_NAME_MANGLING
/*
Default value for max_mangled_name_length, which controls the maximum
length of mangled names.  Zero means no limit.  Names longer than
the limit are truncated by addition of a CRC code.  That makes them shorter
but no longer decodable.
*/
#ifndef DEFAULT_MAX_MANGLED_NAME_LENGTH
#define DEFAULT_MAX_MANGLED_NAME_LENGTH 0 /* No limit. */
#endif /* ifndef DEFAULT_MAX_MANGLED_NAME_LENGTH */
#endif /* NEED_NAME_MANGLING */

/*
This switch controls whether or not the ABI changes for runtime
type information (RTTI) are done.  This affects element 0 of virtual
function tables, the BCS_PUBLIC and BCS_AMBIGUOUS flags in base class
arrays, and the user type_info and name fields in the typeinfo
implementation structure.  Because the name field must be initialized
in all cases, typeinfo variables for non-class types are always
initialized in the new scheme (in the old scheme, a tentative
definition with default initialization to zero was enough).
If the switch is off, compatibility with versions up to 2.28
is preserved, but the RTTI language features are turned off.
The typeinfo generated in that case is adequate for exception
handling but not for RTTI.
*/
#ifndef ABI_CHANGES_FOR_RTTI
#if IA64_ABI
#define ABI_CHANGES_FOR_RTTI TRUE /* Do not change this. */
#else /* !IA64_ABI */
/* This feature CAN be turned on when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#if ABI_COMPATIBILITY_VERSION <= 228 || CFRONT_OBJECT_CODE_COMPATIBILITY
#define ABI_CHANGES_FOR_RTTI FALSE /* Versions up to 2.28. */
#else /* ABI_COMPATIBILITY_VERSION > 228  && !CFRONT_... */
#define ABI_CHANGES_FOR_RTTI TRUE  /* Versions after 2.28. */
#endif /* ABI_COMPATIBILITY_VERSION <= 228 || CFRONT_... */
#endif /* !IA64_ABI */
#endif /* ifndef ABI_CHANGES_FOR_RTTI */
#if ABI_CHANGES_FOR_RTTI && (ABI_COMPATIBILITY_VERSION <= 228)
 #error -- ABI_CHANGES_FOR_RTTI TRUE is incompatible with \
           ABI_COMPATIBILITY_VERSION <= 228
#endif /* ABI_CHANGES_FOR_RTTI && (ABI_COMPATIBILITY_VERSION <= 228) */
#if IA64_ABI && !ABI_CHANGES_FOR_RTTI
 #error -- IA64_ABI requires ABI_CHANGES_FOR_RTTI
#endif /* IA64_ABI && !ABI_CHANGES_FOR_RTTI */

#if DO_IL_LOWERING && ABI_CHANGES_FOR_RTTI
/*
This switch controls whether the typeinfo variables for RTTI are generated
when RTTI is turned off.  Setting this switch to TRUE will reduce memory
use in applications that never use RTTI, but it also makes it possible to
end up with configuration mismatches and link errors or runtime aborts,
e.g., by compiling part of the program in one mode and part in another.
See also the variable generate_rtti_typeinfo.
*/
#ifndef SUPPRESS_TYPEINFO_VARIABLES_WHEN_RTTI_DISABLED
#if DEFAULT_EMULATE_GNU_ABI_BUGS
#define SUPPRESS_TYPEINFO_VARIABLES_WHEN_RTTI_DISABLED TRUE
#else /* !DEFAULT_EMULATE_GNU_ABI_BUGS */
#define SUPPRESS_TYPEINFO_VARIABLES_WHEN_RTTI_DISABLED FALSE
#endif /* DEFAULT_EMULATE_GNU_ABI_BUGS */
#endif /* ifndef SUPPRESS_TYPEINFO_VARIABLES_WHEN_RTTI_DISABLED */
#endif /* DO_IL_LOWERING && ABI_CHANGES_FOR_RTTI */

/*
This switch controls whether or not the ABI changes for array
new and delete are done.  New runtime routines are added, and the
way array sizes are recorded by the runtime is different.
The changes are upward-compatible (you can use old object code
with new object code and the new library).  If the switch is off,
compatibility with versions up to 2.28 is preserved, but the
array new and delete language features are turned off.
*/
#ifndef ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE
/* This feature CAN be turned on when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#if ABI_COMPATIBILITY_VERSION <= 228 || CFRONT_OBJECT_CODE_COMPATIBILITY
#define ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE FALSE /* Versions up to 2.28. */
#else /* ABI_COMPATIBILITY_VERSION > 228 && !CFRONT_... */
#define ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE TRUE  /* Versions after 2.28. */
#endif /* ABI_COMPATIBILITY_VERSION <= 228 || CFRONT_... */
#endif /* ifndef ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE */
#if ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE && (ABI_COMPATIBILITY_VERSION <= 228)
 #error -- ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE TRUE is incompatible with \
           ABI_COMPATIBILITY_VERSION <= 228
#endif /* ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE && ... */

/*
This switch controls whether or not the ABI changes for placement
delete are done.  New runtime routines/variables are added.
The changes are upward-compatible (you can use old object code
with new object code and the new library).  If the switch is off,
compatibility with versions up to 2.33 is preserved, but the
placement delete language feature is turned off.  Allocating an
array with placement new and then using the delete operator on it
is also considered part of "placement delete" and is controlled by
this switch.
*/
#ifndef ABI_CHANGES_FOR_PLACEMENT_DELETE
/* This feature CAN be turned on when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#if ABI_COMPATIBILITY_VERSION <= 233 || CFRONT_OBJECT_CODE_COMPATIBILITY
#define ABI_CHANGES_FOR_PLACEMENT_DELETE FALSE /* Versions up to 2.33. */
#else /* ABI_COMPATIBILITY_VERSION > 233 && !CFRONT_... */
#define ABI_CHANGES_FOR_PLACEMENT_DELETE TRUE  /* Versions after 2.33. */
#endif /* ABI_COMPATIBILITY_VERSION <= 233 || CFRONT_... */
#endif /* ifndef ABI_CHANGES_FOR_PLACEMENT_DELETE */
#if ABI_CHANGES_FOR_PLACEMENT_DELETE && (ABI_COMPATIBILITY_VERSION <= 233)
 #error -- ABI_CHANGES_FOR_PLACEMENT_DELETE TRUE is incompatible with \
           ABI_COMPATIBILITY_VERSION <= 233
#endif /* ABI_CHANGES_FOR_PLACEMENT_DELETE && ... */
#if !ABI_CHANGES_FOR_PLACEMENT_DELETE && IA64_ABI
 #error -- ABI_CHANGES_FOR_PLACEMENT_DELETE FALSE is incompatible with \
           IA64_ABI
#endif /* !ABI_CHANGES_FOR_PLACEMENT_DELETE && IA64_ABI */

/*
This switch controls whether or not ABI changes are made to support
covariant return types on overriding virtual functions.  If the switch is
off, compatibility with versions up to 2.33 is preserved, but support for
covariant return types on overriding virtual functions is disabled (meaning
errors will be issued when compiling programs using the feature).  The same
mechanism is used to create thunks in the IA-64 ABI and therefore this
switch must be TRUE when using the IA-64 ABI.
*/
#ifndef ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
/* This feature CAN be turned on when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#if ABI_COMPATIBILITY_VERSION <= 233 || CFRONT_OBJECT_CODE_COMPATIBILITY
#define ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN FALSE
                                                    /* Versions up to 2.33. */
#else /* ABI_COMPATIBILITY_VERSION > 233 && !CFRONT_... */
#define ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN TRUE
                                                    /* Versions after 2.33. */
#endif /* ABI_COMPATIBILITY_VERSION <= 233 || CFRONT_... */
#endif /* ifndef ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
#if ABI_COMPATIBILITY_VERSION <= 233
 #error -- ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN TRUE is incompatible \
           with ABI_COMPATIBILITY_VERSION <= 233
#endif /* ABI_COMPATIBILITY_VERSION <= 233 */
#endif /* ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#if IA64_ABI && !ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
 #error -- ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN FALSE is \
           incompatible with IA64_ABI
#endif /* IA64_ABI && !ABI_... */

/*
This switch controls whether or not ABI changes are made to fix problems
with virtual function tables during construction of classes that have
virtual base classes.  The changes include adding a parameter to some
constructors and destructors and increasing the size of a field in
the region table for the portable implementation of EH.
*/
#ifndef ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
/* This feature CAN be turned on when CFRONT_OBJECT_CODE_COMPATIBILITY is on,
   but that produces a cfront-like ABI rather than a cfront-compatible ABI. */
#if ABI_COMPATIBILITY_VERSION <= 238 || CFRONT_OBJECT_CODE_COMPATIBILITY
#define ABI_CHANGES_FOR_CONSTRUCTION_VTBLS FALSE
                                                    /* Versions up to 2.38. */
#else /* ABI_COMPATIBILITY_VERSION > 238 && !CFRONT_... */
#define ABI_CHANGES_FOR_CONSTRUCTION_VTBLS TRUE
                                                    /* Versions after 2.38. */
#endif /* ABI_COMPATIBILITY_VERSION <= 238 || CFRONT_... */
#endif /* ifndef ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
#if ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
#if ABI_COMPATIBILITY_VERSION <= 238
 #error -- ABI_CHANGES_FOR_CONSTRUCTION_VTBLS TRUE is incompatible \
           with ABI_COMPATIBILITY_VERSION <= 238
#endif /* ABI_COMPATIBILITY_VERSION <= 238 */
#endif /* ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
#if IA64_ABI && !ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
 #error -- The IA-64 ABI requires ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
#endif /* IA64_ABI && !ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */

/*
In IA-64 ABI versions through (and including) 4.0, construction/destruction of
virtual base class objects was delegated from the complete object
constructor/destructor to the subobject constructor/destructor; such delegation
was indicated at run-time by passing a NULL for the construction vtable
argument to the subobject constructor/destructor.  The IA-64 ABI (in section
3.3.1) doesn't allow for the possibility of a NULL VTT argument, causing a
potential run-time issue if an EDG-generated complete constructor/destructor
were to invoke a GNU generated constructor/destructor.  In practice this is
rare because the complete and subobject constructor/destructors are typically
emitted in the same translation unit.  In versions after 4.0, the default
behavior has changed to handle virtual bases in the complete object
constructor/destructor (HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS is TRUE)
and not in the subobject constructor/destructor
(HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS is FALSE).  This behavior can be
altered by setting these configuration macros individually (at least one must
be TRUE).  In particular, if the possibility exists that a pre-4.0 complete
object constructor/destructor can call a post-4.0 subobject
constructor/destructor, then HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS
should be set to TRUE.  Setting both flags to TRUE will ensure IA-64 ABI
compatibility as well as backward compatibility (at the cost of larger
constructor/destructors).  The Cfront ABI handling is unchanged.
*/
#if IA64_ABI
#ifndef HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS
#if ABI_COMPATIBILITY_VERSION <= 400
#define HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS FALSE
                                                    /* Versions up to 4.00. */
#else /* ABI_COMPATIBILITY_VERSION > 400 */
#define HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS TRUE
                                                    /* Versions after 4.00. */
#endif /* ABI_COMPATIBILITY_VERSION <= 400 */
#endif /* ifndef HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS */
#ifndef HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS
#if ABI_COMPATIBILITY_VERSION <= 400
#define HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS TRUE
                                                    /* Versions up to 4.00. */
#else /* ABI_COMPATIBILITY_VERSION > 400 */
#define HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS FALSE
                                                    /* Versions after 4.00. */
#endif /* ABI_COMPATIBILITY_VERSION <= 400 */
#endif /* ifndef HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS */
#if !HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS && \
    !HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS
 #error -- At least one of HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS or \
           HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS must be TRUE.
#endif /* !HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS && ... */
#else /* !IA64_ABI */
#ifndef HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS
#define HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS FALSE
#endif /* ifndef HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS */
#ifndef HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS
#define HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS TRUE
#endif /* ifndef HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS */
#if HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS
 #error -- HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS must be FALSE \
           when IA64_ABI is FALSE.
#endif /* HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS */
#if !HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS
 #error -- HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS must be TRUE \
           when IA64_ABI is FALSE.
#endif /* !HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS */
#endif /* IA64_ABI */

#if !IA64_ABI
/*
Traditionally the number_of_elements arguments to the Cfront ABI is
an int.  Setting PASS_ELEM_COUNT_TO_RUNTIME_AS_PTRDIFF_T flag to TRUE
changes the argument type to ptrdiff_t.  Supported only in
ABI versions 3.10 and higher.
*/
#ifndef PASS_ELEM_COUNT_TO_RUNTIME_AS_PTRDIFF_T
#if ABI_COMPATIBILITY_VERSION < 310 || CFRONT_OBJECT_CODE_COMPATIBILITY
#define PASS_ELEM_COUNT_TO_RUNTIME_AS_PTRDIFF_T FALSE  /* Do not change. */
#else /* !(ABI_COMPATIBILITY_VERSION < 310 || CFRONT... ) */
#define PASS_ELEM_COUNT_TO_RUNTIME_AS_PTRDIFF_T TRUE  /* Can be changed. */
#endif /* ABI_COMPATIBILITY_VERSION < 310 || CFRONT... */
#endif /* ifndef PASS_ELEM_COUNT_TO_RUNTIME_AS_PTRDIFF_T */
#if PASS_ELEM_COUNT_TO_RUNTIME_AS_PTRDIFF_T && CFRONT_OBJECT_CODE_COMPATIBILITY
 #error -- PASS_ELEM_COUNT_TO_RUNTIME_AS_PTRDIFF_T is incompatible with \
           CFRONT_OBJECT_CODE_COMPATIBILITY
#endif /* PASS_ELEM_COUNT_TO_RUNTIME_AS_PTRDIFF_T && CFRONT_... */
#if PASS_ELEM_COUNT_TO_RUNTIME_AS_PTRDIFF_T && ABI_COMPATIBILITY_VERSION < 310
 #error -- PASS_ELEM_COUNT_TO_RUNTIME_AS_PTRDIFF_T is incompatible with \
           ABI_COMPATIBILITY_VERSION < 310
#endif /* PASS_ELEM_COUNT_TO_RUNTIME_AS_PTRDIFF_T && ABI_... */
#if PASS_ELEM_COUNT_TO_RUNTIME_AS_PTRDIFF_T
#define TARG_RUNTIME_ELEM_COUNT_INT_KIND TARG_PTRDIFF_T_INT_KIND
#else /* !PASS_ELEM_COUNT_TO_RUNTIME_AS_PTRDIFF_T */
#define TARG_RUNTIME_ELEM_COUNT_INT_KIND ((an_integer_kind)ik_int)
#endif /* PASS_ELEM_COUNT_TO_RUNTIME_AS_PTRDIFF_T */
#endif /* !IA64_ABI */

/*
Flag that is TRUE if the definition of extern inline functions and
variables should be controlled by the template instantiation mechanism.

When this flag is set, only one out-of-line copy of an extern inline
function or variable is generated.  For C++17 inline variables, a
single definition is always required, so this feature is needed if COMDAT
support is not available.  For inline functions there is more flexibility.
In the Cfront-like ABI, this facility is more standard conforming as
it ensures that the address of an inline function remains constant
across translation units.  In the IA-64 ABI, instantiating extern inline
functions does not provide any advantage over lowering them, because
lowering them places them in a COMDAT section and therefore guarantees
that there is only one copy of the function at runtime.

INSTANTIATE_EXTERN_INLINE and LOWER_EXTERN_INLINE are mutually exclusive.
*/
#ifndef INSTANTIATE_EXTERN_INLINE
#define INSTANTIATE_EXTERN_INLINE FALSE
#endif /* ifndef INSTANTIATE_EXTERN_INLINE */

#if INSTANTIATE_EXTERN_INLINE && !AUTOMATIC_TEMPLATE_INSTANTIATION
 #error -- extern inline entities cannot be instantiated if automatic \
           template instantiation is disabled
#endif /* INSTANTIATE_EXTERN_INLINE && !AUTOMATIC_TEMPLATE_INSTANTIATION */

/*
This switch controls whether "extern inline" functions are rewritten as
normal inline functions (Cfront-like ABI) or put into COMDAT sections
(IA-64 ABI).  Local static variables are promoted to external so that
all references to them use the same copy.
*/
#ifndef LOWER_EXTERN_INLINE
#if INSTANTIATE_EXTERN_INLINE
#define LOWER_EXTERN_INLINE FALSE
#else /* !INSTANTIATE_EXTERN_INLINE */
#define LOWER_EXTERN_INLINE TRUE
#endif /* INSTANTIATE_EXTERN_INLINE */
#endif /* ifndef LOWER_EXTERN_INLINE */

#if LOWER_EXTERN_INLINE && INSTANTIATE_EXTERN_INLINE
 #error -- extern inline functions cannot be instantiated when they are lowered
#endif /* !(LOWER_EXTERN_INLINE && INSTANTIATE_EXTERN_INLINE) */

/*
Flag that is TRUE if the automatic instantiation mechanism should be used to
handle the instantiation of inline variables.  Like INSTANTIATE_EXTERN_INLINE,
setting this does not provide an advantage with the IA-64 ABI.  This flag is
separate from INSTANTIATE_EXTERN_INLINE so that it can be enabled for use
when LOWER_EXTERN_INLINE is being used rather than INSTANTIATE_EXTERN_INLINE.
*/
#ifndef INSTANTIATE_INLINE_VARIABLES
#if IA64_ABI || LINKER_CAN_DISCARD_DUPLICATE_DEFINITIONS
#define INSTANTIATE_INLINE_VARIABLES FALSE
#else /* !(IA64_ABI || LINKER_CAN_DISCARD_DUPLICATE_DEFINITIONS) */
#define INSTANTIATE_INLINE_VARIABLES TRUE
#endif /* IA64_ABI || LINKER_CAN_DISCARD_DUPLICATE_DEFINITIONS */
#endif /* ifndef INSTANTIATE_INLINE_VARIABLES */

#if MINIMAL_INLINING
/*
This switch controls an aspect of the minimal inlining built in to IL
lowering.  When it is TRUE, statements inserted at a call site to expand
an inline function call will have the source position of the call site.
When it is FALSE, such statements will have their original source positions,
i.e., their positions in the source code of the called inline function.
When an inline call is expanded at other than the top level of a
statement, this switch has no effect (as the expansion is done
completely at the expression level, and therefore there are no inserted
statements whose source position would need to be set).
*/
#ifndef STATEMENTS_INSERTED_FOR_INLINING_HAVE_INVOCATION_POSITION
#define STATEMENTS_INSERTED_FOR_INLINING_HAVE_INVOCATION_POSITION TRUE
#endif /* STATEMENTS_INSERTED_FOR_INLINING_HAVE_INVOCATION_POSITION */

/*
This configuration macro sets the maximum number of statements that a
routine can have and still be inlinable.  The setting is somewhat arbitrary,
but a limit is needed to prevent iterative inlining from exhausting memory.
A setting of zero disables this check.
*/
#ifndef DEFAULT_INLINE_STATEMENT_LIMIT
#define DEFAULT_INLINE_STATEMENT_LIMIT 100
#endif /* ifndef DEFAULT_INLINE_STATEMENT_LIMIT */
#endif /* MINIMAL_INLINING */

#ifdef UNARY_PLUS_IN_IL
/* Up until version 4.2, UNARY_PLUS_IN_IL could be set to FALSE to
   suppress generation of unary "+" operators (eok_unary_plus) in the
   IL.  The option has been eliminated, so back ends must now always
   be prepared to handle eok_unary_plus. */
#if !UNARY_PLUS_IN_IL
 #error -- the UNARY_PLUS_IN_IL macro has been eliminated
#endif /* !UNARY_PLUS_IN_IL */
#endif /* ifdef UNARY_PLUS_IN_IL */

/*
This switch controls whether the eok_parens operator is put out for
parentheses.  If the switch is FALSE, parentheses do not appear in the IL.
*/
#ifndef PARENS_IN_IL
#define PARENS_IN_IL FALSE
#endif /* ifndef PARENS_IN_IL */
#if PARENS_IN_IL && DO_IL_LOWERING
 #error -- PARENS_IN_IL cannot be set when IL lowering is done
#endif /* PARENS_IN_IL && DO_IL_LOWERING */

/*
VLA_DEALLOC_STATEMENTS_IN_IL was previously used to determine whether special
statements (no longer part of our IL specification) should be included in the
IL.  Now we generate enk_vla_dealloc expression nodes instead.  In some cases
we can fairly safely set VLA_DEALLOCATIONS_IN_IL based on the older macro
value, but in other cases we prefer to avoid surprises by forcing the old
configuration option to be revised.
*/
#if defined(VLA_DEALLOC_STATEMENTS_IN_IL)
#if defined(VLA_DEALLOCATIONS_IN_IL)
#define TRIGGER_ERROR_ABOUT_VLA_DEALLOC_STATEMENTS_IN_IL
#else /* !defined(VLA_DEALLOCATIONS_IN_IL) */
#if !VLA_DEALLOC_STATEMENTS_IN_IL
#define VLA_DEALLOCATIONS_IN_IL FALSE
#else /* VLA_DEALLOC_STATEMENTS_IN_IL */
#if defined(LOWER_VARIABLE_LENGTH_ARRAYS)
#define VLA_DEALLOCATIONS_IN_IL VLA_DEALLOC_STATEMENTS_IN_IL
#else /* !defined(LOWER_VARIABLE_LENGTH_ARRAYS) */
#define TRIGGER_ERROR_ABOUT_VLA_DEALLOC_STATEMENTS_IN_IL
#endif /* defined(LOWER_VARIABLE_LENGTH_ARRAYS) */
#endif /* !VLA_DEALLOC_STATEMENTS_IN_IL */
#endif /* defined(VLA_DEALLOCATIONS_IN_IL) */
#endif /* defined(VLA_DEALLOC_STATEMENTS_IN_IL) */

#ifdef TRIGGER_ERROR_ABOUT_VLA_DEALLOC_STATEMENTS_IN_IL
 #error -- VLA_DEALLOC_STATEMENTS_IN_IL is no longer supported; use \
           VLA_DEALLOCATIONS_IN_IL instead
#endif /* TRIGGER_ERROR_ABOUT_VLA_DEALLOC_STATEMENTS_IN_IL */

/*
Flag that is TRUE if, when C mode VLA support is enabled, the front end should
generate enk_vla_dealloc expression nodes to mark the points at which variable
length arrays go out of scope and may be deallocated.  It is used to set global
variable vla_deallocations_in_il.  If the front end is only used in C++ mode,
the flag can be set to FALSE to eliminate the unused code altogether.
*/
#ifndef VLA_DEALLOCATIONS_IN_IL
#if VLA_ALLOWED && !BACK_END_IS_CP_GEN_BE
#define VLA_DEALLOCATIONS_IN_IL TRUE
#else /* !(VLA_ALLOWED && !BACK_END_IS_CP_GEN_BE) */
#define VLA_DEALLOCATIONS_IN_IL FALSE
#endif /* VLA_ALLOWED && !BACK_END_IS_CP_GEN_BE */
#endif /* ifndef VLA_DEALLOCATIONS_IN_IL */
#if VLA_DEALLOCATIONS_IN_IL && !VLA_ALLOWED
  #error -- VLA_DEALLOCATIONS_IN_IL cannot be true unless \
            VLA_ALLOWED is true
#endif /* VLA_DEALLOCATIONS_IN_IL && !VLA_ALLOWED */
#if VLA_DEALLOCATIONS_IN_IL && BACK_END_IS_CP_GEN_BE
  #error -- VLA_DEALLOCATIONS_IN_IL cannot be true when \
            BACK_END_IS_CP_GEN_BE is true
#endif /* VLA_DEALLOCATIONS_IN_IL && BACK_END_IS_CP_GEN_BE */
#if LOWER_VARIABLE_LENGTH_ARRAYS && !VLA_DEALLOCATIONS_IN_IL
 #error -- Lowering of VLAs requires VLA_DEALLOCATIONS_IN_IL to be TRUE
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS && !VLA_DEALLOCATIONS_IN_IL */

/*
Flag that is TRUE if variable-length arrays (VLAs) need to be deallocated
at the ends of their lifetimes.  FALSE if deallocation is not needed,
e.g., because it happens automatically.  If the VLA scheme does allocation
by extending the stack frame, for example, deallocation may not be required.
This flag controls IL lowering, specifically generation of deallocation
code and whether special exception cleanup entries are added to request
deallocation of VLAs on exceptions.

If LOWER_VARIABLE_LENGTH_ARRAYS is FALSE, the usual deallocation will
remain as an enk_vla_dealloc node, but the deallocation on exception
cleanup is still done (as in the configuration with VLA lowering) via
a call of the runtime routine __vla_dealloc_eh, and that call receives
only a pointer to the allocated space (but no size).  If some other
implementation is required, IL lowering would have to be changed.
(Changing the name of the routine is of course one of the easy
possible changes.)
*/
#ifndef VLA_DEALLOCATION_REQUIRED
#if !DO_IL_LOWERING
/* No lowering, so no chance to insert deallocation code. */
#define VLA_DEALLOCATION_REQUIRED FALSE
#else /* DO_IL_LOWERING */
#if LOWER_VARIABLE_LENGTH_ARRAYS
#define VLA_DEALLOCATION_REQUIRED TRUE
#else /* !LOWER_VARIABLE_LENGTH_ARRAYS */
#define VLA_DEALLOCATION_REQUIRED FALSE
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS */
#endif /* DO_IL_LOWERING */
#endif /* ifndef VLA_DEALLOCATION_REQUIRED */

#if LOWER_VARIABLE_LENGTH_ARRAYS && !VLA_DEALLOCATION_REQUIRED
 #error -- Lowering of VLAs requires VLA_DEALLOCATION_REQUIRED
#endif /* LOWER_VARIABLE_LENGTH_ARRAYS && !VLA_DEALLOCATION_REQUIRED */

/*
Flag that is TRUE if, when lowering IL in general but not variable-length
arrays (VLAs), no temporaries should be introduced to hold the dimensions
of VLAs appearing in function parameters (a possibility in some C modes).

In the C-generating back end, the temporaries are useful to avoid duplicating
side effects of VLA dimensions when a VLA type is reused in compiler-generated
casts.  However, function prototype scopes do not permit the declaration of a
temporary variable.  So this flag should always be TRUE when using the
C-generating back end.
*/
#if DO_IL_LOWERING && !LOWER_VARIABLE_LENGTH_ARRAYS
#ifndef NO_VLA_DIMENSION_TEMPORARIES_IN_FUNCTION_PROTOTYPES
#define NO_VLA_DIMENSION_TEMPORARIES_IN_FUNCTION_PROTOTYPES \
                                                         BACK_END_IS_C_GEN_BE
#endif /* NO_VLA_DIMENSION_TEMPORARIES_IN_FUNCTION_PROTOTYPES */

#if BACK_END_IS_C_GEN_BE && \
    !NO_VLA_DIMENSION_TEMPORARIES_IN_FUNCTION_PROTOTYPES
 #error -- The C-generating back end requires \
           NO_VLA_DIMENSION_TEMPORARIES_IN_FUNCTION_PROTOTYPES set to TRUE
#endif /* BACK_END_IS_C_GEN_BE && !NO_VLA_DIMENSION_TEMPORARIES... */
#endif /* DO_IL_LOWERING && !LOWER_VARIABLE_LENGTH_ARRAYS */

/*
Flag that is used as the default setting for global variable vla_enabled.
The variable can also be controlled from the command line by --[no_]vla.
Whatever the default, vla_enabled is forced on or off in certain modes
(e.g., on in GNU C mode, off in default C++ mode), so the default here
applies only in other modes, e.g., default C mode.
*/
#ifndef DEFAULT_VLA_ENABLED
#define DEFAULT_VLA_ENABLED FALSE
#endif /* ifndef DEFAULT_VLA_ENABLED */
#if DEFAULT_VLA_ENABLED && !VLA_ALLOWED
  #error -- DEFAULT_VLA_ENABLED cannot be true unless VLA_ALLOWED is true
#endif /* DEFAULT_VLA_ENABLED && !VLA_ALLOWED */

/*
This switch controls whether or not types and static variables that
are local to function and block scopes are moved onto the file scope
lists.  When the switch is FALSE, no promotions are done.  Local
types and variables are allocated in the file scope memory region,
and they are linked on the local scope types or variables list.  That
accurately reflects the source form, which is desirable for
generating symbolic debug information.  That form probably works fine
when the IL is being fed into a true back end, but will not work when
the IL is being turned into C output (as with the C-generating back
end), because the local types and variables will not be visible from
member functions of local classes and in a file-scope termination
routine when it deals with calling a destructor for a local static
variable.  When the switch here is TRUE, the local types and
variables will be (selectively) promoted to the actual file scope.
*/
#ifndef PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
#define PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE BACK_END_IS_C_GEN_BE
#endif /* ifndef PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE */
#if ENSURE_LOWERED_TYPE_LIST_ORDERING && !PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
 #error -- ENSURE_LOWERED_TYPE_LIST_ORDERING requires \
           PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE
#endif /* ENSURE_LOWERED_TYPE_LIST_ORDERING && !PROMOTE_LOCAL_ENTITIES_... */
#if PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE && !DO_IL_LOWERING
 #error -- PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE requires DO_IL_LOWERING
#endif /* PROMOTE_LOCAL_ENTITIES_TO_FILE_SCOPE && !DO_IL_LOWERING */

/*
In most configurations (when SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
is FALSE) lowering generates a single initialization routine per translation
unit that initializes all file-scope variables that require dynamic
initialization in the translation unit.  When one-instantiation-per-object mode
is used, there can be multiple initialization routines (up to one per "needed"
bit).  When the GNU init_priority attribute is used, each priority that is used
during file-scope initialization has its own routine.
When SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS is TRUE, each dynamic
initialization is given its own initialization routine and these file-scope
dynamic initialization routines are queued (in the order required for proper
initialization) on a list pointed to by
il_header.file_scope_dynamic_init_routines.  In such a configuration, it is
the responsibility of the back end to ensure that each of these routines is
called -- in order -- before execution of the program.  In cases where GNU
init_priority and one-instantiation-per-object are used, the routines are
ordered by "needed" bit number and/or init_priority.  This configuration may be
useful when a back end has somehow determined that a file-scope variable or
static data member is otherwise unused and can be removed from the translation
unit (in which case the routine that initializes it can simply be removed from
this list).

When USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES is TRUE, file-scope
thread_local dynamic initializations are consolidated in a single __tls_init
initialization routine (regardless of the setting of
SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS).  When
USE_LAZY_INITIALIZATION_FOR_THREAD_LOCAL_VARIABLES is FALSE and
SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS is TRUE, separate routines
are created for each thread_local initialization and these routines
are pointed to by il_header.thread_local_dynamic_init_routines (in the
order they should be executed).
*/
#ifndef SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
#define SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS FALSE
#endif /* ifndef SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS */
#if SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS && !DO_IL_LOWERING
 #error -- SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS requires \
           DO_IL_LOWERING
#endif /* SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS && !DO_IL_LOWERING */

/*
When this switch is TRUE, the front end assumes that references will never
have NULL values.  That's required by the C++ standard, but some
implementations allow use of null references.  This is the initial value
of the variable assume_references_cannot_be_null.  The variable gets set
to TRUE in some modes, e.g., strict mode.
*/
#ifndef ASSUME_REFERENCES_CANNOT_BE_NULL
#define ASSUME_REFERENCES_CANNOT_BE_NULL FALSE
#endif /* ASSUME_REFERENCES_CANNOT_BE_NULL */

#if DO_IL_LOWERING

/* Switches that control aspects of IL lowering: */

/*
If TRUE, IL lowering should generate the code that the Cfront
"patch" program needs for startup initialization.  This involves a
generated variable called "__link" that points to the startup
initialization routine.  This is a mostly-obsolete technique.
*/
#ifndef USE_PATCH_INIT_STARTUP
#define USE_PATCH_INIT_STARTUP FALSE
#endif /* ifndef USE_PATCH_INIT_STARTUP */

#if USE_PATCH_INIT_STARTUP && USE_INIT_SECTION_IN_GENERATED_C
 #error -- USE_PATCH_INIT_STARTUP and USE_INIT_SECTION_IN_GENERATED_C \
           cannot both be specified
#endif /* USE_PATCH_INIT_STARTUP && USE_INIT_SECTION_IN_GENERATED_C */

#if USE_PATCH_INIT_STARTUP && SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS
 #error -- SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYNAMIC_INITS and \
           USE_PATCH_INIT_STARTUP cannot both be specified
#endif /* USE_PATCH_INIT_STARTUP && SEPARATE_ROUTINES_FOR_FILE_SCOPE_DYN... */

/*
This switch controls whether zeroing is added to variable definitions
to force them to be definitions in C.  This is generally a good thing,
but it may be wasteful for embedded system cross-compilers.
*/
#ifndef FORCE_VARIABLE_DEFINITION_VIA_ZEROING
#define FORCE_VARIABLE_DEFINITION_VIA_ZEROING TRUE
#endif /* ifndef FORCE_VARIABLE_DEFINITION_VIA_ZEROING */

/*
This switch controls whether or not all functions and function calls will
be turned into old-style unprototyped form.  This is what cfront effectively
does, in generating old-style C that is compiled by a C compiler.
This change is important if one wants to be able to call libraries that
were compiled by cfront.  (cfront's +a1 option requests generation of ANSI C
code; if one wants compatibility with cfront in that mode, this option
should be set to FALSE.)  This is the initial value of the variable
make_all_functions_unprototyped.  There is no command-line option to
change that variable, but having a variable makes it possible to have one.
*/
#ifndef MAKE_ALL_FUNCTIONS_UNPROTOTYPED
#define MAKE_ALL_FUNCTIONS_UNPROTOTYPED CFRONT_OBJECT_CODE_COMPATIBILITY
#endif /* ifndef MAKE_ALL_FUNCTIONS_UNPROTOTYPED */

/*
When this switch is TRUE, lowering (and inlining) can assume that "this" can
never be NULL in conditional operators (i.e., "?", "&&", "||"), allowing
expressions such as "(this ? 1 : 0)" to be optimized to remove dead code.  Even
when this switch is FALSE, the assumption is made that "this" cannot be NULL
in the context of a virtual member function.  This is the initial value of the
variable assume_this_cannot_be_null_in_conditional_operators.  Note that the
assumption that "this" cannot be NULL is made in other places in lowering
(irrespective of the value of this variable), resulting in the removal of a
NULL pointer check on related class casts as well as optimizations in
constructors in the Cfront ABI when NEW_CAN_BE_FOLDED_INTO_CTOR is TRUE.
Invoking a member function through a NULL pointer is undefined behavior, but
GNU and Microsoft compilers allow it, so the
assume_this_cannot_be_null_in_conditional_operators variable is set to FALSE at
run-time in GNU and Microsoft modes.
*/
#ifndef ASSUME_THIS_CANNOT_BE_NULL_IN_CONDITIONAL_OPERATORS
#define ASSUME_THIS_CANNOT_BE_NULL_IN_CONDITIONAL_OPERATORS DO_IL_LOWERING
#endif /* ifndef ASSUME_THIS_CANNOT_BE_NULL_IN_CONDITIONAL_OPERATORS */

/*
When this switch is TRUE, exception handling features will be completely
lowered to C form, in a portable way.  All exception-handling statements and
expressions are completely lowered, code is generated to maintain an
exception handling stack, and cleanup tables and typeinfo entries are
generated.  Note that the C-generating back end requires this mode.
*/
#ifndef DO_FULL_PORTABLE_EH_LOWERING
#define DO_FULL_PORTABLE_EH_LOWERING TRUE
#endif /* ifndef DO_FULL_PORTABLE_EH_LOWERING */

/*
When this switch is TRUE, cleanup tables and typeinfo entries will be generated
for exception-handling constructs.
*/
#if DO_FULL_PORTABLE_EH_LOWERING
#undef GENERATE_EH_TABLES
#define GENERATE_EH_TABLES TRUE  /* Do not change this. */
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
#ifndef GENERATE_EH_TABLES
#define GENERATE_EH_TABLES FALSE
#endif /* ifndef GENERATE_EH_TABLES */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */

/*
When this switch is TRUE, the object lifetime information in the IL
will be left around by IL lowering, but only when exceptions are
enabled.
*/
#ifndef KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED
#if !GENERATE_EH_TABLES
#define KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED TRUE
#else /* GENERATE_EH_TABLES */
#define KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED FALSE
#endif /* !GENERATE_EH_TABLES */
#endif /* ifndef KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED */
#if !GENERATE_EH_TABLES
#if !KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED
 #error -- KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED must be \
           TRUE when GENERATE_EH_TABLES is FALSE
#endif /* !KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED */
#endif /* !GENERATE_EH_TABLES */

/*
When this switch is TRUE, instructions to set the cleanup state will
be emitted even at unreachable ends of blocks.  This may be desirable
if the back end is using the cleanup state instructions to build a
table, rather than leaving them as some kind of executable code.
*/
#ifndef INDICATE_CLEANUP_STATE_IN_UNREACHABLE_CODE
#if DO_FULL_PORTABLE_EH_LOWERING
#define INDICATE_CLEANUP_STATE_IN_UNREACHABLE_CODE FALSE
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
#define INDICATE_CLEANUP_STATE_IN_UNREACHABLE_CODE TRUE
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
#endif /* ifndef INDICATE_CLEANUP_STATE_IN_UNREACHABLE_CODE */

/*
This switch, which affects the portable implementation of exception
handling, controls generation of extra code and extra conditional
flags to ensure that all appropriate expression temporaries are destroyed
on the occurrence of an exception in an expression that includes unordered
construction of temporaries.  "Unordered" temporaries are constructed
in parts of an expression that are, by C/C++ language rules, unordered
with respect to one another.  For example, in "A(1) + A(2)" the temporary
for A(1) could be constructed before or after the temporary for A(2).
The problem for a portable implementation of exception handling is that
is it necessary when indicating the cleanup to be done at the point of
construction of A(1) or A(2) to indicate that the other temporary might
or might not already have been constructed.  This is done by adding a
conditional flag for each temporary that indicates whether or not the
construction has been done, and having the cleanup position in the
cleanup region table include destructions for both temporaries.  The
runtime then tests the conditional flags to indicate whether the
destructions should actually be done.  This method, of course, involves
a time and space overhead, so the present flag is provided as a way to
switch it off.  It can be switched off if the back end will do the
constructions in the canonical order indicated by the IL, or if the
next_in_destruction_list linkage of the initializations is changed before
IL lowering to match the order that will actually be used by the back end,
or if the back end will use some different technique to accomplish the
same result.  Note that when using the C-generating back end, the
order in which temporaries are constructed depends on the C compiler
that will be used, so if one has detailed information about the order
in which it will evaluate expressions, one may be able to switch off
this processing.
*/
#ifndef DO_UNORDERED_EH_PROCESSING
#if GENERATE_EH_TABLES
#define DO_UNORDERED_EH_PROCESSING TRUE
#else /* !GENERATE_EH_TABLES */
#define DO_UNORDERED_EH_PROCESSING FALSE
#endif /* GENERATE_EH_TABLES */
#endif /* ifndef DO_UNORDERED_EH_PROCESSING */

/*
This switch controls generation of code at the end of fully-lowered
try blocks to fool C compilers into suppressing certain harmful optimizations.
Specifically, the code generated is an unreachable call of a runtime
routine, passing the addresses of all local variables modified within
the try block, to force the C compiler to store those immediately
when they are modified.
*/
#if DO_FULL_PORTABLE_EH_LOWERING
#ifndef FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS
#define FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS BACK_END_IS_C_GEN_BE
#endif /* ifndef FORCE_STORES_OF_VARS_MODIFIED_IN_TRY_BLOCKS */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */

/*
This switch can be set to enable the rewriting of the escape character
in universal character names (UCNs), i.e., "\", where it appears in
identifier names, to a single other character.  Note that this is not
a complete solution if the character to which the escape is changed
is a character that is valid in identifiers, because it would then
be possible (if unlikely) that a user might write an identifier that
would match an identifier with a rewritten UCN escape character (for
example, with a rewrite to "_", "x\u00d6" would be rewritten as
"x_u00d6").  A better choice is a character accepted by the linker
but not valid as an identifier character in C.  If there is no
such character, a character like "_" will provide a "good enough"
implementation.  Note also that when non-Unicode multibyte characters
are allowed this rewrite also applies to the "\m00d6" kind of encoding
used for them (see IDENTIFIER_STRINGS_ALLOW_MULTIBYTE_CHARS).
*/
#ifndef REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING
#define REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING TRUE
#ifndef UCN_ESCAPE_REWRITE_CHAR
#define UCN_ESCAPE_REWRITE_CHAR '_' /* Incomplete solution, see above. */
#endif /* ifndef UCN_ESCAPE_REWRITE_CHAR */
#endif /* ifndef REWRITE_UCN_ESCAPE_CHAR_IN_LOWERING */

/*
Flag that is TRUE if IL lowering can generate an optimized code sequence
for certain pointer to member calls for classes that have no virtual
functions.  The C++ standard disallows this optimization, but some
older compilers have done it.  See the WG21 paper N0644 for a description
of the disallowed optimization.
*/
#ifndef DEFAULT_POINTER_TO_MEMBER_CALL_OPTIMIZATION_ALLOWED
#define DEFAULT_POINTER_TO_MEMBER_CALL_OPTIMIZATION_ALLOWED FALSE
#endif /* ifndef DEFAULT_POINTER_TO_MEMBER_CALL_OPTIMIZATION_ALLOWED */

/*
Return value optimization is possible when all return statements in a function
return the same nonstatic local variable; the optimization is to rewrite all
references to the local variable as references to the return-value address
passed by the caller, thus avoiding a copy constructor call on exit.  
Detecting cases where the return value optimization is possible is performed
by the front end in all cases.  This macro controls whether the return
value optimization is performed during lowering.
*/
#ifndef DO_RETURN_VALUE_OPTIMIZATION_IN_LOWERING
#define DO_RETURN_VALUE_OPTIMIZATION_IN_LOWERING TRUE
#endif /* ifndef DO_RETURN_VALUE_OPTIMIZATION_IN_LOWERING */

/*
Integer kind to use for an offset into a class.  This is used for delta
fields in pointers to member functions, etc., but not for pointers to
data members.  If you change this, you will need to change
TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION, and possibly the related alignment
macro as well.
*/
#ifndef TARG_DELTA_INT_KIND
#if IA64_ABI
#define TARG_DELTA_INT_KIND TARG_PTRDIFF_T_INT_KIND
#else /* !IA64_ABI */
#define TARG_DELTA_INT_KIND ((an_integer_kind)ik_short)
#endif /* IA64_ABI */
#endif /* ifndef TARG_DELTA_INT_KIND */

/*
Integer kind to use for an index into a virtual function table.  Must be
no smaller than the size of a_virtual_function_number.  If you change
this, you will need to change TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION, and
possibly the related alignment macro as well.
*/
#ifndef TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND ((an_integer_kind)ik_short)
#endif /* ifndef TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND */

/*
This switch controls whether or not operations with
returns_lvalue_instead_of_usual_rvalue TRUE are rewritten by IL lowering.
These are operations (specifically, assignments, prefix ++/--, and
the "?" and "," operators) that return rvalues in C but can return lvalues
in C++.  When this switch is TRUE, the non-C cases are transformed into
valid C, which usually requires some duplication of parts of the expression
tree.
*/
#ifndef LOWER_LVALUE_RETURNING_OPERATIONS
#define LOWER_LVALUE_RETURNING_OPERATIONS TRUE
#endif /* !defined(LOWER_LVALUE_RETURNING_OPERATIONS) */

#if MICROSOFT_EXTENSIONS_ALLOWED
/*
Microsoft mode allows a nonconstant aggregate initializer in C mode.
This switch controls whether such an aggregate is lowered to normal C.
That is done by invoking some subroutines from IL lowering, not the
whole process.
*/
#ifndef LOWER_MICROSOFT_NONCONSTANT_AGGREGATE
#define LOWER_MICROSOFT_NONCONSTANT_AGGREGATE TRUE
#endif /* ifndef LOWER_MICROSOFT_NONCONSTANT_AGGREGATE */
#endif /* MICROSOFT_EXTENSIONS_ALLOWED */

/*
This switch controls whether designated initializers (a C99 feature)
are lowered to standard C.  Well, almost standard C: a designated
initializer allows initialization of a member other than the first in
a union.  For that case, a ck_designator constant is left in the IL tree
(but only one, and only for members other than the first).  Also,
for extended designators of the form "[a ... b] = x", or when elements
of arrays are skipped in initialization, ck_init_repeat constants
are used to repeat an initializer constant the appropriate number
of times.
*/
#ifndef LOWER_DESIGNATED_INITIALIZERS
#define LOWER_DESIGNATED_INITIALIZERS TRUE
#endif /* ifndef LOWER_DESIGNATED_INITIALIZERS */

/*
This switch controls whether or not "guard" code is placed around
initializations of static data members of templates.  Such guard code is
necessary if template instantiation resolution is done by instantiating
everything and then having the (specially-modified) linker discard
duplicate copies of instantiated routines.  Static data members are
a particular problem: because the initialization code is generated
in startup routines, and is undifferentiated from other code in
those routines, a flag is needed to indicate that initialization
has already been done.  After any one instance of the code does
initialization, all other instances will do nothing.
*/
#ifndef TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE
#if LINKER_CAN_DISCARD_DUPLICATE_DEFINITIONS
#define TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE TRUE
#else /* !LINKER_CAN_DISCARD_DUPLICATE_DEFINITIONS */
#define TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE FALSE
#endif /* LINKER_CAN_DISCARD_DUPLICATE_DEFINITIONS */
#endif /* !defined(TEMPLATE_STATIC_DATA_MEMBER_INIT_GUARD_CODE) */

#if DO_FULL_PORTABLE_EH_LOWERING
/*
jmp_buf is a type defined by <setjmp.h> for use in setjmp/longjmp.  IL lowering
uses setjmp/longjmp for the portable implementation of exception try/throw
statements.

jmp_buf is defined to be an array type.  The front end only directly supports
array types of some kind of integral or floating type.  In the case of a struct
type being used, it's generally sufficient to find an integral type with a
matching alignment, and then create an array of at least equivalent size (in
effect creating an aligned buffer for setjmp to write into and longjmp to read
from).  Notably, this approximation may lead to one or more warnings from the
target compiler consuming the C-generating back end code; however, assuming
correct alignment and sizing, these warnings are spurious.

The definitions here specify the number of elements in the array type and the
integral or floating kind for the array element type.
*/
#ifndef TARG_JMP_BUF_NUM_ELEMENTS
#define TARG_JMP_BUF_NUM_ELEMENTS 9  /* For SPARC, SunOS 4.1.2. */
#endif /* !defined(TARG_JMP_BUF_NUM_ELEMENTS) */
			/* Default value, used to initialize global variable
			   targ_jmp_buf_num_elements. */
#ifndef TARG_JMP_BUF_ELEMENTS_ARE_FLOAT
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT FALSE
#endif /* ifndef TARG_JMP_BUF_ELEMENTS_ARE_FLOAT */
			/* Choose between integer and float. */
#ifndef TARG_JMP_BUF_ELEMENT_INT_KIND
#define TARG_JMP_BUF_ELEMENT_INT_KIND ((an_integer_kind)ik_int)
#endif /* !defined(TARG_JMP_BUF_ELEMENT_INT_KIND) */
			/* Default value, used to initialize global variable
			   targ_jmp_buf_element_int_kind. */
#ifndef TARG_JMP_BUF_ELEMENT_FLOAT_KIND
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND ((a_float_kind)fk_long_double)
#endif /* !defined(TARG_JMP_BUF_ELEMENT_FLOAT_KIND) */
			/* Default value, used to initialize global variable
			   targ_jmp_buf_element_float_kind. */
/*
On some targets, the setjmp function is declared as a macro to another
function.  The macro that follows provides a means to specify the correct
setjmp function implementation.

A common example of where this occurs is in glibc (i.e., Linux).  setjmp starts
out declared as a function, but is later declared as a macro to (the more
efficient) _setjmp.  As setjmp.h is not required to be included for
DO_FULL_PORTABLE_EH_LOWERING, the function to be used must be configured ahead
of time.
*/
#ifndef TARG_SETJMP_FUNC
#define TARG_SETJMP_FUNC "setjmp"
#endif /* !defined(TARG_SETJMP_FUNC) */
			/* Default value, used to initialize global variable
			   targ_setjmp_func. */
#endif /* DO_FULL_PORTABLE_EH_LOWERING */

#if GENERATE_EH_TABLES
/*
The integral kind to be used for a cleanup region number with exception
processing.
*/
#ifndef TARG_REGION_NUMBER_INT_KIND
#define TARG_REGION_NUMBER_INT_KIND ((an_integer_kind)ik_unsigned_short)
#endif /* ifndef TARG_REGION_NUMBER_INT_KIND */

/*
The integral kind to be used for passing flags related to exception handling
to the run time library.  Versions prior to 4.14 used ik_unsigned_char, but
that type doesn't have room for any more bits so a larger type is warranted.
*/
#ifndef TARG_ETS_FLAG_TYPE_INT_KIND
#if ABI_COMPATIBILITY_VERSION < 414
#define TARG_ETS_FLAG_TYPE_INT_KIND ((an_integer_kind)ik_unsigned_char)
#else /* ABI_COMPATIBILITY_VERSION >= 414 */
#define TARG_ETS_FLAG_TYPE_INT_KIND ((an_integer_kind)ik_unsigned_int)
#endif /* ABI_COMPATIBILITY_VERSION < 414 */
#endif /* ifndef TARG_ETS_FLAG_TYPE_INT_KIND */

/*
The integral kind to be used for a local variable identifier in exception
processing.  In the portable scheme, this is an index into the object
address table.  In the partial-lowering scheme, it is an offset in the
stack.
*/
#ifndef TARG_VAR_HANDLE_INT_KIND
#if DO_FULL_PORTABLE_EH_LOWERING
#define TARG_VAR_HANDLE_INT_KIND ((an_integer_kind)ik_unsigned_short)
#else /* !DO_FULL_PORTABLE_EH_LOWERING */
#define TARG_VAR_HANDLE_INT_KIND TARG_SIZE_T_INT_KIND
#endif /* DO_FULL_PORTABLE_EH_LOWERING */
#endif /* ifndef TARG_VAR_HANDLE_INT_KIND */
#endif /* GENERATE_EH_TABLES */

#else /* !DO_IL_LOWERING */
#undef GENERATE_EH_TABLES
#define GENERATE_EH_TABLES FALSE
#endif /* DO_IL_LOWERING */

/*
This switch controls whether constructions and destructions that are known to
have no effect are removed from the IL during lowering.  Removing unneeded
constructions and destructions can greatly reduce the code size of some trivial
functions, particularly when the portable implementation of exception handling
is used (where the invocation of any destruction causes an exception handling
prologue to be generated for the function).  The implementation doesn't detect
all cases where such destructions could potentially be eliminated (for example,
a constructor or destructor that is defined to be empty after code that refers
to it has already been lowered).  Constructors and destructors are determined
to have no effect if their lowered function bodies have no statements that
are deemed to have an effect (with the exception of certain "boilerplate"
statements that exist in all constructors and destructors).
*/

#ifndef LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS
#define LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS \
        DO_IL_LOWERING
#endif /* ifndef LOWERING_REMOVES_UNNEEDED_CONSTRUCTIONS_AND_DESTRUCTIONS */

/*
Integer kind used for the size of a vtable entry in the IA-64 ABI.  Vtable
entries in the IA-64 ABI must be large enough to accommodate the three types
of entities stored there: offsets (ptrdiff_t), data pointers (to type_info),
and function pointers (to virtual functions).  In almost all cases, the
sizes of these items are identical and the same integer kind as is used
to represent ptrdiff_t is appropriate.  The integer kind used here must
be signed (to accommodate negative offsets).  The vtable is represented as an
array of entities of this size.  Strictly speaking, the IA-64 ABI models the
vtable as a sequence of these entries, suggesting that a structure of
heterogeneous elements would be a better model than the array of homogeneous
elements that the front end uses.  In architectures where the types of vtable
entries are not the same size, this macro can be used to specify an integer
kind that can encompass all of the vtable entry types, allowing the array model
to be used (at the expense of larger than necessary vtables).
The integer kind specified here must be the same size as a pointer; lowering
(and the run-time library) make this assumption when de-referencing vtable
entries that contain pointers (i.e., type_info and virtual functions).
In the case where TARG_IA64_VTABLE_ENTRY_INT_KIND specifies an integer kind
that is larger than TARG_DELTA_INT_KIND, the integer offset values are
checked against TARG_DELTA_INT_KIND (i.e., for overflow), but cast to the
larger integer size when initialized in the vtable.  References to these
offsets (both in lowering and in the run-time) use the larger integer kind
(i.e., TARG_IA64_VTABLE_ENTRY_INT_KIND) when retrieving the offset values
at run-time.
*/
#ifndef TARG_IA64_VTABLE_ENTRY_INT_KIND
#if IA64_ABI
#ifdef TARG_DELTA_INT_KIND
#define TARG_IA64_VTABLE_ENTRY_INT_KIND TARG_DELTA_INT_KIND
#else /* !defined TARG_DELTA_INT_KIND */
#define TARG_IA64_VTABLE_ENTRY_INT_KIND TARG_PTRDIFF_T_INT_KIND
#endif /* defined TARG_DELTA_INT_KIND */
#endif /* IA64_ABI */
#endif /* ifndef TARG_IA64_VTABLE_ENTRY_INT_KIND */

/*
When the following switch is TRUE, modes that make exception specifications
part of the function type can be enabled.  This is a requirement for C++17, but
it affects the ABI (it affects name mangling as well as RTTI representation).
*/
#ifndef EXC_SPEC_IN_FUNC_TYPE_ENABLING_POSSIBLE
#define EXC_SPEC_IN_FUNC_TYPE_ENABLING_POSSIBLE TRUE
#endif /* ifndef EXC_SPEC_IN_FUNC_TYPE_ENABLING_POSSIBLE */

/*
Determine whether RTTI can be enabled.  It cannot be if we are doing IL
lowering and the ABI changes for RTTI aren't enabled.
*/
#ifndef RTTI_ENABLING_POSSIBLE
#if DO_IL_LOWERING
#if ABI_CHANGES_FOR_RTTI
#define RTTI_ENABLING_POSSIBLE TRUE
#else /* !ABI_CHANGES_FOR_RTTI */
#define RTTI_ENABLING_POSSIBLE FALSE
#endif /* ABI_CHANGES_FOR_RTTI */
#else /* !DO_IL_LOWERING */
#define RTTI_ENABLING_POSSIBLE TRUE
#endif /* DO_IL_LOWERING */
#else /* ifdef RTTI_ENABLING_POSSIBLE */
#if RTTI_ENABLING_POSSIBLE && DO_IL_LOWERING
#if !ABI_CHANGES_FOR_RTTI
  #error -- ABI_CHANGES_FOR_RTTI must be TRUE when RTTI_ENABLING_POSSIBLE \
            is TRUE
#endif /* !ABI_CHANGES_FOR_RTTI */
#endif /* RTTI_ENABLING_POSSIBLE && DO_IL_LOWERING */
#endif /* ifndef RTTI_ENABLING_POSSIBLE */

/*
Determine whether array new and delete can be enabled.  They cannot be if
we are doing IL lowering and the ABI changes for array new and delete
aren't enabled.
*/
#ifndef ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE
#if DO_IL_LOWERING
#if ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE
#define ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE TRUE
#else /* !ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE */
#define ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE FALSE
#endif /* ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE */
#else /* !DO_IL_LOWERING */
#define ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE TRUE
#endif /* DO_IL_LOWERING */
#else /* ifdef ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE */
#if ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE && DO_IL_LOWERING
#if !ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE
  #error -- ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE must be TRUE when \
            ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE is TRUE
#endif /* !ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE */
#endif /* ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE && DO_IL_LOWERING */
#endif /* ifndef ARRAY_NEW_AND_DELETE_ENABLING_POSSIBLE */

/*
Alignment required of pointers to malloc'd space (i.e., the maximum
alignment required by the host computer).  Use "1" if there are no
alignment requirements.  This must be defined as an actual constant
rather than as something like "sizeof(int)"; see mem_manage.h.
Note that space allocated by malloc must provide at least this
alignment, or the front end is powerless to provide the requested
alignment.

This is really a host configuration macro, and as such should be
in host_envir.h.  However, the most sensible default takes into
account whether LONG_LONG_ALLOWED is set, and that is only known
here.
*/
#ifndef HOST_ALIGNMENT_REQUIRED
#ifdef __alpha
/* Alpha always needs 8 byte alignment. */
#define HOST_ALIGNMENT_REQUIRED 8
#else /* ifndef __alpha */
#if defined(__i386) && !defined(__CYGWIN__)
/* Intel architecture only required 4 byte alignment, even with long long.
   The Windows convention is 8 byte alignment, however.  Windows compilers
   other than Cygwin do not seem to set __i386, so the test above
   essentially checks for i386 Unix compilers. */
#define HOST_ALIGNMENT_REQUIRED 4
#else /* !(defined(__i386) && !defined(__CYGWIN__)) */
/* Use 8 byte alignment if long long is supported and we are using a host
   integer to represent integer values. */
#if LONG_LONG_ALLOWED && INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
#define HOST_ALIGNMENT_REQUIRED 8
#else /* !(LONG_LONG_ALLOWED && INTEGER_VALUE_REPR_IS_A_HOST_INTEGER) */
#define HOST_ALIGNMENT_REQUIRED 4
#endif /* LONG_LONG_ALLOWED && INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */
#endif /* defined(__i386) && !defined(__CYGWIN__) */
#endif /* ifdef __alpha */
#endif /* ifndef HOST_ALIGNMENT_REQUIRED */

/*
The alignment required by host pointers.  This is used to determine the size
of the prefix allocated as part of each IL entry.  When checking code is
enabled, the value of this macro is checked when the front end is executed.
*/
#ifndef HOST_POINTER_ALIGNMENT
#if TARG_SUPPORTS_X86_64 || TARG_SUPPORTS_ARM64 || TARG_SUPPORTS_RISCV64
#define HOST_POINTER_ALIGNMENT 8
#else /* !(TARG_SUPPORTS_X86_64 || TARG_SUPPORTS_ARM64 || ...) */
#define HOST_POINTER_ALIGNMENT 4
#endif /* TARG_SUPPORTS_X86_64 || TARG_SUPPORTS_ARM64 || ... */
#endif /* ifndef HOST_POINTER_ALIGNMENT */

/*
Each IL entity is preceded in memory by an_il_entry_prefix structure that
contains information about the IL entry that follows.  In order to keep the
structure small, the entry_number field can be combined with a small number
of bit fields (four or five, depending on the configuration), thereby avoiding
wasteful padding.  When compiling some translation units (especially those with
many mangled names), the resulting size of the entry_number field may be too
small, resulting in an ec_program_too_large catastrophic error.  In order to
accommodate such programs, the following configuration macro can be set to
FALSE in which case the entry_number field is "full-sized" (i.e., all the bits
of TYPE_FOR_PREFIX_ENTRY_NUMBER are available to indicate an entry number).
*/
#ifndef ENTRY_NUMBER_SHARES_BITS_IN_PREFIX
#define ENTRY_NUMBER_SHARES_BITS_IN_PREFIX TRUE
#endif /* ifndef ENTRY_NUMBER_SHARES_BITS_IN_PREFIX */

/*
The host type for the entry_number bit-field of the an_il_entry_prefix
structure.  This field is typically an unsigned 32-bit type, but for some
complex cases (typically with many long, mangled names), an unsigned 64-bit
type may be needed.  This is only used when IL_SHOULD_BE_WRITTEN_TO_FILE and
ALTERNATE_IL_FILE_FORMAT are TRUE.  In cases where
ENTRY_NUMBER_SHARES_BITS_IN_PREFIX is TRUE (see above), not all bits of this
type may be available.
*/
#ifndef TYPE_FOR_PREFIX_ENTRY_NUMBER
#if EDG_MSDOS
/* Under MS-DOS compilers this bit field is probably bigger than an "int",
   so use "unsigned long". */
#define TYPE_FOR_PREFIX_ENTRY_NUMBER unsigned long
#else /* !EDG_MSDOS */
#define TYPE_FOR_PREFIX_ENTRY_NUMBER unsigned int
#endif /* EDG_MSDOS */
#endif /* ifndef TYPE_FOR_PREFIX_ENTRY_NUMBER */

typedef TYPE_FOR_PREFIX_ENTRY_NUMBER an_il_entry_number;

/*
The alignment required for the an_il_entry_prefix structure defined in
mem_tables.h.  This is used to determine the size of the prefix allocated
as part of each IL entry.  The IL entry prefix alignment must be a multiple
of HOST_POINTER_ALIGNMENT because things like the file scope orphan pointer
are stored immediately before the IL prefix and must be suitably aligned.
Because of this requirement, the default value based on HOST_POINTER_ALIGNMENT
is correct in most cases.  When checking code is enabled, the value of
this macro is checked when the front end is executed.  The check ensures
that the value is large enough.  The preprocessor test below makes sure
the value is a multiple of HOST_POINTER_ALIGNMENT.
*/
#ifndef HOST_IL_ENTRY_PREFIX_ALIGNMENT
#define HOST_IL_ENTRY_PREFIX_ALIGNMENT HOST_POINTER_ALIGNMENT
#endif /* ifndef HOST_IL_ENTRY_PREFIX_ALIGNMENT */

/*
The value of HOST_IL_ENTRY_PREFIX_ALIGNMENT must be a multiple of the value
of HOST_POINTER_ALIGNMENT to ensure the correct alignment of addresses
computed to access pointer "fields" preceding an IL prefix (file-scope
orphan pointers and/or translation unit copy addresses).
*/
#if (HOST_IL_ENTRY_PREFIX_ALIGNMENT % HOST_POINTER_ALIGNMENT) != 0
 #error -- HOST_IL_ENTRY_PREFIX_ALIGNMENT must be multiple of \
           HOST_POINTER_ALIGNMENT
#endif /* (HOST_IL_ENTRY_PREFIX_ALIGNMENT % HOST_POINTER_ALIGNMENT) != 0 */

/*
Named address spaces (an Embedded C extensions described in ISO/IEC TR 18037)
are identified using small integers of the following signed type (the value
-1 is used to indicate "no address space; not even the generic one which has
id zero").  Defined unconditionally because it is used in some function
signatures.
*/
typedef int a_named_address_space_id;

/*
Number of bits needed to represent named address space ids.  This value
must be at least 2 if INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES is TRUE, and
at least 1 otherwise.  If the value is not configured explicitly, these
minimum values are used by default.
*/
#if NAMED_ADDRESS_SPACES_ALLOWED
#ifndef NUM_BITS_FOR_NAMED_ADDRESS_SPACE
#ifndef INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES
#define NUM_BITS_FOR_NAMED_ADDRESS_SPACE 1
#else /* defined(INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES) */
#define NUM_BITS_FOR_NAMED_ADDRESS_SPACE 2
#endif /* ifndef INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES */
#else /* defined(NUM_BITS_FOR_NAMED_ADDRESS_SPACE) */
#ifndef INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES
#if NUM_BITS_FOR_NAMED_ADDRESS_SPACE < 1
 #error -- NUM_BITS_FOR_NAMED_ADDRESS_SPACE must be defined to a positive \
           value
#endif /* NUM_BITS_FOR_NAMED_ADDRESS_SPACE < 1 */
#else /* defined(INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES) */
#if NUM_BITS_FOR_NAMED_ADDRESS_SPACE < 2
 #error -- NUM_BITS_FOR_NAMED_ADDRESS_SPACE must be at least 2
#endif /* NUM_BITS_FOR_NAMED_ADDRESS_SPACE < 2 */
#endif /* ifndef INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES */
#endif /* ifndef NUM_BITS_FOR_NAMED_ADDRESS_SPACE */

/*
A structure describing a named address space.  The structure is primarily
used to construct the array named_address_spaces (see below).
*/
typedef struct a_named_address_space_descr {
  a_const_char
	*name;	/* Pointer to null-terminated name.  TR 18037 ("Embedded C")
		   requires that such address spaces have names in the
		   implementation namespace: They must start with a double
		   underscore, or with an underscore followed by an upper-case
		   letter. */
  a_named_address_space_id
	parent_id;
		/* Id of the named address space enclosing this one, or -1
		   if this named address space is not enclosed by any other
		   address.  The "generic address space" has id zero. */
} a_named_address_space_descr;


constexpr a_named_address_space_descr named_address_spaces[]
/*
A table describing the known named address spaces.  It can be indexed using
a named address space id.
*/
= {
/*  0: */ { "", -1 },	/* Placeholder for the "generic address space." */
#if INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES
/*  1: */ { "_EDG_NAS_A", 0 }, 
/*  2: */ { "_EDG_NAS_B", 1 /* = _EDG_NAS_A */ }, 
/*  3: */ { "_EDG_NAS_C", -1 }, 
#endif /* INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES */
/*  4: */ { NULL, 0 }	/* End-of-array marker. */
};
#endif /* NAMED_ADDRESS_SPACES_ALLOWED */

/*
Named-register storage classes (an Embedded C extension described in ISO/IEC
TR 18037) are identified using small integers of the following integer type.
Defined unconditionally because it is used in some function signatures.
Named-register storage classes are not to be confused with GNU register names
(which serve a similar purpose but rely on the GNU "asm ( string-literal )"
construct).
*/
typedef int a_named_register_id;

#if NAMED_REGISTERS_ALLOWED
/*
A structure describing a named-register storage class (an Embedded C feature
described in ISO/IEC TR 18037).  The structure is primarily used to construct
the array named_register_storage_classes (see below).
*/
typedef struct a_named_register_storage_class_descr {
  a_const_char
	*name;	/* Pointer to null-terminated name.  TR 18037 ("Embedded C")
		   requires that such storage classes have names in the
		   implementation namespace: They must start with a double
		   underscore, or with an underscore followed by an upper-case
		   letter. */
  a_targ_size_t
	size;
		/* Size of the corresponding register: This is an upper
		   bound for the size of a variable with this storage
		   class. */
} a_named_register_storage_class_descr;

/*
A macro representing the number of actual named registers available for
named-register storage classes.
*/
#if INCLUDE_EDG_TEST_NAMED_REGISTERS
#define NUM_NAMED_REGISTERS 3
#else /* !INCLUDE_EDG_TEST_NAMED_REGISTERS */
#define NUM_NAMED_REGISTERS 0
#endif /* INCLUDE_EDG_TEST_NAMED_REGISTERS */

constexpr a_named_register_storage_class_descr
		named_register_storage_classes[NUM_NAMED_REGISTERS+2]
/*
A table describing the known named-register storage classes.  It can be
indexed using a named register id.  The first and last entries do not
correspond to actual registers.
*/
= {
/*  0: */ { "", 0 },	/* Placeholder for "no register." */
#if INCLUDE_EDG_TEST_NAMED_REGISTERS
/*  1: */ { "_EDG_REG_1", 1 }, 
/*  2: */ { "_EDG_REG_2", TARG_SIZEOF_LONG }, 
/*  3: */ { "_EDG_REG_3", TARG_SIZEOF_LONG }, 
#endif /* INCLUDE_EDG_TEST_NAMED_REGISTERS */
/*  4: */ { NULL, 0 }	/* End-of-array marker. */
};
#endif /* NAMED_REGISTERS_ALLOWED */

/*
TRUE if an attempt should be made to fold all initializers to constant
expressions, FALSE if only initializers for variables with static duration
should be so treated.  
*/
#ifndef FAVOR_CONSTANT_RESULT_FOR_NONSTATIC_INIT
/* By default, do not fold automatic initializers if the C++-generating
   back end is in use; this will increase the probability that the
   generated code for these initializers is similar to the input source. */
/*lint -emacro(506,FAVOR_CONSTANT_RESULT_FOR_NONSTATIC_INIT)*/
#define FAVOR_CONSTANT_RESULT_FOR_NONSTATIC_INIT (!BACK_END_IS_CP_GEN_BE)
#endif /* ifndef FAVOR_CONSTANT_RESULT_FOR_NONSTATIC_INIT */

/*
GNU supports function multiversioning in version 4.8 and later, via the
"target" attribute.  Detailed documentation is available here:
http://gcc.gnu.org/wiki/FunctionMultiVersioning.  The "target" attribute allows
multiple versions of a function definition to be supplied, and the choice of
which version to call is determined at load time based on characteristics of
the machine where the program is running.  The choice is made by a
compiler-generated (when lowering is enabled) "resolver" routine that selects
the best routine from the target-specific versions.  The resolver routine is
executed once at load time (because it is associated with an "ifunc" routine)
to determine which target-specific routine to use for the particular CPU.
For back ends that don't support "ifunc", see LOWER_IFUNC.

For example (assuming USE_X86_FUNCTION_MULTIVERSIONING is TRUE):

  __attribute__ ((target("default")))     int foo () { return 222; }
  __attribute__ ((target("arch=corei7"))) int foo () { return 777; }
  __attribute__ ((target("avx")))         int foo () { return 333; }
  int main () {
    int (*fp)() = foo;
    return fp() != foo();
  }

Would generate (when lowered) pseudo-code like this:

  int foo_default () { return 222; }  // "default" foo
  int foo_corei7 () { return 777; }   // "corei7" foo
  int foo_avx () { return 333; }      // "avx" foo
  // Resolver function returns address of specific function
  static (int *foo.resolver()) { // lowering-generated resolver function
    resolved_foo = // one of foo_default, foo_corei7, foo_avx as appropriate
    return resolved_foo;
  }
  int foo.ifunc() __attribute__((ifunc("foo.resolver"));
  int main () {
    int (*fp)() = foo.ifunc;       // &foo lowered to &foo.ifunc
                                   // Dynamic loader invokes foo.resolver
                                   // to select which specific foo.
                                   // foo.ifunc is invoked once during
                                   // startup.
    return fp() != foo.ifunc();    // call of foo is lowered to foo.ifunc
  }

When GNU_FUNCTION_MULTIVERSIONING is TRUE, the front end does the necessary
processing to produce a resolver routine.  When it is FALSE, or in C mode,
the "target" attribute is accepted (with a warning) and included in the IL but
no additional processing is performed.
*/
#ifndef GNU_FUNCTION_MULTIVERSIONING
#define GNU_FUNCTION_MULTIVERSIONING FALSE
#endif /* !defined(GNU_FUNCTION_MULTIVERSIONING) */

#if GNU_FUNCTION_MULTIVERSIONING && !GNU_EXTENSIONS_ALLOWED
  #error GNU_FUNCTION_MULTIVERSIONING requires GNU_EXTENSIONS_ALLOWED
#endif /* GNU_FUNCTION_MULTIVERSIONING && !GNU_EXTENSIONS_ALLOWED */

/*
This flag controls whether the front end implements GNU function
multiversioning that is specific to the x86 family of processors.  When TRUE,
the various CPU and Instruction Set Architectures (ISAs) that are specified
in the GNU Function Multiversioning wiki
(http://gcc.gnu.org/wiki/FunctionMultiVersioning) are recognized and, when
using lowering, a resolver function is created to implement the decision
making process at run-time.  When this flag is FALSE (and
GNU_FUNCTION_MULTIVERSIONING is TRUE), routines with a "target" attribute
are treated as multiversion routines, but no checking of the attributes
is performed, and no resolver function can be created (when lowering).
For this configuration to be useful, front end modifications need to be
made (search for instances of USE_X86_FUNCTION_MULTIVERSIONING), or a
back end must provide the appropriate resolving.

Note that the IA-64 ABI specification does not dictate how to mangle
multiversioned functions, so the mangling that is used by the front end
emulates the mangling that is used by GNU (in the IA-64 ABI -- in the Cfront
ABI an EDG-specific mangling scheme is used).  The mangled names generated by
g++ have some drawbacks: they can't be demangled, and they contain periods
(which cannot be used with C-generating back ends).  See
REPLACE_SPECIAL_CHARACTERS_IN_MANGLED_NAMES below for a workaround for the
latter issue.
*/
#ifndef USE_X86_FUNCTION_MULTIVERSIONING
#define USE_X86_FUNCTION_MULTIVERSIONING GNU_FUNCTION_MULTIVERSIONING
#endif /* !defined(USE_X86_FUNCTION_MULTIVERSIONING) */

#if USE_X86_FUNCTION_MULTIVERSIONING && !GNU_FUNCTION_MULTIVERSIONING
  #error USE_X86_FUNCTION_MULTIVERSIONING requires GNU_FUNCTION_MULTIVERSIONING
#endif /* USE_X86_FUNCTION_MULTIVERSIONING && !GNU_FUNCTION_MULTIVERSIONING */

/*
Mangled names are made up of characters that appear in an identifier, but
in certain cases (i.e., GNU function multiversioning), the mangled names can
contain characters (e.g., a period) that are not allowed in an identifier.
Mangled names with periods cannot be used with the C-generating back end.
Setting this flag to TRUE causes these periods to be replaced with underscores.
*/
#ifndef REPLACE_SPECIAL_CHARACTERS_IN_MANGLED_NAMES
#if BACK_END_IS_C_GEN_BE
#define REPLACE_SPECIAL_CHARACTERS_IN_MANGLED_NAMES TRUE
#else /* !BACK_END_IS_C_GEN_BE */
#define REPLACE_SPECIAL_CHARACTERS_IN_MANGLED_NAMES FALSE
#endif /* BACK_END_IS_C_GEN_BE */
#endif /* !defined(REPLACE_SPECIAL_CHARACTERS_IN_MANGLED_NAMES) */

#if !REPLACE_SPECIAL_CHARACTERS_IN_MANGLED_NAMES && BACK_END_IS_C_GEN_BE
 #error REPLACE_SPECIAL_CHARACTERS_IN_MANGLED_NAMES must be TRUE when \
        BACK_END_IS_C_GEN_BE is TRUE
#endif /* !REPLACE_SPECIAL_CHARACTERS_IN_MANGLED_NAMES && BACK_END_IS_C... */

/*
In configurations where multiple target configurations are used, the
"legacy" configuration, i.e., the configuration that is specified by
target-specific configuration macros without target-specific suffixes, can
be given a name by defining LEGACY_TARGET_CONFIGURATION_NAME to be a string
literal containing the name.  This allows the configuration to be referred
to by name, either in a --target command-line option or by setting
DEFAULT_TARGET_CONFIGURATION_NAME to the same value.  If the legacy
configuration is given a name, there is a subtle difference between the use
of the legacy configuration by name, and the use of the legacy configuration
by default (when DEFAULT_TARGET_CONFIGURATION_NAME is unset): in the former
case, auxiliary_info_dir_name uses the value from EDG_AUXILIARY_INFO_DIR_NAME
and in the latter case, auxiliary_info_dir_name's value is modified to
append the legacy target's given name.  If both methods are used, it may be
wise to ensure that both directories contain the same information (say by
creating a link from one to the other).
*/
#ifndef LEGACY_TARGET_CONFIGURATION_NAME
#define LEGACY_TARGET_CONFIGURATION_NAME (a_const_char *)NULL
#endif /* defined(LEGACY_TARGET_CONFIGURATION_NAME) */

/*
To support the bfloat16 type, the C-generating back end uses the __bf16 type in
generated C code.  Support for the __bf16 type varies widely by version and
architecture and some host compilers support the __bf16 type as a storage type
but can't perform operations on objects of __bf16 type.  For full support, the
host-based back end C compiler must be able to perform operations on such
types.  Setting HOST_COMPILER_SUPPORTS_BFLOAT16 to TRUE indicates that the
back end C/C++ compiler has full support for the __bf16 type.  Setting
HOST_COMPILER_SUPPORTS_BFLOAT16 to FALSE will unconditionally disable the
bfloat16 type support in the EDG runtime library (though not in the front end).
The default value of HOST_COMPILER_SUPPORTS_BFLOAT16 is set heuristically
based on the host compiler being used to compile the front end.
*/
#ifndef HOST_COMPILER_SUPPORTS_BFLOAT16
#if defined(__clang_major__)
#if defined(__apple_build_version__) && defined(__arm64)
/* ARM-based MacOS compilers currently don't fully support __bf16. */
#define HOST_COMPILER_SUPPORTS_BFLOAT16 FALSE
#elif (defined(__arm) || defined(__arm64)) && __clang_major__ >= 11
/* ARM-based clang compilers support __bf16 beginning with version 11. */
#define HOST_COMPILER_SUPPORTS_BFLOAT16 TRUE
#elif (defined(__x86_64) || defined(__x86_64__)) && __clang_major__ >= 15
/* x86-based clang compilers support __bf16 beginning with version 15. */
#define HOST_COMPILER_SUPPORTS_BFLOAT16 TRUE
#endif /* defined(__apple_build_version__) && defined(__arm64) */
#endif /* defined(__clang_major__) */
#if defined(__GNUC__)
#if (defined(__arm) || defined(__arm64)) && __GNUC__ >= 10
/* ARM-based GNU compilers support __bf16 beginning with version 10. */
#define HOST_COMPILER_SUPPORTS_BFLOAT16 TRUE
#elif (defined(__x86_64) || defined(__x86_64__)) && __GNUC__ >= 13
/* x86-based GNU compilers support __bf16 beginning with version 13. */
#define HOST_COMPILER_SUPPORTS_BFLOAT16 TRUE
#endif /* (defined(__arm) || defined(__arm64)) && __GNUC__ >= 10 */
#endif /* defined(__GNUC__) */

#ifndef HOST_COMPILER_SUPPORTS_BFLOAT16
/* If not set above, assume the host compiler does not have support. */
#define HOST_COMPILER_SUPPORTS_BFLOAT16 FALSE 
#endif /* defined(HOST_COMPILER_SUPPORTS_BFLOAT16) */

#endif /* defined(HOST_COMPILER_SUPPORTS_BFLOAT16) */

#ifndef PRESERVE_EMBED_DIRECTIVE_WHEN_OPTIMIZED
/*
When PRESERVE_EMBED_DIRECTIVE_WHEN_OPTIMIZED is TRUE, a ck_string constant
representing an optimized #embed expansion will include a textual
representation of the tokens that form the #embed directive, allowing the
C++-generating back end to put it out in the generated code.  If this macro
is FALSE, the C++-generating back end will put out the list of integer
constants of the directive's expansion.
*/
#if BACK_END_IS_CP_GEN_BE
#define PRESERVE_EMBED_DIRECTIVE_WHEN_OPTIMIZED TRUE
#else /* !BACK_END_IS_CP_GEN_BE*/
#define PRESERVE_EMBED_DIRECTIVE_WHEN_OPTIMIZED FALSE
#endif /* BACK_END_IS_CP_GEN_BE */
#endif /* defined(PRESERVE_EMBED_DIRECTIVE_WHEN_OPTIMIZED */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* !defined(TARG_DEF_H) */


