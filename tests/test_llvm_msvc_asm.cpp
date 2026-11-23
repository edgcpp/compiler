/**
 * @file test_llvm_msvc_asm.cpp
 * @ingroup llvm_gen_be_tests
 * @brief Unit tests for the LLVM backend MSVC inline assembly lowering subsystem.
 * @details Validates Intel dialect MSVC `__asm` variable and nested struct member offset lowering.
 */

#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_asm.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

using namespace edg;

LLVMBackendState* edg::be_state = nullptr;

namespace edg {
  /**
   * @brief Test stub or helper for `is_bool_type`.
   * @details Provides mock or helper behavior for `is_bool_type` within `tests/test_llvm_msvc_asm.cpp`.
   * @param[in] ty Parameter `ty` for `is_bool_type`.
   * @return Result of `is_bool_type` (a_boolean).
   */
  a_boolean is_bool_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `f_alignment_of_type`.
   * @details Provides mock or helper behavior for `f_alignment_of_type` within `tests/test_llvm_msvc_asm.cpp`.
   * @param[in] ty Parameter `ty` for `f_alignment_of_type`.
   * @return Result of `f_alignment_of_type` (a_targ_alignment).
   */
  a_targ_alignment f_alignment_of_type(a_type_ptr ty) { return 1; }
  /**
   * @brief Test stub or helper for `is_trivially_copyable_type`.
   * @details Provides mock or helper behavior for `is_trivially_copyable_type` within `tests/test_llvm_msvc_asm.cpp`.
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
   * @details Provides mock or helper behavior for `conv_seq_to_file_and_line` within `tests/test_llvm_msvc_asm.cpp`.
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
   * @details Provides mock or helper behavior for `mangled_typeinfo_name` within `tests/test_llvm_msvc_asm.cpp`.
   * @param[in] type Parameter `type` for `mangled_typeinfo_name`.
   * @return Result of `mangled_typeinfo_name` (char*).
   */
  char* mangled_typeinfo_name(a_type_ptr type) { return (char*)"_ZTIi"; }
  /**
   * @brief Test stub or helper for `f_get_type_qualifiers`.
   * @details Provides mock or helper behavior for `f_get_type_qualifiers` within `tests/test_llvm_msvc_asm.cpp`.
   * @param[in] type Parameter `type` for `f_get_type_qualifiers`.
   * @param[in] x Parameter `x` for `f_get_type_qualifiers`.
   * @return Result of `f_get_type_qualifiers` (a_type_qualifier_set).
   */
  a_type_qualifier_set f_get_type_qualifiers(a_type_ptr type, a_boolean x) { return 0; }
  /**
   * @brief Test stub or helper for `is_reference_type`.
   * @details Provides mock or helper behavior for `is_reference_type` within `tests/test_llvm_msvc_asm.cpp`.
   * @param[in] ty Parameter `ty` for `is_reference_type`.
   * @return Result of `is_reference_type` (a_boolean).
   */
  a_boolean is_reference_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `is_empty_class_type`.
   * @details Provides mock or helper behavior for `is_empty_class_type` within `tests/test_llvm_msvc_asm.cpp`.
   * @param[in] ty Parameter `ty` for `is_empty_class_type`.
   * @return Result of `is_empty_class_type` (a_boolean).
   */
  a_boolean is_empty_class_type(a_type_ptr ty) { return ty && ty->variant.class_struct_union.is_empty_class; }
  /**
   * @brief Test stub or helper for `evaluate_constant`.
   * @details Provides mock or helper behavior for `evaluate_constant` within `tests/test_llvm_msvc_asm.cpp`.
   * @param[in] con Parameter `con` for `evaluate_constant`.
   * @param[in] ty Parameter `ty` for `evaluate_constant`.
   * @param[out] out_const Parameter `out_const` for `evaluate_constant`.
   * @return Result of `evaluate_constant` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t evaluate_constant(a_constant_ptr con, llvm::Type* ty, llvm::Constant** out_const) noexcept { 
    *out_const = llvm::Constant::getNullValue(ty); return llvm_gen_be_error_t::ok; 
  }
  a_byte_boolean int_kind_is_signed[ik_last] = {0};
  int g_fail_msvc_type = 0; ///< When non-zero, `get_llvm_type` fails.
  /**
   * @brief Test stub or helper for `get_llvm_type`.
   * @details Provides mock or helper behavior for `get_llvm_type` within `tests/test_llvm_msvc_asm.cpp`.
   * @param[in] ty Parameter `ty` for `get_llvm_type`.
   * @param[out] out_ty Parameter `out_ty` for `get_llvm_type`.
   * @return Result of `get_llvm_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_llvm_type(a_type_ptr ty, llvm::Type** out_ty) noexcept {
    (void)ty;
    if (g_fail_msvc_type) return llvm_gen_be_error_t::unsupported_type;
    *out_ty = llvm::Type::getInt32Ty(*be_state->context); return llvm_gen_be_error_t::ok;
  }
  /**
   * @brief Test stub or helper for `get_typeinfo_global`.
   * @details Provides mock or helper behavior for `get_typeinfo_global` within `tests/test_llvm_msvc_asm.cpp`.
   * @param[in] type Parameter `type` for `get_typeinfo_global`.
   * @param[out] out_ti Parameter `out_ti` for `get_typeinfo_global`.
   * @return Result of `get_typeinfo_global` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_typeinfo_global(a_type_ptr type, llvm::Constant** out_ti) noexcept {
    *out_ti = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context)); return llvm_gen_be_error_t::ok;
  }
  /**
   * @brief Test stub or helper for `llvm_lower_statement`.
   * @details Provides mock or helper behavior for `llvm_lower_statement` within `tests/test_llvm_msvc_asm.cpp`.
   * @param[in] stmt Parameter `stmt` for `llvm_lower_statement`.
   * @return Result of `llvm_lower_statement` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_statement(a_statement_ptr stmt) noexcept { return llvm_gen_be_error_t::ok; }
  /**
   * @brief Test stub or helper for `llvm_lower_expression`.
   * @details Provides mock or helper behavior for `llvm_lower_expression` within `tests/test_llvm_msvc_asm.cpp`.
   * @param[in] expr Parameter `expr` for `llvm_lower_expression`.
   * @param[out] out_val Parameter `out_val` for `llvm_lower_expression`.
   * @return Result of `llvm_lower_expression` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_expression(an_expr_node_ptr expr, llvm::Value** out_val) noexcept {
    *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 1); return llvm_gen_be_error_t::ok;
  }
  const char* named_register_names[anr_last + 1] = {0};
  an_il_header il_header = {};
}

/**
 * @brief Entry point for the MSVC inline assembly unit test suite.
 * @details Tests `[my_local]` and `[struct.sub.member]` offset translation in MSVC `__asm` blocks.
 * @return 0 on success.
 */
int main(void) {
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_asm", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  a_scope primary_scope = {};
  edg::il_header.primary_scope = &primary_scope;

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);

