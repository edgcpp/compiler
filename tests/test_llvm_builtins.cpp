/**
 * @file test_llvm_builtins.cpp
 * @brief Unit tests for the LLVM backend compiler builtins subsystem.
 * @details Validates function coverage of builtin lowering in llvm_gen_be_expr.cpp.
 */

#include "llvm_gen_be_internal.h"
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
  llvm_gen_be_error_t evaluate_constant(a_constant_ptr con, llvm::Type* ty, llvm::Constant** out_const) noexcept { 
    if (ty->isIntegerTy()) *out_const = llvm::ConstantInt::get(ty, 1);
    else if (ty->isFloatingPointTy()) *out_const = llvm::ConstantFP::get(ty, 1.0);
    else *out_const = llvm::Constant::getNullValue(ty);
    return llvm_gen_be_error_t::ok; 
  }
  a_byte_boolean int_kind_is_signed[ik_last] = {0};
  
  
  
  llvm_gen_be_error_t get_llvm_type(a_type_ptr ty, llvm::Type** out_ty) noexcept {
    if (ty && ty->kind == tk_routine) {
        *out_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false);
    } else {
        *out_ty = llvm::Type::getInt32Ty(*be_state->context);
    }
    return llvm_gen_be_error_t::ok;
  }


  
  
  
  llvm_gen_be_error_t get_typeinfo_global(a_type_ptr type, llvm::Constant** out_ti) noexcept {
    *out_ti = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
    return llvm_gen_be_error_t::ok;
  }
  
  llvm_gen_be_error_t llvm_lower_statement(a_statement_ptr stmt) noexcept {
    return llvm_gen_be_error_t::ok;
  }

  llvm_gen_be_error_t build_aapcs64_function_type(a_type_ptr ty, llvm::FunctionType** out_ty, llvm::AttributeList* out_attrs) noexcept { return llvm_gen_be_error_t::ok; }
  llvm_gen_be_error_t build_sysv_function_type(a_type_ptr ty, llvm::FunctionType** out_ty, llvm::AttributeList* out_attrs) noexcept { return llvm_gen_be_error_t::ok; }
}

int main() {
  printf("Running test_llvm_builtins...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_builtins", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);

  // Test offsetof
  a_type int_ty = {}; int_ty.kind = tk_integer;
  an_expr_node offsetof_expr = {};
  offsetof_expr.kind = enk_builtin_operation;
  offsetof_expr.type = &int_ty;
  offsetof_expr.variant.builtin_operation.kind = bok_offsetof;
  llvm::Value* val = nullptr;
  assert(llvm_lower_expression(&offsetof_expr, &val) == llvm_gen_be_error_t::ok);

  // Test __builtin_expect call
  an_expr_node call_expr = {};
  call_expr.kind = enk_operation;
  call_expr.type = &int_ty;
  call_expr.variant.operation.kind = eok_call;
  an_expr_node op1 = {};
  op1.kind = enk_routine;
  a_routine r = {};
  r.source_corresp.name = "__builtin_expect";
  op1.variant.routine.ptr = &r;
  call_expr.variant.operation.operands = &op1;
  an_expr_node op2 = {};
  op2.kind = enk_constant;
  op1.next = &op2;
  an_expr_node op3 = {};
  op3.kind = enk_constant;
  op2.next = &op3;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);

  // Test __builtin_memcpy call
  bb = llvm::BasicBlock::Create(*state.context, "entry2", f);
  state.builder->SetInsertPoint(bb);
  r.source_corresp.name = "__builtin_memcpy";
  an_expr_node op4 = {};
  op4.kind = enk_constant;
  op3.next = &op4;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);

  printf("PASS\n");
  return 0;
}
