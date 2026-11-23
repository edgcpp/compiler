#include "llvm_gen_be_internal.h"
/**
 * @file llvm_gen_be_debug.cpp
 * @ingroup llvm_gen_be_debug
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

  llvm_gen_be_debug_state_t* raw_dbg_state = nullptr;
  LLVM_GEN_BE_NEW(raw_dbg_state, llvm_gen_be_debug_state_t());
  std::unique_ptr<llvm_gen_be_debug_state_t> dbg_guard{raw_dbg_state};
  LLVM_GEN_BE_NEW(raw_dbg_state->builder, llvm::DIBuilder(*module));
  llvm_gen_be_debug_state_t* dbg_state = dbg_guard.get();

  // Setup module flags for DWARF or CodeView
  module->addModuleFlag(llvm::Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION);
  llvm::Triple triple = llvm::Triple(module->getTargetTriple());
  if (triple.isWindowsMSVCEnvironment()) {
    module->addModuleFlag(llvm::Module::Warning, "CodeView", 1);
    // LLVM CodeViewDebug backend automatically lowers DICompositeType into LF_STRUCTURE, LF_FIELDLIST, etc.
    dbg_state->is_codeview = true;
  } else {
    unsigned dwarf_version = triple.isOSBinFormatMachO() ? 4 : 5;
    module->addModuleFlag(llvm::Module::Warning, "Dwarf Version", dwarf_version);
    dbg_state->is_codeview = false;
  }

  llvm::DIFile* di_file = nullptr;
  LLVM_GEN_BE_CHECK(get_or_create_di_file(dbg_state, source_file, &di_file));

  std::string producer = "EDG C++ Front End with LLVM IR Backend";
  llvm::StringRef split_name = !be_state->split_dwarf_file.empty()
                                   ? llvm::StringRef(be_state->split_dwarf_file)
                                   : llvm::StringRef("");
  
  dbg_state->compile_unit = dbg_state->builder->createCompileUnit(
      llvm::dwarf::DW_LANG_C_plus_plus,
      di_file,
      producer,
      is_optimized,
      "", // Flags
      0,  // Runtime version
      split_name, // Split name
      llvm::DICompileUnit::DebugEmissionKind::FullDebug,
      0, // DWOId
      true, // SplitDebugInlining
      false, // DebugInfoForProfiling
      llvm::DICompileUnit::DebugNameTableKind::Default
  );

  *out_dbg_state = dbg_guard.release();
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
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());

  std::string path_str = std::string(file_path);
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

  dbg_state->di_files_map[path_str] = di_file;
  *out_di_file = di_file;

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Constructs CodeView/PDB lexical scope, subprogram (`S_GPROC32`/`S_LPROC32`),
 *        local variable (`S_REGREL32`), and inlined site metadata nodes for a scope.
 * @details Translates an EDG `a_scope_ptr` into the corresponding `llvm::DIScope*`
 *          and emits local variable debug records for CodeView/PDB emission.
 * @param[in] scope Pointer to the EDG scope entry.
 * @param[out] out_node Receives the constructed `llvm::DIScope*` metadata node.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_pdb_symbol_records(
    a_scope_ptr scope,
    llvm::DIScope** out_node) noexcept {
  if (scope == nullptr || out_node == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  if (!be_state || !be_state->dbg_state) {
    return llvm_gen_be_error_t::di_metadata_failure;
  }

  llvm_gen_be_debug_state_t* dbg = be_state->dbg_state;
  if (!be_state->module->getModuleFlag("CodeView")) {
    be_state->module->addModuleFlag(llvm::Module::Warning, "CodeView", 1);
  }
  dbg->is_codeview = true;

  llvm::DIFile* di_file = dbg->compile_unit->getFile();
  llvm::DIScope* parent_scope = !dbg->scope_stack.empty()
      ? dbg->scope_stack.back()
      : static_cast<llvm::DIScope*>(dbg->compile_unit);
  llvm::DIScope* result_scope = parent_scope;

  if (scope->kind == sck_function && scope->variant.routine.ptr) {
    a_routine_ptr r = scope->variant.routine.ptr;
    const char* fn_name = r->source_corresp.name ? r->source_corresp.name : "pdb_func";
    a_line_number fn_line = 1;
    if (r->source_corresp.decl_position.seq != 0) {
      a_const_char* f_name = nullptr;
      a_const_char* full_name = nullptr;
      a_boolean at_eos = FALSE;
      conv_seq_to_file_and_line(r->source_corresp.decl_position.seq, &f_name, &full_name, &fn_line, &at_eos);
    }
    llvm::DISubprogram::DISPFlags sp_flags = llvm::DISubprogram::SPFlagDefinition;
    if (r->storage_class == sc_static) {
      sp_flags |= llvm::DISubprogram::SPFlagLocalToUnit; // S_LPROC32 vs S_LPROC32
    }
    llvm::DISubroutineType* sub_ty = dbg->builder->createSubroutineType(
        dbg->builder->getOrCreateTypeArray(llvm::ArrayRef<llvm::Metadata*>()));
    llvm::DISubprogram* sp = dbg->builder->createFunction(
        di_file, fn_name, fn_name, di_file, fn_line, sub_ty, fn_line, llvm::DINode::FlagPrototyped, sp_flags);
    result_scope = sp;
  } else if (scope->kind == sck_block) {
    result_scope = dbg->builder->createLexicalBlock(parent_scope, di_file, 1, 1);
  } else if (scope->kind == sck_namespace && scope->variant.assoc_namespace) {
    const char* ns_name = scope->variant.assoc_namespace->source_corresp.name
        ? scope->variant.assoc_namespace->source_corresp.name
        : "";
    result_scope = dbg->builder->createNameSpace(parent_scope, ns_name, false);
  }

  // Emit S_REGREL32 / local variable records for variables in this scope
  for (a_variable_ptr v = scope->variables; v != nullptr; v = v->next) {
    if (v->type) {
      llvm::DIType* v_di_ty = nullptr;
      LLVM_GEN_BE_CHECK(get_or_create_di_type(dbg, v->type, &v_di_ty));
      const char* v_name = v->source_corresp.name ? v->source_corresp.name : "local";
      a_line_number v_line = 1;
      if (v->source_corresp.decl_position.seq != 0) {
        a_const_char* f_name = nullptr;
        a_const_char* full_name = nullptr;
        a_boolean at_eos = FALSE;
        conv_seq_to_file_and_line(v->source_corresp.decl_position.seq, &f_name, &full_name, &v_line, &at_eos);
      }
      (void)dbg->builder->createAutoVariable(result_scope, v_name, di_file, v_line, v_di_ty);
    }
  }
  for (a_variable_ptr v = scope->nonstatic_variables; v != nullptr; v = v->next) {
    if (v->type) {
      llvm::DIType* v_di_ty = nullptr;
      LLVM_GEN_BE_CHECK(get_or_create_di_type(dbg, v->type, &v_di_ty));
      const char* v_name = v->source_corresp.name ? v->source_corresp.name : "local";
      (void)dbg->builder->createAutoVariable(result_scope, v_name, di_file, 1, v_di_ty);
    }
  }

  *out_node = result_scope;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits DWARF macro debug entries (`DW_MACINFO_define` / `DW_MACINFO_undef`) into a `DIMacroFile`.
 * @details Creates a `DIMacroFile` under @p dbg_state and populates a `DIMacro` entry for
 *          @p macro_name and @p macro_value (`DW_MACINFO_define` when @p is_undef is false,
 *          or `DW_MACINFO_undef` when @p is_undef is true).
 * @param[in,out] dbg_state The active debug state.
 * @param[in] line_no Source line number of the macro directive.
 * @param[in] macro_name Null-terminated macro identifier.
 * @param[in] macro_value Optional macro expansion string (may be null).
 * @param[in] is_undef `true` to emit `DW_MACINFO_undef`, `false` for `DW_MACINFO_define`.
 * @param[out] out_macro Receives the created `DIMacro*` node.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if required pointers are null.
 */
