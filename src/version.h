/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*
version.h -- Front end version number.
*/

/* Avoid including these declarations more than once. */
#ifndef VERSION_H
#define VERSION_H 1

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

/*
Definition of the version number of this version.  It is made a separate
file to make updates easy.
*/
#define VERSION_NUMBER "7.0"  /* September 28, 2026. */

/*
Version number used to set a predefined macro that expands to the
front end version.  This must be a numeric value.
*/
#define VERSION_NUMBER_FOR_MACRO 700

/*
A string identifying this build of the front end, displayed by --version
and --dump_configuration and used to identify compatible precompiled
header files.  If BUILD_IDENTIFIER is defined (as a string literal), it is
used; otherwise, the date and time at which fe_init.c was compiled are
used.  Defining BUILD_IDENTIFIER makes it possible to build bit-for-bit
reproducible executables; builds from different sources should use
different identifiers so that their precompiled headers are not mistaken
for each other.
*/
#ifdef BUILD_IDENTIFIER

EXTERN a_const_char
		*build_stamp
#if VAR_INITIALIZERS
			    = BUILD_IDENTIFIER
#endif /* VAR_INITIALIZERS */
                                      ;

#else /* !defined(BUILD_IDENTIFIER) */

#ifndef __DATE__
#define __DATE__ "[date unknown]"
#endif /* ifndef __DATE__ */

#ifndef __TIME__
#define __TIME__ "[time unknown]"
#endif /* ifndef __TIME__ */

#ifdef _MSC_VER
/* Suppress MSVC warnings about use of __DATE__ and __TIME__. */
#pragma warning( push )
#pragma warning( disable : 5048 )
#endif /* defined(_MSC_VER) */

EXTERN a_const_char
		*build_stamp
#if VAR_INITIALIZERS
			    = __DATE__ " " __TIME__
#endif /* VAR_INITIALIZERS */
                                      ;

#ifdef _MSC_VER
#pragma warning( pop )
#endif /* defined(_MSC_VER) */

#endif /* ifdef BUILD_IDENTIFIER */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* ifndef VERSION_H */

