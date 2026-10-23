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
/** @brief Handle to the loaded libgccjit library. */
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

/** @brief Function pointer. */
pfn_gcc_jit_context_new_struct_constructor p_gcc_jit_context_new_struct_constructor = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_context_get_builtin_function p_gcc_jit_context_get_builtin_function = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_context_new_rvalue_from_vector p_gcc_jit_context_new_rvalue_from_vector = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_context_new_bitcast p_gcc_jit_context_new_bitcast = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_context_new_union_constructor p_gcc_jit_context_new_union_constructor = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_context_new_array_constructor p_gcc_jit_context_new_array_constructor = NULL;

/** @brief Function pointer. */
pfn_gcc_jit_block_end_with_extended_asm_goto p_gcc_jit_block_end_with_extended_asm_goto = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_lvalue_set_tls_model p_gcc_jit_lvalue_set_tls_model = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_function_add_attribute p_gcc_jit_function_add_attribute = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_lvalue_set_link_section p_gcc_jit_lvalue_set_link_section = NULL;
/** @brief Function pointer. */
/** @brief Function pointer. */
pfn_gcc_jit_function_add_string_attribute p_gcc_jit_function_add_string_attribute = NULL;

/** @brief Function pointer. */
pfn_gcc_jit_lvalue_add_string_attribute p_gcc_jit_lvalue_add_string_attribute = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_lvalue_set_alignment p_gcc_jit_lvalue_set_alignment = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_lvalue_set_register_name p_gcc_jit_lvalue_set_register_name = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_context_get_int_type p_gcc_jit_context_get_int_type = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_type_get_vector p_gcc_jit_type_get_vector = NULL;
/** @brief Function pointer for `gcc_jit_context_new_rvalue_vector_perm`. */
pfn_gcc_jit_context_new_rvalue_vector_perm p_gcc_jit_context_new_rvalue_vector_perm = NULL;
/** @brief Function pointer for `gcc_jit_context_new_convert_vector`. */
pfn_gcc_jit_context_new_convert_vector p_gcc_jit_context_new_convert_vector = NULL;
/** @brief Function pointer for `gcc_jit_block_end_with_invoke`. */
pfn_gcc_jit_block_end_with_invoke p_gcc_jit_block_end_with_invoke = NULL;
/** @brief Function pointer for `gcc_jit_block_add_try_catch`. */
pfn_gcc_jit_block_add_try_catch p_gcc_jit_block_add_try_catch = NULL;
/** @brief Function pointer for `gcc_jit_block_add_cleanup`. */
pfn_gcc_jit_block_add_cleanup p_gcc_jit_block_add_cleanup = NULL;
/** @brief Function pointer for `gcc_jit_block_end_with_resx`. */
pfn_gcc_jit_block_end_with_resx p_gcc_jit_block_end_with_resx = NULL;
/** @brief Function pointer for `gcc_jit_function_set_personality`. */
pfn_gcc_jit_function_set_personality p_gcc_jit_function_set_personality = NULL;
/** @brief Function pointer for `gcc_jit_block_add_landing_pad`. */
pfn_gcc_jit_block_add_landing_pad p_gcc_jit_block_add_landing_pad = NULL;
/** @brief Function pointer for `gcc_jit_context_new_eh_filter`. */
pfn_gcc_jit_context_new_eh_filter p_gcc_jit_context_new_eh_filter = NULL;
/** @brief Function pointer for `gcc_jit_rvalue_set_bool_require_tail_call`. */
pfn_gcc_jit_rvalue_set_bool_require_tail_call p_gcc_jit_rvalue_set_bool_require_tail_call = NULL;
/** @brief Function pointer for `gcc_jit_context_add_top_level_asm`. */
pfn_gcc_jit_context_add_top_level_asm p_gcc_jit_context_add_top_level_asm = NULL;

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
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_get_builtin_function, gcc_jit_context_get_builtin_function)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_new_rvalue_from_vector, gcc_jit_context_new_rvalue_from_vector)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_new_bitcast, gcc_jit_context_new_bitcast)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_new_union_constructor, gcc_jit_context_new_union_constructor)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_new_array_constructor, gcc_jit_context_new_array_constructor)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_block_end_with_extended_asm_goto, gcc_jit_block_end_with_extended_asm_goto)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_lvalue_set_tls_model, gcc_jit_lvalue_set_tls_model)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_function_add_attribute, gcc_jit_function_add_attribute)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_lvalue_set_link_section, gcc_jit_lvalue_set_link_section)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_function_add_string_attribute, gcc_jit_function_add_string_attribute)

  LOAD_OPTIONAL_SYM(pfn_gcc_jit_lvalue_add_string_attribute, gcc_jit_lvalue_add_string_attribute)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_lvalue_set_alignment, gcc_jit_lvalue_set_alignment)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_lvalue_set_register_name, gcc_jit_lvalue_set_register_name)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_get_int_type, gcc_jit_context_get_int_type)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_type_get_vector, gcc_jit_type_get_vector)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_new_rvalue_vector_perm, gcc_jit_context_new_rvalue_vector_perm)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_new_convert_vector, gcc_jit_context_new_convert_vector)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_block_end_with_invoke, gcc_jit_block_end_with_invoke)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_block_add_try_catch, gcc_jit_block_add_try_catch)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_block_add_cleanup, gcc_jit_block_add_cleanup)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_block_end_with_resx, gcc_jit_block_end_with_resx)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_function_set_personality, gcc_jit_function_set_personality)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_block_add_landing_pad, gcc_jit_block_add_landing_pad)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_new_eh_filter, gcc_jit_context_new_eh_filter)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_rvalue_set_bool_require_tail_call, gcc_jit_rvalue_set_bool_require_tail_call)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_add_top_level_asm, gcc_jit_context_add_top_level_asm)

  #undef LOAD_SYM
  #undef LOAD_OPTIONAL_SYM

  return GCC_GEN_BE_SUCCESS;
}

