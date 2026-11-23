/**
 * @file test_gcc_be_driver.cpp
 * @brief Exhaustive unit tests for CLI flag forwarding, multi-TU compilation, native/LTO linking, and module initializers.
 * @details Exercises `gcc_gen_be_apply_command_line_options`, `gcc_gen_be_compile_multi_tu`,
 * `gcc_gen_be_link_multi_tu_objects`, `gcc_gen_be_link_multi_tu_artifacts`, `gcc_gen_be_emit_artifact`,
 * and `gcc_gen_be_emit_module_initializer` across all normal and error/OOM branches.
 * @ingroup gcc_gen_be_tests
 */

#include "basic_hdrs.h"
#include "fe_common.h"
#include "il.h"
#include "gcc_gen_be_main.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_error.h"
#include "gcc_gen_be_decl.h"
#include "gcc_gen_be_rtti.h"
#include <libgccjit.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
/**
 * @brief Mock diagnostic reporter for driver unit tests.
 * @details Returns the error code passed to it without terminating.
 * @param[in] err The error code.
 * @param[in] msg Optional diagnostic message.
 * @return The input error code `err`.
 */
gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t err, const char *msg) GCC_GEN_BE_NOEXCEPT {
    return msg ? err : err;
}
}

USING_NAMESPACE_EDG

BEGIN_EDG_NAMESPACE
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
/** @brief Skip IL read flag. */
a_boolean skip_il_read = TRUE;
/** @brief IL output file handle. */
FILE *f_il_output = NULL;
/** @brief Mock IL header. */
an_il_header il_header;

/**
 * @brief Stub for checking nonreal types in driver unit tests.
 * @details Returns `FALSE`.
 * @param[in] tp The frontend type pointer.
 * @return `FALSE`.
 */
a_boolean type_is_nonreal(a_type_ptr tp) {
    return tp ? FALSE : FALSE;
}

/**
 * @brief Stub for looking up routine scope in driver unit tests.
 * @details Resolves `il_header.function_def_table[rout->function_def_number].scope` when available.
 * @param[in] rout The frontend routine pointer.
 * @return Attached scope pointer or NULL.
 */
a_scope_ptr scope_for_routine(a_routine_ptr rout) {
    if (!rout || rout->function_def_number == NULL_function_def_number || !il_header.function_def_table) {
        return NULL;
    }
    return il_header.function_def_table[rout->function_def_number].scope;
}

/**
 * @brief Stub for IL file reading in driver unit tests.
 * @details No-op for unit tests.
 * @param[in] f_il The IL file stream.
 */
void il_read(FILE *f_il) GCC_GEN_BE_NOEXCEPT {
    if (f_il) {}
}

/**
 * @brief Stub for global constructor block lookup in driver unit tests.
 * @details Steps OOM budget and sets `*out_block` to NULL.
 * @param[out] out_block Pointer to receive the global constructor block.
 * @return `GCC_GEN_BE_SUCCESS` or error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_global_ctor_block(struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    if (!out_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_block = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for compile-unit DWARF debug info emission in driver unit tests.
 * @details Steps OOM budget and returns `GCC_GEN_BE_SUCCESS`.
 * @return `GCC_GEN_BE_SUCCESS` or error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_compile_unit_debug_info(void) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for registering OpenMP pragmas in scope in driver unit tests.
 * @details Steps OOM budget and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] scope Frontend scope pointer.
 * @return `GCC_GEN_BE_SUCCESS` or error code.
 */
gcc_gen_be_error_t gcc_gen_be_register_omp_pragmas_in_scope(a_scope_ptr scope) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return scope ? GCC_GEN_BE_SUCCESS : GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for canonical function mangler in driver unit tests.
 * @details Returns NULL.
 * @param[in] routine Frontend routine pointer.
 * @return NULL.
 */
char *get_mangled_function_name(a_routine_ptr routine) GCC_GEN_BE_NOEXCEPT {
    return routine ? nullptr : nullptr;
}

