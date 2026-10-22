/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

basics.h -- Basic declarations.

This should be included first in every compilation unit.

*/

/* Avoid including these declarations more than once. */
#ifndef BASICS_H
#define BASICS_H 1

/*
Include the header file that supplies the default configuration
parameters for this version.
*/
   
#include "defines.h"

/*
Define EDG_CONCAT and stringize macros that use the preprocessor to concatenate
and turn an argument into a string.
*/
/* Concatenate two arguments: */
#define EDG_CONCAT(x, y) x ## y
/* Quote the argument: */
#define stringize(X) #X

/*
The EDG code can either go into the global namespace or the "edg" namespace.
*/
#ifndef USE_EDG_NAMESPACE
#define USE_EDG_NAMESPACE 1
#endif /* USE_EDG_NAMESPACE */

#if USE_EDG_NAMESPACE
#define BEGIN_EDG_NAMESPACE namespace edg {
#define END_EDG_NAMESPACE }  /* edg */
#define EDG_PREFIX edg
#define EDG_QUAL edg::
/*lint -estring(823,USING_NAMESPACE_EDG)*/
#define USING_NAMESPACE_EDG namespace edg{} using namespace edg;
#else /* !USE_EDG_NAMESPACE */
#define BEGIN_EDG_NAMESPACE /* nothing */
#define END_EDG_NAMESPACE /* nothing */
#define EDG_PREFIX /* nothing */
#define EDG_QUAL /* nothing */
#define USING_NAMESPACE_EDG /* nothing */
#endif /* USE_EDG_NAMESPACE */

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Determine if this is a WIN32 (e.g., Windows NT or Windows 95) system.
*/
#ifndef EDG_WIN32
#if defined(__WATCOMC__) && defined(__NT__)
/* Some versions of the Watcom compiler fail to set _WIN32.  Set EDG_WIN32
   when running the Watcom compiler on NT. */
#define EDG_WIN32 1
#endif /* defined(__WATCOMC__) && defined(__NT__) */
#endif /* ifndef EDG_WIN32 */

#ifndef EDG_WIN32
#ifdef _WIN32
#define EDG_WIN32 1
#else /* !_WIN32 */
#define EDG_WIN32 0
#endif /* _WIN32 */
#endif /* ifndef EDG_WIN32 */

/*
Determine if this is MS-DOS and if this is Turbo-C or Microsoft C.  No
other MS-DOS compilers are considered at this time.  If this is MS-DOS
(or, more likely, an early version of Windows) set EDG_MSDOS.  If this is
Windows 95 or later, EDG_WIN32 will also be set.  Note that EDG_MSDOS
will be set for DOS and Windows 3.1, and may also be set for Windows
95/98/NT for older versions of the Microsoft compiler.  EDG_MSDOS is
not set for newer versions of the Microsoft compiler (it is not set in
version 6.x, and may not be set in some earlier versions).  EDG_WIN32
will only be set for 95/98/NT.
*/
#ifndef EDG_MSDOS
#if defined(MSDOS) || defined(__MSDOS__)
/* Turbo-C defines __MSDOS__ and Microsoft C defines MSDOS, so this
   is MS-DOS. */
#define EDG_MSDOS 1
#else /* !(defined(MSDOS) || defined(__MSDOS__)) */
#define EDG_MSDOS 0
#endif /* defined(MSDOS) || defined(__MSDOS__) */
#endif /* ifdef EDG_MSDOS */

/*
Set a flag that indicates that some Microsoft operating system is being
used.  Most of the DOS/Windows code applies to all systems (i.e.,
file name manipulation routines), and so can just test this flag.
*/
#ifndef __MICROSOFT_OS__
#if EDG_WIN32 || EDG_MSDOS
#define __MICROSOFT_OS__ 1
#else /* !(EDG_WIN32 || EDG_MSDOS) */
#define __MICROSOFT_OS__ 0
#endif /* EDG_WIN32 || EDG_MSDOS */
#endif /* ifndef __MICROSOFT_OS__ */

/*
If this is a Microsoft operating system, as indicated by the macro
"__MICROSOFT_OS__", determine which compiler it is.  Borland, Zortech,
and Microsoft are supported.
*/
#if __MICROSOFT_OS__
#ifdef __TURBOC__
/* Borland's (Turbo-C or C++) library is ANSI compatible. */
#define __ANSIC__ 1
#else /* __TURBOC__ */
#ifdef __ZTC__
/* Zortech's library is ANSI compatible. */
#define __ANSIC__ 1
#else /* __ZTC__ */
/* Then it must be MSC. */
#define __MSC__ 1
/* MSC's library is ANSI compatible. */
#define __ANSIC__ 1
#endif /* ifdef __ZTC__ */
#endif /* ifdef __TURBOC__ */
#endif /* ifdef __MICROSOFT_OS__ */
#ifndef __MSC__
#define __MSC__ 0
#endif /* ifndef __MSC__ */

/* VAX/VMS (and Open VMS) are considered to have an ANSI compatible
   library with needed differences controlled by the __VMS__ flag. */
#ifdef __VMS__
#undef __VMS__
#define __VMS__ 1
#define __ANSIC__ 1
#else /* !defined(__VMS__) */
#define __VMS__ 0
#endif /* ifdef __VMS__ */

/* Set the UNIX dialect (__ANSIC__, __BSD__, __SYSV__) if it is not already
   set.  Note that this is the dialect for the LIBRARY and not for the
   COMPILER.  __ANSIC__ is used instead of the more obvious __STDC__ 
   because an ANSI-conformant compiler can be used in a non-ANSI (e.g., SysV)
   environment when doing cross-compilation. */
#ifndef __ANSIC__
#ifndef __BSD__
#ifndef __SYSV__
/*
By default, configure for ANSI C if __STDC__ is set, and for BSD4.n otherwise.
*/
#ifdef __STDC__
#define __ANSIC__ 1
#else /* !defined(__STDC__) */
#define __BSD__ 1
#endif /* ifdef __STDC__ */
#endif /* __SYSV__ */
#endif /* __BSD__ */
#endif /* __ANSIC__ */

/* Standardize the settings of __ANSIC__, __BSD__, and __SYSV__. */
#ifdef __ANSIC__
#undef __ANSIC__
#define __ANSIC__ 1
#else /* !defined(__ANSIC__) */
#define __ANSIC__ 0
#endif /* ifdef __ANSIC__ */

#ifdef __BSD__
#undef __BSD__
#define __BSD__ 1
#else /* !defined(__BSD__) */
#define __BSD__ 0
#endif /* ifdef __BSD__ */

#ifdef __SYSV__
#undef __SYSV__
#define __SYSV__ 1
#else /* !defined(__SYSV__) */
#define __SYSV__ 0
#endif /* ifdef __SYSV__ */

#if __ANSIC__ + __BSD__ + __SYSV__ != 1
 #error -- Exactly one of "__ANSIC__", "__BSD__", and "__SYSV__" must \
            be set.
#endif /* __ANSIC__ + ... */

/*
Definition of a generic byte.  Always "unsigned char".
*/
typedef unsigned char
                a_byte;

/*
Flag that is TRUE if, when compiling the front end as C++ code, the
C++ bool type should be used for a_boolean.  This option is provided
primarily as a checking facility to make sure boolean values are used
appropriately.
*/
#ifndef USE_BOOL_FOR_BOOLEAN_IN_CPLUSPLUS
#define USE_BOOL_FOR_BOOLEAN_IN_CPLUSPLUS 0
#endif /* USE_BOOL_FOR_BOOLEAN_IN_CPLUSPLUS */

/* Simple boolean type: */
#if USE_BOOL_FOR_BOOLEAN_IN_CPLUSPLUS
typedef bool a_boolean;
typedef bool a_byte_boolean;
#define TRUE (0==0)   /* Allow for use in #if. */
#define FALSE (0!=0)
#else /* !USE_BOOL_FOR_BOOLEAN_IN_CPLUSPLUS */
typedef int	a_boolean;
typedef a_byte	a_byte_boolean;
#define FALSE 0
#define TRUE 1
#endif /* USE_BOOL_FOR_BOOLEAN_IN_CPLUSPLUS */

