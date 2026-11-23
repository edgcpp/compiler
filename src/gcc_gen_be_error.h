/**
 * @defgroup gcc_gen_be EDG GCC (libgccjit) Code Generation Backend
 * @brief Native code generation backend lowering EDG IL to machine code via `libgccjit`.
 * @details Encompasses type, declaration, expression, statement, ABI, RTTI/vtable, DWARF/SEH
 * exception handling, C++20 coroutines, C11 atomics, SIMD, OpenMP `libgomp`, and debug lowering.
 */

/**
 * @file gcc_gen_be_error.h
 * @brief Centralized error handling for the GCC code generation backend.
 * @details Defines the centralized nodiscard error enumeration (`gcc_gen_be_error_t`),
 * non-throwing (`GCC_GEN_BE_NOEXCEPT`) macro, and error-propagation macros
 * (`GCC_GEN_BE_CHECK` and `GCC_GEN_BE_CHECK_CLEANUP`) used across the GCC backend.
 * @defgroup gcc_gen_be EDG GCC (libgccjit) Code Generation Backend
 * @ingroup gcc_gen_be
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_ERROR_H
#define GCC_GEN_BE_ERROR_H

#if defined(_MSC_VER) && !defined(_SSIZE_T_DEFINED)
#include <stddef.h>
typedef ptrdiff_t ssize_t;
#define _SSIZE_T_DEFINED
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Macro to enforce that the return value of a function is not ignored.
 *
 * Applies the `warn_unused_result` attribute to ensure that functions returning
 * an error code are properly checked by the caller.
 */
#if (defined(__cplusplus) && __cplusplus >= 201703L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
#define GCC_GEN_BE_NODISCARD [[nodiscard]]
#elif defined(__GNUC__) || defined(__clang__)
#define GCC_GEN_BE_NODISCARD __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#define GCC_GEN_BE_NODISCARD _Check_return_
#else
#define GCC_GEN_BE_NODISCARD
#endif

/** @brief Macro to mark GCC backend functions as non-throwing in C++11+. */
#if (defined(__cplusplus) && __cplusplus >= 201103L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201103L)
#define GCC_GEN_BE_NOEXCEPT noexcept
#else
/** @brief Macro to mark GCC backend functions as non-throwing in C / C++98. */
#define GCC_GEN_BE_NOEXCEPT
#endif

/**
 * @enum gcc_gen_be_error
 * @brief Represents the possible error codes returned by the GCC backend.
 *
 * @details Decorated with GCC_GEN_BE_NODISCARD so that any function returning
 * gcc_gen_be_error_t automatically enforces non-discardable return values.
 */
#if (defined(__cplusplus) && __cplusplus >= 201703L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
enum GCC_GEN_BE_NODISCARD gcc_gen_be_error {
#else
enum gcc_gen_be_error {
#endif
    /** @brief Indicates that the operation completed successfully. */
    GCC_GEN_BE_SUCCESS = 0,

    /** @brief Indicates that a memory allocation failed (Out Of Memory). */
    GCC_GEN_BE_ERROR_OOM,

    /** @brief Indicates that an unsupported operation or language feature was encountered. */
    GCC_GEN_BE_ERROR_UNSUPPORTED,

    /** @brief Indicates that the libgccjit dynamic library could not be loaded. */
    GCC_GEN_BE_ERROR_LIBGCCJIT_LOAD_FAILED,

    /** @brief Indicates that a required symbol could not be found in libgccjit. */
    GCC_GEN_BE_ERROR_LIBGCCJIT_SYMBOL_MISSING,

    /** @brief Indicates an invalid argument was passed to a function. */
    GCC_GEN_BE_ERROR_INVALID_ARGUMENT,

    /** @brief Indicates the backend context or current block is in an invalid state. */
    GCC_GEN_BE_ERROR_INVALID_STATE,

    /** @brief Indicates an internal compiler error within the backend. */
    GCC_GEN_BE_ERROR_INTERNAL,

