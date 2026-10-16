/**
 * @file llvm_gen_be_rtti.h
 * @brief RTTI generation subsystem for the EDG LLVM backend.
 * @details Handles the creation and management of type_info structures, MSVC RTTI descriptors,
 * dynamic_cast lowering, and typeid lowering for both the Itanium and MSVC C++ ABIs.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_RTTI_H
#define LLVM_GEN_BE_RTTI_H 1

#include "basic_hdrs.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/Constant.h>
#include <llvm/IR/Value.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Generates or retrieves the LLVM global variable for a given type's RTTI type_info.
 * @details Generates `type_info` structures for fundamental types, pointers, classes, and derived classes,
 *          including Itanium ABI `__si_class_type_info` and `__vmi_class_type_info` descriptors,
 *          as well as MSVC TypeDescriptors.
 * @param[in] type The EDG AST type pointer.
 * @param[out] out_const Receives the resulting LLVM Constant pointer on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t get_typeinfo_global(
    a_type_ptr type,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Emits MSVC RTTI Complete Object Locator and hierarchy descriptors.
 * @details Generates `_s__RTTICompleteObjectLocator` and associated base class arrays for MSVC.
 * @param[in] type The EDG class AST type pointer.
 * @param[out] out_const Receives the LLVM Constant pointer for the CompleteObjectLocator global.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_emit_msvc_rtti_complete_object_locator(
    a_type_ptr type,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Lowers a C++ dynamic_cast expression to LLVM IR.
 * @details Translates pointer and reference dynamic casts to `__dynamic_cast` runtime calls
 *          with offset hints and bad_cast branch checks.
 * @param[in] expr The EDG AST expression node representing the dynamic_cast.
 * @param[out] out_val Receives the computed LLVM Value pointer on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_dynamic_cast(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a C++ typeid expression to LLVM IR.
 * @details Evaluates static typeid expressions to global typeinfo pointers, or polymorphic
 *          typeid expressions by loading from the object's virtual table with bad_typeid checks.
 * @param[in] expr The EDG AST expression node representing the typeid expression.
 * @param[out] out_val Receives the computed LLVM Value pointer representing the type_info on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_typeid(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

END_EDG_NAMESPACE
#endif

#endif /* LLVM_GEN_BE_RTTI_H */
