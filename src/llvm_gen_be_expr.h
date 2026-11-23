/**
 * @defgroup llvm_gen_be_expr Expression, Atomic, Complex & Builtin Lowering
 * @ingroup llvm_gen_be
 * @brief Expression, atomic, complex, decimal floating-point, and compiler builtin lowering.
 */

/**
 * @file llvm_gen_be_expr.h
 * @ingroup llvm_gen_be_expr
 * @brief Expression lowering subsystem for the EDG LLVM backend.
 * @details Translates EDG front-end expression nodes (an_expr_node_ptr) into LLVM IR
 * values (llvm::Value*). All functions return llvm_gen_be_error_t and output
 * constructed values via output pointers.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_EXPR_H
#define LLVM_GEN_BE_EXPR_H 1

#include "basic_hdrs.h"
#include "fe_common.h"
#include "llvm_gen_be_error.h"
#include <vector>
#include <llvm/IR/Value.h>

BEGIN_EDG_NAMESPACE

/**
 * @brief Emits a conditional UndefinedBehaviorSanitizer (UBSan) runtime handler call when @p cond is true.
 * @details Splits the current basic block into a `ubsan.fail` block that invokes @p handler_name
 *          with a source descriptor pointer and @p handler_args, followed by a `ubsan.cont` continuation block.
 * @param[in] handler_name Null-terminated name of the `__ubsan_handle_*` runtime function.
 * @param[in] cond Boolean fault condition (`i1` `llvm::Value*`).
 * @param[in] handler_args Additional `llvm::Value*` arguments passed to the UBSan handler after the descriptor pointer.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `handler_name` or `cond` is null.
 */
llvm_gen_be_error_t llvm_emit_ubsan_check(
    const char* handler_name,
    llvm::Value* cond,
    const std::vector<llvm::Value*>& handler_args) noexcept;

/**
 * @brief Emits a structured UndefinedBehaviorSanitizer (UBSan) `TypeDescriptor` global constant.
 * @details Constructs `{ i16 TypeKind, i16 TypeInfo, [N x i8] TypeName }` describing @p edg_ty
 *          for UBSan type-mismatch, overflow, and float-cast-overflow diagnostics.
 * @param[in] edg_ty Pointer to the EDG type to describe (may be nullptr for unknown type).
 * @param[out] out_desc Receives the created `llvm::GlobalVariable*` for the UBSan `TypeDescriptor`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if @p out_desc is null.
 */
llvm_gen_be_error_t llvm_emit_ubsan_type_descriptor(
    a_type_ptr edg_ty,
    llvm::GlobalVariable** out_desc) noexcept;

/**
 * @brief Lowers an arbitrary EDG expression node (`an_expr_node_ptr`) into an `llvm::Value*`.
 * @details Dispatches constants, variables, operations, conditions, temporaries, `new`/`delete`,
 *          `throw`, `typeid`, `dynamic_cast`, coroutines, and statement expressions.
 * @param[in] expr Pointer to the EDG expression node to lower.
 * @param[out] out_val Receives the lowered `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers an lvalue expression into its corresponding memory address (`llvm::Value*`).
 * @details Handles local/global variables, array subscripts (including VLAs), struct/union field
 *          selections, pointer-to-member field accesses, dereferences, and complex real/imag parts.
 * @param[in] expr Pointer to the EDG lvalue expression node.
 * @param[out] out_ptr Receives the resulting address pointer value.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_lvalue_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_ptr) noexcept;

/**
 * @brief Lowers an arithmetic, bitwise, pointer arithmetic, or complex arithmetic expression.
 * @details Supports integer, floating-point, vector, pointer, and C99 `_Complex` operations.
 * @param[in] expr Pointer to the EDG arithmetic operation expression node.
 * @param[out] out_val Receives the computed `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_arithmetic_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a logical (`&&`, `||`, `!`) or relational (`==`, `!=`, `<`, `<=`, `>`, `>=`, `<=>`) expression.
 * @details Emits short-circuit basic blocks and PHI nodes for `&&`/`||`, three-way comparison selects for `<=>`,
 *          and `icmp`/`fcmp` instructions for scalar, pointer, and vector comparisons.
 * @param[in] expr Pointer to the EDG logical/relational expression node.
 * @param[out] out_val Receives the computed boolean or integer comparison result.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_logical_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a type cast expression (`eok_cast`, `eok_Pass_by_value`, etc.) into LLVM IR.
 * @details Handles int-to-ptr, ptr-to-int, fpext/fptrunc, fptosi/fptoui, sitofp/uitofp, sext/zext/trunc, and bitcast.
 * @param[in] expr Pointer to the EDG cast expression node.
 * @param[out] out_val Receives the converted `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_cast_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a direct, indirect, virtual, or builtin function call expression into LLVM IR.
 * @details Handles target ABI parameter/return lowering (`sret`, `byval`, register coercion),
 *          compiler builtins, C11/C++11 atomics, virtual dispatch, and `invoke` unwinding.
 * @param[in] expr Pointer to the EDG call expression node.
 * @param[out] out_val Receives the call return `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_call_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a compound assignment (`+=`, `-=`, `*=`, etc.) or increment/decrement (`++`, `--`) expression.
 * @details Evaluates the target lvalue address once, loads the current value (supporting volatile and bitfields),
 *          computes the updated value, stores it back, and returns either the pre- or post-update value.
 * @param[in] expr Pointer to the EDG compound assignment or increment/decrement expression node.
 * @param[out] out_val Receives the resulting `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_compound_assignment(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers an EDG builtin operation node (`enk_builtin_operation`, `a_builtin_operation_kind`) into LLVM IR.
 * @details Supports `bok_offsetof`, `bok_builtin_bit_cast`, `bok_builtin_has_attribute`, `bok_types_compatible`,
 *          `bok_classify_type`, `bok_builtin_launder`, `bok_builtin_complex`, `bok_builtin_addressof`,
 *          `bok_builtin_source_location`, `bok_builtin_shuffle`, `bok_builtin_convertvector`, `bok_uuidof`,
 *          and runtime fallbacks for compiler type-trait `bok_*` operations.
 * @param[in] expr Pointer to the EDG `enk_builtin_operation` expression node.
 * @param[out] out_val Receives the lowered `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_builtin_operation(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a GNU `__builtin_choose_expr` node (`enk_builtin_choose_expr`) into LLVM IR.
 * @details Evaluates either the second or third operand of `expr->variant.builtin_choose_expr`
 *          depending on `choose_first`.
 * @param[in] expr Pointer to the EDG `enk_builtin_choose_expr` expression node.
 * @param[out] out_val Receives the lowered `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if operands are missing.
 */
