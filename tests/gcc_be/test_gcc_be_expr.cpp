/**
 * @file test_gcc_be_expr.cpp
 * @brief Unit test harness and deterministic mock layer for `src/gcc_gen_be_expr.c`.
 * @details Implements configurable mock hooks for type, ABI, declaration, statement,
 * coroutine, exception, RTTI, and C11 atomic dependencies, and drives the complete
 * expression lowering test suite (`run_expr_unit_tests`, `run_expr_lval_and_init_tests`,
 * `run_expr_rval_part1_tests`, and `run_expr_rval_part2_tests`).
 * @ingroup gcc_gen_be_tests
 */

#include "test_gcc_be_expr_common.h"
#include "gcc_gen_be_atomic.h"
#include "gcc_gen_be_rtti.h"
#include "gcc_gen_be_lib_loader.h"
#include "const_ints.h"
#include "float_pt.h"
#include <cstdio>
#include <cstring>

extern "C" {
/**
 * @brief Mock diagnostic reporter for expression unit tests.
 * @details Steps the OOM budget and returns `err` without terminating the test runner.
 * @param[in] err Error code reported by expression lowering.
 * @param[in] msg Optional human-readable context message.
 * @return The input `err` (or `GCC_GEN_BE_ERROR_OOM` if OOM countdown triggers).
 */
gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t err, const char *msg) GCC_GEN_BE_NOEXCEPT {
    (void)msg;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
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
/** @brief Innermost function scope pointer. */
a_scope_ptr innermost_function_scope = NULL;

/** @brief Shared mock state instance for expression unit tests. */
expr_mock_state_t g_expr_mock;

/** @brief Monotonically increasing block counter for `expr_test_fresh_block`. */
static unsigned long g_fresh_blk_seq = 0;

/** @brief Monotonically increasing struct tag counter for mock struct lowering. */
static unsigned long g_mock_struct_seq = 0;

/**
 * @brief Stub for computing type size in expression unit tests.
 * @details Returns `tp->size` directly when `tp` is non-NULL, or `0`.
 * @param[in] tp Frontend type pointer.
 * @return Byte size of `tp`.
 */
a_targ_size_t f_size_of_type(a_type_ptr tp) {
    return tp ? tp->size : 0;
}

/**
 * @brief Stub for extracting host floating-point constant values in expression unit tests.
 * @details Returns `g_expr_mock.host_fp_val`.
 * @param[in] kind Frontend float kind.
 * @param[in] val Internal floating-point storage pointer.
 * @return Configured host floating-point value.
 */
a_host_fp_value fetch_host_fp_value(a_float_kind kind, an_internal_float_value *val) {
    (void)kind;
    (void)val;
    return static_cast<a_host_fp_value>(g_expr_mock.host_fp_val);
}

/**
 * @brief Stub for checking integer constant signedness in expression unit tests.
 * @details Returns `g_expr_mock.int_is_signed`.
 * @param[in] cp Frontend constant pointer.
 * @return `TRUE` when signed, `FALSE` when unsigned.
 */
a_boolean int_constant_is_signed(a_constant_ptr cp) {
    (void)cp;
    return g_expr_mock.int_is_signed;
}

/**
 * @brief Stub for creating an integer bitmask in expression unit tests.
 * @details Zeroes `*mask` and resets `g_expr_mock.shifted_128 = FALSE`.
 * @param[out] mask Destination integer value mask.
 * @param[in] bits Bit width of the mask.
 */
void make_integer_value_mask(an_integer_value *mask, unsigned long bits) {
    (void)bits;
    g_expr_mock.shifted_128 = FALSE;
    if (mask) std::memset(mask, 0, sizeof(*mask));
}

#if !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER
/**
 * @brief Stub for bitwise AND on multi-part EDG integer values in expression unit tests.
 * @details No-op stub satisfying linkage when `INTEGER_VALUE_REPR_IS_A_HOST_INTEGER` is 0.
 * @param[in,out] op_1 Destination and first operand pointer.
 * @param[in] op_2 Second operand pointer.
 */
void and_integer_values(
    [[maybe_unused]] an_integer_value *op_1,
    [[maybe_unused]] an_integer_value *op_2) {
}
#endif /* !INTEGER_VALUE_REPR_IS_A_HOST_INTEGER */

/**
 * @brief Stub for right-shifting an EDG integer value in expression unit tests.
 * @details Marks `g_expr_mock.shifted_128 = TRUE` so subsequent extraction yields `int128_hi`.
 * @param[in,out] val Integer value storage.
 * @param[in] shift Number of bits to shift right.
 * @param[in] is_signed Signedness flag.
 * @param[in] sign_extend Sign-extension flag.
 */
void shift_right_integer_value(an_integer_value *val, int shift, a_boolean is_signed, a_boolean sign_extend) {
    (void)val;
    (void)shift;
    (void)is_signed;
    (void)sign_extend;
    g_expr_mock.shifted_128 = TRUE;
}

/**
 * @brief Stub for extracting an unsigned 64-bit chunk from an EDG integer value.
 * @details Returns `g_expr_mock.int128_hi` after a right shift, otherwise `g_expr_mock.int128_lo`.
 * @param[in] val Integer value storage.
 * @param[in] is_signed Signedness flag.
 * @param[out] ovflo Overflow output flag.
 * @return Extracted 64-bit unsigned chunk.
 */
a_host_large_unsigned unsigned_value_of_integer_value(an_integer_value *val, a_boolean is_signed, a_boolean *ovflo) {
    (void)val;
    (void)is_signed;
    if (ovflo) *ovflo = FALSE;
    if (g_expr_mock.shifted_128) {
        g_expr_mock.shifted_128 = FALSE;
        return g_expr_mock.int128_lo;
    }
    return g_expr_mock.int128_hi;
}

/**
 * @brief Stub for extracting a signed integer constant in expression unit tests.
 * @details Checks `g_expr_mock.const_ints` first, falling back to `g_expr_mock.signed_int_val`.
 * @param[in] cp Frontend constant pointer.
 * @param[out] err Overflow output flag.
 * @return Configured signed integer constant value.
 */
a_host_large_integer value_of_integer_constant(a_constant_ptr cp, a_boolean *err) {
    for (int i = 0; i < g_expr_mock.num_const_ints; ++i) {
        if (g_expr_mock.const_ints[i].con == cp) {
            if (err) *err = FALSE;
            return g_expr_mock.const_ints[i].val;
        }
    }
    if (err) *err = g_expr_mock.signed_int_ovf;
    return g_expr_mock.signed_int_val;
}

/**
 * @brief Stub for extracting an unsigned integer constant in expression unit tests.
 * @details Checks `g_expr_mock.const_ints` first, falling back to `g_expr_mock.unsigned_int_val`.
 * @param[in] cp Frontend constant pointer.
 * @param[out] err Overflow output flag.
 * @return Configured unsigned integer constant value.
 */
a_host_large_unsigned unsigned_value_of_integer_constant(a_constant_ptr cp, a_boolean *err) {
    for (int i = 0; i < g_expr_mock.num_const_ints; ++i) {
        if (g_expr_mock.const_ints[i].con == cp) {
            if (err) *err = FALSE;
            return (a_host_large_unsigned)g_expr_mock.const_ints[i].val;
        }
    }
    if (err) *err = g_expr_mock.unsigned_int_ovf;
    return g_expr_mock.unsigned_int_val;
}

/**
 * @brief Stub for looking up the definition scope of a routine in expression unit tests.
 * @details Returns `g_expr_mock.mock_routine_scope`.
 * @param[in] rout Frontend routine pointer.
 * @return Configured `a_scope_ptr` or `NULL`.
 */
a_scope_ptr scope_for_routine_or_null(a_routine_ptr rout) {
    (void)rout;
    return g_expr_mock.mock_routine_scope;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_lower_type`.
 * @details Honors OOM budgets, explicit error/override tables, type caches, and lowers
 * integer, float, complex, pointer, array, vector, pointer-to-member, and struct/union types.
 * @param[in] tp Frontend type pointer.
 * @param[out] out_type Receives the lowered `gcc_jit_type *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or a configured/OOM error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_type(a_type_ptr tp, struct gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
    if (!out_type) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_expr_mock.force_lower_type_err != GCC_GEN_BE_SUCCESS) {
        return g_expr_mock.force_lower_type_err;
    }
    if (!tp) return GCC_GEN_BE_SUCCESS;

    for (int i = 0; i < g_expr_mock.num_type_overrides; ++i) {
        if (g_expr_mock.type_overrides[i].edg_type == tp) {
            *out_type = g_expr_mock.type_overrides[i].jit_type;
            return GCC_GEN_BE_SUCCESS;
        }
    }

    a_type_ptr ut = tp;
    while (ut && ut->kind == tk_typeref) {
        ut = ut->variant.typeref.type;
        for (int i = 0; i < g_expr_mock.num_type_overrides; ++i) {
            if (g_expr_mock.type_overrides[i].edg_type == ut) {
                *out_type = g_expr_mock.type_overrides[i].jit_type;
                return GCC_GEN_BE_SUCCESS;
            }
        }
    }
    if (!ut) return GCC_GEN_BE_SUCCESS;

    void *cached = NULL;
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_TYPE, ut, &cached));
    if (cached) {
        *out_type = (gcc_jit_type *)cached;
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_SUCCESS;

    switch (ut->kind) {
        case tk_void:
            *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
            return GCC_GEN_BE_SUCCESS;
        case tk_nullptr:
            *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
            return GCC_GEN_BE_SUCCESS;
        case tk_integer: {
            if (ut->variant.integer.bool_type) {
                *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
                return GCC_GEN_BE_SUCCESS;
            }
            an_integer_kind ik = ut->variant.integer.int_kind;
            a_boolean is_unsigned = (
                ik == ik_unsigned_char || ik == ik_unsigned_short ||
                ik == ik_unsigned_int || ik == ik_unsigned_long ||
                ik == ik_unsigned_long_long || ik == ik_unsigned_int128 ||
                ik == ik_unsigned_bit_precise);
            if (ik == ik_char) {
                *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
            } else if (ik == ik_signed_char || ut->size == 1) {
                *out_type = gcc_jit_context_get_type(
                    ctx, is_unsigned ? GCC_JIT_TYPE_UNSIGNED_CHAR : GCC_JIT_TYPE_SIGNED_CHAR);
            } else if (ik == ik_short || ik == ik_unsigned_short || ut->size == 2) {
                *out_type = gcc_jit_context_get_type(
                    ctx, is_unsigned ? GCC_JIT_TYPE_UNSIGNED_SHORT : GCC_JIT_TYPE_SHORT);
            } else if (ik == ik_long) {
                *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
            } else if (ik == ik_unsigned_long) {
                *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG);
            } else if (ik == ik_long_long) {
                *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_LONG);
            } else if (ik == ik_unsigned_long_long || ut->size == 8) {
                *out_type = gcc_jit_context_get_type(
                    ctx, is_unsigned ? GCC_JIT_TYPE_UNSIGNED_LONG_LONG : GCC_JIT_TYPE_LONG_LONG);
            } else if (ik == ik_int128 || ik == ik_unsigned_int128 || ut->size == 16) {
                *out_type = gcc_jit_context_get_int_type(ctx, 16, !is_unsigned);
            } else {
                *out_type = gcc_jit_context_get_type(
                    ctx, is_unsigned ? GCC_JIT_TYPE_UNSIGNED_INT : GCC_JIT_TYPE_INT);
            }
            return GCC_GEN_BE_SUCCESS;
        }
        case tk_float: {
            a_float_kind fk = ut->variant.float_kind;
            if (fk == fk_float) {
                *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
            } else if (fk == fk_long_double || fk == fk_float80 || fk == fk_float128 || fk == fk_std_float128) {
                *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
            } else {
                *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
            }
            return GCC_GEN_BE_SUCCESS;
        }
        case tk_complex: {
            if (g_expr_mock.complex_as_struct && g_expr_mock.cplx_struct_type) {
                *out_type = g_expr_mock.cplx_struct_type;
                return GCC_GEN_BE_SUCCESS;
            }
            a_float_kind fk = ut->variant.float_kind;
            if (fk == fk_float) {
                *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_COMPLEX_FLOAT);
            } else if (fk == fk_long_double) {
                *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_COMPLEX_LONG_DOUBLE);
            } else {
                *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_COMPLEX_DOUBLE);
            }
            return GCC_GEN_BE_SUCCESS;
        }
        case tk_pointer: {
            a_type_ptr pt_edg = ut->variant.pointer.type;
            while (pt_edg && pt_edg->kind == tk_typeref) pt_edg = pt_edg->variant.typeref.type;
            if (!pt_edg || pt_edg->kind == tk_void || pt_edg->kind == tk_routine) {
                *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
                return GCC_GEN_BE_SUCCESS;
            }
            gcc_jit_type *pt_jit = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(pt_edg, &pt_jit));
            if (!pt_jit || pt_jit == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
                *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
            } else {
                *out_type = gcc_jit_type_get_pointer(pt_jit);
            }
            return GCC_GEN_BE_SUCCESS;
        }
        case tk_array: {
            gcc_jit_type *elem_jit = NULL;
            if (ut->variant.array.element_type) {
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(ut->variant.array.element_type, &elem_jit));
            }
            if (!elem_jit) elem_jit = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
            int n = (int)ut->variant.array.variant.number_of_elements;
            if (n <= 0) n = 1;
            *out_type = gcc_jit_context_new_array_type(ctx, NULL, elem_jit, n);
            return GCC_GEN_BE_SUCCESS;
        }
        case tk_vector: {
            gcc_jit_type *elem_jit = NULL;
            a_type_ptr etp = ut->variant.vector.element_type;
            if (etp) {
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(etp, &elem_jit));
            }
            if (!elem_jit) elem_jit = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
            size_t lanes = (etp && etp->size > 0 && ut->size >= etp->size)
                ? (size_t)(ut->size / etp->size)
                : 4U;
            if (lanes == 0) lanes = 4U;
            *out_type = gcc_jit_type_get_vector(elem_jit, lanes);
            return GCC_GEN_BE_SUCCESS;
        }
        case tk_ptr_to_member: {
            a_type_ptr mtp = ut->variant.ptr_to_member.type;
            while (mtp && mtp->kind == tk_typeref) mtp = mtp->variant.typeref.type;
            if (mtp && mtp->kind == tk_routine && g_expr_mock.pmf_struct_type) {
                *out_type = g_expr_mock.pmf_struct_type;
            } else {
                *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
            }
            return GCC_GEN_BE_SUCCESS;
        }
        case tk_struct:
        case tk_class:
        case tk_union: {
            gcc_jit_field *fields[32];
            int nf = 0;
            if (ut->kind != tk_union &&
                ut->variant.class_struct_union.extra_info &&
                ut->variant.class_struct_union.extra_info->base_classes) {
                for (a_base_class_ptr bc = ut->variant.class_struct_union.extra_info->base_classes;
                     bc != NULL && nf < 30;
                     bc = bc->next) {
                    if (bc->is_virtual || bc->direct == FALSE || bc->is_optimized_empty_base) continue;
                    gcc_jit_type *bt = NULL;
                    if (bc->type) {
                        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(bc->type, &bt));
                    }
                    if (!bt) bt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                    char bname[32];
                    std::snprintf(bname, sizeof(bname), "_base_%d", nf);
                    gcc_jit_field *bf = gcc_jit_context_new_field(ctx, NULL, bt, bname);
                    fields[nf++] = bf;
                    if (!g_expr_mock.skip_auto_cache_fields) {
                        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, bc, bf));
                    }
                }
            }
            for (a_field_ptr f = ut->variant.class_struct_union.field_list;
                 f != NULL && nf < 30;
                 f = f->next) {
                if (f->is_bit_field) continue;
                gcc_jit_type *ft = NULL;
                if (f->type) {
                    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(f->type, &ft));
                }
                if (!ft || ft == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
                    ft = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                }
                char fname[32];
                std::snprintf(fname, sizeof(fname), "_fld_%d", nf);
                gcc_jit_field *jf = gcc_jit_context_new_field(ctx, NULL, ft, fname);
                fields[nf++] = jf;
                if (!g_expr_mock.skip_auto_cache_fields) {
                    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, f, jf));
                }
            }
            if (nf == 0 && ut->size > 0) {
                gcc_jit_type *arr_t = gcc_jit_context_new_array_type(
                    ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR), (int)ut->size);
                fields[nf++] = gcc_jit_context_new_field(ctx, NULL, arr_t, "_storage");
            }
            char sname[48];
            std::snprintf(sname, sizeof(sname), "_mock_rec_%lu", ++g_mock_struct_seq);
            gcc_jit_type *rec_t = NULL;
            if (ut->kind == tk_union) {
                rec_t = gcc_jit_context_new_union_type(ctx, NULL, sname, nf, fields);
            } else {
                gcc_jit_struct *st = gcc_jit_context_new_struct_type(ctx, NULL, sname, nf, fields);
                rec_t = gcc_jit_struct_as_type(st);
            }
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, ut, rec_t));
            *out_type = rec_t;
            return GCC_GEN_BE_SUCCESS;
        }
        default:
            *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
            return GCC_GEN_BE_SUCCESS;
    }
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_get_zero_rvalue`.
 * @details Produces type-matched zero/null/struct/array/union/vector rvalues in `ctx`.
 * @param[in] edg_type Frontend type pointer.
 * @param[in] jit_type Lowered `libgccjit` type pointer.
 * @param[out] out_rval Receives the zero `gcc_jit_rvalue *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_zero_rvalue(
    a_type_ptr edg_type,
    struct gcc_jit_type *jit_type,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_expr_mock.force_zero_err != GCC_GEN_BE_SUCCESS) return g_expr_mock.force_zero_err;
    if (g_expr_mock.force_zero_null) return GCC_GEN_BE_SUCCESS;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_SUCCESS;

    gcc_jit_type *t = jit_type;
    if (!t && edg_type) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(edg_type, &t));
    }
    if (!t || t == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
        return GCC_GEN_BE_SUCCESS;
    }
    if (gcc_jit_type_is_pointer(t) != NULL) {
        *out_rval = gcc_jit_context_null(ctx, t);
        return GCC_GEN_BE_SUCCESS;
    }
    if (gcc_jit_type_is_struct(t) != NULL) {
        *out_rval = gcc_jit_context_new_struct_constructor(ctx, NULL, t, 0, NULL, NULL);
        return GCC_GEN_BE_SUCCESS;
    }
    if (t == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT) ||
        t == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE) ||
        t == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE)) {
        *out_rval = gcc_jit_context_new_rvalue_from_double(ctx, t, 0.0);
        return GCC_GEN_BE_SUCCESS;
    }
    if (gcc_jit_type_is_integral(t)) {
        *out_rval = gcc_jit_context_zero(ctx, t);
        return GCC_GEN_BE_SUCCESS;
    }
    a_type_ptr ut = edg_type;
    while (ut && ut->kind == tk_typeref) ut = ut->variant.typeref.type;
    if (ut && ut->kind == tk_array) {
        *out_rval = gcc_jit_context_new_array_constructor(ctx, NULL, t, 0, NULL);
        return GCC_GEN_BE_SUCCESS;
    }
    if (ut && ut->kind == tk_union) {
        *out_rval = gcc_jit_context_new_union_constructor(ctx, NULL, t, NULL, NULL);
        return GCC_GEN_BE_SUCCESS;
    }
    if (ut && ut->kind == tk_vector) {
        gcc_jit_type *lt = NULL;
        if (ut->variant.vector.element_type) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(ut->variant.vector.element_type, &lt));
        }
        if (!lt) lt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
        size_t lanes = (ut->variant.vector.element_type && ut->variant.vector.element_type->size > 0 &&
                        ut->size >= ut->variant.vector.element_type->size)
            ? (size_t)(ut->size / ut->variant.vector.element_type->size)
            : 4U;
        if (lanes == 0 || lanes > 16U) lanes = 4U;
        gcc_jit_rvalue *z = gcc_jit_type_is_integral(lt)
            ? gcc_jit_context_zero(ctx, lt)
            : gcc_jit_context_new_rvalue_from_double(ctx, lt, 0.0);
        gcc_jit_rvalue *elems[16];
        for (size_t i = 0; i < lanes; ++i) elems[i] = z;
        *out_rval = gcc_jit_context_new_rvalue_from_vector(ctx, NULL, t, lanes, elems);
        return GCC_GEN_BE_SUCCESS;
    }
    if (t == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_COMPLEX_FLOAT) ||
        t == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_COMPLEX_DOUBLE) ||
        t == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_COMPLEX_LONG_DOUBLE)) {
        *out_rval = gcc_jit_context_new_cast(
            ctx, NULL, gcc_jit_context_new_rvalue_from_double(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE), 0.0), t);
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (cblock) {
        gcc_jit_function *fn = gcc_jit_block_get_function(cblock);
        gcc_jit_lvalue *zv = gcc_jit_function_new_local(fn, NULL, t, "zero_tmp");
        *out_rval = gcc_jit_lvalue_as_rvalue(zv);
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_normalize_bitint_rvalue`.
 * @details Steps the OOM budget and forwards `src_val` to `*out_rval`.
 * @param[in] tp Frontend type pointer.
 * @param[in] src_val Input `libgccjit` rvalue.
 * @param[out] out_rval Output `libgccjit` rvalue pointer.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_normalize_bitint_rvalue(
    a_type_ptr tp,
    struct gcc_jit_rvalue *src_val,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)tp;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_rval = src_val;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_lower_source_location_builtin`.
 * @details Emits an integer literal for LINE/COLUMN or a string literal otherwise.
 * @param[in] name Source-location builtin name.
 * @param[in] call_pos Call-site source position.
 * @param[in] result_type Expected frontend result type.
 * @param[out] out_rval Output `libgccjit` rvalue pointer.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_source_location_builtin(
    const char *name,
    const a_source_position *call_pos,
    a_type_ptr result_type,
    gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)call_pos;
    if (!name || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;
    if (std::strstr(name, "LINE") != NULL || std::strstr(name, "COLUMN") != NULL) {
        gcc_jit_type *rt = NULL;
        if (result_type) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(result_type, &rt));
        }
        if (!rt) rt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
        *out_rval = gcc_jit_context_new_rvalue_from_int(ctx, rt, 42);
    } else {
        *out_rval = gcc_jit_context_new_string_literal(ctx, "expr_test.cpp");
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_lower_global_variable_decl`.
 * @details Returns `g_expr_mock.mock_global_lval`, `NULL` when configured, or creates an imported global.
 * @param[in] var Frontend variable pointer.
 * @param[out] out_lval Output global `gcc_jit_lvalue *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(
    a_variable_ptr var,
    struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_lval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_expr_mock.force_global_decl_err != GCC_GEN_BE_SUCCESS) {
        return g_expr_mock.force_global_decl_err;
    }
    if (g_expr_mock.global_var_return_null) {
        return GCC_GEN_BE_SUCCESS;
    }
    if (g_expr_mock.mock_global_lval) {
        *out_lval = g_expr_mock.mock_global_lval;
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_SUCCESS;
    gcc_jit_type *vt = NULL;
    if (var && var->type) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(var->type, &vt));
    }
    if (!vt || vt == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
        vt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    }
    const char *nm = (var && var->source_corresp.name) ? var->source_corresp.name : "mock_glob";
    gcc_jit_lvalue *gv = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_IMPORTED, vt, nm);
    if (var) {
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var, gv));
    }
    *out_lval = gv;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_lower_local_variable_decl`.
 * @details Returns `g_expr_mock.mock_local_lval`, `NULL` when configured, or allocates a local in `func`.
 * @param[in] func Owning `gcc_jit_function *`.
 * @param[in] var Frontend variable pointer.
 * @param[out] out_lval Output local `gcc_jit_lvalue *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_local_variable_decl(
    struct gcc_jit_function *func,
    a_variable_ptr var,
    struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    if (!out_lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_lval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_expr_mock.local_var_return_null) {
        return GCC_GEN_BE_SUCCESS;
    }
    if (g_expr_mock.mock_local_lval) {
        *out_lval = g_expr_mock.mock_local_lval;
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx || !func) return GCC_GEN_BE_SUCCESS;
    gcc_jit_type *vt = NULL;
    if (var && var->type) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(var->type, &vt));
    }
    if (!vt || vt == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
        vt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    }
    const char *nm = (var && var->source_corresp.name) ? var->source_corresp.name : "mock_loc";
    gcc_jit_lvalue *lv = gcc_jit_function_new_local(func, NULL, vt, nm);
    if (var) {
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, var, lv));
    }
    *out_lval = lv;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_lower_function_decl`.
 * @details Returns `mock_decl_func` / `mock_decl_func2` or synthesizes an imported `gcc_jit_function` from `rout`.
 * @param[in] rout Frontend routine pointer.
 * @param[out] out_func Output `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_function_decl(
    a_routine_ptr rout,
    struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
    if (!out_func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_func = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_expr_mock.force_func_decl_err != GCC_GEN_BE_SUCCESS) {
        return g_expr_mock.force_func_decl_err;
    }
    if (g_expr_mock.func_decl_return_null) {
        return GCC_GEN_BE_SUCCESS;
    }
    int call_idx = g_expr_mock.func_decl_calls++;
    if (call_idx == 1 && g_expr_mock.mock_decl_func2) {
        *out_func = g_expr_mock.mock_decl_func2;
        return GCC_GEN_BE_SUCCESS;
    }
    if (g_expr_mock.mock_decl_func) {
        *out_func = g_expr_mock.mock_decl_func;
        return GCC_GEN_BE_SUCCESS;
    }
    if (rout) {
        void *cached = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, rout, &cached));
        if (cached) {
            *out_func = (gcc_jit_function *)cached;
            return GCC_GEN_BE_SUCCESS;
        }
    }
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_SUCCESS;

    gcc_jit_type *ret_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    gcc_jit_param *params[16];
    int np = 0;
    int is_var = 0;
    if (rout && rout->type && rout->type->kind == tk_routine) {
        if (rout->type->variant.routine.return_type) {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(rout->type->variant.routine.return_type, &ret_t));
            if (!ret_t) ret_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
        }
        a_routine_type_supplement_ptr rtsp = rout->type->variant.routine.extra_info;
        if (rtsp) {
            is_var = rtsp->has_ellipsis ? 1 : 0;
            for (a_param_type_ptr pt = rtsp->param_type_list; pt != NULL && np < 14; pt = pt->next) {
                gcc_jit_type *pjt = NULL;
                if (pt->type) {
                    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(pt->type, &pjt));
                }
                if (!pjt || pjt == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
                    pjt = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
                }
                char pname[24];
                std::snprintf(pname, sizeof(pname), "p%d", np);
                params[np++] = gcc_jit_context_new_param(ctx, NULL, pjt, pname);
            }
        }
    } else if (rout) {
        params[np++] = gcc_jit_context_new_param(
            ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR), "self");
    }
    const char *fnm = (rout && rout->source_corresp.name) ? rout->source_corresp.name : "mock_fn";
    gcc_jit_function *fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, ret_t, fnm, np, params, is_var);
    if (rout) {
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, rout, fn));
    }
    *out_func = fn;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_lower_statement`.
 * @details Evaluates `stmk_expr` expressions (writing into `stmt_expr_result_lval` if active).
 * @param[in] stmt Frontend statement pointer.
 * @param[in] func Active `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_statement(
    a_statement_ptr stmt,
    struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (stmt && stmt->kind == stmk_expr && stmt->expr) {
        gcc_jit_rvalue *rv = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(stmt->expr, &rv));
        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        if (rv && cblock && gcc_gen_be_singleton_state.stmt_expr_result_lval) {
            gcc_jit_block_add_assignment(
                cblock, NULL, gcc_gen_be_singleton_state.stmt_expr_result_lval, rv);
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_get_or_create_label_block`.
 * @details Returns `g_expr_mock.mock_label_block`, `NULL` when configured, or creates a new block in `func`.
 * @param[in] label Frontend label pointer.
 * @param[in] func Owning `gcc_jit_function *`.
 * @param[out] out_block Output `gcc_jit_block *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_or_create_label_block(
    a_label_ptr label,
    struct gcc_jit_function *func,
    struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    (void)label;
    if (!out_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_block = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_expr_mock.label_return_null) return GCC_GEN_BE_SUCCESS;
    if (g_expr_mock.mock_label_block) {
        *out_block = g_expr_mock.mock_label_block;
        return GCC_GEN_BE_SUCCESS;
    }
    if (func) {
        *out_block = gcc_jit_function_new_block(func, "mock_lbl_blk");
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_get_active_coroutine_frame`.
 * @details Returns `g_expr_mock.coro_frame_type` and `g_expr_mock.coro_frame_lval`.
 * @param[out] out_frame_type Receives active coroutine frame type.
 * @param[out] out_frame_lval Receives active coroutine frame pointer lvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_active_coroutine_frame(
    struct gcc_jit_type **out_frame_type,
    struct gcc_jit_lvalue **out_frame_lval) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (out_frame_type) *out_frame_type = g_expr_mock.coro_frame_type;
    if (out_frame_lval) *out_frame_lval = g_expr_mock.coro_frame_lval;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_register_coro_suspend_point`.
 * @details Populates configured coroutine suspend index, state field lvalue, and frame pointer rvalue.
 * @param[in] resume_block Coroutine resume basic block.
 * @param[out] out_state_idx Receives suspend state index.
 * @param[out] out_state_field_lval Receives suspend state field lvalue.
 * @param[out] out_frame_ptr_rval Receives coroutine frame pointer rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_register_coro_suspend_point(
    struct gcc_jit_block *resume_block,
    int *out_state_idx,
    struct gcc_jit_lvalue **out_state_field_lval,
    struct gcc_jit_rvalue **out_frame_ptr_rval) GCC_GEN_BE_NOEXCEPT {
    (void)resume_block;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (out_state_idx) *out_state_idx = g_expr_mock.coro_suspend_idx;
    if (out_state_field_lval) *out_state_field_lval = g_expr_mock.coro_state_field_lval;
    if (out_frame_ptr_rval) *out_frame_ptr_rval = g_expr_mock.coro_frame_ptr_rval;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_emit_coro_symmetric_transfer`.
 * @details Terminates `suspend_block` with `void` return after stepping the OOM budget.
 * @param[in] func Active `gcc_jit_function *`.
 * @param[in] suspend_block Suspend basic block to terminate.
 * @param[in] handle_rval Target `std::coroutine_handle<>` rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_coro_symmetric_transfer(
    struct gcc_jit_function *func,
    struct gcc_jit_block *suspend_block,
    struct gcc_jit_rvalue *handle_rval) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    (void)handle_rval;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (suspend_block) gcc_jit_block_end_with_void_return(suspend_block, NULL);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_emit_scope_unwind_cleanup`.
 * @details Steps the OOM budget and returns `GCC_GEN_BE_SUCCESS`.
 * @param[in] func Active `gcc_jit_function *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_scope_unwind_cleanup(
    struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_lower_builtin_call`.
 * @details Returns `g_expr_mock.force_builtin_err` or `g_expr_mock.mock_builtin_rval`.
 * @param[in] rout Builtin routine descriptor.
 * @param[in] num_args Number of lowered arguments.
 * @param[in] args Lowered argument rvalue array.
 * @param[out] out_rval Output `gcc_jit_rvalue *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or a configured/OOM error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_builtin_call(
    a_routine_ptr rout,
    int num_args,
    struct gcc_jit_rvalue **args,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)rout;
    (void)num_args;
    (void)args;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_expr_mock.force_builtin_err != GCC_GEN_BE_SUCCESS) {
        return g_expr_mock.force_builtin_err;
    }
    *out_rval = g_expr_mock.mock_builtin_rval;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_get_target_abi`.
 * @details Outputs `g_expr_mock.target_abi`.
 * @param[out] out_abi Receives the configured `gcc_gen_be_target_abi_t`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_target_abi(gcc_gen_be_target_abi_t *out_abi) GCC_GEN_BE_NOEXCEPT {
    if (!out_abi) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_abi = g_expr_mock.target_abi;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_classify_aapcs64`.
 * @details Steps OOM budget and reports `FALSE` unless `g_expr_mock.va_num_fp > 0`.
 * @param[in] type Frontend type pointer.
 * @param[out] out_is_hfa Pointer receiving `TRUE` when HFA/HVA.
 * @return `GCC_GEN_BE_SUCCESS` or error code.
 */
gcc_gen_be_error_t gcc_gen_be_classify_aapcs64(a_type_ptr type, a_boolean *out_is_hfa) GCC_GEN_BE_NOEXCEPT {
    if (!type || !out_is_hfa) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_is_hfa = (g_expr_mock.va_num_fp > 0) ? TRUE : FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_build_vector_rvalue`.
 * @details Steps OOM budget and produces a zero/vector rvalue of `vec_type`.
 * @param[in] vec_type Lowered vector `gcc_jit_type`.
 * @param[in] lane_type Lowered lane `gcc_jit_type`.
 * @param[in] num_lanes Number of scalar lanes.
 * @param[in] elements Array of scalar lane rvalues.
 * @param[out] out_vec Pointer receiving the constructed vector rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_build_vector_rvalue(
    struct gcc_jit_type *vec_type,
    struct gcc_jit_type *lane_type,
    size_t num_lanes,
    struct gcc_jit_rvalue **elements,
    struct gcc_jit_rvalue **out_vec) GCC_GEN_BE_NOEXCEPT {
    (void)lane_type;
    if (!vec_type || num_lanes == 0 || !elements || !out_vec) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return gcc_gen_be_get_zero_rvalue(NULL, vec_type, out_vec);
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_emit_single_dtor_unwind_lp`.
 * @details Steps OOM budget and succeeds without mutating the mock block state.
 * @param[in] func Active JIT function.
 * @param[in] obj_lval Constructed object lvalue.
 * @param[in] dtor_fn Destructor function.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_single_dtor_unwind_lp(
    gcc_jit_function *func,
    struct gcc_jit_lvalue *obj_lval,
    gcc_jit_function *dtor_fn) GCC_GEN_BE_NOEXCEPT {
    if (!func || !obj_lval || !dtor_fn) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_classify_sysv_amd64`.
 * @details Populates `classes` with `g_expr_mock.sysv_class0` and `g_expr_mock.sysv_class1`.
 * @param[in] type Frontend aggregate type.
 * @param[out] classes Two-element SysV eightbyte classification output array.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_classify_sysv_amd64(
    a_type_ptr type,
    gcc_gen_be_abi_class_t classes[2]) GCC_GEN_BE_NOEXCEPT {
    (void)type;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (classes) {
        classes[0] = g_expr_mock.sysv_class0;
        classes[1] = g_expr_mock.sysv_class1;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_classify_va_arg`.
 * @details Populates `va_is_stack`, `va_num_gp`, and `va_num_fp` from `g_expr_mock`.
 * @param[in] type Frontend `va_arg` target type.
 * @param[out] out_is_stack Receives stack-passed flag.
 * @param[out] out_num_gp Receives GP register slot count.
 * @param[out] out_num_fp Receives FP register slot count.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_classify_va_arg(
    a_type_ptr type,
    a_boolean *out_is_stack,
    int *out_num_gp,
    int *out_num_fp) GCC_GEN_BE_NOEXCEPT {
    (void)type;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (out_is_stack) *out_is_stack = g_expr_mock.va_is_stack;
    if (out_num_gp) *out_num_gp = g_expr_mock.va_num_gp;
    if (out_num_fp) *out_num_fp = g_expr_mock.va_num_fp;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_is_returned_via_sret`.
 * @details Outputs `g_expr_mock.is_sret`.
 * @param[in] type Frontend return type.
 * @param[out] out_is_sret Receives `sret` flag.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_is_returned_via_sret(
    a_type_ptr type,
    a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT {
    (void)type;
    if (!out_is_sret) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_is_sret = g_expr_mock.is_sret;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_is_passed_by_invisible_reference`.
 * @details Returns `TRUE` when `invis_ref_all` is set or `type == invis_ref_tp`.
 * @param[in] type Frontend argument type.
 * @param[out] out_is_ref Receives invisible-reference flag.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(
    a_type_ptr type,
    a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT {
    if (!out_is_ref) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_is_ref = (g_expr_mock.invis_ref_all || (type && type == g_expr_mock.invis_ref_tp))
        ? TRUE
        : FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_coerce_rvalue`.
 * @details Returns `coerced_rval_override`, `NULL` when configured, or `src_rval`.
 * @param[in] func Active `gcc_jit_function *`.
 * @param[in] block Active `gcc_jit_block *`.
 * @param[in] src_rval Source `gcc_jit_rvalue *`.
 * @param[in] dst_type Destination `gcc_jit_type *`.
 * @param[out] out_rval Output coerced `gcc_jit_rvalue *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_coerce_rvalue(
    struct gcc_jit_function *func,
    struct gcc_jit_block *block,
    struct gcc_jit_rvalue *src_rval,
    struct gcc_jit_type *dst_type,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    (void)block;
    (void)dst_type;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_expr_mock.force_coerce_rval_err != GCC_GEN_BE_SUCCESS) {
        return g_expr_mock.force_coerce_rval_err;
    }
    if (g_expr_mock.coerce_rval_return_null) {
        *out_rval = NULL;
        return GCC_GEN_BE_SUCCESS;
    }
    *out_rval = g_expr_mock.coerced_rval_override ? g_expr_mock.coerced_rval_override : src_rval;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_get_coerced_abi_type`.
 * @details Outputs `coerced_abi_type` and `needs_abi_coercion` from `g_expr_mock`.
 * @param[in] type Frontend type pointer.
 * @param[out] out_coerced_type Receives coerced `gcc_jit_type *`.
 * @param[out] out_needs_coercion Receives coercion flag.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_coerced_abi_type(
    a_type_ptr type,
    struct gcc_jit_type **out_coerced_type,
    a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT {
    (void)type;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_expr_mock.force_get_coerced_abi_err != GCC_GEN_BE_SUCCESS) {
        return g_expr_mock.force_get_coerced_abi_err;
    }
    if (out_coerced_type) *out_coerced_type = g_expr_mock.coerced_abi_type;
    if (out_needs_coercion) *out_needs_coercion = g_expr_mock.needs_abi_coercion;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_coerce_Arg_to_registers`.
 * @details Returns `coerced_arg_rval`, `NULL` when configured, or `src_rval`.
 * @param[in] func Active `gcc_jit_function *`.
 * @param[in] block Active `gcc_jit_block *`.
 * @param[in] arg_type Frontend argument type.
 * @param[in] src_rval Source argument rvalue.
 * @param[out] out_rval Output register-coerced rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_coerce_Arg_to_registers(
    struct gcc_jit_function *func,
    struct gcc_jit_block *block,
    a_type_ptr arg_type,
    struct gcc_jit_rvalue *src_rval,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)func;
    (void)block;
    (void)arg_type;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_expr_mock.force_coerce_arg_err != GCC_GEN_BE_SUCCESS) {
        return g_expr_mock.force_coerce_arg_err;
    }
    if (g_expr_mock.coerce_arg_return_null) {
        *out_rval = NULL;
        return GCC_GEN_BE_SUCCESS;
    }
    *out_rval = g_expr_mock.coerced_arg_rval ? g_expr_mock.coerced_arg_rval : src_rval;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_emit_throw`.
 * @details Optionally creates a post-throw continuation block when `emit_throw_make_block` is `TRUE`.
 * @param[in] expr Frontend `enk_throw` expression node.
 * @param[out] out_block Receives optional post-throw unreachable block.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_throw(
    an_expr_node_ptr expr,
    struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    (void)expr;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (out_block) *out_block = NULL;
    if (g_expr_mock.emit_throw_make_block) {
        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        if (cblock) {
            gcc_jit_function *fn = gcc_jit_block_get_function(cblock);
            gcc_jit_block *nb = gcc_jit_function_new_block(fn, "after_throw_mock");
            gcc_jit_block_end_with_jump(cblock, NULL, nb);
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(nb));
            if (out_block) *out_block = nb;
        }
    } else {
        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        if (cblock) {
            gcc_jit_block_end_with_void_return(cblock, NULL);
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_emit_unwind_resume`.
 * @details Terminates the current block or hands back an open continuation block when configured.
 * @param[in] exc_ptr Exception pointer rvalue.
 * @param[out] out_block Receives optional continuation block.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_unwind_resume(
    struct gcc_jit_rvalue *exc_ptr,
    struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT {
    (void)exc_ptr;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (out_block) *out_block = NULL;
    gcc_jit_block *cblock = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
    if (cblock) {
        if (g_expr_mock.unwind_resume_make_block) {
            gcc_jit_function *fn = gcc_jit_block_get_function(cblock);
            gcc_jit_block *nb = gcc_jit_function_new_block(fn, "after_resume_mock");
            gcc_jit_block_end_with_jump(cblock, NULL, nb);
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(nb));
            if (out_block) *out_block = nb;
        } else {
            gcc_jit_block_end_with_void_return(cblock, NULL);
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_emit_invoke_call`.
 * @details Jumps from `block` to `normal_dest` (or conditionally to `unwind_dest`) and sets `*out_next_block`.
 * @param[in] ctx Active `gcc_jit_context *`.
 * @param[in] block Calling basic block.
 * @param[in] fn Direct callee function.
 * @param[in] num_args Number of call arguments.
 * @param[in] args Call argument rvalues.
 * @param[in] result_lval Optional destination lvalue for return value.
 * @param[in] normal_dest Normal continuation block.
 * @param[in] unwind_dest Unwind landing-pad block.
 * @param[out] out_next_block Receives `normal_dest`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_invoke_call(
    struct gcc_jit_context *ctx,
    struct gcc_jit_block *block,
    struct gcc_jit_function *fn,
    int num_args,
    struct gcc_jit_rvalue **args,
    struct gcc_jit_lvalue *result_lval,
    struct gcc_jit_block *normal_dest,
    struct gcc_jit_block *unwind_dest,
    struct gcc_jit_block **out_next_block) GCC_GEN_BE_NOEXCEPT {
    (void)fn;
    (void)num_args;
    (void)args;
    (void)result_lval;
    if (!out_next_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_expr_mock.force_invoke_err != GCC_GEN_BE_SUCCESS) return g_expr_mock.force_invoke_err;
    gcc_jit_block *dest = normal_dest;
    if (!dest && block) {
        dest = gcc_jit_function_new_block(gcc_jit_block_get_function(block), "inv_cont_mock");
    }
    if (block && dest) {
        if (ctx && unwind_dest) {
            gcc_jit_rvalue *cond = gcc_jit_context_new_rvalue_from_int(
                ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL), 1);
            gcc_jit_block_end_with_conditional(block, NULL, cond, dest, unwind_dest);
        } else {
            gcc_jit_block_end_with_jump(block, NULL, dest);
        }
        if (!normal_dest) {
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(dest));
        }
    }
    *out_next_block = dest ? dest : block;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_emit_invoke_call_through_ptr`.
 * @details Branches from `block` to `normal_dest` / `unwind_dest` and sets `*out_next_block`.
 * @param[in] ctx Active `gcc_jit_context *`.
 * @param[in] block Calling basic block.
 * @param[in] fn_ptr Indirect callee function pointer rvalue.
 * @param[in] num_args Number of call arguments.
 * @param[in] args Call argument rvalues.
 * @param[in] result_lval Optional destination lvalue for return value.
 * @param[in] normal_dest Normal continuation block.
 * @param[in] unwind_dest Unwind landing-pad block.
 * @param[out] out_next_block Receives `normal_dest`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_invoke_call_through_ptr(
    struct gcc_jit_context *ctx,
    struct gcc_jit_block *block,
    struct gcc_jit_rvalue *fn_ptr,
    int num_args,
    struct gcc_jit_rvalue **args,
    struct gcc_jit_lvalue *result_lval,
    struct gcc_jit_block *normal_dest,
    struct gcc_jit_block *unwind_dest,
    struct gcc_jit_block **out_next_block) GCC_GEN_BE_NOEXCEPT {
    (void)fn_ptr;
    (void)num_args;
    (void)args;
    (void)result_lval;
    if (!out_next_block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_expr_mock.force_invoke_ptr_err != GCC_GEN_BE_SUCCESS) return g_expr_mock.force_invoke_ptr_err;
    gcc_jit_block *dest = normal_dest;
    if (!dest && block) {
        dest = gcc_jit_function_new_block(gcc_jit_block_get_function(block), "inv_ptr_cont_mock");
    }
    if (block && dest) {
        if (ctx && unwind_dest) {
            gcc_jit_rvalue *cond = gcc_jit_context_new_rvalue_from_int(
                ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL), 1);
            gcc_jit_block_end_with_conditional(block, NULL, cond, dest, unwind_dest);
        } else {
            gcc_jit_block_end_with_jump(block, NULL, dest);
        }
        if (!normal_dest) {
            GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(dest));
        }
    }
    *out_next_block = dest ? dest : block;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_get_location`.
 * @details Steps the OOM budget and sets `*out_loc = NULL`.
 * @param[in] pos Frontend source position pointer.
 * @param[out] out_loc Output `gcc_jit_location *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_location(
    a_source_position *pos,
    struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    (void)pos;
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_loc = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_get_location_from_expr`.
 * @details Steps the OOM budget and sets `*out_loc = NULL`.
 * @param[in] expr Frontend expression pointer.
 * @param[out] out_loc Output `gcc_jit_location *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_get_location_from_expr(
    an_expr_node_ptr expr,
    struct gcc_jit_location **out_loc) GCC_GEN_BE_NOEXCEPT {
    (void)expr;
    if (!out_loc) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_loc = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_is_c11_atomic_type`.
 * @details Matches `atomic_target_tp`, `atomic_target_tp2`, or `TQ_C11_ATOMIC` typerefs.
 * @param[in] tp Frontend type pointer.
 * @param[out] out_is_atomic Receives `TRUE` if `tp` is atomic, else `FALSE`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_is_c11_atomic_type(
    a_type_ptr tp,
    a_boolean *out_is_atomic) GCC_GEN_BE_NOEXCEPT {
    if (!out_is_atomic) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_is_atomic = FALSE;
    if (!tp) return GCC_GEN_BE_SUCCESS;
    if (tp == g_expr_mock.atomic_target_tp || tp == g_expr_mock.atomic_target_tp2) {
        *out_is_atomic = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }
    for (a_type_ptr cur = tp; cur && cur->kind == tk_typeref; cur = cur->variant.typeref.type) {
        if ((cur->variant.typeref.qualifiers & TQ_C11_ATOMIC) != 0) {
            *out_is_atomic = TRUE;
            return GCC_GEN_BE_SUCCESS;
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_lower_c11_atomic_load`.
 * @details Emits a direct rvalue read from `addr_lval` (or zero rvalue fallback).
 * @param[in] expr_type Result frontend type.
 * @param[in] addr_lval Atomic object lvalue.
 * @param[out] out_rval Output loaded rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_c11_atomic_load(
    a_type_ptr expr_type,
    struct gcc_jit_lvalue *addr_lval,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (addr_lval) {
        *out_rval = gcc_jit_lvalue_as_rvalue(addr_lval);
        return GCC_GEN_BE_SUCCESS;
    }
    return gcc_gen_be_get_zero_rvalue(expr_type, NULL, out_rval);
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_lower_c11_atomic_store`.
 * @details Forwards `rhs_rval` to `*out_rval`.
 * @param[in] lhs_type Atomic destination type.
 * @param[in] addr_lval Atomic object lvalue.
 * @param[in] rhs_rval Value to store.
 * @param[out] out_rval Output stored rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_c11_atomic_store(
    a_type_ptr lhs_type,
    struct gcc_jit_lvalue *addr_lval,
    struct gcc_jit_rvalue *rhs_rval,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)lhs_type;
    (void)addr_lval;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_rval = rhs_rval;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_lower_c11_atomic_rmw`.
 * @details Returns `rhs_rval` when non-NULL or reads `addr_lval`.
 * @param[in] op Assignment or increment/decrement operator kind.
 * @param[in] lhs_type Atomic destination type.
 * @param[in] addr_lval Atomic object lvalue.
 * @param[in] rhs_rval Optional RHS operand rvalue.
 * @param[out] out_rval Output RMW result rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_c11_atomic_rmw(
    an_expr_operator_kind op,
    a_type_ptr lhs_type,
    struct gcc_jit_lvalue *addr_lval,
    struct gcc_jit_rvalue *rhs_rval,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)op;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (rhs_rval) {
        *out_rval = rhs_rval;
        return GCC_GEN_BE_SUCCESS;
    }
    if (addr_lval) {
        *out_rval = gcc_jit_lvalue_as_rvalue(addr_lval);
        return GCC_GEN_BE_SUCCESS;
    }
    return gcc_gen_be_get_zero_rvalue(lhs_type, NULL, out_rval);
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_lower_generic_atomic_call`.
 * @details Returns `g_expr_mock.force_builtin_err` or `g_expr_mock.mock_builtin_rval`.
 * @param[in] name Generic atomic builtin name.
 * @param[in] rout Builtin routine descriptor.
 * @param[in] num_args Number of lowered argument rvalues.
 * @param[in] args Lowered argument rvalue array.
 * @param[in] explicit_size_bytes Explicit object width in bytes.
 * @param[out] out_rval Output call result rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_generic_atomic_call(
    const char *name,
    a_routine_ptr rout,
    int num_args,
    struct gcc_jit_rvalue **args,
    unsigned long explicit_size_bytes,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)name;
    (void)rout;
    (void)num_args;
    (void)args;
    (void)explicit_size_bytes;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_expr_mock.force_builtin_err != GCC_GEN_BE_SUCCESS) {
        return g_expr_mock.force_builtin_err;
    }
    *out_rval = g_expr_mock.mock_builtin_rval;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_emit_vtable`.
 * @details Returns `mock_vtable_lval`, `NULL` when configured, or an imported `_ZTV_mock` array global.
 * @param[in] class_type Polymorphic class type.
 * @param[out] out_vtable Output vtable global lvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_vtable(
    a_type_ptr class_type,
    struct gcc_jit_lvalue **out_vtable) GCC_GEN_BE_NOEXCEPT {
    (void)class_type;
    if (!out_vtable) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_vtable = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_expr_mock.vtable_return_null) return GCC_GEN_BE_SUCCESS;
    if (g_expr_mock.mock_vtable_lval) {
        *out_vtable = g_expr_mock.mock_vtable_lval;
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_SUCCESS;
    gcc_jit_type *arr_t = gcc_jit_context_new_array_type(
        ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR), 8);
    *out_vtable = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_IMPORTED, arr_t, "_ZTV_mock");
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_emit_vtt`.
 * @details Returns `force_vtt_err`, `mock_vtt_lval`, `NULL` when configured, or an imported `_ZTT_mock` array.
 * @param[in] class_type Virtual-inheriting class type.
 * @param[out] out_vtt Output VTT global lvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_vtt(
    a_type_ptr class_type,
    struct gcc_jit_lvalue **out_vtt) GCC_GEN_BE_NOEXCEPT {
    (void)class_type;
    if (!out_vtt) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_vtt = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_expr_mock.force_vtt_err != GCC_GEN_BE_SUCCESS) return g_expr_mock.force_vtt_err;
    if (g_expr_mock.vtt_return_null) return GCC_GEN_BE_SUCCESS;
    if (g_expr_mock.mock_vtt_lval) {
        *out_vtt = g_expr_mock.mock_vtt_lval;
        return GCC_GEN_BE_SUCCESS;
    }
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_SUCCESS;
    gcc_jit_type *arr_t = gcc_jit_context_new_array_type(
        ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR), 4);
    *out_vtt = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_IMPORTED, arr_t, "_ZTT_mock");
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_lower_typeid`.
 * @details Returns `NULL` when `vtable_return_null` is set, else an imported `_ZTI_mock` address.
 * @param[in] expr Frontend `enk_typeid` expression node.
 * @param[out] out_rval Output `std::type_info *` rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_typeid(
    an_expr_node_ptr expr,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    (void)expr;
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (g_expr_mock.vtable_return_null) return GCC_GEN_BE_SUCCESS;
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_SUCCESS;
    gcc_jit_lvalue *ti = gcc_jit_context_new_global(
        ctx, NULL, GCC_JIT_GLOBAL_IMPORTED, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR), "_ZTI_mock");
    *out_rval = gcc_jit_lvalue_get_address(ti, NULL);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Deterministic mock implementation of `gcc_gen_be_lower_dynamic_cast`.
 * @details Emits a null pointer of `expr->type` after stepping the OOM budget.
 * @param[in] expr Frontend `eok_dynamic_cast` expression node.
 * @param[out] out_rval Output cast result rvalue.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_dynamic_cast(
    an_expr_node_ptr expr,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return gcc_gen_be_get_zero_rvalue(expr ? expr->type : NULL, NULL, out_rval);
}

gcc_gen_be_error_t expr_test_reset_mock(void) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_type *saved_cplx = g_expr_mock.cplx_struct_type;
    gcc_jit_type *saved_pmf = g_expr_mock.pmf_struct_type;
    gcc_jit_type *saved_one = g_expr_mock.one_field_struct_type;
    std::memset(&g_expr_mock, 0, sizeof(g_expr_mock));
    g_expr_mock.cplx_struct_type = saved_cplx;
    g_expr_mock.pmf_struct_type = saved_pmf;
    g_expr_mock.one_field_struct_type = saved_one;
    g_expr_mock.target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
    g_expr_mock.va_is_stack = TRUE;
    g_expr_mock.sysv_class0 = GCC_GEN_BE_ABI_CLASS_INTEGER;
    g_expr_mock.sysv_class1 = GCC_GEN_BE_ABI_CLASS_NO_CLASS;
    g_expr_mock.coro_suspend_idx = 1;
    g_expr_mock.int_is_signed = TRUE;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t expr_test_map_type(
    a_type_ptr edg_tp,
    gcc_jit_type *jit_tp) GCC_GEN_BE_NOEXCEPT {
    for (int i = 0; i < g_expr_mock.num_type_overrides; ++i) {
        if (g_expr_mock.type_overrides[i].edg_type == edg_tp) {
            g_expr_mock.type_overrides[i].jit_type = jit_tp;
            return GCC_GEN_BE_SUCCESS;
        }
    }
    if (g_expr_mock.num_type_overrides >= 256) return GCC_GEN_BE_ERROR_INTERNAL;
    int idx = g_expr_mock.num_type_overrides++;
    g_expr_mock.type_overrides[idx].edg_type = edg_tp;
    g_expr_mock.type_overrides[idx].jit_type = jit_tp;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t expr_test_map_const_int(
    a_constant_ptr con,
    a_host_large_integer val) GCC_GEN_BE_NOEXCEPT {
    for (int i = 0; i < g_expr_mock.num_const_ints; ++i) {
        if (g_expr_mock.const_ints[i].con == con) {
            g_expr_mock.const_ints[i].val = val;
            return GCC_GEN_BE_SUCCESS;
        }
    }
    if (g_expr_mock.num_const_ints >= 256) return GCC_GEN_BE_ERROR_INTERNAL;
    int idx = g_expr_mock.num_const_ints++;
    g_expr_mock.const_ints[idx].con = con;
    g_expr_mock.const_ints[idx].val = val;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t expr_test_setup_fn(
    gcc_jit_context **out_ctx,
    gcc_jit_function **out_fn,
    gcc_jit_block **out_blk) GCC_GEN_BE_NOEXCEPT {
    if (!out_ctx || !out_fn || !out_blk) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    g_gcc_be_oom_countdown = -1;
    GCC_GEN_BE_CHECK(cache_clear_all());
    std::memset(&g_expr_mock, 0, sizeof(g_expr_mock));
    GCC_GEN_BE_CHECK(expr_test_reset_mock());
    exceptions_enabled = TRUE;
    innermost_function_scope = NULL;
    gcc_gen_be_singleton_state.landing_pad_stack_size = 0;
    for (;;) {
        gcc_jit_block *cur_lp = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_active_landing_pad(&cur_lp));
        if (!cur_lp) break;
        GCC_GEN_BE_CHECK(gcc_gen_be_pop_landing_pad());
    }
    gcc_gen_be_singleton_state.scope_stack_size = 0;
    gcc_gen_be_singleton_state.stmt_expr_result_lval = NULL;
    gcc_gen_be_singleton_state.active_thrown_obj_lval = NULL;
    gcc_gen_be_singleton_state.active_caught_obj_lval = NULL;
    gcc_gen_be_singleton_state.active_target_abi_override = 0;

#if defined(_WIN32)
    GCC_GEN_BE_CHECK(load_libgccjit_windows());
#else
    GCC_GEN_BE_CHECK(load_libgccjit_posix());
#endif
    gcc_jit_context *ctx = gcc_jit_context_acquire();
    if (!ctx) return GCC_GEN_BE_ERROR_OOM;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    gcc_jit_field *cf[2] = {
        gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE), "re"),
        gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE), "im")
    };
    g_expr_mock.cplx_struct_type = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_cplx_st", 2, cf));

    gcc_jit_field *pf[2] = {
        gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR), "ptr"),
        gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG), "adj")
    };
    g_expr_mock.pmf_struct_type = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_pmf_st", 2, pf));

    gcc_jit_field *of[1] = {
        gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), "v")
    };
    g_expr_mock.one_field_struct_type = gcc_jit_struct_as_type(
        gcc_jit_context_new_struct_type(ctx, NULL, "_one_st", 1, of));

    gcc_jit_param *p0 = gcc_jit_context_new_param(
        ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR), "arg0");
    gcc_jit_function *fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_EXPORTED,
        gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID), "expr_test_fn", 1, &p0, 0);
    gcc_jit_block *blk = gcc_jit_function_new_block(fn, "entry");
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(blk));

    *out_ctx = ctx;
    *out_fn = fn;
    *out_blk = blk;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t expr_test_fresh_block(
    gcc_jit_function *fn,
    gcc_jit_block **out_blk) GCC_GEN_BE_NOEXCEPT {
    if (!fn) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    char bname[32];
    std::snprintf(bname, sizeof(bname), "blk_%lu", ++g_fresh_blk_seq);
    gcc_jit_block *blk = gcc_jit_function_new_block(fn, bname);
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(blk));
    if (out_blk) *out_blk = blk;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t expr_test_teardown_fn(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT {
    g_gcc_be_oom_countdown = -1;
    GCC_GEN_BE_CHECK(cache_clear_all());
    while (gcc_gen_be_singleton_state.scope_stack_size > 0) {
        GCC_GEN_BE_CHECK(gcc_gen_be_pop_scope(NULL));
    }
    gcc_gen_be_singleton_state.landing_pad_stack_size = 0;
    gcc_gen_be_singleton_state.stmt_expr_result_lval = NULL;
    gcc_gen_be_singleton_state.active_thrown_obj_lval = NULL;
    gcc_gen_be_singleton_state.active_caught_obj_lval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(NULL));
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (ctx) {
        gcc_jit_context_release(ctx);
    }
    g_expr_mock.cplx_struct_type = NULL;
    g_expr_mock.pmf_struct_type = NULL;
    g_expr_mock.one_field_struct_type = NULL;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t expr_test_sweep_oom(
    gcc_jit_function *fn,
    int max_budget,
    expr_oom_step_fn_t step_fn,
    void *user_data) GCC_GEN_BE_NOEXCEPT {
    if (!step_fn) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    int limit = max_budget < 256 ? 256 : max_budget;
    for (int budget = 0; budget <= limit; ++budget) {
        g_gcc_be_oom_countdown = -1;
        if (fn) {
            GCC_GEN_BE_CHECK(expr_test_fresh_block(fn, NULL));
        }
        g_gcc_be_oom_countdown = budget;
        gcc_gen_be_error_t err = step_fn(user_data);
        g_gcc_be_oom_countdown = -1;
        if (err == GCC_GEN_BE_SUCCESS) {
            return GCC_GEN_BE_SUCCESS;
        }
    }
    return GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED;
}

END_EDG_NAMESPACE

/**
 * @brief Executes baseline null-pointer, decimal-float, fixed-point, and bitfield-pack unit tests.
 * @details Verifies top-level entry guards and standalone numeric helper functions in `gcc_gen_be_expr.c`.
 * @param[out] out_ok Pointer receiving `1` on success, `0` on failure.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t run_expr_unit_tests(int *out_ok) GCC_GEN_BE_NOEXCEPT {
    if (!out_ok) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_ok = 0;

    gcc_jit_rvalue *rval = NULL;
    gcc_jit_lvalue *lval = NULL;
    if (gcc_gen_be_lower_expr_rvalue(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_expr_lvalue(NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_constant_rvalue(NULL, NULL, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_constant_rvalue((a_constant_ptr)1, NULL, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(NULL, &rval));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(NULL, &lval));

    if (gcc_gen_be_lower_decimal_float_op('+', 4, NULL, NULL, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_fixed_point_op('+', 16, FALSE, NULL, NULL, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    gcc_jit_context *ctx = NULL;
    gcc_jit_function *fn = NULL;
    gcc_jit_block *blk = NULL;
    GCC_GEN_BE_CHECK(expr_test_setup_fn(&ctx, &fn, &blk));

    gcc_jit_type *u32_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT32_T);
    gcc_jit_type *i64_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_LONG);
    gcc_jit_rvalue *v1 = gcc_jit_context_new_rvalue_from_int(ctx, u32_t, 100);
    gcc_jit_rvalue *v2 = gcc_jit_context_new_rvalue_from_int(ctx, u32_t, 25);
    gcc_jit_rvalue *v64_1 = gcc_jit_context_new_rvalue_from_int(ctx, i64_t, 100);
    gcc_jit_rvalue *v64_2 = gcc_jit_context_new_rvalue_from_int(ctx, i64_t, 25);

    if (gcc_gen_be_lower_decimal_float_op('+', 4, v1, NULL, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_decimal_float_op('+', 4, v1, v2, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_fixed_point_op('+', 15, FALSE, v1, NULL, &rval) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_fixed_point_op('+', 15, FALSE, v1, v2, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_op('+', 4, v1, v2, &rval));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_op('-', 4, v1, v2, &rval));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_op('*', 8, v1, v2, &rval));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_op('/', 16, v1, v2, &rval));
    gcc_gen_be_singleton_state.active_target_abi_override = (int)GCC_GEN_BE_TARGET_ABI_PPC64_ELFV2 + 1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_op('-', 8, v1, v2, &rval));
    gcc_gen_be_singleton_state.active_target_abi_override = (int)GCC_GEN_BE_TARGET_ABI_S390X + 1;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_op('+', 4, v1, v2, &rval));
    gcc_gen_be_singleton_state.active_target_abi_override = 0;
    if (gcc_gen_be_lower_decimal_float_op('+', 3, v1, v2, &rval) != GCC_GEN_BE_ERROR_DECIMAL_FLOAT_UNSUPPORTED) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_decimal_float_op('%', 4, v1, v2, &rval) != GCC_GEN_BE_ERROR_DECIMAL_FLOAT_UNSUPPORTED) return GCC_GEN_BE_ERROR_INTERNAL;

    GCC_GEN_BE_CHECK(gcc_gen_be_lower_fixed_point_op('+', 15, TRUE, v1, v2, &rval));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_fixed_point_op('-', 32, TRUE, v64_1, v64_2, &rval));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_fixed_point_op('*', 15, TRUE, v1, v2, &rval));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_fixed_point_op('/', 15, FALSE, v1, v2, &rval));
    if (gcc_gen_be_lower_fixed_point_op('+', 0, FALSE, v1, v2, &rval) != GCC_GEN_BE_ERROR_FIXED_POINT_OVERFLOW) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_fixed_point_op('+', 64, FALSE, v1, v2, &rval) != GCC_GEN_BE_ERROR_FIXED_POINT_OVERFLOW) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_fixed_point_op('%', 15, FALSE, v1, v2, &rval) != GCC_GEN_BE_ERROR_FIXED_POINT_OVERFLOW) return GCC_GEN_BE_ERROR_INTERNAL;

    /* Invalid state (NULL context) branches on decimal/fixed-point helpers */
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(NULL));
    if (gcc_gen_be_lower_decimal_float_op('+', 4, v1, v2, &rval) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_lower_fixed_point_op('+', 15, FALSE, v1, v2, &rval) != GCC_GEN_BE_ERROR_INVALID_STATE) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_set_context(ctx));

    /* Bitfield constant packing edge cases & OOM */
    unsigned long packed = 0;
    const unsigned long bvals[4] = { 3UL, 1UL, 0xffUL, 5UL };
    const unsigned int boffs[4] = { 0U, 64U, 2U, 4U };
    const unsigned int bwids[4] = { 0U, 4U, 64U, 3U };
    if (gcc_gen_be_pack_bitfield_constants(bvals, boffs, bwids, 4, NULL) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_pack_bitfield_constants(bvals, boffs, bwids, -1, &packed) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_pack_bitfield_constants(NULL, boffs, bwids, 1, &packed) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_pack_bitfield_constants(bvals, NULL, bwids, 1, &packed) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    if (gcc_gen_be_pack_bitfield_constants(bvals, boffs, NULL, 1, &packed) != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return GCC_GEN_BE_ERROR_INTERNAL;
    GCC_GEN_BE_CHECK(gcc_gen_be_pack_bitfield_constants(NULL, NULL, NULL, 0, &packed));
    GCC_GEN_BE_CHECK(expr_sweep_oom_fn(fn, 8, [&]() noexcept -> gcc_gen_be_error_t {
        return gcc_gen_be_pack_bitfield_constants(bvals, boffs, bwids, 4, &packed);
    }));

    GCC_GEN_BE_CHECK(expr_test_teardown_fn(ctx));
    *out_ok = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for `test_gcc_be_expr`.
 * @details Executes baseline, lvalue/init/constant, rvalue-part1, and rvalue-part2 test suites.
 * @return `0` when all suites succeed, `1` otherwise.
 */
int main(void) GCC_GEN_BE_NOEXCEPT {
    int ok = 0;
    gcc_gen_be_error_t err = run_expr_unit_tests(&ok);
    if (err != GCC_GEN_BE_SUCCESS || !ok) {
        std::fprintf(stderr, "FAIL: run_expr_unit_tests (%d)\n", (int)err);
        return 1;
    }
    err = run_expr_lval_and_init_tests();
    if (err != GCC_GEN_BE_SUCCESS) {
        std::fprintf(stderr, "FAIL: run_expr_lval_and_init_tests (%d)\n", (int)err);
        return 1;
    }
    err = run_expr_rval_part1_tests();
    if (err != GCC_GEN_BE_SUCCESS) {
        std::fprintf(stderr, "FAIL: run_expr_rval_part1_tests (%d)\n", (int)err);
        return 1;
    }
    err = run_expr_rval_part2_tests();
    if (err != GCC_GEN_BE_SUCCESS) {
        std::fprintf(stderr, "FAIL: run_expr_rval_part2_tests (%d)\n", (int)err);
        return 1;
    }
    err = run_expr_cov_init_and_const_tests();
    if (err != GCC_GEN_BE_SUCCESS) {
        std::fprintf(stderr, "FAIL: run_expr_cov_init_and_const_tests (%d)\n", (int)err);
        return 1;
    }
    err = run_expr_cov_lval_tests();
    if (err != GCC_GEN_BE_SUCCESS) {
        std::fprintf(stderr, "FAIL: run_expr_cov_lval_tests (%d)\n", (int)err);
        return 1;
    }
    err = run_expr_cov_rval_tests();
    if (err != GCC_GEN_BE_SUCCESS) {
        std::fprintf(stderr, "FAIL: run_expr_cov_rval_tests (%d)\n", (int)err);
        return 1;
    }
    std::puts("PASS");
    return 0;
}
