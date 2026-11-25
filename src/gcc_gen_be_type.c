/**
 * @file gcc_gen_be_type.c
 * @brief Implementation of type lowering for the GCC backend.
 * @details Lowers EDG IL scalar, pointer, array, struct/class/union, routine,
 * pointer-to-member, vector, complex, decimal float, fixed-point, and bit-precise
 * types into `libgccjit` types using dynamically resizing deferred-struct queues.
 * @ingroup gcc_gen_be
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_type.h"
#include "gcc_gen_be_abi.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_location.h"
#include "gcc_gen_be_debug.h"
#include "gcc_gen_be_lib_loader.h"
#include "error.h"
#include "expr.h" /* for value_of_integer_constant if needed, though vectors might use variant.integer_value */
#include <libgccjit.h>
#include <stdlib.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

/**
 * @brief Tracks an opaque struct whose field population has been deferred across a pointer edge.
 * @details Prevents cycles through pointer fields from attempting to instantiate by-value fields
 *          or arrays of an enclosing struct whose `gcc_jit_struct_set_fields` has not completed yet.
 */
typedef struct {
    a_type_ptr tp;              /**< @brief The EDG struct/class type node. */
    gcc_jit_struct *jit_struct; /**< @brief The opaque `gcc_jit_struct` handle. */
    gcc_jit_type *jit_type;     /**< @brief The `gcc_jit_type` view of `jit_struct`. */
    a_boolean in_progress;      /**< @brief `TRUE` while `populate_struct_fields` is active on `tp`. */
    a_boolean completed;        /**< @brief `TRUE` once `gcc_jit_struct_set_fields` has completed. */
} deferred_struct_entry_t;

/** @brief Active recursion depth of by-value struct field lowering. */
static int struct_field_lowering_depth = 0;
/** @brief Last seen `gcc_jit_context` pointer used to reset `deferred_structs_count` on new contexts. */
static gcc_jit_context *last_type_ctx = NULL;

