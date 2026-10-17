/**
 * @file test_gcc_be_decl.cpp
 * @brief Unit tests for declaration lowering (`gcc_gen_be_decl.c`).
 * @details Verifies null-pointer handling for variable and function declaration lowering.
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_decl.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_abi.h"
#include "gcc_gen_be_eh.h"
#include "gcc_gen_be_rtti.h"
#include "gcc_gen_be_debug.h"
#include "gcc_gen_be_location.h"
#include "gcc_gen_be_error.h"
#include <libgccjit.h>
#include <cstdio>
#include <cstring>

extern "C" {
/**
 * @brief Mock diagnostic reporter for declaration unit tests.
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
/** @brief Primary source file name. */
a_const_char *primary_source_file_name = "test_decl.cpp";
/** @brief IL output file handle. */
FILE *f_il_output = NULL;
/** @brief Innermost function scope pointer. */
a_scope_ptr innermost_function_scope = NULL;
/** @brief Memory region table pointer. */
a_mem_block_header_ptr *mem_region_table = NULL;
/** @brief Mock IL header. */
an_il_header il_header;

/**
 * @brief Stub for converting sequence number to file and line in decl unit tests.
 * @details Populates dummy file name and line number and returns NULL.
 * @param[in] seq Sequence number.
 * @param[out] file_name Output file name.
 * @param[out] full_name Output full path.
 * @param[out] line_number Output line number.
 * @param[out] at_end_of_source Output end-of-source flag.
 * @return NULL.
 */
a_source_file_ptr conv_seq_to_file_and_line(
    a_seq_number seq,
    a_const_char **file_name,
    a_const_char **full_name,
    a_line_number *line_number,
    a_boolean *at_end_of_source) {
    (void)seq;
    if (file_name) *file_name = "test_decl.cpp";
    if (full_name) *full_name = "test_decl.cpp";
    if (line_number) *line_number = 10;
    if (at_end_of_source) *at_end_of_source = FALSE;
    return NULL;
}

/**
 * @brief Stub for reading memory region in decl unit tests.
 * @details No-op.
 * @param[in] region_number Memory region number.
 */
void read_memory_region(a_memory_region_number region_number) {
    (void)region_number;
}

/**
 * @brief Stub for looking up routine scope in decl unit tests.
 * @details Returns NULL.
 * @param[in] rout Routine pointer.
 * @return NULL.
 */
a_scope_ptr scope_for_routine_or_null(a_routine_ptr rout) {
    (void)rout;
    return NULL;
}

