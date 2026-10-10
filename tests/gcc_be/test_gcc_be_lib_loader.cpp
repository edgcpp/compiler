#include "basic_hdrs.h"
#include <stdio.h>
#include <string.h>

// Mock dlopen and dlsym
extern "C" {
void* mock_dlopen(const char* filename, int flags);
void* mock_dlsym(void* handle, const char* symbol);
}

#define dlopen mock_dlopen
#define dlsym mock_dlsym

// Include the source directly so we can override the macros and internal state
#include "gcc_gen_be_lib_loader.c"

extern "C" {
    gcc_gen_be_error_t gcc_gen_be_report_diagnostic(gcc_gen_be_error_t err, const char* msg) GCC_GEN_BE_NOEXCEPT { 
        return GCC_GEN_BE_SUCCESS; 
    }
}

static bool should_fail_dlopen = false;
static bool should_fail_dlsym = false;

void* mock_dlopen(const char* filename, int flags) {
    if (should_fail_dlopen) return NULL;
    return (void*)0xDEADBEEF; // Dummy handle
}

void* mock_dlsym(void* handle, const char* symbol) {
    if (should_fail_dlsym) return NULL;
    
    // Return dummy pointers for our optional symbols
    if (strcmp(symbol, "gcc_jit_block_end_with_extended_asm_goto") == 0) return (void*)1;
    if (strcmp(symbol, "gcc_jit_lvalue_set_tls_model") == 0) return (void*)2;
    if (strcmp(symbol, "gcc_jit_function_add_attribute") == 0) return (void*)3;
    if (strcmp(symbol, "gcc_jit_lvalue_add_string_attribute") == 0) return (void*)4;
    if (strcmp(symbol, "gcc_jit_lvalue_set_alignment") == 0) return (void*)5;
    if (strcmp(symbol, "gcc_jit_context_get_int_type") == 0) return (void*)6;
    if (strcmp(symbol, "gcc_jit_type_get_vector") == 0) return (void*)7;

    return NULL;
}

int main() {
    gcc_gen_be_error_t err;

    // Test 1: dlopen failure
    should_fail_dlopen = true;
    libgccjit_handle = NULL; // reset internal state
    err = load_libgccjit_posix();
    if (err != GCC_GEN_BE_ERROR_LIBGCCJIT_LOAD_FAILED) return 1;

    // Test 2: Successful load, missing optional symbols
    should_fail_dlopen = false;
    should_fail_dlsym = true;
    libgccjit_handle = NULL;
    err = load_libgccjit_posix();
    if (err != GCC_GEN_BE_SUCCESS) return 1;
    if (p_gcc_jit_lvalue_set_tls_model != NULL) return 1; // Optional symbols should be NULL

    // Test 3: Successful load, optional symbols present
    should_fail_dlopen = false;
    should_fail_dlsym = false;
    libgccjit_handle = NULL;
    err = load_libgccjit_posix();
    if (err != GCC_GEN_BE_SUCCESS) return 1;
    if (p_gcc_jit_block_end_with_extended_asm_goto != (void*)1) return 1;
    if (p_gcc_jit_lvalue_set_tls_model != (void*)2) return 1;
    if (p_gcc_jit_function_add_attribute != (void*)3) return 1;
    if (p_gcc_jit_lvalue_add_string_attribute != (void*)4) return 1;
    if (p_gcc_jit_lvalue_set_alignment != (void*)5) return 1;
    if (p_gcc_jit_context_get_int_type != (void*)6) return 1;
    if (p_gcc_jit_type_get_vector != (void*)7) return 1;

    // Test 4: Already loaded
    libgccjit_handle = (void*)0xDEADBEEF; // Simulate loaded
    should_fail_dlopen = true; // Should not be called
    err = load_libgccjit_posix();
    if (err != GCC_GEN_BE_SUCCESS) return 1;

    printf("PASS\n");
    return 0;
}