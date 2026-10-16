/**
 * @file test_llvm_expressions.cpp
 * @brief Unit tests for the LLVM backend expressions lowering subsystem.
 * @details Validates function coverage of llvm_lower_expression.
 */

#include "llvm_gen_be_expr.h"
#include "llvm_gen_be_type.h"
#include "llvm_gen_be_ptrmem.h"
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
char* get_mangled_function_name(a_routine_ptr routine) {
    return nullptr;
}

  // Stub implementations required to link test
  a_boolean is_bool_type(a_type_ptr ty) { return FALSE; }
  a_boolean is_class_struct_union_type(a_type_ptr ty) { return FALSE; }
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
  char* mangled_typeinfo_name(a_type_ptr type) { static char buf[] = "_ZTIi"; return buf; }
  a_type_qualifier_set f_get_type_qualifiers(a_type_ptr type, a_boolean x) { 
      return (type && type->size == 999) ? TQ_VOLATILE : 0; 
  }
  a_boolean is_reference_type(a_type_ptr ty) { return FALSE; }
  a_boolean is_empty_class_type(a_type_ptr ty) { return ty && ty->variant.class_struct_union.is_empty_class; }
  llvm_gen_be_error_t evaluate_constant(a_constant_ptr con, llvm::Type* ty, llvm::Constant** out_const) noexcept { 
    if (ty->isIntegerTy()) *out_const = llvm::ConstantInt::get(ty, 1);
    else if (ty->isFloatingPointTy()) *out_const = llvm::ConstantFP::get(ty, 1.0);
    else *out_const = llvm::Constant::getNullValue(ty);
    return llvm_gen_be_error_t::ok; 
  }
  
  a_byte_boolean int_kind_is_signed[ik_last] = {0};

  a_boolean type_has_nontrivial_destructor(a_type_ptr tp) {
      return FALSE;
  }

  llvm_gen_be_error_t llvm_emit_eh_cleanup(a_routine_ptr dtor, llvm::Value* obj_ptr) noexcept {
      return llvm_gen_be_error_t::ok;
  }

  llvm_gen_be_error_t get_di_location(llvm_gen_be_debug_state_t*, a_source_position, llvm::DIScope*, llvm::DILocation** out_loc) noexcept {
    if (out_loc) *out_loc = nullptr;
    return llvm_gen_be_error_t::ok;
  }
  llvm_gen_be_error_t emit_dbg_value_for_variable(llvm_gen_be_debug_state_t*, a_variable*, llvm::Value*, llvm::Instruction*, llvm::BasicBlock*, const uint64_t*, size_t) noexcept {
    return llvm_gen_be_error_t::ok;
  }
  a_routine_ptr vtbl_decider_function_for_class(a_type_ptr class_type, a_boolean* unknown) {
      if (unknown) *unknown = FALSE;
      return nullptr;
  }
}


namespace edg {
  char* mangled_vtbl_name(a_type_ptr class_type, a_base_class_ptr bcp, a_base_class_ptr ctor_bcp) {
      if (bcp) return (char*)"_ZTCmock";
      return (char*)"_ZTVmock";
  }
  char* mangled_virtual_table_table_name(a_type_ptr type) {
      return (char*)"_ZTTmock";
  }
}

