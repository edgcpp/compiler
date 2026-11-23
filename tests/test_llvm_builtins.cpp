/**
 * @file test_llvm_builtins.cpp
 * @ingroup llvm_gen_be_tests
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
  /**
   * @brief Test stub or helper for `is_bool_type`.
   * @details Provides mock or helper behavior for `is_bool_type` within `tests/test_llvm_builtins.cpp`.
   * @param[in] ty Parameter `ty` for `is_bool_type`.
   * @return Result of `is_bool_type` (a_boolean).
   */
  a_boolean is_bool_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `is_class_struct_union_type`.
   * @details Provides mock or helper behavior for `is_class_struct_union_type` within `tests/test_llvm_builtins.cpp`.
   * @param[in] ty Parameter `ty` for `is_class_struct_union_type`.
   * @return Result of `is_class_struct_union_type` (a_boolean).
   */
  a_boolean is_class_struct_union_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `f_alignment_of_type`.
   * @details Provides mock or helper behavior for `f_alignment_of_type` within `tests/test_llvm_builtins.cpp`.
   * @param[in] ty Parameter `ty` for `f_alignment_of_type`.
   * @return Result of `f_alignment_of_type` (a_targ_alignment).
   */
  a_targ_alignment f_alignment_of_type(a_type_ptr ty) { return 1; }
  /**
   * @brief Test stub or helper for `is_trivially_copyable_type`.
   * @details Provides mock or helper behavior for `is_trivially_copyable_type` within `tests/test_llvm_builtins.cpp`.
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
   * @details Provides mock or helper behavior for `mangled_typeinfo_name` within `tests/test_llvm_builtins.cpp`.
   * @param[in] type Parameter `type` for `mangled_typeinfo_name`.
   * @return Result of `mangled_typeinfo_name` (char*).
   */
  char* mangled_typeinfo_name(a_type_ptr type) { return (char*)"_ZTIi"; }
  /**
   * @brief Test stub or helper for `conv_seq_to_file_and_line`.
   * @details Provides mock source file and line resolution within `tests/test_llvm_builtins.cpp`.
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
    if (file_name) *file_name = (seq_number > 100) ? "header.hpp" : nullptr;
    if (full_name) *full_name = (seq_number > 100) ? "include/header.hpp" : nullptr;
    if (line_number) *line_number = (seq_number > 100) ? (seq_number - 100) : seq_number;
    if (at_end_of_source) *at_end_of_source = FALSE;
    return nullptr;
  }
  /**
   * @brief Test stub or helper for `f_get_type_qualifiers`.
   * @details Provides mock or helper behavior for `f_get_type_qualifiers` within `tests/test_llvm_builtins.cpp`.
   * @param[in] type Parameter `type` for `f_get_type_qualifiers`.
   * @param[in] x Parameter `x` for `f_get_type_qualifiers`.
   * @return Result of `f_get_type_qualifiers` (a_type_qualifier_set).
   */
  a_type_qualifier_set f_get_type_qualifiers(a_type_ptr type, a_boolean x) {
    (void)x;
    return (type && type->kind == tk_typeref) ? type->variant.typeref.qualifiers : 0;
  }
  /**
   * @brief Test stub or helper for `is_reference_type`.
   * @details Provides mock or helper behavior for `is_reference_type` within `tests/test_llvm_builtins.cpp`.
   * @param[in] ty Parameter `ty` for `is_reference_type`.
   * @return Result of `is_reference_type` (a_boolean).
   */
  a_boolean is_reference_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `is_empty_class_type`.
   * @details Provides mock or helper behavior for `is_empty_class_type` within `tests/test_llvm_builtins.cpp`.
   * @param[in] ty Parameter `ty` for `is_empty_class_type`.
   * @return Result of `is_empty_class_type` (a_boolean).
   */
  a_boolean is_empty_class_type(a_type_ptr ty) { return ty && ty->variant.class_struct_union.is_empty_class; }
  /**
   * @brief Test stub for `llvm_lower_global_variable`.
   * @details Creates a placeholder external `GlobalVariable` when `var` is named so RTTI lookups succeed in unit tests.
   * @param[in] var Frontend variable pointer.
   * @return `llvm_gen_be_error_t::ok` on success.
   */
  llvm_gen_be_error_t llvm_lower_global_variable(a_variable_ptr var) noexcept {
    if (var && var->source_corresp.name && be_state && be_state->module &&
        !be_state->module->getNamedGlobal(var->source_corresp.name)) {
      llvm::GlobalVariable* gv = nullptr;
      LLVM_GEN_BE_CHECK(llvm_create_global_variable(
          be_state->module.get(),
          llvm::PointerType::getUnqual(*be_state->context),
          true,
          llvm::GlobalValue::ExternalLinkage,
          nullptr,
          var->source_corresp.name,
          &gv));
    }
    return llvm_gen_be_error_t::ok;
  }
  /**
   * @brief Test stub or helper for `evaluate_constant`.
   * @details Provides mock or helper behavior for `evaluate_constant` within `tests/test_llvm_builtins.cpp`.
   * @param[in] con Parameter `con` for `evaluate_constant`.
   * @param[in] ty Parameter `ty` for `evaluate_constant`.
   * @param[out] out_const Parameter `out_const` for `evaluate_constant`.
   * @return Result of `evaluate_constant` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t evaluate_constant(a_constant_ptr con, llvm::Type* ty, llvm::Constant** out_const) noexcept { 
    if (con && con->source_corresp.decl_position.seq == 888) return llvm_gen_be_error_t::unsupported_expr;
    if (ty->isIntegerTy()) {
      uint64_t v = (con && con->source_corresp.decl_position.seq == 10) ? 0 : 1;
      *out_const = llvm::ConstantInt::get(ty, v);
    }
    else if (ty->isFloatingPointTy()) *out_const = llvm::ConstantFP::get(ty, 1.0);
    else *out_const = llvm::Constant::getNullValue(ty);
    return llvm_gen_be_error_t::ok; 
  }
  a_byte_boolean int_kind_is_signed[ik_last] = {0};
  
  /**
   * @brief Test stub or helper for `llvm_lower_statement`.
   * @details Provides mock or helper behavior for `llvm_lower_statement` within `tests/test_llvm_builtins.cpp`.
   * @param[in] stmt Parameter `stmt` for `llvm_lower_statement`.
   * @return Result of `llvm_lower_statement` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_statement(a_statement_ptr stmt) noexcept {
    return llvm_gen_be_error_t::ok;
  }
  /**
   * @brief Test stub or helper for `llvm_emit_dynamic_init`.
   * @details Provides mock or helper behavior for `llvm_emit_dynamic_init` within `tests/test_llvm_builtins.cpp`.
   * @param[in] dip Parameter `dip` for `llvm_emit_dynamic_init`.
   * @param[in] ptr Parameter `ptr` for `llvm_emit_dynamic_init`.
   * @param[in] obj_type Parameter `obj_type` for `llvm_emit_dynamic_init`.
   * @return Result of `llvm_emit_dynamic_init` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_emit_dynamic_init(a_dynamic_init_ptr dip, llvm::Value* ptr, llvm::Type* obj_type) noexcept {
    return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub or helper for `get_di_location`.
   * @details Provides mock or helper behavior for `get_di_location` within `tests/test_llvm_builtins.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `get_di_location`.
   * @param[in] arg_1 Parameter `arg_1` for `get_di_location`.
   * @param[in] arg_2 Parameter `arg_2` for `get_di_location`.
   * @param[out] out_loc Parameter `out_loc` for `get_di_location`.
   * @return Result of `get_di_location` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_di_location(llvm_gen_be_debug_state_t* arg_0, a_source_position arg_1, llvm::DIScope* arg_2, llvm::DILocation** out_loc) noexcept {
    if (out_loc) *out_loc = nullptr;
    return llvm_gen_be_error_t::ok;
  }
  /**
   * @brief Test stub or helper for `emit_dbg_value_for_variable`.
   * @details Provides mock or helper behavior for `emit_dbg_value_for_variable` within `tests/test_llvm_builtins.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `emit_dbg_value_for_variable`.
   * @param[in] arg_1 Parameter `arg_1` for `emit_dbg_value_for_variable`.
   * @param[in] arg_2 Parameter `arg_2` for `emit_dbg_value_for_variable`.
   * @param[in] arg_3 Parameter `arg_3` for `emit_dbg_value_for_variable`.
   * @param[in] arg_4 Parameter `arg_4` for `emit_dbg_value_for_variable`.
   * @param[in] arg_5 Parameter `arg_5` for `emit_dbg_value_for_variable`.
   * @param[in] arg_6 Parameter `arg_6` for `emit_dbg_value_for_variable`.
   * @return Result of `emit_dbg_value_for_variable` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t emit_dbg_value_for_variable(llvm_gen_be_debug_state_t* arg_0, a_variable* arg_1, llvm::Value* arg_2, llvm::Instruction* arg_3, llvm::BasicBlock* arg_4, const uint64_t* arg_5, size_t arg_6) noexcept {
    return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `llvm_emit_eh_cleanup`.
   * @details Provides mock or helper behavior for `llvm_emit_eh_cleanup` within `tests/test_llvm_builtins.cpp`.
   * @param[in] dtor Parameter `dtor` for `llvm_emit_eh_cleanup`.
   * @param[in] obj_ptr Parameter `obj_ptr` for `llvm_emit_eh_cleanup`.
   * @return Result of `llvm_emit_eh_cleanup` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_emit_eh_cleanup(a_routine_ptr dtor, llvm::Value* obj_ptr) noexcept { return llvm_gen_be_error_t::ok; }
  /**
   * @brief Test stub for `llvm_emit_routine_exc_spec_handler` in `tests/test_llvm_builtins.cpp`.
   * @details Sets `*out_handled = false` in builtin unit tests.
   * @param[in] routine Optional EDG routine pointer.
   * @param[in,out] lpad LandingPadInst pointer.
   * @param[out] out_handled Receives `false`.
   * @return llvm_gen_be_error_t::ok.
   */
  llvm_gen_be_error_t llvm_emit_routine_exc_spec_handler(
      a_routine_ptr routine,
      llvm::LandingPadInst* lpad,
      bool* out_handled) noexcept {
    (void)routine;
    (void)lpad;
    if (out_handled) *out_handled = false;
    return llvm_gen_be_error_t::ok;
  }
  /**
   * @brief Test stub for `llvm_append_enclosing_try_clauses` in `tests/test_llvm_builtins.cpp`.
   * @details No-op stub for standalone builtin unit tests.
   * @param[in,out] lpad LandingPadInst pointer.
   * @return llvm_gen_be_error_t::ok.
   */
  llvm_gen_be_error_t llvm_append_enclosing_try_clauses(llvm::LandingPadInst* lpad) noexcept {
    (void)lpad;
    return llvm_gen_be_error_t::ok;
  }
  /**
   * @brief Test stub for `llvm_get_or_create_current_unwind_dest` in `tests/test_llvm_builtins.cpp`.
   * @details Returns `nullptr` for unwind destination in builtin unit tests.
   * @param[out] out_unwind_bb Receives `nullptr`.
   * @return llvm_gen_be_error_t::ok.
   */
  llvm_gen_be_error_t llvm_get_or_create_current_unwind_dest(llvm::BasicBlock** out_unwind_bb) noexcept {
    if (out_unwind_bb) *out_unwind_bb = nullptr;
    return llvm_gen_be_error_t::ok;
  }
  /**
   * @brief Test stub for `llvm_get_or_create_msvc_throw_info` in `tests/test_llvm_builtins.cpp`.
   * @details Sets `*out_throw_info` to a null pointer constant.
   * @param[in] thrown_type The EDG type of the thrown exception object.
   * @param[in] dtor_fn Optional destructor `llvm::Constant*`.
   * @param[out] out_throw_info Receives the `_TI...` `llvm::Constant*` global pointer.
   * @return llvm_gen_be_error_t::ok on success.
   */
  llvm_gen_be_error_t llvm_get_or_create_msvc_throw_info(
      a_type_ptr thrown_type,
      llvm::Constant* dtor_fn,
      llvm::Constant** out_throw_info) noexcept {
    (void)thrown_type;
    (void)dtor_fn;
    if (out_throw_info) {
      *out_throw_info = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
    }
    return llvm_gen_be_error_t::ok;
  }
  /**
   * @brief Test stub or helper for `get_mangled_function_name`.
   * @details Provides mock or helper behavior for `get_mangled_function_name` within `tests/test_llvm_builtins.cpp`.
   * @param[in] routine Parameter `routine` for `get_mangled_function_name`.
   * @return Result of `get_mangled_function_name` (char*).
   */
  char* get_mangled_function_name(a_routine_ptr routine) { return nullptr; }
  /**
   * @brief Test stub or helper for `mangled_vtbl_name`.
   * @details Provides mock or helper behavior for `mangled_vtbl_name` within `tests/test_llvm_builtins.cpp`.
   * @param[in] class_type Parameter `class_type` for `mangled_vtbl_name`.
   * @param[in] bcp Parameter `bcp` for `mangled_vtbl_name`.
   * @param[in] ctor_bcp Parameter `ctor_bcp` for `mangled_vtbl_name`.
   * @return Result of `mangled_vtbl_name` (char*).
   */
  char* mangled_vtbl_name(a_type_ptr class_type, a_base_class_ptr bcp, a_base_class_ptr ctor_bcp) {
    return (char*)(bcp ? "_ZTCmock" : "_ZTVmock");
  }
  /**
   * @brief Test stub or helper for `mangled_virtual_table_table_name`.
   * @details Provides mock or helper behavior for `mangled_virtual_table_table_name` within `tests/test_llvm_builtins.cpp`.
   * @param[in] type Parameter `type` for `mangled_virtual_table_table_name`.
   * @return Result of `mangled_virtual_table_table_name` (char*).
   */
  char* mangled_virtual_table_table_name(a_type_ptr type) { return (char*)"_ZTTmock"; }
  /**
   * @brief Test stub or helper for `type_has_nontrivial_destructor`.
   * @details Provides mock or helper behavior for `type_has_nontrivial_destructor` within `tests/test_llvm_builtins.cpp`.
   * @param[in] type Parameter `type` for `type_has_nontrivial_destructor`.
   * @return Result of `type_has_nontrivial_destructor` (a_boolean).
   */
  a_boolean type_has_nontrivial_destructor(a_type_ptr type) { return (type && type->size == 777) ? TRUE : FALSE; }
  /**
   * @brief Test stub or helper for `vtbl_decider_function_for_class`.
   * @details Provides mock or helper behavior for `vtbl_decider_function_for_class` within `tests/test_llvm_builtins.cpp`.
   * @param[in] class_type Parameter `class_type` for `vtbl_decider_function_for_class`.
   * @param[in] unknown Parameter `unknown` for `vtbl_decider_function_for_class`.
   * @return Result of `vtbl_decider_function_for_class` (a_routine_ptr).
   */
  a_routine_ptr vtbl_decider_function_for_class(a_type_ptr class_type, a_boolean* unknown) { 
      if (unknown) *unknown = FALSE;
      return nullptr; 
  }
}

