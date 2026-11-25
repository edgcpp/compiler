/**
 * @file gcc_gen_be_coro.h
 * @brief C++20 coroutine heap frame layout and suspension lowering helpers for the GCC backend.
 * @details Computes exact aligned byte sizes and alignments of coroutine frames across promise,
 * header pointers, copied parameters, local variables, and spilled temporaries so heap allocation
 * via `operator new` never under-allocates large coroutine frames.
 * @ingroup gcc_gen_be
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_CORO_H
#define GCC_GEN_BE_CORO_H

#include "gcc_gen_be_error.h"
#include "fe_common.h"
#include "il.h"
#include <stddef.h>

struct gcc_jit_type;

BEGIN_EDG_NAMESPACE

/**
 * @brief Computes the exact byte size and alignment of a C++20 coroutine heap frame.
 * @details Sums aligned EDG `tp->size` contributions for `__resume_fn`, `__destroy_fn`,
 * `__promise`, `__suspend_index`, `initial_await_resume_called`, `__coro_flags`, all
 * `descr->parameter_copies`, and all spilled local variables and `enk_temp_init` temporaries
 * across `func_scope` and its sub-scopes, taking the maximum against `gcc_jit_type_get_size(frame_type)`.
 * @param[in] descr Optional EDG coroutine descriptor (`a_coroutine_descr_ptr`).
 * @param[in] func_scope Optional innermost function scope (`a_scope_ptr`) holding coroutine locals.
 * @param[in] frame_type Lowered `gcc_jit_type` of `frame_struct` (may be NULL).
 * @param[out] out_frame_bytes Pointer receiving the exact aligned heap allocation size in bytes.
 * @param[out] out_frame_align Pointer receiving the required frame alignment in bytes.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if output pointers are NULL.
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t gcc_gen_be_compute_coro_frame_layout(
    a_coroutine_descr_ptr descr,
    a_scope_ptr func_scope,
    struct gcc_jit_type *frame_type,
    size_t *out_frame_bytes,
    size_t *out_frame_align) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_CORO_H */
