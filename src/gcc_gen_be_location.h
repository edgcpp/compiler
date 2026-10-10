/**
 * @file gcc_gen_be_location.h
 * @brief Source location mapping for the GCC backend.
 *
 * This file declares functions for converting EDG AST source positions into
 * libgccjit locations for debugging and error reporting.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_LOCATION_H
#define GCC_GEN_BE_LOCATION_H

#include "gcc_gen_be_error.h"
#include "fe_common.h"
#include "il.h"

struct gcc_jit_location;

BEGIN_EDG_NAMESPACE

/**
 * @brief Converts an EDG source position into a libgccjit location.
 *
 * @param pos The EDG source position pointer.
 * @param out_loc A pointer to a gcc_jit_location pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_loc` populated (can be NULL if pos is invalid).
 */
extern gcc_gen_be_error_t gcc_gen_be_get_location(a_source_position *pos, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Attempts to extract a source location from an EDG expression node.
 *
 * @param expr The EDG expression node.
 * @param out_loc A pointer to a gcc_jit_location pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_loc` populated.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_location_from_expr(an_expr_node_ptr expr, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Attempts to extract a source location from an EDG statement node.
 *
 * @param stmt The EDG statement node.
 * @param out_loc A pointer to a gcc_jit_location pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_loc` populated.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_location_from_stmt(a_statement_ptr stmt, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_LOCATION_H */