llvm_gen_be_error_t emit_di_macro_info(
    llvm_gen_be_debug_state_t* dbg_state,
    unsigned line_no,
    const char* macro_name,
    const char* macro_value,
    bool is_undef,
    llvm::DIMacro** out_macro) noexcept {
  if (!dbg_state || !dbg_state->builder || !macro_name || !out_macro) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  llvm::DIMacroFile* mf = dbg_state->builder->createTempMacroFile(
      nullptr, 0, dbg_state->compile_unit->getFile());
  unsigned mac_type = is_undef ? llvm::dwarf::DW_MACINFO_undef : llvm::dwarf::DW_MACINFO_define;
  *out_macro = dbg_state->builder->createMacro(
      mf, line_no, mac_type, macro_name, macro_value ? macro_value : "");
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits `DW_TAG_imported_module` and `DW_TAG_imported_declaration` debug records for a scope.
 * @details Inspects `scope->namespaces` and `scope->routines` to emit `DIImportedEntity` metadata
 *          nodes in @p dbg_state and appends their count to `*out_count`.
 * @param[in,out] dbg_state The active debug state.
 * @param[in] scope The EDG scope to inspect.
 * @param[out] out_count Receives the number of imported entity debug nodes emitted.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if required pointers are null.
 */
llvm_gen_be_error_t emit_scope_imported_entities(
    llvm_gen_be_debug_state_t* dbg_state,
    a_scope_ptr scope,
    unsigned* out_count) noexcept {
  if (!dbg_state || !scope || !out_count) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  unsigned count = 0;
  llvm::DIScope* di_scope = dbg_state->compile_unit;
  llvm::DIFile* di_file = dbg_state->compile_unit->getFile();

  for (a_namespace_ptr ns = scope->namespaces; ns != nullptr; ns = ns->next) {
    const char* ns_name = ns->source_corresp.name ? ns->source_corresp.name : "ns";
    llvm::DINamespace* di_ns = nullptr;
    LLVM_GEN_BE_CHECK(createNameSpace(dbg_state, di_scope, ns_name, false, &di_ns));
    llvm::DIImportedEntity* imp = nullptr;
    LLVM_GEN_BE_CHECK(createImportedModule(dbg_state, di_scope, di_ns, di_file, 1, &imp));
    ++count;
  }

  *out_count = count;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits a DWARF `DICommonBlock` debug metadata node (`DW_TAG_common_block`).
 * @details Wraps `DIBuilder::createCommonBlock` to describe a named common storage block.
 * @param[in,out] dbg_state The active debug state.
 * @param[in] scope Enclosing lexical `DIScope*` (or `compile_unit` if null).
 * @param[in] decl `DIGlobalVariable*` declaration associated with the common block.
 * @param[in] name Null-terminated common block identifier.
 * @param[in] file `DIFile*` where the common block is declared.
 * @param[in] line_no 1-based source line number.
 * @param[out] out_block Receives the created `llvm::DICommonBlock*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if required pointers are null.
 */
llvm_gen_be_error_t emit_debug_common_block(
    llvm_gen_be_debug_state_t* dbg_state,
    llvm::DIScope* scope,
    llvm::DIGlobalVariable* decl,
    const char* name,
    llvm::DIFile* file,
    unsigned line_no,
    llvm::DICommonBlock** out_block) noexcept {
  if (!dbg_state || !dbg_state->builder || !name || !out_block) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  llvm::DIScope* target_scope = scope ? scope : static_cast<llvm::DIScope*>(dbg_state->compile_unit);
  llvm::DIFile* target_file = file ? file : dbg_state->compile_unit->getFile();
  *out_block = dbg_state->builder->createCommonBlock(
      target_scope, decl, name, target_file, line_no);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Constructs a chained `DILocation` metadata node for an inlined call site (`inlinedAt`).
 * @details Creates an `llvm::DILocation` at (`line`, `col`) inside `inline_scope` chained to `call_site_loc`.
 * @param[in] dbg_state The active debug state.
 * @param[in] line 1-based line number inside the inlined routine.
 * @param[in] col 1-based column number inside the inlined routine.
 * @param[in] inline_scope Lexical `DIScope*` of the inlined routine.
 * @param[in] call_site_loc Outer `DILocation*` representing the call site (`inlinedAt`).
 * @param[out] out_loc Receives the chained `llvm::DILocation*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if required pointers are null.
 */
llvm_gen_be_error_t create_inlined_at_debug_location(
    llvm_gen_be_debug_state_t* dbg_state,
    unsigned line,
    unsigned col,
    llvm::DIScope* inline_scope,
    llvm::DILocation* call_site_loc,
    llvm::DILocation** out_loc) noexcept {
  if (!dbg_state || !inline_scope || !call_site_loc || !out_loc) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  *out_loc = llvm::DILocation::get(*be_state->context, line, col, inline_scope, call_site_loc);
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif /* BACK_END_IS_LLVM_GEN_BE */
