/**
 * @file gcc_gen_be_debug.h
 * @brief Debug Metadata and Scope Generation for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_DEBUG_H
#define GCC_GEN_BE_DEBUG_H
#include "gcc_gen_be_error.h"
#include <libgccjit.h>
#include "fe_common.h"

BEGIN_EDG_NAMESPACE

/**
 * @brief Enables DWARF debug information generation for the compilation context.
 *
 * @details Configures libgccjit to emit DW_TAG_compile_unit and detailed DWARF metadata.
 *
 * @param[in,out] ctx The JIT context to configure.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT if ctx is NULL.
 */
extern gcc_gen_be_error_t gcc_gen_be_enable_debug_info(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Ensures debug tracking is active for the current context.
 *
 * @details Retrieves the active compilation context and enables debug generation options.
 *
 * @return GCC_GEN_BE_SUCCESS on success, or an internal error if context retrieval fails.
 */
extern gcc_gen_be_error_t gcc_gen_be_initialize_debug(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits compile unit debug info.
 * @details Synthesizes DW_TAG_compile_unit equivalent by ensuring the primary source file location is initialized.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INTERNAL if the context is missing.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_compile_unit_debug_info(void) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits subprogram debug info.
 * @details Synthesizes DW_TAG_subprogram equivalent by mapping the routine's source location.
 * @param[in] rout Routine to process.
 * @param[out] func JIT function to associate with debug info.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_subprogram_debug_info(a_routine_ptr rout, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits type debug info.
 * @details Synthesizes type equivalent debugging info.
 * @param[in] tp Type.
 * @param[out] jit_type JIT type.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_type_debug_info(a_type_ptr tp, struct gcc_jit_type *jit_type) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits variable debug info.
 * @details Synthesizes DW_TAG_variable equivalent by attaching a location to the global or local variable.
 * @param[in] var Variable.
 * @param[out] lval Lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_variable_debug_info(a_variable_ptr var, gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits a source location mapping for a statement block.
 * @details Attaches lexical scope boundaries to compound statement blocks.
 * @param[in] stmt The statement node.
 * @param[out] block The corresponding JIT block.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_block_debug_info(a_statement_ptr stmt, gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Associates lexical scope coordinates with a statement block without inserting dummy evaluations.
 * @details Resolves the source location for `stmt` and returns it cleanly.
 * @param[in] stmt The statement node.
 * @param[in] block The corresponding JIT block.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_lexical_block_scope(a_statement_ptr stmt, gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Attaches inlined subroutine DWARF metadata (`DW_TAG_inlined_subroutine`) to a call block.
 * @details Associates the inlined routine's definition location and the call-site statement location
 * with `block` using a block comment marker and source location mapping.
 * @param[in] inlined_rout The frontend routine being inlined.
 * @param[in] call_stmt The statement containing the inlined call site.
 * @param[in,out] block The target `gcc_jit_block`.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_inlined_subroutine(
    a_routine_ptr inlined_rout,
    a_statement_ptr call_stmt,
    gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Propagates DWARF debug level and format options to the active JIT context.
 * @details Supports `-g0`, `-g1`, `-g2`, `-g3`, `-gdwarf-4`, `-gdwarf-5`, `-fdebug-macro`,
 * and `-fno-eliminate-unused-debug-types`.
 * @param[in,out] ctx The libgccjit context.
 * @param[in] dwarf_version DWARF format version (e.g., 4 or 5; 0 for default).
 * @param[in] dbg_level Debug verbosity level (0..3).
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_dwarf_info(gcc_jit_context *ctx, int dwarf_version, int dbg_level) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Configures the active DWARF format version (`2..5`) and debug verbosity level (`0..3`).
 * @details Stores the configured DWARF version and verbosity level used by subsequent calls to
 * `gcc_gen_be_enable_debug_info`, `gcc_gen_be_emit_lexical_block_scope`, and `gcc_gen_be_emit_inlined_subroutine`.
 * @param[in] dwarf_version DWARF format version (`2`, `3`, `4`, or `5`; `0` selects default `4`).
 * @param[in] dbg_level Debug verbosity level (`0..3`).
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if out of range.
 */
extern gcc_gen_be_error_t gcc_gen_be_set_dwarf_config(int dwarf_version, int dbg_level) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Retrieves the active DWARF format version and debug verbosity level.
 * @details Writes the active DWARF version and verbosity level into the provided output pointers.
 * @param[out] out_dwarf_version Pointer to receive the active DWARF version (`2..5`).
 * @param[out] out_dbg_level Pointer to receive the active debug verbosity level (`0..3`).
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if both outputs are NULL.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_dwarf_config(int *out_dwarf_version, int *out_dbg_level) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits DWARF debug metadata (`DW_TAG_namespace`) for a C++ namespace or namespace alias.
 * @details Resolves the source location of `ns->source_corresp.decl_position` and classifies the
 * namespace DIE kind (`DW_TAG_namespace`, `DW_TAG_namespace (inline)`, `DW_TAG_namespace (anonymous)`,
 * or `DW_TAG_imported_declaration`) into `*out_dwarf_tag`.
 * @param[in] ns The frontend namespace pointer.
 * @param[out] out_dwarf_tag Pointer to receive the static DWARF tag descriptor string.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_namespace_debug_info(
    a_namespace_ptr ns,
    const char **out_dwarf_tag) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits DWARF inheritance metadata (`DW_TAG_inheritance`) for a C++ class or struct type.
 * @details Walks the base class list of `class_tp`, attaches source locations for each base class type,
 * and writes the number of base classes processed into `*out_base_count`.
 * @param[in] class_tp The frontend `tk_class` or `tk_struct` type pointer.
 * @param[out] out_base_count Pointer to receive the number of `DW_TAG_inheritance` entries processed.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_class_inheritance_debug_info(
    a_type_ptr class_tp,
    int *out_base_count) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits DWARF template parameter metadata (`DW_TAG_template_type_parameter` / `DW_TAG_template_value_parameter`).
 * @details Walks `targ_list`, resolves source locations for type and non-type template arguments,
 * and writes the count of template parameters into `*out_param_count`.
 * @param[in] targ_list Linked list of frontend template arguments (may be NULL for non-template entities).
 * @param[out] out_param_count Pointer to receive the number of template parameters processed.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if `out_param_count` is NULL.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_template_param_debug_info(
    a_template_arg_ptr targ_list,
    int *out_param_count) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Emits DWARF enumeration metadata (`DW_TAG_enumeration_type` and `DW_TAG_enumerator`).
 * @details Walks the enumerator constant list for both unscoped (`enum`) and scoped (`enum class`)
 * integer types, attaches source locations for each enumerator, and writes the enumerator count into
 * `*out_enumerator_count`.
 * @param[in] enum_tp The frontend `tk_integer` enumeration type pointer.
 * @param[out] out_enumerator_count Pointer to receive the number of enumerators processed.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
extern gcc_gen_be_error_t gcc_gen_be_emit_enum_debug_info(
    a_type_ptr enum_tp,
    int *out_enumerator_count) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_DEBUG_H */
