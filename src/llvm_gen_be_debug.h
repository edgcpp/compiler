/**
 * @file llvm_gen_be_debug.h
 * @brief LLVM IR Backend Debug Information (DWARF/CodeView) Subsystem.
 * @details Encapsulates LLVM's DIBuilder and manages debug metadata emission
 * including compile units, lexical scopes, types, and source locations.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_DEBUG_H
#define LLVM_GEN_BE_DEBUG_H 1

#include "basic_hdrs.h"
#include "fe_common.h"
#include "types.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Instructions.h>
#include <unordered_map>
#include <vector>
#include <string>

BEGIN_EDG_NAMESPACE

/**
 * @struct llvm_gen_be_debug_state_t
 * @brief Manages the state and context for LLVM debug information generation.
 * @details Holds the active `DIBuilder`, the current `DICompileUnit`, and caches
 * for files, types, and lexical scopes.
 */
struct llvm_gen_be_debug_state_t {
  /// @brief Pointer to the LLVM DIBuilder instance.
  llvm::DIBuilder* builder;

  /// @brief The main compile unit for the module.
  llvm::DICompileUnit* compile_unit;

  /// @brief Cache mapping file paths to their LLVM DIFile descriptors.
  std::unordered_map<std::string, llvm::DIFile*> di_files_map;

  /// @brief Cache mapping EDG types to their LLVM DIType descriptors.
  std::unordered_map<a_type_ptr, llvm::DIType*> di_types_map;

  /// @brief Stack of current lexical scopes (DIScope*).
  std::vector<llvm::DIScope*> scope_stack;
};

/**
 * @brief Initializes the debug information state and creates the main compile unit.
 * @details Allocates `llvm_gen_be_debug_state_t`, initializes the `DIBuilder`, and registers the `DICompileUnit`.
 *
 * @param[out] out_dbg_state Pointer to the newly allocated debug state.
 * @param[in] module The LLVM Module to attach debug info to.
 * @param[in] source_file The primary source file path.
 * @param[in] comp_dir The compilation directory.
 * @param[in] is_optimized True if the code is compiled with optimizations.
 * @return An `llvm_gen_be_error_t` code indicating success or failure.
 */
llvm_gen_be_error_t debug_info_init(
    llvm_gen_be_debug_state_t** out_dbg_state,
    llvm::Module* module,
    const char* source_file,
    const char* comp_dir,
    bool is_optimized) noexcept;

/**
 * @brief Finalizes the debug information in the `DIBuilder`.
 * @details Must be called after all functions and metadata have been emitted.
 * Calls `finalize()` on the `DIBuilder`.
 *
 * @param[in,out] dbg_state The debug state to finalize.
 * @return An `llvm_gen_be_error_t` code indicating success or failure.
 */
llvm_gen_be_error_t debug_info_finalize(llvm_gen_be_debug_state_t* dbg_state) noexcept;

/**
 * @brief Cleans up and deallocates the debug information state.
 * @details Releases memory for the `DIBuilder` and the `llvm_gen_be_debug_state_t` struct itself.
 * Sets the pointer to null.
 *
 * @param[in,out] dbg_state Pointer to the debug state pointer to be freed.
 * @return An `llvm_gen_be_error_t` code indicating success or failure.
 */
llvm_gen_be_error_t debug_info_cleanup(llvm_gen_be_debug_state_t** dbg_state) noexcept;

/**
 * @brief Retrieves or creates a debug info file descriptor.
 * @details Resolves the given file path into a directory and filename, caching the resulting `DIFile`.
 *
 * @param[in,out] dbg_state The debug state containing the `DIBuilder` and file cache.
 * @param[in] file_path The path to the source file.
 * @param[out] out_di_file Pointer to the resulting `DIFile`.
 * @return An `llvm_gen_be_error_t` code indicating success or failure.
 */
llvm_gen_be_error_t get_or_create_di_file(
    llvm_gen_be_debug_state_t* dbg_state,
    const char* file_path,
    llvm::DIFile** out_di_file) noexcept;

/**
 * @brief Gets or creates a DILocation for a given source position and scope.
 *
 * @param[in,out] dbg_state The debug state.
 * @param[in] src_pos The EDG source position.
 * @param[in] scope The DWARF scope to associate the location with.
 * @param[out] out_loc Pointer to the resulting `DILocation`.
 * @return An `llvm_gen_be_error_t` code indicating success or failure.
 */
llvm_gen_be_error_t get_di_location(
    llvm_gen_be_debug_state_t* dbg_state,
    a_source_position src_pos,
    llvm::DIScope* scope,
    llvm::DILocation** out_loc) noexcept;

/**
 * @brief Applies debug location metadata to an instruction.
 *
 * @param[in] inst The LLVM instruction.
 * @param[in] src_pos The EDG source position.
 * @return An `llvm_gen_be_error_t` code indicating success or failure.
 */
llvm_gen_be_error_t apply_instruction_debug_loc(
    llvm::Instruction* inst,
    a_source_position src_pos) noexcept;

/**
 * @brief Creates a DISubprogram for a given routine and attaches it to the function.
 *
 * @param[in,out] dbg_state The debug state.
 * @param[in] routine The EDG routine.
 * @param[in] fn The LLVM function.
 * @param[out] out_subprogram Pointer to the resulting `DISubprogram`.
 * @return An `llvm_gen_be_error_t` code indicating success or failure.
 */
llvm_gen_be_error_t create_di_subprogram(
    llvm_gen_be_debug_state_t* dbg_state,
    a_routine_ptr routine,
    llvm::Function* fn,
    llvm::DISubprogram** out_subprogram) noexcept;

/**
 * @brief Pushes a new lexical block onto the scope stack.
 *
 * @param[in,out] dbg_state The debug state.
 * @param[in] src_pos The EDG source position.
 * @param[out] out_block Pointer to the resulting `DILexicalBlock`.
 * @return An `llvm_gen_be_error_t` code indicating success or failure.
 */
llvm_gen_be_error_t push_lexical_block(
    llvm_gen_be_debug_state_t* dbg_state,
    a_source_position src_pos,
    llvm::DILexicalBlock** out_block) noexcept;

/**
 * @brief Pops the current lexical block from the scope stack.
 *
 * @param[in,out] dbg_state The debug state.
 * @return An `llvm_gen_be_error_t` code indicating success or failure.
 */
llvm_gen_be_error_t pop_lexical_block(llvm_gen_be_debug_state_t* dbg_state) noexcept;

/**
 * @brief Retrieves or creates a DIType for a given EDG type.
 *
 * @param[in,out] dbg_state The debug state.
 * @param[in] ty The EDG type to process.
 * @param[out] out_di_type Pointer to the resulting `DIType`.
 * @return An `llvm_gen_be_error_t` code indicating success or failure.
 */
llvm_gen_be_error_t get_or_create_di_type(
    llvm_gen_be_debug_state_t* dbg_state,
    a_type_ptr ty,
    llvm::DIType** out_di_type) noexcept;

/**
 * @brief Emits a debug declare intrinsic for a local variable or parameter.
 *
 * @param[in,out] dbg_state The debug state.
 * @param[in] var The EDG variable entry.
 * @param[in] alloca_inst The LLVM alloca instruction representing the variable's storage.
 * @return An `llvm_gen_be_error_t` code indicating success or failure.
 */
llvm_gen_be_error_t emit_dbg_declare_for_variable(
    llvm_gen_be_debug_state_t* dbg_state,
    a_variable_ptr var,
    llvm::AllocaInst* alloca_inst) noexcept;

END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_DEBUG_H */
