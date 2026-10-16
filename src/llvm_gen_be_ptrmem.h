/**
 * @file llvm_gen_be_ptrmem.h
 * @brief Pointer-to-member lowering subsystem for the EDG LLVM backend.
 * @details Implements representation, constants, calls, field selections, conversions,
 * and comparisons for pointer-to-member data and pointer-to-member functions under
 * the Itanium and Microsoft MSVC C++ ABIs.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_PTRMEM_H
#define LLVM_GEN_BE_PTRMEM_H 1

#include "basic_hdrs.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/Constant.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Computes the LLVM Type for an EDG pointer-to-member type.
 * @details Lowers to {ptr, ptrdiff_t} for member functions under Itanium,
 * ptrdiff_t for member data, or MSVC variable-width struct models.
 * @param[in] type Pointer to the EDG pointer-to-member type.
 * @param[out] out_type Receives the computed LLVM Type pointer on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_ptr_to_member(
    a_type_ptr type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Evaluates an EDG ck_ptr_to_member constant into an LLVM Constant.
 * @details Handles null pointer-to-member constants, member data offset constants,
 * and member function pointer constants (virtual and non-virtual).
 * @param[in] con Pointer to the EDG constant structure.
 * @param[in] expected_ty Expected LLVM Type for the constant.
 * @param[out] out_const Receives the generated LLVM Constant on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_const_from_ptr_to_member(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Lowers a pointer-to-member-function call expression (.* or ->*).
 * @details Handles virtual and non-virtual invocation with this-adjustment under Itanium and MSVC.
 * @param[in] expr Pointer to the call expression node (eok_dot_pm_call or eok_points_to_pm_call).
 * @param[out] out_val Receives the result value of the call expression on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_pm_call(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a pointer-to-member-data field dereference expression.
 * @details Displaces the object pointer by the member offset value with null-offset checking.
 * @param[in] expr Pointer to the field selection expression node (eok_pm_field or eok_pm_points_to_field).
 * @param[out] out_val Receives the pointer to the selected member field on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_pm_field_access(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers pointer-to-member conversions between base and derived classes.
 * @details Adjusts the 'adj' displacement or data member offset across inheritance hierarchies.
 * @param[in] expr Pointer to the cast expression node (eok_pm_base_class_cast or eok_pm_derived_class_cast).
 * @param[out] out_val Receives the converted pointer-to-member value on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_pm_cast(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers equality and inequality comparisons between pointer-to-member operands.
 * @details Emits component-wise comparisons for member functions and integer comparisons for data members.
 * @param[in] expr Pointer to the comparison expression node (eok_eq or eok_ne).
 * @param[out] out_val Receives the computed boolean comparison result on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_pm_comparison(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

END_EDG_NAMESPACE
#endif

#endif /* LLVM_GEN_BE_PTRMEM_H */
