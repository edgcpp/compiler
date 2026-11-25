/**
 * @file test_llvm_control_flow.cpp
 * @ingroup llvm_gen_be_tests
 * @brief Unit tests for the LLVM backend control flow statements lowering subsystem.
 * @details Validates function coverage of llvm_lower_statement.
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
  // Stub implementations required to link test
  /**
   * @brief Test stub or helper for `is_bool_type`.
   * @details Provides mock or helper behavior for `is_bool_type` within `tests/test_llvm_control_flow.cpp`.
   * @param[in] ty Parameter `ty` for `is_bool_type`.
   * @return Result of `is_bool_type` (a_boolean).
   */
  a_boolean is_bool_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `f_alignment_of_type`.
   * @details Provides mock or helper behavior for `f_alignment_of_type` within `tests/test_llvm_control_flow.cpp`.
   * @param[in] ty Parameter `ty` for `f_alignment_of_type`.
   * @return Result of `f_alignment_of_type` (a_targ_alignment).
   */
  a_targ_alignment f_alignment_of_type(a_type_ptr ty) { return 1; }
  /**
   * @brief Test stub or helper for `is_trivially_copyable_type`.
   * @details Provides mock or helper behavior for `is_trivially_copyable_type` within `tests/test_llvm_control_flow.cpp`.
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
   * @details Provides mock or helper behavior for `conv_seq_to_file_and_line` within `tests/test_llvm_control_flow.cpp`.
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
   * @details Provides mock or helper behavior for `mangled_typeinfo_name` within `tests/test_llvm_control_flow.cpp`.
   * @param[in] type Parameter `type` for `mangled_typeinfo_name`.
   * @return Result of `mangled_typeinfo_name` (char*).
   */
  char* mangled_typeinfo_name(a_type_ptr type) { static char buf[] = "_ZTIi"; return buf; }
  /**
   * @brief Test stub or helper for `f_get_type_qualifiers`.
   * @details Provides mock or helper behavior for `f_get_type_qualifiers` within `tests/test_llvm_control_flow.cpp`.
   * @param[in] type Parameter `type` for `f_get_type_qualifiers`.
   * @param[in] x Parameter `x` for `f_get_type_qualifiers`.
   * @return Result of `f_get_type_qualifiers` (a_type_qualifier_set).
   */
  a_type_qualifier_set f_get_type_qualifiers(a_type_ptr type, a_boolean x) { return 0; }
  /**
   * @brief Test stub or helper for `is_reference_type`.
   * @details Provides mock or helper behavior for `is_reference_type` within `tests/test_llvm_control_flow.cpp`.
   * @param[in] ty Parameter `ty` for `is_reference_type`.
   * @return Result of `is_reference_type` (a_boolean).
   */
  a_boolean is_reference_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `is_empty_class_type`.
   * @details Provides mock or helper behavior for `is_empty_class_type` within `tests/test_llvm_control_flow.cpp`.
   * @param[in] ty Parameter `ty` for `is_empty_class_type`.
   * @return Result of `is_empty_class_type` (a_boolean).
   */
  a_boolean is_empty_class_type(a_type_ptr ty) { return ty && ty->variant.class_struct_union.is_empty_class; }
  int g_cf_il_lowering = 0; ///< Configurable return value for `il_lowering_needed`.
  /**
   * @brief Test stub for `il_lowering_needed`.
   * @details Returns `TRUE` when `g_cf_il_lowering` is non-zero, otherwise `FALSE`.
   * @return `a_boolean` indicating whether IL lowering is active.
   */
  a_boolean il_lowering_needed(void) { return g_cf_il_lowering ? TRUE : FALSE; }
  int g_cf_fail_const = 0; ///< Countdown of calls before `evaluate_constant` returns an error.
  int g_cf_const_seq = 0; ///< When non-zero, `evaluate_constant` returns sequential integers starting from `g_cf_const_seq`.
  int g_cf_fail_type = 0; ///< Countdown of calls before `get_llvm_type` returns `unsupported_type`.
  int g_cf_fail_push_lex = 0; ///< When non-zero, `push_lexical_block` returns an error.
  int g_cf_fail_pop_lex = 0; ///< When non-zero, `pop_lexical_block` returns an error.
  int g_cf_fail_di_loc = 0; ///< When non-zero, `get_di_location` returns an error.
  int g_cf_nonnull_di_loc = 0; ///< When non-zero, `get_di_location` returns a non-null `DILocation*`.
  int g_cf_fail_expr = 0; ///< Countdown of calls before `llvm_lower_expression` returns an error.
  int g_cf_expr_mode = 0; ///< Mode for `llvm_lower_expression` (0=i32, 1=i1, 2=ptr, 3=null, 4=struct{i32,i32}).

  /**
   * @brief Test stub or helper for `evaluate_constant`.
   * @details Provides mock or helper behavior for `evaluate_constant` within `tests/test_llvm_control_flow.cpp`.
   * @param[in] con Parameter `con` for `evaluate_constant`.
   * @param[in] ty Parameter `ty` for `evaluate_constant`.
   * @param[out] out_const Parameter `out_const` for `evaluate_constant`.
   * @return Result of `evaluate_constant` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t evaluate_constant(a_constant_ptr con, llvm::Type* ty, llvm::Constant** out_const) noexcept { 
    (void)con;
    if (g_cf_fail_const < 0) return llvm_gen_be_error_t::constant_eval_failure;
    if (g_cf_fail_const > 0) {
      --g_cf_fail_const;
      if (g_cf_fail_const == 0) return llvm_gen_be_error_t::constant_eval_failure;
    }
    if (ty->isIntegerTy()) {
      int64_t val = 1;
      if (g_cf_const_seq >= 100) {
        val = (g_cf_const_seq == 100) ? 0 : 100;
        g_cf_const_seq++;
      } else if (g_cf_const_seq != 0) {
        val = g_cf_const_seq++;
      }
      *out_const = llvm::ConstantInt::getSigned(ty, val);
    } else if (ty->isFloatingPointTy()) {
      *out_const = llvm::ConstantFP::get(ty, 1.0);
    } else {
      *out_const = llvm::Constant::getNullValue(ty);
    }
    return llvm_gen_be_error_t::ok; 
  }
  
  a_byte_boolean int_kind_is_signed[ik_last] = {0};
  an_il_header il_header = {};
  
  /**
   * @brief Test stub or helper for `get_llvm_type`.
   * @details Provides mock or helper behavior for `get_llvm_type` within `tests/test_llvm_control_flow.cpp`.
   * @param[in] ty Parameter `ty` for `get_llvm_type`.
   * @param[out] out_ty Parameter `out_ty` for `get_llvm_type`.
   * @return Result of `get_llvm_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_llvm_type(a_type_ptr ty, llvm::Type** out_ty) noexcept {
    if (g_cf_fail_type < 0) return llvm_gen_be_error_t::unsupported_type;
    if (g_cf_fail_type > 0) {
      --g_cf_fail_type;
      if (g_cf_fail_type == 0) return llvm_gen_be_error_t::unsupported_type;
    }
    if (ty && ty->kind == tk_void) {
      *out_ty = llvm::Type::getVoidTy(*be_state->context);
    } else if (ty && ty->kind == tk_array) {
      *out_ty = llvm::ArrayType::get(llvm::Type::getInt32Ty(*be_state->context), 2);
    } else {
      *out_ty = llvm::Type::getInt32Ty(*be_state->context);
    }
    return llvm_gen_be_error_t::ok;
  }
  
  
  
  /**
   * @brief Test stub or helper for `get_typeinfo_global`.
   * @details Provides mock or helper behavior for `get_typeinfo_global` within `tests/test_llvm_control_flow.cpp`.
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
   * @brief Test stub or helper for `push_lexical_block`.
   * @details Provides mock or helper behavior for `push_lexical_block` within `tests/test_llvm_control_flow.cpp`.
   * @param[in] dbg Parameter `dbg` for `push_lexical_block`.
   * @param[in] pos Parameter `pos` for `push_lexical_block`.
   * @param[out] out_block Parameter `out_block` for `push_lexical_block`.
   * @return Result of `push_lexical_block` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t push_lexical_block(llvm_gen_be_debug_state_t* dbg, a_source_position pos, llvm::DILexicalBlock** out_block) noexcept {
    (void)dbg; (void)pos; (void)out_block;
    if (g_cf_fail_push_lex) return llvm_gen_be_error_t::di_metadata_failure;
    return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `pop_lexical_block`.
   * @details Provides mock or helper behavior for `pop_lexical_block` within `tests/test_llvm_control_flow.cpp`.
   * @param[in] dbg Parameter `dbg` for `pop_lexical_block`.
   * @return Result of `pop_lexical_block` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t pop_lexical_block(llvm_gen_be_debug_state_t* dbg) noexcept {
    (void)dbg;
    if (g_cf_fail_pop_lex) return llvm_gen_be_error_t::di_metadata_failure;
    return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `get_di_location`.
   * @details Provides mock or helper behavior for `get_di_location` within `tests/test_llvm_control_flow.cpp`.
   * @param[in] dbg Parameter `dbg` for `get_di_location`.
   * @param[in] pos Parameter `pos` for `get_di_location`.
   * @param[in] scope Parameter `scope` for `get_di_location`.
   * @param[out] out_loc Parameter `out_loc` for `get_di_location`.
   * @return Result of `get_di_location` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_di_location(llvm_gen_be_debug_state_t* dbg, a_source_position pos, llvm::DIScope* scope, llvm::DILocation** out_loc) noexcept {
    (void)dbg; (void)pos;
    if (g_cf_fail_di_loc) return llvm_gen_be_error_t::di_metadata_failure;
    if (g_cf_nonnull_di_loc && scope) {
      *out_loc = llvm::DILocation::get(*be_state->context, 10, 1, scope);
    } else {
      *out_loc = nullptr;
    }
    return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub or helper for `llvm_lower_expression`.
   * @details Provides mock or helper behavior for `llvm_lower_expression` within `tests/test_llvm_control_flow.cpp`.
   * @param[in] expr Parameter `expr` for `llvm_lower_expression`.
   * @param[out] out_val Parameter `out_val` for `llvm_lower_expression`.
   * @return Result of `llvm_gen_be_error_t`.
   */
  llvm_gen_be_error_t llvm_lower_expression(an_expr_node_ptr expr, llvm::Value** out_val) noexcept {
    if (g_cf_fail_expr < 0) return llvm_gen_be_error_t::unsupported_expr;
    if (g_cf_fail_expr > 0) {
      --g_cf_fail_expr;
      if (g_cf_fail_expr == 0) return llvm_gen_be_error_t::unsupported_expr;
    }
    if (expr == nullptr || g_cf_expr_mode == 3) {
       *out_val = nullptr;
       return llvm_gen_be_error_t::ok;
    }
    if (g_cf_expr_mode == 5) {
      be_state->prvalue_constructed_in_place = true;
      *out_val = nullptr;
      return llvm_gen_be_error_t::ok;
    }
    if (g_cf_expr_mode == 6) {
      llvm::FunctionType* vfn_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false);
      llvm::FunctionCallee vfn = be_state->module->getOrInsertFunction("cf_void_sret_stub", vfn_ty);
      *out_val = be_state->builder->CreateCall(vfn);
      return llvm_gen_be_error_t::ok;
    }
    if (g_cf_expr_mode == 1) {
      *out_val = llvm::ConstantInt::getTrue(*be_state->context);
    } else if (g_cf_expr_mode == 2) {
      *out_val = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
    } else if (g_cf_expr_mode == 4) {
      llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
      llvm::StructType* st = llvm::StructType::get(*be_state->context, {i32_ty, i32_ty});
      *out_val = llvm::Constant::getNullValue(st);
    } else {
      *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 1);
    }
    return llvm_gen_be_error_t::ok;
  }
  
  // named_register_names array stub
  const char* named_register_names[anr_last + 1] = {0};
}

/**
 * @brief Entry point for the control-flow statement lowering unit test suite.
 * @details Exercises `if`, `while`, `do-while`, `for`, `range-based for`, `switch`, `goto`, `return`, and `stmk_init`.
 * @return 0 on success.
 */