/**
 * @brief Appends a deferred struct entry to the dynamically resizing `deferred_structs` queue.
 * @details Doubles heap capacity via `realloc` when full and writes the allocated index to `*out_idx`.
 * @param[in] tp The EDG struct/class type node.
 * @param[in] jit_struct The opaque `gcc_jit_struct` handle.
 * @param[in] jit_type The `gcc_jit_type` view of `jit_struct`.
 * @param[in] in_progress Initial `in_progress` flag.
 * @param[out] out_idx Optional pointer receiving the appended entry index.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM` on allocation failure.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t push_deferred_struct(
    a_type_ptr tp,
    gcc_jit_struct *jit_struct,
    gcc_jit_type *jit_type,
    a_boolean in_progress,
    int *out_idx) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (gcc_gen_be_singleton_state.deferred_structs_count >=
        gcc_gen_be_singleton_state.deferred_structs_capacity) {
        int new_cap = (gcc_gen_be_singleton_state.deferred_structs_capacity == 0)
            ? 64
            : (gcc_gen_be_singleton_state.deferred_structs_capacity * 2);
        void *new_buf = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_realloc_buf(
            gcc_gen_be_singleton_state.deferred_structs,
            (size_t)new_cap * sizeof(deferred_struct_entry_t),
            &new_buf));
        gcc_gen_be_singleton_state.deferred_structs = new_buf;
        gcc_gen_be_singleton_state.deferred_structs_capacity = new_cap;
    }
    deferred_struct_entry_t *arr =
        (deferred_struct_entry_t *)gcc_gen_be_singleton_state.deferred_structs;
    int idx = gcc_gen_be_singleton_state.deferred_structs_count++;
    arr[idx].tp = tp;
    arr[idx].jit_struct = jit_struct;
    arr[idx].jit_type = jit_type;
    arr[idx].in_progress = in_progress;
    arr[idx].completed = FALSE;
    if (out_idx) *out_idx = idx;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Computes the byte size of an EDG type after unwrapping `tk_typeref` nodes.
 * @details Walks any chain of `tk_typeref` aliases and writes the underlying `tp->size` to `*out_size`.
 * @param[in] tp The EDG type pointer (may be NULL).
 * @param[out] out_size Non-null pointer to receive the size in bytes (0 if `tp` is NULL).
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM` on injected fault.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t edg_type_byte_size(
    a_type_ptr tp,
    unsigned long *out_size) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    while (tp && tp->kind == tk_typeref) {
        tp = tp->variant.typeref.type;
    }
    *out_size = tp ? (unsigned long)tp->size : 0UL;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Populates the fields of an opaque struct or union type in topological by-value order.
 * @details Increments `struct_field_lowering_depth` while lowering member fields and base classes,
 *          calls `gcc_jit_struct_set_fields` (or `gcc_jit_context_new_union_type`), and drains any
 *          pointer-referenced structs queued in `deferred_structs` once the outermost struct completes.
 * @param[in] ctx The active `gcc_jit_context`.
 * @param[in] tp The EDG struct, class, or union type.
 * @param[in] s The opaque `gcc_jit_struct` to populate.
 * @param[in,out] io_res Pointer to the resulting `gcc_jit_type*`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code on failure.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t populate_struct_fields(
    gcc_jit_context *ctx,
    a_type_ptr tp,
    gcc_jit_struct *s,
    gcc_jit_type **io_res) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    gcc_gen_be_error_t err = GCC_GEN_BE_SUCCESS;
    a_const_char *name = tp->source_corresp.name ? tp->source_corresp.name : "unnamed_struct";
    gcc_jit_type *res = *io_res;

    struct_field_lowering_depth++;

    typedef struct {
        int is_base;
        void *ptr; /* a_field_ptr or a_base_class_ptr */
        unsigned long offset;
        unsigned int bit_rem;
    } layout_entry_t;

    int num_entries = 0;
    a_boolean has_lowered_base_fields = FALSE;
    for (a_field_ptr f = tp->variant.class_struct_union.field_list; f != NULL; f = f->next) {
        num_entries++;
        if (f->source_corresp.name && strncmp(f->source_corresp.name, "__b_", 4) == 0) {
            has_lowered_base_fields = TRUE;
        }
#ifndef EDG_GCC_BE_ISOLATED_TEST
        if (f->is_lowered_base_class) {
            has_lowered_base_fields = TRUE;
        }
#endif
    }
    if (!has_lowered_base_fields && (tp->kind == tk_class || tp->kind == tk_struct)) {
        a_class_type_supplement_ptr extra = tp->variant.class_struct_union.extra_info;
        if (extra) {
            for (a_base_class_ptr b = extra->direct_base_classes; b != NULL; b = b->next_direct) num_entries++;
            for (a_base_class_ptr b = extra->base_classes; b != NULL; b = b->next) {
                if (b->is_virtual && !b->direct) num_entries++;
            }
        }
    }

    layout_entry_t *entries = NULL;
    gcc_jit_field **fields = NULL;
    int max_fields = num_entries * 2 + 1;
    if (num_entries > 0) {
        void *raw_entries = NULL;
        GCC_GEN_BE_CHECK_CLEANUP(
            gcc_gen_be_alloc((size_t)num_entries * sizeof(layout_entry_t), &raw_entries),
            err, populate_fail);
        entries = (layout_entry_t *)raw_entries;

        int i = 0;
        for (a_field_ptr f = tp->variant.class_struct_union.field_list; f != NULL; f = f->next) {
            entries[i].is_base = 0;
            entries[i].ptr = f;
            entries[i].offset = (unsigned long)f->offset;
            entries[i].bit_rem = f->offset_bit_remainder;
            i++;
        }
        if (!has_lowered_base_fields && (tp->kind == tk_class || tp->kind == tk_struct)) {
            a_class_type_supplement_ptr extra = tp->variant.class_struct_union.extra_info;
            if (extra) {
                for (a_base_class_ptr b = extra->direct_base_classes; b != NULL; b = b->next_direct) {
                    entries[i].is_base = 1;
                    entries[i].ptr = b;
                    entries[i].offset = (unsigned long)b->offset;
                    entries[i].bit_rem = 0;
                    i++;
                }
                for (a_base_class_ptr b = extra->base_classes; b != NULL; b = b->next) {
                    if (b->is_virtual && !b->direct) {
                        entries[i].is_base = 1;
                        entries[i].ptr = b;
                        entries[i].offset = (unsigned long)b->offset;
                        entries[i].bit_rem = 0;
                        i++;
                    }
                }
            }
        }

        for (int pass = 0; pass < num_entries - 1; pass++) {
            for (int j = 0; j < num_entries - pass - 1; j++) {
                if (entries[j].offset > entries[j + 1].offset ||
                    (entries[j].offset == entries[j + 1].offset && entries[j].bit_rem > entries[j + 1].bit_rem)) {
                    layout_entry_t tmp = entries[j];
                    entries[j] = entries[j + 1];
                    entries[j + 1] = tmp;
                }
            }
        }
    }

    {
        void *raw_fields = NULL;
        GCC_GEN_BE_CHECK_CLEANUP(
            gcc_gen_be_alloc((size_t)max_fields * sizeof(gcc_jit_field *), &raw_fields),
            err, populate_fail);
        fields = (gcc_jit_field **)raw_fields;

        int num_jit_fields = 0;
        unsigned long current_offset = 0;

        for (int k = 0; k < num_entries; k++) {
            layout_entry_t *entry = &entries[k];

            if (entry->offset > current_offset && tp->kind != tk_union) {
                unsigned long pad_size = entry->offset - current_offset;
                gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
                gcc_jit_type *pad_type = gcc_jit_context_new_array_type(ctx, NULL, char_type, (int)pad_size);
                char pad_name[32];
                snprintf(pad_name, sizeof(pad_name), "__pad_%lu", current_offset);
                fields[num_jit_fields++] = gcc_jit_context_new_field(ctx, NULL, pad_type, pad_name);
                current_offset = entry->offset;
            }

            if (entry->is_base) {
                a_base_class_ptr b = (a_base_class_ptr)entry->ptr;
                gcc_jit_type *btype = NULL;
                GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_lower_type(b->type, &btype), err, populate_fail);

                if (!b->is_optimized_empty_base) {
                    char bname[64];
                    a_const_char *tname = b->type->source_corresp.name;
                    snprintf(bname, sizeof(bname), "__base_%s", tname ? tname : "unnamed");

                    unsigned long full_bsize = 0UL;
                    GCC_GEN_BE_CHECK_CLEANUP(edg_type_byte_size(b->type, &full_bsize), err, populate_fail);
                    unsigned long bsize = full_bsize;
                    if (b->type->kind == tk_class || b->type->kind == tk_struct) {
                        a_class_type_supplement_ptr b_extra = b->type->variant.class_struct_union.extra_info;
                        if (b_extra && b_extra->size_without_virtual_base_classes > 0 &&
                            b_extra->size_without_virtual_base_classes < bsize) {
                            bsize = (unsigned long)b_extra->size_without_virtual_base_classes;
                        }
                    }
                    if (k + 1 < num_entries && entries[k + 1].offset < entry->offset + bsize) {
                        bsize = entries[k + 1].offset - entry->offset;
                    }

                    if (bsize == 0) {
                        /* Zero-size packed base subobject */
                    } else if (bsize < full_bsize) {
                        gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
                        gcc_jit_type *subobj_type = gcc_jit_context_new_array_type(ctx, NULL, char_type, (int)bsize);
                        fields[num_jit_fields] = gcc_jit_context_new_field(ctx, NULL, subobj_type, bname);
                        GCC_GEN_BE_CHECK_CLEANUP(cache_insert(GCC_GEN_BE_CACHE_FIELD, b, fields[num_jit_fields]), err, populate_fail);
                        num_jit_fields++;
                        current_offset = entry->offset + bsize;
                    } else {
                        fields[num_jit_fields] = gcc_jit_context_new_field(ctx, NULL, btype, bname);
                        GCC_GEN_BE_CHECK_CLEANUP(cache_insert(GCC_GEN_BE_CACHE_FIELD, b, fields[num_jit_fields]), err, populate_fail);
                        num_jit_fields++;
                        current_offset = entry->offset + bsize;
                    }
                }
            } else {
                a_field_ptr f = (a_field_ptr)entry->ptr;
                a_const_char *fname = f->source_corresp.name ? f->source_corresp.name : "unnamed_field";

                gcc_jit_type *ftype = NULL;
                GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_lower_type(f->type, &ftype), err, populate_fail);

                if (f->is_optimized_empty_class) {
                    /* Skip 0-size field */
                } else if (f->is_bit_field || f->bit_size > 0) {
                    a_boolean is_zero_width = (f->bit_size == 0) ? TRUE : FALSE;
                    unsigned long f_end = (unsigned long)f->offset +
                        (((unsigned long)f->offset_bit_remainder + (unsigned long)f->bit_size + 7UL) / 8UL);
                    a_boolean container_already_emitted = FALSE;
                    if (tp->kind != tk_union && current_offset >= f_end && current_offset > f->offset) {
                        container_already_emitted = TRUE;
                    }

                    if (!container_already_emitted && !is_zero_width) {
                        int m = k + 1;
                        while (m < num_entries && !entries[m].is_base) {
                            a_field_ptr fm = (a_field_ptr)entries[m].ptr;
                            if (!(fm->is_bit_field || fm->bit_size > 0)) break;
                            m++;
                        }
                        unsigned long limit_offset = (m < num_entries) ? entries[m].offset : (unsigned long)tp->size;
                        unsigned long max_bf_end = f_end;
                        unsigned long max_all_bf_end = f_end;
                        for (int bfi = k + 1; bfi < m; ++bfi) {
                            a_field_ptr nbf = (a_field_ptr)entries[bfi].ptr;
                            if (nbf->bit_size > 0) {
                                unsigned long nbf_end = (unsigned long)nbf->offset +
                                    (((unsigned long)nbf->offset_bit_remainder + (unsigned long)nbf->bit_size + 7UL) / 8UL);
                                if (nbf_end <= limit_offset) {
                                    if (nbf_end > max_all_bf_end) {
                                        max_all_bf_end = nbf_end;
                                    }
                                    if ((nbf_end - current_offset) <= 8UL && nbf_end > max_bf_end) {
                                        max_bf_end = nbf_end;
                                    }
                                }
                            }
                        }
                        unsigned long unit_sz = 0UL;
                        GCC_GEN_BE_CHECK_CLEANUP(edg_type_byte_size(f->type, &unit_sz), err, populate_fail);
                        if (unit_sz == 0 && gcc_jit_type_is_pointer(ftype) == NULL && gcc_jit_type_is_struct(ftype) == NULL) {
                            unit_sz = (unsigned long)gcc_jit_type_get_size(ftype);
                        }
                        if (unit_sz > 0 && unit_sz < 8 &&
                            ((unsigned long)f->offset_bit_remainder + (unsigned long)f->bit_size) > (unit_sz * 8UL) &&
                            (f->offset + 8UL) <= tp->size) {
                            ftype = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG_LONG);
                            unit_sz = 8UL;
                        }
                        unsigned long avail = (limit_offset > current_offset) ? (limit_offset - current_offset) : 1UL;
                        unsigned long req = max_bf_end - current_offset;
                        if (tp->kind != tk_union &&
                            (unit_sz == 0 || unit_sz > avail || (current_offset % unit_sz) != 0 || unit_sz < req)) {
                            if (req <= 1UL && max_all_bf_end == max_bf_end) {
                                unit_sz = 1UL;
                                ftype = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
                            } else if (req <= 2UL && max_all_bf_end == max_bf_end && avail >= 2UL && (current_offset % 2UL) == 0) {
                                unit_sz = 2UL;
                                ftype = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_SHORT);
                            } else if (req <= 4UL && max_all_bf_end == max_bf_end && avail >= 4UL && (current_offset % 4UL) == 0) {
                                unit_sz = 4UL;
                                ftype = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_INT);
                            } else if (req <= 8UL && max_all_bf_end == max_bf_end && avail >= 8UL && (current_offset % 8UL) == 0) {
                                unit_sz = 8UL;
                                ftype = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG_LONG);
                            } else {
                                unit_sz = 1UL;
                                ftype = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
                                max_bf_end = max_all_bf_end;
                            }
                        }
                        char cname[64];
                        unsigned long bf_start_off = current_offset;
                        int bf_start_jit_idx = num_jit_fields;
                        snprintf(cname, sizeof(cname), "__bf_container_%lu_%d", current_offset, num_jit_fields);
                        fields[num_jit_fields] = gcc_jit_context_new_field(ctx, NULL, ftype, cname);
                        GCC_GEN_BE_CHECK_CLEANUP(cache_insert(GCC_GEN_BE_CACHE_FIELD, f, fields[num_jit_fields]), err, populate_fail);
                        GCC_GEN_BE_CHECK_CLEANUP(cache_insert(GCC_GEN_BE_CACHE_TYPE, fields[num_jit_fields], ftype), err, populate_fail);
                        num_jit_fields++;
                        if (tp->kind != tk_union) {
                            current_offset += unit_sz;
                            unsigned long byte_ext_end = (unit_sz == 1UL) ? max_bf_end : f_end;
                            while (current_offset < byte_ext_end && current_offset < limit_offset) {
                                if (num_jit_fields + 2 >= max_fields) {
                                    max_fields = max_fields * 2 + 8;
                                    void *new_fields = NULL;
                                    GCC_GEN_BE_CHECK_CLEANUP(
                                        gcc_gen_be_realloc_buf(fields, (size_t)max_fields * sizeof(gcc_jit_field *), &new_fields),
                                        err, populate_fail);
                                    fields = (gcc_jit_field **)new_fields;
                                }
                                gcc_jit_type *uch_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR);
                                snprintf(cname, sizeof(cname), "__bf_cont_ext_%lu_%d", current_offset, num_jit_fields);
                                GCC_GEN_BE_CHECK_CLEANUP(cache_insert(GCC_GEN_BE_CACHE_TYPE, fields[num_jit_fields - 1], uch_t), err, populate_fail);
                                fields[num_jit_fields] = gcc_jit_context_new_field(ctx, NULL, uch_t, cname);
                                GCC_GEN_BE_CHECK_CLEANUP(cache_insert(GCC_GEN_BE_CACHE_TYPE, fields[num_jit_fields], uch_t), err, populate_fail);
                                num_jit_fields++;
                                current_offset += 1UL;
                            }
                            for (int nk = k + 1; nk < num_entries && !entries[nk].is_base; ++nk) {
                                a_field_ptr nf = (a_field_ptr)entries[nk].ptr;
                                if (!(nf->is_bit_field || nf->bit_size > 0)) break;
                                if ((unsigned long)nf->offset < current_offset && unit_sz == 1UL) {
                                    int map_idx = bf_start_jit_idx + (int)((unsigned long)nf->offset - bf_start_off);
                                    GCC_GEN_BE_CHECK_CLEANUP(cache_insert(GCC_GEN_BE_CACHE_FIELD, nf, fields[map_idx]), err, populate_fail);
                                }
                            }
                        }
                    } else if (num_jit_fields > 0) {
                        void *existing_bf_f = NULL;
                        GCC_GEN_BE_CHECK_CLEANUP(cache_lookup(GCC_GEN_BE_CACHE_FIELD, f, &existing_bf_f), err, populate_fail);
                        if (!existing_bf_f) {
                            GCC_GEN_BE_CHECK_CLEANUP(cache_insert(GCC_GEN_BE_CACHE_FIELD, f, fields[num_jit_fields - 1]), err, populate_fail);
                        }
                    }
                } else {
                    unsigned long fsize = 0UL;
                    GCC_GEN_BE_CHECK_CLEANUP(edg_type_byte_size(f->type, &fsize), err, populate_fail);
                    a_type_ptr f_tp_raw = f->type;
                    while (f_tp_raw->kind == tk_typeref && f_tp_raw->variant.typeref.type != NULL) {
                        f_tp_raw = f_tp_raw->variant.typeref.type;
                    }
#ifndef EDG_GCC_BE_ISOLATED_TEST
                    if ((f_tp_raw->kind == tk_class || f_tp_raw->kind == tk_struct || f_tp_raw->kind == tk_union) &&
                        f_tp_raw->alignment > 1U) {
                        unsigned long al = (unsigned long)f_tp_raw->alignment;
                        fsize = ((fsize + al - 1UL) / al) * al;
                    }
#endif
                    gcc_jit_type *emit_ftype = ftype;
                    if (tp->kind != tk_union && k + 1 < num_entries &&
                        entries[k + 1].offset > entry->offset &&
                        entries[k + 1].offset < entry->offset + fsize) {
                        fsize = entries[k + 1].offset - entry->offset;
                        gcc_jit_type *char_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
                        emit_ftype = gcc_jit_context_new_array_type(ctx, NULL, char_t, (int)fsize);
                    }
                    fields[num_jit_fields] = gcc_jit_context_new_field(ctx, NULL, emit_ftype, fname);
                    GCC_GEN_BE_CHECK_CLEANUP(cache_insert(GCC_GEN_BE_CACHE_FIELD, f, fields[num_jit_fields]), err, populate_fail);
                    if (has_lowered_base_fields && (tp->kind == tk_class || tp->kind == tk_struct) &&
                        tp->variant.class_struct_union.extra_info) {
                        a_class_type_supplement_ptr extra = tp->variant.class_struct_union.extra_info;
                        for (a_base_class_ptr b = extra->base_classes ? extra->base_classes : extra->direct_base_classes;
                             b != NULL;
                             b = extra->base_classes ? b->next : b->next_direct) {
                            if (b->offset == f->offset) {
                                GCC_GEN_BE_CHECK_CLEANUP(cache_insert(GCC_GEN_BE_CACHE_FIELD, b, fields[num_jit_fields]), err, populate_fail);
                            }
                        }
                    }
                    a_type_ptr f_tp = f_tp_raw;
                    if (f->is_anonymous_parent_object &&
                        (f_tp->kind == tk_union || f_tp->kind == tk_struct || f_tp->kind == tk_class)) {
                        for (a_field_ptr subf = f_tp->variant.class_struct_union.field_list; subf != NULL; subf = subf->next) {
                            void *sub_cached = NULL;
                            GCC_GEN_BE_CHECK_CLEANUP(cache_lookup(GCC_GEN_BE_CACHE_FIELD, subf, &sub_cached), err, populate_fail);
                            if (!sub_cached) {
                                GCC_GEN_BE_CHECK_CLEANUP(cache_insert(GCC_GEN_BE_CACHE_FIELD, subf, fields[num_jit_fields]), err, populate_fail);
                            }
                        }
                    }
                    num_jit_fields++;
                    current_offset = entry->offset + fsize;
                }
            }
        }

        if (current_offset < tp->size && tp->kind != tk_union) {
            unsigned long pad_size = (unsigned long)tp->size - current_offset;
            gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
            gcc_jit_type *pad_type = gcc_jit_context_new_array_type(ctx, NULL, char_type, (int)pad_size);
            fields[num_jit_fields++] = gcc_jit_context_new_field(ctx, NULL, pad_type, "__tail_pad");
        } else if (num_jit_fields == 0) {
            gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
            fields[num_jit_fields++] = gcc_jit_context_new_field(ctx, NULL, char_type, "__empty_pad");
        }

        free(entries);
        entries = NULL;

        if (tp->kind == tk_union) {
            res = gcc_jit_context_new_union_type(ctx, NULL, name, num_jit_fields, fields);
            GCC_GEN_BE_CHECK_CLEANUP(cache_insert(GCC_GEN_BE_CACHE_TYPE, tp, res), err, populate_fail);
            GCC_GEN_BE_CHECK_CLEANUP(cache_insert(GCC_GEN_BE_CACHE_TYPE, res, res), err, populate_fail);
        } else {
            gcc_jit_struct_set_fields(s, NULL, num_jit_fields, fields);
        }
        free(fields);
        fields = NULL;
    }

    GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_emit_type_debug_info(tp, res), err, populate_fail);

    struct_field_lowering_depth--;
    if (struct_field_lowering_depth == 0) {
        a_boolean progress = TRUE;
        while (progress) {
            progress = FALSE;
            for (int i = 0; i < gcc_gen_be_singleton_state.deferred_structs_count; ++i) {
                deferred_struct_entry_t *arr =
                    (deferred_struct_entry_t *)gcc_gen_be_singleton_state.deferred_structs;
                if (!arr[i].completed && !arr[i].in_progress) {
                    arr[i].in_progress = TRUE;
                    gcc_jit_type *def_type = arr[i].jit_type;
                    a_type_ptr def_tp = arr[i].tp;
                    gcc_jit_struct *def_struct = arr[i].jit_struct;
                    err = populate_struct_fields(ctx, def_tp, def_struct, &def_type);
                    arr = (deferred_struct_entry_t *)gcc_gen_be_singleton_state.deferred_structs;
                    arr[i].completed = TRUE;
                    arr[i].in_progress = FALSE;
                    if (err != GCC_GEN_BE_SUCCESS) return err;
                    progress = TRUE;
                }
            }
        }
    }

    *io_res = res;
    return GCC_GEN_BE_SUCCESS;

