/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

defines.h -- Defines configuration parameters for a given version of the
             front end.

*/

/*
A word of caution about using TRUE/FALSE values in this header file: The
macros TRUE and FALSE are defined in basics.h, but their definition occurs
only after defines.h is included.  A side-effect of this is that any #if
test of a macro whose value is either TRUE or FALSE will result in the false
branch being taken.  Consequently, it's best to use 1 or 0 instead of TRUE and
FALSE in this header file.
*/

/* Avoid including these declarations more than once. */
#ifndef DEFINES_H
#define DEFINES_H 1

/*
Note: This is the EDG internal version.  The version shipped as part of
the release should contain no defines.
*/

#if defined(USE_CMAKE_DEFINES)

/*
If USE_CMAKE_DEFINES is present, use the CMake generated cmake_defines.h file.
*/
#include "cmake_defines.h"

#else /* !defined(USE_CMAKE_DEFINES) */

/*
High level EDG macros used solely in this file for easy configuration:

  DEMO_VERSION          Used to compile a demo version.
                        Sets SUN_TEST_VERSION, LINUX_TEST_VERSION,
                        MACOSX_TEST_VERSION (as appropriate for the host) to 0.

  OPTIMIZED_VERSION     Create an optimized version (smaller, faster, fewer
                        features).

  SUN_TEST_VERSION      Define a set of 'standard' language features for
                        a Sun hosted compiler.  Defined to 1 by default when
                        'sun' is defined.  Most run_tests were recorded
                        with this set of language features defined.

  LINUX_TEST_VERSION    Define a set of 'standard' language features for
                        a Linux hosted compiler.  Defined to 1 by default when
                        '__linux__' is defined.

  MACOSX_TEST_VERSION   Define a set of 'standard' language features for
                        a Mac OS X hosted compiler.  Defined to 1 by default
                        when '__APPLE__' and '__MACH__' are defined.

  MSVC_IDE_VERSION      Define a set of 'standard' language features for
                        an MSVC compiler from within the IDE.  Several subsets
                        exist to facilitate easily turning on/off related
                        feature macros.

  EDG_TEST_VERSION      Define a set of 'standard' language features that
                        can be used on a variety of hosts.  This set is
                        meant to include many of the major language features.
                        Originally based on the set of SUN_TEST_VERSION
                        features so that many of the run_tests will continue
                        to work properly.

  CP_GEN_BE_VERSION     Flags to be set for any version that uses the 
                        C++ generating back end.

  SSI_VERSION           Generating instantiations in source sequence lists.
                        Only valid when CP_GEN_BE_VERSION is defined.

  SELFCOMP_VERSION      Self-compiled version.  Currently used only on
                        Sun and HP-UX platforms.

The macros SUN_TEST_VERSION, LINUX_TEST_VERSION, MACOSX_TEST_VERSION are
defined and used only within this file.  EDG_TEST_VERSION is defined
when SUN_TEST_VERSION is defined and may also be defined externally to this
file to cause inclusion of a standard set of language features, regardless
of the host system.

*/

/* Used for union-as-struct testing mode. */
#ifdef UNION_AS_STRUCT
#define union struct
#endif /* ifdef UNION_AS_STRUCT */


#ifdef MSVC_IDE_VERSION

#ifndef MSVC_INT128_CONFIG
#define MSVC_INT128_CONFIG 1
#endif /* ifndef MSVC_INT128_CONFIG */
#ifndef MSVC_FLOAT128_CONFIG
#define MSVC_FLOAT128_CONFIG 0
#endif /* ifndef MSVC_FLOAT128_CONFIG */
#ifndef MSVC_FIXED_POINT_CONFIG
#define MSVC_FIXED_POINT_CONFIG 1
#endif /* ifndef MSVC_FIXED_POINT_CONFIG */
#ifndef MSVC_IL_DEBUGGING_CONFIG
#define MSVC_IL_DEBUGGING_CONFIG 0
#endif /* ifndef MSVC_IL_DEBUGGING_CONFIG */
#ifndef MSVC_SSL_CONFIG
#define MSVC_SSL_CONFIG 1
#endif /* ifndef MSVC_SSL_CONFIG */
#ifndef MSVC_CP_GEN_CONFIG
#define MSVC_CP_GEN_CONFIG 0
#endif /* ifndef MSVC_CP_GEN_CONFIG */
#ifndef MSVC_GNU_CONFIG
#define MSVC_GNU_CONFIG 1
#endif /* ifndef MSVC_GNU_CONFIG */
#ifndef MSVC_SUN_CONFIG
#define MSVC_SUN_CONFIG 1
#endif /* ifndef MSVC_SUN_CONFIG */
#ifndef MSVC_IA64_CONFIG
#define MSVC_IA64_CONFIG 0
#endif /* MSVC_IA64_CONFIG */

/* Base configuration. */
#define OPTIMIZED_VERSION 0
#define NO_USR_INCLUDE 1

#define ASM_FUNCTION_ALLOWED 1
#define COMPILE_MULTIPLE_SOURCE_FILES 0
#define COMPILE_MULTIPLE_TRANSLATION_UNITS 1
#define DEFAULT_GNU_COMPATIBILITY 0
#define DEFAULT_INSTANTIATION_MODE tim_none
#define DEFAULT_OUTPUT_MODE om_text
#define DEFAULT_INCOGNITO 0
#define DEFAULT_MACRO_POSITIONS_IN_DIAGNOSTICS 1
#define DEFAULT_MICROSOFT_MODE 0
#define EXPENSIVE_CHECKING 1
#define CHECK_SWITCH_DEFAULT_UNEXPECTED 0
#define EXTRA_SOURCE_POSITIONS_IN_IL 1
#define FULLY_RESOLVED_MACRO_POSITIONS 1
#define IL_SHOULD_BE_WRITTEN_TO_FILE 1
#define MACRO_INVOCATION_TREE_IN_IL 0
#define GNU_VISIBILITY_ATTRIBUTE_ALLOWED 1
#define UPC_EXTENSIONS_ALLOWED 1

#define GCC_IS_GENERATED_CODE_TARGET 0
#define MSVC_IS_GENERATED_CODE_TARGET 1
#define MSVC_TARGET_VERSION_NUMBER 1923

#define DEFAULT_TYPE_INFO_IN_NAMESPACE_STD 1
#define MICROSOFT_MODE_TYPE_INFO_IN_NAMESPACE_STD 1
#define PRAGMA_DEFINE_TYPE_INFO_IS_REQUIRED 1

#define USE_VIRTUAL_FUNCTIONS 1

#ifndef USE_MMAP_FOR_MODULES
#define USE_MMAP_FOR_MODULES 1
#endif /* USE_MMAP_FOR_MODULES */

#ifndef USE_MMAP_FOR_MEMORY_REGIONS
#define USE_MMAP_FOR_MEMORY_REGIONS 1
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */

#define USE_SOFTFLOAT 1

#if MSVC_INT128_CONFIG
#define INT128_EXTENSIONS_ALLOWED 1
#endif /* MSVC_INT128_CONFIG */

#if MSVC_FLOAT128_CONFIG
#define FLOAT128_ENABLING_POSSIBLE 1
#define APPROXIMATE_QUADMATH 1
#endif /* MSVC_FLOAT128_CONFIG */

#if MSVC_FIXED_POINT_CONFIG
#define FIXED_POINT_ALLOWED 1
#define ALLOW_HOST_FP_TOO_SMALL_FOR_LARGEST_FIXED_POINT_TYPE 1
#define DEFAULT_FIXED_POINT_ENABLED 1
#endif /* MSVC_FIXED_POINT_CONFIG */

#if MSVC_IL_DEBUGGING_CONFIG
#define IL_SHOULD_BE_WRITTEN_TO_FILE 1
#define ALTERNATE_IL_FILE_FORMAT 1
#define OVERWRITE_FREED_MEM_BLOCKS 0
#endif /* MSVC_IL_DEBUGGING_CONFIG */

#if MSVC_SSL_CONFIG
#define GENERATE_SOURCE_SEQUENCE_LISTS 1
#define ALLOW_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING 1
#endif /* MSVC_SSL_CONFIG */

#if MSVC_CP_GEN_CONFIG
#define CP_GEN_BE_VERSION 1
#define CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT 0
#define AUTOMATIC_TEMPLATE_INSTANTIATION 1
#define PROTOTYPE_INSTANTIATIONS_IN_IL 1
#undef COMPILE_MULTIPLE_TRANSLATION_UNITS
#endif /* MSVC_CP_GEN_CONFIG */

#if MSVC_GNU_CONFIG
#define GNU_EXTENSIONS_ALLOWED 1
#define GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED 1
#define GNU_TARGET_VERSION_NUMBER 70400
#define GNU_VECTOR_TYPES_ALLOWED 1
#define GNU_X86_ASM_EXTENSIONS_ALLOWED 1
#define GNU_X86_ATTRIBUTES_ALLOWED 1
#endif /* MSVC_GNU_CONFIG */

#if MSVC_SUN_CONFIG
#define SUN_EXTENSIONS_ALLOWED 1
#define DEFAULT_SUN_COMPATIBILITY 0
#define DEFAULT_SUN_LINKER_SCOPE_ALLOWED 0
#endif /* MSVC_SUN_CONFIG */

#if MSVC_IA64_CONFIG
#define IA64_ABI 1
#define INCLUDE_ADDITIONAL_TARGET_CONFIGURATION 0
#endif /* MSVC_IA64_CONFIG */

#endif /* MSVC_IDE_VERSION */

/*
Enable target-specific configurations for all but demo versions (we don't
deliver multiple libraries to support this).
*/
#ifndef INCLUDE_ADDITIONAL_TARGET_CONFIGURATION
#ifdef DEMO_VERSION
#define INCLUDE_ADDITIONAL_TARGET_CONFIGURATION 0
#else /* !DEMO_VERSION */
#define INCLUDE_ADDITIONAL_TARGET_CONFIGURATION 1
#endif /* DEMO_VERSION */
#endif /* defined(INCLUDE_ADDITIONAL_TARGET_CONFIGURATION) */

/*
Set the test version flags to FALSE for demo versions.
*/
#ifdef DEMO_VERSION
#ifdef __sun
#define SUN_TEST_VERSION 0
#endif  /* ifdef __sun */
#ifdef __linux__
#define LINUX_TEST_VERSION 0
#endif /* ifdef __linux__ */
#if defined(__APPLE__) && defined(__MACH__)
#define MACOSX_TEST_VERSION 0
#endif /* defined(__APPLE__) && defined(__MACH__) */
#ifndef DEBUG
#define DEBUG 0
#endif /* ifndef DEBUG */
#else /* !defined(DEMO_VERSION) */
#ifndef __CYGWIN__
/* In development versions, allow values of gnu_version less than 30200 so
   some early gcc compatibility features can be tested. */
#define MIN_GNU_VERSION 29500
#endif /* ifndef __CYGWIN__ */
#endif /* ifdef DEMO_VERSION */

#define ENABLE_TRANS_UNIT_TEST_MODE 1

#ifdef IA64_ABI
#if IA64_ABI
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT (-1)
#endif /* IA64_ABI */
#endif /* ifdef IA64_ABI */

#ifndef DEFAULT_MODULES_ENABLED
#define DEFAULT_MODULES_ENABLED TRUE
#endif /* ifndef DEFAULT_MODULES_ENABLED */

#ifdef CP_GEN_BE_VERSION
/*
Flags to be set for any version that uses the C++ generating back end.
*/
#ifndef CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT
#define CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT 1
#endif /* ifndef CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT */
#define BACK_END_IS_C_GEN_BE 0
#define BACK_END_IS_CP_GEN_BE 1
#define BACK_END_IS_LLVM_GEN_BE 0
#define DEFAULT_EXCEPTIONS_ENABLED 0
#define COMPILE_MULTIPLE_TRANSLATION_UNITS 0
#define DO_IL_LOWERING 0
#ifndef INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL
#define INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL 0
#endif /* ifndef INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL */
#ifdef SSI_VERSION
/* Generating instantiations in source sequence lists. */
#define CLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS 1
#define NONCLASS_TEMPLATE_INSTANTIATIONS_IN_SOURCE_SEQUENCE_LISTS 1
#define AUTOMATIC_TEMPLATE_INSTANTIATION 1
#define RECORD_FORM_OF_NAME_REFERENCE 0
#else /* !defined(SSI_VERSION) */
#define INSTANTIATE_EXTERN_INLINE 0
#ifndef AUTOMATIC_TEMPLATE_INSTANTIATION
#define AUTOMATIC_TEMPLATE_INSTANTIATION 0
#endif /* ifndef AUTOMATIC_TEMPLATE_INSTANTIATION */
#endif /* ifdef SSI_VERSION */
#else /* !defined(CP_GEN_BE_VERSION) */
#ifndef BACK_END_IS_CP_GEN_BE
#define BACK_END_IS_CP_GEN_BE 0
#endif /* ifndef BACK_END_IS_CP_GEN_BE */
#ifndef BACK_END_IS_LLVM_GEN_BE
#define BACK_END_IS_LLVM_GEN_BE 0
#endif /* ifndef BACK_END_IS_LLVM_GEN_BE */
#if BACK_END_IS_LLVM_GEN_BE
#undef BACK_END_IS_C_GEN_BE
#define BACK_END_IS_C_GEN_BE 0
#endif /* BACK_END_IS_LLVM_GEN_BE */
#endif /* ifdef CP_GEN_BE_VERSION */

#ifdef __sun

/* Default to SOLARIS unless SUNOS is defined. */
#ifndef SUNOS
#ifndef SOLARIS
#define SOLARIS 1
#endif /* ifndef SOLARIS */
#endif /* ifndef SUNOS */

#ifdef SUNOS
/* Default to __BSD__ on SunOS, unless __ANSIC__ has been defined. */
#ifndef __BSD__
#ifndef __ANSIC__
#define __BSD__ 1
#endif /* ifndef __ANSIC__ */
#endif /* ifndef __BSD__ */
/* Default to generating pcc C on SunOS. */
#ifndef C_GEN_BE_GENERATES_ANSI_C
#define C_GEN_BE_GENERATES_ANSI_C 0
#define GCC_IS_GENERATED_CODE_TARGET 0
#define SUN_IS_GENERATED_CODE_TARGET 1
#define SUN_TARGET_VERSION_NUMBER 0
#define ASM_FUNCTION_ALLOWED 0
#endif /* ifndef C_GEN_BE_GENERATES_ANSI_C */
/* Implement long double as double. */
#define USE_LONG_DOUBLE_FOR_HOST_FP_VALUE 0
#define TARG_SIZEOF_LONG_DOUBLE 8
#else /* !SUNOS, i.e. SOLARIS */
#endif /* SUNOS */

/* Assume we are generating code for gcc when being compiled by gcc */
#ifndef CP_GEN_BE_VERSION
#ifndef GCC_IS_GENERATED_CODE_TARGET
#if defined(__GNUC__)
#define GCC_IS_GENERATED_CODE_TARGET 1
#endif /* defined(__GNUC__) */
#endif /* ifndef GCC_IS_GENERATED_CODE_TARGET */
#endif /* ifndef CP_GEN_BE_VERSION */

#ifndef SUN_TEST_VERSION
#define SUN_TEST_VERSION 1
#endif /* ifndef SUN_TEST_VERSION */

#if SUN_TEST_VERSION
#define USE_LONG_DOUBLE_FOR_HOST_FP_VALUE 0
#define FP_HAS_LONG_DOUBLE 0
#define ALLOW_HOST_FP_TOO_SMALL_FOR_LARGEST_FIXED_POINT_TYPE 1
#ifndef UNICODE_SOURCE_SUPPORTED
#define UNICODE_SOURCE_SUPPORTED 1
#endif /* ifndef UNICODE_SOURCE_SUPPORTED */
#endif /* SUN_TEST_VERSION */

#include "defines_solaris.h"

#ifndef DEFAULT_EDG_BASE
#define DEFAULT_EDG_BASE "/edg/cpfe"
#endif /* DEFAULT_EDG_BASE */

#if SUN_TEST_VERSION

/* Specify a language feature set by defining EDG_TEST_VERSION.  Add or
   override any additional settings here, as well as any host specific 
   options. */
#define EDG_TEST_VERSION 1

/* Options common to Sun-hosted versions. */

#ifndef CP_GEN_BE_VERSION
#define LOWERING_NORMALIZES_BOOLEAN_CONTROLLING_EXPRESSIONS 1
#endif /* ifndef CP_GEN_BE_VERSION */
#ifndef INSTANTIATE_EXTERN_INLINE
#ifdef SUNOS
#define INSTANTIATE_EXTERN_INLINE 0
#else /* !defined(SUNOS) */
#define INSTANTIATE_EXTERN_INLINE 1
#endif /* ifdef SUNOS */
#endif /* INSTANTIATE_EXTERN_INLINE */
#ifdef SELFCOMP_VERSION
/* Self-compiled version. */
#define ALTERNATE_IL_FILE_FORMAT 0
#define ONE_INSTANTIATION_PER_OBJECT 0
#define MAINTAIN_NEEDED_FLAGS 0
#define DEFAULT_REMOVE_UNNEEDED_ENTITIES 0
#ifndef LOWER_VARIABLE_LENGTH_ARRAYS
#define LOWER_VARIABLE_LENGTH_ARRAYS 0
#endif /* ifndef LOWER_VARIABLE_LENGTH_ARRAYS */
#ifdef SOLARIS
#define __EXTENSIONS__ 1
#endif /* SOLARIS */
#define GCC_IS_GENERATED_CODE_TARGET 1
#endif /* SELFCOMP_VERSION */
#define DEFAULT_EMULATE_MSVC_VALUE_INITIALIZATION_BUGS 1
#define DEFAULT_EMULATE_GNU_VALUE_INITIALIZATION_BUGS 1
#if GNU_EXTENSIONS_ALLOWED
#define GNU_VECTOR_TYPES_ALLOWED 1
#endif /* GNU_EXTENSIONS_ALLOWED */

#ifdef SOLARIS
#ifdef __SUNPRO_C
#if 0
/* Does not always work right. */
#define GUARD_MACRO2_FOR_VA_LIST "_SYS_VA_LIST_H"
#endif /* 0 */
#endif /* ifdef __SUNPRO_C */
#define REDEFINE_EXTNAME_PRAGMA_ENABLED 1
#if defined(IA64_ABI) && IA64_ABI
/* Use the <=4.0 virtual base class handling technique. */
#ifndef HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS
#define HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS 0
#endif /* ifndef HANDLE_VIRTUAL_BASES_IN_COMPLETE_CTOR_DTORS */
#ifndef HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS
#define HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS 1
#endif /* HANDLE_VIRTUAL_BASES_IN_SUBOBJECT_CTOR_DTORS */
#endif /* defined(IA64_ABI) && IA64_ABI */
#else /* !defined(SOLARIS) */
/* SunOS version. */
#ifndef C_GEN_BE_GENERATES_ANSI_C
#define C_GEN_BE_GENERATES_ANSI_C 0
#ifndef GCC_IS_GENERATED_CODE_TARGET
#define GCC_IS_GENERATED_CODE_TARGET 0
#endif /* ifndef GCC_IS_GENERATED_CODE_TARGET */
#endif /* ifndef C_GEN_BE_GENERATES_ANSI_C */
#endif /* ifdef SOLARIS */

#ifndef OPTIMIZED_VERSION
/* Options for Sun test version. */
#define GENERATE_SOURCE_SEQUENCE_LISTS 1
#define ALLOW_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING 1
#define RECORD_HIDDEN_NAMES_IN_IL 1
#define ALLOW_HIDDEN_NAMES_IN_IL_WITH_IL_LOWERING 1
#define RECORD_TEMPLATE_STRINGS 1
#ifndef INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL
#define INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL 1
#endif /* ifndef INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL */
#define RECORD_MACROS_IN_IL 1
#define KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED 1
#define DUMP_LOWERED_EH_CONSTRUCTS_IN_C_GEN_BE 1
#ifdef SOLARIS
#ifndef ASM_FUNCTION_ALLOWED
#define ASM_FUNCTION_ALLOWED 1
#endif /* !defined(ASM_FUNCTION_ALLOWED) */
#endif /* ifdef SOLARIS */

/* Use 1 for mmap PCH, 0 for non-mmap PCH. */
#if 1
#define USE_FIXED_ADDRESS_FOR_MMAP 1
#define FIXED_ADDRESS_FOR_MMAP (0xa0000000)
#ifndef USE_MMAP_FOR_MEMORY_REGIONS
#define USE_MMAP_FOR_MEMORY_REGIONS 1
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
#else /* !1 */
#ifndef USE_MMAP_FOR_MEMORY_REGIONS
#define USE_MMAP_FOR_MEMORY_REGIONS 0
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
#endif /* 1 */

#endif /* ifndef OPTIMIZED_VERSION */

#endif /* SUN_TEST_VERSION */
#define SUN_EXTENSIONS_ALLOWED 1
#define DEFAULT_SUN_COMPATIBILITY 0

#else /* !defined(__sun) */

#if defined(_WIN32) && !defined(__CYGWIN__)

/* Options for Windows-NT version. */

#include "defines_win32.h"

#else /* !defined(_WIN32) */

#ifdef __linux__

/* Linux version. */

#ifndef LINUX_TEST_VERSION
#define LINUX_TEST_VERSION 1
#endif /* ifndef LINUX_TEST_VERSION */

#if LINUX_TEST_VERSION
/* defines_linux.h sets this to TRUE if not already set. */
#ifndef IA64_ABI
#define IA64_ABI 0
#endif /* IA64_ABI */

#if IA64_ABI
/* Tie the GNU ABI version to the GNU version in the IA-64 test version. */
#define TIE_DEFAULT_GNU_ABI_VERSION_TO_GNU_VERSION 1
#undef MIN_GNU_VERSION
/*lint -esym(767,MIN_GNU_VERSION)*/
#define MIN_GNU_VERSION 30200
/* The IA-64 test version includes embedded C support. */
#define EMBEDDED_C_ALLOWED 1
#define DEFAULT_EMBEDDED_C_ENABLED 0
#define INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES 1
#define INCLUDE_EDG_TEST_NAMED_REGISTERS 1
#endif /* IA64_ABI */

/*
defines_linux.h has been updated to provide better compatibility with
the g++ headers.  Setting this macro to 0 retains the previous behavior.
*/
#ifndef CONFIG_FOR_GPP_HEADER_COMPATIBILITY
#define CONFIG_FOR_GPP_HEADER_COMPATIBILITY 0
#endif /* ifndef CONFIG_FOR_GPP_HEADER_COMPATIBILITY */

#endif /* LINUX_TEST_VERSION */

#include "defines_linux.h"

#if LINUX_TEST_VERSION

/* Linux test version definitions. */
#ifndef INCLUDE_EDG_TEST_PRAGMAS
#define INCLUDE_EDG_TEST_PRAGMAS 1
#endif /* ifndef INCLUDE_EDG_TEST_PRAGMAS */
#define INCLUDE_EDG_TEST_ATTRIBUTES 1
#ifndef _lint
#endif /* ifndef _lint */
#ifndef CHECKING
#define CHECKING 1
#endif /* ifndef CHECKING */
#ifndef DEBUG
#define DEBUG 1
#endif /* ifndef DEBUG */
#define SAME_REPR_INTS_INTERCHANGEABLE_IN_IL 0
#ifndef ASSIGNMENT_TO_THIS_ALLOWED
#define ASSIGNMENT_TO_THIS_ALLOWED 0
#endif /* ASSIGNMENT_TO_THIS_ALLOWED */
#define DEFAULT_ALLOW_ANACHRONISMS 0
#define CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG 0
#define CFRONT_2_1_OBJECT_CODE_COMPATIBILITY 0
#ifndef NEW_CAN_BE_FOLDED_INTO_CTOR
#define NEW_CAN_BE_FOLDED_INTO_CTOR 0
#endif /* NEW_CAN_BE_FOLDED_INTO_CTOR */
#define MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED 1
#define USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING 0
#define PRAGMA_WEAK_ALLOWED 1
#ifndef ASM_FUNCTION_ALLOWED
#define ASM_FUNCTION_ALLOWED 1
#endif /* ASM_FUNCTION_ALLOWED */
#define INCLUDE_COMMENTS_IN_ASM_FUNC_BODY 1
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT 1
#ifndef TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT 4
#endif /* ifndef TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT */
#ifndef MAINTAIN_NEEDED_FLAGS
#define MAINTAIN_NEEDED_FLAGS 1
#endif /* ifndef MAINTAIN_NEEDED_FLAGS */
#ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES
#define DEFAULT_REMOVE_UNNEEDED_ENTITIES 0
#endif /* ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES */
#define DEFAULT_FRIEND_INJECTION 1
#ifndef COMPILE_MULTIPLE_TRANSLATION_UNITS
#undef COMPILE_MULTIPLE_SOURCE_FILES
#define COMPILE_MULTIPLE_TRANSLATION_UNITS 1
#endif /* ifndef COMPILE_MULTIPLE_TRANSLATION_UNITS */
#define DEFAULT_DISPLAY_ERROR_CONTEXT_ON_CATASTROPHE 0
#ifndef LOWER_VARIABLE_LENGTH_ARRAYS
#define LOWER_VARIABLE_LENGTH_ARRAYS 0
#endif /* ifndef LOWER_VARIABLE_LENGTH_ARRAYS */
#if !defined(UNICODE_SOURCE_SUPPORTED) || !UNICODE_SOURCE_SUPPORTED
#define ACCEPT_GNU_CARRIAGE_RETURN_LINE_TERMINATOR 1
#endif /* !defined(UNICODE_SOURCE_SUPPORTED) || !UNICODE_SOURCE_SUPPORTED */
#define LOWER_DESIGNATED_INITIALIZERS 1

#ifndef OPTIMIZED_VERSION
#ifndef EXPENSIVE_CHECKING
#define EXPENSIVE_CHECKING 1
#endif /* ifndef EXPENSIVE_CHECKING */
#endif /* ifndef OPTIMIZED_VERSION */

#endif /* LINUX_TEST_VERSION */

#if INCLUDE_ADDITIONAL_TARGET_CONFIGURATION
/*
For testing purposes, include win64 and target configurations.  Note that
typeinfo is handled differently than a typical Linux configuration so
--set_flag force_ms_type_info_not_in_namespace_std may be needed on the
command-line when compiling system headers.
*/
/* Target configuration: win64 */
#define TARGET_CONFIGURATION_2 win64
#define TARG_ALIGNOF_DOUBLE_win64 8
#define TARG_ALIGNOF_FAR_POINTER_win64 4
#define TARG_ALIGNOF_FLOAT_win64 4
#define TARG_ALIGNOF_FLOAT128_win64 16
#define TARG_ALIGNOF_FLOAT80_win64 4
#define TARG_ALIGNOF_INT_win64 4
#define TARG_ALIGNOF_INT128_win64 16
#define TARG_ALIGNOF_LONG_win64 4
#define TARG_ALIGNOF_LONG_DOUBLE_win64 8
#define TARG_ALIGNOF_LONG_LONG_win64 8
#define TARG_ALIGNOF_NEAR_POINTER_win64 2
#define TARG_ALIGNOF_POINTER_win64 8
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER_win64 4
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION_win64 8
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS_win64 4
#define TARG_ALIGNOF_SHORT_win64 2
#define TARG_ALIGNOF_SIGNED_ACCUM_win64 4
#define TARG_ALIGNOF_SIGNED_FRACT_win64 2
#define TARG_ALIGNOF_SIGNED_LONG_ACCUM_win64 8
#define TARG_ALIGNOF_SIGNED_LONG_FRACT_win64 4
#define TARG_ALIGNOF_SIGNED_SHORT_ACCUM_win64 2
#define TARG_ALIGNOF_SIGNED_SHORT_FRACT_win64 1
#define TARG_ALIGNOF_UNSIGNED_ACCUM_win64 4
#define TARG_ALIGNOF_UNSIGNED_FRACT_win64 2
#define TARG_ALIGNOF_UNSIGNED_LONG_ACCUM_win64 8
#define TARG_ALIGNOF_UNSIGNED_LONG_FRACT_win64 4
#define TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM_win64 2
#define TARG_ALIGNOF_UNSIGNED_SHORT_FRACT_win64 1
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO_win64 8
#define TARG_ALL_POINTERS_SAME_SIZE_win64 1
#define TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT_win64 1
#define TARG_BIT_FIELD_CONTAINER_SIZE_win64 (-1)
#define TARG_BOOL_INT_KIND_win64 ((an_integer_kind)ik_char)
#define TARG_C_BOOL_INT_KIND_win64 ((an_integer_kind)ik_unsigned_char)
#define TARG_CHAR16_T_INT_KIND_win64 ((an_integer_kind)ik_unsigned_short)
#define TARG_CHAR32_T_INT_KIND_win64 ((an_integer_kind)ik_unsigned_int)
#define TARG_CHAR_BIT_win64 8
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT_win64 1
#define TARG_DBL_MANT_DIG_win64 53
#define TARG_DBL_MAX_EXP_win64 1024
#define TARG_DBL_MIN_EXP_win64 (-1021)
#define TARG_DEFAULT_NEW_ALIGNMENT_win64 16
#define TARG_DELTA_INT_KIND_win64 ((an_integer_kind)ik_short)
#define TARG_DOUBLE_FIELD_ALIGNMENT_win64 4
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES_win64 1
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED_win64 0
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT_win64 0
#define TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE_win64 1
#define TARG_FLOAT_FIELD_ALIGNMENT_win64 4
#define TARG_FLOAT128_FIELD_ALIGNMENT_win64 16
#define TARG_FLOAT80_FIELD_ALIGNMENT_win64 4
#define TARG_FLT_MANT_DIG_win64 24
#define TARG_FLT_MAX_EXP_win64 128
#define TARG_FLT_MIN_EXP_win64 (-125)
#define TARG_FLT128_MANT_DIG_win64 113
#define TARG_FLT128_MAX_EXP_win64 (16384)
#define TARG_FLT128_MIN_EXP_win64 (-16381)
#define TARG_FLT80_MANT_DIG_win64 64
#define TARG_FLT80_MAX_EXP_win64 (16384)
#define TARG_FLT80_MIN_EXP_win64 (-16381)
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED_win64 1
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM_win64 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT_win64 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM_win64 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT_win64 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM_win64 7
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT_win64 7
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM_win64 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT_win64 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM_win64 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT_win64 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM_win64 8
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT_win64 8
#define TARG_HAS_SIGNED_CHARS_win64 1
#define TARG_HOST_STRING_CHAR_BIT_win64 8
#define TARG_IA64_ABI_USE_GUARD_ACQUIRE_RELEASE_win64 1
#define TARG_IA64_ABI_USE_INT_STATIC_INIT_GUARD_win64 0
#define TARG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES_win64 0
#define TARG_IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR_win64 0
#define TARG_IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS_win64 0
#define TARG_IA64_ABI_VARIANT_KEY_FUNCTION_win64 0
#define TARG_IA64_VTABLE_ENTRY_INT_KIND_win64 ((an_integer_kind)ik_long_long)
#define TARG_INT_FIELD_ALIGNMENT_win64 4
#define TARG_INT128_FIELD_ALIGNMENT_win64 16
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT_win64 0
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND_win64 ((a_float_kind)fk_long_double)
#define TARG_JMP_BUF_ELEMENT_INT_KIND_win64 ((an_integer_kind)ik_int)
#define TARG_JMP_BUF_NUM_ELEMENTS_win64 39
#define TARG_SETJMP_FUNC_win64 "setjmp"
#define TARG_LDBL_MANT_DIG_win64 53
#define TARG_LDBL_MAX_EXP_win64 1024
#define TARG_LDBL_MIN_EXP_win64 (-1021)
#define TARG_LIBGCC_CMP_RETURN_MODE_win64 ((a_type_mode_kind)tmk_SI)
#define TARG_LIBGCC_SHIFT_COUNT_MODE_win64 ((a_type_mode_kind)tmk_SI)
#define TARG_LITTLE_ENDIAN_win64 1
#define TARG_LONG_DOUBLE_FIELD_ALIGNMENT_win64 4
#define TARG_LONG_FIELD_ALIGNMENT_win64 4
#define TARG_LONG_LONG_FIELD_ALIGNMENT_win64 8
#define TARG_MAXIMUM_INTRINSIC_ALIGNMENT_win64 8
#define TARG_MAX_BASE_CLASS_OFFSET_win64 0
#define TARG_MAX_CLASS_OBJECT_SIZE_win64 0
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION_win64 0
#define TARG_MICROSOFT_PTR_TO_MEMBER_SIZING_win64 BACK_END_IS_CP_GEN_BE
#define TARG_MINIMUM_STRUCT_ALIGNMENT_win64 1
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED_win64 1
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT_win64 1
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE_win64 1
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED_win64 0
#define TARG_POINTER_MODE_win64 ((a_type_mode_kind)tmk_SI)
#define TARG_PTRDIFF_T_INT_KIND_win64 ((an_integer_kind)ik_long_long)
#define TARG_REGION_NUMBER_INT_KIND_win64 ((an_integer_kind)ik_unsigned_short)
#define TARG_ETS_FLAG_TYPE_INT_KIND_win64 ((an_integer_kind)ik_unsigned_int)
#define TARG_REUSE_TAIL_PADDING_win64 1
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC_win64 1
#define TARG_RUNTIME_ELEM_COUNT_INT_KIND_win64 ((an_integer_kind)ik_long_long)
#define TARG_SHORT_FIELD_ALIGNMENT_win64 2
#define TARG_SIZEOF_DOUBLE_win64 8
#define TARG_SIZEOF_FAR_POINTER_win64 4
#define TARG_SIZEOF_FLOAT_win64 4
#define TARG_SIZEOF_FLOAT128_win64 16
#define TARG_SIZEOF_FLOAT80_win64 12
#define TARG_SIZEOF_INT_win64 4
#define TARG_SIZEOF_INT128_win64 16
#define TARG_SIZEOF_LARGEST_FIXED_POINT_win64 8
#define TARG_SIZEOF_LARGEST_ATOMIC_win64 16
#define TARG_SIZEOF_LONG_win64 4
#define TARG_SIZEOF_LONG_DOUBLE_win64 8
#define TARG_SIZEOF_LONG_LONG_win64 8
#define TARG_SIZEOF_NEAR_POINTER_win64 2
#define TARG_SIZEOF_POINTER_win64 8

#define TARG_SIZEOF_PTR_TO_DATA_MEMBER_win64 4
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION_win64 16
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS_win64 4
#define TARG_SIZEOF_SHORT_win64 2
#define TARG_SIZEOF_SIGNED_ACCUM_win64 4
#define TARG_SIZEOF_SIGNED_FRACT_win64 2
#define TARG_SIZEOF_SIGNED_LONG_ACCUM_win64 8
#define TARG_SIZEOF_SIGNED_LONG_FRACT_win64 4
#define TARG_SIZEOF_SIGNED_SHORT_ACCUM_win64 2
#define TARG_SIZEOF_SIGNED_SHORT_FRACT_win64 1
#define TARG_SIZEOF_UNSIGNED_ACCUM_win64 4
#define TARG_SIZEOF_UNSIGNED_FRACT_win64 2
#define TARG_SIZEOF_UNSIGNED_LONG_ACCUM_win64 8
#define TARG_SIZEOF_UNSIGNED_LONG_FRACT_win64 4
#define TARG_SIZEOF_UNSIGNED_SHORT_ACCUM_win64 2
#define TARG_SIZEOF_UNSIGNED_SHORT_FRACT_win64 1
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO_win64 8
#define TARG_SIZE_T_INT_KIND_win64 ((an_integer_kind)ik_unsigned_long_long)
#define TARG_SIZE_T_MAX_win64 ((a_targ_size_t)(9223372036854775807LL * 2ULL + 1))
#define TARG_SSIZE_T_INT_KIND_win64 ((an_integer_kind)ik_long_long)
#define TARG_SUPPORTS_ARM32_win64 0
#define TARG_SUPPORTS_ARM64_win64 0
#define TARG_SUPPORTS_RISCV32_win64 0
#define TARG_SUPPORTS_RISCV64_win64 0
#define TARG_SUPPORTS_X86_64_win64 1
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE_win64 0
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_win64 0
#define TARG_UNWIND_WORD_MODE_win64 ((a_type_mode_kind)tmk_SI)
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES_win64 1
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS_win64 1
#define TARG_VAR_HANDLE_INT_KIND_win64 ((an_integer_kind)ik_unsigned_short)
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND_win64 ((an_integer_kind)ik_short)
#define TARG_WCHAR_T_INT_KIND_win64 ((an_integer_kind)ik_unsigned_short)
#define TARG_WINT_T_INT_KIND_win64 ((an_integer_kind)ik_unsigned_short)
#define TARG_WORD_MODE_win64 ((a_type_mode_kind)tmk_SI)
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_win64 1
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT_win64 4

/* Target configuration: win32 */
#define TARGET_CONFIGURATION_3 win32
#define TARG_ALIGNOF_DOUBLE_win32 8
#define TARG_ALIGNOF_FAR_POINTER_win32 4
#define TARG_ALIGNOF_FLOAT_win32 4
#define TARG_ALIGNOF_FLOAT128_win32 16
#define TARG_ALIGNOF_FLOAT80_win32 4
#define TARG_ALIGNOF_INT_win32 4
#define TARG_ALIGNOF_INT128_win32 16
#define TARG_ALIGNOF_LONG_win32 4
#define TARG_ALIGNOF_LONG_DOUBLE_win32 8
#define TARG_ALIGNOF_LONG_LONG_win32 8
#define TARG_ALIGNOF_NEAR_POINTER_win32 2
#define TARG_ALIGNOF_POINTER_win32 4
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER_win32 4
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION_win32 4
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS_win32 4
#define TARG_ALIGNOF_SHORT_win32 2
#define TARG_ALIGNOF_SIGNED_ACCUM_win32 4
#define TARG_ALIGNOF_SIGNED_FRACT_win32 2
#define TARG_ALIGNOF_SIGNED_LONG_ACCUM_win32 8
#define TARG_ALIGNOF_SIGNED_LONG_FRACT_win32 4
#define TARG_ALIGNOF_SIGNED_SHORT_ACCUM_win32 2
#define TARG_ALIGNOF_SIGNED_SHORT_FRACT_win32 1
#define TARG_ALIGNOF_UNSIGNED_ACCUM_win32 4
#define TARG_ALIGNOF_UNSIGNED_FRACT_win32 2
#define TARG_ALIGNOF_UNSIGNED_LONG_ACCUM_win32 8
#define TARG_ALIGNOF_UNSIGNED_LONG_FRACT_win32 4
#define TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM_win32 2
#define TARG_ALIGNOF_UNSIGNED_SHORT_FRACT_win32 1
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO_win32 4
#define TARG_ALL_POINTERS_SAME_SIZE_win32 1
#define TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT_win32 0
#define TARG_BIT_FIELD_CONTAINER_SIZE_win32 (-1)
#define TARG_BOOL_INT_KIND_win32 ((an_integer_kind)ik_char)
#define TARG_C_BOOL_INT_KIND_win32 ((an_integer_kind)ik_unsigned_char)
#define TARG_CHAR16_T_INT_KIND_win32 ((an_integer_kind)ik_unsigned_short)
#define TARG_CHAR32_T_INT_KIND_win32 ((an_integer_kind)ik_unsigned_int)
#define TARG_CHAR_BIT_win32 8
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT_win32 1
#define TARG_DBL_MANT_DIG_win32 53
#define TARG_DBL_MAX_EXP_win32 1024
#define TARG_DBL_MIN_EXP_win32 (-1021)
#define TARG_DEFAULT_NEW_ALIGNMENT_win32 8
#define TARG_DELTA_INT_KIND_win32 ((an_integer_kind)ik_short)
#define TARG_DOUBLE_FIELD_ALIGNMENT_win32 8
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES_win32 0
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED_win32 0
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT_win32 0
#define TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE_win32 1
#define TARG_FLOAT_FIELD_ALIGNMENT_win32 4
#define TARG_FLOAT128_FIELD_ALIGNMENT_win32 16
#define TARG_FLOAT80_FIELD_ALIGNMENT_win32 4
#define TARG_FLT_MANT_DIG_win32 24
#define TARG_FLT_MAX_EXP_win32 128
#define TARG_FLT_MIN_EXP_win32 (-125)
#define TARG_FLT128_MANT_DIG_win32 113
#define TARG_FLT128_MAX_EXP_win32 (16384)
#define TARG_FLT128_MIN_EXP_win32 (-16381)
#define TARG_FLT80_MANT_DIG_win32 64
#define TARG_FLT80_MAX_EXP_win32 (16384)
#define TARG_FLT80_MIN_EXP_win32 (-16381)
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED_win32 1
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM_win32 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT_win32 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM_win32 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT_win32 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM_win32 7
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT_win32 7
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM_win32 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT_win32 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM_win32 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT_win32 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM_win32 8
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT_win32 8
#define TARG_HAS_SIGNED_CHARS_win32 1
#define TARG_HOST_STRING_CHAR_BIT_win32 8
#define TARG_IA64_ABI_USE_GUARD_ACQUIRE_RELEASE_win32 1
#define TARG_IA64_ABI_USE_INT_STATIC_INIT_GUARD_win32 0
#define TARG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES_win32 0
#define TARG_IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR_win32 0
#define TARG_IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS_win32 0
#define TARG_IA64_ABI_VARIANT_KEY_FUNCTION_win32 0
#define TARG_IA64_VTABLE_ENTRY_INT_KIND_win32 ((an_integer_kind)ik_int)
#define TARG_INT_FIELD_ALIGNMENT_win32 4
#define TARG_INT128_FIELD_ALIGNMENT_win32 16
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT_win32 0
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND_win32 ((a_float_kind)fk_long_double)
#define TARG_JMP_BUF_ELEMENT_INT_KIND_win32 ((an_integer_kind)ik_int)
#define TARG_JMP_BUF_NUM_ELEMENTS_win32 16
#define TARG_SETJMP_FUNC_win32 "setjmp"
#define TARG_LDBL_MANT_DIG_win32 53
#define TARG_LDBL_MAX_EXP_win32 1024
#define TARG_LDBL_MIN_EXP_win32 (-1021)
#define TARG_LIBGCC_CMP_RETURN_MODE_win32 ((a_type_mode_kind)tmk_SI)
#define TARG_LIBGCC_SHIFT_COUNT_MODE_win32 ((a_type_mode_kind)tmk_SI)
#define TARG_LITTLE_ENDIAN_win32 1
#define TARG_LONG_DOUBLE_FIELD_ALIGNMENT_win32 8
#define TARG_LONG_FIELD_ALIGNMENT_win32 4
#define TARG_LONG_LONG_FIELD_ALIGNMENT_win32 8
#define TARG_MAXIMUM_INTRINSIC_ALIGNMENT_win32 8
#define TARG_MAX_BASE_CLASS_OFFSET_win32 0
#define TARG_MAX_CLASS_OBJECT_SIZE_win32 0
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION_win32 1
#define TARG_MICROSOFT_PTR_TO_MEMBER_SIZING_win32 BACK_END_IS_CP_GEN_BE
#define TARG_MINIMUM_STRUCT_ALIGNMENT_win32 1
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED_win32 0
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT_win32 1
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE_win32 1
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED_win32 ( !1)
#define TARG_POINTER_MODE_win32 ((a_type_mode_kind)tmk_SI)
#define TARG_PTRDIFF_T_INT_KIND_win32 ((an_integer_kind)ik_int)
#define TARG_REGION_NUMBER_INT_KIND_win32 ((an_integer_kind)ik_unsigned_short)
#define TARG_ETS_FLAG_TYPE_INT_KIND_win32 ((an_integer_kind)ik_unsigned_int)
#define TARG_REUSE_TAIL_PADDING_win32 1
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC_win32 1
#define TARG_RUNTIME_ELEM_COUNT_INT_KIND_win32 ((an_integer_kind)ik_int)
#define TARG_SHORT_FIELD_ALIGNMENT_win32 2
#define TARG_SIZEOF_DOUBLE_win32 8
#define TARG_SIZEOF_FAR_POINTER_win32 4
#define TARG_SIZEOF_FLOAT_win32 4
#define TARG_SIZEOF_FLOAT128_win32 16
#define TARG_SIZEOF_FLOAT80_win32 12
#define TARG_SIZEOF_INT_win32 4
#define TARG_SIZEOF_INT128_win32 16
#define TARG_SIZEOF_LARGEST_FIXED_POINT_win32 8
#define TARG_SIZEOF_LARGEST_ATOMIC_win32 8
#define TARG_SIZEOF_LONG_win32 4
#define TARG_SIZEOF_LONG_DOUBLE_win32 8
#define TARG_SIZEOF_LONG_LONG_win32 8
#define TARG_SIZEOF_NEAR_POINTER_win32 2
#define TARG_SIZEOF_POINTER_win32 4
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER_win32 4
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION_win32 ((((2*2+4-1)/4)+1)* 4)
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS_win32 4
#define TARG_SIZEOF_SHORT_win32 2
#define TARG_SIZEOF_SIGNED_ACCUM_win32 4
#define TARG_SIZEOF_SIGNED_FRACT_win32 2
#define TARG_SIZEOF_SIGNED_LONG_ACCUM_win32 8
#define TARG_SIZEOF_SIGNED_LONG_FRACT_win32 4
#define TARG_SIZEOF_SIGNED_SHORT_ACCUM_win32 2
#define TARG_SIZEOF_SIGNED_SHORT_FRACT_win32 1
#define TARG_SIZEOF_UNSIGNED_ACCUM_win32 4
#define TARG_SIZEOF_UNSIGNED_FRACT_win32 2
#define TARG_SIZEOF_UNSIGNED_LONG_ACCUM_win32 8
#define TARG_SIZEOF_UNSIGNED_LONG_FRACT_win32 4
#define TARG_SIZEOF_UNSIGNED_SHORT_ACCUM_win32 2
#define TARG_SIZEOF_UNSIGNED_SHORT_FRACT_win32 1
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO_win32 4
#define TARG_SIZE_T_INT_KIND_win32 ((an_integer_kind)ik_unsigned_int)
#define TARG_SIZE_T_MAX_win32 ((a_targ_size_t)0xffffffff)
#define TARG_SSIZE_T_INT_KIND_win32 ((an_integer_kind)ik_int)
#define TARG_SUPPORTS_ARM32_win32 0
#define TARG_SUPPORTS_ARM64_win32 0
#define TARG_SUPPORTS_RISCV32_win32 0
#define TARG_SUPPORTS_RISCV64_win32 0
#define TARG_SUPPORTS_X86_64_win32 0
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE_win32 0
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_win32 1
#define TARG_UNWIND_WORD_MODE_win32 ((a_type_mode_kind)tmk_SI)
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES_win32 1
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS_win32 1
#define TARG_VAR_HANDLE_INT_KIND_win32 ((an_integer_kind)ik_unsigned_short)
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND_win32 ((an_integer_kind)ik_short)
#define TARG_WCHAR_T_INT_KIND_win32 ((an_integer_kind)ik_unsigned_short)
#define TARG_WINT_T_INT_KIND_win32 ((an_integer_kind)ik_unsigned_short)
#define TARG_WORD_MODE_win32 ((a_type_mode_kind)tmk_SI)
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_win32 1
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT_win32 (-1)

/* Target configuration: linux_aarch64 */
#define TARGET_CONFIGURATION_4 linux_aarch64
#define TARG_ALIGNOF_DOUBLE_linux_aarch64 8
#define TARG_ALIGNOF_FAR_POINTER_linux_aarch64 4
#define TARG_ALIGNOF_FLOAT_linux_aarch64 4
#define TARG_ALIGNOF_FLOAT128_linux_aarch64 16
#define TARG_ALIGNOF_FLOAT80_linux_aarch64 16
#define TARG_ALIGNOF_INT_linux_aarch64 4
#define TARG_ALIGNOF_INT128_linux_aarch64 16
#define TARG_ALIGNOF_LONG_linux_aarch64 8
#define TARG_ALIGNOF_LONG_DOUBLE_linux_aarch64 16
#define TARG_ALIGNOF_LONG_LONG_linux_aarch64 8
#define TARG_ALIGNOF_NEAR_POINTER_linux_aarch64 2
#define TARG_ALIGNOF_POINTER_linux_aarch64 8
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER_linux_aarch64 8
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION_linux_aarch64 8
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_aarch64 8
#define TARG_ALIGNOF_SHORT_linux_aarch64 2
#define TARG_ALIGNOF_SIGNED_ACCUM_linux_aarch64 4
#define TARG_ALIGNOF_SIGNED_FRACT_linux_aarch64 2
#define TARG_ALIGNOF_SIGNED_LONG_ACCUM_linux_aarch64 8
#define TARG_ALIGNOF_SIGNED_LONG_FRACT_linux_aarch64 4
#define TARG_ALIGNOF_SIGNED_SHORT_ACCUM_linux_aarch64 2
#define TARG_ALIGNOF_SIGNED_SHORT_FRACT_linux_aarch64 1
#define TARG_ALIGNOF_UNSIGNED_ACCUM_linux_aarch64 4
#define TARG_ALIGNOF_UNSIGNED_FRACT_linux_aarch64 2
#define TARG_ALIGNOF_UNSIGNED_LONG_ACCUM_linux_aarch64 8
#define TARG_ALIGNOF_UNSIGNED_LONG_FRACT_linux_aarch64 4
#define TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM_linux_aarch64 2
#define TARG_ALIGNOF_UNSIGNED_SHORT_FRACT_linux_aarch64 1
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO_linux_aarch64 8
#define TARG_ALL_POINTERS_SAME_SIZE_linux_aarch64 1
#define TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT_linux_aarch64 1
#define TARG_BIT_FIELD_CONTAINER_SIZE_linux_aarch64 (-1)
#define TARG_BOOL_INT_KIND_linux_aarch64 ((an_integer_kind)ik_char)
#define TARG_C_BOOL_INT_KIND_linux_aarch64 ((an_integer_kind)ik_unsigned_char)
#define TARG_CHAR16_T_INT_KIND_linux_aarch64 ((an_integer_kind)ik_unsigned_short)
#define TARG_CHAR32_T_INT_KIND_linux_aarch64 ((an_integer_kind)ik_unsigned_int)
#define TARG_CHAR_BIT_linux_aarch64 8
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT_linux_aarch64 1
#define TARG_DBL_MANT_DIG_linux_aarch64 53
#define TARG_DBL_MAX_EXP_linux_aarch64 1024
#define TARG_DBL_MIN_EXP_linux_aarch64 (-1021)
#define TARG_DEFAULT_NEW_ALIGNMENT_linux_aarch64 16
#define TARG_DELTA_INT_KIND_linux_aarch64 ((an_integer_kind)ik_short)
#define TARG_DOUBLE_FIELD_ALIGNMENT_linux_aarch64 8
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES_linux_aarch64 0
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED_linux_aarch64 0
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT_linux_aarch64 0
#define TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE_linux_aarch64 1
#define TARG_FLOAT_FIELD_ALIGNMENT_linux_aarch64 4
#define TARG_FLOAT128_FIELD_ALIGNMENT_linux_aarch64 16
#define TARG_FLOAT80_FIELD_ALIGNMENT_linux_aarch64 16
#define TARG_FLT_MANT_DIG_linux_aarch64 24
#define TARG_FLT_MAX_EXP_linux_aarch64 128
#define TARG_FLT_MIN_EXP_linux_aarch64 (-125)
#define TARG_FLT128_MANT_DIG_linux_aarch64 113
#define TARG_FLT128_MAX_EXP_linux_aarch64 (16384)
#define TARG_FLT128_MIN_EXP_linux_aarch64 (-16381)
#define TARG_FLT80_MANT_DIG_linux_aarch64 64
#define TARG_FLT80_MAX_EXP_linux_aarch64 (16384)
#define TARG_FLT80_MIN_EXP_linux_aarch64 (-16381)
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED_linux_aarch64 1
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM_linux_aarch64 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT_linux_aarch64 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM_linux_aarch64 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT_linux_aarch64 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM_linux_aarch64 7
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT_linux_aarch64 7
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM_linux_aarch64 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT_linux_aarch64 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM_linux_aarch64 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT_linux_aarch64 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM_linux_aarch64 8
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT_linux_aarch64 8
#define TARG_HAS_SIGNED_CHARS_linux_aarch64 1
#define TARG_HOST_STRING_CHAR_BIT_linux_aarch64 8
#define TARG_IA64_ABI_USE_GUARD_ACQUIRE_RELEASE_linux_aarch64 1
#define TARG_IA64_ABI_USE_INT_STATIC_INIT_GUARD_linux_aarch64 0
#define TARG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES_linux_aarch64 0
#define TARG_IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR_linux_aarch64 0
#define TARG_IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS_linux_aarch64 0
#define TARG_IA64_ABI_VARIANT_KEY_FUNCTION_linux_aarch64 0
#define TARG_IA64_VTABLE_ENTRY_INT_KIND_linux_aarch64 ((an_integer_kind)ik_long)
#define TARG_INT_FIELD_ALIGNMENT_linux_aarch64 4
#define TARG_INT128_FIELD_ALIGNMENT_linux_aarch64 16
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT_linux_aarch64 0
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND_linux_aarch64 ((a_float_kind)fk_long_double)
#define TARG_JMP_BUF_ELEMENT_INT_KIND_linux_aarch64 ((an_integer_kind)ik_long)
#define TARG_JMP_BUF_NUM_ELEMENTS_linux_aarch64 25
#define TARG_SETJMP_FUNC_linux_aarch64 "_setjmp"
#define TARG_LDBL_MANT_DIG_linux_aarch64 64
#define TARG_LDBL_MAX_EXP_linux_aarch64 16384
#define TARG_LDBL_MIN_EXP_linux_aarch64 (-16381)
#define TARG_LIBGCC_CMP_RETURN_MODE_linux_aarch64 ((a_type_mode_kind)tmk_DI)
#define TARG_LIBGCC_SHIFT_COUNT_MODE_linux_aarch64 ((a_type_mode_kind)tmk_DI)
#define TARG_LITTLE_ENDIAN_linux_aarch64 1
#define TARG_LONG_DOUBLE_FIELD_ALIGNMENT_linux_aarch64 16
#define TARG_LONG_FIELD_ALIGNMENT_linux_aarch64 8
#define TARG_LONG_LONG_FIELD_ALIGNMENT_linux_aarch64 8
#define TARG_MAXIMUM_INTRINSIC_ALIGNMENT_linux_aarch64 16
#define TARG_MAX_BASE_CLASS_OFFSET_linux_aarch64 0
#define TARG_MAX_CLASS_OBJECT_SIZE_linux_aarch64 0
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION_linux_aarch64 0
#define TARG_MICROSOFT_PTR_TO_MEMBER_SIZING_linux_aarch64 BACK_END_IS_CP_GEN_BE
#define TARG_MINIMUM_STRUCT_ALIGNMENT_linux_aarch64 1
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED_linux_aarch64 1
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT_linux_aarch64 1
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE_linux_aarch64 1
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED_linux_aarch64 0
#define TARG_POINTER_MODE_linux_aarch64 ((a_type_mode_kind)tmk_DI)
#define TARG_PTRDIFF_T_INT_KIND_linux_aarch64 ((an_integer_kind)ik_long)
#define TARG_REGION_NUMBER_INT_KIND_linux_aarch64 ((an_integer_kind)ik_unsigned_short)
#define TARG_ETS_FLAG_TYPE_INT_KIND_linux_aarch64 ((an_integer_kind)ik_unsigned_int)
#define TARG_REUSE_TAIL_PADDING_linux_aarch64 1
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC_linux_aarch64 1
#define TARG_RUNTIME_ELEM_COUNT_INT_KIND_linux_aarch64 ((an_integer_kind)ik_long)
#define TARG_SHORT_FIELD_ALIGNMENT_linux_aarch64 2
#define TARG_SIZEOF_DOUBLE_linux_aarch64 8
#define TARG_SIZEOF_FAR_POINTER_linux_aarch64 4
#define TARG_SIZEOF_FLOAT_linux_aarch64 4
#define TARG_SIZEOF_FLOAT128_linux_aarch64 16
#define TARG_SIZEOF_FLOAT80_linux_aarch64 12
#define TARG_SIZEOF_INT_linux_aarch64 4
#define TARG_SIZEOF_INT128_linux_aarch64 16
#define TARG_SIZEOF_LARGEST_FIXED_POINT_linux_aarch64 8
#define TARG_SIZEOF_LARGEST_ATOMIC_linux_aarch64 16
#define TARG_SIZEOF_LONG_linux_aarch64 8
#define TARG_SIZEOF_LONG_DOUBLE_linux_aarch64 16
#define TARG_SIZEOF_LONG_LONG_linux_aarch64 8
#define TARG_SIZEOF_NEAR_POINTER_linux_aarch64 2
#define TARG_SIZEOF_POINTER_linux_aarch64 8
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER_linux_aarch64 8
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION_linux_aarch64 ((((2*2+8-1)/8)+1)* 8)
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_aarch64 8
#define TARG_SIZEOF_SHORT_linux_aarch64 2
#define TARG_SIZEOF_SIGNED_ACCUM_linux_aarch64 4
#define TARG_SIZEOF_SIGNED_FRACT_linux_aarch64 2
#define TARG_SIZEOF_SIGNED_LONG_ACCUM_linux_aarch64 8
#define TARG_SIZEOF_SIGNED_LONG_FRACT_linux_aarch64 4
#define TARG_SIZEOF_SIGNED_SHORT_ACCUM_linux_aarch64 2
#define TARG_SIZEOF_SIGNED_SHORT_FRACT_linux_aarch64 1
#define TARG_SIZEOF_UNSIGNED_ACCUM_linux_aarch64 4
#define TARG_SIZEOF_UNSIGNED_FRACT_linux_aarch64 2
#define TARG_SIZEOF_UNSIGNED_LONG_ACCUM_linux_aarch64 8
#define TARG_SIZEOF_UNSIGNED_LONG_FRACT_linux_aarch64 4
#define TARG_SIZEOF_UNSIGNED_SHORT_ACCUM_linux_aarch64 2
#define TARG_SIZEOF_UNSIGNED_SHORT_FRACT_linux_aarch64 1
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO_linux_aarch64 8
#define TARG_SIZE_T_INT_KIND_linux_aarch64 ((an_integer_kind)ik_unsigned_long)
#define TARG_SIZE_T_MAX_linux_aarch64 ((a_targ_size_t)(9223372036854775807ULL * 2ULL + 1ULL))
#define TARG_SSIZE_T_INT_KIND_linux_aarch64 ((an_integer_kind)ik_long)
#define TARG_SUPPORTS_ARM32_linux_aarch64 0
#define TARG_SUPPORTS_ARM64_linux_aarch64 1
#define TARG_SUPPORTS_RISCV32_linux_aarch64 0
#define TARG_SUPPORTS_RISCV64_linux_aarch64 0
#define TARG_SUPPORTS_X86_64_linux_aarch64 0
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE_linux_aarch64 0
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_aarch64 0
#define TARG_UNWIND_WORD_MODE_linux_aarch64 ((a_type_mode_kind)tmk_DI)
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES_linux_aarch64 1
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS_linux_aarch64 1
#define TARG_VAR_HANDLE_INT_KIND_linux_aarch64 ((an_integer_kind)ik_unsigned_short)
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND_linux_aarch64 ((an_integer_kind)ik_short)
#define TARG_WCHAR_T_INT_KIND_linux_aarch64 ((an_integer_kind)ik_int)
#define TARG_WINT_T_INT_KIND_linux_aarch64 ((an_integer_kind)ik_unsigned_int)
#define TARG_WORD_MODE_linux_aarch64 ((a_type_mode_kind)tmk_DI)
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_aarch64 1
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT_linux_aarch64 4

