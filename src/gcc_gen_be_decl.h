/**
 * @file gcc_gen_be_decl.h
 * @brief Declaration lowering for the GCC backend.
 *
 * This file declares functions for lowering EDG variables and functions
 * into libgccjit global variables and functions.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_DECL_H
#define GCC_GEN_BE_DECL_H

#include "gcc_gen_be_error.h"
#include "fe_common.h"
#include "il.h"

struct gcc_jit_lvalue;
struct gcc_jit_function;
struct gcc_jit_block;

BEGIN_EDG_NAMESPACE

/**
 * @brief Retrieves or creates the global dynamic initialization block.
 *
 * @param out_block A pointer to receive the gcc_jit_block.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_global_ctor_block(struct gcc_jit_block **out_block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers an EDG variable declaration into a libgccjit global lvalue.
 *
 * @param var The frontend variable node.
 * @param out_lval A pointer to a gcc_jit_lvalue pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_lval` populated.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers an EDG variable declaration into a libgccjit local lvalue.
 *
 * @param func The libgccjit function the local belongs to.
 * @param var The frontend variable node.
 * @param out_lval A pointer to a gcc_jit_lvalue pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_lval` populated.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_local_variable_decl(struct gcc_jit_function *func, a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers a variable-length array (VLA) local variable declaration (`stmk_vla_decl` / `dik_vla_alloc`).
 * @details Computes the runtime byte size of the VLA from its dimension expressions and invokes
 * `__builtin_alloca` to allocate stack storage, caching the resulting pointer lvalue in `GCC_GEN_BE_CACHE_VAR`.
 * @param[in] func The active `gcc_jit_function`.
 * @param[in] var The frontend VLA variable entry.
 * @param[out] out_lval Pointer to receive the allocated VLA local lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_vla_decl(struct gcc_jit_function *func, a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Lowers an EDG function/routine declaration into a libgccjit function.
 *
 * @param rout The frontend routine node.
 * @param out_func A pointer to a gcc_jit_function pointer that will receive the result.
 * @return GCC_GEN_BE_SUCCESS on success, with `*out_func` populated.
 */
extern gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Synthesizes Itanium C++ ABI constructor (`C2`) and destructor (`D2`, `D0`) clones.
 * @details For complete constructors (`C1`), emits the corresponding base object constructor (`C2`).
 * For complete destructors (`D1`), emits the base object destructor (`D2`) and, for polymorphic classes,
 * the deleting destructor (`D0`, which calls `D1` followed by `_ZdlPv`).
 * @param[in] rout The frontend constructor or destructor routine node.
 * @param[in] primary_func The lowered primary (`C1` or `D1`) `gcc_jit_function`.
 * @param[out] out_base_clone Optional pointer to receive the `C2` or `D2` clone function.
 * @param[out] out_deleting_clone Optional pointer to receive the `D0` deleting destructor clone function.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_ctor_dtor_clones(
    a_routine_ptr rout,
    struct gcc_jit_function *primary_func,
    struct gcc_jit_function **out_base_clone,
    struct gcc_jit_function **out_deleting_clone) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Formats cross-platform COMDAT / weak-ODR section and linkage attribute names.
 * @details Produces `.gnu.linkonce.{t,r,d}.<symbol>` + `"weak"` for ELF (`object_format == 0`),
 * `__TEXT,__textcoal_nt` / `__DATA,__const_coal` / `__DATA,__datacoal_nt` + `"weak_definition"` for
 * Mach-O (`object_format == 1`), and `.text$mn` / `.rdata$r` / `.data$r` + `"selectany"` for
 * COFF/PE (`object_format == 2`).
 * @param[in] symbol_name Mangled symbol name for the COMDAT group.
 * @param[in] is_function `TRUE` if the symbol is a function, `FALSE` for data/vtables/RTTI.
 * @param[in] is_readonly_data `TRUE` if the data symbol is read-only (`_ZTV`, `_ZTT`, `_ZTI`, `_ZTS`).
 * @param[in] object_format Object file format selector (`0` = ELF, `1` = Mach-O, `2` = COFF/PE).
 * @param[out] out_section Destination buffer for the formatted section name.
 * @param[in] buf_cap Capacity of `out_section` in bytes.
 * @param[out] out_linkage_attr Pointer to receive the static linkage attribute string.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
extern gcc_gen_be_error_t gcc_gen_be_format_comdat_section_name(
    const char *symbol_name,
    a_boolean is_function,
    a_boolean is_readonly_data,
    int object_format,
    char *out_section,
    size_t buf_cap,
    const char **out_linkage_attr) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Applies cross-platform COMDAT / vague-linkage attributes and section placement to a global lvalue.
 * @details Formats the host object format's COMDAT section and linkage attribute via
 * `gcc_gen_be_format_comdat_section_name()` and attaches them when supported by `libgccjit`.
 * @param[in] global The target `gcc_jit_lvalue` global variable.
 * @param[in] symbol_name Mangled symbol name.
 * @param[in] is_readonly_data `TRUE` if read-only data (`_ZTV`, `_ZTT`, `_ZTI`, `_ZTS`), `FALSE` otherwise.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
extern gcc_gen_be_error_t gcc_gen_be_apply_comdat_linkage(
    struct gcc_jit_lvalue *global,
    const char *symbol_name,
    a_boolean is_readonly_data) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_DECL_H */