populate_fail:;
    free(fields);
    free(entries);
    struct_field_lowering_depth--;
    return err;
}

/**
 * @brief Lowers an EDG frontend type into a libgccjit type.
 * @details Translates any `a_type_ptr` into its corresponding `gcc_jit_type`, caching results
 *          and breaking struct pointer cycles via the dynamic `deferred_structs` queue.
 * @param[in] tp The frontend type to lower.
 * @param[out] out_type A pointer to a gcc_jit_type pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_type` populated.
 */
gcc_gen_be_error_t gcc_gen_be_lower_type(a_type_ptr tp, gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
  if (!out_type) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  *out_type = NULL;
  if (!tp) return GCC_GEN_BE_SUCCESS;
  
  gcc_jit_context *ctx = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
  if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  if (ctx != last_type_ctx) {
      last_type_ctx = ctx;
      gcc_gen_be_singleton_state.deferred_structs_count = 0;
      struct_field_lowering_depth = 0;
  }

  void *cached = NULL;
  gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_TYPE, tp, &cached);
  if (err != GCC_GEN_BE_SUCCESS) return err;
  if (cached) {
      gcc_jit_type *cached_type = (gcc_jit_type *)cached;
      if (tp->kind == tk_class || tp->kind == tk_struct) {
          for (int i = 0; i < gcc_gen_be_singleton_state.deferred_structs_count; ++i) {
              deferred_struct_entry_t *arr =
                  (deferred_struct_entry_t *)gcc_gen_be_singleton_state.deferred_structs;
              if (arr[i].tp == tp) {
                  if (!arr[i].completed && !arr[i].in_progress) {
                      arr[i].in_progress = TRUE;
                      gcc_jit_struct *def_struct = arr[i].jit_struct;
                      GCC_GEN_BE_CHECK(populate_struct_fields(ctx, tp, def_struct, &cached_type));
                      arr = (deferred_struct_entry_t *)gcc_gen_be_singleton_state.deferred_structs;
                      arr[i].completed = TRUE;
                      arr[i].in_progress = FALSE;
                  }
                  break;
              }
          }
      }
      *out_type = cached_type;
      return GCC_GEN_BE_SUCCESS;
  }
  
  if (tp->kind == tk_typeref) {
    gcc_jit_type *base = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(tp->variant.typeref.type, &base));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, tp, base));
    *out_type = base;
    return GCC_GEN_BE_SUCCESS;
  }

  gcc_jit_type *res = NULL;
  switch (tp->kind) {
    case tk_void:
      res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID); break;

    case tk_nullptr:
      res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR); break;
    
    case tk_integer:
      if (tp->variant.integer.bool_type) {
          res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL);
          break;
      }
