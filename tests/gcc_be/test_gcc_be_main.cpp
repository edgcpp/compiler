/**
 * @file test_gcc_be_main.cpp
 * @brief Unit tests for compiler driver and orchestration (`gcc_gen_be_main.c`).
 * @details Tests multi-TU compilation, artifact emission, and command-line parsing.
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_main.h"
#include "gcc_gen_be_error.h"
#include "gcc_gen_be_context.h"
#include <libgccjit.h>
#include <cstdio>

extern "C" {
/**
 * @brief Mock diagnostic reporter for main driver unit tests.
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
/** @brief Assemble only option (`-S`). */
a_boolean gcc_be_assemble_only = FALSE;
/** @brief Compile only option (`-c`). */
a_boolean gcc_be_compile_only = TRUE;
/** @brief Output file name (`-o`). */
a_const_char *gcc_be_output_file_name = NULL;
/** @brief Primary source file name. */
a_const_char *primary_source_file_name = NULL;
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
/** @brief Driver options list pointer. */
void *gcc_be_driver_options = NULL;
/** @brief Compiler options list pointer. */
void *gcc_be_compiler_options = NULL;
/** @brief Active C dialect. */
a_C_dialect C_dialect = C_dialect_cplusplus;
/** @brief GCC compatibility mode flag. */
a_boolean gcc_mode = TRUE;
/** @brief Skip IL read flag. */
a_boolean skip_il_read = TRUE;
/** @brief IL output file handle. */
FILE *f_il_output = NULL;
/** @brief Mock IL header. */
an_il_header il_header;

/**
 * @brief Stub for checking nonreal types in main unit tests.
 * @details Returns `FALSE`.
 * @param[in] tp The frontend type pointer.
 * @return `FALSE`.
 */
a_boolean type_is_nonreal(a_type_ptr tp) {
    (void)tp;
    return FALSE;
}

/**
 * @brief Stub for IL file reading in main unit tests.
 * @details No-op for unit tests.
 * @param[in] f_il The IL file stream.
 */
void il_read(FILE *f_il) {
    (void)f_il;
}

/**
 * @brief Stub for global constructor block lookup in main unit tests.
 * @details Sets `*out_block` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[out] out_block Pointer to receive the global constructor block.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_get_global_ctor_block(struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    if (!out_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_block = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for global variable declaration lowering in main unit tests.
 * @details Sets `*out_lval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] var The frontend variable pointer.
 * @param[out] out_lval Pointer to receive the lowered lvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)var;
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for function declaration lowering in main unit tests.
 * @details Sets `*out_func` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] rout The frontend routine pointer.
 * @param[out] out_func Pointer to receive the lowered function.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
    (void)rout;
    if (!out_func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_func = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for type descriptor emission in main unit tests.
 * @details Sets `*out_descriptor` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] tp The frontend type pointer.
 * @param[out] out_descriptor Pointer to receive the lowered descriptor lvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_emit_type_descriptor(a_type_ptr tp, struct gcc_jit_lvalue **out_descriptor) GCC_GEN_BE_NOEXCEPT {
    (void)tp;
    if (!out_descriptor) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_descriptor = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for vtable emission in main unit tests.
 * @details Sets `*out_vtable` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] tp The frontend class type pointer.
 * @param[out] out_vtable Pointer to receive the lowered vtable lvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_emit_vtable(a_type_ptr tp, struct gcc_jit_lvalue **out_vtable) GCC_GEN_BE_NOEXCEPT {
    (void)tp;
    if (!out_vtable) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_vtable = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for VTT emission in main unit tests.
 * @details Sets `*out_vtt` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] tp The frontend class type pointer.
 * @param[out] out_vtt Pointer to receive the lowered VTT lvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_emit_vtt(a_type_ptr tp, struct gcc_jit_lvalue **out_vtt) GCC_GEN_BE_NOEXCEPT {
    (void)tp;
    if (!out_vtt) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_vtt = NULL;
    return GCC_GEN_BE_SUCCESS;
}
}

/**
 * @brief Executes driver early initialization, null-pointer validation, and option forwarding tests.
 * @details Percolates `gcc_gen_be_error_t` and writes completion status to `out_ok`.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_main_unit_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    GCC_GEN_BE_CHECK(gcc_gen_be_early_init());

    if (gcc_gen_be_apply_command_line_options(NULL, 0, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_compile_multi_tu(NULL, 0, 0, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_artifact(NULL, "obj", "out.o") != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_OOM;

    if (gcc_gen_be_emit_artifact(ctx, NULL, "out.o") != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        gcc_jit_context_release(ctx);
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    if (gcc_gen_be_emit_artifact(ctx, "obj", NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        gcc_jit_context_release(ctx);
        return GCC_GEN_BE_ERROR_INTERNAL;
    }

    const char *argv[] = {"-O3", "-g", "-fPIC", "-march=x86-64"};
    GCC_GEN_BE_CHECK(gcc_gen_be_apply_command_line_options(ctx, 4, (char **)argv));

    gcc_jit_context_release(ctx);
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_gcc_be_main.cpp`.
 * @details Runs `run_main_unit_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_main_unit_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}
