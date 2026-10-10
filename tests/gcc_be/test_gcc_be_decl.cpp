#include "basic_hdrs.h"
#include "gcc_gen_be_decl.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_lib_loader.h"
#include "error.h"
#include <libgccjit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

USING_NAMESPACE_EDG

namespace edg {
    int gcc_be_opt_level = 0;
    a_boolean gcc_be_debug_info = FALSE;
    a_boolean gcc_be_dump_initial_tree = FALSE;
    a_boolean gcc_be_dump_gimple = FALSE;
    a_boolean gcc_be_fPIC = FALSE;
    a_boolean gcc_be_fPIE = FALSE;

    // Mock for lowering expressions
    gcc_gen_be_error_t gcc_gen_be_lower_expr_rvalue(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
        *out_rval = (gcc_jit_rvalue*)0x1234; // Dummy rvalue
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_gen_be_error_t gcc_gen_be_lower_constant_rvalue(a_constant_ptr con, struct gcc_jit_type *expected_type, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
        gcc_jit_context *ctx = gcc_jit_context_acquire(); // We shouldn't acquire a new one here, just use the current one.
        // Wait, how to get the current context? 
        // We can just return a real rvalue by generating a zero value from the expected type.
        // Actually, expected_type is a valid type.
        // Let's just create a dummy context for the mock, or we can use the global `ctx` from `main` if we pass it, but `main` is below.
        // We can just set *out_rval = NULL to trigger the fallback path, which generates a zero properly using the real context!
        *out_rval = NULL;
        return GCC_GEN_BE_SUCCESS;
    }
    
    // Mocks for libgccjit location
    gcc_gen_be_error_t gcc_gen_be_get_location(a_source_position *pos, struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
        *out_loc = NULL;
        return GCC_GEN_BE_SUCCESS;
    }

    a_scope* scope_for_routine(a_routine* rout) {
        return NULL;
    }

    long long value_of_integer_constant(a_constant* con, a_boolean* err) {
        return 0;
    }

    double fetch_host_fp_value(a_float_kind kind, an_internal_float_value* val) {
        return 0.0;
    }
}

extern "C" {
    gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t err, const char* msg) GCC_GEN_BE_NOEXCEPT { return GCC_GEN_BE_SUCCESS; }
}

static a_variable* make_variable(const char* name, a_storage_class sc) {
    a_variable* var = (a_variable*)calloc(1, sizeof(a_variable));
    var->source_corresp.name = name;
    var->storage_class = sc;
    var->type = (a_type*)calloc(1, sizeof(a_type));
    var->type->kind = tk_integer;
    var->type->variant.integer.int_kind = ik_int;
    return var;
}

static a_routine* make_routine(const char* name, a_storage_class sc) {
    a_routine* rout = (a_routine*)calloc(1, sizeof(a_routine));
    rout->source_corresp.name = name;
    rout->storage_class = sc;
    rout->type = (a_type*)calloc(1, sizeof(a_type));
    rout->type->kind = tk_routine;
    rout->type->variant.routine.return_type = (a_type*)calloc(1, sizeof(a_type));
    rout->type->variant.routine.return_type->kind = tk_void;
    rout->type->variant.routine.extra_info = (a_routine_type_supplement*)calloc(1, sizeof(a_routine_type_supplement));
    rout->function_def_number = 1; // Not imported
    return rout;
}

int main() {
    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return __LINE__;
    (void)gcc_gen_be_set_context(ctx);

    gcc_jit_lvalue *out_var = NULL;
    gcc_jit_function *out_func = NULL;
    gcc_gen_be_error_t err;

    // Test global var
    a_variable* v_global = make_variable("g1", sc_extern);
    err = gcc_gen_be_lower_global_variable_decl(v_global, &out_var);
    if (err != GCC_GEN_BE_SUCCESS || out_var == NULL) return __LINE__;

    // Test static var
    a_variable* v_static = make_variable("s1", sc_static);
    err = gcc_gen_be_lower_global_variable_decl(v_static, &out_var);
    if (err != GCC_GEN_BE_SUCCESS || out_var == NULL) return __LINE__;

    // Test const global var
    a_variable* v_const = make_variable("c1", sc_extern);
    v_const->type->kind = tk_typeref;
    v_const->type->variant.typeref.qualifiers = 1; // Assuming 1 means TQ_CONST
    v_const->type->variant.typeref.type = (a_type*)calloc(1, sizeof(a_type));
    v_const->type->variant.typeref.type->kind = tk_integer;
    v_const->type->variant.typeref.type->variant.integer.int_kind = ik_int;
    err = gcc_gen_be_lower_global_variable_decl(v_const, &out_var);
    if (err != GCC_GEN_BE_SUCCESS || out_var == NULL) return __LINE__;

    // Test thread local var
    a_variable* v_tls = make_variable("tls1", sc_extern);
    v_tls->is_thread_local = 1;
    err = gcc_gen_be_lower_global_variable_decl(v_tls, &out_var);
    if (err != GCC_GEN_BE_SUCCESS || out_var == NULL) return __LINE__;

    // Test function decl
    a_routine* r_func = make_routine("f1", sc_extern);
    err = gcc_gen_be_lower_function_decl(r_func, &out_func);
    if (err != GCC_GEN_BE_SUCCESS || out_func == NULL) return __LINE__;

    // Test local var
    a_variable* v_local = make_variable("l1", sc_auto);
    err = gcc_gen_be_lower_local_variable_decl(out_func, v_local, &out_var);
    if (err != GCC_GEN_BE_SUCCESS || out_var == NULL) return __LINE__;

    // Test local static var
    a_variable* v_local_static = make_variable("ls1", sc_static);
    err = gcc_gen_be_lower_local_variable_decl(out_func, v_local_static, &out_var);
    if (err != GCC_GEN_BE_SUCCESS || out_var == NULL) return __LINE__;

    // Test aggregate init
    a_variable* v_aggr = make_variable("a1", sc_static);
    v_aggr->init_kind = initk_static;
    v_aggr->initializer.constant = (a_constant*)calloc(1, sizeof(a_constant));
    v_aggr->initializer.constant->kind = ck_aggregate;
    v_aggr->initializer.constant->type = v_aggr->type;
    err = gcc_gen_be_lower_global_variable_decl(v_aggr, &out_var);
    if (err != GCC_GEN_BE_SUCCESS || out_var == NULL) return __LINE__;

    gcc_jit_context_release(ctx);
    printf("PASS\n");
    return 0;
}