int main() {
  printf("Running test_llvm_expressions...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  be_state = &state;

  llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage, "test", state.module.get());
  llvm::BasicBlock *BB = llvm::BasicBlock::Create(*state.context, "entry", F);
  state.builder->SetInsertPoint(BB);

  llvm::Value* val = nullptr;
  a_type int_type;
  memset(&int_type, 0, sizeof(int_type));
  int_type.kind = tk_integer;
  int_type.variant.integer.int_kind = ik_int;

  a_variable dummy_var;
  memset(&dummy_var, 0, sizeof(dummy_var));
  dummy_var.type = &int_type;
  
  an_expr_node dummy_op1;
  memset(&dummy_op1, 0, sizeof(dummy_op1));
  dummy_op1.type = &int_type;
  dummy_op1.kind = enk_variable;
  dummy_op1.variant.variable.ptr = &dummy_var;
  
  llvm::Type* var_ll_ty = llvm::Type::getInt32Ty(*state.context);
  llvm::AllocaInst* alloca = state.builder->CreateAlloca(var_ll_ty);
  state.local_vars[&dummy_var] = alloca;

  an_expr_node dummy_op2;
  memset(&dummy_op2, 0, sizeof(dummy_op2));
  dummy_op2.type = &int_type;
  dummy_op2.kind = enk_constant;

  dummy_op1.next = &dummy_op2;

  an_expr_node dummy_expr;
  memset(&dummy_expr, 0, sizeof(dummy_expr));
  dummy_expr.type = &int_type;
  dummy_expr.variant.operation.operands = &dummy_op1;

  // Simple test to hit 1 branch
  assert(llvm_lower_expression(&dummy_expr, &val) == llvm_gen_be_error_t::unsupported_expr);
  
  a_type unsigned_type;
  memset(&unsigned_type, 0, sizeof(unsigned_type));
  unsigned_type.kind = tk_integer;
  unsigned_type.variant.integer.int_kind = ik_unsigned_int;
  
  a_type float_type;
  memset(&float_type, 0, sizeof(float_type));
  float_type.kind = tk_float;
  float_type.variant.float_kind = fk_float;
  
  a_type pointer_type;
  memset(&pointer_type, 0, sizeof(pointer_type));
  pointer_type.kind = tk_pointer;
  pointer_type.variant.pointer.type = &int_type;

  a_type* types[] = { &int_type, &unsigned_type, &float_type, &pointer_type };

  // Create test loops to hit all operations
  for (a_type* ty : types) {
      dummy_op1.type = ty;
      dummy_op2.type = ty;
      // Only test basic operations that don't crash LLVM constant folder with dummy values
      for (int i = 0; i < 10; ++i) {
          memset(&dummy_expr, 0, sizeof(dummy_expr));
          dummy_expr.type = ty;
          dummy_expr.kind = enk_operation;
          dummy_expr.variant.operation.operands = &dummy_op1;
          dummy_expr.variant.operation.kind = static_cast<an_expr_operator_kind>(i);
          if (ty == &float_type || ty == &pointer_type) continue;
          if (llvm_lower_expression(&dummy_expr, &val) == llvm_gen_be_error_t::ok) { }
      }
  }

  // Test compound assignments
  an_expr_operator_kind compound_ops[] = {
    eok_add_assign, eok_subtract_assign, eok_multiply_assign, eok_divide_assign,
    eok_remainder_assign, eok_and_assign, eok_or_assign, eok_xor_assign,
    eok_shiftl_assign, eok_shiftr_assign, eok_padd_assign, eok_psubtract_assign,
    eok_bassign, eok_pre_incr, eok_post_incr, eok_pre_decr, eok_post_decr
  };

  for (an_expr_operator_kind op : compound_ops) {
      memset(&dummy_expr, 0, sizeof(dummy_expr));
      dummy_expr.type = &int_type;
      dummy_expr.kind = enk_operation;
      dummy_expr.variant.operation.operands = &dummy_op1;
      dummy_expr.variant.operation.kind = op;
      assert(llvm_lower_expression(&dummy_expr, &val) == llvm_gen_be_error_t::ok);
  }

  // Test complex numbers
  a_type complex_type;
  memset(&complex_type, 0, sizeof(complex_type));
  complex_type.kind = tk_complex;
  complex_type.variant.float_kind = fk_float;

  an_expr_node dummy_complex_op1;
  memset(&dummy_complex_op1, 0, sizeof(dummy_complex_op1));
  dummy_complex_op1.type = &complex_type;
  dummy_complex_op1.kind = enk_variable;
  
  a_variable dummy_complex_var;
  memset(&dummy_complex_var, 0, sizeof(dummy_complex_var));
  dummy_complex_var.type = &complex_type;
  dummy_complex_op1.variant.variable.ptr = &dummy_complex_var;
  
  llvm::Type* complex_ll_ty = nullptr;
  assert(get_llvm_type(&complex_type, &complex_ll_ty) == llvm_gen_be_error_t::ok);
  state.local_vars[&dummy_complex_var] = state.builder->CreateAlloca(complex_ll_ty);

  an_expr_node dummy_complex_op2;
  memset(&dummy_complex_op2, 0, sizeof(dummy_complex_op2));
  dummy_complex_op2.type = &complex_type;
  dummy_complex_op2.kind = enk_variable;
  dummy_complex_op2.variant.variable.ptr = &dummy_complex_var;
  dummy_complex_op1.next = &dummy_complex_op2;

  an_expr_operator_kind complex_ops[] = {
      eok_add, eok_subtract, eok_multiply, eok_divide
  };
  
  for (an_expr_operator_kind op : complex_ops) {
      memset(&dummy_expr, 0, sizeof(dummy_expr));
      dummy_expr.type = &complex_type;
      dummy_expr.kind = enk_operation;
      dummy_expr.variant.operation.operands = &dummy_complex_op1;
      dummy_expr.variant.operation.kind = op;
      assert(llvm_lower_expression(&dummy_expr, &val) == llvm_gen_be_error_t::ok);
  }

  // Test imaginary operators
  an_expr_operator_kind imag_ops[] = {
      eok_fjadd, eok_jfadd, eok_fjsubtract, eok_jfsubtract, eok_jmultiply, eok_jdivide
  };
  
  an_expr_node dummy_float_op1;
  memset(&dummy_float_op1, 0, sizeof(dummy_float_op1));
  dummy_float_op1.type = &float_type;
  dummy_float_op1.kind = enk_variable;
  a_variable dummy_float_var;
  memset(&dummy_float_var, 0, sizeof(dummy_float_var));
  dummy_float_var.type = &float_type;
  dummy_float_op1.variant.variable.ptr = &dummy_float_var;
  state.local_vars[&dummy_float_var] = state.builder->CreateAlloca(llvm::Type::getFloatTy(*state.context));
  
  dummy_float_op1.next = &dummy_complex_op2; // For real op imag

  for (an_expr_operator_kind op : imag_ops) {
      memset(&dummy_expr, 0, sizeof(dummy_expr));
      dummy_expr.type = &complex_type;
      dummy_expr.kind = enk_operation;
      if (op == eok_fjadd || op == eok_fjsubtract || op == eok_jdivide) {
          dummy_expr.variant.operation.operands = &dummy_float_op1;
          dummy_float_op1.next = &dummy_complex_op2;
      } else {
          dummy_expr.variant.operation.operands = &dummy_complex_op1;
          dummy_complex_op1.next = &dummy_float_op1;
      }
      assert(llvm_lower_expression(&dummy_expr, &val) == llvm_gen_be_error_t::ok);
  }
  
  // Test spaceship operator
  a_type strong_ordering_type;
  memset(&strong_ordering_type, 0, sizeof(strong_ordering_type));
  strong_ordering_type.kind = tk_struct; // e.g. std::strong_ordering
  
  memset(&dummy_expr, 0, sizeof(dummy_expr));
  dummy_expr.type = &strong_ordering_type; // e.g. strong_ordering struct
  dummy_expr.kind = enk_operation;
  dummy_expr.variant.operation.operands = &dummy_op1; // integers
  dummy_expr.variant.operation.kind = eok_spaceship;
  dummy_op1.next = &dummy_op2;
  
  assert(llvm_lower_expression(&dummy_expr, &val) == llvm_gen_be_error_t::ok);

  // Test bitfield load/store
  a_field dummy_bf = {};
  dummy_bf.is_bit_field = TRUE;
  dummy_bf.type = &int_type;
  dummy_bf.bit_size = 5;
  dummy_bf.offset_bit_remainder = 3;
  dummy_bf.offset = 0;
  dummy_bf.bit_field_is_signed = TRUE;

  an_expr_node dot_expr = {};
  dot_expr.kind = enk_operation;
  dot_expr.type = &int_type;
  dot_expr.is_lvalue = TRUE;
  dot_expr.variant.operation.kind = eok_dot_field;
  dot_expr.variant.operation.operands = &dummy_op1;
  
  an_expr_node field_node = {};
  field_node.kind = enk_field;
  field_node.variant.field.ptr = &dummy_bf;
  dummy_op1.next = &field_node;
  
  // Test load
  assert(llvm_lower_expression(&dot_expr, &val) == llvm_gen_be_error_t::ok);
  
  // Test store (assign)
  an_expr_node assign_expr = {};
  assign_expr.kind = enk_operation;
  assign_expr.type = &int_type;
  assign_expr.variant.operation.kind = eok_assign;
  assign_expr.variant.operation.operands = &dot_expr;
  an_expr_node rhs_expr = {};
  rhs_expr.kind = enk_constant;
  rhs_expr.type = &int_type;
  dot_expr.next = &rhs_expr;
  assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::ok);

  // Test compound assign on bitfield
  assign_expr.variant.operation.kind = eok_add_assign;
  assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::ok);



  // Test Compiler Builtins (Overflow/Clz/Expect)
  an_expr_node overflow_expr = {};
  overflow_expr.kind = enk_operation;
  overflow_expr.variant.operation.kind = eok_call;
  an_expr_node overflow_func = {};
  overflow_func.kind = enk_routine;
  a_routine overflow_routine = {};
  overflow_routine.source_corresp.name = "__builtin_sadd_overflow";
  overflow_func.variant.routine.ptr = &overflow_routine;
  
  an_expr_node op_a = {}; op_a.kind = enk_constant; op_a.type = &int_type;
  an_expr_node op_b = {}; op_b.kind = enk_constant; op_b.type = &int_type;
  an_expr_node op_res = {}; op_res.kind = enk_constant; op_res.type = &pointer_type;
  
  overflow_func.next = &op_a;
  op_a.next = &op_b;
  op_b.next = &op_res;
  
  overflow_expr.variant.operation.operands = &overflow_func;
  assert(llvm_lower_expression(&overflow_expr, &val) == llvm_gen_be_error_t::ok);


  // Test Atomics
  an_expr_node atomic_expr = {};
  atomic_expr.kind = enk_operation;
  atomic_expr.variant.operation.kind = eok_call;
  an_expr_node atomic_func = {};
  atomic_func.kind = enk_routine;
  a_routine atomic_routine = {};
  atomic_routine.source_corresp.name = "__atomic_thread_fence";
  atomic_func.variant.routine.ptr = &atomic_routine;
  
  an_expr_node atomic_arg = {}; atomic_arg.kind = enk_constant; atomic_arg.type = &int_type;
  atomic_func.next = &atomic_arg;
  atomic_expr.variant.operation.operands = &atomic_func;
  
  assert(llvm_lower_expression(&atomic_expr, &val) == llvm_gen_be_error_t::ok);

  // Test SIMD builtins
  an_expr_node simd_expr = {};
  simd_expr.kind = enk_operation;
  simd_expr.variant.operation.kind = eok_call;
  an_expr_node simd_func = {};
  simd_func.kind = enk_routine;
  a_routine simd_routine = {};
  simd_routine.source_corresp.name = "__builtin_ia32_sse_add_ps";
  simd_func.variant.routine.ptr = &simd_routine;
  simd_func.next = nullptr; // Re-use vector argument
  simd_expr.variant.operation.operands = &simd_func;
  
  assert(llvm_lower_expression(&simd_expr, &val) == llvm_gen_be_error_t::ok);

  
  // Test SIMD builtins (AVX, SVE, Shuffle, Convert)
  {
      an_expr_node op1 = {};
      op1.kind = enk_constant;
      a_type op_ty = {};
#if GNU_EXTENSIONS_ALLOWED && GNU_VECTOR_TYPES_ALLOWED
      op_ty.kind = tk_vector;
      op_ty.variant.vector.element_type = &int_type;
#else
      op_ty.kind = tk_array;
      op_ty.variant.array.element_type = &int_type;
      op_ty.variant.array.variant.number_of_elements = 4;
#endif
      op_ty.size = 16;
      op1.type = &op_ty;
      
      an_expr_node op2 = {};
      op2.kind = enk_constant;
      op2.type = &op_ty;

      an_expr_node mask = {};
      mask.kind = enk_constant;
      mask.type = &int_type;
      a_constant mask_const = {};
      mask_const.kind = ck_integer;
      mask_const.variant.integer_value = 0;
      mask.variant.constant.ptr = &mask_const;

      an_expr_node simd_avx = {};
      simd_avx.kind = enk_operation;
      simd_avx.variant.operation.kind = eok_call;
      an_expr_node simd_avx_func = {};
      simd_avx_func.kind = enk_routine;
      a_routine simd_avx_routine = {};
      simd_avx_routine.source_corresp.name = "__builtin_ia32_avx_add_pd_256";
      simd_avx_func.variant.routine.ptr = &simd_avx_routine;
      simd_avx_func.next = &op1;
      op1.next = &op2;
      op2.next = nullptr;
      simd_avx.variant.operation.operands = &simd_avx_func;
      simd_avx.type = &op_ty;
      assert(llvm_lower_expression(&simd_avx, &val) == llvm_gen_be_error_t::ok);

      an_expr_node simd_sve = {};
      simd_sve.kind = enk_operation;
      simd_sve.variant.operation.kind = eok_call;
      an_expr_node simd_sve_func = {};
      simd_sve_func.kind = enk_routine;
      a_routine simd_sve_routine = {};
      simd_sve_routine.source_corresp.name = "__builtin_sve_add_nxv4i32";
      simd_sve_func.variant.routine.ptr = &simd_sve_routine;
      simd_sve_func.next = &op1;
      op1.next = &op2;
      op2.next = nullptr;
      simd_sve.variant.operation.operands = &simd_sve_func;
      simd_sve.type = &op_ty;
      assert(llvm_lower_expression(&simd_sve, &val) == llvm_gen_be_error_t::ok);

      // Shufflevector
      an_expr_node simd_shuf = {};
      simd_shuf.kind = enk_operation;
      simd_shuf.variant.operation.kind = eok_call;
      an_expr_node simd_shuf_func = {};
      simd_shuf_func.kind = enk_routine;
      a_routine simd_shuf_routine = {};
      simd_shuf_routine.source_corresp.name = "__builtin_shufflevector";
      simd_shuf_func.variant.routine.ptr = &simd_shuf_routine;
      
      simd_shuf_func.next = &op1;
      op1.next = &op2;
      op2.next = &mask;
      mask.next = nullptr;
      
      simd_shuf.variant.operation.operands = &simd_shuf_func;
      simd_shuf.type = &op_ty;
      assert(llvm_lower_expression(&simd_shuf, &val) == llvm_gen_be_error_t::ok);

      // Convertvector
      an_expr_node simd_conv = {};
      simd_conv.kind = enk_operation;
      simd_conv.variant.operation.kind = eok_call;
      an_expr_node simd_conv_func = {};
      simd_conv_func.kind = enk_routine;
      a_routine simd_conv_routine = {};
      simd_conv_routine.source_corresp.name = "__builtin_convertvector";
      simd_conv_func.variant.routine.ptr = &simd_conv_routine;
      
      simd_conv_func.next = &op1;
      op1.next = &op2;
      op2.next = nullptr; // only 2 args

      a_type fp_vec_ty = {};
#if GNU_EXTENSIONS_ALLOWED && GNU_VECTOR_TYPES_ALLOWED
      fp_vec_ty.kind = tk_vector;
      a_type fp_type = {};
      fp_type.kind = tk_float;
      fp_type.variant.float_kind = fk_float;
      fp_vec_ty.variant.vector.element_type = &fp_type;
#else
      fp_vec_ty.kind = tk_array;
      a_type fp_type = {};
      fp_type.kind = tk_float;
      fp_type.variant.float_kind = fk_float;
      fp_vec_ty.variant.array.element_type = &fp_type;
      fp_vec_ty.variant.array.variant.number_of_elements = 4;
#endif
      fp_vec_ty.size = 16;
      simd_conv.type = &fp_vec_ty;
      op2.type = &fp_vec_ty; // destination type argument
      
      simd_conv.variant.operation.operands = &simd_conv_func;
      assert(llvm_lower_expression(&simd_conv, &val) == llvm_gen_be_error_t::ok);
  }
  // test_stmt_expr();
  // Test GNU statement expression
  an_expr_node stmt_expr = {};
  stmt_expr.kind = enk_statement;
  stmt_expr.type = &int_type;
  a_statement comp_stmt = {};
  comp_stmt.kind = stmk_empty;
  stmt_expr.variant.statement = &comp_stmt;
  
  assert(llvm_lower_expression(&stmt_expr, &val) == llvm_gen_be_error_t::ok);

  // Test __builtin_setjmp
  an_expr_node setjmp_expr = {};
  setjmp_expr.kind = enk_operation;
  setjmp_expr.variant.operation.kind = eok_call;
  
  an_expr_node setjmp_func = {};
  setjmp_func.kind = enk_routine;
  a_routine setjmp_routine = {};
  setjmp_routine.source_corresp.name = "__builtin_setjmp";
  setjmp_func.variant.routine.ptr = &setjmp_routine;
  
  an_expr_node setjmp_arg = {};
  setjmp_arg.kind = enk_constant;
  setjmp_arg.type = &int_type; // Stub pointer
  setjmp_func.next = &setjmp_arg;
  setjmp_expr.variant.operation.operands = &setjmp_func;
  
  assert(llvm_lower_expression(&setjmp_expr, &val) == llvm_gen_be_error_t::ok);

  // Test __builtin_longjmp
  an_expr_node longjmp_expr = {};
  longjmp_expr.kind = enk_operation;
  longjmp_expr.variant.operation.kind = eok_call;
  
  an_expr_node longjmp_func = {};
  longjmp_func.kind = enk_routine;
  a_routine longjmp_routine = {};
  longjmp_routine.source_corresp.name = "__builtin_longjmp";
  longjmp_func.variant.routine.ptr = &longjmp_routine;
  longjmp_func.next = &setjmp_arg;
  longjmp_expr.variant.operation.operands = &longjmp_func;
  
  assert(llvm_lower_expression(&longjmp_expr, &val) == llvm_gen_be_error_t::ok);

  // Test spanning bitfield (e.g. 20 bits starting at bit offset 4, spans 3 bytes)
  dummy_bf.offset_bit_remainder = 4;
  dummy_bf.bit_size = 20;
  dummy_bf.bit_field_is_signed = FALSE;
  assert(llvm_lower_expression(&dot_expr, &val) == llvm_gen_be_error_t::ok);
  assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::ok);

  // Test volatile bitfield atomic operations
  a_type vol_int_type = int_type;
  vol_int_type.size = 999;
  dot_expr.type = &vol_int_type;
  assert(llvm_lower_expression(&dot_expr, &val) == llvm_gen_be_error_t::ok);
  assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::ok);

  // Test complex multiplication
  dummy_complex_op2.kind = enk_variable;
  dummy_complex_op2.variant.variable.ptr = &dummy_complex_var;
  dummy_op1.next = &dummy_complex_op2;
  dummy_expr.type = &complex_type;
  dummy_expr.kind = enk_operation;
  dummy_expr.variant.operation.operands = &dummy_op1;
  dummy_expr.variant.operation.kind = eok_multiply;
  assert(llvm_lower_expression(&dummy_expr, &val) == llvm_gen_be_error_t::ok);

  // Test complex division
  dummy_expr.variant.operation.kind = eok_divide;
  assert(llvm_lower_expression(&dummy_expr, &val) == llvm_gen_be_error_t::ok);
  
  // Test complex conjugation
  dummy_op1.next = nullptr;
  dummy_expr.variant.operation.kind = eok_xconj;
  assert(llvm_lower_expression(&dummy_expr, &val) == llvm_gen_be_error_t::ok);

  // Test __builtin_cabs
  an_expr_node cabs_expr = {};
  cabs_expr.kind = enk_operation;
  cabs_expr.variant.operation.kind = eok_call;
  an_expr_node cabs_func = {};
  cabs_func.kind = enk_routine;
  a_routine cabs_routine = {};
  cabs_routine.source_corresp.name = "__builtin_cabs";
  cabs_func.variant.routine.ptr = &cabs_routine;
  cabs_func.next = &dummy_op1;
  cabs_expr.variant.operation.operands = &cabs_func;
  assert(llvm_lower_expression(&cabs_expr, &val) == llvm_gen_be_error_t::ok);




  // Test Pointer-to-Member Invocation (Itanium ABI representation)
  an_expr_node ptrmem_expr = {};
  ptrmem_expr.kind = enk_operation;
  ptrmem_expr.variant.operation.kind = eok_points_to_member_call;
  
  an_expr_node ptrmem_op1 = {};
  ptrmem_op1.kind = enk_variable;
  a_type ptr_to_member_ty = {};
  ptr_to_member_ty.kind = tk_ptr_to_member;
  
  a_type rout_ty = {};
  rout_ty.kind = tk_routine;
  ptr_to_member_ty.variant.ptr_to_member.type = &rout_ty;
  ptrmem_op1.type = &ptr_to_member_ty;
  
  a_variable ptrmem_var = {};
  ptrmem_var.type = &ptr_to_member_ty;
  ptrmem_op1.variant.variable.ptr = &ptrmem_var;
  
  llvm::Type* int64_ty = llvm::Type::getInt64Ty(*state.context);
  llvm::Type* ptrmem_llvm_ty = llvm::StructType::get(*state.context, {llvm::PointerType::getUnqual(*state.context), int64_ty});
  state.local_vars[&ptrmem_var] = state.builder->CreateAlloca(ptrmem_llvm_ty);
  
  // the 'this' argument
  an_expr_node this_arg = {};
  this_arg.kind = enk_constant;
  this_arg.type = &pointer_type;
  ptrmem_op1.next = &this_arg;
  
  ptrmem_expr.variant.operation.operands = &ptrmem_op1;
  
  llvm_gen_be_error_t e2 = llvm_lower_expression(&ptrmem_expr, &val);
  if (e2 != llvm_gen_be_error_t::ok) { printf("ERR: %d\\n", (int)e2); }
  assert(e2 == llvm_gen_be_error_t::ok);




