/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

cp_gen_be.h - Declarations related to cp_gen_be.c (C++/C-generating back end).

*/

/* Avoid including these declarations more than once: */
#ifndef CP_GEN_BE_H
#define CP_GEN_BE_H 1

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */

#if BACK_END_IS_CP_GEN_BE

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

#if !STANDALONE_UTILITY_PROGRAM
#if !BACK_END_IS_LLVM_GEN_BE
extern void back_end(void);
#endif
#endif /* !STANDALONE_UTILITY_PROGRAM */

#if MAKE_FRONT_END_CALLABLE
extern void cp_gen_be_early_init();
extern void cp_gen_be_cleanup();
#endif /* MAKE_FRONT_END_CALLABLE */

extern a_boolean expr_has_comma_operation(an_expr_node_ptr expr);

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* BACK_END_IS_CP_GEN_BE */

#endif /* ifndef CP_GEN_BE_H */

