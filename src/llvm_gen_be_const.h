/**
 * @file llvm_gen_be_const.h
 * @brief Constant evaluator subsystem for the EDG LLVM backend.
 * @details Translates EDG front-end constant structures (a_constant_ptr) into LLVM IR
 * constants (llvm::Constant*). All functions return llvm_gen_be_error_t and output
 * constructed constants via output pointers.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_CONST_H
#define LLVM_GEN_BE_CONST_H 1

#include "basic_hdrs.h"
#include "fe_common.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Type.h>

BEGIN_EDG_NAMESPACE

/**
 * @brief Evaluates an EDG constant expression into an LLVM Constant.
 * @details Delegates to specific constant lowering helpers based on the constant kind.
 * @param[in] con Pointer to the EDG constant structure.
 * @param[in] expected_ty The expected LLVM type for the constant.
 * @param[out] out_const Pointer to store the resulting llvm::Constant*.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t evaluate_constant(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Lowers an EDG integral constant.
 * @details Handles signed and unsigned integer constants.
 * @param[in] con Pointer to the EDG constant structure.
 * @param[in] expected_ty The expected LLVM type for the constant.
 * @param[out] out_const Pointer to store the resulting llvm::Constant*.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_const_from_integer(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Lowers an EDG floating-point constant.
 * @details Handles regular floating-point numbers, infinities, and NaNs.
 * @param[in] con Pointer to the EDG constant structure.
 * @param[in] expected_ty The expected LLVM type for the constant.
 * @param[out] out_const Pointer to store the resulting llvm::Constant*.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_const_from_float(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Lowers an EDG string constant.
 * @details Handles character and wide character string literals.
 * @param[in] con Pointer to the EDG constant structure.
 * @param[in] expected_ty The expected LLVM type for the constant.
 * @param[out] out_const Pointer to store the resulting llvm::Constant*.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_const_from_string(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Lowers an EDG aggregate or struct constant.
 * @details Handles arrays and structs by evaluating elements recursively.
 * @param[in] con Pointer to the EDG constant structure.
 * @param[in] expected_ty The expected LLVM type for the constant.
 * @param[out] out_const Pointer to store the resulting llvm::Constant*.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_const_from_aggregate(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Lowers an EDG address constant.
 * @details Handles block addresses for labels.
 * @param[in] con Pointer to the EDG constant structure.
 * @param[in] expected_ty The expected LLVM type for the constant.
 * @param[out] out_const Pointer to store the resulting llvm::Constant*.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_const_from_address(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept;

END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_CONST_H */