/*
TRUE if the front end is to be called as a subroutine of another
program, or as a library.  When this is TRUE, the front end has
no main program of its own and exits via a return to the caller.
This defaults to TRUE if an alternate name of the main routine has
been specified (via the EDG_MAIN macro).
*/
#ifndef MAKE_FRONT_END_CALLABLE
#ifdef EDG_MAIN
#define MAKE_FRONT_END_CALLABLE TRUE
#else /* ifndef EDG_MAIN */
#define MAKE_FRONT_END_CALLABLE FALSE
#endif /* ifdef EDG_MAIN */
#endif /* MAKE_FRONT_END_CALLABLE */

/*
TRUE if the front end can use multiple threads to support concurrent
compilations within a single process.  Each thread acts as if it were a
separate invocation of the front end.  This is only allowed when
MAKE_FRONT_END_CALLABLE is TRUE.
*/
#ifndef MULTIPLE_THREAD_COMPILATION
#define MULTIPLE_THREAD_COMPILATION FALSE
#endif /* MULTIPLE_THREAD_COMPILATION */

#if MULTIPLE_THREAD_COMPILATION && !MAKE_FRONT_END_CALLABLE
 #error -- MAKE_FRONT_END_CALLABLE must be TRUE when \
           MULTIPLE_THREAD_COMPILATION is TRUE.
#endif /* MULTIPLE_THREAD_COMPILATION && !MAKE_FRONT_END_CALLABLE */

/*
EXTERN is defined usually as "extern"; in the translation unit that
actually defines storage for external variables, it is defined as an
empty string.  EXTERN is used on the declarations of external variables
in .h files.  This scheme makes it easy to define them in only one
place while using the same source in all places.  VAR_INITIALIZERS is
defined to indicate we are in the translation unit in which the definitions
occur and also causes inclusion of initializers for those variables.  (Note
that using EXTERN with a variable declared "const" will not work, as the
resulting definition will have internal linkage.  The variable should
simply be declared "extern const" in that case.)
*/
#ifndef VAR_INITIALIZERS
#define VAR_INITIALIZERS 0
#endif /* ifndef VAR_INITIALIZERS */
#if VAR_INITIALIZERS
#define EXTERN /* nothing */
#else /* !VAR_INITIALIZERS */
#define EXTERN extern
#endif /* VAR_INITIALIZERS */

/*
EDG_THREAD is used to provide either the thread_local keyword or nothing
depending on MULTIPLE_THREAD_COMPILATION.
*/
#ifndef EDG_THREAD
#if MULTIPLE_THREAD_COMPILATION
#define EDG_THREAD thread_local
#else /* !MULTIPLE_THREAD_COMPILATION */
#define EDG_THREAD /* nothing */
#endif /* MULTIPLE_THREAD_COMPILATION */
#endif /* EDG_THREAD */

/*
EXTERN_THREAD is similar to EXTERN but is used to optionally make global
variables thread-local.
*/
#ifndef EXTERN_THREAD
#if MULTIPLE_THREAD_COMPILATION
#define EXTERN_THREAD EXTERN thread_local
#else /* !MULTIPLE_THREAD_COMPILATION */
#define EXTERN_THREAD EXTERN
#endif /* MULTIPLE_THREAD_COMPILATION */
#endif /* EXTERN_THREAD */

/*
STATIC_THREAD is similar to EXTERN_THREAD but is used to make file scope
or function scope variables optionally thread-local.
*/
#ifndef STATIC_THREAD
#if MULTIPLE_THREAD_COMPILATION
#define STATIC_THREAD static thread_local
#else /* !MULTIPLE_THREAD_COMPILATION */
#define STATIC_THREAD static
#endif /* MULTIPLE_THREAD_COMPILATION */
#endif /* STATIC_THREAD */

/*
USING_ISO_C is TRUE if the compiler being used to build the front end
is an ISO C compiler or C++ compiler.  This is used to determine whether
certain language features and preprocessing features are available.
*/
#ifndef USING_ISO_C
#ifdef __STDC__
#define USING_ISO_C TRUE
#else /* !defined(__STDC__) */
#define USING_ISO_C TRUE
#endif /* ifdef __STDC__ */
#endif /* ifndef USING_ISO_C */

/*
Flag that is TRUE if we should include the inttypes.h header to define the
stdint.h types on systems such as Solaris.
*/
#ifndef USE_INT_TYPES_HEADER
#ifdef __sun
#define USE_INT_TYPES_HEADER TRUE
#else /* ifndef __sun */
#define USE_INT_TYPES_HEADER FALSE
#endif /* ifdef __sun */
#endif /* ifndef USE_INT_TYPES_HEADER */

/*
Flag that is TRUE if we should include the C99/C++11 stdint.h header to define
typedefs for the various integer types.
*/
#ifndef USE_STDINT_HEADER
#if USE_INT_TYPES_HEADER
/* Don't use stdint.h if inttypes.h is to be used. */
#define USE_STDINT_HEADER FALSE
#else /* !USE_INT_TYPES_HEADER */
/* gcc as of at least 3.2 includes stdint.h */
#ifdef __GNUC__
#define USE_STDINT_HEADER TRUE
#else /* ifndef __GNUC__ */
#ifdef _MSC_VER
/* The Microsoft compiler as of at least 10.0 includes stdint.h */
#if _MSC_VER >= 1600
#define USE_STDINT_HEADER TRUE
#endif /* _MSC_VER >= 1600 */
#endif /* ifndef _MSC_VER */
#endif /* ifndef __GNUC__ */
#ifndef USE_STDINT_HEADER
/* If not defined above, assume we can't use the stdint.h header. */
#define USE_STDINT_HEADER FALSE
#endif /* ifndef USE_STDINT_HEADER */
#endif /* USE_INT_TYPES_HEADER */
#endif /* ifndef USE_STDINT_HEADER */

#if USE_STDINT_HEADER && USE_INT_TYPES_HEADER
 #error -- USE_STDINT_HEADER and USE_INT_TYPES_HEADER cannot both be TRUE.
#endif /* USE_STDINT_HEADER && USE_INT_TYPES_HEADER */

/*
Flag that is TRUE if the front end should use virtual functions rather than
using its own form of virtual function dispatch.  Note that using virtual
functions introduces a dependency on a C++ run-time support library and may
limit portability.
*/
#ifndef USE_VIRTUAL_FUNCTIONS
#define USE_VIRTUAL_FUNCTIONS FALSE
#endif /* ifndef USE_VIRTUAL_FUNCTIONS */

#if USE_VIRTUAL_FUNCTIONS
#define VIRTUAL virtual
#define OVERRIDE override
#define ABSTRACT = 0
#else /* !USE_VIRTUAL_FUNCTIONS */
#define VIRTUAL /* nothing */
#define OVERRIDE /* nothing */
#define ABSTRACT /* nothing */
#endif /* USE_VIRTUAL_FUNCTIONS */

#ifndef EDG_NOEXCEPT
#define EDG_NOEXCEPT noexcept
#endif /* ifndef EDG_NOEXCEPT */

/*
Flag that is TRUE if, when USE_STDINT_HEADER and USE_INT_TYPES_HEADER
are both FALSE, the EDG-supplied definitions of the stdint.h types should
still not be used.  This flag can be used if the types normally defined in
stdint.h are instead defined in some other unexpected place.
*/
#ifndef SUPPRESS_DEFINITION_OF_STDINT_TYPES
#define SUPPRESS_DEFINITION_OF_STDINT_TYPES FALSE
#endif /* ifndef SUPPRESS_DEFINITION_OF_STDINT_TYPES */

/*
When compiling C++ code on some common platforms, the <stdint.h> and/or
<inttypes.h> headers do not define the macro UINT32_MAX (and other similar
macros) unless __STDC_LIMIT_MACROS is defined before inclusion.
*/
#if USE_STDINT_HEADER || USE_INT_TYPES_HEADER
#ifndef __STDC_LIMIT_MACROS
#define __STDC_LIMIT_MACROS
#endif /* ifndef __STDC_LIMIT_MACROS */
#endif /* USE_STDINT_HEADER || USE_INT_TYPES_HEADER */

/*
Include a header to provide typedefs for integer types of specific sizes.  If
no such header is available, provide typedefs for the selected fixed size
integral types.
*/
#if USE_STDINT_HEADER
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include <stdint.h>
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
#else /* !USE_STDINT_HEADER */
#if USE_INT_TYPES_HEADER
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include <inttypes.h>
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
#else /* !USE_INT_TYPES_HEADER */
#if !SUPPRESS_DEFINITION_OF_STDINT_TYPES
#ifndef EDG_INT8_T
#define EDG_INT8_T signed char
#endif /* ifndef EDG_INT8_T */
typedef EDG_INT8_T int8_t;

#ifndef EDG_UINT8_T
#define EDG_UINT8_T unsigned char
#endif /* ifndef EDG_UINT8_T */
typedef EDG_UINT8_T uint8_t;

#ifndef EDG_INT16_T
#define EDG_INT16_T short
#endif /* ifndef EDG_INT16_T */
typedef EDG_INT16_T int16_t;

#ifndef EDG_UINT16_T
#define EDG_UINT16_T unsigned short
#endif /* ifndef EDG_UINT16_T */
typedef EDG_UINT16_T uint16_t;

#ifndef EDG_INT32_T
#define EDG_INT32_T int
#endif /* ifndef EDG_INT32_T */
typedef EDG_INT32_T int32_t;

#ifndef EDG_UINT32_T
#define EDG_UINT32_T unsigned int
#endif /* ifndef EDG_UINT32_T */
typedef EDG_UINT32_T uint32_t;

/* Define the uintptr_t type. */
#if __MSC__
/* On Microsoft's 64-bit platform, a long is not large enough to hold the bits
   of a pointer.  Furthermore, Microsoft defines the uintptr_t type in a header
   (vadefs.h) other than <stdint.h> or <inttypes.h>.  So we just include that
   header here to avoid conflicts. */
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include "vadefs.h"
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
#else /* !__MSC__ */
#ifndef EDG_UINTPTR_T
#define EDG_UINTPTR_T unsigned long
#endif /* ifndef EDG_UINTPTR_T */
typedef EDG_UINTPTR_T uintptr_t;
#endif /* __MSC__ */

#endif /* SUPPRESS_DEFINITION_OF_STDINT_TYPES */
#endif /* !USE_INT_TYPES_HEADER */
#endif /* !USE_STDINT_HEADER */

#if !defined(UINT32_MAX)
#define UINT32_MAX UINT_MAX
#endif /* !defined(UINT32_MAX) */

#if !defined(INT32_MAX)
#define INT32_MAX INT_MAX
#endif /* !defined(INT32_MAX) */

#if !defined(UINT64_MAX)
#define UINT64_MAX UINT_MAX
#endif /* !defined(UINT64_MAX) */

#if !defined(INT64_MAX)
#define INT64_MAX INT_MAX
#endif /* !defined(INT64_MAX) */

/* Define typedefs to be used for "void *" and "const void *".  When
   using an ANSI C compiler these are just typedefs to the appropriate
   types.  When compiling with an old-style C compiler, "char *" is used. */
#if USING_ISO_C
typedef void * a_void_ptr;
typedef const void * a_const_void_ptr;
#else /* !USING_ISO_C */
typedef char * a_void_ptr;
typedef char * a_const_void_ptr;
#endif /* USING_ISO_C */

/*
Define a type to be used in declaring variables, parameters, and fields
that are intended as pointers to read-only string data.  Since C++11, this
must be a pointer to const char.

In earlier releases (which could be built with C89 compilers), the const
qualifier was omitted when USE_POINTER_TO_CONST_CHAR was configured FALSE.
Catch configurations that still assume the old type.
*/
typedef const char a_const_char;

#ifdef USE_POINTER_TO_CONST_CHAR
#if !USE_POINTER_TO_CONST_CHAR
 #error -- USE_POINTER_TO_CONST_CHAR (obsolete) can no longer be set to FALSE
#endif /* USE_POINTER_TO_CONST_CHAR */
#endif /* ifdef USE_POINTER_TO_CONST_CHAR */

/*
Type to be used for bit fields.
*/
typedef unsigned int a_bit_field;

#if __ANSIC__
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include <limits.h>
#include <stddef.h>
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
/* sizeof_t is used instead of size_t within the front end.  It is the same
   as size_t except on systems where that is too small, e.g., it's 16 bits.
   true_size_t is the true underlying size_t. 
   size_t_arg is used to pass standard library arguments that used to be
   int and are now (in ANSI C) size_t, e.g., the length on fwrite. */
/* Note that size_t_arg may evaluate its argument more than once. */
typedef size_t	true_size_t;
#if !EDG_MSDOS
typedef size_t	sizeof_t;
#define size_t_arg(arg) ((size_t)(arg))
#else /* !EDG_MSDOS */
/* Most MS-DOS C compilers have a 16-bit size_t, so use unsigned long. */
typedef unsigned long sizeof_t;
/* size_t_arg checks for truncation. */
#define size_t_arg(arg) \
  ((sizeof_t)(arg) > UINT_MAX ? size_t_arg_error() : (true_size_t)(arg))
#define NEED_SIZE_T_ARG_ERROR TRUE
extern true_size_t size_t_arg_error(void);
#endif /* !EDG_MSDOS */
/* Use a_ptrdiff for ptrdiff_t because ptrdiff_t appears in <sys/types.h> on
   some UNIX systems. */
typedef ptrdiff_t a_ptrdiff;
#ifdef __TURBOC__
/* Turbo C does not define CHAR_MIN correctly for signed characters.
   It defines it as 0x80, which is not a negative number in int context.
   It should be defined as -128. */
#ifdef CHAR_MIN
#undef CHAR_MIN
#endif /* ifdef CHAR_MIN */
#define CHAR_MIN (-128)
#endif /* __TURBOC__ */
#else /* !__ANSIC__ */
/* Definitions to make pre-ANSI compilers look more like ANSI C: */
#define CHAR_BIT 8		/* Bits per byte (char). */
#define CHAR_MIN (-128)		/* Signed chars. */
#define CHAR_MAX 127
#define UCHAR_MAX 255
#define SHRT_MAX ((short)0x7fff)
#define USHRT_MAX ((unsigned short)0xffff)
#define INT_MAX ((int)0x7fffffff)
#define UINT_MAX ((unsigned int)0xffffffff)
#define LONG_MAX ((long)0x7fffffffL)
#define LONG_MIN ((long)0x80000000L)
#define ULONG_MAX ((unsigned long)0xffffffffL)
#ifdef __GNUC__
/* Using gcc without a conforming ANSI/ISO C library or headers.  Assume
   we have stddef.h anyway. */
#error we should not get here when using C++
#include <stddef.h>
typedef size_t true_size_t;
#else /* !defined(__GNUC__) */
/* Guess at the size of size_t.  This might have to be configured by hand. */
typedef unsigned int
		true_size_t;
#endif /* ifdef __GNUC__ */
/* sizeof_t is used instead of size_t within the front end.  It is the same
   as size_t except on systems where that is too small, e.g., it's 16 bits.
   true_size_t is the true underlying size_t. */
typedef true_size_t
		sizeof_t;
/* size_t_arg is used to pass standard library arguments that used to be
   int and are now (in ANSI C) size_t, e.g., the length on fwrite. */
