/**
 * @file gcc_gen_be_type.c
 * @brief Implementation of type lowering for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_type.h"
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
 * @brief Lowers an EDG frontend type into a libgccjit type.
 *
 * @param tp The frontend type to lower.
 * @param out_type A pointer to a gcc_jit_type pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_type` populated.
 */
gcc_gen_be_error_t gcc_gen_be_lower_type(a_type_ptr tp, gcc_jit_type **out_type) GCC_GEN_BE_NOEXCEPT {
  if (!out_type) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  *out_type = NULL;
  if (!tp) return GCC_GEN_BE_SUCCESS;
  
  gcc_jit_context *ctx = NULL;
  GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
  if (!ctx) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

  void *cached = NULL;
  gcc_gen_be_error_t err = cache_lookup(GCC_GEN_BE_CACHE_TYPE, tp, &cached);
  if (err != GCC_GEN_BE_SUCCESS) return err;
  if (cached) {
      *out_type = (gcc_jit_type *)cached;
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
    
    case tk_integer:
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
#if LONG_LONG_ALLOWED
        case ik_long_long: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_LONG); break;
        case ik_unsigned_long_long: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG_LONG); break;
#endif
#if INT128_EXTENSIONS_ALLOWED
        case ik_int128:
        case ik_unsigned_int128:
            {
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
            }
            break;
#endif
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
#if defined(GCC_JIT_TYPE_BFLOAT16)
        case fk_std_bfloat16: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_BFLOAT16); break;
#else
        case fk_std_bfloat16: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT); break;
#endif
#if defined(GCC_JIT_TYPE_FLOAT16)
        case fk_float16:
        case fk_fp16:
        case fk_std_float16: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT16); break;
#else
        case fk_float16:
        case fk_fp16:
        case fk_std_float16: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT); break;
#endif
#if defined(GCC_JIT_TYPE_FLOAT32)
        case fk_float32x:
        case fk_std_float32: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT32); break;
#else
        case fk_float32x:
        case fk_std_float32: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT); break;
#endif
#if defined(GCC_JIT_TYPE_FLOAT64)
        case fk_float64x:
        case fk_std_float64: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT64); break;
#else
        case fk_float64x:
        case fk_std_float64: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE); break;
#endif
#if defined(GCC_JIT_TYPE_FLOAT80)
        case fk_float80: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT80); break;
#elif defined(GCC_JIT_TYPE_FLOAT128)
        case fk_float80: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE); break;
#else
        case fk_float80: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE); break;
#endif
#if defined(GCC_JIT_TYPE_FLOAT128)
        case fk_float128:
        case fk_std_float128: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT128); break;
#else
        case fk_float128:
        case fk_std_float128: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE); break;
#endif
        default: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE); break;
      }
      break;

#if C99_IL_EXTENSIONS_SUPPORTED
    case tk_complex:
    case tk_imaginary:
      {
          gcc_jit_type *elem = NULL;
          switch (tp->variant.float_kind) {
              case fk_float: elem = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_FLOAT); break;
              case fk_double: elem = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE); break;
              case fk_long_double: elem = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE); break;
              default: elem = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_DOUBLE); break;
          }
          if (tp->kind == tk_imaginary) {
              res = elem;
          } else {
#if defined(GCC_JIT_TYPE_COMPLEX_FLOAT)
              switch (tp->variant.float_kind) {
                  case fk_float: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_COMPLEX_FLOAT); break;
                  case fk_double: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_COMPLEX_DOUBLE); break;
                  case fk_long_double: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_COMPLEX_LONG_DOUBLE); break;
                  default: res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_COMPLEX_DOUBLE); break;
              }
#else
              gcc_jit_field *real_f = gcc_jit_context_new_field(ctx, NULL, elem, "real");
              gcc_jit_field *imag_f = gcc_jit_context_new_field(ctx, NULL, elem, "imag");
              gcc_jit_field *fields[] = {real_f, imag_f};
              gcc_jit_struct *s = gcc_jit_context_new_struct_type(ctx, NULL, "complex", 2, fields);
              res = gcc_jit_struct_as_type(s);
