/**
 * @file gcc_gen_be_builtin.h
 * @brief Builtin and Intrinsic lowering for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_BUILTIN_H
#define GCC_GEN_BE_BUILTIN_H

#include "gcc_gen_be_error.h"
#include <libgccjit.h>
#include "fe_common.h"

BEGIN_EDG_NAMESPACE

/**
 * @brief Lowers a call to a compiler builtin or intrinsic.
 *
 * @param rout The routine entry of the builtin.
 * @param num_args The number of arguments in the call.
 * @param args Array of JIT rvalues for the arguments.
 * @param out_rval Pointer to receive the generated JIT rvalue for the result.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_builtin_call(a_routine_ptr rout, int num_args, gcc_jit_rvalue **args, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers `__builtin_bit_cast(T, src)` to a bit-preserving reinterpretation.
 * @details Validates that `dst_type` and `src_type` have identical byte sizes and bitcasts `src_val` to the lowered destination type.
 * @param[in] dst_type Target EDG type.
 * @param[in] src_type Source EDG type.
 * @param[in] src_val Lowered source rvalue.
 * @param[out] out_rval Pointer to receive the bitcast rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_TYPE_MISMATCH if sizes differ.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_builtin_bit_cast(a_type_ptr dst_type, a_type_ptr src_type, gcc_jit_rvalue *src_val, gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Validates that a SIMD intrinsic immediate operand is a compile-time integer constant in range.
 * @details Inspects `imm_expr` to confirm it is an `enk_constant` of `ck_integer` kind within `[min_val, max_val]`.
 * Reports `GCC_GEN_BE_ERROR_SIMD_LOWERING_FAILED` if `imm_expr` is not a constant immediate.
 * @param[in] intrinsic_name Name of the SIMD intrinsic requiring an immediate.
 * @param[in] imm_expr The EDG expression node for the immediate operand.
 * @param[in] min_val Minimum allowed integer immediate value.
 * @param[in] max_val Maximum allowed integer immediate value.
 * @param[out] out_is_valid Pointer to receive `TRUE` if valid, `FALSE` otherwise.
 * @return `GCC_GEN_BE_SUCCESS` on valid immediate, `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` on NULL pointers,
 * or `GCC_GEN_BE_ERROR_SIMD_LOWERING_FAILED` if `imm_expr` is non-constant or out of range.
 */
extern gcc_gen_be_error_t gcc_gen_be_validate_simd_immediate(
    const char *intrinsic_name,
    an_expr_node_ptr imm_expr,
    int min_val,
    int max_val,
    a_boolean *out_is_valid) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Evaluates `__builtin_object_size` / `__builtin_dynamic_object_size` bounds.
 * @details Returns the static byte size of the pointed-to object when `ptr_type` points to a complete
 * fixed-size aggregate/array, or the standard fallback (`(size_t)-1` when `(bos_mode & 2) == 0`,
 * `0` when `(bos_mode & 2) != 0`).
 * @param[in] ptr_type Optional pointer `a_type_ptr` for the first argument.
 * @param[in] bos_mode The `type` mode argument (`0..3`) of `__builtin_object_size`.
 * @param[out] out_bound Pointer to receive the computed byte size bound.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if `out_bound` is NULL.
 */
extern gcc_gen_be_error_t gcc_gen_be_eval_builtin_object_size(
    a_type_ptr ptr_type,
    int bos_mode,
    size_t *out_bound) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers `__builtin_clear_padding(ptr)` by zeroing padding bytes in a struct/class object.
 * @details Walks the fields of the pointed-to `tk_struct` / `tk_class` type, identifies inter-field
 * and tail padding byte ranges, and emits zero-byte stores in the current basic block.
 * @param[in] ptr_type The EDG pointer type of the argument passed to `__builtin_clear_padding`.
 * @param[in] ptr_val The lowered pointer `gcc_jit_rvalue`.
 * @param[out] out_rval Pointer to receive the resulting `void` / pointer rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_builtin_clear_padding(
    a_type_ptr ptr_type,
    gcc_jit_rvalue *ptr_val,
    gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_BUILTIN_H */