/**
 * @brief Stub for constant rvalue lowering in decl unit tests.
 * @details Sets `*out_rval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] con Constant pointer.
 * @param[in] expected_type Expected JIT type.
 * @param[out] out_rval Output rvalue.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_lower_constant_rvalue(a_constant_ptr con, struct gcc_jit_type *expected_type, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)con;
    (void)expected_type;
    if (out_rval) *out_rval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for dynamic init lowering in decl unit tests.
 * @details Sets `*out_rval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] dip Dynamic init pointer.
 * @param[in] target_lval Target lvalue.
 * @param[out] out_rval Output rvalue.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_lower_dynamic_init(a_dynamic_init_ptr dip, struct gcc_jit_lvalue *target_lval, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)dip;
    (void)target_lval;
    if (out_rval) *out_rval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for RAII scope unwind cleanup emission in decl unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func Active JIT function.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_emit_scope_unwind_cleanup(struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for expression rvalue lowering in decl unit tests.
 * @details Sets `*out_rval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] expr Expression pointer.
 * @param[out] out_rval Output rvalue.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_lower_expr_rvalue(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)expr;
    if (out_rval) *out_rval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for variable attribute application in decl unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] var Variable pointer.
 * @param[in] lval JIT lvalue.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_apply_variable_attributes(a_variable_ptr var, struct gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT {
    (void)var;
    (void)lval;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for function attribute application in decl unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] rout Routine pointer.
 * @param[in] func JIT function.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_apply_function_attributes(a_routine_ptr rout, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)rout;
    (void)func;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for struct attribute application in decl unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] class_type Class type pointer.
 * @param[in] struct_type JIT struct.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_apply_struct_attributes(a_type_ptr class_type, struct gcc_jit_struct *struct_type) GCC_GEN_BE_NOEXCEPT {
    (void)class_type;
    (void)struct_type;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for sret check in decl unit tests.
 * @details Sets `*out_is_sret` to `FALSE`.
 * @param[in] type Type pointer.
 * @param[out] out_is_sret Output boolean.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_is_returned_via_sret(a_type_ptr type, a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT {
    (void)type;
    if (out_is_sret) *out_is_sret = FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for invisible reference check in decl unit tests.
 * @details Sets `*out_is_ref` to `FALSE`.
 * @param[in] type Type pointer.
 * @param[out] out_is_ref Output boolean.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT {
    (void)type;
    if (out_is_ref) *out_is_ref = FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for coerced ABI type lookup in decl unit tests.
 * @details Sets `*out_coerced_type` to NULL and `*out_needs_coercion` to `FALSE`.
 * @param[in] type Type pointer.
 * @param[out] out_coerced_type Output coerced type.
 * @param[out] out_needs_coercion Output boolean.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_get_coerced_abi_type(a_type_ptr type, struct gcc_jit_type **out_coerced_type, a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT {
    (void)type;
    if (out_coerced_type) *out_coerced_type = NULL;
    if (out_needs_coercion) *out_needs_coercion = FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for unpacking coerced register arguments in decl unit tests.
 * @details Sets `*out_lval` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func JIT function.
 * @param[in] block Entry JIT block.
 * @param[in] param_type Frontend parameter type.
 * @param[in] param_rval Coerced parameter rvalue.
 * @param[out] out_lval Output unpacked local lvalue.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_unpack_coerced_Arg(
    struct gcc_jit_function *func,
    struct gcc_jit_block *block,
    a_type_ptr param_type,
    struct gcc_jit_rvalue *param_rval,
    struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    (void)block;
    (void)param_type;
    (void)param_rval;
    if (out_lval) *out_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for variable debug info emission in decl unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] var Variable pointer.
 * @param[in] lval JIT lvalue.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_emit_variable_debug_info(a_variable_ptr var, struct gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT {
    (void)var;
    (void)lval;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for subprogram debug info emission in decl unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] rout Routine pointer.
 * @param[in] func JIT function.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_emit_subprogram_debug_info(a_routine_ptr rout, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)rout;
    (void)func;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for type debug info emission in decl unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] tp Type pointer.
 * @param[in] type JIT type.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_emit_type_debug_info(a_type_ptr tp, struct gcc_jit_type *type) GCC_GEN_BE_NOEXCEPT {
    (void)tp;
    (void)type;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for cxa_atexit emission in decl unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] obj_lval Object lvalue.
 * @param[in] dtor_fn Destructor function.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_emit_cxa_atexit(struct gcc_jit_lvalue *obj_lval, struct gcc_jit_function *dtor_fn) GCC_GEN_BE_NOEXCEPT {
    (void)obj_lval;
    (void)dtor_fn;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for cxa_thread_atexit emission in decl unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] obj_lval Object lvalue.
 * @param[in] dtor_fn Destructor function.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_emit_cxa_thread_atexit(struct gcc_jit_lvalue *obj_lval, struct gcc_jit_function *dtor_fn) GCC_GEN_BE_NOEXCEPT {
    (void)obj_lval;
    (void)dtor_fn;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for priority init registration in decl unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func Init function.
 * @param[in] priority Priority value.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_register_priority_init(struct gcc_jit_function *func, int priority) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    (void)priority;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for priority fini registration in decl unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func Fini function.
 * @param[in] priority Priority value.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_register_priority_fini(struct gcc_jit_function *func, int priority) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    (void)priority;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for active coroutine frame lookup in decl unit tests.
 * @details Sets `*out_frame_type` and `*out_frame_lval` to NULL.
 * @param[out] out_frame_type Output frame type.
 * @param[out] out_frame_lval Output frame lvalue.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_get_active_coroutine_frame(struct gcc_jit_type **out_frame_type, struct gcc_jit_lvalue **out_frame_lval) GCC_GEN_BE_NOEXCEPT {
    if (out_frame_type) *out_frame_type = NULL;
    if (out_frame_lval) *out_frame_lval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for coroutine frame lowering in decl unit tests.
 * @details Sets `*out_type` to NULL.
 * @param[in] rout Routine pointer.
 * @param[out] out_type Output frame type.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_lower_coroutine_frame(a_routine_ptr rout, struct gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
    (void)rout;
    if (out_type) *out_type = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for coroutine dispatchers lowering in decl unit tests.
 * @details Sets `*out_resume` and `*out_destroy` to NULL.
 * @param[in] rout Routine pointer.
 * @param[out] out_resume Output resume function.
 * @param[out] out_destroy Output destroy function.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_lower_coroutine_dispatchers(a_routine_ptr rout, struct gcc_jit_function **out_resume, struct gcc_jit_function **out_destroy) GCC_GEN_BE_NOEXCEPT {
    (void)rout;
    if (out_resume) *out_resume = NULL;
    if (out_destroy) *out_destroy = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for coroutine ramp lowering in decl unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] rout Routine pointer.
 * @param[in] ramp_fn Ramp function.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_lower_coroutine_ramp(a_routine_ptr rout, struct gcc_jit_function *ramp_fn) GCC_GEN_BE_NOEXCEPT {
    (void)rout;
    (void)ramp_fn;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for beginning function statement tracking in decl unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_begin_function_stmts(void) GCC_GEN_BE_NOEXCEPT {
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for ending function statement tracking in decl unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func JIT function.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_end_function_stmts(struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for beginning function EH in decl unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func JIT function.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_begin_function_eh(struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for ending function EH in decl unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func JIT function.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_end_function_eh(struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for lowering routine body in decl unit tests.
 * @details Returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] stmt Body statement.
 * @param[in] func JIT function.
 * @param[in] exc_spec Exception specification.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_lower_routine_body(a_statement_ptr stmt, struct gcc_jit_function *func, an_exception_specification_ptr exc_spec) GCC_GEN_BE_NOEXCEPT {
    (void)stmt;
    (void)func;
    (void)exc_spec;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for function name mangling in decl unit tests.
 * @details Writes `_Z4stubv` into `buffer`.
 * @param[in] rout Routine pointer.
 * @param[in] variant_code Variant code.
 * @param[out] buffer Output buffer.
 * @param[in] size Buffer size.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_mangle_function_name(a_routine_ptr rout, const char *variant_code, char *buffer, size_t size) GCC_GEN_BE_NOEXCEPT {
    (void)rout;
    if (!buffer || size == 0) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    snprintf(buffer, size, "_ZN4Test%sEv", variant_code ? variant_code : "C1");
    return GCC_GEN_BE_SUCCESS;
}
/**
 * @brief Stub for VTT emission in decl unit tests.
 * @details Sets `*out_vtt` to NULL and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] class_type Class type pointer.
 * @param[out] out_vtt Output VTT global lvalue.
 * @return GCC_GEN_BE_SUCCESS.
 */
