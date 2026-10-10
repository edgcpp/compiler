/**
 * @file gcc_gen_be_location.c
 * @brief Implementation of source location mapping for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_location.h"
#include "gcc_gen_be_context.h"
#include <libgccjit.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE




/**
 * @brief Converts an EDG source position into a libgccjit location.
 *
 * @param pos The EDG source position pointer.
 * @param out_loc A pointer to a gcc_jit_location pointer that will receive the result.
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
    
    a_source_file_ptr sfp = conv_seq_to_file_and_line(pos->seq, &file_name, &full_name, &line_number, &at_end);
    if (!file_name) return GCC_GEN_BE_SUCCESS;
    
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    *out_loc = gcc_jit_context_new_location(ctx, file_name, line_number, pos->column);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Attempts to extract a source location from an EDG expression node.
 *
 * @param expr The EDG expression node.
 * @param out_loc A pointer to a gcc_jit_location pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_loc` populated.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_expr(an_expr_node_ptr expr, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_loc = NULL;

    if (!expr) return GCC_GEN_BE_SUCCESS;
    
    /* Source positions for expressions are typically stored in the source_corresp or similar fields, but EDG's AST doesn't always have one simple position. 
       Let's stick to NULL for now or try to extract it if available. */
    if (expr->source_corresp.pos.seq != 0) {
        GCC_GEN_BE_CHECK(gcc_gen_be_get_location(&expr->source_corresp.pos, out_loc));
    }
    
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Attempts to extract a source location from an EDG statement node.
 *
 * @param stmt The EDG statement node.
 * @param out_loc A pointer to a gcc_jit_location pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_loc` populated.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_stmt(a_statement_ptr stmt, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_loc = NULL;

    if (!stmt) return GCC_GEN_BE_SUCCESS;

    if (stmt->source_corresp.pos.seq != 0) {
        GCC_GEN_BE_CHECK(gcc_gen_be_get_location(&stmt->source_corresp.pos, out_loc));
    }

    return GCC_GEN_BE_SUCCESS;
}



END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */