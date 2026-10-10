/**
 * @file gcc_gen_be_expr.h
 * @brief Expression lowering for the GCC backend.
 *
 * This file declares functions for lowering EDG AST expression nodes
 * into libgccjit lvalues and rvalues.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_EXPR_H
#define GCC_GEN_BE_EXPR_H

#include "gcc_gen_be_error.h"
#include "fe_common.h"
#include "expr.h"

struct gcc_jit_lvalue;
struct gcc_jit_rvalue;
struct gcc_jit_type;

BEGIN_EDG_NAMESPACE

/**
 * @brief Lowers an EDG expression into a libgccjit lvalue.
 *
 * @param expr The frontend expression node.
 * @param out_lval A pointer to a gcc_jit_lvalue pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_lval` populated.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_expr_lvalue(an_expr_node_ptr expr, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers an EDG expression into a libgccjit rvalue.
 *
 * @param expr The frontend expression node.
 * @param out_rval A pointer to a gcc_jit_rvalue pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_rval` populated.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_expr_rvalue(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers an EDG constant into a libgccjit rvalue.
 *
 * @param con The frontend constant.
 * @param expected_type The expected libgccjit type.
 * @param out_rval A pointer to a gcc_jit_rvalue pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_rval` populated.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_constant_rvalue(a_constant_ptr con, struct gcc_jit_type *expected_type, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_EXPR_H */