/* Target configuration: linux_armv7 */
#define TARGET_CONFIGURATION_5 linux_armv7
#define TARG_ALIGNOF_DOUBLE_linux_armv7 8
#define TARG_ALIGNOF_FAR_POINTER_linux_armv7 4
#define TARG_ALIGNOF_FLOAT_linux_armv7 4
#define TARG_ALIGNOF_FLOAT128_linux_armv7 16
#define TARG_ALIGNOF_FLOAT80_linux_armv7 16
#define TARG_ALIGNOF_INT_linux_armv7 4
#define TARG_ALIGNOF_INT128_linux_armv7 16
#define TARG_ALIGNOF_LONG_linux_armv7 4
#define TARG_ALIGNOF_LONG_DOUBLE_linux_armv7 8
#define TARG_ALIGNOF_LONG_LONG_linux_armv7 8
#define TARG_ALIGNOF_NEAR_POINTER_linux_armv7 2
#define TARG_ALIGNOF_POINTER_linux_armv7 4
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER_linux_armv7 4
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION_linux_armv7 4
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_armv7 4
#define TARG_ALIGNOF_SHORT_linux_armv7 2
#define TARG_ALIGNOF_SIGNED_ACCUM_linux_armv7 4
#define TARG_ALIGNOF_SIGNED_FRACT_linux_armv7 2
#define TARG_ALIGNOF_SIGNED_LONG_ACCUM_linux_armv7 8
#define TARG_ALIGNOF_SIGNED_LONG_FRACT_linux_armv7 4
#define TARG_ALIGNOF_SIGNED_SHORT_ACCUM_linux_armv7 2
#define TARG_ALIGNOF_SIGNED_SHORT_FRACT_linux_armv7 1
#define TARG_ALIGNOF_UNSIGNED_ACCUM_linux_armv7 4
#define TARG_ALIGNOF_UNSIGNED_FRACT_linux_armv7 2
#define TARG_ALIGNOF_UNSIGNED_LONG_ACCUM_linux_armv7 8
#define TARG_ALIGNOF_UNSIGNED_LONG_FRACT_linux_armv7 4
#define TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM_linux_armv7 2
#define TARG_ALIGNOF_UNSIGNED_SHORT_FRACT_linux_armv7 1
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO_linux_armv7 4
#define TARG_ALL_POINTERS_SAME_SIZE_linux_armv7 1
#define TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT_linux_armv7 1
#define TARG_BIT_FIELD_CONTAINER_SIZE_linux_armv7 (-1)
#define TARG_BOOL_INT_KIND_linux_armv7 ((an_integer_kind)ik_char)
#define TARG_C_BOOL_INT_KIND_linux_armv7 ((an_integer_kind)ik_unsigned_char)
#define TARG_CHAR16_T_INT_KIND_linux_armv7 ((an_integer_kind)ik_unsigned_short)
#define TARG_CHAR32_T_INT_KIND_linux_armv7 ((an_integer_kind)ik_unsigned_int)
#define TARG_CHAR_BIT_linux_armv7 8
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT_linux_armv7 1
#define TARG_DBL_MANT_DIG_linux_armv7 53
#define TARG_DBL_MAX_EXP_linux_armv7 1024
#define TARG_DBL_MIN_EXP_linux_armv7 (-1021)
#define TARG_DEFAULT_NEW_ALIGNMENT_linux_armv7 16
#define TARG_DELTA_INT_KIND_linux_armv7 ((an_integer_kind)ik_short)
#define TARG_DOUBLE_FIELD_ALIGNMENT_linux_armv7 8
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES_linux_armv7 0
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED_linux_armv7 0
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT_linux_armv7 0
#define TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE_linux_armv7 1
#define TARG_FLOAT_FIELD_ALIGNMENT_linux_armv7 4
#define TARG_FLOAT128_FIELD_ALIGNMENT_linux_armv7 16
#define TARG_FLOAT80_FIELD_ALIGNMENT_linux_armv7 16
#define TARG_FLT_MANT_DIG_linux_armv7 24
#define TARG_FLT_MAX_EXP_linux_armv7 128
#define TARG_FLT_MIN_EXP_linux_armv7 (-125)
#define TARG_FLT128_MANT_DIG_linux_armv7 113
#define TARG_FLT128_MAX_EXP_linux_armv7 (16384)
#define TARG_FLT128_MIN_EXP_linux_armv7 (-16381)
#define TARG_FLT80_MANT_DIG_linux_armv7 64
#define TARG_FLT80_MAX_EXP_linux_armv7 (16384)
#define TARG_FLT80_MIN_EXP_linux_armv7 (-16381)
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED_linux_armv7 1
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM_linux_armv7 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT_linux_armv7 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM_linux_armv7 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT_linux_armv7 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM_linux_armv7 7
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT_linux_armv7 7
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM_linux_armv7 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT_linux_armv7 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM_linux_armv7 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT_linux_armv7 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM_linux_armv7 8
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT_linux_armv7 8
#define TARG_HAS_SIGNED_CHARS_linux_armv7 1
#define TARG_HOST_STRING_CHAR_BIT_linux_armv7 8
#define TARG_IA64_ABI_USE_GUARD_ACQUIRE_RELEASE_linux_armv7 1
#define TARG_IA64_ABI_USE_INT_STATIC_INIT_GUARD_linux_armv7 0
#define TARG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES_linux_armv7 0
#define TARG_IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR_linux_armv7 0
#define TARG_IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS_linux_armv7 0
#define TARG_IA64_ABI_VARIANT_KEY_FUNCTION_linux_armv7 0
#define TARG_IA64_VTABLE_ENTRY_INT_KIND_linux_armv7 ((an_integer_kind)ik_int)
#define TARG_INT_FIELD_ALIGNMENT_linux_armv7 4
#define TARG_INT128_FIELD_ALIGNMENT_linux_armv7 16
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT_linux_armv7 0
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND_linux_armv7 ((a_float_kind)fk_long_double)
#define TARG_JMP_BUF_ELEMENT_INT_KIND_linux_armv7 ((an_integer_kind)ik_int)
#define TARG_JMP_BUF_NUM_ELEMENTS_linux_armv7 25
#define TARG_SETJMP_FUNC_linux_armv7 "_setjmp"
#define TARG_LDBL_MANT_DIG_linux_armv7 53
#define TARG_LDBL_MAX_EXP_linux_armv7 1024
#define TARG_LDBL_MIN_EXP_linux_armv7 (-1021)
#define TARG_LIBGCC_CMP_RETURN_MODE_linux_armv7 ((a_type_mode_kind)tmk_SI)
#define TARG_LIBGCC_SHIFT_COUNT_MODE_linux_armv7 ((a_type_mode_kind)tmk_SI)
#define TARG_LITTLE_ENDIAN_linux_armv7 1
#define TARG_LONG_DOUBLE_FIELD_ALIGNMENT_linux_armv7 8
#define TARG_LONG_FIELD_ALIGNMENT_linux_armv7 4
#define TARG_LONG_LONG_FIELD_ALIGNMENT_linux_armv7 8
#define TARG_MAXIMUM_INTRINSIC_ALIGNMENT_linux_armv7 16
#define TARG_MAX_BASE_CLASS_OFFSET_linux_armv7 0
#define TARG_MAX_CLASS_OBJECT_SIZE_linux_armv7 0
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION_linux_armv7 0
#define TARG_MICROSOFT_PTR_TO_MEMBER_SIZING_linux_armv7 BACK_END_IS_CP_GEN_BE
#define TARG_MINIMUM_STRUCT_ALIGNMENT_linux_armv7 1
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED_linux_armv7 1
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT_linux_armv7 1
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE_linux_armv7 1
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED_linux_armv7 0
#define TARG_POINTER_MODE_linux_armv7 ((a_type_mode_kind)tmk_SI)
#define TARG_PTRDIFF_T_INT_KIND_linux_armv7 ((an_integer_kind)ik_int)
#define TARG_REGION_NUMBER_INT_KIND_linux_armv7 ((an_integer_kind)ik_unsigned_short)
#define TARG_ETS_FLAG_TYPE_INT_KIND_linux_armv7 ((an_integer_kind)ik_unsigned_int)
#define TARG_REUSE_TAIL_PADDING_linux_armv7 1
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC_linux_armv7 1
#define TARG_RUNTIME_ELEM_COUNT_INT_KIND_linux_armv7 ((an_integer_kind)ik_int)
#define TARG_SHORT_FIELD_ALIGNMENT_linux_armv7 2
#define TARG_SIZEOF_DOUBLE_linux_armv7 8
#define TARG_SIZEOF_FAR_POINTER_linux_armv7 4
#define TARG_SIZEOF_FLOAT_linux_armv7 4
#define TARG_SIZEOF_FLOAT128_linux_armv7 16
#define TARG_SIZEOF_FLOAT80_linux_armv7 12
#define TARG_SIZEOF_INT_linux_armv7 4
#define TARG_SIZEOF_INT128_linux_armv7 16
#define TARG_SIZEOF_LARGEST_FIXED_POINT_linux_armv7 8
#define TARG_SIZEOF_LARGEST_ATOMIC_linux_armv7 8
#define TARG_SIZEOF_LONG_linux_armv7 4
#define TARG_SIZEOF_LONG_DOUBLE_linux_armv7 8
#define TARG_SIZEOF_LONG_LONG_linux_armv7 8
#define TARG_SIZEOF_NEAR_POINTER_linux_armv7 2
#define TARG_SIZEOF_POINTER_linux_armv7 4
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER_linux_armv7 4
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION_linux_armv7 ((((2*2+4-1)/4)+1)* 4)
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_armv7 4
#define TARG_SIZEOF_SHORT_linux_armv7 2
#define TARG_SIZEOF_SIGNED_ACCUM_linux_armv7 4
#define TARG_SIZEOF_SIGNED_FRACT_linux_armv7 2
#define TARG_SIZEOF_SIGNED_LONG_ACCUM_linux_armv7 8
#define TARG_SIZEOF_SIGNED_LONG_FRACT_linux_armv7 4
#define TARG_SIZEOF_SIGNED_SHORT_ACCUM_linux_armv7 2
#define TARG_SIZEOF_SIGNED_SHORT_FRACT_linux_armv7 1
#define TARG_SIZEOF_UNSIGNED_ACCUM_linux_armv7 4
#define TARG_SIZEOF_UNSIGNED_FRACT_linux_armv7 2
#define TARG_SIZEOF_UNSIGNED_LONG_ACCUM_linux_armv7 8
#define TARG_SIZEOF_UNSIGNED_LONG_FRACT_linux_armv7 4
#define TARG_SIZEOF_UNSIGNED_SHORT_ACCUM_linux_armv7 2
#define TARG_SIZEOF_UNSIGNED_SHORT_FRACT_linux_armv7 1
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO_linux_armv7 4
#define TARG_SIZE_T_INT_KIND_linux_armv7 ((an_integer_kind)ik_unsigned_int)
#define TARG_SIZE_T_MAX_linux_armv7 ((a_targ_size_t)(2147483647 * 2U + 1U))
#define TARG_SSIZE_T_INT_KIND_linux_armv7 ((an_integer_kind)ik_int)
#define TARG_SUPPORTS_ARM32_linux_armv7 1
#define TARG_SUPPORTS_ARM64_linux_armv7 0
#define TARG_SUPPORTS_RISCV32_linux_armv7 0
#define TARG_SUPPORTS_RISCV64_linux_armv7 0
#define TARG_SUPPORTS_X86_64_linux_armv7 0
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE_linux_armv7 0
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_armv7 0
#define TARG_UNWIND_WORD_MODE_linux_armv7 ((a_type_mode_kind)tmk_SI)
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES_linux_armv7 1
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS_linux_armv7 1
#define TARG_VAR_HANDLE_INT_KIND_linux_armv7 ((an_integer_kind)ik_unsigned_short)
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND_linux_armv7 ((an_integer_kind)ik_short)
#define TARG_WCHAR_T_INT_KIND_linux_armv7 ((an_integer_kind)ik_unsigned_int)
#define TARG_WINT_T_INT_KIND_linux_armv7 ((an_integer_kind)ik_unsigned_int)
#define TARG_WORD_MODE_linux_armv7 ((a_type_mode_kind)tmk_SI)
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_armv7 1
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT_linux_armv7 4

/* Target configuration: linux_riscv64 */
#define TARGET_CONFIGURATION_6 linux_riscv64
#define TARG_ALIGNOF_DOUBLE_linux_riscv64 8
#define TARG_ALIGNOF_FAR_POINTER_linux_riscv64 4
#define TARG_ALIGNOF_FLOAT_linux_riscv64 4
#define TARG_ALIGNOF_FLOAT128_linux_riscv64 16
#define TARG_ALIGNOF_FLOAT80_linux_riscv64 16
#define TARG_ALIGNOF_INT_linux_riscv64 4
#define TARG_ALIGNOF_INT128_linux_riscv64 16
#define TARG_ALIGNOF_LONG_linux_riscv64 8
#define TARG_ALIGNOF_LONG_DOUBLE_linux_riscv64 16
#define TARG_ALIGNOF_LONG_LONG_linux_riscv64 8
#define TARG_ALIGNOF_NEAR_POINTER_linux_riscv64 2
#define TARG_ALIGNOF_POINTER_linux_riscv64 8
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER_linux_riscv64 8
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION_linux_riscv64 8
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_riscv64 8
#define TARG_ALIGNOF_SHORT_linux_riscv64 2
#define TARG_ALIGNOF_SIGNED_ACCUM_linux_riscv64 4
#define TARG_ALIGNOF_SIGNED_FRACT_linux_riscv64 2
#define TARG_ALIGNOF_SIGNED_LONG_ACCUM_linux_riscv64 8
#define TARG_ALIGNOF_SIGNED_LONG_FRACT_linux_riscv64 4
#define TARG_ALIGNOF_SIGNED_SHORT_ACCUM_linux_riscv64 2
#define TARG_ALIGNOF_SIGNED_SHORT_FRACT_linux_riscv64 1
#define TARG_ALIGNOF_UNSIGNED_ACCUM_linux_riscv64 4
#define TARG_ALIGNOF_UNSIGNED_FRACT_linux_riscv64 2
#define TARG_ALIGNOF_UNSIGNED_LONG_ACCUM_linux_riscv64 8
#define TARG_ALIGNOF_UNSIGNED_LONG_FRACT_linux_riscv64 4
#define TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM_linux_riscv64 2
#define TARG_ALIGNOF_UNSIGNED_SHORT_FRACT_linux_riscv64 1
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO_linux_riscv64 8
#define TARG_ALL_POINTERS_SAME_SIZE_linux_riscv64 1
#define TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT_linux_riscv64 1
#define TARG_BIT_FIELD_CONTAINER_SIZE_linux_riscv64 (-1)
#define TARG_BOOL_INT_KIND_linux_riscv64 ((an_integer_kind)ik_char)
#define TARG_C_BOOL_INT_KIND_linux_riscv64 ((an_integer_kind)ik_unsigned_char)
#define TARG_CHAR16_T_INT_KIND_linux_riscv64 ((an_integer_kind)ik_unsigned_short)
#define TARG_CHAR32_T_INT_KIND_linux_riscv64 ((an_integer_kind)ik_unsigned_int)
#define TARG_CHAR_BIT_linux_riscv64 8
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT_linux_riscv64 1
#define TARG_DBL_MANT_DIG_linux_riscv64 53
#define TARG_DBL_MAX_EXP_linux_riscv64 1024
#define TARG_DBL_MIN_EXP_linux_riscv64 (-1021)
#define TARG_DEFAULT_NEW_ALIGNMENT_linux_riscv64 16
#define TARG_DELTA_INT_KIND_linux_riscv64 ((an_integer_kind)ik_short)
#define TARG_DOUBLE_FIELD_ALIGNMENT_linux_riscv64 8
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES_linux_riscv64 0
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED_linux_riscv64 0
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT_linux_riscv64 0
#define TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE_linux_riscv64 1
#define TARG_FLOAT_FIELD_ALIGNMENT_linux_riscv64 4
#define TARG_FLOAT128_FIELD_ALIGNMENT_linux_riscv64 16
#define TARG_FLOAT80_FIELD_ALIGNMENT_linux_riscv64 16
#define TARG_FLT_MANT_DIG_linux_riscv64 24
#define TARG_FLT_MAX_EXP_linux_riscv64 128
#define TARG_FLT_MIN_EXP_linux_riscv64 (-125)
#define TARG_FLT128_MANT_DIG_linux_riscv64 113
#define TARG_FLT128_MAX_EXP_linux_riscv64 (16384)
#define TARG_FLT128_MIN_EXP_linux_riscv64 (-16381)
#define TARG_FLT80_MANT_DIG_linux_riscv64 64
#define TARG_FLT80_MAX_EXP_linux_riscv64 (16384)
#define TARG_FLT80_MIN_EXP_linux_riscv64 (-16381)
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED_linux_riscv64 1
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM_linux_riscv64 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT_linux_riscv64 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM_linux_riscv64 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT_linux_riscv64 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM_linux_riscv64 7
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT_linux_riscv64 7
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM_linux_riscv64 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT_linux_riscv64 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM_linux_riscv64 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT_linux_riscv64 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM_linux_riscv64 8
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT_linux_riscv64 8
#define TARG_HAS_SIGNED_CHARS_linux_riscv64 0
#define TARG_HOST_STRING_CHAR_BIT_linux_riscv64 8
#define TARG_IA64_ABI_USE_GUARD_ACQUIRE_RELEASE_linux_riscv64 1
#define TARG_IA64_ABI_USE_INT_STATIC_INIT_GUARD_linux_riscv64 0
#define TARG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES_linux_riscv64 0
#define TARG_IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR_linux_riscv64 0
#define TARG_IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS_linux_riscv64 0
#define TARG_IA64_ABI_VARIANT_KEY_FUNCTION_linux_riscv64 0
#define TARG_IA64_VTABLE_ENTRY_INT_KIND_linux_riscv64 ((an_integer_kind)ik_long)
#define TARG_INT_FIELD_ALIGNMENT_linux_riscv64 4
#define TARG_INT128_FIELD_ALIGNMENT_linux_riscv64 16
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT_linux_riscv64 0
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND_linux_riscv64 ((a_float_kind)fk_long_double)
#define TARG_JMP_BUF_ELEMENT_INT_KIND_linux_riscv64 ((an_integer_kind)ik_long)
#define TARG_JMP_BUF_NUM_ELEMENTS_linux_riscv64 43
#define TARG_SETJMP_FUNC_linux_riscv64 "_setjmp"
#define TARG_LDBL_MANT_DIG_linux_riscv64 113
#define TARG_LDBL_MAX_EXP_linux_riscv64 16384
#define TARG_LDBL_MIN_EXP_linux_riscv64 (-16381)
#define TARG_LIBGCC_CMP_RETURN_MODE_linux_riscv64 ((a_type_mode_kind)tmk_DI)
#define TARG_LIBGCC_SHIFT_COUNT_MODE_linux_riscv64 ((a_type_mode_kind)tmk_DI)
#define TARG_LITTLE_ENDIAN_linux_riscv64 1
#define TARG_LONG_DOUBLE_FIELD_ALIGNMENT_linux_riscv64 16
#define TARG_LONG_FIELD_ALIGNMENT_linux_riscv64 8
#define TARG_LONG_LONG_FIELD_ALIGNMENT_linux_riscv64 8
#define TARG_MAXIMUM_INTRINSIC_ALIGNMENT_linux_riscv64 16
#define TARG_MAX_BASE_CLASS_OFFSET_linux_riscv64 0
#define TARG_MAX_CLASS_OBJECT_SIZE_linux_riscv64 0
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION_linux_riscv64 0
#define TARG_MICROSOFT_PTR_TO_MEMBER_SIZING_linux_riscv64 BACK_END_IS_CP_GEN_BE
#define TARG_MINIMUM_STRUCT_ALIGNMENT_linux_riscv64 1
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED_linux_riscv64 1
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT_linux_riscv64 1
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE_linux_riscv64 1
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED_linux_riscv64 0
#define TARG_POINTER_MODE_linux_riscv64 ((a_type_mode_kind)tmk_DI)
#define TARG_PTRDIFF_T_INT_KIND_linux_riscv64 ((an_integer_kind)ik_long)
#define TARG_REGION_NUMBER_INT_KIND_linux_riscv64 ((an_integer_kind)ik_unsigned_short)
#define TARG_ETS_FLAG_TYPE_INT_KIND_linux_riscv64 ((an_integer_kind)ik_unsigned_int)
#define TARG_REUSE_TAIL_PADDING_linux_riscv64 1
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC_linux_riscv64 1
#define TARG_RUNTIME_ELEM_COUNT_INT_KIND_linux_riscv64 ((an_integer_kind)ik_long)
#define TARG_SHORT_FIELD_ALIGNMENT_linux_riscv64 2
#define TARG_SIZEOF_DOUBLE_linux_riscv64 8
#define TARG_SIZEOF_FAR_POINTER_linux_riscv64 4
#define TARG_SIZEOF_FLOAT_linux_riscv64 4
#define TARG_SIZEOF_FLOAT128_linux_riscv64 16
#define TARG_SIZEOF_FLOAT80_linux_riscv64 12
#define TARG_SIZEOF_INT_linux_riscv64 4
#define TARG_SIZEOF_INT128_linux_riscv64 16
#define TARG_SIZEOF_LARGEST_FIXED_POINT_linux_riscv64 8
#define TARG_SIZEOF_LARGEST_ATOMIC_linux_riscv64 16
#define TARG_SIZEOF_LONG_linux_riscv64 8
#define TARG_SIZEOF_LONG_DOUBLE_linux_riscv64 16
#define TARG_SIZEOF_LONG_LONG_linux_riscv64 8
#define TARG_SIZEOF_NEAR_POINTER_linux_riscv64 2
#define TARG_SIZEOF_POINTER_linux_riscv64 8
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER_linux_riscv64 8
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION_linux_riscv64 ((((2*2+8-1)/8)+1)* 8)
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_riscv64 8
#define TARG_SIZEOF_SHORT_linux_riscv64 2
#define TARG_SIZEOF_SIGNED_ACCUM_linux_riscv64 4
#define TARG_SIZEOF_SIGNED_FRACT_linux_riscv64 2
#define TARG_SIZEOF_SIGNED_LONG_ACCUM_linux_riscv64 8
#define TARG_SIZEOF_SIGNED_LONG_FRACT_linux_riscv64 4
#define TARG_SIZEOF_SIGNED_SHORT_ACCUM_linux_riscv64 2
#define TARG_SIZEOF_SIGNED_SHORT_FRACT_linux_riscv64 1
#define TARG_SIZEOF_UNSIGNED_ACCUM_linux_riscv64 4
#define TARG_SIZEOF_UNSIGNED_FRACT_linux_riscv64 2
#define TARG_SIZEOF_UNSIGNED_LONG_ACCUM_linux_riscv64 8
#define TARG_SIZEOF_UNSIGNED_LONG_FRACT_linux_riscv64 4
#define TARG_SIZEOF_UNSIGNED_SHORT_ACCUM_linux_riscv64 2
#define TARG_SIZEOF_UNSIGNED_SHORT_FRACT_linux_riscv64 1
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO_linux_riscv64 8
#define TARG_SIZE_T_INT_KIND_linux_riscv64 ((an_integer_kind)ik_unsigned_long)
#define TARG_SIZE_T_MAX_linux_riscv64 ((a_targ_size_t)(9223372036854775807ULL * 2ULL + 1ULL))
#define TARG_SSIZE_T_INT_KIND_linux_riscv64 ((an_integer_kind)ik_long)
#define TARG_SUPPORTS_ARM32_linux_riscv64 0
#define TARG_SUPPORTS_ARM64_linux_riscv64 0
#define TARG_SUPPORTS_RISCV32_linux_riscv64 0
#define TARG_SUPPORTS_RISCV64_linux_riscv64 1
#define TARG_SUPPORTS_X86_64_linux_riscv64 0
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE_linux_riscv64 0
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_riscv64 0
#define TARG_UNWIND_WORD_MODE_linux_riscv64 ((a_type_mode_kind)tmk_DI)
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES_linux_riscv64 1
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS_linux_riscv64 1
#define TARG_VAR_HANDLE_INT_KIND_linux_riscv64 ((an_integer_kind)ik_unsigned_short)
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND_linux_riscv64 ((an_integer_kind)ik_short)
#define TARG_WCHAR_T_INT_KIND_linux_riscv64 ((an_integer_kind)ik_int)
#define TARG_WINT_T_INT_KIND_linux_riscv64 ((an_integer_kind)ik_unsigned_int)
#define TARG_WORD_MODE_linux_riscv64 ((a_type_mode_kind)tmk_DI)
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_riscv64 1
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT_linux_riscv64 4

