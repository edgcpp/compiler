/**
 * @file llvm_gen_be_const.h
 * @ingroup llvm_gen_be_types
 * @brief Constant evaluator subsystem for the EDG LLVM backend.
 * @details Translates EDG front-end constant structures (a_constant_ptr) into LLVM IR
 * constants (llvm::Constant*). All functions return llvm_gen_be_error_t and output
 * constructed constants via output pointers.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_CONST_H
#define LLVM_GEN_BE_CONST_H 1

#include "basic_hdrs.h"
#include "fe_common.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Type.h>

BEGIN_EDG_NAMESPACE

/**
 * @brief Dispatches an EDG constant node (`a_constant_ptr`) to the appropriate LLVM constant builder.
 * @details Supports integer, float, string, address, aggregate, and pointer-to-member constants.
 * @param[in] con Pointer to the EDG constant node (or nullptr for zero-initialization).
 * @param[in] expected_ty The expected LLVM type of the resulting constant.
 * @param[out] out_const Receives the evaluated `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t evaluate_constant(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Extracts the low and high 64-bit words from an EDG `an_integer_value`.
 * @details Handles both host-integer (`INTEGER_VALUE_REPR_IS_A_HOST_INTEGER=1`) and
 *          multi-part big-endian (`INTEGER_VALUE_REPR_IS_A_HOST_INTEGER=0`) representations,
 *          including sign-extension into the upper 64 bits when the underlying representation
 *          is 64 bits wide (`INT_VALUE_PARTS_PER_INTEGER_VALUE == 4` or `sizeof(an_integer_value) == 8`).
 * @param[in] val The EDG `an_integer_value` to extract.
 * @param[in] is_signed True if the integer type is signed (for 64-to-128 sign extension when needed).
 * @param[out] out_low Receives the least-significant 64 bits.
 * @param[out] out_high Receives the most-significant 64 bits.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_low` or `out_high` is null.
 */
[[nodiscard]] llvm_gen_be_error_t llvm_extract_edg_int128(
    const an_integer_value& val,
    bool is_signed,
    uint64_t* out_low,
    uint64_t* out_high) noexcept;

/**
 * @brief Extracts an arbitrary-width `llvm::APInt` from an EDG `an_integer_value`.
 * @details Reads all 16-bit parts of `an_integer_value` (or host integer representation) and
 *          sign- or zero-extends/truncates the resulting value to @p bit_width bits (`_BitInt(N)`).
 * @param[in] val The EDG `an_integer_value` to extract.
 * @param[in] bit_width Target bit width of the resulting `llvm::APInt` (must be >= 1).
 * @param[in] is_signed True if the integer type is signed.
 * @param[out] out_apint Receives the constructed `llvm::APInt`.
 * @return `llvm_gen_be_error_t::ok` on success, or `invalid_argument` if `out_apint` is null or `bit_width == 0`.
 */
llvm_gen_be_error_t llvm_extract_edg_apint(
    const an_integer_value& val,
    unsigned bit_width,
    bool is_signed,
    llvm::APInt* out_apint) noexcept;

/**
 * @brief Encodes an IEEE-754-2008 decimal floating-point value (`_Decimal32`, `_Decimal64`, `_Decimal128`) into a BID or DPD integer constant.
 * @details Packs the sign bit, biased base-10 exponent, and 128-bit integer significand into a 32-bit, 64-bit,
 *          or 128-bit `llvm::ConstantInt` using Binary Integer Decimal (BID) or Densely Packed Decimal (DPD) encoding.
 * @param[in] byte_width Byte width of the decimal float (`4`, `8`, or `16`).
 * @param[in] is_dpd True to use Densely Packed Decimal (DPD) encoding; false for Binary Integer Decimal (BID).
 * @param[in] is_negative True if the sign bit is negative (`-0.0dd`, `-1.5dd`, etc.).
 * @param[in] exponent Unbiased base-10 exponent (`coeff * 10^exponent`).
 * @param[in] coeff_low Least-significant 64 bits of the unsigned decimal integer coefficient.
 * @param[in] coeff_high Most-significant 64 bits of the unsigned decimal integer coefficient.
 * @param[out] out_const Receives the encoded `llvm::ConstantInt*`.
 * @return `llvm_gen_be_error_t::ok` on success, `invalid_argument` if `out_const` is null, or `unsupported_type` if `byte_width` is invalid.
 */
