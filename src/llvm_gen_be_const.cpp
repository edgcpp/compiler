/**
 * @file llvm_gen_be_const.cpp
 * @ingroup llvm_gen_be_types
 * @brief Constant evaluator subsystem for the EDG LLVM backend.
 * @details Translates EDG front-end constant structures (a_constant_ptr) into LLVM IR
 * constants (llvm::Constant*).
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_const.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include <vector>

#include "basic_hdrs.h"
#include "fe_common.h"

#include "float_pt.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_ptrmem.h"
#include "target.h"

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

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
llvm_gen_be_error_t llvm_extract_edg_int128(
    const an_integer_value& val,
    bool is_signed,
    uint64_t* out_low,
    uint64_t* out_high) noexcept {
  if (!out_low || !out_high) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  uint64_t low = EDG_INT_VAL_LOW64(val);
  uint64_t high = EDG_INT_VAL_HIGH64(val);
#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
#if !INT128_EXTENSIONS_ALLOWED
  if (is_signed && (low & (1ULL << 63)) != 0ULL) {
    high = ~0ULL;
  }
#else
  (void)is_signed;
#endif
#else
#if !INT128_EXTENSIONS_ALLOWED && !FIXED_POINT_ALLOWED
  if (!is_signed) {
    high = 0ULL;
  }
#else
  (void)is_signed;
#endif
#endif
  *out_low = low;
  *out_high = high;
  return llvm_gen_be_error_t::ok;
}

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
    llvm::APInt* out_apint) noexcept {
  if (!out_apint || bit_width == 0) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
#if INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
  uint64_t low = 0;
  uint64_t high = 0;
  LLVM_GEN_BE_CHECK(llvm_extract_edg_int128(val, is_signed, &low, &high));
  llvm::APInt repr(128, {low, high});
  *out_apint = is_signed ? repr.sextOrTrunc(bit_width) : repr.zextOrTrunc(bit_width);
#else
  constexpr unsigned kNumParts = static_cast<unsigned>(INT_VALUE_PARTS_PER_INTEGER_VALUE);
  constexpr unsigned kReprBits = kNumParts * 16U;
  std::vector<uint64_t> words((kReprBits + 63U) / 64U, 0ULL);
  for (unsigned k = 0; k < kNumParts; ++k) {
    uint64_t part_val = static_cast<uint64_t>(val.part[kNumParts - 1U - k]) & 0xFFFFULL;
    words[k / 4U] |= (part_val << ((k % 4U) * 16U));
  }
  llvm::APInt repr = llvm::APInt(kReprBits, words);
  *out_apint = is_signed ? repr.sextOrTrunc(bit_width) : repr.zextOrTrunc(bit_width);
#endif
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Folds an unlowered constant integer expression tree (`con->expr`) at full `APInt` precision.
 * @details Supports `_BitInt(N > 128)` shifts, bitwise ops, arithmetic, negation, complement, and casts
 *          that exceed EDG's fixed `an_integer_value` part array width.
 * @param[in] expr Pointer to the EDG constant expression node.
 * @param[in] bit_width Target bit width of the folded `llvm::APInt`.
 * @param[out] out_apint Receives the folded `llvm::APInt` when `*out_folded` is true.
 * @param[out] out_folded Receives `true` if @p expr was folded into @p out_apint.
 * @return `llvm_gen_be_error_t::ok` on success.
 */