    /** @brief Indicates that a specific type could not be handled. */
    GCC_GEN_BE_ERROR_UNHANDLED_TYPE,

    /** @brief Indicates that a specific expression could not be handled. */
    GCC_GEN_BE_ERROR_UNHANDLED_EXPR,

    /** @brief Indicates that a specific statement could not be handled. */
    GCC_GEN_BE_ERROR_UNHANDLED_STMT,

    /** @brief Indicates that a specific declaration could not be handled. */
    GCC_GEN_BE_ERROR_UNHANDLED_DECL,

    /** @brief Indicates that a required pointer was null. */
    GCC_GEN_BE_ERROR_NULL_POINTER,

    /** @brief Indicates a type mismatch during code generation. */
    GCC_GEN_BE_ERROR_TYPE_MISMATCH,

    /** @brief Indicates a failure in exception handling setup or generation. */
    GCC_GEN_BE_ERROR_EH_FAILURE,

    /** @brief Indicates that exception handling is not supported for the target. */
    GCC_GEN_BE_ERROR_EH_UNSUPPORTED,

    /** @brief Indicates that creating a landing pad for a try block failed. */
    GCC_GEN_BE_ERROR_EH_LANDING_PAD_FAILED,

    /** @brief Indicates that generating an EH table failed. */
    GCC_GEN_BE_ERROR_EH_TABLE_GENERATION_FAILED,

    /** @brief Indicates that a symbol could not be resolved. */
    GCC_GEN_BE_ERROR_UNRESOLVED_SYMBOL,

    /** @brief Indicates that a builtin is unknown or unsupported. */
    GCC_GEN_BE_ERROR_BUILTIN_UNKNOWN,

    /** @brief Indicates that a builtin signature did not match expectations. */
    GCC_GEN_BE_ERROR_BUILTIN_SIGNATURE_MISMATCH,

    /** @brief Indicates an ABI, calling convention, or structural layout mismatch. */
    GCC_GEN_BE_ERROR_ABI_MISMATCH,

    /** @brief Indicates an unsupported attribute was encountered. */
    GCC_GEN_BE_ERROR_UNSUPPORTED_ATTRIBUTE,

    /** @brief Indicates that generating a vtable failed. */
    GCC_GEN_BE_ERROR_VTABLE_GENERATION_FAILED,

    /** @brief Indicates that vtable layout computation failed. */
    GCC_GEN_BE_ERROR_VTABLE_LAYOUT_FAILED,

    /** @brief Indicates that an RTTI descriptor is invalid. */
    GCC_GEN_BE_ERROR_RTTI_INVALID,

    /** @brief Indicates that generating RTTI information failed. */
    GCC_GEN_BE_ERROR_RTTI_GENERATION_FAILED,

    /** @brief Indicates that RTTI emission failed. */
    GCC_GEN_BE_ERROR_RTTI_EMISSION_FAILED,

    /** @brief Indicates that an inline assembly constraint is invalid. */
    GCC_GEN_BE_ERROR_ASM_CONSTRAINT_INVALID,

    /** @brief Indicates that DWARF debug information emission failed. */
    GCC_GEN_BE_ERROR_DWARF_EMISSION_FAILED,

    /** @brief Indicates that DWARF DIE creation failed. */
    GCC_GEN_BE_ERROR_DWARF_DIE_CREATION_FAILED,

    /** @brief Indicates that JIT compilation failed. */
    GCC_GEN_BE_ERROR_COMPILATION_FAILED,

    /** @brief Indicates that VTT generation failed. */
    GCC_GEN_BE_ERROR_VTT_GENERATION_FAILED,

    /** @brief Indicates that thunk generation failed. */
    GCC_GEN_BE_ERROR_THUNK_GENERATION_FAILED,

    /** @brief Indicates that target ABI parameter classification failed. */
    GCC_GEN_BE_ERROR_ABI_CLASSIFICATION_FAILED,

