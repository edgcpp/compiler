/**
 * @file gcc_gen_be_lib_loader.h
 * @brief Dynamic loading for libgccjit.
 *
 * This file handles dynamically loading libgccjit on Windows, and provides
 * no-op or basic dlopen implementations for POSIX systems where it is usually
 * linked dynamically at build time.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_LIB_LOADER_H
#define GCC_GEN_BE_LIB_LOADER_H

#include "gcc_gen_be_error.h"
#include "fe_common.h"

struct gcc_jit_context;
struct gcc_jit_type;
struct gcc_jit_struct;
struct gcc_jit_field;
struct gcc_jit_lvalue;
struct gcc_jit_rvalue;
struct gcc_jit_param;
struct gcc_jit_function;
struct gcc_jit_block;
struct gcc_jit_location;
struct gcc_jit_extended_asm;

BEGIN_EDG_NAMESPACE

#if defined(_WIN32)
extern void *p_gcc_jit_context_acquire;
#define gcc_jit_context_acquire (...) ((__typeof__(gcc_jit_context_acquire) *)p_gcc_jit_context_acquire)(__VA_ARGS__)
extern void *p_gcc_jit_context_release;
#define gcc_jit_context_release (...) ((__typeof__(gcc_jit_context_release) *)p_gcc_jit_context_release)(__VA_ARGS__)
extern void *p_gcc_jit_context_set_int_option;
#define gcc_jit_context_set_int_option (...) ((__typeof__(gcc_jit_context_set_int_option) *)p_gcc_jit_context_set_int_option)(__VA_ARGS__)
extern void *p_gcc_jit_context_set_bool_option;
#define gcc_jit_context_set_bool_option (...) ((__typeof__(gcc_jit_context_set_bool_option) *)p_gcc_jit_context_set_bool_option)(__VA_ARGS__)
extern void *p_gcc_jit_context_set_str_option;
#define gcc_jit_context_set_str_option (...) ((__typeof__(gcc_jit_context_set_str_option) *)p_gcc_jit_context_set_str_option)(__VA_ARGS__)
extern void *p_gcc_jit_context_add_command_line_option;
#define gcc_jit_context_add_command_line_option (...) ((__typeof__(gcc_jit_context_add_command_line_option) *)p_gcc_jit_context_add_command_line_option)(__VA_ARGS__)
extern void *p_gcc_jit_context_get_type;
#define gcc_jit_context_get_type (...) ((__typeof__(gcc_jit_context_get_type) *)p_gcc_jit_context_get_type)(__VA_ARGS__)
extern void *p_gcc_jit_type_get_pointer;
#define gcc_jit_type_get_pointer (...) ((__typeof__(gcc_jit_type_get_pointer) *)p_gcc_jit_type_get_pointer)(__VA_ARGS__)
extern void *p_gcc_jit_type_get_const;
#define gcc_jit_type_get_const (...) ((__typeof__(gcc_jit_type_get_const) *)p_gcc_jit_type_get_const)(__VA_ARGS__)
extern void *p_gcc_jit_type_get_volatile;
#define gcc_jit_type_get_volatile (...) ((__typeof__(gcc_jit_type_get_volatile) *)p_gcc_jit_type_get_volatile)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_array_type;
#define gcc_jit_context_new_array_type (...) ((__typeof__(gcc_jit_context_new_array_type) *)p_gcc_jit_context_new_array_type)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_field;
#define gcc_jit_context_new_field (...) ((__typeof__(gcc_jit_context_new_field) *)p_gcc_jit_context_new_field)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_bitfield;
#define gcc_jit_context_new_bitfield (...) ((__typeof__(gcc_jit_context_new_bitfield) *)p_gcc_jit_context_new_bitfield)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_struct_type;
#define gcc_jit_context_new_struct_type (...) ((__typeof__(gcc_jit_context_new_struct_type) *)p_gcc_jit_context_new_struct_type)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_opaque_struct;
#define gcc_jit_context_new_opaque_struct (...) ((__typeof__(gcc_jit_context_new_opaque_struct) *)p_gcc_jit_context_new_opaque_struct)(__VA_ARGS__)
extern void *p_gcc_jit_struct_set_fields;
#define gcc_jit_struct_set_fields (...) ((__typeof__(gcc_jit_struct_set_fields) *)p_gcc_jit_struct_set_fields)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_union_type;
#define gcc_jit_context_new_union_type (...) ((__typeof__(gcc_jit_context_new_union_type) *)p_gcc_jit_context_new_union_type)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_function_ptr_type;
#define gcc_jit_context_new_function_ptr_type (...) ((__typeof__(gcc_jit_context_new_function_ptr_type) *)p_gcc_jit_context_new_function_ptr_type)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_param;
#define gcc_jit_context_new_param (...) ((__typeof__(gcc_jit_context_new_param) *)p_gcc_jit_context_new_param)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_function;
#define gcc_jit_context_new_function (...) ((__typeof__(gcc_jit_context_new_function) *)p_gcc_jit_context_new_function)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_global;
#define gcc_jit_context_new_global (...) ((__typeof__(gcc_jit_context_new_global) *)p_gcc_jit_context_new_global)(__VA_ARGS__)
extern void *p_gcc_jit_global_set_initializer;
#define gcc_jit_global_set_initializer (...) ((__typeof__(gcc_jit_global_set_initializer) *)p_gcc_jit_global_set_initializer)(__VA_ARGS__)
extern void *p_gcc_jit_function_new_block;
#define gcc_jit_function_new_block (...) ((__typeof__(gcc_jit_function_new_block) *)p_gcc_jit_function_new_block)(__VA_ARGS__)
extern void *p_gcc_jit_function_new_local;
#define gcc_jit_function_new_local (...) ((__typeof__(gcc_jit_function_new_local) *)p_gcc_jit_function_new_local)(__VA_ARGS__)
extern void *p_gcc_jit_block_add_eval;
#define gcc_jit_block_add_eval (...) ((__typeof__(gcc_jit_block_add_eval) *)p_gcc_jit_block_add_eval)(__VA_ARGS__)
extern void *p_gcc_jit_block_add_assignment;
#define gcc_jit_block_add_assignment (...) ((__typeof__(gcc_jit_block_add_assignment) *)p_gcc_jit_block_add_assignment)(__VA_ARGS__)
extern void *p_gcc_jit_block_add_assignment_op;
#define gcc_jit_block_add_assignment_op (...) ((__typeof__(gcc_jit_block_add_assignment_op) *)p_gcc_jit_block_add_assignment_op)(__VA_ARGS__)
extern void *p_gcc_jit_block_end_with_conditional;
#define gcc_jit_block_end_with_conditional (...) ((__typeof__(gcc_jit_block_end_with_conditional) *)p_gcc_jit_block_end_with_conditional)(__VA_ARGS__)
extern void *p_gcc_jit_block_end_with_jump;
#define gcc_jit_block_end_with_jump (...) ((__typeof__(gcc_jit_block_end_with_jump) *)p_gcc_jit_block_end_with_jump)(__VA_ARGS__)
extern void *p_gcc_jit_block_end_with_return;
#define gcc_jit_block_end_with_return (...) ((__typeof__(gcc_jit_block_end_with_return) *)p_gcc_jit_block_end_with_return)(__VA_ARGS__)
extern void *p_gcc_jit_block_end_with_void_return;
#define gcc_jit_block_end_with_void_return (...) ((__typeof__(gcc_jit_block_end_with_void_return) *)p_gcc_jit_block_end_with_void_return)(__VA_ARGS__)
extern void *p_gcc_jit_block_end_with_switch;
#define gcc_jit_block_end_with_switch (...) ((__typeof__(gcc_jit_block_end_with_switch) *)p_gcc_jit_block_end_with_switch)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_rvalue_from_int;
#define gcc_jit_context_new_rvalue_from_int (...) ((__typeof__(gcc_jit_context_new_rvalue_from_int) *)p_gcc_jit_context_new_rvalue_from_int)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_rvalue_from_long;
#define gcc_jit_context_new_rvalue_from_long (...) ((__typeof__(gcc_jit_context_new_rvalue_from_long) *)p_gcc_jit_context_new_rvalue_from_long)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_rvalue_from_double;
#define gcc_jit_context_new_rvalue_from_double (...) ((__typeof__(gcc_jit_context_new_rvalue_from_double) *)p_gcc_jit_context_new_rvalue_from_double)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_rvalue_from_ptr;
#define gcc_jit_context_new_rvalue_from_ptr (...) ((__typeof__(gcc_jit_context_new_rvalue_from_ptr) *)p_gcc_jit_context_new_rvalue_from_ptr)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_string_literal;
#define gcc_jit_context_new_string_literal (...) ((__typeof__(gcc_jit_context_new_string_literal) *)p_gcc_jit_context_new_string_literal)(__VA_ARGS__)
extern void *p_gcc_jit_context_null;
#define gcc_jit_context_null (...) ((__typeof__(gcc_jit_context_null) *)p_gcc_jit_context_null)(__VA_ARGS__)
extern void *p_gcc_jit_context_zero;
#define gcc_jit_context_zero (...) ((__typeof__(gcc_jit_context_zero) *)p_gcc_jit_context_zero)(__VA_ARGS__)
extern void *p_gcc_jit_context_one;
#define gcc_jit_context_one (...) ((__typeof__(gcc_jit_context_one) *)p_gcc_jit_context_one)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_unary_op;
#define gcc_jit_context_new_unary_op (...) ((__typeof__(gcc_jit_context_new_unary_op) *)p_gcc_jit_context_new_unary_op)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_binary_op;
#define gcc_jit_context_new_binary_op (...) ((__typeof__(gcc_jit_context_new_binary_op) *)p_gcc_jit_context_new_binary_op)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_comparison;
#define gcc_jit_context_new_comparison (...) ((__typeof__(gcc_jit_context_new_comparison) *)p_gcc_jit_context_new_comparison)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_cast;
#define gcc_jit_context_new_cast (...) ((__typeof__(gcc_jit_context_new_cast) *)p_gcc_jit_context_new_cast)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_array_access;
#define gcc_jit_context_new_array_access (...) ((__typeof__(gcc_jit_context_new_array_access) *)p_gcc_jit_context_new_array_access)(__VA_ARGS__)
extern void *p_gcc_jit_lvalue_access_field;
#define gcc_jit_lvalue_access_field (...) ((__typeof__(gcc_jit_lvalue_access_field) *)p_gcc_jit_lvalue_access_field)(__VA_ARGS__)
extern void *p_gcc_jit_rvalue_access_field;
#define gcc_jit_rvalue_access_field (...) ((__typeof__(gcc_jit_rvalue_access_field) *)p_gcc_jit_rvalue_access_field)(__VA_ARGS__)
extern void *p_gcc_jit_rvalue_dereference_field;
#define gcc_jit_rvalue_dereference_field (...) ((__typeof__(gcc_jit_rvalue_dereference_field) *)p_gcc_jit_rvalue_dereference_field)(__VA_ARGS__)
extern void *p_gcc_jit_rvalue_dereference;
#define gcc_jit_rvalue_dereference (...) ((__typeof__(gcc_jit_rvalue_dereference) *)p_gcc_jit_rvalue_dereference)(__VA_ARGS__)
extern void *p_gcc_jit_lvalue_get_address;
#define gcc_jit_lvalue_get_address (...) ((__typeof__(gcc_jit_lvalue_get_address) *)p_gcc_jit_lvalue_get_address)(__VA_ARGS__)
extern void *p_gcc_jit_lvalue_as_rvalue;
#define gcc_jit_lvalue_as_rvalue (...) ((__typeof__(gcc_jit_lvalue_as_rvalue) *)p_gcc_jit_lvalue_as_rvalue)(__VA_ARGS__)
extern void *p_gcc_jit_param_as_lvalue;
#define gcc_jit_param_as_lvalue (...) ((__typeof__(gcc_jit_param_as_lvalue) *)p_gcc_jit_param_as_lvalue)(__VA_ARGS__)
extern void *p_gcc_jit_param_as_rvalue;
#define gcc_jit_param_as_rvalue (...) ((__typeof__(gcc_jit_param_as_rvalue) *)p_gcc_jit_param_as_rvalue)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_call;
#define gcc_jit_context_new_call (...) ((__typeof__(gcc_jit_context_new_call) *)p_gcc_jit_context_new_call)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_call_through_ptr;
#define gcc_jit_context_new_call_through_ptr (...) ((__typeof__(gcc_jit_context_new_call_through_ptr) *)p_gcc_jit_context_new_call_through_ptr)(__VA_ARGS__)
extern void *p_gcc_jit_block_add_extended_asm;
#define gcc_jit_block_add_extended_asm (...) ((__typeof__(gcc_jit_block_add_extended_asm) *)p_gcc_jit_block_add_extended_asm)(__VA_ARGS__)
extern void *p_gcc_jit_context_new_location;
#define gcc_jit_context_new_location (...) ((__typeof__(gcc_jit_context_new_location) *)p_gcc_jit_context_new_location)(__VA_ARGS__)
extern void *p_gcc_jit_context_compile_to_file;
#define gcc_jit_context_compile_to_file (...) ((__typeof__(gcc_jit_context_compile_to_file) *)p_gcc_jit_context_compile_to_file)(__VA_ARGS__)
extern void *p_gcc_jit_context_get_first_error;
#define gcc_jit_context_get_first_error (...) ((__typeof__(gcc_jit_context_get_first_error) *)p_gcc_jit_context_get_first_error)(__VA_ARGS__)
#endif

/* Forward declarations for optional modern libgccjit API features in case they
   are missing from the system libgccjit.h */
