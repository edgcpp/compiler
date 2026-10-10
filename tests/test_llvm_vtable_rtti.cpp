/**
 * @file test_llvm_vtable_rtti.cpp
 * @brief Unit tests for the LLVM backend vtable and RTTI generation subsystem.
 */

#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_rtti.h"
#include "llvm_gen_be_vtable.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

using namespace edg;

LLVMBackendState* edg::be_state = nullptr;

namespace edg {
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
  
  char* mangled_typeinfo_name(a_type_ptr type) {
      if (type->kind == tk_class || type->kind == tk_struct) return (char*)"_ZTI1C";
      if (type->kind == tk_pointer) return (char*)"_ZTIPi";
      return (char*)"_ZTIi"; 
  }
  
  a_type_qualifier_set f_get_type_qualifiers(a_type_ptr type, a_boolean x) { return 0; }
  a_boolean is_reference_type(a_type_ptr ty) { return FALSE; }
  a_byte_boolean int_kind_is_signed[ik_last] = {0};
  
}

int main() {
  printf("Running test_llvm_vtable_rtti...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_rtti", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);

  a_type basic_ty = {};
  basic_ty.kind = tk_integer;
  llvm::Constant* ti = nullptr;
  assert(get_typeinfo_global(&basic_ty, &ti) == llvm_gen_be_error_t::ok);

  a_type ptr_ty = {};
  ptr_ty.kind = tk_pointer;
  ptr_ty.variant.pointer.type = &basic_ty;
  assert(get_typeinfo_global(&ptr_ty, &ti) == llvm_gen_be_error_t::ok);

  a_type class_ty = {};
  class_ty.kind = tk_class;
  a_class_type_supplement extra = {};
  class_ty.variant.class_struct_union.extra_info = &extra;
  assert(get_typeinfo_global(&class_ty, &ti) == llvm_gen_be_error_t::ok);

  a_base_class base = {};
  base.type = &basic_ty;
  extra.direct_base_classes = &base;
  base.is_virtual = FALSE;
  assert(get_typeinfo_global(&class_ty, &ti) == llvm_gen_be_error_t::ok);

  a_base_class base2 = {};
  base2.type = &basic_ty;
  base.next_direct = &base2;
  assert(get_typeinfo_global(&class_ty, &ti) == llvm_gen_be_error_t::ok);

  llvm::Constant* vtable = nullptr;
  assert(llvm_emit_vtable(&class_ty, &vtable) == llvm_gen_be_error_t::ok);

  llvm::Value* obj = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
  a_routine routine = {};
  llvm::Value* func_ptr = nullptr;
  assert(llvm_lower_virtual_dispatch(obj, &routine, &func_ptr) == llvm_gen_be_error_t::ok);

  printf("PASS\n");
  return 0;
}