    /** @brief Indicates that the calling convention is unsupported. */
    GCC_GEN_BE_ERROR_UNSUPPORTED_CALLING_CONV,

    /** @brief Indicates that DWARF type emission failed. */
    GCC_GEN_BE_ERROR_DWARF_TYPE_EMISSION_FAILED,

    /** @brief Indicates that an inline assembly operand is unsupported. */
    GCC_GEN_BE_ERROR_ASM_OPERAND_UNSUPPORTED,

    /** @brief Indicates that struct packing failed. */
    GCC_GEN_BE_ERROR_STRUCT_PACKING_FAILED,

    /** @brief Indicates that bitfield layout failed. */
    GCC_GEN_BE_ERROR_BITFIELD_LAYOUT_FAILED,

    /** @brief Indicates that coroutine frame generation failed. */
    GCC_GEN_BE_ERROR_COROUTINE_FRAME_FAILED,

    /** @brief Indicates that coroutine ramp generation failed. */
    GCC_GEN_BE_ERROR_COROUTINE_RAMP_FAILED,

    /** @brief Indicates that coroutine dispatch generation failed. */
    GCC_GEN_BE_ERROR_COROUTINE_DISPATCH_FAILED,

    /** @brief Indicates that CFI unwind table generation failed. */
    GCC_GEN_BE_ERROR_CFI_UNWIND_FAILED,

    /** @brief Indicates that SIMD lowering failed. */
    GCC_GEN_BE_ERROR_SIMD_LOWERING_FAILED,

    /** @brief Indicates that a target option is invalid. */
    GCC_GEN_BE_ERROR_TARGET_OPTION_INVALID,

    /** @brief Indicates that variadic argument (`va_arg`) state-machine lowering failed. */
    GCC_GEN_BE_ERROR_VA_ARG_LOWERING_FAILED,

    /** @brief Indicates that an Itanium C++ ABI mangled name exceeded buffer capacity. */
    GCC_GEN_BE_ERROR_MANGLING_BUFFER_OVERFLOW,

    /** @brief Indicates that Itanium C++ ABI substitution table management failed. */
    GCC_GEN_BE_ERROR_MANGLING_SUBSTITUTION_FAILED,

    /** @brief Indicates that array `new[]`/`delete[]` cookie calculation or allocation failed. */
    GCC_GEN_BE_ERROR_COOKIE_ALLOCATION_FAILED,

    /** @brief Indicates that 128-bit or extended integer constant conversion failed. */
    GCC_GEN_BE_ERROR_WIDE_INT_CONVERSION_FAILED,

    /** @brief Indicates that variable-length array (VLA) stack allocation failed. */
    GCC_GEN_BE_ERROR_VLA_ALLOCATION_FAILED,

    /** @brief Indicates that computed goto or label address resolution failed. */
    GCC_GEN_BE_ERROR_LABEL_RESOLUTION_FAILED,

    /** @brief Indicates that SIMD vector shuffle or convertvector permutation failed. */
    GCC_GEN_BE_ERROR_VECTOR_PERMUTE_FAILED,

    /** @brief Indicates that extended floating-point precision conversion failed. */
    GCC_GEN_BE_ERROR_FLOAT_PRECISION_FAILED,

    /** @brief Indicates that multi-TU compilation or linker pipeline orchestration failed. */
    GCC_GEN_BE_ERROR_MULTI_TU_PIPELINE_FAILED,

    /** @brief Indicates that __dso_handle symbol declaration or global destructor registration failed. */
    GCC_GEN_BE_ERROR_DSO_HANDLE_FAILED,

    /** @brief Indicates that target ABI register coercion or eightbyte/HFA unpacking failed. */
    GCC_GEN_BE_ERROR_COERCION_FAILED,

    /** @brief Indicates that applying weak or COMDAT linkage attributes failed. */
    GCC_GEN_BE_ERROR_WEAK_LINKAGE_FAILED,