/* Note that size_t_arg may evaluate its argument more than once. */
#define size_t_arg(arg) ((int)(arg))
/* Can't define ptrdiff_t, since it appears in <sys/types.h>, so define
   a_ptrdiff instead. */
typedef int     a_ptrdiff;
#endif /* __ANSIC__ */

END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include <stdio.h>
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
/* Some stdio.h's do not define SEEK_SET. */
#ifndef SEEK_SET
/* For fseek (etc.) parameters: */
#define SEEK_SET 0 /* Normal Unix value. */
#endif /* ifndef SEEK_SET */

#define STDLIB_H_INCLUDED TRUE
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include <stdlib.h>
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */

/* String and block routines: */
#if __ANSIC__
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include <string.h>
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
#define memzero(dest, nbytes) memset(dest, 0, nbytes)
#else /* !__ANSIC__ */
#if __SYSV__
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include <string.h>
#include <memory.h>
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
#define memzero(dest, nbytes) memset(dest, 0, nbytes)
#else /* !__SYSV__ */
#if __BSD__
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include <strings.h>
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
/* Remap string and block functions that do not appear in BSD C. */
#if USING_ISO_C
typedef void bcopy_bzero_return_type;
#else /* !USING_ISO_C */
typedef int bcopy_bzero_return_type;
#endif /* !USING_ISO_C */
extern "C" bcopy_bzero_return_type bcopy(a_const_void_ptr src,
                                         a_void_ptr dest, int nbytes);
extern "C" int bcmp(a_const_void_ptr src1, a_const_void_ptr src2, int nbytes);
extern "C" bcopy_bzero_return_type bzero(a_void_ptr dest, int nbytes);
#if USING_ISO_C
/* When compiling with an ISO C compiler, the standard header files are
   expected to define memcpy and memcmp.  Define a macro for memzero
   (which is not a standard library routine). */
#define memzero(dest, nbytes) memset(dest, 0, nbytes)
#ifdef __GNUC__
/* When using gcc, the header files are often generated automatically from
   the system header files using the Gnu fix_includes utility.  This does
   not automatically provide prototypes for certain functions.  Supply
   prototypes for the mem... functions. */
extern void * memchr (const void *, int, size_t);
extern int memcmp (const void *, const void *, size_t);
extern void * memcpy (void *, const void *, size_t);
#endif /* ifdef __GNUC__ */
#else /* !USING_ISO_C */
/* When using a pcc-style C compiler on BSD, define memcpy and memcmp in
   terms of the BSD bcopy and bcmp routines. */
#define memcpy(dest, src, nbytes) bcopy(src, dest, nbytes)
#define memcmp(src1, src2, nbytes) bcmp(src1, src2, nbytes)
#define memzero(dest, nbytes) bzero(dest, nbytes)
#endif /* USING_ISO_C */
#ifdef __sun
/* SunOS 4.1.x uses the __BSD__ flag, but should use the System V-like
strchr and strrchr routines. */
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include <string.h>
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
#else /* ifndef __sun */
#define strchr(str, c) index(str, c)
#define strrchr(str, c) rindex(str, c)
#endif /* ifdef __sun */
#endif /* __BSD__ */
#endif /* __SYSV__ */
#endif /* __ANSIC__ */

END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
/* Character classification. */
#include <ctype.h>
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */

#ifndef DEBUG
/* Include debugging code. */
#define DEBUG TRUE
#endif /* ifndef DEBUG */
#if DEBUG
/* GCOVR_EXCL_START */
EXTERN_THREAD int
		debug_level;
			/* Debug level.  0 means no debug output, 1 - 5
                            means increasing amounts. */
EXTERN_THREAD a_boolean
		db_active;
			/* TRUE if debug_level is currently non-zero, or
			   if there is the potential for it becoming
			   non-zero (because there is a debug list). */
EXTERN_THREAD FILE
		*f_debug;
			/* Debug output file. */

extern void debug_enter(int reporting_level, a_const_char *function_name);
extern void debug_exit(void);

/* Function entry and exit macros. */
#define db_enter(reporting_level, function_name)	              \
{ if (db_active) debug_enter(reporting_level, function_name);}

#define db_exit()					              \
{ if (db_active) debug_exit();}

/* Macro that returns TRUE if the specified debug flag is set. */
#define db_flag_is_set(name)						\
  (db_active && debug_flag_is_set(name))
/* GCOVR_EXCL_STOP */
#else /* !DEBUG */

/* If debugging code is not included: */
#define db_enter(reporting_level, function_name) /* empty */
#define db_exit()                                /* empty */
#define db_flag_is_set(name) FALSE

#endif /* DEBUG */

#ifndef CHECKING
/* Include consistency-checking code. */
#define CHECKING TRUE
#endif /* ifndef CHECKING */

#ifndef EXPENSIVE_CHECKING
/* Include checking code that involves execution of a significant
   amount of additional code, so should not be enabled by default. */
#define EXPENSIVE_CHECKING FALSE
#endif /* ifndef EXPENSIVE_CHECKING */

#if EXPENSIVE_CHECKING && !CHECKING
 #error -- EXPENSIVE_CHECKING requires CHECKING to be TRUE
#endif /* EXPENSIVE_CHECKING && !CHECKING */

#ifndef ABORT_ON_INIT_COMPONENT_LEAKAGE
/* Abort if not all init-component entries are freed by the end of the
   compilation. */
#if EXPENSIVE_CHECKING
#define ABORT_ON_INIT_COMPONENT_LEAKAGE 1
#else /* !EXPENSIVE_CHECKING */
#define ABORT_ON_INIT_COMPONENT_LEAKAGE 0
#endif /* EXPENSIVE_CHECKING */
#endif /* ifndef ABORT_ON_INIT_COMPONENT_LEAKAGE */

#ifndef ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING
/* Include code that tests the processing of pragmas by inserting pragma
   constructs in many locations.  This may increase compilation time
   significantly. */
#define ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING FALSE
#endif /* ADD_CHECKING_PRAGMAS_FOR_INTERNAL_TESTING */

#ifndef DUMP_CONFIG_ENABLED
/* Include code for the --dump_configuration option.  
   The --dump_configuration option displays on the error output the
   value of each configuration option with which the executable was built.
   The form of the display is a series of #define directives, making the
   output suitable for capture and use directly as a defines.h file.  This
   output can be helpful when reporting issues to EDG support and for
   determining whether two separately-compiled executables were built with
   compatible configuration options. */
#if DEBUG
/* GCOVR_EXCL_START */
#define DUMP_CONFIG_ENABLED TRUE /* Do not change this. */
/* GCOVR_EXCL_STOP */
#else /* !DEBUG */
#define DUMP_CONFIG_ENABLED TRUE /* Okay to change this. */
#endif /* DEBUG */
#endif /* ifndef DUMP_CONFIG_ENABLED */

/*
Overwrite the contents of the blocks that make up memory regions before
they are returned to the available list or freed.  This helps detect
references to memory that has already been freed.  This is enabled
by default when EXPENSIVE_CHECKING is requested.
*/
#ifndef OVERWRITE_FREED_MEM_BLOCKS
#if EXPENSIVE_CHECKING
#define OVERWRITE_FREED_MEM_BLOCKS TRUE
#else /* !EXPENSIVE_CHECKING */
#define OVERWRITE_FREED_MEM_BLOCKS FALSE
#endif /* EXPENSIVE_CHECKING */
#endif /* ifndef OVERWRITE_FREED_MEM_BLOCKS */

/*
Flag that is TRUE if the IL should contain information detailing the tree
of macro invocations from the translation unit.
*/
#ifndef MACRO_INVOCATION_TREE_IN_IL
#define MACRO_INVOCATION_TREE_IN_IL FALSE
#else /* defined(MACRO_INVOCATION_TREE_IN_IL) */
#if MACRO_INVOCATION_TREE_IN_IL
#ifndef RECORD_MACRO_INVOCATIONS
#define RECORD_MACRO_INVOCATIONS TRUE /* Do not change this. */
#endif /* ifndef RECORD_MACRO_INVOCATIONS */
#endif /* MACRO_INVOCATION_TREE_IN_IL */
#endif /* ifndef MACRO_INVOCATION_TREE_IN_IL */

