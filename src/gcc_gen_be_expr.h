/**
 * @file gcc_gen_be_expr.h
 * @brief Expression lowering for the GCC backend.
 *
 * This file declares functions for lowering EDG AST expression nodes
 * into libgccjit lvalues and rvalues.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_EXPR_H
#define GCC_GEN_BE_EXPR_H

#include "gcc_gen_be_error.h"
#include "fe_common.h"
#include "expr.h"

struct gcc_jit_lvalue;
struct gcc_jit_rvalue;
struct gcc_jit_type;

BEGIN_EDG_NAMESPACE

/**
 * @brief Lowers an EDG expression into a libgccjit lvalue.
 *
 * @param expr The frontend expression node.
 * @param out_lval A pointer to a gcc_jit_lvalue pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_lval` populated.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_expr_lvalue(an_expr_node_ptr expr, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers an EDG expression into a libgccjit rvalue.
 *
 * @param expr The frontend expression node.
 * @param out_rval A pointer to a gcc_jit_rvalue pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_rval` populated.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_expr_rvalue(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers an EDG constant into a libgccjit rvalue.
 *
 * @param con The frontend constant.
 * @param expected_type The expected libgccjit type.
 * @param out_rval A pointer to a gcc_jit_rvalue pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_rval` populated.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_constant_rvalue(a_constant_ptr con, struct gcc_jit_type *expected_type, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers an EDG dynamic initialization (`a_dynamic_init_ptr`) into a target lvalue and/or rvalue.
 *
 * @details Supports `dik_zero`, `dik_constant`, `dik_expression`, `dik_class_result_via_ctor`,
 * `dik_constructor`, `dik_nonconstant_aggregate`, `dik_bitwise_copy`, and `dik_lambda`,
 * including array element construction loops (`ck_init_repeat` / `ck_dynamic_init`) and
 * destructor cleanup registration.
 *
 * @param[in] dip The frontend dynamic initialization descriptor.
 * @param[in] target_lval The destination lvalue to initialize (may be NULL if only computing rvalue).
 * @param[out] out_rval Pointer to receive the resulting rvalue (may be NULL if not needed).
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_dynamic_init(a_dynamic_init_ptr dip, struct gcc_jit_lvalue *target_lval, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a C++20 `co_await` or `co_yield` expression (`eok_await`/`eok_yield` or `enk_await`/`enk_yield`).
 * @details Evaluates the operand and `await_ready()`, registers the suspension point via
 * `gcc_gen_be_register_coro_suspend_point()`, lowers `await_suspend()` (handling `void`, `bool`, and
 * symmetric transfer `std::coroutine_handle<T>`), and evaluates `await_resume()` in the resume block.
 * @param[in] expr The frontend `co_await` or `co_yield` expression node.
 * @param[out] out_rval Pointer to receive the resulting `await_resume()` rvalue (or NULL if `void`).
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_await_yield(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a C++20 `co_await` expression (`enk_await` / `eok_await`).
 * @details Delegates to `gcc_gen_be_lower_await_yield()` to evaluate `await_ready()`,
 * register the suspension point, invoke `await_suspend()`, and emit `await_resume()`.
 * @param[in] expr The frontend `co_await` expression node.
 * @param[out] out_rval Pointer to receive the resulting `await_resume()` rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_co_await(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a C++20 `co_yield` expression (`enk_yield` / `eok_yield`).
 * @details Delegates to `gcc_gen_be_lower_await_yield()` to evaluate `promise.yield_value(expr)`,
 * register the suspension point, invoke `await_suspend()`, and emit `await_resume()`.
 * @param[in] expr The frontend `co_yield` expression node.
 * @param[out] out_rval Pointer to receive the resulting `await_resume()` rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_co_yield(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a SIMD vector shuffle or permutation (`bok_builtin_shuffle` or `bok_builtin_shufflevector`).
 * @details Uses `p_gcc_jit_context_new_rvalue_vector_perm` when available in `libgccjit`, or falls back
 * to lane-by-lane extraction and insertion via temporary vector local storage (`mask[i] % (2 * N)`).
 * @param[in] expr The frontend `enk_builtin_operation` expression node.
 * @param[out] out_rval Pointer to receive the permuted vector `gcc_jit_rvalue`.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_vector_shuffle(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a SIMD vector element type conversion (`bok_builtin_convertvector`).
 * @details Uses `p_gcc_jit_context_new_convert_vector` when available in `libgccjit`, or lowers lane-by-lane
 * by extracting each element from the source vector, casting it to the destination element type, and packing
 * the converted lanes into the destination vector type.
 * @param[in] expr The frontend `enk_builtin_operation` expression node.
 * @param[out] out_rval Pointer to receive the converted vector `gcc_jit_rvalue`.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_convertvector(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a floating-point constant (`ck_float`) with full precision for extended float kinds.
 * @details Preserves significand and exponent precision for `fk_long_double`, `fk_float80`,
 * `fk_float128`, `fk_std_float128`, `fk_float16`, `fk_fp16`, and `fk_std_bfloat16` constants.
 * @param[in] con The frontend floating-point constant (`ck_float`).
 * @param[in] expected_type The target `gcc_jit_type` (or NULL to infer from `con->type`).
 * @param[out] out_rval Pointer to receive the lowered floating-point `gcc_jit_rvalue`.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_float_constant(a_constant_ptr con, struct gcc_jit_type *expected_type, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers an IEEE 754 decimal floating-point (`_Decimal32`, `_Decimal64`, `_Decimal128`) binary operation.
 * @details Emits a call to the corresponding `libgcc` BID runtime helper (`__bid_addsd3`, `__bid_subsd3`,
 * `__bid_mulsd3`, `__bid_divsd3`, `__bid_adddd3`, `__bid_addtd3`, etc.) or returns
 * `GCC_GEN_BE_ERROR_DECIMAL_FLOAT_UNSUPPORTED` on unsupported byte widths.
 * @param[in] op_code Binary operator character (`'+'`, `'-'`, `'*'`, `'/'`).
 * @param[in] byte_width Byte width of the decimal float operands (`4`, `8`, or `16`).
 * @param[in] lhs Left-hand side BID/DPD rvalue.
 * @param[in] rhs Right-hand side BID/DPD rvalue.
 * @param[out] out_rval Pointer to receive the resulting decimal float rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_DECIMAL_FLOAT_UNSUPPORTED` / `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_decimal_float_op(
    int op_code,
    int byte_width,
    struct gcc_jit_rvalue *lhs,
    struct gcc_jit_rvalue *rhs,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers an ISO/IEC TR 18037 fixed-point (`_Fract`, `_Accum`, `_Sat`) binary arithmetic operation.
 * @details Performs scaled fixed-point addition, subtraction, multiplication (`(lhs * rhs) >> frac_bits`),
 * or division (`(lhs << frac_bits) / rhs`), clamping to `[min_val, max_val]` when `is_saturating` is `TRUE`,
 * and returns `GCC_GEN_BE_ERROR_FIXED_POINT_OVERFLOW` on invalid `frac_bits` (`<= 0` or `>= 63`).
 * @param[in] op_code Binary operator character (`'+'`, `'-'`, `'*'`, `'/'`).
 * @param[in] frac_bits Number of fractional bits in the fixed-point representation (`1..62`).
 * @param[in] is_saturating `TRUE` if `_Sat` saturation clamping is enabled, `FALSE` otherwise.
 * @param[in] lhs Left-hand side fixed-point integer rvalue.
 * @param[in] rhs Right-hand side fixed-point integer rvalue.
 * @param[out] out_rval Pointer to receive the resulting fixed-point rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_FIXED_POINT_OVERFLOW` / `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_fixed_point_op(
    int op_code,
    int frac_bits,
    a_boolean is_saturating,
    struct gcc_jit_rvalue *lhs,
    struct gcc_jit_rvalue *rhs,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Packs multiple constant bitfield values sharing a single storage container word.
 * @details Masks each `bit_values[i]` to `bit_widths[i]` bits, shifts it left by `bit_offsets[i]` bits,
 * and bitwise-ORs all contributions into `*out_packed_word`.
 * @param[in] bit_values Array of raw unsigned integer values for each bitfield.
 * @param[in] bit_offsets Array of bit shifts relative to the start of the container word.
 * @param[in] bit_widths Array of bit widths (`1..64`) for each bitfield.
 * @param[in] count Number of bitfields to pack (`>= 0`).
 * @param[out] out_packed_word Pointer to receive the combined container word value.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
extern gcc_gen_be_error_t gcc_gen_be_pack_bitfield_constants(
    const unsigned long *bit_values,
    const unsigned int *bit_offsets,
    const unsigned int *bit_widths,
    int count,
    unsigned long *out_packed_word) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_EXPR_H */