/**
 * @file test_gcc_be_debug.cpp
 * @brief Unit tests for DWARF debug metadata generation logic (`gcc_gen_be_debug.c`).
 * @details Validates DW_TAG emission mappings, variable location tracking, and lexical block bounds.
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_debug.h"
#include "gcc_gen_be_error.h"
#include "gcc_gen_be_context.h"
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
/** @brief Primary source file name. */
a_const_char *primary_source_file_name = "test_debug.cpp";

/**
 * @brief Stub for converting sequence number to file and line in debug unit tests.
 * @details Populates dummy file name and line number and returns NULL source file pointer.
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
    (void)seq;
    if (file_name) *file_name = "test_debug.cpp";
    if (full_name) *full_name = "test_debug.cpp";
    if (line_number) *line_number = 10;
    if (at_end_of_source) *at_end_of_source = FALSE;
    return NULL;
}
}

/**
 * @brief Executes debug metadata null-pointer and context configuration tests.
 * @details Verifies `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` on NULL arguments and `GCC_GEN_BE_SUCCESS` on valid context.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_debug_unit_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    if (gcc_gen_be_enable_debug_info(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_subprogram_debug_info(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_type_debug_info(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_variable_debug_info(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_block_debug_info(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_OOM;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    if (gcc_gen_be_set_dwarf_config(1, 2) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_set_dwarf_config(4, 9) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_get_dwarf_config(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_dwarf_config(5, 2));
    int dv = 0, dl = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_dwarf_config(&dv, &dl));
    if (dv != 5 || dl != 2) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_emit_dwarf_info(ctx, 2, 1));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_dwarf_info(ctx, 3, 2));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_dwarf_info(ctx, 5, 3));
    GCC_GEN_BE_CHECK(gcc_gen_be_enable_debug_info(ctx));

    a_field f0 = {}, f_bf = {};
    f0.next = &f_bf;
    f_bf.is_bit_field = TRUE;
    f_bf.offset = 4;
    f_bf.offset_bit_remainder = 2;
    f_bf.bit_size = 5;
    a_type base_tp = {};
    base_tp.kind = tk_class;
    base_tp.source_corresp.name = "BaseCls";
    a_derivation_step deriv_prot = {}, deriv_priv = {};
    deriv_prot.access = as_protected;
    deriv_priv.access = as_private;
    a_base_class bc0 = {}, bc1 = {}, bc2 = {};
    bc0.type = &base_tp;
    bc0.is_virtual = TRUE;
    bc0.next_direct = &bc1;
    bc1.type = &base_tp;
    bc1.derivation = &deriv_prot;
    bc1.next_direct = &bc2;
    bc2.type = NULL;
    bc2.derivation = &deriv_priv;
    a_constant tval_const = {};
    tval_const.source_corresp.name = "N";
    a_template_arg targ_type = {}, targ_val = {};
    targ_type.kind = tak_type;
    targ_type.variant.type = &base_tp;
    targ_type.next = &targ_val;
    targ_val.kind = tak_nontype;
    targ_val.variant.constant = &tval_const;

    a_class_type_supplement cls_supp = {};
    cls_supp.direct_base_classes = &bc0;
    cls_supp.template_arg_list = &targ_type;

    a_type st_tp = {};
    st_tp.kind = tk_struct;
    st_tp.variant.class_struct_union.field_list = &f0;
    st_tp.variant.class_struct_union.extra_info = &cls_supp;
    a_type td_tp = {};
    td_tp.kind = tk_typeref;
    td_tp.variant.typeref.type = &st_tp;
    gcc_jit_type *int_jit_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_debug_info(&td_tp, int_jit_t));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_debug_info(&base_tp, int_jit_t));
    a_type un_tp = {};
    un_tp.kind = tk_union;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_debug_info(&un_tp, int_jit_t));
    a_type fn_tp = {};
    fn_tp.kind = tk_routine;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_debug_info(&fn_tp, int_jit_t));
    a_type ptm_tp = {};
    ptm_tp.kind = tk_ptr_to_member;
    ptm_tp.variant.ptr_to_member.class_of_which_a_member = &base_tp;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_debug_info(&ptm_tp, int_jit_t));
    ptm_tp.variant.ptr_to_member.class_of_which_a_member = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_debug_info(&ptm_tp, int_jit_t));

    /* Test namespace debug info */
    const char *ns_tag = NULL;
    a_namespace ns = {};
    if (gcc_gen_be_emit_namespace_debug_info(NULL, &ns_tag) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_namespace_debug_info(&ns, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_namespace_debug_info(&ns, &ns_tag));
    if (!ns_tag) return GCC_GEN_BE_ERROR_INTERNAL;
    ns.source_corresp.name = "my_ns";
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_namespace_debug_info(&ns, &ns_tag));
    ns.is_inline = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_namespace_debug_info(&ns, &ns_tag));
    ns.is_namespace_alias = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_namespace_debug_info(&ns, &ns_tag));

    /* Test class inheritance & template param debug info */
    int base_cnt = 0;
    int tparam_cnt = 0;
    if (gcc_gen_be_emit_class_inheritance_debug_info(NULL, &base_cnt) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_class_inheritance_debug_info(&st_tp, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_class_inheritance_debug_info(&st_tp, &base_cnt));
    if (base_cnt != 3) return GCC_GEN_BE_ERROR_INTERNAL;

    if (gcc_gen_be_emit_template_param_debug_info(&targ_type, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_template_param_debug_info(&targ_type, &tparam_cnt));
    if (tparam_cnt != 2) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Test enum debug info (unscoped and scoped) */
    a_constant enum_c0 = {}, enum_c1 = {};
    enum_c0.next = &enum_c1;
    a_type enum_tp = {};
    enum_tp.kind = tk_integer;
    enum_tp.variant.integer.enum_type = TRUE;
    enum_tp.variant.integer.enum_info.constant_list = &enum_c0;
    int enum_cnt = 0;
    if (gcc_gen_be_emit_enum_debug_info(NULL, &enum_cnt) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_emit_enum_debug_info(&enum_tp, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_enum_debug_info(&st_tp, &enum_cnt));
    if (enum_cnt != 0) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_enum_debug_info(&enum_tp, &enum_cnt));
    if (enum_cnt != 2) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_debug_info(&enum_tp, int_jit_t));

    a_scope enum_scope = {};
    enum_scope.constants = &enum_c0;
    enum_tp.variant.integer.is_scoped_enum = TRUE;
    enum_tp.variant.integer.enum_info.assoc_scope = &enum_scope;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_enum_debug_info(&enum_tp, &enum_cnt));
    if (enum_cnt != 2) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_context_release(ctx);

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_gcc_be_debug.cpp`.
 * @details Runs `run_debug_unit_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_debug_unit_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}