/* Target configuration: linux_riscv32 */
#define TARGET_CONFIGURATION_7 linux_riscv32
#define TARG_ALIGNOF_DOUBLE_linux_riscv32 8
#define TARG_ALIGNOF_FAR_POINTER_linux_riscv32 4
#define TARG_ALIGNOF_FLOAT_linux_riscv32 4
#define TARG_ALIGNOF_FLOAT128_linux_riscv32 16
#define TARG_ALIGNOF_FLOAT80_linux_riscv32 16
#define TARG_ALIGNOF_INT_linux_riscv32 4
#define TARG_ALIGNOF_INT128_linux_riscv32 16
#define TARG_ALIGNOF_LONG_linux_riscv32 4
#define TARG_ALIGNOF_LONG_DOUBLE_linux_riscv32 16
#define TARG_ALIGNOF_LONG_LONG_linux_riscv32 8
#define TARG_ALIGNOF_NEAR_POINTER_linux_riscv32 2
#define TARG_ALIGNOF_POINTER_linux_riscv32 4
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER_linux_riscv32 4
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION_linux_riscv32 4
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_riscv32 4
#define TARG_ALIGNOF_SHORT_linux_riscv32 2
#define TARG_ALIGNOF_SIGNED_ACCUM_linux_riscv32 4
#define TARG_ALIGNOF_SIGNED_FRACT_linux_riscv32 2
#define TARG_ALIGNOF_SIGNED_LONG_ACCUM_linux_riscv32 8
#define TARG_ALIGNOF_SIGNED_LONG_FRACT_linux_riscv32 4
#define TARG_ALIGNOF_SIGNED_SHORT_ACCUM_linux_riscv32 2
#define TARG_ALIGNOF_SIGNED_SHORT_FRACT_linux_riscv32 1
#define TARG_ALIGNOF_UNSIGNED_ACCUM_linux_riscv32 4
#define TARG_ALIGNOF_UNSIGNED_FRACT_linux_riscv32 2
#define TARG_ALIGNOF_UNSIGNED_LONG_ACCUM_linux_riscv32 8
#define TARG_ALIGNOF_UNSIGNED_LONG_FRACT_linux_riscv32 4
#define TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM_linux_riscv32 2
#define TARG_ALIGNOF_UNSIGNED_SHORT_FRACT_linux_riscv32 1
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO_linux_riscv32 4
#define TARG_ALL_POINTERS_SAME_SIZE_linux_riscv32 1
#define TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT_linux_riscv32 1
#define TARG_BIT_FIELD_CONTAINER_SIZE_linux_riscv32 (-1)
#define TARG_BOOL_INT_KIND_linux_riscv32 ((an_integer_kind)ik_char)
#define TARG_C_BOOL_INT_KIND_linux_riscv32 ((an_integer_kind)ik_unsigned_char)
#define TARG_CHAR16_T_INT_KIND_linux_riscv32 ((an_integer_kind)ik_unsigned_short)
#define TARG_CHAR32_T_INT_KIND_linux_riscv32 ((an_integer_kind)ik_unsigned_int)
#define TARG_CHAR_BIT_linux_riscv32 8
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT_linux_riscv32 1
#define TARG_DBL_MANT_DIG_linux_riscv32 53
#define TARG_DBL_MAX_EXP_linux_riscv32 1024
#define TARG_DBL_MIN_EXP_linux_riscv32 (-1021)
#define TARG_DEFAULT_NEW_ALIGNMENT_linux_riscv32 16
#define TARG_DELTA_INT_KIND_linux_riscv32 ((an_integer_kind)ik_short)
#define TARG_DOUBLE_FIELD_ALIGNMENT_linux_riscv32 8
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES_linux_riscv32 0
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED_linux_riscv32 0
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT_linux_riscv32 0
#define TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE_linux_riscv32 1
#define TARG_FLOAT_FIELD_ALIGNMENT_linux_riscv32 4
#define TARG_FLOAT128_FIELD_ALIGNMENT_linux_riscv32 16
#define TARG_FLOAT80_FIELD_ALIGNMENT_linux_riscv32 16
#define TARG_FLT_MANT_DIG_linux_riscv32 24
#define TARG_FLT_MAX_EXP_linux_riscv32 128
#define TARG_FLT_MIN_EXP_linux_riscv32 (-125)
#define TARG_FLT128_MANT_DIG_linux_riscv32 113
#define TARG_FLT128_MAX_EXP_linux_riscv32 (16384)
#define TARG_FLT128_MIN_EXP_linux_riscv32 (-16381)
#define TARG_FLT80_MANT_DIG_linux_riscv32 64
#define TARG_FLT80_MAX_EXP_linux_riscv32 (16384)
#define TARG_FLT80_MIN_EXP_linux_riscv32 (-16381)
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED_linux_riscv32 1
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM_linux_riscv32 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT_linux_riscv32 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM_linux_riscv32 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT_linux_riscv32 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM_linux_riscv32 7
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT_linux_riscv32 7
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM_linux_riscv32 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT_linux_riscv32 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM_linux_riscv32 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT_linux_riscv32 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM_linux_riscv32 8
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT_linux_riscv32 8
#define TARG_HAS_SIGNED_CHARS_linux_riscv32 0
#define TARG_HOST_STRING_CHAR_BIT_linux_riscv32 8
#define TARG_IA64_ABI_USE_GUARD_ACQUIRE_RELEASE_linux_riscv32 1
#define TARG_IA64_ABI_USE_INT_STATIC_INIT_GUARD_linux_riscv32 0
#define TARG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES_linux_riscv32 0
#define TARG_IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR_linux_riscv32 0
#define TARG_IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS_linux_riscv32 0
#define TARG_IA64_ABI_VARIANT_KEY_FUNCTION_linux_riscv32 0
#define TARG_IA64_VTABLE_ENTRY_INT_KIND_linux_riscv32 ((an_integer_kind)ik_int)
#define TARG_INT_FIELD_ALIGNMENT_linux_riscv32 4
#define TARG_INT128_FIELD_ALIGNMENT_linux_riscv32 16
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT_linux_riscv32 0
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND_linux_riscv32 ((a_float_kind)fk_long_double)
#define TARG_JMP_BUF_ELEMENT_INT_KIND_linux_riscv32 ((an_integer_kind)ik_long_long)
#define TARG_JMP_BUF_NUM_ELEMENTS_linux_riscv32 36
#define TARG_SETJMP_FUNC_linux_riscv32 "_setjmp"
#define TARG_LDBL_MANT_DIG_linux_riscv32 113
#define TARG_LDBL_MAX_EXP_linux_riscv32 16384
#define TARG_LDBL_MIN_EXP_linux_riscv32 (-16381)
#define TARG_LIBGCC_CMP_RETURN_MODE_linux_riscv32 ((a_type_mode_kind)tmk_SI)
#define TARG_LIBGCC_SHIFT_COUNT_MODE_linux_riscv32 ((a_type_mode_kind)tmk_SI)
#define TARG_LITTLE_ENDIAN_linux_riscv32 1
#define TARG_LONG_DOUBLE_FIELD_ALIGNMENT_linux_riscv32 16
#define TARG_LONG_FIELD_ALIGNMENT_linux_riscv32 4
#define TARG_LONG_LONG_FIELD_ALIGNMENT_linux_riscv32 8
#define TARG_MAXIMUM_INTRINSIC_ALIGNMENT_linux_riscv32 16
#define TARG_MAX_BASE_CLASS_OFFSET_linux_riscv32 0
#define TARG_MAX_CLASS_OBJECT_SIZE_linux_riscv32 0
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION_linux_riscv32 0
#define TARG_MICROSOFT_PTR_TO_MEMBER_SIZING_linux_riscv32 BACK_END_IS_CP_GEN_BE
#define TARG_MINIMUM_STRUCT_ALIGNMENT_linux_riscv32 1
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED_linux_riscv32 1
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT_linux_riscv32 1
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE_linux_riscv32 1
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED_linux_riscv32 0
#define TARG_POINTER_MODE_linux_riscv32 ((a_type_mode_kind)tmk_SI)
#define TARG_PTRDIFF_T_INT_KIND_linux_riscv32 ((an_integer_kind)ik_int)
#define TARG_REGION_NUMBER_INT_KIND_linux_riscv32 ((an_integer_kind)ik_unsigned_short)
#define TARG_ETS_FLAG_TYPE_INT_KIND_linux_riscv32 ((an_integer_kind)ik_unsigned_int)
#define TARG_REUSE_TAIL_PADDING_linux_riscv32 1
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC_linux_riscv32 1
#define TARG_RUNTIME_ELEM_COUNT_INT_KIND_linux_riscv32 ((an_integer_kind)ik_int)
#define TARG_SHORT_FIELD_ALIGNMENT_linux_riscv32 2
#define TARG_SIZEOF_DOUBLE_linux_riscv32 8
#define TARG_SIZEOF_FAR_POINTER_linux_riscv32 4
#define TARG_SIZEOF_FLOAT_linux_riscv32 4
#define TARG_SIZEOF_FLOAT128_linux_riscv32 16
#define TARG_SIZEOF_FLOAT80_linux_riscv32 12
#define TARG_SIZEOF_INT_linux_riscv32 4
#define TARG_SIZEOF_INT128_linux_riscv32 16
#define TARG_SIZEOF_LARGEST_FIXED_POINT_linux_riscv32 8
#define TARG_SIZEOF_LARGEST_ATOMIC_linux_riscv32 8
#define TARG_SIZEOF_LONG_linux_riscv32 4
#define TARG_SIZEOF_LONG_DOUBLE_linux_riscv32 16
#define TARG_SIZEOF_LONG_LONG_linux_riscv32 8
#define TARG_SIZEOF_NEAR_POINTER_linux_riscv32 2
#define TARG_SIZEOF_POINTER_linux_riscv32 4
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER_linux_riscv32 4
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION_linux_riscv32 ((((2*2+4-1)/4)+1)* 4)
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_riscv32 4
#define TARG_SIZEOF_SHORT_linux_riscv32 2
#define TARG_SIZEOF_SIGNED_ACCUM_linux_riscv32 4
#define TARG_SIZEOF_SIGNED_FRACT_linux_riscv32 2
#define TARG_SIZEOF_SIGNED_LONG_ACCUM_linux_riscv32 8
#define TARG_SIZEOF_SIGNED_LONG_FRACT_linux_riscv32 4
#define TARG_SIZEOF_SIGNED_SHORT_ACCUM_linux_riscv32 2
#define TARG_SIZEOF_SIGNED_SHORT_FRACT_linux_riscv32 1
#define TARG_SIZEOF_UNSIGNED_ACCUM_linux_riscv32 4
#define TARG_SIZEOF_UNSIGNED_FRACT_linux_riscv32 2
#define TARG_SIZEOF_UNSIGNED_LONG_ACCUM_linux_riscv32 8
#define TARG_SIZEOF_UNSIGNED_LONG_FRACT_linux_riscv32 4
#define TARG_SIZEOF_UNSIGNED_SHORT_ACCUM_linux_riscv32 2
#define TARG_SIZEOF_UNSIGNED_SHORT_FRACT_linux_riscv32 1
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO_linux_riscv32 4
#define TARG_SIZE_T_INT_KIND_linux_riscv32 ((an_integer_kind)ik_unsigned_int)
#define TARG_SIZE_T_MAX_linux_riscv32 ((a_targ_size_t)(2147483647 * 2U + 1U))
#define TARG_SSIZE_T_INT_KIND_linux_riscv32 ((an_integer_kind)ik_int)
#define TARG_SUPPORTS_ARM32_linux_riscv32 0
#define TARG_SUPPORTS_ARM64_linux_riscv32 0
#define TARG_SUPPORTS_RISCV32_linux_riscv32 1
#define TARG_SUPPORTS_RISCV64_linux_riscv32 0
#define TARG_SUPPORTS_X86_64_linux_riscv32 0
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE_linux_riscv32 0
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_riscv32 0
#define TARG_UNWIND_WORD_MODE_linux_riscv32 ((a_type_mode_kind)tmk_SI)
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES_linux_riscv32 1
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS_linux_riscv32 1
#define TARG_VAR_HANDLE_INT_KIND_linux_riscv32 ((an_integer_kind)ik_unsigned_short)
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND_linux_riscv32 ((an_integer_kind)ik_short)
#define TARG_WCHAR_T_INT_KIND_linux_riscv32 ((an_integer_kind)ik_int)
#define TARG_WINT_T_INT_KIND_linux_riscv32 ((an_integer_kind)ik_unsigned_int)
#define TARG_WORD_MODE_linux_riscv32 ((a_type_mode_kind)tmk_SI)
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_riscv32 1
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT_linux_riscv32 4

#endif /* INCLUDE_ADDITIONAL_TARGET_CONFIGURATION */

#else /* ifndef __linux__ */

#ifdef __hpux

/* Options for HP-UX version. */

/* Options to enable quasi-standard Unix features: */
#define _INCLUDE_POSIX_SOURCE 1
#define _INCLUDE_XOPEN_SOURCE 1
#define _INCLUDE_AES_SOURCE 1

/* >>> HP-UX Options determined with dettarg: */
#define TARG_LITTLE_ENDIAN 0
#define TARG_CHAR_BIT 8
#define TARG_HAS_SIGNED_CHARS 1
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT 1
#define TARG_SIZEOF_SHORT 2
#define TARG_ALIGNOF_SHORT 2
#define TARG_SIZEOF_INT 4
#define TARG_ALIGNOF_INT 4
#define TARG_SIZEOF_LONG 4
#define TARG_ALIGNOF_LONG 4
#define TARG_SIZEOF_POINTER 4
#define TARG_ALIGNOF_POINTER 4
#define TARG_SIZEOF_FLOAT 4
#define TARG_ALIGNOF_FLOAT 4
#define TARG_SIZEOF_DOUBLE 8
#define TARG_ALIGNOF_DOUBLE 8
#define TARG_SIZEOF_LONG_DOUBLE 16
#define TARG_ALIGNOF_LONG_DOUBLE 8
#define TARG_WCHAR_T_INT_KIND ((an_integer_kind)ik_unsigned_long)
#define TARG_SIZE_T_INT_KIND ((an_integer_kind)ik_unsigned_int)
#define TARG_PTRDIFF_T_INT_KIND ((an_integer_kind)ik_int)
#define HOST_ALIGNMENT_REQUIRED 4
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC 1
#define TARG_MINIMUM_STRUCT_ALIGNMENT 1
/* --- End of options determined with dettarg. */

/* jmp_buf settings for portable EH on HP-UX: */
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT 1
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND ((a_float_kind)fk_double)
#define TARG_JMP_BUF_NUM_ELEMENTS 25

#define COMPILE_MULTIPLE_SOURCE_FILES 1
#define MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED 1
#define LONG_LONG_ALLOWED 1
#define INTEGER_VALUE_REPR_IS_A_HOST_INTEGER 0
#define USE_LONG_DOUBLE_FOR_HOST_FP_VALUE 1
#define C99_IL_EXTENSIONS_SUPPORTED 1
#define CPP11_IL_EXTENSIONS_SUPPORTED 1
#define IGNORE_CARRIAGE_RETURN_IN_SOURCE 1
#define MICROSOFT_EXTENSIONS_ALLOWED 1
#define PRAGMA_WEAK_ALLOWED 1
#define VLA_ALLOWED 1

#ifdef SELFCOMP_VERSION
/* Self-compiled version (HP-UX). */
#define ALTERNATE_IL_FILE_FORMAT 0
#define ONE_INSTANTIATION_PER_OBJECT 0
#define MAINTAIN_NEEDED_FLAGS 0
#define DEFAULT_REMOVE_UNNEEDED_ENTITIES 0
#define _POSIX_C_SOURCE 1
#define _XOPEN_VERSION 0
#define _XOPEN_SOURCE_EXTENDED 0
#define _XOPEN_SOURCE 0
#define _XOPEN_SOURCE_EXTENDED 0
#endif /* SELFCOMP_VERSION */

#ifndef MAINTAIN_NEEDED_FLAGS
#define MAINTAIN_NEEDED_FLAGS 1
#endif /* ifndef MAINTAIN_NEEDED_FLAGS */
#ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES
#if MAINTAIN_NEEDED_FLAGS
#define DEFAULT_REMOVE_UNNEEDED_ENTITIES 1
#endif /* MAINTAIN_NEEDED_FLAGS */
#endif /* ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES */

#define __ANSIC__ 1
#ifndef C_GEN_BE_GENERATES_ANSI_C
#define C_GEN_BE_GENERATES_ANSI_C 1
#endif /* ifndef C_GEN_BE_GENERATES_ANSI_C */
#define MAKE_ALL_FUNCTIONS_UNPROTOTYPED 0
#define TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED 0
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT 0
#define STDC_ZERO_IN_NONSTRICT_MODE 1
#define GUARD_MACRO_FOR_VA_LIST "_VA_LIST"

/* Use 1 for mmap PCH, 0 for non-mmap PCH. */
#if 1
#define USE_FIXED_ADDRESS_FOR_MMAP 0
#ifndef USE_MMAP_FOR_MEMORY_REGIONS
#define USE_MMAP_FOR_MEMORY_REGIONS 1
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
#else /* !1 */
#ifndef USE_MMAP_FOR_MEMORY_REGIONS
#define USE_MMAP_FOR_MEMORY_REGIONS 0
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */
#endif /* 1 */

#ifdef OPTIMIZED_VERSION

/* Options for HP-UX optimized version. */
#ifndef CHECKING
#define CHECKING 0
#endif /* ifndef CHECKING */
#ifndef DEBUG
#define DEBUG 0
#endif /* ifndef DEBUG */

#else /* !defined(OPTIMIZED_VERSION) */

/* Options for HP-UX test version. */
#ifndef IL_SHOULD_BE_WRITTEN_TO_FILE
#define IL_SHOULD_BE_WRITTEN_TO_FILE 1
#endif /* ifndef IL_SHOULD_BE_WRITTEN_TO_FILE */
#define GENERATE_SOURCE_SEQUENCE_LISTS 1
#define ALLOW_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING 1
#ifndef INCLUDE_EDG_TEST_PRAGMAS
#define INCLUDE_EDG_TEST_PRAGMAS 1
#endif /* ifndef INCLUDE_EDG_TEST_PRAGMAS */
#define RECORD_HIDDEN_NAMES_IN_IL 1
#define ALLOW_HIDDEN_NAMES_IN_IL_WITH_IL_LOWERING 1
#define RECORD_TEMPLATE_STRINGS 1
#define RECORD_MACROS_IN_IL 1
#define KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED 1
#define DUMP_LOWERED_EH_CONSTRUCTS_IN_C_GEN_BE 1
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION 0
#define TARG_MICROSOFT_PTR_TO_MEMBER_SIZING 0
#define DEFAULT_MICROSOFT_MODE 0
#define EXTRA_SOURCE_POSITIONS_IN_IL 1
#define DEFAULT_SVR4_C_MODE 0
#define DEFAULT_VLA_ENABLED 0

#endif /* !defined(OPTIMIZED_VERSION) */


#else /* ifndef __hpux */

#if defined(__APPLE__) && defined(__MACH__)
/* Options for MacOS X (10.2) version. */

#ifndef MACOSX_TEST_VERSION
#define MACOSX_TEST_VERSION 1
#endif /* ifndef MACOSX_TEST_VERSION */

#if MACOSX_TEST_VERSION
#if !defined(MICROSOFT_EXTENSIONS_ALLOWED)
#define MICROSOFT_EXTENSIONS_ALLOWED 1
#endif /* !defined(MICROSOFT_EXTENSIONS_ALLOWED) */
#define DEFAULT_MICROSOFT_MODE 0
#define ALLOW_HOST_FP_TOO_SMALL_FOR_LARGEST_FIXED_POINT_TYPE 1
#ifndef COMPILE_MULTIPLE_TRANSLATION_UNITS
#define COMPILE_MULTIPLE_TRANSLATION_UNITS 1
#endif /* ifndef COMPILE_MULTIPLE_TRANSLATION_UNITS */
#endif /* MACOSX_TEST_VERSION */

#include "defines_macosx.h"

#else /* !(defined(__APPLE__) && defined(__MACH__)) */
#ifdef __CYGWIN__

/* Options for Windows/Cygwin version. */
#if CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT
#define MSVC_IS_GENERATED_CODE_TARGET 1
#endif /* CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT */
#define INT128_EXTENSIONS_ALLOWED 1
#define FLOAT128_ENABLING_POSSIBLE 1
#define HOST_FP_VALUE_IS_128BIT 1
#define THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED 1
#ifndef DEFAULT_INSTANTIATION_MODE
#define DEFAULT_INSTANTIATION_MODE tim_all
#endif /* DEFAULT_INSTANTIATION_MODE */

#define UNICODE_SOURCE_SUPPORTED 1
#define DEFAULT_CHECK_CONCATENATIONS 1
#define ASM_FUNCTION_ALLOWED 1
#define FIXED_POINT_ALLOWED 1
#ifdef DEMO_VERSION
/* Demo versions should support multiple translation units. */
#define COMPILE_MULTIPLE_TRANSLATION_UNITS 1
#define COMPILE_MULTIPLE_SOURCE_FILES 0
#define DEBUG 0
#define EMBEDDED_C_ALLOWED 1
#define INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES 1
#define INCLUDE_EDG_TEST_NAMED_REGISTERS 1
#endif /* ifdef DEMO_VERSION */
#ifndef TIE_DEFAULT_GNU_ABI_VERSION_TO_GNU_VERSION
#define TIE_DEFAULT_GNU_ABI_VERSION_TO_GNU_VERSION 1
#endif /* TIE_DEFAULT_GNU_ABI_VERSION_TO_GNU_VERSION */
#define MIN_GNU_VERSION 30200

#ifndef DEFAULT_EDG_BASE
#define DEFAULT_EDG_BASE "/c/edg/cpfe/release"
#endif /* DEFAULT_EDG_BASE */
#define __ANSIC__ 1
#ifndef COMPILE_MULTIPLE_SOURCE_FILES
#define COMPILE_MULTIPLE_SOURCE_FILES 1
#endif /* ifndef COMPILE_MULTIPLE_SOURCE_FILES */
#define C_GEN_BE_GENERATES_ANSI_C 1
#ifndef RUNTIME_USES_NAMESPACES
#define RUNTIME_USES_NAMESPACES 1
#endif /* ifndef RUNTIME_USES_NAMESPACES */
#define BUILTIN_VA_LIST_OVERRIDE_TYPE_NAME "__gnuc_va_list"
#define TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED 0
#define IGNORE_CARRIAGE_RETURN_IN_SOURCE 1
#define GNU_EXTENSIONS_ALLOWED 1
#define MICROSOFT_EXTENSIONS_ALLOWED 1
#define SUN_EXTENSIONS_ALLOWED 1
#define DEFAULT_SUN_COMPATIBILITY 0
#if defined(__GNUC__) && !defined(GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED)
#define GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED 1
#endif /* defined(__GNUC__) && !defined(GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED) */
#ifndef DEFAULT_GNU_COMPATIBILITY
#define DEFAULT_GNU_COMPATIBILITY 0
#endif /* ifndef DEFAULT_GNU_COMPATIBILITY */
#ifndef GNU_VECTOR_TYPES_ALLOWED
#define GNU_VECTOR_TYPES_ALLOWED 1
#endif /* ifndef GNU_VECTOR_TYPES_ALLOWED */
#ifndef BUILTIN_FUNCTIONS_ENABLED
#define BUILTIN_FUNCTIONS_ENABLED 1
#endif /* ifndef BUILTIN_FUNCTIONS_ENABLED */
#ifndef RISCV_VECTOR_BUILTINS_ENABLED
#define RISCV_VECTOR_BUILTINS_ENABLED 0
#endif /* ifndef RISCV_VECTOR_BUILTINS_ENABLED */
#define DEFAULT_USE_PREDEFINED_MACRO_FILE 1
#ifndef IA64_ABI
#define IA64_ABI 1
#endif /* IA64_ABI */
#ifndef MAINTAIN_NEEDED_FLAGS
#define MAINTAIN_NEEDED_FLAGS 1
#endif /* ifndef MAINTAIN_NEEDED_FLAGS */
#ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES
#if MAINTAIN_NEEDED_FLAGS
#define DEFAULT_REMOVE_UNNEEDED_ENTITIES 0
#endif /* MAINTAIN_NEEDED_FLAGS */
#endif /* ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES */
/* Unless specified otherwise, Cygwin version will have full macro position
   and tracing facilities. */
#define EXTRA_SOURCE_POSITIONS_IN_IL 1
#ifndef FULLY_RESOLVED_MACRO_POSITIONS
#define FULLY_RESOLVED_MACRO_POSITIONS 1
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
#ifndef MACRO_INVOCATION_TREE_IN_IL
#define MACRO_INVOCATION_TREE_IN_IL 1
#endif /* MACRO_INVOCATION_TREE_IN_IL */
#ifndef DEFAULT_MACRO_POSITIONS_IN_DIAGNOSTICS
#define DEFAULT_MACRO_POSITIONS_IN_DIAGNOSTICS 1
#endif /* DEFAULT_MACRO_POSITIONS_IN_DIAGNOSTICS */
/* Settings needed in order for bit-field allocation to match gcc. */
#define TARG_BIT_FIELD_CONTAINER_SIZE (-1)
#define ALLOW_NON_INT_BIT_FIELD_BASE_TYPE_IN_GENERATED_C 1

#ifndef USE_MMAP_FOR_MEMORY_REGIONS
#define USE_MMAP_FOR_MEMORY_REGIONS 0
#endif /* USE_MMAP_FOR_MEMORY_REGIONS */

#define LONG_LONG_ALLOWED 1
#if INT128_EXTENSIONS_ALLOWED
#define INTEGER_VALUE_REPR_IS_A_HOST_INTEGER 0
#else  /* !INT128_EXTENSIONS_ALLOWED */
#define INTEGER_VALUE_REPR_IS_A_HOST_INTEGER 1
#define TYPE_FOR_AN_INTEGER_VALUE unsigned long long
#define TYPE_FOR_A_SIGNED_INTEGER_VALUE long long
#endif /* INT128_EXTENSIONS_ALLOWED */
#define MAX_INTEGER_VALUE 9223372036854775807LL
#define MIN_INTEGER_VALUE (-MAX_INTEGER_VALUE-1)
#define MAX_UNSIGNED_INTEGER_VALUE 18446744073709551615ULL

#ifndef USE_FLOAT128_FOR_HOST_FP_VALUE
#define USE_FLOAT128_FOR_HOST_FP_VALUE 1
#define APPROXIMATE_QUADMATH 1
#endif /* USE_FLOAT128_FOR_HOST_FP_VALUE */
#define FP_LONG_DOUBLE_IS_80BIT_EXTENDED 1

#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION 1
#ifndef TARG_MICROSOFT_PTR_TO_MEMBER_SIZING 
#define TARG_MICROSOFT_PTR_TO_MEMBER_SIZING BACK_END_IS_CP_GEN_BE
#endif /* TARG_MICROSOFT_PTR_TO_MEMBER_SIZING */

/* Configuration definitions determined by dettarg.c: */
#if defined(__x86_64__)

#define TARG_LITTLE_ENDIAN 1
#define TARG_CHAR_BIT 8
#define TARG_HAS_SIGNED_CHARS 1
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT 1
#define TARG_SIZEOF_SHORT 2
#define TARG_ALIGNOF_SHORT 2
#define TARG_SIZEOF_INT 4
#define TARG_ALIGNOF_INT 4
#define TARG_SIZEOF_LONG 8
#define TARG_ALIGNOF_LONG 8
#define TARG_SIZEOF_POINTER 8
#define TARG_ALIGNOF_POINTER 8
#define TARG_SIZEOF_FLOAT 4
#define TARG_ALIGNOF_FLOAT 4
#define TARG_SIZEOF_DOUBLE 8
#define TARG_ALIGNOF_DOUBLE 8
#define TARG_SIZEOF_LONG_DOUBLE 16
#define TARG_ALIGNOF_LONG_DOUBLE 16
#define TARG_WCHAR_T_INT_KIND ((an_integer_kind)ik_unsigned_short)
#define TARG_SIZE_T_INT_KIND ((an_integer_kind)ik_unsigned_long)
#define TARG_SSIZE_T_INT_KIND ((an_integer_kind)ik_long)
#define TARG_SIZE_T_MAX ((a_targ_size_t)0xffffffffUL)
#define TARG_PTRDIFF_T_INT_KIND ((an_integer_kind)ik_long)
#define HOST_ALIGNMENT_REQUIRED 8
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC 1
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE 0
#define TARG_MINIMUM_STRUCT_ALIGNMENT 1
#define TARG_JMP_BUF_NUM_ELEMENTS 32
#define TARG_JMP_BUF_ELEMENT_INT_KIND ((an_integer_kind)ik_long)

#else /* !defined(__x86_64) */

#define TARG_LITTLE_ENDIAN 1
#define TARG_CHAR_BIT 8
#define TARG_HAS_SIGNED_CHARS 1
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT 1
#define TARG_SIZEOF_SHORT 2
#define TARG_ALIGNOF_SHORT 2
#define TARG_SIZEOF_INT 4
#define TARG_ALIGNOF_INT 4
#define TARG_SIZEOF_LONG 4
#define TARG_ALIGNOF_LONG 4
#define TARG_SIZEOF_POINTER 4
#define TARG_ALIGNOF_POINTER 4
#define TARG_SIZEOF_FLOAT 4
#define TARG_ALIGNOF_FLOAT 4
#define TARG_SIZEOF_DOUBLE 8
#define TARG_ALIGNOF_DOUBLE 8
#define TARG_SIZEOF_LONG_DOUBLE 12
#define TARG_ALIGNOF_LONG_DOUBLE 4
#if SIZE_T_IS_LONG
#define TARG_SIZE_T_INT_KIND ((an_integer_kind)ik_unsigned_long)
#define TARG_SSIZE_T_INT_KIND ((an_integer_kind)ik_long)
#define TARG_SIZE_T_MAX ((a_targ_size_t)0xffffffffUL)
#endif /* SIZE_T_IS_LONG */
#define TARG_WCHAR_T_INT_KIND ((an_integer_kind)ik_unsigned_short)
#define HOST_ALIGNMENT_REQUIRED 8
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC 1
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE 0
#define TARG_MINIMUM_STRUCT_ALIGNMENT 1
#define TARG_JMP_BUF_NUM_ELEMENTS 52
#define TARG_JMP_BUF_ELEMENT_INT_KIND ((an_integer_kind)ik_int)

#endif /* defined(__x86_64) */

#if INCLUDE_ADDITIONAL_TARGET_CONFIGURATION
#define LEGACY_TARGET_CONFIGURATION_NAME "cygwin"