#if LONG_LONG_ALLOWED
      if (tp->variant.integer.int_kind == ik_long_long) {
          res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_LONG);
          break;
      }
      if (tp->variant.integer.int_kind == ik_unsigned_long_long) {
          res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG_LONG);
          break;
      }
#endif
#if INT128_EXTENSIONS_ALLOWED
      if (tp->variant.integer.int_kind == ik_int128 ||
          tp->variant.integer.int_kind == ik_unsigned_int128) {
          int is_signed_128 = (tp->variant.integer.int_kind == ik_int128) ? 1 : 0;
          if (p_gcc_jit_context_get_int_type) {
              res = p_gcc_jit_context_get_int_type(ctx, 16, is_signed_128);
          }
          if (!res) {
#if defined(GCC_JIT_TYPE_INT128_T)
              res = gcc_jit_context_get_type(ctx, is_signed_128 ? GCC_JIT_TYPE_INT128_T : GCC_JIT_TYPE_UINT128_T);
#else
              gcc_jit_type *u64 = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG_LONG);
              res = gcc_jit_context_new_array_type(ctx, NULL, u64, 2);
#endif
          }
          break;
      }
#endif
      switch (tp->variant.integer.int_kind) {
        case ik_char: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR); break;
        case ik_signed_char: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIGNED_CHAR); break;
        case ik_unsigned_char: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR); break;
        case ik_short: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SHORT); break;
        case ik_unsigned_short: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_SHORT); break;
        case ik_int: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT); break;
        case ik_unsigned_int: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_INT); break;
        case ik_long: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG); break;
        case ik_unsigned_long: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG); break;
        case ik_bit_precise:
        case ik_unsigned_bit_precise:
            {
                int is_signed_bp = (tp->variant.integer.int_kind == ik_bit_precise) ? 1 : 0;
                unsigned long bits = (tp->variant.integer.extra_info && tp->variant.integer.extra_info->bit_width > 0)
                    ? (unsigned long)tp->variant.integer.extra_info->bit_width
                    : ((unsigned long)tp->size * 8UL);
                if (bits == 0UL || (is_signed_bp && bits < 2UL) || bits > 512UL) {
                    return GCC_GEN_BE_ERROR_BITINT_UNSUPPORTED;
                }
                if (bits <= 128UL) {
                    int cbytes = 1;
                    if (bits > 64UL) cbytes = 16;
                    else if (bits > 32UL) cbytes = 8;
                    else if (bits > 16UL) cbytes = 4;
                    else if (bits > 8UL) cbytes = 2;
                    if (p_gcc_jit_context_get_int_type) {
                        res = p_gcc_jit_context_get_int_type(ctx, cbytes, is_signed_bp);
                    }
                    if (!res) {
                        if (cbytes == 1) res = gcc_jit_context_get_type(ctx, is_signed_bp ? GCC_JIT_TYPE_INT8_T : GCC_JIT_TYPE_UINT8_T);
                        else if (cbytes == 2) res = gcc_jit_context_get_type(ctx, is_signed_bp ? GCC_JIT_TYPE_INT16_T : GCC_JIT_TYPE_UINT16_T);
                        else if (cbytes == 4) res = gcc_jit_context_get_type(ctx, is_signed_bp ? GCC_JIT_TYPE_INT32_T : GCC_JIT_TYPE_UINT32_T);
                        else if (cbytes == 8) res = gcc_jit_context_get_type(ctx, is_signed_bp ? GCC_JIT_TYPE_INT64_T : GCC_JIT_TYPE_UINT64_T);
                        else return GCC_GEN_BE_ERROR_BITINT_UNSUPPORTED;
                    }
                } else {
                    /* Multi-word struct representation for 129 <= N <= 512 */
                    int num_words = (int)((bits + 63UL) / 64UL);
                    gcc_jit_type *u64_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT64_T);
                    gcc_jit_type *words_arr_t = gcc_jit_context_new_array_type(ctx, NULL, u64_t, num_words);
                    gcc_jit_field *w_fld = gcc_jit_context_new_field(ctx, NULL, words_arr_t, "limbs");
                    char bp_sname[64];
                    snprintf(bp_sname, sizeof(bp_sname), "__edg_bitint_%s%lu", is_signed_bp ? "s" : "u", bits);
                    gcc_jit_struct *bp_st = gcc_jit_context_new_struct_type(ctx, NULL, bp_sname, 1, &w_fld);
                    res = gcc_jit_struct_as_type(bp_st);
                }
            }
            break;
        default: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT); break;
      }
      break;

#if FIXED_POINT_ALLOWED
    case tk_fixed_point:
      if (tp->size == 1) {
          res = gcc_jit_context_get_type(ctx, tp->variant.fixed_point.is_unsigned ? GCC_JIT_TYPE_UNSIGNED_CHAR : GCC_JIT_TYPE_SIGNED_CHAR);
      } else if (tp->size == 2) {
          res = gcc_jit_context_get_type(ctx, tp->variant.fixed_point.is_unsigned ? GCC_JIT_TYPE_UNSIGNED_SHORT : GCC_JIT_TYPE_SHORT);
      } else if (tp->size == 8) {
          res = gcc_jit_context_get_type(ctx, tp->variant.fixed_point.is_unsigned ? GCC_JIT_TYPE_UNSIGNED_LONG_LONG : GCC_JIT_TYPE_LONG_LONG);
      } else {
          res = gcc_jit_context_get_type(ctx, tp->variant.fixed_point.is_unsigned ? GCC_JIT_TYPE_UNSIGNED_INT : GCC_JIT_TYPE_INT);
      }
      break;
#endif

    case tk_float:
      switch (tp->variant.float_kind) {
        case fk_float: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT); break;
        case fk_double: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE); break;
        case fk_long_double: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE); break;
        case fk_std_bfloat16:
#if defined(GCC_JIT_TYPE_BFLOAT16)
          res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BFLOAT16);
#else
          res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
#endif
          break;
        case fk_float16:
        case fk_fp16:
        case fk_std_float16:
#if defined(GCC_JIT_TYPE_FLOAT16)
          res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT16);
#else
          res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
#endif
          break;
        case fk_float32x:
        case fk_std_float32:
#if defined(GCC_JIT_TYPE_FLOAT32)
          res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT32);
#else
          res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
#endif
          break;
        case fk_float64x:
        case fk_std_float64:
#if defined(GCC_JIT_TYPE_FLOAT64)
          res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT64);
#else
          res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
#endif
          break;
        case fk_float80:
#if defined(GCC_JIT_TYPE_FLOAT80)
          res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT80);
#else
          res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
#endif
          break;
        case fk_float128:
        case fk_std_float128:
#if defined(GCC_JIT_TYPE_FLOAT128)
          res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT128);
#else
          res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
#endif
          break;
        default: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE); break;
      }
      break;

    case tk_complex:
    case tk_imaginary:
      {
#if C99_IL_EXTENSIONS_SUPPORTED
          gcc_jit_type *elem = NULL;
          a_boolean use_struct_pair = FALSE;
          switch (tp->variant.float_kind) {
              case fk_float:
                  elem = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
                  break;
              case fk_double:
                  elem = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                  break;
              case fk_long_double:
              case fk_float80:
                  elem = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
                  break;
              case fk_float16:
              case fk_fp16:
              case fk_std_float16:
#if defined(GCC_JIT_TYPE_FLOAT16)
                  elem = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT16);
#else
                  elem = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
#endif
                  use_struct_pair = TRUE;
                  break;
              case fk_std_bfloat16:
#if defined(GCC_JIT_TYPE_BFLOAT16)
                  elem = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BFLOAT16);
#else
                  elem = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT);
#endif
                  use_struct_pair = TRUE;
                  break;
              case fk_float128:
              case fk_std_float128:
#if defined(GCC_JIT_TYPE_FLOAT128)
                  elem = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT128);
#else
                  elem = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
#endif
                  use_struct_pair = TRUE;
                  break;
              default:
                  /* GNU integer complex (`__complex__ int` etc.) or extended float fallback */
                  if (tp->size == 2) elem = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT8_T);
                  else if (tp->size == 4) elem = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT16_T);
                  else if (tp->size == 8) elem = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT32_T);
                  else if (tp->size == 16) elem = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT64_T);
                  else elem = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE);
                  use_struct_pair = TRUE;
                  break;
          }
          if (tp->kind == tk_imaginary) {
              res = elem;
          } else if (!use_struct_pair) {
              switch (tp->variant.float_kind) {
                  case fk_float: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_COMPLEX_FLOAT); break;
                  case fk_double: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_COMPLEX_DOUBLE); break;
                  default: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_COMPLEX_LONG_DOUBLE); break;
              }
          } else {
              gcc_jit_field *real_f = gcc_jit_context_new_field(ctx, NULL, elem, "real");
              gcc_jit_field *imag_f = gcc_jit_context_new_field(ctx, NULL, elem, "imag");
              gcc_jit_field *fields[2] = { real_f, imag_f };
              gcc_jit_struct *s = gcc_jit_context_new_struct_type(ctx, NULL, "__edg_complex_pair", 2, fields);
              res = gcc_jit_struct_as_type(s);
          }