static llvm_gen_be_error_t try_fold_wide_int_expr(
    an_expr_node_ptr expr,
    unsigned bit_width,
    llvm::APInt* out_apint,
    bool* out_folded) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  *out_folded = false;
  a_type_ptr ety = expr->type ? skip_typerefs(expr->type) : nullptr;
  bool expr_signed = (ety && ety->kind == tk_integer)
                         ? (int_kind_is_signed[ety->variant.integer.int_kind] != FALSE)
                         : false;
  if (expr->kind == enk_constant && expr->variant.constant.ptr != nullptr &&
      expr->variant.constant.ptr->kind == ck_integer) {
    a_constant_ptr c = expr->variant.constant.ptr;
    if (c->expr != nullptr && c->expr != expr) {
      LLVM_GEN_BE_CHECK(try_fold_wide_int_expr(c->expr, bit_width, out_apint, out_folded));
      if (*out_folded) {
        return llvm_gen_be_error_t::ok;
      }
    }
    LLVM_GEN_BE_CHECK(llvm_extract_edg_apint(
        c->variant.integer_value, bit_width, expr_signed, out_apint));
    *out_folded = true;
    return llvm_gen_be_error_t::ok;
  }
  if (expr->kind != enk_operation) {
    return llvm_gen_be_error_t::ok;
  }
  an_expr_node_ptr op1 = expr->variant.operation.operands;
  if (!op1) {
    return llvm_gen_be_error_t::ok;
  }
  an_expr_operator_kind kind = expr->variant.operation.kind;
  if (kind == eok_cast || kind == eok_negate || kind == eok_complement) {
    unsigned sub_bw = bit_width;
    a_type_ptr sub_ty = op1->type ? skip_typerefs(op1->type) : nullptr;
    if (sub_ty && sub_ty->kind == tk_integer) {
      if ((sub_ty->variant.integer.int_kind == ik_bit_precise ||
           sub_ty->variant.integer.int_kind == ik_unsigned_bit_precise) &&
          sub_ty->variant.integer.extra_info != nullptr &&
          sub_ty->variant.integer.extra_info->bit_width > 0) {
        sub_bw = static_cast<unsigned>(sub_ty->variant.integer.extra_info->bit_width);
      } else if (sub_ty->size > 0) {
        sub_bw = static_cast<unsigned>(sub_ty->size * targ_char_bit);
      }
    }
    llvm::APInt v1 = llvm::APInt(sub_bw, 0);
    bool f1 = false;
    LLVM_GEN_BE_CHECK(try_fold_wide_int_expr(op1, sub_bw, &v1, &f1));
    if (!f1) {
      return llvm_gen_be_error_t::ok;
    }
    bool sub_signed = (sub_ty && sub_ty->kind == tk_integer)
                          ? (int_kind_is_signed[sub_ty->variant.integer.int_kind] != FALSE)
                          : false;
    llvm::APInt resized = sub_signed ? v1.sextOrTrunc(bit_width) : v1.zextOrTrunc(bit_width);
    if (kind == eok_negate) {
      *out_apint = -resized;
    } else if (kind == eok_complement) {
      *out_apint = ~resized;
    } else {
      *out_apint = resized;
    }
    *out_folded = true;
    return llvm_gen_be_error_t::ok;
  }

  an_expr_node_ptr op2 = op1->next;
  if (!op2) {
    return llvm_gen_be_error_t::ok;
  }
  llvm::APInt lhs = llvm::APInt(bit_width, 0);
  llvm::APInt rhs = llvm::APInt(bit_width, 0);
  bool f1 = false;
  bool f2 = false;
  LLVM_GEN_BE_CHECK(try_fold_wide_int_expr(op1, bit_width, &lhs, &f1));
  LLVM_GEN_BE_CHECK(try_fold_wide_int_expr(op2, bit_width, &rhs, &f2));
  if (!f1 || !f2) {
    return llvm_gen_be_error_t::ok;
  }
  switch (kind) {
    case eok_add: *out_apint = lhs + rhs; *out_folded = true; break;
    case eok_subtract: *out_apint = lhs - rhs; *out_folded = true; break;
    case eok_multiply: *out_apint = lhs * rhs; *out_folded = true; break;
    case eok_divide:
      if (!rhs.isZero()) {
        *out_apint = expr_signed ? lhs.sdiv(rhs) : lhs.udiv(rhs);
        *out_folded = true;
      }
      break;
    case eok_remainder:
      if (!rhs.isZero()) {
        *out_apint = expr_signed ? lhs.srem(rhs) : lhs.urem(rhs);
        *out_folded = true;
      }
      break;
    case eok_shiftl:
      *out_apint = lhs.shl(static_cast<unsigned>(rhs.getLimitedValue(bit_width)));
      *out_folded = true;
      break;
    case eok_shiftr: {
      unsigned sh = static_cast<unsigned>(rhs.getLimitedValue(bit_width));
      *out_apint = expr_signed ? lhs.ashr(sh) : lhs.lshr(sh);
      *out_folded = true;
    } break;
    case eok_and: *out_apint = lhs & rhs; *out_folded = true; break;
    case eok_or: *out_apint = lhs | rhs; *out_folded = true; break;
    case eok_xor: *out_apint = lhs ^ rhs; *out_folded = true; break;
    default: break;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Translates an EDG integer constant (`ck_integer`) into an `llvm::ConstantInt` or null pointer.
 * @details Supports `std::nullptr_t` (`tk_nullptr`), boolean canonicalization (`0`/`1`), and arbitrary-width
 *          `_BitInt(N)` (`APInt`) values.
 * @param[in] con Pointer to the EDG constant structure.
 * @param[in] expected_ty The target LLVM integer or pointer type.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if inputs are null.
 */
llvm_gen_be_error_t llvm_const_from_integer(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!con || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  a_type_ptr ctype = con->type ? skip_typerefs(con->type) : nullptr;
  if (ctype && ctype->kind == tk_nullptr) {
    if (!expected_ty || expected_ty->isPointerTy()) {
      llvm::PointerType* pty = expected_ty
                                   ? llvm::cast<llvm::PointerType>(expected_ty)
                                   : llvm::PointerType::getUnqual(*be_state->context);
      *out_const = llvm::ConstantPointerNull::get(pty);
    } else {
      *out_const = llvm::Constant::getNullValue(expected_ty);
    }
    return llvm_gen_be_error_t::ok;
  }

  if (!expected_ty) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  bool is_unsigned = (ctype && ctype->kind == tk_integer)
                         ? !int_kind_is_signed[ctype->variant.integer.int_kind]
                         : false;
  uint64_t low = 0;
  uint64_t high = 0;
  LLVM_GEN_BE_CHECK(llvm_extract_edg_int128(con->variant.integer_value, !is_unsigned, &low, &high));

  if (expected_ty->isPointerTy()) {
    if ((low | high) == 0ULL) {
      *out_const = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(expected_ty));
    } else {
      llvm::Constant* int_c = llvm::ConstantInt::get(
          llvm::Type::getInt64Ty(*be_state->context), low);
      *out_const = llvm::ConstantExpr::getIntToPtr(int_c, expected_ty);
    }
    return llvm_gen_be_error_t::ok;
  }

  if (!expected_ty->isIntegerTy()) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  unsigned bit_width = expected_ty->getIntegerBitWidth();
  llvm::APInt api = llvm::APInt(bit_width, 0);
  bool folded_wide = false;
  if (bit_width > 128 && con->expr != nullptr) {
    LLVM_GEN_BE_CHECK(try_fold_wide_int_expr(con->expr, bit_width, &api, &folded_wide));
  }
  if (!folded_wide) {
    LLVM_GEN_BE_CHECK(llvm_extract_edg_apint(
        con->variant.integer_value, bit_width, !is_unsigned, &api));
  }

  if (ctype && is_bool_type(ctype)) {
    *out_const = llvm::ConstantInt::get(expected_ty, api.isZero() ? 0ULL : 1ULL);
    return llvm_gen_be_error_t::ok;
  }

  *out_const = llvm::ConstantInt::get(expected_ty->getContext(), api);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Helper that converts an `an_internal_float_value` into an `llvm::ConstantFP`.
 * @details Handles normal values, positive/negative infinity, and quiet NaNs.
 * @param[in] fk The EDG float kind (`a_float_kind`).
 * @param[in] fval Pointer to the internal floating-point value.
 * @param[in] fp_ty The target LLVM floating-point type.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t build_fp_constant_from_internal_value(
    a_float_kind fk,
    an_internal_float_value* fval,
    llvm::Type* fp_ty,
    llvm::Constant** out_const) noexcept {
  if (!fp_ty || !fp_ty->isFloatingPointTy()) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  a_boolean pos_inf = FALSE, neg_inf = FALSE, nan = FALSE;
  a_number_buffer hex_str = fp_to_string(fk, fval, &pos_inf, &neg_inf, &nan);

  if (pos_inf) {
    *out_const = llvm::ConstantFP::getInfinity(fp_ty, false);
  } else if (neg_inf) {
    *out_const = llvm::ConstantFP::getInfinity(fp_ty, true);
  } else if (nan) {
    *out_const = llvm::ConstantFP::getQNaN(fp_ty);
  } else {
    llvm::StringRef str_ref = llvm::StringRef(hex_str.as_temp_characters());
    *out_const = llvm::ConstantFP::get(fp_ty, str_ref);
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Compresses a 3-digit BCD group (`0..999`) into a 10-bit IEEE-754 Densely Packed Decimal (DPD) declet.
 * @details Implements Chen-Ho / IEEE-754-2008 Table 3.5 declet packing.
 * @param[in] d2 Hundreds digit (`0..9`).
 * @param[in] d1 Tens digit (`0..9`).
 * @param[in] d0 Units digit (`0..9`).
 * @param[out] out_declet Receives the 10-bit DPD declet.
 * @return `llvm_gen_be_error_t::ok` on success.
 */
static llvm_gen_be_error_t pack_dpd_declet(
    unsigned d2,
    unsigned d1,
    unsigned d0,
    uint32_t* out_declet) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  unsigned b2 = (d2 >= 8U) ? 1U : 0U;
  unsigned b1 = (d1 >= 8U) ? 1U : 0U;
  unsigned b0 = (d0 >= 8U) ? 1U : 0U;
  unsigned pat = (b2 << 2U) | (b1 << 1U) | b0;
  uint32_t dec = 0;
  if (pat == 0U) {
    dec = ((d2 & 7U) << 7U) | ((d1 & 7U) << 4U) | (d0 & 7U);
  } else if (pat == 1U) {
    dec = ((d2 & 7U) << 7U) | ((d1 & 7U) << 4U) | 0x08U | (d0 & 1U);
  } else if (pat == 2U) {
    dec = ((d2 & 7U) << 7U) | ((d0 & 6U) << 4U) | ((d1 & 1U) << 4U) | 0x0AU | (d0 & 1U);
  } else if (pat == 4U) {
    dec = ((d0 & 6U) << 7U) | ((d2 & 1U) << 7U) | ((d1 & 7U) << 4U) | 0x0CU | (d0 & 1U);
  } else if (pat == 3U) {
    dec = ((d2 & 7U) << 7U) | 0x4EU | ((d1 & 1U) << 4U) | (d0 & 1U);
  } else if (pat == 5U) {
    dec = ((d1 & 6U) << 7U) | ((d2 & 1U) << 7U) | 0x2EU | ((d1 & 1U) << 4U) | (d0 & 1U);
  } else if (pat == 6U) {
    dec = ((d0 & 6U) << 7U) | ((d2 & 1U) << 7U) | 0x0EU | ((d1 & 1U) << 4U) | (d0 & 1U);
  } else {
    dec = ((d2 & 1U) << 7U) | 0x6EU | ((d1 & 1U) << 4U) | (d0 & 1U);
  }
  *out_declet = dec & 0x3FFU;
  return llvm_gen_be_error_t::ok;
}

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
    llvm::Constant** out_const) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(out_const);
  unsigned bit_width = 0;
  int32_t bias = 0;
  unsigned exp_bits = 0;
  unsigned sig_bits = 0;
  unsigned declets = 0;
  if (byte_width == 4) {
    bit_width = 32; bias = 101; exp_bits = 8; sig_bits = 23; declets = 2;
  } else if (byte_width == 8) {
    bit_width = 64; bias = 398; exp_bits = 10; sig_bits = 53; declets = 5;
  } else if (byte_width == 16) {
    bit_width = 128; bias = 6176; exp_bits = 14; sig_bits = 113; declets = 11;
  } else {
    return llvm_gen_be_error_t::unsupported_type;
  }

  int32_t max_biased_exp = (3 << (exp_bits - 2)) - 1;
  int32_t biased_exp = exponent + bias;
  if (biased_exp < 0) biased_exp = 0;
  if (biased_exp > max_biased_exp) biased_exp = max_biased_exp;
  uint64_t uexp = static_cast<uint64_t>(biased_exp);

  llvm::APInt coeff(128, {coeff_low, coeff_high});
  llvm::APInt encoded = llvm::APInt(bit_width, 0);

  if (!is_dpd) {
    bool large_sig = coeff.uge(llvm::APInt::getOneBitSet(128, sig_bits));
    if (!large_sig) {
      llvm::APInt sig_mask = llvm::APInt::getLowBitsSet(bit_width, sig_bits);
      encoded |= (coeff.zextOrTrunc(bit_width) & sig_mask);
      encoded |= llvm::APInt(bit_width, uexp).shl(sig_bits);
    } else {
      unsigned small_sig_bits = sig_bits - 2U;
      llvm::APInt sig_mask = llvm::APInt::getLowBitsSet(bit_width, small_sig_bits);
      encoded |= (coeff.zextOrTrunc(bit_width) & sig_mask);
      encoded |= llvm::APInt(bit_width, uexp).shl(small_sig_bits);
      encoded |= llvm::APInt(bit_width, 3ULL).shl(bit_width - 3U);
    }
  } else {
    unsigned total_digits = declets * 3U + 1U;
    std::vector<unsigned> digits(total_digits, 0U);
    llvm::APInt rem_c = coeff;
    llvm::APInt ten = llvm::APInt(128, 10ULL);
    for (unsigned i = 0; i < total_digits; ++i) {
      llvm::APInt q = llvm::APInt(128, 0);
      llvm::APInt r = llvm::APInt(128, 0);
      llvm::APInt::udivrem(rem_c, ten, q, r);
      digits[i] = static_cast<unsigned>(r.getZExtValue());
      rem_c = q;
    }
    for (unsigned k = 0; k < declets; ++k) {
      uint32_t dec = 0;
      LLVM_GEN_BE_CHECK(pack_dpd_declet(
          digits[k * 3U + 2U], digits[k * 3U + 1U], digits[k * 3U], &dec));
      encoded |= llvm::APInt(bit_width, dec).shl(k * 10U);
    }
    unsigned lead_d = digits[declets * 3U];
    unsigned exp_top2 = static_cast<unsigned>((uexp >> (exp_bits - 2U)) & 3ULL);
    uint64_t exp_rem = uexp & ((1ULL << (exp_bits - 2U)) - 1ULL);
    unsigned combo5 = 0;
    if (lead_d < 8U) {
      combo5 = (exp_top2 << 3U) | lead_d;
    } else {
      combo5 = 0x18U | (exp_top2 << 1U) | (lead_d & 1U);
    }
    unsigned trail_exp_bits = exp_bits - 2U;
    encoded |= llvm::APInt(bit_width, exp_rem).shl(declets * 10U);
    encoded |= llvm::APInt(bit_width, combo5).shl(declets * 10U + trail_exp_bits);
  }

  if (is_negative) {
    encoded.setBit(bit_width - 1U);
  }
  *out_const = llvm::ConstantInt::get(*be_state->context, encoded);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Parses an internal floating-point value into `(is_neg, exponent10, coeff_low)` for BID/DPD encoding.
 * @details Formats the floating-point magnitude in base-10 scientific notation and invokes `llvm_encode_decimal_float_bid_dpd`.
 * @param[in] fk The EDG float kind (`a_float_kind`).
 * @param[in] fval Pointer to the internal floating-point value.
 * @param[in] byte_width Byte width of the decimal float (`4`, `8`, or `16`).
 * @param[out] out_const Receives the encoded BID/DPD `llvm::ConstantInt*`.
 * @return `llvm_gen_be_error_t::ok` on success.
 */
static llvm_gen_be_error_t build_decimal_fp_constant(
    a_float_kind fk,
    an_internal_float_value* fval,
    unsigned byte_width,
    llvm::Constant** out_const) noexcept {
  llvm::Constant* bin_fp = nullptr;
  LLVM_GEN_BE_CHECK(build_fp_constant_from_internal_value(
      fk, fval, llvm::Type::getDoubleTy(*be_state->context), &bin_fp));
  auto* cfp = llvm::cast<llvm::ConstantFP>(bin_fp);
  double dval = cfp->getValueAPF().convertToDouble();
  bool is_neg = std::signbit(dval);
  if (is_neg) dval = -dval;

  char dec_buf[64] = {0};
  std::snprintf(dec_buf, sizeof(dec_buf), "%.15g", dval);
  uint64_t coeff = 0;
  int32_t exp10 = 0;
  bool seen_dot = false;
  const char* p = dec_buf;
  while (*p != '\0' && *p != 'e') {
    if (*p == '.') {
      seen_dot = true;
    } else if (static_cast<unsigned>(*p - '0') <= 9U) {
      coeff = coeff * static_cast<uint64_t>(10U) + static_cast<uint64_t>(*p - '0');
      if (seen_dot) --exp10;
    }
    ++p;
  }
  if (*p == 'e') {
    ++p;
    int sign_e = (*p == '-') ? -1 : 1;
    ++p;
    int32_t parsed_e = 0;
    while (static_cast<unsigned>(*p - '0') <= 9U) {
      parsed_e = parsed_e * 10 + (*p - '0');
      ++p;
    }
    exp10 += sign_e * parsed_e;
  }
  llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
  bool is_dpd = triple.isPPC() || triple.isSystemZ();
  return llvm_encode_decimal_float_bid_dpd(
      byte_width, is_dpd, is_neg, exp10, coeff, 0ULL, out_const);
}

/**
 * @brief Translates an EDG floating-point constant (`ck_float`) into an `llvm::ConstantFP` or BID/DPD integer.
 * @details Handles normal values, positive/negative infinity, quiet NaNs, and `_Decimal32`/`_Decimal64`/`_Decimal128` constants.
 * @param[in] con Pointer to the EDG constant structure.
 * @param[in] expected_ty The target LLVM floating-point or decimal carrier type.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if inputs are null.
 */
llvm_gen_be_error_t llvm_const_from_float(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!con || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  unsigned dec_bytes = 0;
  LLVM_GEN_BE_CHECK(llvm_get_decimal_float_byte_width(con->type, &dec_bytes));
  a_float_kind fk = skip_typerefs(con->type)->variant.float_kind;
  if (dec_bytes != 0) {
    return build_decimal_fp_constant(fk, &con->variant.float_value, dec_bytes, out_const);
  }
  return build_fp_constant_from_internal_value(
      fk, &con->variant.float_value, expected_ty, out_const);
}

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
    llvm::Constant** out_const) noexcept {
  if (!con || !expected_ty || !out_const || !con->variant.complex_value) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  llvm::StructType* st_ty = nullptr;
  llvm::Type* elem_ty = nullptr;
  if (expected_ty->isStructTy() && expected_ty->getStructNumElements() == 2) {
    st_ty = llvm::cast<llvm::StructType>(expected_ty);
    elem_ty = st_ty->getElementType(0);
  } else {
    LLVM_GEN_BE_CHECK(llvm_type_from_float(skip_typerefs(con->type), &elem_ty));
    st_ty = llvm::StructType::get(*be_state->context, {elem_ty, elem_ty});
  }

  a_float_kind fk = skip_typerefs(con->type)->variant.float_kind;
  llvm::Constant* real_c = nullptr;
  llvm::Constant* imag_c = nullptr;
  llvm_gen_be_error_t err = build_fp_constant_from_internal_value(
      fk, &con->variant.complex_value->real, elem_ty, &real_c);
  err = build_fp_constant_from_internal_value(
      fk, &con->variant.complex_value->imag, elem_ty, &imag_c);

  *out_const = llvm::ConstantStruct::get(st_ty, {real_c, imag_c});
  return err;
}

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
    llvm::Constant** out_const) noexcept {
  if (!con || !expected_ty || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  a_float_kind fk = con->type->variant.float_kind;
  if (expected_ty->isStructTy() && expected_ty->getStructNumElements() == 2) {
    auto* st_ty = llvm::cast<llvm::StructType>(expected_ty);
    llvm::Type* elem_ty = st_ty->getElementType(0);
    llvm::Constant* zero_real = llvm::ConstantFP::getZero(elem_ty);
    llvm::Constant* imag_c = nullptr;
    llvm_gen_be_error_t err = build_fp_constant_from_internal_value(
        fk, &con->variant.float_value, elem_ty, &imag_c);
    *out_const = llvm::ConstantStruct::get(st_ty, {zero_real, imag_c});
    return err;
  }

  llvm::Type* fp_ty = expected_ty;
  if (!fp_ty->isFloatingPointTy()) {
    LLVM_GEN_BE_CHECK(llvm_type_from_float(con->type, &fp_ty));
  }
  return build_fp_constant_from_internal_value(
      fk, &con->variant.float_value, fp_ty, out_const);
}

#if !GNU_EXTENSIONS_ALLOWED
/** @brief Fallback sentinel value for `ck_label_difference` when GNU extensions are disabled. */
static constexpr a_constant_repr_kind ck_label_difference = static_cast<a_constant_repr_kind>(250);
#endif

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
    llvm::Constant** out_const) noexcept {
#if GNU_EXTENSIONS_ALLOWED
  if (!con || !expected_ty || !out_const ||
      !con->variant.label_difference.to_address ||
      !con->variant.label_difference.from_address) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Constant* to_addr = nullptr;
  llvm::Constant* from_addr = nullptr;
  LLVM_GEN_BE_CHECK(evaluate_constant(
      con->variant.label_difference.to_address, ptr_ty, &to_addr));
  LLVM_GEN_BE_CHECK(evaluate_constant(
      con->variant.label_difference.from_address, ptr_ty, &from_addr));

  llvm::Type* int_ty = expected_ty->isIntegerTy()
                           ? expected_ty
                           : be_state->module->getDataLayout().getIntPtrType(*be_state->context);
  llvm::Constant* to_int = llvm::ConstantExpr::getPtrToInt(to_addr, int_ty);
  llvm::Constant* from_int = llvm::ConstantExpr::getPtrToInt(from_addr, int_ty);
  *out_const = llvm::ConstantExpr::getSub(to_int, from_int);
  return llvm_gen_be_error_t::ok;
#else
  (void)con;
  (void)expected_ty;
  (void)out_const;
  return llvm_gen_be_error_t::invalid_argument;
#endif
}

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
    llvm::Constant** out_const) noexcept {
  if (!con || !expected_ty || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  uint64_t off_val = 0;
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
  off_val = static_cast<uint64_t>(con->variant.stack_offset.offset);
#else
  off_val = EDG_INT_VAL_LOW64(con->variant.integer_value);
#endif
  if (expected_ty->isIntegerTy()) {
    *out_const = llvm::ConstantInt::get(expected_ty, off_val);
  } else {
    *out_const = llvm::Constant::getNullValue(expected_ty);
  }
  return llvm_gen_be_error_t::ok;
}

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
    llvm::Constant** out_const) noexcept {
  if (!con || !expected_ty || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  a_dynamic_init_ptr dip = con->variant.dynamic_init.ptr;
  if (dip != nullptr) {
    if (dip->kind == dik_constant && dip->variant.constant.ptr != nullptr) {
      return evaluate_constant(dip->variant.constant.ptr, expected_ty, out_const);
    }
    if (dip->kind == dik_expression && dip->variant.expression != nullptr &&
        dip->variant.expression->kind == enk_constant &&
        dip->variant.expression->variant.constant.ptr != nullptr) {
      return evaluate_constant(
          dip->variant.expression->variant.constant.ptr, expected_ty, out_const);
    }
  }

  *out_const = llvm::Constant::getNullValue(expected_ty);
  return llvm_gen_be_error_t::ok;
}

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
    llvm::Constant** out_const) noexcept {
  if (!con || !expected_ty || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  a_template_param_constant_kind tp_kind = con->variant.template_param.kind;
  if (tp_kind == tpck_dependent_constant || tp_kind == tpck_address) {
    return evaluate_constant(
        con->variant.template_param.variant.constant, expected_ty, out_const);
  }
  if (tp_kind == tpck_expression) {
    return evaluate_constant(
        con->variant.template_param.variant.expr->variant.constant.ptr, expected_ty, out_const);
  }
  if (tp_kind == tpck_sizeof || tp_kind == tpck_datasizeof || tp_kind == tpck_alignof) {
    a_type_ptr sz_ty = skip_typerefs(con->variant.template_param.variant.templ_sizeof.type);
    uint64_t val = (tp_kind == tpck_alignof)
                       ? static_cast<uint64_t>(EDG_ALIGN_OF_TYPE(sz_ty))
                       : static_cast<uint64_t>(sz_ty->size);
    *out_const = llvm::ConstantInt::get(expected_ty, val);
    return llvm_gen_be_error_t::ok;
  }

  *out_const = llvm::Constant::getNullValue(expected_ty);
  return llvm_gen_be_error_t::constant_eval_failure;
}

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
    llvm::Constant** out_const) noexcept {
  if (!con || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  a_targ_size_t len = con->variant.string.length;
  const char* chars = con->variant.string.value;

  llvm::Type* actual_array_ty = nullptr;
  llvm_gen_be_error_t err = llvm_type_from_edg_type(con->type, &actual_array_ty);
  if (err != llvm_gen_be_error_t::ok) return err;

  llvm::Type* elem_ty = nullptr;
  if (actual_array_ty->isArrayTy()) {
    elem_ty = actual_array_ty->getArrayElementType();
  } else {
    elem_ty = llvm::Type::getInt8Ty(*be_state->context);
  }

  llvm::Constant* arr = nullptr;
  if (elem_ty->isIntegerTy(16)) {
    llvm::ArrayRef<uint16_t> wide_chars(reinterpret_cast<const uint16_t*>(chars), static_cast<size_t>(len / 2));
    arr = llvm::ConstantDataArray::get(*be_state->context, wide_chars);
  } else if (elem_ty->isIntegerTy(32)) {
    llvm::ArrayRef<uint32_t> wide_chars(reinterpret_cast<const uint32_t*>(chars), static_cast<size_t>(len / 4));
    arr = llvm::ConstantDataArray::get(*be_state->context, wide_chars);
  } else {
    uint64_t target_len = expected_ty->isArrayTy() ? expected_ty->getArrayNumElements() : static_cast<uint64_t>(len);
    std::string padded = std::string(chars, static_cast<size_t>(len));
    padded.resize(target_len, '\0');
    arr = llvm::ConstantDataArray::get(
        *be_state->context,
        llvm::ArrayRef<uint8_t>(reinterpret_cast<const uint8_t*>(padded.data()), target_len));
  }

  if (expected_ty->isPointerTy()) {
    std::string str_key = std::string(chars, static_cast<size_t>(len));
    if (be_state->string_literal_cache.count(str_key)) {
      *out_const = be_state->string_literal_cache[str_key];
      return llvm_gen_be_error_t::ok;
    }
    llvm::GlobalVariable* gv = nullptr;
    LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), arr->getType(), true, llvm::GlobalValue::PrivateLinkage, arr, ".str", &gv));
    be_state->string_literal_cache[str_key] = gv;
    *out_const = gv;
  } else {
    *out_const = arr;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Converts a single hexadecimal character into its 4-bit nibble value.
 * @details Supports `'0'`-`'9'`, `'a'`-`'f'`, and `'A'`-`'F'`.
 * @param[in] ch The ASCII character to parse.
 * @param[out] out_nibble Receives the 4-bit integer value (`0`-`15`), or `0` if non-hex.
 * @param[out] out_valid Receives `true` if @p ch is a valid hexadecimal digit.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t parse_hex_nibble(
    char ch,
    uint8_t* out_nibble,
    bool* out_valid) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  if (ch >= '0' && ch <= '9') {
    *out_nibble = static_cast<uint8_t>(ch - '0');
    *out_valid = true;
  } else if (ch >= 'a' && ch <= 'f') {
    *out_nibble = static_cast<uint8_t>(10 + (ch - 'a'));
    *out_valid = true;
  } else if (ch >= 'A' && ch <= 'F') {
    *out_nibble = static_cast<uint8_t>(10 + (ch - 'A'));
    *out_valid = true;
  } else {
    *out_nibble = 0;
    *out_valid = false;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits or retrieves the Microsoft `_GUID` constant global for an `abk_uuidof` type.
 * @details Parses the 32 hex digits of `uuid_string` into `{ i32, i16, i16, [8 x i8] }`
 *          and creates a LinkOnceODR global `__uuidof_<name>`.
 * @param[in] uuid_type Pointer to the EDG type associated with `__uuidof` (or nullptr for zero GUID).
 * @param[out] out_guid_gv Receives the `llvm::Constant*` pointing to the `_GUID` global.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t get_or_create_uuidof_global(
    a_type_ptr uuid_type,
    llvm::Constant** out_guid_gv) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  std::string sym_name = "__uuidof_zero";
  const char* uuid_str = nullptr;
  if (uuid_type != nullptr) {
    a_type_ptr ut = skip_typerefs(uuid_type);
    if (ut->source_corresp.name != nullptr && ut->source_corresp.name[0] != '\0') {
      sym_name = std::string("__uuidof_") + ut->source_corresp.name;
    } else {
      sym_name = "__uuidof_anon";
    }
#if MICROSOFT_EXTENSIONS_ALLOWED
    if (ut->kind == tk_class || ut->kind == tk_struct) {
      if (ut->variant.class_struct_union.extra_info != nullptr) {
        uuid_str = ut->variant.class_struct_union.extra_info->uuid_string;
      }
    } else if (ut->kind == tk_integer && ut->variant.integer.enum_type &&
               ut->variant.integer.extra_info != nullptr) {
      uuid_str = ut->variant.integer.extra_info->uuid_string;
    }
#endif
  }

  if (llvm::GlobalVariable* existing = be_state->module->getNamedGlobal(sym_name)) {
    *out_guid_gv = existing;
    return llvm_gen_be_error_t::ok;
  }

  uint8_t nibbles[32] = {0};
  if (uuid_str != nullptr) {
    size_t idx = 0;
    for (const char* p = uuid_str; *p != '\0' && idx < 32; ++p) {
      uint8_t nib = 0;
      bool valid = false;
      LLVM_GEN_BE_CHECK(parse_hex_nibble(*p, &nib, &valid));
      if (valid) {
        nibbles[idx++] = nib;
      }
    }
  }

  uint32_t data1 = 0;
  for (int i = 0; i < 8; ++i) {
    data1 = (data1 << 4) | nibbles[i];
  }
  uint16_t data2 = static_cast<uint16_t>(
      (nibbles[8] << 12) | (nibbles[9] << 8) | (nibbles[10] << 4) | nibbles[11]);
  uint16_t data3 = static_cast<uint16_t>(
      (nibbles[12] << 12) | (nibbles[13] << 8) | (nibbles[14] << 4) | nibbles[15]);
  uint8_t data4[8] = {0};
  for (int i = 0; i < 8; ++i) {
    data4[i] = static_cast<uint8_t>((nibbles[16 + i * 2] << 4) | nibbles[17 + i * 2]);
  }

  llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
  llvm::Type* i16_ty = llvm::Type::getInt16Ty(*be_state->context);
  llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
  llvm::ArrayType* arr8_ty = llvm::ArrayType::get(i8_ty, 8);
  llvm::StructType* guid_ty = llvm::StructType::get(
      *be_state->context, {i32_ty, i16_ty, i16_ty, arr8_ty});

  llvm::Constant* guid_init = llvm::ConstantStruct::get(
      guid_ty,
      {llvm::ConstantInt::get(i32_ty, data1),
       llvm::ConstantInt::get(i16_ty, data2),
       llvm::ConstantInt::get(i16_ty, data3),
       llvm::ConstantDataArray::get(*be_state->context, llvm::ArrayRef<uint8_t>(data4, 8))});

  llvm::GlobalVariable* gv = nullptr;
  LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), guid_ty, true, llvm::GlobalValue::LinkOnceODRLinkage, guid_init, sym_name, &gv));
  *out_guid_gv = gv;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Computes additional byte offset from an EDG `a_subobject_path_ptr` chain when `offset == 0`.
 * @details Traverses base class, field, and array element subobject path entries.
 * @param[in] path Pointer to the head of the `a_subobject_path` list.
 * @param[out] out_extra_offset Receives the accumulated byte offset.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t compute_subobject_path_offset(
    a_subobject_path_ptr path,
    int64_t* out_extra_offset) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  int64_t total = 0;
  a_type_ptr curr_elem_ty = nullptr;
  for (a_subobject_path_ptr p = path; p != nullptr; p = p->next) {
    if (p->is_base_class) {
      if (p->variant.base_class != nullptr) {
        total += static_cast<int64_t>(p->variant.base_class->offset);
      }
    } else if (p->is_offset) {
      int64_t elem_sz = (curr_elem_ty != nullptr && curr_elem_ty->size > 0)
                            ? static_cast<int64_t>(curr_elem_ty->size)
                            : 1;
      total += static_cast<int64_t>(p->variant.ptr_offset) * elem_sz;
    } else if (p->variant.field != nullptr) {
      total += static_cast<int64_t>(p->variant.field->offset);
      if (p->variant.field->type != nullptr) {
        a_type_ptr ft = skip_typerefs(p->variant.field->type);
        if (ft->kind == tk_array) {
          curr_elem_ty = ft->variant.array.element_type;
        } else {
          curr_elem_ty = ft;
        }
      }
    }
  }
  *out_extra_offset = total;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Translates an EDG address constant (`ck_address`) into an LLVM global, function, or `BlockAddress` constant.
 * @details Supports `abk_label`, `abk_variable`, `abk_routine`, `abk_constant`, `abk_temporary`,
 *          `abk_typeid`, `abk_uuidof`, and `abk_param_ref`, applying byte offsets and `subobject_path`.
 * @param[in] con Pointer to the EDG address constant structure.
 * @param[in] expected_ty The target LLVM pointer type.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_const_from_address(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!con || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  llvm::Constant* base = nullptr;

  if (con->variant.address.kind == abk_label) {
    a_label_ptr label = con->variant.address.variant.label;
    llvm::BasicBlock* label_bb = nullptr;
    if (be_state->label_blocks.count(label)) {
      label_bb = be_state->label_blocks[label];
    } else {
      llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
      label_bb = llvm::BasicBlock::Create(*be_state->context, "label", func);
      be_state->label_blocks[label] = label_bb;
    }
    if (!label_bb->getParent()) {
      label_bb->insertInto(be_state->builder->GetInsertBlock()->getParent());
    }
    base = llvm::BlockAddress::get(label_bb->getParent(), label_bb);
  } else if (con->variant.address.kind == abk_variable) {
    a_variable_ptr var = con->variant.address.variant.variable;
    if (!var) {
      *out_const = llvm::Constant::getNullValue(expected_ty);
      return llvm_gen_be_error_t::ok;
    }
    auto local_it = be_state->local_vars.find(var);
    if (local_it != be_state->local_vars.end()) {
      if (auto* existing_c = llvm::dyn_cast_or_null<llvm::Constant>(local_it->second)) {
        base = existing_c;
      }
    }
    if (!base && !var->source_corresp.name) {
      if (!var->type) {
        *out_const = llvm::Constant::getNullValue(expected_ty);
        return llvm_gen_be_error_t::ok;
      }
      llvm::Type* var_ty = nullptr;
      LLVM_GEN_BE_CHECK(get_llvm_type(var->type, &var_ty));
      llvm::Constant* anon_init = nullptr;
      if (var->init_kind == initk_static && var->initializer.constant != nullptr) {
        LLVM_GEN_BE_CHECK(evaluate_constant(var->initializer.constant, var_ty, &anon_init));
      }
      if (!anon_init) {
        anon_init = llvm::Constant::getNullValue(var_ty);
      }
      static uint64_t s_anon_const_var_id = 0;
      std::string anon_name = ".edg.anon.const_var." + std::to_string(++s_anon_const_var_id);
      llvm::GlobalVariable* anon_gv = nullptr;
      LLVM_GEN_BE_CHECK(llvm_create_global_variable(
          be_state->module.get(), anon_init->getType(), true, llvm::GlobalValue::InternalLinkage,
          anon_init, anon_name, &anon_gv));
      be_state->local_vars[var] = anon_gv;
      base = anon_gv;
    }
    if (!base) {
      const char* sym_name = nullptr;
      LLVM_GEN_BE_CHECK(llvm_get_variable_symbol_name(var, &sym_name));
      llvm::GlobalVariable* gvar = be_state->module->getNamedGlobal(sym_name);
      if (!gvar) {
        llvm::Type* var_ty = nullptr;
        llvm_gen_be_error_t err = get_llvm_type(var->type, &var_ty);
        if (err != llvm_gen_be_error_t::ok) return err;
        gvar = llvm::cast<llvm::GlobalVariable>(
            be_state->module->getOrInsertGlobal(sym_name, var_ty));
      }
      base = gvar;
    }
  } else if (con->variant.address.kind == abk_routine) {
    a_routine_ptr routine = con->variant.address.variant.routine;
    if (!routine || !routine->source_corresp.name) {
      *out_const = llvm::Constant::getNullValue(expected_ty);
      return llvm_gen_be_error_t::ok;
    }
    llvm::Function* func = be_state->module->getFunction(routine->source_corresp.name);
    if (!func) {
      llvm::FunctionType* func_ty = nullptr;
      llvm_gen_be_error_t err = llvm_type_from_routine(routine->type, &func_ty);
      if (err != llvm_gen_be_error_t::ok) return err;
      func = llvm::Function::Create(
          func_ty, llvm::GlobalValue::ExternalLinkage,
          routine->source_corresp.name, be_state->module.get());
    }
    base = func;
  } else if (con->variant.address.kind == abk_constant) {
    a_constant_ptr target = con->variant.address.variant.constant;
    llvm::Type* target_ty = nullptr;
    llvm_gen_be_error_t err = get_llvm_type(target->type, &target_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
    llvm::Type* target_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    err = evaluate_constant(target, target_ptr_ty, &base);
    if (err != llvm_gen_be_error_t::ok) return err;
  } else if (con->variant.address.kind == abk_temporary) {
    a_constant_ptr target = con->variant.address.variant.constant;
    if (!target) {
      *out_const = llvm::Constant::getNullValue(expected_ty);
      return llvm_gen_be_error_t::ok;
    }
    auto it = be_state->temporary_constant_cache.find(target);
    if (it != be_state->temporary_constant_cache.end()) {
      base = it->second;
    } else {
      llvm::Type* target_ty = nullptr;
      LLVM_GEN_BE_CHECK(get_llvm_type(target->type, &target_ty));
      llvm::Constant* init_val = nullptr;
      LLVM_GEN_BE_CHECK(evaluate_constant(target, target_ty, &init_val));
      llvm::GlobalVariable* tmp_gv = nullptr;
      LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), init_val->getType(), true, llvm::GlobalValue::PrivateLinkage, init_val, ".ref.tmp", &tmp_gv));
      be_state->temporary_constant_cache[target] = tmp_gv;
      base = tmp_gv;
    }
  } else if (con->variant.address.kind == abk_typeid) {
    LLVM_GEN_BE_CHECK(get_typeinfo_global(con->variant.address.variant.type, &base));
  } else if (con->variant.address.kind == abk_uuidof) {
    LLVM_GEN_BE_CHECK(get_or_create_uuidof_global(con->variant.address.variant.type, &base));
  } else if (con->variant.address.kind == abk_param_ref) {
    unsigned int pnum = con->variant.address.variant.param_ref.param_num;
    if (pnum > 0 && pnum <= be_state->current_function_params.size()) {
      llvm::Value* pval = be_state->current_function_params[pnum - 1];
      if (auto* cval = llvm::dyn_cast_or_null<llvm::Constant>(pval)) {
        base = cval;
      }
    }
    if (!base) {
      std::string pname = "__param_ref_" + std::to_string(pnum);
      llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
      base = llvm::cast<llvm::GlobalVariable>(
          be_state->module->getOrInsertGlobal(pname, i8_ty));
    }
  } else {
    *out_const = llvm::Constant::getNullValue(expected_ty);
    return llvm_gen_be_error_t::ok;
  }

  int64_t total_offset = static_cast<int64_t>(con->variant.address.offset);
  if (total_offset == 0 && con->variant.address.subobject_path != nullptr) {
    LLVM_GEN_BE_CHECK(compute_subobject_path_offset(
        con->variant.address.subobject_path, &total_offset));
  }

  if (total_offset != 0) {
    llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
    llvm::Constant* offset_val = llvm::ConstantInt::get(
        be_state->module->getDataLayout().getIntPtrType(*be_state->context),
        static_cast<uint64_t>(total_offset));
    base = llvm::ConstantExpr::getGetElementPtr(i8_ty, base, offset_val);
  }

  if (expected_ty->isIntegerTy()) {
    base = llvm::ConstantExpr::getPtrToInt(base, expected_ty);
  }

  *out_const = base;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Resolves the target array index range `[start_idx, end_idx]` for a `ck_designator` entry.
 * @details Handles both non-generic `array_element` and generic `subscript` (single or `[first ... last]` range).
 * @param[in] desig Pointer to the `ck_designator` constant entry.
 * @param[out] out_start Receives the starting array index.
 * @param[out] out_end Receives the inclusive ending array index.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t resolve_array_designator_range(
    a_constant_ptr desig,
    uint64_t* out_start,
    uint64_t* out_end) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  if (desig->variant.designator.is_generic) {
    a_constant_ptr sub = desig->variant.designator.variant.subscript;
    if (sub != nullptr) {
      uint64_t first = EDG_INT_VAL_LOW64(sub->variant.integer_value);
      uint64_t last = (sub->next != nullptr)
                          ? EDG_INT_VAL_LOW64(sub->next->variant.integer_value)
                          : first;
      *out_start = first;
      *out_end = (last >= first) ? last : first;
      return llvm_gen_be_error_t::ok;
    }
  }
  uint64_t idx = static_cast<uint64_t>(desig->variant.designator.variant.array_element);
  *out_start = idx;
  *out_end = idx;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Checks whether @p ty is a `tk_struct` or `tk_class` empty subobject type (EBO / `[[no_unique_address]]`).
 * @details Guards `is_empty_class_type` so non-class EDG types never access the `class_struct_union` union variant.
 * @param[in] ty Pointer to the EDG type to inspect (may be `nullptr`).
 * @param[out] out_empty Receives `true` if @p ty is an empty struct/class type.
 * @return `llvm_gen_be_error_t::ok` on success.
 */
