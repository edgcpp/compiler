/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/**
 * @file gcc_be_eh_rt.c
 * @brief Production C++ ABI & SjLj exception handling runtime bridge for the GCC backend (`lib_src/`).
 * @details Provides `__gcc_be_eh_chain`, `__gcc_be_eh_register_frame`, `__gcc_be_eh_unregister_frame`,
 * `__gcc_be_eh_check_unwind_state`, `__cxa_type_match`, `__builtin_eh_pointer`, and `__builtin_eh_filter`
 * for standalone executables linked against the EDG runtime library when compiled with `cpfe --gcc-be`.
 */

#include "basics.h"
#include "runtime.h"
#include "../src/gcc_gen_be_error.h"
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Active SjLj frame chain head used by `src/gcc_gen_be_eh.c`. */
void *__gcc_be_eh_chain = NULL;

/**
 * @brief Mirrors the 4-field `__eh_frame` struct synthesized by `src/gcc_gen_be_eh.c`.
 * @details Links active `try` and RAII cleanup frames on the stack.
 */
struct gcc_be_sjlj_frame {
  /** @brief Pointer to the enclosing `gcc_be_sjlj_frame`. */
  struct gcc_be_sjlj_frame *next;
  /** @brief `setjmp` buffer storage (`void *[64]`). */
  void *jmpbuf[64];
  /** @brief Thrown exception object payload pointer. */
  void *exception_object;
  /** @brief Thrown `std::type_info` descriptor pointer. */
  void *type_info;
};

/**
 * @brief Mirrors the Itanium C++ ABI `_ZTI*` descriptor layout emitted by `src/gcc_gen_be_rtti.c`.
 * @details Used by `__cxa_type_match` for single, multiple, and virtual inheritance catch matching.
 */
struct gcc_be_abi_type_info {
  /** @brief Virtual table pointer (`_ZTVN10__cxxabiv1*type_infoE + 16`). */
  const void *vptr;
  /** @brief Mangled `_ZTS*` type name string. */
  const char *name;
  /** @brief Base type pointer (SI) or flags word (VMI / pointer). */
  const void *word2;
  /** @brief Pointee type (pointer) or base count (VMI). */
  const void *word3;
};

/**
 * @brief Internal helper to push a stack-allocated `gcc_be_sjlj_frame` onto `__gcc_be_eh_chain`.
 * @details Validates `frame_ptr` and links `frame->next = __gcc_be_eh_chain`.
 * @param[in,out] frame_ptr Pointer to the `gcc_be_sjlj_frame` to register.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if `frame_ptr` is NULL.
 */