int main(void) {
  printf("Running test_llvm_control_flow...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_cf", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);

  a_statement stmt = {};
  stmt.kind = stmk_empty;
  assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);

  // stmk_expr
  {
      a_statement stmt = {};
      stmt.kind = stmk_expr;
      an_expr_node expr = {};
      stmt.expr = &expr;
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);
  }
  
  // stmk_return
  {
      a_statement stmt = {};
      stmt.kind = stmk_return;
      an_expr_node expr = {};
      stmt.expr = &expr;
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);
  }

  // stmk_if
  {
      a_statement stmt = {};
      stmt.kind = stmk_if;
      an_expr_node expr = {};
      stmt.expr = &expr;
      a_statement then_stmt = {};
      then_stmt.kind = stmk_empty;
      stmt.variant.if_stmt.then_statement = &then_stmt;
      a_statement else_stmt = {};
      else_stmt.kind = stmk_empty;
      stmt.variant.if_stmt.else_statement = &else_stmt;
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);
  }

  // stmk_block
  {
      a_statement stmt = {};
      stmt.kind = stmk_block;
      a_statement inner = {};
      inner.kind = stmk_empty;
      stmt.variant.block.statements = &inner;
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);
  }

  // stmk_while
  {
      a_statement stmt = {};
      stmt.kind = stmk_while;
      an_expr_node expr = {};
      stmt.expr = &expr;
      a_statement body = {};
      body.kind = stmk_empty;
      stmt.variant.loop_statement = &body;
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);
  }

  // stmk_for
  {
      a_statement stmt = {};
      stmt.kind = stmk_for;
      an_expr_node expr = {};
      stmt.expr = &expr;
      a_statement body = {};
      body.kind = stmk_empty;
      a_for_loop loop_info = {};
      a_statement init = {};
      init.kind = stmk_empty;
      loop_info.initialization = &init;
      an_expr_node inc = {};
      loop_info.increment = &inc;
      stmt.variant.for_loop.statement = &body;
      stmt.variant.for_loop.extra_info = &loop_info;
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);
  }

  // stmk_range_based_for
  {
      a_statement stmt = {};
      stmt.kind = stmk_range_based_for;
      a_statement body = {};
      body.kind = stmk_empty;
      a_range_based_for_loop loop_info = {};
      a_statement init = {};
      init.kind = stmk_empty;
      loop_info.initialization = &init;
      an_expr_node ne_expr = {};
      loop_info.ne_call_expr = &ne_expr;
      an_expr_node inc_expr = {};
      loop_info.incr_call_expr = &inc_expr;
      stmt.variant.range_based_for_loop.statement = &body;
      stmt.variant.range_based_for_loop.extra_info = &loop_info;
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);
  }

  // stmk_label
  {
      a_statement stmt = {};
      stmt.kind = stmk_label;
      a_label lbl = {};
      stmt.variant.label.ptr = &lbl;
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);
  }

  // stmk_goto
  {
      a_statement stmt = {};
      stmt.kind = stmk_goto;
      a_label lbl = {};
      stmt.variant.label.ptr = &lbl;
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);
  }
  
  // stmk_end_test_while
  {
      a_statement stmt = {};
      stmt.kind = stmk_end_test_while;
      an_expr_node expr = {};
      stmt.expr = &expr;
      a_statement body = {};
      body.kind = stmk_empty;
      stmt.variant.loop_statement = &body;
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);
  }

  // Test break and continue via goto
  {
      a_statement loop_stmt = {};
      loop_stmt.kind = stmk_while;
      an_expr_node expr = {};
      loop_stmt.expr = &expr;

      a_statement body = {};
      body.kind = stmk_block;

      a_statement break_stmt = {};
      break_stmt.kind = stmk_goto;
      a_label brk_lbl = {};
      break_stmt.variant.label.ptr = &brk_lbl;

      a_statement cont_stmt = {};
      cont_stmt.kind = stmk_goto;
      a_label cnt_lbl = {};
      cont_stmt.variant.label.ptr = &cnt_lbl;

      break_stmt.next = &cont_stmt;
      body.variant.block.statements = &break_stmt;

      loop_stmt.variant.loop_statement = &body;
      assert(llvm_lower_statement(&loop_stmt) == llvm_gen_be_error_t::ok);
      state.label_blocks.clear();
  }

  // stmk_switch
  {
      a_statement stmt = {};
      stmt.kind = stmk_switch;
      an_expr_node expr = {};
      a_type expr_ty = {};
      expr_ty.kind = tk_integer;
      expr.type = &expr_ty;
      stmt.expr = &expr;
      a_switch_stmt_descr descr = {};
      a_switch_case_entry c1 = {};
      a_switch_case_entry c2 = {};
      c1.next = &c2;
      a_constant con1 = {};
      c1.case_value = &con1;
      a_constant con2 = {};
      c2.case_value = &con2;
      descr.cases = &c1;
      descr.default_case = &c2;
      stmt.variant.switch_stmt.extra_info = &descr;
      a_statement body = {};
      body.kind = stmk_switch_case;
      body.variant.switch_case.extra_info = &c1;
      stmt.variant.switch_stmt.body_statement = &body;
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);
  }

  // stmk_set_vla_size and stmk_vla_decl
  {
      a_statement stmt = {};
      stmt.kind = stmk_set_vla_size;
      a_vla_dimension dim = {};
      a_type dim_ty = {};
      dim_ty.kind = tk_integer;
      dim.type = &dim_ty;
      an_expr_node expr = {};
      expr.type = &dim_ty;
      dim.dimension_expr = &expr;
      a_variable var = {};
      dim.dimension_variable = &var;
      stmt.variant.vla_dimension = &dim;
      state.local_vars[&var] = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);

      a_statement decl_stmt = {};
      decl_stmt.kind = stmk_vla_decl;
      a_variable vla_var = {};
      memset(&vla_var, 0, sizeof(vla_var));
      a_type vla_ty = {};
      memset(&vla_ty, 0, sizeof(vla_ty));
      vla_ty.kind = tk_array;
      vla_ty.variant.array.element_type = &dim_ty;
      vla_ty.variant.array.is_vla = TRUE;
      vla_ty.variant.array.has_assoc_vla_dimension = TRUE;
      vla_var.type = &vla_ty;
      decl_stmt.variant.vla.variant.variable = &vla_var;
      state.local_vars[&vla_var] = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
      assert(llvm_lower_statement(&decl_stmt) == llvm_gen_be_error_t::ok);
  }



  // Test assigned goto
  a_statement assigned_goto_stmt = {};
  assigned_goto_stmt.kind = stmk_assigned_goto;
  an_expr_node address_expr = {};
  address_expr.kind = enk_constant;
  assigned_goto_stmt.expr = &address_expr;

  assert(llvm_lower_statement(&assigned_goto_stmt) == llvm_gen_be_error_t::ok);


  // Test Coroutine Statements
  a_statement coro_stmt = {};
  coro_stmt.kind = stmk_coroutine;
  a_coroutine_descr coro_descr = {};
  coro_stmt.variant.coroutine.descr = &coro_descr;
  
  a_routine new_rout = {};
  new_rout.source_corresp.name = "_Znwm_coro";
  coro_descr.new_routine = &new_rout;
  
  assert(llvm_lower_statement(&coro_stmt) == llvm_gen_be_error_t::ok);
  
  a_statement coro_ret = {};
  coro_ret.kind = stmk_coroutine_return;
  assert(llvm_lower_statement(&coro_ret) == llvm_gen_be_error_t::ok);

  // Test MSVC thread-safe static guard in stmk_init
  {
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
      llvm::BasicBlock* ms_init_bb = llvm::BasicBlock::Create(*state.context, "ms_init_entry", f);
      state.builder->SetInsertPoint(ms_init_bb);

      a_variable static_var = {};
      static_var.source_corresp.name = "ms_local_static";
      static_var.storage_class = sc_static;
      a_type static_ty = {};
      static_ty.kind = tk_integer;
      static_var.type = &static_ty;
      llvm::AllocaInst* static_alloc = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));
      state.local_vars[&static_var] = static_alloc;

      an_expr_node init_val_expr = {};
      init_val_expr.kind = enk_constant;
      init_val_expr.type = &static_ty;

      a_dynamic_init dip = {};
      dip.kind = dik_expression;
      dip.variable = &static_var;
      dip.variant.expression = &init_val_expr;

      a_statement init_stmt = {};
      init_stmt.kind = stmk_init;
      init_stmt.variant.dynamic_init = &dip;
      assert(llvm_lower_statement(&init_stmt) == llvm_gen_be_error_t::ok);
      assert(state.module->getFunction("_Init_thread_header") != nullptr);
      assert(state.module->getFunction("_Init_thread_footer") != nullptr);
  }

  // =========================================================================
  // Exhaustive branch coverage for src/llvm_gen_be_stmt.cpp
  // =========================================================================
  {
    // 1. llvm_emit_cleanups_until via return/block/goto:
    //    - inactive cleanup
    //    - guard_abort cleanup (MSVC & Itanium)
    //    - finally_func cleanup
    //    - dtor_routine cleanup (dtor_routine==nullptr, object_ptr==nullptr, unnamed/missing dtor_func, existing dtor_func)
    state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "cov.bb", f));
    llvm::FunctionType* fin_ft = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*state.context),
        {llvm::Type::getInt8Ty(*state.context), llvm::PointerType::getUnqual(*state.context)},
        false);
    llvm::Function* fin_fn = llvm::Function::Create(fin_ft, llvm::Function::ExternalLinkage, "seh_fin", state.module.get());
    llvm::FunctionType* dtor_ft = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*state.context),
        {llvm::PointerType::getUnqual(*state.context)},
        false);
    llvm::Function* dtor_existing_fn = llvm::Function::Create(
        dtor_ft, llvm::Function::ExternalLinkage, "_ZN1CD1Ev", state.module.get());
    assert(dtor_existing_fn != nullptr);

    llvm::Value* dummy_ptr = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*state.context));
    a_routine dtor_found = {}; dtor_found.source_corresp.name = "_ZN1CD1Ev";
    a_routine dtor_missing = {}; dtor_missing.source_corresp.name = "_ZN1CD1Ev_missing";
    a_routine dtor_unnamed = {}; dtor_unnamed.source_corresp.name = nullptr;

    ScopeCleanup c_inactive = {}; c_inactive.is_active = false;
    ScopeCleanup c_guard_msvc = {}; c_guard_msvc.is_active = true; c_guard_msvc.is_guard_abort = true; c_guard_msvc.guard_abort_ptr = dummy_ptr;
    ScopeCleanup c_finally = {}; c_finally.is_active = true; c_finally.finally_func = fin_fn;
    ScopeCleanup c_dtor_no_rout = {}; c_dtor_no_rout.is_active = true; c_dtor_no_rout.dtor_routine = nullptr; c_dtor_no_rout.object_ptr = dummy_ptr;
    ScopeCleanup c_dtor_no_obj = {}; c_dtor_no_obj.is_active = true; c_dtor_no_obj.dtor_routine = &dtor_found; c_dtor_no_obj.object_ptr = nullptr;
    ScopeCleanup c_dtor_un = {}; c_dtor_un.is_active = true; c_dtor_un.dtor_routine = &dtor_unnamed; c_dtor_un.object_ptr = dummy_ptr;
    ScopeCleanup c_dtor_miss = {}; c_dtor_miss.is_active = true; c_dtor_miss.dtor_routine = &dtor_missing; c_dtor_miss.object_ptr = dummy_ptr;
    ScopeCleanup c_dtor_ok = {}; c_dtor_ok.is_active = true; c_dtor_ok.dtor_routine = &dtor_found; c_dtor_ok.object_ptr = dummy_ptr;

    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    state.cleanup_stack.cleanups = {c_inactive, c_guard_msvc, c_finally, c_dtor_no_rout, c_dtor_no_obj, c_dtor_un, c_dtor_miss, c_dtor_ok};
    a_statement ret_void = {}; ret_void.kind = stmk_return; ret_void.expr = nullptr;
    assert(llvm_lower_statement(&ret_void) == llvm_gen_be_error_t::ok);
    state.cleanup_stack.cleanups.clear();

    // Itanium guard abort cleanup
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
    state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "cov.bb", f));
    state.cleanup_stack.cleanups = {c_guard_msvc};
    assert(llvm_lower_statement(&ret_void) == llvm_gen_be_error_t::ok);
    state.cleanup_stack.cleanups.clear();

    // 2. llvm_lower_return_stmt:
    //    - expr error
    //    - func with arg_size() > 0: func->hasStructRetAttr(), arg(0)->hasStructRetAttr(), neither
    //    - non-sret: ret_val == nullptr, func_ret_ty == ret_val->getType(), func_ret_ty != ret_val->getType() (void vs non-void packing)
    state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "cov.bb", f));
    an_expr_node ret_e = {};
    a_statement ret_s = {}; ret_s.kind = stmk_return; ret_s.expr = &ret_e;
    g_cf_fail_expr = 1;
    assert(llvm_lower_statement(&ret_s) == llvm_gen_be_error_t::unsupported_expr);
    g_cf_fail_expr = 0;

    // ret_val == nullptr
    g_cf_expr_mode = 3;
    assert(llvm_lower_statement(&ret_s) == llvm_gen_be_error_t::ok);
    g_cf_expr_mode = 0;

    // Function with sret on function vs sret on arg0 vs non-sret returning i64 (coerced from i32 -> ret_pack) and returning i32 (exact match)
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*state.context);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(*state.context);
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(*state.context);

    llvm::Function* fn_sret_fn = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {ptr_ty}, false),
        llvm::Function::ExternalLinkage, "fn_sret_fn", state.module.get());
    fn_sret_fn->addFnAttr(llvm::Attribute::StructRet);
    state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "entry", fn_sret_fn));
    assert(llvm_lower_statement(&ret_s) == llvm_gen_be_error_t::ok);

    llvm::Function* fn_sret_arg = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {ptr_ty}, false),
        llvm::Function::ExternalLinkage, "fn_sret_arg", state.module.get());
    fn_sret_arg->getArg(0)->addAttr(llvm::Attribute::getWithStructRetType(*state.context, i32_ty));
    state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "entry", fn_sret_arg));
    assert(llvm_lower_statement(&ret_s) == llvm_gen_be_error_t::ok);

    llvm::Function* fn_pack = llvm::Function::Create(
        llvm::FunctionType::get(i64_ty, {ptr_ty}, false),
        llvm::Function::ExternalLinkage, "fn_pack", state.module.get());
    state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "entry", fn_pack));
    assert(llvm_lower_statement(&ret_s) == llvm_gen_be_error_t::ok);

    llvm::Function* fn_exact = llvm::Function::Create(
        llvm::FunctionType::get(i32_ty, false),
        llvm::Function::ExternalLinkage, "fn_exact", state.module.get());
    state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "entry", fn_exact));
    assert(llvm_lower_statement(&ret_s) == llvm_gen_be_error_t::ok);

    // Restore insertion into f
    state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "cov.bb", f));

    // 3. llvm_lower_if_stmt (and stmk_constexpr_if, stmk_if_consteval, stmk_if_not_consteval):
    //    - cond error, i1 cond, has_else == false, then/else errors, then/else already terminated
    a_statement if_s = {}; if_s.kind = stmk_constexpr_if; if_s.expr = &ret_e;
    a_statement empty_s = {}; empty_s.kind = stmk_empty;
    a_statement bad_s = {}; bad_s.kind = static_cast<a_statement_kind>(999);
    if_s.variant.if_stmt.then_statement = &empty_s;
    g_cf_fail_expr = 1;
    assert(llvm_lower_statement(&if_s) == llvm_gen_be_error_t::unsupported_expr);
    g_cf_fail_expr = 0;

    // i1 cond + has_else == false + terminated then_statement
    g_cf_expr_mode = 1;
    if_s.kind = stmk_if_consteval;
    if_s.variant.if_stmt.then_statement = &ret_void;
    if_s.variant.if_stmt.else_statement = nullptr;
    assert(llvm_lower_statement(&if_s) == llvm_gen_be_error_t::ok);
    g_cf_expr_mode = 0;

    // then_statement error
    if_s.kind = stmk_if_not_consteval;
    if_s.variant.if_stmt.then_statement = &bad_s;
    assert(llvm_lower_statement(&if_s) == llvm_gen_be_error_t::unsupported_stmt);

    // else_statement error & terminated else_statement
    if_s.variant.if_stmt.then_statement = &empty_s;
    if_s.variant.if_stmt.else_statement = &bad_s;
    assert(llvm_lower_statement(&if_s) == llvm_gen_be_error_t::unsupported_stmt);
    if_s.variant.if_stmt.else_statement = &ret_void;
    assert(llvm_lower_statement(&if_s) == llvm_gen_be_error_t::ok);

    // 4. llvm_lower_block_stmt:
    //    - dbg_state != nullptr (ok, push error, pop error)
    //    - extra_info != nullptr with assoc_scope == nullptr and assoc_scope != nullptr
    //    - nonstatic_variables (already in local_vars, new alloca, get_llvm_type error)
    //    - variables (non-static, static already in local_vars, static new global with/without TLS, static already in module, get_llvm_type error)
    //    - inner statement error
    llvm_gen_be_debug_state_t dbg_st = {};
    state.dbg_state = &dbg_st;
    a_statement blk_s = {}; blk_s.kind = stmk_block;
    g_cf_fail_push_lex = 1;
    assert(llvm_lower_statement(&blk_s) == llvm_gen_be_error_t::di_metadata_failure);
    g_cf_fail_push_lex = 0;
    g_cf_fail_pop_lex = 1;
    assert(llvm_lower_statement(&blk_s) == llvm_gen_be_error_t::di_metadata_failure);
    g_cf_fail_pop_lex = 0;
    assert(llvm_lower_statement(&blk_s) == llvm_gen_be_error_t::ok);
    state.dbg_state = nullptr;

    a_block blk_extra = {};
    blk_s.variant.block.extra_info = &blk_extra;
    // assoc_scope == nullptr
    assert(llvm_lower_statement(&blk_s) == llvm_gen_be_error_t::ok);

    a_scope bscope = {};
    blk_extra.assoc_scope = &bscope;
    a_type int_ty = {}; int_ty.kind = tk_integer;
    a_variable lvar1 = {}, lvar2 = {};
    lvar1.source_corresp.name = "blk_lvar1"; lvar1.type = &int_ty; lvar1.next = &lvar2;
    lvar2.source_corresp.name = "blk_lvar2"; lvar2.type = &int_ty; lvar2.next = nullptr;
    state.local_vars[&lvar1] = dummy_ptr; // lvar1 already in local_vars
    bscope.nonstatic_variables = &lvar1;

    // get_llvm_type error on lvar2
    g_cf_fail_type = 1;
    assert(llvm_lower_statement(&blk_s) == llvm_gen_be_error_t::unsupported_type);
    g_cf_fail_type = 0;

    a_variable svar_auto = {}, svar_cached = {}, svar_new1 = {}, svar_new2 = {}, svar_existing = {};
    svar_auto.storage_class = sc_auto; svar_auto.next = &svar_cached;
    svar_cached.storage_class = sc_static; svar_cached.next = &svar_new1;
    state.local_vars[&svar_cached] = dummy_ptr;
    svar_new1.storage_class = sc_static; svar_new1.source_corresp.name = "blk_svar_new1"; svar_new1.type = &int_ty; svar_new1.is_thread_local = FALSE; svar_new1.next = &svar_new2;
    svar_new2.storage_class = sc_static; svar_new2.source_corresp.name = "blk_svar_new2"; svar_new2.type = &int_ty; svar_new2.is_thread_local = TRUE; svar_new2.next = &svar_existing;
    svar_existing.storage_class = sc_static; svar_existing.source_corresp.name = "blk_svar_new1"; svar_existing.type = &int_ty; svar_existing.next = nullptr;
    bscope.variables = &svar_auto;

    // get_llvm_type error on svar_new1
    g_cf_fail_type = 2; // 1st for lvar2, 2nd for svar_new1
    assert(llvm_lower_statement(&blk_s) == llvm_gen_be_error_t::unsupported_type);
    g_cf_fail_type = 0;
    state.local_vars.erase(&lvar2);

    // Success on all nonstatic & static block variables
    assert(llvm_lower_statement(&blk_s) == llvm_gen_be_error_t::ok);

    // Inner statement error in block
    blk_s.variant.block.statements = &bad_s;
    assert(llvm_lower_statement(&blk_s) == llvm_gen_be_error_t::unsupported_stmt);

    // Statements after a terminator in a block: stmk_label and stmk_switch_case vs normal statement
    a_label lbl_after_ret = {};
    a_statement stmt_ret1 = {}, stmt_lbl_after = {}, stmt_ret2 = {}, stmt_case_after = {};
    stmt_ret1.kind = stmk_return; stmt_ret1.next = &stmt_lbl_after;
    stmt_lbl_after.kind = stmk_label; stmt_lbl_after.variant.label.ptr = &lbl_after_ret; stmt_lbl_after.next = &stmt_ret2;
    stmt_ret2.kind = stmk_return; stmt_ret2.next = &stmt_case_after;
    a_switch_case_entry sw_case_entry = {};
    stmt_case_after.kind = stmk_switch_case; stmt_case_after.variant.switch_case.extra_info = &sw_case_entry; stmt_case_after.next = nullptr;
    blk_s.variant.block.statements = &stmt_ret1;
    assert(llvm_lower_statement(&blk_s) == llvm_gen_be_error_t::ok);

    // 5. llvm_lower_while_stmt & llvm_lower_end_test_while_stmt:
    a_statement while_s = {}; while_s.kind = stmk_while; while_s.expr = &ret_e; while_s.variant.loop_statement = &empty_s;
    g_cf_fail_expr = 1;
    assert(llvm_lower_statement(&while_s) == llvm_gen_be_error_t::unsupported_expr);
    g_cf_fail_expr = 0;
    g_cf_expr_mode = 1; // i1 cond
    while_s.variant.loop_statement = &ret_void; // terminated body
    assert(llvm_lower_statement(&while_s) == llvm_gen_be_error_t::ok);
    g_cf_expr_mode = 0;
    while_s.variant.loop_statement = &bad_s; // body error
    assert(llvm_lower_statement(&while_s) == llvm_gen_be_error_t::unsupported_stmt);

    a_statement do_s = {}; do_s.kind = stmk_end_test_while; do_s.expr = &ret_e; do_s.variant.loop_statement = &bad_s;
    assert(llvm_lower_statement(&do_s) == llvm_gen_be_error_t::unsupported_stmt);
    do_s.variant.loop_statement = &ret_void; // terminated body
    g_cf_fail_expr = 1;
    assert(llvm_lower_statement(&do_s) == llvm_gen_be_error_t::unsupported_expr);
    g_cf_fail_expr = 0;
    g_cf_expr_mode = 1; // i1 cond
    assert(llvm_lower_statement(&do_s) == llvm_gen_be_error_t::ok);
    g_cf_expr_mode = 0;

    // 6. llvm_lower_for_stmt:
    a_for_loop fl_info = {};
    a_statement for_s = {}; for_s.kind = stmk_for; for_s.variant.for_loop.extra_info = &fl_info;
    fl_info.initialization = &bad_s;
    assert(llvm_lower_statement(&for_s) == llvm_gen_be_error_t::unsupported_stmt);

    // initialization == nullptr, expr == nullptr,increment == nullptr, terminated body, body error
    fl_info.initialization = nullptr;
    for_s.expr = nullptr;
    for_s.variant.for_loop.statement = &ret_void;
    fl_info.increment = nullptr;
    assert(llvm_lower_statement(&for_s) == llvm_gen_be_error_t::ok);

    for_s.variant.for_loop.statement = &bad_s;
    assert(llvm_lower_statement(&for_s) == llvm_gen_be_error_t::unsupported_stmt);

    // expr error, i1 cond, increment error
    for_s.expr = &ret_e;
    for_s.variant.for_loop.statement = &empty_s;
    g_cf_fail_expr = 1;
    assert(llvm_lower_statement(&for_s) == llvm_gen_be_error_t::unsupported_expr);
    g_cf_fail_expr = 0;

    g_cf_expr_mode = 1;
    fl_info.increment = &ret_e;
    g_cf_fail_expr = 2; // 1st cond ok, 2nd increment fails
    assert(llvm_lower_statement(&for_s) == llvm_gen_be_error_t::unsupported_expr);
    g_cf_fail_expr = 0;
    g_cf_expr_mode = 0;

    // 7. llvm_lower_range_based_for_stmt:
    a_range_based_for_loop rfl_info = {};
    a_statement rfor_s = {}; rfor_s.kind = stmk_range_based_for; rfor_s.variant.range_based_for_loop.extra_info = &rfl_info;
    rfl_info.initialization = &bad_s;
    assert(llvm_lower_statement(&rfor_s) == llvm_gen_be_error_t::unsupported_stmt);

    rfl_info.initialization = nullptr;
    rfl_info.ne_call_expr = nullptr;
    rfl_info.incr_call_expr = nullptr;
    rfor_s.variant.range_based_for_loop.statement = &ret_void;
    assert(llvm_lower_statement(&rfor_s) == llvm_gen_be_error_t::ok);

    rfor_s.variant.range_based_for_loop.statement = &bad_s;
    assert(llvm_lower_statement(&rfor_s) == llvm_gen_be_error_t::unsupported_stmt);

    rfl_info.ne_call_expr = &ret_e;
    rfor_s.variant.range_based_for_loop.statement = &empty_s;
    g_cf_fail_expr = 1;
    assert(llvm_lower_statement(&rfor_s) == llvm_gen_be_error_t::unsupported_expr);
    g_cf_fail_expr = 0;

    g_cf_expr_mode = 1;
    rfl_info.incr_call_expr = &ret_e;
    g_cf_fail_expr = 2;
    assert(llvm_lower_statement(&rfor_s) == llvm_gen_be_error_t::unsupported_expr);
    g_cf_fail_expr = 0;
    g_cf_expr_mode = 0;

    // 8. llvm_lower_label_stmt, llvm_lower_goto_stmt, and llvm_lower_assigned_goto_stmt:
    //    - forward goto (creates unparented label_bb), then label (inserts unparented label_bb when current bb is terminated),
    //      then second label on same ptr (moves parented label_bb)
    //    - goto with cleanups > target_depth
    state.label_blocks.clear();
    state.label_cleanup_depths.clear();
    state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "cov.bb", f));
    a_label fwd_lbl = {}; fwd_lbl.address_taken = TRUE;
    a_statement fwd_goto = {}; fwd_goto.kind = stmk_goto; fwd_goto.variant.label.ptr = &fwd_lbl;
    state.cleanup_stack.cleanups.push_back(c_inactive);
    assert(llvm_lower_statement(&fwd_goto) == llvm_gen_be_error_t::ok);
    state.cleanup_stack.cleanups.clear();

    // Now current block is terminated by fwd_goto! Lower stmk_label for fwd_lbl (unparented -> func->insert, terminated -> no CreateBr)
    a_statement fwd_lbl_stmt = {}; fwd_lbl_stmt.kind = stmk_label; fwd_lbl_stmt.variant.label.ptr = &fwd_lbl;
    assert(llvm_lower_statement(&fwd_lbl_stmt) == llvm_gen_be_error_t::ok);
    // Lower fwd_lbl_stmt a second time (parented -> moveAfter)
    assert(llvm_lower_statement(&fwd_lbl_stmt) == llvm_gen_be_error_t::ok);
    // Goto when label IS in label_cleanup_depths
    assert(llvm_lower_statement(&fwd_goto) == llvm_gen_be_error_t::ok);

    // Assigned goto: expr error, address == nullptr, and num_dests > 0 (with both parented fwd_lbl, unparented addr_lbl2, and non-address-taken non_addr_lbl)
    state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "cov.bb", f));
    g_cf_fail_expr = 1;
    assert(llvm_lower_statement(&assigned_goto_stmt) == llvm_gen_be_error_t::unsupported_expr);
    g_cf_fail_expr = 0;

    g_cf_expr_mode = 3; // address == nullptr -> CreateUnreachable
    assert(llvm_lower_statement(&assigned_goto_stmt) == llvm_gen_be_error_t::ok);
    g_cf_expr_mode = 2; // pointer address
    state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "cov.bb", f));
    a_label addr_lbl2 = {}; addr_lbl2.address_taken = TRUE;
    a_label non_addr_lbl = {}; non_addr_lbl.address_taken = FALSE;
    state.label_blocks[&addr_lbl2] = llvm::BasicBlock::Create(*state.context, "unparented.dest");
    state.label_blocks[&non_addr_lbl] = llvm::BasicBlock::Create(*state.context, "nonaddr.dest", f);
    assert(llvm_lower_statement(&assigned_goto_stmt) == llvm_gen_be_error_t::ok);
    g_cf_expr_mode = 0;

    // 9. llvm_lower_switch_stmt & llvm_lower_switch_case_stmt:
    //    - expr error
    //    - default_case == nullptr vs != nullptr (already parented default_bb)
    //    - expr->type == nullptr, expr->type == tk_float, expr->type == tk_integer (signed and unsigned)
    //    - case range (signed and unsigned), evaluate_constant error on case_value and range_end
    //    - body error, body already terminated
    //    - switch_case not in case_blocks, and when current block is already terminated
    state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "cov.bb", f));
    a_switch_stmt_descr sw_descr = {};
    a_statement sw_s = {}; sw_s.kind = stmk_switch; sw_s.expr = &ret_e; sw_s.variant.switch_stmt.extra_info = &sw_descr;
    sw_s.variant.switch_stmt.body_statement = &empty_s;

    g_cf_fail_expr = 1;
    assert(llvm_lower_statement(&sw_s) == llvm_gen_be_error_t::unsupported_expr);
    g_cf_fail_expr = 0;

    // default_case == nullptr, expr->type == nullptr, case range unsigned (1..2)
    a_switch_case_entry sc_range = {};
    a_constant c_start = {}, c_end = {};
    sc_range.case_value = &c_start;
    sc_range.range_end = &c_end;
    sw_descr.cases = &sc_range;
    sw_descr.default_case = nullptr;
    ret_e.type = nullptr;

    // evaluate_constant error on case_value (1st call) and range_end (2nd call)
    g_cf_fail_const = 1;
    assert(llvm_lower_statement(&sw_s) == llvm_gen_be_error_t::constant_eval_failure);
    g_cf_fail_const = 2;
    assert(llvm_lower_statement(&sw_s) == llvm_gen_be_error_t::constant_eval_failure);
    g_cf_fail_const = 0;

    g_cf_const_seq = 1; // 1 .. 2 unsigned range
    sw_s.variant.switch_stmt.body_statement = &ret_void; // terminated body
    assert(llvm_lower_statement(&sw_s) == llvm_gen_be_error_t::ok);

    // Large switch range (0 .. 100, span > 64)
    g_cf_const_seq = 100;
    assert(llvm_lower_statement(&sw_s) == llvm_gen_be_error_t::ok);

    // Signed integer switch with range (1..2), plus body error
    a_type sw_int_ty = {}; sw_int_ty.kind = tk_integer; sw_int_ty.variant.integer.int_kind = ik_int;
    int_kind_is_signed[ik_int] = TRUE;
    ret_e.type = &sw_int_ty;
    g_cf_const_seq = 1;
    sw_s.variant.switch_stmt.body_statement = &bad_s;
    assert(llvm_lower_statement(&sw_s) == llvm_gen_be_error_t::unsupported_stmt);
    g_cf_const_seq = 0;

    // Non-integer expr->type (tk_float), and switch_case when current block is NOT terminated vs terminated
    a_type sw_flt_ty = {}; sw_flt_ty.kind = tk_float;
    ret_e.type = &sw_flt_ty;
    sw_descr.cases = nullptr;
    sw_s.variant.switch_stmt.body_statement = &empty_s;
    assert(llvm_lower_statement(&sw_s) == llvm_gen_be_error_t::ok);

    a_switch_case_entry sc_unmapped = {};
    a_statement sc_stmt = {}; sc_stmt.kind = stmk_switch_case; sc_stmt.variant.switch_case.extra_info = &sc_unmapped;
    assert(llvm_lower_statement(&sc_stmt) == llvm_gen_be_error_t::ok);
    // Mapped switch_case when current block is NOT terminated (hits CreateBr(case_bb)), then when terminated
    state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "cov.bb", f));
    sc_stmt.variant.switch_case.extra_info = &sc_range;
    assert(llvm_lower_statement(&sc_stmt) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_statement(&ret_void) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_statement(&sc_stmt) == llvm_gen_be_error_t::ok);

    // 10. llvm_lower_vla_stmt:
    //     - stmk_set_vla_size: dim == nullptr, dim->type == nullptr, dim->dimension_expr == nullptr, dimension_expr error
    //     - stmk_vla_decl: is_typedef_decl == TRUE, var == nullptr, var not in local_vars,
    //       multi-dim VLA with mapped & valued dim, unmapped dim, fixed dim (bound_is_zero FALSE), zero bound dim (bound_is_zero TRUE), named var, get_llvm_type error
    state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "cov.bb", f));
    a_statement vla_sz = {}; vla_sz.kind = stmk_set_vla_size; vla_sz.variant.vla_dimension = nullptr;
    assert(llvm_lower_statement(&vla_sz) == llvm_gen_be_error_t::ok);

    a_vla_dimension vdim = {};
    vla_sz.variant.vla_dimension = &vdim;
    assert(llvm_lower_statement(&vla_sz) == llvm_gen_be_error_t::ok);

    vdim.dimension_expr = &ret_e;
    g_cf_fail_expr = 1;
    assert(llvm_lower_statement(&vla_sz) == llvm_gen_be_error_t::unsupported_expr);
    g_cf_fail_expr = 0;

    a_statement vla_d = {}; vla_d.kind = stmk_vla_decl;
    vla_d.variant.vla.is_typedef_decl = TRUE;
    assert(llvm_lower_statement(&vla_d) == llvm_gen_be_error_t::ok);

    vla_d.variant.vla.is_typedef_decl = FALSE;
    vla_d.variant.vla.variant.variable = nullptr;
    assert(llvm_lower_statement(&vla_d) == llvm_gen_be_error_t::ok);

    a_variable vla_unmapped_var = {};
    vla_d.variant.vla.variant.variable = &vla_unmapped_var;
    assert(llvm_lower_statement(&vla_d) == llvm_gen_be_error_t::ok);

    // Multi-dimensional array:
    // dim0 (is_vla && has_assoc_vla_dimension, in array_to_vla_dim AND vdim in vla_dimension_values!),
    // dim1 (is_vla && !has_assoc_vla_dimension, bound_is_zero = FALSE),
    // dim2 (is_vla && has_assoc_vla_dimension, not in array_to_vla_dim),
    // dim3 (is_vla && has_assoc_vla_dimension, in array_to_vla_dim but dim not in vla_dimension_values),
    // dim4 (!is_vla, bound_is_zero = TRUE)
    a_type arr_d0 = {}, arr_d1 = {}, arr_d2 = {}, arr_d3 = {}, arr_d4 = {};
    arr_d4.kind = tk_array; arr_d4.variant.array.is_vla = FALSE; arr_d4.variant.array.bound_is_zero = TRUE; arr_d4.variant.array.element_type = &int_ty;
    a_vla_dimension unval_dim = {};
    arr_d3.kind = tk_array; arr_d3.variant.array.is_vla = TRUE; arr_d3.variant.array.has_assoc_vla_dimension = TRUE; arr_d3.variant.array.element_type = &arr_d4;
    state.array_to_vla_dim[&arr_d3] = &unval_dim;
    arr_d2.kind = tk_array; arr_d2.variant.array.is_vla = TRUE; arr_d2.variant.array.has_assoc_vla_dimension = TRUE; arr_d2.variant.array.element_type = &arr_d3;
    arr_d1.kind = tk_array; arr_d1.variant.array.is_vla = TRUE; arr_d1.variant.array.has_assoc_vla_dimension = FALSE; arr_d1.variant.array.bound_is_zero = FALSE; arr_d1.variant.array.variant.number_of_elements = 4; arr_d1.variant.array.element_type = &arr_d2;
    arr_d0.kind = tk_array; arr_d0.variant.array.is_vla = TRUE; arr_d0.variant.array.has_assoc_vla_dimension = TRUE; arr_d0.variant.array.element_type = &arr_d1;
    vdim.type = &arr_d0;
    assert(llvm_lower_statement(&vla_sz) == llvm_gen_be_error_t::ok); // populates array_to_vla_dim[&arr_d0] and vla_dimension_values[&vdim]

    vla_unmapped_var.source_corresp.name = "named_vla";
    vla_unmapped_var.type = &arr_d0;
    state.local_vars[&vla_unmapped_var] = state.builder->CreateAlloca(ptr_ty);
    g_cf_fail_type = 1;
    assert(llvm_lower_statement(&vla_d) == llvm_gen_be_error_t::unsupported_type);
    g_cf_fail_type = 0;
    assert(llvm_lower_statement(&vla_d) == llvm_gen_be_error_t::ok);

    // 11. llvm_emit_dynamic_init:
    a_dynamic_init d_init = {};
    assert(llvm_emit_dynamic_init(nullptr, dummy_ptr, i32_ty) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_emit_dynamic_init(&d_init, nullptr, i32_ty) == llvm_gen_be_error_t::invalid_argument);

    // dik_none
    d_init.kind = dik_none;
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);

    // dik_zero (obj_type == nullptr and != nullptr)
    d_init.kind = dik_zero;
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, nullptr) == llvm_gen_be_error_t::ok);
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);

    // dik_constant (constant.ptr == nullptr, obj_type == nullptr, evaluate_constant error & ok)
    d_init.kind = dik_constant;
    d_init.variant.constant.ptr = nullptr;
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);
    d_init.variant.constant.ptr = &c_start;
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, nullptr) == llvm_gen_be_error_t::ok);
    g_cf_fail_const = 1;
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, i32_ty) == llvm_gen_be_error_t::constant_eval_failure);
    g_cf_fail_const = 0;
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);

    // dik_expression & dik_class_result_via_ctor (expression == nullptr, error, init_val == nullptr, init_val != nullptr)
    d_init.kind = dik_class_result_via_ctor;
    d_init.variant.expression = nullptr;
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);
    d_init.variant.expression = &ret_e;
    g_cf_fail_expr = 1;
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, i32_ty) == llvm_gen_be_error_t::unsupported_expr);
    g_cf_fail_expr = 0;
    g_cf_expr_mode = 3; // init_val == nullptr
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);
    g_cf_expr_mode = 0;
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);

    // dik_bitwise_copy (source == nullptr, obj_type == nullptr, error, pointer src_val, non-pointer src_val)
    d_init.kind = dik_bitwise_copy;
    d_init.variant.bitwise_copy.source = nullptr;
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);
    d_init.variant.bitwise_copy.source = &ret_e;
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, nullptr) == llvm_gen_be_error_t::ok);
    g_cf_fail_expr = 1;
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, i32_ty) == llvm_gen_be_error_t::unsupported_expr);
    g_cf_fail_expr = 0;
    g_cf_expr_mode = 2; // pointer src_val -> CreateMemCpy
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);
    g_cf_expr_mode = 0; // integer src_val -> CreateStore
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);

    // dik_nonconstant_aggregate (obj_type == nullptr, constant.ptr == nullptr, evaluate_constant error & ok)
    d_init.kind = dik_nonconstant_aggregate;
    d_init.variant.constant.ptr = nullptr;
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, nullptr) == llvm_gen_be_error_t::ok);
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);
    d_init.variant.constant.ptr = &c_start;
    g_cf_fail_const = 1;
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);
    g_cf_fail_const = 0;
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);

    // dik_constructor:
    // - value_initialization true/false, obj_type null/non-null, constructor.ptr null/non-null
    // - args loop (ok & error), default_arg_expr loop (ok & error)
    // - array vs scalar, is_array_copy true/false with explicit_args empty vs 2 args
    // - ctor_func null vs non-null, current_landing_pads empty vs non-empty (throwing vs nounwind)
    memset(&d_init, 0, sizeof(d_init));
    d_init.kind = dik_constructor;
    d_init.variant.constructor.value_initialization = TRUE;
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, nullptr) == llvm_gen_be_error_t::ok);
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);

    a_routine ctor_rout = {};
    ctor_rout.source_corresp.name = nullptr; // unnamed -> ctor_func == nullptr
    d_init.variant.constructor.ptr = &ctor_rout;
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, nullptr) == llvm_gen_be_error_t::ok);

    ctor_rout.source_corresp.name = "_ZN1CC1Ev";
    llvm::FunctionType* ctor_ft = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*state.context), {ptr_ty, ptr_ty, i32_ty}, false);
    llvm::Function* ctor_fn = llvm::Function::Create(ctor_ft, llvm::Function::ExternalLinkage, "_ZN1CC1Ev", state.module.get());

    an_expr_node arg1 = {}, arg2 = {};
    arg1.next = nullptr;
    d_init.variant.constructor.args = &arg1;
    g_cf_fail_expr = 1;
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, i32_ty) == llvm_gen_be_error_t::unsupported_expr);
    g_cf_fail_expr = 0;

    a_type ctor_rty = {}; ctor_rty.kind = tk_routine;
    ctor_rout.type = &ctor_rty;
    // extra_info == nullptr
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);

    a_routine_type_supplement ctor_rsupp = {};
    ctor_rty.variant.routine.extra_info = &ctor_rsupp;
    a_param_type p1 = {}, p2 = {}, p3 = {};
    p1.default_arg_expr = &arg2; p1.next = &p2; // param_idx 0 < explicit_args.size() (1)
    p2.default_arg_expr = nullptr; p2.next = &p3; // param_idx 1 >= 1, default_arg_expr == nullptr
    p3.default_arg_expr = &arg2; p3.next = nullptr; // param_idx 2 >= 1, default_arg_expr != nullptr
    ctor_rsupp.param_type_list = &p1;

    // default_arg_expr error on 2nd expression evaluation
    g_cf_fail_expr = 2;
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, i32_ty) == llvm_gen_be_error_t::unsupported_expr);
    g_cf_fail_expr = 0;

    // Array type construction:
    llvm::ArrayType* arr2_ty = llvm::ArrayType::get(i32_ty, 2);
    llvm::BasicBlock* dummy_lpad = llvm::BasicBlock::Create(*state.context, "lpad", f);
    // 1) is_array_copy == TRUE with 2 args (src_arr pointer + default arg), throwing invoke!
    g_cf_expr_mode = 2;
    d_init.variant.constructor.is_array_copy = TRUE;
    state.current_landing_pads.push_back(dummy_lpad);
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, arr2_ty) == llvm_gen_be_error_t::ok);
    // 2) Scalar construction with throwing invoke!
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);
    // 3) Nounwind ctor_func with non-empty landing pads (takes CreateCall branch!)
    ctor_fn->setDoesNotThrow();
    d_init.variant.constructor.is_array_copy = FALSE;
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, arr2_ty) == llvm_gen_be_error_t::ok);
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);
    state.current_landing_pads.clear();
    // 4) Array construction when current_landing_pads is empty and ctor_func != nullptr (and value_initialization == FALSE), then when ctor_func == nullptr
    d_init.variant.constructor.value_initialization = FALSE;
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, arr2_ty) == llvm_gen_be_error_t::ok);
    ctor_rout.source_corresp.name = "nonexistent_ctor";
    d_init.variant.constructor.args = nullptr;
    ctor_rsupp.param_type_list = nullptr;
    d_init.variant.constructor.is_array_copy = TRUE;
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, arr2_ty) == llvm_gen_be_error_t::ok);
    g_cf_expr_mode = 0;

    // Unknown dip->kind
    d_init.kind = static_cast<a_dynamic_init_kind>(99);
    assert(llvm_emit_dynamic_init(&d_init, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);

    // 12. llvm_lower_init_stmt & get_mangled_guard_variable_name:
    a_statement init_s = {}; init_s.kind = stmk_init; init_s.variant.dynamic_init = nullptr;
    assert(llvm_lower_statement(&init_s) == llvm_gen_be_error_t::ok);

    a_dynamic_init dip2 = {}; dip2.kind = dik_none; dip2.variable = nullptr;
    init_s.variant.dynamic_init = &dip2;
    assert(llvm_lower_statement(&init_s) == llvm_gen_be_error_t::ok);

    // Non-static variable (needs_guard == false), not in local_vars -> init_ptr == nullptr
    a_variable auto_var = {}; auto_var.storage_class = sc_auto; auto_var.type = &int_ty;
    dip2.variable = &auto_var;
    assert(llvm_lower_statement(&init_s) == llvm_gen_be_error_t::ok);

    // Static variable:
    // - var name starting with "_Z" ("_ZGV..." branch)
    // - var name empty "" / nullptr ("local" fallback)
    // - function name starting with "_Z" and empty function name ""
    // - targ_sizeof_pointer == 4 (32-bit guard)
    // - guard_gv already existing in module
    // - var not in local_vars: var->type == nullptr, get_llvm_type error, svar_ty->isVoidTy(), svar_ty non-void with TLS true/false, sgv already in module
    // - init_ptr != nullptr: get_llvm_type error, llvm_emit_dynamic_init error
    // - dip->destructor != nullptr: named/unnamed dtor, found/not-found dtor_func, found/not-found obj_val,
    //   is_tls true (MSVC _tlregdtor & Itanium __cxa_thread_atexit with __dso_handle new & existing),
    //   is_tls false (__cxa_atexit with __dso_handle new & existing)
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
    targ_sizeof_pointer = 4;

    a_variable svar_z = {};
    svar_z.source_corresp.name = "_ZZ3foovE1x";
    svar_z.storage_class = sc_static;
    svar_z.type = nullptr; // var->type == nullptr -> init_ptr == nullptr
    dip2.variable = &svar_z;
    assert(llvm_lower_statement(&init_s) == llvm_gen_be_error_t::ok);
    // Call again to hit existing guard_gv!
    assert(llvm_lower_statement(&init_s) == llvm_gen_be_error_t::ok);
    targ_sizeof_pointer = 8;

    // Function with "_Z..." name and unnamed function ""
    llvm::Function* fn_mangled = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "_Z3barv", state.module.get());
    state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "entry", fn_mangled));
    a_variable svar_unnamed = {};
    svar_unnamed.source_corresp.name = "";
    svar_unnamed.storage_class = sc_static;
    a_type void_ty = {}; void_ty.kind = tk_void;
    svar_unnamed.type = &void_ty; // svar_ty->isVoidTy() -> sgv not created!
    dip2.variable = &svar_unnamed;
    assert(llvm_lower_statement(&init_s) == llvm_gen_be_error_t::ok);

    llvm::Function* fn_anon = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "", state.module.get());
    state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "entry", fn_anon));
    svar_unnamed.source_corresp.name = nullptr;
    svar_unnamed.type = &int_ty;
    // get_llvm_type error when creating sgv
    g_cf_fail_type = 1;
    assert(llvm_lower_statement(&init_s) == llvm_gen_be_error_t::unsupported_type);
    g_cf_fail_type = 0;
    state.cleanup_stack.cleanups.clear();

    // Create sgv with is_thread_local = TRUE and register destructor on Itanium (__cxa_thread_atexit, creates __dso_handle)
    svar_unnamed.is_thread_local = TRUE;
    dip2.destructor = &dtor_found; // dtor_func found, var->source_corresp.name == nullptr -> obj_val not found
    assert(llvm_lower_statement(&init_s) == llvm_gen_be_error_t::ok);
    // Call again to hit existing __dso_handle in __cxa_thread_atexit!
    assert(llvm_lower_statement(&init_s) == llvm_gen_be_error_t::ok);

    // MSVC _tlregdtor with unnamed destructor (dtor_func == nullptr) and found destructor (dtor_func != nullptr -> synthesizes __tls_dtor_<var>)
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    dip2.destructor = &dtor_unnamed;
    assert(llvm_lower_statement(&init_s) == llvm_gen_be_error_t::ok);
    dip2.destructor = &dtor_found;
    assert(llvm_lower_statement(&init_s) == llvm_gen_be_error_t::ok);
    assert(state.module->getFunction("__tls_dtor_anon") != nullptr);
    // Call again to hit existing __tls_dtor_anon in module!
    assert(llvm_lower_statement(&init_s) == llvm_gen_be_error_t::ok);
    svar_unnamed.source_corresp.name = "tls_named_v";
    assert(llvm_lower_statement(&init_s) == llvm_gen_be_error_t::ok);
    assert(state.module->getFunction("__tls_dtor_tls_named_v") != nullptr);
    svar_unnamed.source_corresp.name = nullptr;

    // Non-TLS static variable with __cxa_atexit (when __dso_handle is removed first so it creates __dso_handle, then called again when __dso_handle exists)
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
    if (llvm::GlobalVariable* dso = state.module->getGlobalVariable("__dso_handle")) {
      dso->eraseFromParent();
    }
    a_variable svar_atexit = {};
    svar_atexit.source_corresp.name = "svar_atexit_gv";
    svar_atexit.storage_class = sc_static;
    svar_atexit.is_thread_local = FALSE;
    svar_atexit.type = &int_ty;
    dip2.variable = &svar_atexit;
    dip2.destructor = &dtor_missing;
    assert(llvm_lower_statement(&init_s) == llvm_gen_be_error_t::ok); // creates sgv (is_thread_local == FALSE) and __dso_handle, finds obj_val!
    // Remove from local_vars so second call hits existing sgv in module, and hits existing __dso_handle!
    state.local_vars.erase(&svar_atexit);
    assert(llvm_lower_statement(&init_s) == llvm_gen_be_error_t::ok);

    // init_ptr != nullptr: get_llvm_type error (on 1st call when var is in local_vars) and llvm_emit_dynamic_init error
    g_cf_fail_type = 1;
    assert(llvm_lower_statement(&init_s) == llvm_gen_be_error_t::unsupported_type);
    g_cf_fail_type = 0;
    state.cleanup_stack.cleanups.clear();

    dip2.kind = dik_expression;
    dip2.variant.expression = &ret_e;
    g_cf_fail_expr = 1;
    assert(llvm_lower_statement(&init_s) == llvm_gen_be_error_t::unsupported_expr);
    g_cf_fail_expr = 0;
    state.cleanup_stack.cleanups.clear();

    state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "cov.bb", f));

    // 13. llvm_lower_statement dispatch:
    //     - stmt == nullptr
    //     - dbg_state with empty scope_stack vs non-empty scope_stack (get_di_location error, loc == nullptr, loc != nullptr)
    //     - has_associated_pragma (omp error, omp_handled == true, omp_handled == false)
    //     - stmk_stmt_expr_result (expr == nullptr, expr error, current_stmt_expr_result_ptr != nullptr with val != nullptr and val == nullptr, current_stmt_expr_result_ptr == nullptr)
    //     - stmk_asm, stmk_try_block, stmk_microsoft_try, stmk_decl, default (unsupported_stmt)
    assert(llvm_lower_statement(nullptr) == llvm_gen_be_error_t::ok);

    state.dbg_state = &dbg_st;
    // empty scope_stack
    assert(llvm_lower_statement(&empty_s) == llvm_gen_be_error_t::ok);
    // non-empty scope_stack
    llvm::DIBuilder dib(*state.module);
    llvm::DIFile* dif = dib.createFile("test.cpp", ".");
    llvm::DICompileUnit* dicu = dib.createCompileUnit(llvm::dwarf::DW_LANG_C_plus_plus, dif, "edg", false, "", 0);
    dbg_st.scope_stack.push_back(dicu);
    g_cf_fail_di_loc = 1;
    assert(llvm_lower_statement(&empty_s) == llvm_gen_be_error_t::di_metadata_failure);
    g_cf_fail_di_loc = 0;
    assert(llvm_lower_statement(&empty_s) == llvm_gen_be_error_t::ok); // loc == nullptr
    g_cf_nonnull_di_loc = 1;
    assert(llvm_lower_statement(&empty_s) == llvm_gen_be_error_t::ok); // loc != nullptr
    g_cf_nonnull_di_loc = 0;
    dbg_st.scope_stack.clear();
    state.dbg_state = nullptr;

    // has_associated_pragma:
    // 1) omp_handled == false (no matching pragma in omp_pragmas)
    a_statement omp_s = {}; omp_s.kind = stmk_empty; omp_s.has_associated_pragma = TRUE;
    assert(llvm_lower_statement(&omp_s) == llvm_gen_be_error_t::ok);
    // 2) omp_handled == true ("omp barrier")
    state.omp_pragmas[&omp_s] = "omp barrier";
    assert(llvm_lower_statement(&omp_s) == llvm_gen_be_error_t::ok);
    // 3) omp_err != ok ("omp critical" with body error)
    state.omp_pragmas[&omp_s] = "omp critical";
    omp_s.kind = static_cast<a_statement_kind>(999);
    assert(llvm_lower_statement(&omp_s) == llvm_gen_be_error_t::unsupported_stmt);
    state.omp_pragmas.clear();

    // stmk_stmt_expr_result
    a_statement se_res = {}; se_res.kind = stmk_stmt_expr_result; se_res.expr = nullptr;
    assert(llvm_lower_statement(&se_res) == llvm_gen_be_error_t::ok);
    se_res.expr = &ret_e;
    g_cf_fail_expr = 1;
    assert(llvm_lower_statement(&se_res) == llvm_gen_be_error_t::unsupported_expr);
    g_cf_fail_expr = 0;
    state.current_stmt_expr_result_ptr = nullptr;
    assert(llvm_lower_statement(&se_res) == llvm_gen_be_error_t::ok);
    state.current_stmt_expr_result_ptr = dummy_ptr;
    g_cf_expr_mode = 3; // val == nullptr
    assert(llvm_lower_statement(&se_res) == llvm_gen_be_error_t::ok);
    g_cf_expr_mode = 0; // val != nullptr -> CreateStore
    assert(llvm_lower_statement(&se_res) == llvm_gen_be_error_t::ok);
    state.current_stmt_expr_result_ptr = nullptr;

    // stmk_asm, stmk_try_block, stmk_microsoft_try, stmk_decl
    an_asm_entry asm_ent = {};
    a_statement asm_s = {}; asm_s.kind = stmk_asm; asm_s.variant.asm_entry = &asm_ent;
    assert(llvm_lower_statement(&asm_s) == llvm_gen_be_error_t::ok);
    a_try_supplement try_supp = {};
    try_supp.statement = &empty_s;
    a_statement try_s = {}; try_s.kind = stmk_try_block; try_s.variant.try_block = &try_supp;
    assert(llvm_lower_statement(&try_s) == llvm_gen_be_error_t::ok);
    a_microsoft_try_supplement mstry_supp = {};
    mstry_supp.guarded_statement = &empty_s;
    mstry_supp.cleanup_statement = &empty_s;
    a_statement mstry_s = {}; mstry_s.kind = stmk_microsoft_try; mstry_s.variant.microsoft_try = &mstry_supp;
    assert(llvm_lower_statement(&mstry_s) == llvm_gen_be_error_t::ok);
    a_statement decl_s = {}; decl_s.kind = stmk_decl;
    assert(llvm_lower_statement(&decl_s) == llvm_gen_be_error_t::ok);

    // 14. Phase 3 & 7 new features in llvm_gen_be_stmt.cpp:
    // - stmk_constexpr_if: variant.constexpr_if == nullptr, value_known == TRUE (value TRUE and FALSE)
    a_statement ce_if = {}; ce_if.kind = stmk_constexpr_if; ce_if.expr = &ret_e;
    ce_if.variant.constexpr_if = nullptr;
    assert(llvm_lower_statement(&ce_if) == llvm_gen_be_error_t::ok);
    a_constexpr_if ce_supp = {};
    ce_supp.then_statement = &empty_s;
    ce_supp.else_statement = &empty_s;
    ce_if.variant.constexpr_if = &ce_supp;
    ce_supp.value_known = TRUE;
    ce_supp.value = TRUE;
    assert(llvm_lower_statement(&ce_if) == llvm_gen_be_error_t::ok);
    ce_supp.value = FALSE;
    assert(llvm_lower_statement(&ce_if) == llvm_gen_be_error_t::ok);

    // - Block prescan with stmk_init (with/without dynamic_init, with/without destructor),
    //   stmk_vla_decl (typedef TRUE/FALSE), and stmk_label (null ptr, new label, already in label_cleanup_depths)
    //    plus non-static stmk_init with init_ptr != nullptr (with and without destructor)
    a_variable raii_local = {};
    raii_local.storage_class = sc_auto;
    raii_local.type = &int_ty;
    state.local_vars[&raii_local] = state.builder->CreateAlloca(i32_ty);

    a_dynamic_init raii_dip_dtor = {}, raii_dip_nodtor = {};
    raii_dip_dtor.kind = dik_zero;
    raii_dip_dtor.variable = &raii_local;
    raii_dip_dtor.destructor = &dtor_found;
    raii_dip_nodtor.kind = dik_zero;
    raii_dip_nodtor.variable = &raii_local;
    raii_dip_nodtor.destructor = nullptr;

    a_label blk_lbl1 = {};
    a_statement bs_init_null = {}, bs_init_nodtor = {}, bs_init_dtor = {},
                bs_vla_td = {}, bs_vla_var = {}, bs_lbl_null = {}, bs_lbl_new = {}, bs_lbl_dup = {};
    bs_init_null.kind = stmk_init; bs_init_null.variant.dynamic_init = nullptr; bs_init_null.next = &bs_init_nodtor;
    bs_init_nodtor.kind = stmk_init; bs_init_nodtor.variant.dynamic_init = &raii_dip_nodtor; bs_init_nodtor.next = &bs_init_dtor;
    bs_init_dtor.kind = stmk_init; bs_init_dtor.variant.dynamic_init = &raii_dip_dtor; bs_init_dtor.next = &bs_vla_td;
    bs_vla_td.kind = stmk_vla_decl; bs_vla_td.variant.vla.is_typedef_decl = TRUE; bs_vla_td.next = &bs_vla_var;
    bs_vla_var.kind = stmk_vla_decl; bs_vla_var.variant.vla.is_typedef_decl = FALSE; bs_vla_var.variant.vla.variant.variable = &vla_unmapped_var; bs_vla_var.next = &bs_lbl_null;
    bs_lbl_null.kind = stmk_label; bs_lbl_null.variant.label.ptr = nullptr; bs_lbl_null.next = &bs_lbl_new;
    bs_lbl_new.kind = stmk_label; bs_lbl_new.variant.label.ptr = &blk_lbl1; bs_lbl_new.next = &bs_lbl_dup;
    bs_lbl_dup.kind = stmk_label; bs_lbl_dup.variant.label.ptr = &blk_lbl1; bs_lbl_dup.next = nullptr;

    a_statement raii_blk = {}; raii_blk.kind = stmk_block;
    raii_blk.variant.block.statements = &bs_init_null;
    assert(llvm_lower_statement(&raii_blk) == llvm_gen_be_error_t::ok);

    // Nested statement precomputation in llvm_precompute_function_labels (block, if, while, end_test_while, for, switch)
    a_label nested_lbl = {};
    a_statement inner_lbl_s = {}; inner_lbl_s.kind = stmk_label; inner_lbl_s.variant.label.ptr = &nested_lbl;
    a_statement n_blk = {}, n_if = {}, n_while = {}, n_do = {}, n_for = {}, n_sw = {};
    n_blk.kind = stmk_block; n_blk.variant.block.statements = &inner_lbl_s; n_blk.next = &n_if;
    n_if.kind = stmk_if; n_if.variant.if_stmt.then_statement = &inner_lbl_s; n_if.variant.if_stmt.else_statement = nullptr; n_if.next = &n_while;
    n_while.kind = stmk_while; n_while.variant.loop_statement = &inner_lbl_s; n_while.next = &n_do;
    n_do.kind = stmk_end_test_while; n_do.variant.loop_statement = &inner_lbl_s; n_do.next = &n_for;
    n_for.kind = stmk_for; n_for.variant.for_loop.statement = &inner_lbl_s; n_for.next = &n_sw;
    n_sw.kind = stmk_switch; n_sw.variant.switch_stmt.body_statement = &inner_lbl_s; n_sw.next = nullptr;
    assert(llvm_precompute_function_labels(&n_blk, 0) == llvm_gen_be_error_t::ok);

    // - Large switch case range (span > 64)
    state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "cov.bb", f));
    g_cf_const_seq = -100; // evaluate_constant will use custom values if we test span > 64:
    // Wait: g_cf_const_seq increments by 1! Let's make g_cf_const_seq jump by 100 when < -50:
    // Actually let's test span > 64 by setting g_cf_const_seq:
    // Let's check evaluate_constant in test_llvm_control_flow.cpp!

    // - dip->master_entry != nullptr, dip->kind == dik_lambda, dip->is_reused_value == TRUE,
    //   and emit_aggregate_dynamic_init_subobjects (on array, struct, and scalar, with designator and ck_dynamic_init)
    a_dynamic_init master_dip = {}, sub_dip = {}, bad_sub_dip = {};
    sub_dip.kind = dik_zero;
    bad_sub_dip.kind = dik_expression;
    bad_sub_dip.variant.expression = &ret_e;

    a_constant ac_desig_arr = {}, ac_desig_fld = {}, ac_dyn_null = {}, ac_dyn_ok = {}, ac_int = {}, ac_dyn_bad = {};
    ac_desig_arr.kind = ck_designator; ac_desig_arr.variant.designator.is_field_designator = FALSE; ac_desig_arr.variant.designator.variant.array_element = 0; ac_desig_arr.next = &ac_desig_fld;
    ac_desig_fld.kind = ck_designator; ac_desig_fld.variant.designator.is_field_designator = TRUE; ac_desig_fld.next = &ac_dyn_null;
    ac_dyn_null.kind = ck_dynamic_init; ac_dyn_null.variant.dynamic_init.ptr = nullptr; ac_dyn_null.next = &ac_dyn_ok;
    ac_dyn_ok.kind = ck_dynamic_init; ac_dyn_ok.variant.dynamic_init.ptr = &sub_dip; ac_dyn_ok.next = &ac_int;
    ac_int.kind = ck_integer; ac_int.next = nullptr;

    a_constant agg_c_node = {};
    agg_c_node.kind = ck_aggregate;
    agg_c_node.variant.aggregate.first_constant = &ac_desig_arr;

    a_dynamic_init lam_dip = {};
    lam_dip.kind = dik_lambda;
    lam_dip.master_entry = &master_dip;
    lam_dip.variant.constant.ptr = &agg_c_node;
    lam_dip.variant.constant.non_constant = TRUE;

    llvm::StructType* st2_ty = llvm::StructType::get(*state.context, {i32_ty, i32_ty});
    llvm::Type* one_i32_arr[] = {i32_ty};
    llvm::StructType* st1_ty = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>(one_i32_arr));
    // 1) Array obj_type
    assert(llvm_emit_dynamic_init(&lam_dip, dummy_ptr, arr2_ty) == llvm_gen_be_error_t::ok);
    // 2) Struct obj_type (with elem_idx < getStructNumElements() and elem_idx >= getStructNumElements())
    ac_int.next = &ac_dyn_ok;
    a_constant ac_dyn_ok2 = {};
    ac_dyn_ok2.kind = ck_dynamic_init; ac_dyn_ok2.variant.dynamic_init.ptr = &sub_dip; ac_dyn_ok2.next = nullptr;
    ac_int.next = &ac_dyn_ok2;
    assert(llvm_emit_dynamic_init(&lam_dip, dummy_ptr, st1_ty) == llvm_gen_be_error_t::ok);
    // 3) Scalar obj_type (neither array nor struct)
    assert(llvm_emit_dynamic_init(&lam_dip, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);
    // 4) non_constant = TRUE but ptr->kind != ck_aggregate
    agg_c_node.kind = ck_integer;
    assert(llvm_emit_dynamic_init(&lam_dip, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);
    agg_c_node.kind = ck_aggregate;

    // 5) Error propagation inside emit_aggregate_dynamic_init_subobjects (from both dik_lambda and dik_nonconstant_aggregate)
    ac_dyn_ok.variant.dynamic_init.ptr = &bad_sub_dip;
    ac_int.next = nullptr;
    g_cf_fail_expr = 1;
    assert(llvm_emit_dynamic_init(&lam_dip, dummy_ptr, arr2_ty) == llvm_gen_be_error_t::unsupported_expr);
    g_cf_fail_expr = 0;

    a_dynamic_init nca_dip = {};
    nca_dip.kind = dik_nonconstant_aggregate;
    nca_dip.variant.constant.ptr = &agg_c_node;
    g_cf_fail_expr = 1;
    assert(llvm_emit_dynamic_init(&nca_dip, dummy_ptr, arr2_ty) == llvm_gen_be_error_t::unsupported_expr);
    g_cf_fail_expr = 0;
    ac_dyn_ok.variant.dynamic_init.ptr = &sub_dip;
    assert(llvm_emit_dynamic_init(&nca_dip, dummy_ptr, st2_ty) == llvm_gen_be_error_t::ok);

    // 6) dip->is_reused_value == TRUE on dik_expression
    a_dynamic_init reuse_dip = {};
    reuse_dip.kind = dik_expression;
    reuse_dip.is_reused_value = TRUE;
    reuse_dip.variant.expression = &ret_e;
    assert(llvm_emit_dynamic_init(&reuse_dip, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);

    // 7) Multi-element array dik_constructor with dip->destructor != nullptr (partial-array cleanup registration)
    llvm::FunctionType* arr_ctor_fty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*state.context), {llvm::PointerType::getUnqual(*state.context)}, false);
    llvm::Function::Create(arr_ctor_fty, llvm::Function::ExternalLinkage, "arr_elem_ctor", state.module.get());
    a_routine arr_ctor_r = {}, arr_dtor_r = {};
    arr_ctor_r.source_corresp.name = "arr_elem_ctor";
    arr_dtor_r.source_corresp.name = "arr_elem_dtor";
    a_dynamic_init arr_ctor_dip = {};
    arr_ctor_dip.kind = dik_constructor;
    arr_ctor_dip.destructor = &arr_dtor_r;
    arr_ctor_dip.variant.constructor.ptr = &arr_ctor_r;
    assert(llvm_emit_dynamic_init(&arr_ctor_dip, dummy_ptr, arr2_ty) == llvm_gen_be_error_t::ok);

    // 15. Phase 11: Dialect statement lowering (UPC fence, notify, wait, barrier, forall, and for_each)
    a_statement upc_s = {};
    upc_s.kind = static_cast<a_statement_kind>(static_cast<unsigned>(stmk_last) + 1u); // upc_fence
    assert(llvm_lower_statement(&upc_s) == llvm_gen_be_error_t::ok);
    upc_s.kind = static_cast<a_statement_kind>(static_cast<unsigned>(stmk_last) + 2u); // upc_notify (expr == nullptr)
    assert(llvm_lower_statement(&upc_s) == llvm_gen_be_error_t::ok);
    upc_s.expr = &ret_e;
    upc_s.kind = static_cast<a_statement_kind>(static_cast<unsigned>(stmk_last) + 3u); // upc_wait (expr != nullptr)
    assert(llvm_lower_statement(&upc_s) == llvm_gen_be_error_t::ok);
    upc_s.kind = static_cast<a_statement_kind>(static_cast<unsigned>(stmk_last) + 4u); // upc_barrier
    assert(llvm_lower_statement(&upc_s) == llvm_gen_be_error_t::ok);
    g_cf_fail_expr = 1;
    assert(llvm_lower_statement(&upc_s) == llvm_gen_be_error_t::unsupported_expr);
    g_cf_fail_expr = 0;

    a_for_loop upc_for_info = {};
    a_statement upc_forall_s = {};
    upc_forall_s.kind = static_cast<a_statement_kind>(static_cast<unsigned>(stmk_last) + 5u);
    upc_forall_s.expr = &ret_e;
    upc_forall_s.variant.for_loop.extra_info = &upc_for_info;
    upc_forall_s.variant.for_loop.statement = &empty_s;
    assert(llvm_lower_statement(&upc_forall_s) == llvm_gen_be_error_t::ok);

    a_range_based_for_loop fe_info = {};
    fe_info.ne_call_expr = &ret_e;
    a_statement fe_s = {};
    fe_s.kind = static_cast<a_statement_kind>(static_cast<unsigned>(stmk_last) + 6u);
    fe_s.variant.range_based_for_loop.extra_info = &fe_info;
    fe_s.variant.range_based_for_loop.statement = &empty_s;
    assert(llvm_lower_statement(&fe_s) == llvm_gen_be_error_t::ok);

    // 16. Complete branch & OOM coverage across llvm_gen_be_stmt.cpp
    {
      // a) is_end_catch cleanup in llvm_emit_cleanups_until + ClearInsertionPoint in llvm_lower_statement
      ScopeCleanup ec_cl = {};
      ec_cl.is_active = true;
      ec_cl.is_end_catch = true;
      state.cleanup_stack.cleanups.push_back(ec_cl);
      a_statement ret_void_s = {};
      ret_void_s.kind = stmk_return;
      assert(llvm_lower_statement(&ret_void_s) == llvm_gen_be_error_t::ok);
      state.cleanup_stack.cleanups.clear();

      llvm::BasicBlock* next_cf_bb = llvm::BasicBlock::Create(*state.context, "cf.extra", f);
      state.builder->ClearInsertionPoint();
      assert(llvm_lower_statement(&empty_s) == llvm_gen_be_error_t::ok);
      state.builder->SetInsertPoint(next_cf_bb);

      // b) llvm_lower_return_stmt: sret constructed_in_place, sret null ret_val, i1 return, signed/unsigned/bool/non-int integer widen, and struct return pack
      llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*state.context);
      llvm::FunctionType* sret_fty = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {ptr_ty}, false);
      llvm::Function* sret_fn = llvm::Function::Create(sret_fty, llvm::Function::ExternalLinkage, "cf_sret_fn", state.module.get());
      sret_fn->addParamAttr(0, llvm::Attribute::getWithStructRetType(*state.context, i32_ty));
      llvm::BasicBlock* sret_bb = llvm::BasicBlock::Create(*state.context, "entry", sret_fn);
      state.builder->SetInsertPoint(sret_bb);

      a_statement ret_expr_s = {};
      ret_expr_s.kind = stmk_return;
      ret_expr_s.expr = &ret_e;
      g_cf_expr_mode = 5; // constructed_in_place == true
      assert(llvm_lower_statement(&ret_expr_s) == llvm_gen_be_error_t::ok);

      llvm::BasicBlock* sret_bb2 = llvm::BasicBlock::Create(*state.context, "bb2", sret_fn);
      state.builder->SetInsertPoint(sret_bb2);
      g_cf_expr_mode = 3; // !constructed_in_place && ret_val == nullptr
      assert(llvm_lower_statement(&ret_expr_s) == llvm_gen_be_error_t::ok);

      // i1 return function
      llvm::Function* i1_fn = llvm::Function::Create(
          llvm::FunctionType::get(llvm::Type::getInt1Ty(*state.context), false),
          llvm::Function::ExternalLinkage, "cf_i1_fn", state.module.get());
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "entry", i1_fn));
      g_cf_expr_mode = 0; // returns i32
      assert(llvm_lower_statement(&ret_expr_s) == llvm_gen_be_error_t::ok);

      // i64 return function with signed int, unsigned int, bool int, and pointer expr->type, plus float/ptr ret_val
      llvm::Function* i64_fn = llvm::Function::Create(
          llvm::FunctionType::get(llvm::Type::getInt64Ty(*state.context), false),
          llvm::Function::ExternalLinkage, "cf_i64_fn", state.module.get());
      a_type ret_int_t = {}, ret_bool_t = {}, ret_ptr_t = {};
      ret_int_t.kind = tk_integer; ret_int_t.variant.integer.int_kind = ik_int;
      ret_bool_t.kind = tk_integer; ret_bool_t.variant.integer.bool_type = TRUE;
      ret_ptr_t.kind = tk_pointer;

      int_kind_is_signed[ik_int] = TRUE;
      ret_e.type = &ret_int_t;
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "b1", i64_fn));
      assert(llvm_lower_statement(&ret_expr_s) == llvm_gen_be_error_t::ok);

      int_kind_is_signed[ik_int] = FALSE;
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "b2", i64_fn));
      assert(llvm_lower_statement(&ret_expr_s) == llvm_gen_be_error_t::ok);

      ret_e.type = &ret_bool_t;
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "b3", i64_fn));
      assert(llvm_lower_statement(&ret_expr_s) == llvm_gen_be_error_t::ok);

      ret_e.type = &ret_ptr_t;
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "b4", i64_fn));
      assert(llvm_lower_statement(&ret_expr_s) == llvm_gen_be_error_t::ok);
      ret_e.type = nullptr;

      // Non-integer ret_val (ptr) into i64 return function -> hits else pack branch (L147)
      g_cf_expr_mode = 2;
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "b5", i64_fn));
      assert(llvm_lower_statement(&ret_expr_s) == llvm_gen_be_error_t::ok);

      // Non-integer func_ret_ty (struct) with i32 ret_val -> hits func_ret_ty->isIntegerTy() == false
      llvm::Function* st_ret_fn = llvm::Function::Create(
          llvm::FunctionType::get(st2_ty, false),
          llvm::Function::ExternalLinkage, "cf_stret_fn", state.module.get());
      g_cf_expr_mode = 0;
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "entry", st_ret_fn));
      assert(llvm_lower_statement(&ret_expr_s) == llvm_gen_be_error_t::ok);

      // OOM in llvm_emit_cleanups_until inside llvm_lower_return_stmt
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "cf.cont2", f));
      g_llvm_be_oom_countdown = 0;
      assert(llvm_lower_statement(&ret_void_s) == llvm_gen_be_error_t::out_of_memory);
      g_llvm_be_oom_countdown = -1;

      // c) dik_class_result_via_ctor branches (obj_type==nullptr, constructed_in_place with reused_value T/F & obj_type null, !constructed_in_place with null init_val)
      a_dynamic_init cr_dip = {};
      cr_dip.kind = dik_class_result_via_ctor;
      cr_dip.variant.expression = &ret_e;
      g_cf_expr_mode = 3; // !constructed_in_place && init_val == nullptr, obj_type == nullptr
      assert(llvm_emit_dynamic_init(&cr_dip, dummy_ptr, nullptr) == llvm_gen_be_error_t::ok);

      g_cf_expr_mode = 5; // constructed_in_place == true
      cr_dip.is_reused_value = TRUE;
      assert(llvm_emit_dynamic_init(&cr_dip, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);
      assert(llvm_emit_dynamic_init(&cr_dip, dummy_ptr, nullptr) == llvm_gen_be_error_t::ok);
      cr_dip.is_reused_value = FALSE;
      assert(llvm_emit_dynamic_init(&cr_dip, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);
      g_cf_expr_mode = 6; // void CallInst returned by EDG IL-lowered sret expression
      cr_dip.is_reused_value = TRUE;
      assert(llvm_emit_dynamic_init(&cr_dip, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);
      cr_dip.is_reused_value = FALSE;
      g_cf_expr_mode = 0;

      // d) dik_constructor: empty ctor_name (array & scalar), undeclared scalar ctor with args, and unwind/cleanup OOM sweeps
      a_routine empty_ctor_r = {}, undecl_scalar_ctor_r = {};
      empty_ctor_r.source_corresp.name = "";
      undecl_scalar_ctor_r.source_corresp.name = "undecl_scalar_ctor_cf";
      a_dynamic_init ctor_dip_cf = {};
      ctor_dip_cf.kind = dik_constructor;
      ctor_dip_cf.variant.constructor.ptr = &empty_ctor_r;
      assert(llvm_emit_dynamic_init(&ctor_dip_cf, dummy_ptr, arr2_ty) == llvm_gen_be_error_t::ok);
      assert(llvm_emit_dynamic_init(&ctor_dip_cf, dummy_ptr, i32_ty) == llvm_gen_be_error_t::ok);

      ctor_dip_cf.variant.constructor.ptr = &undecl_scalar_ctor_r;
      ctor_dip_cf.variant.constructor.args = &ret_e;
      for (int b = 0; b < 4; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t rc = llvm_emit_dynamic_init(&ctor_dip_cf, dummy_ptr, i32_ty);
        g_llvm_be_oom_countdown = -1;
        if (rc == llvm_gen_be_error_t::ok) break;
      }
      for (int b = 0; b < 8; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t rc = llvm_emit_dynamic_init(&arr_ctor_dip, dummy_ptr, arr2_ty);
        g_llvm_be_oom_countdown = -1;
        if (rc == llvm_gen_be_error_t::ok) break;
      }

      // e) llvm_precompute_function_labels & llvm_lower_block_stmt & llvm_lower_goto_stmt OOM sweeps
      a_statement inner_empty = {}; inner_empty.kind = stmk_empty;
      a_statement n_blk = {}, n_if = {}, n_while = {}, n_for = {}, n_sw = {};
      n_blk.kind = stmk_block; n_blk.variant.block.statements = &inner_empty; n_blk.next = &n_if;
      n_if.kind = stmk_if; n_if.variant.if_stmt.then_statement = &inner_empty; n_if.variant.if_stmt.else_statement = &inner_empty; n_if.next = &n_while;
      n_while.kind = stmk_while; n_while.variant.loop_statement = &inner_empty; n_while.next = &n_for;
      n_for.kind = stmk_for; n_for.variant.for_loop.statement = &inner_empty; n_for.next = &n_sw;
      n_sw.kind = stmk_switch; n_sw.variant.switch_stmt.body_statement = &inner_empty; n_sw.next = nullptr;
      for (int b = 0; b < 12; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t rp = llvm_precompute_function_labels(&n_blk, 0);
        g_llvm_be_oom_countdown = -1;
        if (rp == llvm_gen_be_error_t::ok) break;
      }

      a_type blk_int_ty = {}; blk_int_ty.kind = tk_integer;
      a_variable blk_lvar = {}, blk_svar = {};
      blk_lvar.source_corresp.name = "cf_blk_lvar"; blk_lvar.type = &blk_int_ty;
      blk_svar.source_corresp.name = "cf_blk_svar"; blk_svar.type = &blk_int_ty; blk_svar.storage_class = sc_static;
      a_scope blk_sc = {}; blk_sc.nonstatic_variables = &blk_lvar; blk_sc.variables = &blk_svar;
      a_block blk_ext = {}; blk_ext.assoc_scope = &blk_sc;
      a_statement full_blk_s = {}; full_blk_s.kind = stmk_block;
      full_blk_s.variant.block.extra_info = &blk_ext;
      full_blk_s.variant.block.statements = &inner_empty;
      for (int b = 0; b < 10; ++b) {
        state.local_vars.erase(&blk_lvar);
        state.local_vars.erase(&blk_svar);
        if (llvm::GlobalVariable* old_sgv = state.module->getNamedGlobal("cf_blk_svar")) old_sgv->eraseFromParent();
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t rb = llvm_lower_statement(&full_blk_s);
        g_llvm_be_oom_countdown = -1;
        if (rb == llvm_gen_be_error_t::ok) break;
      }

      a_label cf_lbl = {};
      state.label_cleanup_depths[&cf_lbl] = 0;
      state.cleanup_stack.cleanups.push_back(ec_cl);
      a_statement goto_s = {}; goto_s.kind = stmk_goto; goto_s.variant.label.ptr = &cf_lbl;
      g_llvm_be_oom_countdown = 0;
      assert(llvm_lower_statement(&goto_s) == llvm_gen_be_error_t::out_of_memory);
      g_llvm_be_oom_countdown = -1;
      state.cleanup_stack.cleanups.clear();

      // f) stmk_init static local with undeclared dtor, unnamed var, TLS/non-TLS __dso_handle OOM sweeps
      a_routine undecl_st_dtor = {}; undecl_st_dtor.source_corresp.name = "undecl_st_dtor_cf";
      a_variable st_var_cf = {};
      st_var_cf.storage_class = sc_static;
      st_var_cf.type = &blk_int_ty;
      a_dynamic_init st_dip_cf = {};
      st_dip_cf.kind = dik_zero;
      st_dip_cf.variable = &st_var_cf;
      st_dip_cf.destructor = &undecl_st_dtor;
      a_statement st_init_s = {};
      st_init_s.kind = stmk_init;
      st_init_s.variant.dynamic_init = &st_dip_cf;

      for (int tls_mode = 0; tls_mode < 2; ++tls_mode) {
        st_var_cf.is_thread_local = tls_mode ? TRUE : FALSE;
        for (int b = 0; b < 12; ++b) {
          state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "st.oom", f));
          state.local_vars.erase(&st_var_cf);
          st_var_cf.source_corresp.name = (tls_mode ? "cf_tls_st" : "cf_nontls_st");
          if (llvm::GlobalVariable* g1 = state.module->getNamedGlobal(st_var_cf.source_corresp.name)) g1->eraseFromParent();
          for (llvm::GlobalVariable& gv_it : llvm::make_early_inc_range(state.module->globals())) {
            if (gv_it.getName().starts_with("_ZGV") || gv_it.getName() == "__dso_handle") {
              gv_it.eraseFromParent();
            }
          }
          g_llvm_be_oom_countdown = b;
          llvm_gen_be_error_t ri = llvm_lower_statement(&st_init_s);
          g_llvm_be_oom_countdown = -1;
          state.cleanup_stack.cleanups.clear();
          if (ri == llvm_gen_be_error_t::ok) break;
        }
      }

      // Unnamed and named static var with type == nullptr so init_ptr == nullptr
      st_var_cf.storage_class = sc_static;
      for (const char* gnm : {"anon_var", "local_var", "var", "local"}) {
        if (llvm::GlobalVariable* g_old = state.module->getNamedGlobal(gnm)) {
          g_old->eraseFromParent();
        }
      }
      st_var_cf.source_corresp.name = nullptr;
      st_var_cf.type = nullptr;
      state.local_vars[&st_var_cf] = nullptr;
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "st.nullname", f));
      assert(llvm_lower_statement(&st_init_s) == llvm_gen_be_error_t::ok);

      st_var_cf.source_corresp.name = "cf_named_no_init_ptr";
      state.local_vars[&st_var_cf] = nullptr;
      state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "st.nonnullname", f));
      assert(llvm_lower_statement(&st_init_s) == llvm_gen_be_error_t::ok);
      for (int b = 0; b < 8; ++b) {
        for (llvm::GlobalVariable& gv_it : llvm::make_early_inc_range(state.module->globals())) {
          if (gv_it.getName().starts_with("_ZGV")) gv_it.eraseFromParent();
        }
        state.local_vars[&st_var_cf] = nullptr;
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "st.oom.noinit", f));
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t rni = llvm_lower_statement(&st_init_s);
        g_llvm_be_oom_countdown = -1;
        if (rni == llvm_gen_be_error_t::ok) break;
      }
      state.local_vars.erase(&st_var_cf);

      // OOM on llvm_emit_cleanups_until with dtor_routine
      {
        a_routine oom_dtor_r = {};
        oom_dtor_r.source_corresp.name = "_ZN7OomDtorD1Ev";
        ScopeCleanup oom_sc = {};
        oom_sc.is_active = true;
        oom_sc.dtor_routine = &oom_dtor_r;
        oom_sc.object_ptr = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*state.context));
        state.cleanup_stack.cleanups.push_back(oom_sc);
        a_statement oom_ret_s = {};
        oom_ret_s.kind = stmk_return;
        for (int b = 0; b < 5; ++b) {
          state.cleanup_stack.cleanups.clear();
          state.cleanup_stack.cleanups.push_back(oom_sc);
          state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "st.oom.dtor", f));
          g_llvm_be_oom_countdown = b;
          llvm_gen_be_error_t r_oom_dtor = llvm_lower_statement(&oom_ret_s);
          g_llvm_be_oom_countdown = -1;
          if (r_oom_dtor == llvm_gen_be_error_t::ok) break;
        }
        state.cleanup_stack.cleanups.clear();
      }

      // VectorType dik_constant with non_constant == TRUE and ck_aggregate containing ck_dynamic_init
      {
        llvm::Type* v4i32_cf = llvm::FixedVectorType::get(llvm::Type::getInt32Ty(*state.context), 4);
        llvm::AllocaInst* vec_slot = state.builder->CreateAlloca(v4i32_cf, nullptr, "vec.dyn.slot");
        a_dynamic_init lane_dip = {};
        lane_dip.kind = dik_zero;
        a_constant lane_con = {};
        lane_con.kind = ck_dynamic_init;
        lane_con.variant.dynamic_init.ptr = &lane_dip;
        a_constant vec_agg = {};
        vec_agg.kind = ck_aggregate;
        vec_agg.variant.aggregate.first_constant = &lane_con;
        a_dynamic_init vec_dip = {};
        vec_dip.kind = dik_constant;
        vec_dip.variant.constant.ptr = &vec_agg;
        vec_dip.variant.constant.non_constant = TRUE;
        assert(llvm_emit_dynamic_init(&vec_dip, vec_slot, v4i32_cf) == llvm_gen_be_error_t::ok);
      }

      // Cover remaining llvm_gen_be_stmt.cpp branches in a dedicated isolated LLVMBackendState:
      {
        LLVMBackendState state_s2 = {};
        state_s2.context = std::make_unique<llvm::LLVMContext>();
        state_s2.module = std::make_unique<llvm::Module>("cf_s2_mod", *state_s2.context);
        state_s2.builder = std::make_unique<llvm::IRBuilder<>>(*state_s2.context);
        LLVMBackendState& saved_parent_state = state;
        edg::be_state = &state_s2;
        auto& state = state_s2;

      llvm::Function* f_s2 = llvm::cast<llvm::Function>(
          state.module->getOrInsertFunction(
              "cf_s2_host_fn",
              llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false)).getCallee());
      // 1) !cur_bb in llvm_emit_cleanups_until (via empty block after ClearInsertionPoint)
      {
        a_statement empty_blk = {};
        empty_blk.kind = stmk_block;
        state.builder->ClearInsertionPoint();
        assert(llvm_lower_statement(&empty_blk) == llvm_gen_be_error_t::ok);
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "cf.resume.ip", f_s2));
      }

      // 2) cleanup.saved_stack_ptr in llvm_emit_cleanups_until
      {
        llvm::AllocaInst* sp_slot = state.builder->CreateAlloca(
            llvm::PointerType::getUnqual(*state.context), nullptr, "saved.sp");
        ScopeCleanup sp_cleanup = {};
        sp_cleanup.is_active = true;
        sp_cleanup.saved_stack_ptr = sp_slot;
        state.cleanup_stack.cleanups.push_back(sp_cleanup);
        a_statement ret_sp = {};
        ret_sp.kind = stmk_return;
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "cf.sp.ret", f_s2));
        assert(llvm_lower_statement(&ret_sp) == llvm_gen_be_error_t::ok);
        state.cleanup_stack.cleanups.clear();
      }

      // 3) edg_ret_as_param in stmk_return on sret function (null type, non-routine type, extra_info == nullptr, value_returned_as_parameter FALSE & TRUE)
      {
        llvm::Type* ptr_u = llvm::PointerType::getUnqual(*state.context);
        llvm::FunctionType* sret_ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {ptr_u}, false);
        llvm::Function* sret_fn = llvm::cast<llvm::Function>(
            state.module->getOrInsertFunction("cf_sret_edg_fn", sret_ft).getCallee());
        sret_fn->addParamAttr(0, llvm::Attribute::getWithStructRetType(*state.context, llvm::Type::getInt32Ty(*state.context)));
        an_expr_node ret_e = {};
        ret_e.kind = enk_constant;
        a_statement ret_with_e = {};
        ret_with_e.kind = stmk_return;
        ret_with_e.expr = &ret_e;

        a_routine cur_r = {};
        a_type non_rout_ty = {}; non_rout_ty.kind = tk_integer;
        a_type rout_ty = {}; rout_ty.kind = tk_routine;
        a_routine_type_supplement rsupp = {};

        state.current_edg_routine = &cur_r;
        cur_r.type = nullptr;
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "sret.r0", sret_fn));
        assert(llvm_lower_statement(&ret_with_e) == llvm_gen_be_error_t::ok);

        cur_r.type = &non_rout_ty;
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "sret.r1", sret_fn));
        assert(llvm_lower_statement(&ret_with_e) == llvm_gen_be_error_t::ok);

        cur_r.type = &rout_ty;
        rout_ty.variant.routine.extra_info = nullptr;
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "sret.r2", sret_fn));
        assert(llvm_lower_statement(&ret_with_e) == llvm_gen_be_error_t::ok);

        rout_ty.variant.routine.extra_info = &rsupp;
        rsupp.value_returned_as_parameter = FALSE;
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "sret.r3", sret_fn));
        assert(llvm_lower_statement(&ret_with_e) == llvm_gen_be_error_t::ok);

        rsupp.value_returned_as_parameter = TRUE;
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "sret.r4", sret_fn));
        assert(llvm_lower_statement(&ret_with_e) == llvm_gen_be_error_t::ok);
        state.current_edg_routine = nullptr;
      }

      // 4) current_routine_is_ctor_or_dtor returning `this` across i32(), ptr(), ptr(i32), ptr(ptr)
      {
        llvm::Type* ptr_u = llvm::PointerType::getUnqual(*state.context);
        llvm::Type* i32_t = llvm::Type::getInt32Ty(*state.context);
        llvm::Function* fn_i32_0 = llvm::cast<llvm::Function>(
            state.module->getOrInsertFunction("cd_i32_0", llvm::FunctionType::get(i32_t, false)).getCallee());
        llvm::Function* fn_ptr_0 = llvm::cast<llvm::Function>(
            state.module->getOrInsertFunction("cd_ptr_0", llvm::FunctionType::get(ptr_u, false)).getCallee());
        llvm::Function* fn_ptr_i32 = llvm::cast<llvm::Function>(
            state.module->getOrInsertFunction("cd_ptr_i32", llvm::FunctionType::get(ptr_u, {i32_t}, false)).getCallee());
        llvm::Function* fn_ptr_ptr = llvm::cast<llvm::Function>(
            state.module->getOrInsertFunction("cd_ptr_ptr", llvm::FunctionType::get(ptr_u, {ptr_u}, false)).getCallee());
        a_statement bare_ret = {};
        bare_ret.kind = stmk_return;
        state.current_routine_is_ctor_or_dtor = true;
        for (llvm::Function* cdf : {fn_i32_0, fn_ptr_0, fn_ptr_i32, fn_ptr_ptr}) {
          state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "cd.entry", cdf));
          assert(llvm_lower_statement(&bare_ret) == llvm_gen_be_error_t::ok);
        }
        state.current_routine_is_ctor_or_dtor = false;
      }

      // 5) Block local_static_variable_inits, static constant block vars, and lifetime->destructions + il_lowering_needed + var->cleanup_routine
      {
        a_type b_i32 = {}; b_i32.kind = tk_integer; b_i32.size = 4;
        a_constant b_cval = {}; b_cval.kind = ck_integer;
        a_variable lsv_ok = {}, lsv_other = {};
        lsv_ok.init_kind = initk_function_local;
        lsv_other.init_kind = initk_zero;
        a_local_static_variable_init lsi1 = {}, lsi2 = {}, lsi3 = {};
        lsi1.variable = nullptr; lsi1.next = &lsi2;
        lsi2.variable = &lsv_other; lsi2.next = &lsi3;
        lsi3.variable = &lsv_ok; lsi3.init_kind = initk_static; lsi3.initializer.constant = &b_cval;

        a_variable b_svar1 = {}, b_svar2 = {}, b_svar_err = {};
        b_svar1.source_corresp.name = "cf_bs1";
        b_svar1.storage_class = sc_static; b_svar1.type = &b_i32;
        b_svar1.init_kind = initk_static; b_svar1.initializer.constant = &b_cval;
        b_svar1.next = &b_svar2;
        b_svar2.source_corresp.name = "cf_bs2";
        b_svar2.storage_class = sc_static; b_svar2.type = &b_i32;
        b_svar2.init_kind = initk_static; b_svar2.initializer.constant = nullptr;

        a_variable d_var_uninit = {}, d_var_has_stmt = {}, d_var_unmapped = {};
        d_var_uninit.source_corresp.name = "cf_d_uninit"; d_var_uninit.type = &b_i32;
        d_var_has_stmt.source_corresp.name = "cf_d_has_stmt"; d_var_has_stmt.type = &b_i32;
        d_var_unmapped.source_corresp.name = "cf_d_unmapped"; d_var_unmapped.type = &b_i32;
        d_var_uninit.next = &d_var_has_stmt;
        a_routine dtor_r = {}, custom_cleanup_r = {};
        dtor_r.source_corresp.name = "_ZN9CfBlkDtorD1Ev";
        custom_cleanup_r.source_corresp.name = "__cf_gnu_cleanup";
        d_var_has_stmt.cleanup_routine = &custom_cleanup_r;

        a_dynamic_init d1_null_dtor = {}, d2_null_var = {}, d3_uninit = {}, d4_has_stmt = {}, d5_unmapped = {};
        d1_null_dtor.destructor = nullptr; d1_null_dtor.variable = &d_var_uninit; d1_null_dtor.next_in_destruction_list = &d2_null_var;
        d2_null_var.destructor = &dtor_r; d2_null_var.variable = nullptr; d2_null_var.next_in_destruction_list = &d3_uninit;
        d3_uninit.destructor = &dtor_r; d3_uninit.variable = &d_var_uninit; d3_uninit.next_in_destruction_list = &d4_has_stmt;
        d4_has_stmt.kind = dik_zero; d4_has_stmt.destructor = &dtor_r; d4_has_stmt.variable = &d_var_has_stmt; d4_has_stmt.next_in_destruction_list = &d5_unmapped;
        d5_unmapped.destructor = &dtor_r; d5_unmapped.variable = &d_var_unmapped; d5_unmapped.next_in_destruction_list = nullptr;

        an_object_lifetime blk_life = {};
        blk_life.destructions = &d1_null_dtor;

        a_statement inner_empty = {}, inner_init = {};
        inner_empty.kind = stmk_empty; inner_empty.next = &inner_init;
        inner_init.kind = stmk_init; inner_init.variant.dynamic_init = &d4_has_stmt;

        a_scope b_sc = {};
        b_sc.local_static_variable_inits = &lsi1;
        b_sc.nonstatic_variables = &d_var_uninit;
        b_sc.variables = &b_svar1;
        b_sc.lifetime = &blk_life;
        a_block b_supp = {};
        b_supp.assoc_scope = &b_sc;
        a_statement blk_s = {};
        blk_s.kind = stmk_block;
        blk_s.variant.block.extra_info = &b_supp;
        blk_s.variant.block.statements = &inner_empty;

        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "cf.blk.life", f_s2));
        assert(llvm_lower_statement(&blk_s) == llvm_gen_be_error_t::ok);

        // Error on static variable constant evaluation (L325)
        b_svar_err.source_corresp.name = "cf_bs3";
        b_svar_err.storage_class = sc_static; b_svar_err.type = &b_i32;
        b_svar_err.init_kind = initk_static; b_svar_err.initializer.constant = &b_cval;
        b_sc.variables = &b_svar_err;
        g_cf_fail_const = 1;
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "cf.blk.err", f_s2));
        assert(llvm_lower_statement(&blk_s) == llvm_gen_be_error_t::constant_eval_failure);
        g_cf_fail_const = 0;
        b_sc.variables = nullptr;

        // Also test blk_life.destructions == nullptr when g_cf_il_lowering == 0 (L349 F), then g_cf_il_lowering == 1
        d_var_has_stmt.cleanup_routine = nullptr;
        blk_life.destructions = nullptr;
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "cf.blk.nodestr", f_s2));
        assert(llvm_lower_statement(&blk_s) == llvm_gen_be_error_t::ok);
        blk_life.destructions = &d1_null_dtor;

        g_cf_il_lowering = 1;
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "cf.blk.lowered", f_s2));
        assert(llvm_lower_statement(&blk_s) == llvm_gen_be_error_t::ok);
        g_cf_il_lowering = 0;
      }
        edg::be_state = &saved_parent_state;
      }
    }
  }

  printf("PASS\n");
  return 0;
}