static llvm_gen_be_error_t is_empty_subobject_type(
    a_type_ptr ty,
    bool* out_empty) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  a_type_ptr ut = ty ? skip_typerefs(ty) : nullptr;
  *out_empty = (ut != nullptr &&
                (ut->kind == tk_struct || ut->kind == tk_class) &&
                is_empty_class_type(ut) != FALSE);
  return llvm_gen_be_error_t::ok;
}

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
    llvm::Constant** out_packed_const) noexcept {
  if (!io_field || !*io_field || !io_con || !unit_ty || !out_packed_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  bool is_byte_array = unit_ty->isArrayTy() && unit_ty->getArrayElementType()->isIntegerTy(8);
  if (!unit_ty->isIntegerTy() && !is_byte_array) {
    return llvm_gen_be_error_t::bitfield_access_failed;
  }
  unsigned unit_bits = is_byte_array
                           ? static_cast<unsigned>(unit_ty->getArrayNumElements() * 8U)
                           : unit_ty->getIntegerBitWidth();
  llvm::Type* int_unit_ty = is_byte_array
                                ? llvm::IntegerType::get(*be_state->context, unit_bits)
                                : unit_ty;

  a_field_ptr field = *io_field;
  a_constant_ptr c = *io_con;
  bool any_named = false;
  for (a_field_ptr scan = field; scan != nullptr; scan = scan->next) {
    if (scan->source_corresp.name != nullptr && scan->source_corresp.name[0] != '\0') {
      any_named = true;
      break;
    }
  }
  uint64_t unit_start_offset = static_cast<uint64_t>(field->offset);
  llvm::APInt unit_val = llvm::APInt(unit_bits, 0);
  bool is_big_endian = be_state->module->getDataLayout().isBigEndian();

  while (field && field->is_bit_field) {
    if (any_named && field->bit_size == 0) {
      break;
    }
    uint64_t bit_off = (static_cast<uint64_t>(field->offset) - unit_start_offset) * 8 + field->offset_bit_remainder;
    uint64_t bit_sz = field->bit_size;
    if (bit_off + bit_sz > unit_bits) {
      break;
    }
    bool unnamed_bf = any_named &&
                      (field->source_corresp.name == nullptr || field->source_corresp.name[0] == '\0');
    bool consume_const = !unnamed_bf || (c && c->implicit_aggr_element);
    if (consume_const && c && c->kind == ck_integer && bit_sz > 0) {
      llvm::Constant* bf_const = nullptr;
      LLVM_GEN_BE_CHECK(llvm_const_from_integer(c, int_unit_ty, &bf_const));
      auto* ci = llvm::cast<llvm::ConstantInt>(bf_const);
      llvm::APInt mask = llvm::APInt::getLowBitsSet(unit_bits, static_cast<unsigned>(bit_sz));
      unsigned shift_amt = is_big_endian
                               ? static_cast<unsigned>(unit_bits - (bit_off + bit_sz))
                               : static_cast<unsigned>(bit_off);
      llvm::APInt shifted = (ci->getValue().zextOrTrunc(unit_bits) & mask).shl(shift_amt);
      unit_val |= shifted;
    }
    field = field->next;
    if (consume_const && c) {
      c = c->next;
    }
  }

  *io_field = field;
  *io_con = c;
  if (is_byte_array) {
    unsigned num_bytes = unit_bits / 8U;
    std::vector<uint8_t> raw_bytes(num_bytes, 0U);
    for (unsigned b = 0; b < num_bytes; ++b) {
      unsigned byte_idx = is_big_endian ? (num_bytes - 1U - b) : b;
      raw_bytes[byte_idx] = static_cast<uint8_t>(unit_val.extractBitsAsZExtValue(8U, b * 8U));
    }
    *out_packed_const = llvm::ConstantDataArray::get(*be_state->context, raw_bytes);
  } else {
    *out_packed_const = llvm::ConstantInt::get(unit_ty, unit_val);
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Serializes a non-relocatable `llvm::Constant*` into target-endian raw bytes for union coercion.
 * @details Supports `ConstantInt`, `ConstantFP`, `ConstantPointerNull`, `ConstantAggregateZero`,
 *          `ConstantDataSequential`, `ConstantArray`, and `ConstantStruct`.
 * @param[in] val Constant value to serialize.
 * @param[in,out] buf Destination byte buffer.
 * @param[in] base_off Starting byte offset within @p buf.
 * @param[out] out_ok Receives `true` if @p val was completely serialized without relocatable symbols.
 * @return `llvm_gen_be_error_t::ok` on success.
 */
static llvm_gen_be_error_t try_serialize_constant_bytes(
    llvm::Constant* val,
    std::vector<uint8_t>& buf,
    uint64_t base_off,
    bool* out_ok) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  *out_ok = false;
  const llvm::DataLayout& dl = be_state->module->getDataLayout();
  bool is_be = dl.isBigEndian();
  if (base_off >= buf.size()) {
    return llvm_gen_be_error_t::ok;
  }
  if (llvm::isa<llvm::ConstantAggregateZero>(val) ||
      llvm::isa<llvm::ConstantPointerNull>(val)) {
    *out_ok = true;
    return llvm_gen_be_error_t::ok;
  }
  if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(val)) {
    const llvm::APInt& ap = ci->getValue();
    unsigned num_bytes = static_cast<unsigned>((ap.getBitWidth() + 7U) / 8U);
    for (unsigned b = 0; b < num_bytes && (base_off + b) < buf.size(); ++b) {
      unsigned bit_pos = b * 8U;
      unsigned rem = ap.getBitWidth() - bit_pos;
      unsigned take = (rem < 8U) ? rem : 8U;
      unsigned dst_idx = is_be ? (num_bytes - 1U - b) : b;
      buf[base_off + dst_idx] = static_cast<uint8_t>(ap.extractBitsAsZExtValue(take, bit_pos));
    }
    *out_ok = true;
    return llvm_gen_be_error_t::ok;
  }
  if (auto* cfp = llvm::dyn_cast<llvm::ConstantFP>(val)) {
    llvm::APInt ap = cfp->getValueAPF().bitcastToAPInt();
    unsigned num_bytes = static_cast<unsigned>((ap.getBitWidth() + 7U) / 8U);
    for (unsigned b = 0; b < num_bytes && (base_off + b) < buf.size(); ++b) {
      unsigned dst_idx = is_be ? (num_bytes - 1U - b) : b;
      buf[base_off + dst_idx] = static_cast<uint8_t>(ap.extractBitsAsZExtValue(8U, b * 8U));
    }
    *out_ok = true;
    return llvm_gen_be_error_t::ok;
  }
  if (auto* cds = llvm::dyn_cast<llvm::ConstantDataSequential>(val)) {
    llvm::StringRef raw = cds->getRawDataValues();
    for (size_t b = 0; b < raw.size() && (base_off + b) < buf.size(); ++b) {
      buf[base_off + b] = static_cast<uint8_t>(raw[b]);
    }
    *out_ok = true;
    return llvm_gen_be_error_t::ok;
  }
  if (auto* ca = llvm::dyn_cast<llvm::ConstantArray>(val)) {
    uint64_t elem_sz = dl.getTypeAllocSize(ca->getType()->getElementType());
    for (unsigned e = 0; e < ca->getNumOperands(); ++e) {
      bool sub_ok = false;
      LLVM_GEN_BE_CHECK(try_serialize_constant_bytes(
          ca->getOperand(e), buf, base_off + e * elem_sz, &sub_ok));
      if (!sub_ok) return llvm_gen_be_error_t::ok;
    }
    *out_ok = true;
    return llvm_gen_be_error_t::ok;
  }
  if (auto* cs = llvm::dyn_cast<llvm::ConstantStruct>(val)) {
    const llvm::StructLayout* sl = dl.getStructLayout(cs->getType());
    for (unsigned e = 0; e < cs->getNumOperands(); ++e) {
      bool sub_ok = false;
      LLVM_GEN_BE_CHECK(try_serialize_constant_bytes(
          cs->getOperand(e), buf, base_off + sl->getElementOffset(e), &sub_ok));
      if (!sub_ok) return llvm_gen_be_error_t::ok;
    }
    *out_ok = true;
    return llvm_gen_be_error_t::ok;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Reconstructs an `llvm::Constant*` of @p target_ty from serialized target-endian raw bytes.
 * @details Supports `IntegerType`, floating-point types (`half`, `bfloat`, `float`, `double`, `fp128`, `x86_fp80`),
 *          `PointerType` (when all-zero), `ArrayType`, and `StructType`.
 * @param[in] target_ty Target LLVM type to materialize.
 * @param[in] buf Source byte buffer.
 * @param[in] base_off Starting byte offset within @p buf.
 * @param[out] out_const Receives the reconstructed `llvm::Constant*`, or `nullptr` if unsupported.
 * @return `llvm_gen_be_error_t::ok` on success.
 */
static llvm_gen_be_error_t deserialize_constant_from_bytes(
    llvm::Type* target_ty,
    const std::vector<uint8_t>& buf,
    uint64_t base_off,
    llvm::Constant** out_const) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  *out_const = nullptr;
  const llvm::DataLayout& dl = be_state->module->getDataLayout();
  bool is_be = dl.isBigEndian();
  uint64_t alloc_sz = dl.getTypeAllocSize(target_ty);
  if (target_ty->isIntegerTy()) {
    unsigned bw = target_ty->getIntegerBitWidth();
    unsigned num_bytes = static_cast<unsigned>((bw + 7U) / 8U);
    llvm::APInt ap = llvm::APInt(bw, 0);
    for (unsigned b = 0; b < num_bytes; ++b) {
      unsigned src_idx = is_be ? (num_bytes - 1U - b) : b;
      ap |= llvm::APInt(bw, buf[base_off + src_idx]).shl(b * 8U);
    }
    *out_const = llvm::ConstantInt::get(target_ty, ap);
    return llvm_gen_be_error_t::ok;
  }
  if (target_ty->isFloatingPointTy() && !target_ty->isPPC_FP128Ty()) {
    unsigned bw = static_cast<unsigned>(target_ty->getPrimitiveSizeInBits());
    unsigned num_bytes = (bw + 7U) / 8U;
    llvm::APInt ap = llvm::APInt(bw, 0);
    for (unsigned b = 0; b < num_bytes; ++b) {
      unsigned src_idx = is_be ? (num_bytes - 1U - b) : b;
      ap |= llvm::APInt(bw, buf[base_off + src_idx]).shl(b * 8U);
    }
    llvm::APFloat apf{target_ty->getFltSemantics(), ap};
    *out_const = llvm::ConstantFP::get(*be_state->context, apf);
    return llvm_gen_be_error_t::ok;
  }
  if (target_ty->isPointerTy()) {
    bool all_zero = true;
    for (uint64_t b = 0; b < alloc_sz; ++b) {
      if (buf[base_off + b] != 0) {
        all_zero = false;
        break;
      }
    }
    if (all_zero) {
      *out_const = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(target_ty));
    } else {
      llvm::Constant* int_c = nullptr;
      llvm::Type* intptr_ty = dl.getIntPtrType(*be_state->context);
      LLVM_GEN_BE_CHECK(deserialize_constant_from_bytes(intptr_ty, buf, base_off, &int_c));
      *out_const = llvm::ConstantExpr::getIntToPtr(int_c, target_ty);
    }
    return llvm_gen_be_error_t::ok;
  }
  if (auto* arr_ty = llvm::dyn_cast<llvm::ArrayType>(target_ty)) {
    if (arr_ty->getElementType()->isIntegerTy(8)) {
      llvm::ArrayRef<uint8_t> slice{buf.data() + base_off, static_cast<size_t>(arr_ty->getNumElements())};
      *out_const = llvm::ConstantDataArray::get(*be_state->context, slice);
      return llvm_gen_be_error_t::ok;
    }
    uint64_t elem_sz = dl.getTypeAllocSize(arr_ty->getElementType());
    std::vector<llvm::Constant*> elems;
    for (uint64_t e = 0; e < arr_ty->getNumElements(); ++e) {
      llvm::Constant* ec = nullptr;
      LLVM_GEN_BE_CHECK(deserialize_constant_from_bytes(
          arr_ty->getElementType(), buf, base_off + e * elem_sz, &ec));
      if (!ec) return llvm_gen_be_error_t::ok;
      elems.push_back(ec);
    }
    *out_const = llvm::ConstantArray::get(arr_ty, elems);
    return llvm_gen_be_error_t::ok;
  }
  if (auto* st_ty = llvm::dyn_cast<llvm::StructType>(target_ty)) {
    const llvm::StructLayout* sl = dl.getStructLayout(st_ty);
    std::vector<llvm::Constant*> elems;
    for (unsigned e = 0; e < st_ty->getNumElements(); ++e) {
      llvm::Constant* ec = nullptr;
      LLVM_GEN_BE_CHECK(deserialize_constant_from_bytes(
          st_ty->getElementType(e), buf, base_off + sl->getElementOffset(e), &ec));
      if (!ec) return llvm_gen_be_error_t::ok;
      elems.push_back(ec);
    }
    *out_const = llvm::ConstantStruct::get(st_ty, elems);
    return llvm_gen_be_error_t::ok;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Translates an EDG aggregate constant (`ck_aggregate`) into an `llvm::ConstantArray` or `llvm::ConstantStruct`.
 * @details Handles arrays (including `ck_designator` and `ck_init_repeat`), structs (including sparse/out-of-order
 *          field designators, empty base optimization entries, and bitfields), and unions.
 * @param[in] con Pointer to the EDG aggregate constant structure.
 * @param[in] expected_ty The target LLVM array or struct type.
 * @param[out] out_const Receives the resulting `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_const_from_aggregate(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!con || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  if (expected_ty->isVectorTy()) {
    if (auto* sv_ty = llvm::dyn_cast<llvm::ScalableVectorType>(expected_ty)) {
      llvm::Type* elem_ty = sv_ty->getElementType();
      a_constant_ptr c = con->variant.aggregate.first_constant;
      if (c != nullptr) {
        llvm::Constant* elem_const = nullptr;
        llvm_gen_be_error_t err = evaluate_constant(c, elem_ty, &elem_const);
        if (err != llvm_gen_be_error_t::ok) return err;
        *out_const = llvm::ConstantVector::getSplat(sv_ty->getElementCount(), elem_const);
      } else {
        *out_const = llvm::Constant::getNullValue(expected_ty);
      }
      return llvm_gen_be_error_t::ok;
    }
    auto* vec_ty = llvm::cast<llvm::FixedVectorType>(expected_ty);
    llvm::Type* elem_ty = vec_ty->getElementType();
    std::vector<llvm::Constant*> elems;
    for (a_constant_ptr c = con->variant.aggregate.first_constant; c != nullptr; c = c->next) {
      llvm::Constant* elem_const = nullptr;
      llvm_gen_be_error_t err = evaluate_constant(c, elem_ty, &elem_const);
      if (err != llvm_gen_be_error_t::ok) return err;
      elems.push_back(elem_const);
    }
    unsigned expected_elems = vec_ty->getNumElements();
    while (elems.size() < expected_elems) {
      elems.push_back(llvm::Constant::getNullValue(elem_ty));
    }
    *out_const = llvm::ConstantVector::get(elems);
    return llvm_gen_be_error_t::ok;
  }

  if (expected_ty->isArrayTy()) {
    llvm::Type* elem_ty = expected_ty->getArrayElementType();
    unsigned expected_elems = static_cast<unsigned>(expected_ty->getArrayNumElements());
    std::vector<llvm::Constant*> elems(expected_elems, llvm::Constant::getNullValue(elem_ty));
    unsigned cur_idx = 0;

    for (a_constant_ptr c = con->variant.aggregate.first_constant; c != nullptr; c = c->next) {
      if (c->kind == ck_designator) {
        uint64_t start_idx = 0;
        uint64_t end_idx = 0;
        LLVM_GEN_BE_CHECK(resolve_array_designator_range(c, &start_idx, &end_idx));
        c = c->next;
        if (!c) break;
        llvm::Constant* elem_const = nullptr;
        LLVM_GEN_BE_CHECK(evaluate_constant(c, elem_ty, &elem_const));
        for (uint64_t idx = start_idx; idx <= end_idx; ++idx) {
          if (idx < expected_elems) {
            elems[idx] = elem_const;
          }
        }
        cur_idx = static_cast<unsigned>(end_idx + 1);
        continue;
      }
      if (c->kind == ck_init_repeat) {
        uint64_t count = static_cast<uint64_t>(c->variant.init_repeat.count);
        llvm::Constant* rep_const = nullptr;
        LLVM_GEN_BE_CHECK(evaluate_constant(c->variant.init_repeat.constant, elem_ty, &rep_const));
        for (uint64_t r = 0; r < count; ++r) {
          if (cur_idx < expected_elems) {
            elems[cur_idx++] = rep_const;
          }
        }
        continue;
      }
      llvm::Constant* elem_const = nullptr;
      llvm_gen_be_error_t err = evaluate_constant(c, elem_ty, &elem_const);
      if (err != llvm_gen_be_error_t::ok) return err;
      if (cur_idx < expected_elems) {
        elems[cur_idx++] = elem_const;
      }
    }
    *out_const = llvm::ConstantArray::get(llvm::cast<llvm::ArrayType>(expected_ty), elems);
    return llvm_gen_be_error_t::ok;
  } else if (expected_ty->isStructTy()) {
    llvm::StructType* st_ty = llvm::cast<llvm::StructType>(expected_ty);
    a_type_ptr agg_tp = skip_typerefs(con->type);
    if (agg_tp->kind == tk_union) {
      const llvm::DataLayout& dl = be_state->module->getDataLayout();
      uint64_t union_sz = std::max<uint64_t>(static_cast<uint64_t>(agg_tp->size), dl.getTypeAllocSize(st_ty));
      a_constant_ptr c = con->variant.aggregate.first_constant;
      a_field_ptr active_field = agg_tp->variant.class_struct_union.field_list;
      if (c && c->kind == ck_designator) {
        if (!c->variant.designator.is_generic && c->variant.designator.variant.field != nullptr) {
          active_field = c->variant.designator.variant.field;
        } else if (c->variant.designator.is_generic && c->variant.designator.variant.field_name != nullptr) {
          for (a_field_ptr scan = active_field; scan != nullptr; scan = scan->next) {
            if (scan->source_corresp.name != nullptr &&
                std::strcmp(scan->source_corresp.name, c->variant.designator.variant.field_name) == 0) {
              active_field = scan;
              break;
            }
          }
        }
        c = c->next;
      }
      if (c && st_ty->getNumElements() > 0) {
        llvm::Type* target_ty = nullptr;
        a_type_ptr eval_edg_ty = (c->type != nullptr)
                                     ? c->type
                                     : (active_field ? active_field->type : nullptr);
        LLVM_GEN_BE_CHECK(get_llvm_type(eval_edg_ty, &target_ty));
        llvm::Constant* active_val = nullptr;
        if (active_field && active_field->is_bit_field) {
          a_field_ptr bf_cursor = active_field;
          a_constant_ptr c_cursor = c;
          LLVM_GEN_BE_CHECK(llvm_pack_constant_bitfield_bytes(
              &bf_cursor, &c_cursor, st_ty->getElementType(0), &active_val));
        } else {
          LLVM_GEN_BE_CHECK(evaluate_constant(c, target_ty, &active_val));
        }
        if (active_val->getType() == st_ty->getElementType(0)) {
          std::vector<llvm::Constant*> elems;
          elems.push_back(active_val);
          for (unsigned i = 1; i < st_ty->getNumElements(); ++i) {
            elems.push_back(llvm::Constant::getNullValue(st_ty->getElementType(i)));
          }
          *out_const = llvm::ConstantStruct::get(st_ty, elems);
          return llvm_gen_be_error_t::ok;
        }

        std::vector<uint8_t> raw_bytes(union_sz, 0U);
        bool serialized = false;
        LLVM_GEN_BE_CHECK(try_serialize_constant_bytes(active_val, raw_bytes, 0, &serialized));
        if (serialized) {
          llvm::Constant* rebuilt = nullptr;
          LLVM_GEN_BE_CHECK(deserialize_constant_from_bytes(st_ty, raw_bytes, 0, &rebuilt));
          if (rebuilt != nullptr) {
            *out_const = rebuilt;
            return llvm_gen_be_error_t::ok;
          }
        }

        llvm::Type* elem0_ty = st_ty->getElementType(0);
        if (active_val->getType()->isPointerTy() && elem0_ty->isIntegerTy()) {
          std::vector<llvm::Constant*> elems;
          elems.push_back(llvm::ConstantExpr::getPtrToInt(active_val, elem0_ty));
          for (unsigned i = 1; i < st_ty->getNumElements(); ++i) {
            elems.push_back(llvm::Constant::getNullValue(st_ty->getElementType(i)));
          }
          *out_const = llvm::ConstantStruct::get(st_ty, elems);
          return llvm_gen_be_error_t::ok;
        }

        uint64_t active_sz = dl.getTypeAllocSize(active_val->getType());
        std::vector<llvm::Constant*> anon_elems;
        anon_elems.push_back(active_val);
        if (active_sz < union_sz) {
          anon_elems.push_back(llvm::Constant::getNullValue(
              llvm::ArrayType::get(llvm::Type::getInt8Ty(*be_state->context), union_sz - active_sz)));
        }
        *out_const = llvm::ConstantStruct::getAnon(*be_state->context, anon_elems, /*Packed=*/false);
        return llvm_gen_be_error_t::ok;
      }

      *out_const = llvm::Constant::getNullValue(st_ty);
      return llvm_gen_be_error_t::ok;
    }

    a_field_ptr field_head = (agg_tp->kind == tk_struct || agg_tp->kind == tk_class)
                                 ? agg_tp->variant.class_struct_union.field_list
                                 : nullptr;
    a_constant_ptr c = con->variant.aggregate.first_constant;

    if (field_head != nullptr) {
      const llvm::DataLayout& dl = be_state->module->getDataLayout();
      std::vector<llvm::Constant*> elems;
      unsigned i = 0;
      uint64_t current_offset = 0;
      a_field_ptr field = field_head;

      while (i < st_ty->getNumElements() && (c != nullptr || field != nullptr)) {
#if DO_IL_LOWERING
        while (c != nullptr && c->initializes_empty_object) {
          c = c->next;
        }
#endif
        while (field != nullptr && !field->is_bit_field &&
               (field->is_optimized_empty_class ||
                (field->next != nullptr && field->next->offset == field->offset))) {
          bool empty_sub = (field->is_optimized_empty_class != FALSE);
          if (!empty_sub) {
            LLVM_GEN_BE_CHECK(is_empty_subobject_type(field->type, &empty_sub));
          }
          if (!empty_sub) break;
          if (c != nullptr &&
              (c->constant_for_base_class ||
               c->constant_for_base_class_from_constexpr_folding ||
               (c->kind == ck_aggregate && c->variant.aggregate.first_constant == nullptr))) {
            c = c->next;
          }
          field = field->next;
        }

        while (c != nullptr && c->kind == ck_designator) {
          a_field_ptr target_field = nullptr;
          if (!c->variant.designator.is_generic) {
            target_field = c->variant.designator.variant.field;
          } else if (c->variant.designator.variant.field_name != nullptr) {
            for (a_field_ptr scan = field_head; scan != nullptr; scan = scan->next) {
              if (scan->source_corresp.name != nullptr &&
                  std::strcmp(scan->source_corresp.name,
                              c->variant.designator.variant.field_name) == 0) {
                target_field = scan;
                break;
              }
            }
          }
          c = c->next;
          while (target_field != nullptr && field != nullptr && field != target_field &&
                 i < st_ty->getNumElements()) {
            bool empty_sub = false;
            LLVM_GEN_BE_CHECK(is_empty_subobject_type(field->type, &empty_sub));
            if (!field->is_bit_field && field->next != nullptr &&
                field->next->offset == field->offset && empty_sub) {
              field = field->next;
              continue;
            }
            if (field->offset > current_offset) {
              llvm::Type* d_cand_ty = st_ty->getElementType(i);
              if (d_cand_ty->isArrayTy() && d_cand_ty->getArrayElementType()->isIntegerTy(8)) {
                elems.push_back(llvm::Constant::getNullValue(d_cand_ty));
                current_offset = static_cast<uint64_t>(field->offset);
                i++;
                if (i >= st_ty->getNumElements()) break;
              } else {
                current_offset = static_cast<uint64_t>(field->offset);
              }
            }
            if (field->offset < current_offset && (field->is_bit_field || empty_sub)) {
              a_type_ptr fut = field->type ? skip_typerefs(field->type) : nullptr;
              a_targ_size_t fsz = fut ? fut->size : 0;
              if (!field->is_bit_field && static_cast<uint64_t>(field->offset + fsz) > current_offset) {
                elems.push_back(llvm::Constant::getNullValue(st_ty->getElementType(i)));
                current_offset = static_cast<uint64_t>(field->offset + fsz);
                i++;
              }
              field = field->next;
              continue;
            }
            if (field->is_bit_field && field->bit_size == 0) {
              field = field->next;
              continue;
            }
            llvm::Type* skipped_ty = st_ty->getElementType(i);
            elems.push_back(llvm::Constant::getNullValue(skipped_ty));
            uint64_t skipped_sz = dl.getTypeAllocSize(skipped_ty);
            if (field->is_bit_field) {
              uint64_t end_off = current_offset + skipped_sz;
              while (field != nullptr && field->is_bit_field && field->bit_size > 0 &&
                     static_cast<uint64_t>(field->offset) < end_off) {
                field = field->next;
              }
            } else {
              field = field->next;
            }
            current_offset += skipped_sz;
            i++;
          }
          if (i >= st_ty->getNumElements()) {
            break;
          }
        }
        if (i >= st_ty->getNumElements()) {
          break;
        }

        if (field != nullptr) {
          a_type_ptr field_ut = field->type ? skip_typerefs(field->type) : nullptr;
          bool empty_sub = false;
          LLVM_GEN_BE_CHECK(is_empty_subobject_type(field->type, &empty_sub));
          llvm::Type* cand_elem_ty = st_ty->getElementType(i);
          bool cand_is_i8_arr = cand_elem_ty->isArrayTy() &&
                                cand_elem_ty->getArrayElementType()->isIntegerTy(8);
          if (field->offset > current_offset) {
            if (cand_is_i8_arr &&
                (field->is_bit_field || !field_ut || field_ut->kind != tk_array ||
                 cand_elem_ty->getArrayNumElements() == static_cast<uint64_t>(field->offset - current_offset))) {
              elems.push_back(llvm::Constant::getNullValue(cand_elem_ty));
              current_offset = static_cast<uint64_t>(field->offset);
              i++;
              continue;
            }
            current_offset = static_cast<uint64_t>(field->offset);
          }
          if (field->offset < current_offset && (field->is_bit_field || empty_sub)) {
            a_targ_size_t fsz = field_ut ? field_ut->size : 0;
            if (!field->is_bit_field &&
                static_cast<uint64_t>(field->offset + fsz) > current_offset) {
              elems.push_back(llvm::Constant::getNullValue(st_ty->getElementType(i)));
              current_offset = static_cast<uint64_t>(field->offset + fsz);
              i++;
            }
            if (c != nullptr &&
                (c->constant_for_base_class ||
                 c->constant_for_base_class_from_constexpr_folding ||
                 (c->kind == ck_aggregate && c->variant.aggregate.first_constant == nullptr))) {
              c = c->next;
            }
            field = field->next;
            continue;
          }
          if (field->is_bit_field && field->bit_size == 0) {
            field = field->next;
            continue;
          }
        } else {
          // Remaining st_ty elements are tail padding or trailing mock slots
          llvm::Type* rem_ty = st_ty->getElementType(i);
          if (c != nullptr && !(rem_ty->isArrayTy() && rem_ty->getArrayElementType()->isIntegerTy(8))) {
            llvm::Constant* rem_c = nullptr;
            LLVM_GEN_BE_CHECK(evaluate_constant(c, rem_ty, &rem_c));
            elems.push_back(rem_c);
            c = c->next;
          } else {
            elems.push_back(llvm::Constant::getNullValue(rem_ty));
          }
          i++;
          continue;
        }

        llvm::Type* elem_ty = st_ty->getElementType(i);
        a_type_ptr cur_field_ut = field->type ? skip_typerefs(field->type) : nullptr;
        if (!field->is_bit_field &&
            elem_ty->isArrayTy() && elem_ty->getArrayElementType()->isIntegerTy(8) &&
            (!cur_field_ut || cur_field_ut->kind != tk_array)) {
          elems.push_back(llvm::Constant::getNullValue(elem_ty));
          current_offset += dl.getTypeAllocSize(elem_ty);
          i++;
          continue;
        }
        if (field->is_bit_field) {
          llvm::Constant* packed_bf = nullptr;
          LLVM_GEN_BE_CHECK(llvm_pack_constant_bitfield_bytes(&field, &c, elem_ty, &packed_bf));
          elems.push_back(packed_bf);
          current_offset += dl.getTypeAllocSize(elem_ty);
          i++;
          continue;
        }
        a_targ_size_t field_size = cur_field_ut ? cur_field_ut->size : 0;
        uint64_t adv_size = (field_size > 0) ? static_cast<uint64_t>(field_size) : dl.getTypeAllocSize(elem_ty);
        if (c != nullptr) {
          if (c->kind == ck_init_repeat) {
            uint64_t count = static_cast<uint64_t>(c->variant.init_repeat.count);
            a_constant_ptr rep_src = c->variant.init_repeat.constant;
            c = c->next;
            for (uint64_t r = 0; r < count && i < st_ty->getNumElements(); ++r) {
              llvm::Type* cur_et = st_ty->getElementType(i);
              llvm::Constant* rep_const = nullptr;
              llvm_gen_be_error_t err = evaluate_constant(rep_src, cur_et, &rep_const);
              if (err != llvm_gen_be_error_t::ok) return err;
              elems.push_back(rep_const);
              if (field) {
                a_type_ptr rfut = field->type ? skip_typerefs(field->type) : nullptr;
                a_targ_size_t rfsz = rfut ? rfut->size : 0;
                current_offset += (rfsz > 0) ? static_cast<uint64_t>(rfsz) : dl.getTypeAllocSize(cur_et);
                field = field->next;
              }
              i++;
            }
            continue;
          }
          llvm::Constant* elem_const = nullptr;
          llvm_gen_be_error_t err = evaluate_constant(c, elem_ty, &elem_const);
          if (err != llvm_gen_be_error_t::ok) return err;
          elems.push_back(elem_const);
          c = c->next;
        } else {
          elems.push_back(llvm::Constant::getNullValue(elem_ty));
        }
        current_offset += adv_size;
        field = field->next;
        i++;
      }
      while (i < st_ty->getNumElements()) {
        elems.push_back(llvm::Constant::getNullValue(st_ty->getElementType(i)));
        i++;
      }
      *out_const = llvm::ConstantStruct::get(st_ty, elems);
      return llvm_gen_be_error_t::ok;
    }

    std::vector<llvm::Constant*> elems;
    unsigned i = 0;
    for (; c != nullptr; c = c->next) {
      if (i < st_ty->getNumElements()) {
        llvm::Constant* elem_const = nullptr;
        llvm_gen_be_error_t err = evaluate_constant(c, st_ty->getElementType(i), &elem_const);
        if (err != llvm_gen_be_error_t::ok) return err;
        elems.push_back(elem_const);
        i++;
      }
    }
    while (i < st_ty->getNumElements()) {
      elems.push_back(llvm::Constant::getNullValue(st_ty->getElementType(i)));
      i++;
    }
    *out_const = llvm::ConstantStruct::get(st_ty, elems);
    return llvm_gen_be_error_t::ok;
  }

  *out_const = llvm::Constant::getNullValue(expected_ty);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Dispatches an EDG constant node (`a_constant_ptr`) to the appropriate LLVM constant builder.
 * @details Supports integer, float, complex, imaginary, string, address, aggregate, pointer-to-member,
 *          label-difference, stack-offset, dynamic-init, and void constants.
 * @param[in] con Pointer to the EDG constant node (or nullptr for zero-initialization).
 * @param[in] expected_ty The expected LLVM type of the resulting constant.
 * @param[out] out_const Receives the evaluated `llvm::Constant*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t evaluate_constant(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!out_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  if (!con) {
    *out_const = llvm::Constant::getNullValue(expected_ty);
    return llvm_gen_be_error_t::ok;
  }

  switch (con->kind) {
    case ck_integer:
      return llvm_const_from_integer(con, expected_ty, out_const);
#if FIXED_POINT_ALLOWED
    case ck_fixed_point: {
      llvm::Type* fx_ty = expected_ty;
      if (!fx_ty || !fx_ty->isIntegerTy()) {
        LLVM_GEN_BE_CHECK(llvm_type_from_edg_type(con->type, &fx_ty));
      }
      *out_const = llvm::ConstantInt::get(
          fx_ty, EDG_INT_VAL_LOW64(con->variant.fixed_point_value));
      return llvm_gen_be_error_t::ok;
    }
#endif
    case ck_float:
      return llvm_const_from_float(con, expected_ty, out_const);
    case ck_complex:
      return llvm_const_from_complex(con, expected_ty, out_const);
    case ck_imaginary:
      return llvm_const_from_imaginary(con, expected_ty, out_const);
    case ck_string:
      return llvm_const_from_string(con, expected_ty, out_const);
    case ck_address:
      return llvm_const_from_address(con, expected_ty, out_const);
    case ck_aggregate:
      return llvm_const_from_aggregate(con, expected_ty, out_const);
    case ck_ptr_to_member:
      return llvm_const_from_ptr_to_member(con, expected_ty, out_const);
    case ck_label_difference:
      return llvm_const_from_label_difference(con, expected_ty, out_const);
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
    case ck_stack_offset:
      return llvm_const_from_stack_offset(con, expected_ty, out_const);
#endif
    case ck_dynamic_init:
      return llvm_const_from_dynamic_init(con, expected_ty, out_const);
    case ck_template_param:
      return llvm_const_from_template_param(con, expected_ty, out_const);
    case ck_init_repeat: {
      if (auto* arr_ty = llvm::dyn_cast<llvm::ArrayType>(expected_ty)) {
        uint64_t count = static_cast<uint64_t>(con->variant.init_repeat.count);
        uint64_t total_elems = arr_ty->getNumElements();
        llvm::Type* elem_ty = arr_ty->getElementType();
        llvm::Constant* rep_c = nullptr;
        LLVM_GEN_BE_CHECK(evaluate_constant(con->variant.init_repeat.constant, elem_ty, &rep_c));
        std::vector<llvm::Constant*> elems(total_elems, llvm::Constant::getNullValue(elem_ty));
        for (uint64_t ri = 0; ri < count && ri < total_elems; ++ri) {
          elems[ri] = rep_c;
        }
        *out_const = llvm::ConstantArray::get(arr_ty, elems);
        return llvm_gen_be_error_t::ok;
      }
      *out_const = llvm::Constant::getNullValue(expected_ty);
      return llvm_gen_be_error_t::ok;
    }
    case ck_void:
      *out_const = expected_ty->isVoidTy() ? nullptr : llvm::Constant::getNullValue(expected_ty);
      return llvm_gen_be_error_t::ok;
    default:
      *out_const = llvm::Constant::getNullValue(expected_ty);
      return llvm_gen_be_set_error(
          &be_state->err_context, llvm_gen_be_error_t::unsupported_expr,
          __FILE__, __LINE__, 0, "Unsupported constant kind %d",
          static_cast<int>(con->kind));
  }
}

END_EDG_NAMESPACE
#endif
