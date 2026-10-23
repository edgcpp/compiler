/**
 * @file test_gcc_be_errors.cpp
 * @brief Comprehensive standalone unit tests for `gcc_gen_be_error_t` and diagnostic reporting.
 * @details Verifies every enumerator string in `gcc_gen_be_error_string` and `gcc_gen_be_report_diagnostic`.
 */

#include "basic_hdrs.h"
#include "gcc_gen_be_error.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/**
 * @brief Represents a mapping between an error code and its expected human-readable string.
 * @details Used to verify `gcc_gen_be_error_string` across all enumerators.
 */
typedef struct error_test_case {
    /** @brief The error code to test. */
    gcc_gen_be_error_t code;
    /** @brief The expected string representation. */
    const char *expected;
} error_test_case_t;

/**
 * @brief Executes all error string and diagnostic reporting assertions.
 * @details Verifies null pointer handling, all defined enumerators, unknown error fallback,
 * and `gcc_gen_be_report_diagnostic`.
 * @param[out] out_passed Pointer to receive 1 if all checks pass, 0 otherwise.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code on failure.
 */
static gcc_gen_be_error_t run_error_tests(int *out_passed) GCC_GEN_BE_NOEXCEPT {
    if (!out_passed) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_passed = 0;

    const char *str = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_error_string(GCC_GEN_BE_SUCCESS, NULL);
    if (err != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }

    const error_test_case_t test_cases[] = {
        {GCC_GEN_BE_SUCCESS, "Success: The operation completed successfully."},
        {GCC_GEN_BE_ERROR_OOM, "Error: Out of memory (memory allocation failed)."},
        {GCC_GEN_BE_ERROR_UNSUPPORTED, "Error: An unsupported operation or language feature was encountered."},
        {GCC_GEN_BE_ERROR_LIBGCCJIT_LOAD_FAILED, "Error: The libgccjit dynamic library could not be loaded."},
        {GCC_GEN_BE_ERROR_LIBGCCJIT_SYMBOL_MISSING, "Error: A required symbol could not be found in libgccjit."},
        {GCC_GEN_BE_ERROR_INVALID_ARGUMENT, "Error: An invalid argument (e.g., null output pointer) was passed to a function."},
        {GCC_GEN_BE_ERROR_INVALID_STATE, "Error: The GCC backend context or block is in an invalid state."},
        {GCC_GEN_BE_ERROR_INTERNAL, "Error: An internal compiler error occurred within the GCC backend."},
        {GCC_GEN_BE_ERROR_UNHANDLED_TYPE, "Error: A specific type could not be handled or lowered."},
        {GCC_GEN_BE_ERROR_UNHANDLED_EXPR, "Error: A specific expression could not be handled or lowered."},
        {GCC_GEN_BE_ERROR_UNHANDLED_STMT, "Error: A specific statement could not be handled or lowered."},
        {GCC_GEN_BE_ERROR_UNHANDLED_DECL, "Error: A specific declaration could not be handled or lowered."},
        {GCC_GEN_BE_ERROR_NULL_POINTER, "Error: A required pointer was null."},
        {GCC_GEN_BE_ERROR_TYPE_MISMATCH, "Error: A type mismatch was detected during code generation."},
        {GCC_GEN_BE_ERROR_EH_FAILURE, "Error: A failure occurred during exception handling setup or generation."},
        {GCC_GEN_BE_ERROR_EH_UNSUPPORTED, "Error: Exception handling is not supported for the target architecture."},
        {GCC_GEN_BE_ERROR_EH_LANDING_PAD_FAILED, "Error: Creating a landing pad for a try block failed."},
        {GCC_GEN_BE_ERROR_EH_TABLE_GENERATION_FAILED, "Error: Generating an EH table failed."},
        {GCC_GEN_BE_ERROR_UNRESOLVED_SYMBOL, "Error: A required symbol could not be resolved in the current context."},
        {GCC_GEN_BE_ERROR_BUILTIN_UNKNOWN, "Error: A requested compiler builtin function is unknown or unsupported."},
        {GCC_GEN_BE_ERROR_BUILTIN_SIGNATURE_MISMATCH, "Error: A builtin function signature did not match expectations."},
        {GCC_GEN_BE_ERROR_ABI_MISMATCH, "Error: An ABI, calling convention, or structural layout mismatch occurred."},
        {GCC_GEN_BE_ERROR_UNSUPPORTED_ATTRIBUTE, "Error: An unsupported or invalid attribute was encountered."},
        {GCC_GEN_BE_ERROR_VTABLE_GENERATION_FAILED, "Error: Generating a virtual table (vtable) failed."},
        {GCC_GEN_BE_ERROR_VTABLE_LAYOUT_FAILED, "Error: Computing virtual table (vtable) layout failed."},
        {GCC_GEN_BE_ERROR_RTTI_INVALID, "Error: An invalid RTTI descriptor was encountered."},
        {GCC_GEN_BE_ERROR_RTTI_GENERATION_FAILED, "Error: Generating RTTI information failed."},
        {GCC_GEN_BE_ERROR_RTTI_EMISSION_FAILED, "Error: Emitting RTTI information failed."},
        {GCC_GEN_BE_ERROR_ASM_CONSTRAINT_INVALID, "Error: An inline assembly constraint is invalid or unsupported."},
        {GCC_GEN_BE_ERROR_DWARF_EMISSION_FAILED, "Error: DWARF debug information emission failed."},
        {GCC_GEN_BE_ERROR_DWARF_DIE_CREATION_FAILED, "Error: DWARF Debugging Information Entry (DIE) creation failed."},
        {GCC_GEN_BE_ERROR_COMPILATION_FAILED, "Error: JIT compilation failed."},
        {GCC_GEN_BE_ERROR_VTT_GENERATION_FAILED, "Error: Generating Virtual Table Table (VTT) failed."},
        {GCC_GEN_BE_ERROR_THUNK_GENERATION_FAILED, "Error: Generating virtual method thunk failed."},
        {GCC_GEN_BE_ERROR_ABI_CLASSIFICATION_FAILED, "Error: Target ABI parameter classification failed."},
        {GCC_GEN_BE_ERROR_UNSUPPORTED_CALLING_CONV, "Error: The requested calling convention is unsupported."},
        {GCC_GEN_BE_ERROR_DWARF_TYPE_EMISSION_FAILED, "Error: DWARF type debug information emission failed."},
        {GCC_GEN_BE_ERROR_ASM_OPERAND_UNSUPPORTED, "Error: An inline assembly operand is unsupported."},
        {GCC_GEN_BE_ERROR_STRUCT_PACKING_FAILED, "Error: Struct packing failed."},
        {GCC_GEN_BE_ERROR_BITFIELD_LAYOUT_FAILED, "Error: Bitfield layout calculation failed."},
        {GCC_GEN_BE_ERROR_COROUTINE_FRAME_FAILED, "Error: Coroutine frame layout generation failed."},
        {GCC_GEN_BE_ERROR_COROUTINE_RAMP_FAILED, "Error: Coroutine ramp function generation failed."},
        {GCC_GEN_BE_ERROR_COROUTINE_DISPATCH_FAILED, "Error: Coroutine dispatch logic generation failed."},
        {GCC_GEN_BE_ERROR_CFI_UNWIND_FAILED, "Error: Zero-cost DWARF CFI exception handling table generation failed."},
        {GCC_GEN_BE_ERROR_SIMD_LOWERING_FAILED, "Error: SIMD vector intrinsic lowering failed."},
        {GCC_GEN_BE_ERROR_TARGET_OPTION_INVALID, "Error: Target option or attribute is invalid."},
        {GCC_GEN_BE_ERROR_VA_ARG_LOWERING_FAILED, "Error: Variadic argument (va_arg) state-machine lowering failed."},
        {GCC_GEN_BE_ERROR_MANGLING_BUFFER_OVERFLOW, "Error: Itanium C++ ABI mangled name exceeded buffer capacity."},
        {GCC_GEN_BE_ERROR_MANGLING_SUBSTITUTION_FAILED, "Error: Itanium C++ ABI substitution table operation failed."},
        {GCC_GEN_BE_ERROR_COOKIE_ALLOCATION_FAILED, "Error: Array new[]/delete[] cookie calculation or allocation failed."},
        {GCC_GEN_BE_ERROR_WIDE_INT_CONVERSION_FAILED, "Error: 128-bit or wide integer constant conversion failed."},
        {GCC_GEN_BE_ERROR_VLA_ALLOCATION_FAILED, "Error: Variable-length array (VLA) stack allocation failed."},
        {GCC_GEN_BE_ERROR_LABEL_RESOLUTION_FAILED, "Error: Computed goto or label address resolution failed."},
        {GCC_GEN_BE_ERROR_VECTOR_PERMUTE_FAILED, "Error: SIMD vector shuffle or convertvector permutation failed."},
        {GCC_GEN_BE_ERROR_FLOAT_PRECISION_FAILED, "Error: Extended floating-point precision conversion failed."},
        {GCC_GEN_BE_ERROR_MULTI_TU_PIPELINE_FAILED, "Error: Multi-TU compilation or linker pipeline orchestration failed."},
        {GCC_GEN_BE_ERROR_DSO_HANDLE_FAILED, "Error: __dso_handle symbol declaration or global destructor registration failed."},
        {GCC_GEN_BE_ERROR_COERCION_FAILED, "Error: Target ABI register coercion or eightbyte/HFA unpacking failed."},
        {GCC_GEN_BE_ERROR_WEAK_LINKAGE_FAILED, "Error: Applying weak or COMDAT linkage attributes failed."},
        {GCC_GEN_BE_ERROR_UNWIND_CLEANUP_FAILED, "Error: Emitting RAII scope unwind cleanup instructions failed."},
        {GCC_GEN_BE_ERROR_COROUTINE_PROMISE_FAILED, "Error: Coroutine promise object allocation or method resolution failed."},
        {GCC_GEN_BE_ERROR_COROUTINE_SUSPEND_FAILED, "Error: Coroutine co_await/co_yield suspension lowering failed."},
        {GCC_GEN_BE_ERROR_ATOMIC_ALIGNMENT_FAILED, "Error: Atomic builtin operand had invalid size or alignment."},
        {GCC_GEN_BE_ERROR_EH_CFG_EDGE_FAILED, "Error: Constructing an abnormal unwind CFG edge failed."},
        {GCC_GEN_BE_ERROR_COROUTINE_SPLIT_FAILED, "Error: Coroutine live-variable liveness or state-machine splitting failed."},
        {GCC_GEN_BE_ERROR_ABI_PROLOGUE_UNPACK_FAILED, "Error: ABI register coercion prologue or epilogue unpacking failed."},
        {GCC_GEN_BE_ERROR_COMDAT_GROUP_FAILED, "Error: COMDAT or linkonce section group attachment failed."},
        {GCC_GEN_BE_ERROR_DWARF_SCOPE_FAILED, "Error: DWARF lexical scope or member DIE attachment failed."},
        {GCC_GEN_BE_ERROR_DWARF_DIE_FAILED, "Error: DWARF DIE emission or attachment failed."},
        {GCC_GEN_BE_ERROR_LSDA_ENCODE_FAILED, "Error: DWARF or SEH LSDA exception table encoding failed."},
        {GCC_GEN_BE_ERROR_MODULE_IMPORT_FAILED, "Error: C++20 module import symbol or BMI/IFC lowering failed."},
        {GCC_GEN_BE_ERROR_MODULE_EXPORT_FAILED, "Error: C++20 module export or initializer emission failed."},
        {GCC_GEN_BE_ERROR_CORO_TRANSFER_FAILED, "Error: Coroutine symmetric transfer lowering failed."},
        {GCC_GEN_BE_ERROR_CORO_HALO_FAILED, "Error: Coroutine heap allocation elision optimization (HALO) failed."},
        {GCC_GEN_BE_ERROR_FIXED_POINT_OVERFLOW, "Error: ISO fixed-point conversion or arithmetic operation overflowed."},
        {GCC_GEN_BE_ERROR_DECIMAL_FLOAT_UNSUPPORTED, "Error: IEEE 754 decimal floating-point operation is unsupported."},
        {GCC_GEN_BE_ERROR_DPD_FLOAT_CONVERSION_FAILED, "Error: IEEE 754 DPD decimal floating-point conversion failed."},
        {GCC_GEN_BE_ERROR_VLA_STACK_RESTORE_FAILED, "Error: Emitting a VLA scope stack save or restore failed."},
        {GCC_GEN_BE_ERROR_DWARF_DIE_BINDING_FAILED, "Error: Binding a DWARF DIE to a symbol or scope failed."},
        {GCC_GEN_BE_ERROR_RELOCATION_EMISSION_FAILED, "Error: Emitting a label-difference or symbol relocation failed."},
        {GCC_GEN_BE_ERROR_TAIL_CALL_THUNK_FAILED, "Error: Emitting a must-tail virtual thunk call failed."},
        {GCC_GEN_BE_ERROR_TEST_ASSERTION_FAILED, "Error: Unit test assertion failed."},
        {(gcc_gen_be_error_t)9999, "Error: An unknown error occurred."}
    };

    size_t num_cases = sizeof(test_cases) / sizeof(test_cases[0]);
    for (size_t i = 0; i < num_cases; i++) {
        str = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_error_string(test_cases[i].code, &str));
        if (!str || strcmp(str, test_cases[i].expected) != 0) {
            return GCC_GEN_BE_ERROR_INTERNAL;
        }
    }

    err = gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_UNSUPPORTED, "test context");
    if (err != GCC_GEN_BE_ERROR_UNSUPPORTED) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }

    *out_passed = 1;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Main entry point for the error handling unit test.
 * @details Executes `run_error_tests` and returns 0 on success.
 * @return 0 on success, non-zero on failure.
 */
int main(void) {
    int passed = 0;
    gcc_gen_be_error_t err = run_error_tests(&passed);
    if (err != GCC_GEN_BE_SUCCESS || !passed) {
        return 1;
    }
    puts("PASS");
    return 0;
}
