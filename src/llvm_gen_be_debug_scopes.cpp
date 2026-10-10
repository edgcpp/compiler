/**
 * @file llvm_gen_be_debug_scopes.cpp
 * @brief LLVM IR Backend Debug Scopes and Locations.
 * @details Implements source location mapping, subprogram creation, and lexical scope management.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_debug.h"
#include "llvm_gen_be_internal.h"
#include "il.h"

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

llvm_gen_be_error_t get_di_location(
    llvm_gen_be_debug_state_t* dbg_state,
    a_source_position src_pos,
    llvm::DIScope* scope,
    llvm::DILocation** out_loc) noexcept {
  
  if (dbg_state == nullptr || scope == nullptr || out_loc == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  a_const_char* file_name = nullptr;
  a_const_char* full_name = nullptr;
  a_line_number line = 0;
  a_boolean at_end_of_source = FALSE;

  if (src_pos.seq != 0) {
    conv_seq_to_file_and_line(src_pos.seq, &file_name, &full_name, &line, &at_end_of_source);
  }

  unsigned int column = src_pos.column;

  *out_loc = llvm::DILocation::get(*be_state->context, line, column, scope);

  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t apply_instruction_debug_loc(
    llvm::Instruction* inst,
    a_source_position src_pos) noexcept {
  
  if (inst == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  if (be_state->dbg_state == nullptr) {
    return llvm_gen_be_error_t::ok; // Debug info disabled
  }

  if (be_state->dbg_state->scope_stack.empty()) {
    return llvm_gen_be_error_t::ok;
  }

  llvm::DIScope* scope = be_state->dbg_state->scope_stack.back();
  llvm::DILocation* loc = nullptr;
  llvm_gen_be_error_t err = get_di_location(be_state->dbg_state, src_pos, scope, &loc);
  if (err != llvm_gen_be_error_t::ok) return err;

  inst->setDebugLoc(loc);
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t create_di_subprogram(
    llvm_gen_be_debug_state_t* dbg_state,
    a_routine_ptr routine,
    llvm::Function* fn,
    llvm::DISubprogram** out_subprogram) noexcept {

  if (dbg_state == nullptr || routine == nullptr || fn == nullptr || out_subprogram == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  a_const_char* file_name = nullptr;
  a_const_char* full_name = nullptr;
  a_line_number line = 0;
  a_boolean at_end_of_source = FALSE;

  if (routine->source_corresp.decl_position.seq != 0) {
    conv_seq_to_file_and_line(routine->source_corresp.decl_position.seq, &file_name, &full_name, &line, &at_end_of_source);
  }

  llvm::DIFile* di_file = nullptr;
  const char* path = full_name ? full_name : (file_name ? file_name : "");
  if (path[0] == '\0') {
    di_file = dbg_state->compile_unit->getFile();
  } else {
    llvm_gen_be_error_t err = get_or_create_di_file(dbg_state, path, &di_file);
    if (err != llvm_gen_be_error_t::ok) return err;
  }

  std::string name = routine->source_corresp.name ? routine->source_corresp.name : "";
  std::string linkage_name = fn->getName().str();

  llvm::DISubprogram::DISPFlags sp_flags = llvm::DISubprogram::SPFlagDefinition;
  if (routine->storage_class == sc_static) {
    sp_flags |= llvm::DISubprogram::SPFlagLocalToUnit;
  }
  if (name == "main") {
    sp_flags |= llvm::DISubprogram::SPFlagMainSubprogram;
  }

  llvm::DITypeArray type_array = dbg_state->builder->getOrCreateTypeArray(llvm::ArrayRef<llvm::Metadata*>());
  llvm::DISubroutineType* di_type = dbg_state->builder->createSubroutineType(type_array);

  *out_subprogram = dbg_state->builder->createFunction(
      di_file,
      name,
      linkage_name,
      di_file,
      line,
      di_type,
      line, // Scope line
      llvm::DINode::FlagZero,
      sp_flags
  );

  fn->setSubprogram(*out_subprogram);
  dbg_state->scope_stack.push_back(*out_subprogram);
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t push_lexical_block(
    llvm_gen_be_debug_state_t* dbg_state,
    a_source_position src_pos,
    llvm::DILexicalBlock** out_block) noexcept {
  
  if (dbg_state == nullptr || out_block == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  if (dbg_state->scope_stack.empty()) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  llvm::DIScope* parent_scope = dbg_state->scope_stack.back();
  
  a_const_char* file_name = nullptr;
  a_const_char* full_name = nullptr;
  a_line_number line = 0;
  a_boolean at_end_of_source = FALSE;

  if (src_pos.seq != 0) {
    conv_seq_to_file_and_line(src_pos.seq, &file_name, &full_name, &line, &at_end_of_source);
  }

  llvm::DIFile* di_file = nullptr;
  const char* path = full_name ? full_name : (file_name ? file_name : "");
  if (path[0] == '\0') {
    di_file = parent_scope->getFile();
  } else {
    llvm_gen_be_error_t err = get_or_create_di_file(dbg_state, path, &di_file);
    if (err != llvm_gen_be_error_t::ok) return err;
  }

  unsigned int column = src_pos.column;

  llvm::DILexicalBlock* block = dbg_state->builder->createLexicalBlock(
      parent_scope,
      di_file,
      line,
      column
  );

  dbg_state->scope_stack.push_back(block);
  *out_block = block;

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Pop lexical block.
 * @details Implements pop_lexical_block.
 * @return llvm_gen_be_error_t::ok on success.
  * @param[in] _p
 */
llvm_gen_be_error_t pop_lexical_block(llvm_gen_be_debug_state_t* dbg_state) noexcept {
  if (dbg_state == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  if (dbg_state->scope_stack.empty()) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  dbg_state->scope_stack.pop_back();
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif // BACK_END_IS_LLVM_GEN_BE
