/**
 * @file gcc_gen_be_lib_loader.c
 * @brief Implementation of libgccjit dynamic loading.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "fe_common.h"
#include "gcc_gen_be_lib_loader.h"
#include "error.h"

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE




#if defined(_WIN32)
#include <windows.h>
static HMODULE libgccjit_handle = NULL;
void *p_gcc_jit_context_acquire = NULL;
void *p_gcc_jit_context_release = NULL;
void *p_gcc_jit_context_set_int_option = NULL;
void *p_gcc_jit_context_set_bool_option = NULL;
void *p_gcc_jit_context_set_str_option = NULL;
void *p_gcc_jit_context_add_command_line_option = NULL;
void *p_gcc_jit_context_get_type = NULL;
void *p_gcc_jit_type_get_pointer = NULL;
void *p_gcc_jit_type_get_const = NULL;
void *p_gcc_jit_type_get_volatile = NULL;
void *p_gcc_jit_context_new_array_type = NULL;
void *p_gcc_jit_context_new_field = NULL;
void *p_gcc_jit_context_new_bitfield = NULL;
void *p_gcc_jit_context_new_struct_type = NULL;
void *p_gcc_jit_context_new_opaque_struct = NULL;
void *p_gcc_jit_struct_set_fields = NULL;
void *p_gcc_jit_context_new_union_type = NULL;
void *p_gcc_jit_context_new_function_ptr_type = NULL;
void *p_gcc_jit_context_new_param = NULL;
void *p_gcc_jit_context_new_function = NULL;
void *p_gcc_jit_context_new_global = NULL;
void *p_gcc_jit_global_set_initializer = NULL;
void *p_gcc_jit_function_new_block = NULL;
void *p_gcc_jit_function_new_local = NULL;
void *p_gcc_jit_block_add_eval = NULL;
void *p_gcc_jit_block_add_assignment = NULL;
void *p_gcc_jit_block_add_assignment_op = NULL;
void *p_gcc_jit_block_end_with_conditional = NULL;
void *p_gcc_jit_block_end_with_jump = NULL;
void *p_gcc_jit_block_end_with_return = NULL;
void *p_gcc_jit_block_end_with_void_return = NULL;
void *p_gcc_jit_block_end_with_switch = NULL;
void *p_gcc_jit_context_new_rvalue_from_int = NULL;
void *p_gcc_jit_context_new_rvalue_from_long = NULL;
void *p_gcc_jit_context_new_rvalue_from_double = NULL;
void *p_gcc_jit_context_new_rvalue_from_ptr = NULL;
void *p_gcc_jit_context_new_string_literal = NULL;
void *p_gcc_jit_context_null = NULL;
void *p_gcc_jit_context_zero = NULL;
void *p_gcc_jit_context_one = NULL;
void *p_gcc_jit_context_new_unary_op = NULL;
void *p_gcc_jit_context_new_binary_op = NULL;
void *p_gcc_jit_context_new_comparison = NULL;
void *p_gcc_jit_context_new_cast = NULL;
void *p_gcc_jit_context_new_array_access = NULL;
void *p_gcc_jit_lvalue_access_field = NULL;
void *p_gcc_jit_rvalue_access_field = NULL;
void *p_gcc_jit_rvalue_dereference_field = NULL;
void *p_gcc_jit_rvalue_dereference = NULL;
void *p_gcc_jit_lvalue_get_address = NULL;
void *p_gcc_jit_lvalue_as_rvalue = NULL;
void *p_gcc_jit_param_as_lvalue = NULL;
void *p_gcc_jit_param_as_rvalue = NULL;
void *p_gcc_jit_context_new_call = NULL;
void *p_gcc_jit_context_new_call_through_ptr = NULL;
void *p_gcc_jit_block_add_extended_asm = NULL;
void *p_gcc_jit_context_new_location = NULL;
void *p_gcc_jit_context_compile_to_file = NULL;
void *p_gcc_jit_context_get_first_error = NULL;

pfn_gcc_jit_context_new_struct_constructor p_gcc_jit_context_new_struct_constructor = NULL;
pfn_gcc_jit_context_new_array_constructor p_gcc_jit_context_new_array_constructor = NULL;

pfn_gcc_jit_block_end_with_extended_asm_goto p_gcc_jit_block_end_with_extended_asm_goto = NULL;
pfn_gcc_jit_lvalue_set_tls_model p_gcc_jit_lvalue_set_tls_model = NULL;
pfn_gcc_jit_function_add_attribute p_gcc_jit_function_add_attribute = NULL;
pfn_gcc_jit_lvalue_add_string_attribute p_gcc_jit_lvalue_add_string_attribute = NULL;
pfn_gcc_jit_lvalue_set_alignment p_gcc_jit_lvalue_set_alignment = NULL;
pfn_gcc_jit_lvalue_set_register_name p_gcc_jit_lvalue_set_register_name = NULL;
pfn_gcc_jit_context_get_int_type p_gcc_jit_context_get_int_type = NULL;
pfn_gcc_jit_type_get_vector p_gcc_jit_type_get_vector = NULL;

gcc_gen_be_error_t load_libgccjit_windows(void) GCC_GEN_BE_NOEXCEPT {
  if (libgccjit_handle) return GCC_GEN_BE_SUCCESS;
  const char* candidates[] = {
      "libgccjit.dll",
      "C:\\msys64\\mingw64\\bin\\libgccjit-0.dll",
      "libgccjit-0.dll"
  };
  for (int i = 0; i < sizeof(candidates)/sizeof(candidates[0]); i++) {
      libgccjit_handle = LoadLibraryA(candidates[i]);
      if (libgccjit_handle) break;
  }
  if (!libgccjit_handle) {
      return GCC_GEN_BE_ERROR_LIBGCCJIT_LOAD_FAILED;
  }
  
  #define LOAD_SYM(name) \
    p_##name = (void *)GetProcAddress(libgccjit_handle, #name); \
    if (!p_##name) return GCC_GEN_BE_ERROR_LIBGCCJIT_SYMBOL_MISSING;

  #define LOAD_OPTIONAL_SYM(type, name) \
    p_##name = (type)GetProcAddress(libgccjit_handle, #name);

  LOAD_SYM(gcc_jit_context_acquire)
  LOAD_SYM(gcc_jit_context_release)
  LOAD_SYM(gcc_jit_context_set_int_option)
  LOAD_SYM(gcc_jit_context_set_bool_option)
  LOAD_SYM(gcc_jit_context_set_str_option)
  LOAD_SYM(gcc_jit_context_add_command_line_option)
  LOAD_SYM(gcc_jit_context_get_type)
  LOAD_SYM(gcc_jit_type_get_pointer)
  LOAD_SYM(gcc_jit_type_get_const)
  LOAD_SYM(gcc_jit_type_get_volatile)
  LOAD_SYM(gcc_jit_context_new_array_type)
  LOAD_SYM(gcc_jit_context_new_field)
  LOAD_SYM(gcc_jit_context_new_bitfield)
  LOAD_SYM(gcc_jit_context_new_struct_type)
  LOAD_SYM(gcc_jit_context_new_opaque_struct)
  LOAD_SYM(gcc_jit_struct_set_fields)
  LOAD_SYM(gcc_jit_context_new_union_type)
  LOAD_SYM(gcc_jit_context_new_function_ptr_type)
  LOAD_SYM(gcc_jit_context_new_param)
  LOAD_SYM(gcc_jit_context_new_function)
  LOAD_SYM(gcc_jit_context_new_global)
  LOAD_SYM(gcc_jit_global_set_initializer)
  LOAD_SYM(gcc_jit_function_new_block)
  LOAD_SYM(gcc_jit_function_new_local)
  LOAD_SYM(gcc_jit_block_add_eval)
  LOAD_SYM(gcc_jit_block_add_assignment)
  LOAD_SYM(gcc_jit_block_add_assignment_op)
  LOAD_SYM(gcc_jit_block_end_with_conditional)
  LOAD_SYM(gcc_jit_block_end_with_jump)
  LOAD_SYM(gcc_jit_block_end_with_return)
  LOAD_SYM(gcc_jit_block_end_with_void_return)
  LOAD_SYM(gcc_jit_block_end_with_switch)
  LOAD_SYM(gcc_jit_context_new_rvalue_from_int)
  LOAD_SYM(gcc_jit_context_new_rvalue_from_long)
  LOAD_SYM(gcc_jit_context_new_rvalue_from_double)
  LOAD_SYM(gcc_jit_context_new_rvalue_from_ptr)
  LOAD_SYM(gcc_jit_context_new_string_literal)
  LOAD_SYM(gcc_jit_context_null)
  LOAD_SYM(gcc_jit_context_zero)
  LOAD_SYM(gcc_jit_context_one)
  LOAD_SYM(gcc_jit_context_new_unary_op)
  LOAD_SYM(gcc_jit_context_new_binary_op)
  LOAD_SYM(gcc_jit_context_new_comparison)
  LOAD_SYM(gcc_jit_context_new_cast)
  LOAD_SYM(gcc_jit_context_new_array_access)
  LOAD_SYM(gcc_jit_lvalue_access_field)
  LOAD_SYM(gcc_jit_rvalue_access_field)
  LOAD_SYM(gcc_jit_rvalue_dereference_field)
  LOAD_SYM(gcc_jit_rvalue_dereference)
  LOAD_SYM(gcc_jit_lvalue_get_address)
  LOAD_SYM(gcc_jit_lvalue_as_rvalue)
  LOAD_SYM(gcc_jit_param_as_lvalue)
  LOAD_SYM(gcc_jit_param_as_rvalue)
  LOAD_SYM(gcc_jit_context_new_call)
  LOAD_SYM(gcc_jit_context_new_call_through_ptr)
  LOAD_SYM(gcc_jit_block_add_extended_asm)
  LOAD_SYM(gcc_jit_context_new_location)
  LOAD_SYM(gcc_jit_context_compile_to_file)
  LOAD_SYM(gcc_jit_context_get_first_error)
  
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_new_struct_constructor, gcc_jit_context_new_struct_constructor)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_new_array_constructor, gcc_jit_context_new_array_constructor)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_block_end_with_extended_asm_goto, gcc_jit_block_end_with_extended_asm_goto)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_lvalue_set_tls_model, gcc_jit_lvalue_set_tls_model)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_function_add_attribute, gcc_jit_function_add_attribute)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_lvalue_add_string_attribute, gcc_jit_lvalue_add_string_attribute)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_lvalue_set_alignment, gcc_jit_lvalue_set_alignment)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_lvalue_set_register_name, gcc_jit_lvalue_set_register_name)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_get_int_type, gcc_jit_context_get_int_type)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_type_get_vector, gcc_jit_type_get_vector)

  #undef LOAD_SYM
  #undef LOAD_OPTIONAL_SYM

  return GCC_GEN_BE_SUCCESS;
}

#else

#include <dlfcn.h>
static void *libgccjit_handle = NULL;

pfn_gcc_jit_context_new_struct_constructor p_gcc_jit_context_new_struct_constructor = NULL;
pfn_gcc_jit_context_new_array_constructor p_gcc_jit_context_new_array_constructor = NULL;

pfn_gcc_jit_block_end_with_extended_asm_goto p_gcc_jit_block_end_with_extended_asm_goto = NULL;
pfn_gcc_jit_lvalue_set_tls_model p_gcc_jit_lvalue_set_tls_model = NULL;
pfn_gcc_jit_function_add_attribute p_gcc_jit_function_add_attribute = NULL;
pfn_gcc_jit_lvalue_add_string_attribute p_gcc_jit_lvalue_add_string_attribute = NULL;
pfn_gcc_jit_lvalue_set_alignment p_gcc_jit_lvalue_set_alignment = NULL;
pfn_gcc_jit_lvalue_set_register_name p_gcc_jit_lvalue_set_register_name = NULL;
pfn_gcc_jit_context_get_int_type p_gcc_jit_context_get_int_type = NULL;
pfn_gcc_jit_type_get_vector p_gcc_jit_type_get_vector = NULL;

gcc_gen_be_error_t load_libgccjit_posix(void) GCC_GEN_BE_NOEXCEPT {
    if (libgccjit_handle) return GCC_GEN_BE_SUCCESS;
#if defined(__APPLE__)
    const char *lib_name = "libgccjit.dylib";
#else
    const char *lib_name = "libgccjit.so";
#endif
    libgccjit_handle = dlopen(lib_name, RTLD_NOW | RTLD_GLOBAL);
    if (!libgccjit_handle) {
        /* Optionally fallback but typically linked at compile-time */
        /* If dlopen fails on POSIX, we often still return SUCCESS if it's linked */
        /* Plan states: Ensure all dynamic loading failures return explicit gcc_gen_be_error_t codes without unhandled paths. */
        return GCC_GEN_BE_ERROR_LIBGCCJIT_LOAD_FAILED;
    }

    #define LOAD_OPTIONAL_SYM(type, name) \
      p_##name = (type)dlsym(libgccjit_handle, #name);

    LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_new_struct_constructor, gcc_jit_context_new_struct_constructor)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_new_array_constructor, gcc_jit_context_new_array_constructor)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_block_end_with_extended_asm_goto, gcc_jit_block_end_with_extended_asm_goto)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_lvalue_set_tls_model, gcc_jit_lvalue_set_tls_model)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_function_add_attribute, gcc_jit_function_add_attribute)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_lvalue_add_string_attribute, gcc_jit_lvalue_add_string_attribute)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_lvalue_set_alignment, gcc_jit_lvalue_set_alignment)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_lvalue_set_register_name, gcc_jit_lvalue_set_register_name)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_get_int_type, gcc_jit_context_get_int_type)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_type_get_vector, gcc_jit_type_get_vector)

    #undef LOAD_OPTIONAL_SYM

    return GCC_GEN_BE_SUCCESS;
}

#endif



END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