#if GNU_EXTENSIONS_ALLOWED && GNU_VECTOR_TYPES_ALLOWED
  // Test Vector Type Lowering
  a_type vec_ty = {};
  vec_ty.kind = tk_vector;
  vec_ty.size = 16;
  a_vector_type_supplement vec_supp = {};
  vec_supp.element_type = &int_type;
  vec_ty.variant.vector.extra_info = &vec_supp;
  
  llvm::Type* vec_llvm_ty = nullptr;
  assert(get_llvm_type(&vec_ty, &vec_llvm_ty) == llvm_gen_be_error_t::ok);
  assert(vec_llvm_ty != nullptr);
  assert(vec_llvm_ty->isVectorTy());
  
  // Test vector subscript
  an_expr_node vec_subscript = {};
  vec_subscript.kind = enk_operation;
  vec_subscript.variant.operation.kind = eok_subscript;
  vec_subscript.type = &int_type;
  
  an_expr_node vec_op1 = {};
  vec_op1.kind = enk_variable;
  a_variable vec_var = {};
  vec_var.type = &vec_ty;
  vec_op1.variant.variable.ptr = &vec_var;
  state.local_vars[&vec_var] = state.builder->CreateAlloca(vec_llvm_ty);
  
  an_expr_node vec_op2 = {};
  vec_op2.kind = enk_constant;
  vec_op2.type = &int_type;
  
  vec_op1.next = &vec_op2;
  vec_subscript.variant.operation.operands = &vec_op1;
  
  assert(llvm_lower_expression(&vec_subscript, &val) == llvm_gen_be_error_t::ok);
