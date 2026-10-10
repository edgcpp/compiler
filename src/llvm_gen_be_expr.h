/**
 * @file llvm_gen_be_expr.h
 * @brief Expression lowering subsystem for the EDG LLVM backend.
 * @details Translates EDG front-end expression nodes (an_expr_node_ptr) into LLVM IR
 * values (llvm::Value*). All functions return llvm_gen_be_error_t and output
 * constructed values via output pointers.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_EXPR_H
#define LLVM_GEN_BE_EXPR_H 1

#include "basic_hdrs.h"
#include "fe_common.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/Value.h>

BEGIN_EDG_NAMESPACE

/**
 * @brief Lowers an EDG expression into an LLVM Value.
 * @details Traverses the EDG expression tree and emits corresponding LLVM IR instructions.
 * @param[in] expr Pointer to the EDG expression node.
 * @param[out] out_val Pointer to the variable where the resulting llvm::Value* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers an EDG lvalue expression to an LLVM pointer Value.
 * @param[in] expr Pointer to the EDG expression node.
 * @param[out] out_ptr Pointer to the variable where the resulting llvm::Value* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_lvalue_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_ptr) noexcept;

/**
 * @brief Lowers an EDG arithmetic expression to an LLVM Value.
 * @param[in] expr Pointer to the EDG expression node.
 * @param[out] out_val Pointer to the variable where the resulting llvm::Value* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_arithmetic_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers an EDG logical expression to an LLVM Value.
 * @param[in] expr Pointer to the EDG expression node.
 * @param[out] out_val Pointer to the variable where the resulting llvm::Value* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_logical_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers an EDG cast expression to an LLVM Value.
 * @param[in] expr Pointer to the EDG expression node.
 * @param[out] out_val Pointer to the variable where the resulting llvm::Value* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_cast_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers an EDG call expression to an LLVM Value.
 * @param[in] expr Pointer to the EDG expression node.
 * @param[out] out_val Pointer to the variable where the resulting llvm::Value* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_call_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_EXPR_H */
