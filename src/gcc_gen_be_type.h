/**
 * @file gcc_gen_be_type.h
 * @brief Type lowering for the GCC backend.
 *
 * This file declares the function responsible for lowering an EDG frontend
 * type (`a_type_ptr`) into a libgccjit type (`gcc_jit_type`).
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_TYPE_H
#define GCC_GEN_BE_TYPE_H

#include "gcc_gen_be_error.h"
#include "fe_common.h"
#include "types.h"

struct gcc_jit_type;
struct gcc_jit_rvalue;

BEGIN_EDG_NAMESPACE

/**
 * @brief Lowers an EDG frontend type into a libgccjit type.
 *
 * @param tp The frontend type to lower.
 * @param out_type A pointer to a gcc_jit_type pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_type` populated.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_type(a_type_ptr tp, struct gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Creates a zero-initialized rvalue for the specified type.
 *
 * @details Creates a null pointer for pointers, a zero-initialized constructor
 * for aggregates, or a scalar zero value for numeric types.
 *
 * @param[in] tp The frontend type.
 * @param[in] jit_type The lowered libgccjit type.
 * @param[out] out_rval Pointer to receive the zero rvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_zero_rvalue(a_type_ptr tp, struct gcc_jit_type *jit_type, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers an IEEE 754 decimal floating-point storage type (`_Decimal32`, `_Decimal64`, `_Decimal128`).
 * @details Maps 32-bit decimal floats to `uint32_t`, 64-bit decimal floats to `uint64_t`, and 128-bit
 * decimal floats to 128-bit unsigned integers (BID/DPD representation), or returns
 * `GCC_GEN_BE_ERROR_DECIMAL_FLOAT_UNSUPPORTED` for invalid byte widths.
 * @param[in] byte_width Byte width of the decimal floating-point type (`4`, `8`, or `16`).
 * @param[out] out_type Pointer to receive the lowered `gcc_jit_type`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_DECIMAL_FLOAT_UNSUPPORTED` / `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_decimal_float_type(
    int byte_width,
    struct gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a binary arithmetic operation (`+`, `-`, `*`, `/`) on IEEE-754 decimal floating-point operands.
 * @details Emits a call to the corresponding `libgcc` BID runtime helper (`__bid_addsd3`, `__bid_subdd3`,
 * `__bid_multd3`, `__bid_divsd3`, etc.) for `_Decimal32` (4 bytes), `_Decimal64` (8 bytes), or `_Decimal128` (16 bytes).
 * @param[in] byte_width Decimal float storage width in bytes (`4`, `8`, or `16`).
 * @param[in] op_char Arithmetic operation character (`'+'`, `'-'`, `'*'`, or `'/'`).
 * @param[in] lhs Left-hand operand rvalue.
 * @param[in] rhs Right-hand operand rvalue.
 * @param[out] out_rval Pointer to receive the resulting decimal float rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_decimal_float_binary_op(
    int byte_width,
    char op_char,
    struct gcc_jit_rvalue *lhs,
    struct gcc_jit_rvalue *rhs,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a relational comparison (`==`, `!=`, `<`, `<=`, `>`, `>=`) on IEEE-754 decimal floating-point operands.
 * @details Emits a call to the corresponding `libgcc` BID comparison helper (`__bid_eqsd2`, `__bid_nesd2`,
 * `__bid_ltsd2`, `__bid_lesd2`, `__bid_gtsd2`, `__bid_gesd2`, etc.) and compares the result against 0.
 * @param[in] byte_width Decimal float storage width in bytes (`4`, `8`, or `16`).
 * @param[in] cmp_code Two-character comparison code (`"eq"`, `"ne"`, `"lt"`, `"le"`, `"gt"`, or `"ge"`).
 * @param[in] lhs Left-hand operand rvalue.
 * @param[in] rhs Right-hand operand rvalue.
 * @param[out] out_rval Pointer to receive the boolean comparison rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_decimal_float_comparison(
    int byte_width,
    const char *cmp_code,
    struct gcc_jit_rvalue *lhs,
    struct gcc_jit_rvalue *rhs,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a conversion between IEEE-754 decimal floating-point widths (`_Decimal32`, `_Decimal64`, `_Decimal128`).
 * @details Emits a call to the `libgcc` BID conversion helper (`__bid_extendsddd2`, `__bid_truncddsd2`, etc.).
 * @param[in] src_byte_width Source decimal float byte width (`4`, `8`, or `16`).
 * @param[in] dst_byte_width Destination decimal float byte width (`4`, `8`, or `16`).
 * @param[in] src_val Source decimal float rvalue.
 * @param[out] out_rval Pointer to receive the converted decimal float rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_decimal_float_cast(
    int src_byte_width,
    int dst_byte_width,
    struct gcc_jit_rvalue *src_val,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a conversion from an IEEE-754 decimal floating-point value to a binary floating-point type.
 * @details Emits a call to the `libgcc` BID conversion helper (`__bid_truncsdsf`, `__bid_extendsddf`,
 * `__bid_truncdddf`, `__bid_trunctddf`, etc.).
 * @param[in] dec_byte_width Source decimal float width in bytes (`4`, `8`, or `16`).
 * @param[in] bin_byte_width Target binary float width in bytes (`4` for `float`, `8` for `double`).
 * @param[in] src_val Source decimal float rvalue.
 * @param[out] out_rval Pointer to receive the converted binary float rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_decimal_to_binary_float(
    int dec_byte_width,
    int bin_byte_width,
    struct gcc_jit_rvalue *src_val,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a conversion from an IEEE-754 decimal floating-point value to a 32-bit or 64-bit integer.
 * @details Emits a call to `__bid_fixsdsi` / `__bid_fixunssdsi` / `__bid_fixdddi` / `__bid_fixunsdddi`.
 * @param[in] dec_byte_width Source decimal float width in bytes (`4`, `8`, or `16`).
 * @param[in] is_unsigned `TRUE` for unsigned integer conversion, `FALSE` for signed.
 * @param[in] src_val Source decimal float rvalue.
 * @param[out] out_rval Pointer to receive the converted integer rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_decimal_to_int(
    int dec_byte_width,
    a_boolean is_unsigned,
    struct gcc_jit_rvalue *src_val,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a conversion from a 32-bit or 64-bit integer to an IEEE-754 decimal floating-point value.
 * @details Emits a call to `__bid_floatsisd` / `__bid_floatunssisd` / `__bid_floatdidd` / `__bid_floatunsditd`.
 * @param[in] int_byte_width Source integer width in bytes (`4` or `8`).
 * @param[in] is_unsigned `TRUE` for unsigned integer source, `FALSE` for signed.
 * @param[in] dec_byte_width Target decimal float width in bytes (`4`, `8`, or `16`).
 * @param[in] src_val Source integer rvalue.
 * @param[out] out_rval Pointer to receive the converted decimal float rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_int_to_decimal(
    int int_byte_width,
    a_boolean is_unsigned,
    int dec_byte_width,
    struct gcc_jit_rvalue *src_val,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a conversion from a binary floating-point value (`float` or `double`) to an IEEE-754 decimal float.
 * @details Emits a call to `__bid_extendsfdd`, `__bid_truncdfsd`, `__bid_extenddfdd`, `__bid_extenddftd`, etc.
 * @param[in] bin_byte_width Source binary float width in bytes (`4` for `float`, `8` for `double`).
 * @param[in] dec_byte_width Target decimal float width in bytes (`4`, `8`, or `16`).
 * @param[in] src_val Source binary float rvalue.
 * @param[out] out_rval Pointer to receive the converted decimal float rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_binary_float_to_decimal(
    int bin_byte_width,
    int dec_byte_width,
    struct gcc_jit_rvalue *src_val,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a unary negation (`'-'`) or absolute-value (`'a'`) operation on an IEEE-754 decimal float.
 * @details Manipulates the sign bit (`0x80000000U` for `_Decimal32` and `0x8000000000000000ULL` for `_Decimal64`)
 * or invokes the active BID/DPD subtraction helper for `_Decimal128`.
 * @param[in] byte_width Decimal float storage width in bytes (`4`, `8`, or `16`).
 * @param[in] op_char Operation character (`'-'` for negation, `'a'` for `fabs`).
 * @param[in] src_val Source decimal float rvalue.
 * @param[out] out_rval Pointer to receive the resulting decimal float rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_decimal_float_unary_op(
    int byte_width,
    char op_char,
    struct gcc_jit_rvalue *src_val,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers an ISO/IEC TR 18037 fixed-point binary operation (`+`, `-`, `*`, `/`) with fractional scaling and optional saturation.
 * @details Scales `*` by `>> frac_bits` and `/` by `<< frac_bits` in a wider intermediate type, and clamps to
 * `[MIN, MAX]` when `is_saturating == TRUE`.
 * @param[in] byte_width Fixed-point storage width in bytes (`1`, `2`, `4`, or `8`).
 * @param[in] frac_bits Number of fractional bits (`1 .. (byte_width * 8 - 1)`).
 * @param[in] is_unsigned `TRUE` for unsigned fixed-point, `FALSE` for signed.
 * @param[in] is_saturating `TRUE` for `_Sat` saturating fixed-point arithmetic, `FALSE` otherwise.
 * @param[in] op_char Operation character (`'+'`, `'-'`, `'*'`, or `'/'`).
 * @param[in] lhs Left-hand fixed-point rvalue.
 * @param[in] rhs Right-hand fixed-point rvalue.
 * @param[out] out_rval Pointer to receive the resulting fixed-point rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_FIXED_POINT_OVERFLOW` / `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_fixed_point_binary_op(
    int byte_width,
    int frac_bits,
    a_boolean is_unsigned,
    a_boolean is_saturating,
    char op_char,
    struct gcc_jit_rvalue *lhs,
    struct gcc_jit_rvalue *rhs,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_TYPE_H */