/**
 * @brief Stub for canonical variable mangler in driver unit tests.
 * @details Returns NULL.
 * @param[in] variable Frontend variable pointer.
 * @return NULL.
 */
a_const_char *get_mangled_variable_name(a_variable_ptr variable) GCC_GEN_BE_NOEXCEPT {
    return variable ? nullptr : nullptr;
}

/**
 * @brief Stub for canonical vtable mangler in driver unit tests.
 * @details Returns NULL.
 * @param[in] class_type Class type pointer.
 * @param[in] bcp Base class pointer.
 * @param[in] ctor_bcp Construction base class pointer.
 * @return NULL.
 */
char *mangled_vtbl_name(a_type_ptr class_type, a_base_class_ptr bcp, a_base_class_ptr ctor_bcp) GCC_GEN_BE_NOEXCEPT {
    return (class_type || bcp || ctor_bcp) ? nullptr : nullptr;
}

/**
 * @brief Stub for canonical typeinfo mangler in driver unit tests.
 * @details Returns NULL.
 * @param[in] type Frontend type pointer.
 * @return NULL.
 */
char *mangled_typeinfo_name(a_type_ptr type) GCC_GEN_BE_NOEXCEPT {
    return type ? nullptr : nullptr;
}

/**
 * @brief Stub for canonical typeinfo string mangler in driver unit tests.
 * @details Returns NULL.
 * @param[in] type Frontend type pointer.
 * @return NULL.
 */
char *mangled_typeinfo_string_name(a_type_ptr type) GCC_GEN_BE_NOEXCEPT {
    return type ? nullptr : nullptr;
}

/**
 * @brief Stub for canonical VTT mangler in driver unit tests.
 * @details Returns NULL.
 * @param[in] type Frontend type pointer.
 * @return NULL.
 */
char *mangled_virtual_table_table_name(a_type_ptr type) GCC_GEN_BE_NOEXCEPT {
    return type ? nullptr : nullptr;
}