static gcc_gen_be_error_t gcc_be_register_sjlj_frame_impl(void *frame_ptr) GCC_GEN_BE_NOEXCEPT {
  if (!frame_ptr) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  struct gcc_be_sjlj_frame *frame = (struct gcc_be_sjlj_frame *)frame_ptr;
  frame->next = (struct gcc_be_sjlj_frame *)__gcc_be_eh_chain;
  frame->exception_object = NULL;
  frame->type_info = NULL;
  __gcc_be_eh_chain = frame;
  return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Internal helper to pop a `gcc_be_sjlj_frame` from `__gcc_be_eh_chain`.
 * @details Validates `frame_ptr` and unlinks it when it is at the head of `__gcc_be_eh_chain`.
 * @param[in] frame_ptr Pointer to the `gcc_be_sjlj_frame` to unregister.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if `frame_ptr` is NULL.
 */
static gcc_gen_be_error_t gcc_be_unregister_sjlj_frame_impl(void *frame_ptr) GCC_GEN_BE_NOEXCEPT {
  if (!frame_ptr) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  struct gcc_be_sjlj_frame *frame = (struct gcc_be_sjlj_frame *)frame_ptr;
  if (__gcc_be_eh_chain == frame) {
    __gcc_be_eh_chain = frame->next;
  }
  return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Registers a stack-allocated `gcc_be_sjlj_frame` on `__gcc_be_eh_chain`.
 * @details Pushes `frame_ptr` to the head of `__gcc_be_eh_chain`.
 * @param[in,out] frame_ptr Pointer to the `gcc_be_sjlj_frame` to register.
 */
void __gcc_be_eh_register_frame(void *frame_ptr) {
  (void)gcc_be_register_sjlj_frame_impl(frame_ptr);
}

/**
 * @brief Unregisters a `gcc_be_sjlj_frame` from `__gcc_be_eh_chain`.
 * @details Pops `frame_ptr` when it matches `__gcc_be_eh_chain`.
 * @param[in] frame_ptr Pointer to the `gcc_be_sjlj_frame` to unregister.
 */
void __gcc_be_eh_unregister_frame(void *frame_ptr) {
  (void)gcc_be_unregister_sjlj_frame_impl(frame_ptr);
}

/**
 * @brief Queries whether an active unwinding exception is pending at a call site.
 * @details Returns 0 for normal execution flow.
 * @return 0 during normal execution.
 */
int __gcc_be_eh_check_unwind_state(void) {
  return 0;
}

/**
 * @brief Strips a leading `*` from an `_ZTS*` type name string if present.
 * @details Normalizes Darwin/Mach-O unique-name prefixes before string comparison.
 * @param[in] s Input `_ZTS` string.
 * @param[out] out_norm Pointer to receive the normalized string pointer.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if `out_norm` is NULL.
 */
static gcc_gen_be_error_t gcc_be_normalize_tinfo_name(const char *s, const char **out_norm) GCC_GEN_BE_NOEXCEPT {
  if (!out_norm) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  if (!s) {
    *out_norm = "";
    return GCC_GEN_BE_SUCCESS;
  }
  *out_norm = (*s == '*') ? (s + 1) : s;
  return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Recursively matches `throw_ti` against `catch_ti` and adjusts `*inout_ptr`.
 * @details Traverses SI (`__si_class_type_info`) and VMI (`__vmi_class_type_info`) base descriptors,
 * including both non-virtual byte offsets and virtual base vtable offsets.
 * @param[in] catch_ti Catch clause `_ZTI` descriptor.
 * @param[in] throw_ti Thrown object `_ZTI` descriptor.
 * @param[in,out] inout_ptr Pointer to the object pointer to adjust.
 * @param[in] depth Recursion depth guard.
 * @param[out] out_matched Pointer to receive 1 on match, 0 on mismatch.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if `out_matched` is NULL.
 */
static gcc_gen_be_error_t gcc_be_match_abi_type_info(
    const struct gcc_be_abi_type_info *catch_ti,
    const struct gcc_be_abi_type_info *throw_ti,
    void **inout_ptr,
    int depth,
    int *out_matched) GCC_GEN_BE_NOEXCEPT {
  if (!out_matched || !inout_ptr) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
  *out_matched = 0;
  if (!catch_ti || !throw_ti || depth > 16) return GCC_GEN_BE_SUCCESS;
  if (catch_ti == throw_ti) {
    *out_matched = 1;
    return GCC_GEN_BE_SUCCESS;
  }
  const char *cname = "";
  const char *tname = "";
  GCC_GEN_BE_CHECK(gcc_be_normalize_tinfo_name(catch_ti->name, &cname));
  GCC_GEN_BE_CHECK(gcc_be_normalize_tinfo_name(throw_ti->name, &tname));
  if (cname[0] != '\0' && strcmp(cname, tname) == 0) {
    *out_matched = 1;
    return GCC_GEN_BE_SUCCESS;
  }

  if (cname[0] == 'P' && tname[0] == 'P' && catch_ti->word3 && throw_ti->word3) {
    const struct gcc_be_abi_type_info *c_pt = (const struct gcc_be_abi_type_info *)catch_ti->word3;
    const struct gcc_be_abi_type_info *t_pt = (const struct gcc_be_abi_type_info *)throw_ti->word3;
    int pt_matched = 0;
    GCC_GEN_BE_CHECK(gcc_be_match_abi_type_info(c_pt, t_pt, inout_ptr, depth + 1, &pt_matched));
    if (pt_matched) {
      *out_matched = 1;
      return GCC_GEN_BE_SUCCESS;
    }
  }

  uintptr_t w2 = (uintptr_t)throw_ti->word2;
  if (w2 > 0x10000ULL) {
    const struct gcc_be_abi_type_info *base_ti = (const struct gcc_be_abi_type_info *)throw_ti->word2;
    return gcc_be_match_abi_type_info(catch_ti, base_ti, inout_ptr, depth + 1, out_matched);
  }

  uintptr_t base_count = (uintptr_t)throw_ti->word3;
  if ((w2 == 1ULL || w2 == 2ULL) && base_count > 0 && base_count <= 16) {
    const void *const *words = (const void *const *)throw_ti;
    for (uintptr_t i = 0; i < base_count; ++i) {
      const struct gcc_be_abi_type_info *b_ti =
          (const struct gcc_be_abi_type_info *)words[4 + i * 2];
      intptr_t offset_flags = (intptr_t)words[4 + i * 2 + 1];
      int is_public = (offset_flags & 0x2) != 0;
      int is_virtual = (offset_flags & 0x1) != 0;
      intptr_t byte_off = offset_flags >> 8;
      if (is_public && b_ti && *inout_ptr) {
        void *sub_ptr = NULL;
        if (!is_virtual) {
          sub_ptr = (char *)(*inout_ptr) + byte_off;
        } else if (byte_off < 0) {
          const char *vptr = *(const char *const *)(*inout_ptr);
          if (vptr) {
            intptr_t vbase_off = *(const intptr_t *)(vptr + byte_off);
            sub_ptr = (char *)(*inout_ptr) + vbase_off;
          }
        } else {
          sub_ptr = (char *)(*inout_ptr) + byte_off;
        }
        if (sub_ptr) {
          int sub_matched = 0;
          GCC_GEN_BE_CHECK(gcc_be_match_abi_type_info(catch_ti, b_ti, &sub_ptr, depth + 1, &sub_matched));
          if (sub_matched) {
            *inout_ptr = sub_ptr;
            *out_matched = 1;
            return GCC_GEN_BE_SUCCESS;
          }
        }
      }
    }
  }
  return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Matches a thrown exception object of type `throw_type` against `catch_type`.
 * @details Compares `_ZTI` descriptors and adjusts `exc_ptr` to the matching base subobject.
 * @param[in] catch_type Catch clause `_ZTI` descriptor pointer.
 * @param[in] throw_type Thrown object `_ZTI` descriptor pointer.
 * @param[in] exc_ptr Thrown object payload pointer.
 * @return Adjusted subobject pointer on match, or NULL on mismatch.
 */
void *__cxa_type_match(void *catch_type, void *throw_type, void *exc_ptr) {
  if (!catch_type || !exc_ptr) return NULL;
  if (!throw_type || catch_type == throw_type) return exc_ptr;
  void *adjusted = exc_ptr;
  const struct gcc_be_abi_type_info *ct = (const struct gcc_be_abi_type_info *)catch_type;
  const struct gcc_be_abi_type_info *tt = (const struct gcc_be_abi_type_info *)throw_type;
  int matched = 0;
  if (gcc_be_match_abi_type_info(ct, tt, &adjusted, 0, &matched) == GCC_GEN_BE_SUCCESS && matched) {
    return adjusted;
  }
  return NULL;
}

#ifdef __cplusplus
} /* extern "C" */
#endif