#ifndef LIBGCCJIT_HAVE_gcc_jit_lvalue_set_tls_model
enum gcc_jit_tls_model {
  GCC_JIT_TLS_MODEL_NONE,
  GCC_JIT_TLS_MODEL_GLOBAL_DYNAMIC,
  GCC_JIT_TLS_MODEL_LOCAL_DYNAMIC,
  GCC_JIT_TLS_MODEL_INITIAL_EXEC,
  GCC_JIT_TLS_MODEL_LOCAL_EXEC
};
#endif

#ifndef LIBGCCJIT_HAVE_ATTRIBUTES
enum gcc_jit_fn_attribute {
  GCC_JIT_FN_ATTRIBUTE_ALIAS,
  GCC_JIT_FN_ATTRIBUTE_ALWAYS_INLINE,
  GCC_JIT_FN_ATTRIBUTE_INLINE,
  GCC_JIT_FN_ATTRIBUTE_NOINLINE,
  GCC_JIT_FN_ATTRIBUTE_TARGET,
  GCC_JIT_FN_ATTRIBUTE_USED,
  GCC_JIT_FN_ATTRIBUTE_VISIBILITY,
  GCC_JIT_FN_ATTRIBUTE_COLD,
  GCC_JIT_FN_ATTRIBUTE_RETURNS_TWICE,
  GCC_JIT_FN_ATTRIBUTE_PURE,
  GCC_JIT_FN_ATTRIBUTE_CONST,
  GCC_JIT_FN_ATTRIBUTE_WEAK,
  GCC_JIT_FN_ATTRIBUTE_NONNULL,
  GCC_JIT_FN_ATTRIBUTE_FALLTHROUGH,
  GCC_JIT_FN_ATTRIBUTE_MALLOC,
  GCC_JIT_FN_ATTRIBUTE_SECTION,
  GCC_JIT_FN_ATTRIBUTE_MAX
};
enum gcc_jit_variable_attribute {
  GCC_JIT_VARIABLE_ATTRIBUTE_ALIAS,
  GCC_JIT_VARIABLE_ATTRIBUTE_WEAK,
  GCC_JIT_VARIABLE_ATTRIBUTE_VISIBILITY,
  GCC_JIT_VARIABLE_ATTRIBUTE_SECTION,
  GCC_JIT_VARIABLE_ATTRIBUTE_MAX
};
#endif

