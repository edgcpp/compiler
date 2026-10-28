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

#if !defined(_WIN32)
#if defined(__has_include)
#  if __has_include(<libgccjit.h>)
#    include <libgccjit.h>
#  endif
#endif
#endif

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
/** @brief pfn_gcc_jit_block_end_with_extended_asm_goto member. */
typedef void (*pfn_gcc_jit_block_end_with_extended_asm_goto)(
    struct gcc_jit_block *block,
    struct gcc_jit_location *loc,
    const char *asm_template,
    int num_outputs, struct gcc_jit_extended_asm **outputs,
    int num_inputs, struct gcc_jit_extended_asm **inputs,
    int num_clobbers, const char **clobbers,
    int num_goto_blocks, struct gcc_jit_block **goto_blocks,
    struct gcc_jit_block *fallthrough_block);

/** @brief pfn_gcc_jit_lvalue_set_tls_model member. */
typedef void (*pfn_gcc_jit_lvalue_set_tls_model)(
    struct gcc_jit_lvalue *lvalue,
    enum gcc_jit_tls_model model);

/** @brief pfn_gcc_jit_function_add_attribute member. */
typedef void (*pfn_gcc_jit_function_add_attribute)(
    struct gcc_jit_function *func,
    enum gcc_jit_fn_attribute attribute);

/** @brief pfn_gcc_jit_function_add_string_attribute member. */
typedef void (*pfn_gcc_jit_function_add_string_attribute)(
    struct gcc_jit_function *func,
    enum gcc_jit_fn_attribute attribute,
    const char *value);

/** @brief pfn_gcc_jit_lvalue_add_string_attribute member. */
typedef void (*pfn_gcc_jit_lvalue_add_string_attribute)(
    struct gcc_jit_lvalue *variable,
    enum gcc_jit_variable_attribute attribute,
    const char* value);

/** @brief pfn_gcc_jit_lvalue_set_alignment member. */
typedef void (*pfn_gcc_jit_lvalue_set_alignment)(
    struct gcc_jit_lvalue *lvalue,
    unsigned bytes);

/** @brief pfn_gcc_jit_context_new_array_constructor member. */
/** @brief pfn_gcc_jit_context_new_struct_constructor member. */
/** @brief pfn_gcc_jit_type_get_vector member. */
/** @brief pfn_gcc_jit_context_get_int_type member. */
/** @brief pfn_gcc_jit_lvalue_set_register_name member. */
typedef void (*pfn_gcc_jit_lvalue_set_register_name)(
    struct gcc_jit_lvalue *lvalue,
    const char *reg_name);

/** @brief Function pointer type for `gcc_jit_context_get_int_type`. */
typedef struct gcc_jit_type * (*pfn_gcc_jit_context_get_int_type)(
    struct gcc_jit_context *ctxt,
    int num_bytes,
    int is_signed);

/** @brief Function pointer type for `gcc_jit_type_get_vector`. */
typedef struct gcc_jit_type * (*pfn_gcc_jit_type_get_vector)(
    struct gcc_jit_type *type,
    size_t num_units);

/* Optional modern libgccjit API features (loaded dynamically on all platforms) */
/** @brief Function pointer type for `gcc_jit_context_new_struct_constructor`. */
typedef struct gcc_jit_rvalue * (*pfn_gcc_jit_context_new_struct_constructor)(
    struct gcc_jit_context *ctxt,
    struct gcc_jit_location *loc,
    struct gcc_jit_type *type,
    size_t num_values,
    struct gcc_jit_field **fields,
    struct gcc_jit_rvalue **values);

/** @brief Function pointer type for `gcc_jit_context_new_array_constructor`. */
typedef struct gcc_jit_rvalue * (*pfn_gcc_jit_context_new_array_constructor)(
    struct gcc_jit_context *ctxt,
    struct gcc_jit_location *loc,
    struct gcc_jit_type *type,
    size_t num_values,
    struct gcc_jit_rvalue **values);

