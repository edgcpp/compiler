/**
 * @file test_gcc_be_main.cpp
 * @brief Exhaustive unit tests for scope traversal, TU orchestration, and invocation options (`gcc_gen_be_main.c`).
 * @details Exercises `gcc_gen_be_lower_types_in_scope`, `gcc_gen_be_lower_variables_in_scope`,
 * `gcc_gen_be_lower_routines_in_scope`, `gcc_gen_be`, `back_end`, and `gcc_gen_be_apply_invocation_options`
 * across all normal and deterministic OOM/error paths.
 * @ingroup gcc_gen_be_tests
 */

#include "basic_hdrs.h"
#include "fe_common.h"
#include "il.h"
#include "gcc_gen_be_main.h"
#include "gcc_gen_be_error.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_decl.h"
#include "gcc_gen_be_rtti.h"
#include <libgccjit.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
/**
 * @brief Mock diagnostic reporter for main orchestration unit tests.
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

/** @brief Controls whether `type_is_nonreal` returns `TRUE` for `g_nonreal_tp`. */
static a_type_ptr g_nonreal_tp = NULL;
/** @brief Controls whether `gcc_gen_be_lower_function_decl` returns a real `gcc_jit_function *`. */
static a_boolean g_make_real_jit_fn = FALSE;

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
 * @details Returns `TRUE` when `tp == g_nonreal_tp`.
 * @param[in] tp The frontend type pointer.
 * @return `TRUE` if `tp == g_nonreal_tp`, `FALSE` otherwise.
 */
a_boolean type_is_nonreal(a_type_ptr tp) {
    return (tp && tp == g_nonreal_tp) ? TRUE : FALSE;
}

/**
 * @brief Stub for looking up routine scope in main unit tests.
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
 * @brief Stub for IL file reading in main unit tests.
 * @details No-op for unit tests.
 * @param[in] f_il The IL file stream.
 */
void il_read(FILE *f_il) GCC_GEN_BE_NOEXCEPT {
    if (f_il) {}
}

/**
 * @brief Stub for global constructor block lookup in main unit tests.
 * @details Lazily creates `state->global_ctor_func` and `state->global_ctor_block` on the active context.
 * @param[out] out_block Pointer to receive the global constructor block.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_global_ctor_block(struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    if (!out_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    gcc_gen_be_context_t *state = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_state(&state));
    if (!state->global_ctor_block) {
        gcc_jit_context *ctx = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
        gcc_jit_type *vt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        state->global_ctor_func = gcc_jit_context_new_function(
            ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, vt, "__edg_test_gctor", 0, NULL, 0);
        state->global_ctor_block = gcc_jit_function_new_block(state->global_ctor_func, "entry");
    }
    *out_block = state->global_ctor_block;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for compile-unit DWARF debug info emission in main unit tests.
 * @details Steps OOM budget and returns `GCC_GEN_BE_SUCCESS`.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_emit_compile_unit_debug_info(void) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for registering OpenMP pragmas in scope in main unit tests.
 * @details Steps OOM budget and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] scope Frontend scope pointer.
 * @return `GCC_GEN_BE_SUCCESS` or `GCC_GEN_BE_ERROR_OOM`.
 */
gcc_gen_be_error_t gcc_gen_be_register_omp_pragmas_in_scope(a_scope_ptr scope) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return scope ? GCC_GEN_BE_SUCCESS : GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for canonical function mangler in main unit tests.
 * @details Returns NULL.
 * @param[in] routine Frontend routine pointer.
 * @return NULL.
 */
char *get_mangled_function_name(a_routine_ptr routine) GCC_GEN_BE_NOEXCEPT {
    return routine ? nullptr : nullptr;
}

/**
 * @brief Stub for canonical variable mangler in main unit tests.
 * @details Returns NULL.
 * @param[in] variable Frontend variable pointer.
 * @return NULL.
 */
a_const_char *get_mangled_variable_name(a_variable_ptr variable) GCC_GEN_BE_NOEXCEPT {
    return variable ? nullptr : nullptr;
}

/**
 * @brief Stub for canonical vtable mangler in main unit tests.
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
 * @brief Stub for canonical typeinfo mangler in main unit tests.
 * @details Returns NULL.
 * @param[in] type Frontend type pointer.
 * @return NULL.
 */
char *mangled_typeinfo_name(a_type_ptr type) GCC_GEN_BE_NOEXCEPT {
    return type ? nullptr : nullptr;
}

/**
 * @brief Stub for canonical typeinfo string mangler in main unit tests.
 * @details Returns NULL.
 * @param[in] type Frontend type pointer.
 * @return NULL.
 */
char *mangled_typeinfo_string_name(a_type_ptr type) GCC_GEN_BE_NOEXCEPT {
    return type ? nullptr : nullptr;
}

/**
 * @brief Stub for canonical VTT mangler in main unit tests.
 * @details Returns NULL.
 * @param[in] type Frontend type pointer.
 * @return NULL.
 */