#endif
          }
      }
      break;
#endif

    case tk_pointer:
      {
          a_type_ptr pts_to = tp->variant.pointer.type;
          while (pts_to && pts_to->kind == tk_typeref) pts_to = pts_to->variant.typeref.type;
          if (!pts_to || pts_to->kind == tk_void) {
              res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
          } else {
              gcc_jit_type *elem = NULL;
              GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(tp->variant.pointer.type, &elem));
              if (pts_to->kind == tk_routine) {
                  res = elem;
              } else {
                  res = gcc_jit_type_get_pointer(elem);
              }
          }
      }
      break;
      
    case tk_array:
      if (!tp->variant.array.is_variable_size_array && !tp->variant.array.is_template_dependent_size_array && tp->variant.array.variant.number_of_elements != 0) {
         int size = (int)tp->variant.array.variant.number_of_elements;
         gcc_jit_type *elem = NULL;
         GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(tp->variant.array.element_type, &elem));
         res = gcc_jit_context_new_array_type(ctx, NULL, elem, size);
      } else {
         gcc_jit_type *elem = NULL;
         GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(tp->variant.array.element_type, &elem));
         if (tp->variant.array.is_variable_size_array) {
             res = gcc_jit_type_get_pointer(elem);
         } else if (tp->size == 0 && !tp->variant.array.is_template_dependent_size_array) {
             /* C99 flexible array member (e.g., int data[]) with 0 elements */
             res = gcc_jit_context_new_array_type(ctx, NULL, elem, 0);
         } else {
             res = gcc_jit_context_new_array_type(ctx, NULL, elem, 64);
         }
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
         GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_debug_info(tp, res));
         
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
         if (num_entries > 0) {
             entries = (layout_entry_t *)malloc((size_t)num_entries * sizeof(layout_entry_t));
             if (!entries) return GCC_GEN_BE_ERROR_OOM;
             
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
             
             /* Sort entries by offset and bit_rem using bubble sort for simplicity */
             for (int pass = 0; pass < num_entries - 1; pass++) {
                 for (int j = 0; j < num_entries - pass - 1; j++) {
                     if (entries[j].offset > entries[j+1].offset ||
                         (entries[j].offset == entries[j+1].offset && entries[j].bit_rem > entries[j+1].bit_rem)) {
                         layout_entry_t tmp = entries[j];
                         entries[j] = entries[j+1];
                         entries[j+1] = tmp;
                     }
                 }
             }
         }
         
         /* We will build the fields array including padding */
         int max_fields = num_entries * 2 + 1; /* worst case: pad before every field + tail pad */
         gcc_jit_field **fields = NULL;
         if (max_fields > 0) {
             fields = (gcc_jit_field **)malloc((size_t)max_fields * sizeof(gcc_jit_field*));
             if (!fields) {
                 if (entries) free(entries);
                 return GCC_GEN_BE_ERROR_OOM;
             }
             
             int num_jit_fields = 0;
             unsigned long current_offset = 0;
             
             for (int k = 0; k < num_entries; k++) {
                 layout_entry_t *entry = &entries[k];
                 
                 /* Check for padding */
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
                     err = gcc_gen_be_lower_type(b->type, &btype);
                     if (err != GCC_GEN_BE_SUCCESS) { free(fields); free(entries); return err; }
                     
                     if (b->is_optimized_empty_base) {
                         /* Skip zero-size optimized empty base; tail padding or subsequent fields handle offset */
                     } else {
                         char bname[64];
                         a_const_char *tname = b->type->source_corresp.name;
                         snprintf(bname, sizeof(bname), "__base_%s", tname ? tname : "unnamed");

                         /* Determine effective base subobject size (without virtual bases and accounting for tail-padding packing) */
                         unsigned long bsize = (unsigned long)b->type->size;
                         if (b->type->kind == tk_class || b->type->kind == tk_struct) {
                             a_class_type_supplement_ptr b_extra = b->type->variant.class_struct_union.extra_info;
                             if (b_extra && b_extra->size_without_virtual_base_classes > 0 &&
                                 b_extra->size_without_virtual_base_classes < bsize) {
                                 bsize = (unsigned long)b_extra->size_without_virtual_base_classes;
                             }
                         }
                         /* Check if subsequent derived fields or bases are packed into the tail padding of this base subobject */
                         if (k + 1 < num_entries && entries[k + 1].offset >= entry->offset &&
                             entries[k + 1].offset < entry->offset + bsize) {
                             bsize = entries[k + 1].offset - entry->offset;
                         }

                         if (bsize == 0) {
                             /* Zero-size packed base subobject: skip emitting a 0-length array field */
                         } else if (bsize < b->type->size) {
                             gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
                             gcc_jit_type *subobj_type = gcc_jit_context_new_array_type(ctx, NULL, char_type, (int)bsize);
                             fields[num_jit_fields] = gcc_jit_context_new_field(ctx, NULL, subobj_type, bname);
                             GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, b, fields[num_jit_fields]));
                             num_jit_fields++;
                             current_offset = entry->offset + bsize;
                         } else {
                             fields[num_jit_fields] = gcc_jit_context_new_field(ctx, NULL, btype, bname);
                             GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, b, fields[num_jit_fields]));
                             num_jit_fields++;
                             current_offset = entry->offset + bsize;
                         }
                     }
                 } else {
                     a_field_ptr f = (a_field_ptr)entry->ptr;
                     a_const_char *fname = f->source_corresp.name;
                     if (!fname) fname = "unnamed_field";
                     
                     gcc_jit_type *ftype = NULL;
                     err = gcc_gen_be_lower_type(f->type, &ftype);
                     if (err != GCC_GEN_BE_SUCCESS) { free(fields); free(entries); return err; }
                     
                     if (f->is_optimized_empty_class) {
                         /* Optimized empty class field (`[[no_unique_address]]`): skip 0-size field */
                     } else if (f->is_bit_field || f->bit_size > 0) {
                         /* Phase 6: Bitfield Allocation Engine. 
                            Synthesize an explicit container field matching EDG's allocation unit 
                            instead of using gcc_jit_context_new_bitfield. */
                         a_boolean is_zero_width = (f->bit_size == 0) ? TRUE : FALSE;
                         a_boolean container_already_emitted = FALSE;
                         /* If current_offset is ahead of f->offset (in a struct/class), we already emitted a container 
                            that covers this bitfield. In a union, each overlapping bitfield at offset 0 is its own union member. */
                         if (tp->kind != tk_union && current_offset > f->offset) {
                             container_already_emitted = TRUE;
                         }
                         
                         if (!container_already_emitted && !is_zero_width) {
                             char cname[64];
                             snprintf(cname, sizeof(cname), "__bf_container_%lu_%d", current_offset, num_jit_fields);
                             unsigned long unit_sz = (unsigned long)gcc_jit_type_get_size(ftype);
                             if (unit_sz > 0 && unit_sz < 8 &&
                                 ((unsigned long)f->offset_bit_remainder + (unsigned long)f->bit_size) > (unit_sz * 8UL) &&
                                 (f->offset + 8UL) <= tp->size) {
                                 ftype = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_LONG_LONG);
                                 unit_sz = 8UL;
                             }
                             unsigned long aligned_off = (unit_sz > 0) ? (((unsigned long)f->offset / unit_sz) * unit_sz) : (unsigned long)f->offset;
                             fields[num_jit_fields] = gcc_jit_context_new_field(ctx, NULL, ftype, cname);
                             GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, f, fields[num_jit_fields]));
                             num_jit_fields++;
                             if (tp->kind != tk_union) {
                                 current_offset = aligned_off + unit_sz;
                             }
                         } else {
                             if (num_jit_fields > 0) {
                                 GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, f, fields[num_jit_fields - 1]));
                             }
                             if (is_zero_width) {
                                 /* Zero-width bitfields force alignment to next boundary.
                                    EDG handles offset calculation, so we just let next fields pad. */
                             }
                         }
                     } else {
                         unsigned long fsize = (unsigned long)f->type->size;
                         fields[num_jit_fields] = gcc_jit_context_new_field(ctx, NULL, ftype, fname);
                         GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, f, fields[num_jit_fields]));
                         if (has_lowered_base_fields && (tp->kind == tk_class || tp->kind == tk_struct) &&
                             tp->variant.class_struct_union.extra_info) {
                             a_class_type_supplement_ptr extra = tp->variant.class_struct_union.extra_info;
                             for (a_base_class_ptr b = extra->base_classes ? extra->base_classes : extra->direct_base_classes;
                                  b != NULL;
                                  b = extra->base_classes ? b->next : b->next_direct) {
                                 if (b->offset == f->offset) {
                                     GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, b, fields[num_jit_fields]));
                                 }
                             }
                         }
                         /* If this field is an anonymous union or anonymous struct parent object, ensure its inner fields are lowered and cached */
                         if (f->is_anonymous_parent_object && f->type &&
                             (f->type->kind == tk_union || f->type->kind == tk_struct || f->type->kind == tk_class)) {
                             for (a_field_ptr subf = f->type->variant.class_struct_union.field_list; subf != NULL; subf = subf->next) {
                                 void *sub_cached = NULL;
                                 err = cache_lookup(GCC_GEN_BE_CACHE_FIELD, subf, &sub_cached);
                                 if (err != GCC_GEN_BE_SUCCESS || !sub_cached) {
                                     GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FIELD, subf, fields[num_jit_fields]));
                                 }
                             }
                         }
                         num_jit_fields++;
                         current_offset = entry->offset + fsize;
                     }
                 }
             }
             
             /* Tail padding */
             if (current_offset < tp->size && tp->kind != tk_union) {
                 unsigned long pad_size = (unsigned long)tp->size - current_offset;
                 gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
                 gcc_jit_type *pad_type = gcc_jit_context_new_array_type(ctx, NULL, char_type, (int)pad_size);
                 fields[num_jit_fields++] = gcc_jit_context_new_field(ctx, NULL, pad_type, "__tail_pad");
             } else if (num_jit_fields == 0) {
                 gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
                 fields[num_jit_fields++] = gcc_jit_context_new_field(ctx, NULL, char_type, "__empty_pad");
             }
             
             if (entries) free(entries);
             
            if (tp->kind == tk_union) {
                res = gcc_jit_context_new_union_type(ctx, NULL, name, num_jit_fields, fields);
                /* Overwrite cached opaque struct with the actual union */
                err = cache_insert(GCC_GEN_BE_CACHE_TYPE, tp, res);
                if (err != GCC_GEN_BE_SUCCESS) { free(fields); return err; }
            } else {
                gcc_jit_struct_set_fields(s, NULL, num_jit_fields, fields);
            }
            free(fields);
         }
         *out_type = res;
         return GCC_GEN_BE_SUCCESS;
      }

    case tk_routine:
      {
         a_routine_type_supplement_ptr supp = tp->variant.routine.extra_info;
         gcc_jit_type *ret_type = NULL;
         a_type_ptr frontend_ret = tp->variant.routine.return_type;
         extern gcc_gen_be_error_t gcc_gen_be_is_returned_via_sret(a_type_ptr type, a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT;
         extern gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT;
         extern gcc_gen_be_error_t gcc_gen_be_get_coerced_abi_type(a_type_ptr type, struct gcc_jit_type **out_coerced_type, a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT;
         a_boolean uses_sret = FALSE;
         gcc_jit_type *sret_ptr_type = NULL;
         if (frontend_ret) {
             GCC_GEN_BE_CHECK(gcc_gen_be_is_returned_via_sret(frontend_ret, &uses_sret));
         }
         if (uses_sret) {
             ret_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
             gcc_jit_type *actual_ret = NULL;
             GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(frontend_ret, &actual_ret));
             sret_ptr_type = gcc_jit_type_get_pointer(actual_ret);
         } else {
             GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(frontend_ret, &ret_type));
             if (frontend_ret) {
                 gcc_jit_type *coerced_ret = NULL;
                 a_boolean ret_coerced = FALSE;
                 if (gcc_gen_be_get_coerced_abi_type(frontend_ret, &coerced_ret, &ret_coerced) == GCC_GEN_BE_SUCCESS &&
                     ret_coerced && coerced_ret) {
                     ret_type = coerced_ret;
                 }
             }
         }
         
         int num_params = uses_sret ? 1 : 0;
         a_param_type_ptr ptp;
         for (ptp = supp->param_type_list; ptp != NULL; ptp = ptp->next) {
             num_params++;
         }
         
         gcc_jit_type **param_types = NULL;
         if (num_params > 0) {
             param_types = (gcc_jit_type **)malloc((size_t)num_params * sizeof(gcc_jit_type*));
             if (!param_types) return GCC_GEN_BE_ERROR_OOM;
             
             int i = 0;
             if (uses_sret) {
                 param_types[i++] = sret_ptr_type;
             }
             for (ptp = supp->param_type_list; ptp != NULL; ptp = ptp->next) {
                 a_type_ptr ptype = ptp->type;
                 while (ptype && ptype->kind == tk_typeref) ptype = ptype->variant.typeref.type;
                 err = gcc_gen_be_lower_type(ptype, &param_types[i]);
                 if (err != GCC_GEN_BE_SUCCESS) { free(param_types); return err; }
                 if (ptype) {
                     a_boolean is_invis_ref = FALSE;
                     if (gcc_gen_be_is_passed_by_invisible_reference(ptype, &is_invis_ref) == GCC_GEN_BE_SUCCESS && is_invis_ref) {
                         param_types[i] = gcc_jit_type_get_pointer(gcc_jit_type_unqualified(param_types[i]));
                     } else {
                         gcc_jit_type *coerced_pt = NULL;
                         a_boolean needs_coercion = FALSE;
                         if (gcc_gen_be_get_coerced_abi_type(ptype, &coerced_pt, &needs_coercion) == GCC_GEN_BE_SUCCESS &&
                             needs_coercion && coerced_pt) {
                             param_types[i] = coerced_pt;
                         }
                     }
                 }
                 i++;
             }
         }
         
         int is_variadic = supp->has_ellipsis ? 1 : 0;
         res = gcc_jit_context_new_function_ptr_type(ctx, NULL, ret_type, num_params, param_types, is_variadic);
         if (param_types) free(param_types);
      }
      break;
      
    case tk_ptr_to_member:
      {
         a_type_ptr member_type = tp->variant.ptr_to_member.type;
         if (member_type->kind == tk_routine) {
             gcc_jit_field *f1 = gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR), "func_ptr");
             gcc_jit_field *f2 = gcc_jit_context_new_field(ctx, NULL, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG), "this_adj");
             gcc_jit_field *fields[] = {f1, f2};
             gcc_jit_struct *s = gcc_jit_context_new_struct_type(ctx, NULL, "ptr_to_member", 2, fields);
             res = gcc_jit_struct_as_type(s);
         } else {
             res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
         }
      }
      break;
      
