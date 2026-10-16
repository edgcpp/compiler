/**
 * @file test_llvm_debug_info.cpp
 * @brief Unit tests for the LLVM backend debug info subsystem.
 * @details Validates function coverage of llvm_gen_be_debug.cpp,
 * llvm_gen_be_debug_scopes.cpp, and llvm_gen_be_debug_types.cpp.
 */

#include "llvm_gen_be_internal.h"
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
  char* mangled_typeinfo_name(a_type_ptr type) { return (char*)"_ZTIi"; }
  a_type_qualifier_set f_get_type_qualifiers(a_type_ptr type, a_boolean x) { return 0; }
  a_boolean is_reference_type(a_type_ptr ty) { return FALSE; }
  a_boolean is_empty_class_type(a_type_ptr ty) { return ty && ty->variant.class_struct_union.is_empty_class; }
  a_byte_boolean int_kind_is_signed[ik_last] = {0};
  
  an_il_header il_header = {};

  a_source_file_ptr conv_seq_to_file_and_line(a_seq_number, a_const_char** file, a_const_char**, a_line_number* line, a_boolean*) {
      *file = "test.cpp";
      *line = 1;
      return nullptr;
  }
}

int main() {
  printf("Running test_llvm_debug_info...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_debug", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm_gen_be_debug_state_t dbg_state;
  llvm_gen_be_error_t err = debug_info_init(&be_state->dbg_state, state.module.get(), "test.cpp", "edg", false);
  assert(err == llvm_gen_be_error_t::ok);

#if TARG_MICROSOFT
  assert(state.module->getModuleFlag("CodeView") != nullptr);
#else
  assert(state.module->getModuleFlag("Dwarf Version") != nullptr);
#endif

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);

  a_routine r = {};
  r.source_corresp.name = "test_func";
  llvm::DISubprogram* sp = nullptr;
  err = create_di_subprogram(be_state->dbg_state, &r, f, &sp);
  assert(err == llvm_gen_be_error_t::ok);

  llvm::DILexicalBlock* block = nullptr;
  a_source_position pos = {};
  err = push_lexical_block(be_state->dbg_state, pos, &block);
  assert(err == llvm_gen_be_error_t::ok);

  llvm::DILocation* loc = nullptr;
  err = get_di_location(be_state->dbg_state, pos, block, &loc);
  assert(err == llvm_gen_be_error_t::ok);

  llvm::AllocaInst* alloca = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));
  a_variable var = {};
  var.source_corresp.name = "my_var";
  a_type ty = {};
  ty.kind = tk_integer;
  var.type = &ty;
  err = emit_dbg_declare_for_variable(be_state->dbg_state, &var, alloca);
  assert(err == llvm_gen_be_error_t::ok);

  
  // Phase 8.1 Rich DWARF C++ Type Descriptors Tests
  llvm::DIType* base_di_ty = nullptr;
  err = get_or_create_di_type(be_state->dbg_state, &ty, &base_di_ty);
  assert(err == llvm_gen_be_error_t::ok);
  
  a_type derived_ty = {};
  derived_ty.kind = tk_class;
  derived_ty.size = 16;
  
  a_field dummy_field = {};
  dummy_field.is_bit_field = TRUE;
  dummy_field.bit_size = 3;
  dummy_field.offset = 4;
  dummy_field.type = &ty;
  derived_ty.variant.class_struct_union.field_list = &dummy_field;
  
  a_class_type_supplement dummy_supp = {};
  a_base_class dummy_base = {};
  dummy_base.type = &ty;
  dummy_base.is_virtual = TRUE;
  dummy_supp.direct_base_classes = &dummy_base;
  derived_ty.variant.class_struct_union.extra_info = &dummy_supp;

  llvm::DIType* derived_di_ty = nullptr;
  err = get_or_create_di_type(be_state->dbg_state, &derived_ty, &derived_di_ty);
  assert(err == llvm_gen_be_error_t::ok);
  
  llvm::DIDerivedType* inheritance = nullptr;
  err = createInheritance(be_state->dbg_state, derived_di_ty, base_di_ty, 0, 0, llvm::DINode::FlagZero, &inheritance);
  assert(err == llvm_gen_be_error_t::ok);
  
  llvm::DISubprogram* method = nullptr;
  llvm::DISubroutineType* method_ty = be_state->dbg_state->builder->createSubroutineType(
      be_state->dbg_state->builder->getOrCreateTypeArray(llvm::ArrayRef<llvm::Metadata*>()));
  err = createMethod(be_state->dbg_state, sp, "my_method", "_ZN1A9my_methodEv", be_state->dbg_state->compile_unit->getFile(), 
                     10, method_ty, 0, 0, nullptr, llvm::DINode::FlagZero, llvm::DISubprogram::SPFlagZero, &method);
  assert(err == llvm_gen_be_error_t::ok);
  
  llvm::DINamespace* ns = nullptr;
  err = createNameSpace(be_state->dbg_state, be_state->dbg_state->compile_unit, "my_namespace", true, &ns);
  assert(err == llvm_gen_be_error_t::ok);
  
  llvm::DITemplateTypeParameter* ttp = nullptr;
  err = createTemplateTypeParameter(be_state->dbg_state, ns, "T", base_di_ty, false, &ttp);
  assert(err == llvm_gen_be_error_t::ok);
  
  llvm::Metadata* tv_val = llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 42));
  llvm::DITemplateValueParameter* tvp = nullptr;
  err = createTemplateValueParameter(be_state->dbg_state, ns, "V", base_di_ty, false, tv_val, &tvp);
  assert(err == llvm_gen_be_error_t::ok);
  
  llvm::DICompositeType* enum_ty = nullptr;
  llvm::SmallVector<llvm::Metadata*, 1> elements;
  elements.push_back(be_state->dbg_state->builder->createEnumerator("A", 0, true));
  err = createEnumerationType(be_state->dbg_state, ns, "my_enum", be_state->dbg_state->compile_unit->getFile(), 20, 32, 32, 
                              be_state->dbg_state->builder->getOrCreateArray(elements), base_di_ty, "_ZTI7my_enum", true, &enum_ty);
  assert(err == llvm_gen_be_error_t::ok);

uint64_t expr_ops[] = { llvm::dwarf::DW_OP_deref, llvm::dwarf::DW_OP_plus_uconst, 4 };
assert(emit_dbg_value_for_variable(be_state->dbg_state, &var, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 1), alloca, nullptr, expr_ops, 3) == llvm_gen_be_error_t::ok);

assert(emit_dbg_assign_for_variable(be_state->dbg_state, &var, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 1), alloca, alloca) == llvm_gen_be_error_t::ok);

err = pop_lexical_block(be_state->dbg_state);
assert(err == llvm_gen_be_error_t::ok);

  err = debug_info_finalize(be_state->dbg_state);
  assert(err == llvm_gen_be_error_t::ok);

  printf("PASS\n");
  return 0;
}
