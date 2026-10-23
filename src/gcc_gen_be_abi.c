/**
 * @file gcc_gen_be_abi.c
 * @brief Implementation of Target ABI Parameter Classification & Aggregate Layout.
 *
 * This file implements the System V AMD64 ABI classification (eightbyte tracking,
 * INTEGER/SSE splitting, sret return values) and AAPCS64 (ARM64) classification
 * (HFA/HVA passing, indirect byval arguments). It also processes calling
 * convention attributes.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_abi.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_lib_loader.h"
#include "gcc_gen_be_type.h"
#include "il_def.h"

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

/**
 * @brief Applies function calling convention attributes to a JIT function.
 *
 * @details Handles __cdecl, __stdcall, __fastcall, __thiscall, and __vectorcall 
 * by invoking the corresponding libgccjit attribute functions or asm decorators.
 *
 * @param[in] rout The frontend routine describing the attributes.
 * @param[in] func The libgccjit function to modify.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_apply_function_attributes(a_routine_ptr rout, gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    if (!rout || !func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    an_attribute_ptr attr = rout->source_corresp.attributes;
    while (attr) {
        if (p_gcc_jit_function_add_attribute) {
            switch (attr->kind) {
                case ak_always_inline:
                    p_gcc_jit_function_add_attribute(func, GCC_JIT_FN_ATTRIBUTE_ALWAYS_INLINE);
                    break;
                case ak_cold:
                    p_gcc_jit_function_add_attribute(func, GCC_JIT_FN_ATTRIBUTE_COLD);
                    break;
                case ak_noinline:
                    p_gcc_jit_function_add_attribute(func, GCC_JIT_FN_ATTRIBUTE_NOINLINE);
                    break;
                case ak_pure:
                    p_gcc_jit_function_add_attribute(func, GCC_JIT_FN_ATTRIBUTE_PURE);
                    break;
                case ak_const:
                    p_gcc_jit_function_add_attribute(func, GCC_JIT_FN_ATTRIBUTE_CONST);
                    break;
                case ak_used:
                    p_gcc_jit_function_add_attribute(func, GCC_JIT_FN_ATTRIBUTE_USED);
                    break;
                default:
                    break;
            }
        }
        attr = attr->next;
    }

#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
    if (rout->type && rout->type->kind == tk_routine && rout->type->variant.routine.extra_info) {
        a_calling_convention cc = rout->type->variant.routine.extra_info->calling_convention;
        if (p_gcc_jit_function_add_string_attribute) {
            if (cc == cc_stdcall) {
                p_gcc_jit_function_add_string_attribute(func, GCC_JIT_FN_ATTRIBUTE_MAX, "stdcall");
            } else if (cc == cc_fastcall) {
                p_gcc_jit_function_add_string_attribute(func, GCC_JIT_FN_ATTRIBUTE_MAX, "fastcall");
            } else if (cc == cc_thiscall) {
                p_gcc_jit_function_add_string_attribute(func, GCC_JIT_FN_ATTRIBUTE_MAX, "thiscall");
            } else if (cc == cc_vectorcall) {
                p_gcc_jit_function_add_string_attribute(func, GCC_JIT_FN_ATTRIBUTE_MAX, "vectorcall");
            }
        }
    }
#endif

    return GCC_GEN_BE_SUCCESS;
}


/**
 * @brief Applies attributes and struct packing directives to a JIT struct.
 *
 * @param[in] class_type The frontend class type.
 * @param[in] struct_type The libgccjit struct to modify.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_apply_struct_attributes(a_type_ptr class_type, gcc_jit_struct *struct_type) GCC_GEN_BE_NOEXCEPT {
    if (!class_type || !struct_type) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    for (an_attribute_ptr attr = class_type->source_corresp.attributes; attr != NULL; attr = attr->next) {
        if (attr->kind == ak_align || attr->kind == ak_packed) {
            /* Explicit byte offsets and tail padding up to class_type->size are synthesized
               directly in gcc_gen_be_lower_type(). */
            continue;
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Applies variable attributes (alignment, section) to a JIT lvalue.
 *
 * @param[in] var The frontend variable.
 * @param[in] lval The libgccjit lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_apply_variable_attributes(a_variable_ptr var, gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT {
    if (!var || !lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    if (p_gcc_jit_lvalue_set_tls_model && var->is_thread_local) {
        p_gcc_jit_lvalue_set_tls_model(lval, GCC_JIT_TLS_MODEL_GLOBAL_DYNAMIC);
    }
    
#if defined(LIBGCCJIT_HAVE_ALIGNMENT)
    if (var->alignment > 0) {
        if (p_gcc_jit_lvalue_set_alignment) {
            p_gcc_jit_lvalue_set_alignment(lval, var->alignment);
        }
    }
#endif
    
    an_attribute_ptr attr = var->source_corresp.attributes;
    while (attr) {
        if (attr->kind == ak_section && p_gcc_jit_lvalue_set_link_section) {
            an_attribute_arg_ptr arg = attr->arguments;
            if (arg && arg->kind == aak_constant && arg->variant.constant->kind == ck_string) {
                p_gcc_jit_lvalue_set_link_section(lval, (const char*)arg->variant.constant->variant.string.value);
            }
        } else if (attr->kind == ak_visibility && p_gcc_jit_lvalue_add_string_attribute) {
            an_attribute_arg_ptr arg = attr->arguments;
            if (arg && arg->kind == aak_constant && arg->variant.constant->kind == ck_string) {
                p_gcc_jit_lvalue_add_string_attribute(lval, GCC_JIT_VARIABLE_ATTRIBUTE_VISIBILITY, (const char*)arg->variant.constant->variant.string.value);
            }
        }
        attr = attr->next;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Recursively merges two AMD64 ABI classes.
 *
 * @details Combines two eightbyte ABI classifications according to the System V AMD64 ABI merger rules.
 *
 * @param[in] a First class.
 * @param[in] b Second class.
 * @param[out] out_class Pointer to receive the merged class.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT if out_class is NULL.
 */
static gcc_gen_be_error_t merge_amd64_classes(gcc_gen_be_abi_class_t a, gcc_gen_be_abi_class_t b, gcc_gen_be_abi_class_t *out_class) GCC_GEN_BE_NOEXCEPT {
    if (!out_class) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (a == b) { *out_class = a; return GCC_GEN_BE_SUCCESS; }
    if (a == GCC_GEN_BE_ABI_CLASS_NO_CLASS) { *out_class = b; return GCC_GEN_BE_SUCCESS; }
    if (b == GCC_GEN_BE_ABI_CLASS_NO_CLASS) { *out_class = a; return GCC_GEN_BE_SUCCESS; }
    if (a == GCC_GEN_BE_ABI_CLASS_MEMORY || b == GCC_GEN_BE_ABI_CLASS_MEMORY) { *out_class = GCC_GEN_BE_ABI_CLASS_MEMORY; return GCC_GEN_BE_SUCCESS; }
    if (a == GCC_GEN_BE_ABI_CLASS_INTEGER || b == GCC_GEN_BE_ABI_CLASS_INTEGER) { *out_class = GCC_GEN_BE_ABI_CLASS_INTEGER; return GCC_GEN_BE_SUCCESS; }
    if (a == GCC_GEN_BE_ABI_CLASS_X87 || a == GCC_GEN_BE_ABI_CLASS_X87UP || a == GCC_GEN_BE_ABI_CLASS_COMPLEX_X87 ||
        b == GCC_GEN_BE_ABI_CLASS_X87 || b == GCC_GEN_BE_ABI_CLASS_X87UP || b == GCC_GEN_BE_ABI_CLASS_COMPLEX_X87) {
        *out_class = GCC_GEN_BE_ABI_CLASS_MEMORY;
        return GCC_GEN_BE_SUCCESS;
    }
    *out_class = GCC_GEN_BE_ABI_CLASS_SSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Recursively classifies a type into AMD64 ABI eightbyte classes.
 * @details Traverses members up to 16 bytes, merging field classes.
 * @param[in] type Type to classify.
 * @param[in] offset_in_bytes Offset of this type within the top-level object.
 * @param[in,out] classes Eightbyte classes array.
 * @return GCC_GEN_BE_SUCCESS or error.
 */
static gcc_gen_be_error_t do_classify_sysv_amd64(a_type_ptr type, long offset_in_bytes, gcc_gen_be_abi_class_t classes[2]) GCC_GEN_BE_NOEXCEPT {
    a_type_ptr t = type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (!t) return GCC_GEN_BE_SUCCESS;
    
    long sz = (long)size_of_type(t);
    if (sz == 0 || offset_in_bytes >= 16) return GCC_GEN_BE_SUCCESS;
    
    if (t->kind == tk_class || t->kind == tk_struct || t->kind == tk_union) {
        a_class_type_supplement_ptr ctsp = t->variant.class_struct_union.extra_info;
        if (ctsp) {
            /* Classify direct base classes recursively */
            for (a_base_class_ptr bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
                if (bcp->direct && bcp->type) {
                    long base_offset = offset_in_bytes + (long)bcp->offset;
                    GCC_GEN_BE_CHECK(do_classify_sysv_amd64(bcp->type, base_offset, classes));
                }
            }
        }
        a_field_ptr f = t->variant.class_struct_union.field_list;
        while (f) {
            if (f->is_bit_field) {
                if (f->bit_size > 0) {
                    long start_bit = (offset_in_bytes + (long)f->offset) * 8 + (long)f->offset_bit_remainder;
                    long end_bit = start_bit + (long)f->bit_size - 1;
                    long start_eb = start_bit / 64;
                    long end_eb = end_bit / 64;
                    if (start_eb >= 0 && start_eb < 2) {
                        GCC_GEN_BE_CHECK(merge_amd64_classes(classes[start_eb], GCC_GEN_BE_ABI_CLASS_INTEGER, &classes[start_eb]));
                    }
                    if (end_eb != start_eb && end_eb >= 0 && end_eb < 2) {
                        GCC_GEN_BE_CHECK(merge_amd64_classes(classes[end_eb], GCC_GEN_BE_ABI_CLASS_INTEGER, &classes[end_eb]));
                    }
                }
            } else {
                long field_offset = offset_in_bytes + (long)f->offset;
                GCC_GEN_BE_CHECK(do_classify_sysv_amd64(f->type, field_offset, classes));
            }
            f = f->next;
        }
    } else if (t->kind == tk_array) {
        long elem_sz = (long)size_of_type(t->variant.array.element_type);
        long count = (long)t->variant.array.variant.number_of_elements;
        for (long i = 0; i < count; i++) {
            long elem_offset = offset_in_bytes + i * elem_sz;
            GCC_GEN_BE_CHECK(do_classify_sysv_amd64(t->variant.array.element_type, elem_offset, classes));
        }
    } else {
        long idx = offset_in_bytes / 8;
#ifndef EDG_GCC_BE_ISOLATED_TEST
        if (t->kind == tk_complex &&
            (t->variant.float_kind == fk_long_double
#if FLOAT80_ENABLING_POSSIBLE
             || t->variant.float_kind == fk_float80
#endif
            )) {
            if (idx < 2) GCC_GEN_BE_CHECK(merge_amd64_classes(classes[idx], GCC_GEN_BE_ABI_CLASS_COMPLEX_X87, &classes[idx]));
            if (idx + 1 < 2) GCC_GEN_BE_CHECK(merge_amd64_classes(classes[idx + 1], GCC_GEN_BE_ABI_CLASS_COMPLEX_X87, &classes[idx + 1]));
            return GCC_GEN_BE_SUCCESS;
        }
#endif
        if (t->kind == tk_float &&
            (t->variant.float_kind == fk_long_double
#if FLOAT80_ENABLING_POSSIBLE
             || t->variant.float_kind == fk_float80
#endif
            )) {
#ifdef EDG_GCC_BE_ISOLATED_TEST
            if (t->variant.float_supplement.is_complex) {
                if (idx < 2) GCC_GEN_BE_CHECK(merge_amd64_classes(classes[idx], GCC_GEN_BE_ABI_CLASS_COMPLEX_X87, &classes[idx]));
                if (idx + 1 < 2) GCC_GEN_BE_CHECK(merge_amd64_classes(classes[idx + 1], GCC_GEN_BE_ABI_CLASS_COMPLEX_X87, &classes[idx + 1]));
            } else
#endif
            {
                if (idx < 2) GCC_GEN_BE_CHECK(merge_amd64_classes(classes[idx], GCC_GEN_BE_ABI_CLASS_X87, &classes[idx]));
                if (idx + 1 < 2) GCC_GEN_BE_CHECK(merge_amd64_classes(classes[idx + 1], GCC_GEN_BE_ABI_CLASS_X87UP, &classes[idx + 1]));
            }
            return GCC_GEN_BE_SUCCESS;
        }
        gcc_gen_be_abi_class_t c = GCC_GEN_BE_ABI_CLASS_INTEGER;
        if (t->kind == tk_float || t->kind == tk_vector
#ifndef EDG_GCC_BE_ISOLATED_TEST
            || t->kind == tk_complex
#endif
           ) {
            c = GCC_GEN_BE_ABI_CLASS_SSE;
        }
        if (idx < 2) {
            GCC_GEN_BE_CHECK(merge_amd64_classes(classes[idx], c, &classes[idx]));
        }
        /* Handle elements crossing eightbyte boundaries */
        if (offset_in_bytes % 8 != 0 && (offset_in_bytes % 8) + sz > 8) {
            if (idx + 1 < 2) {
                gcc_gen_be_abi_class_t hi_c = (c == GCC_GEN_BE_ABI_CLASS_SSE && t->kind == tk_vector)
                    ? GCC_GEN_BE_ABI_CLASS_SSEUP
                    : c;
                GCC_GEN_BE_CHECK(merge_amd64_classes(classes[idx + 1], hi_c, &classes[idx + 1]));
            }
        } else if (offset_in_bytes % 8 == 0 && sz > 8 && idx + 1 < 2) {
            gcc_gen_be_abi_class_t hi_c = (c == GCC_GEN_BE_ABI_CLASS_SSE && t->kind == tk_vector)
                ? GCC_GEN_BE_ABI_CLASS_SSEUP
                : c;
            GCC_GEN_BE_CHECK(merge_amd64_classes(classes[idx + 1], hi_c, &classes[idx + 1]));
        }
    }
    
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Classifies System V AMD64 ABI.
 * @details Implement Complete System V AMD64 ABI Classification including
 * recursive classification for aggregate types up to 16 bytes.
 * Handles INTEGER, SSE splitting, X87/COMPLEX_X87, and MEMORY passing.
 * @param[in] type Type.
 * @param[out] classes Classes for up to 2 eightbytes.
 * @return GCC_GEN_BE_SUCCESS
 */
gcc_gen_be_error_t gcc_gen_be_classify_sysv_amd64(a_type_ptr type, gcc_gen_be_abi_class_t classes[2]) GCC_GEN_BE_NOEXCEPT {
    if (!type || !classes) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    classes[0] = GCC_GEN_BE_ABI_CLASS_NO_CLASS;
    classes[1] = GCC_GEN_BE_ABI_CLASS_NO_CLASS;
    
    a_type_ptr t = type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (!t) return GCC_GEN_BE_SUCCESS;
    
    long sz = (long)size_of_type(t);
    if (sz == 0) {
        classes[0] = GCC_GEN_BE_ABI_CLASS_MEMORY;
        classes[1] = GCC_GEN_BE_ABI_CLASS_MEMORY;
        return GCC_GEN_BE_SUCCESS;
    }
    if (sz > 16) {
#ifdef EDG_GCC_BE_ISOLATED_TEST
        if (t->kind == tk_float &&
            (t->variant.float_kind == fk_long_double
#if FLOAT80_ENABLING_POSSIBLE
             || t->variant.float_kind == fk_float80
#endif
            ) && t->variant.float_supplement.is_complex) {
            classes[0] = GCC_GEN_BE_ABI_CLASS_COMPLEX_X87;
            classes[1] = GCC_GEN_BE_ABI_CLASS_COMPLEX_X87;
            return GCC_GEN_BE_SUCCESS;
        }
#else
        if (t->kind == tk_complex &&
            (t->variant.float_kind == fk_long_double
#if FLOAT80_ENABLING_POSSIBLE
             || t->variant.float_kind == fk_float80
#endif
            )) {
            classes[0] = GCC_GEN_BE_ABI_CLASS_COMPLEX_X87;
            classes[1] = GCC_GEN_BE_ABI_CLASS_COMPLEX_X87;
            return GCC_GEN_BE_SUCCESS;
        }
#endif
        if (t->kind == tk_vector && (sz == 32 || sz == 64)) {
            classes[0] = GCC_GEN_BE_ABI_CLASS_SSE;
            classes[1] = GCC_GEN_BE_ABI_CLASS_SSEUP;
            return GCC_GEN_BE_SUCCESS;
        }
        classes[0] = GCC_GEN_BE_ABI_CLASS_MEMORY;
        classes[1] = GCC_GEN_BE_ABI_CLASS_MEMORY;
        return GCC_GEN_BE_SUCCESS;
    }
    
    GCC_GEN_BE_CHECK(do_classify_sysv_amd64(t, 0, classes));
    
    /* Post-merger cleanup rules per System V AMD64 ABI 3.2.3:
       (a) If one of the classes is MEMORY, the whole argument is passed in memory.
       (b) If X87UP is not preceded by X87, the whole argument is passed in memory.
       (c) If SSEUP is not preceded by SSE or SSEUP, it is converted to SSE. */
    if (classes[0] == GCC_GEN_BE_ABI_CLASS_MEMORY || classes[1] == GCC_GEN_BE_ABI_CLASS_MEMORY) {
        classes[0] = GCC_GEN_BE_ABI_CLASS_MEMORY;
        classes[1] = GCC_GEN_BE_ABI_CLASS_MEMORY;
    } else if (classes[1] == GCC_GEN_BE_ABI_CLASS_X87UP && classes[0] != GCC_GEN_BE_ABI_CLASS_X87) {
        classes[0] = GCC_GEN_BE_ABI_CLASS_MEMORY;
        classes[1] = GCC_GEN_BE_ABI_CLASS_MEMORY;
    } else if (classes[1] == GCC_GEN_BE_ABI_CLASS_SSEUP &&
               classes[0] != GCC_GEN_BE_ABI_CLASS_SSE &&
               classes[0] != GCC_GEN_BE_ABI_CLASS_SSEUP) {
        classes[1] = GCC_GEN_BE_ABI_CLASS_SSE;
    }
    
    if (classes[0] == GCC_GEN_BE_ABI_CLASS_NO_CLASS) classes[0] = GCC_GEN_BE_ABI_CLASS_INTEGER;
    
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Recursively inspects a type to count homogeneous FP or short-vector elements for AAPCS64 / PPC64LE.
 * @details Traverses nested structs, base classes, and arrays, verifying all leaf elements are
 * floating-point or short-vector types of identical size and kind (up to `max_elements` total elements).
 * @param[in] type The type to inspect.
 * @param[in] max_elements Maximum number of homogeneous elements allowed (4 for AAPCS64, 8 for PPC64LE ELFv2).
 * @param[in,out] elem_size Pointer to the established element size (0 if not yet set).
 * @param[in,out] elem_kind Pointer to the established element kind (0 if not yet set).
 * @param[in,out] count Pointer to the running element count.
 * @param[out] out_valid Pointer to receive TRUE if homogeneous, FALSE otherwise.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t check_hfa_hva_recursive(
    a_type_ptr type,
    int max_elements,
    long *elem_size,
    int *elem_kind,
    int *count,
    a_boolean *out_valid) GCC_GEN_BE_NOEXCEPT {
    if (!elem_size || !elem_kind || !count || !out_valid) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_valid = TRUE;

    a_type_ptr t = type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (!t) {
        *out_valid = FALSE;
        return GCC_GEN_BE_SUCCESS;
    }

    if (t->kind == tk_class || t->kind == tk_struct) {
        a_class_type_supplement_ptr ctsp = t->variant.class_struct_union.extra_info;
        if (ctsp) {
            for (a_base_class_ptr bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
                if (bcp->direct && bcp->type && size_of_type(bcp->type) > 0) {
                    a_boolean sub_ok = FALSE;
                    GCC_GEN_BE_CHECK(check_hfa_hva_recursive(bcp->type, max_elements, elem_size, elem_kind, count, &sub_ok));
                    if (!sub_ok || *count > max_elements) {
                        *out_valid = FALSE;
                        return GCC_GEN_BE_SUCCESS;
                    }
                }
            }
        }
        for (a_field_ptr f = t->variant.class_struct_union.field_list; f != NULL; f = f->next) {
            a_boolean sub_ok = FALSE;
            GCC_GEN_BE_CHECK(check_hfa_hva_recursive(f->type, max_elements, elem_size, elem_kind, count, &sub_ok));
            if (!sub_ok || *count > max_elements) {
                *out_valid = FALSE;
                return GCC_GEN_BE_SUCCESS;
            }
        }
        return GCC_GEN_BE_SUCCESS;
    }

    if (t->kind == tk_array) {
        long arr_len = (long)t->variant.array.variant.number_of_elements;
        if (arr_len <= 0 || arr_len > max_elements) {
            *out_valid = FALSE;
            return GCC_GEN_BE_SUCCESS;
        }
        for (long i = 0; i < arr_len; ++i) {
            a_boolean sub_ok = FALSE;
            GCC_GEN_BE_CHECK(check_hfa_hva_recursive(t->variant.array.element_type, max_elements, elem_size, elem_kind, count, &sub_ok));
            if (!sub_ok || *count > max_elements) {
                *out_valid = FALSE;
                return GCC_GEN_BE_SUCCESS;
            }
        }
        return GCC_GEN_BE_SUCCESS;
    }

    long sz = (long)size_of_type(t);
    if (t->kind == tk_float
#ifndef EDG_GCC_BE_ISOLATED_TEST
        || t->kind == tk_complex
#endif
       ) {
#ifdef EDG_GCC_BE_ISOLATED_TEST
        int kind = 100 + (int)t->variant.float_kind;
        int num_parts = t->variant.float_supplement.is_complex ? 2 : 1;
        long part_sz = t->variant.float_supplement.is_complex ? (sz / 2) : sz;
#else
        int kind = 100 + (int)t->variant.float_kind;
        int num_parts = (t->kind == tk_complex) ? 2 : 1;
        long part_sz = (t->kind == tk_complex) ? (sz / 2) : sz;
#endif
        if (*elem_size == 0) {
            *elem_size = part_sz;
            *elem_kind = kind;
        } else if (*elem_size != part_sz || *elem_kind != kind) {
            *out_valid = FALSE;
            return GCC_GEN_BE_SUCCESS;
        }
        *count += num_parts;
        if (*count > max_elements) *out_valid = FALSE;
        return GCC_GEN_BE_SUCCESS;
    }

    if (t->kind == tk_vector && (sz == 8 || sz == 16)) {
        int kind = 200 + (int)sz;
        if (*elem_size == 0) {
            *elem_size = sz;
            *elem_kind = kind;
        } else if (*elem_size != sz || *elem_kind != kind) {
            *out_valid = FALSE;
            return GCC_GEN_BE_SUCCESS;
        }
        (*count)++;
        if (*count > max_elements) *out_valid = FALSE;
        return GCC_GEN_BE_SUCCESS;
    }

    *out_valid = FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Classifies AAPCS64 (ARM64) ABI.
 * @details Implements Homogeneous Floating-Point Aggregates (HFA) and
 * Homogeneous Short-Vector Aggregates (HVA) classification (1 to 4 members),
 * including nested structs and base classes.
 * @param[in] type Type.
 * @param[out] is_hfa_hva TRUE if HFA/HVA.
 * @return GCC_GEN_BE_SUCCESS
 */
gcc_gen_be_error_t gcc_gen_be_classify_aapcs64(a_type_ptr type, a_boolean *is_hfa_hva) GCC_GEN_BE_NOEXCEPT {
    if (!type || !is_hfa_hva) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *is_hfa_hva = FALSE;
    
    a_type_ptr t = type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (!t) return GCC_GEN_BE_SUCCESS;
    
    if (t->kind == tk_class || t->kind == tk_struct || t->kind == tk_array) {
        long elem_size = 0;
        int elem_kind = 0;
        int count = 0;
        a_boolean valid = FALSE;
        GCC_GEN_BE_CHECK(check_hfa_hva_recursive(t, 4, &elem_size, &elem_kind, &count, &valid));
        if (valid && count >= 1 && count <= 4) {
            *is_hfa_hva = TRUE;
        }
    }
    
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Classifies Windows x64 ABI.
 * @details Determines if passed by reference (size > 8 or non-power-of-two).
 * @param[in] type Type.
 * @param[out] out_is_ref TRUE if passed by reference.
 * @return GCC_GEN_BE_SUCCESS
 */
gcc_gen_be_error_t gcc_gen_be_classify_win64(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT {
    if (!type || !out_is_ref) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_is_ref = FALSE;
    
    a_type_ptr t = type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (!t) return GCC_GEN_BE_SUCCESS;
    
    long sz = (long)size_of_type(t);
    if (sz > 8 || (sz != 1 && sz != 2 && sz != 4 && sz != 8)) {
        *out_is_ref = TRUE;
    }
    
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Recursively counts flattened leaf scalar fields for RISC-V LP64D FP-register classification.
 * @details Traverses nested structs, base classes, and arrays, counting total leaf fields (`field_count`),
 * floating-point fields `<= 8` bytes (`fp_count`), integer/pointer fields `<= 8` bytes (`int_count`),
 * and optionally recording the first two leaf types in `leaf_types`.
 * @param[in] type The frontend type to flatten.
 * @param[in,out] field_count Running count of leaf fields.
 * @param[in,out] fp_count Running count of floating-point leaf fields.
 * @param[in,out] int_count Running count of integer/pointer leaf fields.
 * @param[in,out] leaf_types Optional 2-element array to receive the first two leaf scalar types.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t flatten_riscv64_fields(
    a_type_ptr type,
    int *field_count,
    int *fp_count,
    int *int_count,
    a_type_ptr leaf_types[2]) GCC_GEN_BE_NOEXCEPT {
    if (!field_count || !fp_count || !int_count) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    a_type_ptr t = type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (!t || *field_count > 2) return GCC_GEN_BE_SUCCESS;

    if (t->kind == tk_class || t->kind == tk_struct) {
        a_class_type_supplement_ptr ctsp = t->variant.class_struct_union.extra_info;
        if (ctsp) {
            for (a_base_class_ptr bcp = ctsp->base_classes; bcp != NULL; bcp = bcp->next) {
                if (bcp->direct && bcp->type && size_of_type(bcp->type) > 0) {
                    GCC_GEN_BE_CHECK(flatten_riscv64_fields(bcp->type, field_count, fp_count, int_count, leaf_types));
                }
            }
        }
        for (a_field_ptr f = t->variant.class_struct_union.field_list; f != NULL; f = f->next) {
            if (f->is_bit_field && f->bit_size == 0) continue;
            GCC_GEN_BE_CHECK(flatten_riscv64_fields(f->type, field_count, fp_count, int_count, leaf_types));
        }
        return GCC_GEN_BE_SUCCESS;
    }

    if (t->kind == tk_array) {
        long arr_len = (long)t->variant.array.variant.number_of_elements;
        if (arr_len <= 0 || arr_len > 2) {
            *field_count += 3;
            return GCC_GEN_BE_SUCCESS;
        }
        for (long i = 0; i < arr_len; ++i) {
            GCC_GEN_BE_CHECK(flatten_riscv64_fields(t->variant.array.element_type, field_count, fp_count, int_count, leaf_types));
        }
        return GCC_GEN_BE_SUCCESS;
    }

    if (leaf_types && *field_count >= 0 && *field_count < 2) {
        leaf_types[*field_count] = t;
    }
    (*field_count)++;
    if (t->kind == tk_float && size_of_type(t) <= 8) {
        (*fp_count)++;
    } else if ((t->kind == tk_integer || t->kind == tk_pointer) && size_of_type(t) <= 8) {
        (*int_count)++;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Classifies a type according to the RISC-V LP64D ABI.
 * @details Determines whether an aggregate is passed by reference (`> 2 * XLEN` = 16 bytes),
 * in hardware floating-point registers (`fp+fp` or `fp+int` for 1-2 field structs `<= 16` bytes),
 * or in general-purpose integer registers (`<= 16` bytes).
 * @param[in] type The frontend type to classify.
 * @param[out] out_is_ref Set to TRUE if passed by reference (`> 16` bytes), FALSE otherwise.
 * @param[out] out_uses_fp_regs Optional pointer set to TRUE if passed in `fa0..fa7` FP registers.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_classify_riscv64(
    a_type_ptr type,
    a_boolean *out_is_ref,
    a_boolean *out_uses_fp_regs) GCC_GEN_BE_NOEXCEPT {
    if (!type || !out_is_ref) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_is_ref = FALSE;
    if (out_uses_fp_regs) *out_uses_fp_regs = FALSE;

    a_type_ptr t = type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (!t) return GCC_GEN_BE_SUCCESS;

    long sz = (long)size_of_type(t);
    if (sz > 16) {
        *out_is_ref = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }

    if (t->kind == tk_float && sz <= 8) {
        if (out_uses_fp_regs) *out_uses_fp_regs = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }

    if (t->kind == tk_class || t->kind == tk_struct) {
        int field_count = 0;
        int fp_count = 0;
        int int_count = 0;
        GCC_GEN_BE_CHECK(flatten_riscv64_fields(t, &field_count, &fp_count, &int_count, NULL));
        if ((field_count == 1 && fp_count == 1) ||
            (field_count == 2 && fp_count >= 1 && (fp_count + int_count) == 2)) {
            if (out_uses_fp_regs) *out_uses_fp_regs = TRUE;
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Classifies a type according to the 32-bit x86 (`i386` System V) ABI.
 * @details Determines if an aggregate must be returned via hidden `sret` pointer on the stack.
 * @param[in] type The frontend type to classify.
 * @param[out] out_is_sret Set to TRUE if returned via hidden stack pointer, FALSE otherwise.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_classify_i386(
    a_type_ptr type,
    a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT {
    if (!type || !out_is_sret) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_is_sret = FALSE;

    a_type_ptr t = type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (!t) return GCC_GEN_BE_SUCCESS;

    if (t->kind == tk_class || t->kind == tk_struct || t->kind == tk_union) {
        *out_is_sret = TRUE;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Classifies a type according to the PowerPC64 ELFv2 (`ppc64le`) ABI.
 * @details Determines if a type is a homogeneous float/vector aggregate (up to 8 elements)
 * or fits in general-purpose registers (`<= 16` bytes for return, `<= 64` bytes for parameters).
 * @param[in] type The frontend type to classify.
 * @param[out] out_is_hfa Set to TRUE if homogeneous float/vector aggregate.
 * @param[out] out_is_sret Set to TRUE if returned via hidden `r3` memory buffer (`> 16` bytes and non-HFA).
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_classify_ppc64le(
    a_type_ptr type,
    a_boolean *out_is_hfa,
    a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT {
    if (!type || !out_is_hfa || !out_is_sret) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_is_hfa = FALSE;
    *out_is_sret = FALSE;

    a_type_ptr t = type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (!t) return GCC_GEN_BE_SUCCESS;

    a_boolean hfa = FALSE;
    if (t->kind == tk_class || t->kind == tk_struct || t->kind == tk_array) {
        long elem_size = 0;
        int elem_kind = 0;
        int count = 0;
        a_boolean valid = FALSE;
        GCC_GEN_BE_CHECK(check_hfa_hva_recursive(t, 8, &elem_size, &elem_kind, &count, &valid));
        if (valid && count >= 1 && count <= 8) {
            hfa = TRUE;
        }
    }
    *out_is_hfa = hfa;

    long sz = (long)size_of_type(t);
    if ((t->kind == tk_class || t->kind == tk_struct || t->kind == tk_union) &&
        !hfa && sz > 16) {
        *out_is_sret = TRUE;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Alias classifier for the PowerPC64 ELFv2 ABI (`gcc_gen_be_classify_ppc64_elfv2`).
 * @details Delegates to `gcc_gen_be_classify_ppc64le`.
 * @param[in] type The frontend type to classify.
 * @param[out] out_is_hfa Set to TRUE if homogeneous float/vector aggregate.
 * @param[out] out_is_sret Set to TRUE if returned via hidden `r3` memory buffer (`> 16` bytes and non-HFA).
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_classify_ppc64_elfv2(
    a_type_ptr type,
    a_boolean *out_is_hfa,
    a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT {
    return gcc_gen_be_classify_ppc64le(type, out_is_hfa, out_is_sret);
}

/**
 * @brief Classifies a type according to the IBM z/Architecture 64-bit ELF ABI (`s390x`).
 * @details Determines whether an aggregate is passed by hidden reference (`> 8` bytes or non-power-of-two size)
 * or in a single floating-point register (1-field float/double struct).
 * @param[in] type The frontend type to classify.
 * @param[out] out_is_ref Set to TRUE if passed/returned by hidden reference, FALSE otherwise.
 * @param[out] out_is_fp_reg Optional pointer set to TRUE if passed in a floating-point register.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_classify_s390x(
    a_type_ptr type,
    a_boolean *out_is_ref,
    a_boolean *out_is_fp_reg) GCC_GEN_BE_NOEXCEPT {
    if (!type || !out_is_ref) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_is_ref = FALSE;
    if (out_is_fp_reg) *out_is_fp_reg = FALSE;

    a_type_ptr t = type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (!t) return GCC_GEN_BE_SUCCESS;

    long sz = (long)size_of_type(t);
    if (sz > 8 || (sz != 1 && sz != 2 && sz != 4 && sz != 8)) {
        *out_is_ref = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }

    if (t->kind == tk_class || t->kind == tk_struct) {
        int fcnt = 0;
        int fpcnt = 0;
        int icnt = 0;
        GCC_GEN_BE_CHECK(flatten_riscv64_fields(t, &fcnt, &fpcnt, &icnt, NULL));
        if (fcnt == 1 && fpcnt == 1) {
            if (out_is_fp_reg) *out_is_fp_reg = TRUE;
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/** @brief Context-backed active target ABI override (0 means auto-detect from host/target, or `abi + 1`). */
#define active_target_abi_override (gcc_gen_be_singleton_state.active_target_abi_override)

/**
 * @brief Queries the active target ABI kind.
 *
 * @details Determines the active target ABI from explicit override or host/target macros.
 *
 * @param[out] out_abi Pointer to receive the active target ABI kind.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT if out_abi is NULL.
 */
gcc_gen_be_error_t gcc_gen_be_get_target_abi(gcc_gen_be_target_abi_t *out_abi) GCC_GEN_BE_NOEXCEPT {
    if (!out_abi) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (active_target_abi_override >= ((int)GCC_GEN_BE_TARGET_ABI_SYSV_AMD64 + 1) &&
        active_target_abi_override <= ((int)GCC_GEN_BE_TARGET_ABI_AAPCS32 + 1)) {
        *out_abi = (gcc_gen_be_target_abi_t)(active_target_abi_override - 1);
        return GCC_GEN_BE_SUCCESS;
    }
#if defined(_WIN32) || defined(_WIN64)
    *out_abi = GCC_GEN_BE_TARGET_ABI_WIN64;
#elif defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
    *out_abi = GCC_GEN_BE_TARGET_ABI_DARWIN_ARM64;
#elif defined(__aarch64__) || defined(__arm64__)
    *out_abi = GCC_GEN_BE_TARGET_ABI_AAPCS64;
#elif defined(__riscv) && (__riscv_xlen == 64)
    *out_abi = GCC_GEN_BE_TARGET_ABI_RISCV64_LP64D;
#elif defined(__i386__)
    *out_abi = GCC_GEN_BE_TARGET_ABI_SYSV_I386;
#elif defined(__powerpc64__)
    *out_abi = GCC_GEN_BE_TARGET_ABI_PPC64LE_ELFV2;
#elif defined(__s390x__)
    *out_abi = GCC_GEN_BE_TARGET_ABI_S390X;
#elif defined(__arm__)
    *out_abi = GCC_GEN_BE_TARGET_ABI_AAPCS32;
#else
    *out_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
#endif
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Sets the active target ABI kind.
 *
 * @details Overrides the active target ABI for cross-compilation or target-specific testing.
 *
 * @param[in] abi The target ABI kind to activate.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT if abi is invalid.
 */
gcc_gen_be_error_t gcc_gen_be_set_target_abi(gcc_gen_be_target_abi_t abi) GCC_GEN_BE_NOEXCEPT {
    if ((int)abi < (int)GCC_GEN_BE_TARGET_ABI_SYSV_AMD64 || (int)abi > (int)GCC_GEN_BE_TARGET_ABI_AAPCS32) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    active_target_abi_override = (int)abi + 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Determines if a type must be passed by invisible reference.
 * @details Checks if the type is non-trivially copyable (`passed_via_copy_constructor`) or classified
 * as indirect/memory by the active target ABI.
 * @param[in] type The frontend type.
 * @param[out] out_is_ref Set to TRUE if it must be passed by invisible reference, FALSE otherwise.
 * @return gcc_gen_be_error_t GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT {
    if (!out_is_ref) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_is_ref = FALSE;
    if (!type) return GCC_GEN_BE_SUCCESS;
    while (type && type->kind == tk_typeref) type = type->variant.typeref.type;
    if (!type) return GCC_GEN_BE_SUCCESS;
    if (type->kind != tk_class && type->kind != tk_struct && type->kind != tk_union) return GCC_GEN_BE_SUCCESS;
    extern a_boolean is_trivially_copyable_type(a_type_ptr tp);
    if (!is_trivially_copyable_type(type) ||
        type->variant.class_struct_union.any_virtual_functions_including_in_base_classes ||
        type->variant.class_struct_union.any_virtual_base_classes) {
        *out_is_ref = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_gen_be_target_abi_t target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_target_abi(&target_abi));

    long sz = (long)size_of_type(type);
    if (target_abi == GCC_GEN_BE_TARGET_ABI_WIN64) {
        a_boolean win_ref = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_classify_win64(type, &win_ref));
        *out_is_ref = win_ref;
    } else if (target_abi == GCC_GEN_BE_TARGET_ABI_RISCV64_LP64D) {
        a_boolean rv_ref = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_classify_riscv64(type, &rv_ref, NULL));
        *out_is_ref = rv_ref;
    } else if (target_abi == GCC_GEN_BE_TARGET_ABI_AAPCS64 || target_abi == GCC_GEN_BE_TARGET_ABI_DARWIN_ARM64) {
        a_boolean is_hfa = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs64(type, &is_hfa));
        if (target_abi == GCC_GEN_BE_TARGET_ABI_DARWIN_ARM64) {
            a_boolean is_stack = FALSE;
            GCC_GEN_BE_CHECK(gcc_gen_be_classify_darwin_arm64(type, &is_stack));
        }
        if (!is_hfa && sz > 16) {
            *out_is_ref = TRUE;
        }
    } else if (target_abi == GCC_GEN_BE_TARGET_ABI_S390X) {
        a_boolean s390_fp = FALSE;
        a_boolean s390_ref = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_classify_s390x(type, &s390_fp, &s390_ref));
        *out_is_ref = s390_ref;
    } else {
        gcc_gen_be_abi_class_t classes[2];
        GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(type, classes));
        /* On System V AMD64, aggregates > 16 bytes are classified as MEMORY */
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Determines if a type must be returned via sret.
 * @details Checks if the type is non-trivially copyable (`passed_via_copy_constructor`) or classified
 * as memory return by the active target ABI.
 * @param[in] type The frontend type.
 * @param[out] out_is_sret Set to TRUE if it must be returned via sret, FALSE otherwise.
 * @return gcc_gen_be_error_t GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_is_returned_via_sret(a_type_ptr type, a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT {
    if (!out_is_sret) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_is_sret = FALSE;
    if (!type) return GCC_GEN_BE_SUCCESS;
    while (type && type->kind == tk_typeref) type = type->variant.typeref.type;
    if (!type) return GCC_GEN_BE_SUCCESS;
    if (type->kind != tk_class && type->kind != tk_struct && type->kind != tk_union) return GCC_GEN_BE_SUCCESS;

    extern a_boolean is_trivially_copyable_type(a_type_ptr tp);
    if (!is_trivially_copyable_type(type) ||
        type->variant.class_struct_union.any_virtual_functions_including_in_base_classes ||
        type->variant.class_struct_union.any_virtual_base_classes) {
        *out_is_sret = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_gen_be_target_abi_t target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_target_abi(&target_abi));

    long sz = (long)size_of_type(type);
    if (target_abi == GCC_GEN_BE_TARGET_ABI_WIN64) {
        a_boolean win_ref = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_classify_win64(type, &win_ref));
        *out_is_sret = win_ref;
    } else if (target_abi == GCC_GEN_BE_TARGET_ABI_RISCV64_LP64D) {
        a_boolean rv_ref = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_classify_riscv64(type, &rv_ref, NULL));
        *out_is_sret = rv_ref;
    } else if (target_abi == GCC_GEN_BE_TARGET_ABI_SYSV_I386) {
        a_boolean i386_sret = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_classify_i386(type, &i386_sret));
        *out_is_sret = i386_sret;
    } else if (target_abi == GCC_GEN_BE_TARGET_ABI_AAPCS32) {
        a_boolean aapcs32_sret = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs32(type, &aapcs32_sret, NULL));
        *out_is_sret = aapcs32_sret;
    } else if (target_abi == GCC_GEN_BE_TARGET_ABI_PPC64LE_ELFV2) {
        a_boolean ppc_hfa = FALSE;
        a_boolean ppc_sret = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_classify_ppc64le(type, &ppc_hfa, &ppc_sret));
        *out_is_sret = ppc_sret;
    } else if (target_abi == GCC_GEN_BE_TARGET_ABI_S390X) {
        a_boolean s390_fp = FALSE;
        a_boolean s390_ref = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_classify_s390x(type, &s390_fp, &s390_ref));
        *out_is_sret = s390_ref;
    } else if (target_abi == GCC_GEN_BE_TARGET_ABI_AAPCS64 || target_abi == GCC_GEN_BE_TARGET_ABI_DARWIN_ARM64) {
        a_boolean is_hfa = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs64(type, &is_hfa));
        if (!is_hfa && sz > 16) {
            *out_is_sret = TRUE;
        }
    } else {
        gcc_gen_be_abi_class_t classes[2];
        GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(type, classes));
        if (classes[0] == GCC_GEN_BE_ABI_CLASS_MEMORY || sz > 16) {
            *out_is_sret = TRUE;
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Classifies Darwin ARM64 ABI rules.
 * @details Distinguishes fixed parameters (which pass in registers up to 16 bytes or HFA/HVA up to 4 elements,
 * and indirect/stack when larger) from variadic parameters (which are always stack-allocated without GP/FP save areas).
 * @param[in] type Type.
 * @param[out] out_is_stack TRUE if passed on stack (e.g. variadic or non-HFA > 16 bytes).
 * @return GCC_GEN_BE_SUCCESS
 */
gcc_gen_be_error_t gcc_gen_be_classify_darwin_arm64(a_type_ptr type, a_boolean *out_is_stack) GCC_GEN_BE_NOEXCEPT {
    if (!type || !out_is_stack) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_is_stack = FALSE;
    
    a_type_ptr t = type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (!t) return GCC_GEN_BE_SUCCESS;
    
    a_boolean is_hfa = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs64(t, &is_hfa));
    long sz = (long)size_of_type(t);
    if (!is_hfa && (t->kind == tk_class || t->kind == tk_struct || t->kind == tk_union) && sz > 16) {
        *out_is_stack = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }
    /* Darwin ARM64 variadic promotion places anonymous arguments on the stack without register save areas;
       fixed parameters <= 16 bytes or HFA/HVA fit in registers when registers are available, while variadic calls
       and overflow arguments pack tightly on the stack. */
    *out_is_stack = TRUE;
    
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Classifies a variadic (`va_arg`) argument type for the active target ABI.
 * @details Computes whether the argument is passed in the stack overflow area (`out_is_stack`),
 * and for register-save-area targets (System V AMD64, Linux AAPCS64), computes the number of
 * general-purpose registers (`out_num_gp`, 0..2) and floating-point/vector registers (`out_num_fp`, 0..4).
 * @param[in] type The frontend argument type.
 * @param[out] out_is_stack Pointer to receive TRUE if passed exclusively on the stack/overflow area.
 * @param[out] out_num_gp Optional pointer to receive the count of GP registers required.
 * @param[out] out_num_fp Optional pointer to receive the count of FP/SIMD registers required.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_classify_va_arg(
    a_type_ptr type,
    a_boolean *out_is_stack,
    int *out_num_gp,
    int *out_num_fp) GCC_GEN_BE_NOEXCEPT {
    if (!type || !out_is_stack) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_is_stack = FALSE;
    if (out_num_gp) *out_num_gp = 0;
    if (out_num_fp) *out_num_fp = 0;

    a_type_ptr t = type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (!t) return GCC_GEN_BE_SUCCESS;

    gcc_gen_be_target_abi_t target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_target_abi(&target_abi));

    if (target_abi == GCC_GEN_BE_TARGET_ABI_DARWIN_ARM64 ||
        target_abi == GCC_GEN_BE_TARGET_ABI_WIN64 ||
        target_abi == GCC_GEN_BE_TARGET_ABI_SYSV_I386) {
        *out_is_stack = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }

    if (target_abi == GCC_GEN_BE_TARGET_ABI_RISCV64_LP64D) {
        long sz = (long)size_of_type(t);
        if (sz > 16) {
            *out_is_stack = TRUE;
        } else {
            if (out_num_gp) *out_num_gp = (sz > 8) ? 2 : 1;
        }
        return GCC_GEN_BE_SUCCESS;
    }

    if (target_abi == GCC_GEN_BE_TARGET_ABI_AAPCS64) {
        a_boolean is_hfa = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs64(t, &is_hfa));
        long sz = (long)size_of_type(t);
        if (is_hfa) {
            long elem_sz = 0;
            int elem_kind = 0;
            int count = 0;
            a_boolean valid = FALSE;
            GCC_GEN_BE_CHECK(check_hfa_hva_recursive(t, 4, &elem_sz, &elem_kind, &count, &valid));
            if (out_num_fp) *out_num_fp = count > 0 ? count : 1;
            return GCC_GEN_BE_SUCCESS;
        }
        if (t->kind == tk_float || t->kind == tk_vector
#ifndef EDG_GCC_BE_ISOLATED_TEST
            || t->kind == tk_complex
#endif
           ) {
#ifdef EDG_GCC_BE_ISOLATED_TEST
            if (out_num_fp) *out_num_fp = (t->kind == tk_float && t->variant.float_supplement.is_complex) ? 2 : 1;
#else
            if (out_num_fp) *out_num_fp = (t->kind == tk_complex) ? 2 : 1;
#endif
            return GCC_GEN_BE_SUCCESS;
        }
        if (sz > 16) {
            *out_is_stack = TRUE;
            return GCC_GEN_BE_SUCCESS;
        }
        if (out_num_gp) *out_num_gp = (sz > 8) ? 2 : 1;
        return GCC_GEN_BE_SUCCESS;
    }

    /* System V AMD64 */
    gcc_gen_be_abi_class_t classes[2];
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(t, classes));
    if (classes[0] == GCC_GEN_BE_ABI_CLASS_MEMORY ||
        classes[0] == GCC_GEN_BE_ABI_CLASS_X87 ||
        classes[0] == GCC_GEN_BE_ABI_CLASS_COMPLEX_X87) {
        *out_is_stack = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }

    int gp = 0;
    int fp = 0;
    for (int i = 0; i < 2; ++i) {
        if (classes[i] == GCC_GEN_BE_ABI_CLASS_INTEGER) gp++;
        else if (classes[i] == GCC_GEN_BE_ABI_CLASS_SSE) fp++;
    }
    if (out_num_gp) *out_num_gp = gp;
    if (out_num_fp) *out_num_fp = fp;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_coerced_aapcs64_type(
    a_type_ptr type,
    gcc_jit_type **out_coerced_type,
    a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT {
    if (!type || !out_coerced_type || !out_needs_coercion) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_coerced_type = NULL;
    *out_needs_coercion = FALSE;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    a_type_ptr t = type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (!t) return GCC_GEN_BE_ERROR_COERCION_FAILED;

    if (t->kind != tk_class && t->kind != tk_struct && t->kind != tk_union && t->kind != tk_array) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(t, out_coerced_type));
        return GCC_GEN_BE_SUCCESS;
    }

    long sz = (long)size_of_type(t);
    if (sz <= 0 || sz > 64) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(t, out_coerced_type));
        return GCC_GEN_BE_SUCCESS;
    }

    long elem_sz = 0;
    int elem_kind = 0;
    int count = 0;
    a_boolean is_hfa = FALSE;
    GCC_GEN_BE_CHECK(check_hfa_hva_recursive(t, 4, &elem_sz, &elem_kind, &count, &is_hfa));
    if (is_hfa && count >= 1 && count <= 4) {
        gcc_jit_type *lane_type = NULL;
        if (elem_kind >= 200) {
            gcc_jit_type *u32_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT32_T);
            lane_type = gcc_jit_type_get_vector(u32_t, (size_t)(elem_sz / 4));
        } else if (elem_sz == 4) {
            lane_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
        } else if (elem_sz == 8) {
            lane_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
        } else {
            lane_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
        }
        gcc_jit_field *hfa_fields[4];
        for (int i = 0; i < count; ++i) {
            char fname[32];
            snprintf(fname, sizeof(fname), "hfa_%d", i);
            hfa_fields[i] = gcc_jit_context_new_field(ctx, NULL, lane_type, fname);
        }
        gcc_jit_struct *s = gcc_jit_context_new_struct_type(ctx, NULL, "__aapcs64_hfa_coerced", count, hfa_fields);
        *out_coerced_type = gcc_jit_struct_as_type(s);
        *out_needs_coercion = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }

    if (sz <= 16) {
        gcc_jit_type *u64_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT64_T);
        int num_regs = (sz > 8) ? 2 : 1;
        gcc_jit_field *gp_fields[2];
        for (int i = 0; i < num_regs; ++i) {
            char fname[32];
            snprintf(fname, sizeof(fname), "gp_%d", i);
            gp_fields[i] = gcc_jit_context_new_field(ctx, NULL, u64_t, fname);
        }
        gcc_jit_struct *s = gcc_jit_context_new_struct_type(ctx, NULL, "__aapcs64_gp_coerced", num_regs, gp_fields);
        *out_coerced_type = gcc_jit_struct_as_type(s);
        *out_needs_coercion = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(t, out_coerced_type));
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Computes the coerced register-transfer ABI type for RISC-V LP64D (`fa0`-`fa7` / `a0`-`a7`).
 * @details Flattens structs up to 16 bytes containing one or two floating-point/integer leaf fields
 * into a 1- or 2-field coerced struct (`fp`, `fp+fp`, `fp+gp`, or `gp+fp`), or into `uint64_t` integer
 * register pairs for non-FP composites `<= 16` bytes.
 * @param[in] type The frontend type to inspect.
 * @param[out] out_coerced_type Pointer to receive the coerced `gcc_jit_type`.
 * @param[out] out_needs_coercion Pointer to receive TRUE if explicit register coercion is required, FALSE otherwise.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_coerced_riscv64_type(
    a_type_ptr type,
    gcc_jit_type **out_coerced_type,
    a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT {
    if (!type || !out_coerced_type || !out_needs_coercion) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_coerced_type = NULL;
    *out_needs_coercion = FALSE;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    a_type_ptr t = type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (!t) return GCC_GEN_BE_ERROR_COERCION_FAILED;

    long sz = (long)size_of_type(t);
    if ((t->kind != tk_class && t->kind != tk_struct && t->kind != tk_union) || sz <= 0 || sz > 16) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(t, out_coerced_type));
        return GCC_GEN_BE_SUCCESS;
    }

    if (t->kind == tk_class || t->kind == tk_struct) {
        int field_count = 0;
        int fp_count = 0;
        int int_count = 0;
        a_type_ptr leaves[2] = { NULL, NULL };
        GCC_GEN_BE_CHECK(flatten_riscv64_fields(t, &field_count, &fp_count, &int_count, leaves));
        if ((field_count == 1 && fp_count == 1) ||
            (field_count == 2 && fp_count >= 1 && (fp_count + int_count) == 2)) {
            gcc_jit_field *rv_fields[2];
            for (int i = 0; i < field_count; ++i) {
                gcc_jit_type *ft = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(leaves[i], &ft));
                char fname[32];
                snprintf(fname, sizeof(fname), "rv_fp_gp_%d", i);
                rv_fields[i] = gcc_jit_context_new_field(ctx, NULL, ft, fname);
            }
            gcc_jit_struct *s = gcc_jit_context_new_struct_type(ctx, NULL, "__riscv64_fp_coerced", field_count, rv_fields);
            *out_coerced_type = gcc_jit_struct_as_type(s);
            *out_needs_coercion = TRUE;
            return GCC_GEN_BE_SUCCESS;
        }
    }

    gcc_jit_type *u64_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT64_T);
    int num_regs = (sz > 8) ? 2 : 1;
    gcc_jit_field *gp_fields[2];
    for (int i = 0; i < num_regs; ++i) {
        char fname[32];
        snprintf(fname, sizeof(fname), "rv_gp_%d", i);
        gp_fields[i] = gcc_jit_context_new_field(ctx, NULL, u64_t, fname);
    }
    gcc_jit_struct *s = gcc_jit_context_new_struct_type(ctx, NULL, "__riscv64_gp_coerced", num_regs, gp_fields);
    *out_coerced_type = gcc_jit_struct_as_type(s);
    *out_needs_coercion = TRUE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Computes the coerced register-transfer ABI type for PowerPC64 ELFv2 (`ppc64le`).
 * @details Coerces Homogeneous Floating-Point and Vector Aggregates (HFA/HVA) of up to 8 elements
 * into a homogeneous lane struct, and non-HFA composites `<= 16` bytes into `uint64_t` register pairs.
 * @param[in] type The frontend type to inspect.
 * @param[out] out_coerced_type Pointer to receive the coerced `gcc_jit_type`.
 * @param[out] out_needs_coercion Pointer to receive TRUE if explicit register coercion is required, FALSE otherwise.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_coerced_ppc64le_type(
    a_type_ptr type,
    gcc_jit_type **out_coerced_type,
    a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT {
    if (!type || !out_coerced_type || !out_needs_coercion) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_coerced_type = NULL;
    *out_needs_coercion = FALSE;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    a_type_ptr t = type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (!t) return GCC_GEN_BE_ERROR_COERCION_FAILED;

    if (t->kind != tk_class && t->kind != tk_struct && t->kind != tk_union && t->kind != tk_array) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(t, out_coerced_type));
        return GCC_GEN_BE_SUCCESS;
    }

    long sz = (long)size_of_type(t);
    if (sz <= 0 || sz > 128) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(t, out_coerced_type));
        return GCC_GEN_BE_SUCCESS;
    }

    long elem_sz = 0;
    int elem_kind = 0;
    int count = 0;
    a_boolean is_hfa = FALSE;
    GCC_GEN_BE_CHECK(check_hfa_hva_recursive(t, 8, &elem_sz, &elem_kind, &count, &is_hfa));
    if (is_hfa && count >= 1 && count <= 8) {
        gcc_jit_type *lane_type = NULL;
        if (elem_kind >= 200) {
            gcc_jit_type *u32_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT32_T);
            lane_type = gcc_jit_type_get_vector(u32_t, (size_t)(elem_sz / 4));
        } else if (elem_sz == 4) {
            lane_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
        } else if (elem_sz == 8) {
            lane_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
        } else {
            lane_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
        }
        gcc_jit_field *hfa_fields[8];
        for (int i = 0; i < count; ++i) {
            char fname[32];
            snprintf(fname, sizeof(fname), "ppc_hfa_%d", i);
            hfa_fields[i] = gcc_jit_context_new_field(ctx, NULL, lane_type, fname);
        }
        gcc_jit_struct *s = gcc_jit_context_new_struct_type(ctx, NULL, "__ppc64le_hfa_coerced", count, hfa_fields);
        *out_coerced_type = gcc_jit_struct_as_type(s);
        *out_needs_coercion = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }

    if (sz <= 16) {
        gcc_jit_type *u64_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT64_T);
        int num_regs = (sz > 8) ? 2 : 1;
        gcc_jit_field *gp_fields[2];
        for (int i = 0; i < num_regs; ++i) {
            char fname[32];
            snprintf(fname, sizeof(fname), "ppc_gp_%d", i);
            gp_fields[i] = gcc_jit_context_new_field(ctx, NULL, u64_t, fname);
        }
        gcc_jit_struct *s = gcc_jit_context_new_struct_type(ctx, NULL, "__ppc64le_gp_coerced", num_regs, gp_fields);
        *out_coerced_type = gcc_jit_struct_as_type(s);
        *out_needs_coercion = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(t, out_coerced_type));
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_get_coerced_abi_type(
    a_type_ptr type,
    gcc_jit_type **out_coerced_type,
    a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT {
    if (!type || !out_coerced_type || !out_needs_coercion) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_coerced_type = NULL;
    *out_needs_coercion = FALSE;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_gen_be_target_abi_t target_abi = GCC_GEN_BE_TARGET_ABI_SYSV_AMD64;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_target_abi(&target_abi));

    a_type_ptr t = type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (!t) return GCC_GEN_BE_ERROR_COERCION_FAILED;

    void *cache_key = (void *)((uintptr_t)t ^ (((uintptr_t)target_abi + 1u) << 3));
    void *cached_coerced = NULL;
    if (cache_lookup(GCC_GEN_BE_CACHE_TYPE, cache_key, &cached_coerced) == GCC_GEN_BE_SUCCESS && cached_coerced) {
        *out_coerced_type = (gcc_jit_type *)cached_coerced;
        *out_needs_coercion = TRUE;
        return GCC_GEN_BE_SUCCESS;
    }

    if (target_abi == GCC_GEN_BE_TARGET_ABI_AAPCS64 || target_abi == GCC_GEN_BE_TARGET_ABI_DARWIN_ARM64) {
        GCC_GEN_BE_CHECK(gcc_gen_be_coerced_aapcs64_type(type, out_coerced_type, out_needs_coercion));
        if (*out_needs_coercion && *out_coerced_type) {
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, cache_key, *out_coerced_type));
        }
        return GCC_GEN_BE_SUCCESS;
    }
    if (target_abi == GCC_GEN_BE_TARGET_ABI_RISCV64_LP64D) {
        GCC_GEN_BE_CHECK(gcc_gen_be_coerced_riscv64_type(type, out_coerced_type, out_needs_coercion));
        if (*out_needs_coercion && *out_coerced_type) {
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, cache_key, *out_coerced_type));
        }
        return GCC_GEN_BE_SUCCESS;
    }
    if (target_abi == GCC_GEN_BE_TARGET_ABI_PPC64LE_ELFV2) {
        GCC_GEN_BE_CHECK(gcc_gen_be_coerced_ppc64le_type(type, out_coerced_type, out_needs_coercion));
        if (*out_needs_coercion && *out_coerced_type) {
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, cache_key, *out_coerced_type));
        }
        return GCC_GEN_BE_SUCCESS;
    }

    if (target_abi == GCC_GEN_BE_TARGET_ABI_WIN64) {
        long sz = (long)size_of_type(t);
        if ((t->kind == tk_class || t->kind == tk_struct || t->kind == tk_union) &&
            (sz == 1 || sz == 2 || sz == 4 || sz == 8)) {
            *out_coerced_type = gcc_jit_context_get_int_type(ctx, (int)sz, 0);
            *out_needs_coercion = TRUE;
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, cache_key, *out_coerced_type));
            return GCC_GEN_BE_SUCCESS;
        }
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(t, out_coerced_type));
        return GCC_GEN_BE_SUCCESS;
    }

    /* System V AMD64 eightbyte coercion */
    gcc_gen_be_abi_class_t classes[2];
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(t, classes));

    if (classes[0] == GCC_GEN_BE_ABI_CLASS_X87) {
        *out_coerced_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
        return GCC_GEN_BE_SUCCESS;
    }
    if (classes[0] == GCC_GEN_BE_ABI_CLASS_COMPLEX_X87) {
        *out_coerced_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_COMPLEX_LONG_DOUBLE);
        return GCC_GEN_BE_SUCCESS;
    }
    if (classes[0] == GCC_GEN_BE_ABI_CLASS_SSE && classes[1] == GCC_GEN_BE_ABI_CLASS_SSEUP) {
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(t, out_coerced_type));
        return GCC_GEN_BE_SUCCESS;
    }

    if ((t->kind == tk_class || t->kind == tk_struct || t->kind == tk_union) &&
        classes[0] != GCC_GEN_BE_ABI_CLASS_MEMORY) {
        long sz = (long)size_of_type(t);
        if (sz > 0 && sz <= 16) {
            int num_eb = (sz > 8 && classes[1] != GCC_GEN_BE_ABI_CLASS_NO_CLASS) ? 2 : 1;
            gcc_jit_field *eb_fields[2];
            for (int i = 0; i < num_eb; ++i) {
                gcc_jit_type *eb_type = (classes[i] == GCC_GEN_BE_ABI_CLASS_SSE)
                    ? gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE)
                    : gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT64_T);
                char fname[32];
                snprintf(fname, sizeof(fname), "eb_%d", i);
                eb_fields[i] = gcc_jit_context_new_field(ctx, NULL, eb_type, fname);
            }
            gcc_jit_struct *s = gcc_jit_context_new_struct_type(ctx, NULL, "__sysv_amd64_coerced", num_eb, eb_fields);
            *out_coerced_type = gcc_jit_struct_as_type(s);
            *out_needs_coercion = TRUE;
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, cache_key, *out_coerced_type));
            return GCC_GEN_BE_SUCCESS;
        }
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(t, out_coerced_type));
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_coerce_rvalue(
    gcc_jit_function *func,
    gcc_jit_block *block,
    gcc_jit_rvalue *src_rval,
    gcc_jit_type *dst_type,
    gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!func || !block || !src_rval || !dst_type || !out_rval) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_rval = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    gcc_jit_type *src_type = gcc_jit_rvalue_get_type(src_rval);
    if (src_type == dst_type) {
        *out_rval = src_rval;
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_lvalue *tmp_src = gcc_jit_function_new_local(func, NULL, src_type, "__abi_coerce_src");
    gcc_jit_block_add_assignment(block, NULL, tmp_src, src_rval);
    gcc_jit_type *dst_ptr_type = gcc_jit_type_get_pointer(dst_type);
    gcc_jit_rvalue *cast_addr = gcc_jit_context_new_cast(
        ctx, NULL, gcc_jit_lvalue_get_address(tmp_src, NULL), dst_ptr_type);
    *out_rval = gcc_jit_lvalue_as_rvalue(gcc_jit_rvalue_dereference(cast_addr, NULL));
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_coerce_Arg_to_registers(
    gcc_jit_function *func,
    gcc_jit_block *block,
    a_type_ptr arg_type,
    gcc_jit_rvalue *src_rval,
    gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!func || !block || !arg_type || !src_rval || !out_rval) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_rval = NULL;

    gcc_jit_type *coerced_type = NULL;
    a_boolean needs_coercion = FALSE;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_coerced_abi_type(arg_type, &coerced_type, &needs_coercion));
    if (!needs_coercion || !coerced_type) {
        *out_rval = src_rval;
        return GCC_GEN_BE_SUCCESS;
    }
    return gcc_gen_be_coerce_rvalue(func, block, src_rval, coerced_type, out_rval);
}

