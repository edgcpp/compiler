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
#include "error.h"

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
            *out_str = "GCC_GEN_BE_SUCCESS";
            break;
        case GCC_GEN_BE_ERROR_OOM:
            *out_str = "GCC_GEN_BE_ERROR_OOM";
            break;
        case GCC_GEN_BE_ERROR_UNSUPPORTED:
            *out_str = "GCC_GEN_BE_ERROR_UNSUPPORTED";
            break;
        case GCC_GEN_BE_ERROR_LIBGCCJIT_LOAD_FAILED:
            *out_str = "GCC_GEN_BE_ERROR_LIBGCCJIT_LOAD_FAILED";
            break;
        case GCC_GEN_BE_ERROR_LIBGCCJIT_SYMBOL_MISSING:
            *out_str = "GCC_GEN_BE_ERROR_LIBGCCJIT_SYMBOL_MISSING";
            break;
        case GCC_GEN_BE_ERROR_INVALID_ARGUMENT:
            *out_str = "GCC_GEN_BE_ERROR_INVALID_ARGUMENT";
            break;
        case GCC_GEN_BE_ERROR_INTERNAL:
            *out_str = "GCC_GEN_BE_ERROR_INTERNAL";
            break;
        case GCC_GEN_BE_ERROR_UNHANDLED_TYPE:
            *out_str = "GCC_GEN_BE_ERROR_UNHANDLED_TYPE";
            break;
        case GCC_GEN_BE_ERROR_UNHANDLED_EXPR:
            *out_str = "GCC_GEN_BE_ERROR_UNHANDLED_EXPR";
            break;
        case GCC_GEN_BE_ERROR_UNHANDLED_STMT:
            *out_str = "GCC_GEN_BE_ERROR_UNHANDLED_STMT";
            break;
        case GCC_GEN_BE_ERROR_UNHANDLED_DECL:
            *out_str = "GCC_GEN_BE_ERROR_UNHANDLED_DECL";
            break;
        case GCC_GEN_BE_ERROR_NULL_POINTER:
            *out_str = "GCC_GEN_BE_ERROR_NULL_POINTER";
            break;
        case GCC_GEN_BE_ERROR_TYPE_MISMATCH:
            *out_str = "GCC_GEN_BE_ERROR_TYPE_MISMATCH";
            break;
        case GCC_GEN_BE_ERROR_EH_FAILURE:
            *out_str = "GCC_GEN_BE_ERROR_EH_FAILURE";
            break;
        case GCC_GEN_BE_ERROR_EH_UNSUPPORTED:
            *out_str = "GCC_GEN_BE_ERROR_EH_UNSUPPORTED";
            break;
        case GCC_GEN_BE_ERROR_UNRESOLVED_SYMBOL:
            *out_str = "GCC_GEN_BE_ERROR_UNRESOLVED_SYMBOL";
            break;
        case GCC_GEN_BE_ERROR_VTABLE_GENERATION_FAILED:
            *out_str = "GCC_GEN_BE_ERROR_VTABLE_GENERATION_FAILED";
            break;
        case GCC_GEN_BE_ERROR_RTTI_GENERATION_FAILED:
            *out_str = "GCC_GEN_BE_ERROR_RTTI_GENERATION_FAILED";
            break;
        case GCC_GEN_BE_ERROR_ASM_CONSTRAINT_INVALID:
            *out_str = "GCC_GEN_BE_ERROR_ASM_CONSTRAINT_INVALID";
            break;
        case GCC_GEN_BE_ERROR_DWARF_EMISSION_FAILED:
            *out_str = "GCC_GEN_BE_ERROR_DWARF_EMISSION_FAILED";
            break;
        case GCC_GEN_BE_ERROR_COMPILATION_FAILED:
            *out_str = "GCC_GEN_BE_ERROR_COMPILATION_FAILED";
            break;
        default:
            *out_str = "GCC_GEN_BE_ERROR_UNKNOWN";
            break;
    }
    
    return GCC_GEN_BE_SUCCESS;
}
/**
 * @brief Reports a backend error using EDG diagnostic facilities.
 *
 * @param error The backend error code that occurred.
 * @param context_msg Additional context message to display (can be NULL).
 * @return The passed-in error code for convenience in return statements.
 */
gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t error, const char *context_msg) GCC_GEN_BE_NOEXCEPT {
    const char *err_msg = NULL;
    gcc_gen_be_error_string(error, &err_msg);
    if (!err_msg) {
        err_msg = "Unknown";
    }

    if (f_error) {
        if (context_msg) {
            fprintf(f_error, "GCC Backend Error: %s - %s\n", err_msg, context_msg);
        } else {
            fprintf(f_error, "GCC Backend Error: %s\n", err_msg);
        }
    }

    if (error == GCC_GEN_BE_ERROR_OOM || 
        error == GCC_GEN_BE_ERROR_INTERNAL || 
        error == GCC_GEN_BE_ERROR_COMPILATION_FAILED) {
        diagnostic_counters.total.catastrophes++;
    } else {
        diagnostic_counters.total.errors++;
    }

    return error;
}