  a_statement stmt = {};
  stmt.kind = stmk_asm;
  an_asm_entry aep = {};
  stmt.variant.asm_entry = &aep;
  aep.gnu_asm_form = 0; // MSVC asm!
  
  a_type int_type = {};
  int_type.kind = tk_integer;
  int_type.size = 4;

  a_type sub_struct_ty = {};
  sub_struct_ty.kind = tk_struct;
  sub_struct_ty.size = 8;
  a_field sub_member_field = {};
  sub_member_field.source_corresp.name = (char*)"member";
  sub_member_field.offset = 4;
  sub_member_field.type = &int_type;
  sub_struct_ty.variant.class_struct_union.field_list = &sub_member_field;

  a_type test_struct_ty = {};
  test_struct_ty.kind = tk_struct;
  test_struct_ty.size = 16;
  test_struct_ty.source_corresp.name = (char*)"test_struct";
  a_field direct_member_field = {};
  direct_member_field.source_corresp.name = (char*)"member";
  direct_member_field.offset = 4;
  direct_member_field.type = &int_type;
  a_field sub_field = {};
  sub_field.source_corresp.name = (char*)"sub";
  sub_field.offset = 8;
  sub_field.type = &sub_struct_ty;
  direct_member_field.next = &sub_field;
  test_struct_ty.variant.class_struct_union.field_list = &direct_member_field;
  primary_scope.types = &test_struct_ty;
  
  a_variable dummy_var = {}, arr_var = {}, unnamed_local = {}, local_st_var = {};
  dummy_var.source_corresp.name = "my_local";
  dummy_var.type = &int_type;

  a_type arr_ty = {};
  arr_ty.kind = tk_array;
  arr_ty.size = 16;
  arr_ty.variant.array.variant.number_of_elements = 4;
  arr_var.source_corresp.name = "my_arr";
  arr_var.type = &arr_ty;

  unnamed_local.source_corresp.name = nullptr;
  unnamed_local.type = &int_type;

