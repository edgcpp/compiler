/**
 * @file test_llvm_coro_atomics.cpp
 * @ingroup llvm_gen_be_tests
 * @brief Unit tests for the LLVM backend coroutine and atomic lowering subsystem.
 * @details Validates function coverage of llvm_lower_coroutine_stmt, llvm_lower_coroutine_return_stmt, and co_await/co_yield expressions.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_coro.h"
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
   * @details Provides mock or helper behavior for `is_bool_type` within `tests/test_llvm_coro_atomics.cpp`.
   * @param[in] ty Parameter `ty` for `is_bool_type`.
   * @return Result of `is_bool_type` (a_boolean).
   */
  a_boolean is_bool_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `f_alignment_of_type`.
   * @details Provides mock or helper behavior for `f_alignment_of_type` within `tests/test_llvm_coro_atomics.cpp`.
   * @param[in] ty Parameter `ty` for `f_alignment_of_type`.
   * @return Result of `f_alignment_of_type` (a_targ_alignment).
   */
  a_targ_alignment f_alignment_of_type(a_type_ptr ty) { return 1; }
  /**
   * @brief Test stub or helper for `is_trivially_copyable_type`.
   * @details Provides mock or helper behavior for `is_trivially_copyable_type` within `tests/test_llvm_coro_atomics.cpp`.
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
   * @brief Test stub or helper for `mangled_typeinfo_name`.
   * @details Provides mock or helper behavior for `mangled_typeinfo_name` within `tests/test_llvm_coro_atomics.cpp`.
   * @param[in] type Parameter `type` for `mangled_typeinfo_name`.
   * @return Result of `mangled_typeinfo_name` (char*).
   */
  char* mangled_typeinfo_name(a_type_ptr type) { return (char*)"_ZTIi"; }
  /**
   * @brief Test stub or helper for `conv_seq_to_file_and_line`.
   * @details Provides mock source file and line resolution within `tests/test_llvm_coro_atomics.cpp`.
   * @param[in] seq_number Sequence number to resolve.
   * @param[out] file_name Receives short filename.
   * @param[out] full_name Receives full header/source path.
   * @param[out] line_number Receives 1-based line number.
   * @param[out] at_end_of_source Receives end-of-source flag.
   * @return Null `a_source_file_ptr` in standalone unit tests.
   */
  a_source_file_ptr conv_seq_to_file_and_line(
      a_seq_number seq_number,
      a_const_char** file_name,
      a_const_char** full_name,
      a_line_number* line_number,
      a_boolean* at_end_of_source) {
    if (file_name) *file_name = nullptr;
    if (full_name) *full_name = nullptr;
    if (line_number) *line_number = seq_number;
    if (at_end_of_source) *at_end_of_source = FALSE;
    return nullptr;
  }
  /**
   * @brief Test stub or helper for `f_get_type_qualifiers`.
   * @details Provides mock or helper behavior for `f_get_type_qualifiers` within `tests/test_llvm_coro_atomics.cpp`.
   * @param[in] type Parameter `type` for `f_get_type_qualifiers`.
   * @param[in] x Parameter `x` for `f_get_type_qualifiers`.
   * @return Result of `f_get_type_qualifiers` (a_type_qualifier_set).
   */
  a_type_qualifier_set f_get_type_qualifiers(a_type_ptr type, a_boolean x) { return 0; }
  /**
   * @brief Test stub or helper for `is_reference_type`.
   * @details Provides mock or helper behavior for `is_reference_type` within `tests/test_llvm_coro_atomics.cpp`.
   * @param[in] ty Parameter `ty` for `is_reference_type`.
   * @return Result of `is_reference_type` (a_boolean).
   */
  a_boolean is_reference_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `is_empty_class_type`.
   * @details Provides mock or helper behavior for `is_empty_class_type` within `tests/test_llvm_coro_atomics.cpp`.
   * @param[in] ty Parameter `ty` for `is_empty_class_type`.
   * @return Result of `is_empty_class_type` (a_boolean).
   */
  a_boolean is_empty_class_type(a_type_ptr ty) { return ty && ty->variant.class_struct_union.is_empty_class; }
  /**
   * @brief Test stub or helper for `evaluate_constant`.
   * @details Provides mock or helper behavior for `evaluate_constant` within `tests/test_llvm_coro_atomics.cpp`.
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
  
  int g_coro_type_fail = 0; ///< Countdown before `get_llvm_type` returns `unsupported_type`.
  int g_coro_expr_mode = 0; ///< Selects return value shape in `llvm_lower_expression`.

  /**
   * @brief Test stub or helper for `get_llvm_type`.
   * @details Provides mock or helper behavior for `get_llvm_type` within `tests/test_llvm_coro_atomics.cpp`.
   * @param[in] ty Parameter `ty` for `get_llvm_type`.
   * @param[out] out_ty Parameter `out_ty` for `get_llvm_type`.
   * @return Result of `get_llvm_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_llvm_type(a_type_ptr ty, llvm::Type** out_ty) noexcept {
    if (g_coro_type_fail > 0) {
      --g_coro_type_fail;
      if (g_coro_type_fail == 0) return llvm_gen_be_error_t::unsupported_type;
    }
    if (ty && ty->kind == tk_void) {
      *out_ty = llvm::Type::getVoidTy(*be_state->context);
      return llvm_gen_be_error_t::ok;
    }
    *out_ty = llvm::Type::getInt32Ty(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `get_typeinfo_global`.
   * @details Provides mock or helper behavior for `get_typeinfo_global` within `tests/test_llvm_coro_atomics.cpp`.
   * @param[in] type Parameter `type` for `get_typeinfo_global`.
   * @param[out] out_ti Parameter `out_ti` for `get_typeinfo_global`.
   * @return Result of `get_typeinfo_global` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_typeinfo_global(a_type_ptr type, llvm::Constant** out_ti) noexcept {
    *out_ti = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
    return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `llvm_lower_statement`.
   * @details Provides mock or helper behavior for `llvm_lower_statement` within `tests/test_llvm_coro_atomics.cpp`.
   * @param[in] stmt Parameter `stmt` for `llvm_lower_statement`.
   * @return Result of `llvm_lower_statement` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_statement(a_statement_ptr stmt) noexcept {
    return llvm_gen_be_error_t::ok;
  }
  
  int g_coro_expr_fail = 0; ///< When non-zero, `llvm_lower_expression` returns `unsupported_expr`.
  int g_coro_expr_null = 0; ///< When non-zero, `llvm_lower_expression` sets `*out_val = nullptr`.
  llvm::Value* g_coro_sret_override_val = nullptr; ///< Optional override value returned when `expr->kind == enk_temp_init`.

  /**
   * @brief Test stub or helper for `llvm_lower_expression`.
   * @details Provides mock or helper behavior for `llvm_lower_expression` within `tests/test_llvm_coro_atomics.cpp`.
   * @param[in] expr Parameter `expr` for `llvm_lower_expression`.
   * @param[out] out_val Parameter `out_val` for `llvm_lower_expression`.
   * @return Result of `llvm_gen_be_error_t`.
   */
  llvm_gen_be_error_t llvm_lower_expression(an_expr_node_ptr expr, llvm::Value** out_val) noexcept {
    if (g_coro_expr_fail < 0) return llvm_gen_be_error_t::unsupported_expr;
    if (g_coro_expr_fail > 0) {
      --g_coro_expr_fail;
      if (g_coro_expr_fail == 0) return llvm_gen_be_error_t::unsupported_expr;
    }
    if (expr && expr->kind == enk_temp_init) {
      *out_val = g_coro_sret_override_val;
      return llvm_gen_be_error_t::ok;
    }
    if (expr == nullptr || g_coro_expr_null) {
       *out_val = nullptr;
       return llvm_gen_be_error_t::ok;
    }
    if (expr->type && expr->type->kind == tk_integer && expr->type->variant.integer.bool_type) {
      *out_val = llvm::ConstantInt::get(llvm::Type::getInt1Ty(*be_state->context), 1);
      return llvm_gen_be_error_t::ok;
    }
    if (g_coro_expr_mode == 1) {
      be_state->prvalue_constructed_in_place = true;
      *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 7);
      return llvm_gen_be_error_t::ok;
    }
    if (g_coro_expr_mode == 2) {
      *out_val = llvm::UndefValue::get(llvm::Type::getVoidTy(*be_state->context));
      return llvm_gen_be_error_t::ok;
    }
    if (g_coro_expr_mode == 3) {
      llvm::Type* p_ty = llvm::PointerType::getUnqual(*be_state->context);
      llvm::StructType* st1 = llvm::StructType::get(*be_state->context, llvm::ArrayRef<llvm::Type*>{p_ty});
      *out_val = llvm::Constant::getNullValue(st1);
      return llvm_gen_be_error_t::ok;
    }
    if (g_coro_expr_mode == 4) {
      llvm::StructType* st2 = llvm::StructType::get(*be_state->context, {llvm::Type::getInt32Ty(*be_state->context), llvm::Type::getInt32Ty(*be_state->context)});
      *out_val = llvm::Constant::getNullValue(st2);
      return llvm_gen_be_error_t::ok;
    }
    if (g_coro_expr_mode == 5) {
      llvm::Type* i32_m = llvm::Type::getInt32Ty(*be_state->context);
      llvm::StructType* st1_int = llvm::StructType::get(*be_state->context, llvm::ArrayRef<llvm::Type*>{i32_m});
      *out_val = llvm::Constant::getNullValue(st1_int);
      return llvm_gen_be_error_t::ok;
    }
    if (g_coro_expr_mode == 6) {
      *out_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), 0);
      return llvm_gen_be_error_t::ok;
    }
    if (g_coro_expr_mode == 7) {
      *out_val = llvm::ConstantInt::get(llvm::Type::getInt8Ty(*be_state->context), 1);
      return llvm_gen_be_error_t::ok;
    }
    if (g_coro_expr_mode == 8) {
      *out_val = llvm::ConstantFP::get(llvm::Type::getFloatTy(*be_state->context), 1.0);
      return llvm_gen_be_error_t::ok;
    }
    if (expr->type) {
        if (expr->type->kind == tk_pointer) {
            *out_val = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
            return llvm_gen_be_error_t::ok;
        } else if (expr->type->kind == tk_integer && expr->type->variant.integer.bool_type) {
            *out_val = llvm::ConstantInt::get(llvm::Type::getInt1Ty(*be_state->context), 1);
            return llvm_gen_be_error_t::ok;
        }
    }
    *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 1);
    return llvm_gen_be_error_t::ok;
  }
}

