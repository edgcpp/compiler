/**
 * @file test_gcc_be_expr_common.h
 * @brief Shared mock state and IL builder helpers for GCC backend expression unit tests.
 * @details Provides deterministic mock configuration for type, ABI, declaration, statement,
 * coroutine, exception-handling, atomic, and RTTI hooks together with OOM budget sweepers
 * and EDG IL node construction helpers used across `test_gcc_be_expr*.cpp`.
 * @ingroup gcc_gen_be_tests
 */

#ifndef TEST_GCC_BE_EXPR_COMMON_H
#define TEST_GCC_BE_EXPR_COMMON_H

#include "basic_hdrs.h"
#include "gcc_gen_be_expr.h"
#include "gcc_gen_be_atomic.h"
#include "gcc_gen_be_simd.h"
#include "gcc_gen_be_rtti.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_decl.h"
#include "gcc_gen_be_stmt.h"
#include "gcc_gen_be_abi.h"
#include "gcc_gen_be_eh.h"
#include "gcc_gen_be_builtin.h"
#include "gcc_gen_be_location.h"
#include "gcc_gen_be_lib_loader.h"
#include "gcc_gen_be_error.h"
#include "const_ints.h"
#include "float_pt.h"
#include <libgccjit.h>
#include <cstdio>
#include <cstring>
#include <type_traits>

USING_NAMESPACE_EDG

BEGIN_EDG_NAMESPACE

/**
 * @brief Entry mapping an EDG `a_type_ptr` to an explicit `gcc_jit_type *` override.
 */
struct expr_type_override_t {
    /** @brief Frontend type key. */
    a_type_ptr edg_type;
    /** @brief Mapped `libgccjit` type value (may be NULL to force NULL type lowering). */
    gcc_jit_type *jit_type;
};

/**
 * @brief Entry mapping an EDG `a_constant_ptr` to a deterministic integer constant value.
 */
struct expr_const_int_entry_t {
    /** @brief Frontend constant node pointer key. */
    a_constant_ptr con;
    /** @brief Signed 64-bit integer value associated with `con`. */
    a_host_large_integer val;
};

/**
 * @brief Shared deterministic mock state controlling stubbed backend hooks in expression unit tests.
 */
struct expr_mock_state_t {
    /** @brief Custom `lower_type` override table. */
    expr_type_override_t type_overrides[256];
    /** @brief Number of active entries in `type_overrides`. */
    int num_type_overrides;
    /** @brief Per-constant integer value table for multi-constant expressions. */
    expr_const_int_entry_t const_ints[256];
    /** @brief Number of active entries in `const_ints`. */
    int num_const_ints;
    /** @brief When `TRUE`, struct lowering skips populating `GCC_GEN_BE_CACHE_FIELD` entries. */
    a_boolean skip_auto_cache_fields;
    /** @brief When non-zero, `gcc_gen_be_lower_type` returns this error code. */
    gcc_gen_be_error_t force_lower_type_err;
    /** @brief When `TRUE`, `tk_complex` lowers to a 2-field struct instead of native complex. */
    a_boolean complex_as_struct;
    /** @brief Cached 2-field complex struct `{double re, im}` for current context. */
    gcc_jit_type *cplx_struct_type;
    /** @brief Cached 2-field PMF struct `{void *ptr, long adj}` for current context. */
    gcc_jit_type *pmf_struct_type;
    /** @brief Cached 1-field wrapper struct `{int v}` for current context. */
    gcc_jit_type *one_field_struct_type;

    /** @brief When `TRUE`, `gcc_gen_be_get_zero_rvalue` sets `*out_rval = NULL`. */
    a_boolean force_zero_null;
    /** @brief When non-zero, `gcc_gen_be_get_zero_rvalue` returns this error code. */
    gcc_gen_be_error_t force_zero_err;

    /** @brief Primary EDG type recognized as C11 `_Atomic(T)` by `gcc_gen_be_is_c11_atomic_type`. */
    a_type_ptr atomic_target_tp;
    /** @brief Secondary EDG type recognized as C11 `_Atomic(T)`. */
    a_type_ptr atomic_target_tp2;

