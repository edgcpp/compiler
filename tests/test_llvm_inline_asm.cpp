/**
 * @file test_llvm_inline_asm.cpp
 * @ingroup llvm_gen_be_tests
 * @brief Unit tests for the LLVM backend inline assembly lowering subsystem.
 * @details Validates function coverage of llvm_lower_asm_stmt.
 */

#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_asm.h"
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
  /**
   * @brief Test stub or helper for `is_bool_type`.
   * @details Provides mock or helper behavior for `is_bool_type` within `tests/test_llvm_inline_asm.cpp`.
   * @param[in] ty Parameter `ty` for `is_bool_type`.
   * @return Result of `is_bool_type` (a_boolean).
   */
  a_boolean is_bool_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `f_alignment_of_type`.
   * @details Provides mock or helper behavior for `f_alignment_of_type` within `tests/test_llvm_inline_asm.cpp`.
   * @param[in] ty Parameter `ty` for `f_alignment_of_type`.
   * @return Result of `f_alignment_of_type` (a_targ_alignment).
   */
  a_targ_alignment f_alignment_of_type(a_type_ptr ty) { return 1; }
  /**
   * @brief Test stub or helper for `is_trivially_copyable_type`.
   * @details Provides mock or helper behavior for `is_trivially_copyable_type` within `tests/test_llvm_inline_asm.cpp`.
   * @param[in] ty Parameter `ty` for `is_trivially_copyable_type`.
   * @return Result of `is_trivially_copyable_type` (a_boolean).
   */
  a_boolean is_trivially_copyable_type(a_type_ptr ty) { return TRUE; }
  unsigned int targ_char_bit = 8;
  a_targ_size_t targ_sizeof_short = 2;
  a_targ_size_t targ_sizeof_int = 4;
  a_targ_size_t targ_sizeof_long = 8;
  a_targ_size_t targ_sizeof_long_long = 8;
  a_targ_size_t targ_sizeof_pointer = 8;
  a_boolean targ_little_endian = TRUE;
  a_C_dialect C_dialect = static_cast<a_C_dialect>(0);
  /**
   * @brief Test stub or helper for `conv_seq_to_file_and_line`.
   * @details Provides mock or helper behavior for `conv_seq_to_file_and_line` within `tests/test_llvm_inline_asm.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `conv_seq_to_file_and_line`.
   * @param[out] arg_1 Parameter `arg_1` for `conv_seq_to_file_and_line`.
   * @param[out] arg_2 Parameter `arg_2` for `conv_seq_to_file_and_line`.
   * @param[in] arg_3 Parameter `arg_3` for `conv_seq_to_file_and_line`.
   * @param[in] arg_4 Parameter `arg_4` for `conv_seq_to_file_and_line`.
   * @return Result of `conv_seq_to_file_and_line` (a_source_file_ptr).
   */
  a_source_file_ptr conv_seq_to_file_and_line(a_seq_number arg_0, a_const_char** arg_1, a_const_char** arg_2, a_line_number* arg_3, a_boolean* arg_4) { return nullptr; }
  /**
   * @brief Test stub or helper for `mangled_typeinfo_name`.
   * @details Provides mock or helper behavior for `mangled_typeinfo_name` within `tests/test_llvm_inline_asm.cpp`.
   * @param[in] type Parameter `type` for `mangled_typeinfo_name`.
   * @return Result of `mangled_typeinfo_name` (char*).
   */
  char* mangled_typeinfo_name(a_type_ptr type) { return (char*)"_ZTIi"; }
  /**
   * @brief Test stub or helper for `f_get_type_qualifiers`.
   * @details Provides mock or helper behavior for `f_get_type_qualifiers` within `tests/test_llvm_inline_asm.cpp`.
   * @param[in] type Parameter `type` for `f_get_type_qualifiers`.
   * @param[in] x Parameter `x` for `f_get_type_qualifiers`.
   * @return Result of `f_get_type_qualifiers` (a_type_qualifier_set).
   */
  a_type_qualifier_set f_get_type_qualifiers(a_type_ptr type, a_boolean x) { return 0; }
  /**
   * @brief Test stub or helper for `is_reference_type`.
   * @details Provides mock or helper behavior for `is_reference_type` within `tests/test_llvm_inline_asm.cpp`.
   * @param[in] ty Parameter `ty` for `is_reference_type`.
   * @return Result of `is_reference_type` (a_boolean).
   */
  a_boolean is_reference_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `is_empty_class_type`.
   * @details Provides mock or helper behavior for `is_empty_class_type` within `tests/test_llvm_inline_asm.cpp`.
   * @param[in] ty Parameter `ty` for `is_empty_class_type`.
   * @return Result of `is_empty_class_type` (a_boolean).
   */
  a_boolean is_empty_class_type(a_type_ptr ty) { return ty && ty->variant.class_struct_union.is_empty_class; }
  /**
   * @brief Test stub or helper for `evaluate_constant`.
   * @details Provides mock or helper behavior for `evaluate_constant` within `tests/test_llvm_inline_asm.cpp`.
   * @param[in] con Parameter `con` for `evaluate_constant`.
   * @param[in] ty Parameter `ty` for `evaluate_constant`.
   * @param[out] out_const Parameter `out_const` for `evaluate_constant`.
   * @return Result of `evaluate_constant` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t evaluate_constant(a_constant_ptr con, llvm::Type* ty, llvm::Constant** out_const) noexcept { 
    if (ty->isIntegerTy()) *out_const = llvm::ConstantInt::get(ty, 1);
    else if (ty->isFloatingPointTy()) *out_const = llvm::ConstantFP::get(ty, 1.0);
    else *out_const = llvm::Constant::getNullValue(ty);
    return llvm_gen_be_error_t::ok; 
  }
  a_byte_boolean int_kind_is_signed[ik_last] = {0};
  
  int g_fail_type = 0; ///< When non-zero, `get_llvm_type` fails.
  int g_fail_expr_call = 0; ///< Call counter before `llvm_lower_expression` fails (negative = immediate fail).
  int g_expr_return_int = 0; ///< When non-zero, `llvm_lower_expression` returns an i32 value instead of a pointer.

  /**
   * @brief Test stub or helper for `get_llvm_type`.
   * @details Provides mock or helper behavior for `get_llvm_type` within `tests/test_llvm_inline_asm.cpp`.
   * @param[in] ty Parameter `ty` for `get_llvm_type`.
   * @param[out] out_ty Parameter `out_ty` for `get_llvm_type`.
   * @return Result of `get_llvm_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_llvm_type(a_type_ptr ty, llvm::Type** out_ty) noexcept {
    (void)ty;
    if (g_fail_type) return llvm_gen_be_error_t::unsupported_type;
    *out_ty = llvm::Type::getInt32Ty(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }
  
  
  
  /**
   * @brief Test stub or helper for `get_typeinfo_global`.
   * @details Provides mock or helper behavior for `get_typeinfo_global` within `tests/test_llvm_inline_asm.cpp`.
   * @param[in] type Parameter `type` for `get_typeinfo_global`.
   * @param[out] out_ti Parameter `out_ti` for `get_typeinfo_global`.
   * @return Result of `get_typeinfo_global` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_typeinfo_global(a_type_ptr type, llvm::Constant** out_ti) noexcept {
    (void)type;
    *out_ti = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
    return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `llvm_lower_statement`.
   * @details Provides mock or helper behavior for `llvm_lower_statement` within `tests/test_llvm_inline_asm.cpp`.
   * @param[in] stmt Parameter `stmt` for `llvm_lower_statement`.
   * @return Result of `llvm_lower_statement` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_statement(a_statement_ptr stmt) noexcept {
    (void)stmt;
    return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub or helper for `llvm_lower_expression`.
   * @details Provides mock or helper behavior for `llvm_lower_expression` within `tests/test_llvm_inline_asm.cpp`.
   * @param[in] expr Parameter `expr` for `llvm_lower_expression`.
   * @param[out] out_val Parameter `out_val` for `llvm_lower_expression`.
   * @return Result of `llvm_lower_expression` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_expression(an_expr_node_ptr expr, llvm::Value** out_val) noexcept {
    if (g_fail_expr_call < 0) return llvm_gen_be_error_t::unsupported_expr;
    if (g_fail_expr_call > 0) {
      --g_fail_expr_call;
      if (g_fail_expr_call == 0) return llvm_gen_be_error_t::unsupported_expr;
    }
    if (expr == nullptr) {
       *out_val = nullptr;
       return llvm_gen_be_error_t::ok;
    }
    if (g_expr_return_int > 0) {
       --g_expr_return_int;
       *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 7);
       return llvm_gen_be_error_t::ok;
    }
    *out_val = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
    return llvm_gen_be_error_t::ok;
  }

  const char* named_register_names[anr_last + 1] = {"eax"};
  an_il_header il_header = {};
}

/**
 * @brief Entry point for the GNU inline assembly lowering unit test suite.
 * @details Tests simple inline assembly, output/input constraints, clobbers, and `asm goto`.
 * @return 0 on success.
 */
