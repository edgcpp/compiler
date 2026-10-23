/**
 * @file gcc_gen_be_error.c
 * @brief Implementation of error handling utilities for the GCC backend.
 *
 * This file provides the implementation for functions defined in gcc_gen_be_error.h,
 * primarily for converting error codes into human-readable strings.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_error.h"


/**
 * @brief Converts a GCC backend error code to a human-readable string.
 *
 * @param error The error code to convert.
 * @param out_str Pointer to a string pointer to populate with the string representation.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT if out_str is NULL.
 */
gcc_gen_be_error_t gcc_gen_be_error_string(gcc_gen_be_error_t error, const char **out_str) GCC_GEN_BE_NOEXCEPT {
    if (!out_str) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }

    switch (error) {
        case GCC_GEN_BE_SUCCESS:
            *out_str = "Success: The operation completed successfully.";
            break;
        case GCC_GEN_BE_ERROR_OOM:
            *out_str = "Error: Out of memory (memory allocation failed).";
            break;
        case GCC_GEN_BE_ERROR_UNSUPPORTED:
            *out_str = "Error: An unsupported operation or language feature was encountered.";
            break;
        case GCC_GEN_BE_ERROR_LIBGCCJIT_LOAD_FAILED:
            *out_str = "Error: The libgccjit dynamic library could not be loaded.";
            break;
        case GCC_GEN_BE_ERROR_LIBGCCJIT_SYMBOL_MISSING:
            *out_str = "Error: A required symbol could not be found in libgccjit.";
            break;
        case GCC_GEN_BE_ERROR_INVALID_ARGUMENT:
            *out_str = "Error: An invalid argument (e.g., null output pointer) was passed to a function.";
            break;
        case GCC_GEN_BE_ERROR_INVALID_STATE:
            *out_str = "Error: The GCC backend context or block is in an invalid state.";
            break;
        case GCC_GEN_BE_ERROR_INTERNAL:
            *out_str = "Error: An internal compiler error occurred within the GCC backend.";
            break;
        case GCC_GEN_BE_ERROR_UNHANDLED_TYPE:
            *out_str = "Error: A specific type could not be handled or lowered.";
            break;
        case GCC_GEN_BE_ERROR_UNHANDLED_EXPR:
            *out_str = "Error: A specific expression could not be handled or lowered.";
            break;
        case GCC_GEN_BE_ERROR_UNHANDLED_STMT:
            *out_str = "Error: A specific statement could not be handled or lowered.";
            break;
        case GCC_GEN_BE_ERROR_UNHANDLED_DECL:
            *out_str = "Error: A specific declaration could not be handled or lowered.";
            break;
        case GCC_GEN_BE_ERROR_NULL_POINTER:
            *out_str = "Error: A required pointer was null.";
            break;
        case GCC_GEN_BE_ERROR_TYPE_MISMATCH:
            *out_str = "Error: A type mismatch was detected during code generation.";
            break;
        case GCC_GEN_BE_ERROR_EH_FAILURE:
            *out_str = "Error: A failure occurred during exception handling setup or generation.";
            break;
        case GCC_GEN_BE_ERROR_EH_UNSUPPORTED:
            *out_str = "Error: Exception handling is not supported for the target architecture.";
            break;
        case GCC_GEN_BE_ERROR_EH_LANDING_PAD_FAILED:
            *out_str = "Error: Creating a landing pad for a try block failed.";
            break;
        case GCC_GEN_BE_ERROR_EH_TABLE_GENERATION_FAILED:
            *out_str = "Error: Generating an EH table failed.";
            break;
        case GCC_GEN_BE_ERROR_UNRESOLVED_SYMBOL:
            *out_str = "Error: A required symbol could not be resolved in the current context.";
            break;
        case GCC_GEN_BE_ERROR_BUILTIN_UNKNOWN:
            *out_str = "Error: A requested compiler builtin function is unknown or unsupported.";
            break;
        case GCC_GEN_BE_ERROR_BUILTIN_SIGNATURE_MISMATCH:
            *out_str = "Error: A builtin function signature did not match expectations.";
            break;
        case GCC_GEN_BE_ERROR_ABI_MISMATCH:
            *out_str = "Error: An ABI, calling convention, or structural layout mismatch occurred.";
            break;
        case GCC_GEN_BE_ERROR_UNSUPPORTED_ATTRIBUTE:
            *out_str = "Error: An unsupported or invalid attribute was encountered.";
            break;
        case GCC_GEN_BE_ERROR_VTABLE_GENERATION_FAILED:
            *out_str = "Error: Generating a virtual table (vtable) failed.";
            break;
        case GCC_GEN_BE_ERROR_VTABLE_LAYOUT_FAILED:
            *out_str = "Error: Computing virtual table (vtable) layout failed.";
            break;
        case GCC_GEN_BE_ERROR_RTTI_INVALID:
            *out_str = "Error: An invalid RTTI descriptor was encountered.";
            break;
        case GCC_GEN_BE_ERROR_RTTI_GENERATION_FAILED:
            *out_str = "Error: Generating RTTI information failed.";
            break;
        case GCC_GEN_BE_ERROR_RTTI_EMISSION_FAILED:
            *out_str = "Error: Emitting RTTI information failed.";
            break;
        case GCC_GEN_BE_ERROR_ASM_CONSTRAINT_INVALID:
            *out_str = "Error: An inline assembly constraint is invalid or unsupported.";
            break;
        case GCC_GEN_BE_ERROR_DWARF_EMISSION_FAILED:
            *out_str = "Error: DWARF debug information emission failed.";
            break;
        case GCC_GEN_BE_ERROR_DWARF_DIE_CREATION_FAILED:
            *out_str = "Error: DWARF Debugging Information Entry (DIE) creation failed.";
            break;
        case GCC_GEN_BE_ERROR_COMPILATION_FAILED:
            *out_str = "Error: JIT compilation failed.";
            break;
        case GCC_GEN_BE_ERROR_VTT_GENERATION_FAILED:
            *out_str = "Error: Generating Virtual Table Table (VTT) failed.";
            break;
        case GCC_GEN_BE_ERROR_THUNK_GENERATION_FAILED:
            *out_str = "Error: Generating virtual method thunk failed.";
            break;
        case GCC_GEN_BE_ERROR_ABI_CLASSIFICATION_FAILED:
            *out_str = "Error: Target ABI parameter classification failed.";
            break;
        case GCC_GEN_BE_ERROR_UNSUPPORTED_CALLING_CONV:
            *out_str = "Error: The requested calling convention is unsupported.";
            break;
        case GCC_GEN_BE_ERROR_DWARF_TYPE_EMISSION_FAILED:
            *out_str = "Error: DWARF type debug information emission failed.";
            break;
        case GCC_GEN_BE_ERROR_ASM_OPERAND_UNSUPPORTED:
            *out_str = "Error: An inline assembly operand is unsupported.";
            break;
        case GCC_GEN_BE_ERROR_STRUCT_PACKING_FAILED:
            *out_str = "Error: Struct packing failed.";
            break;
        case GCC_GEN_BE_ERROR_BITFIELD_LAYOUT_FAILED:
            *out_str = "Error: Bitfield layout calculation failed.";
            break;
        case GCC_GEN_BE_ERROR_COROUTINE_FRAME_FAILED:
            *out_str = "Error: Coroutine frame layout generation failed.";
            break;
        case GCC_GEN_BE_ERROR_COROUTINE_RAMP_FAILED:
            *out_str = "Error: Coroutine ramp function generation failed.";
            break;
        case GCC_GEN_BE_ERROR_COROUTINE_DISPATCH_FAILED:
            *out_str = "Error: Coroutine dispatch logic generation failed.";
            break;
        case GCC_GEN_BE_ERROR_CFI_UNWIND_FAILED:
            *out_str = "Error: Zero-cost DWARF CFI exception handling table generation failed.";
            break;
        case GCC_GEN_BE_ERROR_SIMD_LOWERING_FAILED:
            *out_str = "Error: SIMD vector intrinsic lowering failed.";
            break;
        case GCC_GEN_BE_ERROR_TARGET_OPTION_INVALID:
            *out_str = "Error: Target option or attribute is invalid.";
            break;
        case GCC_GEN_BE_ERROR_VA_ARG_LOWERING_FAILED:
            *out_str = "Error: Variadic argument (va_arg) state-machine lowering failed.";
            break;
        case GCC_GEN_BE_ERROR_MANGLING_BUFFER_OVERFLOW:
            *out_str = "Error: Itanium C++ ABI mangled name exceeded buffer capacity.";
            break;
        case GCC_GEN_BE_ERROR_MANGLING_SUBSTITUTION_FAILED:
            *out_str = "Error: Itanium C++ ABI substitution table operation failed.";
            break;
        case GCC_GEN_BE_ERROR_COOKIE_ALLOCATION_FAILED:
            *out_str = "Error: Array new[]/delete[] cookie calculation or allocation failed.";
            break;
        case GCC_GEN_BE_ERROR_WIDE_INT_CONVERSION_FAILED:
            *out_str = "Error: 128-bit or wide integer constant conversion failed.";
            break;
        case GCC_GEN_BE_ERROR_VLA_ALLOCATION_FAILED:
            *out_str = "Error: Variable-length array (VLA) stack allocation failed.";
            break;
        case GCC_GEN_BE_ERROR_LABEL_RESOLUTION_FAILED:
            *out_str = "Error: Computed goto or label address resolution failed.";
            break;
        case GCC_GEN_BE_ERROR_VECTOR_PERMUTE_FAILED:
            *out_str = "Error: SIMD vector shuffle or convertvector permutation failed.";
            break;
        case GCC_GEN_BE_ERROR_FLOAT_PRECISION_FAILED:
            *out_str = "Error: Extended floating-point precision conversion failed.";
            break;
        case GCC_GEN_BE_ERROR_MULTI_TU_PIPELINE_FAILED:
            *out_str = "Error: Multi-TU compilation or linker pipeline orchestration failed.";
            break;
        case GCC_GEN_BE_ERROR_DSO_HANDLE_FAILED:
            *out_str = "Error: __dso_handle symbol declaration or global destructor registration failed.";
            break;
        case GCC_GEN_BE_ERROR_COERCION_FAILED:
            *out_str = "Error: Target ABI register coercion or eightbyte/HFA unpacking failed.";
            break;
        case GCC_GEN_BE_ERROR_WEAK_LINKAGE_FAILED:
            *out_str = "Error: Applying weak or COMDAT linkage attributes failed.";
            break;
        case GCC_GEN_BE_ERROR_UNWIND_CLEANUP_FAILED:
            *out_str = "Error: Emitting RAII scope unwind cleanup instructions failed.";
            break;
        case GCC_GEN_BE_ERROR_COROUTINE_PROMISE_FAILED:
            *out_str = "Error: Coroutine promise object allocation or method resolution failed.";
            break;
        case GCC_GEN_BE_ERROR_COROUTINE_SUSPEND_FAILED:
            *out_str = "Error: Coroutine co_await/co_yield suspension lowering failed.";
            break;
        case GCC_GEN_BE_ERROR_ATOMIC_ALIGNMENT_FAILED:
            *out_str = "Error: Atomic builtin operand had invalid size or alignment.";
            break;
        case GCC_GEN_BE_ERROR_EH_CFG_EDGE_FAILED:
            *out_str = "Error: Constructing an abnormal unwind CFG edge failed.";
            break;
        case GCC_GEN_BE_ERROR_COROUTINE_SPLIT_FAILED:
            *out_str = "Error: Coroutine live-variable liveness or state-machine splitting failed.";
            break;
        case GCC_GEN_BE_ERROR_ABI_PROLOGUE_UNPACK_FAILED:
            *out_str = "Error: ABI register coercion prologue or epilogue unpacking failed.";
            break;
        case GCC_GEN_BE_ERROR_COMDAT_GROUP_FAILED:
            *out_str = "Error: COMDAT or linkonce section group attachment failed.";
            break;
        case GCC_GEN_BE_ERROR_DWARF_SCOPE_FAILED:
            *out_str = "Error: DWARF lexical scope or member DIE attachment failed.";
            break;
        case GCC_GEN_BE_ERROR_DWARF_DIE_FAILED:
            *out_str = "Error: DWARF DIE emission or attachment failed.";
            break;
        case GCC_GEN_BE_ERROR_LSDA_ENCODE_FAILED:
            *out_str = "Error: DWARF or SEH LSDA exception table encoding failed.";
            break;
        case GCC_GEN_BE_ERROR_MODULE_IMPORT_FAILED:
            *out_str = "Error: C++20 module import symbol or BMI/IFC lowering failed.";
            break;
        case GCC_GEN_BE_ERROR_MODULE_EXPORT_FAILED:
            *out_str = "Error: C++20 module export or initializer emission failed.";
            break;
        case GCC_GEN_BE_ERROR_CORO_TRANSFER_FAILED:
            *out_str = "Error: Coroutine symmetric transfer lowering failed.";
            break;
        case GCC_GEN_BE_ERROR_CORO_HALO_FAILED:
            *out_str = "Error: Coroutine heap allocation elision optimization (HALO) failed.";
            break;
        case GCC_GEN_BE_ERROR_FIXED_POINT_OVERFLOW:
            *out_str = "Error: ISO fixed-point conversion or arithmetic operation overflowed.";
            break;
        case GCC_GEN_BE_ERROR_DECIMAL_FLOAT_UNSUPPORTED:
            *out_str = "Error: IEEE 754 decimal floating-point operation is unsupported.";
            break;
        case GCC_GEN_BE_ERROR_DPD_FLOAT_CONVERSION_FAILED:
            *out_str = "Error: IEEE 754 DPD decimal floating-point conversion failed.";
            break;
        case GCC_GEN_BE_ERROR_VLA_STACK_RESTORE_FAILED:
            *out_str = "Error: Emitting a VLA scope stack save or restore failed.";
            break;
        case GCC_GEN_BE_ERROR_DWARF_DIE_BINDING_FAILED:
            *out_str = "Error: Binding a DWARF DIE to a symbol or scope failed.";
            break;
        case GCC_GEN_BE_ERROR_RELOCATION_EMISSION_FAILED:
            *out_str = "Error: Emitting a label-difference or symbol relocation failed.";
            break;
        case GCC_GEN_BE_ERROR_TAIL_CALL_THUNK_FAILED:
            *out_str = "Error: Emitting a must-tail virtual thunk call failed.";
            break;
        case GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED:
            *out_str = "Error: Unit test assertion failed.";
            break;
        default:
            *out_str = "Error: An unknown error occurred.";
            break;
    }
    
    return GCC_GEN_BE_SUCCESS;
}
/**
 * @brief Reports a backend error using EDG diagnostic facilities.
 *
 * @details Retrieves the descriptive string for the given error code and
 * formats it alongside any optional context message into a diagnostic buffer.
 *
 * @param[in] error The backend error code that occurred.
 * @param[in] context_msg Additional context message to display (can be NULL).
 * @return The passed-in error code for convenience in return statements.
 */
gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t error, const char *context_msg) GCC_GEN_BE_NOEXCEPT {
    const char *err_str = NULL;
    gcc_gen_be_error_t lookup_err = gcc_gen_be_error_string(error, &err_str);
    if (lookup_err != GCC_GEN_BE_SUCCESS || !err_str) {
        return error;
    }
    char formatted_diag[512];
    if (context_msg && context_msg[0] != '\0') {
        snprintf(formatted_diag, sizeof(formatted_diag), "%s [%s]", err_str, context_msg);
    } else {
        snprintf(formatted_diag, sizeof(formatted_diag), "%s", err_str);
    }
    if (formatted_diag[0] == '\0') {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    if (error != GCC_GEN_BE_SUCCESS) {
        if (fprintf(stderr, "%s\n", formatted_diag) < 0) {
            return error;
        }
    }
    return error;
}