gcc_gen_be_error_t gcc_gen_be_emit_vtt(a_type_ptr class_type, struct gcc_jit_lvalue **out_vtt) GCC_GEN_BE_NOEXCEPT {
    (void)class_type;
    if (out_vtt) *out_vtt = NULL;
    return GCC_GEN_BE_SUCCESS;
}
}

/**
 * @brief Executes declaration lowering validation tests.
 * @details Verifies `gcc_gen_be_lower_global_variable_decl`, `gcc_gen_be_lower_local_variable_decl`,
 * `gcc_gen_be_lower_function_decl`, and `gcc_gen_be_emit_ctor_dtor_clones` (including C2/D2/D0 clones and virtual base VTT forwarding).
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t run_decl_unit_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    gcc_jit_lvalue *lval = NULL;
    gcc_jit_function *func = NULL;
    if (gcc_gen_be_lower_global_variable_decl(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_local_variable_decl(NULL, NULL, &lval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_function_decl(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(NULL, &lval));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(NULL, &func));
    if (gcc_gen_be_emit_ctor_dtor_clones(NULL, NULL, &func, &func) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_OOM;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *void_ptr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_param *this_p = gcc_jit_context_new_param(ctx, NULL, void_ptr_t, "this_ptr");
    gcc_jit_function *primary_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_t, "_ZN4TestC1Ev", 1, &this_p, 0);

    a_type vbase_class = {};
    vbase_class.kind = tk_class;
    vbase_class.variant.class_struct_union.any_virtual_base_classes = TRUE;
    a_scope class_scope = {};
    class_scope.kind = sck_class_struct_union;
    class_scope.variant.assoc_type = &vbase_class;

    a_routine ctor_rout = {};
    ctor_rout.special_kind = (a_special_function_kind)sfk_constructor;
    ctor_rout.source_corresp.is_class_member = TRUE;
    ctor_rout.source_corresp.parent_scope = &class_scope;

    gcc_jit_function *c2_clone = NULL;
    gcc_jit_function *d0_clone = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&ctor_rout, primary_fn, &c2_clone, &d0_clone));
    if (!c2_clone || d0_clone != NULL) {
        gcc_jit_context_release(ctx);
        return GCC_GEN_BE_ERROR_INTERNAL;
    }

    ctor_rout.special_kind = (a_special_function_kind)sfk_destructor;
    gcc_jit_function *d2_clone = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&ctor_rout, primary_fn, &d2_clone, &d0_clone));
    if (!d2_clone || !d0_clone) {
        gcc_jit_context_release(ctx);
        return GCC_GEN_BE_ERROR_INTERNAL;
    }

    /* Test sized and over-aligned D0 deleting destructor clones */
    vbase_class.size = 32;
    vbase_class.alignment = 8;
    gcc_jit_function *d2_sized = NULL;
    gcc_jit_function *d0_sized = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&ctor_rout, primary_fn, &d2_sized, &d0_sized));
    if (!d2_sized || !d0_sized) {
        gcc_jit_context_release(ctx);
        return GCC_GEN_BE_ERROR_INTERNAL;
    }

    vbase_class.size = 64;
    vbase_class.alignment = 32;
    gcc_jit_function *d2_aligned = NULL;
    gcc_jit_function *d0_aligned = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&ctor_rout, primary_fn, &d2_aligned, &d0_aligned));
    if (!d2_aligned || !d0_aligned) {
        gcc_jit_context_release(ctx);
        return GCC_GEN_BE_ERROR_INTERNAL;
    }

    /* Test COMDAT section formatting across ELF (0), Mach-O (1), and COFF/PE (2) */
    char sec_buf[256];
    const char *link_attr = NULL;
    if (gcc_gen_be_format_comdat_section_name(NULL, FALSE, TRUE, 0, sec_buf, sizeof(sec_buf), &link_attr) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_format_comdat_section_name("_ZTV4Test", FALSE, TRUE, 3, sec_buf, sizeof(sec_buf), &link_attr) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_format_comdat_section_name("_ZTV4Test", FALSE, TRUE, 0, sec_buf, 4, &link_attr) != GCC_GEN_BE_ERROR_MANGLING_BUFFER_OVERFLOW) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_format_comdat_section_name("_ZN4Test1fEv", TRUE, FALSE, 0, sec_buf, sizeof(sec_buf), &link_attr));
    if (std::strcmp(sec_buf, ".gnu.linkonce.t._ZN4Test1fEv") != 0 || std::strcmp(link_attr, "weak") != 0) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_format_comdat_section_name("_ZTV4Test", FALSE, TRUE, 0, sec_buf, sizeof(sec_buf), &link_attr));
    if (std::strcmp(sec_buf, ".gnu.linkonce.r._ZTV4Test") != 0) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_format_comdat_section_name("_ZN4Test3varE", FALSE, FALSE, 0, sec_buf, sizeof(sec_buf), &link_attr));
    if (std::strcmp(sec_buf, ".gnu.linkonce.d._ZN4Test3varE") != 0) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_format_comdat_section_name("_ZN4Test1fEv", TRUE, FALSE, 1, sec_buf, sizeof(sec_buf), &link_attr));
    if (std::strcmp(sec_buf, "__TEXT,__textcoal_nt") != 0 || std::strcmp(link_attr, "weak_definition") != 0) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_format_comdat_section_name("_ZTV4Test", FALSE, TRUE, 1, sec_buf, sizeof(sec_buf), &link_attr));
    if (std::strcmp(sec_buf, "__DATA,__const_coal") != 0) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_format_comdat_section_name("_ZN4Test3varE", FALSE, FALSE, 1, sec_buf, sizeof(sec_buf), &link_attr));
    if (std::strcmp(sec_buf, "__DATA,__datacoal_nt") != 0) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_format_comdat_section_name("_ZN4Test1fEv", TRUE, FALSE, 2, sec_buf, sizeof(sec_buf), &link_attr));
    if (std::strcmp(sec_buf, ".text$mn") != 0 || std::strcmp(link_attr, "selectany") != 0) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_format_comdat_section_name("_ZTV4Test", FALSE, TRUE, 2, sec_buf, sizeof(sec_buf), &link_attr));
    if (std::strcmp(sec_buf, ".rdata$r") != 0) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_format_comdat_section_name("_ZN4Test3varE", FALSE, FALSE, 2, sec_buf, sizeof(sec_buf), &link_attr));
    if (std::strcmp(sec_buf, ".data$r") != 0) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_lvalue *comdat_glob = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, void_ptr_t, "_ZTV4Test");
    if (gcc_gen_be_apply_comdat_linkage(NULL, "_ZTV4Test", TRUE) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_apply_comdat_linkage(comdat_glob, NULL, TRUE) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_apply_comdat_linkage(comdat_glob, "_ZTV4Test", TRUE));

    gcc_jit_context_release(ctx);

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_gcc_be_decl.cpp`.
 * @details Runs `run_decl_unit_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int ok = 0;
    gcc_gen_be_error_t err = run_decl_unit_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}