int main(void) {
  printf("Running test_llvm_inline_asm...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_asm", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);

  // Null and invalid statement tests
  assert(llvm_lower_asm_stmt(nullptr) == llvm_gen_be_error_t::invalid_argument);
  a_statement bad_stmt = {};
  bad_stmt.kind = stmk_empty;
  assert(llvm_lower_asm_stmt(&bad_stmt) == llvm_gen_be_error_t::invalid_argument);
  a_statement stmt = {};
  stmt.kind = stmk_asm;
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::invalid_argument);

  an_asm_entry aep = {};
  stmt.variant.asm_entry = &aep;
  aep.gnu_asm_form = TRUE;

  // Test with null asm_string and non-ck_string asm_string, plus clobbers when operands == nullptr
  a_named_register_list clob1 = {}, clob2 = {};
  clob1.reg = static_cast<a_named_register>(0);
  clob1.next = &clob2;
  clob2.reg = static_cast<a_named_register>(0);
  aep.clobbers = &clob1;
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::ok);

  a_constant cstr = {};
  cstr.kind = ck_integer;
  aep.asm_string = &cstr;
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::ok);

  cstr.kind = ck_string;
  cstr.variant.string.value = "nop";
  cstr.variant.string.length = 4;

  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::ok);

  // Test asm with multiple outputs, inputs, memory constraints (=m, +m, =*m, m, *m), and null constraint string
  an_expr_node ex1 = {}, ex2 = {};
  an_asm_operand op1 = {}, op2 = {}, op3 = {}, op4 = {}, op5 = {}, op6 = {}, op7 = {}, op8 = {};
  op1.is_output_operand = TRUE; op1.constraints_string = "=r"; op1.expression = &ex1; op1.next = &op2;
  op2.is_output_operand = TRUE; op2.constraints_string = "=r"; op2.expression = &ex2; op2.next = &op3;
  op3.is_output_operand = TRUE; op3.constraints_string = "=m"; op3.expression = &ex1; op3.next = &op4;
  op4.is_output_operand = TRUE; op4.constraints_string = "+m"; op4.expression = &ex1; op4.next = &op5;
  op5.is_output_operand = TRUE; op5.constraints_string = "=*m"; op5.expression = &ex1; op5.next = &op6;
  op6.is_output_operand = TRUE; op6.constraints_string = "m"; op6.expression = &ex1; op6.next = &op7;
  op7.is_output_operand = FALSE; op7.constraints_string = "+m"; op7.expression = &ex1; op7.next = &op8;
  op8.is_output_operand = FALSE; op8.constraints_string = nullptr; op8.expression = &ex1; op8.next = nullptr;
  aep.operands = &op1;
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::ok);

  // Input memory constraints "m" and "*m"
  op7.constraints_string = "m";
  op8.constraints_string = "*m";
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::ok);

  // Test single output + error paths
  op1.next = nullptr;
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::ok);
  g_fail_type = 1;
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::unsupported_type);
  g_fail_type = 0;
  g_fail_expr_call = 1; // fails in output_types.size() == 1 store
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::unsupported_expr);
  g_fail_expr_call = 0;

  op1.next = &op2; op2.next = nullptr;
  g_fail_expr_call = 1; // fails in output_types.size() > 1 store
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::unsupported_expr);
  g_fail_expr_call = 0;

  // Test input/memory operand error paths
  aep.operands = &op3; op3.next = nullptr;
  g_fail_expr_call = 1;
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::unsupported_expr);
  g_fail_expr_call = 0;
  g_fail_type = 1;
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::unsupported_type);
  g_fail_type = 0;

  // Test asm goto with both memory and register operands, and both mapped and unmapped labels
  op3.next = &op8; op8.constraints_string = "r"; op8.next = nullptr;
  aep.is_asm_goto = TRUE;
  a_label_list lbl_list1 = {}, lbl_list2 = {};
  a_label lbl1 = {}, lbl2 = {};
  lbl_list1.label = &lbl1;
  lbl_list1.next = &lbl_list2;
  lbl_list2.label = &lbl2; // lbl2 is NOT in state.label_blocks!
  aep.labels = &lbl_list1;

  state.label_blocks[&lbl1] = llvm::BasicBlock::Create(*state.context, "lbl", f);
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::ok);

  // Test +r read-write register constraint (pointer & non-pointer rw_val, plus rw_err)
  aep.is_asm_goto = FALSE;
  an_asm_operand rw_op = {};
  rw_op.is_output_operand = TRUE;
  rw_op.constraints_string = "+r";
  rw_op.expression = &ex1;
  aep.operands = &rw_op;
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::ok);
  g_expr_return_int = 1; // first expression evaluation returns non-pointer i32
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::ok);
  g_fail_expr_call = 1; // fails on rw_err
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::unsupported_expr);
  g_fail_expr_call = 0;

  // Test asm goto with 1 output operand (ok and error path)
  aep.is_asm_goto = TRUE;
  rw_op.constraints_string = "=r";
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::ok);
  g_fail_expr_call = 1;
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::unsupported_expr);
  g_fail_expr_call = 0;

  // Test GNU asm template operand rewriting ($, %%, %0..%9, %w/%x/%b/%h/%k/%q/%c/%P modifiers, non-digit & trailing %)
  aep.is_asm_goto = FALSE;
  rw_op.constraints_string = "=r";
  aep.operands = &rw_op;
  const char* asm_templates[] = {
      "mov $1, %%eax; use %0 %9 %/ %: %w0 %x1 %b2 %h3 %k4 %q5 %c6 %P10end %P12",
      "bad %w/ %w: %z %w",
      "trail %"
  };
  for (const char* tpl : asm_templates) {
    cstr.variant.string.value = const_cast<char*>(tpl);
    cstr.variant.string.length = static_cast<a_targ_size_t>( std::strlen(tpl) + 1U );
    assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::ok);
  }
  cstr.variant.string.value = const_cast<char*>("nop");
  cstr.variant.string.length = 4;

  // Test llvm_canonicalize_asm_constraint across x86, i386, i686, aarch64, arm, and generic triples
  std::string canon;
  assert(llvm_canonicalize_asm_constraint(nullptr, true, "x86_64-linux-gnu", &canon) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_canonicalize_asm_constraint("=r", true, "x86_64-linux-gnu", nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_canonicalize_asm_constraint("=%#!?&a", true, "x86_64-linux-gnu", &canon) == llvm_gen_be_error_t::ok);
  assert(canon == "=&{ax}");
  assert(llvm_canonicalize_asm_constraint("+b", true, "i386-linux-gnu", &canon) == llvm_gen_be_error_t::ok);
  assert(canon == "={bx}");
  assert(llvm_canonicalize_asm_constraint("+c", false, "i686-linux-gnu", &canon) == llvm_gen_be_error_t::ok);
  assert(canon == "+{cx}");
  assert(llvm_canonicalize_asm_constraint("d", false, "x86_64-linux-gnu", &canon) == llvm_gen_be_error_t::ok);
  assert(canon == "{dx}");
  assert(llvm_canonicalize_asm_constraint("S", false, "x86_64-linux-gnu", &canon) == llvm_gen_be_error_t::ok);
  assert(canon == "{si}");
  assert(llvm_canonicalize_asm_constraint("D", false, "x86_64-linux-gnu", &canon) == llvm_gen_be_error_t::ok);
  assert(canon == "{di}");
  assert(llvm_canonicalize_asm_constraint("Yz", false, "x86_64-linux-gnu", &canon) == llvm_gen_be_error_t::ok);
  assert(canon == "{xmm0}");
  assert(llvm_canonicalize_asm_constraint("q", false, "x86_64-linux-gnu", &canon) == llvm_gen_be_error_t::ok);
  assert(canon == "r");
  assert(llvm_canonicalize_asm_constraint("Q", false, "x86_64-linux-gnu", &canon) == llvm_gen_be_error_t::ok);
  assert(canon == "r");
  assert(llvm_canonicalize_asm_constraint("R", false, "x86_64-linux-gnu", &canon) == llvm_gen_be_error_t::ok);
  assert(canon == "r");
  assert(llvm_canonicalize_asm_constraint("r+", false, "x86_64-linux-gnu", &canon) == llvm_gen_be_error_t::ok);
  assert(canon == "r+");
  assert(llvm_canonicalize_asm_constraint("x", false, "aarch64-linux-gnu", &canon) == llvm_gen_be_error_t::ok);
  assert(canon == "w");
  assert(llvm_canonicalize_asm_constraint("Q", false, "armv7-linux-gnueabihf", &canon) == llvm_gen_be_error_t::ok);
  assert(canon == "*m");
  assert(llvm_canonicalize_asm_constraint("w", false, "aarch64-linux-gnu", &canon) == llvm_gen_be_error_t::ok);
  assert(canon == "w");
  assert(llvm_canonicalize_asm_constraint("r", false, "riscv64-linux-gnu", &canon) == llvm_gen_be_error_t::ok);
  assert(canon == "r");

  printf("PASS\n");
  return 0;
}
