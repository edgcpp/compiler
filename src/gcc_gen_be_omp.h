/**
 * @file gcc_gen_be_omp.h
 * @brief OpenMP directive parsing, region outlining, and GNU `libgomp` lowering for the GCC backend.
 * @details Declares functions that collect `#pragma omp` directives from EDG scopes and lower
 * OpenMP `parallel`, `for` / `parallel for` (`static`, `dynamic`, `guided`, and `runtime` schedules,
 * 32-bit & 64-bit bounds, `collapse(K)`, `private`, `firstprivate`, `lastprivate`, `reduction`),
 * `barrier`, `critical`, `single`, `master`/`masked`, `task`/`taskwait`, `atomic`, and `simd`
 * constructs to GNU `libgomp` ABI calls.
 * @ingroup gcc_gen_be
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_OMP_H
#define GCC_GEN_BE_OMP_H

#include "gcc_gen_be_error.h"
#include "basic_hdrs.h"
#include "il_def.h"

struct gcc_jit_function;

BEGIN_EDG_NAMESPACE

/**
 * @brief Registers all `#pragma omp` directives found in an EDG scope and its nested scopes.
 * @details Walks `scope->pragmas` and records unrecognized OpenMP pragmas (`#pragma omp ...`) in
 * `gcc_gen_be_singleton_state.omp_pragma_entries` without duplicates, marking `openmp_enabled = TRUE`.
 * @param[in] scope The EDG scope to scan (may be NULL).
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM` on allocation failure.
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t gcc_gen_be_register_omp_pragmas_in_scope(
    a_scope_ptr scope) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Associates an explicit OpenMP directive string with an EDG statement node.
 * @details Used both by scope pragma scanning and by direct unit tests to bind `#pragma omp ...`
 * directive text to `target_stmt` (or `NULL` for trailing standalone directives).
 * @param[in] target_stmt Target EDG statement pointer (may be NULL for unattached trailing directives).
 * @param[in] owner_scope Scope owning an unattached directive (may be NULL).
 * @param[in] pragma_text Null-terminated `#pragma omp ...` directive string.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t gcc_gen_be_bind_omp_pragma(
    a_statement_ptr target_stmt,
    a_scope_ptr owner_scope,
    const char *pragma_text) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Checks whether `stmt` has an unconsumed OpenMP directive and lowers it via GNU `libgomp`.
 * @details Consumes all `#pragma omp` directives bound to `stmt` (including standalone `barrier` /
 * `taskwait` pragmas preceding `stmt`, or structured `parallel`, `for`, `parallel for`, `critical`,
 * `single`, `master`/`masked`, `task`, `atomic`, and `simd` regions) and sets `*out_handled = TRUE`
 * when `stmt` itself was lowered by the OpenMP subsystem.
 * @param[in] stmt Frontend statement node being lowered.
 * @param[in] func Enclosing `gcc_jit_function` receiving the lowered control flow.
 * @param[out] out_handled Pointer receiving `TRUE` if `stmt` was lowered as an OpenMP construct.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code (`GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED` on invalid directive).
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t gcc_gen_be_try_lower_omp_statement(
    a_statement_ptr stmt,
    struct gcc_jit_function *func,
    a_boolean *out_handled) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits any trailing unattached standalone `#pragma omp barrier` / `taskwait` directives in `scope`.
 * @details Called before exiting a compound block so standalone OpenMP synchronization directives
 * placed immediately before `}` are emitted at the end of the block.
 * @param[in] scope Scope being exited (may be NULL).
 * @param[in] func Active `gcc_jit_function`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t gcc_gen_be_flush_trailing_omp_pragmas(
    a_scope_ptr scope,
    struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_OMP_H */