/*
Flag that is TRUE if a record of macro invocations should be kept, e.g.,
for use in diagnostics.  This also expands source positions to include a
mechanism for determining the macro invocation stack in effect at the point
the position was captured.  Thus must be defined here in order to be
effective for conditional fields in a_source_position.
*/
#ifndef RECORD_MACRO_INVOCATIONS
#define RECORD_MACRO_INVOCATIONS MACRO_INVOCATION_TREE_IN_IL
#else /* defined(RECORD_MACRO_INVOCATIONS) */
#if RECORD_MACRO_INVOCATIONS
#ifndef RECORD_MACROS_IN_IL
#define RECORD_MACROS_IN_IL TRUE /* Do not change this. */
#endif /* ifndef RECORD_MACROS_IN_IL */
#ifndef FULLY_RESOLVED_MACRO_POSITIONS
#define FULLY_RESOLVED_MACRO_POSITIONS TRUE /* Do not change this. */
#endif /* ifndef FULLY_RESOLVED_MACRO_POSITIONS */
#endif /* RECORD_MACRO_INVOCATIONS */
#endif /* ifndef RECORD_MACRO_INVOCATIONS */

/*
Flag that is TRUE if a macro invocation record should contain a copy of the
arguments used in the invocation of a function-like macro.
*/
#ifndef RECORD_MACRO_ARGS
#define RECORD_MACRO_ARGS FALSE
#endif /* ifndef RECORD_MACRO_ARGS */

#if RECORD_MACRO_INVOCATIONS && !RECORD_MACROS_IN_IL
 #error -- RECORD_MACROS_IN_IL must be TRUE when \
           RECORD MACRO_INVOCATIONS is set.
#endif /* MACRO_INVOCATION_TREE_IN_IL && !RECORD_MACROS_IN_IL */

#if RECORD_MACRO_INVOCATIONS && !FULLY_RESOLVED_MACRO_POSITIONS
 #error -- FULLY_RESOLVED_MACRO_POSITIONS must be TRUE when \
           RECORD_MACRO_INVOCATIONS is set.
#endif /* MACRO_INVOCATION_TREE_IN_IL && !FULLY_RESOLVED_MACRO_POSITIONS */

#if MACRO_INVOCATION_TREE_IN_IL && !RECORD_MACRO_INVOCATIONS
 #error -- RECORD_MACRO_INVOCATIONS must be TRUE when \
           MACRO_INVOCATION_TREE_IN_IL is set.
#endif /* MACRO_INVOCATION_TREE_IN_IL && !RECORD_MACRO_INVOCATIONS */

#if RECORD_MACRO_ARGS && !RECORD_MACRO_INVOCATIONS
 #error -- RECORD_MACRO_INVOCATIONS must be TRUE when \
           RECORD_MACRO_ARGS is set.
#endif /* RECORD_MACRO_ARGS && !RECORD_MACRO_INVOCATIONS */

/*
Flag that is TRUE to add an extra seq/column pair to source positions, giving
the original location of text that occurs in a macro expansion (i.e., the
location in the macro argument or macro definition from which the text was
copied into the macro expansion).  This must be defined here in order to be
effective for conditional fields of a_source_position.
*/
#ifndef FULLY_RESOLVED_MACRO_POSITIONS
#define FULLY_RESOLVED_MACRO_POSITIONS FALSE
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */

/*
Data declarations pertaining to positions within source files.
*/
typedef unsigned short
		a_column_number;
			/* A column number: 
                           0..MAX_CHARS_IN_A_LOGICAL_SOURCE_LINE.  Applies to
			   columns of physical source lines.  The first
		           column is column 1.  0 indicates unknown. */
#define MAX_LINE_NUMBER UINT32_MAX
typedef uint32_t
		a_line_number;
			/* A line number within a file; not often used --
			   sequence numbers (below) are more common. */
#define MAX_SEQ_NUMBER UINT32_MAX
typedef uint32_t
		a_seq_number;
			/* A line number in compilation sequence order.
			   Such a number can be mapped back to a file and
			   line number if necessary.  A sequence number of 0
			   indicates "unknown position".  A sequence
			   number one larger than all those in use indicates
			   "after end of file, after the last line". */
typedef int32_t
	 	a_macro_invocation_record_index;
			/* The index of a macro invocation record (defined
			   in il_def.h; all we need is the index type
			   here).  It is defined even in configurations
			   that do not record macro invocations in order to
			   allow its use as a parameter type. */
/*
Value that indicates that there is no corresponding macro invocation
record.  The value 0 is chosen to allow use of memzero to clear blocks
containing macro invocation record indices.  As a result of this choice,
the zeroth macro invocation record will be left unused.
*/
#define NO_PARENT_MACRO_INVOCATION 0
typedef struct a_source_position *a_source_position_ptr;
typedef struct a_source_position {
  /* A source position: sequence number, column.  A source position with
     a sequence number of 0 indicates something special (see list below). */
  /* Remember to change the struct a_simple_source_position and the macro
     copy_source_position below if the structure here is changed. */
  a_seq_number	seq;
  a_column_number
		column;
#if FULLY_RESOLVED_MACRO_POSITIONS
  /* The position originally occupied by the associated location.  If the
     location occurs inside a macro expansion, seq and column will give the
     position in the source text of the start of the top-level macro
     invocation, while orig_seq and orig_column reflect the origin of the text
     at that location in the macro buffer -- either from a macro argument in
     the top-level macro invocation or from the text of a macro definition.
     orig_seq will be 0 and orig_column will be SP_COL_PREDEFINED_MACRO or
     SP_COL_CMD_LINE, respectively, for text resulting from the expansion of
     predefined macros and macros defined on the command line.  If the
     location is not within a macro expansion, orig_seq and orig_column will
     have the same values as seq and column. */
  /* Note that the fields orig_column and orig_seq are declared in reverse
     order of seq and column to improve the layout of the a_source_position
     structure in the common case where a_column_number requires half the
     size and alignment of a_seq_number. */
  a_column_number
		orig_column;
  a_seq_number	orig_seq;
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
#if RECORD_MACRO_INVOCATIONS
  a_macro_invocation_record_index
		macro_context;
			/* If this location occurs within a macro expansion,
			   macro_context is the index of the associated
			   macro invocation record within the macro
			   invocation tree.  This index can be used to
			   determine the macro associated with that expansion
			   as well as its invocation tree.  If this location
			   is not within a macro expansion, macro_context
			   will have the value NO_PARENT_MACRO_INVOCATION. */
#endif /* RECORD_MACRO_INVOCATIONS */
} a_source_position;

#define SP_LINE_UNKNOWN 0
			/* The line number is unknown.  Used for tokens scanned
			   from C++/CLI assemblies. */

/*
When the sequence number in a position is 0, the column is one of the
following, indicating something special:
*/
#define SP_COL_UNKNOWN 0
			/* The position is unknown.  Used during 
			   initialization and for generated entities. */
#define SP_COL_CMD_LINE 1
			/* The position is in the command line. */

#define SP_PREINCLUDE 2
			/* A special source position used for precompiled
			   header processing to indicate that the header
			   stop position is after the preincluded file. */

#define SP_COL_PREDEFINED_MACRO 3
			/* The position is in a predefined macro. */

/* Macro to copy a source position. */
#define copy_source_position(from, to) ((to) = (from))

#if FULLY_RESOLVED_MACRO_POSITIONS
/* Macro to copy the seq/column from a simple source position to both the
   seq/column and orig_seq/orig_column of a full source position, as well as
   setting the macro_context (if any) to NO_PARENT_MACRO_INVOCATION. */