#endif
          break;
      }

    case tk_pointer:
      {
          a_type_ptr pts_to = tp->variant.pointer.type;
          while (pts_to && pts_to->kind == tk_typeref) pts_to = pts_to->variant.typeref.type;
          if (!pts_to || pts_to->kind == tk_void) {
              res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
          } else if (struct_field_lowering_depth > 0 &&
                     (pts_to->kind == tk_class || pts_to->kind == tk_struct)) {
              void *pts_cached = NULL;
              GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_TYPE, pts_to, &pts_cached));
              gcc_jit_type *elem = (gcc_jit_type *)pts_cached;
              if (!elem) {
                  a_const_char *sname = pts_to->source_corresp.name ? pts_to->source_corresp.name : "unnamed_struct";
                  gcc_jit_location *sloc = NULL;
                  GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_type(pts_to, &sloc));
                  gcc_jit_struct *s = gcc_jit_context_new_opaque_struct(ctx, sloc, sname);
                  elem = gcc_jit_struct_as_type(s);
                  GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, pts_to, elem));
                  GCC_GEN_BE_CHECK(push_deferred_struct(pts_to, s, elem, FALSE, NULL));
              }
              res = gcc_jit_type_get_pointer(elem);
          } else {
              gcc_jit_type *elem = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(tp->variant.pointer.type, &elem));
              if (pts_to->kind == tk_routine ||
                  (pts_to->kind == tk_array && pts_to->variant.array.is_variable_size_array)) {
                  res = elem;
              } else {
                  res = gcc_jit_type_get_pointer(elem);
              }
          }
          break;
      }
      
    case tk_array:
      if (!tp->variant.array.is_variable_size_array && !tp->variant.array.is_template_dependent_size_array && tp->variant.array.variant.number_of_elements != 0) {
         int size = (int)tp->variant.array.variant.number_of_elements;
         gcc_jit_type *elem = NULL;
         GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(tp->variant.array.element_type, &elem));
         res = gcc_jit_context_new_array_type(ctx, NULL, elem, size);
      } else if (tp->variant.array.is_variable_size_array) {
         a_type_ptr leaf_tp = tp->variant.array.element_type;
         while (leaf_tp) {
             while (leaf_tp && leaf_tp->kind == tk_typeref) leaf_tp = leaf_tp->variant.typeref.type;
             if (leaf_tp && leaf_tp->kind == tk_array && leaf_tp->variant.array.is_variable_size_array) {
                 leaf_tp = leaf_tp->variant.array.element_type;
             } else {
                 break;
             }
         }
         gcc_jit_type *elem = NULL;
         GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(leaf_tp, &elem));
         res = gcc_jit_type_get_pointer(elem);
      } else {
         gcc_jit_type *elem = NULL;
         GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(tp->variant.array.element_type, &elem));
         /* C99 flexible array member (`T data[];`) or incomplete array (`extern T arr[];`): 0 elements (never fake 64!) */
         res = gcc_jit_context_new_array_type(ctx, NULL, elem, 0);
      }
      break;

    case tk_class:
    case tk_struct:
    case tk_union:
      {
         a_const_char *name = tp->source_corresp.name;
         if (!name) name = "unnamed_struct";
         
         gcc_jit_location *type_loc = NULL;
         GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_type(tp, &type_loc));
         gcc_jit_struct *s = gcc_jit_context_new_opaque_struct(ctx, type_loc, name);
         res = gcc_jit_struct_as_type(s);
         GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, tp, res));
         int def_idx = -1;
         if (tp->kind == tk_class || tp->kind == tk_struct) {
             GCC_GEN_BE_CHECK(push_deferred_struct(tp, s, res, TRUE, &def_idx));
         }
         GCC_GEN_BE_CHECK(populate_struct_fields(ctx, tp, s, &res));
         if (def_idx >= 0) {
             deferred_struct_entry_t *arr =
                 (deferred_struct_entry_t *)gcc_gen_be_singleton_state.deferred_structs;
             arr[def_idx].completed = TRUE;
             arr[def_idx].in_progress = FALSE;
         }
         *out_type = res;
         return GCC_GEN_BE_SUCCESS;
      }

    case tk_routine:
      {
         a_routine_type_supplement_ptr supp = tp->variant.routine.extra_info;
         a_param_type_ptr param_list = supp ? supp->param_type_list : NULL;
         gcc_jit_type *ret_type = NULL;
         a_type_ptr frontend_ret = tp->variant.routine.return_type;
         extern gcc_gen_be_error_t gcc_gen_be_is_returned_via_sret(a_type_ptr type, a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT;
         extern gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT;
         extern gcc_gen_be_error_t gcc_gen_be_get_coerced_abi_type(a_type_ptr type, struct gcc_jit_type **out_coerced_type, a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT;
         a_boolean uses_sret = FALSE;
         a_boolean edg_ret_as_param = FALSE;
#if DO_IL_LOWERING
         if (supp && supp->value_returned_as_parameter) {
             edg_ret_as_param = TRUE;
         }
#endif
         gcc_jit_type *sret_ptr_type = NULL;
         if (!edg_ret_as_param && frontend_ret) {
             GCC_GEN_BE_CHECK(gcc_gen_be_is_returned_via_sret(frontend_ret, &uses_sret));
         }
         if (edg_ret_as_param) {
             ret_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
         } else if (uses_sret) {
             ret_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
             gcc_jit_type *actual_ret = NULL;
             GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(frontend_ret, &actual_ret));
             sret_ptr_type = gcc_jit_type_get_pointer(actual_ret);
         } else {
             GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(frontend_ret, &ret_type));
             if (frontend_ret) {
                 gcc_jit_type *coerced_ret = NULL;
                 a_boolean ret_coerced = FALSE;
                 GCC_GEN_BE_CHECK(gcc_gen_be_get_coerced_abi_type(frontend_ret, &coerced_ret, &ret_coerced));
                 if (ret_coerced && coerced_ret) {
                     ret_type = coerced_ret;
                 }
             }
         }
         
         int num_params = uses_sret ? 1 : 0;
         a_param_type_ptr ptp;
         for (ptp = param_list; ptp != NULL; ptp = ptp->next) {
             num_params++;
         }
         
         gcc_jit_type **param_types = NULL;
         if (num_params > 0) {
             void *raw_ptypes = NULL;
             GCC_GEN_BE_CHECK(gcc_gen_be_alloc((size_t)num_params * sizeof(gcc_jit_type *), &raw_ptypes));
             param_types = (gcc_jit_type **)raw_ptypes;
             
             int i = 0;
             if (uses_sret) {
                 param_types[i++] = sret_ptr_type;
             }
             for (ptp = param_list; ptp != NULL; ptp = ptp->next) {
                 a_type_ptr ptype = ptp->type;
                 while (ptype && ptype->kind == tk_typeref) ptype = ptype->variant.typeref.type;
                 err = gcc_gen_be_lower_type(ptype, &param_types[i]);
                 if (err != GCC_GEN_BE_SUCCESS) { free(param_types); return err; }
                 if (ptype) {
                     a_boolean is_invis_ref = FALSE;
                     err = gcc_gen_be_is_passed_by_invisible_reference(ptype, &is_invis_ref);
                     if (err != GCC_GEN_BE_SUCCESS) { free(param_types); return err; }
                     if (is_invis_ref) {
                         param_types[i] = gcc_jit_type_get_pointer(gcc_jit_type_unqualified(param_types[i]));
                     } else {
                         gcc_jit_type *coerced_pt = NULL;
                         a_boolean needs_coercion = FALSE;
                         err = gcc_gen_be_get_coerced_abi_type(ptype, &coerced_pt, &needs_coercion);
                         if (err != GCC_GEN_BE_SUCCESS) { free(param_types); return err; }
                         if (needs_coercion && coerced_pt) {
                             param_types[i] = coerced_pt;
                         }
                     }
                 }
                 i++;
             }
         }
         
         int is_variadic = (supp && supp->has_ellipsis) ? 1 : 0;
         res = gcc_jit_context_new_function_ptr_type(ctx, NULL, ret_type, num_params, param_types, is_variadic);
         free(param_types);
         break;
      }
      
    case tk_ptr_to_member:
      {
         a_type_ptr member_type = tp->variant.ptr_to_member.type;
         while (member_type && member_type->kind == tk_typeref) member_type = member_type->variant.typeref.type;
         if (member_type && member_type->kind == tk_routine) {
             gcc_jit_field *f1 = gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR), "func_ptr");
             gcc_jit_field *f2 = gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG), "this_adj");
             gcc_jit_field *fields[] = {f1, f2};
             gcc_jit_struct *s = gcc_jit_context_new_struct_type(ctx, NULL, "ptr_to_member", 2, fields);
             res = gcc_jit_struct_as_type(s);
         } else {
             res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
         }
         break;
      }
      
    case tk_vector:
      {
#if GNU_VECTOR_TYPES_ALLOWED
         int num_elements = 1;
         a_type_ptr elem_tp = tp->variant.vector.element_type;
         a_type_ptr raw_elem_tp = elem_tp;
         while (raw_elem_tp && raw_elem_tp->kind == tk_typeref) {
            raw_elem_tp = raw_elem_tp->variant.typeref.type;
         }
         if (raw_elem_tp && raw_elem_tp->size > 0 && tp->size >= raw_elem_tp->size) {
            num_elements = (int)(tp->size / raw_elem_tp->size);
         }
         gcc_jit_type *elem = NULL;
         GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(elem_tp, &elem));
         if (!elem || elem == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL)) {
            elem = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT8_T);
         }
         if (p_gcc_jit_type_get_vector != NULL) {
            res = p_gcc_jit_type_get_vector(elem, (size_t)num_elements);
         } else {
            /* Fallback for older libgccjit that does not support vector types */
            res = gcc_jit_context_new_array_type(ctx, NULL, elem, num_elements);
         }
