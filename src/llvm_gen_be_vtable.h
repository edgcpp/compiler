/**
 * @file llvm_gen_be_vtable.h
 * @brief Virtual Table (vtable) and dispatch generation for the EDG LLVM backend.
 * @details Generates vtables for the Itanium and MSVC C++ ABIs and handles virtual function dispatch.
 *
 * Part of the EDG Compiler Project.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_VTABLE_H
#define LLVM_GEN_BE_VTABLE_H 1

#include "basic_hdrs.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/Constant.h>
#include <llvm/IR/Value.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Emits the virtual table for a given class.
 * @details Iterates over the class vtable entries and creates an LLVM GlobalVariable holding the vtable data.
 * @param[in] type The EDG AST type pointer for the class.
 * @param[out] out_const The resulting LLVM Constant pointer for the vtable global.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_emit_vtable(a_type_ptr type, llvm::Constant** out_const) noexcept;

/**
 * @brief Lowers a virtual method call.
 * @details Emits code to load the vptr, calculate the GEP for the offset, and load the function pointer.
 * @param[in] object_ptr The LLVM value pointing to the object instance.
 * @param[in] method_routine The EDG routine pointer for the virtual method.
 * @param[out] out_func_ptr The loaded virtual function pointer.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_virtual_dispatch(llvm::Value* object_ptr, a_routine_ptr method_routine, llvm::Value** out_func_ptr) noexcept;

END_EDG_NAMESPACE
#endif

#endif /* LLVM_GEN_BE_VTABLE_H */
