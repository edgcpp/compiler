#include "basic_hdrs.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_lib_loader.h"
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
}

extern "C" {
    gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t err, const char* msg) GCC_GEN_BE_NOEXCEPT { return GCC_GEN_BE_SUCCESS; }
    
    // Optional modern API mocks (just to verify they get called)
    static int mock_called = 0;
    gcc_jit_type * mock_get_int_type(gcc_jit_context *ctxt, int num_bytes, int is_signed) {
        mock_called = 1;
        return gcc_jit_context_get_type(ctxt, GCC_JIT_TYPE_INT); // Fake return
    }
    gcc_jit_type * mock_get_vector(gcc_jit_type *type, size_t num_units) {
        mock_called = 2;
        return type; // Fake return
    }
}

static a_type* make_int_type(an_integer_kind kind) {
    a_type* tp = (a_type*)calloc(1, sizeof(a_type));
    tp->kind = tk_integer;
    tp->variant.integer.int_kind = kind;
    return tp;
}

static a_type* make_float_type(a_float_kind kind) {
    a_type* tp = (a_type*)calloc(1, sizeof(a_type));
    tp->kind = tk_float;
    tp->variant.float_kind = kind;
    return tp;
}

int main() {
    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return __LINE__;
    (void)gcc_gen_be_set_context(ctx);
    
    p_gcc_jit_context_get_int_type = (pfn_gcc_jit_context_get_int_type)mock_get_int_type;
    p_gcc_jit_type_get_vector = (pfn_gcc_jit_type_get_vector)mock_get_vector;

    gcc_jit_type *out = NULL;
    gcc_gen_be_error_t err;

    // Test primitive ints
    a_type* t_char = make_int_type(ik_char);
    err = gcc_gen_be_lower_type(t_char, &out);
    if (err != GCC_GEN_BE_SUCCESS || out != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR)) return __LINE__;

    a_type* t_schar = make_int_type(ik_signed_char);
    err = gcc_gen_be_lower_type(t_schar, &out);
    if (err != GCC_GEN_BE_SUCCESS || out != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIGNED_CHAR)) return __LINE__;

    a_type* t_uchar = make_int_type(ik_unsigned_char);
    err = gcc_gen_be_lower_type(t_uchar, &out);
    if (err != GCC_GEN_BE_SUCCESS || out != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR)) return __LINE__;

    a_type* t_short = make_int_type(ik_short);
    err = gcc_gen_be_lower_type(t_short, &out);
    if (err != GCC_GEN_BE_SUCCESS || out != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SHORT)) return __LINE__;

    a_type* t_ushort = make_int_type(ik_unsigned_short);
    err = gcc_gen_be_lower_type(t_ushort, &out);
    if (err != GCC_GEN_BE_SUCCESS || out != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_SHORT)) return __LINE__;

    a_type* t_int = make_int_type(ik_int);
    err = gcc_gen_be_lower_type(t_int, &out);
    if (err != GCC_GEN_BE_SUCCESS || out != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT)) return __LINE__;

    a_type* t_uint = make_int_type(ik_unsigned_int);
    err = gcc_gen_be_lower_type(t_uint, &out);
    if (err != GCC_GEN_BE_SUCCESS || out != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_INT)) return __LINE__;

    a_type* t_long = make_int_type(ik_long);
    err = gcc_gen_be_lower_type(t_long, &out);
    if (err != GCC_GEN_BE_SUCCESS || out != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG)) return __LINE__;

    a_type* t_ulong = make_int_type(ik_unsigned_long);
    err = gcc_gen_be_lower_type(t_ulong, &out);
    if (err != GCC_GEN_BE_SUCCESS || out != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG)) return __LINE__;

    a_type* t_long_long = make_int_type(ik_long_long);
    err = gcc_gen_be_lower_type(t_long_long, &out);
    if (err != GCC_GEN_BE_SUCCESS || out != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_LONG)) return __LINE__;

    a_type* t_ulong_long = make_int_type(ik_unsigned_long_long);
    err = gcc_gen_be_lower_type(t_ulong_long, &out);
    if (err != GCC_GEN_BE_SUCCESS || out != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG_LONG)) return __LINE__;

#if INT128_EXTENSIONS_ALLOWED
    a_type* t_int128 = make_int_type(ik_int128);
    mock_called = 0;
    err = gcc_gen_be_lower_type(t_int128, &out);
    if (err != GCC_GEN_BE_SUCCESS || mock_called != 1) return __LINE__;
#endif

    // Test floats
    a_type* t_float = make_float_type(fk_float);
    err = gcc_gen_be_lower_type(t_float, &out);
    if (err != GCC_GEN_BE_SUCCESS || out != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT)) return __LINE__;

    a_type* t_double = make_float_type(fk_double);
    err = gcc_gen_be_lower_type(t_double, &out);
    if (err != GCC_GEN_BE_SUCCESS || out != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE)) return __LINE__;

    a_type* t_ldouble = make_float_type(fk_long_double);
    err = gcc_gen_be_lower_type(t_ldouble, &out);
    if (err != GCC_GEN_BE_SUCCESS || out != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE)) return __LINE__;

