/**
 * @file test_gcc_be_location.cpp
 * @brief Unit tests for Source Location Tracking (`gcc_gen_be_location.c`).
 * @details Tests AST-to-location mapping with line and column coordinates and null-pointer handling.
 * @ingroup gcc_gen_be_tests
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_location.h"
#include "gcc_gen_be_error.h"
#include "gcc_gen_be_context.h"
#include <libgccjit.h>
#include <cstdio>

extern "C" {
/**
 * @brief Mock diagnostic reporter for location unit tests.
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

BEGIN_EDG_NAMESPACE
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
/** @brief Primary source file name. */
a_const_char *primary_source_file_name = "test_loc.cpp";
/** @brief Selects mock filename scenario in `conv_seq_to_file_and_line`. */
static int g_loc_mode = 0;

/**
 * @brief Stub for converting sequence number to file and line in location unit tests.
 * @details Populates dummy file name and line number according to `g_loc_mode`.
 * @param[in] seq Sequence number.
 * @param[out] file_name Output file name.
 * @param[out] full_name Output full path.
 * @param[out] line_number Output line number.
 * @param[out] at_end_of_source Output end-of-source flag.
 * @return NULL source file pointer.
 */
a_source_file_ptr conv_seq_to_file_and_line(
    a_seq_number seq,
    a_const_char **file_name,
    a_const_char **full_name,
    a_line_number *line_number,
    a_boolean *at_end_of_source) {
    if (line_number) *line_number = (a_line_number)(seq > 0 ? seq : 1);
    if (at_end_of_source) *at_end_of_source = FALSE;
    if (g_loc_mode == 1) {
        if (full_name) *full_name = "";
        if (file_name) *file_name = "short.cpp";
    } else if (g_loc_mode == 2) {
        if (full_name) *full_name = NULL;
        if (file_name) *file_name = "only_short.cpp";
    } else if (g_loc_mode == 3) {
        if (full_name) *full_name = NULL;
        if (file_name) *file_name = NULL;
    } else if (g_loc_mode == 4) {
        if (full_name) *full_name = NULL;
        if (file_name) *file_name = "";
    } else {
        if (full_name) *full_name = "/abs/test_loc.cpp";
        if (file_name) *file_name = "test_loc.cpp";
    }
    return NULL;
}
END_EDG_NAMESPACE

/**
 * @brief Executes source location mapping unit tests.
 * @details Verifies `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` when output pointer is NULL and `GCC_GEN_BE_SUCCESS` across all file/line/column cases.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_location_unit_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    if (gcc_gen_be_get_location(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_get_location_from_type(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_get_location_from_var(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_get_location_from_routine(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_get_location_from_expr(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_get_location_from_stmt(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    struct gcc_jit_location *loc = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location(NULL, &loc));
    a_source_position pos = {};
    pos.seq = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location(&pos, &loc));

    /* Missing active context returns GCC_GEN_BE_ERROR_INVALID_STATE */
    pos.seq = 10;
    g_loc_mode = 0;
    if (gcc_gen_be_get_location(&pos, &loc) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Acquire a real libgccjit context and exercise all g_loc_mode and column branches */
    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_OOM;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    pos.column = 0;
    for (int m = 0; m <= 4; ++m) {
        g_loc_mode = m;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_location(&pos, &loc));
    }
    g_loc_mode = 0;
    pos.column = 7;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location(&pos, &loc));

    GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_type(NULL, &loc));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_var(NULL, &loc));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_routine(NULL, &loc));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_expr(NULL, &loc));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_stmt(NULL, &loc));

    a_type tp = {};
    tp.source_corresp.decl_position = pos;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_type(&tp, &loc));

    a_variable var = {};
    var.source_corresp.decl_position = pos;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_var(&var, &loc));

    a_routine rout = {};
    rout.source_corresp.decl_position = pos;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_routine(&rout, &loc));

    an_expr_node expr = {};
    expr.position = pos;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_expr(&expr, &loc));
#if EXTRA_SOURCE_POSITIONS_IN_IL
    expr.expr_range.start = pos;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_expr(&expr, &loc));
#endif

    a_statement stmt = {};
    stmt.position = pos;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_stmt(&stmt, &loc));

    for (int b = 0; b < 5; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_get_location(&pos, &loc);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    gcc_jit_context_release(ctx);

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_gcc_be_location.cpp`.
 * @details Runs `run_location_unit_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) GCC_GEN_BE_NOEXCEPT {
    int ok = 0;
    gcc_gen_be_error_t err = run_location_unit_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}