/* Function pointer types for modern APIs */
typedef void (*pfn_gcc_jit_block_end_with_extended_asm_goto)(
    struct gcc_jit_block *block,
    struct gcc_jit_location *loc,
    const char *asm_template,
    int num_outputs, struct gcc_jit_extended_asm **outputs,
    int num_inputs, struct gcc_jit_extended_asm **inputs,
    int num_clobbers, const char **clobbers,
    int num_goto_blocks, struct gcc_jit_block **goto_blocks,
    struct gcc_jit_block *fallthrough_block);

typedef void (*pfn_gcc_jit_lvalue_set_tls_model)(
    struct gcc_jit_lvalue *lvalue,
    enum gcc_jit_tls_model model);

typedef void (*pfn_gcc_jit_function_add_attribute)(
    struct gcc_jit_function *func,
    enum gcc_jit_fn_attribute attribute);

typedef void (*pfn_gcc_jit_lvalue_add_string_attribute)(
    struct gcc_jit_lvalue *variable,
    enum gcc_jit_variable_attribute attribute,
    const char* value);

typedef void (*pfn_gcc_jit_lvalue_set_alignment)(
    struct gcc_jit_lvalue *lvalue,
    unsigned bytes);

typedef void (*pfn_gcc_jit_lvalue_set_register_name)(
    struct gcc_jit_lvalue *lvalue,
    const char *reg_name);