#if RECORD_MACRO_INVOCATIONS
#define set_macro_context_to_no_parent(to) \
  (to).macro_context = NO_PARENT_MACRO_INVOCATION;
#else /* !RECORD_MACRO_INVOCATIONS */
#define set_macro_context_to_no_parent(to)  /* nothing */
#endif /* RECORD_MACRO_INVOCATIONS */
#define copy_simple_position_to_full_position(from, to) \
{ (to).seq = (to).orig_seq = (from).seq;                \
  (to).column = (to).orig_column = (from).column;       \
  set_macro_context_to_no_parent((to));                 \
}  /* copy_simple_position_to_full_position */
#define set_position_to(pos, seqno, col)      \
{ (pos).seq = (pos).orig_seq = (seqno);       \
  (pos).column = (pos).orig_column = (col);   \
  set_macro_context_to_no_parent((pos));      \
}  /* set_position_to */

/* Macro to extract the macro context from a source position. */
#if RECORD_MACRO_INVOCATIONS
#define macro_context_of(pos) ((pos).macro_context)
#else /* !RECORD_MACRO_INVOCATIONS */
#define macro_context_of(pos) (NO_PARENT_MACRO_INVOCATION)
#endif /* RECORD_MACRO_INVOCATIONS */
#else /* !FULLY_RESOLVED_MACRO_POSITIONS */
#define set_position_to(pos, seqno, col)    \
{ (pos).seq = (seqno); (pos).column = (col); }
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */

/*
Macro to compare two source positions.

  return >0 if pos1 is greater than pos2
  return  0 if pos1 is equal to pos2
  return <0 if pos1 is less than pos2
*/
#define cmp_source_positions(pos1, pos2)				\
  (((pos1).seq != (pos2).seq)						\
       ?  (long)((pos1).seq) - ((long)(pos2).seq)			\
       :  ((long)(pos1).column) - ((long)(pos2).column))
 
EXTERN a_source_position
		null_source_position
#if VAR_INITIALIZERS
                                     = { 0, SP_COL_UNKNOWN
#if FULLY_RESOLVED_MACRO_POSITIONS
                                         , SP_COL_UNKNOWN, 0
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
#if RECORD_MACRO_INVOCATIONS
                                         , NO_PARENT_MACRO_INVOCATION
#endif /* RECORD_MACRO_INVOCATIONS */
                                       }
#endif /* VAR_INITIALIZERS */
                                       ;
			/* NULL source position, for initialization. */

EXTERN a_source_position
		preinclude_source_position
#if VAR_INITIALIZERS
                                     = { 0, SP_PREINCLUDE
#if FULLY_RESOLVED_MACRO_POSITIONS
                                         , SP_PREINCLUDE, 0
#endif /* FULLY_RESOLVED_MACRO_POSITIONS */
#if RECORD_MACRO_INVOCATIONS
                                         , NO_PARENT_MACRO_INVOCATION
#endif /* RECORD_MACRO_INVOCATIONS */
                                       }
#endif /* VAR_INITIALIZERS */
                                       ;
			/* Special position used for PCH processing of
			   preincluded files. */

typedef struct a_simple_source_position *a_simple_source_position_ptr;
typedef struct a_simple_source_position {
  /* Indicates a source position without the dual-resolution (macro)
     features of a_source_position. */
  a_seq_number	seq;
  a_column_number
		column;
} a_simple_source_position;


inline a_boolean operator<(const a_simple_source_position &p1,
                           const a_simple_source_position &p2)
/* Returns TRUE if p1 has a simple source position less than p2. */
{
  return (a_boolean)(p1.seq < p2.seq ||
                     (p1.seq == p2.seq && p1.column < p2.column));
}  /* operator< */


inline a_boolean operator>(const a_simple_source_position &p1,
                           const a_simple_source_position &p2)
/* Returns TRUE if p1 has a simple source position greater than p2. */
{
  return (a_boolean)(p1.seq > p2.seq ||
                     (p1.seq == p2.seq && p1.column > p2.column));
}  /* operator> */


inline a_boolean operator==(const a_simple_source_position &p1,
                            const a_simple_source_position &p2)
/* Returns TRUE if p1 has a simple source position equal to p2. */
{
  return (a_boolean)(p1.seq == p2.seq && p1.column == p2.column);
}  /* operator== */


enum a_C_dialect {
  /* Possible C/C++ dialects to compile. */
  C_dialect_ANSI,	/* ANSI C. */
  C_dialect_pcc,	/* UNIX pcc C. */
  C_dialect_cplusplus	/* C++. */
};


EXTERN_THREAD a_C_dialect
		C_dialect;
			/* The C dialect to be accepted.  This is here because
			   it's convenient to allow "back end" pieces to
			   use C_mode(). */

/*
Returns TRUE in C mode (ANSI or pcc) and returns FALSE in C++ mode.
*/
#define C_mode() (C_dialect != C_dialect_cplusplus)

#ifndef offsetof
/*
Define a version of the offsetof macro if the host system does not provide
one.
*/
#ifdef __EDG__
#define offsetof(t, memb) ((size_t)__INTADDR__(&(((t *)0)->memb)))
#else /* ifndef __EDG__ */
#define offsetof(t, memb) ((size_t)&(((t *)0)->memb))
#endif /* ifdef __EDG__ */
#else /* defined(offsetof) */
#ifdef __EDG__
/*
Newer GNU headers define offsetof in terms of __builtin_offsetof, so
define a version that will work for self-compilation.
*/
#define __builtin_offsetof(t, memb) ((size_t)__INTADDR__(&(((t *)0)->memb)))
#else /* !defined(__EDG__) */
#ifdef _lint
#define __builtin_offsetof(t, memb) ((size_t)&(((t *)0)->memb))
#endif /* ifdef _lint */
#endif /* ifdef __EDG__ */
#endif /* ifndef offsetof */

#if defined(__sun) && defined(__i386__)
/* Some versions of the GNU float.h file on Intel Solaris incorrectly
   define the long double macros (LDBL_MANT_DIG, etc.), giving them the
   values appropriate for the double type.  If LDBL_MANT_DIG is set
   incorrectly, assume that all of the macros are set incorrectly and
   reset them here. */
#ifdef __GNUC__
#if __GNUC__ == 3 && __GNUC_MINOR__ <= 2
END_EDG_NAMESPACE  /* Conditionally close the "edg" namespace. */
#include <float.h>
BEGIN_EDG_NAMESPACE  /* Conditionally open the "edg" namespace. */
#if LDBL_MANT_DIG == 53
#undef LDBL_MANT_DIG
#undef LDBL_DENORM_MIN
#undef LDBL_DIG
#undef LDBL_EPSILON
#undef LDBL_MANT_DIG
#undef LDBL_MAX_10_EXP
#undef LDBL_MAX
#undef LDBL_MAX_EXP
#undef LDBL_MIN_10_EXP
#undef LDBL_MIN
#undef LDBL_MIN_EXP
#define LDBL_DENORM_MIN 3.64519953188247460253e-4951L
#define LDBL_DIG 18
#define LDBL_EPSILON 1.08420217248550443401e-19L
#define LDBL_MANT_DIG 64
#define LDBL_MAX_10_EXP 4932
#define LDBL_MAX 1.18973149535723176502e+4932L
#define LDBL_MAX_EXP 16384
#define LDBL_MIN_10_EXP (-4931)
#define LDBL_MIN 3.36210314311209350626e-4932L
#define LDBL_MIN_EXP (-16381)
#endif /* LDBL_MANT_DIG == 53 */
#endif /* __GNUC__ == 3 && __GNUC_MINOR__ <= 2 */
#endif /* ifdef __GNUC__ */
#endif /* defined(__sun) && defined(__i386__) */

/*
Type of the value returned by hash functions.
*/
typedef uint32_t
		a_hash_value;


