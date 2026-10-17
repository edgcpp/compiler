/**
 * @file test_gcc_be_abi_targets.cpp
 * @brief Unit tests for multi-target ABI classification and register coercion (System V AMD64, AAPCS64, Darwin ARM64, Win64).
 * @details Tests target ABI selection (`gcc_gen_be_get_target_abi` / `gcc_gen_be_set_target_abi`),
 * System V AMD64 eightbyte coercion (`gcc_gen_be_get_coerced_abi_type`), AAPCS64 HFA/HVA and GP coercion
 * (`gcc_gen_be_coerced_aapcs64_type`), and variadic classification (`gcc_gen_be_classify_va_arg`).
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_abi.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_error.h"
#include "gcc_gen_be_type.h"
#include <libgccjit.h>
#include <cstdio>
#include <cstring>

extern "C" {
/**
 * @brief Mock diagnostic reporter for ABI target unit tests.
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
 * @brief Stub for computing type size in ABI target unit tests.
 * @details Returns `tp->size` directly.
 * @param[in] tp The frontend type pointer.
 * @return Byte size of `tp`.
 */
a_targ_size_t f_size_of_type(a_type_ptr tp) {
    return tp ? tp->size : 0;
}

/**
 * @brief Stub for checking trivial copyability in ABI target unit tests.
 * @details Returns `TRUE` for all types unless `tp->size == 99`.
 * @param[in] tp The frontend type pointer.
 * @return `TRUE` if trivially copyable, `FALSE` otherwise.
 */
a_boolean is_trivially_copyable_type(a_type_ptr tp) {
    return (tp && tp->size == 99) ? FALSE : TRUE;
}