/** @brief Function pointer for `gcc_jit_context_new_struct_constructor`. */
extern pfn_gcc_jit_context_new_struct_constructor p_gcc_jit_context_new_struct_constructor;
/** @brief Function pointer type for `gcc_jit_context_new_union_constructor`. */
typedef struct gcc_jit_rvalue * (*pfn_gcc_jit_context_new_union_constructor)(struct gcc_jit_context *ctxt, struct gcc_jit_location *loc, struct gcc_jit_type *type, struct gcc_jit_field *field, struct gcc_jit_rvalue *value);
/** @brief Function pointer type for `gcc_jit_context_new_bitcast`. */
typedef struct gcc_jit_rvalue * (*pfn_gcc_jit_context_new_bitcast)(struct gcc_jit_context *ctxt, struct gcc_jit_location *loc, struct gcc_jit_rvalue *rvalue, struct gcc_jit_type *type);
/** @brief Function pointer type for `gcc_jit_context_new_rvalue_from_vector`. */
typedef struct gcc_jit_rvalue * (*pfn_gcc_jit_context_new_rvalue_from_vector)(struct gcc_jit_context *ctxt, struct gcc_jit_location *loc, struct gcc_jit_type *vec_type, size_t num_elements, struct gcc_jit_rvalue **elements);
/** @brief Function pointer type for `gcc_jit_context_get_builtin_function`. */
typedef struct gcc_jit_function * (*pfn_gcc_jit_context_get_builtin_function)(struct gcc_jit_context *ctxt, const char *name);
/** @brief Function pointer. */
extern pfn_gcc_jit_context_get_builtin_function p_gcc_jit_context_get_builtin_function;

/** @brief Function pointer. */
extern pfn_gcc_jit_context_new_rvalue_from_vector p_gcc_jit_context_new_rvalue_from_vector;

/** @brief Function pointer. */
extern pfn_gcc_jit_context_new_bitcast p_gcc_jit_context_new_bitcast;

/** @brief Function pointer. */
extern pfn_gcc_jit_context_new_union_constructor p_gcc_jit_context_new_union_constructor;

/** @brief Function pointer. */
extern pfn_gcc_jit_context_new_array_constructor p_gcc_jit_context_new_array_constructor;

/** @brief Function pointer. */
extern pfn_gcc_jit_block_end_with_extended_asm_goto p_gcc_jit_block_end_with_extended_asm_goto;
/** @brief Function pointer. */
extern pfn_gcc_jit_lvalue_set_tls_model p_gcc_jit_lvalue_set_tls_model;
/** @brief Function pointer. */
extern pfn_gcc_jit_function_add_attribute p_gcc_jit_function_add_attribute;
/** @brief pfn_gcc_jit_lvalue_set_link_section member. */
typedef void (*pfn_gcc_jit_lvalue_set_link_section)(struct gcc_jit_lvalue *lvalue, const char *section_name);
/** @brief Function pointer. */
extern pfn_gcc_jit_lvalue_set_link_section p_gcc_jit_lvalue_set_link_section;

/** @brief Function pointer. */
/** @brief Function pointer. */
extern pfn_gcc_jit_function_add_string_attribute p_gcc_jit_function_add_string_attribute;

/** @brief Function pointer. */
extern pfn_gcc_jit_lvalue_add_string_attribute p_gcc_jit_lvalue_add_string_attribute;
/** @brief Function pointer. */
extern pfn_gcc_jit_lvalue_set_alignment p_gcc_jit_lvalue_set_alignment;
/** @brief Function pointer. */
extern pfn_gcc_jit_lvalue_set_register_name p_gcc_jit_lvalue_set_register_name;
/** @brief Function pointer. */
extern pfn_gcc_jit_context_get_int_type p_gcc_jit_context_get_int_type;
/** @brief Function pointer. */
extern pfn_gcc_jit_type_get_vector p_gcc_jit_type_get_vector;