    /** @brief Indicates that emitting RAII scope unwind cleanup instructions failed. */
    GCC_GEN_BE_ERROR_UNWIND_CLEANUP_FAILED,

    /** @brief Indicates that coroutine promise object allocation or method resolution failed. */
    GCC_GEN_BE_ERROR_COROUTINE_PROMISE_FAILED,

    /** @brief Indicates that coroutine co_await/co_yield suspension lowering failed. */
    GCC_GEN_BE_ERROR_COROUTINE_SUSPEND_FAILED,

    /** @brief Indicates that an atomic builtin operand had invalid size or alignment. */
    GCC_GEN_BE_ERROR_ATOMIC_ALIGNMENT_FAILED,

    /** @brief Indicates that constructing an abnormal unwind CFG edge failed. */
    GCC_GEN_BE_ERROR_EH_CFG_EDGE_FAILED,

    /** @brief Indicates that coroutine live-variable liveness or state-machine splitting failed. */
    GCC_GEN_BE_ERROR_COROUTINE_SPLIT_FAILED,

    /** @brief Indicates that ABI register coercion prologue or epilogue unpacking failed. */
    GCC_GEN_BE_ERROR_ABI_PROLOGUE_UNPACK_FAILED,

    /** @brief Indicates that COMDAT or linkonce section group attachment failed. */
    GCC_GEN_BE_ERROR_COMDAT_GROUP_FAILED,

    /** @brief Indicates that DWARF lexical scope or member DIE attachment failed. */
    GCC_GEN_BE_ERROR_DWARF_SCOPE_FAILED,

    /** @brief Indicates that DWARF DIE emission or attachment failed. */
    GCC_GEN_BE_ERROR_DWARF_DIE_FAILED,

    /** @brief Indicates that DWARF or SEH LSDA exception table encoding failed. */
    GCC_GEN_BE_ERROR_LSDA_ENCODE_FAILED,

    /** @brief Indicates that C++20 module import symbol or BMI/IFC lowering failed. */
    GCC_GEN_BE_ERROR_MODULE_IMPORT_FAILED,

    /** @brief Indicates that C++20 module export or initializer emission failed. */
    GCC_GEN_BE_ERROR_MODULE_EXPORT_FAILED,

    /** @brief Indicates that coroutine symmetric transfer lowering failed. */
    GCC_GEN_BE_ERROR_CORO_TRANSFER_FAILED,

    /** @brief Indicates that coroutine heap allocation elision optimization (HALO) failed. */
    GCC_GEN_BE_ERROR_CORO_HALO_FAILED,

    /** @brief Indicates that an ISO fixed-point conversion or arithmetic operation overflowed. */
    GCC_GEN_BE_ERROR_FIXED_POINT_OVERFLOW,

    /** @brief Indicates that an IEEE 754 decimal floating-point operation is unsupported. */
    GCC_GEN_BE_ERROR_DECIMAL_FLOAT_UNSUPPORTED,

    /** @brief Indicates that an IEEE 754 DPD decimal floating-point conversion failed. */
    GCC_GEN_BE_ERROR_DPD_FLOAT_CONVERSION_FAILED,

    /** @brief Indicates that emitting a VLA scope stack save or restore failed. */
    GCC_GEN_BE_ERROR_VLA_STACK_RESTORE_FAILED,

    /** @brief Indicates that binding a DWARF DIE to a symbol or scope failed. */
    GCC_GEN_BE_ERROR_DWARF_DIE_BINDING_FAILED,

    /** @brief Indicates that emitting a label-difference or symbol relocation failed. */
    GCC_GEN_BE_ERROR_RELOCATION_EMISSION_FAILED,

    /** @brief Indicates that emitting a must-tail virtual thunk call failed. */
    GCC_GEN_BE_ERROR_TAIL_CALL_THUNK_FAILED,

    /** @brief Indicates that OpenMP pragma or libgomp runtime lowering failed. */
    GCC_GEN_BE_ERROR_OMP_LOWERING_FAILED,

