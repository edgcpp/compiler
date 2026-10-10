/**
 * @file gcc_gen_be_main.h
 * @brief Main entry points for the GCC backend.
 *
 * This file declares the primary entry points (`gcc_gen_be` and `back_end`)
 * that interface directly with the EDG frontend's execution flow.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_MAIN_H
#define GCC_GEN_BE_MAIN_H

#include "gcc_gen_be_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Main entry point for the GCC backend.
 *
 * It orchestrates the translation of the EDG AST into libgccjit constructs
 * and triggers compilation.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Hook called by the EDG frontend to execute the backend.
 *
 * This is the standard entry point expected by the EDG frontend architecture.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t back_end(void) GCC_GEN_BE_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#endif /* GCC_GEN_BE_MAIN_H */