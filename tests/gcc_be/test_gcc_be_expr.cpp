#include "basic_hdrs.h"
#include "gcc_gen_be_expr.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
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
    a_boolean gcc_mode = FALSE;
    
    static gcc_jit_context *g_ctx = NULL;

    // Mocks for globals we don't have
    gcc_gen_be_error_t gcc_gen_be_lower_type(a_type_ptr tp, struct gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
        *out_type = gcc_jit_context_get_type(g_ctx, GCC_JIT_TYPE_INT);
        return GCC_GEN_BE_SUCCESS;
    }
    
    long long value_of_integer_constant(a_constant* con, a_boolean* err) {
        return 42;
    }
    double fetch_host_fp_value(a_float_kind kind, an_internal_float_value* val) {
        return 3.14;
    }
    
    gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
        return GCC_GEN_BE_SUCCESS;
    }

    a_targ_size_t f_size_of_type(a_type_ptr type) {
        return 4;
    }
}

extern "C" {
    gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t err, const char* msg) GCC_GEN_BE_NOEXCEPT { return GCC_GEN_BE_SUCCESS; }
    
    // Mocks for lib_loader optionally loaded ptrs
    void *p_gcc_jit_context_new_struct_constructor = NULL;
    void *p_gcc_jit_context_new_array_constructor = NULL;
}

static a_type* make_int_type() {
    a_type* tp = (a_type*)calloc(1, sizeof(a_type));
    tp->kind = tk_integer;
    return tp;
}

static an_expr_node* make_int_const() {
    an_expr_node* expr = (an_expr_node*)calloc(1, sizeof(an_expr_node));
    expr->kind = enk_constant;
    expr->type = make_int_type();
    expr->variant.constant.ptr = (a_constant*)calloc(1, sizeof(a_constant));
    expr->variant.constant.ptr->kind = ck_integer;
    expr->variant.constant.ptr->type = expr->type;
    return expr;
}

static an_expr_node* make_bin_op(an_expr_operator_kind op, an_expr_node* op1, an_expr_node* op2) {
    an_expr_node* expr = (an_expr_node*)calloc(1, sizeof(an_expr_node));
    expr->kind = enk_operation;
    expr->type = make_int_type();
    expr->variant.operation.kind = op;
    expr->variant.operation.operands = op1;
    op1->next = op2;
    return expr;
}

int main() {
    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return __LINE__;
    edg::g_ctx = ctx;
    (void)gcc_gen_be_set_context(ctx);

    gcc_jit_rvalue *out_rval = NULL;
    gcc_gen_be_error_t err;

    // Test constant
    an_expr_node* expr_const = make_int_const();
    err = gcc_gen_be_lower_expr_rvalue(expr_const, &out_rval);
    if (err != GCC_GEN_BE_SUCCESS || out_rval == NULL) { printf("err=%d\n", err); return __LINE__; }

    // Test add
    an_expr_node* expr_add = make_bin_op(eok_add, make_int_const(), make_int_const());
    err = gcc_gen_be_lower_expr_rvalue(expr_add, &out_rval);
    if (err != GCC_GEN_BE_SUCCESS || out_rval == NULL) { printf("err=%d\n", err); return __LINE__; }

    // Test short circuit logical AND (requires a block)
    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_function *dummy_func = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, void_type, "dummy", 0, NULL, 0);
    gcc_jit_block *block = gcc_jit_function_new_block(dummy_func, "entry");
    (void)gcc_gen_be_set_current_block(block);
    (void)gcc_gen_be_push_block(block);

    an_expr_node* expr_and = make_bin_op(eok_land, make_int_const(), make_int_const());
    err = gcc_gen_be_lower_expr_rvalue(expr_and, &out_rval);
    if (err != GCC_GEN_BE_SUCCESS || out_rval == NULL) { printf("err=%d\n", err); return __LINE__; }
    
    // Clean up blocks
    gcc_jit_block *popped = NULL;
    (void)gcc_gen_be_pop_block(&popped);

    gcc_jit_context_release(ctx);
    printf("PASS\n");
    return 0;
}