#if GNU_VECTOR_TYPES_ALLOWED
    case tk_vector:
      {
         int num_elements = 1;
         a_type_ptr elem_tp = tp->variant.vector.element_type;
         if (elem_tp && elem_tp->size > 0 && tp->size >= elem_tp->size) {
            num_elements = (int)(tp->size / elem_tp->size);
         }
         gcc_jit_type *elem = NULL;
         GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(elem_tp, &elem));
#if defined(LIBGCCJIT_HAVE_gcc_jit_type_get_vector)
         res = gcc_jit_type_get_vector(elem, (size_t)num_elements);
#else
         /* Fallback for older libgccjit that does not support vector types */
         res = gcc_jit_context_new_array_type(ctx, NULL, elem, num_elements);
#endif
      }
      break;
#endif

    case tk_error:
      /* Ignore / Fallback to void */
      res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
      break;
      
    default:
      res = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
      break;
  }
  
  if (res) {
      GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_TYPE, tp, res));
  }
  
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
  if (stripped && (stripped->kind == tk_pointer || stripped->kind == tk_ptr_to_member)) {
    gcc_jit_type *ptr_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(stripped, &ptr_type));
    *out_rval = gcc_jit_context_null(ctx, ptr_type ? ptr_type : jit_type);
  } else if (stripped && (stripped->kind == tk_class || stripped->kind == tk_struct)) {
    gcc_jit_type *struct_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(stripped, &struct_type));
    *out_rval = gcc_jit_context_new_struct_constructor(ctx, NULL, struct_type, 0, NULL, NULL);
  } else if (stripped && stripped->kind == tk_array) {
    gcc_jit_type *arr_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(stripped, &arr_type));
    *out_rval = gcc_jit_context_new_array_constructor(ctx, NULL, arr_type, 0, NULL);
  } else if (stripped && stripped->kind == tk_void) {
    *out_rval = NULL;
  } else {
    *out_rval = gcc_jit_context_zero(ctx, jit_type);
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
        if (p_gcc_jit_context_get_int_type) {
            *out_type = p_gcc_jit_context_get_int_type(ctx, 16, 0);
        }
        if (!*out_type) {
            gcc_jit_type *u64 = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UINT64_T);
            *out_type = gcc_jit_context_new_array_type(ctx, NULL, u64, 2);
        }
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
    if (!out_prefix) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
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