#endif
         break;
      }

    case tk_error:
      /* Ignore / Fallback to void */
      res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
      break;
      
    default:
      res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
      break;
  }
  
  GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, tp, res));
  *out_type = res;
  return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Creates a zero-initialized rvalue for the specified type.
 *
 * @details Creates a null pointer for pointers, a zero-initialized constructor
 * for aggregates, or a scalar zero value for numeric types.
 *
 * @param[in] tp The frontend type.
 * @param[in] jit_type The lowered libgccjit type.
 * @param[out] out_rval Pointer to receive the zero rvalue.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
gcc_gen_be_error_t gcc_gen_be_get_zero_rvalue(a_type_ptr tp, struct gcc_jit_type *jit_type, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
  if (!out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  *out_rval = NULL;

  gcc_jit_context *ctx = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
  if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

  if (!jit_type && tp) {
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(tp, &jit_type));
  }
  if (!jit_type) {
    jit_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
  }

  a_type_ptr stripped = tp;
  while (stripped && stripped->kind == tk_typeref) {
    stripped = stripped->variant.typeref.type;
  }
  if (stripped && stripped->kind == tk_nullptr) {
    gcc_jit_type *vp_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    *out_rval = gcc_jit_context_null(ctx, vp_type);
  } else if (stripped && stripped->kind == tk_pointer) {
    gcc_jit_type *ptr_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(stripped, &ptr_type));
    *out_rval = gcc_jit_context_null(ctx, ptr_type);
  } else if (stripped && stripped->kind == tk_ptr_to_member) {
    gcc_jit_type *pm_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(stripped, &pm_type));
    a_type_ptr mtp = stripped->variant.ptr_to_member.type;
    while (mtp && mtp->kind == tk_typeref) mtp = mtp->variant.typeref.type;
    if (mtp && mtp->kind == tk_routine) {
      gcc_jit_struct *pm_st = gcc_jit_type_is_struct(pm_type);
      gcc_jit_field *f_ptr = gcc_jit_struct_get_field(pm_st, 0);
      gcc_jit_field *f_adj = gcc_jit_struct_get_field(pm_st, 1);
      gcc_jit_field *flds[2] = { f_ptr, f_adj };
      gcc_jit_rvalue *vals[2] = {
        gcc_jit_context_null(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR)),
        gcc_jit_context_new_rvalue_from_long(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG), 0L)
      };
      *out_rval = gcc_jit_context_new_struct_constructor(ctx, NULL, pm_type, 2, flds, vals);
    } else {
      *out_rval = gcc_jit_context_new_rvalue_from_long(ctx, pm_type, -1L);
    }
  } else if (stripped && (stripped->kind == tk_class || stripped->kind == tk_struct)) {
    gcc_jit_type *struct_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(stripped, &struct_type));
    *out_rval = gcc_jit_context_new_struct_constructor(ctx, NULL, struct_type, 0, NULL, NULL);
  } else if (stripped && stripped->kind == tk_union) {
    gcc_jit_type *union_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(stripped, &union_type));
    *out_rval = gcc_jit_context_new_union_constructor(ctx, NULL, union_type, NULL, NULL);
  } else if (stripped && stripped->kind == tk_array) {
    gcc_jit_type *arr_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(stripped, &arr_type));
    *out_rval = gcc_jit_context_new_array_constructor(ctx, NULL, arr_type, 0, NULL);
#if GNU_VECTOR_TYPES_ALLOWED
  } else if (stripped && stripped->kind == tk_vector) {
    gcc_jit_type *vec_t = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(stripped, &vec_t));
    a_type_ptr etp = stripped->variant.vector.element_type;
    size_t n_elems = (etp && etp->size > 0 && stripped->size >= etp->size)
        ? (size_t)(stripped->size / etp->size)
        : 1U;
    gcc_jit_type *lane_t = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(etp, &lane_t));
    if (lane_t == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BOOL)) {
      lane_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT8_T);
    }
    gcc_jit_rvalue *lz = gcc_jit_context_zero(ctx, lane_t);
    if (p_gcc_jit_context_new_rvalue_from_vector && n_elems <= 64U) {
      gcc_jit_rvalue *lanes[64];
      for (size_t li = 0; li < n_elems; ++li) lanes[li] = lz;
      *out_rval = p_gcc_jit_context_new_rvalue_from_vector(ctx, NULL, vec_t, n_elems, lanes);
    }
#endif
  } else if ((stripped && stripped->kind == tk_void) ||
             jit_type == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
    *out_rval = NULL;
  } else if (gcc_jit_type_is_pointer(jit_type) != NULL) {
    *out_rval = gcc_jit_context_null(ctx, jit_type);
  } else if (gcc_jit_type_is_struct(jit_type) != NULL) {
    *out_rval = gcc_jit_context_new_struct_constructor(ctx, NULL, jit_type, 0, NULL, NULL);
  } else {
    void *is_union_jt = NULL;
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_TYPE, jit_type, &is_union_jt));
    *out_rval = is_union_jt
        ? gcc_jit_context_new_union_constructor(ctx, NULL, jit_type, NULL, NULL)
        : gcc_jit_context_zero(ctx, jit_type);
  }

  return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_decimal_float_type(
    int byte_width,
    struct gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
    if (!out_type) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_type = NULL;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    if (byte_width == 4) {
        *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT32_T);
        return GCC_GEN_BE_SUCCESS;
    }
    if (byte_width == 8) {
        *out_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT64_T);
        return GCC_GEN_BE_SUCCESS;
    }
    if (byte_width == 16) {
        *out_type = p_gcc_jit_context_get_int_type
            ? p_gcc_jit_context_get_int_type(ctx, 16, 0)
            : gcc_jit_context_get_int_type(ctx, 16, 0);
        return GCC_GEN_BE_SUCCESS;
    }

    return GCC_GEN_BE_ERROR_DECIMAL_FLOAT_UNSUPPORTED;
}

/**
 * @brief Selects the IEEE 754 decimal floating-point runtime symbol prefix (`__bid_` or `__dpd_`).
 * @details Uses `__dpd_` (Densely Packed Decimal) on PowerPC64 (`GCC_GEN_BE_TARGET_ABI_PPC64_ELFV2`) and
 * IBM z/Architecture (`GCC_GEN_BE_TARGET_ABI_S390X`), and `__bid_` (Binary Integer Decimal) on all other targets.
 * @param[out] out_prefix Pointer to receive the non-null `"__bid_"` or `"__dpd_"` prefix string.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if `out_prefix` is NULL.
 */