llvm_gen_be_error_t llvm_encode_decimal_float_bid_dpd(
    unsigned byte_width,
    bool is_dpd,
    bool is_negative,
    int32_t exponent,
    uint64_t coeff_low,
    uint64_t coeff_high,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Translates an EDG integer constant (`ck_integer`) into an `llvm::ConstantInt`.
 * @details Supports signed and unsigned integral values up to 128 bits (`APInt`).
 * @param[in] con Pointer to the EDG constant structure.
 * @param[in] expected_ty The target LLVM integer type.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if inputs are null.
 */
llvm_gen_be_error_t llvm_const_from_integer(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Translates an EDG floating-point constant (`ck_float`) into an `llvm::ConstantFP`.
 * @details Handles normal values, positive/negative infinity, and quiet NaNs.
 * @param[in] con Pointer to the EDG constant structure.
 * @param[in] expected_ty The target LLVM floating-point type.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if inputs are null.
 */
llvm_gen_be_error_t llvm_const_from_float(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Translates an EDG complex constant (`ck_complex`) into a two-element `llvm::ConstantStruct`.
 * @details Extracts both real and imaginary components (`an_internal_complex_value`) using `fp_to_string`,
 *          handling normal values, positive/negative infinity, and quiet NaNs independently.
 * @param[in] con Pointer to the EDG complex constant structure.
 * @param[in] expected_ty The target LLVM struct type `{ elem_ty, elem_ty }`.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if inputs are null.
 */
llvm_gen_be_error_t llvm_const_from_complex(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Translates an EDG C99 imaginary constant (`ck_imaginary`) into an `llvm::ConstantFP` or `{0.0, imag}` struct.
 * @details Evaluates `con->variant.float_value` and produces either a scalar floating-point constant
 *          or a two-element `{ 0.0, imag }` `llvm::ConstantStruct` when `expected_ty` is a struct.
 * @param[in] con Pointer to the EDG imaginary constant structure.
 * @param[in] expected_ty The target LLVM floating-point or complex struct type.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if inputs are null.
 */
llvm_gen_be_error_t llvm_const_from_imaginary(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Translates a GNU label-difference constant (`ck_label_difference`, `&&to - &&from`) into an `llvm::Constant*`.
 * @details Evaluates `to_address` and `from_address` as `llvm::BlockAddress*`, converts both via `getPtrToInt`,
 *          and emits `llvm::ConstantExpr::getSub(to_int, from_int)`.
 * @param[in] con Pointer to the EDG label-difference constant structure.
 * @param[in] expected_ty The target LLVM integer type for the offset.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_const_from_label_difference(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Translates a stack-offset constant (`ck_stack_offset`) into an `llvm::Constant*`.
 * @details Evaluates the stack offset constant relative to a local variable for EH table lowering.
 * @param[in] con Pointer to the EDG stack-offset constant structure.
 * @param[in] expected_ty The target LLVM integer or pointer type.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if inputs are null.
 */
llvm_gen_be_error_t llvm_const_from_stack_offset(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Translates an embedded dynamic initialization constant (`ck_dynamic_init`) into an `llvm::Constant*`.
 * @details Folds `dik_constant` or constant `dik_expression` entries to `llvm::Constant*`, and produces
 *          a zero-initialized constant placeholder for runtime-initialized subobjects.
 * @param[in] con Pointer to the EDG dynamic-init constant structure.
 * @param[in] expected_ty The target LLVM type of the subobject.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_const_from_dynamic_init(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Translates a C++ template-parameter constant (`ck_template_param`) into an `llvm::Constant*`.
 * @details Resolves folded `tpck_dependent_constant`, `tpck_address`, `tpck_expression`, `tpck_sizeof`,
 *          `tpck_datasizeof`, and `tpck_alignof` nodes, or returns `constant_eval_failure` for
 *          uninstantiated dependent template parameters.
 * @param[in] con Pointer to the EDG template-parameter constant structure.
 * @param[in] expected_ty The target LLVM type of the constant.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_const_from_template_param(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Translates an EDG string constant (`ck_string`) into an `llvm::ConstantDataArray` or global string pointer.
 * @details Supports narrow (`i8`), UTF-16 (`i16`), and UTF-32/wide (`i32`) character arrays with deduplication.
 * @param[in] con Pointer to the EDG constant structure.
 * @param[in] expected_ty The target LLVM array or pointer type.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_const_from_string(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Translates an EDG aggregate constant (`ck_aggregate`) into an `llvm::ConstantArray` or `llvm::ConstantStruct`.
 * @details Handles arrays, structs, and unions (including active member conversion and zero-padding).
 * @param[in] con Pointer to the EDG aggregate constant structure.
 * @param[in] expected_ty The target LLVM array or struct type.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_const_from_aggregate(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Translates an EDG address constant (`ck_address`) into an LLVM global, function, or `BlockAddress` constant.
 * @details Applies byte offsets via constant `getelementptr` when `con->variant.address.offset != 0`.
 * @param[in] con Pointer to the EDG address constant structure.
 * @param[in] expected_ty The target LLVM pointer type.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_const_from_address(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Packs a consecutive run of constant bitfield initializers into a target-endian integer constant.
 * @details Handles both little-endian (`LSB`-first) and big-endian (`MSB`-first) bitfield placement
 *          within @p unit_ty and advances `*io_field` and `*io_con` past the packed bitfields.
 * @param[in,out] io_field Pointer to the current `a_field_ptr` bitfield cursor.
 * @param[in,out] io_con Pointer to the current `a_constant_ptr` initializer cursor.
 * @param[in] unit_ty Target LLVM integer type representing the packed bitfield storage unit.
 * @param[out] out_packed_const Receives the packed `llvm::Constant*` integer for the storage unit.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if required pointers are null.
 */
llvm_gen_be_error_t llvm_pack_constant_bitfield_bytes(
    a_field_ptr* io_field,
    a_constant_ptr* io_con,
    llvm::Type* unit_ty,
    llvm::Constant** out_packed_const) noexcept;

END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_CONST_H */
