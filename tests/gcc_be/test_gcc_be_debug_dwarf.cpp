/**
 * @file test_gcc_be_debug_dwarf.cpp
 * @brief Unit tests for DWARF debug metadata generation (`gcc_gen_be_debug.c`).
 * @details Tests `gcc_gen_be_initialize_debug`, `gcc_gen_be_emit_compile_unit_debug_info`,
 * `gcc_gen_be_enable_debug_info`, `gcc_gen_be_emit_subprogram_debug_info`,
 * `gcc_gen_be_emit_type_debug_info`, `gcc_gen_be_emit_variable_debug_info`, and
 * `gcc_gen_be_emit_block_debug_info`.
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_debug.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_error.h"
#include <libgccjit.h>
#include <cstdio>
#include <cstring>

extern "C" {
/**
 * @brief Mock diagnostic reporter for DWARF debug unit tests.
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
a_boolean gcc_be_debug_info = TRUE;
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
/** @brief Primary source file name. */
a_const_char *primary_source_file_name = "test_debug.cpp";
/** @brief Mock IL header. */
an_il_header il_header;

/**
 * @brief Stub for sequence number to file/line conversion in DWARF debug unit tests.
 * @details Maps non-zero sequence numbers to `test_debug.cpp:42`.
 * @param[in] seq Sequence number.
 * @param[out] file_name Pointer to receive file name.
 * @param[out] full_name Pointer to receive full path name.
 * @param[out] line_number Pointer to receive line number.
 * @param[out] at_end_of_source Pointer to receive end-of-source flag.
 * @return NULL.
 */
a_source_file_ptr conv_seq_to_file_and_line(
    a_seq_number seq,
    a_const_char **file_name,
    a_const_char **full_name,
    a_line_number *line_number,
    a_boolean *at_end_of_source) {
    if (file_name) *file_name = (seq != 0) ? "test_debug.cpp" : NULL;
    if (full_name) *full_name = (seq != 0) ? "test_debug.cpp" : NULL;
    if (line_number) *line_number = 42;
    if (at_end_of_source) *at_end_of_source = FALSE;
    return NULL;
}
}

/**
 * @brief Runs all DWARF debug metadata emission unit tests.
 * @details Exercises `gcc_gen_be_enable_debug_info`, `gcc_gen_be_initialize_debug`,
 * `gcc_gen_be_emit_compile_unit_debug_info`, `gcc_gen_be_emit_subprogram_debug_info`,
 * `gcc_gen_be_emit_type_debug_info`, `gcc_gen_be_emit_variable_debug_info`, and
 * `gcc_gen_be_emit_block_debug_info`.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_debug_dwarf_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    if (gcc_gen_be_enable_debug_info(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_subprogram_debug_info(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_type_debug_info(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_variable_debug_info(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_block_debug_info(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));
    GCC_GEN_BE_CHECK(cache_clear_all());

    GCC_GEN_BE_CHECK(gcc_gen_be_initialize_debug());
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_compile_unit_debug_info());

    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_function *func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, int_type, "dbg_fn", 0, NULL, 0);
    gcc_jit_block *blk = gcc_jit_function_new_block(func, "entry");
    gcc_jit_lvalue *lval = gcc_jit_function_new_local(func, NULL, int_type, "dbg_local");

    a_routine rout;
    std::memset(&rout, 0, sizeof(rout));
    rout.source_corresp.decl_position.seq = 1;
    rout.compiler_generated = TRUE;
    rout.is_inline = TRUE;
    rout.keep_definition_in_il = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_subprogram_debug_info(&rout, func));

    a_template_arg r_targ;
    std::memset(&r_targ, 0, sizeof(r_targ));
    rout.source_corresp.name = "virt_tmpl_fn";
    rout.source_corresp.unmangled_name_or_mangled_encoding = "_ZN3Cls12virt_tmpl_fnIiEEvv";
    rout.is_virtual = TRUE;
    rout.virtual_function_number = 2;
    rout.template_arg_list = &r_targ;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_subprogram_debug_info(&rout, func));

    a_type tp;
    std::memset(&tp, 0, sizeof(tp));
    tp.source_corresp.decl_position.seq = 2;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_debug_info(&tp, int_type));

    a_variable var;
    std::memset(&var, 0, sizeof(var));
    var.source_corresp.decl_position.seq = 3;
    var.source_corresp.name = "dbg_local";
    var.alignment = 8;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_variable_debug_info(&var, lval));

    a_statement stmt;
    std::memset(&stmt, 0, sizeof(stmt));
    stmt.position.seq = 4;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_block_debug_info(&stmt, blk));
    if (gcc_gen_be_emit_lexical_block_scope(NULL, blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_lexical_block_scope(&stmt, blk));
    if (gcc_gen_be_emit_inlined_subroutine(NULL, &stmt, blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    rout.source_corresp.name = "inlined_helper";
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_inlined_subroutine(&rout, &stmt, blk));

    if (gcc_gen_be_emit_dwarf_info(NULL, 4, 2) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_dwarf_info(ctx, 4, -1) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_dwarf_info(ctx, 4, 0));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_dwarf_info(ctx, 4, 1));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_dwarf_info(ctx, 5, 2));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_dwarf_info(ctx, 5, 3));

    gcc_jit_location *ret_loc = gcc_jit_context_new_location(ctx, "test_debug.cpp", 42, 5);
    gcc_jit_block_add_assignment(blk, ret_loc, lval, gcc_jit_context_new_rvalue_from_int(ctx, int_type, 7));
    gcc_jit_block_end_with_return(blk, ret_loc, gcc_jit_lvalue_as_rvalue(lval));

    const char *asm_path = "/tmp/test_gcc_be_dwarf5.s";
    std::remove(asm_path);
    gcc_jit_context_compile_to_file(ctx, GCC_JIT_OUTPUT_KIND_ASSEMBLER, asm_path);
    FILE *afp = std::fopen(asm_path, "r");
    if (!afp) {
        gcc_jit_context_release(ctx);
        return GCC_GEN_BE_ERROR_COMPILATION_FAILED;
    }
    char line_buf[512];
    bool saw_debug = false;
    while (std::fgets(line_buf, sizeof(line_buf), afp) != NULL) {
        if (std::strstr(line_buf, "debug_info") != NULL ||
            std::strstr(line_buf, "debug_line") != NULL ||
            std::strstr(line_buf, "test_debug.cpp") != NULL) {
            saw_debug = true;
            break;
        }
    }
    std::fclose(afp);
    std::remove(asm_path);
    if (!saw_debug) {
        gcc_jit_context_release(ctx);
        return GCC_GEN_BE_ERROR_INTERNAL;
    }

    GCC_GEN_BE_CHECK(cache_clear_all());
    gcc_jit_context_release(ctx);

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Entry point for the DWARF debug unit test executable.
 * @details Runs `run_debug_dwarf_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_debug_dwarf_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}

