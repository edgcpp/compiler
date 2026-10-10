/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

c_gen_be.h - Declarations related to c_gen_be.c (C-generating back end
             for testing)

*/

/* Avoid including these declarations more than once: */
#ifndef C_GEN_BE_H
#define C_GEN_BE_H 1

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */

#if BACK_END_IS_C_GEN_BE

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

#if !C_GEN_BE_GENERATES_ANSI_C
EXTERN_THREAD a_const_char
		*module_list_for_union_init /* = NULL */;
			/* The operand of the command-line "-i" option,
			   a comma-separated list of modules to be linked
			   with this one, and for which union initialization
			   routines should be called. */
#endif /* !C_GEN_BE_GENERATES_ANSI_C */

#if !STANDALONE_UTILITY_PROGRAM
#if !BACK_END_IS_LLVM_GEN_BE
extern void back_end(void);
#endif
#endif /* !STANDALONE_UTILITY_PROGRAM */

#if MAKE_FRONT_END_CALLABLE
extern void c_gen_be_cleanup(void);
#endif /* MAKE_FRONT_END_CALLABLE */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* BACK_END_IS_C_GEN_BE */

#endif /* ifndef C_GEN_BE_H */