    /** @brief Indicates that recursive C++ namespace or class scope traversal failed. */
    GCC_GEN_BE_ERROR_SCOPE_TRAVERSAL_FAILED,

    /** @brief Indicates that a bit-precise integer (_BitInt(N)) width is unsupported by the target. */
    GCC_GEN_BE_ERROR_BITINT_UNSUPPORTED,

    /** @brief Indicates that a C++ exception runtime unwind bridge operation failed. */
    GCC_GEN_BE_ERROR_EH_RUNTIME_BRIDGE_FAILED,

    /** @brief Indicates that a unit test assertion failed. */
    GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED
};

/**
 * @brief Centralised non-discardable error return type for all GCC backend functions.
 */
typedef enum gcc_gen_be_error gcc_gen_be_error_t;

/**
 * @brief Macro to invoke an expression and return immediately if it fails.
 *
 * This macro evaluates the given expression, which must return a `gcc_gen_be_error_t`.
 * If the returned value is not `GCC_GEN_BE_SUCCESS`, the macro immediately
 * returns that error code from the current function.
 *
 * @param expr The expression to evaluate.
 */
#define GCC_GEN_BE_CHECK(expr) \
    do { \
        gcc_gen_be_error_t _err = (expr); \
        if (_err != GCC_GEN_BE_SUCCESS) { \
            return _err; \
        } \
    } while (0)

/**
 * @brief Macro to invoke an expression, store its error in `err_var`, and jump to `cleanup_label` on failure.
 *
 * @param expr The expression returning `gcc_gen_be_error_t` to evaluate.
 * @param err_var The `gcc_gen_be_error_t` variable to receive the status code.
 * @param cleanup_label The label to jump to if `expr` does not return `GCC_GEN_BE_SUCCESS`.
 */
#define GCC_GEN_BE_CHECK_CLEANUP(expr, err_var, cleanup_label) \
    do { \
        (err_var) = (expr); \
        if ((err_var) != GCC_GEN_BE_SUCCESS) { \
            goto cleanup_label; \
        } \
    } while (0)

/**
 * @brief Converts a GCC backend error code to a human-readable string.
 *
 * @param error The error code to convert.
 * @param out_str Pointer to a string pointer to populate with the string representation.
 * @return A gcc_gen_be_error_t indicating success or invalid argument.
 */
extern gcc_gen_be_error_t gcc_gen_be_error_string(gcc_gen_be_error_t error, const char **out_str) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Reports a backend error using EDG diagnostic facilities.
 *
 * This function logs the error string to f_error, optionally increments
 * diagnostic error counters, and prints an additional context message.
 *
 * @param error The backend error code that occurred.
 * @param context_msg Additional context message to display (can be NULL).
 * @return The passed-in error code for convenience in return statements.
 */
extern gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t error, const char *context_msg) GCC_GEN_BE_NOEXCEPT;

#ifdef __cplusplus
}
#endif

#if defined(__cplusplus) && defined(BACK_END_IS_GCC_GEN_BE)
#include <stdlib.h>

/**
 * @brief Returns a pointer to the process-wide deterministic OOM fault-injection countdown.
 * @details Initialized to `-1` (disabled); `0` fails the next allocation, `>0` decrements on each step.
 * @return Non-null pointer to the active OOM countdown integer.
 */
inline int *gcc_gen_be_oom_countdown_ptr(void) GCC_GEN_BE_NOEXCEPT {
    static int s_oom_countdown = -1;
    return &s_oom_countdown;
}

/** @brief Convenience macro accessing the process-wide deterministic OOM countdown. */
#define g_gcc_be_oom_countdown (*gcc_gen_be_oom_countdown_ptr())

/**
 * @brief Advances the deterministic OOM allocation budget for fault-injection testing.
 * @details Returns `GCC_GEN_BE_ERROR_OOM` and resets `g_gcc_be_oom_countdown` to `-1` when `0`;
 * decrements positive countdowns and returns `GCC_GEN_BE_SUCCESS` otherwise.
 * @return `GCC_GEN_BE_SUCCESS` if allocation may proceed, or `GCC_GEN_BE_ERROR_OOM` on injected exhaustion.
 */