#if defined(GCC_JIT_TYPE_FLOAT128)
    a_type* t_f128 = make_float_type(fk_float128);
    err = gcc_gen_be_lower_type(t_f128, &out);
    if (err != GCC_GEN_BE_SUCCESS || out != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT128)) return __LINE__;
#endif

#if C99_IL_EXTENSIONS_SUPPORTED
    a_type* t_complex = (a_type*)calloc(1, sizeof(a_type));
    t_complex->kind = tk_complex;
    t_complex->variant.float_kind = fk_double;
    err = gcc_gen_be_lower_type(t_complex, &out);
#if defined(GCC_JIT_TYPE_COMPLEX_DOUBLE)
    if (err != GCC_GEN_BE_SUCCESS || out != gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_COMPLEX_DOUBLE)) return __LINE__;
#else
    if (err != GCC_GEN_BE_SUCCESS || out == NULL) return __LINE__; // Struct created
#endif
#endif

    // Test caching
    gcc_jit_type *out_cached = NULL;
    gcc_jit_type *expected = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
    err = gcc_gen_be_lower_type(t_float, &out_cached);
    if (err != GCC_GEN_BE_SUCCESS || expected != out_cached) return __LINE__; // Should return exact same pointer

    // Test arrays
    a_type* t_arr = (a_type*)calloc(1, sizeof(a_type));
    t_arr->kind = tk_array;
    t_arr->variant.array.element_type = t_float;
    t_arr->variant.array.variant.number_of_elements = 10;
    err = gcc_gen_be_lower_type(t_arr, &out);
    if (err != GCC_GEN_BE_SUCCESS || out == NULL) return __LINE__;

    // Test VLA (pointer decay)
    a_type* t_vla = (a_type*)calloc(1, sizeof(a_type));
    t_vla->kind = tk_array;
    t_vla->variant.array.is_variable_size_array = TRUE;
    t_vla->variant.array.element_type = t_float;
    err = gcc_gen_be_lower_type(t_vla, &out);
    if (err != GCC_GEN_BE_SUCCESS || out == NULL) return __LINE__;

    // Test pointers
    a_type* t_ptr = (a_type*)calloc(1, sizeof(a_type));
    t_ptr->kind = tk_pointer;
    t_ptr->variant.pointer.type = t_float;
    err = gcc_gen_be_lower_type(t_ptr, &out);
    if (err != GCC_GEN_BE_SUCCESS || out == NULL) return __LINE__;

    a_type* t_void = (a_type*)calloc(1, sizeof(a_type));
    t_void->kind = tk_void;

    // Test structs and unions
    a_type* t_struct = (a_type*)calloc(1, sizeof(a_type));
    t_struct->kind = tk_struct;
    t_struct->source_corresp.name = "MyStruct";
    a_field* field1 = (a_field*)calloc(1, sizeof(a_field));
    field1->source_corresp.name = "f1";
    field1->type = t_int;
    t_struct->variant.class_struct_union.field_list = field1;
    err = gcc_gen_be_lower_type(t_struct, &out);
    if (err != GCC_GEN_BE_SUCCESS || out == NULL) return __LINE__;

    a_type* t_union = (a_type*)calloc(1, sizeof(a_type));
    t_union->kind = tk_union;
    t_union->source_corresp.name = "MyUnion";
    t_union->variant.class_struct_union.field_list = field1;
    err = gcc_gen_be_lower_type(t_union, &out);
    if (err != GCC_GEN_BE_SUCCESS || out == NULL) return __LINE__;

    // Test functions
    a_type* t_func = (a_type*)calloc(1, sizeof(a_type));
    t_func->kind = tk_routine;
    t_func->variant.routine.return_type = t_void;
    a_routine_type_supplement* supp = (a_routine_type_supplement*)calloc(1, sizeof(a_routine_type_supplement));
    t_func->variant.routine.extra_info = supp;
    err = gcc_gen_be_lower_type(t_func, &out);
    if (err != GCC_GEN_BE_SUCCESS || out == NULL) return __LINE__;

    // Test pointers to members
    a_type* t_ptrmem = (a_type*)calloc(1, sizeof(a_type));
    t_ptrmem->kind = tk_ptr_to_member;
    t_ptrmem->variant.ptr_to_member.type = t_int;
    err = gcc_gen_be_lower_type(t_ptrmem, &out);
    if (err != GCC_GEN_BE_SUCCESS || out == NULL) return __LINE__;

    // Test vectors
#if GNU_VECTOR_TYPES_ALLOWED
    a_type* t_vec = (a_type*)calloc(1, sizeof(a_type));
    t_vec->kind = tk_vector;
    t_vec->variant.vector.element_type = t_float;
    t_vec->variant.vector.number_of_elements = (an_expr_node*)calloc(1, sizeof(an_expr_node));
    t_vec->variant.vector.number_of_elements->variant.integer_value = 4;
    mock_called = 0;
    err = gcc_gen_be_lower_type(t_vec, &out);
    if (err != GCC_GEN_BE_SUCCESS || mock_called != 2) return __LINE__;
#endif

    gcc_jit_context_release(ctx);
    printf("PASS\n");
    return 0;
}