    /** @brief Active target ABI returned by `gcc_gen_be_get_target_abi`. */
    gcc_gen_be_target_abi_t target_abi;
    /** @brief `out_is_stack` returned by `gcc_gen_be_classify_va_arg`. */
    a_boolean va_is_stack;
    /** @brief `out_num_gp` returned by `gcc_gen_be_classify_va_arg`. */
    int va_num_gp;
    /** @brief `out_num_fp` returned by `gcc_gen_be_classify_va_arg`. */
    int va_num_fp;
    /** @brief First SysV eightbyte class returned by `gcc_gen_be_classify_sysv_amd64`. */
    gcc_gen_be_abi_class_t sysv_class0;
    /** @brief Second SysV eightbyte class returned by `gcc_gen_be_classify_sysv_amd64`. */
    gcc_gen_be_abi_class_t sysv_class1;

    /** @brief Flag returned by `gcc_gen_be_is_returned_via_sret`. */
    a_boolean is_sret;
    /** @brief Specific `a_type_ptr` passed by invisible reference (or all when `invis_ref_all` is `TRUE`). */
    a_type_ptr invis_ref_tp;
    /** @brief When `TRUE`, `gcc_gen_be_is_passed_by_invisible_reference` returns `TRUE`. */
    a_boolean invis_ref_all;
    /** @brief Coerced ABI type returned by `gcc_gen_be_get_coerced_abi_type`. */
    gcc_jit_type *coerced_abi_type;
    /** @brief Flag returned by `gcc_gen_be_get_coerced_abi_type`. */
    a_boolean needs_abi_coercion;
    /** @brief Optional coerced rvalue returned by `gcc_gen_be_coerce_Arg_to_registers`. */
    gcc_jit_rvalue *coerced_arg_rval;
    /** @brief When `TRUE`, `gcc_gen_be_coerce_Arg_to_registers` sets `*out_rval = NULL`. */
    a_boolean coerce_arg_return_null;
    /** @brief Error returned by `gcc_gen_be_coerce_Arg_to_registers` when non-zero. */
    gcc_gen_be_error_t force_coerce_arg_err;
    /** @brief Optional coerced rvalue returned by `gcc_gen_be_coerce_rvalue`. */
    gcc_jit_rvalue *coerced_rval_override;
    /** @brief When `TRUE`, `gcc_gen_be_coerce_rvalue` sets `*out_rval = NULL`. */
    a_boolean coerce_rval_return_null;
    /** @brief Error returned by `gcc_gen_be_coerce_rvalue` when non-zero. */
    gcc_gen_be_error_t force_coerce_rval_err;
    /** @brief Error returned by `gcc_gen_be_get_coerced_abi_type` when non-zero. */
    gcc_gen_be_error_t force_get_coerced_abi_err;

    /** @brief Pre-supplied function returned by `gcc_gen_be_lower_function_decl`. */
    gcc_jit_function *mock_decl_func;
    /** @brief Secondary function returned on the 2nd `gcc_gen_be_lower_function_decl` call when non-NULL. */
    gcc_jit_function *mock_decl_func2;
    /** @brief Call counter for `gcc_gen_be_lower_function_decl`. */
    int func_decl_calls;
    /** @brief When `TRUE`, `gcc_gen_be_lower_function_decl` succeeds with `*out_func = NULL`. */
    a_boolean func_decl_return_null;
    /** @brief Error returned by `gcc_gen_be_lower_function_decl` when non-zero. */
    gcc_gen_be_error_t force_func_decl_err;
    /** @brief Pre-supplied global lvalue returned by `gcc_gen_be_lower_global_variable_decl`. */
    gcc_jit_lvalue *mock_global_lval;
    /** @brief When `TRUE`, `gcc_gen_be_lower_global_variable_decl` succeeds with `*out_lval = NULL`. */
    a_boolean global_var_return_null;
    /** @brief Error returned by `gcc_gen_be_lower_global_variable_decl` when non-zero. */
    gcc_gen_be_error_t force_global_decl_err;
    /** @brief Pre-supplied local lvalue returned by `gcc_gen_be_lower_local_variable_decl`. */
    gcc_jit_lvalue *mock_local_lval;
    /** @brief When `TRUE`, `gcc_gen_be_lower_local_variable_decl` succeeds with `*out_lval = NULL`. */
    a_boolean local_var_return_null;
    /** @brief Pre-supplied label block returned by `gcc_gen_be_get_or_create_label_block`. */
    gcc_jit_block *mock_label_block;
    /** @brief When `TRUE`, `gcc_gen_be_get_or_create_label_block` sets `*out_block = NULL`. */
    a_boolean label_return_null;

