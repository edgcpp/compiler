/**
 * @file gcc_gen_be_location.c
 * @brief Implementation of Source Location Tracking for DWARF Generation.
 *
 * Maps EDG IL source coordinates (line, column, filename) to gcc_jit_location
 * to enable precise DWARF metadata emission.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_location.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "il_def.h"
#include <libgccjit.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

/**
 * @brief Converts an EDG source position into a libgccjit location.
 *
 * @details Uses conv_seq_to_file_and_line to map sequence numbers to file and line coordinates.
 *
 * @param[in] pos The EDG source position pointer.
 * @param[out] out_loc A pointer to a gcc_jit_location pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_loc` populated (can be NULL if pos is invalid).
 */
gcc_gen_be_error_t gcc_gen_be_get_location(a_source_position *pos, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_loc = NULL;

    if (!pos || pos->seq == 0) return GCC_GEN_BE_SUCCESS;

    a_const_char *file_name = NULL;
    a_const_char *full_name = NULL;
    a_line_number line_number = 0;
    a_boolean at_end = FALSE;

    (void)conv_seq_to_file_and_line(pos->seq, &file_name, &full_name, &line_number, &at_end);
    if (!file_name) return GCC_GEN_BE_SUCCESS;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    int col = (int)pos->column;
    if (col <= 0) col = 1;

    *out_loc = gcc_jit_context_new_location(ctx, file_name, (int)line_number, col);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Retrieves the JIT location for a given IL type.
 *
 * @details Extracts the declaration position from the type's source correspondence.
 *
 * @param[in] tp The frontend type.
 * @param[out] out_loc Pointer to receive the JIT location.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_type(a_type_ptr tp, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_loc = NULL;
    if (!tp) return GCC_GEN_BE_SUCCESS;
    return gcc_gen_be_get_location(&tp->source_corresp.decl_position, out_loc);
}

/**
 * @brief Retrieves the JIT location for a given IL variable.
 *
 * @details Extracts the declaration position from the variable's source correspondence.
 *
 * @param[in] var The frontend variable.
 * @param[out] out_loc Pointer to receive the JIT location.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_var(a_variable_ptr var, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_loc = NULL;
    if (!var) return GCC_GEN_BE_SUCCESS;
    return gcc_gen_be_get_location(&var->source_corresp.decl_position, out_loc);
}

/**
 * @brief Retrieves the JIT location for a given IL routine.
 *
 * @details Extracts the declaration position from the routine's source correspondence.
 *
 * @param[in] rout The frontend routine.
 * @param[out] out_loc Pointer to receive the JIT location.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_routine(a_routine_ptr rout, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_loc = NULL;
    if (!rout) return GCC_GEN_BE_SUCCESS;
    return gcc_gen_be_get_location(&rout->source_corresp.decl_position, out_loc);
}

/**
 * @brief Retrieves the JIT location for a given IL expression.
 *
 * @details Extracts the expression position when available.
 *
 * @param[in] expr The frontend expression.
 * @param[out] out_loc Pointer to receive the JIT location.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_expr(an_expr_node_ptr expr, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_loc = NULL;
    if (!expr) return GCC_GEN_BE_SUCCESS;
#if EXTRA_SOURCE_POSITIONS_IN_IL
    if (expr->expr_range.start.seq != 0) {
        return gcc_gen_be_get_location(&expr->expr_range.start, out_loc);
    }
#endif
    return gcc_gen_be_get_location(&expr->position, out_loc);
}

/**
 * @brief Retrieves the JIT location for a given IL statement.
 *
 * @details Extracts the statement position from the statement node.
 *
 * @param[in] stmt The frontend statement.
 * @param[out] out_loc Pointer to receive the JIT location.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_stmt(a_statement_ptr stmt, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_loc = NULL;
    if (!stmt) return GCC_GEN_BE_SUCCESS;
    return gcc_gen_be_get_location(&stmt->position, out_loc);
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
