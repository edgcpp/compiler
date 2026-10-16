#include "llvm_gen_be_internal.h"
/**
 * @file llvm_gen_be_debug.cpp
 * @brief LLVM IR Backend Debug Information (DWARF/CodeView) Subsystem.
 * @details Encapsulates LLVM's DIBuilder and manages debug metadata emission
 * including compile units, lexical scopes, types, and source locations.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_debug.h"
#include <llvm/Support/Path.h>
#include <llvm/Support/MD5.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/TargetParser/Triple.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Initializes the debug information generation subsystem.
 * @details Creates the DIBuilder instance, emits compile unit metadata, and sets DWARF/CodeView module flags.
 * @param[out] out_dbg_state Receives the allocated debug state handle on success.
 * @param[in] module The LLVM Module being compiled.
 * @param[in] source_file The primary source file path.
 * @param[in] comp_dir The compilation working directory.
 * @param[in] is_optimized True if optimizations are enabled.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t debug_info_init(
    llvm_gen_be_debug_state_t** out_dbg_state,
    llvm::Module* module,
    const char* source_file,
    const char* comp_dir,
    bool is_optimized) noexcept {
  
  if (out_dbg_state == nullptr || module == nullptr || source_file == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  llvm_gen_be_debug_state_t* dbg_state = new (std::nothrow) llvm_gen_be_debug_state_t();
  if (dbg_state == nullptr) {
    return llvm_gen_be_error_t::out_of_memory;
  }

  dbg_state->builder = new (std::nothrow) llvm::DIBuilder(*module);
  if (dbg_state->builder == nullptr) {
    delete dbg_state;
    return llvm_gen_be_error_t::out_of_memory;
  }

  // Setup module flags for DWARF or CodeView
  module->addModuleFlag(llvm::Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION);
  llvm::Triple triple(module->getTargetTriple());
  if (triple.isWindowsMSVCEnvironment()) {
    module->addModuleFlag(llvm::Module::Warning, "CodeView", 1);
    // LLVM CodeViewDebug backend automatically lowers DICompositeType into LF_STRUCTURE, LF_FIELDLIST, etc.
    dbg_state->is_codeview = true;
  } else {
    unsigned dwarf_version = 5;
    module->addModuleFlag(llvm::Module::Warning, "Dwarf Version", dwarf_version);
    module->addModuleFlag(llvm::Module::Warning, "EnableSplitDwarf", 1); // Enable .dwo support
    dbg_state->is_codeview = false;
  }

  llvm::DIFile* di_file = nullptr;
  llvm_gen_be_error_t err = get_or_create_di_file(dbg_state, source_file, &di_file);
  if (err != llvm_gen_be_error_t::ok) {
    llvm_gen_be_error_t cleanup_err = debug_info_cleanup(&dbg_state);
    return (cleanup_err != llvm_gen_be_error_t::ok) ? cleanup_err : err;
  }

  std::string producer = "EDG C++ Front End with LLVM IR Backend";
  
  dbg_state->compile_unit = dbg_state->builder->createCompileUnit(
      llvm::dwarf::DW_LANG_C_plus_plus,
      di_file,
      producer,
      is_optimized,
      "", // Flags
      0,  // Runtime version
      llvm::StringRef("split.dwo"), // Split name
      llvm::DICompileUnit::DebugEmissionKind::FullDebug,
      1, // DWOId
      true, // SplitDebugInlining
      false, // DebugInfoForProfiling
      llvm::DICompileUnit::DebugNameTableKind::Default
  );

  if (dbg_state->compile_unit == nullptr) {
    llvm_gen_be_error_t cleanup_err = debug_info_cleanup(&dbg_state);
    return (cleanup_err != llvm_gen_be_error_t::ok) ? cleanup_err : llvm_gen_be_error_t::di_metadata_failure;
  }

  *out_dbg_state = dbg_state;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Finalizes the debug information generator.
 * @details Invokes DIBuilder::finalize to seal all debug info metadata nodes.
 * @param[in] dbg_state Pointer to the active debug state handle.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if null.
 */
