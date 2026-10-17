/**
 * @file gcc_gen_be_rtti.h
 * @brief RTTI and VTable generation for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_RTTI_H
#define GCC_GEN_BE_RTTI_H

#include "gcc_gen_be_error.h"
#include <libgccjit.h>


#ifdef EDG_GCC_BE_ISOLATED_TEST
#include "basic_hdrs.h"
struct a_type;
struct a_base_class;
struct a_routine;
struct an_expr_node;
typedef struct a_type *a_type_ptr;
typedef struct a_base_class *a_base_class_ptr;
typedef struct a_routine *a_routine_ptr;
typedef struct an_expr_node *an_expr_node_ptr;
#else
#include "fe_common.h"
#include "il_def.h"
#endif

#ifndef BEGIN_EDG_NAMESPACE
#define BEGIN_EDG_NAMESPACE
#define END_EDG_NAMESPACE
#endif

BEGIN_EDG_NAMESPACE

/**
 * @brief Emits a virtual table for a given class type.
 *
 * @param class_type The class type for which to emit the vtable.
 * @param out_vtable Pointer to receive the generated JIT lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_vtable(a_type_ptr class_type, struct gcc_jit_lvalue **out_vtable) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits a Virtual Table Table (VTT) for classes with virtual base classes.
 *
 * @param class_type The class type for which to emit the VTT.
 * @param out_vtt Pointer to receive the generated JIT lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_vtt(a_type_ptr class_type, struct gcc_jit_lvalue **out_vtt) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits a virtual method dispatch thunk.
 *
 * @param thunk_rp The thunk routine pointer to emit.
 * @param out_func Pointer to receive the generated JIT function.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_thunk(a_routine_ptr thunk_rp, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits an Itanium C++ ABI virtual-base offset call thunk (`_ZTv0_n...`).
 * @details Loads the `vptr` from `this`, reads the `vcall` offset at `*(ptrdiff_t *)((char *)vptr + vcall_offset)`,
 * adjusts `this` by adding that offset, and forwards all arguments to `rout`.
 * @param[in] class_type The enclosing class type.
 * @param[in] vbase The virtual base class entry (may be NULL if `vcall_offset` is explicit).
 * @param[in] rout The target or thunk routine entry.
 * @param[in] vcall_offset Byte offset in the vtable to the `vcall` offset slot.
 * @param[out] out_thunk Pointer to receive the generated thunk `gcc_jit_function`.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_vcall_thunk(
    a_type_ptr class_type,
    a_base_class_ptr vbase,
    a_routine_ptr rout,
    long vcall_offset,
    struct gcc_jit_function **out_thunk) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits an Itanium C++ ABI covariant return adjustment thunk (`_ZTch...` / `_ZTcv...`).
 * @details Adjusts `this` on entry (if non-zero), invokes `overriding_rout`, and if the returned pointer
 * is non-null, adjusts the returned pointer by `result_offset` (or virtual base offset from the returned
 * object's vtable when `is_vbase_return` is TRUE) before returning.
 * @param[in] overriding_rout The overriding virtual routine or thunk wrapper.
 * @param[in] this_offset Fixed `this` pointer byte adjustment on entry.
 * @param[in] result_offset Fixed or vbase-index return pointer adjustment.
 * @param[in] is_vbase_return TRUE if `result_offset` is a vtable vbase offset index, FALSE for fixed offset.
 * @param[out] out_thunk Pointer to receive the generated thunk `gcc_jit_function`.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_covariant_return_thunk(
    a_routine_ptr overriding_rout,
    long this_offset,
    long result_offset,
    a_boolean is_vbase_return,
    struct gcc_jit_function **out_thunk) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a dynamic_cast expression into a call to __dynamic_cast.
 *
 * @param expr The typeid expression node.
 * @param out_rval Pointer to receive the generated JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_dynamic_cast(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a typeid expression into a type_info descriptor.
 *
 * @param expr The typeid expression node.
 * @param out_rval Pointer to receive the generated JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_typeid(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits a std::type_info descriptor for the given type.
 *
 * @param tp The type to describe.
 * @param out_descriptor Pointer to receive the generated JIT lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_type_descriptor(a_type_ptr tp, struct gcc_jit_lvalue **out_descriptor) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Mangles an EDG frontend type into an Itanium C++ ABI type string.
 * @details Resets the active substitution table and writes the null-terminated Itanium
 * type encoding into `buffer`.
 * @param[in] tp The frontend type to mangle.
 * @param[out] buffer Destination buffer to receive the mangled type name.
 * @param[in] size Capacity of `buffer` in bytes.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_mangle_type_name(a_type_ptr tp, char *buffer, size_t size) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Mangles an EDG frontend routine into an Itanium C++ ABI symbol name (`_Z...`).
 * @details Encodes nested scopes, template arguments (`I...E`), special member kinds
 * (`C1`, `C2`, `D0`, `D1`, `D2`), overloaded operators (`nw`, `na`, `dl`, `da`, `cl`, `ix`, `eq`, `lt`, `ss`),
 * CV/ref qualifiers, and parameter types.
 * @param[in] rout The frontend routine to mangle.
 * @param[in] variant_code Optional constructor/destructor variant override (e.g. `"C1"`, `"C2"`, `"D0"`, `"D1"`, `"D2"`, or NULL).
 * @param[out] buffer Destination buffer to receive the mangled function symbol name.
 * @param[in] size Capacity of `buffer` in bytes.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_mangle_function_name(
    a_routine_ptr rout,
    const char *variant_code,
    char *buffer,
    size_t size) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Mangles an EDG frontend expression node into an Itanium C++ ABI expression encoding.
 * @details Supports `sizeof(T)` (`st`), `sizeof(expr)` (`sz`), `alignof(T)` (`at`), `noexcept(expr)` (`nx`),
 * `decltype(expr)` (`DT...E`), unary/binary operators, and integer/address literals inside dependent
 * template arguments and trailing return types.
 * @param[in] expr The frontend expression node to mangle.
 * @param[out] buffer Destination buffer to receive the mangled expression string.
 * @param[in] size Capacity of `buffer` in bytes.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_mangle_expr(
    an_expr_node_ptr expr,
    char *buffer,
    size_t size) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Mangles an EDG frontend type using the shared Itanium C++ ABI substitution table (`S_`, `S0_`, ...).
 * @details Optionally resets `active_subst_table` when `reset_table` is `TRUE`, then encodes `tp` while
 * recording and substituting repeated composite types.
 * @param[in] tp The frontend type to mangle.
 * @param[in] reset_table `TRUE` to clear previous substitutions before mangling, `FALSE` to preserve them.
 * @param[out] buffer Destination buffer to receive the mangled type name.
 * @param[in] size Capacity of `buffer` in bytes.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_mangle_with_substitutions(
    a_type_ptr tp,
    a_boolean reset_table,
    char *buffer,
    size_t size) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_RTTI_H */