typedef struct gcc_jit_type * (*pfn_gcc_jit_context_get_int_type)(
    struct gcc_jit_context *ctxt,
    int num_bytes,
    int is_signed);

typedef struct gcc_jit_type * (*pfn_gcc_jit_type_get_vector)(
    struct gcc_jit_type *type,
    size_t num_units);

/* Optional modern libgccjit API features (loaded dynamically on all platforms) */
typedef struct gcc_jit_rvalue * (*pfn_gcc_jit_context_new_struct_constructor)(
    struct gcc_jit_context *ctxt,
    struct gcc_jit_location *loc,
    struct gcc_jit_type *type,
    size_t num_values,
    struct gcc_jit_field **fields,
    struct gcc_jit_rvalue **values);

typedef struct gcc_jit_rvalue * (*pfn_gcc_jit_context_new_array_constructor)(
    struct gcc_jit_context *ctxt,
    struct gcc_jit_location *loc,
    struct gcc_jit_type *type,
    size_t num_values,
    struct gcc_jit_rvalue **values);

extern pfn_gcc_jit_context_new_struct_constructor p_gcc_jit_context_new_struct_constructor;
extern pfn_gcc_jit_context_new_array_constructor p_gcc_jit_context_new_array_constructor;

extern pfn_gcc_jit_block_end_with_extended_asm_goto p_gcc_jit_block_end_with_extended_asm_goto;
extern pfn_gcc_jit_lvalue_set_tls_model p_gcc_jit_lvalue_set_tls_model;
extern pfn_gcc_jit_function_add_attribute p_gcc_jit_function_add_attribute;
extern pfn_gcc_jit_lvalue_add_string_attribute p_gcc_jit_lvalue_add_string_attribute;
extern pfn_gcc_jit_lvalue_set_alignment p_gcc_jit_lvalue_set_alignment;
extern pfn_gcc_jit_lvalue_set_register_name p_gcc_jit_lvalue_set_register_name;
extern pfn_gcc_jit_context_get_int_type p_gcc_jit_context_get_int_type;
extern pfn_gcc_jit_type_get_vector p_gcc_jit_type_get_vector;

/**
 * @brief Loads the libgccjit dynamic library on Windows.
 *
 * @return GCC_GEN_BE_SUCCESS if loaded successfully, or an error code otherwise.
 */
extern gcc_gen_be_error_t load_libgccjit_windows(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Loads the libgccjit dynamic library on POSIX systems.
 *
 * @return GCC_GEN_BE_SUCCESS if loaded successfully, or an error code otherwise.
 */
extern gcc_gen_be_error_t load_libgccjit_posix(void) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_LIB_LOADER_H */
