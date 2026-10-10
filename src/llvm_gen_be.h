/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

llvm_gen_be.h - Declarations related to llvm_gen_be.cpp (LLVM IR-generating back end)

*/

/* Avoid including these declarations more than once: */
#ifndef LLVM_GEN_BE_H
#define LLVM_GEN_BE_H 1

/**
 * @file llvm_gen_be.h
 * @brief Interface for the EDG LLVM generation backend driver.
 * @details Declares the entry point for lowering EDG IL into LLVM IR, writing
 * to output files, and managing backend state cleanup.
 */

#ifndef HOST_ENVIR_H
#include "host_envir.h"
#endif /* ifndef HOST_ENVIR_H */

#ifndef LLVM_GEN_BE_ERROR_H
#include "llvm_gen_be_error.h"
#endif /* ifndef LLVM_GEN_BE_ERROR_H */

#if BACK_END_IS_LLVM_GEN_BE

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE

#if !STANDALONE_UTILITY_PROGRAM
/**
 * @brief Legacy entry point for the backend, called by EDG frontend.
 * @details Invokes the LLVM backend pipeline. Errors are percolated up.
 * @return llvm_gen_be_error_t::ok on success, or a failure code on error.
 */
extern llvm_gen_be_error_t back_end(void) noexcept;
#endif /* !STANDALONE_UTILITY_PROGRAM */

#if MAKE_FRONT_END_CALLABLE
/**
 * @brief Cleans up internal state and resources allocated by the LLVM backend.
 * @details Used when the frontend is embedded and requires memory to be freed
 * between successive compilations.
 * @return llvm_gen_be_error_t::ok on successful cleanup, or an error code.
 */
extern llvm_gen_be_error_t llvm_gen_be_cleanup(void) noexcept;
#endif /* MAKE_FRONT_END_CALLABLE */

/**
 * @brief Main driver for the LLVM generation backend.
 * @details Lowers all IL constructs in the translation unit to LLVM IR, runs
 * optimization passes if configured, and emits target machine code.
 * @return llvm_gen_be_error_t::ok on success, or a corresponding error code.
 */
extern llvm_gen_be_error_t llvm_gen_be(void) noexcept;

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* BACK_END_IS_LLVM_GEN_BE */

#endif /* ifndef LLVM_GEN_BE_H */