/**
 * @brief Stub for global variable declaration lowering in driver unit tests.
 * @details Steps OOM budget and sets `*out_lval` to NULL.
 * @param[in] var The frontend variable pointer.
 * @param[out] out_lval Pointer to receive the lowered lvalue.
 * @return `GCC_GEN_BE_SUCCESS` or error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    if (!out_lval) return var ? GCC_GEN_BE_ERROR_INVALID_ARGUMENT : GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for function declaration lowering in driver unit tests.
 * @details Steps OOM budget and sets `*out_func` to NULL.
 * @param[in] rout The frontend routine pointer.
 * @param[out] out_func Pointer to receive the lowered function.
 * @return `GCC_GEN_BE_SUCCESS` or error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
    if (!out_func) return rout ? GCC_GEN_BE_ERROR_INVALID_ARGUMENT : GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_func = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for type descriptor emission in driver unit tests.
 * @details Steps OOM budget and sets `*out_descriptor` to NULL.
 * @param[in] tp The frontend type pointer.
 * @param[out] out_descriptor Pointer to receive the lowered descriptor lvalue.
 * @return `GCC_GEN_BE_SUCCESS` or error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_type_descriptor(a_type_ptr tp, struct gcc_jit_lvalue **out_descriptor) GCC_GEN_BE_NOEXCEPT {
    if (!out_descriptor) return tp ? GCC_GEN_BE_ERROR_INVALID_ARGUMENT : GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_descriptor = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for vtable emission in driver unit tests.
 * @details Steps OOM budget and sets `*out_vtable` to NULL.
 * @param[in] tp The frontend class type pointer.
 * @param[out] out_vtable Pointer to receive the lowered vtable lvalue.
 * @return `GCC_GEN_BE_SUCCESS` or error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_vtable(a_type_ptr tp, struct gcc_jit_lvalue **out_vtable) GCC_GEN_BE_NOEXCEPT {
    if (!out_vtable) return tp ? GCC_GEN_BE_ERROR_INVALID_ARGUMENT : GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_vtable = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for VTT emission in driver unit tests.
 * @details Steps OOM budget and sets `*out_vtt` to NULL.
 * @param[in] tp The frontend class type pointer.
 * @param[out] out_vtt Pointer to receive the lowered VTT lvalue.
 * @return `GCC_GEN_BE_SUCCESS` or error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_vtt(a_type_ptr tp, struct gcc_jit_lvalue **out_vtt) GCC_GEN_BE_NOEXCEPT {
    if (!out_vtt) return tp ? GCC_GEN_BE_ERROR_INVALID_ARGUMENT : GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_vtt = NULL;
    return GCC_GEN_BE_SUCCESS;
}
END_EDG_NAMESPACE

/**
 * @brief Exercises CLI flags, `gcc_gen_be_emit_artifact`, and `gcc_gen_be_emit_module_initializer`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_cli_artifact_and_modules(void) GCC_GEN_BE_NOEXCEPT {
    if (gcc_gen_be_apply_command_line_options(NULL, 0, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_artifact(NULL, ".o", "out.o") != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_OOM;

    if (gcc_gen_be_emit_artifact(ctx, NULL, "out.o") != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        gcc_jit_context_release(ctx);
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    if (gcc_gen_be_emit_artifact(ctx, ".o", NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        gcc_jit_context_release(ctx);
        return GCC_GEN_BE_ERROR_INTERNAL;
    }

    const char *flags[] = {
        "-O0", "-O1", "-O2", "-O3", "-Os", "-Ofast", "-Ox",
        "-g", "-g0", "-g1", "-g2", "-g3", "-gdwarf-5", "-ggdb", "-gz",
        "-fPIC", "-fPIE", "-fno-pie",
        "-ffast-math", "-fno-strict-aliasing", "-fwrapv", "-ftrapv",
        "-fdata-sections", "-ffunction-sections", "-fvisibility=hidden", "-fsanitize=address",
        "-L/tmp", "-lm", "-Wl,--as-needed", "-shared", "-static", "-pie", "-no-pie", "-pthread", "-rdynamic",
        "-soname,libtest.so", "-fopenmp", "-flto", "-march=native", "-mtune=generic", "-mcpu=generic",
        "-mfpu=neon", "-mcmodel=small", "--unrecognized-opt"
    };
    int num_flags = (int)(sizeof(flags) / sizeof(flags[0]));
    GCC_GEN_BE_CHECK(gcc_gen_be_apply_command_line_options(ctx, num_flags, (char **)flags));
    gcc_gen_be_singleton_state.openmp_enabled = FALSE;

    /* Test all artifact kind strings in gcc_gen_be_emit_artifact */
    gcc_jit_context_release(ctx);
    const char *kinds[] = { "asm", ".s", "obj", ".o", "dll", ".so", ".dylib", "other" };
    for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); ++i) {
        gcc_jit_context *kctx = gcc_jit_context_acquire();
#if defined(__APPLE__) && defined(LIBGCCJIT_HAVE_gcc_jit_context_add_driver_option)
        gcc_jit_context_add_driver_option(kctx, "-nodefaultlibs");
        gcc_jit_context_add_driver_option(kctx, "-lSystem");
#endif
        char outp[128];
        std::snprintf(outp, sizeof(outp), "/tmp/edg_kind_test_%zu.tmp", i);
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_artifact(kctx, kinds[i], outp));
        std::remove(outp);
        gcc_jit_context_release(kctx);
    }
    for (int ex = 0; ex < 2; ++ex) {
        gcc_jit_context *ectx = gcc_jit_context_acquire();
#if defined(__APPLE__) && defined(LIBGCCJIT_HAVE_gcc_jit_context_add_driver_option)
        gcc_jit_context_add_driver_option(ectx, "-nodefaultlibs");
        gcc_jit_context_add_driver_option(ectx, "-lSystem");
#endif
        gcc_jit_type *int_t = gcc_jit_context_get_type(ectx, GCC_JIT_TYPE_INT);
        gcc_jit_function *mfn = gcc_jit_context_new_function(ectx, NULL, GCC_JIT_FUNCTION_EXPORTED, int_t, "main", 0, NULL, 0);
        gcc_jit_block *mblk = gcc_jit_function_new_block(mfn, "entry");
        gcc_jit_block_end_with_return(mblk, NULL, gcc_jit_context_zero(ectx, int_t));
        const char *ekind = (ex == 0) ? "exe" : ".out";
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_artifact(ectx, ekind, "/tmp/edg_kind_exe.tmp"));
        std::remove("/tmp/edg_kind_exe.tmp");
        gcc_jit_context_release(ectx);
    }
    /* Compilation error branch (first_err != NULL) */
    {
        gcc_jit_context *bctx = gcc_jit_context_acquire();
        if (gcc_gen_be_emit_artifact(bctx, ".o", "/nonexistent_edg_dir/fail.o") != GCC_GEN_BE_ERROR_COMPILATION_FAILED) {
            gcc_jit_context_release(bctx);
            return GCC_GEN_BE_ERROR_INTERNAL;
        }
        gcc_jit_context_release(bctx);
    }

    /* Module initializer tests (short, dotted, >250 char segment loop limit, partition, empty, global_ctor_func, OOM) */
    gcc_jit_function *mod_fn = NULL;
    if (gcc_gen_be_emit_module_initializer(NULL, &mod_fn) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_module_initializer("", &mod_fn) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_module_initializer("math", NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_emit_module_initializer("math", &mod_fn) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;

    ctx = gcc_jit_context_acquire();
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    char long_mod_a[280];
    char long_mod_b[280];
    std::memset(long_mod_a, 'a', sizeof(long_mod_a));
    std::memset(long_mod_b, 'b', sizeof(long_mod_b));
    for (int k = 20; k < 260; k += 25) {
        long_mod_a[k] = '.';
        long_mod_b[k] = '.';
    }
    /* Trailing dot exercises `*p == '\0'` loop exit! */
    long_mod_a[10] = '.';
    long_mod_a[11] = '\0';
    long_mod_b[270] = '\0';

    char long_mod_c[280];
    std::memset(long_mod_c, 'c', sizeof(long_mod_c));
    for (int k = 20; k < 260; k += 25) long_mod_c[k] = '.';
    long_mod_c[270] = '\0';

    a_module m_null_name = {}, m_empty_name = {}, m_hdr = {}, m_part = {}, m_long = {}, m_trail = {};
    m_null_name.kind = mk_unit; m_null_name.variant.unit.name = NULL;
    m_empty_name.kind = mk_unit; m_empty_name.variant.unit.name = "";
    m_hdr.kind = mk_header_unit;
    m_part.kind = mk_unit_partition; m_part.variant.unit_partition.name = "sub.part";
    m_long.kind = mk_unit; m_long.variant.unit.name = long_mod_b;
    m_trail.kind = mk_unit; m_trail.variant.unit.name = long_mod_a;

    a_module_import_decl d0 = {}, d1 = {}, d2 = {}, d3 = {}, d4 = {}, d5 = {}, d6 = {};
    d0.module_info = NULL; d0.next = &d1;
    d1.module_info = &m_null_name; d1.next = &d2;
    d2.module_info = &m_empty_name; d2.next = &d3;
    d3.module_info = &m_hdr; d3.next = &d4;
    d4.module_info = &m_part; d4.next = &d5;
    d5.module_info = &m_long; d5.next = &d6;
    d6.module_info = &m_trail;
    il_header.imported_modules = &d0;

    gcc_jit_type *vt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_function *gctor = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, vt, "__mod_gctor", 0, NULL, 0);
    gcc_jit_block *gblk = gcc_jit_function_new_block(gctor, "entry");
    gcc_jit_block_end_with_void_return(gblk, NULL);
    gcc_gen_be_singleton_state.global_ctor_func = gctor;

    GCC_GEN_BE_CHECK(gcc_gen_be_emit_module_initializer(long_mod_a, &mod_fn));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_module_initializer(long_mod_c, &mod_fn));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_module_initializer("sub.part", &mod_fn));

    for (int b = 0; b < 6; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_emit_module_initializer("m_oom", &mod_fn);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    gcc_gen_be_singleton_state.global_ctor_func = NULL;
    il_header.imported_modules = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    gcc_jit_context_release(ctx);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exercises `gcc_gen_be_compile_multi_tu` and `gcc_gen_be_link_multi_tu_artifacts` across all branches.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_multi_tu_and_linking(void) GCC_GEN_BE_NOEXCEPT {
    if (gcc_gen_be_compile_multi_tu(NULL, 1, 0, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    const char *tu_valid[2] = { "/tmp/edg_drv_tu1", "/tmp/edg_drv_tu2" };
    if (gcc_gen_be_compile_multi_tu(tu_valid, 0, 0, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    const char *tu_with_null[2] = { "/tmp/edg_drv_tu1", NULL };
    if (gcc_gen_be_compile_multi_tu(tu_with_null, 2, 0, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    std::remove("/tmp/edg_drv_tu1.o");

    /* Debug info location creation & tu_loc OOM failure, IL read, primary_scope walkers, and multi-TU link step */
    a_scope pscope = {};
    pscope.kind = sck_file;
    il_header.primary_scope = &pscope;
    gcc_be_debug_info = FALSE;
    gcc_gen_be_singleton_state.debug_level = 2;
    skip_il_read = FALSE;
    FILE *tmp_il = std::tmpfile();
    f_il_output = tmp_il;
    gcc_be_compile_only = FALSE;
    gcc_be_assemble_only = FALSE;
    gcc_be_output_file_name = "/tmp/edg_drv_multi.out";
    setenv("EDG_GCC_DRIVER_BIN", "/usr/bin/true", 1);

    GCC_GEN_BE_CHECK(gcc_gen_be_compile_multi_tu(tu_valid, 2, 0, NULL));

    /* Also test gcc_be_debug_info = TRUE while debug_level = 0, and f_il_output = NULL while !skip_il_read */
    gcc_be_debug_info = TRUE;
    gcc_gen_be_singleton_state.debug_level = 0;
    f_il_output = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_compile_multi_tu(tu_valid, 1, 0, NULL));

    /* Deterministic OOM sweep across gcc_gen_be_compile_multi_tu (hits saved_ctx, tu_ctx, cli opts, cache_clear,
       set_context, get_state, tu_loc DWARF error, all 3 scope walkers, synthesize_ctors, emit_artifact j<i cleanup,
       obj_list, obj_bufs, and link_multi_tu_objects!) */
    for (int b = 0; b < 60; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_compile_multi_tu(tu_valid, 2, 0, NULL);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* Also test gcc_be_assemble_only = TRUE, gcc_be_compile_only = TRUE, and gcc_be_output_file_name = NULL branches skipping link step */
    gcc_be_assemble_only = TRUE;
    gcc_be_debug_info = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_compile_multi_tu(tu_valid, 1, 0, NULL));
    gcc_be_assemble_only = FALSE;
    gcc_be_output_file_name = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_compile_multi_tu(tu_valid, 1, 0, NULL));
    gcc_be_compile_only = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_compile_multi_tu(tu_valid, 1, 0, NULL));
    if (tmp_il) std::fclose(tmp_il);
    f_il_output = NULL;
    skip_il_read = TRUE;
    il_header.primary_scope = NULL;
    unsetenv("EDG_GCC_DRIVER_BIN");

    /* Exhaustive gcc_gen_be_link_multi_tu_artifacts & locate_eh_runtime_archive tests */
    if (gcc_gen_be_link_multi_tu_artifacts(NULL, 1, "/tmp/o", 0, NULL) != GCC_GEN_BE_ERROR_MULTI_TU_PIPELINE_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_link_multi_tu_artifacts(tu_valid, 0, "/tmp/o", 0, NULL) != GCC_GEN_BE_ERROR_MULTI_TU_PIPELINE_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_link_multi_tu_artifacts(tu_valid, 1, NULL, 0, NULL) != GCC_GEN_BE_ERROR_MULTI_TU_PIPELINE_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_link_multi_tu_artifacts(tu_valid, 1, "", 0, NULL) != GCC_GEN_BE_ERROR_MULTI_TU_PIPELINE_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    const char *null_obj[1] = { NULL };
    const char *empty_obj[1] = { "" };
    if (gcc_gen_be_link_multi_tu_artifacts(null_obj, 1, "/tmp/o", 0, NULL) != GCC_GEN_BE_ERROR_MULTI_TU_PIPELINE_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_link_multi_tu_artifacts(empty_obj, 1, "/tmp/o", 0, NULL) != GCC_GEN_BE_ERROR_MULTI_TU_PIPELINE_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;

    const char *real_objs[2] = { "/tmp/edg_drv_tu1.o", "/tmp/edg_drv_tu2.o" };

    /* Test locate_eh_runtime_archive env overrides:
       1. EDG_GCC_EH_RT_LIB="" (empty disables)
       2. EDG_GCC_EH_RT_LIB="/tmp/edg_drv_tu1.o" (existing readable file)
       3. EDG_GCC_EH_RT_LIB="/nonexistent_eh.a" + EDG_GCC_EH_RT_DIR="/nonexistent_dir" (exhausts candidate loop!)
       4. EDG_GCC_EH_RT_DIR="" (falls back to dladdr) */
    setenv("EDG_GCC_DRIVER_BIN", "/usr/bin/true", 1);
    setenv("EDG_GCC_EH_RT_LIB", "", 1);
    GCC_GEN_BE_CHECK(gcc_gen_be_link_multi_tu_artifacts(real_objs, 2, "/tmp/edg_lnk1", 0, NULL));
    setenv("EDG_GCC_EH_RT_LIB", "/tmp/edg_drv_tu1.o", 1);
    GCC_GEN_BE_CHECK(gcc_gen_be_link_multi_tu_artifacts(real_objs, 2, "/tmp/edg_lnk1", 0, NULL));
    setenv("EDG_GCC_EH_RT_LIB", "/nonexistent_edg_eh_rt.a", 1);
    setenv("EDG_GCC_EH_RT_DIR", "/nonexistent_edg_eh_dir", 1);
    GCC_GEN_BE_CHECK(gcc_gen_be_link_multi_tu_artifacts(real_objs, 2, "/tmp/edg_lnk1", 0, NULL));
    setenv("EDG_GCC_EH_RT_DIR", "", 1);
    unsetenv("EDG_GCC_EH_RT_LIB");
    GCC_GEN_BE_CHECK(gcc_gen_be_link_multi_tu_artifacts(real_objs, 2, "/tmp/edg_lnk1", 0, NULL));
    unsetenv("EDG_GCC_EH_RT_DIR");

    /* Test C_mode() == TRUE, static, pthread, openmp, needs_libatomic, sanitizers (bits 0,1,2,4),
       driver_lib_dirs/libs/ldflags (with NULL & non-NULL entries), argv with NULL & -shared & passthrough,
       and EDG_GCC_LIB_DIR ("NONE", "", and temp dir with dummy libgomp.dylib / libatomic.dylib)! */
    C_dialect = C_dialect_ANSI;
    gcc_be_shared = TRUE;
    gcc_be_static = TRUE;
    gcc_gen_be_singleton_state.pthread_enabled = TRUE;
    gcc_gen_be_singleton_state.openmp_enabled = TRUE;
    gcc_gen_be_singleton_state.needs_libatomic = TRUE;
    gcc_gen_be_singleton_state.sanitizers = (1U << 0) | (1U << 1) | (1U << 2) | (1U << 4);

    const char *drv_dirs[2] = { "/tmp", NULL };
    const char *drv_libs[2] = { "m", NULL };
    const char *drv_ld[2] = { "-Wl,-v", NULL };
    edg_be_invocation_options_t lopts = {};
    lopts.artifact_kind = edg_output_artifact_kind_t::shared_library;
    lopts.static_link = true;
    lopts.openmp_enabled = true;
    lopts.pthread_enabled = true;
    lopts.library_search_dirs = drv_dirs;
    lopts.num_library_search_dirs = 2;
    lopts.libraries = drv_libs;
    lopts.num_libraries = 2;
    lopts.linker_passthrough_args = drv_ld;
    lopts.num_linker_passthrough_args = 2;
    GCC_GEN_BE_CHECK(gcc_gen_be_apply_invocation_options(&lopts));
    gcc_gen_be_singleton_state.sanitizers = (1U << 0) | (1U << 1) | (1U << 2) | (1U << 4);
    gcc_gen_be_singleton_state.needs_libatomic = TRUE;

    const char *mix_argv[3] = { "-shared", NULL, "-v" };
    /* 1. EDG_GCC_LIB_DIR="NONE" (skips loop) */
    setenv("EDG_GCC_LIB_DIR", "NONE", 1);
    GCC_GEN_BE_CHECK(gcc_gen_be_link_multi_tu_artifacts(real_objs, 2, "/tmp/edg_lnk2", 3, (char **)mix_argv));
    /* 2. EDG_GCC_LIB_DIR="" (probes default dirs) */
    setenv("EDG_GCC_LIB_DIR", "", 1);
    GCC_GEN_BE_CHECK(gcc_gen_be_link_multi_tu_artifacts(real_objs, 2, "/tmp/edg_lnk2", 2, NULL));
    /* 3. EDG_GCC_LIB_DIR="/tmp" with dummy libgomp.dylib & libatomic.dylib (guarantees hit even if Homebrew GCC absent) */
    FILE *fgomp = std::fopen("/tmp/libgomp.dylib", "wb");
    if (fgomp) std::fclose(fgomp);
    FILE *fatom = std::fopen("/tmp/libatomic.dylib", "wb");
    if (fatom) std::fclose(fatom);
    setenv("EDG_GCC_LIB_DIR", "/tmp", 1);
    GCC_GEN_BE_CHECK(gcc_gen_be_link_multi_tu_artifacts(real_objs, 2, "/tmp/edg_lnk2", 3, (char **)mix_argv));

    /* OOM sweep across native link path (covers locate_eh_runtime_archive, cmd, owned, -L, -l, lflag, rflag!) */
    for (int b = 0; b < 20; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_link_multi_tu_artifacts(real_objs, 2, "/tmp/edg_lnk2", 3, (char **)mix_argv);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    std::remove("/tmp/libgomp.dylib");
    std::remove("/tmp/libatomic.dylib");
    unsetenv("EDG_GCC_LIB_DIR");

    /* Spawn failure & non-zero exit code branches */
    setenv("EDG_GCC_DRIVER_BIN", "/nonexistent_edg_driver_xyz", 1);
    if (gcc_gen_be_link_multi_tu_artifacts(real_objs, 1, "/tmp/edg_lnk_fail", 0, NULL) != GCC_GEN_BE_ERROR_MULTI_TU_PIPELINE_FAILED) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    setenv("EDG_GCC_DRIVER_BIN", "/usr/bin/false", 1);
    if (gcc_gen_be_link_multi_tu_artifacts(real_objs, 1, "/tmp/edg_lnk_fail", 0, NULL) != GCC_GEN_BE_ERROR_MULTI_TU_PIPELINE_FAILED) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    /* Empty AND unset EDG_GCC_DRIVER_BIN both fall back to default cc/c++ */
    setenv("EDG_GCC_DRIVER_BIN", "", 1);
    gcc_be_static = FALSE;
    gcc_gen_be_singleton_state.sanitizers = 0;
    gcc_gen_be_singleton_state.openmp_enabled = FALSE;
    gcc_gen_be_singleton_state.needs_libatomic = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
    GCC_GEN_BE_CHECK(gcc_gen_be_link_multi_tu_artifacts(real_objs, 2, "/tmp/edg_lnk_real.so", 0, NULL));
    std::remove("/tmp/edg_lnk_real.so");
    unsetenv("EDG_GCC_DRIVER_BIN");
    GCC_GEN_BE_CHECK(gcc_gen_be_link_multi_tu_artifacts(real_objs, 2, "/tmp/edg_lnk_real2.so", 0, NULL));
    std::remove("/tmp/edg_lnk_real2.so");

    /* Fallback libgccjit link_ctx path:
       1. Missing object file (`all_objs_exist == FALSE`), C++ mode, eh_rt found & empty, openmp, pthread, shared & exe
       2. `has_lto == TRUE` via `-flto` in argv and via `state.lto_mode = 1`, C mode */
    C_dialect = C_dialect_cplusplus;
    gcc_be_shared = FALSE;
    gcc_gen_be_singleton_state.openmp_enabled = TRUE;
    gcc_gen_be_singleton_state.pthread_enabled = TRUE;
    const char *missing_objs[1] = { "/tmp/edg_missing_obj_xyz.o" };
    const char *lto_argv[3] = { "-flto", NULL, "-shared" };
    if (gcc_gen_be_link_multi_tu_artifacts(missing_objs, 1, "/nonexistent_edg_dir/out", 3, (char **)lto_argv) != GCC_GEN_BE_ERROR_COMPILATION_FAILED) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    /* OOM on link_ctx acquire */
    g_gcc_be_oom_countdown = 1;
    if (gcc_gen_be_link_multi_tu_artifacts(missing_objs, 1, "/tmp/edg_lto_oom", 0, NULL) != GCC_GEN_BE_ERROR_OOM) {
        g_gcc_be_oom_countdown = -1;
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    g_gcc_be_oom_countdown = -1;

    /* C++ mode with empty eh_rt_path and openmp/pthread FALSE, executable out_kind (".out") */
    setenv("EDG_GCC_EH_RT_LIB", "", 1);
    gcc_gen_be_singleton_state.openmp_enabled = FALSE;
    gcc_gen_be_singleton_state.pthread_enabled = FALSE;
    gcc_gen_be_singleton_state.lto_mode = 1;
    if (gcc_gen_be_link_multi_tu_artifacts(real_objs, 1, "/nonexistent_edg_dir/out", 1, NULL) != GCC_GEN_BE_ERROR_COMPILATION_FAILED) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    unsetenv("EDG_GCC_EH_RT_LIB");

    /* C mode on link_ctx path */
    C_dialect = C_dialect_ANSI;
    if (gcc_gen_be_link_multi_tu_artifacts(real_objs, 1, "/nonexistent_edg_dir/out_c", 0, NULL) != GCC_GEN_BE_ERROR_COMPILATION_FAILED) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    C_dialect = C_dialect_cplusplus;
    gcc_gen_be_singleton_state.lto_mode = 0;

    int link_status = -1;
    if (gcc_gen_be_link_multi_tu_objects(real_objs, 1, "/tmp/o", 0, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    if (gcc_gen_be_link_multi_tu_objects(empty_obj, 1, "/tmp/o", 0, NULL, &link_status) != GCC_GEN_BE_ERROR_MULTI_TU_PIPELINE_FAILED) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }

    std::remove("/tmp/edg_drv_tu1.o");
    std::remove("/tmp/edg_drv_tu2.o");
    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Runs all compiler driver, multi-TU, and artifact emission unit tests.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t run_driver_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    GCC_GEN_BE_CHECK(gcc_gen_be_early_init());
    GCC_GEN_BE_CHECK(test_cli_artifact_and_modules());
    GCC_GEN_BE_CHECK(test_multi_tu_and_linking());

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Entry point for the driver unit test executable.
 * @details Runs `run_driver_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) noexcept {
    int ok = 0;
    gcc_gen_be_error_t err = run_driver_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}