/*
Define macros for lint-like annotations.  These can expand to various
annotations depending on the compiler/tool that is being used to read the
source code.

ARG_UNUSED: specifies that the argument that follows may not be used in all
configurations.

LOCAL_UNUSED: specifies that the local variable that follows may not be used in
all configurations.

FALLTHROUGH: specifies that flow-of-control purposely flows from one case to
another without an intervening "break" statement.

NORETURN: specifies that a function will not return to its caller.
*/
#ifdef _lint
/* Use PC-lint annotations. */
#define FALLTHROUGH /*lint -fallthrough*/
#define ARG_UNUSED /*lint -e{715}*/
#define LOCAL_UNUSED /*lint -e{715}*/
#define NORETURN [[noreturn]]
#else /* !defined(_lint) */
/* Use standard attributes if those are supported. */
#ifndef __has_cpp_attribute
#define __has_cpp_attribute(x) 0
#endif /* !defined(__has_cpp_attribute) */

/*
If the front end is compiled in C++17 mode, use the standard fallthrough
attribute.  Otherwise, if this is a compatible version of GCC or Clang, use the
appropriate non-standard attribute.
*/
#if __has_cpp_attribute(fallthrough) && \
    (__cplusplus >= __has_cpp_attribute(fallthrough))
/*lint -estring(823,FALLTHROUGH)*/
#define FALLTHROUGH [[fallthrough]];
#else /* !(__has_cpp_attribute(fallthrough) && ...) */
#if defined(__GNUC__) && \
    (__GNUC__ > 7 || (__GNUC__ == 7 && __GNUC_MINOR__ >= 1))
#define FALLTHROUGH [[gnu::fallthrough]];
#else  /* !(defined(__GNUC__) && __GNUC__ > 7 || (__GNUC__ == 7 && ...)) */
#if defined(__clang__) && \
    (__clang_major__ > 3 || (__clang_major__ == 3 && __clang_minor__ >= 2))
#define FALLTHROUGH [[clang::fallthrough]];
#else  /* !(defined(__clang__) && (__clang_major__ > 3 || ...)) */
#define FALLTHROUGH /*nothing*/
#endif /* defined(__clang__) && (__clang_major__ > 3 || ...) */
#endif /* defined(__GNUC__) && __GNUC__ > 7 || (__GNUC__ == 7 && ...) */
#endif /* __has_cpp_attribute(fallthrough) && ... */

#if __has_cpp_attribute(maybe_unused)
#if defined(__clang__) && __clang__
/*
Clang warns about [[maybe_unused]] in pre-C++17 modes. However, it supports the
[[gnu::unused]] attribute in C++11 mode.
*/
#define ARG_UNUSED [[gnu::unused]]
#define LOCAL_UNUSED [[gnu::unused]]
#else /* !defined(__clang__) && __clang__ */
#define ARG_UNUSED [[maybe_unused]]
#define LOCAL_UNUSED [[maybe_unused]]
#endif /* defined(__clang__) && __clang__ */
#else /* !(__has_cpp_attribute(maybe_unused)) */
#ifdef __GNUC__
#define ARG_UNUSED __attribute__((unused))
#define LOCAL_UNUSED __attribute__((unused))
#else /* !defined __GNUC__ */
#define ARG_UNUSED /*nothing*/
#define LOCAL_UNUSED /*nothing*/
#endif /* defined __GNUC__ */
#endif /* __has_cpp_attribute(maybe_unused) */

/*
Most supported C++ compilers support [[noreturn]], but not all GCC versions.
*/
#ifdef __GNUC__
#if __has_cpp_attribute(noreturn)
#define NORETURN [[noreturn]]
#else  /* !__has_cpp_attribute(noreturn) */
#define NORETURN __attribute__((noreturn))
#endif /* __has_cpp_attribute(noreturn) */
#else /* !defined(__GNUC__) */
#define NORETURN [[noreturn]]
#endif /* defined(__GNUC__) */
#endif /* defined(_lint) */

/*
Define a utility for composing _Pragma string literals.
*/
#define AS_PRAGMA(...) _Pragma(#__VA_ARGS__)

/*
GCC sometimes emits spurious warnings (particularly at high optimization
levels).  SUPPRESS_BAD_GNU_WARNINGS (on by default for GCC builds) controls
global suppression of these warnings.
*/
#ifndef SUPPRESS_BAD_GNU_WARNINGS
#if defined(__GNUC__) && !defined(__clang__) && !defined(__EDG__)
#define SUPPRESS_BAD_GNU_WARNINGS 1
#else  /* !(defined(__GNUC__) && !defined(__clang__) && !defined(__EDG__)) */
#define SUPPRESS_BAD_GNU_WARNINGS 0
#endif /* defined(__GNUC__) && !defined(__clang__) && !defined(__EDG__) */
#endif /* !defined(SUPPRESS_BAD_GNU_WARNINGS) */

/* Define base macros for silencing GCC warnings.  These do not check GCC
   versions and shouldn't be used directly. */
#if SUPPRESS_BAD_GNU_WARNINGS
#define BEGIN_DISABLE_GCC_WARNING(warning) \
  _Pragma("GCC diagnostic push") \
  AS_PRAGMA(GCC diagnostic ignored warning)
#define END_DISABLE_GCC_WARNING \
  _Pragma("GCC diagnostic pop")
#else  /* !SUPPRESS_BAD_GNU_WARNINGS */
#define BEGIN_DISABLE_GCC_WARNING(warning) /* nothing */
#define END_DISABLE_GCC_WARNING /* nothing */
#endif /* SUPPRESS_BAD_GNU_WARNINGS */
/* Define a macro for -Wdangling-pointer (added in GCC 12.1). */
#if defined(__GNUC__) && \
    (__GNUC__ > 12 || (__GNUC__ == 12 && __GNUC_MINOR__ >= 1))
#define BEGIN_DISABLE_GCC_WARNING_DANGLING_PTR \
  BEGIN_DISABLE_GCC_WARNING("-Wdangling-pointer")
#define END_DISABLE_GCC_WARNING_DANGLING_PTR \
  END_DISABLE_GCC_WARNING
#else  /* !(defined(__GNUC__) && __GNUC__ > 12 || (__GNUC__ == 12 && ...)) */
#define BEGIN_DISABLE_GCC_WARNING_DANGLING_PTR /* nothing */
#define END_DISABLE_GCC_WARNING_DANGLING_PTR /* nothing */
#endif /* defined(__GNUC__) && __GNUC__ > 12 || (__GNUC__ == 12 && ...) */
/* Define a macro for -Wstringop-overflow (added in GCC 7.1). */
#if defined(__GNUC__) && \
    (__GNUC__ > 7 || (__GNUC__ == 7 && __GNUC_MINOR__ >= 1))
#define BEGIN_DISABLE_GCC_WARNING_STR_OVERFLOW \
  BEGIN_DISABLE_GCC_WARNING("-Wstringop-overflow")
#define END_DISABLE_GCC_WARNING_STR_OVERFLOW \
  END_DISABLE_GCC_WARNING
#else  /* !(defined(__GNUC__) && __GNUC__ > 7 || (__GNUC__ == 7 && ...)) */
#define BEGIN_DISABLE_GCC_WARNING_STR_OVERFLOW /* nothing */
#define END_DISABLE_GCC_WARNING_STR_OVERFLOW /* nothing */
#endif /* defined(__GNUC__) && __GNUC__ > 7 || (__GNUC__ == 7 && ...) */
/* Define a macro for -Wreturn-local-addr (added in GCC 4.8). */
#if defined(__GNUC__) && \
    (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8))
#define BEGIN_DISABLE_GCC_WARNING_RET_LOCAL_ADDR \
  BEGIN_DISABLE_GCC_WARNING("-Wreturn-local-addr")
#define END_DISABLE_GCC_WARNING_RET_LOCAL_ADDR \
  END_DISABLE_GCC_WARNING
