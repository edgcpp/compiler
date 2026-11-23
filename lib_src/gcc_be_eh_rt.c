/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/**
 * @file gcc_be_eh_rt.c
 * @brief Thread-safe Itanium C++ ABI & DWARF exception runtime bridge for the GCC backend.
 * @details Compiled with `-fno-exceptions -fno-rtti` into `libedg_gcc_eh_rt.a`. Provides:
 * - `thread_local` per-thread EH state (eliminating multi-threaded data races),
 * - Native DWARF/Compact-Unwind assembly trampoline (`__edg_gcc_eh_trampoline_call`) backed by
 *   `__gcc_be_eh_personality_v0` so both `cpfe --backend=gcc` `throw` (`__cxa_throw`) and
 *   precompiled `libstdc++`/`libc++` exceptions unwind cleanly with full RAII cleanup execution,
 * - Exact `_ZTI` vtable-pointer inspection (`__class_type_info`, `__si_class_type_info`,
 *   `__vmi_class_type_info`, `__pointer_type_info`) supporting all `__vmi_class_type_info`
 *   flags (`0..3`), arbitrary `base_count`, and virtual diamond vtable-offset pointer adjustment.
 * @ingroup gcc_gen_be
 */

#include "gcc_be_eh_rt.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unwind.h>
#if !defined(_WIN32)
#include <dlfcn.h>
#endif

/** @brief Minimal Itanium C++ ABI `std::type_info` header layout (`vptr` + `_ZTS` name). */
typedef struct gcc_be_abi_type_info {
    /** @brief Address-point vtable pointer (`&_ZTVN10__cxxabiv1...E[2]`). */
    const void *vptr;
    /** @brief Mangled `_ZTS` C-string pointer (bit 63 may be set on Darwin ARM64 non-unique RTTI). */
    const char *name;
} gcc_be_abi_type_info_t;

/** @brief Itanium C++ ABI `__si_class_type_info` layout (single public non-virtual inheritance at offset 0). */
typedef struct gcc_be_abi_si_class_type_info {
    /** @brief Base `std::type_info` header. */
    gcc_be_abi_type_info_t base;
    /** @brief Pointer to the single direct public base class `_ZTI` descriptor. */
    const gcc_be_abi_type_info_t *base_type;
} gcc_be_abi_si_class_type_info_t;

/** @brief Itanium C++ ABI `__base_class_type_info` entry inside `__vmi_class_type_info`. */
typedef struct gcc_be_abi_vmi_base_entry {
    /** @brief Pointer to the direct base class `_ZTI` descriptor. */
    const gcc_be_abi_type_info_t *base_type;
    /** @brief Packed `(offset << 8) | (is_virtual ? 1 : 0) | (is_public ? 2 : 0)`. */
    long offset_flags;
} gcc_be_abi_vmi_base_entry_t;

/** @brief Itanium C++ ABI `__vmi_class_type_info` layout (multiple and/or virtual inheritance). */
typedef struct gcc_be_abi_vmi_class_type_info {
    /** @brief Base `std::type_info` header. */
    gcc_be_abi_type_info_t base;
    /** @brief Inheritance graph flags (`0` = ordinary MI, `1` = non-diamond repeat, `2` = diamond, `3` = both). */
    unsigned int flags;
    /** @brief Number of direct base entries in `base_info` (unbounded). */
    unsigned int base_count;
    /** @brief Trailing variable-length array of `base_count` direct base descriptors. */
    gcc_be_abi_vmi_base_entry_t base_info[1];
} gcc_be_abi_vmi_class_type_info_t;

/** @brief Itanium C++ ABI `__pbase_type_info` / `__pointer_type_info` layout. */
typedef struct gcc_be_abi_pbase_type_info {
    /** @brief Base `std::type_info` header. */
    gcc_be_abi_type_info_t base;
    /** @brief Qualifier bitmask (`0x1` = const, `0x2` = volatile, `0x4` = restrict). */
    unsigned int flags;
    /** @brief Pointee `std::type_info` descriptor. */
    const gcc_be_abi_type_info_t *pointee;
} gcc_be_abi_pbase_type_info_t;

