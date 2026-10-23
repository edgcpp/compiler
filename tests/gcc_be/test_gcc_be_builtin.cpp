/**
 * @file test_gcc_be_builtin.cpp
 * @brief Unit tests for builtin call lowering validation (`gcc_gen_be_builtin.c`).
 * @details Verifies null-pointer handling in `gcc_gen_be_lower_builtin_call`.
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_builtin.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_error.h"
#include <libgccjit.h>
#include <cstdio>
#include <cstring>

extern "C" {
/**
 * @brief Mock diagnostic reporter for builtin unit tests.
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
/** @brief GCC compatibility mode flag. */
a_boolean gcc_mode = TRUE;

/**
 * @brief Stub for computing type size in builtin unit tests.
 * @details Returns `tp->size` directly.
 * @param[in] tp The frontend type pointer.
 * @return Byte size of `tp`.
 */
a_targ_size_t f_size_of_type(a_type_ptr tp) {
    return tp ? tp->size : 0;
}

/**
 * @brief Stub for type lowering in builtin unit tests.
 * @details Sets `*out_type` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] tp The frontend type pointer.
 * @param[out] out_type Pointer to receive the lowered `gcc_jit_type`.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_type(a_type_ptr tp, struct gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
    (void)tp;
    if (!out_type) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_type = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for active coroutine frame lookup in builtin unit tests.
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
 * @brief Executes builtin lowering validation tests.
 * @details Verifies `gcc_gen_be_lower_builtin_call` rejects null routine and output pointers.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_builtin_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    gcc_jit_rvalue *rval = NULL;
    if (gcc_gen_be_lower_builtin_call(NULL, 0, NULL, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_OOM;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    a_routine rout = {};
    rout.source_corresp.name = "_abnormal_termination";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "_exception_code";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "_exception_info";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_type *void_ptr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *size_t_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
    gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *cstr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CONST_CHAR_PTR);
    gcc_jit_rvalue *chk_args[5] = {
        gcc_jit_context_null(ctx, void_ptr_t),
        gcc_jit_context_null(ctx, void_ptr_t),
        gcc_jit_context_new_rvalue_from_long(ctx, size_t_t, 8),
        gcc_jit_context_new_rvalue_from_long(ctx, size_t_t, -1L),
        gcc_jit_context_new_string_literal(ctx, "%d")
    };
    rout.source_corresp.name = "__builtin___memcpy_chk";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 4, chk_args, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_rvalue *sn_args[7] = {
        gcc_jit_context_null(ctx, void_ptr_t),
        gcc_jit_context_new_rvalue_from_long(ctx, size_t_t, 16),
        gcc_jit_context_new_rvalue_from_int(ctx, int_t, 0),
        gcc_jit_context_new_rvalue_from_long(ctx, size_t_t, -1L),
        gcc_jit_context_new_cast(ctx, NULL, gcc_jit_context_new_string_literal(ctx, "ok %d %d"), cstr_t),
        gcc_jit_context_new_rvalue_from_int(ctx, int_t, 11),
        gcc_jit_context_new_rvalue_from_int(ctx, int_t, 22)
    };
    rout.source_corresp.name = "__builtin___snprintf_chk";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 5, sn_args, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 7, sn_args, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "__builtin_constant_p";
    rout.compiler_generated = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, sn_args, &rval));
    rout.compiler_generated = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, sn_args, &rval));

    rout.source_corresp.name = "__builtin_va_arg_pack";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rval));
    rout.source_corresp.name = "__builtin_va_arg_pack_len";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rval));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, sn_args, &rval));

    /* Test all __builtin_classify_type branches */
    a_type cls_tp = {};
    a_param_type cls_p0 = {};
    cls_p0.type = &cls_tp;
    a_routine_type_supplement cls_rts = {};
    cls_rts.param_type_list = &cls_p0;
    a_type cls_rout_tp = {};
    cls_rout_tp.kind = tk_routine;
    cls_rout_tp.variant.routine.extra_info = &cls_rts;
    rout.type = &cls_rout_tp;
    rout.source_corresp.name = "__builtin_classify_type";
    a_type_kind kinds[] = {
        tk_void, tk_integer, tk_pointer, tk_ptr_to_member, tk_float,
        tk_complex, tk_routine, tk_struct, tk_class, tk_union, tk_array, tk_vector
    };
    for (size_t k = 0; k < sizeof(kinds) / sizeof(kinds[0]); ++k) {
        std::memset(&cls_tp, 0, sizeof(cls_tp));
        cls_tp.kind = kinds[k];
        cls_tp.size = 4;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, sn_args, &rval));
    }
    std::memset(&cls_tp, 0, sizeof(cls_tp));
    cls_tp.kind = tk_integer; cls_tp.variant.integer.bool_type = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, sn_args, &rval));
    std::memset(&cls_tp, 0, sizeof(cls_tp));
    cls_tp.kind = tk_integer; cls_tp.variant.integer.enum_type = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, sn_args, &rval));
    std::memset(&cls_tp, 0, sizeof(cls_tp));
    cls_tp.kind = tk_integer; cls_tp.size = 1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, sn_args, &rval));
    std::memset(&cls_tp, 0, sizeof(cls_tp));
    cls_tp.kind = tk_pointer; cls_tp.variant.pointer.is_reference = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 1, sn_args, &rval));
    rout.type = NULL;

    rout.source_corresp.name = "svsub_f32_z";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, chk_args, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "__riscv_vsub_vv_i32m1";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 2, chk_args, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;

    rout.source_corresp.name = "__builtin_unreachable";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 0, NULL, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;

    a_type odd_struct_tp = {};
    odd_struct_tp.kind = tk_struct;
    odd_struct_tp.size = 24;
    a_type odd_ptr_tp = {};
    odd_ptr_tp.kind = tk_pointer;
    odd_ptr_tp.size = 8;
    odd_ptr_tp.variant.pointer.type = &odd_struct_tp;
    a_param_type atomic_p0 = {};
    atomic_p0.type = &odd_ptr_tp;
    a_routine_type_supplement atomic_rts = {};
    atomic_rts.param_type_list = &atomic_p0;
    a_type atomic_rout_tp = {};
    atomic_rout_tp.kind = tk_routine;
    atomic_rout_tp.variant.routine.extra_info = &atomic_rts;
    rout.type = &atomic_rout_tp;
    rout.source_corresp.name = "__atomic_load";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_builtin_call(&rout, 3, chk_args, &rval));
    if (!rval) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_context_release(ctx);

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_gcc_be_builtin.cpp`.
 * @details Runs `run_builtin_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_builtin_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}