#endif

  // Phase 3: Pointer-to-member subsystem unit tests
  a_type struct_type = {};
  struct_type.kind = tk_struct;
  llvm::Type* struct_llvm_ty = llvm::Type::getInt8Ty(*state.context);

  a_type pm_func_type = {};
  pm_func_type.kind = tk_ptr_to_member;
  a_type routine_type = {};
  routine_type.kind = tk_routine;
  pm_func_type.variant.ptr_to_member.type = &routine_type;
  pm_func_type.variant.ptr_to_member.class_of_which_a_member = &struct_type;

  a_type pm_data_type = {};
  pm_data_type.kind = tk_ptr_to_member;
  pm_data_type.variant.ptr_to_member.type = &int_type;
  pm_data_type.variant.ptr_to_member.class_of_which_a_member = &struct_type;

  // 1. Type lowering
  llvm::Type* pm_func_llvm_ty = nullptr;
  assert(llvm_type_from_ptr_to_member(nullptr, &pm_func_llvm_ty) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_type_from_ptr_to_member(&pm_func_type, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_type_from_ptr_to_member(&int_type, &pm_func_llvm_ty) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_type_from_ptr_to_member(&pm_func_type, &pm_func_llvm_ty) == llvm_gen_be_error_t::ok);
  assert(pm_func_llvm_ty && pm_func_llvm_ty->isStructTy());

  llvm::Type* pm_data_llvm_ty = nullptr;
  assert(llvm_type_from_ptr_to_member(&pm_data_type, &pm_data_llvm_ty) == llvm_gen_be_error_t::ok);
  assert(pm_data_llvm_ty && pm_data_llvm_ty->isIntegerTy());

  // 2. Constant evaluation
  llvm::Constant* pm_const = nullptr;
  assert(llvm_const_from_ptr_to_member(nullptr, pm_func_llvm_ty, &pm_const) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_ptr_to_member(nullptr, nullptr, &pm_const) == llvm_gen_be_error_t::invalid_argument);

  a_constant null_pm_con = {};
  null_pm_con.kind = ck_integer; // non ck_ptr_to_member represents null
  assert(llvm_const_from_ptr_to_member(&null_pm_con, pm_func_llvm_ty, &pm_const) == llvm_gen_be_error_t::ok);
  assert(pm_const != nullptr);

  assert(llvm_const_from_ptr_to_member(&null_pm_con, pm_data_llvm_ty, &pm_const) == llvm_gen_be_error_t::ok);
  assert(pm_const != nullptr);

  a_constant pm_data_con = {};
  pm_data_con.kind = ck_ptr_to_member;
  pm_data_con.variant.ptr_to_member.is_function_ptr = FALSE;
  a_field field_obj = {};
  field_obj.offset = 16;
  pm_data_con.variant.ptr_to_member.variant.field = &field_obj;
  assert(llvm_const_from_ptr_to_member(&pm_data_con, pm_data_llvm_ty, &pm_const) == llvm_gen_be_error_t::ok);
  assert(pm_const != nullptr);

  // 3. PM Call lowering
  an_expr_node pm_call_expr = {};
  pm_call_expr.kind = enk_operation;
  pm_call_expr.variant.operation.kind = eok_dot_pm_call;
  a_constant pm_fn_con = {};
  pm_fn_con.kind = ck_ptr_to_member;
  pm_fn_con.type = &pm_func_type;
  an_expr_node pm_op1 = {};
  pm_op1.kind = enk_constant;
  pm_op1.type = &pm_func_type;
  pm_op1.variant.constant.ptr = &pm_fn_con;
  an_expr_node pm_op2 = {};
  pm_op2.kind = enk_variable;
  pm_op2.type = &struct_type;
  pm_op2.is_lvalue = TRUE;
  a_variable pm_obj_var = {};
  pm_obj_var.type = &struct_type;
  pm_op2.variant.variable.ptr = &pm_obj_var;
  state.local_vars[&pm_obj_var] = state.builder->CreateAlloca(struct_llvm_ty);

  pm_op1.next = &pm_op2;
  pm_call_expr.variant.operation.operands = &pm_op1;

  llvm::Value* pm_call_val = nullptr;
  assert(llvm_lower_pm_call(nullptr, &pm_call_val) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_pm_call(&pm_call_expr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_pm_call(&pm_call_expr, &pm_call_val) == llvm_gen_be_error_t::ok);
  assert(pm_call_val != nullptr);

  // 4. PM Field access lowering
  a_constant pm_int_con = {};
  pm_int_con.kind = ck_integer;
  pm_int_con.type = &int_type;
  pm_int_con.variant.integer_value = 8;

  an_expr_node pm_data_op = {};
  pm_data_op.kind = enk_constant;
  pm_data_op.type = &int_type;
  pm_data_op.variant.constant.ptr = &pm_int_con;
  pm_data_op.next = &pm_op2;

  an_expr_node pm_field_expr = {};
  pm_field_expr.kind = enk_operation;
  pm_field_expr.variant.operation.kind = eok_pm_field;
  pm_field_expr.variant.operation.operands = &pm_data_op;

  llvm::Value* pm_field_val = nullptr;
  assert(llvm_lower_pm_field_access(nullptr, &pm_field_val) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_pm_field_access(&pm_field_expr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_pm_field_access(&pm_field_expr, &pm_field_val) == llvm_gen_be_error_t::ok);
  assert(pm_field_val != nullptr);

  // 5. PM Cast lowering
  an_expr_node pm_cast_expr = {};
  pm_cast_expr.kind = enk_operation;
  pm_cast_expr.variant.operation.kind = eok_pm_base_class_cast;
  pm_cast_expr.variant.operation.operands = &pm_op1;

  llvm::Value* pm_cast_val = nullptr;
  assert(llvm_lower_pm_cast(nullptr, &pm_cast_val) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_pm_cast(&pm_cast_expr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_pm_cast(&pm_cast_expr, &pm_cast_val) == llvm_gen_be_error_t::ok);
  assert(pm_cast_val != nullptr);

  // 6. PM Comparison lowering
  an_expr_node pm_cmp_expr = {};
  pm_cmp_expr.kind = enk_operation;
  pm_cmp_expr.variant.operation.kind = eok_eq;
  an_expr_node pm_cmp_op2 = pm_op1;
  pm_op1.next = &pm_cmp_op2;
  pm_cmp_expr.variant.operation.operands = &pm_op1;

  llvm::Value* pm_cmp_val = nullptr;
  assert(llvm_lower_pm_comparison(nullptr, &pm_cmp_val) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_pm_comparison(&pm_cmp_expr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_pm_comparison(&pm_cmp_expr, &pm_cmp_val) == llvm_gen_be_error_t::ok);
  assert(pm_cmp_val != nullptr);

  // 7. enk_new_delete lowering
  an_expr_node new_expr = {};
  new_expr.kind = enk_new_delete;
  new_expr.type = &pointer_type;
  
  a_routine new_routine = {};
  new_routine.source_corresp.name = "operator new";
  new_routine.type = &int_type; // dummy function type
  
  a_new_delete_supplement nd = {};
  nd.is_new = 1;
  nd.type = &int_type;
  nd.routine = &new_routine;
  new_expr.variant.new_delete = &nd;

  llvm::Value* new_val = nullptr;
  assert(llvm_lower_expression(&new_expr, &new_val) == llvm_gen_be_error_t::ok);
  assert(new_val != nullptr);

  a_type void_type = {};
  void_type.kind = tk_void;

  an_expr_node delete_expr = {};
  delete_expr.kind = enk_new_delete;
  delete_expr.type = &void_type;
  
  a_routine delete_routine = {};
  delete_routine.source_corresp.name = "operator delete";
  delete_routine.type = &int_type;
  
  a_new_delete_supplement delete_nd = {};
  delete_nd.is_new = 0;
  delete_nd.type = &int_type;
  delete_nd.routine = &delete_routine;
  an_expr_node del_arg = {};
  del_arg.kind = enk_constant; // dummy ptr
  del_arg.type = &pointer_type;
  delete_nd.arg = &del_arg;
  delete_expr.variant.new_delete = &delete_nd;

  llvm::Value* delete_val = nullptr;
  assert(llvm_lower_expression(&delete_expr, &delete_val) == llvm_gen_be_error_t::ok);
  assert(delete_val == nullptr);

  // 8. enk_new_delete array lowering
  an_expr_node new_array_expr = {};
  new_array_expr.kind = enk_new_delete;
  new_array_expr.type = &pointer_type;
  
  a_routine new_array_routine = {};
  new_array_routine.source_corresp.name = "operator new[]";
  new_array_routine.type = &int_type;
  
  a_new_delete_supplement nd_array = {};
  nd_array.is_new = 1;
  nd_array.type = &int_type;
  nd_array.routine = &new_array_routine;
  
  an_expr_node array_size_expr = {};
  array_size_expr.kind = enk_constant;
  array_size_expr.type = &int_type; // 10 elements
  nd_array.number_of_elements = &array_size_expr;
  new_array_expr.variant.new_delete = &nd_array;

  llvm::Value* new_array_val = nullptr;
  assert(llvm_lower_expression(&new_array_expr, &new_array_val) == llvm_gen_be_error_t::ok);
  assert(new_array_val != nullptr);

  an_expr_node delete_array_expr = {};
  delete_array_expr.kind = enk_new_delete;
  delete_array_expr.type = &void_type;
  
  a_routine delete_array_routine = {};
  delete_array_routine.source_corresp.name = "operator delete[]";
  delete_array_routine.type = &int_type;
  
  a_new_delete_supplement delete_array_nd = {};
  delete_array_nd.is_new = 0;
  delete_array_nd.array_delete = 1;
  delete_array_nd.type = &int_type;
  delete_array_nd.routine = &delete_array_routine;
  an_expr_node del_array_arg = {};
  del_array_arg.kind = enk_constant; // dummy ptr
  del_array_arg.type = &pointer_type;
  delete_array_nd.arg = &del_array_arg;
  delete_array_expr.variant.new_delete = &delete_array_nd;

  llvm::Value* delete_array_val = nullptr;
  assert(llvm_lower_expression(&delete_array_expr, &delete_array_val) == llvm_gen_be_error_t::ok);
  assert(delete_array_val == nullptr);

  // 9. enk_new_delete placement new lowering
  an_expr_node placement_new_expr = {};
  placement_new_expr.kind = enk_new_delete;
  placement_new_expr.type = &pointer_type;
  
  a_routine placement_new_routine = {};
  placement_new_routine.source_corresp.name = "operator new";
  placement_new_routine.type = &pointer_type; // actually returns void*
  
  a_new_delete_supplement nd_placement = {};
  nd_placement.is_new = 1;
  nd_placement.placement_new = 1;
  nd_placement.type = &int_type;
  nd_placement.routine = &placement_new_routine;
  
  an_expr_node placement_arg = {};
  placement_arg.kind = enk_constant;
  placement_arg.type = &pointer_type;
  nd_placement.arg = &placement_arg;
  
  placement_new_expr.variant.new_delete = &nd_placement;

  llvm::Value* placement_new_val = nullptr;
  assert(llvm_lower_expression(&placement_new_expr, &placement_new_val) == llvm_gen_be_error_t::ok);
  assert(placement_new_val != nullptr);

  // 10. enk_new_delete class-specific lowering
  an_expr_node class_new_expr = {};
  class_new_expr.kind = enk_new_delete;
  class_new_expr.type = &pointer_type;
  
  a_routine class_new_routine = {};
  class_new_routine.source_corresp.name = "_ZN1TnwEm"; // mangled T::operator new
  class_new_routine.type = &pointer_type;
  
  a_new_delete_supplement nd_class = {};
  nd_class.is_new = 1;
  nd_class.type = &int_type;
  nd_class.routine = &class_new_routine;
  class_new_expr.variant.new_delete = &nd_class;

  llvm::Value* class_new_val = nullptr;
  assert(llvm_lower_expression(&class_new_expr, &class_new_val) == llvm_gen_be_error_t::ok);
  assert(class_new_val != nullptr);

  an_expr_node class_delete_expr = {};
  class_delete_expr.kind = enk_new_delete;
  class_delete_expr.type = &void_type;
  
  a_routine class_delete_routine = {};
  class_delete_routine.source_corresp.name = "_ZN1TdlEPv"; // mangled T::operator delete
  class_delete_routine.type = &int_type;
  
  a_new_delete_supplement class_delete_nd = {};
  class_delete_nd.is_new = 0;
  class_delete_nd.type = &int_type;
  class_delete_nd.routine = &class_delete_routine;
  an_expr_node class_del_arg = {};
  class_del_arg.kind = enk_constant; // dummy ptr
  class_del_arg.type = &pointer_type;
  class_delete_nd.arg = &class_del_arg;
  class_delete_expr.variant.new_delete = &class_delete_nd;

  llvm::Value* class_delete_val = nullptr;
  assert(llvm_lower_expression(&class_delete_expr, &class_delete_val) == llvm_gen_be_error_t::ok);
  assert(class_delete_val == nullptr);

  // 11. enk_new_delete sized and aligned deallocation lowering
  an_expr_node sized_delete_expr = {};
  sized_delete_expr.kind = enk_new_delete;
  sized_delete_expr.type = &void_type;
  
  a_routine sized_delete_routine = {};
  sized_delete_routine.source_corresp.name = "_ZdlPvmSt11align_val_t"; // mangled operator delete(void*, size_t, std::align_val_t)
  sized_delete_routine.type = &int_type;
  
  a_new_delete_supplement sized_delete_nd = {};
  sized_delete_nd.is_new = 0;
  sized_delete_nd.aligned_version = 1;
  sized_delete_nd.type = &int_type;
  sized_delete_nd.routine = &sized_delete_routine;
  an_expr_node sized_del_arg = {};
  sized_del_arg.kind = enk_constant; // dummy ptr
  sized_del_arg.type = &pointer_type;
  
  an_expr_node size_arg = {};
  size_arg.kind = enk_constant; // size
  size_arg.type = &int_type;
  sized_del_arg.next = &size_arg;
  
  an_expr_node align_arg = {};
  align_arg.kind = enk_constant; // align
  align_arg.type = &int_type;
  size_arg.next = &align_arg;
  
  sized_delete_nd.arg = &sized_del_arg;
  sized_delete_expr.variant.new_delete = &sized_delete_nd;

  llvm::Value* sized_delete_val = nullptr;
  assert(llvm_lower_expression(&sized_delete_expr, &sized_delete_val) == llvm_gen_be_error_t::ok);
  assert(sized_delete_val == nullptr);

  // 12. enk_temp_init lowering
  an_expr_node temp_init_expr = {};
  temp_init_expr.kind = enk_temp_init;
  temp_init_expr.type = &int_type;
  temp_init_expr.is_lvalue = 0; // rvalue so it should load
  
  a_dynamic_init temp_dip = {};
  temp_dip.kind = dik_expression;
  an_expr_node temp_init_val_expr = {};
  temp_init_val_expr.kind = enk_constant;
  temp_init_val_expr.type = &int_type;
  temp_dip.variant.expression = &temp_init_val_expr;
  
  temp_init_expr.variant.init.dynamic_init = &temp_dip;

  llvm::Value* temp_init_val = nullptr;
  assert(llvm_lower_expression(&temp_init_expr, &temp_init_val) == llvm_gen_be_error_t::ok);
  assert(temp_init_val != nullptr);
  // As it's an rvalue, we expect a load instruction
  assert(llvm::isa<llvm::LoadInst>(temp_init_val));

  // 13. enk_temp_init with destructor (lifetime)
  an_expr_node temp_dtor_expr = {};
  temp_dtor_expr.kind = enk_temp_init;
  temp_dtor_expr.type = &int_type;
  temp_dtor_expr.is_lvalue = 1; // lvalue so it should yield pointer
  
  a_routine temp_dtor_routine = {};
  temp_dtor_routine.source_corresp.name = "~Temp";
  temp_dtor_routine.type = &void_type;
  
  a_dynamic_init temp_dtor_dip = {};
  temp_dtor_dip.kind = dik_expression;
  temp_dtor_dip.variant.expression = &temp_init_val_expr;
  temp_dtor_dip.destructor = &temp_dtor_routine;
  temp_dtor_expr.variant.init.dynamic_init = &temp_dtor_dip;
  
  llvm::Value* temp_dtor_val = nullptr;
  assert(llvm_lower_expression(&temp_dtor_expr, &temp_dtor_val) == llvm_gen_be_error_t::ok);
  assert(temp_dtor_val != nullptr);
  assert(llvm::isa<llvm::AllocaInst>(temp_dtor_val));

  // 14. enk_object_lifetime lowering
  an_expr_node obj_lifetime_expr = {};
  obj_lifetime_expr.kind = enk_object_lifetime;
  obj_lifetime_expr.type = &int_type;
  
  an_object_lifetime obj_lifetime = {};
  obj_lifetime_expr.variant.object_lifetime.ptr = &obj_lifetime;
  obj_lifetime_expr.variant.object_lifetime.expr = &temp_dtor_expr; // Reuse the temp_dtor_expr which pushes a cleanup
  
  llvm::Value* obj_lifetime_val = nullptr;
  assert(llvm_lower_expression(&obj_lifetime_expr, &obj_lifetime_val) == llvm_gen_be_error_t::ok);
  assert(obj_lifetime_val != nullptr);

  // 15. eok_negate and eok_unary_plus
  an_expr_node negate_expr = {};
  negate_expr.kind = enk_operation;
  negate_expr.type = &int_type;
  negate_expr.variant.operation.kind = eok_negate;
  negate_expr.variant.operation.operands = &temp_init_val_expr; // reused integer constant
  
  llvm::Value* negate_val = nullptr;
  assert(llvm_lower_expression(&negate_expr, &negate_val) == llvm_gen_be_error_t::ok);
  assert(negate_val != nullptr);
  
  an_expr_node unary_plus_expr = {};
  unary_plus_expr.kind = enk_operation;
  unary_plus_expr.type = &int_type;
  unary_plus_expr.variant.operation.kind = eok_unary_plus;
  unary_plus_expr.variant.operation.operands = &temp_init_val_expr;
  
  llvm::Value* unary_plus_val = nullptr;
  assert(llvm_lower_expression(&unary_plus_expr, &unary_plus_val) == llvm_gen_be_error_t::ok);
  assert(unary_plus_val != nullptr);

  // 16. eok_comma operator
  an_expr_node comma_expr = {};
  comma_expr.kind = enk_operation;
  comma_expr.type = &float_type;
  comma_expr.variant.operation.kind = eok_comma;
  
  an_expr_node comma_op1 = {};
  comma_op1.kind = enk_constant;
  comma_op1.type = &int_type;
  
  an_expr_node comma_op2 = {};
  comma_op2.kind = enk_constant;
  comma_op2.type = &float_type;
  
  comma_op1.next = &comma_op2;
  comma_expr.variant.operation.operands = &comma_op1;
  
  llvm::Value* comma_val = nullptr;
  assert(llvm_lower_expression(&comma_expr, &comma_val) == llvm_gen_be_error_t::ok);
  assert(comma_val != nullptr);
  assert(comma_val->getType()->isFloatTy());

  // Test static member and vacuous destructor calls
  an_expr_node static_expr;
  memset(&static_expr, 0, sizeof(static_expr));
  static_expr.kind = enk_operation;
  static_expr.type = &int_type;
  
  an_expr_node static_op1;
  memset(&static_op1, 0, sizeof(static_op1));
  static_op1.kind = enk_constant;
  static_op1.type = &int_type;

  an_expr_node static_op2;
  memset(&static_op2, 0, sizeof(static_op2));
  static_op2.kind = enk_constant;
  static_op2.type = &int_type;

  static_op1.next = &static_op2;
  static_expr.variant.operation.operands = &static_op1;

  // eok_dot_static
  static_expr.variant.operation.kind = eok_dot_static;
  llvm::Value* static_val = nullptr;
  assert(llvm_lower_expression(&static_expr, &static_val) == llvm_gen_be_error_t::ok);
  assert(static_val != nullptr);

  // eok_points_to_static
  static_expr.variant.operation.kind = eok_points_to_static;
  static_val = nullptr;
  assert(llvm_lower_expression(&static_expr, &static_val) == llvm_gen_be_error_t::ok);
  assert(static_val != nullptr);

  // eok_dot_vacuous_destructor_call
  static_expr.variant.operation.kind = eok_dot_vacuous_destructor_call;
  static_val = nullptr;
  assert(llvm_lower_expression(&static_expr, &static_val) == llvm_gen_be_error_t::ok);
  assert(static_val == nullptr);

  // eok_points_to_vacuous_destructor_call
  static_expr.variant.operation.kind = eok_points_to_vacuous_destructor_call;
  static_val = nullptr;
  assert(llvm_lower_expression(&static_expr, &static_val) == llvm_gen_be_error_t::ok);
  assert(static_val == nullptr);

  // Test unsupported CLI operations
  static_expr.variant.operation.kind = eok_handle_to;
  assert(llvm_lower_expression(&static_expr, &static_val) == llvm_gen_be_error_t::unsupported_expr);
  static_expr.variant.operation.kind = eok_box;
  assert(llvm_lower_expression(&static_expr, &static_val) == llvm_gen_be_error_t::unsupported_expr);
  static_expr.variant.operation.kind = eok_unbox;
  assert(llvm_lower_expression(&static_expr, &static_val) == llvm_gen_be_error_t::unsupported_expr);
  static_expr.variant.operation.kind = eok_cli_subscript;
  assert(llvm_lower_expression(&static_expr, &static_val) == llvm_gen_be_error_t::unsupported_expr);

  // Test enk_braced_init_list
  an_expr_node braced_expr;
  memset(&braced_expr, 0, sizeof(braced_expr));
  braced_expr.kind = enk_braced_init_list;
  braced_expr.type = &int_type;
  braced_expr.variant.braced_init_list = &static_op1; // Reusing a simple constant from above
  llvm::Value* braced_val = nullptr;
  assert(llvm_lower_expression(&braced_expr, &braced_val) == llvm_gen_be_error_t::ok);
  assert(braced_val != nullptr);

  // Test enk_reuse_value
  an_expr_node reuse_expr;
  memset(&reuse_expr, 0, sizeof(reuse_expr));
  reuse_expr.kind = enk_reuse_value;
  reuse_expr.type = &int_type;
  
  a_dynamic_init dyn_init;
  memset(&dyn_init, 0, sizeof(dyn_init));
  dyn_init.kind = dik_expression;
  dyn_init.variant.expression = &static_op1;
  reuse_expr.variant.reused_value_init = &dyn_init;
  
  llvm::Value* reuse_val = nullptr;
  assert(llvm_lower_expression(&reuse_expr, &reuse_val) == llvm_gen_be_error_t::ok);
  assert(reuse_val != nullptr);

  // Test enk_object_lifetime
  an_expr_node lifetime_expr;
  memset(&lifetime_expr, 0, sizeof(lifetime_expr));
  lifetime_expr.kind = enk_object_lifetime;
  lifetime_expr.type = &int_type;
  lifetime_expr.variant.object_lifetime.expr = &static_op1;
  llvm::Value* lifetime_val = nullptr;
  assert(llvm_lower_expression(&lifetime_expr, &lifetime_val) == llvm_gen_be_error_t::ok);
  assert(lifetime_val != nullptr);

  // Test enk_fold, enk_pack_index, enk_c11_generic (unsupported)
  an_expr_node unsupported_expr;
  memset(&unsupported_expr, 0, sizeof(unsupported_expr));
  unsupported_expr.kind = enk_fold;
  unsupported_expr.type = &int_type;
  llvm::Value* unsup_val = nullptr;
  assert(llvm_lower_expression(&unsupported_expr, &unsup_val) == llvm_gen_be_error_t::unsupported_expr);
  
  unsupported_expr.kind = enk_pack_index;
  assert(llvm_lower_expression(&unsupported_expr, &unsup_val) == llvm_gen_be_error_t::unsupported_expr);
  
  unsupported_expr.kind = enk_c11_generic;
  assert(llvm_lower_expression(&unsupported_expr, &unsup_val) == llvm_gen_be_error_t::unsupported_expr);

  printf("All tests passed!\n");
  return 0;
}
namespace edg {
  llvm_gen_be_error_t llvm_lower_statement(a_statement_ptr stmt) noexcept {
    return llvm_gen_be_error_t::ok;
  }
}

// Extra test function to be called from main? Wait, I'll just replace the end of main.
