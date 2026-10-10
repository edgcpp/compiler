/**
 * @file gcc_gen_be_decl.h
 * @brief Declaration lowering for the GCC backend.
 *
 * This file declares functions for lowering EDG variables and functions
 * into libgccjit global variables and functions.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_DECL_H
#define GCC_GEN_BE_DECL_H

#include "gcc_gen_be_error.h"
#include "fe_common.h"
#include "il.h"

struct gcc_jit_lvalue;
struct gcc_jit_function;
struct gcc_jit_block;

BEGIN_EDG_NAMESPACE

/**
 * @brief Retrieves or creates the global dynamic initialization block.
 *
 * @param out_block A pointer to receive the gcc_jit_block.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_global_ctor_block(struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers an EDG variable declaration into a libgccjit global lvalue.
 *
 * @param var The frontend variable node.
 * @param out_lval A pointer to a gcc_jit_lvalue pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_lval` populated.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers an EDG variable declaration into a libgccjit local lvalue.
 *
 * @param func The libgccjit function the local belongs to.
 * @param var The frontend variable node.
 * @param out_lval A pointer to a gcc_jit_lvalue pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_lval` populated.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_local_variable_decl(struct gcc_jit_function *func, a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers an EDG function/routine declaration into a libgccjit function.
 *
 * @param rout The frontend routine node.
 * @param out_func A pointer to a gcc_jit_function pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_func` populated.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_DECL_H */