extern "C" {

/** @brief Imported system C++ ABI `__class_type_info` vtable. */
extern const void *_ZTVN10__cxxabiv117__class_type_infoE[];
/** @brief Imported system C++ ABI `__si_class_type_info` vtable. */
extern const void *_ZTVN10__cxxabiv120__si_class_type_infoE[];
/** @brief Imported system C++ ABI `__vmi_class_type_info` vtable. */
extern const void *_ZTVN10__cxxabiv121__vmi_class_type_infoE[];
/** @brief Imported system C++ ABI `__pointer_type_info` vtable. */
extern const void *_ZTVN10__cxxabiv119__pointer_type_infoE[];

/** @brief Function signature of standard Itanium ABI `__cxa_begin_catch` and `__cxa_get_exception_ptr`. */
typedef void *(gcc_be_cxa_begin_catch_fn_t)(void *exc_obj) GCC_GEN_BE_NOEXCEPT;
/** @brief Function signature of standard Itanium ABI zero-arg void hooks (`__cxa_end_catch`, `_ZSt9terminatev`). */
typedef void (gcc_be_cxa_void_fn_t)(void) GCC_GEN_BE_NOEXCEPT;
/** @brief Function signature of standard Itanium ABI `__cxa_current_exception_type`. */
typedef const void *(gcc_be_cxa_curr_type_fn_t)(void) GCC_GEN_BE_NOEXCEPT;

/** @brief Standard Itanium C++ ABI `__cxa_begin_catch` hook from `libc++abi`/`libsupc++`. */
extern gcc_be_cxa_begin_catch_fn_t __cxa_begin_catch;
/** @brief Standard Itanium C++ ABI `__cxa_get_exception_ptr` hook from `libc++abi`/`libsupc++`. */
extern gcc_be_cxa_begin_catch_fn_t __cxa_get_exception_ptr;
/** @brief Standard Itanium C++ ABI `__cxa_end_catch` hook from `libc++abi`/`libsupc++`. */
extern gcc_be_cxa_void_fn_t __cxa_end_catch;
/** @brief Standard Itanium C++ ABI `__cxa_current_exception_type` hook from `libc++abi`/`libsupc++`. */
extern gcc_be_cxa_curr_type_fn_t __cxa_current_exception_type;
/** @brief Standard C++ `std::terminate()` symbol from `libc++abi`/`libsupc++`. */
[[noreturn]] extern gcc_be_cxa_void_fn_t _ZSt9terminatev;

/**
 * @brief Low-level DWARF personality-backed assembly trampoline calling `fn(arg)`.
 * @details Saves callee-saved state with `.cfi_*` directives, executes `fn(arg)`, and writes `NULL`
 * to `*out_unwind_exc` on normal return or the intercepted `_Unwind_Exception *` on unwind.
 * @param[in] fn Callback function to invoke (`NULL` triggers `__cxa_rethrow` mark-rethrown helper).
 * @param[in] arg Opaque context pointer passed to `fn`.
 * @param[out] out_unwind_exc Pointer receiving `NULL` on normal completion or `_Unwind_Exception *` on unwind.
 * @return Always `GCC_GEN_BE_SUCCESS` (`0`).
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t __edg_gcc_eh_trampoline_call(
    edg_gcc_eh_region_fn_t fn,
    void *arg,
    void **out_unwind_exc) GCC_GEN_BE_NOEXCEPT;

} /* extern "C" */

#if defined(__APPLE__) && defined(__aarch64__)
__asm__(R"edg_asm(
  .text
  .globl ___edg_gcc_eh_trampoline_call
  .p2align 2
___edg_gcc_eh_trampoline_call:
  .cfi_startproc
  .cfi_personality 155, ___gcc_be_eh_personality_v0
  .cfi_lsda 16, Ledg_eh_tramp_lsda
  stp x20, x19, [sp, #-32]!
  stp x29, x30, [sp, #16]
  add x29, sp, #16
  .cfi_def_cfa w29, 16
  .cfi_offset w30, -8
  .cfi_offset w29, -16
  .cfi_offset w19, -24
  .cfi_offset w20, -32
  mov x19, x2
  mov x8, x0
  mov x0, x1
Ledg_tramp_call_start:
  cbz x8, Ledg_do_rethrow
  blr x8
  b Ledg_tramp_call_end
Ledg_do_rethrow:
  bl ___cxa_rethrow
Ledg_tramp_call_end:
  str xzr, [x19]
  mov w0, #0
  ldp x29, x30, [sp, #16]
  ldp x20, x19, [sp], #32
  ret
Ledg_tramp_landing_pad:
  str x0, [x19]
  mov w0, #0
  ldp x29, x30, [sp, #16]
  ldp x20, x19, [sp], #32
  ret
  .cfi_endproc
  .section __TEXT,__gcc_except_tab
  .p2align 2
Ledg_eh_tramp_lsda:
  .long Ledg_tramp_call_start - ___edg_gcc_eh_trampoline_call
  .long Ledg_tramp_call_end - ___edg_gcc_eh_trampoline_call
  .long Ledg_tramp_landing_pad - ___edg_gcc_eh_trampoline_call
  .text
)edg_asm");
#elif defined(__APPLE__) && defined(__x86_64__)
__asm__(R"edg_asm(
  .text
  .globl ___edg_gcc_eh_trampoline_call
  .p2align 4
___edg_gcc_eh_trampoline_call:
  .cfi_startproc
  .cfi_personality 155, ___gcc_be_eh_personality_v0
  .cfi_lsda 16, Ledg_eh_tramp_lsda
  pushq %rbp
  .cfi_def_cfa_offset 16
  .cfi_offset %rbp, -16
  movq %rsp, %rbp
  .cfi_def_cfa_register %rbp
  pushq %rbx
  pushq %rax
  .cfi_offset %rbx, -24
  movq %rdx, %rbx
  movq %rdi, %rax
  movq %rsi, %rdi
Ledg_tramp_call_start:
  testq %rax, %rax
  je Ledg_do_rethrow
  callq *%rax
  jmp Ledg_tramp_call_end
Ledg_do_rethrow:
  callq ___cxa_rethrow
Ledg_tramp_call_end:
  movq $0, (%rbx)
  xorl %eax, %eax
  addq $8, %rsp
  popq %rbx
  popq %rbp
  retq
Ledg_tramp_landing_pad:
  movq %rax, (%rbx)
  xorl %eax, %eax
  addq $8, %rsp
  popq %rbx
  popq %rbp
  retq
  .cfi_endproc
  .section __TEXT,__gcc_except_tab
  .p2align 2
Ledg_eh_tramp_lsda:
  .long Ledg_tramp_call_start - ___edg_gcc_eh_trampoline_call
  .long Ledg_tramp_call_end - ___edg_gcc_eh_trampoline_call
  .long Ledg_tramp_landing_pad - ___edg_gcc_eh_trampoline_call
  .text
)edg_asm");
#elif defined(__aarch64__)
__asm__(R"edg_asm(
  .text
  .globl __edg_gcc_eh_trampoline_call
  .p2align 2
__edg_gcc_eh_trampoline_call:
  .cfi_startproc
  .cfi_personality 0x9b, DW.ref.__gcc_be_eh_personality_v0
  .cfi_lsda 0x1b, .Ledg_eh_tramp_lsda
  stp x20, x19, [sp, #-32]!
  stp x29, x30, [sp, #16]
  add x29, sp, #16
  .cfi_def_cfa w29, 16
  .cfi_offset w30, -8
  .cfi_offset w29, -16
  .cfi_offset w19, -24
  .cfi_offset w20, -32
  mov x19, x2
  mov x8, x0
  mov x0, x1
.Ledg_tramp_call_start:
  cbz x8, .Ledg_do_rethrow
  blr x8
  b .Ledg_tramp_call_end
.Ledg_do_rethrow:
  bl __cxa_rethrow
.Ledg_tramp_call_end:
  str xzr, [x19]
  mov w0, #0
  ldp x29, x30, [sp, #16]
  ldp x20, x19, [sp], #32
  ret
.Ledg_tramp_landing_pad:
  str x0, [x19]
  mov w0, #0
  ldp x29, x30, [sp, #16]
  ldp x20, x19, [sp], #32
  ret
  .cfi_endproc
  .section .gcc_except_table,"a",@progbits
  .p2align 2
.Ledg_eh_tramp_lsda:
  .long .Ledg_tramp_call_start - __edg_gcc_eh_trampoline_call
  .long .Ledg_tramp_call_end - __edg_gcc_eh_trampoline_call
  .long .Ledg_tramp_landing_pad - __edg_gcc_eh_trampoline_call
  .hidden DW.ref.__gcc_be_eh_personality_v0
  .weak DW.ref.__gcc_be_eh_personality_v0
  .section .data.rel.local.DW.ref.__gcc_be_eh_personality_v0,"awG",@progbits,DW.ref.__gcc_be_eh_personality_v0,comdat
  .p2align 3
DW.ref.__gcc_be_eh_personality_v0:
  .quad __gcc_be_eh_personality_v0
  .text
)edg_asm");
#elif defined(__x86_64__)
__asm__(R"edg_asm(
  .text
  .globl __edg_gcc_eh_trampoline_call
  .p2align 4
__edg_gcc_eh_trampoline_call:
  .cfi_startproc
  .cfi_personality 0x9b, DW.ref.__gcc_be_eh_personality_v0
  .cfi_lsda 0x1b, .Ledg_eh_tramp_lsda
  pushq %rbp
  .cfi_def_cfa_offset 16
  .cfi_offset %rbp, -16
  movq %rsp, %rbp
  .cfi_def_cfa_register %rbp
  pushq %rbx
  pushq %rax
  .cfi_offset %rbx, -24
  movq %rdx, %rbx
  movq %rdi, %rax
  movq %rsi, %rdi
.Ledg_tramp_call_start:
  testq %rax, %rax
  je .Ledg_do_rethrow
  callq *%rax
  jmp .Ledg_tramp_call_end
.Ledg_do_rethrow:
  callq __cxa_rethrow
.Ledg_tramp_call_end:
  movq $0, (%rbx)
  xorl %eax, %eax
  addq $8, %rsp
  popq %rbx
  popq %rbp
  retq
.Ledg_tramp_landing_pad:
  movq %rax, (%rbx)
  xorl %eax, %eax
  addq $8, %rsp
  popq %rbx
  popq %rbp
  retq
  .cfi_endproc
  .section .gcc_except_table,"a",@progbits
  .p2align 2
.Ledg_eh_tramp_lsda:
  .long .Ledg_tramp_call_start - __edg_gcc_eh_trampoline_call
  .long .Ledg_tramp_call_end - __edg_gcc_eh_trampoline_call
  .long .Ledg_tramp_landing_pad - __edg_gcc_eh_trampoline_call
  .hidden DW.ref.__gcc_be_eh_personality_v0
  .weak DW.ref.__gcc_be_eh_personality_v0
  .section .data.rel.local.DW.ref.__gcc_be_eh_personality_v0,"awG",@progbits,DW.ref.__gcc_be_eh_personality_v0,comdat
  .p2align 3
DW.ref.__gcc_be_eh_personality_v0:
  .quad __gcc_be_eh_personality_v0
  .text
)edg_asm");
#endif

/** @brief Per-thread frame chain head pointer (strictly `thread_local`). */
static thread_local void *tls_eh_chain = nullptr;

/** @brief Per-thread active `_Unwind_Exception *` intercepted by `__edg_gcc_eh_invoke_region`. */
static thread_local struct _Unwind_Exception *tls_unwind_exc = nullptr;

/** @brief Per-thread active thrown C++ object address returned by `__cxa_begin_catch`. */
static thread_local void *tls_exc_obj = nullptr;

/** @brief Per-thread active thrown `std::type_info *` returned by `__cxa_current_exception_type`. */
static thread_local const void *tls_exc_tinfo = nullptr;

/** @brief Per-thread flag (`1` while an intercepted unwind is propagating across cleanups/dispatch, else `0`). */
static thread_local int tls_is_unwinding = 0;

/** @brief Per-thread stack of `_Unwind_Exception *` headers for currently active `catch` bodies. */
static thread_local struct _Unwind_Exception *tls_entered_catch_stack[64];

/** @brief Current depth of `tls_entered_catch_stack` on the calling thread. */
static thread_local int tls_entered_catch_depth = 0;

/** @brief Saved per-thread active unwind state when a destructor throws/catches during outer stack unwinding. */
typedef struct gcc_be_saved_unwind {
    /** @brief Outer `_Unwind_Exception *` header currently unwinding. */
    struct _Unwind_Exception *unwind_exc;
    /** @brief Outer thrown C++ object pointer. */
    void *exc_obj;
    /** @brief Outer thrown `std::type_info *` descriptor. */
    const void *exc_tinfo;
} gcc_be_saved_unwind_t;

/** @brief Per-thread stack of suspended outer unwind states across nested cleanup `try`/`catch` blocks. */
static thread_local gcc_be_saved_unwind_t tls_saved_unwind_stack[64];

/** @brief Current depth of `tls_saved_unwind_stack` on the calling thread. */
static thread_local int tls_saved_unwind_depth = 0;

/**
 * @brief Invokes `__edg_gcc_eh_trampoline_call` after checking the deterministic OOM budget.
 * @details Enables unit tests to exercise error percolation on trampoline call sites via `g_gcc_be_oom_countdown`.
 * @param[in] fn Guarded callback function (`NULL` triggers `__cxa_rethrow`).
 * @param[in] arg Opaque callback context pointer.
 * @param[out] out_unwind_exc Pointer receiving `NULL` or the intercepted `_Unwind_Exception *`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM` on injected fault.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t invoke_trampoline_checked(
    edg_gcc_eh_region_fn_t fn,
    void *arg,
    void **out_unwind_exc) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    return __edg_gcc_eh_trampoline_call(fn, arg, out_unwind_exc);
}

/**
 * @brief Extracts the clean C-string from a `std::type_info::__type_name` pointer.
 * @details Clears bit 63 on 64-bit targets (`_LIBCPP_ABI_NON_UNIQUE_TYPEINFO` on Apple Silicon ARM64)
 * so cross-TU `strcmp` on `_ZTS` names never dereferences a tagged high-bit pointer.
 * @param[in] ti Non-null pointer to the `gcc_be_abi_type_info_t` descriptor.
 * @param[out] out_name Non-null pointer receiving the untagged C-string (or NULL).
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM` on injected fault.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t untag_type_info_name(
    const gcc_be_abi_type_info_t *ti,
    const char **out_name) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (!ti->name) {
        *out_name = nullptr;
        return GCC_GEN_BE_SUCCESS;
    }
    uintptr_t raw = (uintptr_t)ti->name;
#if INTPTR_MAX == INT64_MAX
    raw &= (uintptr_t)0x7FFFFFFFFFFFFFFFULL;
#endif
    *out_name = (const char *)raw;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Checks whether two Itanium C++ ABI `std::type_info` descriptors represent the same type.
 * @details Compares descriptor addresses first, then untagged `_ZTS` type-name strings.
 * @param[in] a Non-null first `type_info` descriptor.
 * @param[in] b Non-null second `type_info` descriptor.
 * @param[out] out_equal Non-null pointer receiving `1` if equal, `0` otherwise.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM` on injected fault.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t abi_type_info_equals(
    const gcc_be_abi_type_info_t *a,
    const gcc_be_abi_type_info_t *b,
    int *out_equal) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_equal = 0;
    if (a == b) {
        *out_equal = 1;
        return GCC_GEN_BE_SUCCESS;
    }
    const char *na = nullptr;
    const char *nb = nullptr;
    GCC_GEN_BE_CHECK(untag_type_info_name(a, &na));
    GCC_GEN_BE_CHECK(untag_type_info_name(b, &nb));
    if (na && nb && strcmp(na, nb) == 0) {
        *out_equal = 1;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Classifies an Itanium C++ ABI `_ZTI` descriptor by inspecting its vtable pointer (`vptr`).
 * @details Compares `ti->vptr` against the standard Itanium address point (`&vtable[2]`) of
 * `_ZTVN10__cxxabiv117__class_type_infoE` (`1`), `__si_class_type_info` (`2`),
 * `__vmi_class_type_info` (`3`), and `__pointer_type_info` (`4`), returning `0` for leaf/fundamental types.
 * Never reads past the 16-byte `std::type_info` header!
 * @param[in] ti Non-null `_ZTI` descriptor.
 * @param[out] out_kind Non-null pointer receiving `0` (leaf), `1` (class), `2` (si_class), `3` (vmi_class), or `4` (pointer).
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM` on injected fault.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t classify_abi_type_info_vptr(
    const gcc_be_abi_type_info_t *ti,
    int *out_kind) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    const void *vp = ti->vptr;
    if (vp == (const void *)&_ZTVN10__cxxabiv117__class_type_infoE[2] ||
        vp == (const void *)&_ZTVN10__cxxabiv117__class_type_infoE[0]) {
        *out_kind = 1;
    } else if (vp == (const void *)&_ZTVN10__cxxabiv120__si_class_type_infoE[2] ||
               vp == (const void *)&_ZTVN10__cxxabiv120__si_class_type_infoE[0]) {
        *out_kind = 2;
    } else if (vp == (const void *)&_ZTVN10__cxxabiv121__vmi_class_type_infoE[2] ||
               vp == (const void *)&_ZTVN10__cxxabiv121__vmi_class_type_infoE[0]) {
        *out_kind = 3;
    } else if (vp == (const void *)&_ZTVN10__cxxabiv119__pointer_type_infoE[2] ||
               vp == (const void *)&_ZTVN10__cxxabiv119__pointer_type_infoE[0]) {
        *out_kind = 4;
    } else {
        *out_kind = 0;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Recursively searches a thrown class hierarchy (`__si_class_type_info` / `__vmi_class_type_info`)
 * for an unambiguous public base matching `catch_ti`.
 * @details Supports all `__vmi_class_type_info` flag values (`0` = ordinary MI, `1` = non-diamond repeat,
 * `2` = diamond virtual base, `3` = both), arbitrary `base_count`, and detects ambiguous repeated
 * non-virtual bases by verifying all matching paths adjust to the identical subobject address.
 * @param[in] catch_ti Non-null target catch class `_ZTI` descriptor.
 * @param[in] throw_ti Non-null current thrown/base `_ZTI` descriptor.
 * @param[in] curr_obj Current subobject pointer corresponding to `throw_ti`.
 * @param[in,out] io_found_obj Non-null pointer to first matched subobject address.
 * @param[in,out] io_match_state Non-null state (`0` = not found, `1` = uniquely matched, `-1` = ambiguous).
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t search_class_hierarchy(
    const gcc_be_abi_type_info_t *catch_ti,
    const gcc_be_abi_type_info_t *throw_ti,
    void *curr_obj,
    void **io_found_obj,
    int *io_match_state) GCC_GEN_BE_NOEXCEPT {
    int eq = 0;
    GCC_GEN_BE_CHECK(abi_type_info_equals(catch_ti, throw_ti, &eq));
    if (eq) {
        if (*io_match_state == 0) {
            *io_found_obj = curr_obj;
            *io_match_state = 1;
        } else if (*io_found_obj != curr_obj) {
            /* Two distinct subobjects of the same base type -> ambiguous base, cannot catch! */
            *io_match_state = -1;
        }
        return GCC_GEN_BE_SUCCESS;
    }

    int kind = 0;
    GCC_GEN_BE_CHECK(classify_abi_type_info_vptr(throw_ti, &kind));
    if (kind == 2) {
        const gcc_be_abi_si_class_type_info_t *si =
            (const gcc_be_abi_si_class_type_info_t *)throw_ti;
        if (si->base_type) {
            GCC_GEN_BE_CHECK(search_class_hierarchy(
                catch_ti, si->base_type, curr_obj, io_found_obj, io_match_state));
        }
    } else if (kind == 3) {
        const gcc_be_abi_vmi_class_type_info_t *vmi =
            (const gcc_be_abi_vmi_class_type_info_t *)throw_ti;
        for (unsigned int i = 0; i < vmi->base_count; ++i) {
            const gcc_be_abi_type_info_t *bti = vmi->base_info[i].base_type;
            long oflags = vmi->base_info[i].offset_flags;
            /* Only public bases (0x2 = __public_mask) match C++ catch clauses */
            if (!bti || (oflags & 0x2L) == 0) {
                continue;
            }
            ptrdiff_t off = (ptrdiff_t)(oflags >> 8);
            void *sub_obj = curr_obj;
            if (curr_obj) {
                if ((oflags & 0x1L) != 0 && off < 0) {
                    /* Virtual base: negative `off` is byte offset in vtable from vptr */
                    const char *vptr = *(const char *const *)curr_obj;
                    ptrdiff_t vbase_delta = *(const ptrdiff_t *)(vptr + off);
                    sub_obj = (void *)((char *)curr_obj + vbase_delta);
                } else {
                    sub_obj = (void *)((char *)curr_obj + off);
                }
            }
            GCC_GEN_BE_CHECK(search_class_hierarchy(
                catch_ti, bti, sub_obj, io_found_obj, io_match_state));
            if (*io_match_state == -1) {
                return GCC_GEN_BE_SUCCESS;
            }
            /* Optimization: if non-diamond repeat (0x1) bit is clear, first match is unique */
            if (*io_match_state == 1 && (vmi->flags & 0x1U) == 0U) {
                return GCC_GEN_BE_SUCCESS;
            }
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Matches a catch `_ZTI` against a thrown `_ZTI` including pointer qualification/upcast conversions.
 * @details Handles exact matches, class upcasts, and C++ pointer catch conversions (`Derived* -> const Base*`,
 * `T* -> const void*`, and `nullptr_t -> P*`).
 * @param[in] catch_ti Non-null catch clause `_ZTI` descriptor.
 * @param[in] throw_ti Non-null thrown object `_ZTI` descriptor.
 * @param[in,out] io_obj_ptr Non-null pointer to object address (for pointers, points to the thrown pointer value!).
 * @param[out] out_matched Non-null pointer receiving `1` on match, `0` on mismatch.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t match_abi_type_info_full(
    const gcc_be_abi_type_info_t *catch_ti,
    const gcc_be_abi_type_info_t *throw_ti,
    void **io_obj_ptr,
    int *out_matched) GCC_GEN_BE_NOEXCEPT {
    *out_matched = 0;

    int eq = 0;
    GCC_GEN_BE_CHECK(abi_type_info_equals(catch_ti, throw_ti, &eq));
    if (eq) {
        *out_matched = 1;
        return GCC_GEN_BE_SUCCESS;
    }

    int catch_kind = 0;
    int throw_kind = 0;
    GCC_GEN_BE_CHECK(classify_abi_type_info_vptr(catch_ti, &catch_kind));
    GCC_GEN_BE_CHECK(classify_abi_type_info_vptr(throw_ti, &throw_kind));

    /* Pointer catch handler (`catch (Base *)`, `catch (const void *)`, etc.) */
    if (catch_kind == 4) {
        const gcc_be_abi_pbase_type_info_t *cp = (const gcc_be_abi_pbase_type_info_t *)catch_ti;
        const char *throw_name = nullptr;
        GCC_GEN_BE_CHECK(untag_type_info_name(throw_ti, &throw_name));
        /* std::nullptr_t ("Dn") converts to any pointer catch handler */
        if (throw_name && strcmp(throw_name, "Dn") == 0) {
            *out_matched = 1;
            return GCC_GEN_BE_SUCCESS;
        }
        if (throw_kind != 4 || !cp->pointee) {
            return GCC_GEN_BE_SUCCESS;
        }
        const gcc_be_abi_pbase_type_info_t *tp = (const gcc_be_abi_pbase_type_info_t *)throw_ti;
        /* Thrown pointee cv-qualifiers must be a subset of catch pointee cv-qualifiers */
        if ((tp->flags & ~cp->flags) != 0U || !tp->pointee) {
            return GCC_GEN_BE_SUCCESS;
        }
        const char *catch_ptee_name = nullptr;
        GCC_GEN_BE_CHECK(untag_type_info_name(cp->pointee, &catch_ptee_name));
        if (catch_ptee_name && strcmp(catch_ptee_name, "v") == 0) {
            /* Any non-function object pointer converts to [cv] void* */
            const char *tp_ptee_name = nullptr;
            GCC_GEN_BE_CHECK(untag_type_info_name(tp->pointee, &tp_ptee_name));
            if (!tp_ptee_name || tp_ptee_name[0] != 'F') {
                *out_matched = 1;
            }
            return GCC_GEN_BE_SUCCESS;
        }
        int ptee_eq = 0;
        GCC_GEN_BE_CHECK(abi_type_info_equals(cp->pointee, tp->pointee, &ptee_eq));
        if (ptee_eq) {
            *out_matched = 1;
            return GCC_GEN_BE_SUCCESS;
        }
        /* Pointer to derived class -> pointer to unambiguous public base class */
        void *deref_ptr = (*io_obj_ptr) ? *(void **)(*io_obj_ptr) : nullptr;
        void *found_base = nullptr;
        int state = 0;
        GCC_GEN_BE_CHECK(search_class_hierarchy(cp->pointee, tp->pointee, deref_ptr, &found_base, &state));
        if (state == 1) {
            if (deref_ptr != nullptr) {
                *(void **)(*io_obj_ptr) = found_base;
            }
            *out_matched = 1;
        }
        return GCC_GEN_BE_SUCCESS;
    }

    /* Class hierarchy catch (`catch (const Base &)`) */
    if (throw_kind == 2 || throw_kind == 3) {
        void *found_obj = nullptr;
        int state = 0;
        GCC_GEN_BE_CHECK(search_class_hierarchy(catch_ti, throw_ti, *io_obj_ptr, &found_obj, &state));
        if (state == 1) {
            *io_obj_ptr = found_obj;
            *out_matched = 1;
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

extern "C" {

gcc_gen_be_error_t __edg_gcc_eh_get_tls_chain(void ***out_chain_slot) GCC_GEN_BE_NOEXCEPT {
    if (!out_chain_slot) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_chain_slot = &tls_eh_chain;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t __gcc_be_eh_register_frame(void *fde) GCC_GEN_BE_NOEXCEPT {
    if (!fde) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    tls_eh_chain = fde;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t __gcc_be_eh_unregister_frame(void *fde) GCC_GEN_BE_NOEXCEPT {
    if (!fde) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    if (tls_eh_chain == fde) {
        tls_eh_chain = nullptr;
    }
    return GCC_GEN_BE_SUCCESS;
}

int __gcc_be_eh_check_unwind_state(void) GCC_GEN_BE_NOEXCEPT {
    return tls_is_unwinding;
}

gcc_gen_be_error_t __edg_gcc_eh_invoke_region(
    edg_gcc_eh_region_fn_t body_fn,
    void *ctx,
    int *out_unwound) GCC_GEN_BE_NOEXCEPT {
    if (!body_fn || !out_unwound) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_unwound = 0;

    void *raw_unwind_exc = nullptr;
    GCC_GEN_BE_CHECK(invoke_trampoline_checked(body_fn, ctx, &raw_unwind_exc));
    if (!raw_unwind_exc) {
        return GCC_GEN_BE_SUCCESS;
    }

    if (tls_is_unwinding && tls_unwind_exc != (struct _Unwind_Exception *)raw_unwind_exc) {
        int slot = tls_saved_unwind_depth & 63;
        tls_saved_unwind_stack[slot].unwind_exc = tls_unwind_exc;
        tls_saved_unwind_stack[slot].exc_obj = tls_exc_obj;
        tls_saved_unwind_stack[slot].exc_tinfo = tls_exc_tinfo;
        ++tls_saved_unwind_depth;
    }

    tls_unwind_exc = (struct _Unwind_Exception *)raw_unwind_exc;
    __cxa_begin_catch(raw_unwind_exc);
    tls_exc_tinfo = __cxa_current_exception_type();
    uint64_t exc_cls = (uint64_t)tls_unwind_exc->exception_class;
    if ((exc_cls & 0xFFU) == 0x01U) {
        void **exc_words = (void **)raw_unwind_exc;
        int prim_idx = ((uint32_t)(exc_cls >> 32U) == 0x474E5543U) ? -10 : -11;
        tls_exc_obj = exc_words[prim_idx];
    } else {
        tls_exc_obj = (void *)((struct _Unwind_Exception *)raw_unwind_exc + 1);
    }
    tls_is_unwinding = 1;
    *out_unwound = 1;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t __edg_gcc_eh_get_unwind_info(
    void **out_exc_obj,
    void **out_tinfo) GCC_GEN_BE_NOEXCEPT {
    if (!out_exc_obj || !out_tinfo) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_exc_obj = tls_exc_obj;
    *out_tinfo = (void *)tls_exc_tinfo;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t __edg_gcc_eh_consume_catch(
    void **out_exc_obj) GCC_GEN_BE_NOEXCEPT {
    if (!out_exc_obj) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    if (tls_entered_catch_depth < 64) {
        tls_entered_catch_stack[tls_entered_catch_depth++] = tls_unwind_exc;
    }
    tls_is_unwinding = 0;
    *out_exc_obj = tls_exc_obj;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t __edg_gcc_eh_finish_catch(void) GCC_GEN_BE_NOEXCEPT {
    if (tls_entered_catch_depth > 0) {
        --tls_entered_catch_depth;
    }
    __cxa_end_catch();
    if (tls_saved_unwind_depth > 0) {
        --tls_saved_unwind_depth;
        int slot = tls_saved_unwind_depth & 63;
        tls_unwind_exc = tls_saved_unwind_stack[slot].unwind_exc;
        tls_exc_obj = tls_saved_unwind_stack[slot].exc_obj;
        tls_exc_tinfo = tls_saved_unwind_stack[slot].exc_tinfo;
        tls_is_unwinding = 1;
    } else {
        tls_is_unwinding = 0;
        tls_unwind_exc = nullptr;
        tls_exc_obj = nullptr;
        tls_exc_tinfo = nullptr;
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t __edg_gcc_eh_end_enclosing_catch(void) GCC_GEN_BE_NOEXCEPT {
    if (tls_entered_catch_depth <= 0) {
        return GCC_GEN_BE_SUCCESS;
    }
    struct _Unwind_Exception *prev_exc = tls_entered_catch_stack[tls_entered_catch_depth - 1];
    if (!tls_unwind_exc || tls_unwind_exc == prev_exc) {
        --tls_entered_catch_depth;
        if (!tls_is_unwinding) {
            tls_unwind_exc = nullptr;
            tls_exc_obj = nullptr;
            tls_exc_tinfo = nullptr;
        }
        /* Same exception (`throw;` -> handlerCount == 2) or normal catch exit: single __cxa_end_catch() */
        __cxa_end_catch();
        return GCC_GEN_BE_SUCCESS;
    }
    /* A brand new exception `tls_unwind_exc` was thrown inside `catch (prev_exc)`:
       pop `tls_unwind_exc` temporarily without destroying it, destroy `prev_exc`, and re-enter `tls_unwind_exc`. */
    void *rethrow_intercepted = nullptr;
    GCC_GEN_BE_CHECK(invoke_trampoline_checked(nullptr, nullptr, &rethrow_intercepted));
    --tls_entered_catch_depth;
    __cxa_end_catch();
    __cxa_end_catch();
    __cxa_begin_catch(tls_unwind_exc);
    tls_exc_obj = (void *)(tls_unwind_exc + 1);
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t __edg_gcc_eh_resume_or_rethrow(void) GCC_GEN_BE_NOEXCEPT {
    if (!tls_unwind_exc) {
        return GCC_GEN_BE_ERROR_EH_RUNTIME_BRIDGE_FAILED;
    }
    /* Mark `caughtExceptions` top as rethrown (`handlerCount = -1`), then `__cxa_end_catch()`
       restores `handlerCount = 0` without destroying the exception before `_Unwind_RaiseException`. */
    void *rethrow_intercepted = nullptr;
    GCC_GEN_BE_CHECK(invoke_trampoline_checked(nullptr, nullptr, &rethrow_intercepted));

    struct _Unwind_Exception *exc_to_propagate = tls_unwind_exc;
    tls_is_unwinding = 0;
    tls_unwind_exc = nullptr;
    tls_exc_obj = nullptr;
    tls_exc_tinfo = nullptr;
    __cxa_end_catch();

    _Unwind_RaiseException(exc_to_propagate);
    /* Reached only when no handler exists anywhere up the call stack: invoke std::terminate()! */
    _ZSt9terminatev();
}

int __gcc_be_eh_personality_v0(
    int version,
    int actions,
    uint64_t exception_class,
    void *unwind_exc,
    void *context) GCC_GEN_BE_NOEXCEPT {
    if (version != 1 || !unwind_exc || !context) {
        return (int)_URC_FATAL_PHASE1_ERROR;
    }

    uintptr_t fn_base = _Unwind_GetRegionStart((struct _Unwind_Context *)context);
    const uint32_t *lsda =
        (const uint32_t *)_Unwind_GetLanguageSpecificData((struct _Unwind_Context *)context);
    if (!lsda) {
        return (int)_URC_CONTINUE_UNWIND;
    }

    uintptr_t ip = _Unwind_GetIP((struct _Unwind_Context *)context);
    uintptr_t start_pc = fn_base + (uintptr_t)lsda[0];
    uintptr_t end_pc = fn_base + (uintptr_t)lsda[1];
    uintptr_t lp_pc = fn_base + (uintptr_t)lsda[2];
    if (ip <= start_pc || ip > end_pc) {
        return (int)_URC_CONTINUE_UNWIND;
    }

    if ((actions & _UA_SEARCH_PHASE) != 0) {
        return (int)_URC_HANDLER_FOUND;
    }
    if ((actions & _UA_CLEANUP_PHASE) != 0) {
        _Unwind_SetGR(
            (struct _Unwind_Context *)context,
            __builtin_eh_return_data_regno(0),
            (uintptr_t)unwind_exc);
        _Unwind_SetIP((struct _Unwind_Context *)context, lp_pc);
        return (int)_URC_INSTALL_CONTEXT;
    }
    return (int)_URC_CONTINUE_UNWIND;
}

void *__gcc_be_eh_match_catch(
    const void *catch_type,
    const void *throw_type,
    void *obj_ptr) GCC_GEN_BE_NOEXCEPT {
    if (!catch_type) {
        return obj_ptr ? obj_ptr : (void *)(uintptr_t)1;
    }
    if (!throw_type) {
        return nullptr;
    }
    void *adjusted = obj_ptr;
    int matched = 0;
    if (match_abi_type_info_full(
            (const gcc_be_abi_type_info_t *)catch_type,
            (const gcc_be_abi_type_info_t *)throw_type,
            &adjusted,
            &matched) == GCC_GEN_BE_SUCCESS &&
        matched) {
        return adjusted ? adjusted : (void *)(uintptr_t)1;
    }
    return nullptr;
}

} /* extern "C" */