/* Target configuration: win64 */
#define TARGET_CONFIGURATION_1 win64
#define TARG_ALIGNOF_DOUBLE_win64 8
#define TARG_ALIGNOF_FAR_POINTER_win64 4
#define TARG_ALIGNOF_FLOAT_win64 4
#define TARG_ALIGNOF_FLOAT128_win64 16
#define TARG_ALIGNOF_FLOAT80_win64 4
#define TARG_ALIGNOF_INT_win64 4
#define TARG_ALIGNOF_INT128_win64 16
#define TARG_ALIGNOF_LONG_win64 4
#define TARG_ALIGNOF_LONG_DOUBLE_win64 8
#define TARG_ALIGNOF_LONG_LONG_win64 8
#define TARG_ALIGNOF_NEAR_POINTER_win64 2
#define TARG_ALIGNOF_POINTER_win64 8
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER_win64 8
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION_win64 8
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS_win64 8
#define TARG_ALIGNOF_SHORT_win64 2
#define TARG_ALIGNOF_SIGNED_ACCUM_win64 4
#define TARG_ALIGNOF_SIGNED_FRACT_win64 2
#define TARG_ALIGNOF_SIGNED_LONG_ACCUM_win64 8
#define TARG_ALIGNOF_SIGNED_LONG_FRACT_win64 4
#define TARG_ALIGNOF_SIGNED_SHORT_ACCUM_win64 2
#define TARG_ALIGNOF_SIGNED_SHORT_FRACT_win64 1
#define TARG_ALIGNOF_UNSIGNED_ACCUM_win64 4
#define TARG_ALIGNOF_UNSIGNED_FRACT_win64 2
#define TARG_ALIGNOF_UNSIGNED_LONG_ACCUM_win64 8
#define TARG_ALIGNOF_UNSIGNED_LONG_FRACT_win64 4
#define TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM_win64 2
#define TARG_ALIGNOF_UNSIGNED_SHORT_FRACT_win64 1
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO_win64 8
#define TARG_ALL_POINTERS_SAME_SIZE_win64 1
#define TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT_win64 1
#define TARG_BIT_FIELD_CONTAINER_SIZE_win64 (-1)
#define TARG_BOOL_INT_KIND_win64 ((an_integer_kind)ik_char)
#define TARG_C_BOOL_INT_KIND_win64 ((an_integer_kind)ik_unsigned_char)
#define TARG_CHAR16_T_INT_KIND_win64 ((an_integer_kind)ik_unsigned_short)
#define TARG_CHAR32_T_INT_KIND_win64 ((an_integer_kind)ik_unsigned_int)
#define TARG_CHAR_BIT_win64 8
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT_win64 1
#define TARG_DBL_MANT_DIG_win64 53
#define TARG_DBL_MAX_EXP_win64 1024
#define TARG_DBL_MIN_EXP_win64 (-1021)
#define TARG_DEFAULT_NEW_ALIGNMENT_win64 16
#define TARG_DELTA_INT_KIND_win64 ((an_integer_kind)ik_short)
#define TARG_DOUBLE_FIELD_ALIGNMENT_win64 4
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES_win64 1
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED_win64 0
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT_win64 0
#define TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE_win64 1
#define TARG_FLOAT_FIELD_ALIGNMENT_win64 4
#define TARG_FLOAT128_FIELD_ALIGNMENT_win64 16
#define TARG_FLOAT80_FIELD_ALIGNMENT_win64 4
#define TARG_FLT_MANT_DIG_win64 24
#define TARG_FLT_MAX_EXP_win64 128
#define TARG_FLT_MIN_EXP_win64 (-125)
#define TARG_FLT128_MANT_DIG_win64 113
#define TARG_FLT128_MAX_EXP_win64 (16384)
#define TARG_FLT128_MIN_EXP_win64 (-16381)
#define TARG_FLT80_MANT_DIG_win64 64
#define TARG_FLT80_MAX_EXP_win64 (16384)
#define TARG_FLT80_MIN_EXP_win64 (-16381)
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED_win64 1
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM_win64 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT_win64 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM_win64 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT_win64 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM_win64 7
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT_win64 7
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM_win64 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT_win64 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM_win64 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT_win64 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM_win64 8
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT_win64 8
#define TARG_HAS_SIGNED_CHARS_win64 1
#define TARG_HOST_STRING_CHAR_BIT_win64 8
#define TARG_IA64_ABI_USE_GUARD_ACQUIRE_RELEASE_win64 1
#define TARG_IA64_ABI_USE_INT_STATIC_INIT_GUARD_win64 0
#define TARG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES_win64 0
#define TARG_IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR_win64 0
#define TARG_IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS_win64 0
#define TARG_IA64_ABI_VARIANT_KEY_FUNCTION_win64 0
#define TARG_IA64_VTABLE_ENTRY_INT_KIND_win64 ((an_integer_kind)ik_long_long)
#define TARG_INT_FIELD_ALIGNMENT_win64 4
#define TARG_INT128_FIELD_ALIGNMENT_win64 16
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT_win64 0
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND_win64 ((a_float_kind)fk_long_double)
#define TARG_JMP_BUF_ELEMENT_INT_KIND_win64 ((an_integer_kind)ik_long)
#define TARG_JMP_BUF_NUM_ELEMENTS_win64 39
#define TARG_SETJMP_FUNC_win64 "setjmp"
#define TARG_LDBL_MANT_DIG_win64 53
#define TARG_LDBL_MAX_EXP_win64 1024
#define TARG_LDBL_MIN_EXP_win64 (-1021)
#define TARG_LIBGCC_CMP_RETURN_MODE_win64 ((a_type_mode_kind)tmk_SI)
#define TARG_LIBGCC_SHIFT_COUNT_MODE_win64 ((a_type_mode_kind)tmk_SI)
#define TARG_LITTLE_ENDIAN_win64 1
#define TARG_LONG_DOUBLE_FIELD_ALIGNMENT_win64 4
#define TARG_LONG_FIELD_ALIGNMENT_win64 4
#define TARG_LONG_LONG_FIELD_ALIGNMENT_win64 8
#define TARG_MAXIMUM_INTRINSIC_ALIGNMENT_win64 8
#define TARG_MAX_BASE_CLASS_OFFSET_win64 0
#define TARG_MAX_CLASS_OBJECT_SIZE_win64 0
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION_win64 0
#define TARG_MICROSOFT_PTR_TO_MEMBER_SIZING_win64 BACK_END_IS_CP_GEN_BE
#define TARG_MINIMUM_STRUCT_ALIGNMENT_win64 1
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED_win64 1
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT_win64 1
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE_win64 1
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED_win64 0
#define TARG_POINTER_MODE_win64 ((a_type_mode_kind)tmk_SI)
#define TARG_PTRDIFF_T_INT_KIND_win64 ((an_integer_kind)ik_long_long)
#define TARG_REGION_NUMBER_INT_KIND_win64 ((an_integer_kind)ik_unsigned_short)
#define TARG_ETS_FLAG_TYPE_INT_KIND_win64 ((an_integer_kind)ik_unsigned_int)
#define TARG_REUSE_TAIL_PADDING_win64 1
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC_win64 1
#define TARG_RUNTIME_ELEM_COUNT_INT_KIND_win64 ((an_integer_kind)ik_long_long)
#define TARG_SHORT_FIELD_ALIGNMENT_win64 2
#define TARG_SIZEOF_DOUBLE_win64 8
#define TARG_SIZEOF_FAR_POINTER_win64 4
#define TARG_SIZEOF_FLOAT_win64 4
#define TARG_SIZEOF_FLOAT128_win64 16
#define TARG_SIZEOF_FLOAT80_win64 12
#define TARG_SIZEOF_INT_win64 4
#define TARG_SIZEOF_INT128_win64 16
#define TARG_SIZEOF_LARGEST_FIXED_POINT_win64 8
#define TARG_SIZEOF_LARGEST_ATOMIC_win64 16
#define TARG_SIZEOF_LONG_win64 4
#define TARG_SIZEOF_LONG_DOUBLE_win64 8
#define TARG_SIZEOF_LONG_LONG_win64 8
#define TARG_SIZEOF_NEAR_POINTER_win64 2
#define TARG_SIZEOF_POINTER_win64 8

#define TARG_SIZEOF_PTR_TO_DATA_MEMBER_win64 8
#if IA64_ABI
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION_win64 12
#else /* !IA64_ABI */
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION_win64 16
#endif /* IA64_ABI */
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS_win64 8
#define TARG_SIZEOF_SHORT_win64 2
#define TARG_SIZEOF_SIGNED_ACCUM_win64 4
#define TARG_SIZEOF_SIGNED_FRACT_win64 2
#define TARG_SIZEOF_SIGNED_LONG_ACCUM_win64 8
#define TARG_SIZEOF_SIGNED_LONG_FRACT_win64 4
#define TARG_SIZEOF_SIGNED_SHORT_ACCUM_win64 2
#define TARG_SIZEOF_SIGNED_SHORT_FRACT_win64 1
#define TARG_SIZEOF_UNSIGNED_ACCUM_win64 4
#define TARG_SIZEOF_UNSIGNED_FRACT_win64 2
#define TARG_SIZEOF_UNSIGNED_LONG_ACCUM_win64 8
#define TARG_SIZEOF_UNSIGNED_LONG_FRACT_win64 4
#define TARG_SIZEOF_UNSIGNED_SHORT_ACCUM_win64 2
#define TARG_SIZEOF_UNSIGNED_SHORT_FRACT_win64 1
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO_win64 8
#define TARG_SIZE_T_INT_KIND_win64 ((an_integer_kind)ik_unsigned_long_long)
#define TARG_SIZE_T_MAX_win64 ((a_targ_size_t)(9223372036854775807LL * 2ULL + 1))
#define TARG_SSIZE_T_INT_KIND_win64 ((an_integer_kind)ik_long_long)
#define TARG_SUPPORTS_ARM32_win64 0
#define TARG_SUPPORTS_ARM64_win64 0
#define TARG_SUPPORTS_RISCV32_win64 0
#define TARG_SUPPORTS_RISCV64_win64 0
#define TARG_SUPPORTS_X86_64_win64 1
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE_win64 0
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_win64 0
#define TARG_UNWIND_WORD_MODE_win64 ((a_type_mode_kind)tmk_SI)
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES_win64 1
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS_win64 1
#define TARG_VAR_HANDLE_INT_KIND_win64 ((an_integer_kind)ik_unsigned_short)
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND_win64 ((an_integer_kind)ik_short)
#define TARG_WCHAR_T_INT_KIND_win64 ((an_integer_kind)ik_unsigned_short)
#define TARG_WINT_T_INT_KIND_win64 ((an_integer_kind)ik_unsigned_short)
#define TARG_WORD_MODE_win64 ((a_type_mode_kind)tmk_SI)
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_win64 1
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT_win64 4

/* Target configuration: win32 */
#define TARGET_CONFIGURATION_2 win32
#define TARG_ALIGNOF_DOUBLE_win32 8
#define TARG_ALIGNOF_FAR_POINTER_win32 4
#define TARG_ALIGNOF_FLOAT_win32 4
#define TARG_ALIGNOF_FLOAT128_win32 16
#define TARG_ALIGNOF_FLOAT80_win32 4
#define TARG_ALIGNOF_INT_win32 4
#define TARG_ALIGNOF_INT128_win32 16
#define TARG_ALIGNOF_LONG_win32 4
#define TARG_ALIGNOF_LONG_DOUBLE_win32 8
#define TARG_ALIGNOF_LONG_LONG_win32 8
#define TARG_ALIGNOF_NEAR_POINTER_win32 2
#define TARG_ALIGNOF_POINTER_win32 4
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER_win32 4
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION_win32 4
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS_win32 4
#define TARG_ALIGNOF_SHORT_win32 2
#define TARG_ALIGNOF_SIGNED_ACCUM_win32 4
#define TARG_ALIGNOF_SIGNED_FRACT_win32 2
#define TARG_ALIGNOF_SIGNED_LONG_ACCUM_win32 8
#define TARG_ALIGNOF_SIGNED_LONG_FRACT_win32 4
#define TARG_ALIGNOF_SIGNED_SHORT_ACCUM_win32 2
#define TARG_ALIGNOF_SIGNED_SHORT_FRACT_win32 1
#define TARG_ALIGNOF_UNSIGNED_ACCUM_win32 4
#define TARG_ALIGNOF_UNSIGNED_FRACT_win32 2
#define TARG_ALIGNOF_UNSIGNED_LONG_ACCUM_win32 8
#define TARG_ALIGNOF_UNSIGNED_LONG_FRACT_win32 4
#define TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM_win32 2
#define TARG_ALIGNOF_UNSIGNED_SHORT_FRACT_win32 1
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO_win32 4
#define TARG_ALL_POINTERS_SAME_SIZE_win32 1
#define TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT_win32 0
#define TARG_BIT_FIELD_CONTAINER_SIZE_win32 (-1)
#define TARG_BOOL_INT_KIND_win32 ((an_integer_kind)ik_char)
#define TARG_C_BOOL_INT_KIND_win32 ((an_integer_kind)ik_unsigned_char)
#define TARG_CHAR16_T_INT_KIND_win32 ((an_integer_kind)ik_unsigned_short)
#define TARG_CHAR32_T_INT_KIND_win32 ((an_integer_kind)ik_unsigned_int)
#define TARG_CHAR_BIT_win32 8
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT_win32 1
#define TARG_DBL_MANT_DIG_win32 53
#define TARG_DBL_MAX_EXP_win32 1024
#define TARG_DBL_MIN_EXP_win32 (-1021)
#define TARG_DEFAULT_NEW_ALIGNMENT_win32 8
#define TARG_DELTA_INT_KIND_win32 ((an_integer_kind)ik_short)
#define TARG_DOUBLE_FIELD_ALIGNMENT_win32 8
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES_win32 0
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED_win32 0
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT_win32 0
#define TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE_win32 1
#define TARG_FLOAT_FIELD_ALIGNMENT_win32 4
#define TARG_FLOAT128_FIELD_ALIGNMENT_win32 16
#define TARG_FLOAT80_FIELD_ALIGNMENT_win32 4
#define TARG_FLT_MANT_DIG_win32 24
#define TARG_FLT_MAX_EXP_win32 128
#define TARG_FLT_MIN_EXP_win32 (-125)
#define TARG_FLT128_MANT_DIG_win32 113
#define TARG_FLT128_MAX_EXP_win32 (16384)
#define TARG_FLT128_MIN_EXP_win32 (-16381)
#define TARG_FLT80_MANT_DIG_win32 64
#define TARG_FLT80_MAX_EXP_win32 (16384)
#define TARG_FLT80_MIN_EXP_win32 (-16381)
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED_win32 1
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM_win32 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT_win32 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM_win32 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT_win32 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM_win32 7
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT_win32 7
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM_win32 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT_win32 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM_win32 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT_win32 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM_win32 8
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT_win32 8
#define TARG_HAS_SIGNED_CHARS_win32 1
#define TARG_HOST_STRING_CHAR_BIT_win32 8
#define TARG_IA64_ABI_USE_GUARD_ACQUIRE_RELEASE_win32 1
#define TARG_IA64_ABI_USE_INT_STATIC_INIT_GUARD_win32 0
#define TARG_IA64_ABI_USE_VARIANT_ARRAY_COOKIES_win32 0
#define TARG_IA64_ABI_USE_VARIANT_PTR_TO_MEMBER_FUNCTION_REPR_win32 0
#define TARG_IA64_ABI_VARIANT_CTORS_AND_DTORS_RETURN_THIS_win32 0
#define TARG_IA64_ABI_VARIANT_KEY_FUNCTION_win32 0
#define TARG_IA64_VTABLE_ENTRY_INT_KIND_win32 ((an_integer_kind)ik_int)
#define TARG_INT_FIELD_ALIGNMENT_win32 4
#define TARG_INT128_FIELD_ALIGNMENT_win32 16
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT_win32 0
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND_win32 ((a_float_kind)fk_long_double)
#define TARG_JMP_BUF_ELEMENT_INT_KIND_win32 ((an_integer_kind)ik_long)
#define TARG_JMP_BUF_NUM_ELEMENTS_win32 16
#define TARG_SETJMP_FUNC_win32 "setjmp"
#define TARG_LDBL_MANT_DIG_win32 53
#define TARG_LDBL_MAX_EXP_win32 1024
#define TARG_LDBL_MIN_EXP_win32 (-1021)
#define TARG_LIBGCC_CMP_RETURN_MODE_win32 ((a_type_mode_kind)tmk_SI)
#define TARG_LIBGCC_SHIFT_COUNT_MODE_win32 ((a_type_mode_kind)tmk_SI)
#define TARG_LITTLE_ENDIAN_win32 1
#define TARG_LONG_DOUBLE_FIELD_ALIGNMENT_win32 8
#define TARG_LONG_FIELD_ALIGNMENT_win32 4
#define TARG_LONG_LONG_FIELD_ALIGNMENT_win32 8
#define TARG_MAXIMUM_INTRINSIC_ALIGNMENT_win32 8
#define TARG_MAX_BASE_CLASS_OFFSET_win32 0
#define TARG_MAX_CLASS_OBJECT_SIZE_win32 0
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION_win32 1
#define TARG_MICROSOFT_PTR_TO_MEMBER_SIZING_win32 BACK_END_IS_CP_GEN_BE
#define TARG_MINIMUM_STRUCT_ALIGNMENT_win32 1
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED_win32 0
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT_win32 1
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE_win32 1
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED_win32 ( !1)
#define TARG_POINTER_MODE_win32 ((a_type_mode_kind)tmk_SI)
#define TARG_PTRDIFF_T_INT_KIND_win32 ((an_integer_kind)ik_int)
#define TARG_REGION_NUMBER_INT_KIND_win32 ((an_integer_kind)ik_unsigned_short)
#define TARG_ETS_FLAG_TYPE_INT_KIND_win32 ((an_integer_kind)ik_unsigned_int)
#define TARG_REUSE_TAIL_PADDING_win32 1
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC_win32 1
#define TARG_RUNTIME_ELEM_COUNT_INT_KIND_win32 ((an_integer_kind)ik_int)
#define TARG_SHORT_FIELD_ALIGNMENT_win32 2
#define TARG_SIZEOF_DOUBLE_win32 8
#define TARG_SIZEOF_FAR_POINTER_win32 4
#define TARG_SIZEOF_FLOAT_win32 4
#define TARG_SIZEOF_FLOAT128_win32 16
#define TARG_SIZEOF_FLOAT80_win32 12
#define TARG_SIZEOF_INT_win32 4
#define TARG_SIZEOF_INT128_win32 16
#define TARG_SIZEOF_LARGEST_FIXED_POINT_win32 8
#define TARG_SIZEOF_LARGEST_ATOMIC_win32 8
#define TARG_SIZEOF_LONG_win32 4
#define TARG_SIZEOF_LONG_DOUBLE_win32 8
#define TARG_SIZEOF_LONG_LONG_win32 8
#define TARG_SIZEOF_NEAR_POINTER_win32 2
#define TARG_SIZEOF_POINTER_win32 4
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER_win32 4
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION_win32 ((((2*2+4-1)/4)+1)* 4)
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS_win32 4
#define TARG_SIZEOF_SHORT_win32 2
#define TARG_SIZEOF_SIGNED_ACCUM_win32 4
#define TARG_SIZEOF_SIGNED_FRACT_win32 2
#define TARG_SIZEOF_SIGNED_LONG_ACCUM_win32 8
#define TARG_SIZEOF_SIGNED_LONG_FRACT_win32 4
#define TARG_SIZEOF_SIGNED_SHORT_ACCUM_win32 2
#define TARG_SIZEOF_SIGNED_SHORT_FRACT_win32 1
#define TARG_SIZEOF_UNSIGNED_ACCUM_win32 4
#define TARG_SIZEOF_UNSIGNED_FRACT_win32 2
#define TARG_SIZEOF_UNSIGNED_LONG_ACCUM_win32 8
#define TARG_SIZEOF_UNSIGNED_LONG_FRACT_win32 4
#define TARG_SIZEOF_UNSIGNED_SHORT_ACCUM_win32 2
#define TARG_SIZEOF_UNSIGNED_SHORT_FRACT_win32 1
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO_win32 4
#define TARG_SIZE_T_INT_KIND_win32 ((an_integer_kind)ik_unsigned_int)
#define TARG_SIZE_T_MAX_win32 ((a_targ_size_t)0xffffffff)
#define TARG_SSIZE_T_INT_KIND_win32 ((an_integer_kind)ik_int)
#define TARG_SUPPORTS_ARM32_win32 0
#define TARG_SUPPORTS_ARM64_win32 0
#define TARG_SUPPORTS_RISCV32_win32 0
#define TARG_SUPPORTS_RISCV64_win32 0
#define TARG_SUPPORTS_X86_64_win32 0
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE_win32 0
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_win32 1
#define TARG_UNWIND_WORD_MODE_win32 ((a_type_mode_kind)tmk_SI)
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES_win32 1
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS_win32 1
#define TARG_VAR_HANDLE_INT_KIND_win32 ((an_integer_kind)ik_unsigned_short)
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND_win32 ((an_integer_kind)ik_short)
#define TARG_WCHAR_T_INT_KIND_win32 ((an_integer_kind)ik_unsigned_short)
#define TARG_WINT_T_INT_KIND_win32 ((an_integer_kind)ik_unsigned_short)
#define TARG_WORD_MODE_win32 ((a_type_mode_kind)tmk_SI)
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_win32 1
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT_win32 (-1)

/* Target configuration: linux_x86_64 */
#define TARGET_CONFIGURATION_3 linux_x86_64
#define TARG_ALIGNOF_DOUBLE_linux_x86_64 8
#define TARG_ALIGNOF_FAR_POINTER_linux_x86_64 4
#define TARG_ALIGNOF_FLOAT_linux_x86_64 4
#define TARG_ALIGNOF_FLOAT128_linux_x86_64 16
#define TARG_ALIGNOF_FLOAT80_linux_x86_64 16
#define TARG_ALIGNOF_INT_linux_x86_64 4
#define TARG_ALIGNOF_INT128_linux_x86_64 16
#define TARG_ALIGNOF_LONG_linux_x86_64 8
#define TARG_ALIGNOF_LONG_DOUBLE_linux_x86_64 16
#define TARG_ALIGNOF_LONG_LONG_linux_x86_64 8
#define TARG_ALIGNOF_NEAR_POINTER_linux_x86_64 2
#define TARG_ALIGNOF_POINTER_linux_x86_64 8
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER_linux_x86_64 8
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION_linux_x86_64 8
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_x86_64 8
#define TARG_ALIGNOF_SHORT_linux_x86_64 2
#define TARG_ALIGNOF_SIGNED_ACCUM_linux_x86_64 4
#define TARG_ALIGNOF_SIGNED_FRACT_linux_x86_64 2
#define TARG_ALIGNOF_SIGNED_LONG_ACCUM_linux_x86_64 8
#define TARG_ALIGNOF_SIGNED_LONG_FRACT_linux_x86_64 4
#define TARG_ALIGNOF_SIGNED_SHORT_ACCUM_linux_x86_64 2
#define TARG_ALIGNOF_SIGNED_SHORT_FRACT_linux_x86_64 1
#define TARG_ALIGNOF_UNSIGNED_ACCUM_linux_x86_64 4
#define TARG_ALIGNOF_UNSIGNED_FRACT_linux_x86_64 2
#define TARG_ALIGNOF_UNSIGNED_LONG_ACCUM_linux_x86_64 8
#define TARG_ALIGNOF_UNSIGNED_LONG_FRACT_linux_x86_64 4
#define TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM_linux_x86_64 2
#define TARG_ALIGNOF_UNSIGNED_SHORT_FRACT_linux_x86_64 1
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO_linux_x86_64 8
#define TARG_ALL_POINTERS_SAME_SIZE_linux_x86_64 1
#define TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT_linux_x86_64 1
#define TARG_BIT_FIELD_CONTAINER_SIZE_linux_x86_64 (-1)
#define TARG_BOOL_INT_KIND_linux_x86_64 ((an_integer_kind)ik_char)
#define TARG_C_BOOL_INT_KIND_linux_x86_64 ((an_integer_kind)ik_unsigned_char)
#define TARG_CHAR16_T_INT_KIND_linux_x86_64 ((an_integer_kind)ik_unsigned_short)
#define TARG_CHAR32_T_INT_KIND_linux_x86_64 ((an_integer_kind)ik_unsigned_int)
#define TARG_CHAR_BIT_linux_x86_64 8
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT_linux_x86_64 1
#define TARG_DBL_MANT_DIG_linux_x86_64 53
#define TARG_DBL_MAX_EXP_linux_x86_64 1024
#define TARG_DBL_MIN_EXP_linux_x86_64 (-1021)
#define TARG_DEFAULT_NEW_ALIGNMENT_linux_x86_64 16
#define TARG_DELTA_INT_KIND_linux_x86_64 ((an_integer_kind)ik_short)
#define TARG_DOUBLE_FIELD_ALIGNMENT_linux_x86_64 8
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES_linux_x86_64 0
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED_linux_x86_64 0
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT_linux_x86_64 0
#define TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE_linux_x86_64 1
#define TARG_FLOAT_FIELD_ALIGNMENT_linux_x86_64 4
#define TARG_FLOAT128_FIELD_ALIGNMENT_linux_x86_64 16
#define TARG_FLOAT80_FIELD_ALIGNMENT_linux_x86_64 16
#define TARG_FLT_MANT_DIG_linux_x86_64 24
#define TARG_FLT_MAX_EXP_linux_x86_64 128
#define TARG_FLT_MIN_EXP_linux_x86_64 (-125)
#define TARG_FLT128_MANT_DIG_linux_x86_64 113
#define TARG_FLT128_MAX_EXP_linux_x86_64 (16384)
#define TARG_FLT128_MIN_EXP_linux_x86_64 (-16381)
#define TARG_FLT80_MANT_DIG_linux_x86_64 64
#define TARG_FLT80_MAX_EXP_linux_x86_64 (16384)
#define TARG_FLT80_MIN_EXP_linux_x86_64 (-16381)
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED_linux_x86_64 1
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM_linux_x86_64 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT_linux_x86_64 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM_linux_x86_64 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT_linux_x86_64 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM_linux_x86_64 7
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT_linux_x86_64 7
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM_linux_x86_64 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT_linux_x86_64 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM_linux_x86_64 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT_linux_x86_64 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM_linux_x86_64 8
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT_linux_x86_64 8
#define TARG_HAS_SIGNED_CHARS_linux_x86_64 1
#define TARG_HOST_STRING_CHAR_BIT_linux_x86_64 8
#define TARG_INT_FIELD_ALIGNMENT_linux_x86_64 4
#define TARG_INT128_FIELD_ALIGNMENT_linux_x86_64 16
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT_linux_x86_64 0
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND_linux_x86_64 ((a_float_kind)fk_long_double)
#define TARG_JMP_BUF_ELEMENT_INT_KIND_linux_x86_64 ((an_integer_kind)ik_long)
#define TARG_JMP_BUF_NUM_ELEMENTS_linux_x86_64 25
#define TARG_SETJMP_FUNC_linux_x86_64 "_setjmp"
#define TARG_LDBL_MANT_DIG_linux_x86_64 64
#define TARG_LDBL_MAX_EXP_linux_x86_64 16384
#define TARG_LDBL_MIN_EXP_linux_x86_64 (-16381)
#define TARG_LIBGCC_CMP_RETURN_MODE_linux_x86_64 ((a_type_mode_kind)tmk_DI)
#define TARG_LIBGCC_SHIFT_COUNT_MODE_linux_x86_64 ((a_type_mode_kind)tmk_DI)
#define TARG_LITTLE_ENDIAN_linux_x86_64 1
#define TARG_LONG_DOUBLE_FIELD_ALIGNMENT_linux_x86_64 16
#define TARG_LONG_FIELD_ALIGNMENT_linux_x86_64 8
#define TARG_LONG_LONG_FIELD_ALIGNMENT_linux_x86_64 8
#define TARG_MAXIMUM_INTRINSIC_ALIGNMENT_linux_x86_64 16
#define TARG_MAX_BASE_CLASS_OFFSET_linux_x86_64 0
#define TARG_MAX_CLASS_OBJECT_SIZE_linux_x86_64 0
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION_linux_x86_64 0
#define TARG_MICROSOFT_PTR_TO_MEMBER_SIZING_linux_x86_64 BACK_END_IS_CP_GEN_BE
#define TARG_MINIMUM_STRUCT_ALIGNMENT_linux_x86_64 1
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED_linux_x86_64 1
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT_linux_x86_64 1
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE_linux_x86_64 1
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED_linux_x86_64 0
#define TARG_POINTER_MODE_linux_x86_64 ((a_type_mode_kind)tmk_DI)
#define TARG_PTRDIFF_T_INT_KIND_linux_x86_64 ((an_integer_kind)ik_long)
#define TARG_REGION_NUMBER_INT_KIND_linux_x86_64 ((an_integer_kind)ik_unsigned_short)
#define TARG_ETS_FLAG_TYPE_INT_KIND_linux_x86_64 ((an_integer_kind)ik_unsigned_int)
#define TARG_REUSE_TAIL_PADDING_linux_x86_64 1
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC_linux_x86_64 1
#define TARG_RUNTIME_ELEM_COUNT_INT_KIND_linux_x86_64 ((an_integer_kind)ik_long)
#define TARG_SHORT_FIELD_ALIGNMENT_linux_x86_64 2
#define TARG_SIZEOF_DOUBLE_linux_x86_64 8
#define TARG_SIZEOF_FAR_POINTER_linux_x86_64 4
#define TARG_SIZEOF_FLOAT_linux_x86_64 4
#define TARG_SIZEOF_FLOAT128_linux_x86_64 16
#define TARG_SIZEOF_FLOAT80_linux_x86_64 12
#define TARG_SIZEOF_INT_linux_x86_64 4
#define TARG_SIZEOF_INT128_linux_x86_64 16
#define TARG_SIZEOF_LARGEST_FIXED_POINT_linux_x86_64 8
#define TARG_SIZEOF_LARGEST_ATOMIC_linux_x86_64 16
#define TARG_SIZEOF_LONG_linux_x86_64 8
#define TARG_SIZEOF_LONG_DOUBLE_linux_x86_64 16
#define TARG_SIZEOF_LONG_LONG_linux_x86_64 8
#define TARG_SIZEOF_NEAR_POINTER_linux_x86_64 2
#define TARG_SIZEOF_POINTER_linux_x86_64 8
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER_linux_x86_64 8
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION_linux_x86_64 ((((2*2+8-1)/8)+1)* 8)
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_x86_64 8
#define TARG_SIZEOF_SHORT_linux_x86_64 2
#define TARG_SIZEOF_SIGNED_ACCUM_linux_x86_64 4
#define TARG_SIZEOF_SIGNED_FRACT_linux_x86_64 2
#define TARG_SIZEOF_SIGNED_LONG_ACCUM_linux_x86_64 8
#define TARG_SIZEOF_SIGNED_LONG_FRACT_linux_x86_64 4
#define TARG_SIZEOF_SIGNED_SHORT_ACCUM_linux_x86_64 2
#define TARG_SIZEOF_SIGNED_SHORT_FRACT_linux_x86_64 1
#define TARG_SIZEOF_UNSIGNED_ACCUM_linux_x86_64 4
#define TARG_SIZEOF_UNSIGNED_FRACT_linux_x86_64 2
#define TARG_SIZEOF_UNSIGNED_LONG_ACCUM_linux_x86_64 8
#define TARG_SIZEOF_UNSIGNED_LONG_FRACT_linux_x86_64 4
#define TARG_SIZEOF_UNSIGNED_SHORT_ACCUM_linux_x86_64 2
#define TARG_SIZEOF_UNSIGNED_SHORT_FRACT_linux_x86_64 1
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO_linux_x86_64 8
#define TARG_SIZE_T_INT_KIND_linux_x86_64 ((an_integer_kind)ik_unsigned_long)
#define TARG_SIZE_T_MAX_linux_x86_64 ((a_targ_size_t)(9223372036854775807ULL * 2ULL + 1ULL))
#define TARG_SSIZE_T_INT_KIND_linux_x86_64 ((an_integer_kind)ik_long)
#define TARG_SUPPORTS_ARM32_linux_x86_64 0
#define TARG_SUPPORTS_ARM64_linux_x86_64 0
#define TARG_SUPPORTS_RISCV32_linux_x86_64 0
#define TARG_SUPPORTS_RISCV64_linux_x86_64 0
#define TARG_SUPPORTS_X86_64_linux_x86_64 1
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE_linux_x86_64 0
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_x86_64 0
#define TARG_UNWIND_WORD_MODE_linux_x86_64 ((a_type_mode_kind)tmk_DI)
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES_linux_x86_64 1
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS_linux_x86_64 1
#define TARG_VAR_HANDLE_INT_KIND_linux_x86_64 ((an_integer_kind)ik_unsigned_short)
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND_linux_x86_64 ((an_integer_kind)ik_short)
#define TARG_WCHAR_T_INT_KIND_linux_x86_64 ((an_integer_kind)ik_int)
#define TARG_WINT_T_INT_KIND_linux_x86_64 ((an_integer_kind)ik_unsigned_int)
#define TARG_WORD_MODE_linux_x86_64 ((a_type_mode_kind)tmk_DI)
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_x86_64 1
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT_linux_x86_64 4

/* Target configuration: linux_i686 */
#define TARGET_CONFIGURATION_4 linux_i686
#define TARG_ALIGNOF_DOUBLE_linux_i686 8
#define TARG_ALIGNOF_FAR_POINTER_linux_i686 4
#define TARG_ALIGNOF_FLOAT_linux_i686 4
#define TARG_ALIGNOF_FLOAT128_linux_i686 16
#define TARG_ALIGNOF_FLOAT80_linux_i686 4
#define TARG_ALIGNOF_INT_linux_i686 4
#define TARG_ALIGNOF_INT128_linux_i686 16
#define TARG_ALIGNOF_LONG_linux_i686 4
#define TARG_ALIGNOF_LONG_DOUBLE_linux_i686 4
#define TARG_ALIGNOF_LONG_LONG_linux_i686 8
#define TARG_ALIGNOF_NEAR_POINTER_linux_i686 2
#define TARG_ALIGNOF_POINTER_linux_i686 4
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER_linux_i686 4
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION_linux_i686 4
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_i686 4
#define TARG_ALIGNOF_SHORT_linux_i686 2
#define TARG_ALIGNOF_SIGNED_ACCUM_linux_i686 4
#define TARG_ALIGNOF_SIGNED_FRACT_linux_i686 2
#define TARG_ALIGNOF_SIGNED_LONG_ACCUM_linux_i686 8
#define TARG_ALIGNOF_SIGNED_LONG_FRACT_linux_i686 4
#define TARG_ALIGNOF_SIGNED_SHORT_ACCUM_linux_i686 2
#define TARG_ALIGNOF_SIGNED_SHORT_FRACT_linux_i686 1
#define TARG_ALIGNOF_UNSIGNED_ACCUM_linux_i686 4
#define TARG_ALIGNOF_UNSIGNED_FRACT_linux_i686 2
#define TARG_ALIGNOF_UNSIGNED_LONG_ACCUM_linux_i686 8
#define TARG_ALIGNOF_UNSIGNED_LONG_FRACT_linux_i686 4
#define TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM_linux_i686 2
#define TARG_ALIGNOF_UNSIGNED_SHORT_FRACT_linux_i686 1
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO_linux_i686 4
#define TARG_ALL_POINTERS_SAME_SIZE_linux_i686 1
#define TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT_linux_i686 1
#define TARG_BIT_FIELD_CONTAINER_SIZE_linux_i686 (-1)
#define TARG_BOOL_INT_KIND_linux_i686 ((an_integer_kind)ik_char)
#define TARG_C_BOOL_INT_KIND_linux_i686 ((an_integer_kind)ik_unsigned_char)
#define TARG_CHAR16_T_INT_KIND_linux_i686 ((an_integer_kind)ik_unsigned_short)
#define TARG_CHAR32_T_INT_KIND_linux_i686 ((an_integer_kind)ik_unsigned_int)
#define TARG_CHAR_BIT_linux_i686 8
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT_linux_i686 1
#define TARG_DBL_MANT_DIG_linux_i686 53
#define TARG_DBL_MAX_EXP_linux_i686 1024
#define TARG_DBL_MIN_EXP_linux_i686 (-1021)
#define TARG_DEFAULT_NEW_ALIGNMENT_linux_i686 16
#define TARG_DELTA_INT_KIND_linux_i686 ((an_integer_kind)ik_short)
#define TARG_DOUBLE_FIELD_ALIGNMENT_linux_i686 4
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES_linux_i686 1
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED_linux_i686 0
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT_linux_i686 0
#define TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE_linux_i686 1
#define TARG_FLOAT_FIELD_ALIGNMENT_linux_i686 4
#define TARG_FLOAT128_FIELD_ALIGNMENT_linux_i686 16
#define TARG_FLOAT80_FIELD_ALIGNMENT_linux_i686 4
#define TARG_FLT_MANT_DIG_linux_i686 24
#define TARG_FLT_MAX_EXP_linux_i686 128
#define TARG_FLT_MIN_EXP_linux_i686 (-125)
#define TARG_FLT128_MANT_DIG_linux_i686 113
#define TARG_FLT128_MAX_EXP_linux_i686 (16384)
#define TARG_FLT128_MIN_EXP_linux_i686 (-16381)
#define TARG_FLT80_MANT_DIG_linux_i686 64
#define TARG_FLT80_MAX_EXP_linux_i686 (16384)
#define TARG_FLT80_MIN_EXP_linux_i686 (-16381)
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED_linux_i686 1
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM_linux_i686 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT_linux_i686 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM_linux_i686 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT_linux_i686 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM_linux_i686 7
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT_linux_i686 7
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM_linux_i686 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT_linux_i686 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM_linux_i686 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT_linux_i686 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM_linux_i686 8
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT_linux_i686 8
#define TARG_HAS_SIGNED_CHARS_linux_i686 1
#define TARG_HOST_STRING_CHAR_BIT_linux_i686 8
#define TARG_INT_FIELD_ALIGNMENT_linux_i686 4
#define TARG_INT128_FIELD_ALIGNMENT_linux_i686 16
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT_linux_i686 0
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND_linux_i686 ((a_float_kind)fk_long_double)
#define TARG_JMP_BUF_ELEMENT_INT_KIND_linux_i686 ((an_integer_kind)ik_int)
#define TARG_JMP_BUF_NUM_ELEMENTS_linux_i686 39
#define TARG_SETJMP_FUNC_linux_i686 "_setjmp"
#define TARG_LDBL_MANT_DIG_linux_i686 64
#define TARG_LDBL_MAX_EXP_linux_i686 16384
#define TARG_LDBL_MIN_EXP_linux_i686 (-16381)
#define TARG_LIBGCC_CMP_RETURN_MODE_linux_i686 ((a_type_mode_kind)tmk_SI)
#define TARG_LIBGCC_SHIFT_COUNT_MODE_linux_i686 ((a_type_mode_kind)tmk_SI)
#define TARG_LITTLE_ENDIAN_linux_i686 1
#define TARG_LONG_DOUBLE_FIELD_ALIGNMENT_linux_i686 4
#define TARG_LONG_FIELD_ALIGNMENT_linux_i686 4
#define TARG_LONG_LONG_FIELD_ALIGNMENT_linux_i686 4
#define TARG_MAXIMUM_INTRINSIC_ALIGNMENT_linux_i686 16
#define TARG_MAX_BASE_CLASS_OFFSET_linux_i686 0
#define TARG_MAX_CLASS_OBJECT_SIZE_linux_i686 0
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION_linux_i686 0
#define TARG_MICROSOFT_PTR_TO_MEMBER_SIZING_linux_i686 BACK_END_IS_CP_GEN_BE
#define TARG_MINIMUM_STRUCT_ALIGNMENT_linux_i686 1
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED_linux_i686 1
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT_linux_i686 1
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE_linux_i686 1
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED_linux_i686 0
#define TARG_POINTER_MODE_linux_i686 ((a_type_mode_kind)tmk_SI)
#define TARG_PTRDIFF_T_INT_KIND_linux_i686 ((an_integer_kind)ik_int)
#define TARG_REGION_NUMBER_INT_KIND_linux_i686 ((an_integer_kind)ik_unsigned_short)
#define TARG_ETS_FLAG_TYPE_INT_KIND_linux_i686 ((an_integer_kind)ik_unsigned_int)
#define TARG_REUSE_TAIL_PADDING_linux_i686 1
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC_linux_i686 1
#define TARG_RUNTIME_ELEM_COUNT_INT_KIND_linux_i686 ((an_integer_kind)ik_int)
#define TARG_SHORT_FIELD_ALIGNMENT_linux_i686 2
#define TARG_SIZEOF_DOUBLE_linux_i686 8
#define TARG_SIZEOF_FAR_POINTER_linux_i686 4
#define TARG_SIZEOF_FLOAT_linux_i686 4
#define TARG_SIZEOF_FLOAT128_linux_i686 16
#define TARG_SIZEOF_FLOAT80_linux_i686 12
#define TARG_SIZEOF_INT_linux_i686 4
#define TARG_SIZEOF_INT128_linux_i686 16
#define TARG_SIZEOF_LARGEST_FIXED_POINT_linux_i686 8
#define TARG_SIZEOF_LARGEST_ATOMIC_linux_i686 8
#define TARG_SIZEOF_LONG_linux_i686 4
#define TARG_SIZEOF_LONG_DOUBLE_linux_i686 12
#define TARG_SIZEOF_LONG_LONG_linux_i686 8
#define TARG_SIZEOF_NEAR_POINTER_linux_i686 2
#define TARG_SIZEOF_POINTER_linux_i686 4
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER_linux_i686 4
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION_linux_i686 ((((2*2+4-1)/4)+1)* 4)
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_i686 4
#define TARG_SIZEOF_SHORT_linux_i686 2
#define TARG_SIZEOF_SIGNED_ACCUM_linux_i686 4
#define TARG_SIZEOF_SIGNED_FRACT_linux_i686 2
#define TARG_SIZEOF_SIGNED_LONG_ACCUM_linux_i686 8
#define TARG_SIZEOF_SIGNED_LONG_FRACT_linux_i686 4
#define TARG_SIZEOF_SIGNED_SHORT_ACCUM_linux_i686 2
#define TARG_SIZEOF_SIGNED_SHORT_FRACT_linux_i686 1
#define TARG_SIZEOF_UNSIGNED_ACCUM_linux_i686 4
#define TARG_SIZEOF_UNSIGNED_FRACT_linux_i686 2
#define TARG_SIZEOF_UNSIGNED_LONG_ACCUM_linux_i686 8
#define TARG_SIZEOF_UNSIGNED_LONG_FRACT_linux_i686 4
#define TARG_SIZEOF_UNSIGNED_SHORT_ACCUM_linux_i686 2
#define TARG_SIZEOF_UNSIGNED_SHORT_FRACT_linux_i686 1
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO_linux_i686 4
#define TARG_SIZE_T_INT_KIND_linux_i686 ((an_integer_kind)ik_unsigned_int)
#define TARG_SIZE_T_MAX_linux_i686 ((a_targ_size_t)(2147483647 * 2U + 1U))
#define TARG_SSIZE_T_INT_KIND_linux_i686 ((an_integer_kind)ik_int)
#define TARG_SUPPORTS_ARM32_linux_i686 0
#define TARG_SUPPORTS_ARM64_linux_i686 0
#define TARG_SUPPORTS_RISCV32_linux_i686 0
#define TARG_SUPPORTS_RISCV64_linux_i686 0
#define TARG_SUPPORTS_X86_64_linux_i686 0
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE_linux_i686 0
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_i686 0
#define TARG_UNWIND_WORD_MODE_linux_i686 ((a_type_mode_kind)tmk_SI)
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES_linux_i686 1
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS_linux_i686 1
#define TARG_VAR_HANDLE_INT_KIND_linux_i686 ((an_integer_kind)ik_unsigned_short)
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND_linux_i686 ((an_integer_kind)ik_short)
#define TARG_WCHAR_T_INT_KIND_linux_i686 ((an_integer_kind)ik_long)
#define TARG_WINT_T_INT_KIND_linux_i686 ((an_integer_kind)ik_unsigned_int)
#define TARG_WORD_MODE_linux_i686 ((a_type_mode_kind)tmk_SI)
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_i686 1
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT_linux_i686 4

/* Target configuration: linux_aarch64 */
#define TARGET_CONFIGURATION_5 linux_aarch64
#define TARG_ALIGNOF_DOUBLE_linux_aarch64 8
#define TARG_ALIGNOF_FAR_POINTER_linux_aarch64 4
#define TARG_ALIGNOF_FLOAT_linux_aarch64 4
#define TARG_ALIGNOF_FLOAT128_linux_aarch64 16
#define TARG_ALIGNOF_FLOAT80_linux_aarch64 16
#define TARG_ALIGNOF_INT_linux_aarch64 4
#define TARG_ALIGNOF_INT128_linux_aarch64 16
#define TARG_ALIGNOF_LONG_linux_aarch64 8
#define TARG_ALIGNOF_LONG_DOUBLE_linux_aarch64 16
#define TARG_ALIGNOF_LONG_LONG_linux_aarch64 8
#define TARG_ALIGNOF_NEAR_POINTER_linux_aarch64 2
#define TARG_ALIGNOF_POINTER_linux_aarch64 8
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER_linux_aarch64 8
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION_linux_aarch64 8
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_aarch64 8
#define TARG_ALIGNOF_SHORT_linux_aarch64 2
#define TARG_ALIGNOF_SIGNED_ACCUM_linux_aarch64 4
#define TARG_ALIGNOF_SIGNED_FRACT_linux_aarch64 2
#define TARG_ALIGNOF_SIGNED_LONG_ACCUM_linux_aarch64 8
#define TARG_ALIGNOF_SIGNED_LONG_FRACT_linux_aarch64 4
#define TARG_ALIGNOF_SIGNED_SHORT_ACCUM_linux_aarch64 2
#define TARG_ALIGNOF_SIGNED_SHORT_FRACT_linux_aarch64 1
#define TARG_ALIGNOF_UNSIGNED_ACCUM_linux_aarch64 4
#define TARG_ALIGNOF_UNSIGNED_FRACT_linux_aarch64 2
#define TARG_ALIGNOF_UNSIGNED_LONG_ACCUM_linux_aarch64 8
#define TARG_ALIGNOF_UNSIGNED_LONG_FRACT_linux_aarch64 4
#define TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM_linux_aarch64 2
#define TARG_ALIGNOF_UNSIGNED_SHORT_FRACT_linux_aarch64 1
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO_linux_aarch64 8
#define TARG_ALL_POINTERS_SAME_SIZE_linux_aarch64 1
#define TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT_linux_aarch64 1
#define TARG_BIT_FIELD_CONTAINER_SIZE_linux_aarch64 (-1)
#define TARG_BOOL_INT_KIND_linux_aarch64 ((an_integer_kind)ik_char)
#define TARG_C_BOOL_INT_KIND_linux_aarch64 ((an_integer_kind)ik_unsigned_char)
#define TARG_CHAR16_T_INT_KIND_linux_aarch64 ((an_integer_kind)ik_unsigned_short)
#define TARG_CHAR32_T_INT_KIND_linux_aarch64 ((an_integer_kind)ik_unsigned_int)
#define TARG_CHAR_BIT_linux_aarch64 8
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT_linux_aarch64 1
#define TARG_DBL_MANT_DIG_linux_aarch64 53
#define TARG_DBL_MAX_EXP_linux_aarch64 1024
#define TARG_DBL_MIN_EXP_linux_aarch64 (-1021)
#define TARG_DEFAULT_NEW_ALIGNMENT_linux_aarch64 16
#define TARG_DELTA_INT_KIND_linux_aarch64 ((an_integer_kind)ik_short)
#define TARG_DOUBLE_FIELD_ALIGNMENT_linux_aarch64 8
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES_linux_aarch64 0
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED_linux_aarch64 0
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT_linux_aarch64 0
#define TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE_linux_aarch64 1
#define TARG_FLOAT_FIELD_ALIGNMENT_linux_aarch64 4
#define TARG_FLOAT128_FIELD_ALIGNMENT_linux_aarch64 16
#define TARG_FLOAT80_FIELD_ALIGNMENT_linux_aarch64 16
#define TARG_FLT_MANT_DIG_linux_aarch64 24
#define TARG_FLT_MAX_EXP_linux_aarch64 128
#define TARG_FLT_MIN_EXP_linux_aarch64 (-125)
#define TARG_FLT128_MANT_DIG_linux_aarch64 113
#define TARG_FLT128_MAX_EXP_linux_aarch64 (16384)
#define TARG_FLT128_MIN_EXP_linux_aarch64 (-16381)
#define TARG_FLT80_MANT_DIG_linux_aarch64 64
#define TARG_FLT80_MAX_EXP_linux_aarch64 (16384)
#define TARG_FLT80_MIN_EXP_linux_aarch64 (-16381)
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED_linux_aarch64 1
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM_linux_aarch64 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT_linux_aarch64 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM_linux_aarch64 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT_linux_aarch64 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM_linux_aarch64 7
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT_linux_aarch64 7
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM_linux_aarch64 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT_linux_aarch64 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM_linux_aarch64 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT_linux_aarch64 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM_linux_aarch64 8
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT_linux_aarch64 8
#define TARG_HAS_SIGNED_CHARS_linux_aarch64 1
#define TARG_HOST_STRING_CHAR_BIT_linux_aarch64 8
#define TARG_INT_FIELD_ALIGNMENT_linux_aarch64 4
#define TARG_INT128_FIELD_ALIGNMENT_linux_aarch64 16
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT_linux_aarch64 0
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND_linux_aarch64 ((a_float_kind)fk_long_double)
#define TARG_JMP_BUF_ELEMENT_INT_KIND_linux_aarch64 ((an_integer_kind)ik_long)
#define TARG_JMP_BUF_NUM_ELEMENTS_linux_aarch64 25
#define TARG_SETJMP_FUNC_linux_aarch64 "_setjmp"
#define TARG_LDBL_MANT_DIG_linux_aarch64 64
#define TARG_LDBL_MAX_EXP_linux_aarch64 16384
#define TARG_LDBL_MIN_EXP_linux_aarch64 (-16381)
#define TARG_LIBGCC_CMP_RETURN_MODE_linux_aarch64 ((a_type_mode_kind)tmk_DI)
#define TARG_LIBGCC_SHIFT_COUNT_MODE_linux_aarch64 ((a_type_mode_kind)tmk_DI)
#define TARG_LITTLE_ENDIAN_linux_aarch64 1
#define TARG_LONG_DOUBLE_FIELD_ALIGNMENT_linux_aarch64 16
#define TARG_LONG_FIELD_ALIGNMENT_linux_aarch64 8
#define TARG_LONG_LONG_FIELD_ALIGNMENT_linux_aarch64 8
#define TARG_MAXIMUM_INTRINSIC_ALIGNMENT_linux_aarch64 16
#define TARG_MAX_BASE_CLASS_OFFSET_linux_aarch64 0
#define TARG_MAX_CLASS_OBJECT_SIZE_linux_aarch64 0
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION_linux_aarch64 0
#define TARG_MICROSOFT_PTR_TO_MEMBER_SIZING_linux_aarch64 BACK_END_IS_CP_GEN_BE
#define TARG_MINIMUM_STRUCT_ALIGNMENT_linux_aarch64 1
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED_linux_aarch64 1
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT_linux_aarch64 1
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE_linux_aarch64 1
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED_linux_aarch64 0
#define TARG_POINTER_MODE_linux_aarch64 ((a_type_mode_kind)tmk_DI)
#define TARG_PTRDIFF_T_INT_KIND_linux_aarch64 ((an_integer_kind)ik_long)
#define TARG_REGION_NUMBER_INT_KIND_linux_aarch64 ((an_integer_kind)ik_unsigned_short)
#define TARG_ETS_FLAG_TYPE_INT_KIND_linux_aarch64 ((an_integer_kind)ik_unsigned_int)
#define TARG_REUSE_TAIL_PADDING_linux_aarch64 1
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC_linux_aarch64 1
#define TARG_RUNTIME_ELEM_COUNT_INT_KIND_linux_aarch64 ((an_integer_kind)ik_long)
#define TARG_SHORT_FIELD_ALIGNMENT_linux_aarch64 2
#define TARG_SIZEOF_DOUBLE_linux_aarch64 8
#define TARG_SIZEOF_FAR_POINTER_linux_aarch64 4
#define TARG_SIZEOF_FLOAT_linux_aarch64 4
#define TARG_SIZEOF_FLOAT128_linux_aarch64 16
#define TARG_SIZEOF_FLOAT80_linux_aarch64 12
#define TARG_SIZEOF_INT_linux_aarch64 4
#define TARG_SIZEOF_INT128_linux_aarch64 16
#define TARG_SIZEOF_LARGEST_FIXED_POINT_linux_aarch64 8
#define TARG_SIZEOF_LARGEST_ATOMIC_linux_aarch64 16
#define TARG_SIZEOF_LONG_linux_aarch64 8
#define TARG_SIZEOF_LONG_DOUBLE_linux_aarch64 16
#define TARG_SIZEOF_LONG_LONG_linux_aarch64 8
#define TARG_SIZEOF_NEAR_POINTER_linux_aarch64 2
#define TARG_SIZEOF_POINTER_linux_aarch64 8
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER_linux_aarch64 8
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION_linux_aarch64 ((((2*2+8-1)/8)+1)* 8)
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_aarch64 8
#define TARG_SIZEOF_SHORT_linux_aarch64 2
#define TARG_SIZEOF_SIGNED_ACCUM_linux_aarch64 4
#define TARG_SIZEOF_SIGNED_FRACT_linux_aarch64 2
#define TARG_SIZEOF_SIGNED_LONG_ACCUM_linux_aarch64 8
#define TARG_SIZEOF_SIGNED_LONG_FRACT_linux_aarch64 4
#define TARG_SIZEOF_SIGNED_SHORT_ACCUM_linux_aarch64 2
#define TARG_SIZEOF_SIGNED_SHORT_FRACT_linux_aarch64 1
#define TARG_SIZEOF_UNSIGNED_ACCUM_linux_aarch64 4
#define TARG_SIZEOF_UNSIGNED_FRACT_linux_aarch64 2
#define TARG_SIZEOF_UNSIGNED_LONG_ACCUM_linux_aarch64 8
#define TARG_SIZEOF_UNSIGNED_LONG_FRACT_linux_aarch64 4
#define TARG_SIZEOF_UNSIGNED_SHORT_ACCUM_linux_aarch64 2
#define TARG_SIZEOF_UNSIGNED_SHORT_FRACT_linux_aarch64 1
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO_linux_aarch64 8
#define TARG_SIZE_T_INT_KIND_linux_aarch64 ((an_integer_kind)ik_unsigned_long)
#define TARG_SIZE_T_MAX_linux_aarch64 ((a_targ_size_t)(9223372036854775807ULL * 2ULL + 1ULL))
#define TARG_SSIZE_T_INT_KIND_linux_aarch64 ((an_integer_kind)ik_long)
#define TARG_SUPPORTS_ARM32_linux_aarch64 0
#define TARG_SUPPORTS_ARM64_linux_aarch64 1
#define TARG_SUPPORTS_RISCV32_linux_aarch64 0
#define TARG_SUPPORTS_RISCV64_linux_aarch64 0
#define TARG_SUPPORTS_X86_64_linux_aarch64 0
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE_linux_aarch64 0
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_aarch64 0
#define TARG_UNWIND_WORD_MODE_linux_aarch64 ((a_type_mode_kind)tmk_DI)
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES_linux_aarch64 1
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS_linux_aarch64 1
#define TARG_VAR_HANDLE_INT_KIND_linux_aarch64 ((an_integer_kind)ik_unsigned_short)
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND_linux_aarch64 ((an_integer_kind)ik_short)
#define TARG_WCHAR_T_INT_KIND_linux_aarch64 ((an_integer_kind)ik_int)
#define TARG_WINT_T_INT_KIND_linux_aarch64 ((an_integer_kind)ik_unsigned_int)
#define TARG_WORD_MODE_linux_aarch64 ((a_type_mode_kind)tmk_DI)
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_aarch64 1
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT_linux_aarch64 4