/** @brief Function pointer type for `gcc_jit_context_new_rvalue_vector_perm`. */
typedef struct gcc_jit_rvalue * (*pfn_gcc_jit_context_new_rvalue_vector_perm)(
    struct gcc_jit_context *ctxt,
    struct gcc_jit_location *loc,
    struct gcc_jit_rvalue *elements1,
    struct gcc_jit_rvalue *elements2,
    struct gcc_jit_rvalue *mask);

/** @brief Function pointer type for `gcc_jit_context_new_convert_vector`. */
typedef struct gcc_jit_rvalue * (*pfn_gcc_jit_context_new_convert_vector)(
    struct gcc_jit_context *ctxt,
    struct gcc_jit_location *loc,
    struct gcc_jit_rvalue *vector,
    struct gcc_jit_type *type);

/** @brief Function pointer for `gcc_jit_context_new_rvalue_vector_perm`. */
extern pfn_gcc_jit_context_new_rvalue_vector_perm p_gcc_jit_context_new_rvalue_vector_perm;

/** @brief Function pointer for `gcc_jit_context_new_convert_vector`. */
extern pfn_gcc_jit_context_new_convert_vector p_gcc_jit_context_new_convert_vector;

/** @brief Function pointer type for optional `gcc_jit_block_end_with_invoke` unwind extension. */
typedef void (*pfn_gcc_jit_block_end_with_invoke)(
    struct gcc_jit_block *block,
    struct gcc_jit_location *loc,
    struct gcc_jit_rvalue *call,
    struct gcc_jit_block *normal_dest,
    struct gcc_jit_block *unwind_dest);

/** @brief Function pointer for `gcc_jit_block_end_with_invoke`. */
extern pfn_gcc_jit_block_end_with_invoke p_gcc_jit_block_end_with_invoke;

/** @brief Function pointer type for optional `gcc_jit_block_add_try_catch` GIMPLE EH extension. */
typedef void (*pfn_gcc_jit_block_add_try_catch)(
    struct gcc_jit_block *try_block,
    struct gcc_jit_location *loc,
    struct gcc_jit_block *catch_block);

/** @brief Function pointer for `gcc_jit_block_add_try_catch`. */
extern pfn_gcc_jit_block_add_try_catch p_gcc_jit_block_add_try_catch;

/** @brief Function pointer type for optional `gcc_jit_block_add_cleanup` GIMPLE EH extension. */
typedef void (*pfn_gcc_jit_block_add_cleanup)(
    struct gcc_jit_block *block,
    struct gcc_jit_location *loc,
    struct gcc_jit_block *cleanup_block);

/** @brief Function pointer for `gcc_jit_block_add_cleanup`. */
extern pfn_gcc_jit_block_add_cleanup p_gcc_jit_block_add_cleanup;

/** @brief Function pointer type for optional `gcc_jit_block_end_with_resx` GIMPLE EH extension. */
typedef void (*pfn_gcc_jit_block_end_with_resx)(
    struct gcc_jit_block *block,
    struct gcc_jit_location *loc,
    int region_idx);

/** @brief Function pointer for `gcc_jit_block_end_with_resx`. */
extern pfn_gcc_jit_block_end_with_resx p_gcc_jit_block_end_with_resx;

/** @brief Function pointer type for optional `gcc_jit_function_set_personality` GIMPLE EH extension. */
typedef void (*pfn_gcc_jit_function_set_personality)(
    struct gcc_jit_function *fn,
    struct gcc_jit_function *personality_fn);

/** @brief Function pointer for `gcc_jit_function_set_personality`. */
extern pfn_gcc_jit_function_set_personality p_gcc_jit_function_set_personality;

/** @brief Function pointer type for optional `gcc_jit_block_add_landing_pad` GIMPLE EH extension. */
typedef void (*pfn_gcc_jit_block_add_landing_pad)(
    struct gcc_jit_block *block,
    struct gcc_jit_location *loc,
    struct gcc_jit_function *personality_fn,
    int action_index);

/** @brief Function pointer for `gcc_jit_block_add_landing_pad`. */
extern pfn_gcc_jit_block_add_landing_pad p_gcc_jit_block_add_landing_pad;

