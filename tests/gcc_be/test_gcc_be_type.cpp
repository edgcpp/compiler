/**
 * @file test_gcc_be_type.cpp
 * @brief Unit tests for type lowering (`gcc_gen_be_type.c`).
 * @details Verifies null-pointer validation in `gcc_gen_be_lower_type` and `gcc_gen_be_get_zero_rvalue`.
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_location.h"
#include "gcc_gen_be_debug.h"
#include "gcc_gen_be_error.h"
#include <libgccjit.h>
#include <cstdio>
#include <cstring>

extern "C" {
/**
 * @brief Mock diagnostic reporter for type unit tests.
 * @details Returns the error code passed to it without terminating.
 * @param[in] err The error code.
 * @param[in] msg Optional diagnostic message.
 * @return The input error code `err`.
 */
gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t err, const char *msg) GCC_GEN_BE_NOEXCEPT {
    (void)msg;
    return err;
}
}

USING_NAMESPACE_EDG

namespace edg {
/** @brief Optimization level. */
int gcc_be_opt_level = 0;
/** @brief Debug info flag. */
a_boolean gcc_be_debug_info = FALSE;
/** @brief Dump initial tree flag. */
a_boolean gcc_be_dump_initial_tree = FALSE;
/** @brief Dump gimple flag. */
a_boolean gcc_be_dump_gimple = FALSE;
/** @brief Dump RTL flag. */
a_boolean gcc_be_dump_rtl_all = FALSE;
/** @brief Fast math flag. */
a_boolean gcc_be_ffast_math = FALSE;
/** @brief Associative math flag. */
a_boolean gcc_be_fassociative_math = FALSE;
/** @brief No trapping math flag. */
a_boolean gcc_be_fno_trapping_math = FALSE;
/** @brief Shared library flag. */
a_boolean gcc_be_shared = FALSE;
/** @brief Static linking flag. */
a_boolean gcc_be_static = FALSE;
/** @brief Target architecture string. */
a_const_char *gcc_be_march = NULL;
/** @brief Target CPU string. */
a_const_char *gcc_be_mcpu = NULL;
/** @brief Target tune string. */
a_const_char *gcc_be_mtune = NULL;
/** @brief Target FPU string. */
a_const_char *gcc_be_mfpu = NULL;
/** @brief Target code model string. */
a_const_char *gcc_be_mcmodel = NULL;
/** @brief 64-bit mode flag. */
a_boolean gcc_be_m64 = FALSE;
/** @brief 32-bit mode flag. */
a_boolean gcc_be_m32 = FALSE;
/** @brief PIC flag. */
a_boolean gcc_be_fPIC = FALSE;
/** @brief PIE flag. */
a_boolean gcc_be_fPIE = FALSE;
/** @brief No PIE flag. */
a_boolean gcc_be_fno_pie = FALSE;
/** @brief Exceptions enabled flag. */
a_boolean exceptions_enabled = TRUE;
/** @brief Active C dialect. */
a_C_dialect C_dialect = C_dialect_cplusplus;

/**
 * @brief Stub for getting source location from type in type unit tests.
 * @details Sets `*out_loc` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] tp The frontend type pointer.
 * @param[out] out_loc Pointer to receive the JIT location.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_type(a_type_ptr tp, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    (void)tp;
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_loc = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for emitting type debug info in type unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] tp The frontend type pointer.
 * @param[in] type The JIT type pointer.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_emit_type_debug_info(a_type_ptr tp, struct gcc_jit_type *type) GCC_GEN_BE_NOEXCEPT {
    (void)tp;
    (void)type;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for checking sret return in type unit tests.
 * @details Sets `*out_is_sret` to `FALSE`.
 * @param[in] type The frontend type pointer.
 * @param[out] out_is_sret Pointer to receive the sret flag.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_is_returned_via_sret(a_type_ptr type, a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT {
    (void)type;
    if (out_is_sret) *out_is_sret = FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for checking invisible-reference parameter passing in type unit tests.
 * @details Sets `*out_is_ref` to `FALSE`.
 * @param[in] type The frontend type pointer.
 * @param[out] out_is_ref Pointer to receive the invisible-reference flag.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT {
    (void)type;
    if (out_is_ref) *out_is_ref = FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for coerced ABI type lookup in type unit tests.
 * @details Sets `*out_coerced_type` to NULL and `*out_needs_coercion` to `FALSE`.
 * @param[in] type The frontend type pointer.
 * @param[out] out_coerced_type Pointer to receive the coerced JIT type.
 * @param[out] out_needs_coercion Pointer to receive the coercion flag.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_get_coerced_abi_type(a_type_ptr type, struct gcc_jit_type **out_coerced_type, a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT {
    (void)type;
    if (out_coerced_type) *out_coerced_type = NULL;
    if (out_needs_coercion) *out_needs_coercion = FALSE;
    return GCC_GEN_BE_SUCCESS;
}
}

/**
 * @brief Executes type lowering null-pointer validation tests.
 * @details Verifies `gcc_gen_be_lower_type` and `gcc_gen_be_get_zero_rvalue` return `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` on NULL inputs.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_type_unit_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    gcc_jit_type *type = NULL;
    gcc_jit_rvalue *rval = NULL;
    if (gcc_gen_be_lower_type(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_type(NULL, &type) != GCC_GEN_BE_SUCCESS) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_get_zero_rvalue(NULL, NULL, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_decimal_float_type(4, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_OOM;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_type(4, &type));
    if (!type) return GCC_GEN_BE_ERROR_INTERNAL;
    gcc_jit_rvalue *df32_zero = gcc_jit_context_new_rvalue_from_int(ctx, type, 0);
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_type(8, &type));
    if (!type) return GCC_GEN_BE_ERROR_INTERNAL;
    gcc_jit_rvalue *df64_zero = gcc_jit_context_new_rvalue_from_int(ctx, type, 0);
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_type(16, &type));
    if (!type) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_decimal_float_type(7, &type) != GCC_GEN_BE_ERROR_DECIMAL_FLOAT_UNSUPPORTED) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }

    if (gcc_gen_be_lower_decimal_float_binary_op(4, '+', NULL, df32_zero, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    if (gcc_gen_be_lower_decimal_float_binary_op(4, '?', df32_zero, df32_zero, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    const char df_ops[4] = { '+', '-', '*', '/' };
    for (int i = 0; i < 4; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_binary_op(4, df_ops[i], df32_zero, df32_zero, &rval));
        if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_binary_op(8, '+', df64_zero, df64_zero, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;
    gcc_gen_be_singleton_state.active_target_abi_override = 6; /* PPC64_ELFV2 + 1 -> __dpd_ */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_binary_op(8, '+', df64_zero, df64_zero, &rval));
    gcc_gen_be_singleton_state.active_target_abi_override = 7; /* S390X + 1 -> __dpd_ */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_binary_op(4, '-', df32_zero, df32_zero, &rval));
    gcc_gen_be_singleton_state.active_target_abi_override = 0;

    if (gcc_gen_be_lower_decimal_float_unary_op(4, '-', NULL, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    if (gcc_gen_be_lower_decimal_float_unary_op(4, '?', df32_zero, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_unary_op(4, '-', df32_zero, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_unary_op(8, 'a', df64_zero, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;

    if (gcc_gen_be_lower_decimal_float_comparison(4, NULL, df32_zero, df32_zero, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    const char *df_cmps[6] = { "eq", "ne", "lt", "le", "gt", "ge" };
    for (int i = 0; i < 6; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_comparison(4, df_cmps[i], df32_zero, df32_zero, &rval));
        if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_comparison(8, "eq", df64_zero, df64_zero, &rval));

    if (gcc_gen_be_lower_decimal_float_cast(4, 8, NULL, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_cast(4, 4, df32_zero, &rval));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_cast(4, 8, df32_zero, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_cast(8, 4, df64_zero, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;

    if (gcc_gen_be_lower_decimal_to_binary_float(4, 3, df32_zero, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_to_binary_float(4, 8, df32_zero, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_to_binary_float(8, 4, df64_zero, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;

    if (gcc_gen_be_lower_decimal_to_int(4, FALSE, NULL, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_to_int(4, FALSE, df32_zero, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_to_int(8, TRUE, df64_zero, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test int-to-decimal and binary-float-to-decimal BID helpers */
    gcc_jit_type *int_jit_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_rvalue *int_zero = gcc_jit_context_new_rvalue_from_int(ctx, int_jit_t, 0);
    if (gcc_gen_be_lower_int_to_decimal(2, FALSE, 4, int_zero, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_int_to_decimal(4, FALSE, 4, int_zero, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_int_to_decimal(8, TRUE, 8, df64_zero, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_int_to_decimal(8, FALSE, 16, df64_zero, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_type *flt_jit_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
    gcc_jit_rvalue *flt_zero = gcc_jit_context_new_rvalue_from_double(ctx, flt_jit_t, 0.0);
    if (gcc_gen_be_lower_binary_float_to_decimal(3, 4, flt_zero, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_binary_float_to_decimal(4, 8, flt_zero, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_binary_float_to_decimal(8, 4, flt_zero, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_binary_float_to_decimal(8, 16, flt_zero, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test ISO/IEC TR 18037 fixed-point arithmetic and saturation */
    if (gcc_gen_be_lower_fixed_point_binary_op(3, 8, FALSE, FALSE, '+', int_zero, int_zero, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    if (gcc_gen_be_lower_fixed_point_binary_op(4, 32, FALSE, FALSE, '+', int_zero, int_zero, &rval) != GCC_GEN_BE_ERROR_FIXED_POINT_OVERFLOW) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    if (gcc_gen_be_lower_fixed_point_binary_op(4, 15, FALSE, FALSE, '?', int_zero, int_zero, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    for (int i = 0; i < 4; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_fixed_point_binary_op(2, 7, FALSE, TRUE, df_ops[i], int_zero, int_zero, &rval));
        if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_fixed_point_binary_op(4, 16, TRUE, TRUE, '+', int_zero, int_zero, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_fixed_point_binary_op(8, 31, FALSE, FALSE, '*', df64_zero, df64_zero, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;

#if FIXED_POINT_ALLOWED
    a_type fp_type = {};
    fp_type.kind = tk_fixed_point;
    fp_type.size = 4;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&fp_type, &type));
    if (!type) return GCC_GEN_BE_ERROR_INTERNAL;
#endif

    a_type uint_tp = {};
    uint_tp.kind = tk_integer;
    uint_tp.size = 4;
    uint_tp.variant.integer.int_kind = ik_unsigned_int;
    a_field straddle_bf = {};
    straddle_bf.source_corresp.name = "straddle_bf";
    straddle_bf.type = &uint_tp;
    straddle_bf.is_bit_field = TRUE;
    straddle_bf.bit_size = 24;
    straddle_bf.offset = 0;
    straddle_bf.offset_bit_remainder = 16;
    a_type bf_struct_tp = {};
    bf_struct_tp.kind = tk_struct;
    bf_struct_tp.size = 8;
    bf_struct_tp.source_corresp.name = "StraddleBfStruct";
    bf_struct_tp.variant.class_struct_union.field_list = &straddle_bf;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&bf_struct_tp, &type));
    if (!type) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_context_release(ctx);

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_gcc_be_type.cpp`.
 * @details Runs `run_type_unit_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_type_unit_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}