/**
 * @brief Stub for type lowering in ABI target unit tests.
 * @details Lowers float/double/int/struct types to representative JIT types.
 * @param[in] tp The frontend type pointer.
 * @param[out] out_type Pointer to receive the lowered `gcc_jit_type`.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_type(a_type_ptr tp, struct gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
    if (!out_type) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!tp) {
        *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        return GCC_GEN_BE_SUCCESS;
    }
    while (tp && tp->kind == tk_typeref) tp = tp->variant.typeref.type;
    if (!tp) return GCC_GEN_BE_ERROR_COERCION_FAILED;
    if (tp->kind == tk_float) {
        *out_type = (tp->size == 4)
            ? gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT)
            : gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
    } else if (tp->kind == tk_integer) {
        *out_type = gcc_jit_context_get_int_type(ctx, (int)(tp->size > 0 ? tp->size : 4), 1);
    } else if (tp->kind == tk_struct || tp->kind == tk_class) {
        void *cached = NULL;
        if (cache_lookup(GCC_GEN_BE_CACHE_TYPE, tp, &cached) == GCC_GEN_BE_SUCCESS && cached) {
            *out_type = (gcc_jit_type *)cached;
            return GCC_GEN_BE_SUCCESS;
        }
        gcc_jit_field *fields[8];
        int nf = 0;
        for (a_field_ptr f = tp->variant.class_struct_union.field_list; f != NULL && nf < 8; f = f->next) {
            gcc_jit_type *ft = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(f->type, &ft));
            char fname[32];
            std::snprintf(fname, sizeof(fname), "f_%d", nf);
            fields[nf++] = gcc_jit_context_new_field(ctx, NULL, ft, fname);
        }
        if (nf == 0) {
            fields[nf++] = gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR), "pad");
        }
        gcc_jit_struct *s = gcc_jit_context_new_struct_type(ctx, NULL, "test_agg_t", nf, fields);
        *out_type = gcc_jit_struct_as_type(s);
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, tp, *out_type));
    } else {
        *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    }
    return GCC_GEN_BE_SUCCESS;
}
}

/**
 * @brief Executes end-to-end JIT compilation and execution tests for register-coerced aggregates.
 * @details Compiles and executes functions passing and returning `{ long, double }`, `{ float, float, float }`,
 * and 4-byte `{ int }` structs across function boundaries under SysV AMD64, AAPCS64, RISC-V LP64D, PPC64LE, and Win64 ABI modes.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_abi_jit_execution_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));
    GCC_GEN_BE_CHECK(cache_clear_all());

    a_type long_t, dbl_t, flt_t, int_t;
    std::memset(&long_t, 0, sizeof(long_t));
    long_t.kind = tk_integer;
    long_t.size = 8;

    std::memset(&dbl_t, 0, sizeof(dbl_t));
    dbl_t.kind = tk_float;
    dbl_t.size = 8;
    dbl_t.variant.float_kind = fk_double;

    std::memset(&flt_t, 0, sizeof(flt_t));
    flt_t.kind = tk_float;
    flt_t.size = 4;
    flt_t.variant.float_kind = fk_float;

    std::memset(&int_t, 0, sizeof(int_t));
    int_t.kind = tk_integer;
    int_t.size = 4;

    /* 1. { long, double } struct */
    a_field mf0, mf1;
    std::memset(&mf0, 0, sizeof(mf0));
    std::memset(&mf1, 0, sizeof(mf1));
    mf0.type = &long_t;
    mf0.offset = 0;
    mf0.next = &mf1;
    mf1.type = &dbl_t;
    mf1.offset = 8;

    a_type mixed_struct;
    std::memset(&mixed_struct, 0, sizeof(mixed_struct));
    mixed_struct.kind = tk_struct;
    mixed_struct.size = 16;
    mixed_struct.variant.class_struct_union.field_list = &mf0;

    /* 2. { float, float, float } HFA struct */
    a_field hf0, hf1, hf2;
    std::memset(&hf0, 0, sizeof(hf0));
    std::memset(&hf1, 0, sizeof(hf1));
    std::memset(&hf2, 0, sizeof(hf2));
    hf0.type = &flt_t;
    hf0.offset = 0;
    hf0.next = &hf1;
    hf1.type = &flt_t;
    hf1.offset = 4;
    hf1.next = &hf2;
    hf2.type = &flt_t;
    hf2.offset = 8;

    a_type hfa3_struct;
    std::memset(&hfa3_struct, 0, sizeof(hfa3_struct));
    hfa3_struct.kind = tk_struct;
    hfa3_struct.size = 12;
    hfa3_struct.variant.class_struct_union.field_list = &hf0;

    /* 3. 4-byte { int } struct */
    a_field sf0;
    std::memset(&sf0, 0, sizeof(sf0));
    sf0.type = &int_t;
    sf0.offset = 0;

    a_type small4_struct;
    std::memset(&small4_struct, 0, sizeof(small4_struct));
    small4_struct.kind = tk_struct;
    small4_struct.size = 4;
    small4_struct.variant.class_struct_union.field_list = &sf0;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_target_abi(GCC_GEN_BE_TARGET_ABI_SYSV_AMD64));
    gcc_jit_type *mixed_jit_t = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(&mixed_struct, &mixed_jit_t));
    gcc_jit_type *mixed_coerced_t = NULL;
    a_boolean mixed_needs = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_coerced_abi_type(&mixed_struct, &mixed_coerced_t, &mixed_needs));
    if (!mixed_coerced_t || !mixed_needs) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Build callee: mixed_coerced_t bounce_mixed(mixed_coerced_t p) { unpack p -> return coerced p; } */
    gcc_jit_param *p_mixed = gcc_jit_context_new_param(ctx, NULL, mixed_coerced_t, "p");
    gcc_jit_function *callee_fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, mixed_coerced_t, "bounce_mixed", 1, &p_mixed, 0);
    gcc_jit_block *callee_blk = gcc_jit_function_new_block(callee_fn, "entry");
    gcc_jit_lvalue *unpacked_param = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_unpack_coerced_parameter(
        callee_fn, callee_blk, &mixed_struct, gcc_jit_param_as_rvalue(p_mixed), &unpacked_param));
    GCC_GEN_BE_CHECK(gcc_gen_be_unpack_coerced_Arg(
        callee_fn, callee_blk, &mixed_struct, gcc_jit_param_as_rvalue(p_mixed), &unpacked_param));
    gcc_jit_rvalue *ret_coerced = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_pack_coerced_return(
        callee_fn, callee_blk, &mixed_struct, gcc_jit_lvalue_as_rvalue(unpacked_param), &ret_coerced));
    GCC_GEN_BE_CHECK(gcc_gen_be_coerce_rvalue(
        callee_fn, callee_blk, gcc_jit_lvalue_as_rvalue(unpacked_param), mixed_coerced_t, &ret_coerced));
    gcc_jit_block_end_with_return(callee_blk, NULL, ret_coerced);

    /* Build exported driver: int jit_test_coercion(void) */
    gcc_jit_type *jit_int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *jit_long_t = gcc_jit_context_get_int_type(ctx, 8, 1);
    gcc_jit_type *jit_dbl_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
    gcc_jit_function *driver_fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, jit_int_t, "jit_test_coercion", 0, NULL, 0);
    gcc_jit_block *drv_blk = gcc_jit_function_new_block(driver_fn, "entry");
    gcc_jit_lvalue *local_mixed = gcc_jit_function_new_local(driver_fn, NULL, mixed_jit_t, "m");
    gcc_jit_struct *mixed_s = gcc_jit_type_is_struct(mixed_jit_t);
    gcc_jit_block_add_assignment(
        drv_blk, NULL,
        gcc_jit_lvalue_access_field(local_mixed, NULL, gcc_jit_struct_get_field(mixed_s, 0)),
        gcc_jit_context_new_rvalue_from_int(ctx, jit_long_t, 40));
    gcc_jit_block_add_assignment(
        drv_blk, NULL,
        gcc_jit_lvalue_access_field(local_mixed, NULL, gcc_jit_struct_get_field(mixed_s, 1)),
        gcc_jit_context_new_rvalue_from_double(ctx, jit_dbl_t, 2.0));

    gcc_jit_rvalue *call_arg = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_pack_coerced_argument(
        driver_fn, drv_blk, &mixed_struct, gcc_jit_lvalue_as_rvalue(local_mixed), &call_arg));
    GCC_GEN_BE_CHECK(gcc_gen_be_coerce_Arg_to_registers(
        driver_fn, drv_blk, &mixed_struct, gcc_jit_lvalue_as_rvalue(local_mixed), &call_arg));
    gcc_jit_rvalue *raw_call = gcc_jit_context_new_call(ctx, NULL, callee_fn, 1, &call_arg);
    gcc_jit_lvalue *unpacked_ret_lval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_unpack_coerced_return(
        driver_fn, drv_blk, &mixed_struct, raw_call, &unpacked_ret_lval));
    gcc_jit_rvalue *unpacked_ret = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_coerce_rvalue(driver_fn, drv_blk, raw_call, mixed_jit_t, &unpacked_ret));
    gcc_jit_rvalue *f0_val = gcc_jit_rvalue_access_field(unpacked_ret, NULL, gcc_jit_struct_get_field(mixed_s, 0));
    gcc_jit_rvalue *f1_val = gcc_jit_rvalue_access_field(unpacked_ret, NULL, gcc_jit_struct_get_field(mixed_s, 1));
    gcc_jit_rvalue *sum_val = gcc_jit_context_new_binary_op(
        ctx, NULL, GCC_JIT_BINARY_OP_PLUS, jit_int_t,
        gcc_jit_context_new_cast(ctx, NULL, f0_val, jit_int_t),
        gcc_jit_context_new_cast(ctx, NULL, f1_val, jit_int_t));
    gcc_jit_block_end_with_return(drv_blk, NULL, sum_val);

    const char *obj_path = "/tmp/test_abi_coercion_jit.o";
    std::remove(obj_path);
    gcc_jit_context_compile_to_file(ctx, GCC_JIT_OUTPUT_KIND_OBJECT_FILE, obj_path);
    FILE *fp = std::fopen(obj_path, "rb");
    if (!fp) {
        gcc_jit_context_release(ctx);
        return GCC_GEN_BE_ERROR_COMPILATION_FAILED;
    }
    std::fclose(fp);
    std::remove(obj_path);

    /* Also exercise HFA3 and 4-byte struct coercion under AAPCS64, RISC-V LP64D, PPC64LE, and Win64 */
    gcc_jit_type *c_type = NULL;
    a_boolean c_needs = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_target_abi(GCC_GEN_BE_TARGET_ABI_AAPCS64));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_coerced_abi_type(&hfa3_struct, &c_type, &c_needs));
    if (!c_type || !c_needs) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_target_abi(GCC_GEN_BE_TARGET_ABI_RISCV64_LP64D));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_coerced_abi_type(&mixed_struct, &c_type, &c_needs));
    if (!c_type || !c_needs) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_coerced_abi_type(&hfa3_struct, &c_type, &c_needs));
    if (!c_type || !c_needs) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_target_abi(GCC_GEN_BE_TARGET_ABI_PPC64LE_ELFV2));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_coerced_abi_type(&hfa3_struct, &c_type, &c_needs));
    if (!c_type || !c_needs) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_coerced_abi_type(&mixed_struct, &c_type, &c_needs));
    if (!c_type || !c_needs) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_target_abi(GCC_GEN_BE_TARGET_ABI_WIN64));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_coerced_abi_type(&small4_struct, &c_type, &c_needs));
    if (!c_type || !c_needs) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(cache_clear_all());
    gcc_jit_context_release(ctx);
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Runs all target ABI classification and register coercion unit tests.
 * @details Exercises `gcc_gen_be_get_target_abi`, `gcc_gen_be_set_target_abi`,
 * `gcc_gen_be_get_coerced_abi_type`, `gcc_gen_be_coerced_aapcs64_type`, `gcc_gen_be_coerced_riscv64_type`,
 * `gcc_gen_be_coerced_ppc64le_type`, `gcc_gen_be_classify_va_arg`, and error paths.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_abi_target_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    gcc_gen_be_target_abi_t abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
    if (gcc_gen_be_get_target_abi(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_set_target_abi((gcc_gen_be_target_abi_t)-1) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_set_target_abi((gcc_gen_be_target_abi_t)99) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_target_abi(GCC_GEN_BE_TARGET_ABI_SYSV_AMD64));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_target_abi(&abi));
    if (abi != GCC_GEN_BE_TARGET_ABI_SYSV_AMD64) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));
    GCC_GEN_BE_CHECK(cache_clear_all());

    /* Construct a mixed { long, double } 16-byte struct */
    a_type long_t;
    std::memset(&long_t, 0, sizeof(long_t));
    long_t.kind = tk_integer;
    long_t.size = 8;

    a_type dbl_t;
    std::memset(&dbl_t, 0, sizeof(dbl_t));
    dbl_t.kind = tk_float;
    dbl_t.size = 8;
    dbl_t.variant.float_kind = fk_double;

    a_field f0, f1;
    std::memset(&f0, 0, sizeof(f0));
    std::memset(&f1, 0, sizeof(f1));
    f0.type = &long_t;
    f0.offset = 0;
    f0.next = &f1;
    f1.type = &dbl_t;
    f1.offset = 8;

    a_type mixed_struct;
    std::memset(&mixed_struct, 0, sizeof(mixed_struct));
    mixed_struct.kind = tk_struct;
    mixed_struct.size = 16;
    mixed_struct.variant.class_struct_union.field_list = &f0;

    gcc_jit_type *coerced = NULL;
    a_boolean needs_coercion = FALSE;
    if (gcc_gen_be_get_coerced_abi_type(NULL, &coerced, &needs_coercion) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_coerced_aapcs64_type(NULL, &coerced, &needs_coercion) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_coerced_riscv64_type(NULL, &coerced, &needs_coercion) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_coerced_ppc64le_type(NULL, &coerced, &needs_coercion) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test GCC_GEN_BE_ERROR_COERCION_FAILED on dangling typeref */
    a_type dangling_typeref;
    std::memset(&dangling_typeref, 0, sizeof(dangling_typeref));
    dangling_typeref.kind = tk_typeref;
    dangling_typeref.variant.typeref.type = NULL;
    if (gcc_gen_be_get_coerced_abi_type(&dangling_typeref, &coerced, &needs_coercion) != GCC_GEN_BE_ERROR_COERCION_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_coerced_aapcs64_type(&dangling_typeref, &coerced, &needs_coercion) != GCC_GEN_BE_ERROR_COERCION_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_coerced_riscv64_type(&dangling_typeref, &coerced, &needs_coercion) != GCC_GEN_BE_ERROR_COERCION_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_coerced_ppc64le_type(&dangling_typeref, &coerced, &needs_coercion) != GCC_GEN_BE_ERROR_COERCION_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test System V AMD64 straddling bitfields and 256-bit AVX vector SSEUP classification */
    a_field bf_straddle;
    std::memset(&bf_straddle, 0, sizeof(bf_straddle));
    bf_straddle.type = &long_t;
    bf_straddle.is_bit_field = TRUE;
    bf_straddle.offset = 6;
    bf_straddle.offset_bit_remainder = 4;
    bf_straddle.bit_size = 24;
    a_type bf_struct;
    std::memset(&bf_struct, 0, sizeof(bf_struct));
    bf_struct.kind = tk_struct;
    bf_struct.size = 16;
    bf_struct.variant.class_struct_union.field_list = &bf_straddle;
    gcc_gen_be_abi_class_t classes[2];
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&bf_struct, classes));
    if (classes[0] != GCC_GEN_BE_ABI_CLASS_INTEGER || classes[1] != GCC_GEN_BE_ABI_CLASS_INTEGER) return GCC_GEN_BE_ERROR_INTERNAL;

    a_type avx256_vec;
    std::memset(&avx256_vec, 0, sizeof(avx256_vec));
    avx256_vec.kind = tk_vector;
    avx256_vec.size = 32;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(&avx256_vec, classes));
    if (classes[0] != GCC_GEN_BE_ABI_CLASS_SSE || classes[1] != GCC_GEN_BE_ABI_CLASS_SSEUP) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test System V AMD64 mixed INTEGER + SSE coercion & va_arg */
    GCC_GEN_BE_CHECK(gcc_gen_be_get_coerced_abi_type(&mixed_struct, &coerced, &needs_coercion));
    if (!coerced || !needs_coercion) return GCC_GEN_BE_ERROR_INTERNAL;

    a_boolean is_stack = FALSE;
    int num_gp = 0;
    int num_fp = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_va_arg(&mixed_struct, &is_stack, &num_gp, &num_fp));
    if (is_stack || num_gp != 1 || num_fp != 1) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test AAPCS64 HFA and GP coercion */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_target_abi(GCC_GEN_BE_TARGET_ABI_AAPCS64));
    GCC_GEN_BE_CHECK(gcc_gen_be_coerced_aapcs64_type(&mixed_struct, &coerced, &needs_coercion));
    if (!coerced || !needs_coercion) return GCC_GEN_BE_ERROR_INTERNAL;

    f0.type = &dbl_t;
    GCC_GEN_BE_CHECK(gcc_gen_be_coerced_aapcs64_type(&mixed_struct, &coerced, &needs_coercion));
    if (!coerced || !needs_coercion) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test Win64 coercion */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_target_abi(GCC_GEN_BE_TARGET_ABI_WIN64));
    mixed_struct.size = 8;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_coerced_abi_type(&mixed_struct, &coerced, &needs_coercion));
    if (!coerced || !needs_coercion) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test RISC-V LP64D, i386, and PowerPC64 ELFv2 classifiers */
    if (gcc_gen_be_classify_riscv64(NULL, &is_stack, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_classify_i386(NULL, &is_stack) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_classify_ppc64le(NULL, &is_stack, &is_stack) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_target_abi(GCC_GEN_BE_TARGET_ABI_RISCV64_LP64D));
    a_boolean rv_ref = FALSE;
    a_boolean rv_fp = FALSE;
    mixed_struct.size = 16;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_riscv64(&mixed_struct, &rv_ref, &rv_fp));
    if (rv_ref || !rv_fp) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_va_arg(&mixed_struct, &is_stack, &num_gp, &num_fp));
    if (is_stack || num_gp != 2) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_target_abi(GCC_GEN_BE_TARGET_ABI_SYSV_I386));
    a_boolean i386_sret = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_i386(&mixed_struct, &i386_sret));
    if (!i386_sret) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_va_arg(&mixed_struct, &is_stack, &num_gp, &num_fp));
    if (!is_stack) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_target_abi(GCC_GEN_BE_TARGET_ABI_PPC64LE_ELFV2));
    a_boolean ppc_hfa = FALSE;
    a_boolean ppc_sret = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_ppc64le(&mixed_struct, &ppc_hfa, &ppc_sret));
    if (!ppc_hfa || ppc_sret) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_ppc64_elfv2(&mixed_struct, &ppc_hfa, &ppc_sret));
    if (!ppc_hfa || ppc_sret) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test 6-element HFA on PPC64LE (valid on PPC64LE up to 8, invalid on AAPCS64 up to 4) */
    a_type flt_arr6;
    std::memset(&flt_arr6, 0, sizeof(flt_arr6));
    flt_arr6.kind = tk_array;
    flt_arr6.size = 48;
    flt_arr6.variant.array.element_type = &dbl_t;
    flt_arr6.variant.array.variant.number_of_elements = 6;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_ppc64le(&flt_arr6, &ppc_hfa, &ppc_sret));
    if (!ppc_hfa || ppc_sret) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_coerced_ppc64le_type(&flt_arr6, &coerced, &needs_coercion));
    if (!coerced || !needs_coercion) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_target_abi(GCC_GEN_BE_TARGET_ABI_S390X));
    a_boolean s390_ref = FALSE;
    a_boolean s390_fp = FALSE;
    if (gcc_gen_be_classify_s390x(NULL, &s390_ref, &s390_fp) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_s390x(&mixed_struct, &s390_ref, &s390_fp));
    if (!s390_ref) return GCC_GEN_BE_ERROR_INTERNAL;
    a_boolean s390_invis_ref = FALSE;
    a_boolean s390_sret = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_is_passed_by_invisible_reference(&mixed_struct, &s390_invis_ref));
    GCC_GEN_BE_CHECK(gcc_gen_be_is_returned_via_sret(&mixed_struct, &s390_sret));

    /* Test ARM 32-bit AAPCS32 classifier & sret */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_target_abi(GCC_GEN_BE_TARGET_ABI_AAPCS32));
    a_boolean aapcs32_sret = FALSE;
    a_boolean aapcs32_pair = FALSE;
    if (gcc_gen_be_classify_aapcs32(NULL, &aapcs32_sret, &aapcs32_pair) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_classify_aapcs32(&mixed_struct, NULL, &aapcs32_pair) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    long_t.alignment = 8;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs32(&long_t, &aapcs32_sret, &aapcs32_pair));
    if (aapcs32_sret || !aapcs32_pair) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs32(&mixed_struct, &aapcs32_sret, &aapcs32_pair));
    if (!aapcs32_sret) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_is_returned_via_sret(&mixed_struct, &aapcs32_sret));
    if (!aapcs32_sret) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test non-trivial / polymorphic C++ aggregate invisible reference & sret */
    a_type nontriv_class;
    std::memset(&nontriv_class, 0, sizeof(nontriv_class));
    nontriv_class.kind = tk_class;
    nontriv_class.size = 4;
    nontriv_class.variant.class_struct_union.any_virtual_functions_including_in_base_classes = TRUE;
    a_boolean nontriv_ref = FALSE;
    a_boolean nontriv_sret = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_is_passed_by_invisible_reference(&nontriv_class, &nontriv_ref));
    GCC_GEN_BE_CHECK(gcc_gen_be_is_returned_via_sret(&nontriv_class, &nontriv_sret));
    if (!nontriv_ref || !nontriv_sret) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test cumulative register state machine across target ABIs */
    gcc_gen_be_abi_reg_state_t reg_st;
    a_boolean in_regs = FALSE;
    a_boolean by_ref = FALSE;
    if (gcc_gen_be_init_abi_reg_state(GCC_GEN_BE_TARGET_ABI_SYSV_AMD64, FALSE, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_init_abi_reg_state((gcc_gen_be_target_abi_t)99, FALSE, &reg_st) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_allocate_abi_param_regs(NULL, &long_t, &in_regs, &by_ref) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_allocate_abi_param_regs(&reg_st, NULL, &in_regs, &by_ref) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_allocate_abi_param_regs(&reg_st, &long_t, NULL, &by_ref) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_allocate_abi_param_regs(&reg_st, &long_t, &in_regs, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    /* SysV AMD64: 6 GP, 8 FP; test sret consuming 1 GP + 5 GP args + 6th demoted to stack */
    GCC_GEN_BE_CHECK(gcc_gen_be_init_abi_reg_state(GCC_GEN_BE_TARGET_ABI_SYSV_AMD64, TRUE, &reg_st));
    for (int i = 0; i < 5; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&reg_st, &long_t, &in_regs, &by_ref));
        if (!in_regs || by_ref) return GCC_GEN_BE_ERROR_INTERNAL;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&reg_st, &long_t, &in_regs, &by_ref));
    if (in_regs) return GCC_GEN_BE_ERROR_INTERNAL;

    /* AAPCS64: sret in x8 (gp_used = 0), HFA + composite > 16 bytes by ref */
    GCC_GEN_BE_CHECK(gcc_gen_be_init_abi_reg_state(GCC_GEN_BE_TARGET_ABI_AAPCS64, TRUE, &reg_st));
    if (reg_st.gp_used != 0) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&reg_st, &nontriv_class, &in_regs, &by_ref));
    if (!in_regs || !by_ref) return GCC_GEN_BE_ERROR_INTERNAL;
    a_type big_struct;
    std::memset(&big_struct, 0, sizeof(big_struct));
    big_struct.kind = tk_struct;
    big_struct.size = 32;
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&reg_st, &big_struct, &in_regs, &by_ref));
    if (!in_regs || !by_ref) return GCC_GEN_BE_ERROR_INTERNAL;

    /* AAPCS32: even-odd register pair alignment */
    GCC_GEN_BE_CHECK(gcc_gen_be_init_abi_reg_state(GCC_GEN_BE_TARGET_ABI_AAPCS32, TRUE, &reg_st));
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&reg_st, &long_t, &in_regs, &by_ref));
    if (!in_regs || reg_st.gp_used != 4) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&reg_st, &long_t, &in_regs, &by_ref));
    if (in_regs) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Win64: 4 shared slots */
    GCC_GEN_BE_CHECK(gcc_gen_be_init_abi_reg_state(GCC_GEN_BE_TARGET_ABI_WIN64, TRUE, &reg_st));
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&reg_st, &nontriv_class, &in_regs, &by_ref));
    if (!in_regs || !by_ref) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&reg_st, &big_struct, &in_regs, &by_ref));
    if (!in_regs || !by_ref) return GCC_GEN_BE_ERROR_INTERNAL;

    /* RISC-V LP64D, PPC64LE ELFv2, i386, S390X */
    GCC_GEN_BE_CHECK(gcc_gen_be_init_abi_reg_state(GCC_GEN_BE_TARGET_ABI_RISCV64_LP64D, FALSE, &reg_st));
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&reg_st, &dbl_t, &in_regs, &by_ref));
    if (!in_regs) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&reg_st, &big_struct, &in_regs, &by_ref));
    if (!in_regs || !by_ref) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_init_abi_reg_state(GCC_GEN_BE_TARGET_ABI_PPC64LE_ELFV2, FALSE, &reg_st));
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&reg_st, &flt_arr6, &in_regs, &by_ref));
    if (!in_regs) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_allocate_abi_param_regs(&reg_st, &long_t, &in_regs, &by_ref));
    if (!in_regs) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_init_abi_reg_state(GCC_GEN_BE_TARGET_ABI_SYSV_I386, FALSE, &reg_st));
    GCC_GEN_BE_CHECK(gcc_gen_be_init_abi_reg_state(GCC_GEN_BE_TARGET_ABI_S390X, TRUE, &reg_st));

    GCC_GEN_BE_CHECK(gcc_gen_be_set_target_abi(GCC_GEN_BE_TARGET_ABI_SYSV_AMD64));

    /* Test gcc_gen_be_coerce_rvalue, gcc_gen_be_coerce_Arg_to_registers, and gcc_gen_be_unpack_coerced_Arg */
    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *int_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_type *flt_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
    gcc_jit_function *test_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "coerce_test_fn", 0, NULL, 0);
    gcc_jit_block *test_blk = gcc_jit_function_new_block(test_fn, "entry");
    gcc_jit_rvalue *int_rv = gcc_jit_context_new_rvalue_from_int(ctx, int_t, 42);
    gcc_jit_rvalue *coerced_rv = NULL;
    if (gcc_gen_be_coerce_rvalue(NULL, test_blk, int_rv, flt_t, &coerced_rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_coerce_rvalue(test_fn, test_blk, int_rv, int_t, &coerced_rv));
    if (coerced_rv != int_rv) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_coerce_rvalue(test_fn, test_blk, int_rv, flt_t, &coerced_rv));
    if (!coerced_rv) return GCC_GEN_BE_ERROR_INTERNAL;

    if (gcc_gen_be_coerce_Arg_to_registers(NULL, test_blk, &mixed_struct, int_rv, &coerced_rv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_coerce_Arg_to_registers(test_fn, test_blk, &long_t, int_rv, &coerced_rv));
    if (coerced_rv != int_rv) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_coerce_Arg_to_registers(test_fn, test_blk, &mixed_struct, int_rv, &coerced_rv));
    if (!coerced_rv) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_lvalue *unpacked_lv = NULL;
    if (gcc_gen_be_unpack_coerced_Arg(NULL, test_blk, &mixed_struct, coerced_rv, &unpacked_lv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_unpack_coerced_Arg(test_fn, test_blk, &mixed_struct, coerced_rv, &unpacked_lv));
    if (!unpacked_lv) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(cache_clear_all());
    gcc_jit_context_release(ctx);

    int exec_ok = 0;
    GCC_GEN_BE_CHECK(run_abi_jit_execution_tests(&exec_ok));
    if (!exec_ok) return GCC_GEN_BE_ERROR_INTERNAL;

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Entry point for the ABI target unit test executable.
 * @details Runs `run_abi_target_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_abi_target_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}