/* Target configuration: linux_armv7 */
#define TARGET_CONFIGURATION_6 linux_armv7
#define TARG_ALIGNOF_DOUBLE_linux_armv7 8
#define TARG_ALIGNOF_FAR_POINTER_linux_armv7 4
#define TARG_ALIGNOF_FLOAT_linux_armv7 4
#define TARG_ALIGNOF_FLOAT128_linux_armv7 16
#define TARG_ALIGNOF_FLOAT80_linux_armv7 16
#define TARG_ALIGNOF_INT_linux_armv7 4
#define TARG_ALIGNOF_INT128_linux_armv7 16
#define TARG_ALIGNOF_LONG_linux_armv7 4
#define TARG_ALIGNOF_LONG_DOUBLE_linux_armv7 8
#define TARG_ALIGNOF_LONG_LONG_linux_armv7 8
#define TARG_ALIGNOF_NEAR_POINTER_linux_armv7 2
#define TARG_ALIGNOF_POINTER_linux_armv7 4
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER_linux_armv7 4
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION_linux_armv7 4
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_armv7 4
#define TARG_ALIGNOF_SHORT_linux_armv7 2
#define TARG_ALIGNOF_SIGNED_ACCUM_linux_armv7 4
#define TARG_ALIGNOF_SIGNED_FRACT_linux_armv7 2
#define TARG_ALIGNOF_SIGNED_LONG_ACCUM_linux_armv7 8
#define TARG_ALIGNOF_SIGNED_LONG_FRACT_linux_armv7 4
#define TARG_ALIGNOF_SIGNED_SHORT_ACCUM_linux_armv7 2
#define TARG_ALIGNOF_SIGNED_SHORT_FRACT_linux_armv7 1
#define TARG_ALIGNOF_UNSIGNED_ACCUM_linux_armv7 4
#define TARG_ALIGNOF_UNSIGNED_FRACT_linux_armv7 2
#define TARG_ALIGNOF_UNSIGNED_LONG_ACCUM_linux_armv7 8
#define TARG_ALIGNOF_UNSIGNED_LONG_FRACT_linux_armv7 4
#define TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM_linux_armv7 2
#define TARG_ALIGNOF_UNSIGNED_SHORT_FRACT_linux_armv7 1
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO_linux_armv7 4
#define TARG_ALL_POINTERS_SAME_SIZE_linux_armv7 1
#define TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT_linux_armv7 1
#define TARG_BIT_FIELD_CONTAINER_SIZE_linux_armv7 (-1)
#define TARG_BOOL_INT_KIND_linux_armv7 ((an_integer_kind)ik_char)
#define TARG_C_BOOL_INT_KIND_linux_armv7 ((an_integer_kind)ik_unsigned_char)
#define TARG_CHAR16_T_INT_KIND_linux_armv7 ((an_integer_kind)ik_unsigned_short)
#define TARG_CHAR32_T_INT_KIND_linux_armv7 ((an_integer_kind)ik_unsigned_int)
#define TARG_CHAR_BIT_linux_armv7 8
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT_linux_armv7 1
#define TARG_DBL_MANT_DIG_linux_armv7 53
#define TARG_DBL_MAX_EXP_linux_armv7 1024
#define TARG_DBL_MIN_EXP_linux_armv7 (-1021)
#define TARG_DEFAULT_NEW_ALIGNMENT_linux_armv7 16
#define TARG_DELTA_INT_KIND_linux_armv7 ((an_integer_kind)ik_short)
#define TARG_DOUBLE_FIELD_ALIGNMENT_linux_armv7 8
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES_linux_armv7 0
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED_linux_armv7 0
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT_linux_armv7 0
#define TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE_linux_armv7 1
#define TARG_FLOAT_FIELD_ALIGNMENT_linux_armv7 4
#define TARG_FLOAT128_FIELD_ALIGNMENT_linux_armv7 16
#define TARG_FLOAT80_FIELD_ALIGNMENT_linux_armv7 16
#define TARG_FLT_MANT_DIG_linux_armv7 24
#define TARG_FLT_MAX_EXP_linux_armv7 128
#define TARG_FLT_MIN_EXP_linux_armv7 (-125)
#define TARG_FLT128_MANT_DIG_linux_armv7 113
#define TARG_FLT128_MAX_EXP_linux_armv7 (16384)
#define TARG_FLT128_MIN_EXP_linux_armv7 (-16381)
#define TARG_FLT80_MANT_DIG_linux_armv7 64
#define TARG_FLT80_MAX_EXP_linux_armv7 (16384)
#define TARG_FLT80_MIN_EXP_linux_armv7 (-16381)
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED_linux_armv7 1
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM_linux_armv7 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT_linux_armv7 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM_linux_armv7 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT_linux_armv7 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM_linux_armv7 7
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT_linux_armv7 7
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM_linux_armv7 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT_linux_armv7 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM_linux_armv7 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT_linux_armv7 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM_linux_armv7 8
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT_linux_armv7 8
#define TARG_HAS_SIGNED_CHARS_linux_armv7 1
#define TARG_HOST_STRING_CHAR_BIT_linux_armv7 8
#define TARG_INT_FIELD_ALIGNMENT_linux_armv7 4
#define TARG_INT128_FIELD_ALIGNMENT_linux_armv7 16
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT_linux_armv7 0
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND_linux_armv7 ((a_float_kind)fk_long_double)
#define TARG_JMP_BUF_ELEMENT_INT_KIND_linux_armv7 ((an_integer_kind)ik_int)
#define TARG_JMP_BUF_NUM_ELEMENTS_linux_armv7 25
#define TARG_SETJMP_FUNC_linux_armv7 "_setjmp"
#define TARG_LDBL_MANT_DIG_linux_armv7 53
#define TARG_LDBL_MAX_EXP_linux_armv7 1024
#define TARG_LDBL_MIN_EXP_linux_armv7 (-1021)
#define TARG_LIBGCC_CMP_RETURN_MODE_linux_armv7 ((a_type_mode_kind)tmk_SI)
#define TARG_LIBGCC_SHIFT_COUNT_MODE_linux_armv7 ((a_type_mode_kind)tmk_SI)
#define TARG_LITTLE_ENDIAN_linux_armv7 1
#define TARG_LONG_DOUBLE_FIELD_ALIGNMENT_linux_armv7 8
#define TARG_LONG_FIELD_ALIGNMENT_linux_armv7 4
#define TARG_LONG_LONG_FIELD_ALIGNMENT_linux_armv7 8
#define TARG_MAXIMUM_INTRINSIC_ALIGNMENT_linux_armv7 16
#define TARG_MAX_BASE_CLASS_OFFSET_linux_armv7 0
#define TARG_MAX_CLASS_OBJECT_SIZE_linux_armv7 0
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION_linux_armv7 0
#define TARG_MICROSOFT_PTR_TO_MEMBER_SIZING_linux_armv7 BACK_END_IS_CP_GEN_BE
#define TARG_MINIMUM_STRUCT_ALIGNMENT_linux_armv7 1
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED_linux_armv7 1
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT_linux_armv7 1
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE_linux_armv7 1
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED_linux_armv7 0
#define TARG_POINTER_MODE_linux_armv7 ((a_type_mode_kind)tmk_SI)
#define TARG_PTRDIFF_T_INT_KIND_linux_armv7 ((an_integer_kind)ik_int)
#define TARG_REGION_NUMBER_INT_KIND_linux_armv7 ((an_integer_kind)ik_unsigned_short)
#define TARG_ETS_FLAG_TYPE_INT_KIND_linux_armv7 ((an_integer_kind)ik_unsigned_int)
#define TARG_REUSE_TAIL_PADDING_linux_armv7 1
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC_linux_armv7 1
#define TARG_RUNTIME_ELEM_COUNT_INT_KIND_linux_armv7 ((an_integer_kind)ik_int)
#define TARG_SHORT_FIELD_ALIGNMENT_linux_armv7 2
#define TARG_SIZEOF_DOUBLE_linux_armv7 8
#define TARG_SIZEOF_FAR_POINTER_linux_armv7 4
#define TARG_SIZEOF_FLOAT_linux_armv7 4
#define TARG_SIZEOF_FLOAT128_linux_armv7 16
#define TARG_SIZEOF_FLOAT80_linux_armv7 12
#define TARG_SIZEOF_INT_linux_armv7 4
#define TARG_SIZEOF_INT128_linux_armv7 16
#define TARG_SIZEOF_LARGEST_FIXED_POINT_linux_armv7 8
#define TARG_SIZEOF_LARGEST_ATOMIC_linux_armv7 8
#define TARG_SIZEOF_LONG_linux_armv7 4
#define TARG_SIZEOF_LONG_DOUBLE_linux_armv7 8
#define TARG_SIZEOF_LONG_LONG_linux_armv7 8
#define TARG_SIZEOF_NEAR_POINTER_linux_armv7 2
#define TARG_SIZEOF_POINTER_linux_armv7 4
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER_linux_armv7 4
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION_linux_armv7 ((((2*2+4-1)/4)+1)* 4)
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_armv7 4
#define TARG_SIZEOF_SHORT_linux_armv7 2
#define TARG_SIZEOF_SIGNED_ACCUM_linux_armv7 4
#define TARG_SIZEOF_SIGNED_FRACT_linux_armv7 2
#define TARG_SIZEOF_SIGNED_LONG_ACCUM_linux_armv7 8
#define TARG_SIZEOF_SIGNED_LONG_FRACT_linux_armv7 4
#define TARG_SIZEOF_SIGNED_SHORT_ACCUM_linux_armv7 2
#define TARG_SIZEOF_SIGNED_SHORT_FRACT_linux_armv7 1
#define TARG_SIZEOF_UNSIGNED_ACCUM_linux_armv7 4
#define TARG_SIZEOF_UNSIGNED_FRACT_linux_armv7 2
#define TARG_SIZEOF_UNSIGNED_LONG_ACCUM_linux_armv7 8
#define TARG_SIZEOF_UNSIGNED_LONG_FRACT_linux_armv7 4
#define TARG_SIZEOF_UNSIGNED_SHORT_ACCUM_linux_armv7 2
#define TARG_SIZEOF_UNSIGNED_SHORT_FRACT_linux_armv7 1
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO_linux_armv7 4
#define TARG_SIZE_T_INT_KIND_linux_armv7 ((an_integer_kind)ik_unsigned_int)
#define TARG_SIZE_T_MAX_linux_armv7 ((a_targ_size_t)(2147483647 * 2U + 1U))
#define TARG_SSIZE_T_INT_KIND_linux_armv7 ((an_integer_kind)ik_int)
#define TARG_SUPPORTS_ARM32_linux_armv7 1
#define TARG_SUPPORTS_ARM64_linux_armv7 0
#define TARG_SUPPORTS_RISCV32_linux_armv7 0
#define TARG_SUPPORTS_RISCV64_linux_armv7 0
#define TARG_SUPPORTS_X86_64_linux_armv7 0
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE_linux_armv7 0
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_armv7 0
#define TARG_UNWIND_WORD_MODE_linux_armv7 ((a_type_mode_kind)tmk_DI)
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES_linux_armv7 1
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS_linux_armv7 1
#define TARG_VAR_HANDLE_INT_KIND_linux_armv7 ((an_integer_kind)ik_unsigned_short)
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND_linux_armv7 ((an_integer_kind)ik_short)
#define TARG_WCHAR_T_INT_KIND_linux_armv7 ((an_integer_kind)ik_unsigned_int)
#define TARG_WINT_T_INT_KIND_linux_armv7 ((an_integer_kind)ik_unsigned_int)
#define TARG_WORD_MODE_linux_armv7 ((a_type_mode_kind)tmk_SI)
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_armv7 1
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT_linux_armv7 4

/* Target configuration: linux_riscv64 */
#define TARGET_CONFIGURATION_7 linux_riscv64
#define TARG_ALIGNOF_DOUBLE_linux_riscv64 8
#define TARG_ALIGNOF_FAR_POINTER_linux_riscv64 4
#define TARG_ALIGNOF_FLOAT_linux_riscv64 4
#define TARG_ALIGNOF_FLOAT128_linux_riscv64 16
#define TARG_ALIGNOF_FLOAT80_linux_riscv64 16
#define TARG_ALIGNOF_INT_linux_riscv64 4
#define TARG_ALIGNOF_INT128_linux_riscv64 16
#define TARG_ALIGNOF_LONG_linux_riscv64 8
#define TARG_ALIGNOF_LONG_DOUBLE_linux_riscv64 16
#define TARG_ALIGNOF_LONG_LONG_linux_riscv64 8
#define TARG_ALIGNOF_NEAR_POINTER_linux_riscv64 2
#define TARG_ALIGNOF_POINTER_linux_riscv64 8
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER_linux_riscv64 8
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION_linux_riscv64 8
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_riscv64 8
#define TARG_ALIGNOF_SHORT_linux_riscv64 2
#define TARG_ALIGNOF_SIGNED_ACCUM_linux_riscv64 4
#define TARG_ALIGNOF_SIGNED_FRACT_linux_riscv64 2
#define TARG_ALIGNOF_SIGNED_LONG_ACCUM_linux_riscv64 8
#define TARG_ALIGNOF_SIGNED_LONG_FRACT_linux_riscv64 4
#define TARG_ALIGNOF_SIGNED_SHORT_ACCUM_linux_riscv64 2
#define TARG_ALIGNOF_SIGNED_SHORT_FRACT_linux_riscv64 1
#define TARG_ALIGNOF_UNSIGNED_ACCUM_linux_riscv64 4
#define TARG_ALIGNOF_UNSIGNED_FRACT_linux_riscv64 2
#define TARG_ALIGNOF_UNSIGNED_LONG_ACCUM_linux_riscv64 8
#define TARG_ALIGNOF_UNSIGNED_LONG_FRACT_linux_riscv64 4
#define TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM_linux_riscv64 2
#define TARG_ALIGNOF_UNSIGNED_SHORT_FRACT_linux_riscv64 1
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO_linux_riscv64 8
#define TARG_ALL_POINTERS_SAME_SIZE_linux_riscv64 1
#define TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT_linux_riscv64 1
#define TARG_BIT_FIELD_CONTAINER_SIZE_linux_riscv64 (-1)
#define TARG_BOOL_INT_KIND_linux_riscv64 ((an_integer_kind)ik_char)
#define TARG_C_BOOL_INT_KIND_linux_riscv64 ((an_integer_kind)ik_unsigned_char)
#define TARG_CHAR16_T_INT_KIND_linux_riscv64 ((an_integer_kind)ik_unsigned_short)
#define TARG_CHAR32_T_INT_KIND_linux_riscv64 ((an_integer_kind)ik_unsigned_int)
#define TARG_CHAR_BIT_linux_riscv64 8
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT_linux_riscv64 1
#define TARG_DBL_MANT_DIG_linux_riscv64 53
#define TARG_DBL_MAX_EXP_linux_riscv64 1024
#define TARG_DBL_MIN_EXP_linux_riscv64 (-1021)
#define TARG_DEFAULT_NEW_ALIGNMENT_linux_riscv64 16
#define TARG_DELTA_INT_KIND_linux_riscv64 ((an_integer_kind)ik_short)
#define TARG_DOUBLE_FIELD_ALIGNMENT_linux_riscv64 8
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES_linux_riscv64 0
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED_linux_riscv64 0
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT_linux_riscv64 0
#define TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE_linux_riscv64 1
#define TARG_FLOAT_FIELD_ALIGNMENT_linux_riscv64 4
#define TARG_FLOAT128_FIELD_ALIGNMENT_linux_riscv64 16
#define TARG_FLOAT80_FIELD_ALIGNMENT_linux_riscv64 16
#define TARG_FLT_MANT_DIG_linux_riscv64 24
#define TARG_FLT_MAX_EXP_linux_riscv64 128
#define TARG_FLT_MIN_EXP_linux_riscv64 (-125)
#define TARG_FLT128_MANT_DIG_linux_riscv64 113
#define TARG_FLT128_MAX_EXP_linux_riscv64 (16384)
#define TARG_FLT128_MIN_EXP_linux_riscv64 (-16381)
#define TARG_FLT80_MANT_DIG_linux_riscv64 64
#define TARG_FLT80_MAX_EXP_linux_riscv64 (16384)
#define TARG_FLT80_MIN_EXP_linux_riscv64 (-16381)
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED_linux_riscv64 1
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM_linux_riscv64 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT_linux_riscv64 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM_linux_riscv64 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT_linux_riscv64 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM_linux_riscv64 7
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT_linux_riscv64 7
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM_linux_riscv64 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT_linux_riscv64 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM_linux_riscv64 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT_linux_riscv64 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM_linux_riscv64 8
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT_linux_riscv64 8
#define TARG_HAS_SIGNED_CHARS_linux_riscv64 0
#define TARG_HOST_STRING_CHAR_BIT_linux_riscv64 8
#define TARG_INT_FIELD_ALIGNMENT_linux_riscv64 4
#define TARG_INT128_FIELD_ALIGNMENT_linux_riscv64 16
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT_linux_riscv64 0
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND_linux_riscv64 ((a_float_kind)fk_long_double)
#define TARG_JMP_BUF_ELEMENT_INT_KIND_linux_riscv64 ((an_integer_kind)ik_long)
#define TARG_JMP_BUF_NUM_ELEMENTS_linux_riscv64 43
#define TARG_SETJMP_FUNC_linux_riscv64 "_setjmp"
#define TARG_LDBL_MANT_DIG_linux_riscv64 113
#define TARG_LDBL_MAX_EXP_linux_riscv64 16384
#define TARG_LDBL_MIN_EXP_linux_riscv64 (-16381)
#define TARG_LIBGCC_CMP_RETURN_MODE_linux_riscv64 ((a_type_mode_kind)tmk_DI)
#define TARG_LIBGCC_SHIFT_COUNT_MODE_linux_riscv64 ((a_type_mode_kind)tmk_DI)
#define TARG_LITTLE_ENDIAN_linux_riscv64 1
#define TARG_LONG_DOUBLE_FIELD_ALIGNMENT_linux_riscv64 16
#define TARG_LONG_FIELD_ALIGNMENT_linux_riscv64 8
#define TARG_LONG_LONG_FIELD_ALIGNMENT_linux_riscv64 8
#define TARG_MAXIMUM_INTRINSIC_ALIGNMENT_linux_riscv64 16
#define TARG_MAX_BASE_CLASS_OFFSET_linux_riscv64 0
#define TARG_MAX_CLASS_OBJECT_SIZE_linux_riscv64 0
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION_linux_riscv64 0
#define TARG_MICROSOFT_PTR_TO_MEMBER_SIZING_linux_riscv64 BACK_END_IS_CP_GEN_BE
#define TARG_MINIMUM_STRUCT_ALIGNMENT_linux_riscv64 1
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED_linux_riscv64 1
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT_linux_riscv64 1
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE_linux_riscv64 1
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED_linux_riscv64 0
#define TARG_POINTER_MODE_linux_riscv64 ((a_type_mode_kind)tmk_DI)
#define TARG_PTRDIFF_T_INT_KIND_linux_riscv64 ((an_integer_kind)ik_long)
#define TARG_REGION_NUMBER_INT_KIND_linux_riscv64 ((an_integer_kind)ik_unsigned_short)
#define TARG_ETS_FLAG_TYPE_INT_KIND_linux_riscv64 ((an_integer_kind)ik_unsigned_int)
#define TARG_REUSE_TAIL_PADDING_linux_riscv64 1
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC_linux_riscv64 1
#define TARG_RUNTIME_ELEM_COUNT_INT_KIND_linux_riscv64 ((an_integer_kind)ik_long)
#define TARG_SHORT_FIELD_ALIGNMENT_linux_riscv64 2
#define TARG_SIZEOF_DOUBLE_linux_riscv64 8
#define TARG_SIZEOF_FAR_POINTER_linux_riscv64 4
#define TARG_SIZEOF_FLOAT_linux_riscv64 4
#define TARG_SIZEOF_FLOAT128_linux_riscv64 16
#define TARG_SIZEOF_FLOAT80_linux_riscv64 12
#define TARG_SIZEOF_INT_linux_riscv64 4
#define TARG_SIZEOF_INT128_linux_riscv64 16
#define TARG_SIZEOF_LARGEST_FIXED_POINT_linux_riscv64 8
#define TARG_SIZEOF_LARGEST_ATOMIC_linux_riscv64 16
#define TARG_SIZEOF_LONG_linux_riscv64 8
#define TARG_SIZEOF_LONG_DOUBLE_linux_riscv64 16
#define TARG_SIZEOF_LONG_LONG_linux_riscv64 8
#define TARG_SIZEOF_NEAR_POINTER_linux_riscv64 2
#define TARG_SIZEOF_POINTER_linux_riscv64 8
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER_linux_riscv64 8
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION_linux_riscv64 ((((2*2+8-1)/8)+1)* 8)
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_riscv64 8
#define TARG_SIZEOF_SHORT_linux_riscv64 2
#define TARG_SIZEOF_SIGNED_ACCUM_linux_riscv64 4
#define TARG_SIZEOF_SIGNED_FRACT_linux_riscv64 2
#define TARG_SIZEOF_SIGNED_LONG_ACCUM_linux_riscv64 8
#define TARG_SIZEOF_SIGNED_LONG_FRACT_linux_riscv64 4
#define TARG_SIZEOF_SIGNED_SHORT_ACCUM_linux_riscv64 2
#define TARG_SIZEOF_SIGNED_SHORT_FRACT_linux_riscv64 1
#define TARG_SIZEOF_UNSIGNED_ACCUM_linux_riscv64 4
#define TARG_SIZEOF_UNSIGNED_FRACT_linux_riscv64 2
#define TARG_SIZEOF_UNSIGNED_LONG_ACCUM_linux_riscv64 8
#define TARG_SIZEOF_UNSIGNED_LONG_FRACT_linux_riscv64 4
#define TARG_SIZEOF_UNSIGNED_SHORT_ACCUM_linux_riscv64 2
#define TARG_SIZEOF_UNSIGNED_SHORT_FRACT_linux_riscv64 1
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO_linux_riscv64 8
#define TARG_SIZE_T_INT_KIND_linux_riscv64 ((an_integer_kind)ik_unsigned_long)
#define TARG_SIZE_T_MAX_linux_riscv64 ((a_targ_size_t)(9223372036854775807ULL * 2ULL + 1ULL))
#define TARG_SSIZE_T_INT_KIND_linux_riscv64 ((an_integer_kind)ik_long)
#define TARG_SUPPORTS_ARM32_linux_riscv64 0
#define TARG_SUPPORTS_ARM64_linux_riscv64 0
#define TARG_SUPPORTS_RISCV32_linux_riscv64 0
#define TARG_SUPPORTS_RISCV64_linux_riscv64 1
#define TARG_SUPPORTS_X86_64_linux_riscv64 0
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE_linux_riscv64 0
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_riscv64 0
#define TARG_UNWIND_WORD_MODE_linux_riscv64 ((a_type_mode_kind)tmk_DI)
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES_linux_riscv64 1
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS_linux_riscv64 1
#define TARG_VAR_HANDLE_INT_KIND_linux_riscv64 ((an_integer_kind)ik_unsigned_short)
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND_linux_riscv64 ((an_integer_kind)ik_short)
#define TARG_WCHAR_T_INT_KIND_linux_riscv64 ((an_integer_kind)ik_int)
#define TARG_WINT_T_INT_KIND_linux_riscv64 ((an_integer_kind)ik_unsigned_int)
#define TARG_WORD_MODE_linux_riscv64 ((a_type_mode_kind)tmk_DI)
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_riscv64 1
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT_linux_riscv64 4

/* Target configuration: linux_riscv32 */
#define TARGET_CONFIGURATION_8 linux_riscv32
#define TARG_ALIGNOF_DOUBLE_linux_riscv32 8
#define TARG_ALIGNOF_FAR_POINTER_linux_riscv32 4
#define TARG_ALIGNOF_FLOAT_linux_riscv32 4
#define TARG_ALIGNOF_FLOAT128_linux_riscv32 16
#define TARG_ALIGNOF_FLOAT80_linux_riscv32 16
#define TARG_ALIGNOF_INT_linux_riscv32 4
#define TARG_ALIGNOF_INT128_linux_riscv32 16
#define TARG_ALIGNOF_LONG_linux_riscv32 4
#define TARG_ALIGNOF_LONG_DOUBLE_linux_riscv32 8
#define TARG_ALIGNOF_LONG_LONG_linux_riscv32 8
#define TARG_ALIGNOF_NEAR_POINTER_linux_riscv32 2
#define TARG_ALIGNOF_POINTER_linux_riscv32 4
#define TARG_ALIGNOF_PTR_TO_DATA_MEMBER_linux_riscv32 4
#define TARG_ALIGNOF_PTR_TO_MEMBER_FUNCTION_linux_riscv32 4
#define TARG_ALIGNOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_riscv32 4
#define TARG_ALIGNOF_SHORT_linux_riscv32 2
#define TARG_ALIGNOF_SIGNED_ACCUM_linux_riscv32 4
#define TARG_ALIGNOF_SIGNED_FRACT_linux_riscv32 2
#define TARG_ALIGNOF_SIGNED_LONG_ACCUM_linux_riscv32 8
#define TARG_ALIGNOF_SIGNED_LONG_FRACT_linux_riscv32 4
#define TARG_ALIGNOF_SIGNED_SHORT_ACCUM_linux_riscv32 2
#define TARG_ALIGNOF_SIGNED_SHORT_FRACT_linux_riscv32 1
#define TARG_ALIGNOF_UNSIGNED_ACCUM_linux_riscv32 4
#define TARG_ALIGNOF_UNSIGNED_FRACT_linux_riscv32 2
#define TARG_ALIGNOF_UNSIGNED_LONG_ACCUM_linux_riscv32 8
#define TARG_ALIGNOF_UNSIGNED_LONG_FRACT_linux_riscv32 4
#define TARG_ALIGNOF_UNSIGNED_SHORT_ACCUM_linux_riscv32 2
#define TARG_ALIGNOF_UNSIGNED_SHORT_FRACT_linux_riscv32 1
#define TARG_ALIGNOF_VIRTUAL_FUNCTION_INFO_linux_riscv32 4
#define TARG_ALL_POINTERS_SAME_SIZE_linux_riscv32 1
#define TARG_BIT_FIELD_AFFECTS_UNION_ALIGNMENT_linux_riscv32 1
#define TARG_BIT_FIELD_CONTAINER_SIZE_linux_riscv32 (-1)
#define TARG_BOOL_INT_KIND_linux_riscv32 ((an_integer_kind)ik_char)
#define TARG_C_BOOL_INT_KIND_linux_riscv32 ((an_integer_kind)ik_unsigned_char)
#define TARG_CHAR16_T_INT_KIND_linux_riscv32 ((an_integer_kind)ik_unsigned_short)
#define TARG_CHAR32_T_INT_KIND_linux_riscv32 ((an_integer_kind)ik_unsigned_int)
#define TARG_CHAR_BIT_linux_riscv32 8
#define TARG_CHAR_CONSTANT_FIRST_CHAR_MOST_SIGNIFICANT_linux_riscv32 1
#define TARG_DBL_MANT_DIG_linux_riscv32 53
#define TARG_DBL_MAX_EXP_linux_riscv32 1024
#define TARG_DBL_MIN_EXP_linux_riscv32 (-1021)
#define TARG_DEFAULT_NEW_ALIGNMENT_linux_riscv32 16
#define TARG_DELTA_INT_KIND_linux_riscv32 ((an_integer_kind)ik_short)
#define TARG_DOUBLE_FIELD_ALIGNMENT_linux_riscv32 8
#define TARG_DUAL_ALIGNMENTS_FOR_BUILTIN_TYPES_linux_riscv32 0
#define TARG_ENUM_BIT_FIELDS_ARE_ALWAYS_UNSIGNED_linux_riscv32 0
#define TARG_ENUM_TYPES_CAN_BE_SMALLER_THAN_INT_linux_riscv32 0
#define TARG_FIELD_ALLOC_SEQUENCE_EQUALS_DECL_SEQUENCE_linux_riscv32 1
#define TARG_FLOAT_FIELD_ALIGNMENT_linux_riscv32 4
#define TARG_FLOAT128_FIELD_ALIGNMENT_linux_riscv32 16
#define TARG_FLOAT80_FIELD_ALIGNMENT_linux_riscv32 16
#define TARG_FLT_MANT_DIG_linux_riscv32 24
#define TARG_FLT_MAX_EXP_linux_riscv32 128
#define TARG_FLT_MIN_EXP_linux_riscv32 (-125)
#define TARG_FLT128_MANT_DIG_linux_riscv32 113
#define TARG_FLT128_MAX_EXP_linux_riscv32 (16384)
#define TARG_FLT128_MIN_EXP_linux_riscv32 (-16381)
#define TARG_FLT80_MANT_DIG_linux_riscv32 64
#define TARG_FLT80_MAX_EXP_linux_riscv32 (16384)
#define TARG_FLT80_MIN_EXP_linux_riscv32 (-16381)
#define TARG_FORCE_ONE_BIT_BIT_FIELD_TO_BE_UNSIGNED_linux_riscv32 1
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_ACCUM_linux_riscv32 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_FRACT_linux_riscv32 15
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_ACCUM_linux_riscv32 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_LONG_FRACT_linux_riscv32 31
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_ACCUM_linux_riscv32 7
#define TARG_FRACTIONAL_BITS_FOR_SIGNED_SHORT_FRACT_linux_riscv32 7
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_ACCUM_linux_riscv32 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_FRACT_linux_riscv32 16
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_ACCUM_linux_riscv32 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_LONG_FRACT_linux_riscv32 32
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_ACCUM_linux_riscv32 8
#define TARG_FRACTIONAL_BITS_FOR_UNSIGNED_SHORT_FRACT_linux_riscv32 8
#define TARG_HAS_SIGNED_CHARS_linux_riscv32 0
#define TARG_HOST_STRING_CHAR_BIT_linux_riscv32 8
#define TARG_INT_FIELD_ALIGNMENT_linux_riscv32 4
#define TARG_INT128_FIELD_ALIGNMENT_linux_riscv32 16
#define TARG_JMP_BUF_ELEMENTS_ARE_FLOAT_linux_riscv32 0
#define TARG_JMP_BUF_ELEMENT_FLOAT_KIND_linux_riscv32 ((a_float_kind)fk_long_double)
#define TARG_JMP_BUF_ELEMENT_INT_KIND_linux_riscv32 ((an_integer_kind)ik_unsigned_long_long)
#define TARG_JMP_BUF_NUM_ELEMENTS_linux_riscv32 36
#define TARG_SETJMP_FUNC_linux_riscv32 "_setjmp"
#define TARG_LDBL_MANT_DIG_linux_riscv32 113
#define TARG_LDBL_MAX_EXP_linux_riscv32 16384
#define TARG_LDBL_MIN_EXP_linux_riscv32 (-16381)
#define TARG_LIBGCC_CMP_RETURN_MODE_linux_riscv32 ((a_type_mode_kind)tmk_SI)
#define TARG_LIBGCC_SHIFT_COUNT_MODE_linux_riscv32 ((a_type_mode_kind)tmk_SI)
#define TARG_LITTLE_ENDIAN_linux_riscv32 1
#define TARG_LONG_DOUBLE_FIELD_ALIGNMENT_linux_riscv32 8
#define TARG_LONG_FIELD_ALIGNMENT_linux_riscv32 4
#define TARG_LONG_LONG_FIELD_ALIGNMENT_linux_riscv32 8
#define TARG_MAXIMUM_INTRINSIC_ALIGNMENT_linux_riscv32 16
#define TARG_MAX_BASE_CLASS_OFFSET_linux_riscv32 0
#define TARG_MAX_CLASS_OBJECT_SIZE_linux_riscv32 0
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION_linux_riscv32 0
#define TARG_MICROSOFT_PTR_TO_MEMBER_SIZING_linux_riscv32 BACK_END_IS_CP_GEN_BE
#define TARG_MINIMUM_STRUCT_ALIGNMENT_linux_riscv32 1
#define TARG_NONNEGATIVE_ENUM_BIT_FIELD_IS_UNSIGNED_linux_riscv32 1
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT_linux_riscv32 1
#define TARG_PAD_BIT_FIELDS_LARGER_THAN_BASE_TYPE_linux_riscv32 1
#define TARG_PLAIN_INT_BIT_FIELD_IS_UNSIGNED_linux_riscv32 0
#define TARG_POINTER_MODE_linux_riscv32 ((a_type_mode_kind)tmk_SI)
#define TARG_PTRDIFF_T_INT_KIND_linux_riscv32 ((an_integer_kind)ik_int)
#define TARG_REGION_NUMBER_INT_KIND_linux_riscv32 ((an_integer_kind)ik_unsigned_short)
#define TARG_ETS_FLAG_TYPE_INT_KIND_linux_riscv32 ((an_integer_kind)ik_unsigned_int)
#define TARG_REUSE_TAIL_PADDING_linux_riscv32 1
#define TARG_RIGHT_SHIFT_IS_ARITHMETIC_linux_riscv32 1
#define TARG_RUNTIME_ELEM_COUNT_INT_KIND_linux_riscv32 ((an_integer_kind)ik_int)
#define TARG_SHORT_FIELD_ALIGNMENT_linux_riscv32 2
#define TARG_SIZEOF_DOUBLE_linux_riscv32 8
#define TARG_SIZEOF_FAR_POINTER_linux_riscv32 4
#define TARG_SIZEOF_FLOAT_linux_riscv32 4
#define TARG_SIZEOF_FLOAT128_linux_riscv32 16
#define TARG_SIZEOF_FLOAT80_linux_riscv32 12
#define TARG_SIZEOF_INT_linux_riscv32 4
#define TARG_SIZEOF_INT128_linux_riscv32 16
#define TARG_SIZEOF_LARGEST_FIXED_POINT_linux_riscv32 8
#define TARG_SIZEOF_LARGEST_ATOMIC_linux_riscv32 8
#define TARG_SIZEOF_LONG_linux_riscv32 4
#define TARG_SIZEOF_LONG_DOUBLE_linux_riscv32 8
#define TARG_SIZEOF_LONG_LONG_linux_riscv32 8
#define TARG_SIZEOF_NEAR_POINTER_linux_riscv32 2
#define TARG_SIZEOF_POINTER_linux_riscv32 4
#define TARG_SIZEOF_PTR_TO_DATA_MEMBER_linux_riscv32 4
#define TARG_SIZEOF_PTR_TO_MEMBER_FUNCTION_linux_riscv32 ((((2*2+4-1)/4)+1)* 4)
#define TARG_SIZEOF_PTR_TO_VIRTUAL_BASE_CLASS_linux_riscv32 4
#define TARG_SIZEOF_SHORT_linux_riscv32 2
#define TARG_SIZEOF_SIGNED_ACCUM_linux_riscv32 4
#define TARG_SIZEOF_SIGNED_FRACT_linux_riscv32 2
#define TARG_SIZEOF_SIGNED_LONG_ACCUM_linux_riscv32 8
#define TARG_SIZEOF_SIGNED_LONG_FRACT_linux_riscv32 4
#define TARG_SIZEOF_SIGNED_SHORT_ACCUM_linux_riscv32 2
#define TARG_SIZEOF_SIGNED_SHORT_FRACT_linux_riscv32 1
#define TARG_SIZEOF_UNSIGNED_ACCUM_linux_riscv32 4
#define TARG_SIZEOF_UNSIGNED_FRACT_linux_riscv32 2
#define TARG_SIZEOF_UNSIGNED_LONG_ACCUM_linux_riscv32 8
#define TARG_SIZEOF_UNSIGNED_LONG_FRACT_linux_riscv32 4
#define TARG_SIZEOF_UNSIGNED_SHORT_ACCUM_linux_riscv32 2
#define TARG_SIZEOF_UNSIGNED_SHORT_FRACT_linux_riscv32 1
#define TARG_SIZEOF_VIRTUAL_FUNCTION_INFO_linux_riscv32 4
#define TARG_SIZE_T_INT_KIND_linux_riscv32 ((an_integer_kind)ik_unsigned_int)
#define TARG_SIZE_T_MAX_linux_riscv32 ((a_targ_size_t)(2147483647 * 2U + 1U))
#define TARG_SSIZE_T_INT_KIND_linux_riscv32 ((an_integer_kind)ik_int)
#define TARG_SUPPORTS_ARM32_linux_riscv32 0
#define TARG_SUPPORTS_ARM64_linux_riscv32 0
#define TARG_SUPPORTS_RISCV32_linux_riscv32 1
#define TARG_SUPPORTS_RISCV64_linux_riscv32 0
#define TARG_SUPPORTS_X86_64_linux_riscv32 0
#define TARG_TOO_LARGE_SHIFT_COUNT_IS_TAKEN_MODULO_SIZE_linux_riscv32 0
#define TARG_UNNAMED_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_riscv32 0
#define TARG_UNWIND_WORD_MODE_linux_riscv32 ((a_type_mode_kind)tmk_DI)
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BASE_CLASSES_linux_riscv32 1
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS_linux_riscv32 1
#define TARG_VAR_HANDLE_INT_KIND_linux_riscv32 ((an_integer_kind)ik_unsigned_short)
#define TARG_VIRTUAL_FUNCTION_INDEX_INT_KIND_linux_riscv32 ((an_integer_kind)ik_short)
#define TARG_WCHAR_T_INT_KIND_linux_riscv32 ((an_integer_kind)ik_unsigned_int)
#define TARG_WINT_T_INT_KIND_linux_riscv32 ((an_integer_kind)ik_unsigned_int)
#define TARG_WORD_MODE_linux_riscv32 ((a_type_mode_kind)tmk_SI)
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT_linux_riscv32 1
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT_linux_riscv32 4

