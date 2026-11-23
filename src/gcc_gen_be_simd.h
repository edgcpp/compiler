/**
 * @file gcc_gen_be_simd.h
 * @brief Target SIMD vector construction, lane manipulation, and x86/ARM intrinsic lowering.
 * @details Declares helpers for lowering SIMD vector splat/constructors via
 * `gcc_jit_context_new_rvalue_from_vector`, lane extract/insert, arithmetic/bitwise/min/max/FMA
 * vector intrinsics, horizontal reductions, and unaligned vector loads/stores across x86 SSE/AVX
 * and ARM64 NEON builtin families.
 * @ingroup gcc_gen_be
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_SIMD_H
#define GCC_GEN_BE_SIMD_H

#include "gcc_gen_be_error.h"
#include "basic_hdrs.h"
#include "il_def.h"

struct gcc_jit_type;
struct gcc_jit_rvalue;

BEGIN_EDG_NAMESPACE

/**
 * @brief Constructs a vector rvalue from lane rvalues using `gcc_jit_context_new_rvalue_from_vector` or memory fallback.
 * @details Uses `p_gcc_jit_context_new_rvalue_from_vector` when available in `libgccjit` and falls back
 * to stack-slot lane stores when running on an older `libgccjit` without vector constructor support.
 * @param[in] vec_type Lowered `gcc_jit_type` of the target vector.
 * @param[in] lane_type Lowered `gcc_jit_type` of an individual scalar lane.
 * @param[in] num_lanes Number of scalar elements in `elements` (must be `>= 1`).
 * @param[in] elements Array of `num_lanes` scalar lane rvalues.
 * @param[out] out_vec Pointer receiving the constructed vector `gcc_jit_rvalue`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t gcc_gen_be_build_vector_rvalue(
    struct gcc_jit_type *vec_type,
    struct gcc_jit_type *lane_type,
    size_t num_lanes,
    struct gcc_jit_rvalue **elements,
    struct gcc_jit_rvalue **out_vec) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Broadcasts a scalar `lane_val` across all `num_lanes` lanes of `vec_type`.
 * @details Allocates a temporary lane array and delegates to `gcc_gen_be_build_vector_rvalue`.
 * @param[in] vec_type Lowered `gcc_jit_type` of the target vector.
 * @param[in] lane_type Lowered `gcc_jit_type` of each scalar lane.
 * @param[in] num_lanes Number of lanes in `vec_type` (must be `>= 1`).
 * @param[in] lane_val Scalar rvalue to replicate into every lane.
 * @param[out] out_vec Pointer receiving the broadcasted vector `gcc_jit_rvalue`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t gcc_gen_be_splat_vector_rvalue(
    struct gcc_jit_type *vec_type,
    struct gcc_jit_type *lane_type,
    size_t num_lanes,
    struct gcc_jit_rvalue *lane_val,
    struct gcc_jit_rvalue **out_vec) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers common x86 SSE/AVX (`__builtin_ia32_*`) and ARM NEON (`__builtin_neon_*`) vector builtins.
 * @details Recognizes target-independent vector splat, lane extract/insert, arithmetic (`add`, `sub`,
 * `mul`, `div`), bitwise (`and`, `or`, `xor`, `andnot`), `min`/`max`, `fma`, horizontal `addv`, and
 * unaligned load/store builtins, setting `*out_handled = TRUE` when lowered or delegating unknown
 * target-specific intrinsics cleanly when `*out_handled == FALSE`.
 * @param[in] name Builtin function name.
 * @param[in] rout Frontend routine descriptor (may be NULL in unit tests).
 * @param[in] num_args Number of argument rvalues in `args`.
 * @param[in] args Array of lowered argument rvalues.
 * @param[out] out_handled Pointer receiving `TRUE` if lowered by this SIMD helper, else `FALSE`.
 * @param[out] out_rval Pointer receiving the lowered result rvalue (`NULL` for `void` stores).
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t gcc_gen_be_try_lower_simd_builtin(
    const char *name,
    a_routine_ptr rout,
    int num_args,
    struct gcc_jit_rvalue **args,
    a_boolean *out_handled,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_SIMD_H */