#else  /* !(defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && ...))) */
#define BEGIN_DISABLE_GCC_WARNING_RET_LOCAL_ADDR /* nothing */
#define END_DISABLE_GCC_WARNING_RET_LOCAL_ADDR /* nothing */
#endif /* defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && ...)) */
/* Define a macro for -Wmaybe-unitialized (added in GCC 4.7). */
#if defined(__GNUC__) && \
    (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7))
#define BEGIN_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED \
  BEGIN_DISABLE_GCC_WARNING("-Wmaybe-uninitialized")
#define END_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED \
  END_DISABLE_GCC_WARNING
#else  /* !(defined(__GNUC__) && __GNUC__ > 4 || (__GNUC__ == 4 && ...)) */
#define BEGIN_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED /* nothing */
#define END_DISABLE_GCC_WARNING_MAYBE_UNITIALIZED /* nothing */
#endif /* defined(__GNUC__) && __GNUC__ > 4 || (__GNUC__ == 4 && ...) */

/*
As some compilers have issues with deleted function templates,
HOST_SUPPORTS_DELETED_FUNCTION_TEMPLATES and DELETED_FN_DEF are provided to
allow workarounds to be implemented.

Typically, the DELETED_FN_DEF macro is used in place of an explicit "= delete;"
for a deleted function.  This macro covers the general case by simply using an
empty function body (which no known problematic implementation diagnoses for
non-constexpr function templates that specify a return type).  In other cases,
HOST_SUPPORTS_DELETED_FUNCTION_TEMPLATES can be used directly to create
appropriate logic.

Deleted function template support is (by default) automatically disabled for
versions of Clang prior to 3.9 (which all suffer from this problem).  If the
default is incorrect for the compiler being used to build the front end,
setting HOST_SUPPORTS_DELETED_FUNCTION_TEMPLATES to the desired value should be
sufficient.
*/
#ifndef HOST_SUPPORTS_DELETED_FUNCTION_TEMPLATES
#if defined(__clang__) && \
    (__clang_major__ == 3 && __clang_minor__ < 9)
#define HOST_SUPPORTS_DELETED_FUNCTION_TEMPLATES 0
#else  /* !(defined(__clang__) && (__clang_major__ == 3 && ...)) */
#define HOST_SUPPORTS_DELETED_FUNCTION_TEMPLATES 1
#endif /* defined(__clang__) && (__clang_major__ == 3 && ...) */
#endif /* ifndef HOST_SUPPORTS_DELETED_FUNCTION_TEMPLATES */
#if HOST_SUPPORTS_DELETED_FUNCTION_TEMPLATES
#define DELETED_FN_DEF = delete;
#else /* !HOST_SUPPORTS_DELETED_FUNCTION_TEMPLATES */
#define DELETED_FN_DEF {}
#endif /* HOST_SUPPORTS_DELETED_FUNCTION_TEMPLATES */

/*
Define macros for declaring arrays with static initialization of a
given specifiers, type, name, and size.

Global arrays that should be initialized during the compilation of fe_init.c
should use EXTERN_CONSTINIT_ARRAY and EXTERN_CONSTINIT_ARRAY_END as follows
(for an array of 23 integers named example_decl):

  EXTERN_CONSTINIT_ARRAY(int, example_decl, 23)
  #if VAR_INITIALIZERS
  = {
    // values
  }
  #endif
  EXTERN_CONSTINIT_ARRAY_END(example_decl)

This will create a declaration that in translation units other than fe_init.c
is equivalent to:

  extern int example_decl[23];

in the fe_init.c translation unit this will appear as:

  static constexpr sizeof_t example_decl_expected_size = 23;
  int example_decl[] = {
    // values
  };
  static_assert((sizeof(example_decl) / sizeof(example_decl[0]) >=
                example_decl_expected_size,
                "there are one or more missing elements in the array "
                "\"example_decl\"");
  static_assert((sizeof(example_decl) / sizeof(example_decl[0]) <=
                example_decl_expected_size,
                "there are one or more extra elements in the array "
                "\"example_decl\"");

If a forward declaration is needed,
EXTERN_CONSTINIT_ARRAY_FORWARD_DECL can be used:

  EXTERN_CONSTINIT_ARRAY_FORWARD_DECL(int, example_decl, 46)

this will expand to:

  extern int example_decl[46];

unless the translation unit being compiled is fe_init.c (in that case it
expands to nothing).  If a forward declaration is created manually, the array
size will not be properly checked as the forward declaration gives the array an
explicit size rather than allowing the host compiler to compute array length.

For declarations that do not follow this initialization pattern,
CONSTINIT_ARRAY can be used to set up the appropriate checking with explicit
specifier and initialization management.  For instance, to create an internally
linked array of 46 integers named example_decl:

  CONSTINIT_ARRAY(static, int, example_decl, 46)
  = {
    // values
  }
  CONSTINIT_ARRAY_END(example_decl)

this will similarly expand to:

  static constexpr sizeof_t example_decl_expected_size = 46;
  static int example_decl[] = {
    // values
  };
  static_assert((sizeof(example_decl) / sizeof(example_decl[0]) >=
                example_decl_expected_size,
                "there are one or more missing elements in the array "
                "\"example_decl\"");
  static_assert((sizeof(example_decl) / sizeof(example_decl[0]) <=
                example_decl_expected_size,
                "there are one or more extra elements in the array "
                "\"example_decl\"");

*/
#define CONSTINIT_ARRAY(specifiers, type, name, size)                         \
  static constexpr sizeof_t EDG_CONCAT(name, _expected_size) = size;          \
  specifiers type name[]
#define CONSTINIT_ARRAY_END(name)                               ;             \
  static_assert(((sizeof(name) / sizeof(name[0])) >=                          \
                 EDG_CONCAT(name, _expected_size)),                           \
                "there are one or more missing elements in the array "        \
                "\"" #name "\"");                                             \
  static_assert(((sizeof(name) / sizeof(name[0])) <=                          \
                 EDG_CONCAT(name, _expected_size)),                           \
                "there are one or more extra elements in the array "          \
                "\"" #name "\"");
#if VAR_INITIALIZERS
#define EXTERN_CONSTINIT_ARRAY_FORWARD_DECL(type, name, size)                 \
  /* nothing */
#define EXTERN_CONSTINIT_ARRAY(type, name, size)                              \
  CONSTINIT_ARRAY(, type, name, size)
#define EXTERN_CONSTINIT_ARRAY_END(name) CONSTINIT_ARRAY_END(name)
#else /* !VAR_INITIALIZERS */
#define EXTERN_CONSTINIT_ARRAY_FORWARD_DECL(type, name, size)                 \
  extern type name[size];
#define EXTERN_CONSTINIT_ARRAY(type, name, size) extern type name[size]
#define EXTERN_CONSTINIT_ARRAY_END(name) ;
#endif /* VAR_INITIALIZERS */

/*
Some coding standards require a default label in switches even if it's
unreachable, while others prefer to leave this off to allow the compiler to
catch when the case labels are not exhaustive.  A TRUE value for
CHECK_SWITCH_DEFAULT_UNEXPECTED causes the default_is_unexpected and
default_is_unexpected_str macros (defined in checking.h) to expand to default
cases that abort the compilation if reached in a CHECKING configuration; a
FALSE value results in omission of the default cases.
*/
#ifndef CHECK_SWITCH_DEFAULT_UNEXPECTED
#define CHECK_SWITCH_DEFAULT_UNEXPECTED CHECKING
#endif /* ifndef CHECK_SWITCH_DEFAULT_UNEXPECTED */

/*
In some cases, lint mistakenly determines that a value may be uninitialized
and warns on every use of that variable.  This macro is used to convince lint
that the value is indeed initialized.
*/
#ifdef _lint
/*lint -emacro(530,LINT_IS_INITIALIZED)*/
#define LINT_IS_INITIALIZED(x) ((x) = (x))
#else /* !defined(_lint) */
#define LINT_IS_INITIALIZED(x) /*nothing*/
#endif /* defined(_lint) */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef BASICS_H */