#endif /* INCLUDE_ADDITIONAL_TARGET_CONFIGURATION */

#else /* ifndef __CYGWIN__ */
/* Options for UnixWare test version. */
#define __ANSIC__ 1
#define COMPILE_MULTIPLE_SOURCE_FILES 1
#define TARG_EXTERNAL_NAMES_GET_UNDERSCORE_ADDED 0
#ifndef INCLUDE_EDG_TEST_PRAGMAS
#define INCLUDE_EDG_TEST_PRAGMAS 1
#endif /* ifndef INCLUDE_EDG_TEST_PRAGMAS */
#define TARG_ALIGNOF_DOUBLE 4
#define TARG_ALIGNOF_LONG_DOUBLE 4
#define TARG_SIZEOF_LONG_DOUBLE 12
#define TARG_JMP_BUF_NUM_ELEMENTS 10
#define TARG_USER_CONTROL_OF_STRUCT_PACKING_AFFECTS_BIT_FIELDS 0
#ifndef CHECKING
#define CHECKING 1
#endif /* ifndef CHECKING */
#define DEBUG 1
#define C_GEN_BE_GENERATES_ANSI_C 1
#define SAME_REPR_INTS_INTERCHANGEABLE_IN_IL 0
#define ASSIGNMENT_TO_THIS_ALLOWED 0
#define DEFAULT_ALLOW_ANACHRONISMS 0
#define CFRONT_GLOBAL_VS_MEMBER_NAME_LOOKUP_BUG 0
#define CFRONT_2_1_OBJECT_CODE_COMPATIBILITY 0
#define NEW_CAN_BE_FOLDED_INTO_CTOR 0
#define TARG_WCHAR_T_INT_KIND ((an_integer_kind)ik_long)
#define MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED 1
#define USE_OWN_SJIS_MULTIBYTE_CHAR_PROCESSING 0
#define PRAGMA_WEAK_ALLOWED 1
#define MAKE_ALL_FUNCTIONS_UNPROTOTYPED 0
#define ASM_FUNCTION_ALLOWED 1
#define INCLUDE_COMMENTS_IN_ASM_FUNC_BODY 1
#define TARG_ZERO_WIDTH_BIT_FIELD_AFFECTS_STRUCT_ALIGNMENT 1
#ifndef TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT
#define TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT 4
#endif /* ifndef TARG_ZERO_WIDTH_BIT_FIELD_ALIGNMENT */
#ifndef MAINTAIN_NEEDED_FLAGS
#define MAINTAIN_NEEDED_FLAGS 1
#endif /* ifndef MAINTAIN_NEEDED_FLAGS */
#ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES
#define DEFAULT_REMOVE_UNNEEDED_ENTITIES 0
#endif /* ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES */
#define DEFAULT_FRIEND_INJECTION 1

#ifdef OPTIMIZED_VERSION
#define SVR4_TRAP_NULL_POINTER_REFERENCES 0
#else /* ifndef OPTIMIZED_VERSION */
#define EXPENSIVE_CHECKING 1
#endif /* ifdef OPTIMIZED_VERSION */

#ifndef SVR4_TRAP_NULL_POINTER_REFERENCES
#define SVR4_TRAP_NULL_POINTER_REFERENCES 1
#endif /* ifndef SVR4_TRAP_NULL_POINTER_REFERENCES */

#endif /* ifdef __CYGWIN__ */
#endif /* defined(__APPLE__) && defined(__MACH__) */
#endif /* ifdef __hpux */
#endif /* ifdef __linux__ */
#endif /* defined(_WIN32) */
#endif /* defined(__sun) */

#ifdef EDG_TEST_VERSION

/* Define a full-featured set of language features. */

#ifndef COMPILE_MULTIPLE_TRANSLATION_UNITS
#define COMPILE_MULTIPLE_TRANSLATION_UNITS 1
#endif /* ifndef COMPILE_MULTIPLE_TRANSLATION_UNITS */
#ifndef MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED
#define MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED 1
#endif /* ifndef MULTIBYTE_CHARS_IN_SOURCE_SUPPORTED */
#ifndef C99_IL_EXTENSIONS_SUPPORTED
#define C99_IL_EXTENSIONS_SUPPORTED 1
#endif /* ifndef C99_IL_EXTENSIONS_SUPPORTED */
#ifndef CPP11_IL_EXTENSIONS_SUPPORTED
#define CPP11_IL_EXTENSIONS_SUPPORTED 1
#endif /* ifndef CPP11_IL_EXTENSIONS_SUPPORTED */
#ifndef INSTANTIATE_EXTERN_INLINE
#define INSTANTIATE_EXTERN_INLINE 1
#endif /* ifndef INSTANTIATE_EXTERN_INLINE */
#ifndef INSTANTIATE_BEFORE_PCH_CREATION
#define INSTANTIATE_BEFORE_PCH_CREATION 1
#endif /* ifndef INSTANTIATE_BEFORE_PCH_CREATION */
#ifndef MAINTAIN_CLASS_MEMBER_LIST
#define MAINTAIN_CLASS_MEMBER_LIST 1
#endif /* ifndef MAINTAIN_CLASS_MEMBER_LIST */
#ifndef MAINTAIN_NEEDED_FLAGS
#define MAINTAIN_NEEDED_FLAGS 1
#endif /* ifndef MAINTAIN_NEEDED_FLAGS */
#ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES
#if MAINTAIN_NEEDED_FLAGS
#define DEFAULT_REMOVE_UNNEEDED_ENTITIES 1
#endif /* MAINTAIN_NEEDED_FLAGS */
#endif /* ifndef DEFAULT_REMOVE_UNNEEDED_ENTITIES */
#ifndef EMBEDDED_C_ALLOWED
#define EMBEDDED_C_ALLOWED 1
#endif /* ifndef EMBEDDED_C_ALLOWED */
#ifndef DEFAULT_EMBEDDED_C_ENABLED
#define DEFAULT_EMBEDDED_C_ENABLED 0
#endif /* ifndef DEFAULT_EMBEDDED_C_ENABLED */
#ifndef INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES
#define INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES 1
#endif /* ifndef INCLUDE_EDG_TEST_NAMED_ADDRESS_SPACES */
#ifndef INCLUDE_EDG_TEST_NAMED_REGISTERS
#define INCLUDE_EDG_TEST_NAMED_REGISTERS 1
#endif /* ifndef INCLUDE_EDG_TEST_NAMED_REGISTERS */
#ifndef THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
#define THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED 1
#endif /* ifndef THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED */
#ifndef REDEFINE_EXTNAME_PRAGMA_ENABLED
#define REDEFINE_EXTNAME_PRAGMA_ENABLED 1
#endif /* ifndef REDEFINE_EXTNAME_PRAGMA_ENABLED */
#ifndef ABORT_ON_INIT_COMPONENT_LEAKAGE
#define ABORT_ON_INIT_COMPONENT_LEAKAGE 1
#endif /* ifndef ABORT_ON_INIT_COMPONENT_LEAKAGE */

#ifdef OPTIMIZED_VERSION

/* Options for optimized version. */
#ifndef CHECKING
#define CHECKING 1
#endif /* ifndef CHECKING */
#ifndef DEBUG
#define DEBUG 0
#endif /* ifndef DEBUG */

#else /* !defined(OPTIMIZED_VERSION) */

/* Options for full featured test version. */
#ifndef IL_SHOULD_BE_WRITTEN_TO_FILE
#define IL_SHOULD_BE_WRITTEN_TO_FILE 1
#endif /* ifndef IL_SHOULD_BE_WRITTEN_TO_FILE */
#ifndef INCLUDE_EDG_TEST_PRAGMAS
#define INCLUDE_EDG_TEST_PRAGMAS 1
#endif /* ifndef INCLUDE_EDG_TEST_PRAGMAS */
#ifndef INCLUDE_EDG_TEST_ATTRIBUTES
#define INCLUDE_EDG_TEST_ATTRIBUTES 1
#endif /* ifndef INCLUDE_EDG_TEST_ATTRIBUTES */
#ifndef MICROSOFT_EXTENSIONS_ALLOWED
#define MICROSOFT_EXTENSIONS_ALLOWED 1
#endif /* ifndef MICROSOFT_EXTENSIONS_ALLOWED */
#ifndef TARG_MICROSOFT_BIT_FIELD_ALLOCATION
#define TARG_MICROSOFT_BIT_FIELD_ALLOCATION 0
#endif /* ifndef TARG_MICROSOFT_BIT_FIELD_ALLOCATION */
#ifndef DEFAULT_MICROSOFT_MODE
#define DEFAULT_MICROSOFT_MODE 0
#endif /* ifndef DEFAULT_MICROSOFT_MODE */
#ifndef UPC_EXTENSIONS_ALLOWED
#define UPC_EXTENSIONS_ALLOWED 1
#endif /* ifndef UPC_EXTENSIONS_ALLOWED */
#ifndef ASM_FUNCTION_ALLOWED
#define ASM_FUNCTION_ALLOWED 1
#endif /* ifdef ASM_FUNCTION_ALLOWED */
#ifndef EXTRA_SOURCE_POSITIONS_IN_IL
#define EXTRA_SOURCE_POSITIONS_IN_IL 1
#endif /* ifndef EXTRA_SOURCE_POSITIONS_IN_IL */
#ifndef DEFAULT_SVR4_C_MODE
#define DEFAULT_SVR4_C_MODE 0
#endif /* ifndef DEFAULT_SVR4_C_MODE */
#ifndef PRAGMA_WEAK_ALLOWED
#define PRAGMA_WEAK_ALLOWED 1
#endif /* ifndef PRAGMA_WEAK_ALLOWED */
#ifndef VLA_ALLOWED
#define VLA_ALLOWED 1
#endif /* ifndef VLA_ALLOWED */
#ifndef DEFAULT_VLA_ENABLED
#define DEFAULT_VLA_ENABLED 0
#endif /* ifndef DEFAULT_VLA_ENABLED */
#ifndef SUN_EXTENSIONS_ALLOWED
#define SUN_EXTENSIONS_ALLOWED 1
#endif /* ifndef SUN_EXTENSIONS_ALLOWED */
#ifndef DEFAULT_SUN_COMPATIBILITY
#define DEFAULT_SUN_COMPATIBILITY 0
#endif /* ifndef DEFAULT_SUN_COMPATIBILITY */
#ifndef GNU_EXTENSIONS_ALLOWED
#define GNU_EXTENSIONS_ALLOWED 1
#endif /* ifndef GNU_EXTENSIONS_ALLOWED */
#ifndef DEFAULT_GNU_COMPATIBILITY
#define DEFAULT_GNU_COMPATIBILITY 0
#endif /* ifndef DEFAULT_GNU_COMPATIBILITY */

#endif /* !defined(OPTIMIZED_VERSION) */

#endif /* defined(EDG_TEST_VERSION) */

/*
Enable recognition of Microsoft attributes for internal versions.
*/
#ifndef SUPPRESS_MICROSOFT_ATTRIBUTE_PROCESSING
#define SUPPRESS_MICROSOFT_ATTRIBUTE_PROCESSING 0
#endif /* ifndef SUPPRESS_MICROSOFT_ATTRIBUTE_PROCESSING */

/*
For test versions, eschew the Microsoft approach to predeclaring type_info
in the global namespace since that doesn't match the EDG run-time support
library.
*/
#ifndef MICROSOFT_MODE_TYPE_INFO_IN_NAMESPACE_STD
#if defined(EDG_TEST_VERSION) || defined(MACOSX_TEST_VERSION) || \
    defined(LINUX_TEST_VERSION)
#define MICROSOFT_MODE_TYPE_INFO_IN_NAMESPACE_STD 1
#endif /* defined(EDG_TEST_VERSION) || defined(MACOSX_TEST_VERSION) || ... */
#endif /* ifndef MICROSOFT_MODE_TYPE_INFO_IN_NAMESPACE_STD */


#ifndef LOWER_FIXED_POINT
#ifndef EMBEDDED_C_ALLOWED
#define EMBEDDED_C_ALLOWED 0
#endif /* ifndef EMBEDDED_C_ALLOWED */
#ifndef FIXED_POINT_ALLOWED
#if EMBEDDED_C_ALLOWED
#define FIXED_POINT_ALLOWED 1
#else /* !EMBEDDED_C_ALLOWED */
#define FIXED_POINT_ALLOWED 0
#endif /* EMBEDDED_C_ALLOWED */
#endif /* ifndef FIXED_POINT_ALLOWED */
#if !defined(CP_GEN_BE_VERSION) && (EMBEDDED_C_ALLOWED || FIXED_POINT_ALLOWED)
#define LOWER_FIXED_POINT 1
#endif /* !defined(CP_GEN_BE_VERSION) && ... */
#endif /* ifndef LOWER_FIXED_POINT */

/*
GNU target compiler configuration.  When not using a GNU compiler to compile
the front end, set GNU_TARGET_VERSION_NUMBER to a reasonable default.
*/
#ifndef GNU_TARGET_VERSION_NUMBER
#if !defined(__GNUC__) || !defined(__GNUC_MINOR__) || \
    !defined(__GNUC_PATCHLEVEL__)
#define GNU_TARGET_VERSION_NUMBER 30200
#endif /* !defined(__GNUC__) || !defined(__GNUC_MINOR__) || ... */
#endif /* defined(GNU_TARGET_VERSION_NUMBER) */

/*
Set ABI-related switches.  This is done late so that individual configurations
(above) can do something different from the EDG default by setting the
switches before this point.
*/
#ifndef ABI_COMPATIBILITY_VERSION
#define ABI_COMPATIBILITY_VERSION 99999 /* Use latest version. */
#ifndef IA64_ABI
/* We want enough cfront compatibility to be able to use I/O streams compiled
   by cfront, but we also want the latest features. */
#ifndef CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
#define CFRONT_2_1_OBJECT_CODE_COMPATIBILITY 1
#endif /* ifndef CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */
#ifndef ABI_CHANGES_FOR_RTTI
#define ABI_CHANGES_FOR_RTTI 1
#endif /* ifndef ABI_CHANGES_FOR_RTTI */
#ifndef ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE
#define ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE 1
#endif /* ifndef ABI_CHANGES_FOR_ARRAY_NEW_AND_DELETE */
#ifndef DEFAULT_REMOVE_QUALIFIERS_FROM_PARAM_TYPES
#define DEFAULT_REMOVE_QUALIFIERS_FROM_PARAM_TYPES 1
#endif /* ifndef DEFAULT_REMOVE_QUALIFIERS_FROM_PARAM_TYPES */
#ifndef DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT
#define DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT 1
#endif /* ifndef DEFAULT_C_AND_CPP_FUNCTION_TYPES_ARE_DISTINCT */
#ifndef DEFAULT_DISTINCT_TEMPLATE_SIGNATURES
#define DEFAULT_DISTINCT_TEMPLATE_SIGNATURES 1
#endif /* ifndef DEFAULT_DISTINCT_TEMPLATE_SIGNATURES */
#ifndef RUNTIME_USES_NAMESPACES
#define RUNTIME_USES_NAMESPACES 1
#endif /* ifndef RUNTIME_USES_NAMESPACES */
#ifndef ABI_CHANGES_FOR_PLACEMENT_DELETE
#define ABI_CHANGES_FOR_PLACEMENT_DELETE 1
#endif /* ifndef ABI_CHANGES_FOR_PLACEMENT_DELETE */
#ifndef ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
#define ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN 1
#endif /* ifndef ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN */
#ifndef ABI_CHANGES_FOR_CONSTRUCTION_VTBLS
#define ABI_CHANGES_FOR_CONSTRUCTION_VTBLS 1
#endif /* ifndef ABI_CHANGES_FOR_CONSTRUCTION_VTBLS */
#ifndef DEFAULT_COMPRESS_MANGLED_NAMES
#define DEFAULT_COMPRESS_MANGLED_NAMES 1
#endif /* ifndef DEFAULT_COMPRESS_MANGLED_NAMES */
#ifndef TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT
#define TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT 1
#endif /* ifndef TARG_OPTIMIZE_EMPTY_BASE_CLASS_LAYOUT */
#endif /* ifndef IA64_ABI */
#endif /* ifndef ABI_COMPATIBILITY_VERSION */

#if ABI_COMPATIBILITY_VERSION == 228
#ifdef IL_SHOULD_BE_WRITTEN_TO_FILE
#if IL_SHOULD_BE_WRITTEN_TO_FILE
/* When doing 2.28 ABI testing, also use the non-alternate IL file format.
   Only set this here if IL_SHOULD_BE_WRITTEN_TO_FILE has previously been
   defined to TRUE. */
#define ALTERNATE_IL_FILE_FORMAT 0
#endif /* IL_SHOULD_BE_WRITTEN_TO_FILE */
#endif /* ifdef IL_SHOULD_BE_WRITTEN_TO_FILE */

/* Use cfront 2.1 object compatibility for 2.28 ABI testing. */
#ifndef CFRONT_2_1_OBJECT_CODE_COMPATIBILITY
#define CFRONT_2_1_OBJECT_CODE_COMPATIBILITY 1
#endif /* ifndef CFRONT_2_1_OBJECT_CODE_COMPATIBILITY */

#endif /* ABI_COMPATIBILITY_VERSION */

#ifndef BACK_END_IS_CP_GEN_BE
#define BACK_END_IS_CP_GEN_BE 0
#endif /* ifndef BACK_END_IS_CP_GEN_BE */

#if !defined(SUN_TARGET_VERSION_NUMBER) &&          \
    ((defined(SUN_IS_GENERATED_CODE_TARGET) &&      \
      SUN_IS_GENERATED_CODE_TARGET) ||              \
     (BACK_END_IS_CP_GEN_BE &&                      \
      CP_GEN_BE_TARGET_MATCHES_SOURCE_DIALECT &&    \
      defined(SUN_EXTENSIONS_ALLOWED) &&            \
      SUN_EXTENSIONS_ALLOWED)) &&                   \
    !(defined(__SUNPRO_CC) || defined(__SUNPRO_C))
#define SUN_TARGET_VERSION_NUMBER 0x530
#endif /* !defined(SUN_TARGET_VERSION_NUMBER) && ... */

/*
Overwrite freed memory to detect later uses.
*/
#ifndef OVERWRITE_FREED_MEM_BLOCKS
#ifdef DEMO_VERSION
#define OVERWRITE_FREED_MEM_BLOCKS 0
#else /* ifndef DEMO_VERSION */
#define OVERWRITE_FREED_MEM_BLOCKS 1
#endif /* ifdef DEMO_VERSION */
#endif /* ifndef OVERWRITE_FREED_MEM_BLOCKS */

/*
If EXPENSIVE_CHECKING has been requested, also enable checking pragmas.
*/
#ifdef EXPENSIVE_CHECKING
#if EXPENSIVE_CHECKING
#define ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING 1
#endif /* EXPENSIVE_CHECKING */
#endif /* ifndef EXPENSIVE_CHECKING */

/*
Allow C++/CLI and C++/CX to be enabled if Microsoft extensions are allowed.
*/
#if defined(MICROSOFT_EXTENSIONS_ALLOWED) && MICROSOFT_EXTENSIONS_ALLOWED
#ifndef CPPCLI_ENABLING_POSSIBLE
#define CPPCLI_ENABLING_POSSIBLE 1
#endif /* ifndef CPPCLI_ENABLING_POSSIBLE */
#ifndef CPPCX_ENABLING_POSSIBLE
#if CPPCLI_ENABLING_POSSIBLE
#define CPPCX_ENABLING_POSSIBLE 1
#else /* !CPPCLI_ENABLING_POSSIBLE */
#define CPPCX_ENABLING_POSSIBLE 0
#endif /* CPPCLI_ENABLING_POSSIBLE */
#endif /* ifndef CPPCX_ENABLING_POSSIBLE */
#if CPPCLI_ENABLING_POSSIBLE || CPPCX_ENABLING_POSSIBLE
#if !defined(CP_GEN_BE_VERSION) || !CP_GEN_BE_VERSION
#if !defined(DEMO_VERSION) || !DEMO_VERSION
#define ALLOW_CPPCLI_AND_CPPCX_WITH_LOWERING 1
#endif /* !defined(DEMO_VERSION) || !DEMO_VERSION */
#endif /* !defined(CP_GEN_BE_VERSION) || !CP_GEN_BE_VERSION */
#endif /* CPPCLI_ENABLING_POSSIBLE || CPPCX_ENABLED_POSSIBLE */
#if !defined(READ_CPPCLI_PORTABLE_ASSEMBLIES) && !defined(_WIN32) && \
    CPPCLI_ENABLING_POSSIBLE
#define READ_CPPCLI_PORTABLE_ASSEMBLIES 1
#endif /* !defined(READ_CPPCLI_PORTABLE_ASSEMBLIES) && !defined(_WIN32) ... */
#endif /* defined(MICROSOFT_EXTENSIONS_ALLOWED) &&
          MICROSOFT_EXTENSIONS_ALLOWED */
#if defined(READ_CPPCLI_PORTABLE_ASSEMBLIES) && \
    READ_CPPCLI_PORTABLE_ASSEMBLIES && \
    !defined(CPPCLI_PORTABLE_ASSEMBLY_PATH)
#define CPPCLI_PORTABLE_ASSEMBLY_PATH "/edg/cpfe/ms_assemblies"
#endif /* READ_CPPCLI_PORTABLE_ASSEMBLIES && !defined(...) */
#if defined(READ_CPPCLI_PORTABLE_ASSEMBLIES) && \
    READ_CPPCLI_PORTABLE_ASSEMBLIES && \
    !defined(CPPCX_INCLUDE_PATH)
#define CPPCX_INCLUDE_PATH "/edg/cpfe/ms_includes"
#endif /* READ_CPPCLI_PORTABLE_ASSEMBLIES && !defined(...) */

/*
Enable GNU function multiversioning on systems where GNU extensions are
enabled.
*/
#if defined(GNU_EXTENSIONS_ALLOWED) && GNU_EXTENSIONS_ALLOWED
#ifndef GNU_FUNCTION_MULTIVERSIONING
#define GNU_FUNCTION_MULTIVERSIONING 1
#endif /* defined(GNU_FUNCTION_MULTIVERSIONING) */
#endif /* defined(GNU_EXTENSIONS_ALLOWED) && GNU_EXTENSIONS_ALLOWED */

/*
Allow export to be enabled and enable it by default.
*/
#ifndef EXPORT_ENABLING_POSSIBLE
#define EXPORT_ENABLING_POSSIBLE 1
#define DEFAULT_EXPORT_TEMPLATE_ALLOWED 1
#endif /* ifndef EXPORT_ENABLING_POSSIBLE */

#ifndef SEQUENCING_DIAGNOSTICS_ENABLED
#if defined(EXTRA_SOURCE_POSITIONS_IN_IL) && EXTRA_SOURCE_POSITIONS_IN_IL
#define SEQUENCING_DIAGNOSTICS_ENABLED 1
#endif /* defined(EXTRA_SOURCE_POSITIONS_IN_IL) && EXTRA_SOURCE_POSITIONS... */
#endif /* !defined(SEQUENCING_DIAGNOSTICS_ENABLED) */

/*
Assume all IFC module files are little-endian, regardless of the target.
*/
#ifndef ASSUME_LITTLE_ENDIAN_IFC_MODULES
#define ASSUME_LITTLE_ENDIAN_IFC_MODULES 1
#endif /* ifndef ASSUME_LITTLE_ENDIAN_IFC_MODULES */

#ifndef REFLECTION_ENABLING_POSSIBLE
#define REFLECTION_ENABLING_POSSIBLE 1
#endif /* ifndef REFLECTION_ENABLING_POSSIBLE */

/*
If using lint on a non-Sun platform, define some features that are in the
SUN_TEST_VERSION but not the EDG_TEST_VERSION.
*/
#ifdef _lint
#ifndef __sun
#define GENERATE_SOURCE_SEQUENCE_LISTS 1
#define ALLOW_SOURCE_SEQUENCE_LISTS_WITH_IL_LOWERING 1
#define RECORD_HIDDEN_NAMES_IN_IL 1
#define ALLOW_HIDDEN_NAMES_IN_IL_WITH_IL_LOWERING 1
#define RECORD_TEMPLATE_STRINGS 1
#define RECORD_MACROS_IN_IL 1
#define KEEP_OBJECT_LIFETIME_INFO_IN_LOWERED_IL_WHEN_EH_ENABLED 1
#define DUMP_LOWERED_EH_CONSTRUCTS_IN_C_GEN_BE 1
#undef LOWER_VARIABLE_LENGTH_ARRAYS
#define LOWER_VARIABLE_LENGTH_ARRAYS 1
#endif /* ifndef __sun */
#endif /* ifdef lint */

/*
The following configuration macros are no longer used, but are kept in this
file so that git-find-commit will continue to work (because this file may be
used to compile older versions where the macros are used).
*/
#define TARG_MINIMUM_PACK_ALIGNMENT_linux_x86_64 1
#define TARG_MAXIMUM_PACK_ALIGNMENT_linux_x86_64 128
#define TARG_MINIMUM_PACK_ALIGNMENT_linux_i686 1
#define TARG_MAXIMUM_PACK_ALIGNMENT_linux_i686 128
#define TARG_MINIMUM_PACK_ALIGNMENT_win64 1
#define TARG_MAXIMUM_PACK_ALIGNMENT_win64 128
#define TARG_MINIMUM_PACK_ALIGNMENT_win32 1
#define TARG_MAXIMUM_PACK_ALIGNMENT_win32 128

#endif /* defined(USE_CMAKE_DEFINES) */

#endif /* ifndef DEFINES_H */

 