gcc_gen_be_error_t gcc_gen_be_unpack_coerced_Arg(
    gcc_jit_function *func,
    gcc_jit_block *block,
    a_type_ptr param_type,
    gcc_jit_rvalue *param_rval,
    gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    if (!func || !block || !param_type || !param_rval || !out_lval) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_lval = NULL;

    gcc_jit_type *agg_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(param_type, &agg_type));
    if (!agg_type) return GCC_GEN_BE_ERROR_COERCION_FAILED;

    gcc_jit_rvalue *unpacked_rval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_coerce_rvalue(func, block, param_rval, agg_type, &unpacked_rval));
    gcc_jit_lvalue *agg_local = gcc_jit_function_new_local(func, NULL, agg_type, "__abi_unpacked_arg");
    gcc_jit_block_add_assignment(block, NULL, agg_local, unpacked_rval);
    *out_lval = agg_local;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_pack_coerced_argument(
    gcc_jit_function *func,
    gcc_jit_block *block,
    a_type_ptr arg_type,
    gcc_jit_rvalue *src_rval,
    gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    return gcc_gen_be_coerce_Arg_to_registers(func, block, arg_type, src_rval, out_rval);
}

gcc_gen_be_error_t gcc_gen_be_unpack_coerced_parameter(
    gcc_jit_function *func,
    gcc_jit_block *block,
    a_type_ptr param_type,
    gcc_jit_rvalue *param_rval,
    gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    return gcc_gen_be_unpack_coerced_Arg(func, block, param_type, param_rval, out_lval);
}

gcc_gen_be_error_t gcc_gen_be_pack_coerced_return(
    gcc_jit_function *func,
    gcc_jit_block *block,
    a_type_ptr ret_type,
    gcc_jit_rvalue *ret_rval,
    gcc_jit_rvalue **out_coerced_rval) GCC_GEN_BE_NOEXCEPT {
    return gcc_gen_be_coerce_Arg_to_registers(func, block, ret_type, ret_rval, out_coerced_rval);
}

gcc_gen_be_error_t gcc_gen_be_unpack_coerced_return(
    gcc_jit_function *func,
    gcc_jit_block *block,
    a_type_ptr ret_type,
    gcc_jit_rvalue *coerced_rval,
    gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT {
    return gcc_gen_be_unpack_coerced_Arg(func, block, ret_type, coerced_rval, out_lval);
}

gcc_gen_be_error_t gcc_gen_be_classify_aapcs32(
    a_type_ptr type,
    a_boolean *out_is_sret,
    a_boolean *out_even_odd_pair) GCC_GEN_BE_NOEXCEPT {
    if (!type || !out_is_sret) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_is_sret = FALSE;
    if (out_even_odd_pair) *out_even_odd_pair = FALSE;

    a_type_ptr t = type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (!t) return GCC_GEN_BE_SUCCESS;

    long sz = (long)size_of_type(t);
    long align = (long)t->alignment;
    if ((t->kind == tk_class || t->kind == tk_struct || t->kind == tk_union) && sz > 4) {
        *out_is_sret = TRUE;
    }
    if (out_even_odd_pair && (align >= 8 || (t->kind == tk_integer && sz >= 8))) {
        *out_even_odd_pair = TRUE;
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_init_abi_reg_state(
    gcc_gen_be_target_abi_t abi,
    a_boolean has_sret,
    gcc_gen_be_abi_reg_state_t *out_state) GCC_GEN_BE_NOEXCEPT {
    if (!out_state || (int)abi < (int)GCC_GEN_BE_TARGET_ABI_SYSV_AMD64 || (int)abi > (int)GCC_GEN_BE_TARGET_ABI_AAPCS32) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    out_state->target_abi = abi;
    out_state->gp_used = 0;
    out_state->fp_used = 0;

    if (abi == GCC_GEN_BE_TARGET_ABI_AAPCS64 || abi == GCC_GEN_BE_TARGET_ABI_DARWIN_ARM64) {
        /* AArch64 uses dedicated x8 for sret, leaving x0..x7 and v0..v7 untouched */
        out_state->gp_limit = 8;
        out_state->fp_limit = 8;
    } else if (abi == GCC_GEN_BE_TARGET_ABI_AAPCS32) {
        out_state->gp_limit = 4;
        out_state->fp_limit = 16;
        out_state->gp_used = has_sret ? 1 : 0;
    } else if (abi == GCC_GEN_BE_TARGET_ABI_WIN64) {
        out_state->gp_limit = 4;
        out_state->fp_limit = 4;
        out_state->gp_used = has_sret ? 1 : 0;
        out_state->fp_used = out_state->gp_used;
    } else if (abi == GCC_GEN_BE_TARGET_ABI_RISCV64_LP64D) {
        out_state->gp_limit = 8;
        out_state->fp_limit = 8;
        out_state->gp_used = has_sret ? 1 : 0;
    } else if (abi == GCC_GEN_BE_TARGET_ABI_PPC64LE_ELFV2) {
        out_state->gp_limit = 8;
        out_state->fp_limit = 13;
        out_state->gp_used = has_sret ? 1 : 0;
    } else if (abi == GCC_GEN_BE_TARGET_ABI_SYSV_I386) {
        out_state->gp_limit = 0;
        out_state->fp_limit = 0;
    } else if (abi == GCC_GEN_BE_TARGET_ABI_S390X) {
        out_state->gp_limit = 5;
        out_state->fp_limit = 4;
        out_state->gp_used = has_sret ? 1 : 0;
    } else {
        /* System V AMD64: rdi, rsi, rdx, rcx, r8, r9 (6 GP), xmm0..xmm7 (8 SSE) */
        out_state->gp_limit = 6;
        out_state->fp_limit = 8;
        out_state->gp_used = has_sret ? 1 : 0;
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_allocate_abi_param_regs(
    gcc_gen_be_abi_reg_state_t *inout_state,
    a_type_ptr param_type,
    a_boolean *out_in_registers,
    a_boolean *out_by_ref) GCC_GEN_BE_NOEXCEPT {
    if (!inout_state || !param_type || !out_in_registers || !out_by_ref) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_in_registers = FALSE;
    *out_by_ref = FALSE;

    a_type_ptr t = param_type;
    while (t && t->kind == tk_typeref) t = t->variant.typeref.type;
    if (!t) return GCC_GEN_BE_SUCCESS;

    extern a_boolean is_trivially_copyable_type(a_type_ptr tp);
    if ((t->kind == tk_class || t->kind == tk_struct || t->kind == tk_union) &&
        (!is_trivially_copyable_type(t) ||
         t->variant.class_struct_union.any_virtual_functions_including_in_base_classes ||
         t->variant.class_struct_union.any_virtual_base_classes)) {
        *out_by_ref = TRUE;
        if (inout_state->gp_used < inout_state->gp_limit) {
            inout_state->gp_used++;
            if (inout_state->target_abi == GCC_GEN_BE_TARGET_ABI_WIN64) {
                inout_state->fp_used = inout_state->gp_used;
            }
            *out_in_registers = TRUE;
        }
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_gen_be_target_abi_t abi = inout_state->target_abi;
    long sz = (long)size_of_type(t);

    if (abi == GCC_GEN_BE_TARGET_ABI_WIN64) {
        a_boolean win_ref = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_classify_win64(t, &win_ref));
        *out_by_ref = win_ref;
        if (inout_state->gp_used < inout_state->gp_limit) {
            inout_state->gp_used++;
            inout_state->fp_used = inout_state->gp_used;
            *out_in_registers = TRUE;
        }
        return GCC_GEN_BE_SUCCESS;
    }

    if (abi == GCC_GEN_BE_TARGET_ABI_AAPCS64 || abi == GCC_GEN_BE_TARGET_ABI_DARWIN_ARM64) {
        a_boolean is_hfa = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs64(t, &is_hfa));
        if (is_hfa) {
            long elem_sz = 0;
            int elem_kind = 0;
            int count = 0;
            a_boolean valid = FALSE;
            GCC_GEN_BE_CHECK(check_hfa_hva_recursive(t, 4, &elem_sz, &elem_kind, &count, &valid));
            if (inout_state->fp_used + count <= inout_state->fp_limit) {
                inout_state->fp_used += count;
                *out_in_registers = TRUE;
            } else {
                inout_state->fp_used = inout_state->fp_limit;
            }
            return GCC_GEN_BE_SUCCESS;
        }
        if ((t->kind == tk_class || t->kind == tk_struct || t->kind == tk_union) && sz > 16) {
            /* AAPCS64 B.4: composites > 16 bytes are copied by caller and passed by pointer */
            *out_by_ref = TRUE;
            if (inout_state->gp_used < inout_state->gp_limit) {
                inout_state->gp_used++;
                *out_in_registers = TRUE;
            }
            return GCC_GEN_BE_SUCCESS;
        }
        int needed_gp = (sz > 8) ? 2 : 1;
        if (inout_state->gp_used + needed_gp <= inout_state->gp_limit) {
            inout_state->gp_used += needed_gp;
            *out_in_registers = TRUE;
        } else {
            inout_state->gp_used = inout_state->gp_limit;
        }
        return GCC_GEN_BE_SUCCESS;
    }

    if (abi == GCC_GEN_BE_TARGET_ABI_AAPCS32) {
        a_boolean is_sret = FALSE;
        a_boolean even_odd = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_classify_aapcs32(t, &is_sret, &even_odd));
        if (even_odd && (inout_state->gp_used & 1) != 0) {
            inout_state->gp_used++;
        }
        int words = (int)((sz + 3) / 4);
        if (words < 1) words = 1;
        if (inout_state->gp_used + words <= inout_state->gp_limit) {
            inout_state->gp_used += words;
            *out_in_registers = TRUE;
        } else {
            inout_state->gp_used = inout_state->gp_limit;
        }
        return GCC_GEN_BE_SUCCESS;
    }

    if (abi == GCC_GEN_BE_TARGET_ABI_RISCV64_LP64D) {
        a_boolean rv_ref = FALSE;
        a_boolean rv_fp = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_classify_riscv64(t, &rv_ref, &rv_fp));
        if (rv_ref) {
            *out_by_ref = TRUE;
            if (inout_state->gp_used < inout_state->gp_limit) {
                inout_state->gp_used++;
                *out_in_registers = TRUE;
            }
            return GCC_GEN_BE_SUCCESS;
        }
        if (rv_fp && inout_state->fp_used < inout_state->fp_limit) {
            inout_state->fp_used++;
            *out_in_registers = TRUE;
            return GCC_GEN_BE_SUCCESS;
        }
        int needed_gp = (sz > 8) ? 2 : 1;
        if (inout_state->gp_used + needed_gp <= inout_state->gp_limit) {
            inout_state->gp_used += needed_gp;
            *out_in_registers = TRUE;
        }
        return GCC_GEN_BE_SUCCESS;
    }

    if (abi == GCC_GEN_BE_TARGET_ABI_PPC64LE_ELFV2) {
        a_boolean ppc_hfa = FALSE;
        a_boolean ppc_sret = FALSE;
        GCC_GEN_BE_CHECK(gcc_gen_be_classify_ppc64le(t, &ppc_hfa, &ppc_sret));
        if (ppc_hfa) {
            long elem_sz = 0;
            int elem_kind = 0;
            int count = 0;
            a_boolean valid = FALSE;
            GCC_GEN_BE_CHECK(check_hfa_hva_recursive(t, 8, &elem_sz, &elem_kind, &count, &valid));
            if (inout_state->fp_used + count <= inout_state->fp_limit) {
                inout_state->fp_used += count;
                *out_in_registers = TRUE;
            }
            return GCC_GEN_BE_SUCCESS;
        }
        int needed_gp = (int)((sz + 7) / 8);
        if (needed_gp < 1) needed_gp = 1;
        if (inout_state->gp_used + needed_gp <= inout_state->gp_limit) {
            inout_state->gp_used += needed_gp;
            *out_in_registers = TRUE;
        }
        return GCC_GEN_BE_SUCCESS;
    }

    /* System V AMD64 cumulative register tracking */
    gcc_gen_be_abi_class_t classes[2];
    GCC_GEN_BE_CHECK(gcc_gen_be_classify_sysv_amd64(t, classes));
    if (classes[0] == GCC_GEN_BE_ABI_CLASS_MEMORY ||
        classes[0] == GCC_GEN_BE_ABI_CLASS_X87 ||
        classes[0] == GCC_GEN_BE_ABI_CLASS_COMPLEX_X87) {
        return GCC_GEN_BE_SUCCESS;
    }
    int need_gp = 0;
    int need_fp = 0;
    for (int i = 0; i < 2; ++i) {
        if (classes[i] == GCC_GEN_BE_ABI_CLASS_INTEGER) need_gp++;
        else if (classes[i] == GCC_GEN_BE_ABI_CLASS_SSE) need_fp++;
    }
    if (inout_state->gp_used + need_gp <= inout_state->gp_limit &&
        inout_state->fp_used + need_fp <= inout_state->fp_limit) {
        inout_state->gp_used += need_gp;
        inout_state->fp_used += need_fp;
        *out_in_registers = TRUE;
    }
    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