/** @brief Function pointer type for optional `gcc_jit_context_new_eh_filter` GIMPLE EH extension. */
typedef struct gcc_jit_rvalue * (*pfn_gcc_jit_context_new_eh_filter)(
    struct gcc_jit_context *ctxt,
    struct gcc_jit_location *loc,
    int filter_index);

/** @brief Function pointer for `gcc_jit_context_new_eh_filter`. */
extern pfn_gcc_jit_context_new_eh_filter p_gcc_jit_context_new_eh_filter;

/** @brief Function pointer type for optional `gcc_jit_rvalue_set_bool_require_tail_call`. */
typedef void (*pfn_gcc_jit_rvalue_set_bool_require_tail_call)(
    struct gcc_jit_rvalue *call,
    int require_tail_call);

/** @brief Function pointer for `gcc_jit_rvalue_set_bool_require_tail_call`. */
extern pfn_gcc_jit_rvalue_set_bool_require_tail_call p_gcc_jit_rvalue_set_bool_require_tail_call;

/** @brief Function pointer type for optional `gcc_jit_context_add_top_level_asm`. */
typedef void (*pfn_gcc_jit_context_add_top_level_asm)(
    struct gcc_jit_context *ctxt,
    struct gcc_jit_location *loc,
    const char *asm_stmts);

/** @brief Function pointer for `gcc_jit_context_add_top_level_asm`. */
extern pfn_gcc_jit_context_add_top_level_asm p_gcc_jit_context_add_top_level_asm;

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

/**
 * @brief Probes `libgccjit` for exception handling and unwind builtins.
 * @details Queries `gcc_jit_context_get_builtin_function` (or dynamic `p_gcc_jit_context_get_builtin_function`)
 * for `__builtin_eh_pointer`, `__builtin_eh_filter`, and `__builtin_unwind_resume`, and ensures `_Unwind_Resume`
 * is available in the context.
 * @param[in] ctx The active `libgccjit` context.
 * @param[out] out_has_eh_builtins Pointer to receive `TRUE` if native EH builtins are available, `FALSE` otherwise.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code if arguments are invalid.
 */
extern gcc_gen_be_error_t gcc_gen_be_probe_eh_builtins(struct gcc_jit_context *ctx, a_boolean *out_has_eh_builtins) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Probes `libgccjit` for native GIMPLE EH region entrypoints (`TRY_CATCH_EXPR`, `CLEANUP_POINT_EXPR`, `RESX_EXPR`).
 * @details Checks whether the loaded `libgccjit` shared library exports `gcc_jit_block_add_try_catch`,
 * `gcc_jit_block_add_cleanup`, `gcc_jit_block_end_with_resx`, and `gcc_jit_function_set_personality`.
 * @param[in] ctx The active `libgccjit` context.
 * @param[out] out_supported Pointer to receive `TRUE` if native GIMPLE EH region entrypoints are present, `FALSE` otherwise.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if `ctx` or `out_supported` is NULL.
 */
extern gcc_gen_be_error_t gcc_gen_be_probe_native_eh_support(
    struct gcc_jit_context *ctx,
    a_boolean *out_supported) GCC_GEN_BE_NOEXCEPT;

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
extern gcc_gen_be_error_t gcc_gen_be_resolve_eh_entrypoints(
    struct gcc_jit_context *ctx,
    a_boolean *out_has_invoke,
    a_boolean *out_has_landing_pad) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Probes `libgccjit` at runtime for extended floating-point `enum gcc_jit_types` support.
 * @details Checks whether `GCC_JIT_TYPE_BFLOAT16`, `_Float16`, `_Float32`, `_Float64`, and `_Float128`
 * enumerators are compiled into the active headers and runtime library without triggering errors.
 * @param[in] ctx Active `gcc_jit_context`.
 * @param[out] out_has_ext_float Pointer to receive `TRUE` if extended float types are supported, `FALSE` otherwise.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
extern gcc_gen_be_error_t gcc_gen_be_probe_extended_float_types(
    struct gcc_jit_context *ctx,
    a_boolean *out_has_ext_float) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_LIB_LOADER_H */
