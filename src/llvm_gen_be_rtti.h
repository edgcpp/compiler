/**
 * @file llvm_gen_be_rtti.h
 * @brief RTTI generation subsystem for the EDG LLVM backend.
 * @details Handles the creation and management of type_info structures and globals for the Itanium and MSVC C++ ABIs.
 *
 * Part of the EDG Compiler Project.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_RTTI_H
#define LLVM_GEN_BE_RTTI_H 1

#include "basic_hdrs.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/Constant.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Generates or retrieves the LLVM global variable for a given type's RTTI type_info.
 * @details Generates `type_info` structures for fundamental types, pointers, classes, and derived classes,
 *          including Itanium ABI `__si_class_type_info` and `__vmi_class_type_info` descriptors.
 * @param[in] type The EDG AST type pointer.
 * @param[out] out_const The resulting LLVM Constant pointer (typically a GlobalVariable).
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t get_typeinfo_global(a_type_ptr type, llvm::Constant** out_const) noexcept;

END_EDG_NAMESPACE
#endif

#endif /* LLVM_GEN_BE_RTTI_H */
