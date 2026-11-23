/**
 * @file gcc_gen_be_location.h
 * @brief Source location mapping for the GCC backend.
 * @details Declares helpers converting EDG front-end source sequence coordinates
 * into `gcc_jit_location` objects via `conv_seq_to_file_and_line`.
 * @ingroup gcc_gen_be
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
 * @details Resolves full source/header file path and 1-based line/column coordinates
 * via `conv_seq_to_file_and_line`.
 * @param[in] pos The EDG source position pointer.
 * @param[out] out_loc Pointer to a `gcc_jit_location *` receiving the result (NULL if invalid).
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_location(
    a_source_position *pos,
    struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Extracts a `gcc_jit_location` from an EDG expression node.
 * @details Uses `expr_range.start` when available, falling back to `expr->position`.
 * @param[in] expr The EDG expression node.
 * @param[out] out_loc Pointer to receive the `gcc_jit_location *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_location_from_expr(
    an_expr_node_ptr expr,
    struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Extracts a `gcc_jit_location` from an EDG statement node.
 * @details Resolves `stmt->position` via `gcc_gen_be_get_location`.
 * @param[in] stmt The EDG statement node.
 * @param[out] out_loc Pointer to receive the `gcc_jit_location *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_location_from_stmt(
    a_statement_ptr stmt,
    struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Extracts a `gcc_jit_location` from an EDG type declaration.
 * @details Resolves `tp->source_corresp.decl_position` via `gcc_gen_be_get_location`.
 * @param[in] tp The frontend type pointer.
 * @param[out] out_loc Pointer to receive the `gcc_jit_location *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_location_from_type(
    a_type_ptr tp,
    struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Extracts a `gcc_jit_location` from an EDG variable declaration.
 * @details Resolves `var->source_corresp.decl_position` via `gcc_gen_be_get_location`.
 * @param[in] var The frontend variable pointer.
 * @param[out] out_loc Pointer to receive the `gcc_jit_location *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_location_from_var(
    a_variable_ptr var,
    struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Extracts a `gcc_jit_location` from an EDG routine declaration.
 * @details Resolves `rout->source_corresp.decl_position` via `gcc_gen_be_get_location`.
 * @param[in] rout The frontend routine pointer.
 * @param[out] out_loc Pointer to receive the `gcc_jit_location *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_location_from_routine(
    a_routine_ptr rout,
    struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_LOCATION_H */