llvm_gen_be_error_t llvm_lower_builtin_choose_expr(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers an ISO Embedded C fixed-point binary arithmetic operation using LLVM intrinsics.
 * @details Supports saturating (`llvm.sadd.sat`, `llvm.uadd.sat`, `llvm.ssub.sat`, `llvm.usub.sat`,
 *          `llvm.smul.fix.sat`, `llvm.umul.fix.sat`) and non-saturating (`llvm.smul.fix`, `llvm.umul.fix`,
 *          `llvm.sdiv.fix`, `llvm.udiv.fix`, `add`, `sub`) fixed-point arithmetic.
 * @param[in] op_kind The EDG binary expression operator kind (`eok_add`, `eok_subtract`, `eok_multiply`, `eok_divide`).
 * @param[in] is_signed Whether the fixed-point type is signed.
 * @param[in] is_saturating Whether the fixed-point type is saturating (`_Sat`).
 * @param[in] scale Fractional bit scale factor for fixed-point multiplication/division.
 * @param[in] lhs Lowered left-hand integer representation operand.
 * @param[in] rhs Lowered right-hand integer representation operand.
 * @param[out] out_val Receives the computed fixed-point result value.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_fixed_point_binop(
    an_expr_operator_kind op_kind,
    bool is_signed,
    bool is_saturating,
    unsigned scale,
    llvm::Value* lhs,
    llvm::Value* rhs,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a C++ `new` or `new[]` expression (`enk_new_delete` or unlowered `eok_new`).
 * @details Computes allocation size (including array cookies and overflow checks), invokes `operator new`,
 *          initializes the storage via `llvm_emit_dynamic_init`, and registers exception cleanup.
 * @param[in] expr Pointer to the EDG `new` expression node.
 * @param[out] out_val Receives the allocated and initialized pointer `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_new_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a C++ `delete` or `delete[]` expression (`enk_new_delete` or unlowered `eok_delete`).
 * @details Emits a null check, runs scalar or reverse-order array element destructors (adjusting for
 *          array cookies), and invokes `operator delete`.
 * @param[in] expr Pointer to the EDG `delete` expression node.
 * @param[out] out_val Receives a null pointer `llvm::Value*` representing the `void` result.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_delete_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a C++11 `sizeof...(pack)` expression (`enk_sizeof_pack`).
 * @details Extracts the resolved pack element count from the EDG expression node and returns an integer constant.
 * @param[in] expr Pointer to the EDG `enk_sizeof_pack` expression node.
 * @param[out] out_val Receives the `llvm::ConstantInt*` pack count.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_sizeof_pack(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers a C++17 fold expression (`enk_fold`).
 * @details Evaluates unary and binary left/right folds across arithmetic, bitwise, shift, relational,
 *          short-circuit logical (`&&`, `||`), assignment, pointer-to-member, and comma operators.
 * @param[in] expr Pointer to the EDG `enk_fold` expression node.
 * @param[out] out_val Receives the folded `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_fold_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept;

/**
 * @brief Computes a per-byte padding mask (`0x00` = padding byte, `0xFF` = value byte, or bitmask for bitfields) for an EDG type.
 * @details Marks all non-padding field, bitfield, and base-class bytes in @p out_mask so that
 *          `__builtin_clear_padding` zeroes only padding bytes and bits.
 * @param[in] type Pointer to the EDG type to inspect.
 * @param[out] out_mask Receives a byte vector of length `type->size` where `1` bits represent active value bits and `0` bits represent padding.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_mask` is null.
 */
llvm_gen_be_error_t llvm_compute_type_padding_mask(
    a_type_ptr type,
    std::vector<uint8_t>* out_mask) noexcept;

/**
 * @brief Lowers an IEEE-754 decimal floating-point binary arithmetic operation (`+`, `-`, `*`, `/`) via `libgcc` `__bid_*` / `__dpd_*` runtime calls.
 * @details Selects `__bid_*` (x86/ARM) or `__dpd_*` (PowerPC/SystemZ) runtime helper for `_Decimal32` (`sd`), `_Decimal64` (`dd`), or `_Decimal128` (`td`).
 * @param[in] byte_width Byte width of the decimal float (`4`, `8`, or `16`).
 * @param[in] op_kind Binary operator kind (`eok_add`, `eok_subtract`, `eok_multiply`, `eok_divide`).
 * @param[in] lhs Lowered left-hand carrier operand (`i32`, `i64`, or `i128`).
 * @param[in] rhs Lowered right-hand carrier operand (`i32`, `i64`, or `i128`).
 * @param[out] out_val Receives the resulting decimal float carrier value.
 * @return `llvm_gen_be_error_t::ok` on success, or an error code on invalid arguments/operator.
 */
llvm_gen_be_error_t llvm_lower_decimal_float_binop(
    unsigned byte_width,
    an_expr_operator_kind op_kind,
    llvm::Value* lhs,
    llvm::Value* rhs,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers an IEEE-754 decimal floating-point comparison (`==`, `!=`, `<`, `<=`, `>`, `>=`) via `libgcc` `__bid_*` / `__dpd_*` runtime calls.
 * @details Calls `__bid_eqsd2` / `__dpd_eqsd2` (etc.), compares the `i32` status result against `0`, and zero-extends to @p result_ty.
 * @param[in] byte_width Byte width of the decimal float (`4`, `8`, or `16`).
 * @param[in] op_kind Relational operator kind (`eok_eq`, `eok_ne`, `eok_lt`, `eok_le`, `eok_gt`, `eok_ge`).
 * @param[in] lhs Lowered left-hand carrier operand.
 * @param[in] rhs Lowered right-hand carrier operand.
 * @param[in] result_ty Target boolean/integer result LLVM type (e.g. `i8` or `i32`).
 * @param[out] out_val Receives the comparison result value.
 * @return `llvm_gen_be_error_t::ok` on success, or an error code on invalid arguments/operator.
 */
llvm_gen_be_error_t llvm_lower_decimal_float_cmp(
    unsigned byte_width,
    an_expr_operator_kind op_kind,
    llvm::Value* lhs,
    llvm::Value* rhs,
    llvm::Type* result_ty,
    llvm::Value** out_val) noexcept;

/**
 * @brief Lowers conversions between IEEE-754 decimal floating-point types and decimal/binary-float/integer types via `libgcc` `__bid_*` / `__dpd_*` runtime calls.
 * @details Supports widening/narrowing between `_Decimal32`/`_Decimal64`/`_Decimal128`, conversions to/from `float`/`double`/`x86_fp80`/`fp128`, and signed/unsigned integer conversions.
 * @param[in] src_dec_bytes Source decimal float byte width (`4`, `8`, `16`, or `0` if source is binary float / integer).
 * @param[in] dst_dec_bytes Destination decimal float byte width (`4`, `8`, `16`, or `0` if destination is binary float / integer).
 * @param[in] int_is_unsigned True when converting from/to an unsigned integer type.
 * @param[in] src_val Lowered source operand value.
 * @param[in] dest_ty Target LLVM type.
 * @param[out] out_val Receives the converted LLVM value.
 * @return `llvm_gen_be_error_t::ok` on success, or an error code on invalid conversion widths/types.
 */
llvm_gen_be_error_t llvm_lower_decimal_float_cast(
    unsigned src_dec_bytes,
    unsigned dst_dec_bytes,
    bool int_is_unsigned,
    llvm::Value* src_val,
    llvm::Type* dest_ty,
    llvm::Value** out_val) noexcept;

END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_EXPR_H */