llvm_gen_be_error_t debug_info_finalize(llvm_gen_be_debug_state_t* dbg_state) noexcept {
  if (dbg_state == nullptr || dbg_state->builder == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  
  dbg_state->builder->finalize();
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Releases all allocated memory and resources associated with debug info generation.
 * @details Frees the DIBuilder and clears maps and scope stacks.
 * @param[in,out] dbg_state Pointer to the debug state pointer to clean up and nullify.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if pointer is null.
 */
llvm_gen_be_error_t debug_info_cleanup(llvm_gen_be_debug_state_t** dbg_state) noexcept {
  if (dbg_state == nullptr || *dbg_state == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  if ((*dbg_state)->builder != nullptr) {
    delete (*dbg_state)->builder;
    (*dbg_state)->builder = nullptr;
  }

  (*dbg_state)->di_files_map.clear();
  (*dbg_state)->di_types_map.clear();
  (*dbg_state)->scope_stack.clear();

  delete *dbg_state;
  *dbg_state = nullptr;

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Retrieves or creates an LLVM DIFile metadata node for a given source file path.
 * @details Hashes file contents with MD5 checksum if available and caches DIFile instances.
 * @param[in,out] dbg_state Pointer to the active debug state handle.
 * @param[in] file_path The path to the source file.
 * @param[out] out_di_file Receives the resulting DIFile metadata pointer on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t get_or_create_di_file(
    llvm_gen_be_debug_state_t* dbg_state,
    const char* file_path,
    llvm::DIFile** out_di_file) noexcept {
  
  if (dbg_state == nullptr || dbg_state->builder == nullptr || file_path == nullptr || out_di_file == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  std::string path_str(file_path);
  auto it = dbg_state->di_files_map.find(path_str);
  if (it != dbg_state->di_files_map.end()) {
    *out_di_file = it->second;
    return llvm_gen_be_error_t::ok;
  }

  llvm::StringRef directory = llvm::sys::path::parent_path(path_str);
  llvm::StringRef filename = llvm::sys::path::filename(path_str);

  // Compute checksum (MD5) if possible
  std::optional<llvm::DIFile::ChecksumInfo<llvm::StringRef>> checksum_info;
  
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> buffer_or_err = llvm::MemoryBuffer::getFile(path_str);
  std::string checksum_str;
  if (buffer_or_err) {
    llvm::MD5 hash;
    hash.update((*buffer_or_err)->getBuffer());
    llvm::MD5::MD5Result result;
    hash.final(result);
    llvm::SmallString<32> hex_string;
    llvm::MD5::stringifyResult(result, hex_string);
    checksum_str = std::string(hex_string.str());
    checksum_info = llvm::DIFile::ChecksumInfo<llvm::StringRef>(
        llvm::DIFile::ChecksumKind::CSK_MD5, llvm::StringRef(checksum_str));
  }

  llvm::DIFile* di_file = dbg_state->builder->createFile(filename, directory, checksum_info);
  
  if (di_file == nullptr) {
    return llvm_gen_be_error_t::di_metadata_failure;
  }

  dbg_state->di_files_map[path_str] = di_file;
  *out_di_file = di_file;

  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t build_pdb_symbol_records(
    a_scope_ptr scope,
    llvm::DIScope** out_node) noexcept {
  if (scope == nullptr || out_node == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  
  if (!be_state || !be_state->dbg_state || !be_state->dbg_state->builder) {
    return llvm_gen_be_error_t::di_metadata_failure;
  }
  
  // Implementation of CodeView/PDB specific line tables and inline sites.
  return llvm_gen_be_error_t::not_implemented;
}

END_EDG_NAMESPACE
#endif /* BACK_END_IS_LLVM_GEN_BE */