    /** @brief Pre-supplied vtable lvalue returned by `gcc_gen_be_emit_vtable`. */
    gcc_jit_lvalue *mock_vtable_lval;
    /** @brief When `TRUE`, `gcc_gen_be_emit_vtable` succeeds with `*out_vtable = NULL`. */
    a_boolean vtable_return_null;
    /** @brief Pre-supplied VTT lvalue returned by `gcc_gen_be_emit_vtt`. */
    gcc_jit_lvalue *mock_vtt_lval;
    /** @brief When `TRUE`, `gcc_gen_be_emit_vtt` succeeds with `*out_vtt = NULL`. */
    a_boolean vtt_return_null;
    /** @brief Error returned by `gcc_gen_be_emit_vtt` when non-zero. */
    gcc_gen_be_error_t force_vtt_err;

    /** @brief When `TRUE`, `gcc_gen_be_emit_unwind_resume` creates and returns a continuation block. */
    a_boolean unwind_resume_make_block;
    /** @brief When `TRUE`, `gcc_gen_be_emit_throw` creates and returns a continuation block. */
    a_boolean emit_throw_make_block;
    /** @brief Error returned by `gcc_gen_be_emit_invoke_call` when non-zero. */
    gcc_gen_be_error_t force_invoke_err;
    /** @brief Error returned by `gcc_gen_be_emit_invoke_call_through_ptr` when non-zero. */
    gcc_gen_be_error_t force_invoke_ptr_err;

    /** @brief Active coroutine frame type returned by `gcc_gen_be_get_active_coroutine_frame`. */
    gcc_jit_type *coro_frame_type;
    /** @brief Active coroutine frame pointer lvalue returned by `gcc_gen_be_get_active_coroutine_frame`. */
    gcc_jit_lvalue *coro_frame_lval;
    /** @brief State index returned by `gcc_gen_be_register_coro_suspend_point`. */
    int coro_suspend_idx;
    /** @brief Coroutine state field lvalue returned by `gcc_gen_be_register_coro_suspend_point`. */
    gcc_jit_lvalue *coro_state_field_lval;
    /** @brief Coroutine frame pointer rvalue returned by `gcc_gen_be_register_coro_suspend_point`. */
    gcc_jit_rvalue *coro_frame_ptr_rval;

    /** @brief Pre-supplied rvalue returned by `gcc_gen_be_lower_builtin_call` / `generic_atomic_call`. */
    gcc_jit_rvalue *mock_builtin_rval;
    /** @brief Error returned by `gcc_gen_be_lower_builtin_call` when non-zero. */
    gcc_gen_be_error_t force_builtin_err;

    /** @brief Signed integer value returned by `value_of_integer_constant`. */
    a_host_large_integer signed_int_val;
    /** @brief Overflow flag set by `value_of_integer_constant`. */
    a_boolean signed_int_ovf;
    /** @brief Unsigned integer value returned by `unsigned_value_of_integer_constant`. */
    a_host_large_unsigned unsigned_int_val;
    /** @brief Overflow flag set by `unsigned_value_of_integer_constant`. */
    a_boolean unsigned_int_ovf;
    /** @brief Signedness flag returned by `int_constant_is_signed`. */
    a_boolean int_is_signed;
    /** @brief Low 64-bit chunk returned on first `unsigned_value_of_integer_value` call. */
    a_host_large_unsigned int128_lo;
    /** @brief High 64-bit chunk returned on shifted `unsigned_value_of_integer_value` call. */
    a_host_large_unsigned int128_hi;
    /** @brief Tracks whether `shift_right_integer_value` was invoked during 128-bit extraction. */
    a_boolean shifted_128;
    /** @brief Floating-point constant value returned by `fetch_host_fp_value`. */
    double host_fp_val;
    /** @brief Optional routine scope returned by `scope_for_routine_or_null`. */
    a_scope_ptr mock_routine_scope;
};