char *mangled_virtual_table_table_name(a_type_ptr type) GCC_GEN_BE_NOEXCEPT {
    return type ? nullptr : nullptr;
}

/**
 * @brief Stub for global variable declaration lowering in main unit tests.
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
 * @brief Stub for function declaration lowering in main unit tests.
 * @details Steps OOM budget and optionally synthesizes an empty `void(void)` function when `g_make_real_jit_fn` is set.
 * @param[in] rout The frontend routine pointer.
 * @param[out] out_func Pointer to receive the lowered function.
 * @return `GCC_GEN_BE_SUCCESS` or error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
    if (!out_func) return rout ? GCC_GEN_BE_ERROR_INVALID_ARGUMENT : GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_func = NULL;
    if (g_make_real_jit_fn && rout && rout->source_corresp.name) {
        void *cached = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, rout, &cached));
        if (cached) {
            *out_func = (gcc_jit_function *)cached;
            return GCC_GEN_BE_SUCCESS;
        }
        gcc_jit_context *ctx = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
        if (ctx) {
            gcc_jit_type *vt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
            gcc_jit_function *fn = gcc_jit_context_new_function(
                ctx, NULL, GCC_JIT_FUNCTION_INTERNAL, vt, rout->source_corresp.name, 0, NULL, 0);
            gcc_jit_block *blk = gcc_jit_function_new_block(fn, "entry");
            gcc_jit_block_end_with_void_return(blk, NULL);
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, rout, fn));
            *out_func = fn;
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Stub for type descriptor emission in main unit tests.
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
 * @brief Stub for vtable emission in main unit tests.
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
 * @brief Stub for VTT emission in main unit tests.
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
 * @brief Exercises recursive scope walkers (`types`, `variables`, `routines`) and visited-scope resizing.
 * @details Covers all type/variable/routine filter predicates, asm entries, namespaces, sub-scopes, and OOM paths.
 * @param[in] ctx Active `gcc_jit_context`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_scope_walkers(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT {
    if (gcc_gen_be_lower_types_in_scope(NULL) != GCC_GEN_BE_ERROR_SCOPE_TRAVERSAL_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_variables_in_scope(NULL) != GCC_GEN_BE_ERROR_SCOPE_TRAVERSAL_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_routines_in_scope(NULL) != GCC_GEN_BE_ERROR_SCOPE_TRAVERSAL_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));
    GCC_GEN_BE_CHECK(cache_clear_all());

    /* Test >64 distinct scopes to exercise visited_scopes 0->32->64->128 growth & OOM */
    a_scope many_scopes[70];
    std::memset(many_scopes, 0, sizeof(many_scopes));
    for (int i = 0; i < 69; ++i) {
        many_scopes[i].kind = sck_namespace;
        many_scopes[i].scopes = &many_scopes[i + 1];
    }
    many_scopes[69].kind = sck_namespace;

    free(gcc_gen_be_singleton_state.visited_scopes);
    gcc_gen_be_singleton_state.visited_scopes = NULL;
    gcc_gen_be_singleton_state.visited_scopes_count = 0;
    gcc_gen_be_singleton_state.visited_scopes_capacity = 0;
    g_gcc_be_oom_countdown = 1;
    if (gcc_gen_be_lower_types_in_scope(&many_scopes[0]) != GCC_GEN_BE_ERROR_OOM) return GCC_GEN_BE_ERROR_INTERNAL;
    g_gcc_be_oom_countdown = -1;

    gcc_gen_be_singleton_state.visited_scopes_count = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_types_in_scope(&many_scopes[0]));
    /* Re-visiting an already visited scope hits the cache hit early-return */
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_types_in_scope(&many_scopes[0]));

    /* Construct rich scope hierarchy covering all asm, type, variable, routine, namespace, and sub-scope branches */
    a_scope root_scope = {};
    a_scope assoc_cls_scope = {};
    a_scope ns_scope = {};
    a_scope sub_file_scope = {};
    a_scope sub_ns_scope = {};
    a_scope sub_nsext_scope = {};
    a_scope sub_csu_scope = {};
    a_scope sub_blk_scope = {};
    root_scope.kind = sck_file;
    assoc_cls_scope.kind = sck_class_struct_union;
    ns_scope.kind = sck_namespace;
    sub_file_scope.kind = sck_file;
    sub_ns_scope.kind = sck_namespace;
    sub_nsext_scope.kind = sck_namespace_extension;
    sub_csu_scope.kind = sck_class_struct_union;
    sub_blk_scope.kind = sck_block;
    sub_file_scope.next = &sub_ns_scope;
    sub_ns_scope.next = &sub_nsext_scope;
    sub_nsext_scope.next = &sub_csu_scope;
    sub_csu_scope.next = &sub_blk_scope;
    root_scope.scopes = &sub_file_scope;

    /* ASM entries: null string, non-ck_string, ck_string null value, ck_string valid value */
    a_constant asm_int_c = {}, asm_null_c = {}, asm_ok_c = {};
    asm_int_c.kind = ck_integer;
    asm_null_c.kind = ck_string;
    asm_null_c.variant.string.value = NULL;
    asm_ok_c.kind = ck_string;
    asm_ok_c.variant.string.value = ".text";
    an_asm_entry a1 = {}, a2 = {}, a3 = {}, a4 = {};
    a1.asm_string = NULL; a1.next = &a2;
    a2.asm_string = &asm_int_c; a2.next = &a3;
    a3.asm_string = &asm_null_c; a3.next = &a4;
    a4.asm_string = &asm_ok_c;
    root_scope.asm_entries = &a1;

    /* Types: ignored nonreal, __SO__ prefix, tk_integer, prototype class, unspecialized template,
       specialized template, union, non-virtual struct (null extra_info), struct with null assoc_scope,
       virtual class (uncached + vbase), virtual class (cached) */
    a_class_type_supplement sup_with_scope = {}, sup_no_scope = {};
    sup_with_scope.assoc_scope = &assoc_cls_scope;
    sup_no_scope.assoc_scope = NULL;

    a_type t_nonreal = {}, t_so = {}, t_int = {}, t_proto = {}, t_tmpl_unspec = {};
    a_type t_tmpl_spec = {}, t_union = {}, t_plain = {}, t_virt_novbase = {}, t_virt_vbase = {}, t_virt_cached = {};
    g_nonreal_tp = &t_nonreal;
    t_nonreal.kind = tk_struct; t_nonreal.next = &t_so;
    t_so.kind = tk_struct; t_so.source_corresp.name = "__SO__internal"; t_so.next = &t_int;
    t_int.kind = tk_integer; t_int.source_corresp.name = "my_int"; t_int.next = &t_proto;
    t_proto.kind = tk_class; t_proto.variant.class_struct_union.is_prototype_instantiation = TRUE; t_proto.next = &t_tmpl_unspec;
    t_tmpl_unspec.kind = tk_class; t_tmpl_unspec.variant.class_struct_union.is_template_class = TRUE;
    t_tmpl_unspec.variant.class_struct_union.is_specialized = FALSE; t_tmpl_unspec.next = &t_tmpl_spec;
    t_tmpl_spec.kind = tk_struct; t_tmpl_spec.variant.class_struct_union.is_template_class = TRUE;
    t_tmpl_spec.variant.class_struct_union.is_specialized = TRUE;
    t_tmpl_spec.variant.class_struct_union.extra_info = &sup_no_scope; t_tmpl_spec.next = &t_union;
    t_union.kind = tk_union; t_union.variant.class_struct_union.extra_info = &sup_with_scope; t_union.next = &t_plain;
    t_plain.kind = tk_struct; t_plain.variant.class_struct_union.extra_info = NULL; t_plain.next = &t_virt_novbase;
    t_virt_novbase.kind = tk_class;
    t_virt_novbase.variant.class_struct_union.any_virtual_functions = TRUE;
    t_virt_novbase.variant.class_struct_union.any_virtual_base_classes = FALSE;
    t_virt_novbase.next = &t_virt_vbase;
    t_virt_vbase.kind = tk_class;
    t_virt_vbase.variant.class_struct_union.any_virtual_functions = TRUE;
    t_virt_vbase.variant.class_struct_union.any_virtual_base_classes = TRUE;
    t_virt_vbase.next = &t_virt_cached;
    t_virt_cached.kind = tk_struct;
    t_virt_cached.variant.class_struct_union.any_virtual_functions = TRUE;
    t_virt_cached.variant.class_struct_union.any_virtual_base_classes = FALSE;
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &t_virt_cached, (void *)0x1));
    root_scope.types = &t_nonreal;

    /* Namespaces: alias, null assoc_scope, valid assoc_scope */
    a_namespace ns_alias = {}, ns_null = {}, ns_valid = {};
    ns_alias.is_namespace_alias = TRUE; ns_alias.variant.assoc_scope = &ns_scope; ns_alias.next = &ns_null;
    ns_null.is_namespace_alias = FALSE; ns_null.variant.assoc_scope = NULL; ns_null.next = &ns_valid;
    ns_valid.is_namespace_alias = FALSE; ns_valid.variant.assoc_scope = &ns_scope;
    root_scope.namespaces = &ns_alias;

    /* Variables in root_scope (primary) and ns_scope (non-primary) */
    a_variable v_ign = {}, v_bind = {}, v_param = {}, v_proto = {}, v_tmpl_u = {}, v_tmpl_s = {};
    a_variable v_anon_stat = {}, v_named_stat = {}, v_stat_dyn = {};
    v_ign.type = &t_nonreal; v_ign.is_nonreal = TRUE; v_ign.next = &v_bind;
    v_bind.type = &t_int; v_bind.init_kind = initk_binding; v_bind.next = &v_param;
    v_param.type = &t_int; v_param.is_parameter = TRUE; v_param.next = &v_proto;
    v_proto.type = &t_int; v_proto.is_prototype_instantiation = TRUE; v_proto.next = &v_tmpl_u;
    v_tmpl_u.type = &t_int; v_tmpl_u.is_template_variable = TRUE; v_tmpl_u.is_specialized = FALSE; v_tmpl_u.next = &v_tmpl_s;
    v_tmpl_s.type = &t_int; v_tmpl_s.is_template_variable = TRUE; v_tmpl_s.is_specialized = TRUE;
    v_tmpl_s.source_corresp.name = "v_tmpl_s"; v_tmpl_s.next = &v_anon_stat;
    v_anon_stat.type = &t_int; v_anon_stat.storage_class = sc_static; v_anon_stat.init_kind = initk_static; v_anon_stat.next = &v_named_stat;
    v_named_stat.type = &t_int; v_named_stat.storage_class = sc_static; v_named_stat.source_corresp.name = "v_named";
    v_named_stat.init_kind = initk_static; v_named_stat.next = &v_stat_dyn;
    v_stat_dyn.type = &t_int; v_stat_dyn.storage_class = sc_static; v_stat_dyn.init_kind = initk_dynamic;
    root_scope.variables = &v_ign;

    a_variable nv_ext_none = {}, nv_ext_init = {}, nv_norm = {};
    nv_ext_none.type = &t_int; nv_ext_none.storage_class = sc_extern; nv_ext_none.init_kind = initk_none; nv_ext_none.next = &nv_ext_init;
    nv_ext_init.type = &t_int; nv_ext_init.storage_class = sc_extern; nv_ext_init.init_kind = initk_static;
    nv_ext_init.source_corresp.name = "nv_ext_init"; nv_ext_init.next = &nv_norm;
    nv_norm.type = &t_int; nv_norm.storage_class = sc_unspecified; nv_norm.source_corresp.name = "nv_norm";
    ns_scope.variables = &nv_ext_none;

    /* Routines in root_scope (primary) and ns_scope (non-primary) */
    a_routine r_ded = {}, r_ceval = {}, r_del = {}, r_pure = {}, r_proto = {}, r_ok = {};
    r_ded.type = &t_int; r_ded.special_kind = sfk_deduction_guide; r_ded.next = &r_ceval;
    r_ceval.type = &t_int; r_ceval.is_consteval = TRUE; r_ceval.next = &r_del;
    r_del.type = &t_int; r_del.is_deleted = TRUE; r_del.next = &r_pure;
    r_pure.type = &t_int; r_pure.pure_virtual = TRUE; r_pure.next = &r_proto;
    r_proto.type = &t_int; r_proto.is_prototype_instantiation = TRUE; r_proto.next = &r_ok;
    r_ok.type = &t_int; r_ok.source_corresp.name = "r_ok"; r_ok.function_def_number = 1;
    root_scope.routines = &r_ded;

    a_routine nr_undef = {}, nr_def = {};
    nr_undef.type = &t_int; nr_undef.function_def_number = NULL_function_def_number; nr_undef.next = &nr_def;
    nr_def.type = &t_int; nr_def.source_corresp.name = "nr_def"; nr_def.function_def_number = 2;
    ns_scope.routines = &nr_undef;

    il_header.primary_scope = &root_scope;

    /* Also test top_ctx == NULL branch in asm_entries */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    gcc_gen_be_singleton_state.visited_scopes_count = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_types_in_scope(&root_scope));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    /* Deterministic OOM sweeps across all 3 scope walkers */
    for (int b = 0; b < 40; ++b) {
        gcc_gen_be_singleton_state.visited_scopes_count = 0;
        GCC_GEN_BE_CHECK(cache_clear_all());
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, &t_virt_cached, (void *)0x1));
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_types_in_scope(&root_scope);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 40; ++b) {
        gcc_gen_be_singleton_state.visited_scopes_count = 0;
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_variables_in_scope(&root_scope);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    for (int b = 0; b < 40; ++b) {
        gcc_gen_be_singleton_state.visited_scopes_count = 0;
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_lower_routines_in_scope(&root_scope);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* Also run variables & routines when root_scope != primary_scope, including visited-cache hit */
    il_header.primary_scope = NULL;
    gcc_gen_be_singleton_state.visited_scopes_count = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_variables_in_scope(&root_scope));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_variables_in_scope(&root_scope));
    gcc_gen_be_singleton_state.visited_scopes_count = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_routines_in_scope(&root_scope));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_routines_in_scope(&root_scope));

    /* Exercise MAINTAIN_NEEDED_FLAGS unreferenced routine pruning branches when function_def_table != NULL */
    a_function_def_descr fdd_prune[4] = {};
    il_header.function_def_table = fdd_prune;
    a_scope prune_pri = {}, prune_ns = {};
    prune_pri.kind = sck_file;
    prune_ns.kind = sck_namespace;
    prune_pri.scopes = &prune_ns;
    a_routine rp_undef = {}, rp_need = {}, rp_keep = {}, rp_sinl = {}, rp_inl_ext = {}, rp_pure_def = {};
    rp_undef.type = &t_int; rp_undef.function_def_number = NULL_function_def_number; rp_undef.next = &rp_need;
    rp_need.type = &t_int; rp_need.function_def_number = 1; rp_need.definition_needed = TRUE; rp_need.next = &rp_keep;
    rp_keep.type = &t_int; rp_keep.function_def_number = 1; rp_keep.keep_definition_in_il = TRUE; rp_keep.next = &rp_sinl;
    rp_sinl.type = &t_int; rp_sinl.function_def_number = 1; rp_sinl.is_inline = TRUE; rp_sinl.storage_class = sc_static; rp_sinl.next = &rp_inl_ext;
    rp_inl_ext.type = &t_int; rp_inl_ext.function_def_number = 1; rp_inl_ext.is_inline = TRUE; rp_inl_ext.storage_class = sc_unspecified; rp_inl_ext.next = &rp_pure_def;
    rp_pure_def.type = &t_int; rp_pure_def.pure_virtual = TRUE; rp_pure_def.function_def_number = 1; rp_pure_def.definition_needed = TRUE;
    prune_pri.routines = &rp_undef;
    a_routine rn_inl = {}, rn_ref = {}, rn_unref = {};
    rn_inl.type = &t_int; rn_inl.function_def_number = 1; rn_inl.is_inline = TRUE; rn_inl.source_corresp.referenced = TRUE; rn_inl.next = &rn_ref;
    rn_ref.type = &t_int; rn_ref.function_def_number = 1; rn_ref.source_corresp.referenced = TRUE; rn_ref.next = &rn_unref;
    rn_unref.type = &t_int; rn_unref.function_def_number = 1; rn_unref.source_corresp.referenced = FALSE;
    prune_ns.routines = &rn_inl;
    il_header.primary_scope = &prune_pri;
    gcc_gen_be_singleton_state.visited_scopes_count = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_routines_in_scope(&prune_pri));
    il_header.function_def_table = NULL;
    il_header.primary_scope = NULL;
    g_nonreal_tp = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Exercises `gcc_gen_be()`, `back_end()`, `__sti__*` synthesis, module imports, and output path derivation.
 * @details Covers IL reading, module partitions, `.s`/`.o`/`.so`/`a.out` default names, single-TU linking, and OOM sweeps.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t test_gcc_gen_be_pipeline(void) GCC_GEN_BE_NOEXCEPT {
    /* 1. IL reading branches: !skip_il_read with NULL f_il_output, non-NULL f_il_output with/without primary_source_file */
    skip_il_read = FALSE;
    f_il_output = NULL;
    gcc_be_compile_only = TRUE;
    gcc_be_assemble_only = FALSE;
    gcc_be_shared = FALSE;
    gcc_be_output_file_name = NULL;
    primary_source_file_name = NULL;
    il_header.primary_scope = NULL;
    il_header.imported_modules = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be());

    FILE *tmp_il = std::tmpfile();
    if (!tmp_il) return GCC_GEN_BE_ERROR_INTERNAL;
    f_il_output = tmp_il;
    il_header.primary_source_file = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be());

    a_source_file sf = {};
    sf.file_name = "/tmp/edg_main_il_src.cpp";
    il_header.primary_source_file = &sf;
    GCC_GEN_BE_CHECK(gcc_gen_be());
    std::remove("/tmp/edg_main_il_src.o");
    std::fclose(tmp_il);
    f_il_output = NULL;
    skip_il_read = TRUE;
    il_header.primary_source_file = NULL;

    /* 2. __sti__* routines (unnamed, non-__sti__, undefined __sti__, defined __sti__ returning NULL vs real fn) */
    a_scope pscope = {};
    pscope.kind = sck_file;
    a_type vt = {};
    vt.kind = tk_void;
    a_routine r1 = {}, r2 = {}, r3 = {}, r4 = {};
    r1.type = &vt; r1.source_corresp.name = NULL; r1.function_def_number = 1; r1.next = &r2;
    r2.type = &vt; r2.source_corresp.name = "normal_fn"; r2.function_def_number = 2; r2.next = &r3;
    r3.type = &vt; r3.source_corresp.name = "__sti__undef"; r3.function_def_number = NULL_function_def_number; r3.next = &r4;
    r4.type = &vt; r4.source_corresp.name = "__sti__tu0"; r4.function_def_number = 3;
    pscope.routines = &r1;
    il_header.primary_scope = &pscope;

    /* Imported modules: null module_info, mk_unit null name, mk_header_unit, mk_unit_partition null & valid name */
    a_module m_unit_null = {}, m_hdr = {}, m_part_null = {}, m_part_ok = {};
    m_unit_null.kind = mk_unit; m_unit_null.variant.unit.name = NULL;
    m_hdr.kind = mk_header_unit;
    m_part_null.kind = mk_unit_partition; m_part_null.variant.unit_partition.name = NULL;
    m_part_ok.kind = mk_unit_partition; m_part_ok.variant.unit_partition.name = "mod:part";
    a_module_import_decl id0 = {}, id1 = {}, id2 = {}, id3 = {}, id4 = {};
    id0.module_info = NULL; id0.next = &id1;
    id1.module_info = &m_unit_null; id1.next = &id2;
    id2.module_info = &m_hdr; id2.next = &id3;
    id3.module_info = &m_part_null; id3.next = &id4;
    id4.module_info = &m_part_ok;
    il_header.imported_modules = &id0;

    gcc_be_output_file_name = "/tmp/edg_main_sti.o";
    g_make_real_jit_fn = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be());
    g_make_real_jit_fn = TRUE;
    GCC_GEN_BE_CHECK(gcc_gen_be());
    std::remove("/tmp/edg_main_sti.o");

    /* Imported module list with no named unit/partition (tu_mod_name == NULL) */
    id3.next = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be());
    std::remove("/tmp/edg_main_sti.o");

    /* Imported module with named mk_unit */
    a_module m_unit_ok = {};
    m_unit_ok.kind = mk_unit;
    m_unit_ok.variant.unit.name = "math.core";
    a_module_import_decl id_unit_ok = {};
    id_unit_ok.module_info = &m_unit_ok;
    il_header.imported_modules = &id_unit_ok;
    GCC_GEN_BE_CHECK(gcc_gen_be());
    std::remove("/tmp/edg_main_sti.o");

    /* OOM sweep across gcc_gen_be() with primary_scope, __sti__, and module partition */
    id3.next = &id4;
    il_header.imported_modules = &id0;
    for (int b = 0; b < 150; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be();
        g_gcc_be_oom_countdown = -1;
        GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    std::remove("/tmp/edg_main_sti.o");
    g_make_real_jit_fn = FALSE;
    il_header.primary_scope = NULL;
    il_header.imported_modules = NULL;

    /* 3. Default output filename derivation & artifact modes */
    gcc_be_output_file_name = NULL;
    /* assemble_only (-S) with dot == primary_source_file_name (".hidden") */
    gcc_be_assemble_only = TRUE;
    gcc_be_compile_only = FALSE;
    primary_source_file_name = "/tmp/.edg_no_dot_test";
    /* First test true leading dot in cwd-safe /tmp: strrchr finds '.', != primary; test ".edg_hidden" directly */
    primary_source_file_name = ".edg_hidden_tmp";
    GCC_GEN_BE_CHECK(gcc_gen_be());
    std::remove(".edg_hidden_tmp.s");

    /* No dot at all */
    primary_source_file_name = "/tmp/edg_nodot_file";
    /* Note: no '.' in path! */
    GCC_GEN_BE_CHECK(gcc_gen_be());
    std::remove("/tmp/edg_nodot_file.s");

    /* >500 character prefix before '.cpp' */
    char long_src[540];
    std::memset(long_src, 'a', sizeof(long_src));
    std::memcpy(long_src, "/tmp/", 5);
    std::snprintf(long_src + 505, sizeof(long_src) - 505, ".cpp");
    primary_source_file_name = long_src;
    /* Will fail compile_to_file due to ENAMETOOLONG, covering emit_artifact error in gcc_gen_be */
    if (gcc_gen_be() != GCC_GEN_BE_ERROR_COMPILATION_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());

    /* Measure exact OOM step reaching gcc_gen_be_emit_artifact, then run with lto_mode = 1 */
    gcc_be_assemble_only = FALSE;
    gcc_be_compile_only = TRUE;
    gcc_be_shared = FALSE;
    primary_source_file_name = "dummy_src.cpp";
    gcc_gen_be_singleton_state.lto_mode = 0;
    gcc_be_output_file_name = "/nonexistent_edg_dir/probe.o";
    g_gcc_be_oom_countdown = 10000;
    if (gcc_gen_be() != GCC_GEN_BE_ERROR_COMPILATION_FAILED) {
        g_gcc_be_oom_countdown = -1;
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    int emit_step = 10000 - g_gcc_be_oom_countdown - 1;
    g_gcc_be_oom_countdown = -1;
    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());

    gcc_be_compile_only = FALSE;
    gcc_gen_be_singleton_state.lto_mode = 1;
    gcc_be_output_file_name = "/tmp/edg_lto_direct_test.out";
    g_gcc_be_oom_countdown = emit_step;
    if (gcc_gen_be() != GCC_GEN_BE_ERROR_OOM) {
        g_gcc_be_oom_countdown = -1;
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    g_gcc_be_oom_countdown = -1;
    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());

    /* Default "a.out" branch when final_out_path == NULL and ext == "" (use EDG_GCC_DRIVER_BIN=/usr/bin/false so no ./a.out is created) */
    gcc_gen_be_singleton_state.lto_mode = 0;
    gcc_be_output_file_name = NULL;
    setenv("EDG_GCC_DRIVER_BIN", "/usr/bin/false", 1);
    if (gcc_gen_be() != GCC_GEN_BE_ERROR_MULTI_TU_PIPELINE_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());

    /* Single-TU native link success for executable and shared library, plus tmp_obj emit error & OOM */
    setenv("EDG_GCC_DRIVER_BIN", "/usr/bin/true", 1);
    gcc_be_output_file_name = "/tmp/edg_single_exe.out";
    GCC_GEN_BE_CHECK(gcc_gen_be());
    gcc_be_shared = TRUE;
    gcc_be_output_file_name = NULL;
    primary_source_file_name = "/tmp/edg_single_sh.cpp";
    GCC_GEN_BE_CHECK(gcc_gen_be());

    /* tmp_obj emit failure */
    gcc_be_output_file_name = "/nonexistent_edg_dir/edg_bad_out";
    if (gcc_gen_be() != GCC_GEN_BE_ERROR_COMPILATION_FAILED) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());

    /* OOM sweep across single-TU native link & cleanup in gcc_gen_be */
    gcc_be_output_file_name = "/tmp/edg_single_oom.out";
    for (int b = 0; b < 150; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be();
        g_gcc_be_oom_countdown = -1;
        GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());
        if (e == GCC_GEN_BE_SUCCESS) break;
    }
    unsetenv("EDG_GCC_DRIVER_BIN");
    gcc_be_shared = FALSE;
    gcc_be_compile_only = TRUE;
    gcc_be_output_file_name = NULL;
    primary_source_file_name = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Executes all unit tests for `gcc_gen_be_main.c` scope walkers, pipeline, and invocation options.
 * @param[out] out_ok Pointer to receive 1 on success, 0 on failure.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t run_main_unit_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    GCC_GEN_BE_CHECK(gcc_gen_be_early_init());

    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_OOM;

    GCC_GEN_BE_CHECK(test_scope_walkers(ctx));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    gcc_jit_context_release(ctx);
    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());

    GCC_GEN_BE_CHECK(test_gcc_gen_be_pipeline());

    /* Exhaustive gcc_gen_be_apply_invocation_options & copy_driver_string_array tests */
    if (gcc_gen_be_apply_invocation_options(NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }

    const char *dirs1[2] = { "/usr/lib", NULL };
    const char *libs1[2] = { "m", "pthread" };
    const char *ld1[1] = { "-Wl,-rpath,/usr/lib" };
    edg_be_invocation_options_t opts1 = {};
    opts1.opt_level = 3;
    opts1.opt_size_level = 1;
    opts1.debug_level = 3;
    opts1.artifact_kind = edg_output_artifact_kind_t::ir_or_c_file;
    opts1.reloc_mode = edg_reloc_mode_t::static_no_pic;
    opts1.static_link = true;
    opts1.fast_math = true;
    opts1.lto_mode = edg_lto_mode_t::full_lto;
    opts1.symbol_visibility = edg_visibility_mode_t::hidden_vis;
    opts1.function_sections = true;
    opts1.data_sections = true;
    opts1.openmp_enabled = true;
    opts1.pthread_enabled = true;
    opts1.sanitize_address = true;
    opts1.sanitize_undefined = true;
    opts1.sanitize_thread = true;
    opts1.sanitize_memory = true;
    opts1.mcpu = "generic";
    opts1.march = "armv8-a";
    opts1.mcmodel = "small";
    opts1.output_file_path = "/tmp/out1.s";
    opts1.library_search_dirs = dirs1;
    opts1.num_library_search_dirs = 2;
    opts1.libraries = libs1;
    opts1.num_libraries = 2;
    opts1.linker_passthrough_args = ld1;
    opts1.num_linker_passthrough_args = 1;
    GCC_GEN_BE_CHECK(gcc_gen_be_apply_invocation_options(&opts1));

    /* Second call with empty strings for mcpu/march/mcmodel, NULL output_file_path, and replacement arrays */
    edg_be_invocation_options_t opts2 = {};
    opts2.artifact_kind = edg_output_artifact_kind_t::executable;
    opts2.reloc_mode = edg_reloc_mode_t::pic;
    opts2.mcpu = "";
    opts2.march = "";
    opts2.mcmodel = "";
    opts2.output_file_path = NULL;
    opts2.libraries = libs1;
    opts2.num_libraries = 0;
    GCC_GEN_BE_CHECK(gcc_gen_be_apply_invocation_options(&opts2));

    edg_be_invocation_options_t opts3 = {};
    opts3.artifact_kind = edg_output_artifact_kind_t::object_file;
    opts3.reloc_mode = edg_reloc_mode_t::pie;
    GCC_GEN_BE_CHECK(gcc_gen_be_apply_invocation_options(&opts3));

    edg_be_invocation_options_t opts4 = {};
    opts4.artifact_kind = edg_output_artifact_kind_t::assembly_file;
    GCC_GEN_BE_CHECK(gcc_gen_be_apply_invocation_options(&opts4));

    edg_be_invocation_options_t opts5 = {};
    opts5.artifact_kind = edg_output_artifact_kind_t::shared_library;
    GCC_GEN_BE_CHECK(gcc_gen_be_apply_invocation_options(&opts5));

    /* Deterministic OOM sweep across copy_driver_string_array (hits arr alloc & per-string alloc j < i cleanup) */
    for (int b = 0; b < 10; ++b) {
        g_gcc_be_oom_countdown = b;
        gcc_gen_be_error_t e = gcc_gen_be_apply_invocation_options(&opts1);
        g_gcc_be_oom_countdown = -1;
        if (e == GCC_GEN_BE_SUCCESS) break;
    }

    /* Exercise v->is_pack, v->is_handler_param, v->type == NULL, sck_func_prototype subscope, and link_multi_tu ldflags prefixes */
    {
        gcc_jit_context *mctx = gcc_jit_context_acquire();
        GCC_GEN_BE_CHECK(gcc_gen_be_set_context(mctx));
        a_variable mv_pack = {}, mv_hparam = {}, mv_null_ty = {};
        mv_pack.is_pack = TRUE; mv_pack.next = &mv_hparam;
        mv_hparam.is_handler_param = TRUE; mv_hparam.next = &mv_null_ty;
        mv_null_ty.type = NULL; mv_null_ty.source_corresp.name = "mv_null_ty_sym";

        a_scope msc_fproto = {}, msc_exp_proto = {}, msc_fnbody = {}, msc_top = {};
        msc_fproto.kind = sck_func_prototype;
        msc_exp_proto.kind = sck_block;
        msc_exp_proto.is_expansion_prototype = TRUE;
        msc_fproto.next = &msc_exp_proto;
        msc_fnbody.kind = sck_function;
        a_function_def_descr mfdefs[2] = {};
        mfdefs[1].scope = &msc_fnbody;
        a_type mr_int_tp = {}; mr_int_tp.kind = tk_integer; mr_int_tp.size = 4;
        a_routine mr_def = {};
        mr_def.type = &mr_int_tp; mr_def.source_corresp.name = "mr_def_fn";
        mr_def.function_def_number = 1; mr_def.definition_needed = TRUE;
        msc_top.kind = sck_file;
        msc_top.variables = &mv_pack;
        msc_top.routines = &mr_def;
        msc_top.scopes = &msc_fproto;
        il_header.primary_scope = &msc_top;
        il_header.function_def_table = mfdefs;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_variables_in_scope(&msc_top));
        for (int b = 0; b < 20; ++b) {
            gcc_gen_be_singleton_state.visited_scopes_count = 0;
            g_gcc_be_oom_countdown = b;
            gcc_gen_be_error_t mre = gcc_gen_be_lower_routines_in_scope(&msc_top);
            g_gcc_be_oom_countdown = -1;
            if (mre == GCC_GEN_BE_SUCCESS) break;
        }
        gcc_gen_be_singleton_state.visited_scopes_count = 0;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_routines_in_scope(&msc_top));
        il_header.function_def_table = NULL;
        il_header.primary_scope = NULL;

        const char *ld_all[] = {"", "-fuse-ld=lld", "-F/System/Library/Frameworks", "-Wl,-v", "-Xlinker", "--custom-flag"};
        edg_be_invocation_options_t lopts = {};
        lopts.linker_passthrough_args = ld_all;
        lopts.num_linker_passthrough_args = 6;
        GCC_GEN_BE_CHECK(gcc_gen_be_apply_invocation_options(&lopts));
        const char *tmp_obj_path = ".tmp/agent-gcc/main_ldflags_probe.o";
        std::FILE *tf = std::fopen(tmp_obj_path, "wb");
        if (tf) std::fclose(tf);
        const char *fake_objs[] = {tmp_obj_path};
        int link_status = 0;
        /* Invokes gcc_gen_be_link_multi_tu_objects exercising all driver_ldflags branches (1158..1167) */
        gcc_gen_be_error_t link_rc = gcc_gen_be_link_multi_tu_objects(fake_objs, 1, ".tmp/agent-gcc/main_ldflags_probe.out", 0, NULL, &link_status);
        std::remove(tmp_obj_path);
        std::remove(".tmp/agent-gcc/main_ldflags_probe.out");
        if (link_rc == GCC_GEN_BE_SUCCESS && link_status == 0) return GCC_GEN_BE_ERROR_INTERNAL;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_cleanup());

    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_gcc_be_main.cpp`.
 * @details Runs `run_main_unit_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) noexcept {
    int ok = 0;
    gcc_gen_be_error_t err = run_main_unit_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) return 1;
    std::puts("PASS");
    return 0;
}