/**
 * @brief Entry point for the C++20 coroutine and atomic operations unit test suite.
 * @details Exercises coroutine frame setup, `co_await`, `co_yield`, `co_return`, and atomic intrinsics.
 * @return 0 on success.
 */
int main(void) {
  printf("Running test_llvm_coro_atomics...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_coro", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);

  // 1. Negative tests
  assert(llvm_lower_coroutine_stmt(nullptr) == llvm_gen_be_error_t::invalid_argument);
  a_statement wrong_stmt = {};
  wrong_stmt.kind = stmk_empty;
  assert(llvm_lower_coroutine_stmt(&wrong_stmt) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_coroutine_return_stmt(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_coroutine_return_stmt(&wrong_stmt) == llvm_gen_be_error_t::invalid_argument);

  llvm::Value* coro_val = nullptr;
  assert(llvm_lower_coro_suspend(nullptr, &coro_val) == llvm_gen_be_error_t::invalid_argument);
  an_expr_node wrong_expr = {};
  wrong_expr.kind = enk_constant;
  assert(llvm_lower_coro_suspend(&wrong_expr, &coro_val) == llvm_gen_be_error_t::invalid_argument);

  // 2. Coroutine setup statement
  a_statement stmt = {};
  stmt.kind = stmk_coroutine;
  a_coroutine_descr descr = {};
  stmt.variant.coroutine.descr = &descr;
  
  assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

  // Custom allocator
  a_routine new_rt = {};
  new_rt.source_corresp.name = "custom_new";
  descr.new_routine = &new_rt;
  
  llvm::Type* int32_ty = llvm::Type::getInt32Ty(*state.context);
  llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*state.context);
  llvm::FunctionType* custom_new_fty = llvm::FunctionType::get(int8_ptr_ty, {llvm::Type::getInt64Ty(*state.context), int32_ty}, false);
  llvm::Function::Create(custom_new_fty, llvm::Function::ExternalLinkage, "custom_new", state.module.get());
  
  // Create an argument in the parent function
  llvm::FunctionType* f_with_args = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {int32_ty}, false);
  llvm::Function* parent_f = llvm::Function::Create(f_with_args, llvm::Function::ExternalLinkage, "coro_func", state.module.get());
  llvm::BasicBlock* bb2 = llvm::BasicBlock::Create(*state.context, "entry", parent_f);
  state.builder->SetInsertPoint(bb2);
  
  assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

  an_expr_node unhandled_ex = {};
  unhandled_ex.kind = enk_constant;
  descr.unhandled_exception_call = &unhandled_ex;
  a_label fin_lbl = {};
  descr.final_suspend_label = &fin_lbl;
  assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

  // 3. Coroutine return statement
  bb = llvm::BasicBlock::Create(*state.context, "entry_ret", f);
  state.builder->SetInsertPoint(bb);
  stmt.kind = stmk_coroutine_return;
  assert(llvm_lower_coroutine_return_stmt(&stmt) == llvm_gen_be_error_t::ok);
  
  // 4. Coroutine co_await expression
  bb = llvm::BasicBlock::Create(*state.context, "entry_await", f);
  state.builder->SetInsertPoint(bb);

  an_expr_node ready_op = {};
  ready_op.kind = enk_constant;
  an_expr_node resume_op = {};
  resume_op.kind = enk_constant;
  an_expr_node suspend_op = {};
  suspend_op.kind = enk_constant;

  ready_op.next = &resume_op;
  resume_op.next = &suspend_op;

  an_expr_node await_expr = {};
  await_expr.kind = enk_await;
  await_expr.variant.await_info.ready_resume_suspend = &ready_op;
  assert(llvm_lower_coro_suspend(&await_expr, &coro_val) == llvm_gen_be_error_t::ok);
  assert(coro_val != nullptr);

  // Symmetric transfer await_suspend (pointer)
  a_type ptr_ty = {};
  ptr_ty.kind = tk_pointer;
  an_expr_node suspend_ptr_op = {};
  suspend_ptr_op.kind = enk_constant;
  suspend_ptr_op.type = &ptr_ty;
  resume_op.next = &suspend_ptr_op;
  bb = llvm::BasicBlock::Create(*state.context, "entry_await_ptr", f);
  state.builder->SetInsertPoint(bb);
  assert(llvm_lower_coro_suspend(&await_expr, &coro_val) == llvm_gen_be_error_t::ok);

  // Bool await_suspend (boolean)
  a_type bool_ty = {};
  bool_ty.kind = tk_integer;
  bool_ty.variant.integer.bool_type = TRUE;
  an_expr_node suspend_bool_op = {};
  suspend_bool_op.kind = enk_constant;
  suspend_bool_op.type = &bool_ty;
  resume_op.next = &suspend_bool_op;
  bb = llvm::BasicBlock::Create(*state.context, "entry_await_bool", f);
  state.builder->SetInsertPoint(bb);
  assert(llvm_lower_coro_suspend(&await_expr, &coro_val) == llvm_gen_be_error_t::ok);

  // 5. Coroutine co_yield expression
  bb = llvm::BasicBlock::Create(*state.context, "entry_yield", f);
  state.builder->SetInsertPoint(bb);

  an_expr_node yield_expr = {};
  yield_expr.kind = enk_yield;
  yield_expr.variant.await_info.ready_resume_suspend = &ready_op;
  assert(llvm_lower_coro_suspend(&yield_expr, &coro_val) == llvm_gen_be_error_t::ok);
  assert(coro_val != nullptr);

  // Test target-specific global operator new fallback (MSVC 64-bit, MSVC 32-bit, Itanium 32-bit)
  descr.new_routine = nullptr;
  EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
  bb = llvm::BasicBlock::Create(*state.context, "entry_msvc64", f);
  state.builder->SetInsertPoint(bb);
  stmt.kind = stmk_coroutine;
  assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
  assert(state.module->getFunction("??2@YAPEAX_K@Z") != nullptr);

  EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "i686-pc-windows-msvc");
  bb = llvm::BasicBlock::Create(*state.context, "entry_msvc32", f);
  state.builder->SetInsertPoint(bb);
  assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
  assert(state.module->getFunction("??2@YAPAXI@Z") != nullptr);

  EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "i686-pc-linux-gnu");
  bb = llvm::BasicBlock::Create(*state.context, "entry_itanium32", f);
  state.builder->SetInsertPoint(bb);
  assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
  assert(state.module->getFunction("_Znwj") != nullptr);

  assert(llvm_lower_coro_halo_hints(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_coro_halo_hints(f) == llvm_gen_be_error_t::ok);

  // 6. Exhaustive branch coverage for llvm_gen_be_coro.cpp
  {
    assert(llvm_lower_coro_suspend(&await_expr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    a_statement null_descr_stmt = {};
    null_descr_stmt.kind = stmk_coroutine;
    null_descr_stmt.variant.coroutine.descr = nullptr;
    assert(llvm_lower_coroutine_stmt(&null_descr_stmt) == llvm_gen_be_error_t::invalid_argument);

    // Promise in local_vars and not in local_vars (with type == nullptr and type != nullptr)
    a_variable prom_var = {}, unmapped_prom = {}, typed_prom = {};
    typed_prom.type = &bool_ty;
    state.local_vars[&prom_var] = state.builder->CreateAlloca(int32_ty, nullptr, "prom");
    descr.promise = &unmapped_prom;
    bb = llvm::BasicBlock::Create(*state.context, "entry_prom1", f);
    state.builder->SetInsertPoint(bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

    descr.promise = &typed_prom;
    bb = llvm::BasicBlock::Create(*state.context, "entry_prom_typed", f);
    state.builder->SetInsertPoint(bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

    // Test this_param_copy and parameter_copies (both when param_idx < arg_size and >= arg_size)
    a_variable this_copy = {}, pcopy1 = {}, pcopy2 = {};
    pcopy1.next = &pcopy2;
    descr.this_param_copy = &this_copy;
    descr.parameter_copies = &pcopy1;
    // First in 0-arg function `f` (param_idx >= func->arg_size())
    bb = llvm::BasicBlock::Create(*state.context, "entry_pcopy_0arg", f);
    state.builder->SetInsertPoint(bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    // Next in 1-arg function `parent_f` (this_param_copy < arg_size, pcopy1 >= arg_size)
    bb2 = llvm::BasicBlock::Create(*state.context, "entry_pcopy_1arg", parent_f);
    state.builder->SetInsertPoint(bb2);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    descr.this_param_copy = nullptr;
    // Now in 1-arg function `parent_f` with this_param_copy == nullptr (pcopy1 < arg_size, pcopy2 >= arg_size)
    bb2 = llvm::BasicBlock::Create(*state.context, "entry_pcopy_1arg_nothis", parent_f);
    state.builder->SetInsertPoint(bb2);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    descr.parameter_copies = nullptr;

    // Test get_return_object_call, initial_suspend_call, and final_suspend_call (ok and error paths, label cached vs uncached)
    an_expr_node gro_call = {}, init_susp_call = {}, fin_susp_call = {};
    gro_call.kind = enk_constant;
    init_susp_call.kind = enk_constant;
    fin_susp_call.kind = enk_constant;
    descr.get_return_object_call = &gro_call;
    descr.initial_suspend_call = &init_susp_call;
    descr.final_suspend_call = &fin_susp_call;
    state.label_blocks.clear(); // uncached final_suspend_label
    bb = llvm::BasicBlock::Create(*state.context, "entry_lifecycle_ok1", f);
    state.builder->SetInsertPoint(bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    // Second call hits cached final_suspend_label
    bb = llvm::BasicBlock::Create(*state.context, "entry_lifecycle_ok2", f);
    state.builder->SetInsertPoint(bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    // final_suspend_call != nullptr && final_suspend_label == nullptr
    descr.final_suspend_label = nullptr;
    bb = llvm::BasicBlock::Create(*state.context, "entry_lifecycle_nolbl", f);
    state.builder->SetInsertPoint(bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    descr.final_suspend_label = &fin_lbl;

    // Error paths on get_return_object_call (1), initial_suspend_call (2), final_suspend_call (3)
    g_coro_expr_fail = 1;
    bb = llvm::BasicBlock::Create(*state.context, "entry_gro_call_err", f);
    state.builder->SetInsertPoint(bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::unsupported_expr);
    g_coro_expr_fail = 2;
    bb = llvm::BasicBlock::Create(*state.context, "entry_init_susp_err", f);
    state.builder->SetInsertPoint(bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::unsupported_expr);
    g_coro_expr_fail = 3;
    bb = llvm::BasicBlock::Create(*state.context, "entry_fin_susp_err", f);
    state.builder->SetInsertPoint(bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::unsupported_expr);
    g_coro_expr_fail = 0;
    descr.get_return_object_call = nullptr;
    descr.initial_suspend_call = nullptr;
    descr.final_suspend_call = nullptr;

    descr.promise = &prom_var;
    // Custom new_routine with null name, non-existent function, and multi-arg casting (IntCast, BitOrPointerCast, default null arg)
    a_routine new_null_name = {};
    new_null_name.source_corresp.name = nullptr;
    descr.new_routine = &new_null_name;
    bb = llvm::BasicBlock::Create(*state.context, "entry_new_null", f);
    state.builder->SetInsertPoint(bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

    llvm::Type* i64_ty = llvm::Type::getInt64Ty(*state.context);
    llvm::Type* f32_ty = llvm::Type::getFloatTy(*state.context);
    llvm::FunctionType* cast_new_fty = llvm::FunctionType::get(int8_ptr_ty, {i64_ty, i64_ty, i64_ty, f32_ty, int32_ty}, false);
    llvm::Function::Create(cast_new_fty, llvm::Function::ExternalLinkage, "custom_new_cast", state.module.get());
    new_null_name.source_corresp.name = (char*)"custom_new_cast";

    // Non-void return coroutine function with 3 args (float, int32, int32) -> casts arg0 from float to i64, arg1 from i32 to i64 (IntCast), arg2 from i32 to float, arg3 defaults to 0!
    llvm::FunctionType* nonvoid_coro_fty = llvm::FunctionType::get(int8_ptr_ty, {f32_ty, int32_ty, int32_ty}, false);
    llvm::Function* nonvoid_f = llvm::Function::Create(nonvoid_coro_fty, llvm::Function::ExternalLinkage, "nonvoid_coro", state.module.get());
    llvm::BasicBlock* nv_bb = llvm::BasicBlock::Create(*state.context, "entry", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);

    // Custom delete_routine (1 arg, 2 args, 0 args, non-existent, and null name)
    a_routine del_rt = {};
    del_rt.source_corresp.name = (char*)"custom_del_2";
    llvm::FunctionType* del2_fty = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {int8_ptr_ty, i64_ty}, false);
    llvm::Function::Create(del2_fty, llvm::Function::ExternalLinkage, "custom_del_2", state.module.get());
    descr.delete_routine = &del_rt;

    // alloc_failure_gro_call returning i32 in ptr-returning function (casts fail_val), returning i32 in void-returning function (f), returning ptr (exact match), returning null, and failing
    an_expr_node gro_expr = {};
    gro_expr.kind = enk_constant;
    descr.alloc_failure_gro_call = &gro_expr;
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

    // Also test alloc_failure_gro_call in void-returning function f (and unhandled_exception_call != nullptr with final_suspend_label == nullptr)
    descr.final_suspend_label = nullptr;
    bb = llvm::BasicBlock::Create(*state.context, "entry_gro_void", f);
    state.builder->SetInsertPoint(bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    descr.final_suspend_label = &fin_lbl;

    gro_expr.type = &ptr_ty; // exact match with nonvoid_f return type (int8_ptr_ty)
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_gro_ptr", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

    g_coro_expr_null = 1; // fail_val == nullptr
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_gro_null", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    g_coro_expr_null = 0;

    g_coro_expr_fail = 1; // alloc_failure_gro_call expression error
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_gro_err", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::unsupported_expr);
    g_coro_expr_fail = 0;
    descr.alloc_failure_gro_call = nullptr;

    // Custom delete_routine with 1 arg, 0 args, non-existent, and null name
    llvm::FunctionType* del1_fty = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {int8_ptr_ty}, false);
    llvm::Function::Create(del1_fty, llvm::Function::ExternalLinkage, "custom_del_1", state.module.get());
    del_rt.source_corresp.name = (char*)"custom_del_1";
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_del1", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

    llvm::FunctionType* del0_fty = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {}, false);
    llvm::Function::Create(del0_fty, llvm::Function::ExternalLinkage, "custom_del_0", state.module.get());
    del_rt.source_corresp.name = (char*)"custom_del_0";
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_del0", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

    del_rt.source_corresp.name = (char*)"nonexistent_del";
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_del_no", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

    del_rt.source_corresp.name = nullptr;
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_del_null", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    descr.delete_routine = nullptr;

    // Unhandled exception error path on both Itanium and MSVC, and final_suspend_label not yet in label_blocks on MSVC
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
    g_coro_expr_fail = 1;
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_uh_err_it", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::unsupported_expr);

    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    g_coro_expr_fail = 1;
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_uh_err_ms", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::unsupported_expr);
    g_coro_expr_fail = 0;

    state.label_blocks.clear(); // force it == label_blocks.end() on MSVC
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_uh_ok_ms", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

    descr.unhandled_exception_call = nullptr;
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_no_uh", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    descr.final_suspend_label = nullptr;

    // Coroutine return statement: with stmt->expr (ok & error), non-void function, and active_hdl in same function vs nullptr
    a_statement ret_stmt = {};
    ret_stmt.kind = stmk_coroutine_return;
    ret_stmt.expr = &gro_expr;
    assert(llvm_lower_coroutine_return_stmt(&ret_stmt) == llvm_gen_be_error_t::ok); // active_hdl in nonvoid_f!

    g_coro_expr_fail = 1;
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_ret_err", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_return_stmt(&ret_stmt) == llvm_gen_be_error_t::unsupported_expr);
    g_coro_expr_fail = 0;

    state.current_coro_handle = nullptr;
    assert(llvm_lower_coroutine_return_stmt(&ret_stmt) == llvm_gen_be_error_t::ok);

    // Coroutine suspend:
    // 1. ready_expr == nullptr, resume_expr == nullptr, suspend_expr == nullptr, in nonvoid_f with current_coro_cleanup_bb in nonvoid_f
    an_expr_node bare_await = {};
    bare_await.kind = enk_await;
    bare_await.variant.await_info.ready_resume_suspend = nullptr;
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_bare_await", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::ok);

    // 2. ready_expr is bool (i1), ready_expr returns nullptr, ready_expr fails
    ready_op.type = &bool_ty;
    ready_op.next = nullptr; // resume_expr == nullptr, suspend_expr == nullptr
    bare_await.variant.await_info.ready_resume_suspend = &ready_op;
    state.current_coro_cleanup_bb = nullptr;
    state.current_coro_handle = state.builder->CreateAlloca(int32_ty); // instruction in nonvoid_f!
    assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::ok);

    // Also test when current_coro_handle is in a DIFFERENT function (f) while lowering in nonvoid_f
    llvm::BasicBlock* f_tmp_bb = llvm::BasicBlock::Create(*state.context, "tmp_hdl", f);
    state.builder->SetInsertPoint(f_tmp_bb);
    llvm::Value* hdl_in_f = state.builder->CreateAlloca(int32_ty);
    state.builder->SetInsertPoint(nv_bb);
    state.current_coro_handle = hdl_in_f;

    g_coro_expr_null = 1; // is_ready == nullptr, susp_val == nullptr, res_val == nullptr
    ready_op.next = &resume_op;
    resume_op.next = &suspend_op;
    assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::ok);
    g_coro_expr_null = 0;

    // Error paths on ready_expr (call 1), suspend_expr (call 2), and resume_expr (call 3), plus current_coro_handle == nullptr
    state.current_coro_handle = nullptr;
    assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::ok);
    g_coro_expr_fail = 1;
    assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::unsupported_expr);
    g_coro_expr_fail = 2;
    assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::unsupported_expr);
    g_coro_expr_fail = 3;
    assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::unsupported_expr);
    g_coro_expr_fail = 0;

    // Test get_return_object_call in nonvoid_f (gro_val cast to return type, exact match, and gro_val == nullptr) + suspend loading current_coro_gro_slot (in same function vs different function) + final_suspend_bb set
    descr.get_return_object_call = &gro_call;
    gro_call.type = nullptr; // returns i32, cast to int8_ptr_ty
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_gro_slot_cast", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    assert(state.current_coro_gro_slot != nullptr);
    state.current_coro_suspend_bb = nv_bb;
    assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::ok);
    state.current_coro_suspend_bb = nullptr;

    // Also test current_coro_gro_slot belonging to a different function (f) when suspending in nonvoid_f
    llvm::BasicBlock* f_gro_bb = llvm::BasicBlock::Create(*state.context, "tmp_gro", f);
    state.builder->SetInsertPoint(f_gro_bb);
    llvm::Value* gro_in_f = state.builder->CreateAlloca(int8_ptr_ty);
    state.builder->SetInsertPoint(nv_bb);
    state.current_coro_gro_slot = gro_in_f;
    assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::ok);

    // gro_call exact type match in nonvoid_f, and gro_call returning nullptr
    gro_call.type = &ptr_ty;
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_gro_slot_exact", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    g_coro_expr_null = 1;
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_gro_slot_null", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    g_coro_expr_null = 0;
    descr.get_return_object_call = nullptr;

    // Over-aligned coroutine promise (alignment = 32 > 16) on Itanium and MSVC
    a_variable typed_prom32 = {};
    bool_ty.alignment = 32;
    typed_prom32.type = &bool_ty;
    descr.promise = &typed_prom32;
    descr.new_routine = nullptr;
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_overaligned_it", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_overaligned_ms", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    bool_ty.alignment = 1;

    // Custom 3-arg aligned delete_routine(void*, size_t, align_val_t)
    llvm::FunctionType* del3_fty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*state.context), {int8_ptr_ty, i64_ty, i64_ty}, false);
    llvm::Function::Create(del3_fty, llvm::Function::ExternalLinkage, "custom_del_3", state.module.get());
    del_rt.source_corresp.name = (char*)"custom_del_3";
    descr.delete_routine = &del_rt;
    nv_bb = llvm::BasicBlock::Create(*state.context, "entry_del3", nonvoid_f);
    state.builder->SetInsertPoint(nv_bb);
    assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
    descr.delete_routine = nullptr;

    // Direct validation of helper error branches and llvm_lower_coro_noop
    uint32_t copied_cnt = 0;
    assert(llvm_emit_coro_parameter_copies(nullptr, nonvoid_f, &copied_cnt) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_emit_coro_parameter_copies(&descr, nullptr, &copied_cnt) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_emit_coro_parameter_copies(&descr, nonvoid_f, nullptr) == llvm_gen_be_error_t::invalid_argument);

    llvm::Value* alloc_out = nullptr;
    llvm::Value* dummy_sz = llvm::ConstantInt::get(i64_ty, 64);
    assert(llvm_lower_coro_promise_alloc(nullptr, nonvoid_f, dummy_sz, 0, &alloc_out) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_coro_promise_alloc(&descr, nullptr, dummy_sz, 0, &alloc_out) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_coro_promise_alloc(&descr, nonvoid_f, nullptr, 0, &alloc_out) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_coro_promise_alloc(&descr, nonvoid_f, dummy_sz, 0, nullptr) == llvm_gen_be_error_t::invalid_argument);

    llvm::BasicBlock* lpad_out = nullptr;
    assert(llvm_emit_coro_unhandled_exception_landing_pad(nullptr, nonvoid_f, &lpad_out) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_emit_coro_unhandled_exception_landing_pad(&descr, nullptr, &lpad_out) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_emit_coro_unhandled_exception_landing_pad(&descr, nonvoid_f, nullptr) == llvm_gen_be_error_t::invalid_argument);
    descr.unhandled_exception_call = nullptr;
    assert(llvm_emit_coro_unhandled_exception_landing_pad(&descr, nonvoid_f, &lpad_out) == llvm_gen_be_error_t::invalid_argument);
    descr.unhandled_exception_call = &unhandled_ex;
    descr.final_suspend_label = nullptr;
    assert(llvm_emit_coro_unhandled_exception_landing_pad(&descr, nonvoid_f, &lpad_out) == llvm_gen_be_error_t::invalid_argument);

    assert(llvm_lower_coro_noop(nullptr) == llvm_gen_be_error_t::invalid_argument);
    llvm::Value* noop_hdl = nullptr;
    assert(llvm_lower_coro_noop(&noop_hdl) == llvm_gen_be_error_t::ok);
    assert(noop_hdl != nullptr);

    // 7. Complete branch & OOM coverage across llvm_gen_be_coro.cpp
    {
      // Unhandled exception landing pad with label NOT yet in label_blocks (Itanium & MSVC)
      a_label fresh_fs_lbl1 = {}, fresh_fs_lbl2 = {};
      descr.unhandled_exception_call = &unhandled_ex;
      descr.final_suspend_label = &fresh_fs_lbl1;
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
      assert(llvm_emit_coro_unhandled_exception_landing_pad(&descr, nonvoid_f, &lpad_out) == llvm_gen_be_error_t::ok);
      descr.final_suspend_label = &fresh_fs_lbl2;
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
      assert(llvm_emit_coro_unhandled_exception_landing_pad(&descr, nonvoid_f, &lpad_out) == llvm_gen_be_error_t::ok);
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");

      // Unsized promise type (tk_void) pre-populated in local_vars
      a_type void_prom_ty = {}; void_prom_ty.kind = tk_void;
      a_variable void_prom_var = {}; void_prom_var.type = &void_prom_ty;
      state.local_vars[&void_prom_var] = state.builder->CreateAlloca(int32_ty);
      descr.promise = &void_prom_var;
      nv_bb = llvm::BasicBlock::Create(*state.context, "entry_void_prom", nonvoid_f);
      state.builder->SetInsertPoint(nv_bb);
      assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
      descr.promise = &typed_prom32;

      // descr->handle in local_vars, not in local_vars with type, and not in local_vars without type
      a_variable hdl_var = {};
      hdl_var.type = nullptr;
      descr.handle = &hdl_var;
      nv_bb = llvm::BasicBlock::Create(*state.context, "entry_hdl_notype", nonvoid_f);
      state.builder->SetInsertPoint(nv_bb);
      assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

      hdl_var.type = &ptr_ty;
      nv_bb = llvm::BasicBlock::Create(*state.context, "entry_hdl_typed", nonvoid_f);
      state.builder->SetInsertPoint(nv_bb);
      g_coro_type_fail = 2; // fail on get_llvm_type(descr->handle->type)
      assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::unsupported_type);
      g_coro_type_fail = 0;

      nv_bb = llvm::BasicBlock::Create(*state.context, "entry_hdl_typed_ok", nonvoid_f);
      state.builder->SetInsertPoint(nv_bb);
      assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok); // creates & caches hdl_var
      nv_bb = llvm::BasicBlock::Create(*state.context, "entry_hdl_cached", nonvoid_f);
      state.builder->SetInsertPoint(nv_bb);
      assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok); // hits local_vars cache
      descr.handle = nullptr;

      // body_generated == TRUE (with fresh final_suspend_label)
      a_label bg_lbl = {};
      an_expr_node fs_expr = {}; fs_expr.kind = enk_await;
      descr.body_generated = TRUE;
      descr.final_suspend_label = &bg_lbl;
      descr.final_suspend_call = &fs_expr;
      nv_bb = llvm::BasicBlock::Create(*state.context, "entry_body_gen", nonvoid_f);
      state.builder->SetInsertPoint(nv_bb);
      assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
      assert(state.current_coro_final_await_expr == &fs_expr);
      // Lower fs_expr as final await (expr == current_coro_final_await_expr -> true & false)
      assert(llvm_lower_coro_suspend(&fs_expr, &coro_val) == llvm_gen_be_error_t::ok);
      assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::ok);
      descr.body_generated = FALSE;

      // sret coroutine ramp function: constructed_in_place, non-in-place store, and void gro_val
      llvm::FunctionType* sret_coro_fty = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {int8_ptr_ty}, false);
      llvm::Function* sret_coro_f = llvm::Function::Create(sret_coro_fty, llvm::Function::ExternalLinkage, "sret_coro_f", state.module.get());
      sret_coro_f->addParamAttr(0, llvm::Attribute::getWithStructRetType(*state.context, int32_ty));
      descr.get_return_object_call = &gro_call;
      gro_call.type = nullptr;

      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "sret1", sret_coro_f));
      assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok); // !constructed_in_sret && non-void -> stores to arg 0

      g_coro_expr_mode = 1; // prvalue_constructed_in_place = true
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "sret2", sret_coro_f));
      assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

      g_coro_expr_mode = 2; // void gro_val
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "sret3", sret_coro_f));
      assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
      g_coro_expr_mode = 0;

      g_coro_expr_null = 1; // gro_val == nullptr on sret
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "sret4", sret_coro_f));
      assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
      g_coro_expr_null = 0;

      // Cover parameter_copies sentinel stop on promise, init_await_resume, and handle (L49)
      {
        a_variable pc_prom = {}, pc_iaw = {}, pc_hdl = {};
        a_coroutine_descr pc_d = {};
        pc_d.promise = &pc_prom;
        pc_d.init_await_resume = &pc_iaw;
        pc_d.handle = &pc_hdl;
        uint32_t pc_cnt = 0;
        pc_d.parameter_copies = &pc_prom;
        assert(llvm_emit_coro_parameter_copies(&pc_d, sret_coro_f, &pc_cnt) == llvm_gen_be_error_t::ok);
        pc_d.parameter_copies = &pc_iaw;
        assert(llvm_emit_coro_parameter_copies(&pc_d, sret_coro_f, &pc_cnt) == llvm_gen_be_error_t::ok);
        pc_d.parameter_copies = &pc_hdl;
        assert(llvm_emit_coro_parameter_copies(&pc_d, sret_coro_f, &pc_cnt) == llvm_gen_be_error_t::ok);
      }

      // Cover sret GRO comma-unwrapping & eok_call memcpy fallback branches (L420-L437)
      {
        an_expr_node c_lhs = {}, sret_callee = {}, sret_arg = {}, inner_call = {}, outer_comma = {};
        sret_callee.kind = enk_routine;
        sret_arg.kind = enk_temp_init;
        inner_call.kind = enk_operation;
        inner_call.variant.operation.kind = eok_call;
        outer_comma.kind = enk_operation;
        outer_comma.variant.operation.kind = eok_comma;

        g_coro_expr_mode = 2; // void gro_val
        descr.get_return_object_call = &outer_comma;

        // 1) comma with operands == nullptr
        outer_comma.variant.operation.operands = nullptr;
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "sret_c0", sret_coro_f));
        assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

        // 2) comma with operands->next == nullptr
        c_lhs.next = nullptr;
        outer_comma.variant.operation.operands = &c_lhs;
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "sret_c1", sret_coro_f));
        assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

        // 3) comma unwraps to eok_call with operands == nullptr, then operands->next == nullptr
        c_lhs.next = &inner_call;
        inner_call.variant.operation.operands = nullptr;
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "sret_c2", sret_coro_f));
        assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

        sret_callee.next = nullptr;
        inner_call.variant.operation.operands = &sret_callee;
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "sret_c3", sret_coro_f));
        assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

        // 4) eok_call with sret_arg_node: error on 2nd expr call, null ptr, non-ptr, == arg(0), unsized sret, and valid sized memcpy
        sret_callee.next = &sret_arg;
        g_coro_expr_fail = 2;
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "sret_c4_err", sret_coro_f));
        assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::unsupported_expr);
        g_coro_expr_fail = 0;

        g_coro_sret_override_val = nullptr;
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "sret_c5_null", sret_coro_f));
        assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

        g_coro_sret_override_val = llvm::ConstantInt::get(int32_ty, 0);
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "sret_c6_nonptr", sret_coro_f));
        assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

        g_coro_sret_override_val = sret_coro_f->getArg(0);
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "sret_c7_same", sret_coro_f));
        assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

        llvm::AllocaInst* tmp_gro_alloca = state.builder->CreateAlloca(int32_ty);
        g_coro_sret_override_val = tmp_gro_alloca;
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "sret_c8_memcpy", sret_coro_f));
        assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

        // Unsized sret element type (void) -> sret_elem_ty->isSized() False
        llvm::Function* sret_void_f = llvm::Function::Create(sret_coro_fty, llvm::Function::ExternalLinkage, "sret_void_coro_f", state.module.get());
        sret_void_f->addParamAttr(0, llvm::Attribute::getWithStructRetType(*state.context, llvm::Type::getVoidTy(*state.context)));
        llvm::BasicBlock* sv_bb = llvm::BasicBlock::Create(*state.context, "sret_c9_unsized", sret_void_f);
        state.builder->SetInsertPoint(sv_bb);
        g_coro_sret_override_val = state.builder->CreateAlloca(int32_ty);
        assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

        g_coro_sret_override_val = nullptr;
        g_coro_expr_mode = 0;
        descr.get_return_object_call = &gro_call;
      }

      // Cover destroy-block active_lowered_eh_cleanup destructor chains in llvm_lower_coro_suspend (L738-L762)
      {
        llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false),
                               llvm::Function::ExternalLinkage, "coro_dtor_0arg", state.module.get());
        llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {int8_ptr_ty}, false),
                               llvm::Function::ExternalLinkage, "coro_dtor_1arg", state.module.get());
        llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {int8_ptr_ty, int32_ty}, false),
                               llvm::Function::ExternalLinkage, "coro_dtor_2int", state.module.get());
        llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {int8_ptr_ty, int8_ptr_ty}, false),
                               llvm::Function::ExternalLinkage, "coro_dtor_2ptr", state.module.get());

        a_routine dr_null = {}, dr_0 = {}, dr_1 = {}, dr_2i = {}, dr_2p = {}, dr_err = {};
        dr_null.source_corresp.name = ""; // dtor_fn == nullptr
        dr_0.source_corresp.name = "coro_dtor_0arg";
        dr_1.source_corresp.name = "coro_dtor_1arg";
        dr_2i.source_corresp.name = "coro_dtor_2int";
        dr_2p.source_corresp.name = "coro_dtor_2ptr";
        a_type dr_err_ty = {}; dr_err_ty.kind = tk_routine;
        dr_err.source_corresp.name = "coro_dtor_unseen_err";
        dr_err.type = &dr_err_ty;

        a_variable cv_unmapped = {}, cv_null_val = {}, cv_prom_match = {}, cv_live = {};
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "coro_eh_destroy_entry", nonvoid_f));
        llvm::AllocaInst* prom_slot_eh = state.builder->CreateAlloca(int32_ty);
        llvm::AllocaInst* live_slot_eh = state.builder->CreateAlloca(int32_ty);
        state.current_coro_promise = prom_slot_eh;
        state.local_vars[&cv_null_val] = nullptr;
        state.local_vars[&cv_prom_match] = prom_slot_eh;
        state.local_vars[&cv_live] = live_slot_eh;

        a_dynamic_init cd1 = {}, cd2 = {}, cd3 = {}, cd4 = {}, cd5 = {}, cd6 = {}, cd7 = {}, cd8 = {}, cd9 = {}, cd10 = {};
        an_object_lifetime parent_life = {};
        cd1.destructor = nullptr; cd1.variable = &cv_live; cd1.next_in_destruction_list = &cd2;
        cd2.destructor = &dr_1; cd2.variable = nullptr; cd2.next_in_destruction_list = &cd3;
        cd3.destructor = &dr_1; cd3.variable = &cv_unmapped; cd3.next_in_destruction_list = &cd4;
        cd4.destructor = &dr_1; cd4.variable = &cv_null_val; cd4.next_in_destruction_list = &cd5;
        cd5.destructor = &dr_1; cd5.variable = &cv_prom_match; cd5.next_in_destruction_list = &cd6;
        cd6.destructor = &dr_null; cd6.variable = &cv_live; cd6.next_in_destruction_list = &cd7;
        cd7.destructor = &dr_0; cd7.variable = &cv_live; cd7.next_in_destruction_list = &cd8;
        cd8.destructor = &dr_1; cd8.variable = &cv_live; cd8.next_in_destruction_list = nullptr;
        cd8.lifetime = &parent_life;
        parent_life.parent_destruction_sublist = &cd9;
        cd9.destructor = &dr_2i; cd9.variable = &cv_live; cd9.next_in_destruction_list = &cd10;
        cd10.destructor = &dr_2p; cd10.variable = &cv_live; cd10.next_in_destruction_list = nullptr;
        cd10.lifetime = nullptr;

        state.active_lowered_eh_cleanup = &cd1;
        assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::ok);

        // Also test llvm_get_or_declare_routine failure at L750
        a_dynamic_init cd_err = {};
        cd_err.destructor = &dr_err;
        cd_err.variable = &cv_live;
        state.active_lowered_eh_cleanup = &cd_err;
        g_coro_type_fail = 1;
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "coro_eh_destroy_err", nonvoid_f));
        assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::unsupported_type);
        g_coro_type_fail = 0;
        state.active_lowered_eh_cleanup = nullptr;
      }

      // Float ramp function with ptr and i32 gro_val (fn_ret_ty->isIntegerTy() == false)
      llvm::Function* flt_coro_f = llvm::Function::Create(
          llvm::FunctionType::get(llvm::Type::getFloatTy(*state.context), false),
          llvm::Function::ExternalLinkage, "flt_coro_f", state.module.get());
      gro_call.type = &ptr_ty;
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "flt_ptr", flt_coro_f));
      assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
      gro_call.type = nullptr;
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "flt_int", flt_coro_f));
      assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

      // Ramp return coercions in i64-returning ramp function: aggregate gro_val, ptr->i64, i32->i64, float->i64
      llvm::Function* i64_coro_f = llvm::Function::Create(
          llvm::FunctionType::get(i64_ty, false), llvm::Function::ExternalLinkage, "i64_coro_f", state.module.get());
      g_coro_expr_mode = 4; // aggregate struct gro_val
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "c_agg", i64_coro_f));
      assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

      llvm::StructType* pair_ret_ty = llvm::StructType::get(*state.context, {int32_ty, int32_ty});
      llvm::Function* st_coro_f = llvm::Function::Create(
          llvm::FunctionType::get(pair_ret_ty, false), llvm::Function::ExternalLinkage, "st_coro_f", state.module.get());
      g_coro_expr_mode = 0; // i32 gro_val into aggregate fn_ret_ty
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "c_fnagg", st_coro_f));
      assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

      gro_call.type = &ptr_ty; // ptr gro_val -> PtrToInt to i64
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "c_p2i", i64_coro_f));
      assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
      gro_call.type = nullptr; // i32 gro_val -> IntCast to i64
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "c_i2i", i64_coro_f));
      assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);

      llvm::Function* i32_coro_f = llvm::Function::Create(
          llvm::FunctionType::get(int32_ty, false), llvm::Function::ExternalLinkage, "i32_coro_f", state.module.get());
      g_coro_expr_mode = 8; // float gro_val -> BitOrPointerCast to i32
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "c_f2i", i32_coro_f));
      assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
      g_coro_expr_mode = 0;

      // co_return fallback branches when final_suspend_bb & cleanup_bb are null in non-void function (gro_slot in func, in other func, and null)
      a_statement co_ret_s = {}; co_ret_s.kind = stmk_coroutine_return;
      state.current_coro_final_suspend_bb = nullptr;
      state.current_coro_cleanup_bb = nullptr;

      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "cr1", i32_coro_f));
      assert(llvm_lower_coroutine_return_stmt(&co_ret_s) == llvm_gen_be_error_t::ok); // gro_slot in i32_coro_f

      state.current_coro_gro_slot = gro_in_f; // in different function f
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "cr2", i32_coro_f));
      assert(llvm_lower_coroutine_return_stmt(&co_ret_s) == llvm_gen_be_error_t::ok);

      state.current_coro_gro_slot = nullptr;
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "cr3", i32_coro_f));
      assert(llvm_lower_coroutine_return_stmt(&co_ret_s) == llvm_gen_be_error_t::ok);

      // co_await: operand != nullptr (ok + fail), symmetric transfer {ptr}, {i32,i32}, {i32}, i64 struct/class/int, i8 bool, and current_coro_end_bb == nullptr in non-void fn
      an_expr_node aw_operand = {};
      bare_await.variant.await_info.operand = &aw_operand;
      state.current_coro_end_bb = nullptr;
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "aw_op_fail", i32_coro_f));
      g_coro_expr_fail = 1;
      assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::unsupported_expr);
      g_coro_expr_fail = 0;

      // current_coro_end_bb == nullptr in nonvoid fn with gro_slot == nullptr, in other fn, and in same fn
      state.current_coro_gro_slot = nullptr;
      assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::ok);
      state.current_coro_gro_slot = gro_in_f;
      assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::ok);
      llvm::BasicBlock* aw_slot_bb = llvm::BasicBlock::Create(*state.context, "aw_slot", i32_coro_f);
      state.builder->SetInsertPoint(aw_slot_bb);
      state.current_coro_gro_slot = state.builder->CreateAlloca(int32_ty);
      assert(llvm_lower_coro_suspend(&bare_await, &coro_val) == llvm_gen_be_error_t::ok);
      bare_await.variant.await_info.operand = nullptr;

      // Symmetric transfer & i8 bool branches in suspend_expr (ready_expr = nullptr so only suspend_expr is evaluated in mode)
      an_expr_node sym_susp = {}, sym_aw = {};
      sym_aw.kind = enk_await;
      sym_aw.variant.await_info.ready_resume_suspend = nullptr;
      // Ready -> resume -> suspend chain where ready_op has bool_type, resume_op is null-safe, suspend_op uses g_coro_expr_mode
      ready_op.type = &bool_ty;
      ready_op.next = &resume_op;
      resume_op.next = &sym_susp;
      sym_aw.variant.await_info.ready_resume_suspend = &ready_op;

      for (int m : {3, 4, 5, 7}) {
        // Notice g_coro_expr_mode applies to all 3 expressions unless ready_op is checked first — let's clear ready_op.type=nullptr for m=7!
        g_coro_expr_mode = m;
        assert(llvm_lower_coro_suspend(&sym_aw, &coro_val) == llvm_gen_be_error_t::ok);
      }
      a_type aw_st_ty = {}, aw_cls_ty = {}, aw_int_ty = {};
      aw_st_ty.kind = tk_struct; aw_cls_ty.kind = tk_class; aw_int_ty.kind = tk_integer;
      g_coro_expr_mode = 6; // i64 return
      sym_susp.type = &aw_st_ty;
      assert(llvm_lower_coro_suspend(&sym_aw, &coro_val) == llvm_gen_be_error_t::ok);
      sym_susp.type = &aw_cls_ty;
      assert(llvm_lower_coro_suspend(&sym_aw, &coro_val) == llvm_gen_be_error_t::ok);
      sym_susp.type = &aw_int_ty;
      assert(llvm_lower_coro_suspend(&sym_aw, &coro_val) == llvm_gen_be_error_t::ok);
      sym_susp.type = nullptr;
      assert(llvm_lower_coro_suspend(&sym_aw, &coro_val) == llvm_gen_be_error_t::ok);
      g_coro_expr_mode = 0;

      // OOM & type-failure sweeps on llvm_lower_coroutine_stmt
      descr.promise = &void_prom_var;
      void_prom_var.type = &ptr_ty;
      descr.new_routine = &new_rt;
      descr.delete_routine = &del_rt;
      descr.unhandled_exception_call = &unhandled_ex;
      descr.final_suspend_label = &fresh_fs_lbl1;
      state.local_vars.erase(&void_prom_var);
      g_coro_type_fail = 1; // fail first get_llvm_type(descr->promise->type)
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "coro_tf1", i32_coro_f));
      assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::unsupported_type);
      g_coro_type_fail = 2; // fail second get_llvm_type(descr->promise->type) at L350
      state.local_vars.erase(&void_prom_var);
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "coro_tf2", i32_coro_f));
      assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::unsupported_type);
      g_coro_type_fail = 0;

      for (int b = 0; b < 16; ++b) {
        state.local_vars.erase(&void_prom_var);
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "coro_oom", i32_coro_f));
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t rc = llvm_lower_coroutine_stmt(&stmt);
        g_llvm_be_oom_countdown = -1;
        if (rc == llvm_gen_be_error_t::ok) break;
      }

      // alloc_failure_gro_call across sret (in-place, non-void store, void, null) and aggregate/scalar return coercion
      an_expr_node fail_gro_expr = {};
      fail_gro_expr.kind = enk_constant;
      descr.alloc_failure_gro_call = &fail_gro_expr;
      descr.unhandled_exception_call = nullptr;
      llvm::StructType* coro_ret_st = llvm::StructType::get(
          *state.context, llvm::ArrayRef<llvm::Type*>{llvm::PointerType::getUnqual(*state.context)});

      // 1) sret function (`fail_has_sret == true`): mode 0 (store), mode 1 (in-place), mode 2 (void), null
      for (int sret_case = 0; sret_case < 4; ++sret_case) {
        llvm::Function* sret_coro_f = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context),
                                    {llvm::PointerType::getUnqual(*state.context)}, false),
            llvm::Function::ExternalLinkage, "coro_sret_fail_" + std::to_string(sret_case), state.module.get());
        sret_coro_f->getArg(0)->addAttr(llvm::Attribute::getWithStructRetType(*state.context, coro_ret_st));
        state.local_vars.erase(&void_prom_var);
        a_label fs_sret_lbl = {};
        descr.final_suspend_label = &fs_sret_lbl;
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "entry", sret_coro_f));
        g_coro_expr_mode = (sret_case == 1) ? 1 : ((sret_case == 2) ? 2 : 0);
        g_coro_expr_null = (sret_case == 3) ? 1 : 0;
        assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
        g_coro_expr_mode = 0;
        g_coro_expr_null = 0;
      }

      // 2) Aggregate fail_val -> i32 fn_ret_ty (mode 3 on i32_coro_f)
      {
        state.local_vars.erase(&void_prom_var);
        a_label fs_agg1_lbl = {};
        descr.final_suspend_label = &fs_agg1_lbl;
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "coro_agg_to_i32", i32_coro_f));
        g_coro_expr_mode = 3;
        assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
        g_coro_expr_mode = 0;
      }

      // 3) Scalar i32 fail_val -> aggregate StructType fn_ret_ty (mode 0 on struct-returning coro with 1 non-sret arg)
      {
        llvm::Function* st_coro_f = llvm::Function::Create(
            llvm::FunctionType::get(coro_ret_st, {llvm::Type::getInt32Ty(*state.context)}, false),
            llvm::Function::ExternalLinkage, "coro_i32_to_agg", state.module.get());
        state.local_vars.erase(&void_prom_var);
        a_label fs_agg2_lbl = {};
        descr.final_suspend_label = &fs_agg2_lbl;
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "entry", st_coro_f));
        g_coro_expr_mode = 0;
        assert(llvm_lower_coroutine_stmt(&stmt) == llvm_gen_be_error_t::ok);
      }
      descr.alloc_failure_gro_call = nullptr;
    }
  }

  printf("PASS\n");
  return 0;
}
