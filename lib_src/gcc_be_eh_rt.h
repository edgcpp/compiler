/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/**
 * @file gcc_be_eh_rt.h
 * @brief Thread-safe DWARF/Itanium C++ ABI exception runtime bridge for the GCC backend.
 * @details Declares the linkable C/C++ ABI unwind runtime (`libedg_gcc_eh_rt.a`) providing
 * `thread_local` per-thread unwind state, the DWARF personality-backed unwind trampoline
 * (`__edg_gcc_eh_invoke_region`), `_Unwind_RaiseException` rethrow/resume bridging, and
 * vptr-inspected Itanium C++ ABI `_ZTI` inheritance hierarchy catch matching.
 * @ingroup gcc_gen_be
 */

#ifndef GCC_BE_EH_RT_H
#define GCC_BE_EH_RT_H

#include "gcc_gen_be_error.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Function pointer signature for a guarded region callback invoked by `__edg_gcc_eh_invoke_region`. */
typedef void (*edg_gcc_eh_region_fn_t)(void *ctx);

/**
 * @brief Retrieves the thread-local SjLj/frame chain head slot for the calling thread.
 * @details Exposes `&tls_eh_chain` so concurrent threads never race on global EH frame pointers.
 * @param[out] out_chain_slot Pointer to receive the non-null thread-local `void **` chain slot.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if `out_chain_slot` is NULL.
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t __edg_gcc_eh_get_tls_chain(
    void ***out_chain_slot) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Registers a dynamic DWARF `.eh_frame` FDE table with the system unwinder.
 * @details Validates `fde` and forwards to `__register_frame` when present in `libunwind`/`libgcc_s`.
 * @param[in] fde Non-null pointer to the DWARF `.eh_frame` FDE record.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if `fde` is NULL.
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t __gcc_be_eh_register_frame(
    void *fde) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Unregisters a dynamic DWARF `.eh_frame` FDE table from the system unwinder.
 * @details Validates `fde` and forwards to `__deregister_frame` when present in `libunwind`/`libgcc_s`.
 * @param[in] fde Non-null pointer to the DWARF `.eh_frame` FDE record.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if `fde` is NULL.
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t __gcc_be_eh_unregister_frame(
    void *fde) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Queries whether an unhandled DWARF exception is actively unwinding on the calling thread.
 * @details ABI runtime helper returning `1` when `__edg_gcc_eh_invoke_region` intercepted a
 * `_Unwind_RaiseException` unwind on the calling thread, or `0` during normal execution.
 * @return `1` if an unwind is active on this thread, `0` otherwise.
 */