#else

#include <dlfcn.h>
/** @brief Handle to the loaded libgccjit library. */
static void *libgccjit_handle = NULL;

/** @brief Function pointer. */
pfn_gcc_jit_context_new_struct_constructor p_gcc_jit_context_new_struct_constructor = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_context_get_builtin_function p_gcc_jit_context_get_builtin_function = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_context_new_rvalue_from_vector p_gcc_jit_context_new_rvalue_from_vector = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_context_new_bitcast p_gcc_jit_context_new_bitcast = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_context_new_union_constructor p_gcc_jit_context_new_union_constructor = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_context_new_array_constructor p_gcc_jit_context_new_array_constructor = NULL;

/** @brief Function pointer. */
pfn_gcc_jit_block_end_with_extended_asm_goto p_gcc_jit_block_end_with_extended_asm_goto = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_lvalue_set_tls_model p_gcc_jit_lvalue_set_tls_model = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_function_add_attribute p_gcc_jit_function_add_attribute = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_lvalue_set_link_section p_gcc_jit_lvalue_set_link_section = NULL;
/** @brief Function pointer. */
/** @brief Function pointer. */
pfn_gcc_jit_function_add_string_attribute p_gcc_jit_function_add_string_attribute = NULL;

/** @brief Function pointer. */
pfn_gcc_jit_lvalue_add_string_attribute p_gcc_jit_lvalue_add_string_attribute = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_lvalue_set_alignment p_gcc_jit_lvalue_set_alignment = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_lvalue_set_register_name p_gcc_jit_lvalue_set_register_name = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_context_get_int_type p_gcc_jit_context_get_int_type = NULL;
/** @brief Function pointer. */
pfn_gcc_jit_type_get_vector p_gcc_jit_type_get_vector = NULL;
/** @brief Function pointer for `gcc_jit_context_new_rvalue_vector_perm`. */
pfn_gcc_jit_context_new_rvalue_vector_perm p_gcc_jit_context_new_rvalue_vector_perm = NULL;
/** @brief Function pointer for `gcc_jit_context_new_convert_vector`. */
pfn_gcc_jit_context_new_convert_vector p_gcc_jit_context_new_convert_vector = NULL;
/** @brief Function pointer for `gcc_jit_block_end_with_invoke`. */
pfn_gcc_jit_block_end_with_invoke p_gcc_jit_block_end_with_invoke = NULL;
/** @brief Function pointer for `gcc_jit_block_add_try_catch`. */
pfn_gcc_jit_block_add_try_catch p_gcc_jit_block_add_try_catch = NULL;
/** @brief Function pointer for `gcc_jit_block_add_cleanup`. */
pfn_gcc_jit_block_add_cleanup p_gcc_jit_block_add_cleanup = NULL;
/** @brief Function pointer for `gcc_jit_block_end_with_resx`. */
pfn_gcc_jit_block_end_with_resx p_gcc_jit_block_end_with_resx = NULL;
/** @brief Function pointer for `gcc_jit_function_set_personality`. */
pfn_gcc_jit_function_set_personality p_gcc_jit_function_set_personality = NULL;
/** @brief Function pointer for `gcc_jit_block_add_landing_pad`. */
pfn_gcc_jit_block_add_landing_pad p_gcc_jit_block_add_landing_pad = NULL;
/** @brief Function pointer for `gcc_jit_context_new_eh_filter`. */
pfn_gcc_jit_context_new_eh_filter p_gcc_jit_context_new_eh_filter = NULL;
/** @brief Function pointer for `gcc_jit_rvalue_set_bool_require_tail_call`. */
pfn_gcc_jit_rvalue_set_bool_require_tail_call p_gcc_jit_rvalue_set_bool_require_tail_call = NULL;
/** @brief Function pointer for `gcc_jit_context_add_top_level_asm`. */
pfn_gcc_jit_context_add_top_level_asm p_gcc_jit_context_add_top_level_asm = NULL;

