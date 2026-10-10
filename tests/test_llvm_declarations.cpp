/**
 * @file test_llvm_declarations.cpp
 * @brief Unit tests for the LLVM backend declarations and globals subsystem.
 * @details Validates function coverage of llvm_lower_global_variable,
 * llvm_lower_function_prototype, llvm_lower_function_body, and global ctors/dtors.
 */

#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_abi_aapcs64.h"
#include "llvm_gen_be_abi_sysv_x86_64.h"
#include "llvm_gen_be_debug.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

using namespace edg;

LLVMBackendState* edg::be_state = nullptr;

namespace edg {
  // Stubs
  a_boolean is_bool_type(a_type_ptr ty) { return FALSE; }
  a_targ_alignment f_alignment_of_type(a_type_ptr ty) { return 1; }
  a_boolean is_trivially_copyable_type(a_type_ptr ty) { return TRUE; }
  unsigned int targ_char_bit = 8;
  a_targ_size_t targ_sizeof_short = 2;
  a_targ_size_t targ_sizeof_int = 4;
  a_targ_size_t targ_sizeof_long = 8;
  a_targ_size_t targ_sizeof_long_long = 8;
  a_targ_size_t targ_sizeof_pointer = 8;
  a_boolean targ_little_endian = TRUE;
  a_C_dialect C_dialect = static_cast<a_C_dialect>(0);
  a_source_file_ptr conv_seq_to_file_and_line(a_seq_number, a_const_char**, a_const_char**, a_line_number*, a_boolean*) { return nullptr; }
  char* mangled_typeinfo_name(a_type_ptr type) { return (char*)"_ZTIi"; }
  a_type_qualifier_set f_get_type_qualifiers(a_type_ptr type, a_boolean x) { return 0; }
  a_boolean is_reference_type(a_type_ptr ty) { return FALSE; }
  llvm_gen_be_error_t evaluate_constant(a_constant_ptr con, llvm::Type* ty, llvm::Constant** out_const) noexcept { 
    if (ty->isIntegerTy()) *out_const = llvm::ConstantInt::get(ty, 1);
    else if (ty->isFloatingPointTy()) *out_const = llvm::ConstantFP::get(ty, 1.0);
    else *out_const = llvm::Constant::getNullValue(ty);
    return llvm_gen_be_error_t::ok; 
  }
  a_byte_boolean int_kind_is_signed[ik_last] = {0};
  
  llvm_gen_be_error_t get_llvm_type(a_type_ptr ty, llvm::Type** out_ty) noexcept {
    *out_ty = llvm::Type::getInt32Ty(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }
  
  void get_variable_initializer(a_variable_ptr var, a_scope_ptr scope, an_init_kind* kind, an_initializer_ptr* init) {
      *kind = initk_none;
      *init = nullptr;
  }
  
  llvm_gen_be_error_t build_aapcs64_function_type(a_type_ptr ty, llvm::FunctionType** out_ty, llvm::AttributeList* out_attrs) noexcept {
      *out_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false);
      return llvm_gen_be_error_t::ok;
  }
  
  llvm_gen_be_error_t build_sysv_function_type(a_type_ptr ty, llvm::FunctionType** out_ty, llvm::AttributeList* out_attrs) noexcept {
      *out_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false);
      return llvm_gen_be_error_t::ok;
  }
  
  llvm_gen_be_error_t compute_aapcs64_return_info(a_type_ptr ty, aapcs64_arg_info_t* out_info, bool* out_sret) noexcept {
      *out_sret = false;
      return llvm_gen_be_error_t::ok;
  }
  
  llvm_gen_be_error_t compute_sysv_return_info(a_type_ptr ty, x86_64_abi_arg_info_t* out_info, bool* out_sret) noexcept {
      *out_sret = false;
      return llvm_gen_be_error_t::ok;
  }
  
  
  a_scope_ptr scope_for_routine(a_routine_ptr r) { return nullptr; }
  
  llvm_gen_be_error_t llvm_lower_statement(a_statement_ptr stmt) noexcept {
    return llvm_gen_be_error_t::ok;
  }
  
  
  
  llvm_gen_be_error_t get_typeinfo_global(a_type_ptr type, llvm::Constant** out_ti) noexcept {
    *out_ti = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
    return llvm_gen_be_error_t::ok;
  }

  llvm_gen_be_error_t emit_dbg_declare_for_variable(llvm_gen_be_debug_state_t* dbg, a_variable_ptr var, llvm::AllocaInst* alloca_inst) noexcept {
    return llvm_gen_be_error_t::ok;
  }
  
  llvm_gen_be_error_t create_di_subprogram(llvm_gen_be_debug_state_t* dbg, a_routine_ptr routine, llvm::Function* fn, llvm::DISubprogram** out_subprogram) noexcept {
    return llvm_gen_be_error_t::ok;
  }
  
  llvm_gen_be_error_t push_lexical_block(llvm_gen_be_debug_state_t* dbg, a_source_position pos, llvm::DILexicalBlock** out_block) noexcept {
    return llvm_gen_be_error_t::ok;
  }
  
  llvm_gen_be_error_t pop_lexical_block(llvm_gen_be_debug_state_t* dbg) noexcept {
    return llvm_gen_be_error_t::ok;
  }
  
  an_il_header il_header = {};
}

int main() {
  printf("Running test_llvm_declarations...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_decl", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  a_scope root_scope = {};
  edg::il_header.primary_scope = &root_scope;

  // Test global variable
  a_variable gvar = {};
  gvar.source_corresp.name = "my_global";
  gvar.storage_class = sc_extern;
  root_scope.variables = &gvar;

  assert(llvm_lower_global_variables() == llvm_gen_be_error_t::ok);
  assert(state.module->getNamedGlobal("my_global") != nullptr);

  // Test function prototype
  a_routine fn = {};
  fn.source_corresp.name = "my_func";
  fn.storage_class = sc_extern;
  root_scope.routines = &fn;
  
  a_type fn_ty = {};
  fn_ty.kind = tk_routine;
  fn.type = &fn_ty;
  
  assert(llvm_lower_function_declarations() == llvm_gen_be_error_t::ok);
  assert(state.module->getFunction("my_func") != nullptr);

  // Test function body
  fn.function_def_number = 1;
  a_function_def_descr def_table[2] = {};
  edg::il_header.function_def_table = def_table;
  // Stub scope_for_routine returns nullptr, so body is empty block, which will be terminated with ret void.
  assert(llvm_lower_function_definitions() == llvm_gen_be_error_t::ok);
  
  // Test ctors
  fn.is_initialization_routine = TRUE;
  assert(llvm_lower_global_ctors_and_dtors() == llvm_gen_be_error_t::ok);
  assert(state.module->getNamedGlobal("llvm.global_ctors") != nullptr);

  printf("PASS\n");
  return 0;
}