  a_type cls_ty = sub_struct_ty; cls_ty.kind = tk_class;
  a_type un_ty = sub_struct_ty; un_ty.kind = tk_union;
  a_field unnamed_f = {};
  unnamed_f.source_corresp.name = nullptr;
  unnamed_f.type = &int_type;
  unnamed_f.next = &sub_member_field;
  cls_ty.variant.class_struct_union.field_list = &unnamed_f;

  local_st_var.source_corresp.name = "local_cls";
  local_st_var.type = &cls_ty;

  llvm::AllocaInst* alloca = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));
  state.local_vars[&dummy_var] = alloca;
  state.local_vars[&arr_var] = alloca;
  state.local_vars[&unnamed_local] = alloca;
  state.local_vars[&local_st_var] = alloca;

  a_type unnamed_tp = {};
  unnamed_tp.source_corresp.name = nullptr;
  unnamed_tp.next = &test_struct_ty;
  primary_scope.types = &unnamed_tp;

  a_variable unnamed_scope_var = {}, scope_un_var = {};
  unnamed_scope_var.source_corresp.name = nullptr;
  unnamed_scope_var.next = &scope_un_var;
  scope_un_var.source_corresp.name = "glob_un";
  scope_un_var.type = &un_ty;
  primary_scope.variables = &unnamed_scope_var;
  
  a_constant cstr = {};
  cstr.kind = ck_string;
  cstr.variant.string.value =
      "mov _x, TYPE my_local\n"
      "mov eax, SIZE my_local\n"
      "mov ebx, LENGTH my_local\n"
      "mov ecx, LENGTH my_arr\n"
      "mov edx, OFFSET my_local\n"
      "mov edx, TYPE unknown_var\n"
      "mov esi, DWORD PTR [my_local]\n"
      "mov edi, [local_cls.member]\n"
      "mov ebp, [local_cls.member.bad]\n"
      "mov esp, [glob_un.member]\n"
      "mov rax, [test_struct.member]\n"
      "mov rbx, [test_struct.sub.member]\n"
      "mov rcx, [test_struct.nonexistent]\n"
      "mov rdx, [unknown_struct.member]\n"
      "mov r8, TYPE a.b\n"
      "mov r9, TYPE \n"
      "movaps xmm0, xmm15\n"
      "movq mm0, mm7\n"
      "cpuid\n"
      "rdtsc\n"
      "idiv ecx\n"
      "div ecx\n"
      "rep movsb\n"
      "rep stosb\n"
      "jmp lbl_done\n"
      "je lbl_eq\n"
      "jne lbl_ne\n"
      "jg lbl_gt\n"
      "jl\t\n"
      "loop lbl_loop\n"
      "nop";
  cstr.variant.string.length = strlen(cstr.variant.string.value) + 1;
  aep.asm_string = &cstr;

  assert(parse_msvc_asm_block("nop", nullptr) == llvm_gen_be_error_t::invalid_argument);
  for (int b = 0; b < 64; ++b) {
    g_llvm_be_oom_countdown = b;
    llvm_gen_be_error_t r_oom = llvm_lower_asm_stmt(&stmt);
    g_llvm_be_oom_countdown = -1;
    if (r_oom == llvm_gen_be_error_t::ok) break;
  }
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::ok);

  // Test error propagation when get_llvm_type fails on OFFSET my_local or [my_local]
  g_fail_msvc_type = 1;
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::unsupported_type);
  cstr.variant.string.value = "mov eax, [my_local]";
  cstr.variant.string.length = strlen(cstr.variant.string.value) + 1;
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::unsupported_type);
  g_fail_msvc_type = 0;

  // Test when primary_scope is nullptr and when primary_scope->variables is nullptr
  cstr.variant.string.value = "mov eax, [glob_un.member]";
  cstr.variant.string.length = strlen(cstr.variant.string.value) + 1;
  primary_scope.variables = nullptr;
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::ok);
  edg::il_header.primary_scope = nullptr;
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::ok);

  // Test asm_str ending in "TYPE ", "jmp ", and "jmp lbl_end" (at end of string)
  cstr.variant.string.value = "TYPE ";
  cstr.variant.string.length = strlen(cstr.variant.string.value) + 1;
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::ok);
  cstr.variant.string.value = "jmp ";
  cstr.variant.string.length = strlen(cstr.variant.string.value) + 1;
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::ok);
  cstr.variant.string.value = "jmp lbl_end";
  cstr.variant.string.length = strlen(cstr.variant.string.value) + 1;
  assert(llvm_lower_asm_stmt(&stmt) == llvm_gen_be_error_t::ok);
  printf("PASS\n");
  return 0;
}
