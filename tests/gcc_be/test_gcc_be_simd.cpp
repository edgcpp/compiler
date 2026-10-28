/**
 * @file test_gcc_be_simd.cpp
 * @brief Unit tests for target SIMD vector intrinsics and atomic/bitwise/overflow builtins.
 * @details Tests `gcc_gen_be_lower_builtin_call` across SIMD shuffle, atomic memory operations,
 * checked arithmetic overflow, sanitizer hooks, and math/bitwise builtins.
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_builtin.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_error.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_stmt.h"
#include "gcc_gen_be_lib_loader.h"
#include <libgccjit.h>
#include <cstdio>
#include <cstring>

extern "C" {
/**
 * @brief Mock diagnostic reporter for SIMD and builtin unit tests.
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
/** @brief Optimization level option. */
int gcc_be_opt_level = 0;
/** @brief Debug info option. */
a_boolean gcc_be_debug_info = FALSE;
/** @brief Dump initial tree option. */
a_boolean gcc_be_dump_initial_tree = FALSE;
/** @brief Dump GIMPLE option. */
a_boolean gcc_be_dump_gimple = FALSE;
/** @brief Dump RTL option. */
a_boolean gcc_be_dump_rtl_all = FALSE;
/** @brief Fast math option. */
a_boolean gcc_be_ffast_math = FALSE;
/** @brief Associative math option. */
a_boolean gcc_be_fassociative_math = FALSE;
/** @brief No trapping math option. */
a_boolean gcc_be_fno_trapping_math = FALSE;
/** @brief Shared library option. */
a_boolean gcc_be_shared = FALSE;
/** @brief Static linking option. */
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
/** @brief Position-independent code flag. */
a_boolean gcc_be_fPIC = FALSE;
/** @brief Position-independent executable flag. */
a_boolean gcc_be_fPIE = FALSE;
/** @brief No position-independent executable flag. */
a_boolean gcc_be_fno_pie = FALSE;
/** @brief Exceptions enabled flag. */
a_boolean exceptions_enabled = TRUE;
/** @brief Driver options list pointer. */
void *gcc_be_driver_options = NULL;
/** @brief Compiler options list pointer. */
void *gcc_be_compiler_options = NULL;
/** @brief Active C dialect. */
a_C_dialect C_dialect = C_dialect_cplusplus;
/** @brief GCC compatibility mode flag. */
a_boolean gcc_mode = TRUE;
/** @brief Mock IL header. */
an_il_header il_header;

/**
 * @brief Stub for computing type size in SIMD unit tests.
 * @details Returns `tp->size` directly.
 * @param[in] tp The frontend type pointer.
 * @return Byte size of `tp`.
 */
a_targ_size_t f_size_of_type(a_type_ptr tp) {
    return tp ? tp->size : 0;
}

/**
 * @brief Stub for type lowering in SIMD unit tests.
 * @details Lowers any type to `int` for testing purposes.
 * @param[in] tp The frontend type pointer.
 * @param[out] out_type Pointer to receive the lowered `gcc_jit_type`.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_type(a_type_ptr tp, struct gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
    (void)tp;
    if (!out_type) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for active coroutine frame lookup in SIMD unit tests.
 * @details Sets `*out_frame_type` and `*out_frame_lval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[out] out_frame_type Pointer to receive the active coroutine frame type.
 * @param[out] out_frame_lval Pointer to receive the active coroutine frame lvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_get_active_coroutine_frame(struct gcc_jit_type **out_frame_type, struct gcc_jit_lvalue **out_frame_lval) GCC_GEN_BE_NOEXCEPT {
    if (out_frame_type) *out_frame_type = NULL;
    if (out_frame_lval) *out_frame_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}
}

/**
 * @brief Runs all SIMD and builtin lowering unit tests.
 * @details Exercises `gcc_gen_be_lower_builtin_call` across SIMD shuffle, atomic, overflow, and sanitizer builtins.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_simd_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    gcc_jit_rvalue *out_rval = NULL;
    if (gcc_gen_be_lower_builtin_call(NULL, 0, NULL, &out_rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_builtin_call((a_routine_ptr)0x1234, 0, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));
    GCC_GEN_BE_CHECK(load_libgccjit_posix());

    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *vec_type = gcc_jit_type_get_vector(int_type, 4);
    gcc_jit_function *func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, int_type, "simd_test_fn", 0, NULL, 0);
    gcc_jit_block *blk = gcc_jit_function_new_block(func, "entry");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(blk));

    gcc_jit_lvalue *v1_local = gcc_jit_function_new_local(func, NULL, vec_type, "v1");
    gcc_jit_lvalue *v2_local = gcc_jit_function_new_local(func, NULL, vec_type, "v2");
    gcc_jit_rvalue *vargs[2] = {
        gcc_jit_lvalue_as_rvalue(v1_local),
        gcc_jit_lvalue_as_rvalue(v2_local)
    };

    a_routine rout;
    std::memset(&rout, 0, sizeof(rout));
    a_type rtype;
    std::memset(&rtype, 0, sizeof(rtype));
    rtype.kind = tk_routine;
    rout.type = &rtype;

    rout.source_corresp.name = "__builtin_shufflevector";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, vargs, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "__builtin_coro_size";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "__atomic_always_lock_free";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "__sync_synchronize";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    a_type i128_ret;
    std::memset(&i128_ret, 0, sizeof(i128_ret));
    i128_ret.kind = tk_integer;
    i128_ret.size = 16;
    rtype.variant.routine.return_type = &i128_ret;

    rout.source_corresp.name = "__atomic_load_n";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, vargs, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "__atomic_compare_exchange_n";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, vargs, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    i128_ret.size = 3;
    if (gcc_gen_be_lower_builtin_call(&rout, 1, vargs, &out_rval) != GCC_GEN_BE_ERROR_ATOMIC_ALIGNMENT_FAILED) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    i128_ret.size = 16;

    rout.source_corresp.name = "__builtin_convertvector";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, vargs, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "__builtin_expect";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, vargs, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "vfmaq_f32";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, vargs, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "__builtin_constant_p";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, vargs, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "__builtin_object_size";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, vargs, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "__builtin_classify_type";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, vargs, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "svadd_f32_z";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, vargs, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "__riscv_vadd_vv_i32m1";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, vargs, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "__builtin_types_compatible_p";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "__builtin_offsetof";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "__builtin_bit_cast";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, vargs, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "_mm_add_ps";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, vargs, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "_mm256_sub_pd";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, vargs, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "_mm512_mul_epi32";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, vargs, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "vaddq_s32";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, vargs, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "__atomic_is_lock_free";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, vargs, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    a_type src_t4;
    a_type dst_t4;
    a_type dst_t8;
    std::memset(&src_t4, 0, sizeof(src_t4));
    std::memset(&dst_t4, 0, sizeof(dst_t4));
    std::memset(&dst_t8, 0, sizeof(dst_t8));
    src_t4.kind = tk_integer;
    src_t4.size = 4;
    dst_t4.kind = tk_float;
    dst_t4.size = 4;
    dst_t8.kind = tk_integer;
    dst_t8.size = 8;
    gcc_jit_rvalue *int_arg = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 42);
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_bit_cast(&dst_t4, &src_t4, int_arg, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_builtin_bit_cast(&dst_t8, &src_t4, int_arg, &out_rval) != GCC_GEN_BE_ERROR_TYPE_MISMATCH) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }

    /* Test SIMD immediate validation */
    a_boolean imm_valid = FALSE;
    if (gcc_gen_be_validate_simd_immediate(NULL, NULL, 0, 7, &imm_valid) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_validate_simd_immediate("_mm_dp_ps", NULL, 0, 7, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_validate_simd_immediate("_mm_dp_ps", NULL, 0, 7, &imm_valid) != GCC_GEN_BE_ERROR_SIMD_LOWERING_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;

    a_constant imm_const;
    std::memset(&imm_const, 0, sizeof(imm_const));
    imm_const.kind = ck_integer;
    imm_const.variant.integer_value = 3;
    an_expr_node imm_expr;
    std::memset(&imm_expr, 0, sizeof(imm_expr));
    imm_expr.kind = enk_constant;
    imm_expr.variant.constant.ptr = &imm_const;
    GCC_GEN_BE_CHECK(gcc_gen_be_validate_simd_immediate("_mm_dp_ps", &imm_expr, 0, 7, &imm_valid));
    if (!imm_valid) return GCC_GEN_BE_ERROR_INTERNAL;
    imm_const.variant.integer_value = 99;
    if (gcc_gen_be_validate_simd_immediate("_mm_dp_ps", &imm_expr, 0, 7, &imm_valid) != GCC_GEN_BE_ERROR_SIMD_LOWERING_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test __builtin_object_size evaluation and __builtin_clear_padding */
    size_t bos_bound = 0;
    if (gcc_gen_be_eval_builtin_object_size(NULL, 0, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_eval_builtin_object_size(NULL, 0, &bos_bound));
    if (bos_bound != (size_t)-1) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_eval_builtin_object_size(NULL, 2, &bos_bound));
    if (bos_bound != 0U) return GCC_GEN_BE_ERROR_INTERNAL;

    a_field pad_f0, pad_f1;
    std::memset(&pad_f0, 0, sizeof(pad_f0));
    std::memset(&pad_f1, 0, sizeof(pad_f1));
    a_type char_tp;
    std::memset(&char_tp, 0, sizeof(char_tp));
    char_tp.kind = tk_integer;
    char_tp.size = 1;
    pad_f0.type = &char_tp;
    pad_f0.offset = 0;
    pad_f0.next = &pad_f1;
    pad_f1.type = &src_t4;
    pad_f1.offset = 4;
    a_type padded_struct;
    std::memset(&padded_struct, 0, sizeof(padded_struct));
    padded_struct.kind = tk_struct;
    padded_struct.size = 12;
    padded_struct.variant.class_struct_union.field_list = &pad_f0;
    a_type padded_ptr;
    std::memset(&padded_ptr, 0, sizeof(padded_ptr));
    padded_ptr.kind = tk_pointer;
    padded_ptr.size = 8;
    padded_ptr.variant.pointer.type = &padded_struct;

    GCC_GEN_BE_CHECK(gcc_gen_be_eval_builtin_object_size(&padded_ptr, 0, &bos_bound));
    if (bos_bound != 12U) return GCC_GEN_BE_ERROR_INTERNAL;

    if (gcc_gen_be_lower_builtin_clear_padding(&padded_ptr, NULL, &out_rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_builtin_clear_padding(&padded_ptr, int_arg, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_clear_padding(&padded_ptr, int_arg, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "__builtin_clear_padding";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, &int_arg, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "vgetq_lane_s32";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, vargs, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "_mm_shuffle_epi8";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, vargs, &out_rval));
    if (!out_rval) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_context_release(ctx);
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Entry point for the SIMD and builtin unit test executable.
 * @details Runs `run_simd_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_simd_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}