gcc_gen_be_error_t gcc_gen_be_lower_decimal_to_int(
    int dec_byte_width,
    a_boolean is_unsigned,
    struct gcc_jit_rvalue *src_val,
    struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!src_val || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;

    gcc_jit_type *src_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_decimal_float_type(dec_byte_width, &src_type));

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    gcc_jit_type *dst_type = gcc_jit_context_get_type(
        ctx, is_unsigned ? GCC_JIT_TYPE_UNSIGNED_INT : GCC_JIT_TYPE_INT);
    const char *src_m = (dec_byte_width == 4) ? "sd" : ((dec_byte_width == 8) ? "dd" : "td");
    const char *fix_op = is_unsigned ? "fixuns" : "fix";

    char fn_name[64];
    snprintf(fn_name, sizeof(fn_name), "__bid_%s%ssi", fix_op, src_m);
    gcc_jit_param *p0 = gcc_jit_context_new_param(ctx, NULL, src_type, "v");
    gcc_jit_function *fn = gcc_jit_context_new_function(
        ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, dst_type, fn_name, 1, &p0, 0);
    *out_rval = gcc_jit_context_new_call(ctx, NULL, fn, 1, &src_val);
    return GCC_GEN_BE_SUCCESS;
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
    gcc_jit_rvalue *one_rv = gcc_jit_context_new_rvalue_from_int(ctx, df_type, 1);
    gcc_jit_rvalue *sh_rv = gcc_jit_context_new_rvalue_from_int(ctx, df_type, (byte_width == 4) ? 31 : 63);
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
    gcc_jit_type *res_type = gcc_jit_rvalue_get_type(lhs);
    gcc_jit_type *wide_type = gcc_jit_context_get_type(
        ctx, is_unsigned ? GCC_JIT_TYPE_UINT64_T : GCC_JIT_TYPE_INT64_T);

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

    if (is_saturating && byte_width < 8) {
        long max_val = is_unsigned
            ? (long)((1ULL << (byte_width * 8)) - 1ULL)
            : (long)((1ULL << (byte_width * 8 - 1)) - 1ULL);
        long min_val = is_unsigned ? 0L : (-max_val - 1L);
        gcc_jit_rvalue *max_rv = gcc_jit_context_new_rvalue_from_long(ctx, wide_type, max_val);
        gcc_jit_rvalue *min_rv = gcc_jit_context_new_rvalue_from_long(ctx, wide_type, min_val);
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

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */