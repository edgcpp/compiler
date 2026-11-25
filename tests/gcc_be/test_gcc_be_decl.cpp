/**
 * @file test_gcc_be_decl.cpp
 * @brief Exhaustive unit tests for declaration lowering (`gcc_gen_be_decl.c`).
 * @details Exercises global/local variable lowering, function declaration/body lowering,
 * Itanium C++ ABI constructor/destructor clone synthesis (`C1`/`C2`/`D0`/`D1`/`D2`),
 * COMDAT section formatting/application, and deterministic OOM fault injection across
 * every function, line, region, and branch of `src/gcc_gen_be_decl.c`.
 * @ingroup gcc_gen_be_tests
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_decl.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_abi.h"
#include "gcc_gen_be_dtor.h"
#include "gcc_gen_be_eh.h"
#include "gcc_gen_be_rtti.h"
#include "gcc_gen_be_debug.h"
#include "gcc_gen_be_location.h"
#include "gcc_gen_be_lib_loader.h"
#include "gcc_gen_be_error.h"
#include <libgccjit.h>
#include <cstdio>
#include <cstdlib>
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

/** @brief Active routine scope returned by `scope_for_routine_or_null` stub. */
static a_scope_ptr g_mock_def_scope = NULL;
/** @brief Active constant rvalue returned by `gcc_gen_be_lower_constant_rvalue` stub. */
static gcc_jit_rvalue *g_mock_const_rval = NULL;
/** @brief Flag instructing `gcc_gen_be_lower_constant_rvalue` to clear `current_block`. */
static a_boolean g_mock_const_clears_block = FALSE;
/** @brief Flag instructing `gcc_gen_be_lower_dynamic_init` to clear `current_block`. */
static a_boolean g_mock_dyn_clears_block = FALSE;
/** @brief Flag returned by `gcc_gen_be_is_returned_via_sret` stub. */
static a_boolean g_mock_sret = FALSE;
/** @brief Specific type pointer passed by invisible reference in stub. */
static a_type_ptr g_mock_invis_ref_type = NULL;
/** @brief Secondary type pointer passed by invisible reference in stub. */
static a_type_ptr g_mock_invis_ref_type2 = NULL;
/** @brief Specific type pointer coerced in registers in stub. */
static a_type_ptr g_mock_coerced_src_type = NULL;
/** @brief Coerced `gcc_jit_type` returned by `gcc_gen_be_get_coerced_abi_type` stub. */
static gcc_jit_type *g_mock_coerced_jit_type = NULL;
/** @brief Flag controlling whether `gcc_gen_be_unpack_coerced_Arg` emits a stack local. */
static a_boolean g_mock_emit_unpacked_local = TRUE;
/** @brief Active coroutine frame lvalue returned by `gcc_gen_be_get_active_coroutine_frame` stub. */
static gcc_jit_lvalue *g_mock_coro_flval = NULL;
/** @brief Flag instructing `gcc_gen_be_lower_routine_body` to clear `current_block`. */
static a_boolean g_mock_body_clears_block = FALSE;
/** @brief Error returned by `gcc_gen_be_lower_routine_body` stub. */
static gcc_gen_be_error_t g_mock_body_err = GCC_GEN_BE_SUCCESS;
/** @brief Flag instructing `gcc_gen_be_lower_routine_body` to allocate `eh_state.action_table` before failing. */
static a_boolean g_mock_body_alloc_action_table = FALSE;
/** @brief Minimum buffer size required before `gcc_gen_be_mangle_function_name` succeeds. */
static size_t g_mock_mangle_min_size = 0;
/** @brief Override string returned by `gcc_gen_be_mangle_function_name` when `variant_code == NULL`. */
static const char *g_mock_primary_mangle = NULL;
/** @brief Override string returned by `gcc_gen_be_mangle_function_name` when `variant_code != NULL`. */
static const char *g_mock_variant_mangle = NULL;
/** @brief Counter of `read_memory_region` invocations. */
static int g_read_mem_region_calls = 0;

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
 * @details Increments `g_read_mem_region_calls`.
 * @param[in] region_number Memory region number.
 */
void read_memory_region(a_memory_region_number region_number) {
    (void)region_number;
    g_read_mem_region_calls++;
}

/**
 * @brief Stub for registering OpenMP pragmas in scope in decl unit tests.
 * @details Steps the OOM fault-injection budget and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] scope Frontend scope pointer.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_register_omp_pragmas_in_scope(a_scope_ptr scope) GCC_GEN_BE_NOEXCEPT {
    (void)scope;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for looking up routine scope in decl unit tests.
 * @details Returns `g_mock_def_scope` when `rout->memory_region != 99`, else NULL (for nested dtor decls).
 * @param[in] rout Routine pointer.
 * @return `g_mock_def_scope` or NULL.
 */
a_scope_ptr scope_for_routine_or_null(a_routine_ptr rout) {
    if (rout && rout->memory_region == 99) return NULL;
    return g_mock_def_scope;
}

/**
 * @brief Stub for constant rvalue lowering in decl unit tests.
 * @details Steps OOM budget, optionally clears `current_block`, and writes `g_mock_const_rval`.
 * @param[in] con Constant pointer.
 * @param[in] expected_type Expected JIT type.
 * @param[out] out_rval Output rvalue.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_lower_constant_rvalue(a_constant_ptr con, struct gcc_jit_type *expected_type, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)con;
    (void)expected_type;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_mock_const_clears_block) {
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    }
    if (out_rval) *out_rval = g_mock_const_rval;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for dynamic init lowering in decl unit tests.
 * @details Steps OOM budget and optionally clears `current_block`.
 * @param[in] dip Dynamic init pointer.
 * @param[in] target_lval Target lvalue.
 * @param[out] out_rval Output rvalue.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_lower_dynamic_init(a_dynamic_init_ptr dip, struct gcc_jit_lvalue *target_lval, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)dip;
    (void)target_lval;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_mock_dyn_clears_block) {
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    }
    if (out_rval) *out_rval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for RAII scope unwind cleanup emission in decl unit tests.
 * @details Steps OOM budget and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func Active JIT function.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_emit_scope_unwind_cleanup(struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for expression rvalue lowering in decl unit tests.
 * @details Steps OOM budget and sets `*out_rval` to NULL.
 * @param[in] expr Expression pointer.
 * @param[out] out_rval Output rvalue.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_lower_expr_rvalue(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)expr;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (out_rval) *out_rval = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for variable attribute application in decl unit tests.
 * @details Steps OOM budget and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] var Variable pointer.
 * @param[in] lval JIT lvalue.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_apply_variable_attributes(a_variable_ptr var, struct gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT {
    (void)var;
    (void)lval;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for function attribute application in decl unit tests.
 * @details Steps OOM budget and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] rout Routine pointer.
 * @param[in] func JIT function.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_apply_function_attributes(a_routine_ptr rout, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)rout;
    (void)func;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for struct attribute application in decl unit tests.
 * @details Steps OOM budget and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] class_type Class type pointer.
 * @param[in] struct_type JIT struct.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_apply_struct_attributes(a_type_ptr class_type, struct gcc_jit_struct *struct_type) GCC_GEN_BE_NOEXCEPT {
    (void)class_type;
    (void)struct_type;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for sret check in decl unit tests.
 * @details Steps OOM budget and sets `*out_is_sret` to `g_mock_sret`.
 * @param[in] type Type pointer.
 * @param[out] out_is_sret Output boolean.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_is_returned_via_sret(a_type_ptr type, a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT {
    (void)type;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (out_is_sret) *out_is_sret = g_mock_sret;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for invisible reference check in decl unit tests.
 * @details Steps OOM budget and sets `*out_is_ref` when `type == g_mock_invis_ref_type`.
 * @param[in] type Type pointer.
 * @param[out] out_is_ref Output boolean.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (out_is_ref) *out_is_ref = (type && (type == g_mock_invis_ref_type || type == g_mock_invis_ref_type2)) ? TRUE : FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for coerced ABI type lookup in decl unit tests.
 * @details Steps OOM budget and returns `g_mock_coerced_jit_type` when `type == g_mock_coerced_src_type`.
 * @param[in] type Type pointer.
 * @param[out] out_coerced_type Output coerced type.
 * @param[out] out_needs_coercion Output boolean.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_get_coerced_abi_type(a_type_ptr type, struct gcc_jit_type **out_coerced_type, a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    a_boolean match = (type && type == g_mock_coerced_src_type) ? TRUE : FALSE;
    if (out_coerced_type) *out_coerced_type = match ? g_mock_coerced_jit_type : NULL;
    if (out_needs_coercion) *out_needs_coercion = match;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for unpacking coerced register arguments in decl unit tests.
 * @details Steps OOM budget and creates a stack local when `g_mock_emit_unpacked_local` is set.
 * @param[in] func JIT function.
 * @param[in] block Entry JIT block.
 * @param[in] param_type Frontend parameter type.
 * @param[in] param_rval Coerced parameter rvalue.
 * @param[out] out_lval Output unpacked local lvalue.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_unpack_coerced_Arg(
    struct gcc_jit_function *func,
    struct gcc_jit_block *block,
    a_type_ptr param_type,
    struct gcc_jit_rvalue *param_rval,
    struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    (void)block;
    (void)param_type;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (out_lval) {
        *out_lval = (g_mock_emit_unpacked_local && func && param_rval)
            ? gcc_jit_function_new_local(func, NULL, gcc_jit_rvalue_get_type(param_rval), "coerced_unp")
            : NULL;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for variable debug info emission in decl unit tests.
 * @details Steps OOM budget and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] var Variable pointer.
 * @param[in] lval JIT lvalue.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_emit_variable_debug_info(a_variable_ptr var, struct gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT {
    (void)var;
    (void)lval;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for subprogram debug info emission in decl unit tests.
 * @details Steps OOM budget and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] rout Routine pointer.
 * @param[in] func JIT function.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_emit_subprogram_debug_info(a_routine_ptr rout, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)rout;
    (void)func;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for type debug info emission in decl unit tests.
 * @details Steps OOM budget and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] tp Type pointer.
 * @param[in] type JIT type.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_emit_type_debug_info(a_type_ptr tp, struct gcc_jit_type *type) GCC_GEN_BE_NOEXCEPT {
    (void)tp;
    (void)type;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return GCC_GEN_BE_SUCCESS;
}

/** @brief Configurable target ABI returned by `gcc_gen_be_get_target_abi`. */
static gcc_gen_be_target_abi_t g_mock_target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;

/**
 * @brief Stub for querying active target ABI in decl unit tests.
 * @details Steps OOM budget and sets `*out_abi` to `g_mock_target_abi`.
 * @param[out] out_abi Pointer receiving the active target ABI.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_get_target_abi(gcc_gen_be_target_abi_t *out_abi) GCC_GEN_BE_NOEXCEPT {
    if (!out_abi) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_abi = g_mock_target_abi;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for retrieving `&__dso_handle` in decl unit tests.
 * @details Steps OOM budget and returns a `void *` NULL rvalue for `ctx`.
 * @param[in] ctx Active `gcc_jit_context`.
 * @param[out] out_dso_handle Pointer receiving the `&__dso_handle` rvalue.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_get_dso_handle(gcc_jit_context *ctx, gcc_jit_rvalue **out_dso_handle) GCC_GEN_BE_NOEXCEPT {
    if (!ctx || !out_dso_handle) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    gcc_jit_type *vp_ty = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    *out_dso_handle = gcc_jit_context_null(ctx, vp_ty);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for active coroutine frame lookup in decl unit tests.
 * @details Steps OOM budget and returns `g_mock_coro_flval`.
 * @param[out] out_frame_type Output frame type.
 * @param[out] out_frame_lval Output frame lvalue.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_get_active_coroutine_frame(struct gcc_jit_type **out_frame_type, struct gcc_jit_lvalue **out_frame_lval) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (out_frame_type) *out_frame_type = NULL;
    if (out_frame_lval) *out_frame_lval = g_mock_coro_flval;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for coroutine frame lowering in decl unit tests.
 * @details Sets `*out_type` to NULL.
 * @param[in] rout Routine pointer.
 * @param[out] out_type Output frame type.
 * @return `GCC_GEN_BE_SUCCESS`.
 */
gcc_gen_be_error_t gcc_gen_be_lower_coroutine_frame(a_routine_ptr rout, struct gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
    (void)rout;
    if (out_type) *out_type = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for coroutine ramp lowering in decl unit tests.
 * @details Steps OOM budget and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] rout Routine pointer.
 * @param[in] ramp_fn Ramp function.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_lower_coroutine_ramp(a_routine_ptr rout, struct gcc_jit_function *ramp_fn) GCC_GEN_BE_NOEXCEPT {
    (void)rout;
    (void)ramp_fn;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for beginning function statement tracking in decl unit tests.
 * @details Steps OOM budget and returns `GCC_GEN_BE_SUCCESS`.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_begin_function_stmts(void) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for ending function statement tracking in decl unit tests.
 * @details Steps OOM budget and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func JIT function.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_end_function_stmts(struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for beginning function EH in decl unit tests.
 * @details Steps OOM budget and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func JIT function.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_begin_function_eh(struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for ending function EH in decl unit tests.
 * @details Steps OOM budget and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func JIT function.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_end_function_eh(struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for lowering routine body in decl unit tests.
 * @details Steps OOM budget, optionally clears `current_block` or allocates `action_table`, and returns `g_mock_body_err`.
 * @param[in] stmt Body statement.
 * @param[in] func JIT function.
 * @param[in] exc_spec Exception specification.
 * @return `GCC_GEN_BE_SUCCESS` or configured error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_routine_body(a_statement_ptr stmt, struct gcc_jit_function *func, an_exception_specification_ptr exc_spec) GCC_GEN_BE_NOEXCEPT {
    (void)stmt;
    (void)func;
    (void)exc_spec;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_mock_body_clears_block) {
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    }
    if (g_mock_body_alloc_action_table) {
        gcc_gen_be_singleton_state.eh_state.action_table = (char *)std::malloc(16);
    }
    return g_mock_body_err;
}

/**
 * @brief Stub for function name mangling in decl unit tests.
 * @details Supports simulated buffer overflow retries (`g_mock_mangle_min_size`) and custom mangled strings.
 * @param[in] rout Routine pointer.
 * @param[in] variant_code Variant code.
 * @param[out] buffer Output buffer.
 * @param[in] size Buffer size.
 * @return `GCC_GEN_BE_SUCCESS` or simulated overflow/error code.
 */
gcc_gen_be_error_t gcc_gen_be_mangle_function_name(a_routine_ptr rout, const char *variant_code, char *buffer, size_t size) GCC_GEN_BE_NOEXCEPT {
    (void)rout;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (!buffer || size == 0) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (size < g_mock_mangle_min_size) {
        return GCC_GEN_BE_ERROR_MANGLING_BUFFER_OVERFLOW;
    }
    if (!variant_code && g_mock_primary_mangle) {
        std::snprintf(buffer, size, "%s", g_mock_primary_mangle);
        return GCC_GEN_BE_SUCCESS;
    }
    if (variant_code && g_mock_variant_mangle) {
        std::snprintf(buffer, size, "%s", g_mock_variant_mangle);
        return GCC_GEN_BE_SUCCESS;
    }
    std::snprintf(buffer, size, "_ZN4Test%sEv", variant_code ? variant_code : "C1");
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for VTT emission in decl unit tests.
 * @details Steps OOM budget, sets `*out_vtt` to NULL, and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] class_type Class type pointer.
 * @param[out] out_vtt Output VTT global lvalue.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_emit_vtt(a_type_ptr class_type, struct gcc_jit_lvalue **out_vtt) GCC_GEN_BE_NOEXCEPT {
    (void)class_type;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (out_vtt) *out_vtt = NULL;
    return GCC_GEN_BE_SUCCESS;
}
END_EDG_NAMESPACE

/** @brief Canonical variable mangler return value for unit testing. */
static a_const_char *g_canon_var_ret = "_ZN2ns5g_varE";
/** @brief Canonical function mangler return value for unit testing. */
static char g_canon_fn_buf[64] = "_ZN2ns4funcEv";
/** @brief Flag returning NULL from `mock_canon_mangle_fn`. */
static a_boolean g_canon_fn_null = FALSE;

/**
 * @brief Mock canonical variable mangler callback.
 * @param[in] var Variable node.
 * @return `g_canon_var_ret`.
 */
static a_const_char *mock_canon_mangle_var(a_variable_ptr var) noexcept {
    (void)var;
    return g_canon_var_ret;
}

/**
 * @brief Mock canonical function mangler callback.
 * @param[in] rout Routine node.
 * @return `g_canon_fn_buf` or NULL.
 */
static char *mock_canon_mangle_fn(a_routine_ptr rout) noexcept {
    (void)rout;
    return g_canon_fn_null ? NULL : g_canon_fn_buf;
}

/**
 * @brief Mock `gcc_jit_function_add_attribute` callback.
 * @param[in] fn Target JIT function.
 * @param[in] attr Function attribute enum.
 */
static void mock_fn_add_attr(gcc_jit_function *fn, enum gcc_jit_fn_attribute attr) noexcept {
    (void)fn;
    (void)attr;
}

/**
 * @brief Mock `gcc_jit_lvalue_set_tls_model` callback.
 * @param[in] lval Target global lvalue.
 * @param[in] model TLS model enum.
 */
static void mock_set_tls_model(gcc_jit_lvalue *lval, enum gcc_jit_tls_model model) noexcept {
    (void)lval;
    (void)model;
}

/**
 * @brief Exercises `gcc_gen_be_get_global_ctor_block` and `gcc_gen_be_lower_global_variable_decl`.
 * @details Covers all linkage kinds, mangling modes, TLS models, static/dynamic initializers,
 * interior address offset detection, duplicate symbol resolution, and OOM fault-injection paths.
 * @param[in] ctx Active `gcc_jit_context`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_global_vars_and_ctors(gcc_jit_context *ctx) noexcept {
    gcc_jit_block *cblk = NULL;
    if (gcc_gen_be_get_global_ctor_block(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_get_global_ctor_block(&cblk) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    gcc_jit_lvalue *lv = NULL;
    a_variable dummy_v = {};
    if (gcc_gen_be_lower_global_variable_decl(&dummy_v, &lv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    auto saved_fn_attr = p_gcc_jit_function_add_attribute;
    p_gcc_jit_function_add_attribute = &mock_fn_add_attr;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_global_ctor_block(&cblk));
    GCC_GEN_BE_CHECK(gcc_gen_be_get_global_ctor_block(&cblk)); /* cached global_ctor_func */
    gcc_gen_be_singleton_state.global_ctor_func = NULL;
    gcc_gen_be_singleton_state.global_ctor_block = NULL;
    p_gcc_jit_function_add_attribute = NULL;
    primary_source_file_name = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_global_ctor_block(&cblk));
    primary_source_file_name = "test_decl.cpp";
    for (int b = 0; b < 6; ++b) {
        gcc_gen_be_singleton_state.global_ctor_func = NULL;
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_get_global_ctor_block(&cblk);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    p_gcc_jit_function_add_attribute = saved_fn_attr;

    /* Prepare common types */
    a_type t_i32 = {}, t_i64 = {}, t_void = {}, t_ptr = {}, t_arr_inc = {}, t_arr_vla = {}, t_arr_sz = {};
    t_i32.kind = tk_integer; t_i32.size = 4; t_i32.variant.integer.int_kind = (an_integer_kind)ik_int;
    t_i64.kind = tk_integer; t_i64.size = 8; t_i64.variant.integer.int_kind = (an_integer_kind)ik_long;
    t_void.kind = tk_void; t_void.size = 0;
    t_ptr.kind = tk_pointer; t_ptr.size = 8; t_ptr.variant.pointer.type = &t_i32;
    t_arr_inc.kind = tk_array; t_arr_inc.size = 0; t_arr_inc.variant.array.element_type = &t_i32;
    t_arr_vla.kind = tk_array; t_arr_vla.size = 0; t_arr_vla.variant.array.element_type = &t_i32; t_arr_vla.variant.array.is_variable_size_array = TRUE;
    t_arr_sz.kind = tk_array; t_arr_sz.size = 8; t_arr_sz.variant.array.element_type = &t_i32; t_arr_sz.variant.array.variant.number_of_elements = 2;

    a_type t_char = {}, t_str_arr = {}, t_str_arr0 = {};
    t_char.kind = tk_integer; t_char.size = 1; t_char.variant.integer.int_kind = (an_integer_kind)ik_char;
    t_str_arr.kind = tk_array; t_str_arr.size = 8; t_str_arr.variant.array.element_type = &t_char; t_str_arr.variant.array.variant.number_of_elements = 8;
    t_str_arr0.kind = tk_array; t_str_arr0.size = 0; t_str_arr0.variant.array.element_type = &t_char;

    a_field sf0 = {}, sf1 = {}, sf2 = {};
    sf0.source_corresp.name = NULL; sf0.type = &t_i32; sf0.offset = 0; sf0.next = &sf1;
    sf1.type = &t_arr_sz; sf1.offset = 4; sf1.next = &sf2;
    sf2.type = &t_i32; sf2.offset = 12; sf2.next = NULL;
    a_type t_struct1 = {}, t_struct3 = {};
    a_field s1_f = {}; s1_f.type = &t_i32; s1_f.offset = 0;
    t_struct1.kind = tk_struct; t_struct1.size = 4; t_struct1.variant.class_struct_union.field_list = &s1_f;
    t_struct3.kind = tk_struct; t_struct3.size = 16; t_struct3.variant.class_struct_union.field_list = &sf0;

    a_type t_const_ref = {};
    t_const_ref.kind = tk_typeref; t_const_ref.variant.typeref.type = &t_i32; t_const_ref.variant.typeref.qualifiers = TQ_CONST;

    a_type t_bad_bitint = {};
    t_bad_bitint.kind = tk_integer; t_bad_bitint.size = 128; t_bad_bitint.variant.integer.int_kind = (an_integer_kind)ik_bit_precise;

    /* 1. Incomplete extern arrays (_ZTV, normal, NULL name), VLA extern, sized extern, void type, lower_type error, cached hit */
    a_variable v_ztv = {}, v_inc = {}, v_inc_noname = {}, v_vla = {}, v_sz = {}, v_void = {}, v_bad = {};
    v_ztv.storage_class = (a_storage_class)sc_extern; v_ztv.init_kind = (an_init_kind)initk_none; v_ztv.type = &t_arr_inc; v_ztv.source_corresp.name = "_ZTV3Ext";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_ztv, &lv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_ztv, &lv)); /* cached hit */
    v_inc.storage_class = (a_storage_class)sc_extern; v_inc.init_kind = (an_init_kind)initk_none; v_inc.type = &t_arr_inc; v_inc.source_corresp.name = "ext_arr";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_inc, &lv));
    v_inc_noname.storage_class = (a_storage_class)sc_extern; v_inc_noname.init_kind = (an_init_kind)initk_none; v_inc_noname.type = &t_arr_inc; v_inc_noname.source_corresp.name = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_inc_noname, &lv));
    v_vla.storage_class = (a_storage_class)sc_extern; v_vla.init_kind = (an_init_kind)initk_none; v_vla.type = &t_arr_vla; v_vla.source_corresp.name = "vla_arr";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_vla, &lv));
    v_sz.storage_class = (a_storage_class)sc_extern; v_sz.init_kind = (an_init_kind)initk_none; v_sz.type = &t_arr_sz; v_sz.source_corresp.name = "sz_arr";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_sz, &lv));
    {
        a_variable v_ext_z = {}, v_ext_nt = {};
        v_ext_z.storage_class = (a_storage_class)sc_extern; v_ext_z.init_kind = (an_init_kind)initk_zero; v_ext_z.type = &t_i32; v_ext_z.source_corresp.name = "ext_z";
        v_ext_nt.storage_class = (a_storage_class)sc_extern; v_ext_nt.init_kind = (an_init_kind)initk_none; v_ext_nt.type = NULL; v_ext_nt.source_corresp.name = "ext_nt";
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_ext_z, &lv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_ext_nt, &lv));
    }
    v_void.type = &t_void; v_void.source_corresp.name = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_void, &lv));
    v_bad.type = &t_bad_bitint; v_bad.source_corresp.name = "bad_bi";
    if (gcc_gen_be_lower_global_variable_decl(&v_bad, &lv) != GCC_GEN_BE_ERROR_BITINT_UNSUPPORTED) return GCC_GEN_BE_ERROR_INTERNAL;

    /* 2. Canonical & fallback variable mangling across nested scopes */
    a_namespace ns_named = {}, ns_anon = {}, ns_nullnm = {};
    ns_named.source_corresp.name = "outer";
    ns_anon.source_corresp.name = "";
    ns_nullnm.source_corresp.name = NULL;
    a_type cls_named = {}, cls_unnamed = {};
    cls_named.kind = tk_class; cls_named.size = 4; cls_named.source_corresp.name = "Box";
    cls_unnamed.kind = tk_class; cls_unnamed.size = 4; cls_unnamed.source_corresp.name = NULL;

    a_scope sc_file = {}, sc_ns1 = {}, sc_ns_anon = {}, sc_ns_nullnm = {}, sc_ns_null = {}, sc_cls1 = {}, sc_cls_un = {}, sc_cls_null = {}, sc_fn_mid = {}, sc_ns_orphan = {};
    sc_file.kind = sck_file;
    sc_ns1.kind = sck_namespace; sc_ns1.variant.assoc_namespace = &ns_named; sc_ns1.parent = &sc_file;
    sc_fn_mid.kind = sck_function; sc_fn_mid.parent = &sc_ns1;
    sc_ns_anon.kind = sck_namespace; sc_ns_anon.variant.assoc_namespace = &ns_anon; sc_ns_anon.parent = &sc_fn_mid;
    sc_ns_nullnm.kind = sck_namespace; sc_ns_nullnm.variant.assoc_namespace = &ns_nullnm; sc_ns_nullnm.parent = &sc_ns_anon;
    sc_ns_null.kind = sck_namespace; sc_ns_null.variant.assoc_namespace = NULL; sc_ns_null.parent = &sc_ns_nullnm;
    sc_cls_null.kind = sck_class_struct_union; sc_cls_null.variant.assoc_type = NULL; sc_cls_null.parent = &sc_ns_null;
    sc_cls1.kind = sck_class_struct_union; sc_cls1.variant.assoc_type = &cls_named; sc_cls1.parent = &sc_cls_null;
    sc_cls_un.kind = sck_class_struct_union; sc_cls_un.variant.assoc_type = &cls_unnamed; sc_cls_un.parent = &sc_cls1;
    sc_ns_orphan.kind = sck_namespace; sc_ns_orphan.variant.assoc_namespace = &ns_named; sc_ns_orphan.parent = NULL;

    /* Fallback mangler (canonical_mangle_var == NULL): nested scopes, orphan parent==NULL, 65-deep chain, empty name, _Z name, file scope */
    gcc_gen_be_singleton_state.canonical_mangle_var = NULL;
    a_variable vf1 = {}, vf_orph = {}, vf_emp = {}, vf_z = {}, vf_file = {}, vf_deep = {};
    vf1.type = &t_i32; vf1.source_corresp.name = "member_v"; vf1.source_corresp.parent_scope = &sc_cls_un;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vf1, &lv));
    vf_orph.type = &t_i32; vf_orph.source_corresp.name = "orph_v"; vf_orph.source_corresp.parent_scope = &sc_ns_orphan;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vf_orph, &lv));
    vf_emp.type = &t_i32; vf_emp.source_corresp.name = ""; vf_emp.source_corresp.parent_scope = &sc_ns1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vf_emp, &lv));
    vf_z.type = &t_i32; vf_z.source_corresp.name = "_ZN1X1yE"; vf_z.source_corresp.parent_scope = &sc_ns1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vf_z, &lv));
    vf_file.type = &t_i32; vf_file.source_corresp.name = "file_v"; vf_file.source_corresp.parent_scope = &sc_file;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vf_file, &lv));

    a_scope deep_scopes[66] = {};
    for (int i = 0; i < 66; ++i) {
        deep_scopes[i].kind = sck_namespace;
        deep_scopes[i].variant.assoc_namespace = &ns_named;
        deep_scopes[i].parent = (i == 0) ? NULL : &deep_scopes[i - 1];
    }
    vf_deep.type = &t_i32; vf_deep.source_corresp.name = "dv"; vf_deep.source_corresp.parent_scope = &deep_scopes[65];
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vf_deep, &lv));

    /* Canonical mangler: non-empty, empty, NULL, static skip, _Z skip, __ skip, template var, nlk_external */
    gcc_gen_be_singleton_state.canonical_mangle_var = &mock_canon_mangle_var;
    a_variable vc1 = {}, vc_emp = {}, vc_null = {}, vc_st = {}, vc_dunder = {}, vc_extc = {}, vc_cls = {};
    g_canon_var_ret = "_ZN2ns5vc1_mE";
    vc1.type = &t_i32; vc1.source_corresp.name = "vc1"; vc1.source_corresp.name_linkage = (a_name_linkage_kind)nlk_cplusplus_external;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vc1, &lv));
    g_canon_var_ret = "";
    vc_emp.type = &t_i32; vc_emp.source_corresp.name = "vc_emp"; vc_emp.is_template_variable = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vc_emp, &lv));
    g_canon_var_ret = NULL;
    vc_null.type = &t_i32; vc_null.source_corresp.name = "vc_null"; vc_null.source_corresp.parent_scope = &sc_ns1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vc_null, &lv));
    g_canon_var_ret = "_ZN2ns5vc_clE";
    vc_cls.type = &t_i32; vc_cls.source_corresp.name = "vc_cls"; vc_cls.source_corresp.parent_scope = &sc_cls1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vc_cls, &lv));
    vc_st.type = &t_i32; vc_st.source_corresp.name = "vc_st"; vc_st.storage_class = (a_storage_class)sc_static; vc_st.source_corresp.name_linkage = (a_name_linkage_kind)nlk_cplusplus_external;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vc_st, &lv));
    vc_dunder.type = &t_i32; vc_dunder.source_corresp.name = "__dunder_v"; vc_dunder.source_corresp.name_linkage = (a_name_linkage_kind)nlk_cplusplus_external;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vc_dunder, &lv));
    {
        a_variable vc_z = {}; vc_z.type = &t_i32; vc_z.source_corresp.name = "_Zalready"; vc_z.source_corresp.name_linkage = (a_name_linkage_kind)nlk_cplusplus_external;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vc_z, &lv));
    }
    vc_extc.type = &t_i32; vc_extc.source_corresp.name = "vc_extc"; vc_extc.source_corresp.name_linkage = (a_name_linkage_kind)nlk_external; vc_extc.source_corresp.parent_scope = &sc_ns1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vc_extc, &lv));
    gcc_gen_be_singleton_state.canonical_mangle_var = NULL;

    /* 3. Duplicate symbol resolution in GCC_GEN_BE_CACHE_STRING */
    a_constant c_int = {}; c_int.kind = (a_constant_repr_kind)ck_integer;
    gcc_jit_type *i32_jt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_rvalue *c_i32_rv = gcc_jit_context_new_rvalue_from_int(ctx, i32_jt, 42);

    a_variable dup_base = {}, dup_static = {}, dup_dunder = {}, dup_def = {}, dup_def_null = {};
    dup_base.type = &t_i32; dup_base.source_corresp.name = "dup_sym"; dup_base.storage_class = (a_storage_class)sc_extern;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&dup_base, &lv));
    dup_static.type = &t_i32; dup_static.source_corresp.name = "dup_sym"; dup_static.storage_class = (a_storage_class)sc_static;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&dup_static, &lv));
    dup_def.type = &t_i32; dup_def.source_corresp.name = "dup_sym"; dup_def.init_kind = (an_init_kind)initk_static; dup_def.initializer.constant = &c_int;
    g_mock_const_rval = c_i32_rv;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&dup_def, &lv));
    dup_def_null.type = &t_i32; dup_def_null.source_corresp.name = "dup_sym"; dup_def_null.init_kind = (an_init_kind)initk_static; dup_def_null.initializer.constant = &c_int;
    g_mock_const_rval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&dup_def_null, &lv));
    {
        a_variable dup_nocon = {}; dup_nocon.type = &t_i32; dup_nocon.source_corresp.name = "dup_sym"; dup_nocon.init_kind = (an_init_kind)initk_static; dup_nocon.initializer.constant = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&dup_nocon, &lv));
    }

    a_variable dup_dun2 = {};
    dup_dun2.type = &t_i32; dup_dun2.source_corresp.name = "__dunder_v"; dup_dun2.storage_class = (a_storage_class)sc_static;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&dup_dun2, &lv));

    /* Pre-seeded RTTI/VTable lvalues in CACHE_STRING (where existing_named is a gcc_jit_lvalue*, not in CACHE_VAR) */
    const char *rtti_pfxs[] = { "_ZTV4Pre", "_ZTT4Pre", "_ZTC4Pre", "_ZTI4Pre", "_ZTS4Pre", "_ZZ4Pref", "non_rtti_pre" };
    a_variable vr_arr[7] = {};
    for (size_t i = 0; i < sizeof(rtti_pfxs) / sizeof(rtti_pfxs[0]); ++i) {
        gcc_jit_lvalue *pre_g = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_EXPORTED, i32_jt, rtti_pfxs[i]);
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)rtti_pfxs[i], pre_g));
        vr_arr[i].type = &t_i32; vr_arr[i].source_corresp.name = rtti_pfxs[i];
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vr_arr[i], &lv));
    }

    /* Vague globals across all prefix triggers & flags */
    const char *vague_names[] = {
        "v_inline", "v_weak", "_ZTV5Vague", "_ZTT5Vague", "_ZTC5Vague", "_ZTI5Vague", "_ZTS5Vague", "_ZGV5Vague", "_ZZ5Vague"
    };
    a_variable vv_arr[9] = {};
    for (size_t i = 0; i < sizeof(vague_names) / sizeof(vague_names[0]); ++i) {
        vv_arr[i].type = &t_i32; vv_arr[i].source_corresp.name = vague_names[i];
        if (i == 0) vv_arr[i].is_inline = TRUE;
        if (i == 1) vv_arr[i].is_weak = TRUE;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vv_arr[i], &lv));
    }

    /* 4. TLS models across EXPORTED, INTERNAL, IMPORTED and fPIC/fPIE */
    auto saved_tls = p_gcc_jit_lvalue_set_tls_model;
    p_gcc_jit_lvalue_set_tls_model = &mock_set_tls_model;
    a_variable vt_exp_arr[3] = {}, vt_int_arr[3] = {}, vt_imp_arr[3] = {};
    char b0_arr[3][32], b1_arr[3][32], b2_arr[3][32];
    for (int pic = 0; pic < 3; ++pic) {
        gcc_be_fPIC = (pic == 1) ? TRUE : FALSE;
        gcc_be_fPIE = (pic == 2) ? TRUE : FALSE;
        std::snprintf(b0_arr[pic], sizeof(b0_arr[pic]), "tls_exp_%d", pic);
        std::snprintf(b1_arr[pic], sizeof(b1_arr[pic]), "tls_int_%d", pic);
        std::snprintf(b2_arr[pic], sizeof(b2_arr[pic]), "tls_imp_%d", pic);
        vt_exp_arr[pic].type = &t_i32; vt_exp_arr[pic].source_corresp.name = b0_arr[pic]; vt_exp_arr[pic].is_thread_local = TRUE;
        vt_int_arr[pic].type = &t_i32; vt_int_arr[pic].source_corresp.name = b1_arr[pic]; vt_int_arr[pic].is_thread_local = TRUE; vt_int_arr[pic].storage_class = (a_storage_class)sc_static;
        vt_imp_arr[pic].type = &t_i32; vt_imp_arr[pic].source_corresp.name = b2_arr[pic]; vt_imp_arr[pic].is_thread_local = TRUE; vt_imp_arr[pic].storage_class = (a_storage_class)sc_extern;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vt_exp_arr[pic], &lv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vt_int_arr[pic], &lv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vt_imp_arr[pic], &lv));
    }
    gcc_be_fPIC = FALSE; gcc_be_fPIE = FALSE;
    p_gcc_jit_lvalue_set_tls_model = NULL;
    {
        a_variable vt_no_fn = {}; vt_no_fn.type = &t_i32; vt_no_fn.source_corresp.name = "tls_nofn"; vt_no_fn.is_thread_local = TRUE;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vt_no_fn, &lv));
    }
    p_gcc_jit_lvalue_set_tls_model = saved_tls;

    /* 5. constant_has_non_zero_address_offset + TQ_CONST + initk_zero + initk_static + initk_dynamic */
    a_variable v_z0 = {}, v_zvoid = {};
    v_z0.type = &t_i32; v_z0.source_corresp.name = "g_zero"; v_z0.init_kind = (an_init_kind)initk_zero;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_z0, &lv));
    v_zvoid.type = NULL; v_zvoid.source_corresp.name = "g_znull"; v_zvoid.init_kind = (an_init_kind)initk_zero;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_zvoid, &lv));

    /* ck_string array initializers: sized, undersized, 0-sized, empty string, NULL string */
    a_constant c_str = {}, c_str_long = {}, c_str_emp = {};
    c_str.kind = (a_constant_repr_kind)ck_string; c_str.variant.string.value = "hi"; c_str.variant.string.length = 3;
    c_str_long.kind = (a_constant_repr_kind)ck_string; c_str_long.variant.string.value = "0123456789"; c_str_long.variant.string.length = 11;
    c_str_emp.kind = (a_constant_repr_kind)ck_string; c_str_emp.variant.string.value = NULL; c_str_emp.variant.string.length = 0;

    a_variable vs1 = {}, vs2 = {}, vs3 = {};
    vs1.type = &t_str_arr; vs1.source_corresp.name = "s_arr1"; vs1.init_kind = (an_init_kind)initk_static; vs1.initializer.constant = &c_str;
    vs2.type = &t_str_arr; vs2.source_corresp.name = "s_arr2"; vs2.init_kind = (an_init_kind)initk_static; vs2.initializer.constant = &c_str_long;
    vs3.type = &t_str_arr0; vs3.source_corresp.name = "s_arr3"; vs3.init_kind = (an_init_kind)initk_static; vs3.initializer.constant = &c_str_emp;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vs1, &lv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vs2, &lv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vs3, &lv));

    /* Address offset cases in constant_has_non_zero_address_offset */
    a_variable addr_arr_v = {}, addr_arr0_v = {}, addr_arr_null_v = {}, addr_arr_tn_v = {}, addr_st_v = {}, addr_int_v = {}, addr_vnull = {};
    a_type t_arr_z = {}, t_arr_ne = {}, t_tref_null = {}, t_arr_tn = {};
    t_tref_null.kind = tk_typeref; t_tref_null.variant.typeref.type = NULL;
    t_arr_z.kind = tk_array; t_arr_z.variant.array.element_type = &t_void;
    t_arr_ne.kind = tk_array; t_arr_ne.variant.array.element_type = NULL;
    t_arr_tn.kind = tk_array; t_arr_tn.size = 8; t_arr_tn.variant.array.element_type = &t_tref_null;
    addr_arr_v.type = &t_arr_sz;
    addr_arr0_v.type = &t_arr_z;
    addr_arr_null_v.type = &t_arr_ne;
    addr_arr_tn_v.type = &t_arr_tn;
    addr_st_v.type = &t_struct3;
    addr_int_v.type = &t_i32;
    addr_vnull.type = NULL;

    a_field bad_arr_fld = {}, null_tp_fld = {}, null_el_fld = {}, tn_el_fld = {};
    null_tp_fld.type = NULL; null_tp_fld.offset = 0; null_tp_fld.next = &null_el_fld;
    null_el_fld.type = &t_arr_ne; null_el_fld.offset = 0; null_el_fld.next = &tn_el_fld;
    tn_el_fld.type = &t_arr_tn; tn_el_fld.offset = 0; tn_el_fld.next = &bad_arr_fld;
    bad_arr_fld.type = &t_arr_z; bad_arr_fld.offset = 4; bad_arr_fld.next = NULL;
    a_type t_struct_bad_arr = {};
    t_struct_bad_arr.kind = tk_class; t_struct_bad_arr.size = 16; t_struct_bad_arr.variant.class_struct_union.field_list = &null_tp_fld;
    t_arr_z.size = 8;
    a_variable addr_st_bad_v = {}; addr_st_bad_v.type = &t_struct_bad_arr;

    a_constant addr_cases[14] = {};
    /* 0: offset == 0 */
    addr_cases[0].kind = (a_constant_repr_kind)ck_address; addr_cases[0].variant.address.offset = 0;
    /* 1: abk_routine with offset != 0 -> has_offset TRUE */
    addr_cases[1].kind = (a_constant_repr_kind)ck_address; addr_cases[1].variant.address.offset = 16; addr_cases[1].variant.address.kind = (an_address_base_kind)abk_routine;
    /* 2: abk_variable NULL -> TRUE */
    addr_cases[2].kind = (a_constant_repr_kind)ck_address; addr_cases[2].variant.address.offset = 4; addr_cases[2].variant.address.kind = (an_address_base_kind)abk_variable;
    /* 3: array aligned offset -> FALSE */
    addr_cases[3].kind = (a_constant_repr_kind)ck_address; addr_cases[3].variant.address.offset = 4; addr_cases[3].variant.address.kind = (an_address_base_kind)abk_variable; addr_cases[3].variant.address.variant.variable = &addr_arr_v;
    /* 4: array unaligned offset -> TRUE */
    addr_cases[4].kind = (a_constant_repr_kind)ck_address; addr_cases[4].variant.address.offset = 2; addr_cases[4].variant.address.kind = (an_address_base_kind)abk_variable; addr_cases[4].variant.address.variant.variable = &addr_arr_v;
    /* 5: array 0-sized element -> TRUE */
    addr_cases[5].kind = (a_constant_repr_kind)ck_address; addr_cases[5].variant.address.offset = 4; addr_cases[5].variant.address.kind = (an_address_base_kind)abk_variable; addr_cases[5].variant.address.variant.variable = &addr_arr0_v;
    /* 6: array NULL element -> TRUE */
    addr_cases[6].kind = (a_constant_repr_kind)ck_address; addr_cases[6].variant.address.offset = 4; addr_cases[6].variant.address.kind = (an_address_base_kind)abk_variable; addr_cases[6].variant.address.variant.variable = &addr_arr_null_v;
    /* 7: struct exact field offset (12) -> FALSE */
    addr_cases[7].kind = (a_constant_repr_kind)ck_address; addr_cases[7].variant.address.offset = 12; addr_cases[7].variant.address.kind = (an_address_base_kind)abk_variable; addr_cases[7].variant.address.variant.variable = &addr_st_v;
    /* 8: struct interior array field aligned (8) -> FALSE */
    addr_cases[8].kind = (a_constant_repr_kind)ck_address; addr_cases[8].variant.address.offset = 8; addr_cases[8].variant.address.kind = (an_address_base_kind)abk_variable; addr_cases[8].variant.address.variant.variable = &addr_st_v;
    /* 9: struct interior array field unaligned (2 < sf1.offset 4, and 6 inside sf1) -> TRUE */
    addr_cases[9].kind = (a_constant_repr_kind)ck_address; addr_cases[9].variant.address.offset = 6; addr_cases[9].variant.address.kind = (an_address_base_kind)abk_variable; addr_cases[9].variant.address.variant.variable = &addr_st_v;
    /* 10: struct interior NULL ftp / NULL element_type / NULL fetp / off < fld->offset -> TRUE */
    addr_cases[10].kind = (a_constant_repr_kind)ck_address; addr_cases[10].variant.address.offset = 2; addr_cases[10].variant.address.kind = (an_address_base_kind)abk_variable; addr_cases[10].variant.address.variant.variable = &addr_st_bad_v;
    /* 11: scalar variable offset -> TRUE */
    addr_cases[11].kind = (a_constant_repr_kind)ck_address; addr_cases[11].variant.address.offset = 2; addr_cases[11].variant.address.kind = (an_address_base_kind)abk_variable; addr_cases[11].variant.address.variant.variable = &addr_int_v;
    /* 12: variable with NULL type -> TRUE */
    addr_cases[12].kind = (a_constant_repr_kind)ck_address; addr_cases[12].variant.address.offset = 2; addr_cases[12].variant.address.kind = (an_address_base_kind)abk_variable; addr_cases[12].variant.address.variant.variable = &addr_vnull;
    /* 13: array with typeref->NULL element -> TRUE */
    addr_cases[13].kind = (a_constant_repr_kind)ck_address; addr_cases[13].variant.address.offset = 2; addr_cases[13].variant.address.kind = (an_address_base_kind)abk_variable; addr_cases[13].variant.address.variant.variable = &addr_arr_tn_v;

    g_mock_const_rval = c_i32_rv;
    a_variable va_arr[14] = {};
    char anm_arr[14][32];
    for (int i = 0; i < 14; ++i) {
        std::snprintf(anm_arr[i], sizeof(anm_arr[i]), "g_addr_%d", i);
        va_arr[i].type = &t_const_ref; va_arr[i].source_corresp.name = anm_arr[i];
        va_arr[i].init_kind = (an_init_kind)initk_static; va_arr[i].initializer.constant = &addr_cases[i];
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&va_arr[i], &lv));
    }
    /* Aggregate constant containing false then true sub-constant, plus _ZTV/_ZTT/_ZTI/anon reloc exemptions */
    a_constant agg_e0 = addr_cases[3], agg_e0b = addr_cases[8], agg_e1 = addr_cases[1], c_agg = {}, c_agg_empty = {};
    agg_e0.next = &agg_e0b; agg_e0b.next = &agg_e1; agg_e1.next = NULL;
    c_agg.kind = (a_constant_repr_kind)ck_aggregate; c_agg.variant.aggregate.first_constant = &agg_e0;
    c_agg_empty.kind = (a_constant_repr_kind)ck_aggregate; c_agg_empty.variant.aggregate.first_constant = NULL;
    {
        a_variable v_agg = {}, v_agg_e = {}, v_ztv_rel = {}, v_ztt_rel = {}, v_zti_rel = {}, v_anon_rel = {};
        v_agg.type = &t_i32; v_agg.source_corresp.name = "g_agg_rel"; v_agg.init_kind = (an_init_kind)initk_static; v_agg.initializer.constant = &c_agg;
        v_agg_e.type = &t_const_ref; v_agg_e.source_corresp.name = NULL; v_agg_e.init_kind = (an_init_kind)initk_static; v_agg_e.initializer.constant = &c_agg_empty;
        v_ztv_rel.type = &t_i32; v_ztv_rel.source_corresp.name = "_ZTV6RelExm"; v_ztv_rel.init_kind = (an_init_kind)initk_static; v_ztv_rel.initializer.constant = &addr_cases[1];
        v_ztt_rel.type = &t_i32; v_ztt_rel.source_corresp.name = "_ZTT6RelExm"; v_ztt_rel.init_kind = (an_init_kind)initk_static; v_ztt_rel.initializer.constant = &addr_cases[1];
        v_zti_rel.type = &t_i32; v_zti_rel.source_corresp.name = "_ZTI6RelExm"; v_zti_rel.init_kind = (an_init_kind)initk_static; v_zti_rel.initializer.constant = &addr_cases[1];
        v_anon_rel.type = &t_i32; v_anon_rel.source_corresp.name = NULL; v_anon_rel.init_kind = (an_init_kind)initk_static; v_anon_rel.initializer.constant = &addr_cases[1];
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_agg, &lv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_agg_e, &lv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_ztv_rel, &lv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_ztt_rel, &lv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_zti_rel, &lv));
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_anon_rel, &lv));
    }

    /* Static init type coercions: ptr->ptr, int->1-field-struct, int->3-field-struct, int->i64, NULL init_rval, NULL init_con */
    gcc_jit_rvalue *vptr_null = gcc_jit_context_null(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR));
    {
        a_variable vp1 = {}, vp2_ip = {}, vs_w1 = {}, vs_w3 = {}, vi64 = {}, v_nr = {}, v_nc1 = {}, v_nc_c = {}, v_nc_z = {}, v_nconst_ref = {}, v_nt_con = {}, v_arr_int = {};
        a_type t_nonconst_ref = {}; t_nonconst_ref.kind = tk_typeref; t_nonconst_ref.variant.typeref.type = &t_i32; t_nonconst_ref.variant.typeref.qualifiers = 0;
        g_mock_const_rval = vptr_null;
        vp1.type = &t_i64; vp1.source_corresp.name = "g_pcast"; vp1.init_kind = (an_init_kind)initk_static; vp1.initializer.constant = &c_int;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vp1, &lv));
        g_mock_const_rval = c_i32_rv;
        vp2_ip.type = &t_ptr; vp2_ip.source_corresp.name = "g_ipcast"; vp2_ip.init_kind = (an_init_kind)initk_static; vp2_ip.initializer.constant = &c_int;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vp2_ip, &lv));
        v_nconst_ref.type = &t_nonconst_ref; v_nconst_ref.source_corresp.name = "g_ncref"; v_nconst_ref.init_kind = (an_init_kind)initk_static; v_nconst_ref.initializer.constant = &c_int;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_nconst_ref, &lv));
        v_nt_con.type = NULL; v_nt_con.source_corresp.name = "g_ntcon"; v_nt_con.init_kind = (an_init_kind)initk_static; v_nt_con.initializer.constant = &c_int;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_nt_con, &lv));
        v_arr_int.type = &t_arr_sz; v_arr_int.source_corresp.name = "g_arrint"; v_arr_int.init_kind = (an_init_kind)initk_static; v_arr_int.initializer.constant = &c_int;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_arr_int, &lv));
        vs_w1.type = &t_struct1; vs_w1.source_corresp.name = "g_sw1"; vs_w1.init_kind = (an_init_kind)initk_static; vs_w1.initializer.constant = &c_int;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vs_w1, &lv));
        vs_w3.type = &t_struct3; vs_w3.source_corresp.name = "g_sw3"; vs_w3.init_kind = (an_init_kind)initk_static; vs_w3.initializer.constant = &c_int;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vs_w3, &lv));
        vi64.type = &t_i64; vi64.source_corresp.name = "g_i64m"; vi64.init_kind = (an_init_kind)initk_static; vi64.initializer.constant = &c_int;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vi64, &lv));
        g_mock_const_rval = NULL;
        v_nr.type = &t_i32; v_nr.source_corresp.name = "g_nr"; v_nr.init_kind = (an_init_kind)initk_static; v_nr.initializer.constant = &c_int;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_nr, &lv));

        v_nc1.type = &t_i32; v_nc1.source_corresp.name = "g_nc1"; v_nc1.init_kind = (an_init_kind)initk_static; v_nc1.initializer.constant = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_nc1, &lv));
        v_nc_c.type = &t_i32; v_nc_c.source_corresp.name = "__C99"; v_nc_c.storage_class = (a_storage_class)sc_static; v_nc_c.init_kind = (an_init_kind)initk_static; v_nc_c.initializer.constant = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_nc_c, &lv));
        v_nc_z.type = NULL; v_nc_z.source_corresp.name = "g_nc_z"; v_nc_z.storage_class = (a_storage_class)sc_static; v_nc_z.init_kind = (an_init_kind)initk_static; v_nc_z.initializer.constant = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&v_nc_z, &lv));
    }

    /* Dynamic initializers: NULL dynamic, dik_none, dik_constructor (block kept & cleared), dtor (uncached, cached, thread_local, builtin-null dtor) */
    a_routine dtor_r = {}, dtor_null_r = {};
    dtor_r.source_corresp.name = "my_g_dtor"; dtor_r.memory_region = 99;
    dtor_null_r.source_corresp.name = "__builtin_dtor_noop";

    a_dynamic_init di_none = {}, di_ctor = {}, di_tls = {}, di_fail = {};
    di_none.kind = (a_dynamic_init_kind)dik_none; di_none.destructor = NULL;
    di_ctor.kind = (a_dynamic_init_kind)dik_class_result_via_ctor; di_ctor.destructor = &dtor_r;
    di_tls.kind = (a_dynamic_init_kind)dik_none; di_tls.destructor = &dtor_r;
    di_fail.kind = (a_dynamic_init_kind)dik_none; di_fail.destructor = &dtor_null_r;

    a_variable vd_null = {}, vd0 = {}, vd1 = {}, vd2 = {}, vd3 = {}, vd_const = {};
    vd_null.type = &t_i32; vd_null.source_corresp.name = "vd_null"; vd_null.init_kind = (an_init_kind)initk_dynamic; vd_null.initializer.dynamic = NULL;
    vd0.type = &t_i32; vd0.source_corresp.name = "vd0"; vd0.init_kind = (an_init_kind)initk_dynamic; vd0.initializer.dynamic = &di_none;
    vd1.type = &t_i32; vd1.source_corresp.name = "vd1"; vd1.init_kind = (an_init_kind)initk_dynamic; vd1.initializer.dynamic = &di_ctor;
    vd2.type = &t_i32; vd2.source_corresp.name = "vd2"; vd2.is_thread_local = TRUE; vd2.init_kind = (an_init_kind)initk_dynamic; vd2.initializer.dynamic = &di_tls;
    vd3.type = &t_i32; vd3.source_corresp.name = "vd3"; vd3.init_kind = (an_init_kind)initk_dynamic; vd3.initializer.dynamic = &di_fail;
    vd_const.type = &t_const_ref; vd_const.source_corresp.name = "vd_const"; vd_const.init_kind = (an_init_kind)initk_dynamic; vd_const.initializer.dynamic = &di_ctor;
    a_variable vd_rewritten = {};
    vd_rewritten.type = &t_const_ref; vd_rewritten.source_corresp.name = "vd_rewritten"; vd_rewritten.init_kind = (an_init_kind)initk_zero; vd_rewritten.initialization_rewritten_as_assignment = TRUE;
    a_type t_mut_cls = {}, t_const_mut = {};
    t_mut_cls.kind = (a_type_kind)tk_class; t_mut_cls.size = 4; t_mut_cls.variant.class_struct_union.any_mutable_member = TRUE;
    t_const_mut.kind = (a_type_kind)tk_typeref; t_const_mut.variant.typeref.type = &t_mut_cls; t_const_mut.variant.typeref.qualifiers = TQ_CONST;
    a_variable vd_mut = {};
    vd_mut.type = &t_const_mut; vd_mut.source_corresp.name = "vd_mut"; vd_mut.init_kind = (an_init_kind)initk_zero;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vd_null, &lv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vd0, &lv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vd1, &lv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vd_const, &lv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vd_rewritten, &lv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vd_mut, &lv));
    g_mock_dyn_clears_block = TRUE;
    a_variable vd1b = {}; vd1b.type = &t_i32; vd1b.source_corresp.name = "vd1b"; vd1b.init_kind = (an_init_kind)initk_dynamic; vd1b.initializer.dynamic = &di_ctor;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vd1b, &lv));
    g_mock_dyn_clears_block = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vd2, &lv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&vd3, &lv));

    /* Deterministic OOM sweeps on gcc_gen_be_lower_global_variable_decl */
    for (int cfg = 0; cfg < 9; ++cfg) {
        for (int b = 0; b < 100; ++b) {
            GCC_GEN_BE_CHECK(cache_clear_all());
            gcc_gen_be_singleton_state.global_ctor_func = NULL;
            gcc_gen_be_singleton_state.global_ctor_block = NULL;
            a_variable vo = {};
            char onm[32]; std::snprintf(onm, sizeof(onm), "oom_gv_%d_%d", cfg, b);
            vo.type = &t_i32;
            vo.source_corresp.name = onm;
            if (cfg == 0) {
                gcc_gen_be_singleton_state.canonical_mangle_var = &mock_canon_mangle_var;
                g_canon_var_ret = "_ZN3oom2gvE";
                vo.source_corresp.name_linkage = (a_name_linkage_kind)nlk_cplusplus_external;
                vo.is_inline = TRUE;
            } else if (cfg == 1) {
                gcc_gen_be_singleton_state.canonical_mangle_var = NULL;
                vo.source_corresp.parent_scope = &sc_cls_un;
                vo.init_kind = (an_init_kind)initk_zero;
            } else if (cfg == 2) {
                vo.type = &t_str_arr;
                vo.init_kind = (an_init_kind)initk_static;
                vo.initializer.constant = &c_str;
            } else if (cfg == 3) {
                g_mock_const_rval = c_i32_rv;
                vo.init_kind = (an_init_kind)initk_static;
                vo.initializer.constant = &c_agg;
            } else if (cfg == 4) {
                vo.init_kind = (an_init_kind)initk_static;
                vo.initializer.constant = NULL;
            } else if (cfg == 5) {
                vo.init_kind = (an_init_kind)initk_dynamic;
                vo.initializer.dynamic = &di_ctor;
            } else if (cfg == 6) {
                a_variable pre = {}; pre.type = &t_i32; pre.source_corresp.name = onm;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&pre, &lv));
                vo.storage_class = (a_storage_class)sc_static;
            } else if (cfg == 7) {
                a_variable pre = {}; pre.type = &t_i32; pre.source_corresp.name = onm;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&pre, &lv));
                vo.init_kind = (an_init_kind)initk_static;
                vo.initializer.constant = &c_int;
                g_mock_const_rval = c_i32_rv;
            } else {
                gcc_jit_lvalue *pre_z = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_EXPORTED, i32_jt, "_ZTV7OomPre");
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"_ZTV7OomPre", pre_z));
                vo.source_corresp.name = "_ZTV7OomPre";
            }
            g_gcc_be_oom_countdown = b;
            gcc_gen_be_error_t e = gcc_gen_be_lower_global_variable_decl(&vo, &lv);
            g_gcc_be_oom_countdown = -1;
            gcc_gen_be_singleton_state.canonical_mangle_var = NULL;
            g_mock_const_rval = NULL;
            if (e == GCC_GEN_BE_SUCCESS) break;
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exercises `gcc_gen_be_lower_local_variable_decl` across all paths and OOM sweeps.
 * @param[in] ctx Active `gcc_jit_context`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_local_vars(gcc_jit_context *ctx) noexcept {
    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *i32_jt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_function *fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, void_t, "local_test_fn", 0, NULL, 0);
    gcc_jit_block *blk = gcc_jit_function_new_block(fn, "entry");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(blk));

    gcc_jit_lvalue *lv = NULL;
    if (gcc_gen_be_lower_local_variable_decl(fn, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_local_variable_decl(NULL, NULL, &lv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(fn, NULL, &lv));

    a_type t_i32 = {}, t_void = {}, t_bad = {};
    t_i32.kind = tk_integer; t_i32.size = 4; t_i32.variant.integer.int_kind = (an_integer_kind)ik_int;
    t_void.kind = tk_void; t_void.size = 0;
    t_bad.kind = tk_integer; t_bad.size = 128; t_bad.variant.integer.int_kind = (an_integer_kind)ik_bit_precise;

    /* Static local forwards to global */
    a_variable v_st = {}; v_st.storage_class = (a_storage_class)sc_static; v_st.type = &t_i32; v_st.source_corresp.name = "static_loc";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(fn, &v_st, &lv));

    /* Null ctx */
    a_variable v_nc = {}; v_nc.type = &t_i32;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_lower_local_variable_decl(fn, &v_nc, &lv) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    /* Bad type */
    a_variable v_bad = {}; v_bad.type = &t_bad;
    if (gcc_gen_be_lower_local_variable_decl(fn, &v_bad, &lv) != GCC_GEN_BE_ERROR_BITINT_UNSUPPORTED) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Coroutine frame field lookup (hit & miss) */
    gcc_jit_field *cfld = gcc_jit_context_new_field(ctx, NULL, i32_jt, "spilled_x");
    gcc_jit_struct *cst = gcc_jit_context_new_struct_type(ctx, NULL, "coro_st", 1, &cfld);
    gcc_jit_lvalue *cptr_lv = gcc_jit_function_new_local(fn, NULL, gcc_jit_type_get_pointer(gcc_jit_struct_as_type(cst)), "frame_p");
    a_variable v_coro = {}, v_coro_nofld = {};
    v_coro.type = &t_i32; v_coro.source_corresp.name = "spilled_x";
    v_coro_nofld.type = &t_void; v_coro_nofld.source_corresp.name = NULL;
    g_mock_coro_flval = cptr_lv;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &v_coro, cfld));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(fn, &v_coro, &lv));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(fn, &v_coro, &lv)); /* cached */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(fn, &v_coro_nofld, &lv));
    g_mock_coro_flval = NULL;

    /* initk_static (with rval, NULL rval, block cleared) & initk_zero (non-null & null zero) & cblock == NULL */
    a_constant c_int = {}; c_int.kind = (a_constant_repr_kind)ck_integer;
    gcc_jit_rvalue *rv42 = gcc_jit_context_new_rvalue_from_int(ctx, i32_jt, 42);
    a_variable vl1 = {}, vl2 = {}, vl3 = {}, vl4 = {}, vl5 = {}, vl6 = {};
    vl1.type = &t_i32; vl1.source_corresp.name = "vl1"; vl1.init_kind = (an_init_kind)initk_static; vl1.initializer.constant = &c_int;
    g_mock_const_rval = rv42;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(fn, &vl1, &lv));
    {
        a_variable vl_nocon = {}; vl_nocon.type = &t_i32; vl_nocon.source_corresp.name = "vl_nc"; vl_nocon.init_kind = (an_init_kind)initk_static; vl_nocon.initializer.constant = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(fn, &vl_nocon, &lv));
    }

    vl2.type = &t_i32; vl2.source_corresp.name = "vl2"; vl2.init_kind = (an_init_kind)initk_static; vl2.initializer.constant = &c_int;
    g_mock_const_rval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(fn, &vl2, &lv));

    vl3.type = &t_i32; vl3.source_corresp.name = "vl3"; vl3.init_kind = (an_init_kind)initk_static; vl3.initializer.constant = &c_int;
    g_mock_const_rval = rv42; g_mock_const_clears_block = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(fn, &vl3, &lv));
    g_mock_const_clears_block = FALSE; g_mock_const_rval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(blk));

    vl4.type = &t_i32; vl4.source_corresp.name = "vl4"; vl4.init_kind = (an_init_kind)initk_zero;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(fn, &vl4, &lv));

    vl5.type = NULL; vl5.source_corresp.name = "vl5"; vl5.init_kind = (an_init_kind)initk_zero;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(fn, &vl5, &lv));

    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    vl6.type = &t_i32; vl6.source_corresp.name = "vl6"; vl6.init_kind = (an_init_kind)initk_zero;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_local_variable_decl(fn, &vl6, &lv));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(blk));

    for (int cfg = 0; cfg < 3; ++cfg) {
        for (int b = 0; b < 20; ++b) {
            GCC_GEN_BE_CHECK(cache_clear_all());
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(blk));
            a_variable vlo = {};
            vlo.type = &t_i32; vlo.source_corresp.name = "vlo";
            if (cfg == 0) {
                g_mock_coro_flval = cptr_lv;
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, &vlo, cfld));
            } else if (cfg == 1) {
                vlo.init_kind = (an_init_kind)initk_static; vlo.initializer.constant = &c_int;
                g_mock_const_rval = rv42;
            } else {
                vlo.init_kind = (an_init_kind)initk_zero;
            }
            g_gcc_be_oom_countdown = b;
            gcc_gen_be_error_t e = gcc_gen_be_lower_local_variable_decl(fn, &vlo, &lv);
            g_gcc_be_oom_countdown = -1;
            g_mock_coro_flval = NULL; g_mock_const_rval = NULL;
            if (e == GCC_GEN_BE_SUCCESS) break;
        }
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exercises `gcc_gen_be_lower_function_decl` and `gcc_gen_be_emit_ctor_dtor_clones` across all paths and OOM sweeps.
 * @param[in] ctx Active `gcc_jit_context`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_functions_and_clones(gcc_jit_context *ctx) noexcept {
    gcc_jit_function *fn = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    a_routine r_nc = {};
    if (gcc_gen_be_lower_function_decl(&r_nc, &fn) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    /* Builtin skip prefixes */
    const char *b_pfxs[] = { "__builtin_sin", "__atomic_load_4", "__sync_synchronize", "__c11_atomic_load" };
    a_routine rb_arr[4] = {};
    for (size_t i = 0; i < 4; ++i) {
        rb_arr[i].source_corresp.name = b_pfxs[i];
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&rb_arr[i], &fn));
    }

    a_type t_i32 = {}, t_void = {}, t_arr = {}, t_invis = {}, t_invis0 = {}, t_coerced = {}, t_bad = {}, t_pmf = {};
    t_i32.kind = tk_integer; t_i32.size = 4; t_i32.variant.integer.int_kind = (an_integer_kind)ik_int;
    t_void.kind = tk_void; t_void.size = 0;
    t_arr.kind = tk_array; t_arr.size = 8; t_arr.variant.array.element_type = &t_i32;
    t_invis.kind = tk_struct; t_invis.size = 16;
    t_invis0.kind = tk_struct; t_invis0.size = 0;
    t_coerced.kind = tk_struct; t_coerced.size = 8;
    t_bad.kind = tk_integer; t_bad.size = 128; t_bad.variant.integer.int_kind = (an_integer_kind)ik_bit_precise;
    t_pmf.kind = tk_ptr_to_member; t_pmf.size = 16; t_pmf.variant.ptr_to_member.type = &t_void;
    a_routine_type_supplement pmf_rts = {}; t_pmf.variant.ptr_to_member.type = &t_arr; /* non-routine -> data member ptr */
    (void)pmf_rts;

    gcc_jit_type *i64_jt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
    g_mock_invis_ref_type = &t_invis;
    g_mock_invis_ref_type2 = &t_invis0;
    g_mock_coerced_src_type = &t_coerced;
    g_mock_coerced_jit_type = i64_jt;

    /* Bad return type (with and without sret) & bad param types */
    a_routine_type_supplement rts_bad = {};
    a_type rtp_bad = {}; rtp_bad.kind = tk_routine; rtp_bad.variant.routine.return_type = &t_bad; rtp_bad.variant.routine.extra_info = &rts_bad;
    a_routine r_badret = {}; r_badret.type = &rtp_bad; r_badret.source_corresp.name = "bad_ret";
    if (gcc_gen_be_lower_function_decl(&r_badret, &fn) != GCC_GEN_BE_ERROR_BITINT_UNSUPPORTED) return GCC_GEN_BE_ERROR_INTERNAL;
    g_mock_sret = TRUE;
    if (gcc_gen_be_lower_function_decl(&r_badret, &fn) != GCC_GEN_BE_ERROR_BITINT_UNSUPPORTED) return GCC_GEN_BE_ERROR_INTERNAL;
    g_mock_sret = FALSE;

    a_type t_bad_arr = {}; t_bad_arr.kind = tk_array; t_bad_arr.variant.array.element_type = &t_bad;
    a_param_type pt_bad = {}; pt_bad.type = &t_bad_arr;
    rts_bad.param_type_list = &pt_bad; rtp_bad.variant.routine.return_type = &t_void;
    if (gcc_gen_be_lower_function_decl(&r_badret, &fn) != GCC_GEN_BE_ERROR_BITINT_UNSUPPORTED) return GCC_GEN_BE_ERROR_INTERNAL;
    pt_bad.type = &t_bad;
    if (gcc_gen_be_lower_function_decl(&r_badret, &fn) != GCC_GEN_BE_ERROR_BITINT_UNSUPPORTED) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Mangling & name linkage combinations */
    a_namespace ns = {}; ns.source_corresp.name = "ns";
    a_scope sc_ns = {}, sc_anon = {};
    a_namespace ns_anon = {}; ns_anon.source_corresp.name = NULL;
    sc_ns.kind = sck_namespace; sc_ns.variant.assoc_namespace = &ns;
    sc_anon.kind = sck_namespace; sc_anon.variant.assoc_namespace = &ns_anon;

    a_routine_type_supplement rts0 = {};
    a_type rtp0 = {}; rtp0.kind = tk_routine; rtp0.variant.routine.return_type = &t_coerced; rtp0.variant.routine.extra_info = &rts0;

    /* 1: unmangled_name_or_mangled_encoding with _Z and without _Z, NULL name, main, __fn, explicit extern "C", canonical_mangle_func, fallback mangle */
    a_routine r_enc = {}, r_noenc = {}, r_anon_nm = {}, r_main = {}, r_dund = {}, r_extc1 = {}, r_extc2 = {}, r_tmpl = {}, r_op = {};
    r_enc.type = &rtp0; r_enc.source_corresp.name = "enc"; r_enc.source_corresp.unmangled_name_or_mangled_encoding = "_ZN3enc1fEv";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_enc, &fn));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_enc, &fn)); /* cached hit */

    r_noenc.type = NULL; r_noenc.source_corresp.name = NULL; r_noenc.source_corresp.unmangled_name_or_mangled_encoding = "plain_unmangled";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_noenc, &fn));

    r_main.type = &rtp0; r_main.source_corresp.name = "main";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_main, &fn));
    r_dund.type = &rtp0; r_dund.source_corresp.name = "__helper";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_dund, &fn));

    r_extc1.type = &rtp0; r_extc1.source_corresp.name = "c_fn1"; r_extc1.source_corresp.name_linkage = (a_name_linkage_kind)nlk_external; r_extc1.source_corresp.parent_scope = &sc_ns;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_extc1, &fn));

    rts0.routine_name_linkage_is_explicit = TRUE; rts0.routine_name_linkage = (a_name_linkage_kind)nlk_external;
    r_extc2.type = &rtp0; r_extc2.source_corresp.name = "c_fn2"; r_extc2.source_corresp.parent_scope = &sc_ns;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_extc2, &fn));
    {
        a_scope sc_cls_fn = {}, sc_file_fn = {};
        sc_cls_fn.kind = sck_class_struct_union; sc_file_fn.kind = sck_file;
        a_routine r_extra_cov[6] = {};
        rts0.routine_name_linkage = (a_name_linkage_kind)nlk_cplusplus_external;
        r_extra_cov[0].type = &rtp0; r_extra_cov[0].source_corresp.name = "cxx_expl"; r_extra_cov[0].source_corresp.parent_scope = &sc_cls_fn;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_extra_cov[0], &fn));
        rts0.routine_name_linkage_is_explicit = FALSE;

        r_extra_cov[1].type = &rtp0; r_extra_cov[1].source_corresp.name = "file_fn"; r_extra_cov[1].source_corresp.parent_scope = &sc_file_fn;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_extra_cov[1], &fn));

        r_extra_cov[2].type = &rtp0; r_extra_cov[2].source_corresp.name = "ctor_mng"; r_extra_cov[2].special_kind = (a_special_function_kind)sfk_constructor;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_extra_cov[2], &fn));

        r_extra_cov[3].type = &t_i32; /* non-tk_routine rout->type */ r_extra_cov[3].source_corresp.name = "non_rout_tp";
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_extra_cov[3], &fn));

        rtp0.variant.routine.return_type = NULL; /* frontend_ret == NULL */
        r_extra_cov[4].type = &rtp0; r_extra_cov[4].source_corresp.name = "null_ret_tp";
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_extra_cov[4], &fn));
        rtp0.variant.routine.return_type = &t_coerced;

        /* Imported duplicate symbol where existing_named has no CACHE_FUNC entry */
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"dup_nofn", &r_extra_cov[0]));
        r_extra_cov[5].source_corresp.name = "dup_nofn";
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_extra_cov[5], &fn));
    }

    gcc_gen_be_singleton_state.canonical_mangle_func = &mock_canon_mangle_fn;
    std::snprintf(g_canon_fn_buf, sizeof(g_canon_fn_buf), "_ZN2ns5cfn_aEv");
    r_tmpl.type = &rtp0; r_tmpl.source_corresp.name = "cfn_a"; r_tmpl.is_template_function = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_tmpl, &fn));

    g_canon_fn_buf[0] = '\0';
    r_op.type = &rtp0; r_op.source_corresp.name = "operator+"; r_op.source_corresp.name_linkage = (a_name_linkage_kind)nlk_cplusplus_external;
    g_mock_primary_mangle = "_Zpl1A1A";
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_op, &fn));

    g_canon_fn_null = TRUE;
    g_mock_primary_mangle = ""; /* triggers alloc_mangled_routine_name failure fallback */
    a_routine r_mfail = {}; r_mfail.type = &rtp0; r_mfail.source_corresp.name = "operator-"; r_mfail.source_corresp.name_linkage = (a_name_linkage_kind)nlk_cplusplus_external;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_mfail, &fn));
    g_canon_fn_null = FALSE;
    g_mock_primary_mangle = NULL;
    gcc_gen_be_singleton_state.canonical_mangle_func = NULL;

    /* Duplicate function names: imported reuse, static uniquify, defined uniquify */
    a_routine r_dup_imp1 = {}, r_dup_imp2 = {}, r_dup_st = {}, r_dup_def = {};
    r_dup_imp1.source_corresp.name = "dup_fn";
    r_dup_imp2.source_corresp.name = "dup_fn";
    r_dup_st.source_corresp.name = "dup_fn"; r_dup_st.storage_class = (a_storage_class)sc_static;
    r_dup_def.source_corresp.name = "dup_fn"; r_dup_def.function_def_number = 1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_dup_imp1, &fn));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_dup_imp2, &fn));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_dup_st, &fn));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_dup_def, &fn));

    /* Full defined function with sret, array/invis/invis0/coerced/null-ptype params, variadic, attributes, local_static_variable_inits, locals, RAII destructions, coroutines, error rollback, non-void return */
    a_mem_block_header dummy_blk_hdr = {};
    a_mem_block_header_ptr fake_regions[4] = { NULL, NULL, &dummy_blk_hdr, NULL };
    mem_region_table = fake_regions;
    f_il_output = stdout;

    an_attribute attr_other = {}, attr_ai = {};
    attr_other.kind = (an_attribute_kind)ak_noreturn; attr_other.next = &attr_ai;
    attr_ai.kind = (an_attribute_kind)ak_always_inline; attr_ai.next = NULL;

    a_type t_tref_i32 = {};
    t_tref_i32.kind = tk_typeref; t_tref_i32.variant.typeref.type = &t_i32;

    a_param_type p_arr = {}, p_inv = {}, p_inv2 = {}, p_inv0 = {}, p_coe = {}, p_nul = {}, p_ex = {};
    p_arr.type = &t_arr; p_arr.next = &p_inv;
    p_inv.type = &t_invis; p_inv.next = &p_inv2;
    p_inv2.type = &t_invis; p_inv2.next = &p_inv0;
    p_inv0.type = &t_invis0; p_inv0.next = &p_coe;
    p_coe.type = &t_coerced; p_coe.next = &p_nul;
    p_nul.type = &t_tref_i32; p_nul.next = &p_ex;
    p_ex.type = &t_i32; p_ex.next = NULL;

    a_variable vp1 = {}, vp2 = {}, vp2b = {}, vp3 = {}, vp4 = {}, vp5 = {}, vp_uncached = {};
    vp1.source_corresp.name = "pa"; vp1.type = &t_arr; vp1.next = &vp2;
    vp2.source_corresp.name = NULL; vp2.type = &t_invis; vp2.next = &vp2b;
    vp2b.source_corresp.name = "named_invis"; vp2b.type = &t_invis; vp2b.next = &vp3;
    vp3.source_corresp.name = "pi0"; vp3.type = &t_invis0; vp3.next = &vp4;
    vp4.source_corresp.name = "pc"; vp4.type = &t_coerced; vp4.next = &vp5;
    vp5.source_corresp.name = "pn"; vp5.type = NULL; vp5.next = NULL; /* utype == NULL on unpack! */

    a_variable lsv_v1 = {}, lsv_v2 = {};
    lsv_v1.type = &t_i32; lsv_v1.init_kind = (an_init_kind)initk_function_local;
    lsv_v2.type = &t_i32; lsv_v2.init_kind = (an_init_kind)initk_zero;
    a_local_static_variable_init lsv1 = {}, lsv2 = {}, lsv3 = {};
    lsv1.variable = &lsv_v1; lsv1.init_kind = (an_init_kind)initk_zero; lsv1.next = &lsv2;
    lsv2.variable = &lsv_v2; lsv2.next = &lsv3;
    lsv3.variable = NULL; lsv3.next = NULL;

    a_variable sv_ok = {}, sv_st_named = {}, sv_st_z = {}, sv_parm = {}, sv_vla = {}, sv_lit = {};
    sv_ok.type = &t_i32; sv_ok.source_corresp.name = "sv_ok"; sv_ok.next = &sv_st_named;
    sv_st_named.type = &t_i32; sv_st_named.storage_class = (a_storage_class)sc_static; sv_st_named.source_corresp.name = "sv_stn"; sv_st_named.init_kind = (an_init_kind)initk_static; sv_st_named.next = &sv_st_z;
    sv_st_z.type = &t_i32; sv_st_z.storage_class = (a_storage_class)sc_static; sv_st_z.source_corresp.name = NULL; sv_st_z.init_kind = (an_init_kind)initk_zero; sv_st_z.next = &sv_parm;
    sv_parm.is_parameter = TRUE; sv_parm.next = &sv_vla;
    sv_vla.has_variably_modified_type = TRUE; sv_vla.next = &sv_lit;
    sv_lit.storage_class = (a_storage_class)sc_static; sv_lit.source_corresp.name = NULL; sv_lit.init_kind = (an_init_kind)initk_static; sv_lit.next = NULL;

    a_variable nsv_ok = {}, nsv_p = {}, nsv_v = {};
    nsv_ok.type = &t_i32; nsv_ok.source_corresp.name = "nsv_ok"; nsv_ok.next = &nsv_p;
    nsv_p.is_parameter = TRUE; nsv_p.next = &nsv_v;
    nsv_v.has_variably_modified_type = TRUE; nsv_v.next = NULL;

    a_variable raii_v = {}; raii_v.type = &t_i32; raii_v.source_corresp.name = "raii_v";
    a_routine raii_dtor = {}; raii_dtor.source_corresp.name = "raii_dtor"; raii_dtor.memory_region = 99;
    a_dynamic_init d_ok = {}, d_novar = {}, d_skip = {};
    d_ok.variable = &raii_v; d_ok.destructor = &raii_dtor; d_ok.next_in_destruction_list = &d_novar;
    d_novar.variable = NULL; d_novar.destructor = &raii_dtor; d_novar.next_in_destruction_list = &d_skip;
    d_skip.variable = NULL; d_skip.destructor = NULL; d_skip.next_in_destruction_list = NULL;
    an_object_lifetime life = {}; life.destructions = &d_ok;

    a_statement body_stmt = {}; body_stmt.kind = (a_statement_kind)stmk_empty;
    a_scope def_sc = {};
    def_sc.kind = sck_function;
    def_sc.assoc_block = &body_stmt;
    def_sc.variant.routine.parameters = &vp1;
    def_sc.local_static_variable_inits = &lsv1;
    def_sc.variables = &sv_ok;
    def_sc.nonstatic_variables = &nsv_ok;
    def_sc.lifetime = &life;
    g_mock_def_scope = &def_sc;

    a_routine_type_supplement rts_full = {};
    rts_full.param_type_list = &p_arr;
    rts_full.has_ellipsis = TRUE;
    a_type rtp_full = {};
    rtp_full.kind = tk_routine; rtp_full.variant.routine.return_type = &t_invis; rtp_full.variant.routine.extra_info = &rts_full;

    g_mock_sret = TRUE;
    a_routine r_full = {};
    r_full.type = &rtp_full; r_full.source_corresp.name = "full_defined_fn";
    r_full.source_corresp.parent_scope = &sc_ns; /* exercises should_mangle_cxx via pscope */
    r_full.function_def_number = 1; r_full.memory_region = 1;
    r_full.source_corresp.attributes = &attr_other;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_full, &fn));
    g_mock_sret = FALSE;

    /* Cover mem_region_table[2] != NULL, f_il_output == NULL, mem_region_table == NULL, assoc_block == NULL, and builtin d_fn == NULL in destructions */
    {
        a_routine r_mr[4] = {};
        def_sc.assoc_block = NULL;
        r_mr[0].source_corresp.name = "mr_nonnull"; r_mr[0].function_def_number = 1; r_mr[0].memory_region = 2;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_mr[0], &fn));
        f_il_output = NULL;
        r_mr[1].source_corresp.name = "mr_nofil"; r_mr[1].function_def_number = 1; r_mr[1].memory_region = 1;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_mr[1], &fn));
        mem_region_table = NULL;
        r_mr[2].source_corresp.name = "mr_notbl"; r_mr[2].function_def_number = 1; r_mr[2].memory_region = 1;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_mr[2], &fn));
        mem_region_table = fake_regions; f_il_output = stdout;
        def_sc.assoc_block = &body_stmt;

        a_routine bdtor = {}; bdtor.source_corresp.name = "__builtin_dtor_skip";
        a_dynamic_init d_nullfn = {}; d_nullfn.variable = &raii_v; d_nullfn.destructor = &bdtor;
        an_object_lifetime life_nf = {}; life_nf.destructions = &d_nullfn;
        def_sc.lifetime = &life_nf;
        def_sc.variant.routine.parameters = NULL; def_sc.local_static_variable_inits = NULL;
        def_sc.variables = NULL; def_sc.nonstatic_variables = NULL;
        r_mr[3].source_corresp.name = "mr_bdtor"; r_mr[3].function_def_number = 1;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_mr[3], &fn));

        /* Empty lifetime->destructions, defined sc_static, defined class-member sfk_none, imported with params & always_inline */
        an_object_lifetime life_emp = {}; life_emp.destructions = NULL;
        def_sc.lifetime = &life_emp;
        a_routine r_more[4] = {};
        r_more[0].source_corresp.name = "def_static_fn"; r_more[0].storage_class = (a_storage_class)sc_static; r_more[0].function_def_number = 1;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_more[0], &fn));
        r_more[1].source_corresp.name = "cls_method_fn"; r_more[1].source_corresp.is_class_member = TRUE; r_more[1].special_kind = (a_special_function_kind)sfk_none; r_more[1].function_def_number = 1;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_more[1], &fn));

        a_param_type p_imp_char = {};
        a_type t_u8 = {}; t_u8.kind = tk_integer; t_u8.size = 1; t_u8.variant.integer.int_kind = (an_integer_kind)ik_unsigned_char;
        p_imp_char.type = NULL; /* ptype == NULL lowers to VOID -> use sret-only for def_scope==NULL, plus non-void param! */
        p_imp_char.type = &t_u8;
        a_routine_type_supplement rts_imp = {}; rts_imp.param_type_list = &p_imp_char;
        a_type rtp_imp = {}; rtp_imp.kind = tk_routine; rtp_imp.variant.routine.return_type = &t_void; rtp_imp.variant.routine.extra_info = &rts_imp;
        r_more[2].type = &rtp_imp; r_more[2].source_corresp.name = "imp_ai_fn"; r_more[2].source_corresp.attributes = &attr_ai;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_more[2], &fn));
        p_imp_char.type = NULL; /* ptype == NULL on imported function */
        r_more[3].type = &rtp_imp; r_more[3].source_corresp.name = "imp_null_ptp_fn";
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_more[3], &fn));
    }

    /* Vague linkage flags on defined exported functions (is_weak, is_inline, is_template_function, specialized, compiler_generated, _ZZ, class ctor) */
    a_routine rv_arr[7] = {};
    char vnm_arr[7][32];
    for (int vf = 0; vf < 7; ++vf) {
        std::snprintf(vnm_arr[vf], sizeof(vnm_arr[vf]), "vague_fn_%d", vf);
        rv_arr[vf].type = &rtp0; rtp0.variant.routine.return_type = &t_i32;
        rv_arr[vf].source_corresp.name = vnm_arr[vf]; rv_arr[vf].function_def_number = 1;
        def_sc.variant.routine.parameters = NULL;
        def_sc.local_static_variable_inits = NULL;
        def_sc.variables = NULL; def_sc.nonstatic_variables = NULL; def_sc.lifetime = NULL;
        if (vf == 0) rv_arr[vf].is_weak = TRUE;
        else if (vf == 1) rv_arr[vf].is_inline = TRUE;
        else if (vf == 2) { rv_arr[vf].is_template_function = TRUE; g_mock_body_clears_block = TRUE; }
        else if (vf == 3) { rv_arr[vf].is_template_function = TRUE; rv_arr[vf].is_specialized = TRUE; }
        else if (vf == 4) rv_arr[vf].compiler_generated = TRUE;
        else if (vf == 5) rv_arr[vf].source_corresp.name = "_ZZ1fvEN1LclEv";
        else {
            rv_arr[vf].source_corresp.is_class_member = TRUE;
            rv_arr[vf].special_kind = (a_special_function_kind)sfk_constructor;
            rv_arr[vf].source_corresp.name = "_ZN3ClsC2Ev"; /* C2E skips auto clone */
        }
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&rv_arr[vf], &fn));
        g_mock_body_clears_block = FALSE;
    }

    /* Non-void return zero mismatch (`t_pmf` frontend_ret with `i64_jt` coerced ret_type) & NULL zero fallback (`void_t`? No: struct without fields / NULL frontend_ret on struct) */
    {
        a_type t_empty_st = {}; t_empty_st.kind = tk_struct; t_empty_st.size = 4;
        a_routine r_zmis = {}, r_znul = {}, r_anon_def = {};
        rtp0.variant.routine.return_type = &t_coerced; /* ret_type coerced to i64_jt, while frontend_ret is t_coerced (struct) -> type mismatch! */
        r_zmis.type = &rtp0; r_zmis.source_corresp.name = "fn_zmis"; r_zmis.function_def_number = 1;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_zmis, &fn));

        /* Coroutine ramp lowering */
        a_routine r_coro = {};
        r_coro.type = &rtp0; r_coro.source_corresp.name = "fn_coro"; r_coro.function_def_number = 1; r_coro.is_coroutine = TRUE;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_coro, &fn));

        /* Anonymous namespace defined function + exceptions_enabled == FALSE + unpack_coerced_Arg returning NULL */
        exceptions_enabled = FALSE;
        g_mock_emit_unpacked_local = FALSE;
        vp_uncached.type = &t_coerced; vp_uncached.next = NULL;
        def_sc.variant.routine.parameters = &vp4; vp4.next = &vp_uncached;
        def_sc.lifetime = &life;
        a_param_type p_only_coe = {}; p_only_coe.type = &t_coerced;
        rts0.param_type_list = &p_only_coe;
        r_anon_def.type = &rtp0; r_anon_def.source_corresp.name = "_ZN12_GLOBAL__N_13barEv";
        r_anon_def.source_corresp.parent_scope = &sc_anon; r_anon_def.function_def_number = 1;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_anon_def, &fn));
        rts0.param_type_list = NULL;
        exceptions_enabled = TRUE;
        g_mock_emit_unpacked_local = TRUE;
        def_sc.variant.routine.parameters = NULL;
        def_sc.lifetime = NULL;

        /* Routine body error rollback (with allocated action_table!) */
        g_mock_body_err = GCC_GEN_BE_ERROR_INTERNAL;
        g_mock_body_alloc_action_table = TRUE;
        a_routine r_berr = {}; r_berr.type = &rtp0; r_berr.source_corresp.name = "fn_berr"; r_berr.function_def_number = 1;
        if (gcc_gen_be_lower_function_decl(&r_berr, &fn) != GCC_GEN_BE_ERROR_INTERNAL) return GCC_GEN_BE_ERROR_INTERNAL;
        g_mock_body_err = GCC_GEN_BE_SUCCESS;
        g_mock_body_alloc_action_table = FALSE;

        /* Auto-clone trigger from gcc_gen_be_lower_function_decl when il_header.primary_scope == NULL vs non-null, plus D2E/D0E/NULL name */
        rtp0.variant.routine.return_type = &t_void;
        il_header.primary_scope = NULL;
        a_routine r_auto_c1 = {}, r_auto_nullnm = {}, r_auto_d2 = {}, r_auto_d0 = {}, r_auto_d1 = {};
        r_auto_c1.type = &rtp0; r_auto_c1.source_corresp.name = "_ZN4AutoC1Ev"; r_auto_c1.function_def_number = 1;
        r_auto_c1.special_kind = (a_special_function_kind)sfk_constructor;
        g_mock_primary_mangle = "_ZN4AutoC1Ev"; g_mock_variant_mangle = "_ZN4AutoC2Ev";
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_auto_c1, &fn));

        r_auto_nullnm.type = &rtp0; r_auto_nullnm.source_corresp.name = NULL; r_auto_nullnm.function_def_number = 1;
        r_auto_nullnm.special_kind = (a_special_function_kind)sfk_constructor;
        g_mock_primary_mangle = "_ZN8AutoNullC1Ev"; g_mock_variant_mangle = "_ZN8AutoNullC2Ev";
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_auto_nullnm, &fn));

        r_auto_d2.type = &rtp0; r_auto_d2.source_corresp.name = "_ZN4AutoD2Ev"; r_auto_d2.function_def_number = 1;
        r_auto_d2.special_kind = (a_special_function_kind)sfk_destructor;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_auto_d2, &fn));

        r_auto_d0.type = &rtp0; r_auto_d0.source_corresp.name = "_ZN4AutoD0Ev"; r_auto_d0.function_def_number = 1;
        r_auto_d0.special_kind = (a_special_function_kind)sfk_destructor;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_auto_d0, &fn));

        r_auto_d1.type = &rtp0; r_auto_d1.source_corresp.name = "_ZN4AutoD1Ev"; r_auto_d1.function_def_number = 1;
        r_auto_d1.special_kind = (a_special_function_kind)sfk_destructor;
        g_mock_primary_mangle = "_ZN4AutoD1Ev"; g_mock_variant_mangle = "_ZN4AutoD2Ev";
        il_header.primary_scope = &sc_ns; /* non-null primary_scope skips auto-clone */
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_auto_d1, &fn));
        il_header.primary_scope = NULL;
        g_mock_primary_mangle = NULL; g_mock_variant_mangle = NULL;
    }

    /* Exhaustive gcc_gen_be_emit_ctor_dtor_clones coverage */
    gcc_jit_type *void_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_type *vptr_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_param *this_p = gcc_jit_context_new_param(ctx, NULL, vptr_t, "this_p");
    gcc_jit_function *fn_1p = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_t, "prim_1p", 1, &this_p, 0);
    gcc_jit_function *fn_0p = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_t, "prim_0p", 0, NULL, 0);

    /* sfk_none early exit & NULL ctx */
    a_routine r_norm = {}; r_norm.special_kind = (a_special_function_kind)sfk_none;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&r_norm, fn_1p, NULL, NULL));
    r_norm.special_kind = (a_special_function_kind)sfk_constructor;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_emit_ctor_dtor_clones(&r_norm, fn_1p, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    /* Overflow retry (min_size = 1024) & >64KiB overflow error */
    g_mock_mangle_min_size = 1024;
    g_mock_primary_mangle = "_ZN5RetryC2Ev"; g_mock_variant_mangle = "_ZN5RetryC1Ev";
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&r_norm, fn_1p, NULL, NULL));
    g_mock_mangle_min_size = 100000;
    if (gcc_gen_be_emit_ctor_dtor_clones(&r_norm, fn_1p, NULL, NULL) != GCC_GEN_BE_ERROR_MANGLING_BUFFER_OVERFLOW) return GCC_GEN_BE_ERROR_INTERNAL;
    g_mock_mangle_min_size = 0;

    /* D2 -> D1 + D0, skip_base_clone via identical name, anonymous clone (both skipped & non-skipped base_clone), 0-param primary with ptp list & 0-param vbase */
    g_mock_primary_mangle = "_ZN5SameCD2Ev"; g_mock_variant_mangle = "_ZN5SameCD2Ev";
    r_norm.special_kind = (a_special_function_kind)sfk_destructor;
    r_norm.source_corresp.parent_scope = &sc_anon;
    gcc_jit_function *bc = NULL, *dc = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&r_norm, fn_1p, &bc, &dc));
    g_mock_primary_mangle = "_ZN8AnonBaseC1Ev"; g_mock_variant_mangle = "_ZN8AnonBaseC2Ev";
    r_norm.special_kind = (a_special_function_kind)sfk_constructor;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&r_norm, fn_1p, &bc, &dc));

    /* Sibling lookup in parent_scope & il_header.primary_scope (including exclude_rout, _Z encoding, NULL name, and existing cache hits) */
    a_routine sib_self = {}, sib_unm = {}, sib_noname = {}, sib_match = {}, sib_d0 = {};
    a_scope sib_parent = {}, sib_prim = {};
    sib_noname.source_corresp.name = NULL; sib_noname.next = &sib_unm;
    sib_unm.source_corresp.name = "other"; sib_unm.source_corresp.unmangled_name_or_mangled_encoding = "not_z"; sib_unm.next = &sib_match;
    sib_match.source_corresp.name = "raw"; sib_match.source_corresp.unmangled_name_or_mangled_encoding = "_ZN3SibC2Ev"; sib_match.memory_region = 99; sib_match.next = NULL;
    sib_d0.source_corresp.name = "_ZN3SibD0Ev"; sib_d0.memory_region = 99; sib_d0.next = NULL;

    sib_self.special_kind = (a_special_function_kind)sfk_constructor;
    sib_self.source_corresp.parent_scope = &sib_parent;
    sib_self.next = &sib_noname;
    sib_parent.routines = &sib_self;
    g_mock_primary_mangle = "_ZN3SibC1Ev"; g_mock_variant_mangle = "_ZN3SibC2Ev";
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&sib_self, fn_1p, &bc, &dc));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&sib_self, fn_1p, NULL, NULL));

    /* Sibling in parent_scope AND in il_header.primary_scope for D0 */
    sib_parent.routines = &sib_d0;
    sib_prim.routines = &sib_d0;
    il_header.primary_scope = &sib_prim;
    sib_self.special_kind = (a_special_function_kind)sfk_destructor;
    g_mock_primary_mangle = "_ZN3SibD1Ev"; g_mock_variant_mangle = "_ZN3SibD0Ev";
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&sib_self, fn_1p, &bc, &dc));
    sib_parent.routines = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&sib_self, fn_1p, &bc, &dc));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&sib_self, fn_1p, NULL, NULL));
    il_header.primary_scope = NULL;

    /* Existing cached symbol in CACHE_STRING (both with & without CACHE_FUNC entry, and with out_clone != NULL & == NULL) */
    g_mock_primary_mangle = NULL; g_mock_variant_mangle = NULL;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"_ZN4TestD2Ev", fn_1p));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"_ZN4TestD0Ev", fn_1p));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&sib_self, fn_1p, &bc, &dc));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&sib_self, fn_1p, NULL, NULL));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"_ZN4TestD2Ev", &r_dup_imp1));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"_ZN4TestD0Ev", &r_dup_imp1));
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&sib_self, fn_1p, &bc, &dc));

    /* parent_class variations: is_class_member && parent_scope == NULL, tk_union, tk_class without vbases, align > 16 && size == 0, non-routine rout.type, NULL extra_info, ptp->type == NULL */
    {
        GCC_GEN_BE_CHECK(cache_clear_all());
        a_routine r_pv = {};
        r_pv.special_kind = (a_special_function_kind)sfk_destructor;
        r_pv.source_corresp.is_class_member = TRUE; r_pv.source_corresp.parent_scope = NULL;
        r_pv.type = &t_i32; /* non-routine rout->type */
        g_mock_primary_mangle = "_ZN3Pv1D1Ev"; g_mock_variant_mangle = "_ZN3Pv1D2Ev";
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&r_pv, fn_1p, NULL, NULL));

        GCC_GEN_BE_CHECK(cache_clear_all());
        a_type u_cls = {}; u_cls.kind = tk_union; u_cls.alignment = 32; u_cls.size = 0;
        a_scope u_sc = {}; u_sc.kind = sck_class_struct_union; u_sc.variant.assoc_type = &u_cls;
        a_type rtp_noex = {}; rtp_noex.kind = tk_routine; rtp_noex.variant.routine.extra_info = NULL;
        r_pv.source_corresp.parent_scope = &u_sc; r_pv.type = &rtp_noex;
        g_mock_primary_mangle = NULL; g_mock_variant_mangle = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&r_pv, fn_1p, NULL, NULL));

        GCC_GEN_BE_CHECK(cache_clear_all());
        a_type nv_cls = {}; nv_cls.kind = tk_class; nv_cls.variant.class_struct_union.any_virtual_base_classes = FALSE;
        u_sc.variant.assoc_type = &nv_cls;
        a_param_type p_nt = {}; p_nt.type = NULL;
        a_routine_type_supplement rts_nt = {}; rts_nt.param_type_list = &p_nt;
        rtp_noex.variant.routine.extra_info = &rts_nt;
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&r_pv, fn_0p, &bc, &dc));
    }

    /* primary_param_cnt == 0 with ptp list (2 params) AND 0 params with vbase (`num_params == 0 && clone_needs_vtt_parm`) */
    GCC_GEN_BE_CHECK(cache_clear_all());
    a_type vbase_cls = {}; vbase_cls.kind = tk_struct; vbase_cls.variant.class_struct_union.any_virtual_base_classes = TRUE;
    a_scope vbase_sc = {}; vbase_sc.kind = sck_class_struct_union; vbase_sc.variant.assoc_type = &vbase_cls;
    a_type t_ptr = {}; t_ptr.kind = tk_pointer; t_ptr.size = 8; t_ptr.variant.pointer.type = &t_i32;
    a_param_type cp0 = {}, cp1 = {};
    cp0.type = &t_ptr; cp0.next = &cp1;
    cp1.type = NULL; cp1.next = NULL;
    a_routine_type_supplement crts = {}; crts.param_type_list = &cp0;
    a_type crtp = {}; crtp.kind = tk_routine; crtp.variant.routine.extra_info = &crts;
    a_routine r_0p = {};
    r_0p.type = &crtp; r_0p.special_kind = (a_special_function_kind)sfk_destructor;
    r_0p.source_corresp.is_class_member = TRUE; r_0p.source_corresp.parent_scope = &vbase_sc;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&r_0p, fn_0p, &bc, &dc));

    GCC_GEN_BE_CHECK(cache_clear_all());
    crts.param_type_list = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&r_0p, fn_0p, &bc, &dc));

    /* Bad param type error in c2_params and d0_this */
    GCC_GEN_BE_CHECK(cache_clear_all());
    cp0.type = &t_bad; cp0.next = NULL; crts.param_type_list = &cp0;
    if (gcc_gen_be_emit_ctor_dtor_clones(&r_0p, fn_0p, &bc, &dc) != GCC_GEN_BE_ERROR_BITINT_UNSUPPORTED) return GCC_GEN_BE_ERROR_INTERNAL;
    g_mock_primary_mangle = "_ZN4TestD2Ev"; g_mock_variant_mangle = "_ZN4TestD2Ev"; /* skip base clone, fail on D0 ptp->type! */
    if (gcc_gen_be_emit_ctor_dtor_clones(&r_0p, fn_0p, &bc, &dc) != GCC_GEN_BE_ERROR_BITINT_UNSUPPORTED) return GCC_GEN_BE_ERROR_INTERNAL;
    g_mock_primary_mangle = NULL; g_mock_variant_mangle = NULL;

    /* OOM sweeps across function_decl, emit_ctor_dtor_clones, and apply_comdat_linkage */
    vp4.next = &vp5;
    for (int cfg = 0; cfg < 10; ++cfg) {
        for (int b = 0; b < 300; ++b) {
            GCC_GEN_BE_CHECK(cache_clear_all());
            lsv_v1.init_kind = (an_init_kind)initk_function_local;
            def_sc.variant.routine.parameters = (cfg == 0) ? &vp1 : NULL;
            def_sc.local_static_variable_inits = (cfg == 9) ? &lsv1 : NULL;
            def_sc.variables = (cfg == 9) ? &sv_ok : NULL;
            def_sc.nonstatic_variables = (cfg == 9) ? &nsv_ok : NULL;
            def_sc.lifetime = (cfg == 9) ? &life : NULL;
            a_routine ro = {};
            char rnm[32]; std::snprintf(rnm, sizeof(rnm), "oom_fn_%d_%d", cfg, b);
            if (cfg == 0) {
                g_mock_sret = TRUE;
                ro.type = &rtp_full; ro.source_corresp.name = rnm;
                ro.function_def_number = 1; ro.memory_region = 1;
                ro.is_inline = TRUE;
            } else if (cfg == 1) {
                gcc_gen_be_singleton_state.canonical_mangle_func = &mock_canon_mangle_fn;
                std::snprintf(g_canon_fn_buf, sizeof(g_canon_fn_buf), "_ZN3oom2fnE");
                ro.type = &rtp0; rtp0.variant.routine.return_type = &t_coerced;
                ro.source_corresp.name = rnm; ro.is_template_function = TRUE;
                ro.function_def_number = 1;
            } else if (cfg == 2) {
                ro.type = &rtp0; ro.source_corresp.name = rnm;
                ro.function_def_number = 1; ro.is_coroutine = TRUE;
            } else if (cfg == 3) {
                a_routine pre = {}; pre.source_corresp.name = rnm;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&pre, &fn));
                ro.source_corresp.name = rnm;
                ro.storage_class = (a_storage_class)sc_static;
            } else if (cfg == 4) {
                a_routine pre = {}; pre.source_corresp.name = rnm;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&pre, &fn));
                ro.source_corresp.name = rnm;
            } else if (cfg == 5) {
                ro.type = &rtp0; rtp0.variant.routine.return_type = &t_void;
                ro.source_corresp.name = "_ZN7OomAutoC1Ev"; ro.function_def_number = 1;
                ro.special_kind = (a_special_function_kind)sfk_constructor;
            } else if (cfg == 6) {
                cp0.type = &t_ptr; crts.param_type_list = &cp0;
                r_0p.type = &crtp;
                il_header.primary_scope = &sib_prim; sib_prim.routines = NULL;
                g_gcc_be_oom_countdown = b;
                gcc_gen_be_error_t ce = gcc_gen_be_emit_ctor_dtor_clones(&r_0p, fn_0p, &bc, &dc);
                g_gcc_be_oom_countdown = -1;
                il_header.primary_scope = NULL;
                if (ce == GCC_GEN_BE_SUCCESS) break;
                continue;
            } else if (cfg == 7) {
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"_ZN4TestD2Ev", fn_1p));
                GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)"_ZN4TestD0Ev", fn_1p));
                g_gcc_be_oom_countdown = b;
                gcc_gen_be_error_t ce = gcc_gen_be_emit_ctor_dtor_clones(&r_0p, fn_1p, &bc, &dc);
                g_gcc_be_oom_countdown = -1;
                if (ce == GCC_GEN_BE_SUCCESS) break;
                continue;
            } else if (cfg == 8) {
                ro.source_corresp.name = "oom_mangle_fn";
                ro.is_template_function = TRUE;
            } else {
                ro.type = &rtp0; rtp0.variant.routine.return_type = &t_void;
                ro.source_corresp.name = rnm; ro.function_def_number = 1;
            }
            g_gcc_be_oom_countdown = b;
            gcc_gen_be_error_t e = gcc_gen_be_lower_function_decl(&ro, &fn);
            g_gcc_be_oom_countdown = -1;
            g_mock_sret = FALSE;
            gcc_gen_be_singleton_state.canonical_mangle_func = NULL;
            if (e == GCC_GEN_BE_SUCCESS) break;
        }
    }

    /* Sibling lowering OOM in emit_ctor_dtor_clones (separate base sibling and D0 sibling sweeps) */
    for (int mode = 0; mode < 2; ++mode) {
        for (int b = 0; b < 45; ++b) {
            GCC_GEN_BE_CHECK(cache_clear_all());
            sib_match.source_corresp.unmangled_name_or_mangled_encoding = "_ZN4TestD2Ev";
            sib_d0.source_corresp.name = "_ZN4TestD0Ev";
            sib_parent.routines = (mode == 0) ? &sib_match : NULL;
            sib_prim.routines = (mode == 1) ? &sib_d0 : NULL;
            il_header.primary_scope = &sib_prim;
            sib_self.special_kind = (a_special_function_kind)sfk_destructor;
            sib_self.source_corresp.parent_scope = &sib_parent;
            g_gcc_be_oom_countdown = b;
            gcc_gen_be_error_t e = gcc_gen_be_emit_ctor_dtor_clones(&sib_self, fn_1p, &bc, &dc);
            g_gcc_be_oom_countdown = -1;
            il_header.primary_scope = NULL;
            if (e == GCC_GEN_BE_SUCCESS) break;
        }
    }

    /* COMDAT OOM & invalid argument edge cases */
    char sec_buf[256];
    const char *link_attr = NULL;
    if (gcc_gen_be_format_comdat_section_name("", FALSE, TRUE, 0, sec_buf, sizeof(sec_buf), &link_attr) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_format_comdat_section_name("_ZTV1A", FALSE, TRUE, 0, NULL, sizeof(sec_buf), &link_attr) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_format_comdat_section_name("_ZTV1A", FALSE, TRUE, 0, sec_buf, 0, &link_attr) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_format_comdat_section_name("_ZTV1A", FALSE, TRUE, 0, sec_buf, sizeof(sec_buf), NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_format_comdat_section_name("_ZTV1A", FALSE, TRUE, -1, sec_buf, sizeof(sec_buf), &link_attr) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_lvalue *cg = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, vptr_t, "_ZTV4Cmdt");
    if (gcc_gen_be_apply_comdat_linkage(cg, "", TRUE) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    for (int b = 0; b < 6; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_apply_comdat_linkage(cg, "_ZTV4Cmdt", TRUE);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    g_mock_def_scope = NULL;
    mem_region_table = NULL;
    f_il_output = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Executes all declaration lowering validation tests.
 * @details Runs basic validation, COMDAT section tests, global/ctor tests, local tests, and function/clone tests.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t run_decl_unit_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    *out_ok = 0;

    gcc_jit_lvalue *lval = NULL;
    gcc_jit_function *func = NULL;
    if (gcc_gen_be_lower_global_variable_decl(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_local_variable_decl(NULL, NULL, &lval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_function_decl(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(NULL, &lval));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(NULL, &func));
    if (gcc_gen_be_emit_ctor_dtor_clones(NULL, NULL, &func, &func) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    a_routine dummy_r = {};
    if (gcc_gen_be_emit_ctor_dtor_clones(&dummy_r, NULL, &func, &func) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

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
    GCC_GEN_BE_CHECK(cache_clear_all());
    gcc_jit_function *d2_clone = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&ctor_rout, primary_fn, &d2_clone, &d0_clone));
    if (!d2_clone || !d0_clone) {
        gcc_jit_context_release(ctx);
        return GCC_GEN_BE_ERROR_INTERNAL;
    }

    vbase_class.size = 32;
    vbase_class.alignment = 8;
    GCC_GEN_BE_CHECK(cache_clear_all());
    gcc_jit_function *d2_sized = NULL;
    gcc_jit_function *d0_sized = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&ctor_rout, primary_fn, &d2_sized, &d0_sized));

    vbase_class.size = 64;
    vbase_class.alignment = 32;
    GCC_GEN_BE_CHECK(cache_clear_all());
    gcc_jit_function *d2_aligned = NULL;
    gcc_jit_function *d0_aligned = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_ctor_dtor_clones(&ctor_rout, primary_fn, &d2_aligned, &d0_aligned));

    char sec_buf[256];
    const char *link_attr = NULL;
    if (gcc_gen_be_format_comdat_section_name(NULL, FALSE, TRUE, 0, sec_buf, sizeof(sec_buf), &link_attr) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_format_comdat_section_name("_ZTV4Test", FALSE, TRUE, 3, sec_buf, sizeof(sec_buf), &link_attr) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_format_comdat_section_name("_ZTV4Test", FALSE, TRUE, 0, sec_buf, 4, &link_attr) != GCC_GEN_BE_ERROR_MANGLING_BUFFER_OVERFLOW) return GCC_GEN_BE_ERROR_INTERNAL;

    for (int fmt = 0; fmt <= 2; ++fmt) {
        GCC_GEN_BE_CHECK(gcc_gen_be_format_comdat_section_name("_ZN4Test1fEv", TRUE, FALSE, fmt, sec_buf, sizeof(sec_buf), &link_attr));
        GCC_GEN_BE_CHECK(gcc_gen_be_format_comdat_section_name("_ZTV4Test", FALSE, TRUE, fmt, sec_buf, sizeof(sec_buf), &link_attr));
        GCC_GEN_BE_CHECK(gcc_gen_be_format_comdat_section_name("_ZN4Test3varE", FALSE, FALSE, fmt, sec_buf, sizeof(sec_buf), &link_attr));
    }

    gcc_jit_lvalue *comdat_glob = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, void_ptr_t, "_ZTV4Test");
    if (gcc_gen_be_apply_comdat_linkage(NULL, "_ZTV4Test", TRUE) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_apply_comdat_linkage(comdat_glob, NULL, TRUE) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_apply_comdat_linkage(comdat_glob, "_ZTV4Test", TRUE));

    GCC_GEN_BE_CHECK(test_global_vars_and_ctors(ctx));
    GCC_GEN_BE_CHECK(test_local_vars(ctx));
    GCC_GEN_BE_CHECK(test_functions_and_clones(ctx));

    /* Stage-2 declaration coverage: Darwin TLS + OOM, _ZTH weak stubs + OOM,
       aarch64 x8 sret thunks + OOM, and __attribute__((constructor/destructor)) + OOM */
    {
        gcc_jit_context *s2_ctx = gcc_jit_context_acquire();
        GCC_GEN_BE_CHECK(gcc_gen_be_set_context(s2_ctx));

        a_type t_i32 = {}, t_zero = {}, t_void = {}, t_ptr = {};
        t_i32.kind = tk_integer; t_i32.size = 4; t_i32.variant.integer.int_kind = ik_int;
        t_zero.kind = tk_array; t_zero.size = 0; t_zero.variant.array.element_type = &t_i32;
        t_void.kind = tk_void;
        t_ptr.kind = tk_pointer; t_ptr.size = 8; t_ptr.variant.pointer.type = &t_i32;

        /* 1) Darwin TLS: imported, internal, exported, vague-exported, vague-internal, zero-size, cached helpers, and full OOM sweep */
        GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
        for (int b = 0; b < 160; ++b) {
            GCC_GEN_BE_CHECK(gcc_gen_be_init());
            gcc_jit_context *tctx = gcc_jit_context_acquire();
            GCC_GEN_BE_CHECK(gcc_gen_be_set_context(tctx));
            a_variable tv_imp = {}, tv_def = {}, tv_int = {};
            tv_imp.source_corresp.name = "dtls_imp"; tv_imp.type = &t_i32; tv_imp.is_thread_local = TRUE; tv_imp.storage_class = sc_extern;
            tv_def.source_corresp.name = "dtls_def"; tv_def.type = &t_zero; tv_def.is_thread_local = TRUE; tv_def.init_kind = initk_zero;
            tv_int.source_corresp.name = "dtls_int"; tv_int.type = &t_i32; tv_int.is_thread_local = TRUE; tv_int.storage_class = sc_static; tv_int.init_kind = initk_zero;
            gcc_jit_lvalue *olv = NULL;
            g_gcc_be_oom_countdown = b;
            gcc_gen_be_error_t e = gcc_gen_be_lower_global_variable_decl(&tv_imp, &olv);
            if (e == GCC_GEN_BE_SUCCESS) e = gcc_gen_be_lower_global_variable_decl(&tv_def, &olv);
            if (e == GCC_GEN_BE_SUCCESS) e = gcc_gen_be_lower_global_variable_decl(&tv_int, &olv);
            g_gcc_be_oom_countdown = -1;
            GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
            if (e == GCC_GEN_BE_SUCCESS) break;
        }
        for (int b = 0; b < 120; ++b) {
            GCC_GEN_BE_CHECK(gcc_gen_be_init());
            gcc_jit_context *tctx = gcc_jit_context_acquire();
            GCC_GEN_BE_CHECK(gcc_gen_be_set_context(tctx));
            a_variable tv_vag = {}, tv_vag_int = {};
            tv_vag.source_corresp.name = "_ZZ3foovE8dtls_vag"; tv_vag.type = &t_i32; tv_vag.is_thread_local = TRUE; tv_vag.is_weak = TRUE; tv_vag.init_kind = initk_zero;
            tv_vag_int.source_corresp.name = "_ZZ3foovE12dtls_vag_int"; tv_vag_int.type = &t_i32; tv_vag_int.is_thread_local = TRUE; tv_vag_int.is_weak = TRUE; tv_vag_int.storage_class = sc_static; tv_vag_int.init_kind = initk_zero;
            gcc_jit_lvalue *olv = NULL;
            g_gcc_be_oom_countdown = b;
            gcc_gen_be_error_t e = gcc_gen_be_lower_global_variable_decl(&tv_vag, &olv);
            if (e == GCC_GEN_BE_SUCCESS) e = gcc_gen_be_lower_global_variable_decl(&tv_vag_int, &olv);
            g_gcc_be_oom_countdown = -1;
            GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
            if (e == GCC_GEN_BE_SUCCESS) break;
        }
        {
            GCC_GEN_BE_CHECK(gcc_gen_be_init());
            gcc_jit_context *tv_ctx = gcc_jit_context_acquire();
            GCC_GEN_BE_CHECK(gcc_gen_be_set_context(tv_ctx));
            a_variable tv_vag_int2 = {};
            tv_vag_int2.source_corresp.name = "_ZZ3foovE13dtls_vag_int2"; tv_vag_int2.type = &t_i32; tv_vag_int2.is_thread_local = TRUE; tv_vag_int2.is_weak = TRUE; tv_vag_int2.storage_class = sc_static; tv_vag_int2.init_kind = initk_zero;
            gcc_jit_lvalue *olv2 = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_global_variable_decl(&tv_vag_int2, &olv2));
            GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
        }

        /* 2) __c99_* helper skip & _ZTH weak stub matching / non-matching / null primary_scope TLS variables + OOM */
        {
            a_variable ztv_non_tls = {}, ztv_ext = {}, ztv_unnamed = {}, ztv_short = {}, ztv_nomatch = {}, ztv_match = {}, ztv_match_st = {};
            ztv_non_tls.is_thread_local = FALSE; ztv_non_tls.next = &ztv_ext;
            ztv_ext.is_thread_local = TRUE; ztv_ext.storage_class = sc_extern; ztv_ext.source_corresp.name = "x"; ztv_ext.next = &ztv_unnamed;
            ztv_unnamed.is_thread_local = TRUE; ztv_unnamed.source_corresp.name = NULL; ztv_unnamed.next = &ztv_short;
            ztv_short.is_thread_local = TRUE; ztv_short.source_corresp.name = "verylongtlsname"; ztv_short.next = &ztv_nomatch;
            ztv_nomatch.is_thread_local = TRUE; ztv_nomatch.source_corresp.name = "other"; ztv_nomatch.next = &ztv_match;
            ztv_match.is_thread_local = TRUE; ztv_match.source_corresp.name = "mytls"; ztv_match.next = &ztv_match_st;
            ztv_match_st.is_thread_local = TRUE; ztv_match_st.source_corresp.name = "mytls_s";

            a_scope zpri = {}, zpri_nomatch = {};
            zpri.kind = sck_file; zpri.variables = &ztv_non_tls;
            a_variable ztv_only_other = {}; ztv_only_other.is_thread_local = TRUE; ztv_only_other.source_corresp.name = "other";
            zpri_nomatch.kind = sck_file; zpri_nomatch.variables = &ztv_only_other;
            a_type zr_ty = {}; zr_ty.kind = tk_routine; zr_ty.variant.routine.return_type = &t_void;

            GCC_GEN_BE_CHECK(gcc_gen_be_init());
            gcc_jit_context *c99_ctx = gcc_jit_context_acquire();
            GCC_GEN_BE_CHECK(gcc_gen_be_set_context(c99_ctx));
            a_routine r_c99 = {}, zr_null_pri = {}, zr_miss = {};
            r_c99.source_corresp.name = "__c99_cabsl"; r_c99.type = &zr_ty; r_c99.function_def_number = NULL_function_def_number;
            zr_null_pri.source_corresp.name = "_ZTH5mytls"; zr_null_pri.type = &zr_ty; zr_null_pri.function_def_number = NULL_function_def_number;
            zr_miss.source_corresp.name = "_ZTH9miss_tls1"; zr_miss.type = &zr_ty; zr_miss.function_def_number = NULL_function_def_number;
            gcc_jit_function *zfn0 = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&r_c99, &zfn0));
            il_header.primary_scope = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&zr_null_pri, &zfn0));
            il_header.primary_scope = &zpri_nomatch;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&zr_miss, &zfn0));
            il_header.primary_scope = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());

            for (int b = 0; b < 80; ++b) {
                GCC_GEN_BE_CHECK(gcc_gen_be_init());
                gcc_jit_context *zctx = gcc_jit_context_acquire();
                GCC_GEN_BE_CHECK(gcc_gen_be_set_context(zctx));
                il_header.primary_scope = &zpri;
                a_routine zr = {}, zr_int = {};
                zr.source_corresp.name = "_ZTH5mytls"; zr.type = &zr_ty; zr.function_def_number = NULL_function_def_number;
                zr_int.source_corresp.name = "_ZTH7mytls_s"; zr_int.type = &zr_ty; zr_int.function_def_number = NULL_function_def_number; zr_int.storage_class = sc_static;
                gcc_jit_function *zfn = NULL;
                g_gcc_be_oom_countdown = b;
                gcc_gen_be_error_t ze = gcc_gen_be_lower_function_decl(&zr, &zfn);
                if (ze == GCC_GEN_BE_SUCCESS) ze = gcc_gen_be_lower_function_decl(&zr_int, &zfn);
                g_gcc_be_oom_countdown = -1;
                il_header.primary_scope = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
                if (ze == GCC_GEN_BE_SUCCESS) break;
            }
        }

        /* 3) aarch64 x8 sret thunks (value_returned_as_parameter = TRUE: 0-param, imported 1-param & 2-param, defined static/exported/vague, non-arm64 ABI, and OOM) */
        {
            auto saved_fn_attr_x8 = p_gcc_jit_function_add_attribute;
            p_gcc_jit_function_add_attribute = &mock_fn_add_attr;
            a_param_type xp0 = {}, xp1 = {};
            xp0.type = &t_ptr; xp0.next = &xp1;
            xp1.type = &t_i32; xp1.next = NULL;
            a_routine_type_supplement xsupp_2 = {}, xsupp_1 = {}, xsupp_0 = {};
            xsupp_2.param_type_list = &xp0; xsupp_2.value_returned_as_parameter = TRUE;
            a_param_type xp_single = {}; xp_single.type = &t_ptr;
            xsupp_1.param_type_list = &xp_single; xsupp_1.value_returned_as_parameter = TRUE;
            xsupp_0.param_type_list = NULL; xsupp_0.value_returned_as_parameter = TRUE;
            a_type xr_ty2 = {}, xr_ty1 = {}, xr_ty0 = {};
            xr_ty2.kind = tk_routine; xr_ty2.variant.routine.return_type = &t_void; xr_ty2.variant.routine.extra_info = &xsupp_2;
            xr_ty1.kind = tk_routine; xr_ty1.variant.routine.return_type = &t_void; xr_ty1.variant.routine.extra_info = &xsupp_1;
            xr_ty0.kind = tk_routine; xr_ty0.variant.routine.return_type = &t_void; xr_ty0.variant.routine.extra_info = &xsupp_0;

            /* SYSV_AMD64, AAPCS64, and DARWIN_ARM64 target ABIs on value_returned_as_parameter = TRUE */
            GCC_GEN_BE_CHECK(gcc_gen_be_init());
            gcc_jit_context *x86_ctx = gcc_jit_context_acquire();
            GCC_GEN_BE_CHECK(gcc_gen_be_set_context(x86_ctx));
            g_mock_target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
            a_routine xr_x86 = {}, xr_0p = {};
            xr_x86.source_corresp.name = "x8_on_x86"; xr_x86.type = &xr_ty1; xr_x86.function_def_number = NULL_function_def_number;
            xr_0p.source_corresp.name = "x8_zero_p"; xr_0p.type = &xr_ty0; xr_0p.function_def_number = NULL_function_def_number;
            gcc_jit_function *xfn = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&xr_x86, &xfn));
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&xr_0p, &xfn));
            g_mock_target_abi = GCC_GEN_BE_TARGET_ABI_AAPCS64;
            a_routine xr_aapcs = {}; xr_aapcs.source_corresp.name = "x8_on_aapcs"; xr_aapcs.type = &xr_ty1; xr_aapcs.function_def_number = NULL_function_def_number;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&xr_aapcs, &xfn));
            GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());

            g_mock_target_abi = GCC_GEN_BE_TARGET_ABI_DARWIN_ARM64;
            for (int b = 0; b < 120; ++b) {
                GCC_GEN_BE_CHECK(gcc_gen_be_init());
                gcc_jit_context *xctx = gcc_jit_context_acquire();
                GCC_GEN_BE_CHECK(gcc_gen_be_set_context(xctx));
                a_routine xr_imp2_only = {};
                xr_imp2_only.source_corresp.name = "x8_imp2_only"; xr_imp2_only.type = &xr_ty2; xr_imp2_only.function_def_number = NULL_function_def_number;
                g_gcc_be_oom_countdown = b;
                gcc_gen_be_error_t xe = gcc_gen_be_lower_function_decl(&xr_imp2_only, &xfn);
                g_gcc_be_oom_countdown = -1;
                GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
                if (xe == GCC_GEN_BE_SUCCESS) break;
            }
            for (int b = 0; b < 120; ++b) {
                GCC_GEN_BE_CHECK(gcc_gen_be_init());
                gcc_jit_context *xctx = gcc_jit_context_acquire();
                GCC_GEN_BE_CHECK(gcc_gen_be_set_context(xctx));
                p_gcc_jit_function_add_attribute = NULL;
                a_routine xr_vag_only = {};
                xr_vag_only.source_corresp.name = "x8_vag_only"; xr_vag_only.type = &xr_ty2; xr_vag_only.function_def_number = 1; xr_vag_only.is_inline = TRUE;
                g_gcc_be_oom_countdown = b;
                gcc_gen_be_error_t xe = gcc_gen_be_lower_function_decl(&xr_vag_only, &xfn);
                g_gcc_be_oom_countdown = -1;
                GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
                if (xe == GCC_GEN_BE_SUCCESS) break;
            }
            for (int b = 0; b < 220; ++b) {
                GCC_GEN_BE_CHECK(gcc_gen_be_init());
                gcc_jit_context *xctx = gcc_jit_context_acquire();
                GCC_GEN_BE_CHECK(gcc_gen_be_set_context(xctx));
                a_routine xr_imp1 = {}, xr_imp2 = {}, xr_def_int = {}, xr_def_vag = {};
                xr_imp1.source_corresp.name = "x8_imp1"; xr_imp1.type = &xr_ty1; xr_imp1.function_def_number = NULL_function_def_number;
                xr_imp2.source_corresp.name = "x8_imp2"; xr_imp2.type = &xr_ty2; xr_imp2.function_def_number = NULL_function_def_number;
                xr_def_int.source_corresp.name = "x8_def_int"; xr_def_int.type = &xr_ty2; xr_def_int.function_def_number = 1; xr_def_int.storage_class = sc_static;
                xr_def_vag.source_corresp.name = "x8_def_inl"; xr_def_vag.type = &xr_ty2; xr_def_vag.function_def_number = 1; xr_def_vag.is_inline = TRUE;
                g_gcc_be_oom_countdown = b;
                gcc_gen_be_error_t xe = gcc_gen_be_lower_function_decl(&xr_imp1, &xfn);
                if (xe == GCC_GEN_BE_SUCCESS) xe = gcc_gen_be_lower_function_decl(&xr_imp2, &xfn);
                if (xe == GCC_GEN_BE_SUCCESS) xe = gcc_gen_be_lower_function_decl(&xr_def_int, &xfn);
                if (xe == GCC_GEN_BE_SUCCESS) xe = gcc_gen_be_lower_function_decl(&xr_def_vag, &xfn);
                g_gcc_be_oom_countdown = -1;
                GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
                if (xe == GCC_GEN_BE_SUCCESS) break;
            }
            {
                GCC_GEN_BE_CHECK(gcc_gen_be_init());
                gcc_jit_context *xctx2 = gcc_jit_context_acquire();
                GCC_GEN_BE_CHECK(gcc_gen_be_set_context(xctx2));
                p_gcc_jit_function_add_attribute = NULL; /* also cover 1495:11 False! */
                a_routine xr_noattr = {}; xr_noattr.source_corresp.name = "x8_noattr"; xr_noattr.type = &xr_ty1; xr_noattr.function_def_number = NULL_function_def_number;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&xr_noattr, &xfn));
                p_gcc_jit_function_add_attribute = &mock_fn_add_attr;
                a_routine xr_def_weak = {}, xr_def_cg = {}, xr_def_zz = {}, xr_def_sp = {}, xr_def_mnone = {}, xr_def_plain = {}, xr_def_cls = {}, xr_def_tpl = {};
                xr_def_weak.source_corresp.name = "x8_def_wk"; xr_def_weak.type = &xr_ty1; xr_def_weak.function_def_number = 1; xr_def_weak.is_weak = TRUE;
                xr_def_cg.source_corresp.name = "x8_def_cg"; xr_def_cg.type = &xr_ty1; xr_def_cg.function_def_number = 1; xr_def_cg.compiler_generated = TRUE;
                xr_def_zz.source_corresp.name = "_ZZ2fnv"; xr_def_zz.type = &xr_ty1; xr_def_zz.function_def_number = 1;
                xr_def_sp.source_corresp.name = "x8_def_sp"; xr_def_sp.type = &xr_ty1; xr_def_sp.function_def_number = 1; xr_def_sp.is_template_function = TRUE; xr_def_sp.is_specialized = TRUE;
                xr_def_mnone.source_corresp.name = "x8_def_mn"; xr_def_mnone.type = &xr_ty1; xr_def_mnone.function_def_number = 1; xr_def_mnone.source_corresp.is_class_member = TRUE; xr_def_mnone.special_kind = static_cast<a_special_function_kind>(sfk_none);
                xr_def_plain.source_corresp.name = "x8_def_plain"; xr_def_plain.type = &xr_ty1; xr_def_plain.function_def_number = 1;
                xr_def_cls.source_corresp.name = "x8_def_cls"; xr_def_cls.type = &xr_ty1; xr_def_cls.function_def_number = 1; xr_def_cls.source_corresp.is_class_member = TRUE; xr_def_cls.special_kind = static_cast<a_special_function_kind>(sfk_constructor);
                xr_def_tpl.source_corresp.name = "x8_def_tpl"; xr_def_tpl.type = &xr_ty1; xr_def_tpl.function_def_number = 1; xr_def_tpl.is_template_function = TRUE;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&xr_def_weak, &xfn));
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&xr_def_cg, &xfn));
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&xr_def_zz, &xfn));
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&xr_def_sp, &xfn));
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&xr_def_mnone, &xfn));
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&xr_def_plain, &xfn));
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&xr_def_cls, &xfn));
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(&xr_def_tpl, &xfn));
                GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
            }
            p_gcc_jit_function_add_attribute = saved_fn_attr_x8;
            g_mock_target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
        }

        /* 4) __attribute__((constructor)) / __attribute__((destructor)) & priority flags + eh_cleanup_dip/barrier save/restore + OOM */
        {
            a_type r_void0_t = {}, r_int0_t = {}, r_void1_t = {};
            r_void0_t.kind = tk_routine; r_void0_t.variant.routine.return_type = &t_void;
            r_int0_t.kind = tk_routine; r_int0_t.variant.routine.return_type = &t_i32;
            a_param_type cd_p1 = {}; cd_p1.type = &t_i32;
            a_routine_type_supplement cd_supp = {}; cd_supp.param_type_list = &cd_p1;
            r_void1_t.kind = tk_routine; r_void1_t.variant.routine.return_type = &t_void; r_void1_t.variant.routine.extra_info = &cd_supp;
            a_statement cd_body_stmt = {}; cd_body_stmt.kind = stmk_block;
            a_scope cd_def_sc = {}; cd_def_sc.kind = sck_function; cd_def_sc.assoc_block = &cd_body_stmt;
            g_mock_def_scope = &cd_def_sc;

            for (int b = 0; b < 320; ++b) {
                GCC_GEN_BE_CHECK(gcc_gen_be_init());
                gcc_jit_context *cctx = gcc_jit_context_acquire();
                GCC_GEN_BE_CHECK(gcc_gen_be_set_context(cctx));
                gcc_gen_be_singleton_state.eh_cleanup_dip_depth = 1U;
                gcc_gen_be_singleton_state.eh_try_barrier_depth = 1U;
                a_routine rc1 = {}, rc2 = {}, rc3 = {}, rc4 = {}, rc5 = {};
                rc1.source_corresp.name = "attr_ctor_fn"; rc1.type = &r_void0_t; rc1.function_def_number = 1; rc1.is_initialization_routine = TRUE;
                rc2.source_corresp.name = "attr_dtor_fn1"; rc2.type = &r_void0_t; rc2.function_def_number = 1; rc2.is_finalization_routine = TRUE;
                rc3.source_corresp.name = "attr_cd_prio"; rc3.type = &r_void0_t; rc3.function_def_number = 1; rc3.has_ctor_priority = TRUE; rc3.has_dtor_priority = TRUE;
                rc4.source_corresp.name = "attr_ctor_1p"; rc4.type = &r_void1_t; rc4.function_def_number = 1; rc4.is_initialization_routine = TRUE;
                rc5.source_corresp.name = "attr_ctor_int"; rc5.type = &r_int0_t; rc5.function_def_number = 1; rc5.is_initialization_routine = TRUE;
                gcc_jit_function *cf = NULL;
                g_gcc_be_oom_countdown = b;
                gcc_gen_be_error_t ce = gcc_gen_be_lower_function_decl(&rc1, &cf);
                if (ce == GCC_GEN_BE_SUCCESS) ce = gcc_gen_be_lower_function_decl(&rc2, &cf);
                if (ce == GCC_GEN_BE_SUCCESS) ce = gcc_gen_be_lower_function_decl(&rc3, &cf);
                if (ce == GCC_GEN_BE_SUCCESS) ce = gcc_gen_be_lower_function_decl(&rc4, &cf);
                if (ce == GCC_GEN_BE_SUCCESS) ce = gcc_gen_be_lower_function_decl(&rc5, &cf);
                g_gcc_be_oom_countdown = -1;
                GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
                if (ce == GCC_GEN_BE_SUCCESS) break;
            }
            g_mock_def_scope = NULL;
        }

        GCC_GEN_BE_CHECK(gcc_gen_be_init());
        gcc_jit_context *end_ctx = gcc_jit_context_acquire();
        GCC_GEN_BE_CHECK(gcc_gen_be_set_context(end_ctx));
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_gcc_be_decl.cpp`.
 * @details Runs `run_decl_unit_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) noexcept {
    int ok = 0;
    gcc_gen_be_error_t err = run_decl_unit_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}