GCC_GEN_BE_NODISCARD inline gcc_gen_be_error_t gcc_gen_be_step_oom_budget(void) GCC_GEN_BE_NOEXCEPT {
    if (g_gcc_be_oom_countdown == 0) {
        g_gcc_be_oom_countdown = -1;
        return GCC_GEN_BE_ERROR_OOM;
    }
    if (g_gcc_be_oom_countdown > 0) {
        --g_gcc_be_oom_countdown;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Allocates `bytes` of zero-checked heap memory with deterministic OOM fault-injection support.
 * @details Steps `g_gcc_be_oom_countdown` and invokes `malloc`, returning `GCC_GEN_BE_ERROR_OOM` on failure.
 * @param[in] bytes Number of bytes to allocate (clamped to at least 1 byte).
 * @param[out] out_ptr Pointer receiving the allocated heap block.
 * @return `GCC_GEN_BE_SUCCESS` on success, `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if `out_ptr` is NULL, or `GCC_GEN_BE_ERROR_OOM`.
 */
GCC_GEN_BE_NODISCARD inline gcc_gen_be_error_t gcc_gen_be_alloc(
    size_t bytes,
    void **out_ptr) GCC_GEN_BE_NOEXCEPT {
    if (!out_ptr) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_ptr = NULL;
    void *p = (gcc_gen_be_step_oom_budget() == GCC_GEN_BE_SUCCESS)
                  ? malloc(bytes > 0 ? bytes : 1U)
                  : NULL;
    if (!p) {
        return GCC_GEN_BE_ERROR_OOM;
    }
    *out_ptr = p;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Resizes a heap buffer to `bytes` with deterministic OOM fault-injection support.
 * @details Steps `g_gcc_be_oom_countdown` and invokes `realloc`, preserving `old_ptr` on failure.
 * @param[in,out] old_ptr Existing heap pointer (or NULL for initial allocation).
 * @param[in] bytes New capacity in bytes (clamped to at least 1 byte).
 * @param[out] out_ptr Pointer receiving the resized heap block.
 * @return `GCC_GEN_BE_SUCCESS` on success, `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if `out_ptr` is NULL, or `GCC_GEN_BE_ERROR_OOM`.
 */
GCC_GEN_BE_NODISCARD inline gcc_gen_be_error_t gcc_gen_be_realloc_buf(
    void *old_ptr,
    size_t bytes,
    void **out_ptr) GCC_GEN_BE_NOEXCEPT {
    if (!out_ptr) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_ptr = NULL;
    void *p = (gcc_gen_be_step_oom_budget() == GCC_GEN_BE_SUCCESS)
                  ? realloc(old_ptr, bytes > 0 ? bytes : 1U)
                  : NULL;
    if (!p) {
        return GCC_GEN_BE_ERROR_OOM;
    }
    *out_ptr = p;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Combines a primary operation status with a cleanup operation status without discarding either.
 * @details Returns `primary_err` when `primary_err != GCC_GEN_BE_SUCCESS`, or `secondary_err` otherwise.
 * @param[in] primary_err Primary operation error status.
 * @param[in] secondary_err Secondary/cleanup operation error status.
 * @return First non-success `gcc_gen_be_error_t`, or `GCC_GEN_BE_SUCCESS` if both succeeded.
 */
GCC_GEN_BE_NODISCARD inline gcc_gen_be_error_t gcc_gen_be_first_error(
    gcc_gen_be_error_t primary_err,
    gcc_gen_be_error_t secondary_err) GCC_GEN_BE_NOEXCEPT {
    return (primary_err != GCC_GEN_BE_SUCCESS) ? primary_err : secondary_err;
}
#endif /* __cplusplus */

#endif /* GCC_GEN_BE_ERROR_H */