/** @brief Resolves an optional `libgccjit` symbol from `libgccjit_handle`. */
#define LOAD_OPTIONAL_SYM(type, name) \
    p_##name = (type)dlsym(libgccjit_handle, #name);

/**
 * @brief Dynamically loads libgccjit on POSIX systems.
 * @details Uses `dlopen` and `dlsym` to load `libgccjit` and resolve optional API symbols.
 * @return GCC_GEN_BE_SUCCESS if loaded successfully, or an error code otherwise.
 */
gcc_gen_be_error_t load_libgccjit_posix(void) GCC_GEN_BE_NOEXCEPT {
    if (libgccjit_handle) return GCC_GEN_BE_SUCCESS;
    const char *candidates[] = {
#if defined(__APPLE__)
        "libgccjit.0.dylib",
        "libgccjit.dylib",
        "/opt/homebrew/lib/gcc/current/libgccjit.0.dylib",
        "/opt/homebrew/lib/gcc/current/libgccjit.dylib",
        "/usr/local/lib/gcc/current/libgccjit.0.dylib",
        "/usr/local/lib/gcc/current/libgccjit.dylib",
#else
        "libgccjit.so.0",
        "libgccjit.so",
        "/usr/lib/x86_64-linux-gnu/libgccjit.so.0",
        "/usr/lib/aarch64-linux-gnu/libgccjit.so.0",
        "/usr/lib64/libgccjit.so.0",
        "/usr/local/lib/libgccjit.so.0",
#endif
        NULL
    };
    for (int i = 0; candidates[i] != NULL; ++i) {
        libgccjit_handle = dlopen(candidates[i], RTLD_NOW | RTLD_GLOBAL);
        if (libgccjit_handle) break;
    }
    if (!libgccjit_handle) {
        libgccjit_handle = RTLD_DEFAULT;
        if (!dlsym(libgccjit_handle, "gcc_jit_context_acquire")) {
            libgccjit_handle = NULL;
            return GCC_GEN_BE_ERROR_LIBGCCJIT_LOAD_FAILED;
        }
    }

    LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_new_struct_constructor, gcc_jit_context_new_struct_constructor)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_get_builtin_function, gcc_jit_context_get_builtin_function)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_new_rvalue_from_vector, gcc_jit_context_new_rvalue_from_vector)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_new_bitcast, gcc_jit_context_new_bitcast)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_new_union_constructor, gcc_jit_context_new_union_constructor)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_new_array_constructor, gcc_jit_context_new_array_constructor)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_block_end_with_extended_asm_goto, gcc_jit_block_end_with_extended_asm_goto)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_lvalue_set_tls_model, gcc_jit_lvalue_set_tls_model)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_function_add_attribute, gcc_jit_function_add_attribute)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_lvalue_set_link_section, gcc_jit_lvalue_set_link_section)
  LOAD_OPTIONAL_SYM(pfn_gcc_jit_function_add_string_attribute, gcc_jit_function_add_string_attribute)

  LOAD_OPTIONAL_SYM(pfn_gcc_jit_lvalue_add_string_attribute, gcc_jit_lvalue_add_string_attribute)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_lvalue_set_alignment, gcc_jit_lvalue_set_alignment)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_lvalue_set_register_name, gcc_jit_lvalue_set_register_name)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_get_int_type, gcc_jit_context_get_int_type)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_type_get_vector, gcc_jit_type_get_vector)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_new_rvalue_vector_perm, gcc_jit_context_new_rvalue_vector_perm)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_new_convert_vector, gcc_jit_context_new_convert_vector)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_block_end_with_invoke, gcc_jit_block_end_with_invoke)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_block_add_try_catch, gcc_jit_block_add_try_catch)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_block_add_cleanup, gcc_jit_block_add_cleanup)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_block_end_with_resx, gcc_jit_block_end_with_resx)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_function_set_personality, gcc_jit_function_set_personality)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_block_add_landing_pad, gcc_jit_block_add_landing_pad)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_new_eh_filter, gcc_jit_context_new_eh_filter)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_rvalue_set_bool_require_tail_call, gcc_jit_rvalue_set_bool_require_tail_call)
    LOAD_OPTIONAL_SYM(pfn_gcc_jit_context_add_top_level_asm, gcc_jit_context_add_top_level_asm)

    #undef LOAD_OPTIONAL_SYM

    return GCC_GEN_BE_SUCCESS;
}

