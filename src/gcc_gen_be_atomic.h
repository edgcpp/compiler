/**
 * @file gcc_gen_be_atomic.h
 * @brief C11 `_Atomic(T)` and generic `__atomic_*` / `libatomic` lowering for the GCC backend.
 * @details Declares helpers for C11 `TQ_C11_ATOMIC` qualified loads, stores, and read-modify-write
 * operations (including lock-free scalar builtins, floating-point CAS loops, and non-lock-free
 * struct `libatomic` forwarding), as well as generic `__atomic_load`, `__atomic_store`,
 * `__atomic_exchange`, and `__atomic_compare_exchange` lowering that writes `*expected` only on failure.
 * @ingroup gcc_gen_be
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_ATOMIC_H
#define GCC_GEN_BE_ATOMIC_H

#include "gcc_gen_be_error.h"
#include "basic_hdrs.h"
#include "il_def.h"

struct gcc_jit_lvalue;
struct gcc_jit_rvalue;

BEGIN_EDG_NAMESPACE

/**
 * @brief Queries whether an EDG type carries the C11 `_Atomic` (`TQ_C11_ATOMIC`) qualifier.
 * @details Walks any chain of `tk_typeref` nodes and sets `*out_is_atomic` to `TRUE` when
 * `(tp->variant.typeref.qualifiers & TQ_C11_ATOMIC) != 0`.
 * @param[in] tp The EDG type pointer to inspect (may be NULL).
 * @param[out] out_is_atomic Pointer receiving `TRUE` if `tp` is C11 `_Atomic`-qualified, else `FALSE`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if `out_is_atomic` is NULL.
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t gcc_gen_be_is_c11_atomic_type(
    a_type_ptr tp,
    a_boolean *out_is_atomic) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a sequentially consistent (`__ATOMIC_SEQ_CST`) load from a C11 `_Atomic(T)` lvalue.
 * @details Uses lock-free `__atomic_load_1/2/4/8` builtins for 1/2/4/8-byte scalar/pointer types
 * and forwards arbitrary-width / struct `_Atomic(T)` loads to `libatomic` `__atomic_load`.
 * @param[in] expr_type The EDG result type of the loaded value.
 * @param[in] addr_lval The addressable `gcc_jit_lvalue` of the `_Atomic(T)` object.
 * @param[out] out_rval Pointer receiving the loaded `gcc_jit_rvalue`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t gcc_gen_be_lower_c11_atomic_load(
    a_type_ptr expr_type,
    struct gcc_jit_lvalue *addr_lval,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a sequentially consistent (`__ATOMIC_SEQ_CST`) store to a C11 `_Atomic(T)` lvalue.
 * @details Evaluates `rhs_rval` once, emits `__atomic_store_1/2/4/8` for lock-free widths or
 * `libatomic` `__atomic_store` for non-lock-free structs, and outputs the stored value.
 * @param[in] lhs_type The `_Atomic(T)` destination EDG type.
 * @param[in] addr_lval The addressable `gcc_jit_lvalue` of the `_Atomic(T)` object.
 * @param[in] rhs_rval The value to store.
 * @param[out] out_rval Pointer receiving the stored `gcc_jit_rvalue` expression result.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t gcc_gen_be_lower_c11_atomic_store(
    a_type_ptr lhs_type,
    struct gcc_jit_lvalue *addr_lval,
    struct gcc_jit_rvalue *rhs_rval,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a C11 `_Atomic(T)` compound assignment or pre/post increment/decrement RMW operation.
 * @details Uses native `__atomic_fetch_*` / `__atomic_*_fetch` builtins for lock-free integer/pointer
 * `+=`, `-=`, `&=`, `|=`, `^=`, `++`, `--` operations, and emits an atomic compare-exchange loop for
 * floating-point or general compound-assignment operators.
 * @param[in] op The EDG assignment or increment/decrement operator kind (`eok_add_assign`, `eok_pre_incr`, etc.).
 * @param[in] lhs_type The `_Atomic(T)` destination EDG type.
 * @param[in] addr_lval The addressable `gcc_jit_lvalue` of the `_Atomic(T)` object.
 * @param[in] rhs_rval The RHS operand rvalue (may be NULL for unary `++`/`--`).
 * @param[out] out_rval Pointer receiving the pre-RMW (post-inc/dec) or post-RMW rvalue result.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t gcc_gen_be_lower_c11_atomic_rmw(
    an_expr_operator_kind op,
    a_type_ptr lhs_type,
    struct gcc_jit_lvalue *addr_lval,
    struct gcc_jit_rvalue *rhs_rval,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers generic `__atomic_load`, `__atomic_store`, `__atomic_exchange`, and `__atomic_compare_exchange`.
 * @details Supports both EDG front-end `size_t`-prepended generic calls and direct user-arity calls,
 * dispatching lock-free sizes (`1`, `2`, `4`, `8`) to native `libgccjit` atomic builtins and forwarding
 * non-lock-free / struct sizes to `libatomic` while writing `*expected` only on failure.
 * @param[in] name The builtin name (`"__atomic_load"`, `"__atomic_compare_exchange"`, etc.).
 * @param[in] rout The EDG routine descriptor (may be NULL in synthetic unit tests).
 * @param[in] num_args Number of lowered argument rvalues in `args`.
 * @param[in] args Array of lowered argument rvalues.
 * @param[in] explicit_size_bytes Explicit object byte width (`0` to infer from `rout` or `args`).
 * @param[out] out_rval Pointer receiving the lowered call result rvalue (`NULL` for `void` calls).
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t gcc_gen_be_lower_generic_atomic_call(
    const char *name,
    a_routine_ptr rout,
    int num_args,
    struct gcc_jit_rvalue **args,
    unsigned long explicit_size_bytes,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_ATOMIC_H */