/** @brief Global mock state instance shared across expression unit test translation units. */
extern expr_mock_state_t g_expr_mock;

/** @brief Global exceptions-enabled flag declared in EDG front-end state. */
extern a_boolean exceptions_enabled;

/**
 * @brief Resets `g_expr_mock` to clean defaults while preserving active context helper types.
 * @details Clears overrides, error injectors, ABI flags, and coroutine/atomic targets.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
GCC_GEN_BE_NODISCARD gcc_gen_be_error_t expr_test_reset_mock(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Registers a custom `a_type_ptr -> gcc_jit_type *` mapping in `g_expr_mock`.
 * @details Appends `(edg_tp, jit_tp)` to `g_expr_mock.type_overrides`.
 * @param[in] edg_tp Frontend type pointer key.
 * @param[in] jit_tp Target `libgccjit` type pointer (may be NULL).
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INTERNAL` if full.
 */
GCC_GEN_BE_NODISCARD gcc_gen_be_error_t expr_test_map_type(
    a_type_ptr edg_tp,
    gcc_jit_type *jit_tp) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Registers a deterministic integer constant value for `con` in `g_expr_mock.const_ints`.
 * @details Allows multi-constant expressions (aggregates, shufflevector masks) to yield distinct values.
 * @param[in] con Frontend constant pointer key.
 * @param[in] val Signed integer value to associate with `con`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INTERNAL` if full.
 */
GCC_GEN_BE_NODISCARD gcc_gen_be_error_t expr_test_map_const_int(
    a_constant_ptr con,
    a_host_large_integer val) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Acquires a fresh `gcc_jit_context`, creates an active test function and entry block, and resets caches.
 * @details Sets `ctx` and `entry_blk` on `gcc_gen_be_singleton_state` and initializes PMF/complex/wrapper helper types.
 * @param[out] out_ctx Receives the acquired `gcc_jit_context *`.
 * @param[out] out_fn Receives the created `gcc_jit_function *`.
 * @param[out] out_blk Receives the active entry `gcc_jit_block *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD gcc_gen_be_error_t expr_test_setup_fn(
    gcc_jit_context **out_ctx,
    gcc_jit_function **out_fn,
    gcc_jit_block **out_blk) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Creates and activates a fresh basic block in `fn` so block-terminating expressions can run repeatedly.
 * @details Calls `gcc_jit_function_new_block` and `gcc_gen_be_set_current_block`.
 * @param[in] fn Owning `gcc_jit_function *`.
 * @param[out] out_blk Optional pointer receiving the newly created active block.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD gcc_gen_be_error_t expr_test_fresh_block(
    gcc_jit_function *fn,
    gcc_jit_block **out_blk = NULL) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Releases the active test `gcc_jit_context` and clears `gcc_gen_be` singleton/cache state.
 * @details Resets OOM countdown, landing-pad stack, scopes, active block/context, and caches.
 * @param[in] ctx Context acquired by `expr_test_setup_fn`.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