static gcc_gen_be_error_t get_decimal_float_abi_prefix(const char **out_prefix) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    int abi_ov = gcc_gen_be_singleton_state.active_target_abi_override;
    if (abi_ov == ((int)GCC_GEN_BE_TARGET_ABI_PPC64_ELFV2 + 1) ||
        abi_ov == ((int)GCC_GEN_BE_TARGET_ABI_S390X + 1)) {
        *out_prefix = "__dpd_";
    } else {
        *out_prefix = "__bid_";
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_decimal_float_binary_op(
    int byte_width,
    char op_char,
    struct gcc_jit_rvalue *lhs,
    struct gcc_jit_rvalue *rhs,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!lhs || !rhs || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;

    gcc_jit_type *df_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_type(byte_width, &df_type));

    const char *mode_suf = (byte_width == 4) ? "sd3" : ((byte_width == 8) ? "dd3" : "td3");
    const char *op_name = "add";
    if (op_char == '-') op_name = "sub";
    else if (op_char == '*') op_name = "mul";
    else if (op_char == '/') op_name = "div";
    else if (op_char != '+') return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    const char *df_prefix = "__bid_";
    GCC_GEN_BE_CHECK(get_decimal_float_abi_prefix(&df_prefix));
    char fn_name[64];
    snprintf(fn_name, sizeof(fn_name), "%s%s%s", df_prefix, op_name, mode_suf);

    gcc_jit_param *params[2];
    params[0] = gcc_jit_context_new_param(ctx, NULL, df_type, "a");
    params[1] = gcc_jit_context_new_param(ctx, NULL, df_type, "b");
    gcc_jit_function *fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, df_type, fn_name, 2, params, 0);
    gcc_jit_rvalue *call_args[2] = { lhs, rhs };
    *out_rval = gcc_jit_context_new_call(ctx, NULL, fn, 2, call_args);
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_decimal_float_comparison(
    int byte_width,
    const char *cmp_code,
    struct gcc_jit_rvalue *lhs,
    struct gcc_jit_rvalue *rhs,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!cmp_code || !lhs || !rhs || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;

    gcc_jit_type *df_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_type(byte_width, &df_type));

    const char *mode_suf = (byte_width == 4) ? "sd2" : ((byte_width == 8) ? "dd2" : "td2");
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);

    char fn_name[64];
    snprintf(fn_name, sizeof(fn_name), "__bid_%s%s", cmp_code, mode_suf);
    gcc_jit_param *params[2];
    params[0] = gcc_jit_context_new_param(ctx, NULL, df_type, "a");
    params[1] = gcc_jit_context_new_param(ctx, NULL, df_type, "b");
    gcc_jit_function *fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, int_type, fn_name, 2, params, 0);
    gcc_jit_rvalue *call_args[2] = { lhs, rhs };
    gcc_jit_rvalue *cmp_res = gcc_jit_context_new_call(ctx, NULL, fn, 2, call_args);
    gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctx, int_type, 0);
    enum gcc_jit_comparison jit_cmp = GCC_JIT_COMPARISON_EQ;
    if (strcmp(cmp_code, "ne") == 0) jit_cmp = GCC_JIT_COMPARISON_NE;
    else if (strcmp(cmp_code, "lt") == 0) jit_cmp = GCC_JIT_COMPARISON_LT;
    else if (strcmp(cmp_code, "le") == 0) jit_cmp = GCC_JIT_COMPARISON_LE;
    else if (strcmp(cmp_code, "gt") == 0) jit_cmp = GCC_JIT_COMPARISON_GT;
    else if (strcmp(cmp_code, "ge") == 0) jit_cmp = GCC_JIT_COMPARISON_GE;
    *out_rval = gcc_jit_context_new_comparison(ctx, NULL, jit_cmp, cmp_res, zero);
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_decimal_float_cast(
    int src_byte_width,
    int dst_byte_width,
    struct gcc_jit_rvalue *src_val,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!src_val || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;

    gcc_jit_type *src_type = NULL;
    gcc_jit_type *dst_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_type(src_byte_width, &src_type));
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_type(dst_byte_width, &dst_type));
    if (src_byte_width == dst_byte_width) {
        *out_rval = src_val;
        return GCC_GEN_BE_SUCCESS;
    }

    const char *src_m = (src_byte_width == 4) ? "sd" : ((src_byte_width == 8) ? "dd" : "td");
    const char *dst_m = (dst_byte_width == 4) ? "sd" : ((dst_byte_width == 8) ? "dd" : "td");
    const char *verb = (dst_byte_width > src_byte_width) ? "extend" : "trunc";
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));

    char fn_name[64];
    snprintf(fn_name, sizeof(fn_name), "__bid_%s%s%s2", verb, src_m, dst_m);
    gcc_jit_param *p0 = gcc_jit_context_new_param(ctx, NULL, src_type, "v");
    gcc_jit_function *fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, dst_type, fn_name, 1, &p0, 0);
    *out_rval = gcc_jit_context_new_call(ctx, NULL, fn, 1, &src_val);
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_decimal_to_binary_float(
    int dec_byte_width,
    int bin_byte_width,
    struct gcc_jit_rvalue *src_val,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!src_val || !out_rval || (bin_byte_width != 4 && bin_byte_width != 8)) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_rval = NULL;

    gcc_jit_type *src_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_type(dec_byte_width, &src_type));

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_type *dst_type = gcc_jit_context_get_type(
        ctx, (bin_byte_width == 4) ? GCC_JIT_TYPE_FLOAT : GCC_JIT_TYPE_DOUBLE);
    const char *src_m = (dec_byte_width == 4) ? "sd" : ((dec_byte_width == 8) ? "dd" : "td");
    const char *dst_m = (bin_byte_width == 4) ? "sf" : "df";
    const char *verb = (bin_byte_width > dec_byte_width) ? "extend" : "trunc";

    char fn_name[64];
    snprintf(fn_name, sizeof(fn_name), "__bid_%s%s%s", verb, src_m, dst_m);
    gcc_jit_param *p0 = gcc_jit_context_new_param(ctx, NULL, src_type, "v");
    gcc_jit_function *fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, dst_type, fn_name, 1, &p0, 0);
    *out_rval = gcc_jit_context_new_call(ctx, NULL, fn, 1, &src_val);
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_decimal_to_int_sized(
    int dec_byte_width,
    int int_byte_width,
    a_boolean is_unsigned,
    struct gcc_jit_rvalue *src_val,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!src_val || !out_rval || (int_byte_width != 4 && int_byte_width != 8)) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_rval = NULL;

    gcc_jit_type *src_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_type(dec_byte_width, &src_type));

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_type *dst_type = (int_byte_width == 8)
        ? gcc_jit_context_get_type(ctx, is_unsigned ? GCC_JIT_TYPE_UINT64_T : GCC_JIT_TYPE_INT64_T)
        : gcc_jit_context_get_type(ctx, is_unsigned ? GCC_JIT_TYPE_UNSIGNED_INT : GCC_JIT_TYPE_INT);
    const char *src_m = (dec_byte_width == 4) ? "sd" : ((dec_byte_width == 8) ? "dd" : "td");
    const char *int_m = (int_byte_width == 8) ? "di" : "si";
    const char *fix_op = is_unsigned ? "fixuns" : "fix";

    char fn_name[64];
    snprintf(fn_name, sizeof(fn_name), "__bid_%s%s%s", fix_op, src_m, int_m);
    gcc_jit_param *p0 = gcc_jit_context_new_param(ctx, NULL, src_type, "v");
    gcc_jit_function *fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, dst_type, fn_name, 1, &p0, 0);
    *out_rval = gcc_jit_context_new_call(ctx, NULL, fn, 1, &src_val);
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_decimal_to_int(
    int dec_byte_width,
    a_boolean is_unsigned,
    struct gcc_jit_rvalue *src_val,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    return gcc_gen_be_lower_decimal_to_int_sized(dec_byte_width, 4, is_unsigned, src_val, out_rval);
}

