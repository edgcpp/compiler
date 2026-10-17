/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

runtime_vars.c -- Definitions of the runtime library variables used by the
                  routines that cpfe-self takes from the runtime library.

lib_src/vars.c defines all of the runtime library's variables, but it also
includes the exception handling declarations, and those (by way of <typeinfo>)
cannot be processed in Microsoft mode.  This defines the variables declared in
runtime.h and main.h in the same way that vars.c does.

*/

#define EXTERN /* empty */
#define VAR_INITIALIZERS 1
#include "basics.h"
#include "runtime.h"
#include "main.h"