GCC_GEN_BE_NODISCARD gcc_gen_be_error_t expr_test_teardown_fn(
    gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT;

/** @brief Callback signature for deterministic OOM budget sweeps in expression unit tests. */
typedef gcc_gen_be_error_t (*expr_oom_step_fn_t)(void *user_data) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Sweeps OOM countdown budgets `0 .. max_budget` until `step_fn` succeeds.
 * @details Resets the active block to a fresh block in `fn` (when non-NULL) before each budget attempt
 * and exercises every `GCC_GEN_BE_CHECK` error-percolation branch along `step_fn`.
 * @param[in] fn Active `gcc_jit_function *` used to allocate a clean block per iteration (may be NULL).
 * @param[in] max_budget Maximum OOM step budget to test.
 * @param[in] step_fn Callback under test.
 * @param[in] user_data Opaque context pointer passed to `step_fn`.
 * @return `GCC_GEN_BE_SUCCESS` once `step_fn` succeeds within `max_budget`, or an error code.
 */
GCC_GEN_BE_NODISCARD gcc_gen_be_error_t expr_test_sweep_oom(
    gcc_jit_function *fn,
    int max_budget,
    expr_oom_step_fn_t step_fn,
    void *user_data) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Type-erased callback adapter forwarding `ud` to callable `Fn`.
 * @details Invokes `(*static_cast<Fn *>(ud))()` for `expr_test_sweep_oom`.
 * @tparam Fn Callable type taking zero arguments and returning `gcc_gen_be_error_t`.
 * @param[in] ud Opaque pointer to the `Fn` instance.
 * @return Result of invoking `*static_cast<Fn *>(ud)`.
 */
template <typename Fn>
GCC_GEN_BE_NODISCARD inline gcc_gen_be_error_t expr_sweep_oom_thunk(void *ud) GCC_GEN_BE_NOEXCEPT {
    return (*static_cast<Fn *>(ud))();
}

/**
 * @brief Invokes a `noexcept` callable across OOM countdown budgets `0 .. max_budget`.
 * @details Adapts any `noexcept` lambda or functor returning `gcc_gen_be_error_t` to `expr_test_sweep_oom`.
 * @tparam Fn Callable type taking zero arguments and returning `gcc_gen_be_error_t`.
 * @param[in] fn Active `gcc_jit_function *` used to allocate a clean block per iteration (may be NULL).
 * @param[in] max_budget Maximum OOM step budget to test.
 * @param[in] step Callable under test.
 * @return `GCC_GEN_BE_SUCCESS` once `step` succeeds within `max_budget`, or an error code.
 */
template <typename Fn>
GCC_GEN_BE_NODISCARD inline gcc_gen_be_error_t expr_sweep_oom_fn(
    gcc_jit_function *fn,
    int max_budget,
    Fn &&step) GCC_GEN_BE_NOEXCEPT {
    return expr_test_sweep_oom(fn, max_budget, &expr_sweep_oom_thunk<typename std::remove_reference<Fn>::type>, static_cast<void *>(&step));
}

/**
 * @brief Sweeps OOM budgets on `gcc_gen_be_lower_expr_lvalue` while preserving `cblock == NULL` state.
 * @param[in] expr Frontend expression node to lower as an lvalue.
 * @param[out] out_lv Output lvalue slot.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD inline gcc_gen_be_error_t expr_sweep_lval_oom(
    an_expr_node_ptr expr,
    gcc_jit_lvalue **out_lv) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_block *orig_blk = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&orig_blk));
    gcc_jit_function *owner_fn = orig_blk ? gcc_jit_block_get_function(orig_blk) : NULL;
    return expr_sweep_oom_fn(owner_fn, 128, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_expr_lvalue(expr, out_lv);
    });
}

/**
 * @brief Sweeps OOM budgets on `gcc_gen_be_lower_constant_rvalue` while preserving `cblock == NULL` state.
 * @param[in] con Frontend constant node to lower.
 * @param[in] expected_type Expected destination `gcc_jit_type *`.
 * @param[out] out_rv Output rvalue slot.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD inline gcc_gen_be_error_t expr_sweep_const_oom(
    a_constant_ptr con,
    gcc_jit_type *expected_type,
    gcc_jit_rvalue **out_rv) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_block *orig_blk = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&orig_blk));
    gcc_jit_function *owner_fn = orig_blk ? gcc_jit_block_get_function(orig_blk) : NULL;
    return expr_sweep_oom_fn(owner_fn, 128, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_constant_rvalue(con, expected_type, out_rv);
    });
}

/**
 * @brief Sweeps OOM budgets on `gcc_gen_be_lower_dynamic_init` while preserving `cblock == NULL` state.
 * @param[in] dip Frontend dynamic initialization descriptor.
 * @param[in] target_lval Optional destination lvalue.
 * @param[out] out_rv Output rvalue slot.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD inline gcc_gen_be_error_t expr_sweep_dyn_init_oom(
    a_dynamic_init_ptr dip,
    gcc_jit_lvalue *target_lval,
    gcc_jit_rvalue **out_rv) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_block *orig_blk = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&orig_blk));
    gcc_jit_function *owner_fn = orig_blk ? gcc_jit_block_get_function(orig_blk) : NULL;
    return expr_sweep_oom_fn(owner_fn, 128, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_lower_dynamic_init(dip, target_lval, out_rv);
    });
}

/**
 * @brief Initializes an EDG integer `a_type` node in place.
 * @details Zeroes `*tp` and configures `tk_integer`, `int_kind`, `size`, and `alignment`.
 * @param[out] tp Target `a_type` storage.
 * @param[in] ik EDG integer kind (`ik_int`, `ik_long`, `ik_unsigned_int`, etc.).
 * @param[in] sz Byte size of the integer type.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
GCC_GEN_BE_NODISCARD inline gcc_gen_be_error_t expr_init_int_type(
    a_type *tp,
    an_integer_kind ik,
    a_targ_size_t sz) GCC_GEN_BE_NOEXCEPT {
    if (!tp) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    std::memset(tp, 0, sizeof(*tp));
    tp->kind = tk_integer;
    tp->size = sz;
    tp->alignment = (a_targ_alignment)(sz > 0 ? sz : 1);
    tp->variant.integer.int_kind = ik;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Initializes an EDG floating-point `a_type` node in place.
 * @details Zeroes `*tp` and configures `tk_float`, `float_kind`, `size`, and `alignment`.
 * @param[out] tp Target `a_type` storage.
 * @param[in] fk EDG float kind (`fk_float`, `fk_double`, `fk_long_double`, etc.).
 * @param[in] sz Byte size of the floating-point type.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
GCC_GEN_BE_NODISCARD inline gcc_gen_be_error_t expr_init_float_type(
    a_type *tp,
    a_float_kind fk,
    a_targ_size_t sz) GCC_GEN_BE_NOEXCEPT {
    if (!tp) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    std::memset(tp, 0, sizeof(*tp));
    tp->kind = tk_float;
    tp->size = sz;
    tp->alignment = (a_targ_alignment)(sz > 0 ? sz : 1);
    tp->variant.float_kind = fk;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Initializes an EDG `_Complex` `a_type` node in place.
 * @details Zeroes `*tp` and configures `tk_complex`, `float_kind`, and `size`.
 * @param[out] tp Target `a_type` storage.
 * @param[in] fk Element float kind (`fk_float`, `fk_double`, `fk_long_double`).
 * @param[in] sz Total byte size of the complex type.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
GCC_GEN_BE_NODISCARD inline gcc_gen_be_error_t expr_init_complex_type(
    a_type *tp,
    a_float_kind fk,
    a_targ_size_t sz) GCC_GEN_BE_NOEXCEPT {
    if (!tp) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    std::memset(tp, 0, sizeof(*tp));
    tp->kind = tk_complex;
    tp->size = sz;
    tp->alignment = 8;
    tp->variant.float_kind = fk;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Initializes an EDG pointer or reference `a_type` node in place.
 * @details Zeroes `*tp` and configures `tk_pointer`, `pointee`, and `is_reference`.
 * @param[out] tp Target `a_type` storage.
 * @param[in] pointee Pointed-to frontend type.
 * @param[in] is_ref `TRUE` for reference type, `FALSE` for pointer type.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
GCC_GEN_BE_NODISCARD inline gcc_gen_be_error_t expr_init_ptr_type(
    a_type *tp,
    a_type_ptr pointee,
    a_boolean is_ref) GCC_GEN_BE_NOEXCEPT {
    if (!tp) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    std::memset(tp, 0, sizeof(*tp));
    tp->kind = tk_pointer;
    tp->size = sizeof(void *);
    tp->alignment = (a_targ_alignment)sizeof(void *);
    tp->variant.pointer.type = pointee;
    tp->variant.pointer.is_reference = is_ref;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Initializes an EDG `tk_typeref` alias/qualifier wrapper `a_type` node in place.
 * @details Wraps `underlying` with optional `qualifiers` (e.g. `TQ_CONST` or `TQ_C11_ATOMIC`).
 * @param[out] tp Target `a_type` storage.
 * @param[in] underlying Referenced underlying `a_type_ptr`.
 * @param[in] qualifiers Qualifier bitmask (`TQ_NONE`, `TQ_CONST`, `TQ_C11_ATOMIC`, etc.).
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
GCC_GEN_BE_NODISCARD inline gcc_gen_be_error_t expr_init_typeref(
    a_type *tp,
    a_type_ptr underlying,
    a_type_qualifier_set qualifiers = TQ_NONE) GCC_GEN_BE_NOEXCEPT {
    if (!tp) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    std::memset(tp, 0, sizeof(*tp));
    tp->kind = tk_typeref;
    tp->size = underlying ? underlying->size : 0;
    tp->alignment = underlying ? underlying->alignment : 1;
    tp->variant.typeref.type = underlying;
    tp->variant.typeref.qualifiers = qualifiers;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Initializes an EDG integer `enk_constant` expression and backing `a_constant` node.
 * @details Zeroes `*en` and `*con` and wires `en->variant.constant.ptr = con` with `ck_integer`.
 * @param[out] en Target `an_expr_node` storage.
 * @param[out] con Target `a_constant` storage.
 * @param[in] tp Expression and constant frontend type.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
GCC_GEN_BE_NODISCARD inline gcc_gen_be_error_t expr_init_int_const_expr(
    an_expr_node *en,
    a_constant *con,
    a_type_ptr tp) GCC_GEN_BE_NOEXCEPT {
    if (!en || !con) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    std::memset(en, 0, sizeof(*en));
    std::memset(con, 0, sizeof(*con));
    con->kind = ck_integer;
    con->type = tp;
    en->kind = enk_constant;
    en->type = tp;
    en->variant.constant.ptr = con;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Initializes an EDG `enk_variable` expression node in place.
 * @details Zeroes `*en` and binds `en->variant.variable.ptr = var`.
 * @param[out] en Target `an_expr_node` storage.
 * @param[in] var Referenced `a_variable_ptr`.
 * @param[in] tp Expression frontend type.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
GCC_GEN_BE_NODISCARD inline gcc_gen_be_error_t expr_init_var_expr(
    an_expr_node *en,
    a_variable_ptr var,
    a_type_ptr tp) GCC_GEN_BE_NOEXCEPT {
    if (!en) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    std::memset(en, 0, sizeof(*en));
    en->kind = enk_variable;
    en->type = tp;
    en->variant.variable.ptr = var;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Initializes an EDG `enk_operation` expression node with up to 3 chained operands.
 * @details Zeroes `*en`, sets `op` and `tp`, and links `op1 -> op2 -> op3`.
 * @param[out] en Target `an_expr_node` storage.
 * @param[in] op Operator kind (`eok_add`, `eok_assign`, `eok_call`, etc.).
 * @param[in] tp Result frontend type.
 * @param[in] op1 First operand (may be NULL).
 * @param[in] op2 Second operand chained via `op1->next` (may be NULL).
 * @param[in] op3 Third operand chained via `op2->next` (may be NULL).
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
GCC_GEN_BE_NODISCARD inline gcc_gen_be_error_t expr_init_op_expr(
    an_expr_node *en,
    an_expr_operator_kind op,
    a_type_ptr tp,
    an_expr_node_ptr op1,
    an_expr_node_ptr op2 = NULL,
    an_expr_node_ptr op3 = NULL) GCC_GEN_BE_NOEXCEPT {
    if (!en) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    std::memset(en, 0, sizeof(*en));
    en->kind = enk_operation;
    en->type = tp;
    en->variant.operation.kind = op;
    en->variant.operation.operands = op1;
    if (op1) op1->next = op2;
    if (op2) op2->next = op3;
    if (op3) op3->next = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Initializes an EDG integer `a_constant` node and registers its integer value in `g_expr_mock`.
 * @details Zeroes `*con`, sets `ck_integer` and `tp`, and records `val` via `expr_test_map_const_int`.
 * @param[out] con Target `a_constant` storage.
 * @param[in] tp Constant frontend type (may be NULL).
 * @param[in] val Signed host integer value for `value_of_integer_constant`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD inline gcc_gen_be_error_t expr_init_int_const(
    a_constant *con,
    a_type_ptr tp,
    a_host_large_integer val) GCC_GEN_BE_NOEXCEPT {
    if (!con) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    std::memset(con, 0, sizeof(*con));
    con->kind = ck_integer;
    con->type = tp;
    return expr_test_map_const_int(con, val);
}

/**
 * @brief Initializes an `enk_constant` expression node referencing `con`.
 * @details Zeroes `*en` and sets `en->type = con->type` and `en->variant.constant.ptr = con`.
 * @param[out] en Target `an_expr_node` storage.
 * @param[in] con Referenced `a_constant_ptr`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
GCC_GEN_BE_NODISCARD inline gcc_gen_be_error_t expr_init_const_expr(
    an_expr_node *en,
    a_constant_ptr con) GCC_GEN_BE_NOEXCEPT {
    if (!en || !con) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    std::memset(en, 0, sizeof(*en));
    en->kind = enk_constant;
    en->type = con->type;
    en->variant.constant.ptr = con;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Initializes an `enk_variable` expression node inferring `en->type` from `var->type`.
 * @details Delegates to the 3-argument `expr_init_var_expr` using `var ? var->type : NULL`.
 * @param[out] en Target `an_expr_node` storage.
 * @param[in] var Referenced `a_variable_ptr`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
GCC_GEN_BE_NODISCARD inline gcc_gen_be_error_t expr_init_var_expr(
    an_expr_node *en,
    a_variable_ptr var) GCC_GEN_BE_NOEXCEPT {
    return expr_init_var_expr(en, var, var ? var->type : NULL);
}

/**
 * @brief Initializes an `enk_operation` expression node with first operand `op1` without clearing `op1->next`.
 * @details Preserves any pre-linked `op1->next` sibling list on `op1`.
 * @param[out] en Target `an_expr_node` storage.
 * @param[in] op Operator kind.
 * @param[in] tp Expression result type.
 * @param[in] op1 First operand expression node (may be NULL).
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
GCC_GEN_BE_NODISCARD inline gcc_gen_be_error_t expr_init_op(
    an_expr_node *en,
    an_expr_operator_kind op,
    a_type_ptr tp,
    an_expr_node_ptr op1) GCC_GEN_BE_NOEXCEPT {
    if (!en) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    std::memset(en, 0, sizeof(*en));
    en->kind = enk_operation;
    en->type = tp;
    en->variant.operation.kind = op;
    en->variant.operation.operands = op1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Runs dynamic initializer, lvalue, bitfield, VLA stride, and `safe_ptr_or_int_cast` unit tests.
 * @details Exercises `gcc_gen_be_lower_dynamic_init`, `gcc_gen_be_lower_expr_lvalue`, bitfield R/W, and OOM branches.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD gcc_gen_be_error_t run_expr_lval_and_init_tests(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Runs Part 1 of `gcc_gen_be_lower_expr_rvalue` unit tests (constants, vars, assignments, binary/unary ops).
 * @details Exercises rvalue expression lowering and OOM branches for arithmetic, complex, bitfield, atomic, and pointer ops.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD gcc_gen_be_error_t run_expr_rval_part1_tests(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Runs Part 2 of `gcc_gen_be_lower_expr_rvalue` unit tests (calls, `va_arg`, casts, `new`/`delete`, builtins, EH).
 * @details Exercises rvalue lowering and OOM branches for direct/virtual/PMF/coerced calls, variadics, casts, and remaining `enk_*` nodes.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD gcc_gen_be_error_t run_expr_rval_part2_tests(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Executes exhaustive branch/line/OOM coverage tests for helpers, dynamic init, bitfields, and constants.
 * @details Covers all helper functions, `gcc_gen_be_lower_dynamic_init`, bitfield read/write,
 * and aggregate/address/scalar constant rvalue lowering in `src/gcc_gen_be_expr.c`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD gcc_gen_be_error_t run_expr_cov_init_and_const_tests(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Executes exhaustive branch/line/OOM coverage tests for `gcc_gen_be_lower_expr_lvalue`.
 * @details Covers all lvalue expression kinds, operation subcases, NULL/error paths, and OOM budgets.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD gcc_gen_be_error_t run_expr_cov_lval_tests(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Executes exhaustive branch/line/OOM coverage tests for `gcc_gen_be_lower_expr_rvalue`.
 * @details Covers all rvalue expression kinds, operator branches, call/ABI/cast/EH/new-delete paths, and OOM budgets.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD gcc_gen_be_error_t run_expr_cov_rval_tests(void) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* TEST_GCC_BE_EXPR_COMMON_H */
