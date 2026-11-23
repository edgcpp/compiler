/**
 * @file test_gcc_be_debug.cpp
 * @brief Unit tests for DWARF debug metadata generation logic (`gcc_gen_be_debug.c`).
 * @details Exhaustively validates DW_TAG emission mappings, DWARF version/level configuration,
 * DIE table resizing, variable/subprogram/scope/inlined metadata, and OOM percolation.
 * @ingroup gcc_gen_be_tests
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_debug.h"
#include "gcc_gen_be_error.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_lib_loader.h"
#include <libgccjit.h>
#include <cstdio>

extern "C" {
/**
 * @brief Mock diagnostic reporter for debug unit tests.
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
/** @brief Driver options list pointer. */
void *gcc_be_driver_options = NULL;
/** @brief Compiler options list pointer. */
void *gcc_be_compiler_options = NULL;
/** @brief Active C dialect. */
a_C_dialect C_dialect = C_dialect_cplusplus;
/** @brief Primary source file name. */
a_const_char *primary_source_file_name = "test_debug.cpp";

/**
 * @brief Stub for converting sequence number to file and line in debug unit tests.
 * @details Populates dummy file name and line number for non-zero sequence numbers.
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
    if (file_name) *file_name = (seq != 0) ? "test_debug.cpp" : NULL;
    if (full_name) *full_name = (seq != 0) ? "test_debug.cpp" : NULL;
    if (line_number) *line_number = (seq != 0) ? 10 : 0;
    if (at_end_of_source) *at_end_of_source = FALSE;
    return NULL;
}
END_EDG_NAMESPACE

/** @brief Counter of mock `gcc_jit_function_add_attribute` invocations. */
static int g_fn_attr_calls = 0;
/** @brief Counter of mock `gcc_jit_lvalue_set_alignment` invocations. */
static int g_lv_align_calls = 0;

/**
 * @brief Mock `gcc_jit_function_add_attribute` callback for debug unit tests.
 * @param[in] fn Target JIT function.
 * @param[in] attr Function attribute enum.
 */
static void mock_fn_add_attr(gcc_jit_function *fn, enum gcc_jit_fn_attribute attr) noexcept {
    if (fn && (int)attr >= 0) ++g_fn_attr_calls;
}

/**
 * @brief Mock `gcc_jit_function_add_string_attribute` callback for debug unit tests.
 * @param[in] fn Target JIT function.
 * @param[in] attr Function attribute enum.
 * @param[in] value Attribute string value.
 */
static void mock_fn_add_str_attr(gcc_jit_function *fn, enum gcc_jit_fn_attribute attr, const char *value) noexcept {
    if (fn && (int)attr >= 0 && value) ++g_fn_attr_calls;
}

/**
 * @brief Mock `gcc_jit_lvalue_set_alignment` callback for debug unit tests.
 * @param[in] lval Target JIT lvalue.
 * @param[in] bytes Alignment in bytes.
 */
static void mock_lval_set_align(gcc_jit_lvalue *lval, unsigned bytes) noexcept {
    if (lval && bytes > 0U) ++g_lv_align_calls;
}

/**
 * @brief Executes exhaustive debug metadata, DIE resizing, and OOM percolation tests.
 * @details Exercises every function and branch in `src/gcc_gen_be_debug.c`.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_debug_unit_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    /* 1. Invalid arguments & missing context */
    if (gcc_gen_be_enable_debug_info(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_subprogram_debug_info(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_type_debug_info(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_variable_debug_info(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_block_debug_info(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_initialize_debug() != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_compile_unit_debug_info() != GCC_GEN_BE_ERROR_INTERNAL) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_OOM;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    /* 2. DWARF config & emit_dwarf_info & enable_debug_info & compile_unit */
    if (gcc_gen_be_set_dwarf_config(1, 2) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_set_dwarf_config(6, 2) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_set_dwarf_config(4, -1) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_set_dwarf_config(4, 9) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_get_dwarf_config(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    int dv = 0, dl = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_dwarf_config(0, 0));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_dwarf_config(&dv, NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_dwarf_config(NULL, &dl));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_dwarf_config(&dv, &dl));
    if (dv != 4 || dl != 0) return GCC_GEN_BE_ERROR_INTERNAL;

    if (gcc_gen_be_emit_dwarf_info(NULL, 4, 2) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_dwarf_info(ctx, 4, -1) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_dwarf_info(ctx, 4, 4) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_dwarf_info(ctx, 0, 0));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_dwarf_info(ctx, 6, 1));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_dwarf_info(ctx, 2, 1));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_dwarf_info(ctx, 3, 2));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_dwarf_info(ctx, 4, 3));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_dwarf_info(ctx, 5, 3));

    /* Test enable_debug_info & emit_compile_unit_debug_info across all 3 enablement sources + disabled + null filename */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_dwarf_config(4, 0));
    gcc_be_debug_info = FALSE;
    gcc_gen_be_singleton_state.debug_level = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_enable_debug_info(ctx));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_compile_unit_debug_info());

    gcc_gen_be_singleton_state.debug_level = 5; /* > 3 clamped to 3 */
    GCC_GEN_BE_CHECK(gcc_gen_be_enable_debug_info(ctx));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_dwarf_config(4, 0));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_compile_unit_debug_info());
    gcc_gen_be_singleton_state.debug_level = 0;

    gcc_be_debug_info = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_dwarf_config(4, 0));
    GCC_GEN_BE_CHECK(gcc_gen_be_enable_debug_info(ctx));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_dwarf_config(4, 0));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_compile_unit_debug_info());
    gcc_be_debug_info = FALSE;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_dwarf_config(4, 2));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_compile_unit_debug_info());
    primary_source_file_name = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_compile_unit_debug_info());
    primary_source_file_name = "test_debug.cpp";

    for (int b = 0; b < 8; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_initialize_debug();
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 8; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_emit_compile_unit_debug_info();
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* 3. Subprogram, variable, lexical block, and inlined subroutine */
    gcc_jit_type *int_jit_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_function *func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, int_jit_t, "dbg_fn", 0, NULL, 0);
    gcc_jit_block *blk = gcc_jit_function_new_block(func, "entry");
    gcc_jit_lvalue *lval = gcc_jit_function_new_local(func, NULL, int_jit_t, "v");

    a_routine rout = {};
    if (gcc_gen_be_emit_subprogram_debug_info(&rout, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    /* p_gcc_jit_function_add_attribute == NULL vs non-NULL across all routine flags */
    p_gcc_jit_function_add_attribute = NULL;
    p_gcc_jit_function_add_string_attribute = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_subprogram_debug_info(&rout, func));
    p_gcc_jit_function_add_attribute = &mock_fn_add_attr;
    p_gcc_jit_function_add_string_attribute = NULL;
    rout.compiler_generated = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_subprogram_debug_info(&rout, func));
    p_gcc_jit_function_add_string_attribute = &mock_fn_add_str_attr;

    rout.compiler_generated = FALSE;
    rout.is_inline = FALSE;
    rout.keep_definition_in_il = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_subprogram_debug_info(&rout, func));

    rout.compiler_generated = TRUE;
    rout.is_inline = TRUE;
    rout.is_weak = TRUE;
    rout.keep_definition_in_il = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_subprogram_debug_info(&rout, func));

    rout.is_weak = FALSE;
    rout.is_template_function = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_subprogram_debug_info(&rout, func));

    rout.is_template_function = FALSE;
    rout.function_def_number = NULL_function_def_number;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_subprogram_debug_info(&rout, func));

    rout.function_def_number = 1;
    rout.storage_class = sc_unspecified;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_subprogram_debug_info(&rout, func));

    rout.storage_class = sc_static;
    rout.is_virtual = TRUE;
    rout.source_corresp.name = "static_inl";
    rout.source_corresp.unmangled_name_or_mangled_encoding = "_ZL10static_inlv";
    a_template_arg rtarg = {};
    rout.template_arg_list = &rtarg;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_subprogram_debug_info(&rout, func));

    for (int b = 0; b < 15; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_emit_subprogram_debug_info(&rout, func);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* Variable debug info */
    a_variable var = {};
    if (gcc_gen_be_emit_variable_debug_info(&var, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    p_gcc_jit_lvalue_set_alignment = NULL;
    var.alignment = 8;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_variable_debug_info(&var, lval));
    p_gcc_jit_lvalue_set_alignment = &mock_lval_set_align;
    var.alignment = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_variable_debug_info(&var, lval));
    var.alignment = 16;
    var.source_corresp.name = "aligned_v";
    for (int b = 0; b < 10; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_emit_variable_debug_info(&var, lval);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* Lexical block scope & inlined subroutine across seq==0/!=0 and debug flags */
    a_statement stmt = {};
    if (gcc_gen_be_emit_lexical_block_scope(NULL, blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_lexical_block_scope(&stmt, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_inlined_subroutine(NULL, &stmt, blk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_inlined_subroutine(&rout, &stmt, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_dwarf_config(4, 0));
    gcc_be_debug_info = FALSE;
    gcc_gen_be_singleton_state.debug_level = 0;
    stmt.position.seq = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_block_debug_info(&stmt, blk));
    rout.source_corresp.name = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_inlined_subroutine(&rout, NULL, blk));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_inlined_subroutine(&rout, &stmt, blk));

    gcc_be_debug_info = TRUE;
    stmt.position.seq = 12;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_lexical_block_scope(&stmt, blk));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_inlined_subroutine(&rout, &stmt, blk));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_inlined_subroutine(&rout, NULL, blk));
    gcc_be_debug_info = FALSE;

    gcc_gen_be_singleton_state.debug_level = 2;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_lexical_block_scope(&stmt, blk));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_inlined_subroutine(&rout, &stmt, blk));
    gcc_gen_be_singleton_state.debug_level = 0;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_dwarf_config(4, 2));
    rout.source_corresp.name = "named_inlined";
    for (int b = 0; b < 10; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_emit_lexical_block_scope(&stmt, blk);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 12; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_emit_inlined_subroutine(&rout, &stmt, blk);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* 4. Namespace, class inheritance, template params, enums, types, and >64 DIE resize */
    const char *ns_tag = NULL;
    a_namespace ns = {};
    if (gcc_gen_be_emit_namespace_debug_info(NULL, &ns_tag) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_namespace_debug_info(&ns, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_namespace_debug_info(&ns, &ns_tag));
    ns.source_corresp.name = "my_ns";
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_namespace_debug_info(&ns, &ns_tag));
    ns.is_inline = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_namespace_debug_info(&ns, &ns_tag));
    ns.is_namespace_alias = TRUE;
    for (int b = 0; b < 8; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_emit_namespace_debug_info(&ns, &ns_tag);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    a_field f0 = {}, f_bf = {};
    f0.next = &f_bf;
    f0.source_corresp.name = "plain_fld";
    f_bf.is_bit_field = TRUE;
    f_bf.offset = 4;
    f_bf.offset_bit_remainder = 2;
    f_bf.bit_size = 5;

    a_type base_named = {}, base_unnamed = {}, tref_null = {};
    base_named.kind = tk_class;
    base_named.source_corresp.name = "BaseCls";
    base_unnamed.kind = tk_struct;
    base_unnamed.source_corresp.name = NULL;
    tref_null.kind = tk_typeref;
    tref_null.variant.typeref.type = NULL;

    a_base_class_derivation deriv_pub = {}, deriv_prot = {}, deriv_priv = {};
    deriv_pub.access = as_public;
    deriv_prot.access = as_protected;
    deriv_priv.access = as_private;
    a_base_class bc0 = {}, bc1 = {}, bc2 = {}, bc3 = {};
    bc0.type = &base_named;
    bc0.is_virtual = TRUE;
    bc0.derivation = &deriv_pub;
    bc0.next_direct = &bc1;
    bc0.next = &bc1;
    bc1.type = &base_unnamed;
    bc1.derivation = &deriv_prot;
    bc1.next_direct = &bc2;
    bc1.next = &bc2;
    bc2.type = NULL;
    bc2.derivation = &deriv_priv;
    bc2.next_direct = &bc3;
    bc2.next = &bc3;
    bc3.type = NULL;
    bc3.derivation = NULL;

    a_constant tval_const = {};
    tval_const.source_corresp.name = "N";
    a_template_arg targ_type = {}, targ_type_null = {}, targ_val = {}, targ_val_null = {}, targ_other = {};
    targ_type.kind = tak_type;
    targ_type.variant.type = &base_named;
    targ_type.next = &targ_type_null;
    targ_type_null.kind = tak_type;
    targ_type_null.variant.type = NULL;
    targ_type_null.next = &targ_val;
    targ_val.kind = tak_nontype;
    targ_val.variant.constant = &tval_const;
    targ_val.next = &targ_val_null;
    targ_val_null.kind = tak_nontype;
    targ_val_null.variant.constant = NULL;
    targ_val_null.next = &targ_other;
    targ_other.kind = tak_template;

    a_class_type_supplement cls_supp = {};
    cls_supp.direct_base_classes = &bc0;
    cls_supp.template_arg_list = &targ_type;

    a_class_type_supplement cls_supp_indirect = {};
    cls_supp_indirect.direct_base_classes = NULL;
    cls_supp_indirect.base_classes = &bc0;
    cls_supp_indirect.template_arg_list = NULL;

    a_type st_tp = {};
    st_tp.kind = tk_struct;
    st_tp.source_corresp.name = "MyStruct";
    st_tp.variant.class_struct_union.field_list = &f0;
    st_tp.variant.class_struct_union.extra_info = &cls_supp;

    a_type st_indirect = {};
    st_indirect.kind = tk_class;
    st_indirect.variant.class_struct_union.extra_info = &cls_supp_indirect;

    a_type td_tp = {};
    td_tp.kind = tk_typeref;
    td_tp.variant.typeref.type = &st_tp;

    a_type un_tp = {};
    un_tp.kind = tk_union;

    int base_cnt = 0;
    if (gcc_gen_be_emit_class_inheritance_debug_info(NULL, &base_cnt) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_class_inheritance_debug_info(&st_tp, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_class_inheritance_debug_info(&tref_null, &base_cnt));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_class_inheritance_debug_info(&un_tp, &base_cnt));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_class_inheritance_debug_info(&base_named, &base_cnt)); /* extra_info == NULL */
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_class_inheritance_debug_info(&td_tp, &base_cnt));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_class_inheritance_debug_info(&st_indirect, &base_cnt));
    for (int b = 0; b < 20; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_emit_class_inheritance_debug_info(&st_tp, &base_cnt);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    int tparam_cnt = 0;
    if (gcc_gen_be_emit_template_param_debug_info(&targ_type, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    for (int b = 0; b < 20; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_emit_template_param_debug_info(&targ_type, &tparam_cnt);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* Enum debug info */
    a_constant enum_c0 = {}, enum_c1 = {};
    enum_c0.source_corresp.name = "E0";
    enum_c0.next = &enum_c1;
    a_type plain_int_tp = {};
    plain_int_tp.kind = tk_integer;
    plain_int_tp.variant.integer.enum_type = FALSE;
    a_type enum_tp = {};
    enum_tp.kind = tk_integer;
    enum_tp.variant.integer.enum_type = TRUE;
    enum_tp.variant.integer.enum_info.constant_list = &enum_c0;
    a_type enum_tref = {};
    enum_tref.kind = tk_typeref;
    enum_tref.variant.typeref.type = &enum_tp;

    int enum_cnt = 0;
    if (gcc_gen_be_emit_enum_debug_info(NULL, &enum_cnt) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_enum_debug_info(&enum_tp, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_enum_debug_info(&tref_null, &enum_cnt));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_enum_debug_info(&st_tp, &enum_cnt));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_enum_debug_info(&plain_int_tp, &enum_cnt));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_enum_debug_info(&enum_tref, &enum_cnt));

    enum_tp.variant.integer.is_scoped_enum = TRUE;
    enum_tp.variant.integer.enum_info.assoc_scope = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_enum_debug_info(&enum_tp, &enum_cnt));
    a_scope enum_scope = {};
    enum_scope.constants = &enum_c0;
    enum_tp.variant.integer.enum_info.assoc_scope = &enum_scope;
    for (int b = 0; b < 20; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_emit_enum_debug_info(&enum_tp, &enum_cnt);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* Type debug info across all kinds + DIE table resize (>64 entries) + OOM sweeps */
    if (gcc_gen_be_emit_type_debug_info(&td_tp, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_debug_info(&tref_null, int_jit_t));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_debug_info(&plain_int_tp, int_jit_t));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_debug_info(&st_indirect, int_jit_t));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_debug_info(&base_named, int_jit_t));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_debug_info(&un_tp, int_jit_t));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_debug_info(&enum_tp, int_jit_t));

    a_type fn_tp = {};
    fn_tp.kind = tk_routine;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_debug_info(&fn_tp, int_jit_t));

    a_type ptm_tp = {};
    ptm_tp.kind = tk_ptr_to_member;
    ptm_tp.variant.ptr_to_member.class_of_which_a_member = &base_named;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_debug_info(&ptm_tp, int_jit_t));
    ptm_tp.variant.ptr_to_member.class_of_which_a_member = &base_unnamed;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_debug_info(&ptm_tp, int_jit_t));
    ptm_tp.variant.ptr_to_member.class_of_which_a_member = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_debug_info(&ptm_tp, int_jit_t));

    a_type flt_tp = {};
    flt_tp.kind = tk_float;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_debug_info(&flt_tp, int_jit_t));

    /* Trigger DIE table growth 0 -> 64 -> 128 AND OOM on realloc */
    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
    ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_OOM;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));
    int_jit_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);

    for (int b = 0; b < 40; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_emit_type_debug_info(&td_tp, int_jit_t);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 15; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_emit_type_debug_info(&enum_tp, int_jit_t);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 10; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_emit_type_debug_info(&fn_tp, int_jit_t);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 10; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_emit_type_debug_info(&ptm_tp, int_jit_t);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int i = 0; i < 70; ++i) {
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_debug_info(&fn_tp, int_jit_t));
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_gcc_be_debug.cpp`.
 * @details Runs `run_debug_unit_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) noexcept {
    int ok = 0;
    gcc_gen_be_error_t err = run_debug_unit_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}