/**
 * @brief Entry point for the compiler builtins unit test suite.
 * @details Exercises `offsetof`, `__builtin_expect`, bit manipulation, math, and overflow builtins.
 * @return 0 on success.
 */
int main(void) {
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

  printf("Test offsetof\n");
  a_type int_ty = {}; int_ty.kind = tk_integer;
  an_expr_node offsetof_expr = {};
  offsetof_expr.kind = enk_builtin_operation;
  offsetof_expr.type = &int_ty;
  offsetof_expr.variant.builtin_operation.kind = bok_offsetof;
  an_expr_node op_ty = {};
  offsetof_expr.variant.builtin_operation.operands = &op_ty;
  llvm::Value* val = nullptr;
  assert(llvm_lower_expression(&offsetof_expr, &val) == llvm_gen_be_error_t::ok);

  printf("Test __builtin_expect\n");
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
  op3.type = &int_ty;
  op3.kind = enk_constant;
  op2.next = &op3;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);

  a_type ptr_ty = {}; ptr_ty.kind = tk_pointer; ptr_ty.variant.pointer.type = &int_ty;
  a_variable dummy_ptr_var;
  memset(&dummy_ptr_var, 0, sizeof(dummy_ptr_var));
  dummy_ptr_var.type = &ptr_ty;
  state.local_vars[&dummy_ptr_var] = state.builder->CreateAlloca(llvm::Type::getInt8Ty(*state.context));

  an_expr_node ptr_op = {};
  ptr_op.kind = enk_variable;
  ptr_op.type = &ptr_ty;
  ptr_op.variant.variable.ptr = &dummy_ptr_var;

  an_expr_node ptr_op2 = {};
  ptr_op2.kind = enk_variable;
  ptr_op2.type = &ptr_ty;
  ptr_op2.variant.variable.ptr = &dummy_ptr_var;

  const char* builtin_names[] = {
      "__builtin_expect", "__builtin_unreachable", "__builtin_trap",
      "__builtin_clz", "__builtin_ctz", "__builtin_popcount", "__builtin_bswap32",
      "__builtin___emutls_get_address",
      "__builtin_memcpy", "__builtin_memmove", "__builtin_memset",
      "__builtin_assume", "__atomic_thread_fence", "__atomic_fetch_add",
      "__atomic_compare_exchange", "__atomic_load", "__atomic_store", "__atomic_exchange",
      "__custom_non_builtin_void_call"
  };

  an_expr_node ptr_op3 = ptr_op;
  an_expr_node op4 = op3;
  an_expr_node op5 = op3;
  an_expr_node op6 = op3;

  for (const char* name : builtin_names) {
      printf("Testing %s\n", name);
      r.source_corresp.name = name;
      op1.next = &ptr_op;
      ptr_op.next = &ptr_op2;
      ptr_op2.next = &ptr_op3;
      ptr_op3.next = &op4;
      op4.next = &op5;
      op5.next = &op6;
      op6.next = nullptr;
      llvm_gen_be_error_t b_err = llvm_lower_expression(&call_expr, &val);
      assert(b_err == llvm_gen_be_error_t::ok);
  }

  const char* simd_int_builtin_names[] = {
      "__builtin_ia32_paddd128", "__builtin_neon_vadd_v", "__builtin_sve_svadd_m",
      "__builtin_rvv_vadd_vv", "__builtin_rvv_vsetvli"
  };
  for (const char* name : simd_int_builtin_names) {
      r.source_corresp.name = name;
      op1.next = &op4;
      op4.next = &op5;
      op5.next = &op6;
      op6.next = nullptr;
      assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);
  }

  // Test bok_builtin_has_attribute
  an_attribute var_attr = {};
  var_attr.kind = ak_align;
  dummy_ptr_var.source_corresp.attributes = &var_attr;

  an_attribute query_attr = {};
  query_attr.kind = ak_align;
  a_constant attr_const = {};
  attr_const.source_corresp.attributes = &query_attr;
  an_expr_node attr_arg = {};
  attr_arg.kind = enk_constant;
  attr_arg.variant.constant.ptr = &attr_const;

  an_expr_node has_attr_expr = {};
  has_attr_expr.kind = enk_builtin_operation;
  has_attr_expr.type = &int_ty;
  has_attr_expr.variant.builtin_operation.kind = bok_builtin_has_attribute;
  ptr_op.next = &attr_arg;
  attr_arg.next = nullptr;
  has_attr_expr.variant.builtin_operation.operands = &ptr_op;

  llvm::Value* has_attr_val = nullptr;
  assert(llvm_lower_expression(&has_attr_expr, &has_attr_val) == llvm_gen_be_error_t::ok);
  auto* has_attr_ci = llvm::dyn_cast_or_null<llvm::ConstantInt>(has_attr_val);
  assert(has_attr_ci != nullptr && has_attr_ci->getZExtValue() == 1);

  query_attr.kind = ak_packed;
  assert(llvm_lower_expression(&has_attr_expr, &has_attr_val) == llvm_gen_be_error_t::ok);
  has_attr_ci = llvm::dyn_cast_or_null<llvm::ConstantInt>(has_attr_val);
  assert(has_attr_ci != nullptr && has_attr_ci->getZExtValue() == 0);

  // Phase 2 Tests: llvm_lower_builtin_operation & new call-based builtins
  assert(llvm_lower_builtin_operation(nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_builtin_operation(&has_attr_expr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  an_expr_node empty_bok = {};
  empty_bok.kind = enk_builtin_operation;
  empty_bok.variant.builtin_operation.operands = nullptr;
  assert(llvm_lower_builtin_operation(&empty_bok, &val) == llvm_gen_be_error_t::invalid_argument);

  // Call-based builtins: __builtin_assume_aligned (2 and 3 args), __builtin_choose_expr (true, false, non-const),
  // __builtin_va_arg_pack, __builtin_va_arg_pack_len, __builtin_is_constant_evaluated (int and bool),
  // __builtin_nondeterministic_value, __builtin_addressof, __builtin_complex, __builtin_source_location,
  // __builtin_classify_type, __uuidof
  a_type bool_ty = {}; bool_ty.kind = tk_integer; bool_ty.size = 1; bool_ty.variant.integer.bool_type = TRUE;
  a_type void_ty = {}; void_ty.kind = tk_void;
  a_type char_ty = {}; char_ty.kind = tk_integer; char_ty.size = 1; char_ty.variant.integer.int_kind = ik_char;
  a_type schar_ty = {}; schar_ty.kind = tk_integer; schar_ty.size = 1; schar_ty.variant.integer.int_kind = ik_signed_char;
  a_type uchar_ty = {}; uchar_ty.kind = tk_integer; uchar_ty.size = 1; uchar_ty.variant.integer.int_kind = ik_unsigned_char;
  a_type enum_ty = {}; enum_ty.kind = tk_integer; enum_ty.size = 4; enum_ty.variant.integer.enum_type = TRUE;
  a_type ref_ty = {}; ref_ty.kind = tk_pointer; ref_ty.size = 8; ref_ty.variant.pointer.is_reference = TRUE; ref_ty.variant.pointer.type = &int_ty;
  a_type rout_ty = {}; rout_ty.kind = tk_routine;
  a_type ptm_data_ty = {}; ptm_data_ty.kind = tk_ptr_to_member; ptm_data_ty.size = 8; ptm_data_ty.variant.ptr_to_member.type = &int_ty;
  a_type ptm_fn_ty = {}; ptm_fn_ty.kind = tk_ptr_to_member; ptm_fn_ty.size = 16; ptm_fn_ty.variant.ptr_to_member.type = &rout_ty;
  a_type ptm_null_ty = {}; ptm_null_ty.kind = tk_ptr_to_member; ptm_null_ty.size = 8; ptm_null_ty.variant.ptr_to_member.type = nullptr;
  a_type flt_ty = {}; flt_ty.kind = tk_float; flt_ty.size = 4; flt_ty.variant.float_kind = fk_float;
  a_type imag_ty = {}; imag_ty.kind = tk_imaginary; imag_ty.size = 4; imag_ty.variant.float_kind = fk_float;
  a_type cplx_ty = {}; cplx_ty.kind = tk_complex; cplx_ty.size = 8; cplx_ty.variant.float_kind = fk_float;
  a_type struct_ty = {}; struct_ty.kind = tk_struct; struct_ty.size = 4;
  a_type class_ty = {}; class_ty.kind = tk_class; class_ty.size = 4;
  a_type union_ty = {}; union_ty.kind = tk_union; union_ty.size = 4;
  a_type arr_ty = {}; arr_ty.kind = tk_array; arr_ty.size = 8; arr_ty.variant.array.element_type = &int_ty; arr_ty.variant.array.variant.number_of_elements = 2;
  a_type vec_ty = {}; vec_ty.kind = tk_integer; vec_ty.size = 16;
  llvm::Type* llvm_v4i32 = llvm::FixedVectorType::get(llvm::Type::getInt32Ty(*state.context), 4);
  state.type_cache[&vec_ty] = llvm_v4i32;
  a_type unk_ty = {}; unk_ty.kind = tk_unknown;

  r.source_corresp.name = "__builtin_assume_aligned";
  op1.next = &ptr_op; ptr_op.next = &op3; op3.next = nullptr;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);
  op3.next = &op4; op4.next = nullptr;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);

  r.source_corresp.name = "__builtin_choose_expr";
  // non-zero ConstantInt, zero ConstantInt, non-ConstantInt
  op3.variant.constant.ptr = &attr_const;
  a_constant zero_c = {}; zero_c.source_corresp.decl_position.seq = 10;
  an_expr_node zero_op = {}; zero_op.kind = enk_constant; zero_op.type = &int_ty; zero_op.variant.constant.ptr = &zero_c;
  op1.next = &op3; op3.next = &op4; op4.next = &op5; op5.next = nullptr;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);
  op1.next = &zero_op; zero_op.next = &op4; op4.next = &op5; op5.next = nullptr;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);
  op1.next = &ptr_op; ptr_op.next = &op4; op4.next = &op5; op5.next = nullptr;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);

  r.source_corresp.name = "__builtin_va_arg_pack";
  call_expr.type = &int_ty;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);
  r.source_corresp.name = "__builtin_va_arg_pack_len";
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);
  r.source_corresp.name = "__builtin_is_constant_evaluated";
  call_expr.type = &bool_ty;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);
  call_expr.type = nullptr;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);
  call_expr.type = &int_ty;

  r.source_corresp.name = "__builtin_nondeterministic_value";
  op1.next = &op3; op3.next = nullptr;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);

  r.source_corresp.name = "__builtin_addressof";
  op1.next = &ptr_op; ptr_op.next = nullptr;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);

  r.source_corresp.name = "__builtin_complex";
  op1.next = &op3; op3.next = &op4; op4.next = nullptr;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);

  r.source_corresp.name = "__builtin_source_location";
  op1.next = nullptr;
  call_expr.position.seq = 0; call_expr.position.column = 0;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);
  call_expr.position.seq = 42; call_expr.position.column = 7;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);

  r.source_corresp.name = "__builtin_FILE";
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);
  r.source_corresp.name = "__builtin_FUNCTION";
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);
  r.source_corresp.name = "__builtin_LINE";
  call_expr.position.seq = 0;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);
  call_expr.position.seq = 12;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);
  r.source_corresp.name = "__builtin_COLUMN";
  call_expr.position.column = 0;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);
  call_expr.position.column = 5;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);

  r.source_corresp.name = "__builtin_clear_padding";
  op1.next = &ptr_op; ptr_op.next = nullptr;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);

  const char* stdbit_builtins[] = {
      "__builtin_stdc_count_ones",
      "__builtin_stdc_count_zeros",
      "__builtin_stdc_bit_width",
      "__builtin_stdc_has_single_bit",
      "__builtin_stdc_bit_floor",
      "__builtin_stdc_bit_ceil",
      "__builtin_stdc_first_leading_one",
      "__builtin_stdc_first_leading_zero",
      "__builtin_stdc_first_trailing_one",
      "__builtin_stdc_first_trailing_zero"
  };
  for (const char* sb : stdbit_builtins) {
    r.source_corresp.name = sb;
    op1.next = &op3; op3.next = nullptr;
    assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);
  }

  const char* two_arg_stdbit_and_ovf_builtins[] = {
      "__builtin_stdc_rotate_left",
      "__builtin_stdc_rotate_right",
      "__builtin_add_overflow_p",
      "__builtin_sub_overflow_p",
      "__builtin_mul_overflow_p"
  };
  for (const char* b2 : two_arg_stdbit_and_ovf_builtins) {
    r.source_corresp.name = b2;
    op1.next = &op3; op3.next = &op4; op4.next = nullptr;
    assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);
  }

  r.source_corresp.name = "__builtin_classify_type";
  op1.next = nullptr; // arg_node == nullptr
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);
  an_expr_node ty_op_node = {};
  ty_op_node.kind = enk_type_operand;
  op1.next = &ty_op_node;
  a_type ik_int_ty = {}; ik_int_ty.kind = tk_integer; ik_int_ty.size = 4; ik_int_ty.variant.integer.int_kind = ik_int;
  a_type_ptr cls_types[] = {
      nullptr, &void_ty, &bool_ty, &enum_ty, &char_ty, &schar_ty, &uchar_ty, &ik_int_ty,
      &ptr_ty, &ref_ty, &ptm_data_ty, &ptm_fn_ty, &ptm_null_ty, &flt_ty, &imag_ty,
      &cplx_ty, &rout_ty, &struct_ty, &class_ty, &union_ty, &arr_ty, &vec_ty, &unk_ty
  };
  for (a_type_ptr ct : cls_types) {
    ty_op_node.variant.type_operand.type = ct;
    assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);
  }
  op1.next = &op3; op3.next = nullptr; // non-enk_type_operand
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);

  r.source_corresp.name = "__uuidof";
  op1.next = nullptr;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);
  op1.next = &ty_op_node; ty_op_node.variant.type_operand.type = &class_ty;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);
  op1.next = &op3;
  assert(llvm_lower_expression(&call_expr, &val) == llvm_gen_be_error_t::ok);

  // bok_intaddr, bok_types_compatible, bok_builtin_complex, bok_builtin_addressof,
  // bok_builtin_shuffle, bok_builtin_shufflevector, bok_builtin_convertvector, and evaluate_type_trait_builtin
  an_expr_node bok_node = {};
  bok_node.kind = enk_builtin_operation;
  bok_node.type = &int_ty;
  bok_node.variant.builtin_operation.kind = bok_intaddr;
  ptr_op.next = nullptr;
  bok_node.variant.builtin_operation.operands = &ptr_op;
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
  bok_node.type = nullptr;
  bok_node.variant.builtin_operation.operands = &op3;
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
  bok_node.type = &int_ty;

  // bok_types_compatible
  bok_node.variant.builtin_operation.kind = bok_types_compatible;
  an_expr_node ty_op2 = {};
  ty_op2.kind = enk_type_operand;
  ty_op_node.variant.type_operand.type = &int_ty;
  ty_op2.variant.type_operand.type = &int_ty;
  ty_op_node.next = &ty_op2;
  bok_node.variant.builtin_operation.operands = &ty_op_node;
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
  ty_op2.variant.type_operand.type = &flt_ty; // same size (4), different kind
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
  a_type int_ty2 = int_ty; // different pointer, same kind and size!
  ty_op2.variant.type_operand.type = &int_ty2;
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
  a_type i64_edg_ty = int_ty; i64_edg_ty.size = 8; // same kind, different size!
  ty_op2.variant.type_operand.type = &i64_edg_ty;
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
  ty_op_node.variant.type_operand.type = nullptr;
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
  op3.next = nullptr; // op2 == nullptr, op1 is not enk_type_operand
  bok_node.variant.builtin_operation.operands = &op3;
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
  op3.next = &op4; op4.next = nullptr; // op2 is not enk_type_operand
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);

  // bok_builtin_complex & bok_builtin_addressof
  bok_node.variant.builtin_operation.kind = bok_builtin_complex;
  bok_node.variant.builtin_operation.operands = &op3;
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
  bok_node.variant.builtin_operation.kind = bok_builtin_addressof;
  bok_node.variant.builtin_operation.operands = &ptr_op;
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);

  // bok_builtin_shuffle & bok_builtin_shufflevector & bok_builtin_convertvector
  a_type fvec_ty = {}; fvec_ty.kind = tk_float; fvec_ty.size = 16;
  llvm::Type* llvm_v4f32 = llvm::FixedVectorType::get(llvm::Type::getFloatTy(*state.context), 4);
  state.type_cache[&fvec_ty] = llvm_v4f32;
  a_variable vec_var = {}, fvec_var = {};
  vec_var.type = &vec_ty;
  fvec_var.type = &fvec_ty;
  state.local_vars[&vec_var] = state.builder->CreateAlloca(llvm_v4i32);
  state.local_vars[&fvec_var] = state.builder->CreateAlloca(llvm_v4f32);

  an_expr_node v_op1 = {}, v_op2 = {}, v_op3 = {}, f_op1 = {};
  v_op1.kind = enk_variable; v_op1.type = &vec_ty; v_op1.variant.variable.ptr = &vec_var;
  v_op2.kind = enk_constant; v_op2.type = &vec_ty;
  v_op3.kind = enk_variable; v_op3.type = &vec_ty; v_op3.variant.variable.ptr = &vec_var;
  f_op1.kind = enk_variable; f_op1.type = &fvec_ty; f_op1.variant.variable.ptr = &fvec_var;

  bok_node.variant.builtin_operation.kind = bok_builtin_shuffle;
  v_op1.next = &v_op2; v_op2.next = nullptr; // 2-arg shuffle with Constant mask
  bok_node.variant.builtin_operation.operands = &v_op1;
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
  v_op1.next = &v_op3; v_op3.next = &v_op3; // 3-arg shuffle with non-Constant mask!
  v_op3.next = &v_op2; v_op2.next = nullptr;
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
  v_op1.next = &v_op3; v_op3.next = nullptr; // 2-arg shuffle with non-Constant mask!
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);

  bok_node.variant.builtin_operation.kind = bok_builtin_shufflevector;
  v_op1.next = &v_op3; v_op3.next = &op3; op3.next = &ptr_op; ptr_op.next = nullptr; // ConstantInt and non-ConstantInt mask indices
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
  v_op3.next = nullptr; // op2->next == nullptr fallback
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);

  bok_node.variant.builtin_operation.kind = bok_builtin_convertvector;
  v_op1.next = nullptr;
  bok_node.type = &vec_ty; // int -> int
  bok_node.variant.builtin_operation.operands = &v_op1;
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
  bok_node.type = &fvec_ty; // int -> fp
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
  bok_node.variant.builtin_operation.operands = &f_op1;
  bok_node.type = &fvec_ty; // fp -> fp
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
  bok_node.type = &vec_ty; // fp -> int
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);

  // Type trait builtins:
  a_field dummy_fld = {};
  a_type poly_cls = class_ty; poly_cls.variant.class_struct_union.any_virtual_functions = TRUE;
  poly_cls.variant.class_struct_union.any_virtual_functions_including_in_base_classes = TRUE;
  a_type non_empty_cls = class_ty; non_empty_cls.variant.class_struct_union.field_list = &dummy_fld;
  a_type abst_cls = class_ty; abst_cls.variant.class_struct_union.abstract = TRUE;
  a_type fin_cls = class_ty; fin_cls.variant.class_struct_union.final = TRUE;
  a_base_class bc_nonmatch = {}, bc_null = {}, bc_virt = {}, bc_nonvirt = {};
  bc_null.type = nullptr; bc_null.next = &bc_nonmatch;
  bc_nonmatch.type = &struct_ty; bc_nonmatch.next = &bc_nonvirt;
  bc_nonvirt.type = &class_ty; bc_nonvirt.is_virtual = FALSE; bc_nonvirt.next = &bc_virt;
  bc_virt.type = &class_ty; bc_virt.is_virtual = TRUE; bc_virt.next = nullptr;
  a_class_type_supplement derived_supp = {};
  derived_supp.base_classes = &bc_null;
  a_type derived_cls = struct_ty;
  derived_cls.variant.class_struct_union.extra_info = &derived_supp;

  int_kind_is_signed[ik_int] = TRUE;
  int_kind_is_signed[ik_unsigned_int] = FALSE;
  a_type signed_int_ty = int_ty; signed_int_ty.variant.integer.int_kind = ik_int;
  a_type unsigned_int_ty = int_ty; unsigned_int_ty.variant.integer.int_kind = ik_unsigned_int;
  a_type scoped_enum_ty = enum_ty; scoped_enum_ty.variant.integer.is_scoped_enum = TRUE;
  a_type nullptr_edg_ty = {}; nullptr_edg_ty.kind = tk_nullptr;
  a_type unb_arr_ty = arr_ty; unb_arr_ty.variant.array.variant.number_of_elements = 0;
  a_type rref_ty = ref_ty; rref_ty.variant.pointer.is_rvalue_reference = TRUE;
  a_type const_tref = {}; const_tref.kind = tk_typeref; const_tref.variant.typeref.type = &int_ty; const_tref.variant.typeref.qualifiers = TQ_CONST;
  a_type vol_tref = {}; vol_tref.kind = tk_typeref; vol_tref.variant.typeref.type = &int_ty; vol_tref.variant.typeref.qualifiers = TQ_VOLATILE;

  a_builtin_operation_kind trait_kinds[] = {
      bok_is_class, bok_is_union, bok_is_enum, bok_is_empty, bok_is_polymorphic,
      bok_is_abstract, bok_is_final, bok_is_same, bok_is_same_as, bok_is_base_of,
      bok_builtin_is_virtual_base_of, bok_array_rank, bok_array_extent, bok_is_trivial,
      bok_is_void, bok_is_integral, bok_is_floating_point, bok_is_arithmetic,
      bok_is_fundamental, bok_is_compound, bok_is_array, bok_is_bounded_array,
      bok_is_unbounded_array, bok_is_pointer, bok_is_lvalue_reference,
      bok_is_rvalue_reference, bok_is_reference, bok_is_member_pointer,
      bok_is_member_function_pointer, bok_is_member_object_pointer, bok_is_function,
      bok_is_scalar, bok_is_object, bok_is_const, bok_is_volatile, bok_is_signed,
      bok_is_unsigned, bok_is_scoped_enum, bok_has_virtual_destructor,
      bok_is_aggregate, bok_is_layout_compatible, bok_is_pointer_interconvertible_base_of,
      bok_reference_binds_to_temporary, bok_reference_constructs_from_temporary,
      bok_reference_converts_from_temporary
  };
  a_type_ptr trait_test_types[] = {
      nullptr, &void_ty, &nullptr_edg_ty, &bool_ty, &signed_int_ty, &unsigned_int_ty,
      &flt_ty, &class_ty, &struct_ty, &union_ty, &enum_ty, &scoped_enum_ty,
      &poly_cls, &non_empty_cls, &abst_cls, &fin_cls, &derived_cls, &arr_ty,
      &unb_arr_ty, &ptr_ty, &ref_ty, &rref_ty, &ptm_data_ty, &ptm_fn_ty,
      &ptm_null_ty, &rout_ty, &const_tref, &vol_tref, &i64_edg_ty
  };
  bok_node.type = &bool_ty;
  for (a_builtin_operation_kind tk : trait_kinds) {
    bok_node.variant.builtin_operation.kind = tk;
    for (a_type_ptr tt1 : trait_test_types) {
      ty_op_node.variant.type_operand.type = tt1;
      ty_op2.variant.type_operand.type = &derived_cls;
      ty_op_node.next = &ty_op2;
      bok_node.variant.builtin_operation.operands = &ty_op_node;
      assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
      ty_op2.variant.type_operand.type = &poly_cls;
      assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
      ty_op2.variant.type_operand.type = &int_ty;
      assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
      ty_op2.variant.type_operand.type = &int_ty2;
      assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
      ty_op2.variant.type_operand.type = nullptr;
      assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
      ty_op2.variant.type_operand.type = tt1;
      assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
    }
  }
  // Also test bok_is_const and bok_is_volatile when op1->kind != enk_type_operand
  op3.type = &const_tref; op3.next = nullptr;
  bok_node.variant.builtin_operation.operands = &op3;
  bok_node.variant.builtin_operation.kind = bok_is_const;
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
  op3.type = &vol_tref;
  bok_node.variant.builtin_operation.kind = bok_is_volatile;
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
  op3.type = &int_ty;
  // bok_array_extent with constant dim_idx = 0 and dim_idx = 1, and non-array cur, and op2 == nullptr
  a_constant ext_c = {}; ext_c.kind = ck_integer;
  assert(llvm_set_edg_sint64(&ext_c.variant.integer_value, 1) == llvm_gen_be_error_t::ok);
  an_expr_node ext_op2 = {}; ext_op2.kind = enk_constant; ext_op2.variant.constant.ptr = &ext_c;
  ty_op_node.variant.type_operand.type = &arr_ty;
  ty_op_node.next = &ext_op2;
  bok_node.type = &int_ty;
  bok_node.variant.builtin_operation.kind = bok_array_extent;
  bok_node.variant.builtin_operation.operands = &ty_op_node;
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
  assert(llvm_set_edg_sint64(&ext_c.variant.integer_value, 2) == llvm_gen_be_error_t::ok); // dim_idx = 2 -> cur becomes &int_ty (cur->kind != tk_array)
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
  ext_op2.variant.constant.ptr = nullptr;
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
  ty_op_node.next = nullptr; // op2 == nullptr
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);

  // bok_is_base_of with t1 = &class_ty and t2 = nullptr / t2 = &int_ty
  bok_node.variant.builtin_operation.kind = bok_is_base_of;
  ty_op_node.variant.type_operand.type = &class_ty;
  ty_op2.variant.type_operand.type = nullptr;
  ty_op_node.next = &ty_op2;
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
  ty_op2.variant.type_operand.type = &int_ty;
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);

  // bok_is_trivial with type_has_nontrivial_destructor == TRUE and expr->type == nullptr
  a_type dtor_cls = class_ty; dtor_cls.size = 777;
  ty_op_node.variant.type_operand.type = &dtor_cls;
  ty_op_node.next = nullptr;
  bok_node.type = nullptr;
  bok_node.variant.builtin_operation.kind = bok_is_trivial;
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::ok);
  bok_node.type = &int_ty;

  // Error propagation in bok_intaddr, bok_builtin_complex, bok_builtin_shuffle, bok_builtin_shufflevector, bok_builtin_convertvector
  a_constant bad_c = {}; bad_c.source_corresp.decl_position.seq = 888;
  an_expr_node bad_e = {}; bad_e.kind = enk_constant; bad_e.type = &int_ty; bad_e.variant.constant.ptr = &bad_c;

  bok_node.variant.builtin_operation.kind = bok_intaddr;
  bok_node.variant.builtin_operation.operands = &bad_e;
  assert(llvm_lower_expression(&bok_node, &val) != llvm_gen_be_error_t::ok);
  bok_node.type = &unk_ty;
  bok_node.variant.builtin_operation.operands = &op3;
  assert(llvm_lower_expression(&bok_node, &val) != llvm_gen_be_error_t::ok);
  bok_node.type = &int_ty;

  bok_node.variant.builtin_operation.kind = bok_builtin_complex;
  bok_node.variant.builtin_operation.operands = &bad_e;
  assert(llvm_lower_expression(&bok_node, &val) != llvm_gen_be_error_t::ok);

  bok_node.variant.builtin_operation.kind = bok_builtin_shuffle;
  bok_node.variant.builtin_operation.operands = &bad_e;
  assert(llvm_lower_expression(&bok_node, &val) != llvm_gen_be_error_t::ok);
  v_op1.next = &bad_e; bad_e.next = nullptr;
  bok_node.variant.builtin_operation.operands = &v_op1;
  assert(llvm_lower_expression(&bok_node, &val) != llvm_gen_be_error_t::ok);
  v_op1.next = &v_op3; v_op3.next = &bad_e; bad_e.next = nullptr;
  assert(llvm_lower_expression(&bok_node, &val) != llvm_gen_be_error_t::ok);

  bok_node.variant.builtin_operation.kind = bok_builtin_shufflevector;
  assert(llvm_lower_expression(&bok_node, &val) != llvm_gen_be_error_t::ok);

  bok_node.variant.builtin_operation.kind = bok_builtin_convertvector;
  bok_node.variant.builtin_operation.operands = &bad_e;
  assert(llvm_lower_expression(&bok_node, &val) != llvm_gen_be_error_t::ok);
  v_op1.next = nullptr;
  bok_node.type = &unk_ty;
  bok_node.variant.builtin_operation.operands = &v_op1;
  assert(llvm_lower_expression(&bok_node, &val) != llvm_gen_be_error_t::ok);
  bok_node.type = &int_ty;

  // Unhandled bok (bok_last)
  bok_node.variant.builtin_operation.kind = bok_last;
  assert(llvm_lower_expression(&bok_node, &val) == llvm_gen_be_error_t::unsupported_expr);

  printf("PASS\n");
  return 0;
}