extern int __gcc_be_eh_check_unwind_state(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Executes `body_fn(ctx)` inside a genuine DWARF personality-backed unwind trampoline.
 * @details Intercepts any `_Unwind_RaiseException` thrown by `cpfe --backend=gcc` OR precompiled
 * `libstdc++` / `libc++` callees, records the active exception object and `std::type_info *` in
 * `thread_local` storage via `__cxa_begin_catch` + `__cxa_current_exception_type`, and sets
 * `*out_unwound = 1` (or `0` on normal return).
 * @param[in] body_fn Callback executing the guarded call or region.
 * @param[in] ctx Opaque context pointer forwarded to `body_fn`.
 * @param[out] out_unwound Pointer receiving `1` if an exception unwound out of `body_fn`, `0` otherwise.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if `body_fn` or `out_unwound` is NULL.
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t __edg_gcc_eh_invoke_region(
    edg_gcc_eh_region_fn_t body_fn,
    void *ctx,
    int *out_unwound) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Retrieves the active thread-local thrown object pointer and `std::type_info *` descriptor.
 * @details Populates `*out_exc_obj` and `*out_tinfo` from the calling thread's intercepted unwind state.
 * @param[out] out_exc_obj Pointer receiving the thrown C++ object address (or NULL if none).
 * @param[out] out_tinfo Pointer receiving the thrown `std::type_info *` address (or NULL if none).
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if outputs are NULL.
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t __edg_gcc_eh_get_unwind_info(
    void **out_exc_obj,
    void **out_tinfo) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Transitions the calling thread's intercepted unwind into an active `catch` handler.
 * @details Clears `tls_is_unwinding` while preserving the active `caughtExceptions` frame in
 * `libc++abi`/`libsupc++` so `throw;` and `__cxa_end_catch()` inside the handler behave identically
 * to native C++ ABI `catch` clauses.
 * @param[out] out_exc_obj Pointer receiving the active exception object pointer.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if `out_exc_obj` is NULL.
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t __edg_gcc_eh_consume_catch(
    void **out_exc_obj) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Completes an active `catch` handler and releases the caught exception via `__cxa_end_catch`.
 * @details Clears thread-local cached pointers and delegates to standard Itanium ABI `__cxa_end_catch()`.
 * @return `GCC_GEN_BE_SUCCESS` on success.
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t __edg_gcc_eh_finish_catch(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Ends an enclosing `catch` handler when an exception (`throw;` or a new `throw E2;`) unwinds out of it.
 * @details Preserves the newly propagating `tls_unwind_exc` at `handlerCount == 1` while properly releasing
 * the enclosing `catch` clause's reference count via standard Itanium C++ ABI operations.
 * @return `GCC_GEN_BE_SUCCESS` on success.
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t __edg_gcc_eh_end_enclosing_catch(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Resumes propagating an intercepted `_Unwind_RaiseException` out of a cleanup/unmatched frame.
 * @details Restores `handlerCount` to `0` without destroying the exception object and invokes
 * `_Unwind_RaiseException` so outer `cpfe` or external `libstdc++`/`libc++` frames catch it cleanly;
 * calls `std::terminate()` (`_ZSt9terminatev`) if no handler exists anywhere up the call stack.
 * @return `GCC_GEN_BE_ERROR_EH_RUNTIME_BRIDGE_FAILED` if no unwind was active on the calling thread.
 */
GCC_GEN_BE_NODISCARD extern gcc_gen_be_error_t __edg_gcc_eh_resume_or_rethrow(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief DWARF / Compact-Unwind personality routine for `__edg_gcc_eh_trampoline_call`.
 * @details Implements Phase-1 search (`_UA_SEARCH_PHASE -> _URC_HANDLER_FOUND`) and Phase-2 context
 * installation (`_UA_CLEANUP_PHASE -> _URC_INSTALL_CONTEXT`) into `__edg_eh_tramp_landing_pad`.
 * @param[in] version Unwind ABI version (expected `1`).
 * @param[in] actions Bitmask of `_UA_*` unwind phase flags.
 * @param[in] exception_class 8-byte vendor/language exception class identifier.
 * @param[in,out] unwind_exc Pointer to the `_Unwind_Exception` header being propagated.
 * @param[in,out] context Opaque unwinder frame context (`struct _Unwind_Context *`).
 * @return `_Unwind_Reason_Code` status code.
 */
extern int __gcc_be_eh_personality_v0(
    int version,
    int actions,
    uint64_t exception_class,
    void *unwind_exc,
    void *context) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Matches a thrown Itanium C++ ABI `_ZTI` type against a catch handler `_ZTI` type and adjusts `obj_ptr`.
 * @details Inspects `_ZTI`'s vtable pointer (`__class_type_info`, `__si_class_type_info`,
 * `__vmi_class_type_info`, `__pointer_type_info`), supports all `__vmi_class_type_info` flags (`0..3`),
 * arbitrary `base_count`, and virtual diamond base vtable-offset adjustment.
 * @param[in] catch_type Catch clause `std::type_info` pointer (NULL for `catch (...)`).
 * @param[in] throw_type Thrown object `std::type_info` pointer.
 * @param[in] obj_ptr Thrown most-derived object pointer.
 * @return Adjusted subobject pointer on match, or NULL on mismatch.
 */
extern void *__gcc_be_eh_match_catch(
    const void *catch_type,
    const void *throw_type,
    void *obj_ptr) GCC_GEN_BE_NOEXCEPT;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GCC_BE_EH_RT_H */