gcc_gen_be_error_t gcc_gen_be_lower_int_to_decimal(
    int int_byte_width,
    a_boolean is_unsigned,
    int dec_byte_width,
    struct gcc_jit_rvalue *src_val,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!src_val || !out_rval || (int_byte_width != 4 && int_byte_width != 8)) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_rval = NULL;

    gcc_jit_type *dst_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_type(dec_byte_width, &dst_type));

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_type *src_type = gcc_jit_rvalue_get_type(src_val);
    const char *int_m = (int_byte_width == 4) ? "si" : "di";
    const char *dec_m = (dec_byte_width == 4) ? "sd" : ((dec_byte_width == 8) ? "dd" : "td");
    const char *flt_op = is_unsigned ? "floatuns" : "float";

    char fn_name[64];
    snprintf(fn_name, sizeof(fn_name), "__bid_%s%s%s", flt_op, int_m, dec_m);
    gcc_jit_param *p0 = gcc_jit_context_new_param(ctx, NULL, src_type, "v");
    gcc_jit_function *fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, dst_type, fn_name, 1, &p0, 0);
    *out_rval = gcc_jit_context_new_call(ctx, NULL, fn, 1, &src_val);
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_binary_float_to_decimal(
    int bin_byte_width,
    int dec_byte_width,
    struct gcc_jit_rvalue *src_val,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!src_val || !out_rval || (bin_byte_width != 4 && bin_byte_width != 8)) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_rval = NULL;

    gcc_jit_type *dst_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_type(dec_byte_width, &dst_type));

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_type *src_type = gcc_jit_rvalue_get_type(src_val);
    const char *src_m = (bin_byte_width == 4) ? "sf" : "df";
    const char *dst_m = (dec_byte_width == 4) ? "sd" : ((dec_byte_width == 8) ? "dd" : "td");
    const char *verb = (dec_byte_width >= bin_byte_width) ? "extend" : "trunc";

    char fn_name[64];
    snprintf(fn_name, sizeof(fn_name), "__bid_%s%s%s", verb, src_m, dst_m);
    gcc_jit_param *p0 = gcc_jit_context_new_param(ctx, NULL, src_type, "v");
    gcc_jit_function *fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, dst_type, fn_name, 1, &p0, 0);
    *out_rval = gcc_jit_context_new_call(ctx, NULL, fn, 1, &src_val);
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_decimal_float_unary_op(
    int byte_width,
    char op_char,
    struct gcc_jit_rvalue *src_val,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!src_val || !out_rval || (op_char != '-' && op_char != 'a')) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_rval = NULL;

    gcc_jit_type *df_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_type(byte_width, &df_type));

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    int sign_shift = (byte_width == 4) ? 31 : ((byte_width == 8) ? 63 : 127);
    gcc_jit_rvalue *one_rv = gcc_jit_context_new_rvalue_from_int(ctx, df_type, 1);
    gcc_jit_rvalue *sh_rv = gcc_jit_context_new_rvalue_from_int(ctx, df_type, sign_shift);
    gcc_jit_rvalue *sign_bit = gcc_jit_context_new_binary_op(
        ctx, NULL, GCC_JIT_BINARY_OP_LSHIFT, df_type, one_rv, sh_rv);

    if (op_char == '-') {
        *out_rval = gcc_jit_context_new_binary_op(
            ctx, NULL, GCC_JIT_BINARY_OP_BITWISE_XOR, df_type, src_val, sign_bit);
    } else {
        gcc_jit_rvalue *mask = gcc_jit_context_new_unary_op(
            ctx, NULL, GCC_JIT_UNARY_OP_BITWISE_NEGATE, df_type, sign_bit);
        *out_rval = gcc_jit_context_new_binary_op(
            ctx, NULL, GCC_JIT_BINARY_OP_BITWISE_AND, df_type, src_val, mask);
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_lower_fixed_point_binary_op(
    int byte_width,
    int frac_bits,
    a_boolean is_unsigned,
    a_boolean is_saturating,
    char op_char,
    struct gcc_jit_rvalue *lhs,
    struct gcc_jit_rvalue *rhs,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!lhs || !rhs || !out_rval ||
        (byte_width != 1 && byte_width != 2 && byte_width != 4 && byte_width != 8)) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_rval = NULL;
    if (frac_bits < 0 || frac_bits >= (byte_width * 8)) {
        return GCC_GEN_BE_ERROR_FIXED_POINT_OVERFLOW;
    }

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;
    gcc_jit_type *res_type = gcc_jit_rvalue_get_type(lhs);
    gcc_jit_type *wide_type = NULL;
    if (byte_width == 8) {
        wide_type = p_gcc_jit_context_get_int_type
            ? p_gcc_jit_context_get_int_type(ctx, 16, is_unsigned ? 0 : 1)
            : gcc_jit_context_get_int_type(ctx, 16, is_unsigned ? 0 : 1);
    } else {
        wide_type = gcc_jit_context_get_type(
            ctx, is_unsigned ? GCC_JIT_TYPE_UINT64_T : GCC_JIT_TYPE_INT64_T);
    }

    gcc_jit_rvalue *w_lhs = gcc_jit_context_new_cast(ctx, NULL, lhs, wide_type);
    gcc_jit_rvalue *w_rhs = gcc_jit_context_new_cast(ctx, NULL, rhs, wide_type);
    gcc_jit_rvalue *w_res = NULL;

    if (op_char == '+') {
        w_res = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_PLUS, wide_type, w_lhs, w_rhs);
    } else if (op_char == '-') {
        w_res = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MINUS, wide_type, w_lhs, w_rhs);
    } else if (op_char == '*') {
        gcc_jit_rvalue *prod = gcc_jit_context_new_binary_op(
            ctx, NULL, GCC_JIT_BINARY_OP_MULT, wide_type, w_lhs, w_rhs);
        gcc_jit_rvalue *sh = gcc_jit_context_new_rvalue_from_int(ctx, wide_type, frac_bits);
        w_res = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_RSHIFT, wide_type, prod, sh);
    } else if (op_char == '/') {
        gcc_jit_rvalue *sh = gcc_jit_context_new_rvalue_from_int(ctx, wide_type, frac_bits);
        gcc_jit_rvalue *scaled = gcc_jit_context_new_binary_op(
            ctx, NULL, GCC_JIT_BINARY_OP_LSHIFT, wide_type, w_lhs, sh);
        w_res = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_DIVIDE, wide_type, scaled, w_rhs);
    } else {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }

    if (is_saturating) {
        gcc_jit_rvalue *max_rv = NULL;
        gcc_jit_rvalue *min_rv = NULL;
        if (byte_width == 8) {
            gcc_jit_type *u64_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT64_T);
            gcc_jit_type *i64_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT64_T);
            if (is_unsigned) {
                gcc_jit_rvalue *u64_max = gcc_jit_context_new_unary_op(
                    ctx, NULL, GCC_JIT_UNARY_OP_BITWISE_NEGATE, u64_t,
                    gcc_jit_context_new_rvalue_from_int(ctx, u64_t, 0));
                max_rv = gcc_jit_context_new_cast(ctx, NULL, u64_max, wide_type);
                min_rv = gcc_jit_context_new_rvalue_from_int(ctx, wide_type, 0);
            } else {
                gcc_jit_rvalue *u64_smax = gcc_jit_context_new_rvalue_from_long(
                    ctx, i64_t, (long)0x7FFFFFFFFFFFFFFFL);
                gcc_jit_rvalue *u64_smin = gcc_jit_context_new_binary_op(
                    ctx, NULL, GCC_JIT_BINARY_OP_MINUS, i64_t,
                    gcc_jit_context_new_unary_op(ctx, NULL, GCC_JIT_UNARY_OP_MINUS, i64_t, u64_smax),
                    gcc_jit_context_new_rvalue_from_int(ctx, i64_t, 1));
                max_rv = gcc_jit_context_new_cast(ctx, NULL, u64_smax, wide_type);
                min_rv = gcc_jit_context_new_cast(ctx, NULL, u64_smin, wide_type);
            }
        } else {
            long max_val = is_unsigned
                ? (long)((1ULL << (byte_width * 8)) - 1ULL)
                : (long)((1ULL << (byte_width * 8 - 1)) - 1ULL);
            long min_val = is_unsigned ? 0L : (-max_val - 1L);
            max_rv = gcc_jit_context_new_rvalue_from_long(ctx, wide_type, max_val);
            min_rv = gcc_jit_context_new_rvalue_from_long(ctx, wide_type, min_val);
        }
        gcc_jit_rvalue *gt_max = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_GT, w_res, max_rv);
        gcc_jit_rvalue *lt_min = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_LT, w_res, min_rv);
        gcc_jit_rvalue *gt_mask = gcc_jit_context_new_cast(ctx, NULL, gt_max, wide_type);
        gcc_jit_rvalue *lt_mask = gcc_jit_context_new_cast(ctx, NULL, lt_min, wide_type);
        gcc_jit_rvalue *one_rv = gcc_jit_context_new_rvalue_from_int(ctx, wide_type, 1);
        gcc_jit_rvalue *keep_hi = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MINUS, wide_type, one_rv, gt_mask);
        w_res = gcc_jit_context_new_binary_op(
            ctx, NULL, GCC_JIT_BINARY_OP_PLUS, wide_type,
            gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MULT, wide_type, w_res, keep_hi),
            gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MULT, wide_type, max_rv, gt_mask));
        gcc_jit_rvalue *keep_lo = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MINUS, wide_type, one_rv, lt_mask);
        w_res = gcc_jit_context_new_binary_op(
            ctx, NULL, GCC_JIT_BINARY_OP_PLUS, wide_type,
            gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MULT, wide_type, w_res, keep_lo),
            gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_MULT, wide_type, min_rv, lt_mask));
    }

    *out_rval = gcc_jit_context_new_cast(ctx, NULL, w_res, res_type);
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_normalize_bitint_rvalue(
    a_type_ptr tp,
    struct gcc_jit_rvalue *src_val,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!src_val || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = src_val;
    while (tp && tp->kind == tk_typeref) tp = tp->variant.typeref.type;
    if (!tp || tp->kind != tk_integer) return GCC_GEN_BE_SUCCESS;
    if (tp->variant.integer.int_kind != ik_bit_precise &&
        tp->variant.integer.int_kind != ik_unsigned_bit_precise) {
        return GCC_GEN_BE_SUCCESS;
    }
    unsigned long bits = (tp->variant.integer.extra_info && tp->variant.integer.extra_info->bit_width > 0)
        ? (unsigned long)tp->variant.integer.extra_info->bit_width
        : ((unsigned long)tp->size * 8UL);
    if (bits == 0UL || bits > 128UL || bits == 8UL || bits == 16UL ||
        bits == 32UL || bits == 64UL || bits == 128UL) {
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_type *val_type = gcc_jit_rvalue_get_type(src_val);
    unsigned long cbits = 8UL;
    if (bits > 64UL) cbits = 128UL;
    else if (bits > 32UL) cbits = 64UL;
    else if (bits > 16UL) cbits = 32UL;
    else if (bits > 8UL) cbits = 16UL;

    int shift_amt = (int)(cbits - bits);
    if (tp->variant.integer.int_kind == ik_bit_precise) {
        gcc_jit_rvalue *sh = gcc_jit_context_new_rvalue_from_int(ctx, val_type, shift_amt);
        gcc_jit_rvalue *shl = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_LSHIFT, val_type, src_val, sh);
        *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_RSHIFT, val_type, shl, sh);
    } else {
        gcc_jit_rvalue *one = gcc_jit_context_new_rvalue_from_int(ctx, val_type, 1);
        gcc_jit_rvalue *b_rv = gcc_jit_context_new_rvalue_from_int(ctx, val_type, (int)bits);
        gcc_jit_rvalue *mask = gcc_jit_context_new_binary_op(
            ctx, NULL, GCC_JIT_BINARY_OP_MINUS, val_type,
            gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_LSHIFT, val_type, one, b_rv),
            one);
        *out_rval = gcc_jit_context_new_binary_op(ctx, NULL, GCC_JIT_BINARY_OP_BITWISE_AND, val_type, src_val, mask);
    }
    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */