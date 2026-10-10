/**
 * @file llvm_gen_be_type.h
 * @brief Type generation and lowering subsystem for the EDG LLVM backend.
 * @details Translates EDG front-end type structures (a_type_ptr) into LLVM IR
 * types (llvm::Type*). All functions return llvm_gen_be_error_t and output
 * constructed types via output pointers.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_TYPE_H
#define LLVM_GEN_BE_TYPE_H 1

#include "basic_hdrs.h"
#include "fe_common.h"
#include "error.h"
#include "il.h"
#include "types.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Type.h>

BEGIN_EDG_NAMESPACE

/**
 * @brief Translates an EDG type representation into a corresponding LLVM type.
 * @details Traverses the EDG type structure, looks up or populates the type cache,
 * and creates LLVM primitive, pointer, array, aggregate, or routine types.
 * @param[in] edg_type Pointer to the EDG type description structure. If null, produces void.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, llvm_gen_be_error_t::invalid_argument if
 *         out_type is null, or an error code indicating the failure reason.
 */
llvm_gen_be_error_t llvm_type_from_edg_type(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Lowers an EDG integer type into a matching LLVM IntegerType.
 * @details Maps integer kinds (char, short, int, long, long long, __int128, bool)
 * into appropriate bit-width LLVM integer types.
 * @param[in] edg_type Pointer to the EDG integer type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_integer(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Lowers an EDG floating-point type into a matching LLVM floating-point Type.
 * @details Handles half/fp16, bfloat16, float, double, x86_fp80, and fp128.
 * @param[in] edg_type Pointer to the EDG floating-point type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_float(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Lowers an EDG pointer or reference type into an LLVM opaque PointerType.
 * @details Constructs an opaque pointer using LLVM's unadorned pointer representation.
 * @param[in] edg_type Pointer to the EDG pointer type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_pointer(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Lowers an EDG array type into an LLVM ArrayType or PointerType.
 * @details Maps fixed-size arrays to llvm::ArrayType, zero-bound arrays to 0-length arrays,
 * and variable-length or incomplete arrays to opaque pointers.
 * @param[in] edg_type Pointer to the EDG array type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_array(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Lowers an EDG vector type into an LLVM VectorType.
 * @details Translates GNU/Clang vector types into llvm::FixedVectorType.
 * @param[in] edg_type Pointer to the EDG vector type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_vector(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Lowers an EDG struct, union, or class type into an LLVM StructType.
 * @details Generates packed structures matching EDG field byte offsets and tail padding.
 * Cyclic struct references are resolved using forward-declared opaque StructTypes.
 * @param[in] edg_type Pointer to the EDG struct, union, or class type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_struct(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Lowers an EDG routine (function) type into an LLVM FunctionType.
 * @details Gathers return type, parameter types, and variadic ellipsis flag.
 * @param[in] edg_type Pointer to the EDG routine type structure.
 * @param[out] out_fn_type Pointer to the variable where the resulting FunctionType* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_routine(
    a_type_ptr edg_type,
    llvm::FunctionType** out_fn_type) noexcept;

/**
 * @brief Legacy compatibility helper redirecting to llvm_type_from_edg_type.
 * @details Convenience wrapper returning llvm::Type* while inspecting error codes internally.
 * @param[in] edg_type Pointer to the EDG type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t get_llvm_type(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_TYPE_H */
