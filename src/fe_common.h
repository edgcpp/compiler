/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

fe_common.h -- Inclusion of header files common to all EDG front end files.

These header files involve declarations upon which other declarations depend
and/or which are used throughout the compiler rather than in a single phase
of processing.  This is a list of the files that are directly or indirectly
incorporated:

    attribute.h
    basics.h
    cmd_line.h
    const_ints.h
    debug.h
    def_arg.h
    defines.h
    err_codes.h
    error.h
    extasm.h
    float_pt.h
    floating.h
    header_util.h
    host_envir.h
    il.h
    il_def.h
    il_to_str.h
    lang_feat.h
    lexical.h
    mem_manage.h
    mem_tables.h
    modules.h
    src_seq.h
    symbol_tbl.h
    trans_corresp.h
    trans_unit.h
    types.h
    targ_def.h
    target.h
    util.h
    version.h

*/

/* Avoid including these declarations more than once. */
#ifndef FE_COMMON_H
#define FE_COMMON_H 1

/* Basic configuration declarations.  This header file pulls in basics.h,
   defines.h, lang_feat.h, host_envir.h, and targ_def.h. */
#ifndef BASIC_HDRS_H
#include "basic_hdrs.h"
#endif /* ifndef BASIC_HDRS_H */

/* Assertion checking. */
#include "checking.h"

/* Errors.  error.h also pulls in err_codes.h. */
#include "error.h"

/* Front end version number. */
#include "version.h"

/* General utility components. */
#include "util.h"

/* Memory management data structures. */
#include "mem_tables.h"

/* IL declarations.  Note that il.h pulls in il_def.h. */
#include "il.h"

/* IL allocation declarations. */
#include "il_alloc.h"

/* Target configuration variables. */
#include "target.h"

/* Additional declarations relating to memory management. */
#include "mem_manage.h"

/* Manipulation of target integer constants. */
#include "const_ints.h"

#if FIXED_POINT_ALLOWED
/* Manipulation of internal fixed-point quantities. */
#include "fixed_pt.h"
#endif /* FIXED_POINT_ALLOWED */

/* Manipulation of internal floating point quantities. */
#include "float_pt.h"
#include "floating.h"

/* Production of a string-form representation of IL entities. */
#include "il_to_str.h"

#if !STANDALONE_UTILITY_PROGRAM
/* Symbol table declarations.  symbol_tbl.h also pulls in lexical.h. */
#include "symbol_tbl.h"

/* Identifier lookup routines. */
#include "lookup.h"

/* Variables set on the basis of command line options. */
#include "cmd_line.h"

/* Attributes. */
#include "attribute.h"

/* Extended asm statements. */
#include "extasm.h"

#endif /* !STANDALONE_UTILITY_PROGRAM */

#include "ms_metadata.h"

/* Type system support. */
#include "types.h"

#if GENERATE_SOURCE_SEQUENCE_LISTS
/* Source sequence list management */
#include "src_seq.h"
#endif /* GENERATE_SOURCE_SEQUENCE_LISTS */

/* Translation unit data structures. */
#include "trans_unit.h"
#include "trans_corresp.h"

#if DEBUG
/* GCOVR_EXCL_START */
/* Debug declarations. */
#include "debug.h"
/* GCOVR_EXCL_STOP */
#endif /* DEBUG */

#include "sys_predef.h"

/* Module file support. */
#include "modules.h"

#endif /* ifndef FE_COMMON_H */