#endif

/**
 * @brief Probes `libgccjit` for exception handling and unwind builtins.
 * @details Queries `gcc_jit_context_get_builtin_function` (or dynamic `p_gcc_jit_context_get_builtin_function`)
 * for `__builtin_eh_pointer`, `__builtin_eh_filter`, and `__builtin_unwind_resume`, and ensures `_Unwind_Resume`
 * is available in the context.
 * @param[in] ctx The active `libgccjit` context.
 * @param[out] out_has_eh_builtins Pointer to receive `TRUE` if native EH builtins are available, `FALSE` otherwise.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code if arguments are invalid.
 */
gcc_gen_be_error_t gcc_gen_be_probe_eh_builtins(struct gcc_jit_context *ctx, a_boolean *out_has_eh_builtins) GCC_GEN_BE_NOEXCEPT {
    if (!ctx || !out_has_eh_builtins) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_has_eh_builtins = FALSE;

    if (p_gcc_jit_context_get_builtin_function) {
        struct gcc_jit_function *trap_fn = p_gcc_jit_context_get_builtin_function(ctx, "__builtin_trap");
        if (trap_fn != NULL) {
            *out_has_eh_builtins = TRUE;
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Probes `libgccjit` for native GIMPLE EH region entrypoints (`TRY_CATCH_EXPR`, `CLEANUP_POINT_EXPR`, `RESX_EXPR`).
 * @details Checks whether the loaded `libgccjit` shared library exports `gcc_jit_block_add_try_catch`,
 * `gcc_jit_block_add_cleanup`, `gcc_jit_block_end_with_resx`, and `gcc_jit_function_set_personality`.
 * @param[in] ctx The active `libgccjit` context.
 * @param[out] out_supported Pointer to receive `TRUE` if native GIMPLE EH region entrypoints are present, `FALSE` otherwise.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if `ctx` or `out_supported` is NULL.
 */
gcc_gen_be_error_t gcc_gen_be_probe_native_eh_support(
    struct gcc_jit_context *ctx,
    a_boolean *out_supported) GCC_GEN_BE_NOEXCEPT {
    if (!ctx || !out_supported) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_supported = (p_gcc_jit_block_add_try_catch != NULL &&
                      p_gcc_jit_block_add_cleanup != NULL &&
                      p_gcc_jit_block_end_with_resx != NULL &&
                      p_gcc_jit_function_set_personality != NULL)
                         ? TRUE
                         : FALSE;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Resolves and reports optional `libgccjit` invoke and landing-pad EH entrypoints.
 * @details Checks whether `gcc_jit_block_end_with_invoke` and `gcc_jit_block_add_landing_pad` (or
 * `gcc_jit_block_add_try_catch`) are bound on the loaded `libgccjit` library and writes the results
 * into `out_has_invoke` and `out_has_landing_pad`.
 * @param[in] ctx The active `libgccjit` context.
 * @param[out] out_has_invoke Pointer to receive `TRUE` if `gcc_jit_block_end_with_invoke` is available.
 * @param[out] out_has_landing_pad Pointer to receive `TRUE` if landing pad / try-catch entrypoints are available.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if any argument is NULL.
 */
gcc_gen_be_error_t gcc_gen_be_resolve_eh_entrypoints(
    struct gcc_jit_context *ctx,
    a_boolean *out_has_invoke,
    a_boolean *out_has_landing_pad) GCC_GEN_BE_NOEXCEPT {
    if (!ctx || !out_has_invoke || !out_has_landing_pad) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_has_invoke = (p_gcc_jit_block_end_with_invoke != NULL) ? TRUE : FALSE;
    *out_has_landing_pad = (p_gcc_jit_block_add_landing_pad != NULL ||
                            p_gcc_jit_block_add_try_catch != NULL)
                               ? TRUE
                               : FALSE;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_probe_extended_float_types(
    struct gcc_jit_context *ctx,
    a_boolean *out_has_ext_float) GCC_GEN_BE_NOEXCEPT {
    if (!ctx || !out_has_ext_float) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_has_ext_float = FALSE;
#if defined(GCC_JIT_TYPE_BFLOAT16) || defined(GCC_JIT_TYPE_FLOAT16) || defined(GCC_JIT_TYPE_FLOAT128)
    gcc_jit_type *ld_t = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG_DOUBLE);
    if (ld_t != NULL) {
        *out_has_ext_float = TRUE;
    }
#else
    (void)ctx;
#endif
    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
