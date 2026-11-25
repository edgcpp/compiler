/**
 * @file test_llvm_expressions.cpp
 * @ingroup llvm_gen_be_tests
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
#include <llvm/IR/DIBuilder.h>

using namespace edg;

LLVMBackendState* edg::be_state = nullptr;
static int g_force_cip_in_dyn_init = 0; ///< When non-zero, `llvm_emit_dynamic_init` sets `prvalue_constructed_in_place = true`.

namespace edg {
  /**
   * @brief Resolves and extracts the LLVM FunctionType from an EDG routine or pointer-to-routine type.
   * @details Unwraps pointers, references, and typerefs, lowering the underlying routine type.
   * @param[in] type The EDG AST type pointer representing the routine or function pointer.
   * @param[out] out_ty Receives the lowered LLVM FunctionType pointer on success.
   * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
   */
  llvm_gen_be_error_t get_llvm_function_type(a_type_ptr type, llvm::FunctionType** out_ty) noexcept;
  /**
   * @brief Emits an LLVM bitfield load from a container memory address.
   * @details Loads the bitfield storage unit and shifts/masks/sign-extends the extracted bits.
   * @param[in] base_ptr Pointer to the bitfield storage unit.
   * @param[in] field The EDG bitfield descriptor.
   * @param[in] expr_type The resulting expression type of the bitfield load.
   * @param[out] out_val Receives the extracted bitfield value.
   * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
   */
  llvm_gen_be_error_t llvm_emit_bitfield_load(llvm::Value* base_ptr, a_field_ptr field, a_type_ptr expr_type, llvm::Value** out_val) noexcept;
  /**
   * @brief Emits an LLVM bitfield store into a container memory address.
   * @details Reads-modifies-writes the bitfield storage unit with the masked value.
   * @param[in] base_ptr Pointer to the bitfield storage unit.
   * @param[in] val The new value to store into the bitfield.
   * @param[in] field The EDG bitfield descriptor.
   * @param[in] expr_type The expression type of the bitfield assignment.
   * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
   */
  llvm_gen_be_error_t llvm_emit_bitfield_store(llvm::Value* base_ptr, llvm::Value* val, a_field_ptr field, a_type_ptr expr_type) noexcept;

/**
 * @brief Stub implementation of `get_mangled_function_name`.
 * @details Returns `nullptr` for expression unit tests.
 * @param[in] routine The EDG routine pointer.
 * @return Always `nullptr`.
 */
char* get_mangled_function_name(a_routine_ptr routine) {
    (void)routine;
    return nullptr;
}

  // Stub implementations required to link test
  /**
   * @brief Test stub or helper for `is_bool_type`.
   * @details Provides mock or helper behavior for `is_bool_type` within `tests/test_llvm_expressions.cpp`.
   * @param[in] ty Parameter `ty` for `is_bool_type`.
   * @return Result of `is_bool_type` (a_boolean).
   */
  a_boolean is_bool_type(a_type_ptr ty) {
    return (ty && ty->kind == tk_integer && ty->variant.integer.bool_type) ? TRUE : FALSE;
  }
  /**
   * @brief Test stub or helper for `is_class_struct_union_type`.
   * @details Provides mock or helper behavior for `is_class_struct_union_type` within `tests/test_llvm_expressions.cpp`.
   * @param[in] ty Parameter `ty` for `is_class_struct_union_type`.
   * @return Result of `is_class_struct_union_type` (a_boolean).
   */
  a_boolean is_class_struct_union_type(a_type_ptr ty) {
    return (ty && ty->alignment == 77) ? TRUE : FALSE;
  }
  /**
   * @brief Test stub or helper for `f_alignment_of_type`.
   * @details Provides mock or helper behavior for `f_alignment_of_type` within `tests/test_llvm_expressions.cpp`.
   * @param[in] ty Parameter `ty` for `f_alignment_of_type`.
   * @return Result of `f_alignment_of_type` (a_targ_alignment).
   */
  a_targ_alignment f_alignment_of_type(a_type_ptr ty) { return 1; }
  /**
   * @brief Test stub or helper for `is_trivially_copyable_type`.
   * @details Provides mock or helper behavior for `is_trivially_copyable_type` within `tests/test_llvm_expressions.cpp`.
   * @param[in] ty Parameter `ty` for `is_trivially_copyable_type`.
   * @return Result of `is_trivially_copyable_type` (a_boolean).
   */
  a_boolean is_trivially_copyable_type(a_type_ptr ty) { return (ty && ty->size == 444) ? FALSE : TRUE; }
  unsigned int targ_char_bit = 8;
  a_targ_size_t targ_sizeof_short = 2;
  a_targ_size_t targ_sizeof_int = 4;
  a_targ_size_t targ_sizeof_long = 8;
  a_targ_size_t targ_sizeof_long_long = 8;
  a_targ_size_t targ_sizeof_pointer = 8;
  a_boolean targ_little_endian = TRUE;
  a_C_dialect C_dialect = static_cast<a_C_dialect>(0);
  a_const_char* g_seq_short_fn = nullptr; ///< Optional short filename returned by `conv_seq_to_file_and_line`.
  a_const_char* g_seq_full_fn = nullptr; ///< Optional full filename returned by `conv_seq_to_file_and_line`.
  a_line_number g_seq_line = 0; ///< Optional 1-based line number returned by `conv_seq_to_file_and_line`.
  int g_dyn_init_emit_invoke = 0; ///< When non-zero, `llvm_emit_dynamic_init` emits an `invoke` to the active landing pad.

  /**
   * @brief Test stub or helper for `conv_seq_to_file_and_line`.
   * @details Provides mock or helper behavior for `conv_seq_to_file_and_line` within `tests/test_llvm_expressions.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `conv_seq_to_file_and_line`.
   * @param[out] arg_1 Parameter `arg_1` for `conv_seq_to_file_and_line`.
   * @param[out] arg_2 Parameter `arg_2` for `conv_seq_to_file_and_line`.
   * @param[in] arg_3 Parameter `arg_3` for `conv_seq_to_file_and_line`.
   * @param[in] arg_4 Parameter `arg_4` for `conv_seq_to_file_and_line`.
   * @return Result of `conv_seq_to_file_and_line` (a_source_file_ptr).
   */
  a_source_file_ptr conv_seq_to_file_and_line(a_seq_number arg_0, a_const_char** arg_1, a_const_char** arg_2, a_line_number* arg_3, a_boolean* arg_4) {
    (void)arg_0;
    if (arg_1) *arg_1 = g_seq_short_fn;
    if (arg_2) *arg_2 = g_seq_full_fn;
    if (arg_3) *arg_3 = g_seq_line;
    if (arg_4) *arg_4 = FALSE;
    return nullptr;
  }
  /**
   * @brief Test stub or helper for `mangled_typeinfo_name`.
   * @details Provides mock or helper behavior for `mangled_typeinfo_name` within `tests/test_llvm_expressions.cpp`.
   * @param[in] type Parameter `type` for `mangled_typeinfo_name`.
   * @return Result of `mangled_typeinfo_name` (char*).
   */
  char* mangled_typeinfo_name(a_type_ptr type) { static char buf[] = "_ZTIi"; return buf; }
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
   * @brief Test stub or helper for `f_get_type_qualifiers`.
   * @details Provides mock or helper behavior for `f_get_type_qualifiers` within `tests/test_llvm_expressions.cpp`.
   * @param[in] type Parameter `type` for `f_get_type_qualifiers`.
   * @param[in] x Parameter `x` for `f_get_type_qualifiers`.
   * @return Result of `f_get_type_qualifiers` (a_type_qualifier_set).
   */
  a_type_qualifier_set f_get_type_qualifiers(a_type_ptr type, a_boolean x) {
      (void)x;
      if (type && type->kind == tk_typeref && type->variant.typeref.qualifiers != 0) {
          return type->variant.typeref.qualifiers;
      }
      return (type && type->size == 999) ? TQ_VOLATILE : 0;
  }
  /**
   * @brief Test stub or helper for `is_reference_type`.
   * @details Provides mock or helper behavior for `is_reference_type` within `tests/test_llvm_expressions.cpp`.
   * @param[in] ty Parameter `ty` for `is_reference_type`.
   * @return Result of `is_reference_type` (a_boolean).
   */
  a_boolean is_reference_type(a_type_ptr ty) { return (ty && ty->size == 555) ? TRUE : FALSE; }
  /**
   * @brief Test stub or helper for `is_empty_class_type`.
   * @details Provides mock or helper behavior for `is_empty_class_type` within `tests/test_llvm_expressions.cpp`.
   * @param[in] ty Parameter `ty` for `is_empty_class_type`.
   * @return Result of `is_empty_class_type` (a_boolean).
   */
  a_boolean is_empty_class_type(a_type_ptr ty) { return ty && ty->variant.class_struct_union.is_empty_class; }
  /**
   * @brief Test stub or helper for `evaluate_constant`.
   * @details Provides mock or helper behavior for `evaluate_constant` within `tests/test_llvm_expressions.cpp`.
   * @param[in] con Parameter `con` for `evaluate_constant`.
   * @param[in] ty Parameter `ty` for `evaluate_constant`.
   * @param[out] out_const Parameter `out_const` for `evaluate_constant`.
   * @return Result of `evaluate_constant` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t evaluate_constant(a_constant_ptr con, llvm::Type* ty, llvm::Constant** out_const) noexcept { 
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    if (con && con->source_corresp.decl_position.seq == 888) return llvm_gen_be_error_t::unsupported_expr;
    if (ty->isIntegerTy()) {
      uint64_t v = (con && con->source_corresp.decl_position.seq >= 10)
                       ? (con->source_corresp.decl_position.seq - 10)
                       : 1;
      *out_const = llvm::ConstantInt::get(ty, v);
    }
    else if (ty->isFloatingPointTy()) *out_const = llvm::ConstantFP::get(ty, 1.0);
    else *out_const = llvm::Constant::getNullValue(ty);
    return llvm_gen_be_error_t::ok; 
  }
  
  a_byte_boolean int_kind_is_signed[ik_last] = {0};

  /**
   * @brief Test stub or helper for `type_has_nontrivial_destructor`.
   * @details Provides mock or helper behavior for `type_has_nontrivial_destructor` within `tests/test_llvm_expressions.cpp`.
   * @param[in] tp Parameter `tp` for `type_has_nontrivial_destructor`.
   * @return Result of `type_has_nontrivial_destructor` (a_boolean).
   */
  a_boolean type_has_nontrivial_destructor(a_type_ptr tp) {
      return (tp && tp->size == 777) ? TRUE : FALSE;
  }

  /**
   * @brief Test stub or helper for `llvm_emit_eh_cleanup`.
   * @details Provides mock or helper behavior for `llvm_emit_eh_cleanup` within `tests/test_llvm_expressions.cpp`.
   * @param[in] dtor Parameter `dtor` for `llvm_emit_eh_cleanup`.
   * @param[in] obj_ptr Parameter `obj_ptr` for `llvm_emit_eh_cleanup`.
   * @return Result of `llvm_emit_eh_cleanup` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_emit_eh_cleanup(a_routine_ptr dtor, llvm::Value* obj_ptr) noexcept {
      LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
      if (dtor && obj_ptr) {
        ScopeCleanup sc = {};
        sc.dtor_routine = dtor;
        sc.object_ptr = obj_ptr;
        sc.is_active = true;
        be_state->cleanup_stack.cleanups.push_back(sc);
      }
      return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub for `llvm_emit_routine_exc_spec_handler` in `tests/test_llvm_expressions.cpp`.
   * @details Emits an `unreachable` terminator when `routine` has a non-`throw_any` exception specification.
   * @param[in] routine Optional EDG routine whose exception specification is inspected.
   * @param[in,out] lpad LandingPadInst to inspect.
   * @param[out] out_handled Receives `true` if handled.
   * @return llvm_gen_be_error_t::ok on success.
   */
  llvm_gen_be_error_t llvm_emit_routine_exc_spec_handler(
      a_routine_ptr routine,
      llvm::LandingPadInst* lpad,
      bool* out_handled) noexcept {
      if (!lpad || !out_handled) return llvm_gen_be_error_t::invalid_argument;
      LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
      *out_handled = false;
      if (routine && routine->type && routine->type->kind == tk_routine &&
          routine->type->variant.routine.extra_info &&
          routine->type->variant.routine.extra_info->exception_specification &&
          !routine->type->variant.routine.extra_info->exception_specification->throw_any) {
        be_state->builder->CreateUnreachable();
        *out_handled = true;
      }
      return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub for `llvm_append_enclosing_try_clauses` in `tests/test_llvm_expressions.cpp`.
   * @details Appends a catch-all null clause onto @p lpad in standalone expression tests.
   * @param[in,out] lpad LandingPadInst to populate.
   * @return llvm_gen_be_error_t::ok on success.
   */
  llvm_gen_be_error_t llvm_append_enclosing_try_clauses(
      llvm::LandingPadInst* lpad) noexcept {
      if (!lpad) return llvm_gen_be_error_t::invalid_argument;
      LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
      lpad->addClause(llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context)));
      return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub for `llvm_get_or_create_current_unwind_dest` in `tests/test_llvm_expressions.cpp`.
   * @details Returns the top of `be_state->current_landing_pads` (or `nullptr`).
   * @param[out] out_unwind_bb Receives the active landing pad basic block.
   * @return llvm_gen_be_error_t::ok on success.
   */
  llvm_gen_be_error_t llvm_get_or_create_current_unwind_dest(llvm::BasicBlock** out_unwind_bb) noexcept {
      if (!out_unwind_bb) return llvm_gen_be_error_t::invalid_argument;
      LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
      *out_unwind_bb = be_state->current_landing_pads.empty() ? nullptr : be_state->current_landing_pads.back();
      return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub for `llvm_get_or_create_msvc_throw_info` in `tests/test_llvm_expressions.cpp`.
   * @details Creates or retrieves a `_TI1?AU...` global variable in the active module.
   * @param[in] thrown_type The EDG type of the thrown exception object.
   * @param[in] dtor_fn Optional destructor `llvm::Constant*`.
   * @param[out] out_throw_info Receives the `_TI...` `llvm::Constant*` global pointer.
   * @return llvm_gen_be_error_t::ok on success.
   */
  llvm_gen_be_error_t llvm_get_or_create_msvc_throw_info(
      a_type_ptr thrown_type,
      llvm::Constant* dtor_fn,
      llvm::Constant** out_throw_info) noexcept {
      if (!thrown_type || !out_throw_info) return llvm_gen_be_error_t::invalid_argument;
      LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
      const char* raw_tname = thrown_type->source_corresp.name ? thrown_type->source_corresp.name : (dtor_fn ? "dtor_type" : "type");
      std::string ti_name = std::string("_TI1?AU") + raw_tname + "@@";
      llvm::GlobalVariable* gv = be_state->module->getNamedGlobal(ti_name);
      if (!gv) {
          llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
          LLVM_GEN_BE_CHECK(llvm_create_global_variable(
              be_state->module.get(), i8_ty, true, llvm::GlobalValue::LinkOnceODRLinkage,
              llvm::ConstantInt::get(i8_ty, 0), ti_name, &gv));
      }
      *out_throw_info = gv;
      return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub or helper for `get_di_location`.
   * @details Provides mock or helper behavior for `get_di_location` within `tests/test_llvm_expressions.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `get_di_location`.
   * @param[in] arg_1 Parameter `arg_1` for `get_di_location`.
   * @param[in] arg_2 Parameter `arg_2` for `get_di_location`.
   * @param[out] out_loc Parameter `out_loc` for `get_di_location`.
   * @return Result of `get_di_location` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_di_location(llvm_gen_be_debug_state_t* arg_0, a_source_position arg_1, llvm::DIScope* arg_2, llvm::DILocation** out_loc) noexcept {
    if (arg_1.seq == 888) return llvm_gen_be_error_t::unsupported_expr;
    if (out_loc) {
      if (arg_1.seq == 777 || !arg_2) *out_loc = nullptr;
      else *out_loc = llvm::DILocation::get(*be_state->context, arg_1.seq, 1, arg_2);
    }
    return llvm_gen_be_error_t::ok;
  }
  /**
   * @brief Test stub or helper for `emit_dbg_value_for_variable`.
   * @details Provides mock or helper behavior for `emit_dbg_value_for_variable` within `tests/test_llvm_expressions.cpp`.
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
    if (arg_1 && arg_1->alignment == 99) return llvm_gen_be_error_t::unsupported_expr;
    return llvm_gen_be_error_t::ok;
  }
  /**
   * @brief Test stub or helper for `vtbl_decider_function_for_class`.
   * @details Provides mock or helper behavior for `vtbl_decider_function_for_class` within `tests/test_llvm_expressions.cpp`.
   * @param[in] class_type Parameter `class_type` for `vtbl_decider_function_for_class`.
   * @param[in] unknown Parameter `unknown` for `vtbl_decider_function_for_class`.
   * @return Result of `vtbl_decider_function_for_class` (a_routine_ptr).
   */
  a_routine_ptr vtbl_decider_function_for_class(a_type_ptr class_type, a_boolean* unknown) {
      if (unknown) *unknown = FALSE;
      return nullptr;
  }
}


namespace edg {
  /**
   * @brief Test stub or helper for `mangled_vtbl_name`.
   * @details Provides mock or helper behavior for `mangled_vtbl_name` within `tests/test_llvm_expressions.cpp`.
   * @param[in] class_type Parameter `class_type` for `mangled_vtbl_name`.
   * @param[in] bcp Parameter `bcp` for `mangled_vtbl_name`.
   * @param[in] ctor_bcp Parameter `ctor_bcp` for `mangled_vtbl_name`.
   * @return Result of `mangled_vtbl_name` (char*).
   */
  char* mangled_vtbl_name(a_type_ptr class_type, a_base_class_ptr bcp, a_base_class_ptr ctor_bcp) {
      if (bcp) return (char*)"_ZTCmock";
      return (char*)"_ZTVmock";
  }
  /**
   * @brief Test stub or helper for `mangled_virtual_table_table_name`.
   * @details Provides mock or helper behavior for `mangled_virtual_table_table_name` within `tests/test_llvm_expressions.cpp`.
   * @param[in] type Parameter `type` for `mangled_virtual_table_table_name`.
   * @return Result of `mangled_virtual_table_table_name` (char*).
   */
  char* mangled_virtual_table_table_name(a_type_ptr type) {
      return (char*)"_ZTTmock";
  }
}

/**
 * @brief Lowers a builtin call expression with the specified routine name, return type, and operands.
 * @details Constructs a synthetic `enk_operation` (`eok_call`) node targeting a routine named @p name
 *          and invokes `llvm_lower_call_expression`.
 * @param[in] name Builtin routine name string.
 * @param[in] ret_t EDG return type for the call expression.
 * @param[in] ops Initializer list of operand expression node pointers.
 * @return llvm_gen_be_error_t::ok on success, or an error code from `llvm_lower_call_expression`.
 */
static llvm_gen_be_error_t call_test_builtin(
    const char* name,
    a_type_ptr ret_t,
    std::initializer_list<an_expr_node*> ops) noexcept {
  a_routine br = {};
  br.source_corresp.name = const_cast<char*>(name);
  an_expr_node bop1 = {};
  bop1.kind = enk_routine;
  bop1.variant.routine.ptr = &br;
  std::vector<an_expr_node> local_ops;
  local_ops.reserve(ops.size());
  for (an_expr_node* op : ops) {
    local_ops.push_back(*op);
    local_ops.back().next = nullptr;
  }
  an_expr_node* prev = &bop1;
  for (size_t i = 0; i < local_ops.size(); ++i) {
    prev->next = &local_ops[i];
    prev = &local_ops[i];
  }
  an_expr_node cexpr = {};
  cexpr.kind = enk_operation;
  cexpr.type = ret_t;
  cexpr.variant.operation.kind = eok_call;
  cexpr.variant.operation.operands = &bop1;
  llvm::Value* out = nullptr;
  return llvm_lower_call_expression(&cexpr, &out);
}

/**
 * @brief Lowers a throw expression and resets the IRBuilder insertion point to a fresh basic block.
 * @details Calls `llvm_lower_expression` on @p te and creates a continuation basic block in @p fn.
 * @param[in] te Pointer to the throw expression node.
 * @param[in,out] fn Enclosing `llvm::Function` to append the post-throw basic block to.
 * @return llvm_gen_be_error_t::ok on success, or an error code from `llvm_lower_expression`.
 */
static llvm_gen_be_error_t run_test_throw(
    an_expr_node* te,
    llvm::Function* fn) noexcept {
  llvm::Value* val = nullptr;
  llvm_gen_be_error_t err = llvm_lower_expression(te, &val);
  llvm::BasicBlock* next_bb = llvm::BasicBlock::Create(*be_state->context, "post.throw", fn);
  be_state->builder->SetInsertPoint(next_bb);
  return err;
}

/**
 * @brief Entry point for the expression lowering unit test suite.
 * @details Exercises lvalues, arithmetic, logical, cast, call, compound assignment, complex, and C++ expressions.
 * @return 0 on success.
 */
int main(void) {
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
  simd_routine.source_corresp.name = "__builtin_ia32_pause";
  simd_func.variant.routine.ptr = &simd_routine;
  simd_func.next = nullptr;
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
      state.type_cache[&op_ty] = llvm::FixedVectorType::get(
          llvm::Type::getInt32Ty(*state.context), 4);
      op1.type = &op_ty;
      
      an_expr_node op2 = {};
      op2.kind = enk_constant;
      op2.type = &op_ty;

      an_expr_node mask = {};
      mask.kind = enk_constant;
      mask.type = &int_type;
      a_constant mask_const = {};
      mask_const.kind = ck_integer;
      assert(llvm_set_edg_sint64(&mask_const.variant.integer_value, 0) == llvm_gen_be_error_t::ok);
      mask.variant.constant.ptr = &mask_const;

      an_expr_node simd_avx = {};
      simd_avx.kind = enk_operation;
      simd_avx.variant.operation.kind = eok_call;
      an_expr_node simd_avx_func = {};
      simd_avx_func.kind = enk_routine;
      a_routine simd_avx_routine = {};
      simd_avx_routine.source_corresp.name = "__builtin_ia32_paddd128";
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
      state.type_cache[&fp_vec_ty] = llvm::FixedVectorType::get(
          llvm::Type::getFloatTy(*state.context), 4);
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
  assert(llvm_set_edg_sint64(&pm_int_con.variant.integer_value, 8) == llvm_gen_be_error_t::ok);

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

  // Test enk_fold, enk_pack_index, enk_c11_generic (empty/unsupported and valid)
  an_expr_node unsupported_expr;
  memset(&unsupported_expr, 0, sizeof(unsupported_expr));
  unsupported_expr.kind = enk_fold;
  unsupported_expr.type = &int_type;
  llvm::Value* unsup_val = nullptr;
  assert(llvm_lower_expression(&unsupported_expr, &unsup_val) == llvm_gen_be_error_t::unsupported_expr);

  an_expr_node fold_op1 = static_op1;
  an_expr_node fold_op2 = static_op1;
  fold_op1.next = &fold_op2;
  fold_op2.next = nullptr;
  unsupported_expr.variant.fold.operands = &fold_op1;
  unsupported_expr.variant.fold.operator_token = tok_plus;
  assert(llvm_lower_expression(&unsupported_expr, &unsup_val) == llvm_gen_be_error_t::ok);
  assert(unsup_val != nullptr);

  unsupported_expr.kind = enk_pack_index;
  memset(&unsupported_expr.variant, 0, sizeof(unsupported_expr.variant));
  assert(llvm_lower_expression(&unsupported_expr, &unsup_val) == llvm_gen_be_error_t::unsupported_expr);

  unsupported_expr.variant.pack_index.expr = &fold_op1;
  unsupported_expr.variant.pack_index.index_expr = &static_op1;
  assert(llvm_lower_expression(&unsupported_expr, &unsup_val) == llvm_gen_be_error_t::ok);
  assert(unsup_val != nullptr);

  unsupported_expr.kind = enk_c11_generic;
  memset(&unsupported_expr.variant, 0, sizeof(unsupported_expr.variant));
  assert(llvm_lower_expression(&unsupported_expr, &unsup_val) == llvm_gen_be_error_t::unsupported_expr);

  unsupported_expr.variant.c11_generic.result = &static_op1;
  assert(llvm_lower_expression(&unsupported_expr, &unsup_val) == llvm_gen_be_error_t::ok);
  assert(unsup_val != nullptr);

  // Test enk_reuse_value with dik_zero and dik_constant
  a_dynamic_init zero_init = {};
  zero_init.kind = dik_zero;
  reuse_expr.variant.reused_value_init = &zero_init;
  assert(llvm_lower_expression(&reuse_expr, &reuse_val) == llvm_gen_be_error_t::ok);
  assert(reuse_val != nullptr);

  a_dynamic_init const_init = {};
  const_init.kind = dik_constant;
  a_constant con_dummy = {};
  const_init.variant.constant.ptr = &con_dummy;
  reuse_expr.variant.reused_value_init = &const_init;
  assert(llvm_lower_expression(&reuse_expr, &reuse_val) == llvm_gen_be_error_t::ok);
  assert(reuse_val != nullptr);

  // Test eok_va_start, eok_va_arg, eok_va_end, eok_va_copy
  a_type ptr_type = {};
  ptr_type.kind = tk_pointer;
  ptr_type.size = 8;
  ptr_type.variant.pointer.type = &int_type;

  a_variable va_var = {};
  va_var.source_corresp.name = (char*)"ap";
  va_var.type = &ptr_type;
  llvm::AllocaInst* va_alloca = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context), nullptr, "ap");
  state.local_vars[&va_var] = va_alloca;

  an_expr_node va_var_node = {};
  va_var_node.kind = enk_variable;
  va_var_node.type = &ptr_type;
  va_var_node.variant.variable.ptr = &va_var;

  an_expr_node va_op_expr = {};
  va_op_expr.kind = enk_operation;
  va_op_expr.type = &int_type;
  va_op_expr.variant.operation.operands = &va_var_node;

  llvm::Value* va_out = nullptr;
  va_op_expr.variant.operation.kind = eok_va_start_single_operand;
  assert(llvm_lower_expression(&va_op_expr, &va_out) == llvm_gen_be_error_t::ok);
  va_op_expr.variant.operation.kind = eok_va_arg;
  assert(llvm_lower_expression(&va_op_expr, &va_out) == llvm_gen_be_error_t::ok);
  va_op_expr.variant.operation.kind = eok_va_end;
  assert(llvm_lower_expression(&va_op_expr, &va_out) == llvm_gen_be_error_t::ok);

  an_expr_node va_src_node = va_var_node;
  va_var_node.next = &va_src_node;
  va_op_expr.variant.operation.kind = eok_va_copy;
  assert(llvm_lower_expression(&va_op_expr, &va_out) == llvm_gen_be_error_t::ok);
  va_var_node.next = nullptr;

  // Test _Complex equality/inequality and scalar-to-_Complex / _Complex-to-scalar casts
  a_type cplx_ty = {};
  cplx_ty.kind = tk_complex;
  cplx_ty.size = 8;
  cplx_ty.variant.float_kind = fk_float;

  a_variable cplx_var = {};
  cplx_var.type = &cplx_ty;
  llvm::Type* cplx_llvm_ty = nullptr;
  assert(get_llvm_type(&cplx_ty, &cplx_llvm_ty) == llvm_gen_be_error_t::ok);
  state.local_vars[&cplx_var] = state.builder->CreateAlloca(cplx_llvm_ty, nullptr, "cplx_v");

  an_expr_node cplx_op1 = {};
  cplx_op1.kind = enk_variable;
  cplx_op1.type = &cplx_ty;
  cplx_op1.variant.variable.ptr = &cplx_var;
  an_expr_node cplx_op2 = cplx_op1;
  cplx_op1.next = &cplx_op2;

  an_expr_node cplx_cmp = {};
  cplx_cmp.kind = enk_operation;
  cplx_cmp.type = &int_type;
  cplx_cmp.variant.operation.kind = eok_eq;
  cplx_cmp.variant.operation.operands = &cplx_op1;
  llvm::Value* cplx_cmp_val = nullptr;
  assert(llvm_lower_expression(&cplx_cmp, &cplx_cmp_val) == llvm_gen_be_error_t::ok);
  cplx_cmp.variant.operation.kind = eok_ne;
  assert(llvm_lower_expression(&cplx_cmp, &cplx_cmp_val) == llvm_gen_be_error_t::ok);
  cplx_op1.next = nullptr;

  an_expr_node to_cplx_cast = {};
  to_cplx_cast.kind = enk_operation;
  to_cplx_cast.type = &cplx_ty;
  to_cplx_cast.variant.operation.kind = eok_cast;
  to_cplx_cast.variant.operation.operands = &static_op1;
  llvm::Value* cplx_cast_val = nullptr;
  assert(llvm_lower_expression(&to_cplx_cast, &cplx_cast_val) == llvm_gen_be_error_t::ok);

  an_expr_node from_cplx_cast = {};
  from_cplx_cast.kind = enk_operation;
  from_cplx_cast.type = &float_type;
  from_cplx_cast.variant.operation.kind = eok_cast;
  from_cplx_cast.variant.operation.operands = &cplx_op1;
  assert(llvm_lower_expression(&from_cplx_cast, &cplx_cast_val) == llvm_gen_be_error_t::ok);

  // --- Comprehensive coverage tests for llvm_gen_be_expr.cpp ---
  {
    a_type bad_ty = {};
    bad_ty.kind = static_cast<a_type_kind>(250);
    a_type void_ty = {};
    void_ty.kind = tk_void;
    a_type i8_ty = {};
    i8_ty.kind = tk_integer;
    i8_ty.size = 1;
    i8_ty.variant.integer.int_kind = ik_char;
    a_type i16_ty = {};
    i16_ty.kind = tk_integer;
    i16_ty.size = 2;
    i16_ty.variant.integer.int_kind = ik_short;
    a_type i64_ty = {};
    i64_ty.kind = tk_integer;
    i64_ty.size = 8;
    i64_ty.variant.integer.int_kind = ik_long;
    a_type u32_ty = {};
    u32_ty.kind = tk_integer;
    u32_ty.size = 4;
    u32_ty.variant.integer.int_kind = ik_unsigned_int;
    int_kind_is_signed[ik_int] = TRUE;
    int_kind_is_signed[ik_long] = TRUE;
    int_kind_is_signed[ik_short] = TRUE;
    int_kind_is_signed[ik_char] = TRUE;
    int_kind_is_signed[ik_unsigned_int] = FALSE;

    a_type double_ty = {};
    double_ty.kind = tk_float;
    double_ty.size = 8;
    double_ty.variant.float_kind = fk_double;

    a_type cplx_double_ty = {};
    cplx_double_ty.kind = tk_complex;
    cplx_double_ty.size = 16;
    cplx_double_ty.variant.float_kind = fk_double;

    // 1. get_llvm_function_type coverage
    llvm::FunctionType* fn_ty_out = nullptr;
    assert(get_llvm_function_type(nullptr, &fn_ty_out) == llvm_gen_be_error_t::ok);
    assert(get_llvm_function_type(&int_type, &fn_ty_out) == llvm_gen_be_error_t::ok);

    a_routine_type_supplement rts_fn = {};
    a_param_type p1_fn = {};
    p1_fn.type = &int_type;
    rts_fn.param_type_list = &p1_fn;
    a_type rout_ty = {};
    rout_ty.kind = tk_routine;
    rout_ty.variant.routine.return_type = &int_type;
    rout_ty.variant.routine.extra_info = &rts_fn;
    a_type tref_rout_ty = {};
    tref_rout_ty.kind = tk_typeref;
    tref_rout_ty.variant.typeref.type = &rout_ty;
    a_type ptr_rout_ty = {};
    ptr_rout_ty.kind = tk_pointer;
    ptr_rout_ty.size = 8;
    ptr_rout_ty.variant.pointer.type = &tref_rout_ty;
    assert(get_llvm_function_type(&ptr_rout_ty, &fn_ty_out) == llvm_gen_be_error_t::ok);

    a_type ref_rout_ty = {};
    ref_rout_ty.kind = tk_typeref;
    ref_rout_ty.size = 555; // triggers is_reference_type
    ref_rout_ty.variant.pointer.type = &rout_ty;
    assert(get_llvm_function_type(&ref_rout_ty, &fn_ty_out) == llvm_gen_be_error_t::ok);

    state.type_cache.erase(&rout_ty);
    rout_ty.variant.routine.return_type = &bad_ty;
    assert(get_llvm_function_type(&rout_ty, &fn_ty_out) != llvm_gen_be_error_t::ok);
    state.type_cache.erase(&rout_ty);
    rout_ty.variant.routine.return_type = nullptr;
    p1_fn.type = &bad_ty;
    assert(get_llvm_function_type(&rout_ty, &fn_ty_out) != llvm_gen_be_error_t::ok);
    state.type_cache.erase(&rout_ty);
    p1_fn.type = &int_type;
    assert(get_llvm_function_type(&rout_ty, &fn_ty_out) == llvm_gen_be_error_t::ok);
    state.type_cache.erase(&rout_ty);
    rout_ty.variant.routine.return_type = &int_type;

    // 2. Bitfield load & store (volatile, MSVC, spanning, wider/narrower dest_ty, big-endian, shift==0)
    a_type vol_int_ty = int_type;
    vol_int_ty.size = 999; // triggers TQ_VOLATILE in f_get_type_qualifiers
    a_field bf_f = {};
    bf_f.is_bit_field = TRUE;
    bf_f.type = &int_type;
    int_type.size = 4;
    bf_f.offset_bit_remainder = 0; // shift == 0
    bf_f.bit_size = 5;
    llvm::Value* bf_ptr = state.builder->CreateAlloca(llvm::Type::getInt64Ty(*state.context));
    llvm::Value* bf_val = nullptr;
    assert(llvm_emit_bitfield_load(bf_ptr, &bf_f, &bad_ty, &bf_val) != llvm_gen_be_error_t::ok);
    assert(llvm_emit_bitfield_load(bf_ptr, &bf_f, &vol_int_ty, &bf_val) == llvm_gen_be_error_t::ok);
    assert(llvm_emit_bitfield_store(bf_ptr, bf_val, &bf_f, &int_type) == llvm_gen_be_error_t::ok);
    bf_f.offset_bit_remainder = 2; // shift > 0
    assert(llvm_emit_bitfield_load(bf_ptr, &bf_f, &i64_ty, &bf_val) == llvm_gen_be_error_t::ok);
    assert(llvm_emit_bitfield_load(bf_ptr, &bf_f, &i8_ty, &bf_val) == llvm_gen_be_error_t::ok);
    targ_little_endian = FALSE;
    assert(llvm_emit_bitfield_load(bf_ptr, &bf_f, &int_type, &bf_val) == llvm_gen_be_error_t::ok);
    assert(llvm_emit_bitfield_store(bf_ptr, bf_val, &bf_f, &int_type) == llvm_gen_be_error_t::ok);
    targ_little_endian = TRUE;
    bf_f.offset_bit_remainder = 4;
    bf_f.bit_size = 6; // 10 bits -> 16-bit container
    assert(llvm_emit_bitfield_load(bf_ptr, &bf_f, &u32_ty, &bf_val) == llvm_gen_be_error_t::ok);
    bf_f.offset_bit_remainder = 4;
    bf_f.bit_size = 16; // 20 bits -> 32-bit container
    assert(llvm_emit_bitfield_load(bf_ptr, &bf_f, &int_type, &bf_val) == llvm_gen_be_error_t::ok);

    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    assert(llvm_emit_bitfield_load(bf_ptr, &bf_f, &int_type, &bf_val) == llvm_gen_be_error_t::ok);
    assert(llvm_emit_bitfield_store(bf_ptr, bf_val, &bf_f, &vol_int_ty) == llvm_gen_be_error_t::ok);
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
    assert(llvm_emit_bitfield_store(bf_ptr, state.builder->getInt64(3), &bf_f, &int_type) == llvm_gen_be_error_t::ok);

    // 3. Complex arithmetic & comparisons & casts (float and double and int-complex)
    a_variable cplx_d_var = {};
    cplx_d_var.type = &cplx_double_ty;
    llvm::Type* cplx_d_llvm_ty = nullptr;
    assert(get_llvm_type(&cplx_double_ty, &cplx_d_llvm_ty) == llvm_gen_be_error_t::ok);
    state.local_vars[&cplx_d_var] = state.builder->CreateAlloca(cplx_d_llvm_ty);

    an_expr_node cd_op1 = {};
    cd_op1.kind = enk_variable;
    cd_op1.type = &cplx_double_ty;
    cd_op1.variant.variable.ptr = &cplx_d_var;
    an_expr_node cd_op2 = cd_op1;
    cd_op1.next = &cd_op2;

    an_expr_node cd_arith = {};
    cd_arith.kind = enk_operation;
    cd_arith.type = &cplx_double_ty;
    cd_arith.variant.operation.operands = &cd_op1;
    for (an_expr_operator_kind k : {eok_negate, eok_add, eok_subtract, eok_multiply, eok_divide, eok_xconj}) {
      cd_arith.variant.operation.kind = k;
      assert(llvm_lower_arithmetic_expression(&cd_arith, &val) == llvm_gen_be_error_t::ok);
    }
    cplx_op1.next = &cplx_op2;
    cd_arith.type = &cplx_ty;
    cd_arith.variant.operation.operands = &cplx_op1;
    for (an_expr_operator_kind k : {eok_multiply, eok_divide, eok_xconj}) {
      cd_arith.variant.operation.kind = k;
      assert(llvm_lower_arithmetic_expression(&cd_arith, &val) == llvm_gen_be_error_t::ok);
    }
    cplx_op1.next = nullptr;
    cd_op1.next = nullptr;

    // Complex to complex cast, float to complex cast, complex to double cast
    an_expr_node c2c_cast = {};
    c2c_cast.kind = enk_operation;
    c2c_cast.type = &cplx_double_ty;
    c2c_cast.variant.operation.kind = eok_cast;
    c2c_cast.variant.operation.operands = &cplx_op1;
    assert(llvm_lower_cast_expression(&c2c_cast, &val) == llvm_gen_be_error_t::ok);
    c2c_cast.type = &double_ty;
    assert(llvm_lower_cast_expression(&c2c_cast, &val) == llvm_gen_be_error_t::ok);

    an_expr_node flt_const_node = {};
    flt_const_node.kind = enk_constant;
    flt_const_node.type = &float_type;
    c2c_cast.type = &cplx_double_ty;
    c2c_cast.variant.operation.operands = &flt_const_node;
    assert(llvm_lower_cast_expression(&c2c_cast, &val) == llvm_gen_be_error_t::ok);

    // Integer complex equality/inequality (2-element int struct)
    a_field ic_f1 = {}, ic_f2 = {};
    ic_f1.type = &int_type;
    ic_f2.type = &int_type;
    ic_f2.offset = 4;
    ic_f1.next = &ic_f2;
    a_type ic_struct_ty = {};
    ic_struct_ty.kind = tk_struct;
    ic_struct_ty.size = 8;
    ic_struct_ty.variant.class_struct_union.field_list = &ic_f1;
    llvm::Type* ic_llvm_ty = nullptr;
    assert(get_llvm_type(&ic_struct_ty, &ic_llvm_ty) == llvm_gen_be_error_t::ok);
    a_variable ic_var = {};
    ic_var.type = &ic_struct_ty;
    state.local_vars[&ic_var] = state.builder->CreateAlloca(ic_llvm_ty);
    an_expr_node ic_op1 = {}, ic_op2 = {};
    ic_op1.kind = enk_variable;
    ic_op1.type = &ic_struct_ty;
    ic_op1.variant.variable.ptr = &ic_var;
    ic_op2 = ic_op1;
    ic_op1.next = &ic_op2;
    an_expr_node ic_cmp = {};
    ic_cmp.kind = enk_operation;
    ic_cmp.type = &int_type;
    ic_cmp.variant.operation.operands = &ic_op1;
    ic_cmp.variant.operation.kind = eok_eq;
    assert(llvm_lower_logical_expression(&ic_cmp, &val) == llvm_gen_be_error_t::ok);
    ic_cmp.variant.operation.kind = eok_ne;
    assert(llvm_lower_logical_expression(&ic_cmp, &val) == llvm_gen_be_error_t::ok);

    // eok_real_part and eok_imag_part lvalue & rvalue on struct, scalar, void, null
    an_expr_node ri_expr = {};
    ri_expr.kind = enk_operation;
    ri_expr.type = &float_type;
    ri_expr.variant.operation.operands = &cplx_op1;
    ri_expr.variant.operation.kind = eok_real_part;
    assert(llvm_lower_lvalue_expression(&ri_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&ri_expr, &val) == llvm_gen_be_error_t::ok);
    ri_expr.variant.operation.kind = eok_imag_part;
    assert(llvm_lower_lvalue_expression(&ri_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&ri_expr, &val) == llvm_gen_be_error_t::ok);

    ri_expr.variant.operation.operands = &dummy_op1;
    dummy_op1.next = nullptr;
    ri_expr.type = &int_type;
    ri_expr.variant.operation.kind = eok_real_part;
    assert(llvm_lower_lvalue_expression(&ri_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&ri_expr, &val) == llvm_gen_be_error_t::ok);
    ri_expr.variant.operation.kind = eok_imag_part;
    assert(llvm_lower_lvalue_expression(&ri_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&ri_expr, &val) == llvm_gen_be_error_t::ok);
    ri_expr.type = &void_ty;
    assert(llvm_lower_lvalue_expression(&ri_expr, &val) == llvm_gen_be_error_t::unsupported_expr);
    ri_expr.type = &bad_ty;
    assert(llvm_lower_lvalue_expression(&ri_expr, &val) != llvm_gen_be_error_t::ok);
    ri_expr.type = nullptr;
    assert(llvm_lower_lvalue_expression(&ri_expr, &val) == llvm_gen_be_error_t::unsupported_expr);

    // 4. Multi-dimensional VLA subscript lvalue
    a_vla_dimension vla_dim = {};
    state.vla_dimension_values[&vla_dim] = state.builder->getInt32(4);
    a_type vla_inner_arr = {};
    vla_inner_arr.kind = tk_array;
    vla_inner_arr.variant.array.element_type = &int_type;
    vla_inner_arr.variant.array.bound_is_zero = FALSE;
    vla_inner_arr.variant.array.variant.number_of_elements = 8;
    a_type vla_zero_arr = {};
    vla_zero_arr.kind = tk_array;
    vla_zero_arr.variant.array.element_type = &vla_inner_arr;
    vla_zero_arr.variant.array.bound_is_zero = TRUE;
    a_type vla_outer_arr = {};
    vla_outer_arr.kind = tk_array;
    vla_outer_arr.variant.array.is_vla = TRUE;
    vla_outer_arr.variant.array.has_assoc_vla_dimension = TRUE;
    vla_outer_arr.variant.array.element_type = &vla_zero_arr;
    state.array_to_vla_dim[&vla_outer_arr] = &vla_dim;

    an_expr_node vla_ptr_op = {};
    vla_ptr_op.kind = enk_variable;
    vla_ptr_op.type = &ptr_type;
    vla_ptr_op.variant.variable.ptr = &va_var;
    an_expr_node vla_idx_op = {};
    vla_idx_op.kind = enk_constant;
    vla_idx_op.type = &int_type;
    vla_ptr_op.next = &vla_idx_op;

    an_expr_node vla_sub = {};
    vla_sub.kind = enk_operation;
    vla_sub.type = &vla_outer_arr;
    vla_sub.variant.operation.kind = eok_subscript;
    vla_sub.variant.operation.operands = &vla_ptr_op;
    assert(llvm_lower_lvalue_expression(&vla_sub, &val) == llvm_gen_be_error_t::ok);
    vla_ptr_op.next = nullptr;

    // 5. All target ABIs + builtins + atomics in llvm_lower_call_expression
    const char* target_triples[] = {
      "x86_64-pc-windows-msvc",
      "aarch64-unknown-linux-gnu",
      "i686-unknown-linux-gnu",
      "armv7-unknown-linux-gnueabihf",
      "thumbv7-unknown-linux-gnueabihf",
      "riscv64-unknown-linux-gnu",
      "loongarch64-unknown-linux-gnu",
      "mips64-unknown-linux-gnu",
      "powerpc64le-unknown-linux-gnu",
      "s390x-ibm-linux",
      "sparcv9-sun-solaris",
      "wasm32-unknown-unknown",
      "x86_64-unknown-linux-gnu"
    };

    a_routine ext_fn_rout = {};
    ext_fn_rout.source_corresp.name = (char*)"abi_test_fn";
    ext_fn_rout.type = &rout_ty;
    an_expr_node fn_callee_op = {};
    fn_callee_op.kind = enk_routine;
    fn_callee_op.type = &ptr_rout_ty;
    fn_callee_op.variant.routine.ptr = &ext_fn_rout;
    an_expr_node fn_arg1 = {};
    fn_arg1.kind = enk_constant;
    fn_arg1.type = &int_type;
    fn_callee_op.next = &fn_arg1;
    an_expr_node abi_call_expr = {};
    abi_call_expr.kind = enk_operation;
    abi_call_expr.type = &int_type;
    abi_call_expr.variant.operation.kind = eok_call;
    abi_call_expr.variant.operation.operands = &fn_callee_op;

    int abi_idx = 0;
    static char abi_fnames[16][64];
    for (const char* t : target_triples) {
      snprintf(abi_fnames[abi_idx], sizeof(abi_fnames[abi_idx]), "abi_test_fn_%d", abi_idx);
      ext_fn_rout.source_corresp.name = abi_fnames[abi_idx++];
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), t);
      assert(llvm_lower_call_expression(&abi_call_expr, &val) == llvm_gen_be_error_t::ok);
      // Also test va_start, va_arg, va_end, va_copy on every target triple
      va_var_node.next = nullptr;
      va_op_expr.variant.operation.kind = eok_va_start;
      assert(llvm_lower_expression(&va_op_expr, &va_out) == llvm_gen_be_error_t::ok);
      va_op_expr.variant.operation.kind = eok_va_arg;
      assert(llvm_lower_expression(&va_op_expr, &va_out) == llvm_gen_be_error_t::ok);
      va_op_expr.variant.operation.kind = eok_va_end;
      assert(llvm_lower_expression(&va_op_expr, &va_out) == llvm_gen_be_error_t::ok);
      va_var_node.next = &va_src_node;
      va_op_expr.variant.operation.kind = eok_va_copy;
      assert(llvm_lower_expression(&va_op_expr, &va_out) == llvm_gen_be_error_t::ok);
      va_var_node.next = nullptr;
    }

    // Also test resolve_va_list_ptr when operand is not in local_vars
    an_expr_node va_expr_ptr = {};
    va_expr_ptr.kind = enk_operation;
    va_expr_ptr.type = &ptr_type;
    va_expr_ptr.variant.operation.kind = eok_address_of;
    va_expr_ptr.variant.operation.operands = &va_var_node;
    va_op_expr.variant.operation.operands = &va_expr_ptr;
    va_op_expr.variant.operation.kind = eok_va_end;
    assert(llvm_lower_expression(&va_op_expr, &va_out) == llvm_gen_be_error_t::ok);
    va_op_expr.variant.operation.operands = &va_var_node;

    // Test all builtins in llvm_lower_call_expression
    a_constant c_ord[7] = {};
    an_expr_node n_ord[7] = {};
    for (int i = 0; i < 7; ++i) {
      c_ord[i].source_corresp.decl_position.seq = 10 + i; // evaluates to i
      n_ord[i].kind = enk_constant;
      n_ord[i].type = &int_type;
      n_ord[i].variant.constant.ptr = &c_ord[i];
    }

    an_expr_node i32_arg1 = n_ord[1], i32_arg2 = n_ord[2], i32_arg3 = n_ord[3];
    an_expr_node i8_arg = {};
    i8_arg.kind = enk_constant;
    i8_arg.type = &i8_ty;
    an_expr_node i64_arg = {};
    i64_arg.kind = enk_constant;
    i64_arg.type = &i64_ty;
    an_expr_node u32_arg = {};
    u32_arg.kind = enk_constant;
    u32_arg.type = &u32_ty;
    an_expr_node flt_arg1 = flt_const_node, flt_arg2 = flt_const_node, flt_arg3 = flt_const_node;
    an_expr_node ptr_arg1 = va_var_node, ptr_arg2 = va_var_node, ptr_arg3 = va_var_node;

    for (const char* u_name : {
           "__builtin_sqrt", "__builtin_sin", "__builtin_cos", "__builtin_exp",
           "__builtin_log", "__builtin_round", "__builtin_trunc", "__builtin_floor",
           "__builtin_ceil", "__builtin_cabs"}) {
      assert(call_test_builtin(u_name, &float_type, {&flt_arg1}) == llvm_gen_be_error_t::ok);
    }
    assert(call_test_builtin("__builtin_cabs", &float_type, {&cplx_op1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_pow", &float_type, {&flt_arg1, &flt_arg2}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_copysign", &float_type, {&flt_arg1, &flt_arg2}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_fma", &float_type, {&flt_arg1, &flt_arg2, &flt_arg3}) == llvm_gen_be_error_t::ok);

    for (const char* b_name : {
           "__builtin_bswap32", "__builtin_popcount", "__builtin_constant_p",
           "__builtin_clz", "__builtin_ctz", "__builtin_ffs", "__builtin_clrsb",
           "__builtin_parity",
           "__builtin_frame_address", "__builtin_return_address",
           "__builtin_alloca", "__builtin_assume", "setjmp", "longjmp",
           "__builtin___emutls_get_address", "__builtin_strlen"}) {
      assert(call_test_builtin(b_name, &int_type, {&i32_arg1}) == llvm_gen_be_error_t::ok);
    }
    assert(call_test_builtin("__builtin_launder", &ptr_type, {&ptr_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_dump_struct", &int_type, {&ptr_arg1, &ptr_arg2}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_prefetch", &void_ty, {&ptr_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_prefetch", &void_ty, {&ptr_arg1, &i32_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_prefetch", &void_ty, {&ptr_arg1, &i32_arg1, &i32_arg2}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_clear_cache", &void_ty, {&ptr_arg1, &ptr_arg2}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_readcyclecounter", &i64_ty, {}) == llvm_gen_be_error_t::ok);
    for (const char* fp_cls_name : {
           "__builtin_isnan", "__builtin_isinf", "__builtin_isfinite",
           "__builtin_isnormal", "__builtin_signbit", "__builtin_signbitf"}) {
      assert(call_test_builtin(fp_cls_name, &int_type, {&flt_arg1}) == llvm_gen_be_error_t::ok);
    }
    assert(call_test_builtin("__builtin_fpclassify", &int_type,
                 {&i32_arg1, &i32_arg2, &i32_arg3, &n_ord[4], &n_ord[5], &flt_arg1}) == llvm_gen_be_error_t::ok);
    a_constant c_16 = {};
    c_16.source_corresp.decl_position.seq = 10 + 16;
    an_expr_node n_16 = {};
    n_16.kind = enk_constant;
    n_16.type = &int_type;
    n_16.variant.constant.ptr = &c_16;
    assert(call_test_builtin("__builtin___emutls_get_address", &ptr_type, {&ptr_arg1}) == llvm_gen_be_error_t::ok); // second call reuses existing function
    assert(call_test_builtin("__builtin_expect", &i64_ty, {&i64_arg, &i64_arg}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_expect_with_probability", &int_type, {&i32_arg1, &i32_arg2, &flt_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_alloca_with_align", &ptr_type, {&i32_arg1, &n_16}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_alloca_with_align", &ptr_type, {&i32_arg1, &dummy_op1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_rotateleft32", &int_type, {&i32_arg1, &i32_arg2}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_rotateright32", &int_type, {&i32_arg1, &i32_arg2}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_object_size", &i64_ty, {&ptr_arg1, &i32_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_dynamic_object_size", &i64_ty, {&ptr_arg1, &i32_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_memcpy", &ptr_type, {&ptr_arg1, &ptr_arg2, &i64_arg}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_memmove", &ptr_type, {&ptr_arg1, &ptr_arg2, &i64_arg}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_memset", &ptr_type, {&ptr_arg1, &i32_arg1, &i64_arg}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_strcmp", &int_type, {&ptr_arg1, &ptr_arg2}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_strchr", &ptr_type, {&ptr_arg1, &i32_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_strchr", &ptr_type, {&ptr_arg1, &i8_arg}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_strchr", &ptr_type, {&ptr_arg1, &i64_arg}) == llvm_gen_be_error_t::ok);

    // Overflow builtins (signed & unsigned, add/sub/mul)
    for (const char* ov_name : {
           "__builtin_add_overflow", "__builtin_sub_overflow", "__builtin_mul_overflow",
           "__builtin_sadd_overflow", "__builtin_uadd_overflow",
           "__builtin_ssub_overflow", "__builtin_usub_overflow",
           "__builtin_smul_overflow", "__builtin_umul_overflow"}) {
      assert(call_test_builtin(ov_name, &int_type, {&i32_arg1, &i32_arg2, &ptr_arg1}) == llvm_gen_be_error_t::ok);
      assert(call_test_builtin(ov_name, &int_type, {&u32_arg, &u32_arg, &ptr_arg1}) == llvm_gen_be_error_t::ok);
    }
    assert(call_test_builtin("__builtin_addcll", &u32_ty, {&u32_arg, &u32_arg, &u32_arg, &ptr_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_subcll", &u32_ty, {&u32_arg, &u32_arg, &u32_arg, &ptr_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_addcll", &u32_ty, {&u32_arg}) == llvm_gen_be_error_t::ok);

    // Target builtins: x86 avx2, avx, pause, neon, sve, rvv, vsetvli, vsetvlimax + invalid rejection
    assert(call_test_builtin("__builtin_ia32_avx512_mask", &int_type, {&i32_arg1}) ==
           llvm_gen_be_error_t::builtin_lowering_failed);
    assert(call_test_builtin("__builtin_ia32_avx2_paddb", &int_type, {&i32_arg1, &i32_arg2}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_ia32_avx_vzeroall", &void_ty, {}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_ia32_pause", &void_ty, {}) == llvm_gen_be_error_t::ok);
    for (const char* tb_name : {
           "__builtin_neon_vadd",
           "__builtin_sve_svadd", "__builtin_rvv_vadd"}) {
      assert(call_test_builtin(tb_name, &int_type, {&i32_arg1, &i32_arg2}) == llvm_gen_be_error_t::ok);
    }
    assert(call_test_builtin("__builtin_rvv_vsetvli", &i64_ty, {&i64_arg, &i64_arg, &i64_arg}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_rvv_vsetvli", &int_type, {&i32_arg1, &i32_arg2, &i32_arg3}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_rvv_vsetvlimax", &i64_ty, {&i64_arg, &i64_arg}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_rvv_vsetvlimax", &int_type, {&i32_arg1, &i32_arg2}) == llvm_gen_be_error_t::ok);

    // Atomics and __sync builtins
    for (int i = 0; i < 7; ++i) {
      assert(call_test_builtin("__atomic_thread_fence", &void_ty, {&n_ord[i]}) == llvm_gen_be_error_t::ok);
      assert(call_test_builtin("__atomic_signal_fence", &void_ty, {&n_ord[i]}) == llvm_gen_be_error_t::ok);
      assert(call_test_builtin("__atomic_always_lock_free", &int_type, {&n_ord[i]}) == llvm_gen_be_error_t::ok);
      assert(call_test_builtin("__atomic_load_n", &int_type, {&ptr_arg1, &n_ord[i]}) == llvm_gen_be_error_t::ok);
      assert(call_test_builtin("__atomic_store_n", &void_ty, {&ptr_arg1, &i32_arg1, &n_ord[i]}) == llvm_gen_be_error_t::ok);
      assert(call_test_builtin("__atomic_exchange_n", &int_type, {&ptr_arg1, &i32_arg1, &n_ord[i]}) == llvm_gen_be_error_t::ok);
    }
    assert(call_test_builtin("__atomic_is_lock_free", &int_type, {&n_16}) == llvm_gen_be_error_t::ok);
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "riscv64-unknown-linux-gnu");
    assert(call_test_builtin("__atomic_is_lock_free", &int_type, {&n_16}) == llvm_gen_be_error_t::ok);
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "aarch64-unknown-linux-gnu");
    assert(call_test_builtin("__atomic_is_lock_free", &int_type, {&n_16}) == llvm_gen_be_error_t::ok);
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
    assert(call_test_builtin("__atomic_is_lock_free", &int_type, {&dummy_op1}) == llvm_gen_be_error_t::ok);

    assert(call_test_builtin("__builtin_atomic_wait", &void_ty, {&ptr_arg1, &i32_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_atomic_wait", &void_ty, {&ptr_arg1, &i32_arg1}) == llvm_gen_be_error_t::ok); // reuse existing fn
    assert(call_test_builtin("__builtin_atomic_notify", &void_ty, {&ptr_arg1}) == llvm_gen_be_error_t::ok);

    assert(call_test_builtin("__atomic_load", &void_ty, {&ptr_arg1, &ptr_arg2, &n_ord[2]}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__atomic_store", &void_ty, {&ptr_arg1, &ptr_arg2, &n_ord[3]}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__atomic_exchange", &void_ty, {&ptr_arg1, &ptr_arg2, &ptr_arg3, &n_ord[4]}) == llvm_gen_be_error_t::ok);
    // Also test __atomic_load / __atomic_store / __atomic_exchange on float pointer and aggregate struct pointer
    {
      a_type flt_ptr_t = {}; flt_ptr_t.kind = tk_pointer; flt_ptr_t.size = 8; flt_ptr_t.variant.pointer.type = &float_type;
      state.type_cache[&flt_ptr_t] = llvm::PointerType::getUnqual(*state.context);
      an_expr_node flt_ptr_arg = ptr_arg2; flt_ptr_arg.type = &flt_ptr_t;
      assert(call_test_builtin("__atomic_load", &void_ty, {&ptr_arg1, &flt_ptr_arg, &n_ord[2]}) == llvm_gen_be_error_t::ok);
      assert(call_test_builtin("__atomic_store", &void_ty, {&ptr_arg1, &flt_ptr_arg, &n_ord[3]}) == llvm_gen_be_error_t::ok);
      assert(call_test_builtin("__atomic_exchange", &void_ty, {&ptr_arg1, &flt_ptr_arg, &ptr_arg3, &n_ord[4]}) == llvm_gen_be_error_t::ok);

      a_type agg_ptr_t = {}; agg_ptr_t.kind = tk_pointer; agg_ptr_t.size = 8; agg_ptr_t.variant.pointer.type = &struct_type;
      state.type_cache[&agg_ptr_t] = llvm::PointerType::getUnqual(*state.context);
      an_expr_node agg_ptr_arg = ptr_arg2; agg_ptr_arg.type = &agg_ptr_t;
      assert(call_test_builtin("__atomic_load", &void_ty, {&ptr_arg1, &agg_ptr_arg, &n_ord[2]}) == llvm_gen_be_error_t::ok);
      assert(call_test_builtin("__atomic_store", &void_ty, {&ptr_arg1, &agg_ptr_arg, &n_ord[3]}) == llvm_gen_be_error_t::ok);
      assert(call_test_builtin("__atomic_exchange", &void_ty, {&ptr_arg1, &agg_ptr_arg, &ptr_arg3, &n_ord[4]}) == llvm_gen_be_error_t::ok);
    }
    assert(call_test_builtin("__atomic_compare_exchange_n", &int_type,
                 {&ptr_arg1, &ptr_arg2, &i32_arg1, &i32_arg2, &n_ord[4], &n_ord[2]}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__atomic_compare_exchange_n", &int_type,
                 {&ptr_arg1, &ptr_arg2, &flt_arg1, &i32_arg2, &n_ord[5], &n_ord[5]}) == llvm_gen_be_error_t::ok);

    assert(call_test_builtin("__sync_synchronize", &void_ty, {}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__sync_lock_test_and_set", &int_type, {&ptr_arg1, &i32_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__sync_lock_release", &void_ty, {&ptr_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__sync_val_compare_and_swap", &int_type, {&ptr_arg1, &i32_arg1, &i32_arg2}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__sync_bool_compare_and_swap", &int_type, {&ptr_arg1, &i32_arg1, &i32_arg2}) == llvm_gen_be_error_t::ok);

    for (const char* rmw_name : {
           "__atomic_fetch_add", "__atomic_fetch_sub", "__atomic_fetch_and",
           "__atomic_fetch_or", "__atomic_fetch_xor", "__atomic_fetch_nand",
           "__atomic_fetch_min", "__atomic_fetch_max",
           "__sync_add_and_fetch", "__sync_sub_and_fetch", "__sync_and_and_fetch",
           "__sync_or_and_fetch", "__sync_xor_and_fetch", "__sync_nand_and_fetch"}) {
      assert(call_test_builtin(rmw_name, &int_type, {&ptr_arg1, &i32_arg1, &n_ord[5]}) == llvm_gen_be_error_t::ok);
    }
    for (const char* fp_rmw_name : {
           "__atomic_fetch_add", "__atomic_fetch_sub",
           "__atomic_fetch_min", "__atomic_fetch_max",
           "__sync_add_and_fetch", "__sync_sub_and_fetch"}) {
      assert(call_test_builtin(fp_rmw_name, &float_type, {&ptr_arg1, &flt_arg1, &n_ord[5]}) == llvm_gen_be_error_t::ok);
    }

    // 6. Debug location saving in llvm_lower_expression & emit_dbg_value_for_variable in eok_assign
    llvm::DIBuilder di_b(*state.module);
    llvm_gen_be_debug_state_t dbg_st = {};
    dbg_st.builder = &di_b;
    llvm::DIFile* di_file = di_b.createFile("expr.c", ".");
    dbg_st.compile_unit = di_b.createCompileUnit(llvm::dwarf::DW_LANG_C99, di_file, "edg", false, "", 0);
    dbg_st.scope_stack.push_back(di_file);
    state.dbg_state = &dbg_st;

    an_expr_node assign_expr = {};
    assign_expr.kind = enk_operation;
    assign_expr.type = &int_type;
    assign_expr.position.seq = 10;
    assign_expr.variant.operation.kind = eok_assign;
    dummy_op1.is_lvalue = TRUE;
    dummy_op1.next = &i32_arg1;
    i32_arg1.next = nullptr;
    assign_expr.variant.operation.operands = &dummy_op1;
    assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::ok);

    assign_expr.position.seq = 777; // null loc
    assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::ok);
    assign_expr.position.seq = 888; // error in get_di_location (ignored by DILocSaver)
    assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::ok);
    dummy_var.alignment = 99; // error in emit_dbg_value_for_variable
    assert(llvm_lower_expression(&assign_expr, &val) != llvm_gen_be_error_t::ok);
    dummy_var.alignment = 0;
    state.dbg_state = nullptr;

    // 7. LValue expression coverage (globals, dot_static, pm_field, indirect, errors)
    a_variable ext_gvar = {};
    ext_gvar.source_corresp.name = (char*)"ext_global_var";
    ext_gvar.type = &int_type;
    an_expr_node gvar_node = {};
    gvar_node.kind = enk_variable;
    gvar_node.type = &int_type;
    gvar_node.variant.variable.ptr = &ext_gvar;
    assert(llvm_lower_lvalue_expression(&gvar_node, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_lvalue_expression(&gvar_node, &val) == llvm_gen_be_error_t::ok); // existing global
    a_variable unnamed_var = {};
    gvar_node.variant.variable.ptr = &unnamed_var;
    assert(llvm_lower_lvalue_expression(&gvar_node, &val) == llvm_gen_be_error_t::unsupported_expr);
    gvar_node.variant.variable.ptr = nullptr;
    assert(llvm_lower_lvalue_expression(&gvar_node, &val) == llvm_gen_be_error_t::unsupported_expr);
    gvar_node.variant.variable.ptr = &ext_gvar;

    // Non-VLA subscript lvalue & rvalue
    vla_ptr_op.next = &vla_idx_op;
    vla_sub.type = &int_type;
    assert(llvm_lower_lvalue_expression(&vla_sub, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&vla_sub, &val) == llvm_gen_be_error_t::ok);
    vla_sub.type = &vol_int_ty;
    assert(llvm_lower_expression(&vla_sub, &val) == llvm_gen_be_error_t::ok);
    vla_ptr_op.next = nullptr;

    // dot_field, dot_static, pm_field, indirect lvalues & rvalues
    a_field norm_f = {};
    norm_f.offset = 4;
    norm_f.type = &int_type;
    an_expr_node f_node = {};
    f_node.kind = enk_field;
    f_node.variant.field.ptr = &norm_f;
    vla_ptr_op.next = &f_node;
    an_expr_node df_expr = {};
    df_expr.kind = enk_operation;
    df_expr.type = &int_type;
    df_expr.variant.operation.kind = eok_dot_field;
    df_expr.variant.operation.operands = &vla_ptr_op;
    assert(llvm_lower_lvalue_expression(&df_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&df_expr, &val) == llvm_gen_be_error_t::ok);
    df_expr.type = &vol_int_ty;
    assert(llvm_lower_expression(&df_expr, &val) == llvm_gen_be_error_t::ok);
    f_node.variant.field.ptr = &bf_f;
    df_expr.type = &int_type;
    assert(llvm_lower_expression(&df_expr, &val) == llvm_gen_be_error_t::ok); // bitfield load via dot_field
    vla_ptr_op.next = nullptr;
    assert(llvm_lower_lvalue_expression(&df_expr, &val) == llvm_gen_be_error_t::unsupported_expr);

    vla_ptr_op.next = &gvar_node;
    df_expr.variant.operation.kind = eok_dot_static;
    assert(llvm_lower_lvalue_expression(&df_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_lvalue_expression(&pm_field_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&pm_field_expr, &val) == llvm_gen_be_error_t::ok);
    vla_ptr_op.next = nullptr;
    df_expr.variant.operation.kind = eok_indirect;
    assert(llvm_lower_lvalue_expression(&df_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&df_expr, &val) == llvm_gen_be_error_t::ok);
    df_expr.variant.operation.kind = eok_add;
    assert(llvm_lower_lvalue_expression(&df_expr, &val) == llvm_gen_be_error_t::unsupported_expr);
    assert(llvm_lower_lvalue_expression(&i32_arg1, &val) == llvm_gen_be_error_t::unsupported_expr);

    // 8. Arithmetic expression full operator & type matrix
    assert(llvm_lower_lvalue_expression(nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_lvalue_expression(&gvar_node, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_arithmetic_expression(nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_logical_expression(nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_cast_expression(nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_call_expression(nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_expression(&gvar_node, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_compound_assignment(&gvar_node, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_compound_assignment(&gvar_node, &val) == llvm_gen_be_error_t::invalid_argument);

    ptr_type.variant.pointer.type = &int_type;
    an_expr_node arith_op1 = {}, arith_op2 = {};
    arith_op1.kind = enk_constant;
    arith_op2.kind = enk_constant;
    arith_op1.next = &arith_op2;
    an_expr_node arith_e = {};
    arith_e.kind = enk_operation;
    arith_e.variant.operation.operands = &arith_op1;

    for (a_type_ptr aty : {&int_type, &u32_ty, &float_type}) {
      arith_op1.type = aty;
      arith_op2.type = aty;
      arith_e.type = aty;
      for (an_expr_operator_kind k : {
             eok_negate, eok_unary_plus, eok_add, eok_subtract, eok_multiply,
             eok_divide, eok_remainder, eok_xconj}) {
        arith_e.variant.operation.kind = k;
        assert(llvm_lower_expression(&arith_e, &val) == llvm_gen_be_error_t::ok);
      }
    }
    for (a_type_ptr ity : {&int_type, &u32_ty}) {
      arith_op1.type = ity;
      arith_op2.type = ity;
      arith_e.type = ity;
      for (an_expr_operator_kind k : {
             eok_complement, eok_and, eok_or, eok_xor, eok_shiftl, eok_shiftr}) {
        arith_e.variant.operation.kind = k;
        assert(llvm_lower_expression(&arith_e, &val) == llvm_gen_be_error_t::ok);
      }
    }
    arith_op1.type = &float_type;
    arith_op2.type = &float_type;
    arith_e.type = &cplx_ty;
    for (an_expr_operator_kind k : {eok_fjadd, eok_jfadd, eok_fjsubtract, eok_jfsubtract}) {
      arith_e.variant.operation.kind = k;
      assert(llvm_lower_expression(&arith_e, &val) == llvm_gen_be_error_t::ok);
    }
    arith_e.type = &float_type;
    for (an_expr_operator_kind k : {eok_jmultiply, eok_jdivide}) {
      arith_e.variant.operation.kind = k;
      assert(llvm_lower_expression(&arith_e, &val) == llvm_gen_be_error_t::ok);
    }
    // Pointer arithmetic
    arith_op1.type = &ptr_type;
    arith_op2.type = &int_type;
    arith_e.type = &ptr_type;
    arith_e.variant.operation.kind = eok_padd;
    assert(llvm_lower_expression(&arith_e, &val) == llvm_gen_be_error_t::ok);
    arith_e.variant.operation.kind = eok_psubtract;
    assert(llvm_lower_expression(&arith_e, &val) == llvm_gen_be_error_t::ok);
    arith_op2.type = &ptr_type;
    arith_e.type = &i64_ty;
    arith_e.variant.operation.kind = eok_pdiff;
    assert(llvm_lower_expression(&arith_e, &val) == llvm_gen_be_error_t::ok);

    // Non-pointer fallbacks in padd/psubtract/pdiff
    arith_op1.type = &int_type;
    arith_op2.type = &int_type;
    arith_e.variant.operation.kind = eok_padd;
    assert(llvm_lower_expression(&arith_e, &val) == llvm_gen_be_error_t::ok);
    arith_e.variant.operation.kind = eok_psubtract;
    assert(llvm_lower_expression(&arith_e, &val) == llvm_gen_be_error_t::ok);
    arith_e.variant.operation.kind = eok_pdiff;
    assert(llvm_lower_expression(&arith_e, &val) == llvm_gen_be_error_t::unsupported_expr);
    arith_e.variant.operation.kind = eok_assign;
    assert(llvm_lower_arithmetic_expression(&arith_e, &val) == llvm_gen_be_error_t::unsupported_expr);

    // 9. Logical & relational expression matrix
    a_type vec_ty = {}, fvec_ty = {}, bool_ty = {};
    state.type_cache[&vec_ty] = llvm::FixedVectorType::get(llvm::Type::getInt32Ty(*state.context), 4);
    state.type_cache[&fvec_ty] = llvm::FixedVectorType::get(llvm::Type::getFloatTy(*state.context), 4);
    state.type_cache[&bool_ty] = llvm::Type::getInt1Ty(*state.context);

    an_expr_node log_e = {};
    log_e.kind = enk_operation;
    log_e.type = &int_type;
    log_e.variant.operation.operands = &arith_op1;
    for (an_expr_operator_kind k : {eok_land, eok_lor}) {
      log_e.variant.operation.kind = k;
      assert(llvm_lower_expression(&log_e, &val) == llvm_gen_be_error_t::ok);
    }
    for (a_type_ptr lty : {&int_type, &u32_ty, &float_type, &ptr_type}) {
      arith_op1.type = lty;
      arith_op2.type = lty;
      for (an_expr_operator_kind k : {eok_eq, eok_ne, eok_lt, eok_le, eok_gt, eok_ge, eok_spaceship}) {
        log_e.variant.operation.kind = k;
        log_e.type = &int_type;
        assert(llvm_lower_expression(&log_e, &val) == llvm_gen_be_error_t::ok);
      }
    }
    // Vector comparison SExt
    arith_op1.type = &vec_ty;
    arith_op2.type = &vec_ty;
    log_e.type = &vec_ty;
    log_e.variant.operation.kind = eok_eq;
    assert(llvm_lower_expression(&log_e, &val) == llvm_gen_be_error_t::ok);

    // Default unsupported binary op in llvm_lower_logical_expression
    arith_op1.type = &int_type;
    arith_op2.type = &int_type;
    log_e.type = &int_type;
    log_e.variant.operation.kind = eok_add;
    assert(llvm_lower_logical_expression(&log_e, &val) == llvm_gen_be_error_t::unsupported_expr);

    // Unary eok_not (int, i1, vector)
    arith_op1.next = nullptr;
    log_e.variant.operation.kind = eok_not;
    assert(llvm_lower_expression(&log_e, &val) == llvm_gen_be_error_t::ok);
    log_e.type = &bool_ty;
    assert(llvm_lower_expression(&log_e, &val) == llvm_gen_be_error_t::ok);
    arith_op1.type = &vec_ty;
    log_e.type = &vec_ty;
    assert(llvm_lower_expression(&log_e, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&pm_cmp_expr, &val) == llvm_gen_be_error_t::ok);
    arith_op1.next = &arith_op2;

    // 10. Cast expression matrix
    an_expr_node cast_e = {};
    cast_e.kind = enk_operation;
    cast_e.variant.operation.kind = eok_cast;
    cast_e.variant.operation.operands = &arith_op1;
    arith_op1.next = nullptr;
    std::pair<a_type_ptr, a_type_ptr> cast_pairs[] = {
      {&int_type, &int_type},
      {&ptr_type, &i64_ty},
      {&i64_ty, &ptr_type},
      {&float_type, &double_ty},
      {&double_ty, &float_type},
      {&float_type, &int_type},
      {&float_type, &u32_ty},
      {&int_type, &float_type},
      {&u32_ty, &float_type},
      {&int_type, &i64_ty},
      {&u32_ty, &i64_ty},
      {&i64_ty, &i8_ty},
      {&int_type, &cplx_double_ty},
      {&vec_ty, &fvec_ty},
    };
    for (const auto& cp : cast_pairs) {
      arith_op1.type = cp.first;
      cast_e.type = cp.second;
      assert(llvm_lower_expression(&cast_e, &val) == llvm_gen_be_error_t::ok);
    }
    cast_e.variant.operation.kind = eok_lvalue_cast;
    assert(llvm_lower_expression(&cast_e, &val) == llvm_gen_be_error_t::ok);

    // 11. Call expression: sret, byval, 2-reg aggregate split, virtual call, invoke, convertvector, shufflevector
    a_field big_f = {};
    big_f.type = &i64_ty;
    a_type big_struct_ty = {};
    big_struct_ty.kind = tk_struct;
    big_struct_ty.size = 24;
    big_struct_ty.variant.class_struct_union.field_list = &big_f;
    llvm::Type* big_llvm_ty = nullptr;
    assert(get_llvm_type(&big_struct_ty, &big_llvm_ty) == llvm_gen_be_error_t::ok);

    a_variable big_var = {};
    big_var.type = &big_struct_ty;
    state.local_vars[&big_var] = state.builder->CreateAlloca(big_llvm_ty);
    an_expr_node big_var_node = {};
    big_var_node.kind = enk_variable;
    big_var_node.type = &big_struct_ty;
    big_var_node.variant.variable.ptr = &big_var;

    a_variable ic_var2 = {};
    ic_var2.type = &ic_struct_ty;
    state.local_vars[&ic_var2] = state.builder->CreateAlloca(ic_llvm_ty);
    an_expr_node ic_var_node = {};
    ic_var_node.kind = enk_variable;
    ic_var_node.type = &ic_struct_ty;
    ic_var_node.variant.variable.ptr = &ic_var2;

    a_param_type p_big = {}, p_ic = {};
    p_big.type = &big_struct_ty;
    p_ic.type = &ic_struct_ty;
    p_big.next = &p_ic;
    a_routine_type_supplement sret_rts = {};
    sret_rts.param_type_list = &p_big;
    a_type sret_rout_ty = {};
    sret_rout_ty.kind = tk_routine;
    sret_rout_ty.variant.routine.return_type = &big_struct_ty;
    sret_rout_ty.variant.routine.extra_info = &sret_rts;

    a_routine sret_rout = {};
    sret_rout.source_corresp.name = (char*)"sret_byval_fn";
    sret_rout.type = &sret_rout_ty;
    an_expr_node sret_callee = {};
    sret_callee.kind = enk_routine;
    sret_callee.type = &sret_rout_ty;
    sret_callee.variant.routine.ptr = &sret_rout;
    sret_callee.next = &big_var_node;
    big_var_node.next = &ic_var_node;
    ic_var_node.next = nullptr;
    an_expr_node sret_call = {};
    sret_call.kind = enk_operation;
    sret_call.type = &big_struct_ty;
    sret_call.variant.operation.kind = eok_call;
    sret_call.variant.operation.operands = &sret_callee;
    assert(llvm_lower_call_expression(&sret_call, &val) == llvm_gen_be_error_t::ok);

    // 8-byte struct coerced to i64 on Win64 and unpacked on return
    a_param_type p_s8 = {};
    p_s8.type = &ic_struct_ty;
    a_routine_type_supplement s8_rts = {};
    s8_rts.param_type_list = &p_s8;
    a_type s8_rout_ty = {};
    s8_rout_ty.kind = tk_routine;
    s8_rout_ty.variant.routine.return_type = &ic_struct_ty;
    s8_rout_ty.variant.routine.extra_info = &s8_rts;
    a_routine s8_rout = {};
    s8_rout.source_corresp.name = (char*)"s8_win64_fn";
    s8_rout.type = &s8_rout_ty;
    an_expr_node s8_callee = {};
    s8_callee.kind = enk_routine;
    s8_callee.type = &s8_rout_ty;
    s8_callee.variant.routine.ptr = &s8_rout;
    s8_callee.next = &ic_var_node;
    ic_var_node.next = nullptr;
    an_expr_node s8_call = {};
    s8_call.kind = enk_operation;
    s8_call.type = &ic_struct_ty;
    s8_call.variant.operation.kind = eok_call;
    s8_call.variant.operation.operands = &s8_callee;
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    assert(llvm_lower_call_expression(&s8_call, &val) == llvm_gen_be_error_t::ok);
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");

    // Virtual call on Itanium and MSVC, plus invoke with landing pad & attributes
    a_routine vcall_rout = {};
    vcall_rout.source_corresp.name = (char*)"vcall_test_fn";
    an_expr_node vcall_callee = {};
    vcall_callee.kind = enk_routine;
    vcall_callee.variant.routine.ptr = &vcall_rout;
    vcall_callee.next = &ptr_arg1;
    ptr_arg1.next = nullptr;
    an_expr_node vcall_expr = {};
    vcall_expr.kind = enk_operation;
    vcall_expr.type = &int_type;
    vcall_expr.variant.operation.kind = eok_call;
    vcall_expr.variant.operation.is_virtual_call = TRUE;
    vcall_expr.variant.operation.operands = &vcall_callee;
    assert(llvm_lower_call_expression(&vcall_expr, &val) == llvm_gen_be_error_t::ok);
    vcall_expr.variant.operation.kind = eok_dot_member_call;
    assert(llvm_lower_expression(&vcall_expr, &val) == llvm_gen_be_error_t::ok);
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    assert(llvm_lower_call_expression(&vcall_expr, &val) == llvm_gen_be_error_t::ok);
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");

    llvm::Function* fn = state.builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* dummy_lpad = llvm::BasicBlock::Create(*state.context, "lpad", fn);
    state.current_landing_pads.push_back(dummy_lpad);
    assert(llvm_lower_call_expression(&sret_call, &val) == llvm_gen_be_error_t::ok);
    state.current_landing_pads.pop_back();

    // Target builtin with non-void callee_ty (hits overload_types.push_back(callee_ty->getReturnType()))
    a_routine tb_ov_rout = {};
    tb_ov_rout.source_corresp.name = (char*)"__builtin_sve_add_nxv4i32";
    tb_ov_rout.type = &rout_ty;
    an_expr_node tb_ov_callee = {};
    tb_ov_callee.kind = enk_routine;
    tb_ov_callee.type = &rout_ty;
    tb_ov_callee.variant.routine.ptr = &tb_ov_rout;
    tb_ov_callee.next = &i32_arg1;
    i32_arg1.next = &i32_arg2;
    i32_arg2.next = nullptr;
    an_expr_node tb_ov_call = {};
    tb_ov_call.kind = enk_operation;
    tb_ov_call.type = &int_type;
    tb_ov_call.variant.operation.kind = eok_call;
    tb_ov_call.variant.operation.operands = &tb_ov_callee;
    assert(llvm_lower_call_expression(&tb_ov_call, &val) == llvm_gen_be_error_t::ok);
    i32_arg1.next = nullptr;

    // Indirect call without callee_ty
    an_expr_node ind_callee = ptr_arg1;
    ind_callee.type = &ptr_type;
    ind_callee.next = &i32_arg1;
    i32_arg1.next = nullptr;
    an_expr_node ind_call = {};
    ind_call.kind = enk_operation;
    ind_call.type = &int_type;
    ind_call.variant.operation.kind = eok_call;
    ind_call.variant.operation.operands = &ind_callee;
    assert(llvm_lower_call_expression(&ind_call, &val) == llvm_gen_be_error_t::ok);

    // Builtins: shufflevector, convertvector, trap, unreachable
    an_expr_node iv_arg1 = {}, iv_arg2 = {}, fv_arg1 = {};
    iv_arg1.kind = enk_constant;
    iv_arg1.type = &vec_ty;
    iv_arg2 = iv_arg1;
    fv_arg1.kind = enk_constant;
    fv_arg1.type = &fvec_ty;
    assert(call_test_builtin("__builtin_shufflevector", &vec_ty, {&iv_arg1, &iv_arg2, &n_ord[0], &dummy_op1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_convertvector", &vec_ty, {&iv_arg1, &iv_arg2}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_convertvector", &fvec_ty, {&fv_arg1, &fv_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_convertvector", &fvec_ty, {&iv_arg1, &fv_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_convertvector", &vec_ty, {&fv_arg1, &iv_arg1}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_trap", &void_ty, {}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_unreachable", &void_ty, {}) == llvm_gen_be_error_t::ok);
    llvm::BasicBlock* post_unreach_bb = llvm::BasicBlock::Create(*state.context, "post.unreach", fn);
    state.builder->SetInsertPoint(post_unreach_bb);

    // Vector subscript rvalue (CreateExtractElement)
    iv_arg1.next = &i32_arg1;
    i32_arg1.next = nullptr;
    an_expr_node vec_sub = {};
    vec_sub.kind = enk_operation;
    vec_sub.type = &int_type;
    vec_sub.variant.operation.kind = eok_subscript;
    vec_sub.variant.operation.operands = &iv_arg1;
    assert(llvm_lower_expression(&vec_sub, &val) == llvm_gen_be_error_t::ok);
    iv_arg1.next = nullptr;

    // 12. Remaining enk_* kinds in llvm_lower_expression
    assert(llvm_lower_expression(nullptr, &val) == llvm_gen_be_error_t::ok);
    // is_lvalue with non-lvalue op
    arith_op1.type = &int_type;
    arith_op2.type = &int_type;
    arith_op1.next = &arith_op2;
    arith_e.type = &int_type;
    arith_e.is_lvalue = TRUE;
    arith_e.variant.operation.kind = eok_add;
    assert(llvm_lower_expression(&arith_e, &val) == llvm_gen_be_error_t::ok);
    arith_e.is_lvalue = FALSE;

    // enk_braced_init_list: void, struct, and 1-element scalar
    braced_expr.type = &void_ty;
    assert(llvm_lower_expression(&braced_expr, &val) == llvm_gen_be_error_t::ok);
    braced_expr.type = &ic_struct_ty;
    assert(llvm_lower_expression(&braced_expr, &val) == llvm_gen_be_error_t::ok);
    braced_expr.type = &int_type;
    braced_expr.variant.braced_init_list = &i32_arg1;
    i32_arg1.next = nullptr;
    assert(llvm_lower_expression(&braced_expr, &val) == llvm_gen_be_error_t::ok);

    // enk_reuse_value: cache hit, dik_zero, dik_constant, dik_constructor, dik_none
    assert(llvm_lower_expression(&reuse_expr, &val) == llvm_gen_be_error_t::ok); // cache hit
    a_dynamic_init rv_zero = {}, rv_con = {}, rv_ctor = {}, rv_bad = {};
    rv_zero.kind = dik_zero;
    reuse_expr.variant.reused_value_init = &rv_zero;
    assert(llvm_lower_expression(&reuse_expr, &val) == llvm_gen_be_error_t::ok);
    rv_con.kind = dik_constant;
    rv_con.variant.constant.ptr = &c_ord[1];
    reuse_expr.variant.reused_value_init = &rv_con;
    assert(llvm_lower_expression(&reuse_expr, &val) == llvm_gen_be_error_t::ok);
    rv_ctor.kind = dik_constructor;
    reuse_expr.variant.reused_value_init = &rv_ctor;
    assert(llvm_lower_expression(&reuse_expr, &val) == llvm_gen_be_error_t::ok);
    rv_bad.kind = dik_none;
    reuse_expr.variant.reused_value_init = &rv_bad;
    assert(llvm_lower_expression(&reuse_expr, &val) == llvm_gen_be_error_t::unsupported_expr);

    // enk_c11_generic, enk_fold, enk_pack_index, enk_routine (null)
    an_expr_node gen_e = {};
    gen_e.kind = enk_c11_generic;
    gen_e.type = &int_type;
    assert(llvm_lower_expression(&gen_e, &val) == llvm_gen_be_error_t::unsupported_expr);
    gen_e.variant.c11_generic.result = &i32_arg1;
    assert(llvm_lower_expression(&gen_e, &val) == llvm_gen_be_error_t::ok);

    an_expr_node fold_e = {};
    fold_e.kind = enk_fold;
    fold_e.type = &int_type;
    assert(llvm_lower_expression(&fold_e, &val) == llvm_gen_be_error_t::unsupported_expr);
    i32_arg1.next = &i32_arg2;
    i32_arg2.next = nullptr;
    fold_e.variant.fold.operands = &i32_arg1;
    for (a_token_kind tk : {tok_plus, tok_minus, tok_star, tok_ampersand, tok_or, tok_excl_or, tok_comma, tok_assign}) {
      fold_e.variant.fold.operator_token = tk;
      assert(llvm_lower_expression(&fold_e, &val) == llvm_gen_be_error_t::ok);
    }
    flt_arg1.next = &flt_arg2;
    flt_arg2.next = nullptr;
    fold_e.type = &float_type;
    fold_e.variant.fold.operands = &flt_arg1;
    for (a_token_kind tk : {tok_plus, tok_minus, tok_star}) {
      fold_e.variant.fold.operator_token = tk;
      assert(llvm_lower_expression(&fold_e, &val) == llvm_gen_be_error_t::ok);
    }
    i32_arg1.next = nullptr;
    flt_arg1.next = nullptr;

    an_expr_node pack_e = {};
    pack_e.kind = enk_pack_index;
    pack_e.type = &int_type;
    assert(llvm_lower_expression(&pack_e, &val) == llvm_gen_be_error_t::unsupported_expr);
    i32_arg1.next = &i32_arg2;
    pack_e.variant.pack_index.expr = &i32_arg1;
    pack_e.variant.pack_index.index_expr = &n_ord[1];
    assert(llvm_lower_expression(&pack_e, &val) == llvm_gen_be_error_t::ok);
    i32_arg1.next = nullptr;

    an_expr_node null_rout_e = {};
    null_rout_e.kind = enk_routine;
    null_rout_e.type = &ptr_type;
    assert(llvm_lower_expression(&null_rout_e, &val) == llvm_gen_be_error_t::ok);

    // eok_question (int & void), eok_assign (bitfield & >16B struct memcpy & store & null rhs), eok_dynamic_cast, eok_pm_base_class_cast, eok_dot_pm_call
    an_expr_node q_e = {};
    q_e.kind = enk_operation;
    q_e.type = &int_type;
    q_e.variant.operation.kind = eok_question;
    i32_arg1.next = &i32_arg2;
    i32_arg2.next = &i32_arg3;
    i32_arg3.next = nullptr;
    q_e.variant.operation.operands = &i32_arg1;
    assert(llvm_lower_expression(&q_e, &val) == llvm_gen_be_error_t::ok);
    q_e.type = &void_ty;
    assert(llvm_lower_expression(&q_e, &val) == llvm_gen_be_error_t::ok);
    // eok_question with terminated true arm and terminated false arm
    an_expr_node q_throw_arm = {};
    q_throw_arm.kind = enk_throw;
    q_throw_arm.type = &void_ty;
    q_e.type = &int_type;
    i32_arg1.next = &q_throw_arm;
    q_throw_arm.next = &i32_arg3;
    i32_arg3.next = nullptr;
    assert(llvm_lower_expression(&q_e, &val) == llvm_gen_be_error_t::ok);
    i32_arg1.next = &i32_arg2;
    i32_arg2.next = &q_throw_arm;
    q_throw_arm.next = nullptr;
    assert(llvm_lower_expression(&q_e, &val) == llvm_gen_be_error_t::ok);
    i32_arg1.next = nullptr;
    i32_arg2.next = nullptr;

    // Bitfield assign, >16B struct assign, and unsupported assign (null rhs)
    df_expr.is_lvalue = TRUE;
    df_expr.variant.operation.kind = eok_dot_field;
    vla_ptr_op.next = &f_node;
    f_node.variant.field.ptr = &bf_f;
    df_expr.next = &i32_arg1;
    assign_expr.variant.operation.operands = &df_expr;
    assign_expr.position.seq = 0;
    assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::ok);
    df_expr.next = nullptr;
    assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::unsupported_expr);
    vla_ptr_op.next = nullptr;

    an_expr_node big_lhs = big_var_node;
    big_lhs.is_lvalue = TRUE;
    an_expr_node big_rhs_load = big_var_node;
    big_rhs_load.is_lvalue = FALSE;
    big_lhs.next = &big_rhs_load;
    assign_expr.type = &big_struct_ty;
    assign_expr.variant.operation.operands = &big_lhs;
    assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::ok); // MemCpy path
    an_expr_node big_rhs_const = {};
    big_rhs_const.kind = enk_constant;
    big_rhs_const.type = &big_struct_ty;
    big_lhs.next = &big_rhs_const;
    assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::ok); // Store path

    pm_op1.next = &pm_op2;
    assert(llvm_lower_expression(&pm_cast_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&pm_call_expr, &val) == llvm_gen_be_error_t::ok);
    an_expr_node dcast_e = {};
    dcast_e.kind = enk_operation;
    dcast_e.type = &ptr_type;
    dcast_e.variant.operation.kind = eok_dynamic_cast;
    dcast_e.variant.operation.operands = &ptr_arg1;
    assert(llvm_lower_expression(&dcast_e, &val) == llvm_gen_be_error_t::ok);

    // enk_await, enk_builtin_operation (offsetof, bit_cast, has_attribute), enk_statement, enk_typeid
    an_expr_node await_e = {};
    await_e.kind = enk_await;
    await_e.type = &int_type;
    assert(llvm_lower_expression(&await_e, &val) == llvm_gen_be_error_t::ok);

    an_expr_node bok_e = {};
    bok_e.kind = enk_builtin_operation;
    bok_e.type = &i64_ty;
    bok_e.variant.builtin_operation.kind = bok_offsetof;
    an_expr_node bok_op1 = i32_arg1;
    f_node.variant.field.ptr = &norm_f;
    an_expr_node bok_df = df_expr;
    bok_df.is_lvalue = FALSE;
    vla_ptr_op.next = &f_node;
    bok_op1.next = &f_node;
    f_node.next = &bok_df;
    bok_df.next = nullptr;
    bok_e.variant.builtin_operation.operands = &bok_op1;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);
    f_node.next = nullptr;

    bok_e.variant.builtin_operation.kind = bok_builtin_bit_cast;
    bok_e.type = &float_type;
    bok_op1.next = &i32_arg1;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);
    bok_e.type = &int_type;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);
    bok_e.type = &ic_struct_ty;
    bok_op1.next = &ic_var_node;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);

    // bok_builtin_has_attribute
    an_attribute attr1 = {};
    attr1.kind = ak_align;
    ext_gvar.source_corresp.attributes = &attr1;
    ext_fn_rout.source_corresp.attributes = &attr1;
    norm_f.source_corresp.attributes = &attr1;
    int_type.source_corresp.attributes = &attr1;
    c_ord[0].source_corresp.attributes = &attr1;

    an_expr_node paren_subj = {};
    paren_subj.kind = enk_operation;
    paren_subj.variant.operation.kind = eok_parens;
    paren_subj.variant.operation.operands = &gvar_node;
    paren_subj.next = &n_ord[0];
    bok_e.variant.builtin_operation.kind = bok_builtin_has_attribute;
    bok_e.variant.builtin_operation.operands = &paren_subj;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);

    df_expr.next = &n_ord[0];
    bok_e.variant.builtin_operation.operands = &df_expr;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);
    fn_callee_op.next = &n_ord[0];
    bok_e.variant.builtin_operation.operands = &fn_callee_op;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);
    an_expr_node type_op_subj = {};
    type_op_subj.kind = enk_type_operand;
    type_op_subj.variant.type_operand.type = &int_type;
    type_op_subj.next = &n_ord[0];
    bok_e.variant.builtin_operation.operands = &type_op_subj;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);
    arith_e.next = &n_ord[0];
    bok_e.variant.builtin_operation.operands = &arith_e;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);
    arith_e.next = nullptr;
    fn_callee_op.next = &fn_arg1;
    df_expr.next = nullptr;
    vla_ptr_op.next = nullptr;

    bok_e.variant.builtin_operation.kind = static_cast<a_builtin_operation_kind>(250);
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::unsupported_expr);

    // enk_statement, enk_typeid, enk_vla_dealloc
    a_statement stmt_ok = {}, stmt_err = {};
    stmt_ok.kind = stmk_expr;
    stmt_err.kind = stmk_return;
    an_expr_node stmt_e = {};
    stmt_e.kind = enk_statement;
    stmt_e.type = &int_type;
    stmt_e.variant.statement = &stmt_ok;
    assert(llvm_lower_expression(&stmt_e, &val) == llvm_gen_be_error_t::ok);
    stmt_e.type = &void_ty;
    assert(llvm_lower_expression(&stmt_e, &val) == llvm_gen_be_error_t::ok);
    stmt_e.variant.statement = &stmt_err;
    assert(llvm_lower_expression(&stmt_e, &val) != llvm_gen_be_error_t::ok);

    an_expr_node tid_e = {};
    tid_e.kind = enk_typeid;
    tid_e.type = &int_type;
    type_op_subj.next = nullptr;
    tid_e.variant.typeid_info.type_with_opt_expr = &type_op_subj;
    assert(llvm_lower_expression(&tid_e, &val) == llvm_gen_be_error_t::ok);

    an_expr_node vla_dealloc_e = {};
    vla_dealloc_e.kind = enk_vla_dealloc;
    vla_dealloc_e.variant.vla_variable = &dummy_var;
    state.vla_saved_stacks[&dummy_var] = bf_ptr;
    assert(llvm_lower_expression(&vla_dealloc_e, &val) == llvm_gen_be_error_t::ok);

    // 13. enk_throw (rethrow & throw, Itanium & MSVC, with/without landing pad & destructor)
    an_expr_node throw_e = {};
    throw_e.kind = enk_throw;
    throw_e.type = &void_ty;
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok); // Itanium rethrow
    state.current_landing_pads.push_back(dummy_lpad);
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok); // Itanium rethrow with invoke
    state.current_landing_pads.pop_back();
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok); // MSVC rethrow

    a_routine exc_dtor = {};
    exc_dtor.source_corresp.name = (char*)"exc_dtor_fn";
    exc_dtor.type = &rout_ty;
    a_dynamic_init exc_dip = {};
    exc_dip.kind = dik_zero;
    exc_dip.destructor = &exc_dtor;
    a_throw_supplement t_supp = {};
    t_supp.type = &int_type;
    t_supp.dynamic_init = &exc_dip;
    throw_e.variant.throw_info = &t_supp;
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok); // MSVC throw
    state.current_landing_pads.push_back(dummy_lpad);
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok); // MSVC throw with invoke & cached _TI

    // MSVC throw of derived class with base classes (null type, named/unnamed, size 0/8)
    a_type base_cls1 = {}, base_cls2 = {}, derived_exc_cls = {};
    base_cls1.kind = tk_struct; base_cls1.size = 8; base_cls1.source_corresp.name = (char*)"BaseExc1";
    base_cls2.kind = tk_class; base_cls2.size = 0; base_cls2.source_corresp.name = nullptr;
    a_base_class bc_null = {}, bc_dir1 = {}, bc_dir2 = {};
    bc_null.type = nullptr; bc_null.next = &bc_dir1;
    bc_dir1.type = &base_cls1; bc_dir1.offset = 0; bc_dir1.next = &bc_dir2;
    bc_dir2.type = &base_cls2; bc_dir2.offset = 8; bc_dir2.next = nullptr;
    a_class_type_supplement derived_exc_supp = {};
    derived_exc_supp.base_classes = &bc_null;
    derived_exc_cls.kind = tk_class;
    derived_exc_cls.size = 16;
    derived_exc_cls.source_corresp.name = (char*)"DerivedExc";
    derived_exc_cls.variant.class_struct_union.extra_info = &derived_exc_supp;
    t_supp.type = &derived_exc_cls;
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok);
    a_type struct_no_supp_exc = {};
    struct_no_supp_exc.kind = tk_struct;
    struct_no_supp_exc.source_corresp.name = (char*)"StructNoSuppExc";
    t_supp.type = &struct_no_supp_exc;
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok);
    t_supp.type = &int_type;
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
    exc_dtor.source_corresp.name = (char*)"itanium_exc_dtor_fn";
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok); // Itanium throw with invoke & new dtor
    state.current_landing_pads.pop_back();
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok); // Itanium throw with call & existing dtor

    // 14. enk_temp_init (has_temporary_lifetime), enk_object_lifetime (destructions & cleanups), enk_new_delete (cookies & exception cleanup)
    temp_dtor_dip.has_temporary_lifetime = TRUE;
    a_dynamic_init dtor_list_item = {};
    dtor_list_item.destructor = &temp_dtor_routine;
    dtor_list_item.variable = &dummy_var;
    obj_lifetime.destructions = &dtor_list_item;
    obj_lifetime_expr.variant.object_lifetime.expr = &temp_dtor_expr;
    assert(llvm_lower_expression(&obj_lifetime_expr, &val) == llvm_gen_be_error_t::ok);

    // Array new[] and delete[] with nontrivial destructor cookie (size == 777)
    a_type cookie_ty = int_type;
    cookie_ty.size = 777;
    nd_array.type = &cookie_ty;
    assert(llvm_lower_expression(&new_array_expr, &val) == llvm_gen_be_error_t::ok);
    delete_array_nd.type = &cookie_ty;
    assert(llvm_lower_expression(&delete_array_expr, &val) == llvm_gen_be_error_t::ok);
    a_dynamic_init del_dtor_dip = {};
    del_dtor_dip.destructor = &temp_dtor_routine;
    delete_array_nd.dynamic_init = &del_dtor_dip;
    assert(llvm_lower_expression(&delete_array_expr, &val) == llvm_gen_be_error_t::ok);
    del_dtor_dip.destructor = nullptr;
    assert(llvm_lower_expression(&delete_array_expr, &val) == llvm_gen_be_error_t::ok);
    del_dtor_dip.destructor = &temp_dtor_routine;
    a_type bad_cookie_ty = bad_ty;
    bad_cookie_ty.size = 777;
    delete_array_nd.type = &bad_cookie_ty;
    assert(llvm_lower_expression(&delete_array_expr, &val) == llvm_gen_be_error_t::unsupported_type);
    delete_array_nd.type = &cookie_ty;
    delete_array_nd.dynamic_init = nullptr;

    // Single-object delete with dynamic_init (destructor != nullptr and destructor == nullptr)
    delete_nd.dynamic_init = &del_dtor_dip;
    assert(llvm_lower_expression(&delete_expr, &val) == llvm_gen_be_error_t::ok);
    del_dtor_dip.destructor = nullptr;
    assert(llvm_lower_expression(&delete_expr, &val) == llvm_gen_be_error_t::ok);
    delete_nd.dynamic_init = nullptr;

    // new with dynamic_init and freeing_of_storage_on_exception (new cleanup function)
    a_routine custom_del_routine = {};
    custom_del_routine.source_corresp.name = (char*)"custom_op_delete";
    custom_del_routine.type = &int_type;
    a_dynamic_init new_free_dip = {};
    new_free_dip.destructor = &custom_del_routine;
    nd.dynamic_init = &exc_dip;
    nd.freeing_of_storage_on_exception = &new_free_dip;
    assert(llvm_lower_expression(&new_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&new_expr, &val) == llvm_gen_be_error_t::ok); // second call when parent_fn->hasPersonalityFn() is true
    nd.dynamic_init = nullptr;
    nd.freeing_of_storage_on_exception = nullptr;

    // 15. llvm_lower_compound_assignment full operator & type matrix
    an_expr_node ca_lhs = dummy_op1;
    ca_lhs.is_lvalue = TRUE;
    an_expr_node ca_rhs = i32_arg1;
    ca_lhs.next = &ca_rhs;
    an_expr_node ca_expr = {};
    ca_expr.kind = enk_operation;
    ca_expr.type = &int_type;
    ca_expr.variant.operation.operands = &ca_lhs;

    for (a_type_ptr cty : {&int_type, &u32_ty, &float_type}) {
      ca_lhs.type = cty;
      ca_rhs.type = cty;
      ca_expr.type = cty;
      for (an_expr_operator_kind k : {
             eok_pre_incr, eok_post_incr, eok_pre_decr, eok_post_decr,
             eok_add_assign, eok_subtract_assign, eok_multiply_assign,
             eok_divide_assign, eok_remainder_assign}) {
        ca_lhs.next = (k == eok_pre_incr || k == eok_post_incr || k == eok_pre_decr || k == eok_post_decr) ? nullptr : &ca_rhs;
        ca_expr.variant.operation.kind = k;
        assert(llvm_lower_expression(&ca_expr, &val) == llvm_gen_be_error_t::ok);
      }
    }
    for (a_type_ptr ity : {&int_type, &u32_ty}) {
      ca_lhs.type = ity;
      ca_rhs.type = ity;
      ca_expr.type = ity;
      ca_lhs.next = &ca_rhs;
      for (an_expr_operator_kind k : {
             eok_and_assign, eok_or_assign, eok_xor_assign,
             eok_shiftl_assign, eok_shiftr_assign}) {
        ca_expr.variant.operation.kind = k;
        assert(llvm_lower_expression(&ca_expr, &val) == llvm_gen_be_error_t::ok);
      }
    }
    // padd_assign, psubtract_assign, bassign
    ca_lhs = va_var_node;
    ca_lhs.is_lvalue = TRUE;
    ca_lhs.type = &ptr_type;
    ca_rhs.type = &int_type;
    ca_lhs.next = &ca_rhs;
    ca_expr.type = &ptr_type;
    ca_expr.variant.operation.kind = eok_padd_assign;
    assert(llvm_lower_expression(&ca_expr, &val) == llvm_gen_be_error_t::ok);
    ca_expr.variant.operation.kind = eok_psubtract_assign;
    assert(llvm_lower_expression(&ca_expr, &val) == llvm_gen_be_error_t::ok);
    ca_rhs = va_var_node;
    ca_lhs.next = &ca_rhs;
    ca_expr.variant.operation.kind = eok_bassign;
    assert(llvm_lower_expression(&ca_expr, &val) == llvm_gen_be_error_t::ok);

    // Bitfield & volatile compound assignment + unsupported op
    df_expr.is_lvalue = TRUE;
    df_expr.type = &int_type;
    vla_ptr_op.next = &f_node;
    f_node.variant.field.ptr = &bf_f;
    df_expr.next = nullptr;
    ca_expr.variant.operation.operands = &df_expr;
    ca_expr.variant.operation.kind = eok_pre_incr;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) == llvm_gen_be_error_t::ok);
    ca_lhs = dummy_op1;
    ca_lhs.type = &vol_int_ty;
    ca_lhs.next = nullptr;
    ca_expr.variant.operation.operands = &ca_lhs;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) == llvm_gen_be_error_t::ok);
    ca_expr.variant.operation.kind = eok_eq;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) == llvm_gen_be_error_t::unsupported_expr);

    // 16. Remaining switch arms, edge cases, and error percolation
    a_constant bad_con = {};
    bad_con.source_corresp.decl_position.seq = 888; // evaluate_constant returns unsupported_expr
    an_expr_node bad_op = {};
    bad_op.kind = enk_constant;
    bad_op.type = &int_type;
    bad_op.variant.constant.ptr = &bad_con;

    // Null out_val checks
    assert(llvm_lower_arithmetic_expression(&arith_e, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_logical_expression(&log_e, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_cast_expression(&cast_e, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_call_expression(&abi_call_expr, nullptr) == llvm_gen_be_error_t::invalid_argument);

    // lower_binary_operands & load_from_lvalue_ptr error percolation
    bad_op.next = &i32_arg1;
    arith_e.variant.operation.operands = &bad_op;
    arith_e.variant.operation.kind = eok_add;
    assert(llvm_lower_arithmetic_expression(&arith_e, &val) != llvm_gen_be_error_t::ok);
    i32_arg1.next = &bad_op;
    bad_op.next = nullptr;
    arith_e.variant.operation.operands = &i32_arg1;
    assert(llvm_lower_arithmetic_expression(&arith_e, &val) != llvm_gen_be_error_t::ok);
    i32_arg1.next = nullptr;

    // load_from_lvalue_ptr error (bad_ty on enk_variable rvalue)
    an_expr_node bad_var_rval = dummy_op1;
    bad_var_rval.is_lvalue = FALSE;
    bad_var_rval.type = &bad_ty;
    assert(llvm_lower_expression(&bad_var_rval, &val) != llvm_gen_be_error_t::ok);

    // Remaining switch cases in llvm_lower_lvalue_expression & llvm_lower_expression
    pm_field_expr.variant.operation.kind = eok_pm_points_to_field;
    assert(llvm_lower_lvalue_expression(&pm_field_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&pm_field_expr, &val) == llvm_gen_be_error_t::ok);
    pm_cast_expr.variant.operation.kind = eok_pm_derived_class_cast;
    assert(llvm_lower_expression(&pm_cast_expr, &val) == llvm_gen_be_error_t::ok);
    dcast_e.variant.operation.kind = eok_ref_dynamic_cast;
    assert(llvm_lower_expression(&dcast_e, &val) == llvm_gen_be_error_t::ok);
    pm_call_expr.variant.operation.kind = eok_points_to_pm_call;
    assert(llvm_lower_expression(&pm_call_expr, &val) == llvm_gen_be_error_t::ok);

    vla_ptr_op.next = &f_node;
    f_node.variant.field.ptr = &norm_f;
    df_expr.variant.operation.kind = eok_points_to_field;
    df_expr.type = &int_type;
    df_expr.is_lvalue = TRUE;
    assert(llvm_lower_expression(&df_expr, &val) == llvm_gen_be_error_t::ok);
    df_expr.is_lvalue = FALSE;
    assert(llvm_lower_expression(&df_expr, &val) == llvm_gen_be_error_t::ok);
    vla_ptr_op.next = &gvar_node;
    df_expr.variant.operation.kind = eok_points_to_static;
    assert(llvm_lower_lvalue_expression(&df_expr, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&df_expr, &val) == llvm_gen_be_error_t::ok);

    // is_lvalue = TRUE with eok_subscript and eok_indirect
    vla_ptr_op.next = &vla_idx_op;
    vla_sub.is_lvalue = TRUE;
    assert(llvm_lower_expression(&vla_sub, &val) == llvm_gen_be_error_t::ok);
    vla_sub.is_lvalue = FALSE;
    vla_ptr_op.next = nullptr;
    df_expr.variant.operation.kind = eok_indirect;
    df_expr.is_lvalue = TRUE;
    assert(llvm_lower_expression(&df_expr, &val) == llvm_gen_be_error_t::ok);
    df_expr.is_lvalue = FALSE;

    // CLI unsupported operators
    for (an_expr_operator_kind ck : {eok_handle_to, eok_handle_to_box, eok_box, eok_unbox, eok_unbox_lvalue, eok_cli_subscript}) {
      df_expr.variant.operation.kind = ck;
      assert(llvm_lower_expression(&df_expr, &val) == llvm_gen_be_error_t::unsupported_expr);
    }

    // enk_yield
    await_e.kind = enk_yield;
    assert(llvm_lower_expression(&await_e, &val) == llvm_gen_be_error_t::ok);

    // enk_fold with tok_and_and and tok_or_or
    i32_arg1.next = &i32_arg2;
    i32_arg2.next = nullptr;
    fold_e.type = &int_type;
    fold_e.variant.fold.operands = &i32_arg1;
    for (a_token_kind tk : {tok_and_and, tok_or_or}) {
      fold_e.variant.fold.operator_token = tk;
      assert(llvm_lower_expression(&fold_e, &val) == llvm_gen_be_error_t::ok);
    }
    i32_arg1.next = nullptr;

    // enk_reuse_value with nullptr, dik_class_result_via_ctor, dik_nonconstant_aggregate, dik_bitwise_copy
    reuse_expr.variant.reused_value_init = nullptr;
    assert(llvm_lower_expression(&reuse_expr, &val) == llvm_gen_be_error_t::invalid_argument);
    a_dynamic_init rv_via_ctor = {}, rv_agg = {}, rv_bcopy = {};
    rv_via_ctor.kind = dik_class_result_via_ctor;
    rv_via_ctor.variant.expression = &i32_arg1;
    reuse_expr.variant.reused_value_init = &rv_via_ctor;
    assert(llvm_lower_expression(&reuse_expr, &val) == llvm_gen_be_error_t::ok);
    rv_agg.kind = dik_nonconstant_aggregate;
    reuse_expr.variant.reused_value_init = &rv_agg;
    assert(llvm_lower_expression(&reuse_expr, &val) == llvm_gen_be_error_t::ok);
    rv_bcopy.kind = dik_bitwise_copy;
    reuse_expr.variant.reused_value_init = &rv_bcopy;
    assert(llvm_lower_expression(&reuse_expr, &val) == llvm_gen_be_error_t::ok);

    // enk_pack_index with null index_expr and non-constant index_expr
    i32_arg1.next = &i32_arg2;
    pack_e.variant.pack_index.expr = &i32_arg1;
    pack_e.variant.pack_index.index_expr = nullptr;
    assert(llvm_lower_expression(&pack_e, &val) == llvm_gen_be_error_t::ok);
    pack_e.variant.pack_index.index_expr = &dummy_op1;
    assert(llvm_lower_expression(&pack_e, &val) == llvm_gen_be_error_t::ok);
    pack_e.variant.pack_index.index_expr = &n_ord[5]; // clamps to last element
    assert(llvm_lower_expression(&pack_e, &val) == llvm_gen_be_error_t::ok);
    i32_arg1.next = nullptr;

    // eok_question with i1 condition
    an_expr_node bool_cond = {};
    bool_cond.kind = enk_constant;
    bool_cond.type = &bool_ty;
    bool_cond.next = &i32_arg1;
    i32_arg1.next = &i32_arg2;
    i32_arg2.next = nullptr;
    q_e.type = &int_type;
    q_e.variant.operation.operands = &bool_cond;
    assert(llvm_lower_expression(&q_e, &val) == llvm_gen_be_error_t::ok);
    i32_arg1.next = nullptr;

    // Target builtin cached lookup + non-constant atomic ordering + 32-bit __atomic_is_lock_free
    assert(call_test_builtin("__builtin_ia32_pause", &void_ty, {}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin_ia32_pause", &void_ty, {}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__atomic_load_n", &int_type, {&ptr_arg1, &dummy_op1}) == llvm_gen_be_error_t::ok);
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "i686-unknown-linux-gnu");
    assert(call_test_builtin("__atomic_is_lock_free", &int_type, {&n_16}) == llvm_gen_be_error_t::ok);
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");

    // bok_builtin_operation null operands & bok_builtin_has_attribute with null arg2 & unmatched attr
    bok_e.variant.builtin_operation.operands = nullptr;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::invalid_argument);
    bok_e.variant.builtin_operation.kind = bok_builtin_has_attribute;
    gvar_node.next = nullptr;
    bok_e.variant.builtin_operation.operands = &gvar_node;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);
    an_attribute attr2 = {};
    attr2.kind = ak_noreturn;
    c_ord[1].source_corresp.attributes = &attr2;
    gvar_node.next = &n_ord[1];
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);
    gvar_node.next = nullptr;

    // enk_throw with type_size == 0, dynamic_init == nullptr, named type, and null dtor
    a_type zero_sz_ty = int_type;
    zero_sz_ty.size = 0;
    zero_sz_ty.source_corresp.name = (char*)"EmptyExc";
    t_supp.type = &zero_sz_ty;
    t_supp.dynamic_init = nullptr;
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok);
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok);
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
    exc_dip.destructor = nullptr;
    t_supp.dynamic_init = &exc_dip;
    assert(run_test_throw(&throw_e, fn) == llvm_gen_be_error_t::ok);

    // enk_vla_dealloc with unmapped variable, enk_temp_init with null dynamic_init, enk_object_lifetime with null inner_expr & reused_values
    vla_dealloc_e.variant.vla_variable = &unnamed_var;
    assert(llvm_lower_expression(&vla_dealloc_e, &val) == llvm_gen_be_error_t::ok);
    an_expr_node empty_temp_e = {};
    empty_temp_e.kind = enk_temp_init;
    empty_temp_e.type = &int_type;
    assert(llvm_lower_expression(&empty_temp_e, &val) == llvm_gen_be_error_t::ok);

    state.reused_values[&dtor_list_item] = bf_ptr;
    obj_lifetime_expr.variant.object_lifetime.expr = nullptr;
    assert(llvm_lower_expression(&obj_lifetime_expr, &val) == llvm_gen_be_error_t::ok);
    dtor_list_item.destructor = nullptr;
    assert(llvm_lower_expression(&obj_lifetime_expr, &val) == llvm_gen_be_error_t::ok);

    // enk_new_delete edge cases (null nd, null routine, unnamed routine, placement_new custom, dynamic_init without exception cleanup)
    an_expr_node bad_nd_e = {};
    bad_nd_e.kind = enk_new_delete;
    bad_nd_e.type = &ptr_type;
    assert(llvm_lower_expression(&bad_nd_e, &val) == llvm_gen_be_error_t::invalid_argument);
    a_new_delete_supplement custom_nd = {};
    custom_nd.is_new = TRUE;
    custom_nd.type = &int_type;
    bad_nd_e.variant.new_delete = &custom_nd;
    assert(llvm_lower_expression(&bad_nd_e, &val) == llvm_gen_be_error_t::unsupported_expr);
    a_routine custom_new_rt = {};
    custom_nd.routine = &custom_new_rt;
    assert(llvm_lower_expression(&bad_nd_e, &val) == llvm_gen_be_error_t::unsupported_expr);
    custom_new_rt.source_corresp.name = (char*)"my_placement_new";
    custom_nd.placement_new = TRUE;
    custom_nd.arg = &ptr_arg1;
    ptr_arg1.next = nullptr;
    custom_nd.dynamic_init = &exc_dip;
    assert(llvm_lower_expression(&bad_nd_e, &val) == llvm_gen_be_error_t::ok);

    // Additional branch and error percolation tests
    // 1. enk_routine with FunctionType
    a_routine fn_ty_rt = {};
    fn_ty_rt.source_corresp.name = (char*)"fn_with_rout_ty";
    fn_ty_rt.type = &rout_ty;
    an_expr_node fn_ty_rt_e = {};
    fn_ty_rt_e.kind = enk_routine;
    fn_ty_rt_e.type = &ptr_type;
    fn_ty_rt_e.variant.routine.ptr = &fn_ty_rt;
    assert(llvm_lower_expression(&fn_ty_rt_e, &val) == llvm_gen_be_error_t::ok);

    // 2. eok_assign with dbg_state != nullptr and op1 != enk_variable
    state.dbg_state = &dbg_st;
    vla_ptr_op.next = &f_node;
    f_node.variant.field.ptr = &norm_f;
    df_expr.variant.operation.kind = eok_dot_field;
    df_expr.type = &int_type;
    df_expr.is_lvalue = TRUE;
    df_expr.next = &i32_arg1;
    i32_arg1.next = nullptr;
    assign_expr.type = &int_type;
    assign_expr.variant.operation.operands = &df_expr;
    assert(llvm_lower_expression(&assign_expr, &val) == llvm_gen_be_error_t::ok);
    state.dbg_state = nullptr;

    // 3. bok_builtin_bit_cast with scalar src and aggregate dest
    bok_op1.next = &i32_arg1;
    bok_e.variant.builtin_operation.kind = bok_builtin_bit_cast;
    bok_e.variant.builtin_operation.operands = &bok_op1;
    bok_e.type = &ic_struct_ty;
    assert(llvm_lower_expression(&bok_e, &val) == llvm_gen_be_error_t::ok);

    // 4. enk_new_delete when parent_fn->hasPersonalityFn() is already true
    new_expr.variant.new_delete->is_new = TRUE;
    new_expr.variant.new_delete->arg = nullptr;
    assert(llvm_lower_expression(&new_expr, &val) == llvm_gen_be_error_t::ok);

    // 5. llvm_lower_lvalue_expression error percolation
    a_variable bad_gvar = {};
    bad_gvar.source_corresp.name = (char*)"bad_gvar_ty";
    an_expr_node bad_gvar_e = {};
    bad_gvar_e.kind = enk_variable;
    bad_gvar_e.type = &bad_ty;
    bad_gvar_e.variant.variable.ptr = &bad_gvar;
    assert(llvm_lower_lvalue_expression(&bad_gvar_e, &val) != llvm_gen_be_error_t::ok);

    an_expr_node bad_lval = {};
    bad_lval.kind = enk_variable;
    bad_lval.type = &int_type;
    an_expr_node real_e = {};
    real_e.kind = enk_operation;
    real_e.type = &int_type;
    real_e.variant.operation.kind = eok_real_part;
    real_e.variant.operation.operands = &bad_lval;
    assert(llvm_lower_lvalue_expression(&real_e, &val) != llvm_gen_be_error_t::ok);
    an_expr_node local_bad_ty_var = dummy_op1;
    local_bad_ty_var.type = &bad_ty;
    real_e.variant.operation.operands = &local_bad_ty_var;
    assert(llvm_lower_lvalue_expression(&real_e, &val) != llvm_gen_be_error_t::ok);

    bad_op.next = &gvar_node;
    df_expr.variant.operation.kind = eok_dot_static;
    df_expr.variant.operation.operands = &bad_op;
    assert(llvm_lower_lvalue_expression(&df_expr, &val) != llvm_gen_be_error_t::ok);
    df_expr.variant.operation.kind = eok_indirect;
    assert(llvm_lower_lvalue_expression(&df_expr, &val) != llvm_gen_be_error_t::ok);

    vla_ptr_op.next = &bad_op;
    bad_op.next = nullptr;
    assert(llvm_lower_lvalue_expression(&vla_sub, &val) != llvm_gen_be_error_t::ok);
    vla_ptr_op.next = &vla_idx_op;
    vla_sub.type = &bad_ty;
    assert(llvm_lower_lvalue_expression(&vla_sub, &val) != llvm_gen_be_error_t::ok);
    vla_sub.type = &vla_outer_arr;
    vla_inner_arr.variant.array.element_type = &bad_ty;
    assert(llvm_lower_lvalue_expression(&vla_sub, &val) != llvm_gen_be_error_t::ok);
    vla_inner_arr.variant.array.element_type = &int_type;

    // 6. llvm_lower_arithmetic_expression & llvm_lower_logical_expression & llvm_lower_cast_expression error percolation
    a_type ptr_to_bad_ty = {};
    ptr_to_bad_ty.kind = tk_pointer;
    ptr_to_bad_ty.size = 8;
    ptr_to_bad_ty.variant.pointer.type = &bad_ty;
    ptr_arg1.type = &ptr_to_bad_ty;
    ptr_arg1.next = &i32_arg1;
    arith_e.variant.operation.kind = eok_padd;
    arith_e.variant.operation.operands = &ptr_arg1;
    assert(llvm_lower_arithmetic_expression(&arith_e, &val) != llvm_gen_be_error_t::ok);
    ptr_arg1.type = &ptr_type;
    ptr_arg1.next = nullptr;

    log_e.type = &bad_ty;
    log_e.variant.operation.kind = eok_eq;
    log_e.variant.operation.operands = &i32_arg1;
    i32_arg1.next = &i32_arg2;
    assert(llvm_lower_logical_expression(&log_e, &val) != llvm_gen_be_error_t::ok);
    log_e.type = &int_type;
    log_e.variant.operation.kind = eok_land;
    bad_op.next = &i32_arg2;
    log_e.variant.operation.operands = &bad_op;
    assert(llvm_lower_logical_expression(&log_e, &val) != llvm_gen_be_error_t::ok);
    i32_arg1.next = &bad_op;
    bad_op.next = nullptr;
    log_e.variant.operation.operands = &i32_arg1;
    assert(llvm_lower_logical_expression(&log_e, &val) != llvm_gen_be_error_t::ok);
    log_e.variant.operation.kind = eok_eq;
    assert(llvm_lower_logical_expression(&log_e, &val) != llvm_gen_be_error_t::ok);
    i32_arg1.next = nullptr;

    cast_e.variant.operation.operands = &bad_op;
    assert(llvm_lower_cast_expression(&cast_e, &val) != llvm_gen_be_error_t::ok);
    cast_e.variant.operation.operands = &i32_arg1;
    cast_e.type = &bad_ty;
    assert(llvm_lower_cast_expression(&cast_e, &val) != llvm_gen_be_error_t::ok);

    // 7. llvm_lower_call_expression error percolation
    a_type bad_rout_ty = {};
    bad_rout_ty.kind = tk_routine;
    bad_rout_ty.variant.routine.return_type = &bad_ty;
    fn_callee_op.type = &bad_rout_ty;
    assert(llvm_lower_call_expression(&abi_call_expr, &val) != llvm_gen_be_error_t::ok);
    fn_callee_op.type = &ptr_rout_ty;
    fn_callee_op.next = &bad_op;
    bad_op.next = nullptr;
    assert(llvm_lower_call_expression(&abi_call_expr, &val) != llvm_gen_be_error_t::ok);
    fn_callee_op.next = &fn_arg1;

    vcall_callee.next = nullptr;
    assert(llvm_lower_call_expression(&vcall_expr, &val) != llvm_gen_be_error_t::ok);
    vcall_callee.next = &ptr_arg1;
    vcall_expr.type = &bad_ty;
    assert(llvm_lower_call_expression(&vcall_expr, &val) != llvm_gen_be_error_t::ok);
    vcall_expr.type = &int_type;

    ind_call.variant.operation.operands = &bad_op;
    assert(llvm_lower_call_expression(&ind_call, &val) != llvm_gen_be_error_t::ok);
    a_type ref_bad_rout_ty = {};
    ref_bad_rout_ty.kind = tk_pointer;
    ref_bad_rout_ty.size = 555; // is_reference_type == TRUE -> skipped by top unwrap, caught by get_llvm_function_type
    ref_bad_rout_ty.variant.pointer.type = &bad_rout_ty;
    ind_callee.type = &ref_bad_rout_ty;
    ind_callee.next = nullptr;
    ind_call.variant.operation.operands = &ind_callee;
    assert(llvm_lower_call_expression(&ind_call, &val) != llvm_gen_be_error_t::ok);
    ind_callee.type = &ptr_type;
    ind_call.type = &bad_ty;
    assert(llvm_lower_call_expression(&ind_call, &val) != llvm_gen_be_error_t::ok);
    ind_call.type = &int_type;

    // Direct call with op1->type = nullptr (uses f->getFunctionType()) and expr->type = &bad_ty (hits line 1551)
    an_expr_node direct_no_ty_callee = fn_ty_rt_e;
    direct_no_ty_callee.type = nullptr;
    direct_no_ty_callee.next = &i32_arg1;
    i32_arg1.next = nullptr;
    an_expr_node direct_bad_ret_call = {};
    direct_bad_ret_call.kind = enk_operation;
    direct_bad_ret_call.type = &bad_ty;
    direct_bad_ret_call.variant.operation.kind = eok_call;
    direct_bad_ret_call.variant.operation.operands = &direct_no_ty_callee;
    assert(llvm_lower_call_expression(&direct_bad_ret_call, &val) != llvm_gen_be_error_t::ok);

    a_routine b_rt = {};
    an_expr_node b_callee = {};
    b_callee.kind = enk_routine;
    b_callee.variant.routine.ptr = &b_rt;
    an_expr_node b_call = {};
    b_call.kind = enk_operation;
    b_call.variant.operation.kind = eok_call;
    b_call.variant.operation.operands = &b_callee;
    an_expr_node bad_ptr_arg = ptr_arg1;
    bad_ptr_arg.type = &ptr_to_bad_ty;
    b_rt.source_corresp.name = (char*)"__builtin_convertvector";
    b_call.type = &bad_ty;
    b_callee.next = &iv_arg1;
    iv_arg1.next = nullptr;
    assert(llvm_lower_call_expression(&b_call, &val) != llvm_gen_be_error_t::ok);
    b_rt.source_corresp.name = (char*)"__atomic_load_n";
    b_callee.next = &ptr_arg1;
    ptr_arg1.next = &n_ord[0];
    n_ord[0].next = nullptr;
    assert(llvm_lower_call_expression(&b_call, &val) != llvm_gen_be_error_t::ok);
    b_rt.source_corresp.name = (char*)"__atomic_load";
    b_call.type = &void_ty;
    ptr_arg1.next = &bad_ptr_arg;
    bad_ptr_arg.next = &n_ord[0];
    assert(llvm_lower_call_expression(&b_call, &val) != llvm_gen_be_error_t::ok);
    b_rt.source_corresp.name = (char*)"__atomic_store";
    assert(llvm_lower_call_expression(&b_call, &val) != llvm_gen_be_error_t::ok);
    b_rt.source_corresp.name = (char*)"__atomic_exchange";
    bad_ptr_arg.next = &ptr_arg3;
    ptr_arg3.next = &n_ord[0];
    assert(llvm_lower_call_expression(&b_call, &val) != llvm_gen_be_error_t::ok);
    ptr_arg1.next = nullptr;
    bad_ptr_arg.next = nullptr;
    ptr_arg3.next = nullptr;

    // 8. llvm_lower_expression error percolation
    braced_expr.type = &bad_ty;
    assert(llvm_lower_expression(&braced_expr, &val) != llvm_gen_be_error_t::ok);
    braced_expr.type = &ic_struct_ty;
    braced_expr.variant.braced_init_list = &bad_op;
    assert(llvm_lower_expression(&braced_expr, &val) != llvm_gen_be_error_t::ok);

    a_dynamic_init rv_err1 = {}, rv_err2 = {}, rv_err3 = {}, rv_err4 = {};
    rv_err1.kind = dik_expression;
    rv_err1.variant.expression = &bad_op;
    reuse_expr.type = &int_type;
    reuse_expr.variant.reused_value_init = &rv_err1;
    assert(llvm_lower_expression(&reuse_expr, &val) != llvm_gen_be_error_t::ok);
    rv_err2.kind = dik_zero;
    reuse_expr.type = &bad_ty;
    reuse_expr.variant.reused_value_init = &rv_err2;
    assert(llvm_lower_expression(&reuse_expr, &val) != llvm_gen_be_error_t::ok);
    reuse_expr.type = &int_type;
    rv_err3.kind = dik_constant;
    rv_err3.variant.constant.ptr = &bad_con;
    reuse_expr.variant.reused_value_init = &rv_err3;
    assert(llvm_lower_expression(&reuse_expr, &val) != llvm_gen_be_error_t::ok);
    a_dynamic_init bad_dip = {};
    bad_dip.kind = dik_none;
    rv_err4.kind = dik_bitwise_copy;
    rv_err4.variable = &bad_gvar;
    reuse_expr.variant.reused_value_init = &rv_err4;
    assert(llvm_lower_expression(&reuse_expr, &val) != llvm_gen_be_error_t::ok);

    fold_e.variant.fold.operands = &bad_op;
    assert(llvm_lower_expression(&fold_e, &val) != llvm_gen_be_error_t::ok);
    pack_e.variant.pack_index.expr = &i32_arg1;
    pack_e.variant.pack_index.index_expr = &bad_op;
    assert(llvm_lower_expression(&pack_e, &val) != llvm_gen_be_error_t::ok);

    an_expr_node bad_con_ty_e = {};
    bad_con_ty_e.kind = enk_constant;
    bad_con_ty_e.type = &bad_ty;
    assert(llvm_lower_expression(&bad_con_ty_e, &val) != llvm_gen_be_error_t::ok);

    an_expr_node bad_rout_e = {};
    bad_rout_e.kind = enk_routine;
    bad_rout_e.type = &bad_ty;
    assert(llvm_lower_expression(&bad_rout_e, &val) != llvm_gen_be_error_t::ok);
    a_routine err_rt = {};
    err_rt.source_corresp.name = (char*)"err_rt_fn";
    err_rt.type = &bad_ty;
    bad_rout_e.type = &ptr_type;
    bad_rout_e.variant.routine.ptr = &err_rt;
    assert(llvm_lower_expression(&bad_rout_e, &val) != llvm_gen_be_error_t::ok);

    assert(llvm_lower_expression(&bad_lval, &val) != llvm_gen_be_error_t::ok);

    // eok_subscript, eok_dot_field, eok_dot_vacuous_destructor_call, eok_real_part rvalue errors
    bad_op.next = &i32_arg1;
    vla_sub.type = &int_type;
    vla_sub.variant.operation.operands = &bad_op;
    assert(llvm_lower_expression(&vla_sub, &val) != llvm_gen_be_error_t::ok);
    i32_arg1.next = &i32_arg2;
    vla_sub.variant.operation.operands = &i32_arg1; // non-pointer -> lvalue expression fails
    assert(llvm_lower_expression(&vla_sub, &val) != llvm_gen_be_error_t::ok);

    df_expr.is_lvalue = FALSE;
    df_expr.variant.operation.kind = eok_dot_field;
    df_expr.variant.operation.operands = &i32_arg1; // non-pointer -> lvalue expression fails
    assert(llvm_lower_expression(&df_expr, &val) != llvm_gen_be_error_t::ok);

    an_expr_node vac_e = {};
    vac_e.kind = enk_operation;
    vac_e.type = &void_ty;
    vac_e.variant.operation.kind = eok_dot_vacuous_destructor_call;
    vac_e.variant.operation.operands = &bad_op;
    assert(llvm_lower_expression(&vac_e, &val) != llvm_gen_be_error_t::ok);

    real_e.is_lvalue = FALSE;
    real_e.variant.operation.operands = &bad_op;
    assert(llvm_lower_expression(&real_e, &val) != llvm_gen_be_error_t::ok);

    // eok_va_copy resolve_va_list_ptr errors
    va_op_expr.variant.operation.kind = eok_va_copy;
    va_op_expr.variant.operation.operands = &bad_op;
    assert(llvm_lower_expression(&va_op_expr, &va_out) != llvm_gen_be_error_t::ok);
    va_var_node.next = &bad_op;
    va_op_expr.variant.operation.operands = &va_var_node;
    assert(llvm_lower_expression(&va_op_expr, &va_out) != llvm_gen_be_error_t::ok);
    va_var_node.next = nullptr;

    // eok_question errors
    bad_op.next = &i32_arg1;
    i32_arg1.next = &i32_arg2;
    i32_arg2.next = nullptr;
    q_e.variant.operation.operands = &bad_op;
    assert(llvm_lower_expression(&q_e, &val) != llvm_gen_be_error_t::ok);
    bool_cond.next = &bad_op;
    bad_op.next = &i32_arg2;
    q_e.variant.operation.operands = &bool_cond;
    assert(llvm_lower_expression(&q_e, &val) != llvm_gen_be_error_t::ok);
    bool_cond.next = &i32_arg1;
    i32_arg1.next = &bad_op;
    bad_op.next = nullptr;
    assert(llvm_lower_expression(&q_e, &val) != llvm_gen_be_error_t::ok);
    i32_arg1.next = &i32_arg2;
    q_e.type = &bad_ty;
    assert(llvm_lower_expression(&q_e, &val) != llvm_gen_be_error_t::ok);
    i32_arg1.next = nullptr;

    // eok_assign errors
    bad_op.next = &i32_arg1;
    assign_expr.variant.operation.operands = &bad_op;
    assert(llvm_lower_expression(&assign_expr, &val) != llvm_gen_be_error_t::ok);
    vla_ptr_op.next = &i32_arg1;
    assign_expr.variant.operation.operands = &vla_ptr_op;
    assign_expr.type = &bad_ty;
    assert(llvm_lower_expression(&assign_expr, &val) != llvm_gen_be_error_t::ok);
    vla_ptr_op.next = nullptr;

    // bok_builtin_bit_cast & enk_statement & enk_throw & enk_temp_init & enk_object_lifetime & enk_new_delete errors
    bok_op1.next = &bad_op;
    bok_e.variant.builtin_operation.kind = bok_builtin_bit_cast;
    bok_e.variant.builtin_operation.operands = &bok_op1;
    assert(llvm_lower_expression(&bok_e, &val) != llvm_gen_be_error_t::ok);
    bok_op1.next = &i32_arg1;
    bok_e.type = &bad_ty;
    assert(llvm_lower_expression(&bok_e, &val) != llvm_gen_be_error_t::ok);

    stmt_e.type = &bad_ty;
    assert(llvm_lower_expression(&stmt_e, &val) != llvm_gen_be_error_t::ok);

    t_supp.type = &bad_ty;
    t_supp.dynamic_init = &exc_dip;
    assert(llvm_lower_expression(&throw_e, &val) != llvm_gen_be_error_t::ok);
    t_supp.type = &int_type;
    t_supp.dynamic_init = &bad_dip;
    assert(llvm_lower_expression(&throw_e, &val) != llvm_gen_be_error_t::ok);
    a_type null_ti_ty = {};
    null_ti_ty.size = 4;
    t_supp.type = &null_ti_ty;
    t_supp.dynamic_init = nullptr;
    // get_typeinfo_global with nullptr type:
    t_supp.type = nullptr;

    empty_temp_e.type = &bad_ty;
    assert(llvm_lower_expression(&empty_temp_e, &val) != llvm_gen_be_error_t::ok);
    empty_temp_e.type = &int_type;
    empty_temp_e.variant.init.dynamic_init = &bad_dip;
    assert(llvm_lower_expression(&empty_temp_e, &val) != llvm_gen_be_error_t::ok);

    obj_lifetime_expr.variant.object_lifetime.expr = &bad_op;
    assert(llvm_lower_expression(&obj_lifetime_expr, &val) != llvm_gen_be_error_t::ok);

    custom_nd.type = &bad_ty;
    assert(llvm_lower_expression(&bad_nd_e, &val) != llvm_gen_be_error_t::ok);
    custom_nd.type = &int_type;
    custom_nd.placement_new = FALSE;
    custom_nd.number_of_elements = &bad_op;
    assert(llvm_lower_expression(&bad_nd_e, &val) != llvm_gen_be_error_t::ok);
    custom_nd.number_of_elements = nullptr;
    custom_nd.arg = &bad_op;
    assert(llvm_lower_expression(&bad_nd_e, &val) != llvm_gen_be_error_t::ok);
    custom_nd.arg = nullptr;
    bad_nd_e.type = &bad_ty;
    assert(llvm_lower_expression(&bad_nd_e, &val) != llvm_gen_be_error_t::ok);
    bad_nd_e.type = &ptr_type;
    custom_nd.dynamic_init = &bad_dip;
    assert(llvm_lower_expression(&bad_nd_e, &val) != llvm_gen_be_error_t::ok);
    custom_nd.freeing_of_storage_on_exception = &new_free_dip;
    assert(llvm_lower_expression(&bad_nd_e, &val) != llvm_gen_be_error_t::ok);

    custom_nd.is_new = FALSE;
    custom_nd.arg = &bad_op;
    assert(llvm_lower_expression(&bad_nd_e, &val) != llvm_gen_be_error_t::ok);
    ptr_arg1.next = &bad_op;
    bad_op.next = nullptr;
    custom_nd.arg = &ptr_arg1;
    assert(llvm_lower_expression(&bad_nd_e, &val) != llvm_gen_be_error_t::ok);
    ptr_arg1.next = nullptr;

    // 9. llvm_lower_compound_assignment error percolation
    ca_expr.variant.operation.kind = eok_pre_incr;
    ca_expr.variant.operation.operands = &bad_lval;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) != llvm_gen_be_error_t::ok);
    local_bad_ty_var.is_lvalue = TRUE;
    ca_expr.variant.operation.operands = &local_bad_ty_var;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) != llvm_gen_be_error_t::ok);
    ca_lhs = dummy_op1;
    ca_lhs.type = &int_type;
    ca_lhs.next = &bad_op;
    ca_expr.variant.operation.kind = eok_add_assign;
    ca_expr.variant.operation.operands = &ca_lhs;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) != llvm_gen_be_error_t::ok);

    // 10. Phase 3 Expression Features:
    // - broadcast_scalar_to_vector (both t1=vec,t2=scalar and t1=scalar,t2=vec; fp->fp, int->fp, int->int)
    a_type vec_type = {}; vec_type.kind = tk_integer; vec_type.size = 16;
    llvm::Type* llvm_v4i32 = llvm::FixedVectorType::get(llvm::Type::getInt32Ty(*state.context), 4);
    state.type_cache[&vec_type] = llvm_v4i32;
    a_variable vec_var = {}; vec_var.type = &vec_type;
    state.local_vars[&vec_var] = state.builder->CreateAlloca(llvm_v4i32);

    a_type f32_vec_ty = {}; f32_vec_ty.kind = tk_float; f32_vec_ty.size = 16;
    llvm::Type* llvm_v4f32 = llvm::FixedVectorType::get(llvm::Type::getFloatTy(*state.context), 4);
    state.type_cache[&f32_vec_ty] = llvm_v4f32;
    a_variable f32_vec_var = {}; f32_vec_var.type = &f32_vec_ty;
    state.local_vars[&f32_vec_var] = state.builder->CreateAlloca(llvm_v4f32);

    a_type i64_edg_ty = {}; i64_edg_ty.kind = tk_integer; i64_edg_ty.size = 8; i64_edg_ty.variant.integer.int_kind = ik_long_long;
    a_variable i64_var = {}; i64_var.type = &i64_edg_ty;
    state.local_vars[&i64_var] = state.builder->CreateAlloca(llvm::Type::getInt64Ty(*state.context));

    a_type f64_edg_ty = {}; f64_edg_ty.kind = tk_float; f64_edg_ty.size = 8; f64_edg_ty.variant.float_kind = fk_double;
    a_variable f64_var = {}; f64_var.type = &f64_edg_ty;
    state.local_vars[&f64_var] = state.builder->CreateAlloca(llvm::Type::getDoubleTy(*state.context));

    an_expr_node fv_op = {}, iv_op = {}, i64_op = {}, f64_op = {};
    fv_op.kind = enk_variable; fv_op.type = &f32_vec_ty; fv_op.variant.variable.ptr = &f32_vec_var;
    iv_op.kind = enk_variable; iv_op.type = &vec_type; iv_op.variant.variable.ptr = &vec_var;
    i64_op.kind = enk_variable; i64_op.type = &i64_edg_ty; i64_op.variant.variable.ptr = &i64_var;
    f64_op.kind = enk_variable; f64_op.type = &f64_edg_ty; f64_op.variant.variable.ptr = &f64_var;

    an_expr_node vec_bin_e = {};
    vec_bin_e.kind = enk_operation;
    vec_bin_e.variant.operation.kind = eok_add;
    // a) f32_vec + f64 (fp->fp cast)
    vec_bin_e.type = &f32_vec_ty;
    fv_op.next = &f64_op; f64_op.next = nullptr;
    vec_bin_e.variant.operation.operands = &fv_op;
    assert(llvm_lower_expression(&vec_bin_e, &val) == llvm_gen_be_error_t::ok);
    // b) i64 + f32_vec (int->fp cast, scalar on left)
    i64_op.next = &fv_op; fv_op.next = nullptr;
    vec_bin_e.variant.operation.operands = &i64_op;
    assert(llvm_lower_expression(&vec_bin_e, &val) == llvm_gen_be_error_t::ok);
    // c) i32_vec + i64 (int->int cast)
    vec_bin_e.type = &vec_type;
    iv_op.next = &i64_op; i64_op.next = nullptr;
    vec_bin_e.variant.operation.operands = &iv_op;
    assert(llvm_lower_expression(&vec_bin_e, &val) == llvm_gen_be_error_t::ok);

    // - enk_lambda & dip->static_temp == TRUE (and rvalue load error when expr->type is bad_ty after lvalue succeeds)
    a_dynamic_init static_tmp_dip = {};
    static_tmp_dip.kind = dik_zero;
    static_tmp_dip.static_temp = TRUE;
    an_expr_node lam_e = {};
    lam_e.kind = enk_lambda;
    lam_e.type = &int_type;
    lam_e.variant.init.dynamic_init = &static_tmp_dip;
    assert(llvm_lower_expression(&lam_e, &val) == llvm_gen_be_error_t::ok);

    // - eok_parens and eok_lvalue (both lvalue and rvalue)
    an_expr_node par_e = {};
    par_e.kind = enk_operation;
    par_e.type = &int_type;
    par_e.variant.operation.kind = eok_parens;
    dummy_op1.next = nullptr;
    par_e.variant.operation.operands = &dummy_op1;
    par_e.is_lvalue = TRUE;
    assert(llvm_lower_expression(&par_e, &val) == llvm_gen_be_error_t::ok);
    par_e.is_lvalue = FALSE;
    assert(llvm_lower_expression(&par_e, &val) == llvm_gen_be_error_t::ok);
    par_e.variant.operation.kind = eok_lvalue;
    par_e.is_lvalue = TRUE;
    assert(llvm_lower_expression(&par_e, &val) == llvm_gen_be_error_t::ok);
    par_e.is_lvalue = FALSE;
    assert(llvm_lower_expression(&par_e, &val) == llvm_gen_be_error_t::ok);

    // - eok_gnu_min and eok_gnu_max (fp, signed int, unsigned int, non-integer pointer)
    an_expr_node minmax_e = {};
    minmax_e.kind = enk_operation;
    minmax_e.type = &f64_edg_ty;
    an_expr_node f64_b = f64_op; f64_b.next = nullptr;
    f64_op.next = &f64_b;
    minmax_e.variant.operation.operands = &f64_op;
    minmax_e.variant.operation.kind = eok_gnu_min;
    assert(llvm_lower_expression(&minmax_e, &val) == llvm_gen_be_error_t::ok);
    minmax_e.variant.operation.kind = eok_gnu_max;
    assert(llvm_lower_expression(&minmax_e, &val) == llvm_gen_be_error_t::ok);

    an_expr_node i_op_a = i64_op, i_op_b = i64_op;
    i64_edg_ty.variant.integer.int_kind = ik_int;
    i_op_a.next = &i_op_b; i_op_b.next = nullptr;
    minmax_e.type = &i64_edg_ty;
    minmax_e.variant.operation.operands = &i_op_a;
    int_kind_is_signed[ik_int] = TRUE;
    minmax_e.variant.operation.kind = eok_gnu_min;
    assert(llvm_lower_expression(&minmax_e, &val) == llvm_gen_be_error_t::ok);
    minmax_e.variant.operation.kind = eok_gnu_max;
    assert(llvm_lower_expression(&minmax_e, &val) == llvm_gen_be_error_t::ok);
    int_kind_is_signed[ik_int] = FALSE;
    minmax_e.variant.operation.kind = eok_gnu_min;
    assert(llvm_lower_expression(&minmax_e, &val) == llvm_gen_be_error_t::ok);
    minmax_e.variant.operation.kind = eok_gnu_max;
    assert(llvm_lower_expression(&minmax_e, &val) == llvm_gen_be_error_t::ok);
    int_kind_is_signed[ik_int] = TRUE;
    i64_edg_ty.variant.integer.int_kind = ik_long_long;

    // - eok_dot_pm_func_ptr and eok_points_to_pm_func_ptr (struct pm_val, scalar pm_val, error propagation)
    a_type pmf_struct_edg_ty = {}; pmf_struct_edg_ty.kind = tk_ptr_to_member; pmf_struct_edg_ty.size = 16;
    llvm::Type* llvm_pmf_st = llvm::StructType::get(*state.context, {llvm::Type::getInt64Ty(*state.context), llvm::Type::getInt64Ty(*state.context)});
    state.type_cache[&pmf_struct_edg_ty] = llvm_pmf_st;
    a_variable pmf_var = {}; pmf_var.type = &pmf_struct_edg_ty;
    state.local_vars[&pmf_var] = state.builder->CreateAlloca(llvm_pmf_st);

    an_expr_node pmf_op = {}; pmf_op.kind = enk_variable; pmf_op.type = &pmf_struct_edg_ty; pmf_op.variant.variable.ptr = &pmf_var;
    an_expr_node pmfp_e = {}; pmfp_e.kind = enk_operation; pmfp_e.type = &ptr_type;
    pmfp_e.variant.operation.kind = eok_dot_pm_func_ptr;
    dummy_op1.next = &pmf_op; pmf_op.next = nullptr;
    pmfp_e.variant.operation.operands = &dummy_op1;
    assert(llvm_lower_expression(&pmfp_e, &val) == llvm_gen_be_error_t::ok);
    pmfp_e.variant.operation.kind = eok_points_to_pm_func_ptr;
    dummy_op1.next = &dummy_op2; dummy_op2.next = nullptr;
    assert(llvm_lower_expression(&pmfp_e, &val) == llvm_gen_be_error_t::ok);
    dummy_op1.next = &bad_op; bad_op.next = nullptr;
    assert(llvm_lower_expression(&pmfp_e, &val) != llvm_gen_be_error_t::ok);

    // - eok_vector_subscript (rvalue, eok_assign lvalue, and compound assignment lvalue + error paths)
    an_expr_node vsub_e = {}; vsub_e.kind = enk_operation; vsub_e.type = &int_type;
    vsub_e.variant.operation.kind = eok_vector_subscript;
    iv_op.next = &dummy_op2; dummy_op2.next = nullptr;
    vsub_e.variant.operation.operands = &iv_op;
    assert(llvm_lower_expression(&vsub_e, &val) == llvm_gen_be_error_t::ok);

    an_expr_node vass_e = {}; vass_e.kind = enk_operation; vass_e.type = &int_type;
    vass_e.variant.operation.kind = eok_assign;
    vsub_e.next = &dummy_op1; dummy_op1.next = nullptr;
    vass_e.variant.operation.operands = &vsub_e;
    assert(llvm_lower_expression(&vass_e, &val) == llvm_gen_be_error_t::ok);
    // Error paths in eok_assign vector_subscript:
    vsub_e.next = &bad_op;
    assert(llvm_lower_expression(&vass_e, &val) != llvm_gen_be_error_t::ok);
    iv_op.next = &bad_op;
    assert(llvm_lower_expression(&vass_e, &val) != llvm_gen_be_error_t::ok);
    vsub_e.variant.operation.operands = &bad_lval;
    assert(llvm_lower_expression(&vass_e, &val) != llvm_gen_be_error_t::ok);
    an_expr_node bad_ty_vbase = iv_op; bad_ty_vbase.type = &bad_ty; bad_ty_vbase.next = &dummy_op2;
    vsub_e.variant.operation.operands = &bad_ty_vbase;
    vsub_e.next = &dummy_op1;
    assert(llvm_lower_expression(&vass_e, &val) != llvm_gen_be_error_t::ok);

    // Compound assignment on eok_vector_subscript:
    iv_op.next = &dummy_op2; dummy_op2.next = nullptr;
    vsub_e.variant.operation.operands = &iv_op;
    vsub_e.next = &dummy_op1; dummy_op1.next = nullptr;
    ca_expr.variant.operation.kind = eok_add_assign;
    ca_expr.variant.operation.operands = &vsub_e;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) == llvm_gen_be_error_t::ok);
    // Error paths on compound assignment eok_vector_subscript:
    vsub_e.variant.operation.operands = &bad_ty_vbase;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) != llvm_gen_be_error_t::ok);
    iv_op.next = &bad_op;
    vsub_e.variant.operation.operands = &iv_op;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) != llvm_gen_be_error_t::ok);
    vsub_e.variant.operation.operands = &bad_lval;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) != llvm_gen_be_error_t::ok);

    // - eok_vector_question (success and error in true_expr/false_expr)
    an_expr_node vq_e = {}; vq_e.kind = enk_operation; vq_e.type = &vec_type;
    vq_e.variant.operation.kind = eok_vector_question;
    an_expr_node iv_op2 = iv_op, iv_op3 = iv_op;
    iv_op.next = &iv_op2; iv_op2.next = &iv_op3; iv_op3.next = nullptr;
    vq_e.variant.operation.operands = &iv_op;
    assert(llvm_lower_expression(&vq_e, &val) == llvm_gen_be_error_t::ok);
    iv_op2.next = &bad_op; bad_op.next = nullptr;
    assert(llvm_lower_expression(&vq_e, &val) != llvm_gen_be_error_t::ok);

    // - enk_builtin_choose_expr (invalid args, choose_first TRUE and FALSE)
    an_expr_node bce_e = {}; bce_e.kind = enk_builtin_choose_expr; bce_e.type = &int_type;
    bce_e.variant.builtin_choose_expr.operands = nullptr;
    assert(llvm_lower_expression(&bce_e, &val) == llvm_gen_be_error_t::invalid_argument);
    dummy_op1.next = nullptr;
    bce_e.variant.builtin_choose_expr.operands = &dummy_op1;
    assert(llvm_lower_expression(&bce_e, &val) == llvm_gen_be_error_t::invalid_argument);
    dummy_op1.next = &dummy_op2; dummy_op2.next = nullptr;
    assert(llvm_lower_expression(&bce_e, &val) == llvm_gen_be_error_t::invalid_argument);
    an_expr_node dummy_op3 = dummy_op1; dummy_op3.next = nullptr;
    dummy_op2.next = &dummy_op3;
    bce_e.variant.builtin_choose_expr.choose_first = TRUE;
    assert(llvm_lower_expression(&bce_e, &val) == llvm_gen_be_error_t::ok);
    bce_e.variant.builtin_choose_expr.choose_first = FALSE;
    assert(llvm_lower_expression(&bce_e, &val) == llvm_gen_be_error_t::ok);

    // - enk_condition (null supp, initialization ok/fail, dynamic_init new/cached var, bad type, bad dip)
    an_expr_node cond_e = {}; cond_e.kind = enk_condition; cond_e.type = &int_type;
    cond_e.variant.condition = nullptr;
    assert(llvm_lower_expression(&cond_e, &val) == llvm_gen_be_error_t::invalid_argument);

    a_statement cond_ok_stmt = {}; cond_ok_stmt.kind = stmk_empty;
    a_statement cond_bad_stmt = {}; cond_bad_stmt.kind = stmk_return;
    a_variable cond_new_var = {}; cond_new_var.type = &int_type;
    a_dynamic_init cond_dip = {}; cond_dip.kind = dik_zero; cond_dip.variable = nullptr;
    a_condition_supplement cond_supp = {};
    cond_supp.expr = &dummy_op1; dummy_op1.next = nullptr;
    cond_e.variant.condition = &cond_supp;
    assert(llvm_lower_expression(&cond_e, &val) == llvm_gen_be_error_t::ok);

    cond_supp.initialization = &cond_bad_stmt;
    assert(llvm_lower_expression(&cond_e, &val) != llvm_gen_be_error_t::ok);
    cond_supp.initialization = &cond_ok_stmt;
    // Wait: in test_llvm_expressions.cpp stub, llvm_emit_dynamic_init fails when dip->variable != nullptr!
    // Let's temporarily use a variable with bad_ty first to test get_llvm_type failure:
    a_variable cond_bad_ty_var = {}; cond_bad_ty_var.type = &bad_ty;
    cond_dip.variable = &cond_bad_ty_var;
    cond_supp.dynamic_init = &cond_dip;
    assert(llvm_lower_expression(&cond_e, &val) != llvm_gen_be_error_t::ok);
    // Now cond_new_var (not in local_vars -> allocates and fails in stub llvm_emit_dynamic_init because dip->variable != nullptr)
    cond_dip.variable = &cond_new_var;
    assert(llvm_lower_expression(&cond_e, &val) != llvm_gen_be_error_t::ok);
    // Second call finds cond_new_var in local_vars!
    cond_dip.kind = dik_constant; // we'll allow dik_constant with variable in stub below
    assert(llvm_lower_expression(&cond_e, &val) == llvm_gen_be_error_t::ok);

    // - enk_field (null ptr and valid ptr)
    an_expr_node fld_e = {}; fld_e.kind = enk_field; fld_e.type = &int_type;
    fld_e.variant.field.ptr = nullptr;
    assert(llvm_lower_expression(&fld_e, &val) == llvm_gen_be_error_t::invalid_argument);
    a_field test_fld = {}; test_fld.offset = 12;
    fld_e.variant.field.ptr = &test_fld;
    assert(llvm_lower_expression(&fld_e, &val) == llvm_gen_be_error_t::ok);

    // - enk_sizeof, enk_alignof, enk_datasizeof, enk_sizeof_pack, enk_initializer
    an_expr_node sz_e = {}; sz_e.kind = enk_sizeof; sz_e.type = &int_type;
    sz_e.variant.sizeof_info.is_type = TRUE;
    sz_e.variant.sizeof_info.variant.type = &int_type;
    assert(llvm_lower_expression(&sz_e, &val) == llvm_gen_be_error_t::ok);
    sz_e.variant.sizeof_info.is_type = FALSE;
    sz_e.variant.sizeof_info.variant.expr = &dummy_op1;
    assert(llvm_lower_expression(&sz_e, &val) == llvm_gen_be_error_t::ok);
    sz_e.variant.sizeof_info.variant.expr = nullptr;
    assert(llvm_lower_expression(&sz_e, &val) == llvm_gen_be_error_t::ok);

    sz_e.kind = enk_alignof;
    sz_e.variant.sizeof_info.is_type = TRUE;
    sz_e.variant.sizeof_info.variant.type = &int_type;
    assert(llvm_lower_expression(&sz_e, &val) == llvm_gen_be_error_t::ok);
    sz_e.variant.sizeof_info.variant.type = nullptr;
    assert(llvm_lower_expression(&sz_e, &val) == llvm_gen_be_error_t::ok);

    // VLA sizeof: 3D array (mapped VLA dim, unmapped VLA dim, fixed dim > 0, fixed dim == 0) with element size > 1 and == 1
    a_vla_dimension vdim_mapped = {}, vdim_unmapped = {};
    state.vla_dimension_values[&vdim_mapped] = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 5);
    a_type vla_el_ty = int_type; vla_el_ty.size = 4;
    a_type vla_el1_ty = int_type; vla_el1_ty.size = 1;
    a_type vla_a4 = {}; vla_a4.kind = tk_array; vla_a4.variant.array.element_type = &vla_el_ty; vla_a4.variant.array.variant.number_of_elements = 0;
    a_type vla_a3 = {}; vla_a3.kind = tk_array; vla_a3.variant.array.element_type = &vla_a4; vla_a3.variant.array.variant.number_of_elements = 3;
    a_type vla_a2 = {}; vla_a2.kind = tk_array; vla_a2.variant.array.element_type = &vla_a3;
    a_type vla_a1 = {}; vla_a1.kind = tk_array; vla_a1.variant.array.element_type = &vla_a2;
    state.array_to_vla_dim[&vla_a1] = &vdim_mapped;
    state.array_to_vla_dim[&vla_a2] = &vdim_unmapped;

    sz_e.kind = enk_datasizeof;
    sz_e.variant.sizeof_info.is_type = TRUE;
    sz_e.variant.sizeof_info.variant.type = &vla_a1;
    assert(llvm_lower_expression(&sz_e, &val) == llvm_gen_be_error_t::ok);
    vla_a4.variant.array.element_type = &vla_el1_ty;
    assert(llvm_lower_expression(&sz_e, &val) == llvm_gen_be_error_t::ok);

    an_expr_node sz_pack_e = {}; sz_pack_e.kind = enk_sizeof_pack; sz_pack_e.type = &int_type;
    assert(llvm_lower_expression(&sz_pack_e, &val) == llvm_gen_be_error_t::ok);

    an_expr_node init_e = {}; init_e.kind = enk_initializer; init_e.type = &int_type;
    init_e.variant.initializer.dyn_init = nullptr;
    assert(llvm_lower_expression(&init_e, &val) == llvm_gen_be_error_t::invalid_argument);
    a_dynamic_init ok_init_dip = {}; ok_init_dip.kind = dik_zero;
    init_e.variant.initializer.dyn_init = &ok_init_dip;
    assert(llvm_lower_expression(&init_e, &val) == llvm_gen_be_error_t::ok);
    init_e.type = &bad_ty;
    assert(llvm_lower_expression(&init_e, &val) != llvm_gen_be_error_t::ok);
    init_e.type = &int_type;
    init_e.variant.initializer.dyn_init = &bad_dip;
    assert(llvm_lower_expression(&init_e, &val) != llvm_gen_be_error_t::ok);

    // - Complex compound assignment (+=, -=, *=, /=) with complex RHS, float RHS, and int RHS
    a_type cplx_edg_ty = {}; cplx_edg_ty.kind = tk_complex; cplx_edg_ty.size = 8; cplx_edg_ty.variant.float_kind = fk_float;
    llvm::Type* llvm_cplx_f32 = llvm::StructType::get(*state.context, {llvm::Type::getFloatTy(*state.context), llvm::Type::getFloatTy(*state.context)});
    state.type_cache[&cplx_edg_ty] = llvm_cplx_f32;
    a_variable cplx_var = {}; cplx_var.type = &cplx_edg_ty;
    state.local_vars[&cplx_var] = state.builder->CreateAlloca(llvm_cplx_f32);

    an_expr_node cplx_lhs = {}, cplx_rhs = {};
    cplx_lhs.kind = enk_variable; cplx_lhs.type = &cplx_edg_ty; cplx_lhs.variant.variable.ptr = &cplx_var;
    cplx_rhs.kind = enk_variable; cplx_rhs.type = &cplx_edg_ty; cplx_rhs.variant.variable.ptr = &cplx_var;

    ca_expr.type = &cplx_edg_ty;
    ca_expr.variant.operation.operands = &cplx_lhs;
    cplx_lhs.next = &cplx_rhs; cplx_rhs.next = nullptr;
    for (an_expr_operator_kind ck : {eok_add_assign, eok_subtract_assign, eok_multiply_assign, eok_divide_assign}) {
      ca_expr.variant.operation.kind = ck;
      assert(llvm_lower_compound_assignment(&ca_expr, &val) == llvm_gen_be_error_t::ok);
    }
    // Complex += float and Complex += int
    cplx_lhs.next = &f64_op; f64_op.next = nullptr;
    ca_expr.variant.operation.kind = eok_add_assign;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) == llvm_gen_be_error_t::ok);
    cplx_lhs.next = &dummy_op1; dummy_op1.next = nullptr;
    assert(llvm_lower_compound_assignment(&ca_expr, &val) == llvm_gen_be_error_t::ok);

    // enk_new_delete with active landing pad (invoke) and nounwind operator new
    llvm::Function* cur_f = state.builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* new_lpad = llvm::BasicBlock::Create(*state.context, "new.lpad", cur_f);
    state.current_landing_pads.push_back(new_lpad);
    custom_nd.is_new = TRUE;
    custom_nd.type = &int_type;
    custom_nd.dynamic_init = nullptr;
    custom_nd.freeing_of_storage_on_exception = nullptr;
    assert(llvm_lower_expression(&bad_nd_e, &val) == llvm_gen_be_error_t::ok);
    llvm::Function* op_new_fn = state.module->getFunction(custom_nd.routine->source_corresp.name);
    if (op_new_fn) op_new_fn->setDoesNotThrow();
    assert(llvm_lower_expression(&bad_nd_e, &val) == llvm_gen_be_error_t::ok);
    state.current_landing_pads.pop_back();

    // Also test llvm_lower_call_expression when callee is a nounwind Function
    an_expr_node nw_callee = {};
    nw_callee.kind = enk_routine;
    nw_callee.variant.routine.ptr = custom_nd.routine;
    an_expr_node nw_call = {};
    nw_call.kind = enk_operation;
    nw_call.type = &int_type;
    nw_call.variant.operation.kind = eok_call;
    nw_call.variant.operation.operands = &nw_callee;
    assert(llvm_lower_expression(&nw_call, &val) == llvm_gen_be_error_t::ok);

    // Test multi-register (2-eightbyte struct + trailing int) aggregate argument packing in llvm_lower_call_expression
    llvm::Type* i64_llvm_ty = llvm::Type::getInt64Ty(*state.context);
    llvm::StructType* pair64_llvm_ty = llvm::StructType::get(*state.context, {i64_llvm_ty, i64_llvm_ty});
    a_type pair64_edg_ty = {};
    pair64_edg_ty.kind = tk_struct;
    pair64_edg_ty.size = 16;
    pair64_edg_ty.alignment = 8;
    a_field pf1 = {}, pf2 = {};
    pf1.type = &i64_ty; pf1.offset = 0; pf1.next = &pf2;
    pf2.type = &i64_ty; pf2.offset = 8; pf2.next = nullptr;
    pair64_edg_ty.variant.class_struct_union.field_list = &pf1;
    state.type_cache[&pair64_edg_ty] = pair64_llvm_ty;
    a_variable pair64_var = {};
    pair64_var.type = &pair64_edg_ty;
    state.local_vars[&pair64_var] = state.builder->CreateAlloca(pair64_llvm_ty);

    a_param_type hfa_p1 = {}, hfa_p2 = {};
    hfa_p1.type = &pair64_edg_ty;
    hfa_p1.next = &hfa_p2;
    hfa_p2.type = &int_type;
    a_routine_type_supplement hfa_rsupp = {};
    hfa_rsupp.param_type_list = &hfa_p1;
    a_type hfa_rout_ty = {};
    hfa_rout_ty.kind = tk_routine;
    hfa_rout_ty.variant.routine.return_type = &int_type;
    hfa_rout_ty.variant.routine.extra_info = &hfa_rsupp;

    llvm::FunctionType* hfa4_callee_fty = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*state.context), {i64_llvm_ty, i64_llvm_ty, llvm::Type::getInt32Ty(*state.context)}, false);
    llvm::Function::Create(hfa4_callee_fty, llvm::GlobalValue::ExternalLinkage, "hfa4_callee", state.module.get());
    a_routine hfa4_rout = {};
    hfa4_rout.source_corresp.name = "hfa4_callee";
    hfa4_rout.type = &hfa_rout_ty;

    a_constant hfa4_c_int = {};
    hfa4_c_int.kind = ck_integer;
    hfa4_c_int.type = &int_type;
    an_expr_node hfa4_callee_node = {}, hfa4_arg1 = {}, hfa4_arg2 = {};
    hfa4_callee_node.kind = enk_routine;
    hfa4_callee_node.type = &hfa_rout_ty;
    hfa4_callee_node.variant.routine.ptr = &hfa4_rout;
    hfa4_callee_node.next = &hfa4_arg1;
    hfa4_arg1.kind = enk_variable;
    hfa4_arg1.type = &pair64_edg_ty;
    hfa4_arg1.variant.variable.ptr = &pair64_var;
    hfa4_arg1.next = &hfa4_arg2;
    hfa4_arg2.kind = enk_constant;
    hfa4_arg2.type = &int_type;
    hfa4_arg2.variant.constant.ptr = &hfa4_c_int;
    nw_call.variant.operation.operands = &hfa4_callee_node;
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
    assert(llvm_lower_expression(&nw_call, &val) == llvm_gen_be_error_t::ok);

    // Also test when hfa4_callee has only 2 params (so 16-byte struct coerced to 1st i64 stops before consuming 2nd param), and when op1 is non-enk_routine
    llvm::Function* hfa4_fn = state.module->getFunction("hfa4_callee");
    if (hfa4_fn) hfa4_fn->eraseFromParent();
    llvm::FunctionType* hfa2_callee_fty = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*state.context), {i64_llvm_ty, llvm::Type::getInt32Ty(*state.context)}, false);
    llvm::Function::Create(hfa2_callee_fty, llvm::GlobalValue::ExternalLinkage, "hfa4_callee", state.module.get());
    assert(llvm_lower_expression(&nw_call, &val) == llvm_gen_be_error_t::ok);

    a_type ptr_hfa_rout_ty = {};
    ptr_hfa_rout_ty.kind = tk_pointer;
    ptr_hfa_rout_ty.size = 8;
    ptr_hfa_rout_ty.variant.pointer.type = &hfa_rout_ty;
    state.type_cache[&ptr_hfa_rout_ty] = llvm::PointerType::getUnqual(*state.context);
    a_variable fn_ptr_var = {};
    fn_ptr_var.type = &ptr_hfa_rout_ty;
    state.local_vars[&fn_ptr_var] = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
    hfa4_callee_node.kind = enk_variable;
    hfa4_callee_node.type = &ptr_hfa_rout_ty;
    hfa4_callee_node.variant.variable.ptr = &fn_ptr_var;
    assert(llvm_lower_expression(&nw_call, &val) == llvm_gen_be_error_t::ok);

    // Phase 17:
    // 1. x86_32, arm32, thumb, and wasm eok_va_{start,arg,end,copy} dispatch
    a_variable va_v17 = {};
    va_v17.source_corresp.name = "va_v17";
    va_v17.type = &ptr_type;
    state.local_vars[&va_v17] = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
    an_expr_node va_op1_17 = {}, va_op2_17 = {}, va_expr_17 = {};
    va_op1_17.kind = enk_variable;
    va_op1_17.type = &ptr_type;
    va_op1_17.variant.variable.ptr = &va_v17;
    va_op2_17 = va_op1_17;
    va_op1_17.next = &va_op2_17;
    va_expr_17.kind = enk_operation;
    va_expr_17.type = &int_type;
    va_expr_17.variant.operation.operands = &va_op1_17;

    const char* va_triples_17[] = {
        "i686-unknown-linux-gnu",
        "armv7-unknown-linux-gnueabihf",
        "thumbv7-unknown-linux-gnueabihf",
        "wasm32-unknown-unknown"
    };
    const an_expr_operator_kind va_ops_17[] = {eok_va_start, eok_va_arg, eok_va_end, eok_va_copy};
    for (const char* t_str : va_triples_17) {
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), t_str);
      for (an_expr_operator_kind vk : va_ops_17) {
        va_expr_17.variant.operation.kind = vk;
        assert(llvm_lower_expression(&va_expr_17, &val) == llvm_gen_be_error_t::ok);
      }
    }
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");

    // 2. C11 _Atomic (TQ_C11_ATOMIC) loads and stores (scalar sizes 1, 2, 4, 8, 16, pointer, volatile, and non-power-of-two/aggregate)
    unsigned atomic_sizes[] = {1, 2, 4, 8, 16};
    for (unsigned asz : atomic_sizes) {
      a_type base_int_at = {};
      base_int_at.kind = tk_integer;
      base_int_at.size = asz;
      state.type_cache[&base_int_at] = llvm::Type::getIntNTy(*state.context, asz * 8);
      a_type atomic_int_ty = {};
      atomic_int_ty.kind = tk_typeref;
      atomic_int_ty.size = asz;
      atomic_int_ty.variant.typeref.type = &base_int_at;
      atomic_int_ty.variant.typeref.qualifiers = (asz == 4) ? (TQ_C11_ATOMIC | TQ_VOLATILE) : TQ_C11_ATOMIC;
      state.type_cache[&atomic_int_ty] = llvm::Type::getIntNTy(*state.context, asz * 8);

      a_variable at_var = {};
      at_var.source_corresp.name = "at_var";
      at_var.type = &atomic_int_ty;
      state.local_vars[&at_var] = state.builder->CreateAlloca(llvm::Type::getIntNTy(*state.context, asz * 8));

      an_expr_node at_var_node = {};
      at_var_node.kind = enk_variable;
      at_var_node.type = &atomic_int_ty;
      at_var_node.variant.variable.ptr = &at_var;
      assert(llvm_lower_expression(&at_var_node, &val) == llvm_gen_be_error_t::ok);

      an_expr_node at_assign = {};
      at_assign.kind = enk_operation;
      at_assign.type = &atomic_int_ty;
      at_assign.variant.operation.kind = eok_assign;
      an_expr_node at_rhs = at_var_node;
      at_var_node.next = &at_rhs;
      at_assign.variant.operation.operands = &at_var_node;
      assert(llvm_lower_expression(&at_assign, &val) == llvm_gen_be_error_t::ok);
    }

    // _Atomic pointer load/store
    a_type atomic_ptr_ty = {};
    atomic_ptr_ty.kind = tk_typeref;
    atomic_ptr_ty.size = 8;
    atomic_ptr_ty.variant.typeref.type = &ptr_type;
    atomic_ptr_ty.variant.typeref.qualifiers = TQ_C11_ATOMIC;
    state.type_cache[&atomic_ptr_ty] = llvm::PointerType::getUnqual(*state.context);
    a_variable at_ptr_var = {};
    at_ptr_var.type = &atomic_ptr_ty;
    state.local_vars[&at_ptr_var] = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
    an_expr_node at_ptr_lhs = {}, at_ptr_rhs = {}, at_ptr_assign = {};
    at_ptr_lhs.kind = enk_variable;
    at_ptr_lhs.type = &atomic_ptr_ty;
    at_ptr_lhs.variant.variable.ptr = &at_ptr_var;
    at_ptr_rhs = at_ptr_lhs;
    at_ptr_lhs.next = &at_ptr_rhs;
    at_ptr_assign.kind = enk_operation;
    at_ptr_assign.type = &atomic_ptr_ty;
    at_ptr_assign.variant.operation.kind = eok_assign;
    at_ptr_assign.variant.operation.operands = &at_ptr_lhs;
    assert(llvm_lower_expression(&at_ptr_assign, &val) == llvm_gen_be_error_t::ok);

    // _Atomic aggregate (non-power-of-two size 24 struct -> __atomic_load / __atomic_store) and _Atomic 3-byte integer
    llvm::StructType* aggr24_llvm_ty = llvm::StructType::get(
        *state.context, {i64_llvm_ty, i64_llvm_ty, i64_llvm_ty});
    a_type aggr24_edg = {};
    aggr24_edg.kind = tk_struct;
    aggr24_edg.size = 24;
    a_type atomic_aggr24_ty = {};
    atomic_aggr24_ty.kind = tk_typeref;
    atomic_aggr24_ty.size = 24;
    atomic_aggr24_ty.variant.typeref.type = &aggr24_edg;
    atomic_aggr24_ty.variant.typeref.qualifiers = TQ_C11_ATOMIC;
    state.type_cache[&atomic_aggr24_ty] = aggr24_llvm_ty;
    a_variable at_aggr_var = {};
    at_aggr_var.type = &atomic_aggr24_ty;
    state.local_vars[&at_aggr_var] = state.builder->CreateAlloca(aggr24_llvm_ty);
    an_expr_node at_aggr_lhs = {}, at_aggr_rhs = {}, at_aggr_assign = {};
    at_aggr_lhs.kind = enk_variable;
    at_aggr_lhs.type = &atomic_aggr24_ty;
    at_aggr_lhs.variant.variable.ptr = &at_aggr_var;
    at_aggr_rhs = at_aggr_lhs;
    at_aggr_lhs.next = &at_aggr_rhs;
    at_aggr_assign.kind = enk_operation;
    at_aggr_assign.type = &atomic_aggr24_ty;
    at_aggr_assign.variant.operation.kind = eok_assign;
    at_aggr_assign.variant.operation.operands = &at_aggr_lhs;
    assert(llvm_lower_expression(&at_aggr_assign, &val) == llvm_gen_be_error_t::ok);

    a_type atomic_i24_ty = {};
    atomic_i24_ty.kind = tk_typeref;
    atomic_i24_ty.size = 3;
    atomic_i24_ty.variant.typeref.type = &int_type;
    atomic_i24_ty.variant.typeref.qualifiers = TQ_C11_ATOMIC;
    state.type_cache[&atomic_i24_ty] = llvm::Type::getIntNTy(*state.context, 24);
    a_variable at_i24_var = {};
    at_i24_var.type = &atomic_i24_ty;
    state.local_vars[&at_i24_var] = state.builder->CreateAlloca(llvm::Type::getIntNTy(*state.context, 24));
    at_aggr_lhs.type = &atomic_i24_ty;
    at_aggr_lhs.variant.variable.ptr = &at_i24_var;
    at_aggr_rhs.type = &atomic_i24_ty;
    at_aggr_rhs.variant.variable.ptr = &at_i24_var;
    at_aggr_assign.type = &atomic_i24_ty;
    assert(llvm_lower_expression(&at_aggr_assign, &val) == llvm_gen_be_error_t::ok);

    // 3. Fixed-point binary arithmetic (llvm_lower_fixed_point_binop)
    llvm::Value* fx_lhs = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 100);
    llvm::Value* fx_rhs = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 25);
    assert(llvm_lower_fixed_point_binop(eok_add, true, false, 15, nullptr, fx_rhs, &val) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_fixed_point_binop(eok_add, true, false, 15, fx_lhs, nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_fixed_point_binop(eok_add, true, false, 15, fx_lhs, fx_rhs, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_fixed_point_binop(eok_add, true, false, 15, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_add, true, true, 15, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_add, false, true, 16, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_subtract, true, false, 15, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_subtract, true, true, 15, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_subtract, false, true, 16, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_multiply, true, false, 15, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_multiply, false, false, 16, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_multiply, true, true, 15, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_multiply, false, true, 16, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_divide, true, false, 15, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_divide, false, false, 16, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_fixed_point_binop(eok_remainder, true, false, 15, fx_lhs, fx_rhs, &val) == llvm_gen_be_error_t::unsupported_expr);

    // Phase 18:
    // 1. Front-end UBSan checks (add/sub/mul overflow, div/rem zero, shiftl/shiftr bounds, null load)
    state.enable_ubsan_checks = true;
    int_kind_is_signed[ik_int] = TRUE;
    int_type.variant.integer.int_kind = ik_int;
    an_expr_node ubsan_op1 = {}, ubsan_op2 = {}, ubsan_bin = {};
    a_variable ubsan_var = {};
    ubsan_var.source_corresp.name = "ubsan_var";
    ubsan_var.type = &int_type;
    state.local_vars[&ubsan_var] = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));
    ubsan_op1.kind = enk_variable;
    ubsan_op1.type = &int_type;
    ubsan_op1.variant.variable.ptr = &ubsan_var;
    ubsan_op2 = ubsan_op1;
    ubsan_op1.next = &ubsan_op2;
    ubsan_bin.kind = enk_operation;
    ubsan_bin.type = &int_type;
    ubsan_bin.variant.operation.operands = &ubsan_op1;

    const an_expr_operator_kind ubsan_arith_ops[] = {
        eok_negate, eok_add, eok_subtract, eok_multiply, eok_divide, eok_remainder, eok_shiftl, eok_shiftr
    };
    assert(llvm_emit_ubsan_check(nullptr, val, {}) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_emit_ubsan_check("__ubsan_handle_out_of_bounds", nullptr, {}) == llvm_gen_be_error_t::invalid_argument);
    for (an_expr_operator_kind aop : ubsan_arith_ops) {
      ubsan_bin.variant.operation.kind = aop;
      assert(llvm_lower_expression(&ubsan_bin, &val) == llvm_gen_be_error_t::ok);
    }
    // Also test unsigned negate/add/sub/mul and non-tk_integer with enable_ubsan_checks == true
    int_kind_is_signed[ik_int] = FALSE;
    for (an_expr_operator_kind aop : {eok_negate, eok_add, eok_subtract, eok_multiply}) {
      ubsan_bin.variant.operation.kind = aop;
      assert(llvm_lower_expression(&ubsan_bin, &val) == llvm_gen_be_error_t::ok);
    }
    int_kind_is_signed[ik_int] = TRUE;
    a_type enum_int_ty = int_type;
    enum_int_ty.kind = tk_enum;
    state.type_cache[&enum_int_ty] = llvm::Type::getInt32Ty(*state.context);
    ubsan_op1.type = &enum_int_ty;
    for (an_expr_operator_kind aop : {eok_add, eok_subtract, eok_multiply}) {
      ubsan_bin.variant.operation.kind = aop;
      assert(llvm_lower_expression(&ubsan_bin, &val) == llvm_gen_be_error_t::ok);
    }
    ubsan_op1.type = &int_type;
    // Also test vector add/sub/mul/div/rem/shiftl/shiftr with enable_ubsan_checks == true
    llvm::Type* vec2i32_ty = llvm::FixedVectorType::get(llvm::Type::getInt32Ty(*state.context), 2);
    a_type edg_vec2i32_ty = {};
    edg_vec2i32_ty.kind = tk_integer;
    edg_vec2i32_ty.size = 8;
    state.type_cache[&edg_vec2i32_ty] = vec2i32_ty;
    a_variable ubsan_vec_var = {};
    ubsan_vec_var.type = &edg_vec2i32_ty;
    state.local_vars[&ubsan_vec_var] = state.builder->CreateAlloca(vec2i32_ty);
    ubsan_op1.type = &edg_vec2i32_ty;
    ubsan_op1.variant.variable.ptr = &ubsan_vec_var;
    ubsan_op2.type = &edg_vec2i32_ty;
    ubsan_op2.variant.variable.ptr = &ubsan_vec_var;
    ubsan_bin.type = &edg_vec2i32_ty;
    for (an_expr_operator_kind aop : ubsan_arith_ops) {
      ubsan_bin.variant.operation.kind = aop;
      assert(llvm_lower_expression(&ubsan_bin, &val) == llvm_gen_be_error_t::ok);
    }
    state.enable_ubsan_checks = false;

    // 2. TBAA metadata (!tbaa and !tbaa.struct) when opt_level != O0
    state.opt_level = llvm_opt_level_t::O2;
    ubsan_op1.type = &int_type;
    ubsan_op1.variant.variable.ptr = &ubsan_var;
    ubsan_op2.type = &int_type;
    ubsan_op2.variant.variable.ptr = &ubsan_var;
    ubsan_bin.type = &int_type;
    ubsan_bin.variant.operation.kind = eok_assign;
    assert(llvm_lower_expression(&ubsan_bin, &val) == llvm_gen_be_error_t::ok);

    // Float and pointer load/store with TBAA
    a_variable flt_tbaa_var = {};
    flt_tbaa_var.type = &float_type;
    state.local_vars[&flt_tbaa_var] = state.builder->CreateAlloca(llvm::Type::getFloatTy(*state.context));
    ubsan_op1.type = &float_type;
    ubsan_op1.variant.variable.ptr = &flt_tbaa_var;
    ubsan_op2.type = &float_type;
    ubsan_op2.variant.variable.ptr = &flt_tbaa_var;
    ubsan_bin.type = &float_type;
    assert(llvm_lower_expression(&ubsan_bin, &val) == llvm_gen_be_error_t::ok);

    ubsan_op1.type = &ptr_type;
    ubsan_op1.variant.variable.ptr = &va_v17;
    ubsan_op2.type = &ptr_type;
    ubsan_op2.variant.variable.ptr = &va_v17;
    ubsan_bin.type = &ptr_type;
    assert(llvm_lower_expression(&ubsan_bin, &val) == llvm_gen_be_error_t::ok);

    // Vector (non-scalar, size <= 16) load/store with opt_level == O2
    ubsan_op1.type = &edg_vec2i32_ty;
    ubsan_op1.variant.variable.ptr = &ubsan_vec_var;
    ubsan_op2.type = &edg_vec2i32_ty;
    ubsan_op2.variant.variable.ptr = &ubsan_vec_var;
    ubsan_bin.type = &edg_vec2i32_ty;
    assert(llvm_lower_expression(&ubsan_bin, &val) == llvm_gen_be_error_t::ok);

    // Aggregate (> 16 bytes) assignment with opt_level == O2 -> !tbaa.struct on memcpy
    state.type_cache[&aggr24_edg] = aggr24_llvm_ty;
    a_variable aggr24_plain_var = {};
    aggr24_plain_var.type = &aggr24_edg;
    state.local_vars[&aggr24_plain_var] = state.builder->CreateAlloca(aggr24_llvm_ty);
    ubsan_op1.type = &aggr24_edg;
    ubsan_op1.variant.variable.ptr = &aggr24_plain_var;
    ubsan_op2.type = &aggr24_edg;
    ubsan_op2.variant.variable.ptr = &aggr24_plain_var;
    ubsan_bin.type = &aggr24_edg;
    assert(llvm_lower_expression(&ubsan_bin, &val) == llvm_gen_be_error_t::ok);
    state.opt_level = llvm_opt_level_t::O0;

    // 3. Cast to void, lvalue eok_lvalue_adjust/eok_lvalue_cast/eok_question, eok_address_of with is_lvalue, and variadic call
    a_type void_edg_ty = {};
    void_edg_ty.kind = tk_void;
    state.type_cache[&void_edg_ty] = llvm::Type::getVoidTy(*state.context);
    an_expr_node void_cast_op = {};
    void_cast_op.kind = enk_operation;
    void_cast_op.type = &void_edg_ty;
    void_cast_op.variant.operation.kind = eok_cast;
    ubsan_op1.type = &int_type;
    ubsan_op1.variant.variable.ptr = &ubsan_var;
    ubsan_op1.next = nullptr;
    void_cast_op.variant.operation.operands = &ubsan_op1;
    assert(llvm_lower_expression(&void_cast_op, &val) == llvm_gen_be_error_t::ok);
    assert(val == nullptr);

    void_cast_op.type = &int_type;
    void_cast_op.variant.operation.kind = eok_lvalue_adjust;
    assert(llvm_lower_lvalue_expression(&void_cast_op, &val) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_expression(&void_cast_op, &val) == llvm_gen_be_error_t::ok);
    void_cast_op.variant.operation.kind = eok_lvalue_cast;
    assert(llvm_lower_lvalue_expression(&void_cast_op, &val) == llvm_gen_be_error_t::ok);

    // Lvalue eok_question (i32 cond, i1 cond, and error propagation on cond/true/false)
    an_expr_node lq_cond = {}, lq_true = {}, lq_false = {}, lq_expr = {};
    lq_cond.kind = enk_variable;
    lq_cond.type = &int_type;
    lq_cond.variant.variable.ptr = &ubsan_var;
    lq_true = lq_cond;
    lq_false = lq_cond;
    lq_cond.next = &lq_true;
    lq_true.next = &lq_false;
    lq_expr.kind = enk_operation;
    lq_expr.type = &int_type;
    lq_expr.is_lvalue = TRUE;
    lq_expr.variant.operation.kind = eok_question;
    lq_expr.variant.operation.operands = &lq_cond;
    assert(llvm_lower_lvalue_expression(&lq_expr, &val) == llvm_gen_be_error_t::ok);

    a_type i1_edg_ty = {};
    i1_edg_ty.kind = tk_integer;
    i1_edg_ty.size = 1;
    state.type_cache[&i1_edg_ty] = llvm::Type::getInt1Ty(*state.context);
    a_variable i1_var = {};
    i1_var.type = &i1_edg_ty;
    state.local_vars[&i1_var] = state.builder->CreateAlloca(llvm::Type::getInt1Ty(*state.context));
    lq_cond.type = &i1_edg_ty;
    lq_cond.variant.variable.ptr = &i1_var;
    assert(llvm_lower_lvalue_expression(&lq_expr, &val) == llvm_gen_be_error_t::ok);

    lq_false.variant.variable.ptr = nullptr;
    assert(llvm_lower_lvalue_expression(&lq_expr, &val) == llvm_gen_be_error_t::unsupported_expr);
    lq_false.variant.variable.ptr = &ubsan_var;
    lq_true.variant.variable.ptr = nullptr;
    assert(llvm_lower_lvalue_expression(&lq_expr, &val) == llvm_gen_be_error_t::unsupported_expr);
    lq_true.variant.variable.ptr = &ubsan_var;
    lq_cond.variant.variable.ptr = nullptr;
    assert(llvm_lower_lvalue_expression(&lq_expr, &val) == llvm_gen_be_error_t::unsupported_expr);
    lq_cond.variant.variable.ptr = &i1_var;

    // eok_address_of with op1->is_lvalue = TRUE and op1->kind == enk_operation (eok_lvalue_adjust, eok_question, and other)
    an_expr_node addr_lv_op = {};
    addr_lv_op.kind = enk_operation;
    addr_lv_op.type = &ptr_type;
    addr_lv_op.variant.operation.kind = eok_address_of;
    lq_expr.next = nullptr;
    addr_lv_op.variant.operation.operands = &lq_expr;
    assert(llvm_lower_expression(&addr_lv_op, &val) == llvm_gen_be_error_t::ok);

    lq_expr.is_lvalue = FALSE;
    assert(llvm_lower_expression(&addr_lv_op, &val) == llvm_gen_be_error_t::ok);
    lq_expr.is_lvalue = TRUE;

    void_cast_op.variant.operation.kind = eok_lvalue_adjust;
    void_cast_op.is_lvalue = FALSE;
    addr_lv_op.variant.operation.operands = &void_cast_op;
    assert(llvm_lower_expression(&addr_lv_op, &val) == llvm_gen_be_error_t::ok);

    void_cast_op.variant.operation.kind = eok_parens;
    assert(llvm_lower_expression(&addr_lv_op, &val) == llvm_gen_be_error_t::ok);

    // Variadic call where callee_arg_idx >= callee_ty->getNumParams()
    llvm::FunctionType* vararg_fty = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(*state.context), {llvm::Type::getInt32Ty(*state.context)}, true);
    llvm::Function::Create(vararg_fty, llvm::GlobalValue::ExternalLinkage, "vararg_test_fn", state.module.get());
    a_routine vararg_rout = {};
    vararg_rout.source_corresp.name = "vararg_test_fn";
    vararg_rout.type = &rout_ty;
    state.type_cache[&rout_ty] = vararg_fty;
    an_expr_node va_callee_node = {}, va_arg1 = {}, va_arg2 = {}, va_call = {};
    va_callee_node.kind = enk_routine;
    va_callee_node.type = &rout_ty;
    va_callee_node.variant.routine.ptr = &vararg_rout;
    va_arg1.kind = enk_variable;
    va_arg1.type = &int_type;
    va_arg1.variant.variable.ptr = &ubsan_var;
    va_arg2 = va_arg1;
    va_callee_node.next = &va_arg1;
    va_arg1.next = &va_arg2;
    va_call.kind = enk_operation;
    va_call.type = &int_type;
    va_call.variant.operation.kind = eok_call;
    va_call.variant.operation.operands = &va_callee_node;
    assert(llvm_lower_expression(&va_call, &val) == llvm_gen_be_error_t::ok);

    // 4. Phase 2 tests: llvm_compute_type_padding_mask, __builtin_clear_padding, llvm_lower_sizeof_pack,
    //    llvm_lower_fold_expression (all 32 operators, short-circuit &&/||, signed/unsigned/float),
    //    llvm_lower_new_expression (eok_new), and llvm_lower_delete_expression (eok_delete)
    assert(llvm_compute_type_padding_mask(nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    std::vector<uint8_t> pad_mask;
    assert(llvm_compute_type_padding_mask(nullptr, &pad_mask) == llvm_gen_be_error_t::ok);
    a_type zero_sz_struct = {};
    zero_sz_struct.kind = tk_struct;
    zero_sz_struct.size = 0;
    assert(llvm_compute_type_padding_mask(&zero_sz_struct, &pad_mask) == llvm_gen_be_error_t::ok);

    a_type base_pad_ty = {};
    base_pad_ty.kind = tk_integer;
    base_pad_ty.size = 2;
    a_base_class bc_pad1 = {}, bc_pad_null = {}, bc_pad_oob = {};
    bc_pad1.type = &base_pad_ty; bc_pad1.offset = 0; bc_pad1.next = &bc_pad_null;
    bc_pad_null.type = nullptr; bc_pad_null.next = &bc_pad_oob;
    bc_pad_oob.type = &base_pad_ty; bc_pad_oob.offset = 10; bc_pad_oob.next = nullptr;
    a_class_type_supplement pad_supp = {};
    pad_supp.base_classes = &bc_pad1;

    a_field pf_bf1 = {}, pf_bf_oob = {}, pf_norm = {}, pf_null = {}, pf_oob = {};
    pf_bf1.is_bit_field = TRUE; pf_bf1.offset = 2; pf_bf1.offset_bit_remainder = 0; pf_bf1.bit_size = 3; pf_bf1.next = &pf_bf_oob;
    pf_bf_oob.is_bit_field = TRUE; pf_bf_oob.offset = 8; pf_bf_oob.offset_bit_remainder = 0; pf_bf_oob.bit_size = 4; pf_bf_oob.next = &pf_norm;
    pf_norm.is_bit_field = FALSE; pf_norm.offset = 4; pf_norm.type = &base_pad_ty; pf_norm.next = &pf_null;
    pf_null.is_bit_field = FALSE; pf_null.offset = 6; pf_null.type = nullptr; pf_null.next = &pf_oob;
    pf_oob.is_bit_field = FALSE; pf_oob.offset = 10; pf_oob.type = &base_pad_ty; pf_oob.next = nullptr;

    a_type padded_cls = {};
    padded_cls.kind = tk_class;
    padded_cls.size = 8; // bytes 0..1 = 0xFF, byte 2 = 0x07 (partial bitfield), byte 3 = 0x00 (padding), bytes 4..5 = 0xFF, bytes 6..7 = 0x00
    padded_cls.variant.class_struct_union.extra_info = &pad_supp;
    padded_cls.variant.class_struct_union.field_list = &pf_bf1;

    a_type ptr_to_padded = {};
    ptr_to_padded.kind = tk_pointer;
    ptr_to_padded.size = 8;
    ptr_to_padded.variant.pointer.type = &padded_cls;
    state.type_cache[&ptr_to_padded] = llvm::PointerType::getUnqual(*state.context);
    state.type_cache[&ptr_type] = llvm::PointerType::getUnqual(*state.context);

    an_expr_node pad_ptr_arg = va_var_node;
    pad_ptr_arg.type = &ptr_to_padded;
    assert(call_test_builtin("__builtin_clear_padding", &int_type, {&pad_ptr_arg}) == llvm_gen_be_error_t::ok);
    pad_ptr_arg.type = &ptr_type;
    ptr_type.variant.pointer.type = nullptr; // pointer to null type fallback
    assert(call_test_builtin("__builtin_clear_padding", &int_type, {&pad_ptr_arg}) == llvm_gen_be_error_t::ok);
    ptr_type.variant.pointer.type = &int_type;
    // Also test non-pointer type and tk_struct without extra_info directly on llvm_compute_type_padding_mask
    assert(llvm_compute_type_padding_mask(&int_type, &pad_mask) == llvm_gen_be_error_t::ok);
    a_type plain_struct_pad = {};
    plain_struct_pad.kind = tk_struct;
    plain_struct_pad.size = 4;
    plain_struct_pad.variant.class_struct_union.extra_info = nullptr;
    assert(llvm_compute_type_padding_mask(&plain_struct_pad, &pad_mask) == llvm_gen_be_error_t::ok);

    // llvm_lower_sizeof_pack
    assert(llvm_lower_sizeof_pack(nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
    an_expr_node sp_node = {}, sp_elem1 = {}, sp_elem2 = {};
    sp_node.kind = enk_sizeof_pack;
    sp_node.type = &int_type;
    assert(llvm_lower_sizeof_pack(&sp_node, nullptr) == llvm_gen_be_error_t::invalid_argument);
    sp_node.next = &sp_elem1;
    sp_elem1.next = &sp_elem2;
    sp_elem2.next = nullptr;
    assert(llvm_lower_expression(&sp_node, &val) == llvm_gen_be_error_t::ok);
    sp_node.next = nullptr;

    // llvm_lower_fold_expression
    assert(llvm_lower_fold_expression(nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
    an_expr_node fold_node = {}, fold_op1 = {}, fold_op2 = {};
    fold_node.kind = enk_fold;
    fold_node.type = &int_type;
    assert(llvm_lower_fold_expression(&fold_node, nullptr) == llvm_gen_be_error_t::invalid_argument);
    fold_op1.kind = enk_variable;
    fold_op1.type = &int_type;
    fold_op1.variant.variable.ptr = &ubsan_var;
    fold_op2 = fold_op1;
    fold_op1.next = &fold_op2;
    fold_op2.next = nullptr;
    fold_node.variant.fold.operands = &fold_op1;

    const a_token_kind fold_tokens[] = {
        tok_plus, tok_plus_assign, tok_minus, tok_minus_assign,
        tok_star, tok_times_assign, tok_divide, tok_divide_assign,
        tok_remainder, tok_remainder_assign, tok_ampersand, tok_and_assign,
        tok_or, tok_or_assign, tok_excl_or, tok_excl_or_assign,
        tok_shift_left, tok_shift_left_assign, tok_shift_right, tok_shift_right_assign,
        tok_eq, tok_ne, tok_lt, tok_le, tok_gt, tok_ge,
        tok_and_and, tok_or_or,
        tok_comma, tok_assign, tok_period_star, tok_arrow_star,
        tok_compl
    };
    for (a_token_kind ftk : fold_tokens) {
      fold_node.variant.fold.operator_token = ftk;
      assert(llvm_lower_expression(&fold_node, &val) == llvm_gen_be_error_t::ok);
    }
    // Unsigned integer fold for /, %, >>, <, <=, >, >=, and non-tk_integer fold
    int_kind_is_signed[ik_int] = FALSE;
    for (a_token_kind ftk : {tok_divide, tok_remainder, tok_shift_right, tok_lt, tok_le, tok_gt, tok_ge}) {
      fold_node.variant.fold.operator_token = ftk;
      assert(llvm_lower_expression(&fold_node, &val) == llvm_gen_be_error_t::ok);
    }
    int_kind_is_signed[ik_int] = TRUE;
    fold_op1.type = &enum_int_ty;
    fold_node.variant.fold.operator_token = tok_divide;
    assert(llvm_lower_expression(&fold_node, &val) == llvm_gen_be_error_t::ok);

    // Floating-point fold for +, -, *, /, %, ==, !=, <, <=, >, >=, &&
    fold_op1.type = &float_type;
    fold_op1.variant.variable.ptr = &flt_tbaa_var;
    fold_op2.type = &float_type;
    fold_op2.variant.variable.ptr = &flt_tbaa_var;
    fold_node.type = &float_type;
    for (a_token_kind ftk : {tok_plus, tok_minus, tok_star, tok_divide, tok_remainder,
                             tok_eq, tok_ne, tok_lt, tok_le, tok_gt, tok_ge}) {
      fold_node.variant.fold.operator_token = ftk;
      assert(llvm_lower_expression(&fold_node, &val) == llvm_gen_be_error_t::ok);
    }
    // Pointer fold for && (non-integer acc type in &&/||!)
    fold_op1.type = &ptr_type;
    fold_op1.variant.variable.ptr = &va_v17;
    fold_op2.type = &ptr_type;
    fold_op2.variant.variable.ptr = &va_v17;
    fold_node.type = &ptr_type;
    fold_node.variant.fold.operator_token = tok_and_and;
    assert(llvm_lower_expression(&fold_node, &val) == llvm_gen_be_error_t::ok);
    // Error on rhs inside && fold and inside non-logical fold
    fold_op2.variant.variable.ptr = nullptr;
    assert(llvm_lower_expression(&fold_node, &val) == llvm_gen_be_error_t::unsupported_expr);
    fold_node.variant.fold.operator_token = tok_plus;
    assert(llvm_lower_expression(&fold_node, &val) == llvm_gen_be_error_t::unsupported_expr);
    fold_op2.variant.variable.ptr = &va_v17;

    // Unlowered new and delete via llvm_lower_new_expression / llvm_lower_delete_expression
    assert(llvm_lower_new_expression(nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_delete_expression(nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
    an_expr_node unlow_new = {}, unlow_del = {};
    unlow_new.kind = enk_operation;
    unlow_new.type = &ptr_type;
    assert(llvm_lower_new_expression(&unlow_new, nullptr) == llvm_gen_be_error_t::invalid_argument);
    unlow_new.variant.operation.operands = &va_arg1;
    va_arg1.next = nullptr;
    assert(llvm_lower_new_expression(&unlow_new, &val) == llvm_gen_be_error_t::ok);
    va_arg1.variant.variable.ptr = nullptr;
    assert(llvm_lower_new_expression(&unlow_new, &val) == llvm_gen_be_error_t::unsupported_expr);
    va_arg1.variant.variable.ptr = &ubsan_var;

    unlow_new.variant.operation.operands = nullptr;
    ptr_type.variant.pointer.type = &int_type;
    int_type.size = 4;
    assert(llvm_lower_new_expression(&unlow_new, &val) == llvm_gen_be_error_t::ok);
    int_type.size = 0;
    assert(llvm_lower_new_expression(&unlow_new, &val) == llvm_gen_be_error_t::ok);
    int_type.size = 4;
    ptr_type.variant.pointer.type = nullptr;
    assert(llvm_lower_new_expression(&unlow_new, &val) == llvm_gen_be_error_t::ok);
    unlow_new.type = &int_type;
    assert(llvm_lower_new_expression(&unlow_new, &val) == llvm_gen_be_error_t::ok);
    unlow_new.type = nullptr;
    assert(llvm_lower_new_expression(&unlow_new, &val) == llvm_gen_be_error_t::ok);
    ptr_type.variant.pointer.type = &int_type;

    unlow_del.kind = enk_operation;
    unlow_del.type = &void_edg_ty;
    assert(llvm_lower_delete_expression(&unlow_del, nullptr) == llvm_gen_be_error_t::invalid_argument);
    unlow_del.variant.operation.operands = nullptr;
    assert(llvm_lower_delete_expression(&unlow_del, &val) == llvm_gen_be_error_t::invalid_argument);
    an_expr_node ptr_del_op = {};
    ptr_del_op.kind = enk_variable;
    ptr_del_op.type = &ptr_type;
    ptr_del_op.variant.variable.ptr = &va_v17;
    unlow_del.variant.operation.operands = &ptr_del_op;
    assert(llvm_lower_delete_expression(&unlow_del, &val) == llvm_gen_be_error_t::ok);
    ptr_del_op.variant.variable.ptr = nullptr;
    assert(llvm_lower_delete_expression(&unlow_del, &val) == llvm_gen_be_error_t::unsupported_expr);

    an_expr_node nd_null_expr = {};
    nd_null_expr.kind = enk_new_delete;
    nd_null_expr.variant.new_delete = nullptr;
    assert(llvm_lower_new_expression(&nd_null_expr, &val) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_delete_expression(&nd_null_expr, &val) == llvm_gen_be_error_t::invalid_argument);

    // Phase 1.3: Array new[] and delete[] with over-aligned cookie (alignment == 16) and sized array delete (_ZdaPvm)
    a_type aligned16_cls = {};
    aligned16_cls.kind = tk_class;
    aligned16_cls.size = 777;
    aligned16_cls.alignment = 16;
    state.type_cache[&aligned16_cls] = llvm::StructType::get(*state.context, {llvm::Type::getInt64Ty(*state.context), llvm::Type::getInt64Ty(*state.context)});

    a_routine new_arr_rout = {}, del_arr_sized_rout = {};
    new_arr_rout.source_corresp.name = "_Znam";
    del_arr_sized_rout.source_corresp.name = "_ZdaPvm";

    a_new_delete_supplement nd_arr_aligned = {};
    nd_arr_aligned.routine = &new_arr_rout;
    nd_arr_aligned.type = &aligned16_cls;
    nd_arr_aligned.number_of_elements = &va_arg1;
    va_arg1.next = nullptr;
    an_expr_node new_arr_aligned_expr = {};
    new_arr_aligned_expr.kind = enk_new_delete;
    new_arr_aligned_expr.type = &ptr_type;
    new_arr_aligned_expr.variant.new_delete = &nd_arr_aligned;
    assert(llvm_lower_new_expression(&new_arr_aligned_expr, &val) == llvm_gen_be_error_t::ok);

    ptr_del_op.variant.variable.ptr = &va_v17;
    ptr_del_op.next = nullptr;
    a_new_delete_supplement nd_del_aligned = {};
    nd_del_aligned.routine = &del_arr_sized_rout;
    nd_del_aligned.type = &aligned16_cls;
    nd_del_aligned.array_delete = TRUE;
    nd_del_aligned.arg = &ptr_del_op;
    an_expr_node del_arr_aligned_expr = {};
    del_arr_aligned_expr.kind = enk_new_delete;
    del_arr_aligned_expr.type = &void_edg_ty;
    del_arr_aligned_expr.variant.new_delete = &nd_del_aligned;
    assert(llvm_lower_delete_expression(&del_arr_aligned_expr, &val) == llvm_gen_be_error_t::ok);

    // Also test when get_llvm_type(nd->type, &elem_llvm_ty) fails in array_delete
    a_type bad_elem_cls = {};
    bad_elem_cls.kind = static_cast<a_type_kind>(999);
    bad_elem_cls.size = 777;
    bad_elem_cls.alignment = 8;
    nd_del_aligned.type = &bad_elem_cls;
    assert(llvm_lower_delete_expression(&del_arr_aligned_expr, &val) != llvm_gen_be_error_t::ok);

    // Phase 5: Dynamic atomic fence ordering & C11 _Atomic compound assignments / increment / decrement
    a_variable p5_iv = {}, p5_fv = {};
    p5_iv.type = &int_type;
    p5_fv.type = &float_type;
    state.local_vars[&p5_iv] = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));
    state.local_vars[&p5_fv] = state.builder->CreateAlloca(llvm::Type::getFloatTy(*state.context));

    a_routine dyn_fence_r = {};
    dyn_fence_r.source_corresp.name = "__atomic_thread_fence";
    an_expr_node dyn_fence_callee = {};
    dyn_fence_callee.kind = enk_routine;
    dyn_fence_callee.variant.routine.ptr = &dyn_fence_r;
    an_expr_node dyn_fence_arg = {};
    dyn_fence_arg.kind = enk_variable;
    dyn_fence_arg.type = &int_type;
    dyn_fence_arg.variant.variable.ptr = &p5_iv;
    dyn_fence_callee.next = &dyn_fence_arg;
    an_expr_node dyn_fence_call = {};
    dyn_fence_call.kind = enk_operation;
    dyn_fence_call.type = &void_edg_ty;
    dyn_fence_call.variant.operation.kind = eok_call;
    dyn_fence_call.variant.operation.operands = &dyn_fence_callee;
    assert(llvm_lower_expression(&dyn_fence_call, &val) == llvm_gen_be_error_t::ok);

    a_type atomic_int_tref = {};
    atomic_int_tref.kind = tk_typeref;
    atomic_int_tref.variant.typeref.type = &int_type;
    atomic_int_tref.variant.typeref.qualifiers = TQ_C11_ATOMIC;
    a_type atomic_flt_tref = {};
    atomic_flt_tref.kind = tk_typeref;
    atomic_flt_tref.variant.typeref.type = &float_type;
    atomic_flt_tref.variant.typeref.qualifiers = TQ_C11_ATOMIC;

    a_variable atomic_iv = {}, atomic_fv = {};
    atomic_iv.type = &atomic_int_tref;
    atomic_fv.type = &atomic_flt_tref;
    state.local_vars[&atomic_iv] = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));
    state.local_vars[&atomic_fv] = state.builder->CreateAlloca(llvm::Type::getFloatTy(*state.context));

    an_expr_node a_ilhs = {}, a_irhs = {}, a_flhs = {}, a_frhs = {};
    a_ilhs.kind = enk_variable; a_ilhs.type = &atomic_int_tref; a_ilhs.variant.variable.ptr = &atomic_iv;
    a_irhs.kind = enk_variable; a_irhs.type = &int_type; a_irhs.variant.variable.ptr = &p5_iv;
    a_flhs.kind = enk_variable; a_flhs.type = &atomic_flt_tref; a_flhs.variant.variable.ptr = &atomic_fv;
    a_frhs.kind = enk_variable; a_frhs.type = &float_type; a_frhs.variant.variable.ptr = &p5_fv;

    an_expr_operator_kind atomic_i_ops[] = {
        eok_pre_incr, eok_post_incr, eok_pre_decr, eok_post_decr,
        eok_add_assign, eok_subtract_assign, eok_and_assign, eok_or_assign, eok_xor_assign, eok_multiply_assign
    };
    for (an_expr_operator_kind ak : atomic_i_ops) {
      an_expr_node a_ce = {};
      a_ce.kind = enk_operation;
      a_ce.type = &int_type;
      a_ce.variant.operation.kind = ak;
      a_ilhs.next = (ak == eok_pre_incr || ak == eok_post_incr || ak == eok_pre_decr || ak == eok_post_decr) ? nullptr : &a_irhs;
      a_ce.variant.operation.operands = &a_ilhs;
      assert(llvm_lower_compound_assignment(&a_ce, &val) == llvm_gen_be_error_t::ok);
    }
    an_expr_operator_kind atomic_f_ops[] = {
        eok_pre_incr, eok_post_incr, eok_pre_decr, eok_post_decr, eok_add_assign, eok_subtract_assign
    };
    for (an_expr_operator_kind ak : atomic_f_ops) {
      an_expr_node a_ce = {};
      a_ce.kind = enk_operation;
      a_ce.type = &float_type;
      a_ce.variant.operation.kind = ak;
      a_flhs.next = (ak == eok_pre_incr || ak == eok_post_incr || ak == eok_pre_decr || ak == eok_post_decr) ? nullptr : &a_frhs;
      a_ce.variant.operation.operands = &a_flhs;
      assert(llvm_lower_compound_assignment(&a_ce, &val) == llvm_gen_be_error_t::ok);
    }

    // Phase 6: Extended complex libcalls (x86_fp80 __mulxc3/__divxc3, fp128 & ppc_fp128 __multc3/__divtc3)
    a_type cplx_fp80_ty = {}, cplx_fp128_ty = {}, cplx_ppcfp128_ty = {};
    cplx_fp80_ty.kind = tk_complex; cplx_fp80_ty.size = 32;
    cplx_fp128_ty.kind = tk_complex; cplx_fp128_ty.size = 32;
    cplx_ppcfp128_ty.kind = tk_complex; cplx_ppcfp128_ty.size = 32;
    llvm::Type* fp80_lt = llvm::Type::getX86_FP80Ty(*state.context);
    llvm::Type* fp128_lt = llvm::Type::getFP128Ty(*state.context);
    llvm::Type* ppc128_lt = llvm::Type::getPPC_FP128Ty(*state.context);
    state.type_cache[&cplx_fp80_ty] = llvm::StructType::get(*state.context, {fp80_lt, fp80_lt});
    state.type_cache[&cplx_fp128_ty] = llvm::StructType::get(*state.context, {fp128_lt, fp128_lt});
    state.type_cache[&cplx_ppcfp128_ty] = llvm::StructType::get(*state.context, {ppc128_lt, ppc128_lt});

    a_type_ptr ext_cplx_tys[3] = {&cplx_fp80_ty, &cplx_fp128_ty, &cplx_ppcfp128_ty};
    for (int ci = 0; ci < 3; ++ci) {
      a_variable cv1 = {}, cv2 = {};
      cv1.type = ext_cplx_tys[ci]; cv2.type = ext_cplx_tys[ci];
      state.local_vars[&cv1] = state.builder->CreateAlloca(state.type_cache[ext_cplx_tys[ci]]);
      state.local_vars[&cv2] = state.builder->CreateAlloca(state.type_cache[ext_cplx_tys[ci]]);
      an_expr_node cop1 = {}, cop2 = {}, cmul = {}, cdiv = {};
      cop1.kind = enk_variable; cop1.type = ext_cplx_tys[ci]; cop1.variant.variable.ptr = &cv1; cop1.next = &cop2;
      cop2.kind = enk_variable; cop2.type = ext_cplx_tys[ci]; cop2.variant.variable.ptr = &cv2;
      cmul.kind = enk_operation; cmul.type = ext_cplx_tys[ci]; cmul.variant.operation.kind = eok_multiply; cmul.variant.operation.operands = &cop1;
      cdiv.kind = enk_operation; cdiv.type = ext_cplx_tys[ci]; cdiv.variant.operation.kind = eok_divide; cdiv.variant.operation.operands = &cop1;
      assert(llvm_lower_arithmetic_expression(&cmul, &val) == llvm_gen_be_error_t::ok);
      assert(llvm_lower_arithmetic_expression(&cdiv, &val) == llvm_gen_be_error_t::ok);
    }

#if FIXED_POINT_ALLOWED
    // Phase 8: Fixed-point arithmetic & casts via llvm_lower_arithmetic_expression and llvm_lower_cast_expression
    a_type fx_sfract = {}, fx_ufract_sat = {}, fx_saccum = {}, fx_unsized = {};
    fx_sfract.kind = tk_fixed_point; fx_sfract.size = 4;
    fx_sfract.variant.fixed_point.is_fract_type = TRUE; fx_sfract.variant.fixed_point.is_unsigned = FALSE; fx_sfract.variant.fixed_point.saturating = FALSE;
    fx_ufract_sat.kind = tk_fixed_point; fx_ufract_sat.size = 4;
    fx_ufract_sat.variant.fixed_point.is_fract_type = TRUE; fx_ufract_sat.variant.fixed_point.is_unsigned = TRUE; fx_ufract_sat.variant.fixed_point.saturating = TRUE;
    fx_saccum.kind = tk_fixed_point; fx_saccum.size = 4;
    fx_saccum.variant.fixed_point.is_fract_type = FALSE; fx_saccum.variant.fixed_point.is_unsigned = FALSE; fx_saccum.variant.fixed_point.saturating = TRUE;
    fx_unsized.kind = tk_fixed_point; fx_unsized.size = 0;
    fx_unsized.variant.fixed_point.is_fract_type = FALSE; fx_unsized.variant.fixed_point.is_unsigned = TRUE; fx_unsized.variant.fixed_point.saturating = FALSE;
    state.type_cache[&fx_sfract] = llvm::Type::getInt32Ty(*state.context);
    state.type_cache[&fx_ufract_sat] = llvm::Type::getInt32Ty(*state.context);
    state.type_cache[&fx_saccum] = llvm::Type::getInt32Ty(*state.context);
    state.type_cache[&fx_unsized] = llvm::Type::getInt32Ty(*state.context);

    an_expr_node fx_op1 = {}, fx_op2 = {}, fx_bin = {};
    fx_op1.kind = enk_variable; fx_op1.type = &fx_sfract; fx_op1.variant.variable.ptr = &p5_iv; fx_op1.next = &fx_op2;
    fx_op2.kind = enk_variable; fx_op2.type = &fx_sfract; fx_op2.variant.variable.ptr = &p5_iv;
    fx_bin.kind = enk_operation; fx_bin.type = &fx_sfract; fx_bin.variant.operation.kind = eok_add; fx_bin.variant.operation.operands = &fx_op1;
    assert(llvm_lower_arithmetic_expression(&fx_bin, &val) == llvm_gen_be_error_t::ok);
    fx_bin.type = &fx_ufract_sat; fx_bin.variant.operation.kind = eok_subtract;
    assert(llvm_lower_arithmetic_expression(&fx_bin, &val) == llvm_gen_be_error_t::ok);
    fx_bin.type = &fx_saccum; fx_bin.variant.operation.kind = eok_multiply;
    assert(llvm_lower_arithmetic_expression(&fx_bin, &val) == llvm_gen_be_error_t::ok);
    fx_bin.type = &fx_unsized; fx_bin.variant.operation.kind = eok_divide;
    assert(llvm_lower_arithmetic_expression(&fx_bin, &val) == llvm_gen_be_error_t::ok);

    // Fixed-point <-> floating-point casts (signed and unsigned)
    an_expr_node fx_cast = {};
    fx_cast.kind = enk_operation; fx_cast.variant.operation.kind = eok_cast;
    fx_op1.next = nullptr;
    fx_op1.type = &fx_sfract; fx_cast.type = &float_type; fx_cast.variant.operation.operands = &fx_op1;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);
    fx_op1.type = &fx_ufract_sat; fx_cast.type = &float_type;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);
    an_expr_node flt_cast_op = {};
    flt_cast_op.kind = enk_variable; flt_cast_op.type = &float_type; flt_cast_op.variant.variable.ptr = &p5_fv;
    fx_cast.variant.operation.operands = &flt_cast_op;
    fx_cast.type = &fx_sfract;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);
    fx_cast.type = &fx_ufract_sat;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);
#endif

    // Phase 9: UBSan __builtin_unreachable and module source filename resolution in UBSan / __builtin_FILE / __builtin_source_location
    state.module->setSourceFileName("real_unit.cpp");
    state.enable_ubsan_checks = true;
    a_routine unreach_r = {}, bfile_r = {}, bsloc_r = {};
    unreach_r.source_corresp.name = "__builtin_unreachable";
    bfile_r.source_corresp.name = "__builtin_FILE";
    bsloc_r.source_corresp.name = "__builtin_source_location";
    an_expr_node p9_callee = {}, p9_call = {};
    p9_callee.kind = enk_routine;
    p9_call.kind = enk_operation; p9_call.type = &ptr_type; p9_call.variant.operation.kind = eok_call; p9_call.variant.operation.operands = &p9_callee;
    p9_callee.variant.routine.ptr = &bfile_r;
    assert(llvm_lower_expression(&p9_call, &val) == llvm_gen_be_error_t::ok);
    p9_callee.variant.routine.ptr = &bsloc_r;
    assert(llvm_lower_expression(&p9_call, &val) == llvm_gen_be_error_t::ok);
    p9_callee.variant.routine.ptr = &unreach_r;
    assert(llvm_lower_expression(&p9_call, &val) == llvm_gen_be_error_t::ok);
    llvm::BasicBlock* after_unreach_bb = llvm::BasicBlock::Create(
        *state.context, "after.unreach", state.builder->GetInsertBlock()->getParent());
    state.builder->SetInsertPoint(after_unreach_bb);
    state.module->setSourceFileName("");
    state.enable_ubsan_checks = true;
    p9_callee.variant.routine.ptr = &unreach_r;
    assert(llvm_lower_expression(&p9_call, &val) == llvm_gen_be_error_t::ok);
    llvm::BasicBlock* after_unreach_bb2 = llvm::BasicBlock::Create(
        *state.context, "after.unreach2", state.builder->GetInsertBlock()->getParent());
    state.builder->SetInsertPoint(after_unreach_bb2);
    state.enable_ubsan_checks = false;
    p9_callee.variant.routine.ptr = &bfile_r;
    assert(llvm_lower_expression(&p9_call, &val) == llvm_gen_be_error_t::ok);
    p9_callee.variant.routine.ptr = &bsloc_r;
    assert(llvm_lower_expression(&p9_call, &val) == llvm_gen_be_error_t::ok);

    // Section 7.2: Extended evaluate_type_trait_builtin and C++17 over-aligned new/delete
    a_type trait_cls_triv = {}, trait_cls_virt = {}, trait_cls_vbase = {}, trait_cls_abs = {};
    trait_cls_triv.kind = tk_class; trait_cls_triv.size = 8;
    trait_cls_virt.kind = tk_struct; trait_cls_virt.size = 8; trait_cls_virt.variant.class_struct_union.any_virtual_functions = TRUE;
    trait_cls_vbase.kind = tk_class; trait_cls_vbase.size = 8; trait_cls_vbase.variant.class_struct_union.any_virtual_base_classes = TRUE;
    trait_cls_abs.kind = tk_class; trait_cls_abs.size = 8; trait_cls_abs.variant.class_struct_union.abstract = TRUE;

    an_expr_node tt_op1 = {}, tt_op2 = {}, tt_expr = {};
    tt_op1.kind = enk_type_operand;
    tt_op2.kind = enk_type_operand;
    tt_expr.kind = enk_builtin_operation;
    tt_expr.type = &int_type;
    tt_expr.variant.builtin_operation.operands = &tt_op1;

    const a_builtin_operation_kind layout_boks[] = {
        bok_is_standard_layout, bok_is_pod, bok_is_literal_type, bok_is_trivially_copyable, bok_is_trivial
    };
    a_type_ptr layout_types[] = {
        &int_type, &float_type, &ptr_type, &trait_cls_triv, &trait_cls_virt, &trait_cls_vbase, &void_edg_ty, nullptr
    };
    for (a_builtin_operation_kind lb : layout_boks) {
      tt_expr.variant.builtin_operation.kind = lb;
      for (a_type_ptr lt : layout_types) {
        tt_op1.variant.type_operand.type = lt;
        assert(llvm_lower_expression(&tt_expr, &val) == llvm_gen_be_error_t::ok);
      }
    }

    tt_expr.variant.builtin_operation.kind = bok_has_unique_object_representations;
    for (a_type_ptr ut : {&int_type, &padded_cls, &ptr_type, &void_edg_ty, &zero_sz_struct}) {
      tt_op1.variant.type_operand.type = ut;
      assert(llvm_lower_expression(&tt_expr, &val) == llvm_gen_be_error_t::ok);
    }

    const a_builtin_operation_kind ctor_boks[] = {
        bok_is_trivially_constructible, bok_is_constructible, bok_is_nothrow_constructible,
        bok_is_destructible, bok_is_nothrow_destructible, bok_is_trivially_destructible
    };
    for (a_builtin_operation_kind cb : ctor_boks) {
      tt_expr.variant.builtin_operation.kind = cb;
      for (a_type_ptr ct : {&int_type, &trait_cls_triv, &trait_cls_abs, &aligned16_cls, &void_edg_ty, &rout_ty}) {
        tt_op1.variant.type_operand.type = ct;
        assert(llvm_lower_expression(&tt_expr, &val) == llvm_gen_be_error_t::ok);
      }
    }

    const a_builtin_operation_kind assign_boks[] = {
        bok_is_assignable, bok_is_trivially_assignable, bok_is_nothrow_assignable,
        bok_is_trivially_copy_assignable, bok_is_assignable_no_precondition_check,
        bok_is_convertible_to, bok_is_convertible, bok_is_nothrow_convertible
    };
    tt_op1.next = &tt_op2;
    for (a_builtin_operation_kind ab : assign_boks) {
      tt_expr.variant.builtin_operation.kind = ab;
      tt_op1.variant.type_operand.type = &int_type;
      tt_op2.variant.type_operand.type = &int_type;
      assert(llvm_lower_expression(&tt_expr, &val) == llvm_gen_be_error_t::ok);
      tt_op1.variant.type_operand.type = &aligned16_cls;
      assert(llvm_lower_expression(&tt_expr, &val) == llvm_gen_be_error_t::ok);
      tt_op2.variant.type_operand.type = &void_edg_ty;
      assert(llvm_lower_expression(&tt_expr, &val) == llvm_gen_be_error_t::ok);
      tt_op1.variant.type_operand.type = &void_edg_ty;
      assert(llvm_lower_expression(&tt_expr, &val) == llvm_gen_be_error_t::ok);
    }
    tt_op1.next = nullptr;

    // C++17 over-aligned new/delete (alignment = 32 > 16)
    a_type aligned32_cls = {};
    aligned32_cls.kind = tk_struct;
    aligned32_cls.size = 777;
    aligned32_cls.alignment = 32;
    state.type_cache[&aligned32_cls] = llvm::StructType::get(
        *state.context, {llvm::Type::getInt64Ty(*state.context), llvm::Type::getInt64Ty(*state.context)});
    a_type ptr_aligned32_ty = {};
    ptr_aligned32_ty.kind = tk_pointer;
    ptr_aligned32_ty.size = 8;
    ptr_aligned32_ty.variant.pointer.type = &aligned32_cls;
    state.type_cache[&ptr_aligned32_ty] = llvm::PointerType::getUnqual(*state.context);

    unlow_new.type = &ptr_aligned32_ty;
    unlow_new.variant.operation.operands = nullptr;
    assert(llvm_lower_new_expression(&unlow_new, &val) == llvm_gen_be_error_t::ok);

    a_routine new_align_rout = {}, new_plain32_rout = {}, del_align_rout = {}, del_sized_rout = {}, del_arr_align_rout = {};
    new_align_rout.source_corresp.name = "_ZnwmSt11align_val_t";
    new_plain32_rout.source_corresp.name = "_Znwm";
    del_align_rout.source_corresp.name = "_ZdlPvmSt11align_val_t";
    del_sized_rout.source_corresp.name = "_ZdlPvm";
    del_arr_align_rout.source_corresp.name = "_ZdaPvmSt11align_val_t";

    a_new_delete_supplement nd_new32 = {};
    nd_new32.is_new = TRUE;
    nd_new32.routine = &new_align_rout;
    nd_new32.type = &aligned32_cls;
    an_expr_node new32_expr = {};
    new32_expr.kind = enk_new_delete;
    new32_expr.type = &ptr_aligned32_ty;
    new32_expr.variant.new_delete = &nd_new32;
    assert(llvm_lower_new_expression(&new32_expr, &val) == llvm_gen_be_error_t::ok);
    nd_new32.routine = &new_plain32_rout;
    assert(llvm_lower_new_expression(&new32_expr, &val) == llvm_gen_be_error_t::ok);

    ptr_del_op.variant.variable.ptr = &va_v17;
    ptr_del_op.next = nullptr;
    a_new_delete_supplement nd_del32 = {};
    nd_del32.routine = &del_align_rout;
    nd_del32.type = &aligned32_cls;
    nd_del32.arg = &ptr_del_op;
    an_expr_node del32_expr = {};
    del32_expr.kind = enk_new_delete;
    del32_expr.type = &void_edg_ty;
    del32_expr.variant.new_delete = &nd_del32;
    assert(llvm_lower_delete_expression(&del32_expr, &val) == llvm_gen_be_error_t::ok);

    nd_del32.routine = &del_sized_rout;
    assert(llvm_lower_delete_expression(&del32_expr, &val) == llvm_gen_be_error_t::ok);

    nd_del32.routine = &del_arr_align_rout;
    nd_del32.array_delete = TRUE;
    assert(llvm_lower_delete_expression(&del32_expr, &val) == llvm_gen_be_error_t::ok);

    // Phase 5: UBSan TypeDescriptor, DebugLoc SourceLocation, invalid-bool load, and float-cast-overflow
    llvm::GlobalVariable* ubsan_td = nullptr;
    assert(llvm_emit_ubsan_type_descriptor(&int_type, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_emit_ubsan_type_descriptor(nullptr, &ubsan_td) == llvm_gen_be_error_t::ok);
    assert(ubsan_td != nullptr);
    int_type.source_corresp.name = (char*)"int";
    int_type.size = 4;
    assert(llvm_emit_ubsan_type_descriptor(&int_type, &ubsan_td) == llvm_gen_be_error_t::ok);
    int_type.size = 0;
    int_type.source_corresp.name = nullptr;
    assert(llvm_emit_ubsan_type_descriptor(&int_type, &ubsan_td) == llvm_gen_be_error_t::ok);
    int_type.size = 4;

    a_type flt_td_ty = {};
    flt_td_ty.kind = tk_float;
    flt_td_ty.size = 4;
    assert(llvm_emit_ubsan_type_descriptor(&flt_td_ty, &ubsan_td) == llvm_gen_be_error_t::ok);
    assert(llvm_emit_ubsan_type_descriptor(&aligned32_cls, &ubsan_td) == llvm_gen_be_error_t::ok);

    // Test UBSan check with active DebugLoc, invalid bool load, and float-to-int cast
    llvm::DIBuilder dib(*state.module);
    llvm::DIFile* dif = dib.createFile("ubsan_test.cpp", ".");
    llvm::DISubprogram* disp = dib.createFunction(
        dif, "ubsan_fn", "ubsan_fn", dif, 10, dib.createSubroutineType(dib.getOrCreateTypeArray({})), 10,
        llvm::DINode::FlagZero, llvm::DISubprogram::SPFlagDefinition);
    state.builder->SetCurrentDebugLocation(llvm::DILocation::get(*state.context, 42, 7, disp));
    state.enable_ubsan_checks = true;

    a_type bool_load_ty = {};
    bool_load_ty.kind = tk_integer;
    bool_load_ty.variant.integer.bool_type = TRUE;
    bool_load_ty.size = 1;
    state.type_cache[&bool_load_ty] = llvm::Type::getInt8Ty(*state.context);
    a_variable bool_var = {};
    bool_var.type = &bool_load_ty;
    state.local_vars[&bool_var] = state.builder->CreateAlloca(llvm::Type::getInt8Ty(*state.context));
    an_expr_node bool_var_expr = {};
    bool_var_expr.kind = enk_variable;
    bool_var_expr.type = &bool_load_ty;
    bool_var_expr.variant.variable.ptr = &bool_var;
    assert(llvm_lower_expression(&bool_var_expr, &val) == llvm_gen_be_error_t::ok);

    // Also test is_bool_type == TRUE with non-i8 type (e.g. i32) to cover false branch of isIntegerTy(8)
    state.type_cache[&bool_load_ty] = llvm::Type::getInt32Ty(*state.context);
    state.local_vars[&bool_var] = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));
    assert(llvm_lower_expression(&bool_var_expr, &val) == llvm_gen_be_error_t::ok);

    state.type_cache[&flt_td_ty] = llvm::Type::getFloatTy(*state.context);
    a_variable flt_var = {};
    flt_var.type = &flt_td_ty;
    state.local_vars[&flt_var] = state.builder->CreateAlloca(llvm::Type::getFloatTy(*state.context));
    an_expr_node flt_op = {};
    flt_op.kind = enk_variable;
    flt_op.type = &flt_td_ty;
    flt_op.variant.variable.ptr = &flt_var;
    an_expr_node fp_cast_expr = {};
    fp_cast_expr.kind = enk_operation;
    fp_cast_expr.type = &int_type;
    fp_cast_expr.variant.operation.kind = eok_cast;
    fp_cast_expr.variant.operation.operands = &flt_op;
    assert(llvm_lower_cast_expression(&fp_cast_expr, &val) == llvm_gen_be_error_t::ok);

    state.enable_ubsan_checks = false;
    state.builder->SetCurrentDebugLocation(llvm::DebugLoc());

    // Phase 9: Test addrspacecast in llvm_lower_cast_expression
    a_type ptr_as0_ty = {};
    ptr_as0_ty.kind = tk_pointer;
    a_type ptr_as3_ty = {};
    ptr_as3_ty.kind = tk_pointer;
    state.type_cache[&ptr_as0_ty] = llvm::PointerType::get(*state.context, 0);
    state.type_cache[&ptr_as3_ty] = llvm::PointerType::get(*state.context, 3);
    a_variable ptr_as0_var = {};
    ptr_as0_var.type = &ptr_as0_ty;
    state.local_vars[&ptr_as0_var] = state.builder->CreateAlloca(llvm::PointerType::get(*state.context, 0));
    an_expr_node ptr_as0_op = {};
    ptr_as0_op.kind = enk_variable;
    ptr_as0_op.type = &ptr_as0_ty;
    ptr_as0_op.variant.variable.ptr = &ptr_as0_var;
    an_expr_node as_cast_expr = {};
    as_cast_expr.kind = enk_operation;
    as_cast_expr.type = &ptr_as3_ty;
    as_cast_expr.variant.operation.kind = eok_cast;
    as_cast_expr.variant.operation.operands = &ptr_as0_op;
    assert(llvm_lower_cast_expression(&as_cast_expr, &val) == llvm_gen_be_error_t::ok);

    // Test enk_constant lvalue expression (ck_string success, non-string error, null constant error)
    a_constant str_lval_con = {};
    str_lval_con.kind = ck_string;
    an_expr_node con_lval_expr = {};
    con_lval_expr.kind = enk_constant;
    con_lval_expr.type = &ptr_type;
    con_lval_expr.variant.constant.ptr = nullptr;
    assert(llvm_lower_lvalue_expression(&con_lval_expr, &val) == llvm_gen_be_error_t::unsupported_expr);
    a_constant int_lval_con = {};
    int_lval_con.kind = ck_integer;
    con_lval_expr.variant.constant.ptr = &int_lval_con;
    assert(llvm_lower_lvalue_expression(&con_lval_expr, &val) == llvm_gen_be_error_t::unsupported_expr);
    con_lval_expr.variant.constant.ptr = &str_lval_con;
    assert(llvm_lower_lvalue_expression(&con_lval_expr, &val) == llvm_gen_be_error_t::ok);

    // Test eok_array_to_pointer in llvm_lower_expression and eok_subscript on tk_array in llvm_lower_lvalue_expression
    a_type arr_1d_ty = {};
    arr_1d_ty.kind = tk_array;
    arr_1d_ty.size = 16;
    arr_1d_ty.variant.array.element_type = &int_type;
    arr_1d_ty.variant.array.variant.number_of_elements = 4;
    state.type_cache[&arr_1d_ty] = llvm::ArrayType::get(llvm::Type::getInt32Ty(*state.context), 4);
    a_variable arr_1d_var = {};
    arr_1d_var.type = &arr_1d_ty;
    state.local_vars[&arr_1d_var] = state.builder->CreateAlloca(state.type_cache[&arr_1d_ty]);
    an_expr_node arr_1d_op = {};
    arr_1d_op.kind = enk_variable;
    arr_1d_op.type = &arr_1d_ty;
    arr_1d_op.variant.variable.ptr = &arr_1d_var;

    an_expr_node arr_to_ptr_expr = {};
    arr_to_ptr_expr.kind = enk_operation;
    arr_to_ptr_expr.type = &ptr_type;
    arr_to_ptr_expr.variant.operation.kind = eok_array_to_pointer;
    arr_to_ptr_expr.variant.operation.operands = &arr_1d_op;
    assert(llvm_lower_expression(&arr_to_ptr_expr, &val) == llvm_gen_be_error_t::ok);

    an_expr_node idx_op = static_op1;
    idx_op.next = nullptr;
    arr_1d_op.next = &idx_op;
    an_expr_node sub_arr_expr = {};
    sub_arr_expr.kind = enk_operation;
    sub_arr_expr.type = &int_type;
    sub_arr_expr.variant.operation.kind = eok_subscript;
    sub_arr_expr.variant.operation.operands = &arr_1d_op;
    assert(llvm_lower_expression(&sub_arr_expr, &val) == llvm_gen_be_error_t::ok);
    arr_1d_op.next = nullptr;

    // Test compound assignment (+= and <<=) where integer RHS has different bitwidth (i64) than LHS (i32)
    a_variable lhs_i32_var = {};
    lhs_i32_var.type = &int_type;
    state.local_vars[&lhs_i32_var] = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));
    a_variable i64_rhs_var = {};
    i64_rhs_var.type = &i64_ty;
    state.type_cache[&i64_ty] = llvm::Type::getInt64Ty(*state.context);
    state.local_vars[&i64_rhs_var] = state.builder->CreateAlloca(llvm::Type::getInt64Ty(*state.context));
    an_expr_node lhs_i32_op = {};
    lhs_i32_op.kind = enk_variable;
    lhs_i32_op.type = &int_type;
    lhs_i32_op.variant.variable.ptr = &lhs_i32_var;
    an_expr_node rhs_i64_op = {};
    rhs_i64_op.kind = enk_variable;
    rhs_i64_op.type = &i64_ty;
    rhs_i64_op.variant.variable.ptr = &i64_rhs_var;
    lhs_i32_op.next = &rhs_i64_op;
    an_expr_node comp_cast_expr = {};
    comp_cast_expr.kind = enk_operation;
    comp_cast_expr.type = &int_type;
    comp_cast_expr.variant.operation.kind = eok_add_assign;
    comp_cast_expr.variant.operation.operands = &lhs_i32_op;
    assert(llvm_lower_compound_assignment(&comp_cast_expr, &val) == llvm_gen_be_error_t::ok);
    comp_cast_expr.variant.operation.kind = eok_shiftl_assign;
    assert(llvm_lower_compound_assignment(&comp_cast_expr, &val) == llvm_gen_be_error_t::ok);

    // Test fortified __builtin___*_chk calls and __va_start
    assert(call_test_builtin("__builtin___memcpy_chk", &ptr_type, {&va_var_node, &va_var_node, &rhs_i64_op}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin___memmove_chk", &ptr_type, {&va_var_node, &va_var_node, &rhs_i64_op}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin___memset_chk", &ptr_type, {&va_var_node, &lhs_i32_op, &rhs_i64_op}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin___snprintf_chk", &int_type, {&va_var_node, &rhs_i64_op}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__builtin___custom_no_suffix", &int_type, {&lhs_i32_op}) == llvm_gen_be_error_t::ok);
    assert(call_test_builtin("__va_start", &ptr_type, {&va_var_node}) == llvm_gen_be_error_t::ok);

    // Additional coverage for lvalue eok_array_to_pointer, default lvalue kind, eok_indirect on tk_routine, eok_subscript error paths, and compound assignment pointer/unsigned/shiftr cases
    assert(llvm_lower_lvalue_expression(&arr_to_ptr_expr, &val) == llvm_gen_be_error_t::ok);
    an_expr_node unsup_lval_expr = {};
    unsup_lval_expr.kind = enk_routine;
    assert(llvm_lower_lvalue_expression(&unsup_lval_expr, &val) == llvm_gen_be_error_t::unsupported_expr);

    an_expr_node bad_comma_expr = {};
    bad_comma_expr.kind = enk_operation;
    bad_comma_expr.type = &int_type;
    bad_comma_expr.variant.operation.kind = eok_comma;
    bad_comma_expr.variant.operation.operands = &bad_op;
    assert(llvm_lower_expression(&bad_comma_expr, &val) == llvm_gen_be_error_t::unsupported_expr);

    // eok_subscript error paths: get_llvm_type failure on op1->type, and lower_binary_operands failure on vector subscript
    an_expr_node bad_sub_op1 = {};
    bad_sub_op1.kind = enk_variable;
    bad_sub_op1.type = &bad_ty;
    bad_sub_op1.next = &idx_op;
    sub_arr_expr.variant.operation.operands = &bad_sub_op1;
    assert(llvm_lower_expression(&sub_arr_expr, &val) == llvm_gen_be_error_t::unsupported_type);

    an_expr_node bad_vec_op1 = {};
    bad_vec_op1.kind = enk_constant;
    bad_vec_op1.type = &edg_vec2i32_ty;
    bad_vec_op1.variant.constant.ptr = &bad_con;
    bad_vec_op1.next = &idx_op;
    sub_arr_expr.variant.operation.operands = &bad_vec_op1;
    assert(llvm_lower_expression(&sub_arr_expr, &val) == llvm_gen_be_error_t::unsupported_expr);

    // eok_indirect on tk_routine (dereferencing function pointer)
    an_expr_node deref_fn_expr = {};
    deref_fn_expr.kind = enk_operation;
    deref_fn_expr.type = &rout_ty;
    deref_fn_expr.variant.operation.kind = eok_indirect;
    va_var_node.next = nullptr;
    deref_fn_expr.variant.operation.operands = &va_var_node;
    assert(llvm_lower_expression(&deref_fn_expr, &val) == llvm_gen_be_error_t::ok);

    // Pointer eok_pre_incr (op2 == nullptr with pointer loaded_val)
    an_expr_node ptr_incr_op = va_var_node;
    ptr_incr_op.next = nullptr;
    an_expr_node ptr_incr_expr = {};
    ptr_incr_expr.kind = enk_operation;
    ptr_incr_expr.type = &ptr_type;
    ptr_incr_expr.variant.operation.kind = eok_pre_incr;
    ptr_incr_expr.variant.operation.operands = &ptr_incr_op;
    assert(llvm_lower_compound_assignment(&ptr_incr_expr, &val) == llvm_gen_be_error_t::ok);

    // Compound assignment shiftr_assign and unsigned add_assign with i64 RHS
    comp_cast_expr.variant.operation.kind = eok_shiftr_assign;
    assert(llvm_lower_compound_assignment(&comp_cast_expr, &val) == llvm_gen_be_error_t::ok);
    lhs_i32_op.type = &unsigned_type;
    comp_cast_expr.type = &unsigned_type;
    comp_cast_expr.variant.operation.kind = eok_add_assign;
    assert(llvm_lower_compound_assignment(&comp_cast_expr, &val) == llvm_gen_be_error_t::ok);
    lhs_i32_op.type = &int_type;

    // Pointer compound assignment where raw_op1_ty->kind != tk_pointer (e.g. pointer-cached non-tk_pointer type)
    a_type custom_ptr_ty = {};
    custom_ptr_ty.kind = tk_integer;
    state.type_cache[&custom_ptr_ty] = llvm::PointerType::getUnqual(*state.context);
    a_variable custom_ptr_var = {};
    custom_ptr_var.type = &custom_ptr_ty;
    state.local_vars[&custom_ptr_var] = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
    an_expr_node custom_ptr_lhs = {};
    custom_ptr_lhs.kind = enk_variable;
    custom_ptr_lhs.type = &custom_ptr_ty;
    custom_ptr_lhs.variant.variable.ptr = &custom_ptr_var;
    custom_ptr_lhs.next = &rhs_i64_op;
    comp_cast_expr.type = &custom_ptr_ty;
    comp_cast_expr.variant.operation.operands = &custom_ptr_lhs;
    comp_cast_expr.variant.operation.kind = eok_padd_assign;
    assert(llvm_lower_compound_assignment(&comp_cast_expr, &val) == llvm_gen_be_error_t::ok);
    comp_cast_expr.variant.operation.kind = eok_psubtract_assign;
    assert(llvm_lower_compound_assignment(&comp_cast_expr, &val) == llvm_gen_be_error_t::ok);

#if FIXED_POINT_ALLOWED
    {
    // Workstream 9: Test fixed-point arithmetic (add, subtract, multiply, divide, saturating & non-saturating, signed & unsigned) and conversions
    a_type fx_s16 = {}, fx_u16 = {}, fx_s32 = {}, fx_u32 = {};
    fx_s16.kind = tk_fixed_point; fx_s16.size = 2; fx_s16.variant.fixed_point.precision = fpp_short; fx_s16.variant.fixed_point.is_unsigned = FALSE; fx_s16.variant.fixed_point.saturating = FALSE;
    fx_u16.kind = tk_fixed_point; fx_u16.size = 2; fx_u16.variant.fixed_point.precision = fpp_short; fx_u16.variant.fixed_point.is_unsigned = TRUE; fx_u16.variant.fixed_point.saturating = TRUE;
    fx_s32.kind = tk_fixed_point; fx_s32.size = 4; fx_s32.variant.fixed_point.precision = fpp_default; fx_s32.variant.fixed_point.is_unsigned = FALSE; fx_s32.variant.fixed_point.saturating = TRUE;
    fx_u32.kind = tk_fixed_point; fx_u32.size = 4; fx_u32.variant.fixed_point.precision = fpp_long; fx_u32.variant.fixed_point.is_unsigned = TRUE; fx_u32.variant.fixed_point.saturating = FALSE;

    state.type_cache[&fx_s16] = llvm::Type::getInt16Ty(*state.context);
    state.type_cache[&fx_u16] = llvm::Type::getInt16Ty(*state.context);
    state.type_cache[&fx_s32] = llvm::Type::getInt32Ty(*state.context);
    state.type_cache[&fx_u32] = llvm::Type::getInt32Ty(*state.context);

    a_variable fx_v16 = {}, fx_v32 = {};
    fx_v16.type = &fx_s16;
    fx_v32.type = &fx_s32;
    state.local_vars[&fx_v16] = state.builder->CreateAlloca(llvm::Type::getInt16Ty(*state.context));
    state.local_vars[&fx_v32] = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));

    an_expr_node fx_op1 = {}, fx_op2 = {};
    fx_op1.kind = enk_variable; fx_op1.type = &fx_s32; fx_op1.variant.variable.ptr = &fx_v32; fx_op1.next = &fx_op2;
    fx_op2.kind = enk_variable; fx_op2.type = &fx_s32; fx_op2.variant.variable.ptr = &fx_v32; fx_op2.next = nullptr;

    an_expr_node fx_arith = {};
    fx_arith.kind = enk_operation;
    fx_arith.variant.operation.operands = &fx_op1;
    const an_expr_operator_kind fx_ops[] = {eok_add, eok_subtract, eok_multiply, eok_divide, eok_and};
    a_type* fx_tys[] = {&fx_s16, &fx_u16, &fx_s32, &fx_u32};
    for (a_type* fxt : fx_tys) {
      fx_arith.type = fxt;
      for (an_expr_operator_kind ok : fx_ops) {
        fx_arith.variant.operation.kind = ok;
        assert(llvm_lower_arithmetic_expression(&fx_arith, &val) == llvm_gen_be_error_t::ok);
      }
    }
    // Also test unary negate on tk_fixed_point (v2 == nullptr)
    fx_op1.next = nullptr;
    fx_arith.type = &fx_s32;
    fx_arith.variant.operation.kind = eok_negate;
    assert(llvm_lower_arithmetic_expression(&fx_arith, &val) == llvm_gen_be_error_t::ok);

    // Fixed-point conversions: fx->fx (dst_f > src_f, src_f > dst_f signed & unsigned, dst_f == src_f), fx->fp (signed & unsigned), fp->fx (signed & unsigned), fx->int (signed & unsigned), int->fx (signed & unsigned)
    an_expr_node fx_cast = {};
    fx_cast.kind = enk_operation;
    fx_cast.variant.operation.kind = eok_cast;
    fx_cast.variant.operation.operands = &fx_op1;

    // 1. fx_s16 (f=7) -> fx_s32 (f=15) [dst_f > src_f]
    fx_op1.type = &fx_s16; fx_op1.variant.variable.ptr = &fx_v16;
    fx_cast.type = &fx_s32;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);

    // 2. fx_s32 (f=15, signed) -> fx_s16 (f=7) [src_f > dst_f, signed]
    fx_op1.type = &fx_s32; fx_op1.variant.variable.ptr = &fx_v32;
    fx_cast.type = &fx_s16;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);

    // 3. fx_u32 (f=32, unsigned) -> fx_s16 (f=7) [src_f > dst_f, unsigned]
    fx_op1.type = &fx_u32; fx_op1.variant.variable.ptr = &fx_v32;
    fx_cast.type = &fx_s16;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);

    // 4. fx_s32 -> fx_s32 [dst_f == src_f]
    fx_op1.type = &fx_s32; fx_op1.variant.variable.ptr = &fx_v32;
    fx_cast.type = &fx_s32;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);

    // 5. fx->fp (signed & unsigned)
    fx_op1.type = &fx_s32; fx_cast.type = &flt_td_ty;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);
    fx_op1.type = &fx_u32; fx_cast.type = &flt_td_ty;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);

    // 6. fp->fx (signed & unsigned)
    fx_cast.variant.operation.operands = &flt_op;
    fx_cast.type = &fx_s32;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);
    fx_cast.type = &fx_u32;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);

    // 7. fx->int (signed & unsigned)
    fx_cast.variant.operation.operands = &fx_op1;
    fx_op1.type = &fx_s32; fx_cast.type = &int_type;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);
    fx_op1.type = &fx_u32; fx_cast.type = &int_type;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);

    // 8. int->fx
    a_variable int_fx_var = {};
    int_fx_var.type = &int_type;
    state.local_vars[&int_fx_var] = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));
    an_expr_node int_fx_op = {};
    int_fx_op.kind = enk_variable; int_fx_op.type = &int_type; int_fx_op.variant.variable.ptr = &int_fx_var;
    fx_cast.variant.operation.operands = &int_fx_op;
    fx_cast.type = &fx_s32;
    assert(llvm_lower_cast_expression(&fx_cast, &val) == llvm_gen_be_error_t::ok);
    }
#endif

    // =========================================================================
    // Workstream 10: Exhaustive coverage for Sections 2-6 in llvm_gen_be_expr
    // =========================================================================
    {
      state.type_cache.clear();
      int_kind_is_signed[ik_int] = TRUE;
      int_kind_is_signed[ik_short] = TRUE;
      int_kind_is_signed[ik_long_long] = TRUE;
      int_kind_is_signed[ik_unsigned_int] = FALSE;
      int_kind_is_signed[ik_unsigned_short] = FALSE;
      int_kind_is_signed[ik_unsigned_long_long] = FALSE;

      llvm::Type* i1_t = llvm::Type::getInt1Ty(*state.context);
      llvm::Type* i8_t = llvm::Type::getInt8Ty(*state.context);
      llvm::Type* i16_t = llvm::Type::getInt16Ty(*state.context);
      llvm::Type* i32_t = llvm::Type::getInt32Ty(*state.context);
      llvm::Type* i64_t = llvm::Type::getInt64Ty(*state.context);
      llvm::Type* i128_t = llvm::Type::getInt128Ty(*state.context);
      llvm::Type* f16_t = llvm::Type::getHalfTy(*state.context);
      llvm::Type* bf16_t = llvm::Type::getBFloatTy(*state.context);
      llvm::Type* f32_t = llvm::Type::getFloatTy(*state.context);
      llvm::Type* f64_t = llvm::Type::getDoubleTy(*state.context);
      llvm::Type* f80_t = llvm::Type::getX86_FP80Ty(*state.context);
      llvm::Type* f128_t = llvm::Type::getFP128Ty(*state.context);
      llvm::Type* ptr_t = llvm::PointerType::getUnqual(*state.context);
      llvm::Function* ws10_fn = state.builder->GetInsertBlock()->getParent();

      auto ensure_expr_bb = [&]() noexcept {
        llvm::BasicBlock* cur = state.builder->GetInsertBlock();
        if (!cur || EDG_LLVM_GET_TERMINATOR(cur)) {
          llvm::BasicBlock* nb = llvm::BasicBlock::Create(*state.context, "ws10.bb", ws10_fn);
          state.builder->SetInsertPoint(nb);
        }
      };

      auto run_expr_oom = [&](auto&& fn) noexcept {
        for (int b = 0; b < 48; ++b) {
          ensure_expr_bb();
          g_llvm_be_oom_countdown = b;
          llvm_gen_be_error_t r = fn();
          g_llvm_be_oom_countdown = -1;
          if (r == llvm_gen_be_error_t::ok) break;
        }
      };

      // --- 10.A: Bitfield loads/stores (invalid args, big-endian, bool i1/i8, straddling, 0-bit, untyped, OOM) ---
      {
        ensure_expr_bb();
        a_type s32_e = {}; s32_e.kind = tk_integer; s32_e.size = 4; s32_e.variant.integer.int_kind = ik_int;
        a_type u32_e = {}; u32_e.kind = tk_integer; u32_e.size = 4; u32_e.variant.integer.int_kind = ik_unsigned_int;
        a_type vol32_e = {}; vol32_e.kind = tk_integer; vol32_e.size = 999; // TQ_VOLATILE in stub!
        a_type vol_ok_e = {}; vol_ok_e.kind = tk_typeref; vol_ok_e.variant.typeref.type = &s32_e; vol_ok_e.variant.typeref.qualifiers = TQ_VOLATILE;
        a_type bool_i8_e = {}; bool_i8_e.kind = tk_integer; bool_i8_e.size = 1; bool_i8_e.variant.integer.bool_type = TRUE;
        a_type bool_i1_e = {}; bool_i1_e.kind = tk_integer; bool_i1_e.size = 1; bool_i1_e.variant.integer.bool_type = TRUE;
        state.type_cache[&bool_i1_e] = i1_t;

        llvm::AllocaInst* bf_slot = state.builder->CreateAlloca(llvm::ArrayType::get(i8_t, 16), nullptr, "bf.slot");
        a_field bf_f = {}; bf_f.is_bit_field = TRUE; bf_f.type = &s32_e; bf_f.bit_size = 5; bf_f.offset_bit_remainder = 3;

        // Invalid argument checks
        assert(llvm_emit_bitfield_load(nullptr, &bf_f, &s32_e, &val) == llvm_gen_be_error_t::bitfield_access_failed);
        assert(llvm_emit_bitfield_load(bf_slot, nullptr, &s32_e, &val) == llvm_gen_be_error_t::bitfield_access_failed);
        assert(llvm_emit_bitfield_load(bf_slot, &bf_f, nullptr, &val) == llvm_gen_be_error_t::bitfield_access_failed);
        assert(llvm_emit_bitfield_load(bf_slot, &bf_f, &s32_e, nullptr) == llvm_gen_be_error_t::bitfield_access_failed);

        llvm::Value* v_five = llvm::ConstantInt::get(i32_t, 5);
        assert(llvm_emit_bitfield_store(nullptr, v_five, &bf_f, &s32_e) == llvm_gen_be_error_t::bitfield_access_failed);
        assert(llvm_emit_bitfield_store(bf_slot, nullptr, &bf_f, &s32_e) == llvm_gen_be_error_t::bitfield_access_failed);
        assert(llvm_emit_bitfield_store(bf_slot, v_five, nullptr, &s32_e) == llvm_gen_be_error_t::bitfield_access_failed);
        assert(llvm_emit_bitfield_store(bf_slot, v_five, &bf_f, nullptr) == llvm_gen_be_error_t::bitfield_access_failed);

        // OOM sweeps on load & store
        run_expr_oom([&]() noexcept { return llvm_emit_bitfield_load(bf_slot, &bf_f, &s32_e, &val); });
        run_expr_oom([&]() noexcept { return llvm_emit_bitfield_store(bf_slot, v_five, &bf_f, &s32_e); });

        // Volatile non-straddling 32-bit, volatile straddling (bit_offset=28, bit_size=8 -> 24/32 straddle -> byte_wise!),
        // 0-bit span (`bit_offset=0, bit_size=0`), untyped `field->type = nullptr`, big-endian (`targ_little_endian = FALSE`),
        // `byte_fmask == 0U` (`bit_offset = 8`, `bit_size = 12` -> byte 0 has mask 0, spans 24 bits -> byte_wise!),
        // bool bitfield into `i8` AND into `i1`, and full-width (`bit_size == 8`) unsigned bitfield!
        bf_f.type = &vol_ok_e; bf_f.offset_bit_remainder = 4; bf_f.bit_size = 8;
        assert(llvm_emit_bitfield_load(bf_slot, &bf_f, &vol_ok_e, &val) == llvm_gen_be_error_t::ok);
        assert(llvm_emit_bitfield_store(bf_slot, v_five, &bf_f, &vol_ok_e) == llvm_gen_be_error_t::ok);

        bf_f.offset_bit_remainder = 28; bf_f.bit_size = 8; // straddles 32-bit declared container!
        assert(llvm_emit_bitfield_load(bf_slot, &bf_f, &vol_ok_e, &val) == llvm_gen_be_error_t::ok);
        assert(llvm_emit_bitfield_store(bf_slot, v_five, &bf_f, &vol_ok_e) == llvm_gen_be_error_t::ok);

        // Big-endian byte-wise load & store with leading untouched byte (`offset_bit_remainder = 8, bit_size = 12` -> 24-bit byte_wise, byte 0 untouched!)
        targ_little_endian = FALSE;
        bf_f.type = &s32_e; bf_f.offset_bit_remainder = 8; bf_f.bit_size = 12;
        assert(llvm_emit_bitfield_load(bf_slot, &bf_f, &s32_e, &val) == llvm_gen_be_error_t::ok);
        assert(llvm_emit_bitfield_store(bf_slot, v_five, &bf_f, &s32_e) == llvm_gen_be_error_t::ok);
        targ_little_endian = TRUE;

        // `span_bits == 0` (`bit_offset=0, bit_size=0`), `field->type == nullptr`, and non-power-of-2 `vol32_e` (`size = 999`)
        bf_f.type = nullptr; bf_f.offset_bit_remainder = 0; bf_f.bit_size = 0;
        assert(llvm_emit_bitfield_load(bf_slot, &bf_f, &u32_e, &val) == llvm_gen_be_error_t::ok);
        assert(llvm_emit_bitfield_store(bf_slot, v_five, &bf_f, &u32_e) == llvm_gen_be_error_t::ok);
        bf_f.type = &vol32_e; bf_f.bit_size = 4;
        assert(llvm_emit_bitfield_load(bf_slot, &bf_f, &vol32_e, &val) == llvm_gen_be_error_t::ok);

        // Bool bitfield load into `i8` vs `i1`, and full-width `bit_size == container_bits` (`bit_size = 8, offset = 0`)
        bf_f.type = &bool_i8_e; bf_f.offset_bit_remainder = 0; bf_f.bit_size = 1;
        assert(llvm_emit_bitfield_load(bf_slot, &bf_f, &bool_i8_e, &val) == llvm_gen_be_error_t::ok);
        assert(llvm_emit_bitfield_load(bf_slot, &bf_f, &bool_i1_e, &val) == llvm_gen_be_error_t::ok);
        assert(llvm_emit_bitfield_store(bf_slot, llvm::ConstantInt::get(i8_t, 1), &bf_f, &bool_i8_e) == llvm_gen_be_error_t::ok);
        bf_f.type = &u32_e; bf_f.offset_bit_remainder = 0; bf_f.bit_size = 8;
        assert(llvm_emit_bitfield_load(bf_slot, &bf_f, &u32_e, &val) == llvm_gen_be_error_t::ok);
        assert(llvm_emit_bitfield_store(bf_slot, v_five, &bf_f, &u32_e) == llvm_gen_be_error_t::ok);
      }

      // --- 10.B: `_Complex` (`int`, `_Float16`, `float` Win64/SysV/AAPCS64) & `_Decimal32`/`64`/`128` ---
      {
        ensure_expr_bb();
        a_type s32_e = {}; s32_e.kind = tk_integer; s32_e.size = 4; s32_e.variant.integer.int_kind = ik_int;
        a_type u32_e = {}; u32_e.kind = tk_integer; u32_e.size = 4; u32_e.variant.integer.int_kind = ik_unsigned_int;
        a_type s64_e = {}; s64_e.kind = tk_integer; s64_e.size = 8; s64_e.variant.integer.int_kind = ik_long_long;
        a_type u64_e = {}; u64_e.kind = tk_integer; u64_e.size = 8; u64_e.variant.integer.int_kind = ik_unsigned_long_long;
        a_type f16_e = {}; f16_e.kind = tk_float; f16_e.size = 2; f16_e.variant.float_kind = fk_float;
        state.type_cache[&f16_e] = f16_t;
        a_type f32_e = {}; f32_e.kind = tk_float; f32_e.size = 4; f32_e.variant.float_kind = fk_float;

        a_type cplx_int_e = {}; cplx_int_e.kind = tk_complex; cplx_int_e.size = 8;
        llvm::StructType* cplx_i32_ll = llvm::StructType::get(*state.context, {i32_t, i32_t});
        state.type_cache[&cplx_int_e] = cplx_i32_ll;

        a_type cplx_f16_e = {}; cplx_f16_e.kind = tk_complex; cplx_f16_e.size = 4;
        llvm::StructType* cplx_f16_ll = llvm::StructType::get(*state.context, {f16_t, f16_t});
        state.type_cache[&cplx_f16_e] = cplx_f16_ll;

        a_type cplx_f32_e = {}; cplx_f32_e.kind = tk_complex; cplx_f32_e.size = 8;
        llvm::StructType* cplx_f32_ll = llvm::StructType::get(*state.context, {f32_t, f32_t});
        state.type_cache[&cplx_f32_e] = cplx_f32_ll;

        a_variable vc_i = {}, vc_h = {}, vc_f = {};
        vc_i.type = &cplx_int_e; vc_h.type = &cplx_f16_e; vc_f.type = &cplx_f32_e;
        state.local_vars[&vc_i] = state.builder->CreateAlloca(cplx_i32_ll);
        state.local_vars[&vc_h] = state.builder->CreateAlloca(cplx_f16_ll);
        state.local_vars[&vc_f] = state.builder->CreateAlloca(cplx_f32_ll);

        an_expr_node ci1 = {}, ci2 = {}, ch1 = {}, ch2 = {}, cf1 = {}, cf2 = {};
        ci1.kind = enk_variable; ci1.type = &cplx_int_e; ci1.variant.variable.ptr = &vc_i; ci1.next = &ci2;
        ci2.kind = enk_variable; ci2.type = &cplx_int_e; ci2.variant.variable.ptr = &vc_i;
        ch1.kind = enk_variable; ch1.type = &cplx_f16_e; ch1.variant.variable.ptr = &vc_h; ch1.next = &ch2;
        ch2.kind = enk_variable; ch2.type = &cplx_f16_e; ch2.variant.variable.ptr = &vc_h;
        cf1.kind = enk_variable; cf1.type = &cplx_f32_e; cf1.variant.variable.ptr = &vc_f; cf1.next = &cf2;
        cf2.kind = enk_variable; cf2.type = &cplx_f32_e; cf2.variant.variable.ptr = &vc_f;

        an_expr_node ca = {}; ca.kind = enk_operation;
        // `_Complex int` +, -, *, /, unary -, and `~z` (`eok_complement`)
        ca.type = &cplx_int_e; ca.variant.operation.operands = &ci1;
        for (an_expr_operator_kind k : {eok_add, eok_subtract, eok_multiply, eok_divide}) {
          ca.variant.operation.kind = k;
          assert(llvm_lower_arithmetic_expression(&ca, &val) == llvm_gen_be_error_t::ok);
        }
        ci1.next = nullptr;
        ca.variant.operation.kind = eok_negate;
        assert(llvm_lower_arithmetic_expression(&ca, &val) == llvm_gen_be_error_t::ok);
        ca.variant.operation.kind = eok_complement;
        assert(llvm_lower_arithmetic_expression(&ca, &val) == llvm_gen_be_error_t::ok);
        cf1.next = nullptr; ca.type = &cplx_f32_e; ca.variant.operation.operands = &cf1;
        assert(llvm_lower_arithmetic_expression(&ca, &val) == llvm_gen_be_error_t::ok);
        cf1.next = &cf2;

        // `_Complex _Float16` (`__mulhc3` / `__divhc3`) + OOM sweep on `emit_complex_libcall`
        ca.type = &cplx_f16_e; ca.variant.operation.operands = &ch1;
        ca.variant.operation.kind = eok_multiply;
        run_expr_oom([&]() noexcept { return llvm_lower_arithmetic_expression(&ca, &val); });
        ca.variant.operation.kind = eok_divide;
        assert(llvm_lower_arithmetic_expression(&ca, &val) == llvm_gen_be_error_t::ok);

        // Win64 `_Complex float` `i64` return unpacking in `emit_complex_libcall`
        std::string orig_triple = llvm::Triple(state.module->getTargetTriple()).str();
        EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
        ca.type = &cplx_f32_e; ca.variant.operation.operands = &cf1; ca.variant.operation.kind = eok_multiply;
        assert(llvm_lower_arithmetic_expression(&ca, &val) == llvm_gen_be_error_t::ok);
        EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), orig_triple);

        // `_Decimal32`, `_Decimal64`, `_Decimal128` direct & expression-level arithmetic, comparisons, and casts
        llvm::Value* d32_v = llvm::ConstantInt::get(i32_t, 1);
        llvm::Value* d64_v = llvm::ConstantInt::get(i64_t, 2);
        llvm::Value* d128_v = llvm::ConstantInt::get(i128_t, 3);

        // Invalid args & unsupported widths/ops on direct decimal helpers
        assert(llvm_lower_decimal_float_binop(4, eok_add, nullptr, d32_v, &val) == llvm_gen_be_error_t::invalid_argument);
        assert(llvm_lower_decimal_float_binop(4, eok_add, d32_v, nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
        assert(llvm_lower_decimal_float_binop(4, eok_add, d32_v, d32_v, nullptr) == llvm_gen_be_error_t::invalid_argument);
        assert(llvm_lower_decimal_float_binop(7, eok_add, d32_v, d32_v, &val) == llvm_gen_be_error_t::unsupported_type);
        assert(llvm_lower_decimal_float_binop(4, eok_remainder, d32_v, d32_v, &val) == llvm_gen_be_error_t::unsupported_expr);
        run_expr_oom([&]() noexcept { return llvm_lower_decimal_float_binop(4, eok_add, d32_v, d32_v, &val); });

        assert(llvm_lower_decimal_float_cmp(4, eok_eq, nullptr, d32_v, i8_t, &val) == llvm_gen_be_error_t::invalid_argument);
        assert(llvm_lower_decimal_float_cmp(4, eok_eq, d32_v, nullptr, i8_t, &val) == llvm_gen_be_error_t::invalid_argument);
        assert(llvm_lower_decimal_float_cmp(4, eok_eq, d32_v, d32_v, nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
        assert(llvm_lower_decimal_float_cmp(4, eok_eq, d32_v, d32_v, i8_t, nullptr) == llvm_gen_be_error_t::invalid_argument);
        assert(llvm_lower_decimal_float_cmp(7, eok_eq, d32_v, d32_v, i8_t, &val) == llvm_gen_be_error_t::unsupported_type);
        assert(llvm_lower_decimal_float_cmp(4, eok_spaceship, d32_v, d32_v, i8_t, &val) == llvm_gen_be_error_t::unsupported_expr);
        run_expr_oom([&]() noexcept { return llvm_lower_decimal_float_cmp(8, eok_eq, d64_v, d64_v, i8_t, &val); });
        assert(llvm_lower_decimal_float_cmp(16, eok_ne, d128_v, d128_v, i1_t, &val) == llvm_gen_be_error_t::ok);

        assert(llvm_lower_decimal_float_cast(4, 8, false, nullptr, i64_t, &val) == llvm_gen_be_error_t::invalid_argument);
        assert(llvm_lower_decimal_float_cast(4, 8, false, d32_v, nullptr, &val) == llvm_gen_be_error_t::invalid_argument);
        assert(llvm_lower_decimal_float_cast(4, 8, false, d32_v, i64_t, nullptr) == llvm_gen_be_error_t::invalid_argument);

        // PPC (`__dpd_`) and SystemZ (`__dpd_`) vs x86 (`__bid_`)
        EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "powerpc64le-unknown-linux-gnu");
        assert(llvm_lower_decimal_float_binop(8, eok_subtract, d64_v, d64_v, &val) == llvm_gen_be_error_t::ok);
        EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "s390x-ibm-linux-gnu");
        assert(llvm_lower_decimal_float_binop(16, eok_multiply, d128_v, d128_v, &val) == llvm_gen_be_error_t::ok);
        EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), orig_triple);

        // All decimal-to-decimal, decimal<->FP (`half`, `bfloat`, `float`, `double`, `x86_fp80`, `fp128`, unsupported `ppc_fp128`),
        // decimal<->int (`i1`, `i16`, `i32`, `i64`, `i128` signed & unsigned), and unsupported pointer dest
        assert(llvm_lower_decimal_float_cast(4, 4, false, d32_v, i32_t, &val) == llvm_gen_be_error_t::ok); // same width
        run_expr_oom([&]() noexcept { return llvm_lower_decimal_float_cast(4, 8, false, d32_v, i64_t, &val); });
        assert(llvm_lower_decimal_float_cast(7, 8, false, d32_v, i64_t, &val) == llvm_gen_be_error_t::unsupported_type);
        assert(llvm_lower_decimal_float_cast(4, 7, false, d32_v, i64_t, &val) == llvm_gen_be_error_t::unsupported_type);

        llvm::Type* fp_tys[] = {f16_t, bf16_t, f32_t, f64_t, f80_t, f128_t};
        for (llvm::Type* fpt : fp_tys) {
          assert(llvm_lower_decimal_float_cast(8, 0, false, d64_v, fpt, &val) == llvm_gen_be_error_t::ok);
          assert(llvm_lower_decimal_float_cast(0, 8, false, llvm::ConstantFP::get(fpt, 1.5), i64_t, &val) == llvm_gen_be_error_t::ok);
        }
        run_expr_oom([&]() noexcept { return llvm_lower_decimal_float_cast(8, 0, false, d64_v, f16_t, &val); });
        run_expr_oom([&]() noexcept { return llvm_lower_decimal_float_cast(0, 8, false, llvm::ConstantFP::get(f16_t, 1.0), i64_t, &val); });
        llvm::Type* ppc_fp_t = llvm::Type::getPPC_FP128Ty(*state.context);
        assert(llvm_lower_decimal_float_cast(8, 0, false, d64_v, ppc_fp_t, &val) == llvm_gen_be_error_t::ok);
        assert(llvm_lower_decimal_float_cast(0, 8, false, llvm::ConstantFP::get(ppc_fp_t, 1.0), i64_t, &val) == llvm_gen_be_error_t::ok);
        assert(llvm_lower_decimal_float_cast(7, 0, false, d32_v, f32_t, &val) == llvm_gen_be_error_t::unsupported_type);
        assert(llvm_lower_decimal_float_cast(0, 7, false, llvm::ConstantFP::get(f32_t, 1.0), i32_t, &val) == llvm_gen_be_error_t::unsupported_type);

        // Decimal <-> integers (`i1`, `i16`, `i32`, `i64`, `i128` signed & unsigned) + OOM sweeps + invalid widths + pointer unsupported
        assert(llvm_lower_decimal_float_cast(4, 0, false, d32_v, i1_t, &val) == llvm_gen_be_error_t::ok);
        assert(llvm_lower_decimal_float_cast(0, 4, false, llvm::ConstantInt::get(i1_t, 1), i32_t, &val) == llvm_gen_be_error_t::ok);
        for (bool uns : {false, true}) {
          for (llvm::Type* it : {i16_t, i32_t, i64_t, i128_t}) {
            assert(llvm_lower_decimal_float_cast(8, 0, uns, d64_v, it, &val) == llvm_gen_be_error_t::ok);
            assert(llvm_lower_decimal_float_cast(0, 8, uns, llvm::ConstantInt::get(it, 3), i64_t, &val) == llvm_gen_be_error_t::ok);
          }
        }
        run_expr_oom([&]() noexcept { return llvm_lower_decimal_float_cast(8, 0, false, d64_v, i16_t, &val); });
        run_expr_oom([&]() noexcept { return llvm_lower_decimal_float_cast(0, 8, true, llvm::ConstantInt::get(i64_t, 3), i64_t, &val); });
        assert(llvm_lower_decimal_float_cast(7, 0, false, d32_v, i32_t, &val) == llvm_gen_be_error_t::unsupported_type);
        assert(llvm_lower_decimal_float_cast(0, 7, false, d32_v, i32_t, &val) == llvm_gen_be_error_t::unsupported_type);
        assert(llvm_lower_decimal_float_cast(4, 0, false, d32_v, ptr_t, &val) == llvm_gen_be_error_t::unsupported_type);

        // Expression-level `_Decimal64` arithmetic, unary_plus, negate, comparisons, and casts!
        a_type dec64_e = {}; dec64_e.kind = tk_typeref; dec64_e.source_corresp.name = (char*)"_Decimal64"; dec64_e.variant.typeref.type = &s64_e;
        a_variable v_dec = {}; v_dec.type = &dec64_e;
        state.local_vars[&v_dec] = state.builder->CreateAlloca(i64_t);
        an_expr_node dop1 = {}, dop2 = {};
        dop1.kind = enk_variable; dop1.type = &dec64_e; dop1.variant.variable.ptr = &v_dec; dop1.next = &dop2;
        dop2.kind = enk_variable; dop2.type = &dec64_e; dop2.variant.variable.ptr = &v_dec;

        an_expr_node d_arith = {}; d_arith.kind = enk_operation; d_arith.type = &dec64_e; d_arith.variant.operation.operands = &dop1;
        for (an_expr_operator_kind dk : {eok_add, eok_subtract, eok_multiply, eok_divide}) {
          d_arith.variant.operation.kind = dk;
          assert(llvm_lower_arithmetic_expression(&d_arith, &val) == llvm_gen_be_error_t::ok);
        }
        dop1.next = nullptr;
        d_arith.variant.operation.kind = eok_unary_plus;
        assert(llvm_lower_arithmetic_expression(&d_arith, &val) == llvm_gen_be_error_t::ok);
        d_arith.variant.operation.kind = eok_negate;
        run_expr_oom([&]() noexcept { return llvm_lower_arithmetic_expression(&d_arith, &val); });

        dop1.next = &dop2;
        an_expr_node d_cmp = {}; d_cmp.kind = enk_operation; d_cmp.type = &s32_e; d_cmp.variant.operation.operands = &dop1;
        for (an_expr_operator_kind ck : {eok_eq, eok_ne, eok_lt, eok_le, eok_gt, eok_ge}) {
          d_cmp.variant.operation.kind = ck;
          assert(llvm_lower_logical_expression(&d_cmp, &val) == llvm_gen_be_error_t::ok);
        }
        run_expr_oom([&]() noexcept { return llvm_lower_logical_expression(&d_cmp, &val); });

        dop1.next = nullptr;
        an_expr_node d_cast = {}; d_cast.kind = enk_operation; d_cast.variant.operation.kind = eok_cast;
        d_cast.variant.operation.operands = &dop1; d_cast.type = &u32_e;
        run_expr_oom([&]() noexcept { return llvm_lower_cast_expression(&d_cast, &val); });
        an_expr_node f_op_dec = {}; f_op_dec.kind = enk_constant; f_op_dec.type = &f32_e;
        d_cast.variant.operation.operands = &f_op_dec; d_cast.type = &dec64_e;
        assert(llvm_lower_cast_expression(&d_cast, &val) == llvm_gen_be_error_t::ok);
      }

      // --- 10.C: Casts (`tk_nullptr`, `bool` i1/i8 from complex/fp/int/bool, `_Complex int` <-> `_Complex float` / scalar) ---
      {
        ensure_expr_bb();
        a_type nullp_e = {}; nullp_e.kind = tk_nullptr; nullp_e.size = 8;
        a_type s32_e = {}; s32_e.kind = tk_integer; s32_e.size = 4; s32_e.variant.integer.int_kind = ik_int;
        a_type ptr_e = {}; ptr_e.kind = tk_pointer; ptr_e.size = 8; ptr_e.variant.pointer.type = &s32_e;
        a_type pmd_e = {}; pmd_e.kind = tk_ptr_to_member; pmd_e.size = 8; pmd_e.variant.ptr_to_member.type = &s32_e;
        a_type rty_e = {}; rty_e.kind = tk_routine;
        a_type pmf_e = {}; pmf_e.kind = tk_ptr_to_member; pmf_e.size = 16; pmf_e.variant.ptr_to_member.type = &rty_e;
        a_type bool_i8_e = {}; bool_i8_e.kind = tk_integer; bool_i8_e.size = 1; bool_i8_e.variant.integer.bool_type = TRUE;
        a_type bool_i1_e = {}; bool_i1_e.kind = tk_integer; bool_i1_e.size = 1; bool_i1_e.variant.integer.bool_type = TRUE;
        state.type_cache[&bool_i1_e] = i1_t;
        a_type f32_e = {}; f32_e.kind = tk_float; f32_e.size = 4; f32_e.variant.float_kind = fk_float;
        a_type cplx_i32_e = {}; cplx_i32_e.kind = tk_complex; cplx_i32_e.size = 8;
        state.type_cache[&cplx_i32_e] = llvm::StructType::get(*state.context, {i32_t, i32_t});
        a_type cplx_i64_e = {}; cplx_i64_e.kind = tk_complex; cplx_i64_e.size = 16;
        state.type_cache[&cplx_i64_e] = llvm::StructType::get(*state.context, {i64_t, i64_t});
        a_type cplx_f32_e = {}; cplx_f32_e.kind = tk_complex; cplx_f32_e.size = 8;
        state.type_cache[&cplx_f32_e] = llvm::StructType::get(*state.context, {f32_t, f32_t});

        an_expr_node np_src = {}; np_src.kind = enk_constant; np_src.type = &nullp_e;
        an_expr_node int_src = {}; int_src.kind = enk_constant; int_src.type = &s32_e;
        an_expr_node flt_src = {}; flt_src.kind = enk_constant; flt_src.type = &f32_e;
        an_expr_node b8_src = {}; b8_src.kind = enk_constant; b8_src.type = &bool_i8_e;
        an_expr_node b1_src = {}; b1_src.kind = enk_constant; b1_src.type = &bool_i1_e;
        an_expr_node ci_src = {}; ci_src.kind = enk_constant; ci_src.type = &cplx_i32_e;
        an_expr_node cf_src = {}; cf_src.kind = enk_constant; cf_src.type = &cplx_f32_e;

        an_expr_node ce = {}; ce.kind = enk_operation; ce.variant.operation.kind = eok_cast;
        // Cast TO nullptr_t
        ce.variant.operation.operands = &int_src; ce.type = &nullp_e;
        assert(llvm_lower_cast_expression(&ce, &val) == llvm_gen_be_error_t::ok);
        // Cast FROM nullptr_t to PMD (-1), PMF ({0, 0}), pointer, and integer
        ce.variant.operation.operands = &np_src;
        for (a_type* dt : {&pmd_e, &pmf_e, &ptr_e, &s32_e}) {
          ce.type = dt;
          assert(llvm_lower_cast_expression(&ce, &val) == llvm_gen_be_error_t::ok);
        }
        // Cast TO bool (`bool_i8_e` & `bool_i1_e`) from `b8`, `b1`, `flt`, `ci`, `cf`, `int`
        for (a_type* bdt : {&bool_i8_e, &bool_i1_e}) {
          ce.type = bdt;
          for (an_expr_node* s : {&b8_src, &b1_src, &flt_src, &ci_src, &cf_src, &int_src}) {
            ce.variant.operation.operands = s;
            assert(llvm_lower_cast_expression(&ce, &val) == llvm_gen_be_error_t::ok);
          }
        }
        // Cast `_Complex` <-> `_Complex` (`ci->ci64`, `ci->cf`, `cf->ci`) and scalar (`int`, `flt`) -> `ci` / `cf`
        ce.variant.operation.operands = &ci_src; ce.type = &cplx_i64_e;
        assert(llvm_lower_cast_expression(&ce, &val) == llvm_gen_be_error_t::ok);
        ce.variant.operation.operands = &ci_src; ce.type = &cplx_f32_e;
        assert(llvm_lower_cast_expression(&ce, &val) == llvm_gen_be_error_t::ok);
        ce.variant.operation.operands = &cf_src; ce.type = &cplx_i32_e;
        assert(llvm_lower_cast_expression(&ce, &val) == llvm_gen_be_error_t::ok);
        for (an_expr_node* s : {&int_src, &flt_src}) {
          ce.variant.operation.operands = s; ce.type = &cplx_i32_e;
          assert(llvm_lower_cast_expression(&ce, &val) == llvm_gen_be_error_t::ok);
          ce.type = &cplx_f32_e;
          assert(llvm_lower_cast_expression(&ce, &val) == llvm_gen_be_error_t::ok);
        }
        // Null `op1->type` in cast expression
        an_expr_node untyped_src = {}; untyped_src.kind = enk_type_operand; untyped_src.type = nullptr;
        ce.variant.operation.operands = &untyped_src; ce.type = &s32_e;
        assert(llvm_lower_cast_expression(&ce, &val) == llvm_gen_be_error_t::ok);
      }

      // --- 10.D: Source location demangling, C11/libatomic atomics, coro/chk/bit_cast builtins, ABI call coercion ---
      {
        ensure_expr_bb();
        a_type s32_e = {}; s32_e.kind = tk_integer; s32_e.size = 4; s32_e.variant.integer.int_kind = ik_int;
        a_type s64_e = {}; s64_e.kind = tk_integer; s64_e.size = 8; s64_e.variant.integer.int_kind = ik_long_long;
        a_type s16_e = {}; s16_e.kind = tk_integer; s16_e.size = 2; s16_e.variant.integer.int_kind = ik_short;
        a_type u16_e = {}; u16_e.kind = tk_integer; u16_e.size = 2; u16_e.variant.integer.int_kind = ik_unsigned_short;
        a_type bool_e = {}; bool_e.kind = tk_integer; bool_e.size = 1; bool_e.variant.integer.bool_type = TRUE;
        a_type bool_i1_e = {}; bool_i1_e.kind = tk_integer; bool_i1_e.size = 1; bool_i1_e.variant.integer.bool_type = TRUE;
        state.type_cache[&bool_i1_e] = i1_t;
        a_type void_e = {}; void_e.kind = tk_void;
        a_type f32_e = {}; f32_e.kind = tk_float; f32_e.size = 4; f32_e.variant.float_kind = fk_float;
        a_type f64_e = {}; f64_e.kind = tk_float; f64_e.size = 8; f64_e.variant.float_kind = fk_double;
        a_type ptr_s32_e = {}; ptr_s32_e.kind = tk_pointer; ptr_s32_e.size = 8; ptr_s32_e.variant.pointer.type = &s32_e;
        a_type ptr_f32_e = {}; ptr_f32_e.kind = tk_pointer; ptr_f32_e.size = 8; ptr_f32_e.variant.pointer.type = &f32_e;
        a_type ptr_void_e = {}; ptr_void_e.kind = tk_pointer; ptr_void_e.size = 8; ptr_void_e.variant.pointer.type = &void_e;
        a_type st24_e = {}; st24_e.kind = tk_struct; st24_e.size = 24; st24_e.alignment = 77;
        llvm::StructType* st24_ll = llvm::StructType::get(*state.context, {i64_t, i64_t, i64_t});
        state.type_cache[&st24_e] = st24_ll;
        a_type st4_e = {}; st4_e.kind = tk_struct; st4_e.size = 4; st4_e.alignment = 77;
        llvm::StructType* st4_ll = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>(i32_t));
        state.type_cache[&st4_e] = st4_ll;
        a_type ptr_st24_e = {}; ptr_st24_e.kind = tk_pointer; ptr_st24_e.size = 8; ptr_st24_e.variant.pointer.type = &st24_e;
        a_type ptr_st4_e = {}; ptr_st4_e.kind = tk_pointer; ptr_st4_e.size = 8; ptr_st4_e.variant.pointer.type = &st4_e;

        // 1. `resolve_builtin_source_location`:
        // - unmangled `current_edg_routine` + `full_fn` non-empty
        // - mangled `_Z3fooi` function name (demangles with `(`!) + `short_fn` non-empty & `resolved_line == 0`
        // - invalid `_Z...` / `?foo@@` function name (demangle unchanged / MSVC) + empty `short_fn`/`full_fn`
        a_routine cur_rt = {};
        cur_rt.source_corresp.name = (char*)"human_readable_fn";
        cur_rt.source_corresp.name_has_been_mangled = FALSE;
        state.current_edg_routine = &cur_rt;
        g_seq_full_fn = "/inc/hdr.hpp"; g_seq_short_fn = "hdr.hpp"; g_seq_line = 99;

        a_routine bi_rt = {}; bi_rt.source_corresp.name = (char*)"__builtin_source_location";
        an_expr_node bi_op = {}; bi_op.kind = enk_routine; bi_op.variant.routine.ptr = &bi_rt;
        an_expr_node bi_call = {}; bi_call.kind = enk_operation; bi_call.type = &ptr_void_e;
        bi_call.position.seq = 12; bi_call.position.column = 5;
        bi_call.variant.operation.kind = eok_call; bi_call.variant.operation.operands = &bi_op;
        run_expr_oom([&]() noexcept { return llvm_lower_call_expression(&bi_call, &val); });

        state.current_edg_routine = nullptr;
        g_seq_full_fn = ""; g_seq_short_fn = "short_only.h"; g_seq_line = 0;
        llvm::Function* mangled_fn = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false),
            llvm::Function::ExternalLinkage, "_Z3fooi", state.module.get());
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "entry", mangled_fn));
        bi_rt.source_corresp.name = (char*)"__builtin_FUNCTION";
        assert(llvm_lower_call_expression(&bi_call, &val) == llvm_gen_be_error_t::ok);

        g_seq_full_fn = nullptr; g_seq_short_fn = ""; g_seq_line = 5;
        llvm::Function* msvc_fn = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false),
            llvm::Function::ExternalLinkage, "?not_valid_itanium", state.module.get());
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "entry", msvc_fn));
        assert(llvm_lower_call_expression(&bi_call, &val) == llvm_gen_be_error_t::ok);

        llvm::Function* short_fn_ir = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false),
            llvm::Function::ExternalLinkage, "f", state.module.get());
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "entry", short_fn_ir));
        assert(llvm_lower_call_expression(&bi_call, &val) == llvm_gen_be_error_t::ok);

        ensure_expr_bb();
        g_seq_full_fn = nullptr; g_seq_short_fn = nullptr; g_seq_line = 0;

        // 2. Coroutine, bit_cast, bswap, constant_p, math, memory, and `_chk` builtins:
        a_variable vp = {}, vi32 = {}, vi64 = {}, vf32 = {}, vst24 = {}, vst4 = {}, vb1 = {};
        vp.type = &ptr_s32_e; vi32.type = &s32_e; vi64.type = &s64_e; vf32.type = &f32_e;
        vst24.type = &st24_e; vst4.type = &st4_e; vb1.type = &bool_i1_e;
        state.local_vars[&vp] = state.builder->CreateAlloca(ptr_t);
        state.local_vars[&vi32] = state.builder->CreateAlloca(i32_t);
        state.local_vars[&vi64] = state.builder->CreateAlloca(i64_t);
        state.local_vars[&vf32] = state.builder->CreateAlloca(f32_t);
        state.local_vars[&vst24] = state.builder->CreateAlloca(st24_ll);
        state.local_vars[&vst4] = state.builder->CreateAlloca(st4_ll);
        state.local_vars[&vb1] = state.builder->CreateAlloca(i1_t);

        an_expr_node ep = {}, ei32 = {}, ei64 = {}, ef32 = {}, est24 = {}, eb1 = {};
        ep.kind = enk_variable; ep.type = &ptr_s32_e; ep.variant.variable.ptr = &vp;
        ei32.kind = enk_variable; ei32.type = &s32_e; ei32.variant.variable.ptr = &vi32;
        ei64.kind = enk_variable; ei64.type = &s64_e; ei64.variant.variable.ptr = &vi64;
        ef32.kind = enk_variable; ef32.type = &f32_e; ef32.variant.variable.ptr = &vf32;
        est24.kind = enk_variable; est24.type = &st24_e; est24.variant.variable.ptr = &vst24;
        eb1.kind = enk_variable; eb1.type = &bool_i1_e; eb1.variant.variable.ptr = &vb1;

        assert(call_test_builtin("__builtin_constant_p", &s32_e, {&ei32}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_constant_p", nullptr, {&ei32}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_bswap16", &s32_e, {&ei32}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_bswap32", &s32_e, {&ei32}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_bswap64", &s64_e, {&ei64}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_coro_resume", &void_e, {&ep}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_coro_destroy", &void_e, {&ep}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_coro_done", &bool_e, {&ep}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_coro_promise", &ptr_s32_e, {&ep, &ei64, &ei32}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_coro_promise", &ptr_s32_e, {&ep, &ei32, &eb1}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_coro_noop", &ptr_s32_e, {}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_bit_cast", &f32_e, {&ei32}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_bit_cast", nullptr, {&ei32}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_isunordered", &s32_e, {&ef32, &ef32}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_huge_valf", &f32_e, {}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_inf", nullptr, {}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_nanf", &f32_e, {&ep}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_nan", nullptr, {&ep}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__yield", &void_e, {}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_memcmp", &s32_e, {&ep, &ep, &ei64}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_strncmp", &s32_e, {&ep, &ep, &ei64}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_strcpy", &ptr_s32_e, {&ep, &ep}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_strncpy", &ptr_s32_e, {&ep, &ep, &ei64}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin___snprintf_chk", &s32_e, {&ep, &ei64, &ei32, &ei64, &ep, &ei32}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin___sprintf_chk", &s32_e, {&ep, &ei32, &ei64, &ep, &ei32}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin___fprintf_chk", &s32_e, {&ep, &ei32, &ep, &ei32}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin___printf_chk", &s32_e, {&ei32, &ep, &ei32}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_add_overflow", &bool_i1_e, {&ei32, &ei32, &ep}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_add_overflow_p", &bool_i1_e, {&ei32, &ei32, &ei32}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_stdc_has_single_bit", &bool_i1_e, {&ei32}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__builtin_always_lock_free", &bool_e, {&ei64, &ep}) == llvm_gen_be_error_t::ok);

        // 3. Atomics (`__c11_atomic_*`, `__atomic_load`/`store`/`exchange`/`compare_exchange` with & without size arg,
        //    lock-free float/int vs 4-byte struct vs 24-byte libatomic struct, `(void*)` cast & `&var` wrappers, float & pointer `fetch_add`/`fetch_sub`)
        an_expr_node c_ord = {}; c_ord.kind = enk_constant; c_ord.type = &s32_e;
        a_constant ord5 = {}; ord5.source_corresp.decl_position.seq = 15; // 5 (seq_cst)
        c_ord.variant.constant.ptr = &ord5;

        // Wrap `&vst24`, `&vst4`, `&vf32` in `(void*)` `eok_cast` and `eok_parens` to cover `resolve_atomic_pointee_llvm_type`!
        an_expr_node addr_st24 = {}, cast_void_st24 = {}, parens_st24 = {};
        addr_st24.kind = enk_operation; addr_st24.type = &ptr_st24_e; addr_st24.variant.operation.kind = eok_address_of; addr_st24.variant.operation.operands = &est24;
        cast_void_st24.kind = enk_operation; cast_void_st24.type = &ptr_void_e; cast_void_st24.variant.operation.kind = eok_cast; cast_void_st24.variant.operation.operands = &addr_st24;
        parens_st24.kind = enk_operation; parens_st24.type = &ptr_void_e; parens_st24.variant.operation.kind = eok_parens; parens_st24.variant.operation.operands = &cast_void_st24;

        an_expr_node addr_void_st24 = {}; // eok_address_of with void* outer type so `eok_address_of` branch in `resolve_atomic_pointee_llvm_type` triggers!
        addr_void_st24.kind = enk_operation; addr_void_st24.type = &ptr_void_e; addr_void_st24.variant.operation.kind = eok_address_of; addr_void_st24.variant.operation.operands = &est24;

        an_expr_node est4 = {}; est4.kind = enk_variable; est4.type = &st4_e; est4.variant.variable.ptr = &vst4;
        an_expr_node addr_st4 = {}; addr_st4.kind = enk_operation; addr_st4.type = &ptr_st4_e; addr_st4.variant.operation.kind = eok_address_of; addr_st4.variant.operation.operands = &est4;

        an_expr_node addr_f32 = {}, cast_f32 = {};
        addr_f32.kind = enk_operation; addr_f32.type = &ptr_f32_e; addr_f32.variant.operation.kind = eok_address_of; addr_f32.variant.operation.operands = &ef32;
        cast_f32.kind = enk_operation; cast_f32.type = &ptr_f32_e; cast_f32.variant.operation.kind = eok_cast; cast_f32.variant.operation.operands = &addr_f32;

        // `__atomic_load` (void return with 24B struct -> libatomic, 4B struct -> native IntN, float -> native, plus `has_sz_arg`!)
        // and `__atomic_load_n` / `__c11_atomic_load` returning 24B struct -> libatomic!
        assert(call_test_builtin("__atomic_load", &void_e, {&parens_st24, &parens_st24, &c_ord}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__atomic_load", &void_e, {&ei64, &addr_void_st24, &addr_void_st24, &c_ord}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__atomic_load", &void_e, {&addr_st4, &addr_st4, &c_ord}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__atomic_load", &void_e, {&cast_f32, &cast_f32, &c_ord}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__c11_atomic_load", &st24_e, {&parens_st24, &c_ord}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__atomic_load", &void_e, {&ei32, &ei32, &c_ord}) == llvm_gen_be_error_t::atomic_lowering_failed);

        // `__atomic_store` & `__atomic_store_n` (24B struct, 4B struct, float, `has_sz_arg`)
        assert(call_test_builtin("__atomic_store", &void_e, {&parens_st24, &parens_st24, &c_ord}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__atomic_store", &void_e, {&ei64, &addr_st4, &addr_st4, &c_ord}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__atomic_store", &void_e, {&cast_f32, &cast_f32, &c_ord}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__atomic_store_n", &void_e, {&parens_st24, &est24, &c_ord}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__atomic_store", &void_e, {&ei32, &ei32, &c_ord}) == llvm_gen_be_error_t::atomic_lowering_failed);

        // `__atomic_exchange` & `__atomic_exchange_n` (24B struct, float, `has_sz_arg`)
        assert(call_test_builtin("__atomic_exchange", &void_e, {&parens_st24, &parens_st24, &parens_st24, &c_ord}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__atomic_exchange", &void_e, {&ei64, &cast_f32, &cast_f32, &cast_f32, &c_ord}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__atomic_exchange_n", &st24_e, {&parens_st24, &est24, &c_ord}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__atomic_exchange", &void_e, {&ei32, &ei32, &ei32, &c_ord}) == llvm_gen_be_error_t::atomic_lowering_failed);

        // `__atomic_compare_exchange` & `__atomic_compare_exchange_n` (24B struct libatomic with `bool_e` & `bool_i1_e`,
        // float lock-free, `has_sz_arg` 5-arg, non-ConstantInt `weak`, non-pointer `exp_arg_node`)
        assert(call_test_builtin("__atomic_compare_exchange", &bool_e, {&parens_st24, &parens_st24, &parens_st24, &c_ord, &c_ord, &c_ord}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__atomic_compare_exchange_n", &bool_i1_e, {&parens_st24, &parens_st24, &est24, &ei32, &c_ord, &c_ord}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__atomic_compare_exchange", &bool_i1_e, {&ei64, &cast_f32, &cast_f32, &cast_f32, &c_ord, &c_ord}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__atomic_compare_exchange_n", &bool_e, {&cast_f32, &ei32, &ef32, &ei32, &c_ord, &c_ord}) == llvm_gen_be_error_t::ok);

        // Float & pointer `atomic_fetch_add` / `fetch_sub` / `add_and_fetch` / `sub_and_fetch` (including `__c11_atomic_fetch_add` on `int*` and `void*`!)
        assert(call_test_builtin("__atomic_fetch_add", &f32_e, {&cast_f32, &ef32, &c_ord}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__atomic_add_fetch", &f32_e, {&cast_f32, &ef32, &c_ord}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__atomic_sub_fetch", &f32_e, {&cast_f32, &ef32, &c_ord}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__c11_atomic_fetch_add", &ptr_s32_e, {&ep, &ei64, &c_ord}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__atomic_add_fetch", &ptr_s32_e, {&ep, &ei64, &c_ord}) == llvm_gen_be_error_t::ok);
        assert(call_test_builtin("__atomic_sub_fetch", &ptr_void_e, {&ep, &ei64, &c_ord}) == llvm_gen_be_error_t::ok);

        // 4. Direct `sret` destination elision (`active_prvalue_dest_ptr`!) + ABI multi-slot aggregate packing & narrow int/float coercion
        a_routine_type_supplement rts_sret = {};
        a_type fn_sret_edg = {}; fn_sret_edg.kind = tk_routine;
        fn_sret_edg.variant.routine.return_type = &st24_e;
        fn_sret_edg.variant.routine.extra_info = &rts_sret;
        a_routine r_sret = {}; r_sret.source_corresp.name = (char*)"fn_sret_prval"; r_sret.type = &fn_sret_edg;
        an_expr_node op_sret = {}; op_sret.kind = enk_routine; op_sret.type = &fn_sret_edg; op_sret.variant.routine.ptr = &r_sret;
        an_expr_node call_sret = {}; call_sret.kind = enk_operation; call_sret.type = &st24_e;
        call_sret.variant.operation.kind = eok_call; call_sret.variant.operation.operands = &op_sret;

        llvm::AllocaInst* dest_sret = state.builder->CreateAlloca(st24_ll, nullptr, "prval.sret");
        state.active_prvalue_dest_ptr = dest_sret;
        state.active_prvalue_dest_ty = st4_ll; // type mismatch -> falls back to sret_alloc!
        assert(llvm_lower_call_expression(&call_sret, &val) == llvm_gen_be_error_t::ok);
        state.active_prvalue_dest_ptr = dest_sret;
        state.active_prvalue_dest_ty = st24_ll; // exact match -> direct_sret_dest!
        assert(llvm_lower_call_expression(&call_sret, &val) == llvm_gen_be_error_t::ok);
        assert(val == nullptr);
        state.active_prvalue_dest_ptr = nullptr;
        state.active_prvalue_dest_ty = nullptr;
        state.prvalue_constructed_in_place = false;

        // Also test multi-slot packing where `packed_bytes > total_arg_bytes` (e.g. 12-byte struct `{i32, i32, i32}` coerced to `{i64, i64}`!)
        // and narrow integer signed/unsigned/bool coercion + float->double variadic promotion
        a_type st12_e = {}; st12_e.kind = tk_struct; st12_e.size = 12; st12_e.alignment = 77;
        llvm::StructType* st12_ll = llvm::StructType::get(*state.context, {i32_t, i32_t, i32_t});
        state.type_cache[&st12_e] = st12_ll;
        a_variable vst12 = {}, vs16 = {}, vu16 = {};
        vst12.type = &st12_e; vs16.type = &s16_e; vu16.type = &u16_e;
        state.local_vars[&vst12] = state.builder->CreateAlloca(st12_ll);
        state.local_vars[&vs16] = state.builder->CreateAlloca(i16_t);
        state.local_vars[&vu16] = state.builder->CreateAlloca(i16_t);

        llvm::FunctionType* pre_fty = llvm::FunctionType::get(
            i32_t, {i64_t, i64_t, i32_t, i32_t, i8_t, f64_t}, false);
        llvm::Function::Create(pre_fty, llvm::Function::ExternalLinkage, "fn_coerced_args", state.module.get());
        a_routine r_coerced = {}; r_coerced.source_corresp.name = (char*)"fn_coerced_args"; r_coerced.type = &fn_sret_edg;
        an_expr_node c_op0 = {}, a_st12 = {}, a_s16 = {}, a_u16 = {}, a_b1 = {}, a_f32 = {};
        c_op0.kind = enk_routine; c_op0.variant.routine.ptr = &r_coerced; c_op0.next = &a_st12;
        a_st12.kind = enk_variable; a_st12.type = &st12_e; a_st12.variant.variable.ptr = &vst12; a_st12.next = &a_s16;
        a_s16.kind = enk_variable; a_s16.type = &s16_e; a_s16.variant.variable.ptr = &vs16; a_s16.next = &a_u16;
        a_u16.kind = enk_variable; a_u16.type = &u16_e; a_u16.variant.variable.ptr = &vu16; a_u16.next = &a_b1;
        a_b1.kind = enk_variable; a_b1.type = &bool_i1_e; a_b1.variant.variable.ptr = &vb1; a_b1.next = &a_f32;
        a_f32.kind = enk_variable; a_f32.type = &f32_e; a_f32.variant.variable.ptr = &vf32; a_f32.next = nullptr;
        an_expr_node call_coerced = {}; call_coerced.kind = enk_operation; call_coerced.type = &s32_e;
        call_coerced.variant.operation.kind = eok_call; call_coerced.variant.operation.operands = &c_op0;
        assert(llvm_lower_call_expression(&call_coerced, &val) == llvm_gen_be_error_t::ok);
      }

      // --- 10.E: Type traits (`evaluate_type_trait_builtin`), LValue/RValue/EH/Overriding/CompoundAssign/New ---
      {
        ensure_expr_bb();
        a_type s32_e = {}; s32_e.kind = tk_integer; s32_e.size = 4; s32_e.variant.integer.int_kind = ik_int;
        a_type void_e = {}; void_e.kind = tk_void;
        a_type rty_e = {}; rty_e.kind = tk_routine;
        a_type ref_e = {}; ref_e.kind = tk_pointer; ref_e.size = 555; ref_e.variant.pointer.is_reference = TRUE;

        // Build base & derived classes for DFS `bok_is_base_of`, `bok_has_virtual_destructor`, `bok_is_aggregate`, `bok_is_trivial`, `bok_is_standard_layout`, `bok_is_pod`
        a_routine vdtor = {}; vdtor.special_kind = sfk_destructor; vdtor.is_virtual = TRUE;
        a_scope base_a_scope = {}; base_a_scope.routines = &vdtor;
        a_class_type_supplement base_a_ext = {}; base_a_ext.assoc_scope = &base_a_scope;
        a_type cls_a = {}; cls_a.kind = tk_class; cls_a.size = 8; cls_a.variant.class_struct_union.extra_info = &base_a_ext;

        a_base_class_derivation d_pub = {}, d_priv = {};
        d_pub.access = as_public;
        d_priv.access = as_private;

        a_base_class bc_b_to_a = {}; bc_b_to_a.type = &cls_a; bc_b_to_a.derivation = &d_pub;
        a_class_type_supplement base_b_ext = {}; base_b_ext.base_classes = &bc_b_to_a; base_b_ext.direct_base_classes = &bc_b_to_a;
        a_type cls_b = {}; cls_b.kind = tk_class; cls_b.size = 8; cls_b.variant.class_struct_union.extra_info = &base_b_ext;

        // Diamond / indirect C -> (null base, B -> A, B again)
        a_base_class bc_null = {}, bc_c_to_b1 = {}, bc_c_to_b2 = {};
        bc_null.type = nullptr; bc_null.next = &bc_c_to_b1;
        bc_c_to_b1.type = &cls_b; bc_c_to_b1.derivation = &d_pub; bc_c_to_b1.next = &bc_c_to_b2; bc_c_to_b1.next_direct = &bc_c_to_b2;
        bc_c_to_b2.type = &cls_b; bc_c_to_b2.derivation = &d_priv; bc_c_to_b2.is_virtual = TRUE; bc_c_to_b2.next = nullptr; bc_c_to_b2.next_direct = nullptr;
        a_routine ud_ctor = {}; ud_ctor.special_kind = sfk_constructor; ud_ctor.compiler_generated = FALSE; ud_ctor.is_trivial_default_constructor = FALSE;
        a_scope c_scope = {}; c_scope.routines = &ud_ctor;
        a_class_type_supplement c_ext = {}; c_ext.base_classes = &bc_null; c_ext.direct_base_classes = &bc_c_to_b1; c_ext.assoc_scope = &c_scope;
        a_field f_priv = {}, f_pub = {}, f_inited = {}, f_ref = {}, f_void = {};
        f_priv.type = &s32_e; f_priv.source_corresp.access = as_private; f_priv.next = &f_pub;
        f_pub.type = &s32_e; f_pub.source_corresp.access = as_public; f_pub.next = nullptr;
        f_inited.type = &s32_e; f_inited.source_corresp.access = as_public;
        f_ref.type = &ref_e; f_ref.source_corresp.access = as_public;
        f_void.type = &void_e; f_void.source_corresp.access = as_public;

        a_type cls_c = {}; cls_c.kind = tk_class; cls_c.size = 16;
        cls_c.variant.class_struct_union.extra_info = &c_ext;
        cls_c.variant.class_struct_union.field_list = &f_priv;

        auto eval_trait = [&](a_builtin_operation_kind k, a_type_ptr t1, a_type_ptr t2 = nullptr) noexcept -> llvm_gen_be_error_t {
          an_expr_node o1 = {}, o2 = {}, be = {};
          o1.kind = enk_type_operand; o1.variant.type_operand.type = t1;
          if (t2) { o2.kind = enk_type_operand; o2.variant.type_operand.type = t2; o1.next = &o2; }
          be.kind = enk_builtin_operation; be.type = &s32_e; be.variant.builtin_operation.kind = k;
          be.variant.builtin_operation.operands = &o1;
          return llvm_lower_builtin_operation(&be, &val);
        };

        // Indirect base C -> B -> A, virtual dtor inherited from A, non-trivially-copyable (`size = 444`)
        assert(eval_trait(bok_is_base_of, &cls_a, &cls_c) == llvm_gen_be_error_t::ok);
        assert(eval_trait(bok_has_virtual_destructor, &cls_c) == llvm_gen_be_error_t::ok);
        run_expr_oom([&]() noexcept { return eval_trait(bok_is_base_of, &cls_a, &cls_c); });

        // `bok_is_aggregate` (private field, virtual/private base, user-defined ctor, clean aggregate)
        assert(eval_trait(bok_is_aggregate, &cls_c) == llvm_gen_be_error_t::ok);
        a_type cls_agg = {}; cls_agg.kind = tk_struct; cls_agg.size = 4; cls_agg.variant.class_struct_union.field_list = &f_inited;
        assert(eval_trait(bok_is_aggregate, &cls_agg) == llvm_gen_be_error_t::ok);
        cls_agg.variant.class_struct_union.extra_info = &c_ext; // private/virtual base & user ctor!
        assert(eval_trait(bok_is_aggregate, &cls_agg) == llvm_gen_be_error_t::ok);

        // `bok_is_trivially_copyable` & `bok_is_trivial` (`size = 444` false, `has_field_initializer` false, `ud_ctor` false, clean true, `void` false)
        a_type nontriv_e = {}; nontriv_e.kind = tk_struct; nontriv_e.size = 444;
        assert(eval_trait(bok_is_trivially_copyable, &nontriv_e) == llvm_gen_be_error_t::ok);
        assert(eval_trait(bok_is_trivially_copyable, &void_e) == llvm_gen_be_error_t::ok);
        assert(eval_trait(bok_is_trivial, &cls_c) == llvm_gen_be_error_t::ok);
        a_class_type_supplement inited_ext = {}; inited_ext.has_field_initializer = TRUE;
        cls_agg.variant.class_struct_union.extra_info = &inited_ext;
        assert(eval_trait(bok_is_trivial, &cls_agg) == llvm_gen_be_error_t::ok);

        // `bok_is_standard_layout` & `bok_is_pod` (`void`, `rty`, mixed access `f_priv->f_pub`, ref field `f_ref`, void field `f_void`, multiple classes with fields!)
        assert(eval_trait(bok_is_standard_layout, &void_e) == llvm_gen_be_error_t::ok);
        assert(eval_trait(bok_is_standard_layout, &rty_e) == llvm_gen_be_error_t::ok);
        assert(eval_trait(bok_is_standard_layout, &cls_c) == llvm_gen_be_error_t::ok); // mixed access!
        a_type cls_ref_f = {}; cls_ref_f.kind = tk_struct; cls_ref_f.size = 8; cls_ref_f.variant.class_struct_union.field_list = &f_ref;
        assert(eval_trait(bok_is_standard_layout, &cls_ref_f) == llvm_gen_be_error_t::ok);
        cls_ref_f.variant.class_struct_union.field_list = &f_void;
        assert(eval_trait(bok_is_standard_layout, &cls_ref_f) == llvm_gen_be_error_t::ok);
        // Derived with fields AND base with fields (`classes_with_fields > 1`)!
        a_field f_base_one = {}; f_base_one.type = &s32_e; f_base_one.source_corresp.access = as_public;
        a_type cls_base_wf = {}; cls_base_wf.kind = tk_struct; cls_base_wf.size = 4; cls_base_wf.variant.class_struct_union.field_list = &f_base_one;
        a_base_class bc_wf = {}; bc_wf.type = &cls_base_wf; bc_wf.derivation = &d_pub;
        a_class_type_supplement drv_wf_ext = {}; drv_wf_ext.base_classes = &bc_wf;
        a_type cls_drv_wf = {}; cls_drv_wf.kind = tk_struct; cls_drv_wf.size = 8;
        cls_drv_wf.variant.class_struct_union.field_list = &f_pub;
        cls_drv_wf.variant.class_struct_union.extra_info = &drv_wf_ext;
        assert(eval_trait(bok_is_standard_layout, &cls_drv_wf) == llvm_gen_be_error_t::ok);
        assert(eval_trait(bok_is_pod, &cls_base_wf) == llvm_gen_be_error_t::ok);

        // LValue: `enk_routine` with valid routine & empty routine, unmaterialized local (`sc_auto`, unnamed `sc_static`, unnamed `is_thread_local`), `eok_dot_static` & `eok_comma`!
        a_routine valid_lval_rt = {}; valid_lval_rt.source_corresp.name = (char*)"valid_lval_fn"; valid_lval_rt.type = &rty_e;
        llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false),
                               llvm::Function::ExternalLinkage, "valid_lval_fn", state.module.get());
        an_expr_node lr_e = {}; lr_e.kind = enk_routine; lr_e.type = &rty_e; lr_e.variant.routine.ptr = &valid_lval_rt;
        assert(llvm_lower_lvalue_expression(&lr_e, &val) == llvm_gen_be_error_t::ok);
        a_routine empty_lval_rt = {}; empty_lval_rt.source_corresp.name = (char*)""; empty_lval_rt.type = &s32_e;
        lr_e.variant.routine.ptr = &empty_lval_rt;
        assert(llvm_lower_lvalue_expression(&lr_e, &val) == llvm_gen_be_error_t::ok);

        a_variable coro_auto_v = {}, coro_stat_v = {}, coro_tls_v = {};
        coro_auto_v.type = &s32_e; coro_auto_v.storage_class = sc_auto; coro_auto_v.source_corresp.name = nullptr;
        coro_stat_v.type = &s32_e; coro_stat_v.storage_class = sc_static; coro_stat_v.source_corresp.name = nullptr;
        coro_tls_v.type = &s32_e; coro_tls_v.storage_class = sc_static; coro_tls_v.is_thread_local = TRUE; coro_tls_v.source_corresp.name = nullptr;
        for (a_variable* cv : {&coro_auto_v, &coro_stat_v, &coro_tls_v}) {
          an_expr_node cve = {}; cve.kind = enk_variable; cve.type = &s32_e; cve.variant.variable.ptr = cv;
          assert(llvm_lower_lvalue_expression(&cve, &val) == llvm_gen_be_error_t::ok);
        }

        // `eok_dot_static` & `eok_comma` in `llvm_lower_lvalue_expression`, and `eok_lvalue` rvalue load in `llvm_lower_expression`!
        a_variable lv_s32 = {}; lv_s32.type = &s32_e;
        state.local_vars[&lv_s32] = state.builder->CreateAlloca(i32_t);
        an_expr_node lv_op1 = {}, lv_op2 = {};
        lv_op1.kind = enk_constant; lv_op1.type = &s32_e; lv_op1.next = &lv_op2;
        lv_op2.kind = enk_variable; lv_op2.type = &s32_e; lv_op2.is_lvalue = TRUE; lv_op2.variant.variable.ptr = &lv_s32;
        an_expr_node dot_st_lval = {}; dot_st_lval.kind = enk_operation; dot_st_lval.type = &s32_e;
        dot_st_lval.variant.operation.kind = eok_dot_static; dot_st_lval.variant.operation.operands = &lv_op1;
        assert(llvm_lower_lvalue_expression(&dot_st_lval, &val) == llvm_gen_be_error_t::ok);
        dot_st_lval.variant.operation.kind = eok_comma;
        assert(llvm_lower_lvalue_expression(&dot_st_lval, &val) == llvm_gen_be_error_t::ok);

        an_expr_node rval_from_lval = {}; rval_from_lval.kind = enk_operation; rval_from_lval.type = &s32_e;
        rval_from_lval.is_lvalue = FALSE; rval_from_lval.variant.operation.kind = eok_lvalue;
        rval_from_lval.variant.operation.operands = &lv_op2;
        assert(llvm_lower_expression(&rval_from_lval, &val) == llvm_gen_be_error_t::ok);

        // `leck_*` (`enk_lowered_eh_construct`) & `enk_result_of_overriding_function`!
#if DO_IL_LOWERING
        an_expr_node eh_e = {}; eh_e.kind = enk_lowered_eh_construct; eh_e.type = &void_e;
        for (a_lowered_eh_construct_kind lk : {leck_caught_object_address, leck_thrown_object_address,
                                                leck_cleanup_state, leck_unreachable_cleanup_state,
                                                static_cast<a_lowered_eh_construct_kind>(99)}) {
          eh_e.variant.lowered_eh.kind = lk;
          assert(llvm_lower_expression(&eh_e, &val) == llvm_gen_be_error_t::ok);
        }
        state.active_caught_object_ptr = state.local_vars[&lv_s32];
        state.active_thrown_object_ptr = state.local_vars[&lv_s32];
        eh_e.variant.lowered_eh.kind = leck_caught_object_address;
        assert(llvm_lower_expression(&eh_e, &val) == llvm_gen_be_error_t::ok);
        eh_e.variant.lowered_eh.kind = leck_thrown_object_address;
        assert(llvm_lower_expression(&eh_e, &val) == llvm_gen_be_error_t::ok);
        state.active_caught_object_ptr = nullptr;
        state.active_thrown_object_ptr = nullptr;

        // `leck_internal_try`: null `first_e`, non-throwing `first_e` (`hasNPredecessors(0)`), throwing `first_e` with `second_e` & with/without `enclosing_unwind`!
        eh_e.variant.lowered_eh.kind = leck_internal_try;
        eh_e.variant.lowered_eh.variant.try_and_catch_expr = nullptr;
        assert(llvm_lower_expression(&eh_e, &val) == llvm_gen_be_error_t::ok);
        eh_e.variant.lowered_eh.variant.try_and_catch_expr = &lv_op2;
        assert(llvm_lower_expression(&eh_e, &val) == llvm_gen_be_error_t::ok); // 0 predecessors -> erased!

        // Throwing `first_e` via `enk_initializer` with `g_dyn_init_emit_invoke = 1`!
        a_dynamic_init try_dip = {}; try_dip.kind = dik_constructor;
        an_expr_node throw_first = {}, clean_second = {};
        throw_first.kind = enk_initializer; throw_first.type = &s32_e; throw_first.variant.initializer.dyn_init = &try_dip;
        throw_first.next = &clean_second;
        clean_second.kind = enk_constant; clean_second.type = &s32_e;
        eh_e.variant.lowered_eh.variant.try_and_catch_expr = &throw_first;
        g_dyn_init_emit_invoke = 1;
        assert(llvm_lower_expression(&eh_e, &val) == llvm_gen_be_error_t::ok); // no enclosing_unwind -> CreateResume!
        llvm::BasicBlock* outer_lpad = llvm::BasicBlock::Create(*state.context, "outer.lpad", ws10_fn);
        state.current_landing_pads.push_back(outer_lpad);
        assert(llvm_lower_expression(&eh_e, &val) == llvm_gen_be_error_t::ok); // enclosing_unwind -> CreateBr!
        state.current_landing_pads.pop_back();
        {
          llvm::IRBuilder<> ob(outer_lpad);
          ob.CreateUnreachable();
        }
        g_dyn_init_emit_invoke = 0;

#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
        an_expr_node ovr_e = {}; ovr_e.kind = enk_result_of_overriding_function; ovr_e.type = &s32_e;
        state.current_edg_routine = nullptr;
        assert(llvm_lower_expression(&ovr_e, &val) == llvm_gen_be_error_t::invalid_argument);
        a_routine wrap_rt = {}, under_empty = {}, under_ok = {};
        under_empty.source_corresp.name = (char*)""; under_empty.type = &s32_e;
        wrap_rt.overriding_function_for_wrapper = &under_empty;
        state.current_edg_routine = &wrap_rt;
        assert(llvm_lower_expression(&ovr_e, &val) == llvm_gen_be_error_t::invalid_argument);

        llvm::Function* under_fn_ir = llvm::Function::Create(
            llvm::FunctionType::get(i32_t, {i32_t, ptr_t}, false),
            llvm::Function::ExternalLinkage, "covariant_under_fn", state.module.get());
        assert(under_fn_ir != nullptr);
        under_ok.source_corresp.name = (char*)"covariant_under_fn"; under_ok.type = &rty_e;
        wrap_rt.overriding_function_for_wrapper = &under_ok;

        llvm::Function* wrapper_fn_ir = llvm::Function::Create(
            llvm::FunctionType::get(i32_t, {i32_t, ptr_t}, false),
            llvm::Function::ExternalLinkage, "covariant_wrapper_fn", state.module.get());
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "entry", wrapper_fn_ir));
        a_variable wp1 = {}; wp1.type = &s32_e;
        state.local_vars[&wp1] = state.builder->CreateAlloca(i32_t);
        a_scope wscope = {}; wscope.variant.routine.parameters = &wp1;
        state.current_function_scope = &wscope;
        assert(llvm_lower_expression(&ovr_e, &val) == llvm_gen_be_error_t::ok);
        state.current_function_scope = nullptr;
        state.current_edg_routine = nullptr;
        state.builder->SetInsertPoint(&ws10_fn->back());
        ensure_expr_bb();
#endif
#endif

        // Atomic pointer & float compound assignments (`++`, `--`, `+=`, `-=`, `p+=`, `p-=`) + `_Complex int` += scalar!
        a_type ptr_s32_e = {}; ptr_s32_e.kind = tk_pointer; ptr_s32_e.size = 8; ptr_s32_e.variant.pointer.type = &s32_e;
        a_type atom_ptr_e = {}; atom_ptr_e.kind = tk_typeref; atom_ptr_e.variant.typeref.type = &ptr_s32_e; atom_ptr_e.variant.typeref.qualifiers = TQ_C11_ATOMIC;
        a_type f32_e = {}; f32_e.kind = tk_float; f32_e.size = 4; f32_e.variant.float_kind = fk_float;
        a_type atom_f32_e = {}; atom_f32_e.kind = tk_typeref; atom_f32_e.variant.typeref.type = &f32_e; atom_f32_e.variant.typeref.qualifiers = TQ_C11_ATOMIC;

        a_variable vap = {}, vaf = {};
        vap.type = &atom_ptr_e; vaf.type = &atom_f32_e;
        state.local_vars[&vap] = state.builder->CreateAlloca(ptr_t);
        state.local_vars[&vaf] = state.builder->CreateAlloca(f32_t);

        an_expr_node eap = {}, eaf = {}, erhs1 = {};
        eap.kind = enk_variable; eap.type = &atom_ptr_e; eap.variant.variable.ptr = &vap;
        eaf.kind = enk_variable; eaf.type = &atom_f32_e; eaf.variant.variable.ptr = &vaf;
        erhs1.kind = enk_constant; erhs1.type = &s32_e;

        an_expr_node ca_e = {}; ca_e.kind = enk_operation;
        for (an_expr_operator_kind pk : {eok_pre_incr, eok_post_incr, eok_pre_decr, eok_post_decr}) {
          eap.next = nullptr; ca_e.type = &ptr_s32_e; ca_e.variant.operation.operands = &eap; ca_e.variant.operation.kind = pk;
          assert(llvm_lower_compound_assignment(&ca_e, &val) == llvm_gen_be_error_t::ok);
          eaf.next = nullptr; ca_e.type = &f32_e; ca_e.variant.operation.operands = &eaf; ca_e.variant.operation.kind = pk;
          assert(llvm_lower_compound_assignment(&ca_e, &val) == llvm_gen_be_error_t::ok);
        }
        eap.next = &erhs1; ca_e.type = &ptr_s32_e; ca_e.variant.operation.operands = &eap;
        for (an_expr_operator_kind pk : {eok_add_assign, eok_subtract_assign, eok_padd_assign, eok_psubtract_assign}) {
          ca_e.variant.operation.kind = pk;
          assert(llvm_lower_compound_assignment(&ca_e, &val) == llvm_gen_be_error_t::ok);
        }

        // Array `new T[N]` with `dik_constructor` + `destructor` + `freeing_of_storage_on_exception` (`g_dyn_init_emit_invoke = 1`!)
        ensure_expr_bb();
        a_routine new_arr_rt = {}, elem_ctor_rt = {}, elem_dtor_rt = {}, free_rt = {};
        new_arr_rt.source_corresp.name = (char*)"_Znam"; new_arr_rt.type = &rty_e;
        elem_ctor_rt.source_corresp.name = (char*)"_ZN4ElemC1Ev";
        elem_dtor_rt.source_corresp.name = (char*)"_ZN4ElemD1Ev";
        free_rt.source_corresp.name = (char*)"_ZdaPv";
        a_dynamic_init arr_ctor_dip = {}, arr_free_dip = {};
        arr_ctor_dip.kind = dik_constructor;
        arr_ctor_dip.variant.constructor.ptr = &elem_ctor_rt;
        arr_ctor_dip.destructor = &elem_dtor_rt;
        arr_free_dip.kind = dik_none; arr_free_dip.destructor = &free_rt;
        an_expr_node sz_arg = {}, cnt_e = {};
        sz_arg.kind = enk_constant; sz_arg.type = &s32_e;
        cnt_e.kind = enk_constant; cnt_e.type = &s32_e;
        a_new_delete_supplement nd_arr = {};
        nd_arr.is_new = TRUE;
        nd_arr.type = &s32_e; nd_arr.routine = &new_arr_rt; nd_arr.arg = &sz_arg;
        nd_arr.number_of_elements = &cnt_e;
        nd_arr.dynamic_init = &arr_ctor_dip;
        nd_arr.freeing_of_storage_on_exception = &arr_free_dip;
        an_expr_node new_arr_expr = {}; new_arr_expr.kind = enk_new_delete; new_arr_expr.type = &ptr_s32_e;
        new_arr_expr.variant.new_delete = &nd_arr;

        // Without invoke (unwind_bb has 0 predecessors -> erased!), then WITH invoke on a function WITHOUT personality AND WITH personality,
        // plus variants with `destructor == nullptr` and `freeing_of_storage_on_exception == nullptr` / `free_dip.destructor == nullptr`!
        assert(llvm_lower_new_expression(&new_arr_expr, &val) == llvm_gen_be_error_t::ok);
        g_dyn_init_emit_invoke = 1;
        llvm::Function* fresh_new_fn = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false),
            llvm::Function::ExternalLinkage, "fresh_new_no_pers", state.module.get());
        state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "entry", fresh_new_fn));
        assert(llvm_lower_new_expression(&new_arr_expr, &val) == llvm_gen_be_error_t::ok); // !hasPersonalityFn -> sets personality!
        run_expr_oom([&]() noexcept {
          if (!state.builder->GetInsertBlock() || EDG_LLVM_GET_TERMINATOR(state.builder->GetInsertBlock())) {
            state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "oom.new", fresh_new_fn));
          }
          return llvm_lower_new_expression(&new_arr_expr, &val);
        });
        arr_ctor_dip.destructor = nullptr;
        arr_free_dip.destructor = nullptr;
        assert(llvm_lower_new_expression(&new_arr_expr, &val) == llvm_gen_be_error_t::ok);
        nd_arr.freeing_of_storage_on_exception = nullptr;
        assert(llvm_lower_new_expression(&new_arr_expr, &val) == llvm_gen_be_error_t::ok); // need_arr_eh == false!
        g_dyn_init_emit_invoke = 0;
        {
          llvm::IRBuilder<> fb(&fresh_new_fn->back());
          if (!EDG_LLVM_GET_TERMINATOR(&fresh_new_fn->back())) fb.CreateRetVoid();
        }
        state.builder->SetInsertPoint(&ws10_fn->back());
        ensure_expr_bb();
      }

      // --- 10.F (Workstream 11): Final branch & OOM closure across llvm_gen_be_expr.cpp ---
      {
        state.type_cache.clear();
        ensure_expr_bb();
        a_type s8_e = {}; s8_e.kind = tk_integer; s8_e.size = 1; s8_e.variant.integer.int_kind = ik_int;
        a_type s16_e = {}; s16_e.kind = tk_integer; s16_e.size = 2; s16_e.variant.integer.int_kind = ik_short;
        a_type u16_e = {}; u16_e.kind = tk_integer; u16_e.size = 2; u16_e.variant.integer.int_kind = ik_unsigned_short;
        a_type s32_e = {}; s32_e.kind = tk_integer; s32_e.size = 4; s32_e.variant.integer.int_kind = ik_int;
        a_type u32_e = {}; u32_e.kind = tk_integer; u32_e.size = 4; u32_e.variant.integer.int_kind = ik_unsigned_int;
        a_type s64_e = {}; s64_e.kind = tk_integer; s64_e.size = 8; s64_e.variant.integer.int_kind = ik_long_long;
        a_type s256_e = {}; s256_e.kind = tk_integer; s256_e.size = 32; s256_e.variant.integer.int_kind = ik_int;
        state.type_cache[&s256_e] = llvm::IntegerType::get(*state.context, 256);
        a_type bool_e = {}; bool_e.kind = tk_integer; bool_e.size = 1; bool_e.variant.integer.bool_type = TRUE;
        a_type bool_i1_e = {}; bool_i1_e.kind = tk_integer; bool_i1_e.size = 1; bool_i1_e.variant.integer.bool_type = TRUE;
        state.type_cache[&bool_i1_e] = i1_t;
        a_type void_e = {}; void_e.kind = tk_void;
        a_type f32_e = {}; f32_e.kind = tk_float; f32_e.size = 4; f32_e.variant.float_kind = fk_float;
        a_type f64_e = {}; f64_e.kind = tk_float; f64_e.size = 8; f64_e.variant.float_kind = fk_double;
        a_type rty_e = {}; rty_e.kind = tk_routine;
        a_type ref_e = {}; ref_e.kind = tk_pointer; ref_e.size = 555; ref_e.variant.pointer.is_reference = TRUE;
        a_type ptr_s32_e = {}; ptr_s32_e.kind = tk_pointer; ptr_s32_e.size = 8; ptr_s32_e.variant.pointer.type = &s32_e;
        a_type ptr_s8_e = {}; ptr_s8_e.kind = tk_pointer; ptr_s8_e.size = 8; ptr_s8_e.variant.pointer.type = &s8_e;
        a_type ptr_void_e = {}; ptr_void_e.kind = tk_pointer; ptr_void_e.size = 8; ptr_void_e.variant.pointer.type = &void_e;
        a_type ptr_null_e = {}; ptr_null_e.kind = tk_pointer; ptr_null_e.size = 8; ptr_null_e.variant.pointer.type = nullptr;
        a_type ptr_ptr_s32_e = {}; ptr_ptr_s32_e.kind = tk_pointer; ptr_ptr_s32_e.size = 8; ptr_ptr_s32_e.variant.pointer.type = &ptr_s32_e;
        a_type ptr_ptr_s8_e = {}; ptr_ptr_s8_e.kind = tk_pointer; ptr_ptr_s8_e.size = 8; ptr_ptr_s8_e.variant.pointer.type = &ptr_s8_e;
        a_type ptr_ptr_void_e = {}; ptr_ptr_void_e.kind = tk_pointer; ptr_ptr_void_e.size = 8; ptr_ptr_void_e.variant.pointer.type = &ptr_void_e;
        a_type tref_null_e = {}; tref_null_e.kind = tk_typeref; tref_null_e.variant.typeref.type = &s32_e;
        state.type_cache[&tref_null_e] = i32_t;

        // 1. Bitfield >64-bit span (`offset=0, bit_size=72` -> `*out_bits > 64U` TRUE at L192!), non-power-of-2 volatile `size=3` (24 bits -> `isPowerOf2_32` FALSE at L185!),
        //    and signed full-width bitfield (`bit_field_is_signed = TRUE` && `bit_size == container_bits` at L256!)
        {
          a_type vol24_e = {}; vol24_e.kind = tk_integer; vol24_e.size = 3;
          a_type vol24_wrap = {}; vol24_wrap.kind = tk_typeref; vol24_wrap.variant.typeref.type = &vol24_e; vol24_wrap.variant.typeref.qualifiers = TQ_VOLATILE;
          llvm::AllocaInst* bf16 = state.builder->CreateAlloca(llvm::ArrayType::get(i8_t, 16));
          a_field bf_wide = {}; bf_wide.is_bit_field = TRUE; bf_wide.type = &vol24_wrap; bf_wide.offset_bit_remainder = 0; bf_wide.bit_size = 72;
          assert(llvm_emit_bitfield_load(bf16, &bf_wide, &s64_e, &val) == llvm_gen_be_error_t::ok);
          bf_wide.type = &s8_e; bf_wide.bit_field_is_signed = TRUE; bf_wide.bit_size = 8;
          assert(llvm_emit_bitfield_load(bf16, &bf_wide, &s8_e, &val) == llvm_gen_be_error_t::ok);
        }

        // 2. UBSan OOM sweeps (`llvm_emit_ubsan_type_descriptor` & `llvm_emit_ubsan_check`), atomic 256-bit `load_from_lvalue_ptr` (`size <= 16` FALSE at L525 & L4573!),
        //    and `broadcast_scalar_to_vector` with matching scalar type (`sc->getType() == elem_ty` at L584!)
        {
          llvm::GlobalVariable* udesc = nullptr;
          run_expr_oom([&]() noexcept { return llvm_emit_ubsan_type_descriptor(&s32_e, &udesc); });
          run_expr_oom([&]() noexcept { return llvm_emit_ubsan_check("__ubsan_handle_add_overflow", llvm::ConstantInt::getTrue(*state.context), {}); });

          a_type atom_s256 = {}; atom_s256.kind = tk_typeref; atom_s256.variant.typeref.type = &s256_e; atom_s256.variant.typeref.qualifiers = TQ_C11_ATOMIC;
          a_variable v_a256 = {}; v_a256.type = &atom_s256;
          state.local_vars[&v_a256] = state.builder->CreateAlloca(llvm::IntegerType::get(*state.context, 256));
          an_expr_node e_a256 = {}, e_a256_rhs = {}, e_a256_asgn = {};
          e_a256.kind = enk_variable; e_a256.type = &atom_s256; e_a256.is_lvalue = FALSE; e_a256.variant.variable.ptr = &v_a256;
          assert(llvm_lower_expression(&e_a256, &val) == llvm_gen_be_error_t::ok); // load_from_lvalue_ptr size=32 > 16!
          e_a256.is_lvalue = TRUE;
          e_a256_rhs.kind = enk_constant; e_a256_rhs.type = &s256_e;
          e_a256.next = &e_a256_rhs;
          e_a256_asgn.kind = enk_operation; e_a256_asgn.type = &s256_e; e_a256_asgn.variant.operation.kind = eok_assign; e_a256_asgn.variant.operation.operands = &e_a256;
          assert(llvm_lower_expression(&e_a256_asgn, &val) == llvm_gen_be_error_t::ok); // eok_assign size=32 > 16!

          // Vector + matching scalar `i32` (`broadcast_scalar_to_vector` `sc->getType() == elem_ty`!)
          llvm::VectorType* vec4_i32 = llvm::FixedVectorType::get(i32_t, 4);
          a_type vec_e = {}; vec_e.kind = tk_array; vec_e.size = 16;
          state.type_cache[&vec_e] = vec4_i32;
          a_variable vv4 = {}, vs32 = {}; vv4.type = &vec_e; vs32.type = &s32_e;
          state.local_vars[&vv4] = state.builder->CreateAlloca(vec4_i32);
          state.local_vars[&vs32] = state.builder->CreateAlloca(i32_t);
          an_expr_node ev4 = {}, es32 = {}, e_vadd = {};
          ev4.kind = enk_variable; ev4.type = &vec_e; ev4.variant.variable.ptr = &vv4; ev4.next = &es32;
          es32.kind = enk_variable; es32.type = &s32_e; es32.variant.variable.ptr = &vs32;
          e_vadd.kind = enk_operation; e_vadd.type = &vec_e; e_vadd.variant.operation.kind = eok_add; e_vadd.variant.operation.operands = &ev4;
          run_expr_oom([&]() noexcept { return llvm_lower_arithmetic_expression(&e_vadd, &val); });
        }

        // 3. `llvm_lower_lvalue_expression` remaining branches (`sc_register`, named coro local, untyped `sc_auto` & `sc_static`,
        //    `eok_call` lvalue with in-place/value/void/null-type, `eok_ref_indirect`, `default:`, and OOM sweeps!)
        {
          a_variable vr_named = {}, va_untyped = {}, vs_untyped = {};
          vr_named.storage_class = sc_register; vr_named.type = &s32_e; vr_named.source_corresp.name = (char*)"reg_loc";
          va_untyped.storage_class = sc_auto; va_untyped.type = nullptr;
          vs_untyped.storage_class = sc_static; vs_untyped.type = nullptr;
          an_expr_node ev = {}; ev.kind = enk_variable; ev.type = &s32_e;
          ev.variant.variable.ptr = &vr_named;
          assert(llvm_lower_lvalue_expression(&ev, &val) == llvm_gen_be_error_t::ok);
          ev.variant.variable.ptr = &va_untyped;
          assert(llvm_lower_lvalue_expression(&ev, &val) == llvm_gen_be_error_t::unsupported_expr);
          ev.variant.variable.ptr = &vs_untyped;
          assert(llvm_lower_lvalue_expression(&ev, &val) == llvm_gen_be_error_t::unsupported_expr);

          // Detached builder (`GetInsertBlock() == nullptr`) on `sc_auto`!
          llvm::IRBuilder<>::InsertPoint saved_ip = state.builder->saveIP();
          state.builder->ClearInsertionPoint();
          a_variable va_no_bb = {}; va_no_bb.storage_class = sc_auto; va_no_bb.type = &s32_e;
          ev.variant.variable.ptr = &va_no_bb;
          assert(llvm_lower_lvalue_expression(&ev, &val) == llvm_gen_be_error_t::unsupported_expr);
          state.builder->restoreIP(saved_ip);

          // `eok_call` in `llvm_lower_lvalue_expression` (normal value spill, void return, in-place `sret`, and `expr->type == nullptr`)!
          a_type st24_e = {}; st24_e.kind = tk_struct; st24_e.size = 24; st24_e.alignment = 77;
          llvm::StructType* st24_ll = llvm::StructType::get(*state.context, {i64_t, i64_t, i64_t});
          state.type_cache[&st24_e] = st24_ll;
          a_routine_type_supplement rts0 = {};
          a_type fn_s32_edg = {}; fn_s32_edg.kind = tk_routine; fn_s32_edg.variant.routine.return_type = &s32_e; fn_s32_edg.variant.routine.extra_info = &rts0;
          a_type fn_void_edg = {}; fn_void_edg.kind = tk_routine; fn_void_edg.variant.routine.return_type = &void_e; fn_void_edg.variant.routine.extra_info = &rts0;
          a_type fn_st24_edg = {}; fn_st24_edg.kind = tk_routine; fn_st24_edg.variant.routine.return_type = &st24_e; fn_st24_edg.variant.routine.extra_info = &rts0;
          a_routine r_s32 = {}, r_void = {}, r_st24 = {};
          r_s32.source_corresp.name = (char*)"lv_fn_s32"; r_s32.type = &fn_s32_edg;
          r_void.source_corresp.name = (char*)"lv_fn_void"; r_void.type = &fn_void_edg;
          r_st24.source_corresp.name = (char*)"lv_fn_st24"; r_st24.type = &fn_st24_edg;

          an_expr_node op_c = {}, lv_call = {};
          op_c.kind = enk_routine; lv_call.kind = enk_operation; lv_call.variant.operation.kind = eok_call; lv_call.variant.operation.operands = &op_c;

          op_c.type = &fn_s32_edg; op_c.variant.routine.ptr = &r_s32; lv_call.type = &s32_e;
          run_expr_oom([&]() noexcept { return llvm_lower_lvalue_expression(&lv_call, &val); });
          op_c.type = &fn_void_edg; op_c.variant.routine.ptr = &r_void; lv_call.type = &void_e;
          assert(llvm_lower_lvalue_expression(&lv_call, &val) == llvm_gen_be_error_t::ok);
          lv_call.type = nullptr;
          assert(llvm_lower_lvalue_expression(&lv_call, &val) == llvm_gen_be_error_t::unsupported_expr);

          llvm::AllocaInst* lv_sret_dst = state.builder->CreateAlloca(st24_ll);
          op_c.type = &fn_st24_edg; op_c.variant.routine.ptr = &r_st24; lv_call.type = &st24_e;
          state.active_prvalue_dest_ptr = lv_sret_dst;
          assert(llvm_lower_lvalue_expression(&lv_call, &val) == llvm_gen_be_error_t::ok);

          // `eok_ref_indirect` & `default:` in `llvm_lower_lvalue_expression`
          a_variable vp = {}; vp.type = &ptr_s32_e; state.local_vars[&vp] = state.builder->CreateAlloca(ptr_t);
          an_expr_node ep = {}, ref_ind = {};
          ep.kind = enk_variable; ep.type = &ptr_s32_e; ep.variant.variable.ptr = &vp;
          ref_ind.kind = enk_operation; ref_ind.type = &s32_e; ref_ind.variant.operation.kind = eok_ref_indirect; ref_ind.variant.operation.operands = &ep;
          assert(llvm_lower_lvalue_expression(&ref_ind, &val) == llvm_gen_be_error_t::ok);

          an_expr_node unk_lv = {}; unk_lv.kind = static_cast<an_expr_node_kind>(250);
          assert(llvm_lower_lvalue_expression(&unk_lv, &val) == llvm_gen_be_error_t::unsupported_expr);
        }

        // 4. Decimal float / complex / logical / cast edge cases (`L987`, `L1303`, `L1324`, `L1421`..`L1540`, `L6162`..`L6179`)
        {
          // Decimal float `eok_remainder` (unsupported binary op at L987!) and unary op on decimal float in `llvm_lower_logical_expression` (`v2 == nullptr` at L1303!)
          // and `llvm_lower_logical_expression` returning `i1` (`bool_i1_e` at L1324!) and `f32` (`!isIntegerTy()` at L1324!)
          a_type dec32_e = {}; dec32_e.kind = tk_typeref; dec32_e.source_corresp.name = (char*)"_Decimal32"; dec32_e.variant.typeref.type = &s32_e;
          a_variable vd32 = {}; vd32.type = &dec32_e; state.local_vars[&vd32] = state.builder->CreateAlloca(i32_t);
          an_expr_node ed1 = {}, ed2 = {};
          ed1.kind = enk_variable; ed1.type = &dec32_e; ed1.variant.variable.ptr = &vd32; ed1.next = &ed2;
          ed2.kind = enk_variable; ed2.type = &dec32_e; ed2.variant.variable.ptr = &vd32;

          an_expr_node d_rem = {}; d_rem.kind = enk_operation; d_rem.type = &dec32_e; d_rem.variant.operation.kind = eok_remainder; d_rem.variant.operation.operands = &ed1;
          assert(llvm_lower_arithmetic_expression(&d_rem, &val) == llvm_gen_be_error_t::ok);

          ed1.next = nullptr;
          an_expr_node d_not = {}; d_not.kind = enk_operation; d_not.type = &bool_i1_e; d_not.variant.operation.kind = eok_not; d_not.variant.operation.operands = &ed1;
          assert(llvm_lower_logical_expression(&d_not, &val) == llvm_gen_be_error_t::ok); // v2 == nullptr & expr_llvm_ty == i1!

          // `_Complex int` equality with `bool_i1_e` (`getIntegerBitWidth() == 1` at L1324!)
          a_type cplx_i32_cmp = {}; cplx_i32_cmp.kind = tk_complex; cplx_i32_cmp.size = 8;
          state.type_cache[&cplx_i32_cmp] = llvm::StructType::get(*state.context, {i32_t, i32_t});
          an_expr_node cc1 = {}, cc2 = {}, cc_eq = {};
          cc1.kind = enk_constant; cc1.type = &cplx_i32_cmp; cc1.next = &cc2;
          cc2.kind = enk_constant; cc2.type = &cplx_i32_cmp;
          cc_eq.kind = enk_operation; cc_eq.type = &bool_i1_e; cc_eq.variant.operation.kind = eok_eq; cc_eq.variant.operation.operands = &cc1;
          assert(llvm_lower_logical_expression(&cc_eq, &val) == llvm_gen_be_error_t::ok);
          cc_eq.variant.operation.kind = eok_ne;
          assert(llvm_lower_logical_expression(&cc_eq, &val) == llvm_gen_be_error_t::ok);

          // `llvm_lower_decimal_float_cast`: `src_dec_bytes == 0 && dst_dec_bytes == 0` (L6162!) and `dst_dec_bytes < src_dec_bytes` (`trunc` at L6179!)
          assert(llvm_lower_decimal_float_cast(0, 0, false, llvm::ConstantInt::get(i32_t, 1), i32_t, &val) == llvm_gen_be_error_t::invalid_argument);
          assert(llvm_lower_decimal_float_cast(8, 4, false, llvm::ConstantInt::get(i64_t, 1), i32_t, &val) == llvm_gen_be_error_t::ok);

          // Cast with `raw_dst_edg == nullptr` (`tref_null_e`), `int_edg == nullptr` (untyped -> `_Decimal32`), `bool` from untyped `i1`, and `_Complex float -> _Complex float` (`!src_elem_fp` & `dst_elem_fp` false at L1540!)
          an_expr_node untyped_i32 = {}; untyped_i32.kind = enk_type_operand; untyped_i32.type = nullptr;
          an_expr_node ce = {}; ce.kind = enk_operation; ce.variant.operation.kind = eok_cast;
          ce.variant.operation.operands = &untyped_i32; ce.type = &dec32_e;
          assert(llvm_lower_cast_expression(&ce, &val) == llvm_gen_be_error_t::ok); // int_edg == nullptr!
          an_expr_node c_tref_null = {}; c_tref_null.kind = enk_constant; c_tref_null.type = &s32_e;
          ce.variant.operation.operands = &c_tref_null; ce.type = &tref_null_e;
          assert(llvm_lower_cast_expression(&ce, &val) == llvm_gen_be_error_t::ok); // raw_dst_edg == nullptr at L1428 & L1447!

          a_type nullp_e = {}; nullp_e.kind = tk_nullptr; nullp_e.size = 8;
          an_expr_node np_src = {}; np_src.kind = enk_constant; np_src.type = &nullp_e;
          ce.variant.operation.operands = &np_src; ce.type = &tref_null_e;
          assert(llvm_lower_cast_expression(&ce, &val) == llvm_gen_be_error_t::ok); // raw_dst_edg == nullptr at L1433!

          an_expr_node c_untyped_b = {}; c_untyped_b.kind = enk_type_operand; c_untyped_b.type = nullptr;
          ce.variant.operation.operands = &c_untyped_b; ce.type = &bool_e;
          assert(llvm_lower_cast_expression(&ce, &val) == llvm_gen_be_error_t::ok); // raw_src_edg == nullptr at L1448!

          a_type cplx_f32_e = {}; cplx_f32_e.kind = tk_complex; cplx_f32_e.size = 8;
          state.type_cache[&cplx_f32_e] = llvm::StructType::get(*state.context, {f32_t, f32_t});
          a_type cplx_f64_e = {}; cplx_f64_e.kind = tk_complex; cplx_f64_e.size = 16;
          state.type_cache[&cplx_f64_e] = llvm::StructType::get(*state.context, {f64_t, f64_t});
          an_expr_node cf32_src = {}; cf32_src.kind = enk_constant; cf32_src.type = &cplx_f32_e;
          ce.variant.operation.operands = &cf32_src; ce.type = &cplx_f64_e;
          run_expr_oom([&]() noexcept { return llvm_lower_cast_expression(&ce, &val); }); // src_elem_fp && dst_elem_fp!
        }

        // 5. Source location edge cases (`name_has_been_mangled = TRUE`, null/empty routine name, detached/parentless BB, demangled without `(`),
        //    `coerce_builtin_int_return` (`val` non-int, `ret_ty` non-int), `resolve_atomic_pointee_llvm_type`, and call ABI coercion (`L2028-2066`)!
        {
          a_routine mr = {}; mr.source_corresp.name = (char*)"_Z3foo"; mr.source_corresp.name_has_been_mangled = TRUE;
          state.current_edg_routine = &mr;
          assert(call_test_builtin("__builtin_FUNCTION", &ptr_void_e, {}) == llvm_gen_be_error_t::ok);
          mr.source_corresp.name_has_been_mangled = FALSE; mr.source_corresp.name = nullptr;
          assert(call_test_builtin("__builtin_FUNCTION", &ptr_void_e, {}) == llvm_gen_be_error_t::ok);
          mr.source_corresp.name = (char*)"";
          assert(call_test_builtin("__builtin_FUNCTION", &ptr_void_e, {}) == llvm_gen_be_error_t::ok);
          state.current_edg_routine = nullptr;

          // `_Z3bar` demangles to `"bar"` (demangled != raw_fn AND `paren_pos == npos` at L1754!)
          llvm::Function* bar_fn = llvm::Function::Create(
              llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false),
              llvm::Function::ExternalLinkage, "_Z3bar", state.module.get());
          state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "entry", bar_fn));
          assert(call_test_builtin("__builtin_FUNCTION", &ptr_void_e, {}) == llvm_gen_be_error_t::ok);
          { llvm::IRBuilder<> bb_b(&bar_fn->back()); bb_b.CreateRetVoid(); }

          // Parentless BasicBlock (`GetInsertBlock() != nullptr && getParent() == nullptr` at L1747!)
          std::unique_ptr<llvm::BasicBlock> orphan_bb(llvm::BasicBlock::Create(*state.context, "orphan"));
          state.builder->SetInsertPoint(orphan_bb.get());
          assert(call_test_builtin("__builtin_LINE", &s32_e, {}) == llvm_gen_be_error_t::ok);
          // Detached builder (`GetInsertBlock() == nullptr` at L1746!)
          state.builder->ClearInsertionPoint();
          assert(call_test_builtin("__builtin_COLUMN", &s32_e, {}) == llvm_gen_be_error_t::ok);
          state.builder->SetInsertPoint(&ws10_fn->back());
          ensure_expr_bb();

          // `coerce_builtin_int_return` with `ret_ty` non-integer (`f32_e` at L1783!) & `__builtin_constant_p` with `f32_e` (`L1874`) & `__builtin_bswap` unknown suffix (`L1882`)!
          an_expr_node ei32 = {}; ei32.kind = enk_constant; ei32.type = &s32_e;
          an_expr_node ef32 = {}; ef32.kind = enk_constant; ef32.type = &f32_e;
          llvm::VectorType* vec4_bswap_t = llvm::FixedVectorType::get(i32_t, 4);
          a_type vec4_bswap_e = {}; vec4_bswap_e.kind = tk_array; vec4_bswap_e.size = 16;
          state.type_cache[&vec4_bswap_e] = vec4_bswap_t;
          an_expr_node e_vec_bswap = {}; e_vec_bswap.kind = enk_constant; e_vec_bswap.type = &vec4_bswap_e;
          assert(call_test_builtin("__builtin_clz", &f32_e, {&ei32}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__builtin_constant_p", &f32_e, {&ei32}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__builtin_bswap128", &s32_e, {&ei32}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__builtin_bswap", &vec4_bswap_e, {&e_vec_bswap}) == llvm_gen_be_error_t::ok);

          // Call ABI argument coercion (`c_op0.type = &fn_abi_edg`!):
          // Cover `is_class_struct_union_type` (12B struct -> `{i64, i64}` where `packed_bytes > total_arg_bytes` AND 24B struct -> `byval` `ptr`!),
          // `tk_complex`, `tk_array`, `_BitInt(256)` (`size > 16`), untyped arg (`arg->type == nullptr`),
          // narrow `s16`, `u16`, `bool_i1`, and `f32 -> f64`, plus `is_vararg_rout == true`!
          a_type st12_e = {}; st12_e.kind = tk_struct; st12_e.size = 12; st12_e.alignment = 77;
          llvm::StructType* st12_ll = llvm::StructType::get(*state.context, {i32_t, i32_t, i32_t});
          state.type_cache[&st12_e] = st12_ll;
          a_type st24_e = {}; st24_e.kind = tk_struct; st24_e.size = 24; st24_e.alignment = 77;
          llvm::StructType* st24_ll = llvm::StructType::get(*state.context, {i64_t, i64_t, i64_t});
          state.type_cache[&st24_e] = st24_ll;
          a_type cplx_f32_e = {}; cplx_f32_e.kind = tk_complex; cplx_f32_e.size = 8;
          state.type_cache[&cplx_f32_e] = llvm::StructType::get(*state.context, {f32_t, f32_t});
          a_type arr2_e = {}; arr2_e.kind = tk_array; arr2_e.size = 8; arr2_e.variant.array.element_type = &s32_e; arr2_e.variant.array.variant.number_of_elements = 2;

          llvm::FunctionType* rich_fty = llvm::FunctionType::get(
              i32_t, {i64_t, i64_t, ptr_t, i64_t, i64_t, ptr_t, i32_t, i32_t, i32_t, i8_t, f64_t}, false);
          llvm::Function::Create(rich_fty, llvm::Function::ExternalLinkage, "fn_rich_coercion", state.module.get());

          a_routine_type_supplement rts_rich = {};
          a_type fn_rich_edg = {}; fn_rich_edg.kind = tk_routine; fn_rich_edg.variant.routine.return_type = &s32_e; fn_rich_edg.variant.routine.extra_info = &rts_rich;
          a_routine r_rich = {}; r_rich.source_corresp.name = (char*)"fn_rich_coercion"; r_rich.type = &fn_rich_edg;

          a_variable v12 = {}, v24 = {}, vc = {}, va = {}, v256 = {}, vs16 = {}, vu16 = {}, vb1 = {}, vf = {};
          v12.type = &st12_e; v24.type = &st24_e; vc.type = &cplx_f32_e; va.type = &arr2_e; v256.type = &s256_e;
          vs16.type = &s16_e; vu16.type = &u16_e; vb1.type = &bool_i1_e; vf.type = &f32_e;
          state.local_vars[&v12] = state.builder->CreateAlloca(st12_ll);
          state.local_vars[&v24] = state.builder->CreateAlloca(st24_ll);
          state.local_vars[&vc] = state.builder->CreateAlloca(state.type_cache[&cplx_f32_e]);
          state.local_vars[&va] = state.builder->CreateAlloca(llvm::ArrayType::get(i32_t, 2));
          state.local_vars[&v256] = state.builder->CreateAlloca(llvm::IntegerType::get(*state.context, 256));
          state.local_vars[&vs16] = state.builder->CreateAlloca(i16_t);
          state.local_vars[&vu16] = state.builder->CreateAlloca(i16_t);
          state.local_vars[&vb1] = state.builder->CreateAlloca(i1_t);
          state.local_vars[&vf] = state.builder->CreateAlloca(f32_t);

          an_expr_node op0 = {}, a1 = {}, a2 = {}, a3 = {}, a4 = {}, a5 = {}, a6 = {}, a7 = {}, a8 = {}, a9 = {}, a10 = {};
          op0.kind = enk_routine; op0.type = &fn_rich_edg; op0.variant.routine.ptr = &r_rich; op0.next = &a1;
          a1.kind = enk_constant; a1.type = &st12_e; a1.next = &a2;
          a2.kind = enk_constant; a2.type = &st24_e; a2.next = &a3;
          a3.kind = enk_constant; a3.type = &cplx_f32_e; a3.next = &a4;
          a4.kind = enk_constant; a4.type = &arr2_e; a4.next = &a5;
          a5.kind = enk_constant; a5.type = &s256_e; a5.next = &a6;
          a6.kind = enk_type_operand; a6.type = nullptr; a6.next = &a7; // untyped i32 -> i32!
          a7.kind = enk_constant; a7.type = &s16_e; a7.next = &a8;
          a8.kind = enk_constant; a8.type = &u16_e; a8.next = &a9;
          a9.kind = enk_constant; a9.type = &bool_i1_e; a9.next = &a10;
          a10.kind = enk_constant; a10.type = &f32_e; a10.next = nullptr;

          an_expr_node call_rich = {}; call_rich.kind = enk_operation; call_rich.type = &s32_e;
          call_rich.variant.operation.kind = eok_call; call_rich.variant.operation.operands = &op0;
          run_expr_oom([&]() noexcept { return llvm_lower_call_expression(&call_rich, &val); });

          // Also test when `call_param_tys` runs out mid-aggregate (`callee_arg_idx < call_param_tys.size()` false at L2066)
          // and `is_vararg_rout == true` at L1984!
          llvm::FunctionType* short_fty = llvm::FunctionType::get(i32_t, {i64_t}, false);
          llvm::Function::Create(short_fty, llvm::Function::ExternalLinkage, "fn_short_coercion", state.module.get());
          r_rich.source_corresp.name = (char*)"fn_short_coercion";
          a1.next = nullptr;
          assert(llvm_lower_call_expression(&call_rich, &val) == llvm_gen_be_error_t::ok);
          rts_rich.has_ellipsis = TRUE;
          assert(llvm_lower_call_expression(&call_rich, &val) == llvm_gen_be_error_t::ok);

          // Indirect call with unnamed routine (`op1->kind != enk_routine` & `routine.ptr == nullptr` & `name == nullptr` at L1952/1980)!
          a_routine r_unnamed = {}; r_unnamed.source_corresp.name = nullptr; r_unnamed.type = &fn_rich_edg;
          op0.variant.routine.ptr = &r_unnamed;
          // Indirect function pointer variable:
          a_type fn_ptr_edg = {}; fn_ptr_edg.kind = tk_pointer; fn_ptr_edg.size = 8; fn_ptr_edg.variant.pointer.type = &fn_rich_edg;
          a_variable vfp = {}; vfp.type = &fn_ptr_edg; state.local_vars[&vfp] = state.builder->CreateAlloca(ptr_t);
          an_expr_node op_fp = {}; op_fp.kind = enk_variable; op_fp.type = &fn_ptr_edg; op_fp.variant.variable.ptr = &vfp;
          call_rich.variant.operation.operands = &op_fp;
          assert(llvm_lower_call_expression(&call_rich, &val) == llvm_gen_be_error_t::ok);
        }

        // 6. Atomics & Builtins remaining branches (`__builtin_assume` i1, untyped overflow/signbit/always_lock_free,
        //    empty-arg coro/bit_cast/chk, `eok_cast` float `fetch_add`, `int**` / `char**` `__c11_atomic_fetch_add`/`sub_and_fetch`,
        //    `resolve_atomic_pointee_llvm_type` null/untyped/void* fallbacks, and OOM sweeps on atomics!)
        {
          a_variable vb1 = {}, vp = {}, vpp4 = {}, vpp1 = {}, vppv = {}, vf = {}, vd = {}, vi64 = {};
          vb1.type = &bool_i1_e; vp.type = &ptr_void_e; vpp4.type = &ptr_ptr_s32_e; vpp1.type = &ptr_ptr_s8_e; vppv.type = &ptr_ptr_void_e;
          vf.type = &f32_e; vd.type = &f64_e; vi64.type = &s64_e;
          state.local_vars[&vb1] = state.builder->CreateAlloca(i1_t);
          state.local_vars[&vp] = state.builder->CreateAlloca(ptr_t);
          state.local_vars[&vpp4] = state.builder->CreateAlloca(ptr_t);
          state.local_vars[&vpp1] = state.builder->CreateAlloca(ptr_t);
          state.local_vars[&vppv] = state.builder->CreateAlloca(ptr_t);
          state.local_vars[&vf] = state.builder->CreateAlloca(f32_t);
          state.local_vars[&vd] = state.builder->CreateAlloca(f64_t);
          state.local_vars[&vi64] = state.builder->CreateAlloca(i64_t);

          an_expr_node eb1 = {}, epv = {}, epp4 = {}, epp1 = {}, eppv = {}, ef = {}, ed = {}, ei64 = {}, ei32 = {};
          eb1.kind = enk_constant; eb1.type = &bool_i1_e;
          epv.kind = enk_variable; epv.type = &ptr_void_e; epv.variant.variable.ptr = &vp;
          epp4.kind = enk_variable; epp4.type = &ptr_ptr_s32_e; epp4.variant.variable.ptr = &vpp4;
          epp1.kind = enk_variable; epp1.type = &ptr_ptr_s8_e; epp1.variant.variable.ptr = &vpp1;
          eppv.kind = enk_variable; eppv.type = &ptr_ptr_void_e; eppv.variant.variable.ptr = &vppv;
          ef.kind = enk_constant; ef.type = &f32_e;
          ed.kind = enk_constant; ed.type = &f64_e;
          ei64.kind = enk_variable; ei64.type = &s64_e; ei64.variant.variable.ptr = &vi64;
          ei32.kind = enk_constant; ei32.type = &s32_e;

          assert(call_test_builtin("__builtin_assume", &void_e, {&eb1}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__builtin_add_overflow", nullptr, {&ei32, &ei32, &epv}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__builtin_signbit", nullptr, {&ef}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__builtin_always_lock_free", nullptr, {&ei64, &epv}) == llvm_gen_be_error_t::ok);
          // Too-few-args fallthrough on coro/bit_cast/chk builtins (declares external fallback cleanly)
          assert(call_test_builtin("__builtin_coro_resume", &void_e, {}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__builtin_coro_destroy", &void_e, {}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__builtin_coro_done", &s32_e, {}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__builtin_coro_promise", &ptr_void_e, {&epv}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__builtin_bit_cast", &s32_e, {}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__builtin___sprintf_chk", &s32_e, {&epv}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__builtin___fprintf_chk", &s32_e, {&epv}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__builtin___printf_chk", &s32_e, {&ei32}) == llvm_gen_be_error_t::ok);

          // Float `fetch_add` where `args[1]` was lowered as `i64` via `eok_cast(&ef)` / `eok_cast(&ed)` / `eok_cast(&ei32)` / non-cast `ei64`!
          an_expr_node cast_i64_from_f = {}, cast_i64_from_d = {}, cast_i64_from_i = {}, cast_i64_noop = {};
          cast_i64_from_f.kind = enk_operation; cast_i64_from_f.type = &s64_e; cast_i64_from_f.variant.operation.kind = eok_cast; cast_i64_from_f.variant.operation.operands = &ef;
          cast_i64_from_d.kind = enk_operation; cast_i64_from_d.type = &s64_e; cast_i64_from_d.variant.operation.kind = eok_cast; cast_i64_from_d.variant.operation.operands = &ed;
          cast_i64_from_i.kind = enk_operation; cast_i64_from_i.type = &s64_e; cast_i64_from_i.variant.operation.kind = eok_cast; cast_i64_from_i.variant.operation.operands = &ei32;
          cast_i64_noop.kind = enk_operation; cast_i64_noop.type = &s64_e; cast_i64_noop.variant.operation.kind = eok_parens; cast_i64_noop.variant.operation.operands = &ei64;
          assert(call_test_builtin("__atomic_fetch_add", &f32_e, {&epv, &cast_i64_from_f, &ei32}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__atomic_fetch_add", &f32_e, {&epv, &cast_i64_from_d, &ei32}) == llvm_gen_be_error_t::ok); // FPCast double -> float!
          assert(call_test_builtin("__atomic_fetch_add", &f32_e, {&epv, &cast_i64_from_i, &ei32}) == llvm_gen_be_error_t::ok); // inner is int!
          assert(call_test_builtin("__atomic_fetch_add", &f32_e, {&epv, &cast_i64_noop, &ei32}) == llvm_gen_be_error_t::ok); // SIToFP fallback!

          // Pointer `fetch_add` / `fetch_sub` / `__sync_add_and_fetch` / `__sync_sub_and_fetch` / `__c11_atomic_fetch_add` on `int**` (4B), `char**` (1B), `void**` (0B), untyped return, and pointer delta!
          assert(call_test_builtin("__c11_atomic_fetch_add", &ptr_s32_e, {&epp4, &ei64, &ei32}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__c11_atomic_fetch_sub", &ptr_s8_e, {&epp1, &ei64, &ei32}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__c11_atomic_fetch_add", nullptr, {&eppv, &epv, &ei32}) == llvm_gen_be_error_t::ok); // untyped expr & pointer args[1]!
          assert(call_test_builtin("__sync_add_and_fetch", &ptr_s32_e, {&epp4, &ei64}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__sync_sub_and_fetch", &ptr_s32_e, {&epp4, &ei64}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__atomic_fetch_xor", nullptr, {&epp4, &ei64, &ei32}) == llvm_gen_be_error_t::ok); // target_is_ptr true, add/sub false!

          // `resolve_atomic_pointee_llvm_type` remaining branches: `epv` (`void*` -> L1842 `i8_ty` fallback!), untyped `eok_cast`, non-ptr `eok_cast`, null-ptee `eok_cast`, null `eok_address_of`, and OOM sweeps on atomics!
          an_expr_node cast_untyped = {}, cast_nonptr = {}, cast_nullpt = {}, addr_untyped = {}, untyped_var = {};
          untyped_var.kind = enk_variable; untyped_var.is_lvalue = TRUE; untyped_var.type = nullptr; untyped_var.variant.variable.ptr = &vp;
          cast_untyped.kind = enk_operation; cast_untyped.type = nullptr; cast_untyped.variant.operation.kind = eok_cast; cast_untyped.variant.operation.operands = &epv;
          cast_nonptr.kind = enk_operation; cast_nonptr.type = &s64_e; cast_nonptr.variant.operation.kind = eok_cast; cast_nonptr.variant.operation.operands = &epv;
          cast_nullpt.kind = enk_operation; cast_nullpt.type = &ptr_null_e; cast_nullpt.variant.operation.kind = eok_cast; cast_nullpt.variant.operation.operands = &epv;
          addr_untyped.kind = enk_operation; addr_untyped.type = nullptr; addr_untyped.variant.operation.kind = eok_address_of; addr_untyped.variant.operation.operands = &untyped_var;

          assert(call_test_builtin("__atomic_load", &void_e, {&epv, &epv, &ei32}) == llvm_gen_be_error_t::ok); // L1842 void* -> i8_ty!
          assert(call_test_builtin("__atomic_load", &void_e, {&epv, &cast_untyped, &ei32}) == llvm_gen_be_error_t::atomic_lowering_failed);
          assert(call_test_builtin("__atomic_load", &void_e, {&epv, &cast_nonptr, &ei32}) == llvm_gen_be_error_t::atomic_lowering_failed);
          assert(call_test_builtin("__atomic_load", &void_e, {&epv, &cast_nullpt, &ei32}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__atomic_load", &void_e, {&epv, &addr_untyped, &ei32}) == llvm_gen_be_error_t::atomic_lowering_failed);
          // 1-arg `__atomic_load` / `__atomic_store` / `__atomic_exchange` (`ret_ptr_node == nullptr` / `val_ptr_node == nullptr`)!
          assert(call_test_builtin("__atomic_load", &void_e, {&epv}) == llvm_gen_be_error_t::atomic_lowering_failed);
          assert(call_test_builtin("__atomic_load", &void_e, {&ei64, &epv}) == llvm_gen_be_error_t::atomic_lowering_failed);
          assert(call_test_builtin("__atomic_store", &void_e, {&epv}) == llvm_gen_be_error_t::atomic_lowering_failed);
          assert(call_test_builtin("__atomic_store", &void_e, {&ei64, &epv}) == llvm_gen_be_error_t::atomic_lowering_failed);
          assert(call_test_builtin("__atomic_exchange", &void_e, {&epv}) == llvm_gen_be_error_t::atomic_lowering_failed);
          assert(call_test_builtin("__atomic_exchange", &void_e, {&ei64, &epv}) == llvm_gen_be_error_t::atomic_lowering_failed);
          assert(call_test_builtin("__atomic_exchange", &void_e, {&ei64, &epv, &epv, &epv, &ei32}) == llvm_gen_be_error_t::ok);
          // 4-arg `__atomic_compare_exchange` (both `args.size() < 6` and `expr->type == nullptr` and `has_sz_arg` with short op list)!
          assert(call_test_builtin("__atomic_compare_exchange", nullptr, {&epv, &epv, &epv, &ei32}) == llvm_gen_be_error_t::ok);
          run_expr_oom([&]() noexcept { return call_test_builtin("__atomic_load", &void_e, {&epv, &epv, &ei32}); });
          run_expr_oom([&]() noexcept { return call_test_builtin("__atomic_store", &void_e, {&epv, &epv, &ei32}); });
          run_expr_oom([&]() noexcept { return call_test_builtin("__atomic_exchange", &void_e, {&epv, &epv, &epv, &ei32}); });
          run_expr_oom([&]() noexcept { return call_test_builtin("__atomic_compare_exchange", &bool_e, {&epv, &epv, &epv, &ei32, &ei32, &ei32}); });
          run_expr_oom([&]() noexcept { return call_test_builtin("__atomic_thread_fence", &void_e, {&ei64}); });
        }

        // 7. Type traits remaining branches (`as_protected`, non-virtual private base, aggregate with user ctor & public fields/bases,
        //    compiler-generated & trivial-default ctors in `check_trivial_elem`, union & virtual-fn/vbase base in `check_standard_layout_elem`,
        //    `C_dialect_pcc` on `void` and `ref` in `bok_is_pod`, and `t2 == void` in `bok_is_assignable`)!
        {
          auto eval_t = [&](a_builtin_operation_kind k, a_type_ptr t1, a_type_ptr t2 = nullptr) noexcept -> llvm_gen_be_error_t {
            an_expr_node o1 = {}, o2 = {}, be = {};
            o1.kind = enk_type_operand; o1.variant.type_operand.type = t1;
            if (t2) { o2.kind = enk_type_operand; o2.variant.type_operand.type = t2; o1.next = &o2; }
            be.kind = enk_builtin_operation; be.type = &s32_e; be.variant.builtin_operation.kind = k;
            be.variant.builtin_operation.operands = &o1;
            return llvm_lower_builtin_operation(&be, &val);
          };

          a_field f_prot = {}; f_prot.type = &s32_e; f_prot.source_corresp.access = as_protected;
          a_type cls_prot = {}; cls_prot.kind = tk_struct; cls_prot.size = 4; cls_prot.variant.class_struct_union.field_list = &f_prot;
          assert(eval_t(bok_is_aggregate, &cls_prot) == llvm_gen_be_error_t::ok); // as_protected TRUE!

          a_base_class_derivation d_priv = {}; d_priv.access = as_private;
          a_type base_empty = {}; base_empty.kind = tk_struct; base_empty.size = 1;
          a_base_class bc_priv_nonvirt = {}; bc_priv_nonvirt.type = &base_empty; bc_priv_nonvirt.is_virtual = FALSE; bc_priv_nonvirt.derivation = &d_priv;
          a_class_type_supplement ext_priv_b = {}; ext_priv_b.direct_base_classes = &bc_priv_nonvirt;
          a_type cls_priv_b = {}; cls_priv_b.kind = tk_struct; cls_priv_b.size = 4; cls_priv_b.variant.class_struct_union.extra_info = &ext_priv_b;
          assert(eval_t(bok_is_aggregate, &cls_priv_b) == llvm_gen_be_error_t::ok); // non-virtual private base!

          a_routine cg_ctor = {}, tr_ctor = {}, ud_ctor = {}, non_ctor = {};
          non_ctor.special_kind = sfk_none; non_ctor.next = &cg_ctor;
          cg_ctor.special_kind = sfk_constructor; cg_ctor.compiler_generated = TRUE; cg_ctor.next = &tr_ctor;
          tr_ctor.special_kind = sfk_constructor; tr_ctor.compiler_generated = FALSE; tr_ctor.is_trivial_default_constructor = TRUE; tr_ctor.next = &ud_ctor;
          ud_ctor.special_kind = sfk_constructor; ud_ctor.compiler_generated = FALSE; ud_ctor.is_trivial_default_constructor = FALSE; ud_ctor.next = nullptr;
          a_scope sc_ctors = {}; sc_ctors.routines = &non_ctor;
          a_class_type_supplement ext_ctors = {}; ext_ctors.assoc_scope = &sc_ctors;
          a_type cls_ctors = {}; cls_ctors.kind = tk_struct; cls_ctors.size = 4; cls_ctors.variant.class_struct_union.extra_info = &ext_ctors;
          assert(eval_t(bok_is_aggregate, &cls_ctors) == llvm_gen_be_error_t::ok); // enters L3539 assoc_scope loop!
          ud_ctor.is_trivial_default_constructor = TRUE; // now all ctors are trivial so loop finishes at nullptr!
          tr_ctor.next = nullptr;
          assert(eval_t(bok_is_trivial, &cls_ctors) == llvm_gen_be_error_t::ok);

          // Union, class with `any_virtual_functions`, base of `tk_union` / `nullptr`, base with `any_virtual_functions` / `any_virtual_base_classes` in `bok_is_standard_layout` & `bok_is_pod`!
          a_type uni_t = {}; uni_t.kind = tk_union; uni_t.size = 4;
          assert(eval_t(bok_is_standard_layout, &uni_t) == llvm_gen_be_error_t::ok);
          assert(eval_t(bok_is_standard_layout, &ref_e) == llvm_gen_be_error_t::ok); // L3619 reference!
          assert(eval_t(bok_is_pod, &uni_t) == llvm_gen_be_error_t::ok);
          assert(eval_t(bok_is_pod, &ref_e) == llvm_gen_be_error_t::ok);
          C_dialect = C_dialect_pcc;
          assert(eval_t(bok_is_pod, &void_e) == llvm_gen_be_error_t::ok);
          C_dialect = C_dialect_cplusplus;

          a_type cls_vfn = {}; cls_vfn.kind = tk_class; cls_vfn.size = 8;
          cls_vfn.variant.class_struct_union.any_virtual_functions_including_in_base_classes = TRUE;
          assert(eval_t(bok_is_standard_layout, &cls_vfn) == llvm_gen_be_error_t::ok);
          assert(eval_t(bok_is_aggregate, &cls_vfn) == llvm_gen_be_error_t::ok);
          assert(eval_t(bok_is_trivial, &cls_vfn) == llvm_gen_be_error_t::ok);

          a_type base_vfn = {}, base_vb = {};
          base_vfn.kind = tk_class; base_vfn.size = 8; base_vfn.variant.class_struct_union.any_virtual_functions = TRUE;
          base_vb.kind = tk_class; base_vb.size = 8; base_vb.variant.class_struct_union.any_virtual_base_classes = TRUE;
          a_base_class bc_null_sl = {}, bc_uni_sl = {}, bc_vfn_sl = {}, bc_vb_sl = {};
          bc_null_sl.type = nullptr; bc_null_sl.next = &bc_uni_sl;
          bc_uni_sl.type = &uni_t; bc_uni_sl.next = &bc_vfn_sl;
          bc_vfn_sl.type = &base_vfn; bc_vfn_sl.next = nullptr;
          a_class_type_supplement ext_sl1 = {}, ext_sl2 = {};
          ext_sl1.base_classes = &bc_null_sl;
          ext_sl2.base_classes = &bc_vb_sl; bc_vb_sl.type = &base_vb;
          a_type cls_sl1 = {}, cls_sl2 = {};
          cls_sl1.kind = tk_struct; cls_sl1.size = 8; cls_sl1.variant.class_struct_union.extra_info = &ext_sl1;
          cls_sl2.kind = tk_struct; cls_sl2.size = 8; cls_sl2.variant.class_struct_union.extra_info = &ext_sl2;
          assert(eval_t(bok_is_standard_layout, &cls_sl1) == llvm_gen_be_error_t::ok);
          assert(eval_t(bok_is_standard_layout, &cls_sl2) == llvm_gen_be_error_t::ok);
          a_field f_untyped_sl = {}; f_untyped_sl.type = nullptr; f_untyped_sl.source_corresp.access = as_public;
          cls_sl2.variant.class_struct_union.extra_info = nullptr;
          cls_sl2.variant.class_struct_union.field_list = &f_untyped_sl;
          assert(eval_t(bok_is_standard_layout, &cls_sl2) == llvm_gen_be_error_t::ok);

          assert(eval_t(bok_is_assignable, &s32_e, &void_e) == llvm_gen_be_error_t::ok); // t2 == void!
          assert(eval_t(bok_is_constructible, &s32_e, nullptr) == llvm_gen_be_error_t::ok); // t2 == nullptr!
        }

        // 8. `llvm_lower_expression` & `llvm_lower_compound_assignment` remaining branches:
        //    - `eok_lvalue_adjust`, `eok_lvalue_cast`, `eok_parens`, `eok_ref_indirect` rvalue load
        //    - `eok_assign` rvalue with `ref_e` (`is_reference` TRUE at L4267!) and `expr->type == nullptr`
        //    - pointer - pointer (`v1->getType()->isPointerTy()` at L4289!)
        //    - ternary `?:` with pointer `true_val` (`L4509`) and integer width mismatch on `true_val` (`i16` vs `i32` at `L4512`) and `false_val` (`i16` vs `i32` at `L4517`)!
        //    - `enk_initializer` (`dik_constant` and `dik_class_result_via_ctor` / `dik_constructor`) with `active_prvalue_dest_ptr` (`L4732`, `L4758`, `L4777`)!
        //    - `enk_throw` with `throw_info->expr` & `throw_info->destructor` (`L4839-4850`)!
        //    - `leck_internal_try` with failing `first_e` (`L4989`), `second_e == nullptr` (`L5007`), and terminated `first_e` (`L4931`)!
        //    - `enk_result_of_overriding_function` with `current_function_scope == nullptr`, extra `cur_fn` args, unmapped param, and `void` `under_fn` (`L5040-5054`)!
        //    - `llvm_lower_compound_assignment` on `_Complex int` `+=` / `-=` with float & int scalar RHS (`L5208-5267`), and `char*` / untyped pointer `+=` (`L5137-5140`)!
        {
          state.type_cache.clear();
          state.type_cache[&s256_e] = llvm::IntegerType::get(*state.context, 256);
          state.type_cache[&bool_i1_e] = i1_t;

          a_variable vs32 = {}, vs16 = {}, vp1 = {}, vp8 = {};
          vs32.type = &s32_e; vs16.type = &s16_e; vp1.type = &ptr_s32_e; vp8.type = &ptr_s8_e;
          state.local_vars[&vs32] = state.builder->CreateAlloca(i32_t);
          state.local_vars[&vs16] = state.builder->CreateAlloca(i16_t);
          state.local_vars[&vp1] = state.builder->CreateAlloca(ptr_t);
          state.local_vars[&vp8] = state.builder->CreateAlloca(ptr_t);

          an_expr_node lv32 = {}; lv32.kind = enk_variable; lv32.type = &s32_e; lv32.is_lvalue = TRUE; lv32.variant.variable.ptr = &vs32;
          for (an_expr_operator_kind lk : {eok_parens, eok_lvalue_adjust, eok_lvalue_cast}) {
            an_expr_node lve = {}; lve.kind = enk_operation; lve.type = &s32_e; lve.is_lvalue = FALSE;
            lve.variant.operation.kind = lk; lve.variant.operation.operands = &lv32;
            assert(llvm_lower_expression(&lve, &val) == llvm_gen_be_error_t::ok);
          }

          // `eok_lvalue_adjust` with `ref_e` & `nullptr` expr->type (L4264-4267!)
          an_expr_node rhs32 = {}; rhs32.kind = enk_constant; rhs32.type = &s32_e;
          an_expr_node lv_adj_ref = {}; lv_adj_ref.kind = enk_operation; lv_adj_ref.type = &ref_e; lv_adj_ref.is_lvalue = FALSE;
          lv_adj_ref.variant.operation.kind = eok_lvalue_adjust; lv_adj_ref.variant.operation.operands = &lv32;
          assert(llvm_lower_expression(&lv_adj_ref, &val) == llvm_gen_be_error_t::ok);
          lv_adj_ref.type = nullptr;
          assert(llvm_lower_expression(&lv_adj_ref, &val) == llvm_gen_be_error_t::ok);

          // `p1 - p2` in `llvm_lower_expression` (`eok_subtract` with `PointerTy` at L4289!)
          an_expr_node ep1 = {}, ep2 = {}, psub_e = {};
          ep1.kind = enk_variable; ep1.type = &ptr_s32_e; ep1.variant.variable.ptr = &vp1; ep1.next = &ep2;
          ep2.kind = enk_variable; ep2.type = &ptr_s32_e; ep2.variant.variable.ptr = &vp1;
          psub_e.kind = enk_operation; psub_e.type = &s64_e; psub_e.variant.operation.kind = eok_subtract; psub_e.variant.operation.operands = &ep1;
          assert(llvm_lower_expression(&psub_e, &val) == llvm_gen_be_error_t::ok);

          // Ternary `?:` (`eok_question`) with pointer result (`L4509`) and `i16` vs `i32` integer width mismatch (`L4512` & `L4517`)!
          an_expr_node q_cond = {}, q_p1 = {}, q_p2 = {}, q_s16_t = {}, q_s16_f = {}, q_e = {};
          q_cond.kind = enk_constant; q_cond.type = &s32_e;
          q_p1 = ep1; q_p2 = ep2; q_p1.next = &q_p2; q_p2.next = nullptr;
          q_cond.next = &q_p1;
          q_e.kind = enk_operation; q_e.type = &ptr_s32_e; q_e.variant.operation.kind = eok_question; q_e.variant.operation.operands = &q_cond;
          assert(llvm_lower_expression(&q_e, &val) == llvm_gen_be_error_t::ok);

          q_s16_t.kind = enk_constant; q_s16_t.type = &s16_e;
          q_s16_f.kind = enk_constant; q_s16_f.type = &s16_e;
          q_s16_t.next = &q_s16_f; q_cond.next = &q_s16_t;
          q_e.type = &s32_e;
          assert(llvm_lower_expression(&q_e, &val) == llvm_gen_be_error_t::ok);

          // `enk_initializer` (`dik_constant`, `dik_class_result_via_ctor`, `dik_constructor`) with `active_prvalue_dest_ptr` (matching & mismatching ty)!
          a_type st24_e = {}; st24_e.kind = tk_struct; st24_e.size = 24; st24_e.alignment = 77;
          llvm::StructType* st24_ll = llvm::StructType::get(*state.context, {i64_t, i64_t, i64_t});
          state.type_cache[&st24_e] = st24_ll;
          llvm::AllocaInst* init_dst = state.builder->CreateAlloca(st24_ll);
          a_constant st24_con = {}; st24_con.kind = ck_aggregate; st24_con.type = &st24_e;
          a_dynamic_init dip_con = {}; dip_con.kind = dik_constant; dip_con.variant.constant.ptr = &st24_con;
          an_expr_node init_con_e = {}; init_con_e.kind = enk_initializer; init_con_e.type = &st24_e; init_con_e.variant.initializer.dyn_init = &dip_con;
          state.active_prvalue_dest_ptr = init_dst; state.active_prvalue_dest_ty = i32_t; // mismatch!
          assert(llvm_lower_expression(&init_con_e, &val) == llvm_gen_be_error_t::ok);
          state.active_prvalue_dest_ptr = init_dst; state.active_prvalue_dest_ty = st24_ll; // match!
          assert(llvm_lower_expression(&init_con_e, &val) == llvm_gen_be_error_t::ok);

          a_routine_type_supplement rts0 = {};
          a_type fn_st24_edg = {}; fn_st24_edg.kind = tk_routine; fn_st24_edg.variant.routine.return_type = &st24_e; fn_st24_edg.variant.routine.extra_info = &rts0;
          a_routine r_st24 = {}; r_st24.source_corresp.name = (char*)"init_prval_fn"; r_st24.type = &fn_st24_edg;
          an_expr_node op_st24 = {}, call_st24 = {};
          op_st24.kind = enk_routine; op_st24.type = &fn_st24_edg; op_st24.variant.routine.ptr = &r_st24;
          call_st24.kind = enk_operation; call_st24.type = &st24_e; call_st24.variant.operation.kind = eok_call; call_st24.variant.operation.operands = &op_st24;
          a_dynamic_init dip_cr = {}; dip_cr.kind = dik_class_result_via_ctor; dip_cr.variant.expression = &call_st24;
          init_con_e.variant.initializer.dyn_init = &dip_cr;
          state.active_prvalue_dest_ptr = init_dst; state.active_prvalue_dest_ty = i32_t; // mismatch!
          assert(llvm_lower_expression(&init_con_e, &val) == llvm_gen_be_error_t::ok);
          state.active_prvalue_dest_ptr = init_dst; state.active_prvalue_dest_ty = st24_ll; // match -> direct_in_place!
          assert(llvm_lower_expression(&init_con_e, &val) == llvm_gen_be_error_t::ok);
          state.active_prvalue_dest_ptr = nullptr; state.active_prvalue_dest_ty = nullptr;
          state.prvalue_constructed_in_place = false;

          // `enk_throw` with `throw_info->expr` (ok & error) and `throw_info->destructor`!
          ensure_expr_bb();
          a_routine thr_dtor = {}; thr_dtor.source_corresp.name = (char*)"_ZN3ExcD1Ev";
          a_throw_supplement thr_sup = {};
          thr_sup.type = &s32_e; thr_sup.expr = &rhs32; thr_sup.destructor = &thr_dtor;
          an_expr_node thr_e = {}; thr_e.kind = enk_throw; thr_e.type = &void_e; thr_e.variant.throw_info = &thr_sup;
          run_expr_oom([&]() noexcept { return llvm_lower_expression(&thr_e, &val); });
          ensure_expr_bb();
          an_expr_node bad_thr_sub = {}; bad_thr_sub.kind = static_cast<an_expr_node_kind>(250);
          thr_sup.expr = &bad_thr_sub;
          assert(llvm_lower_expression(&thr_e, &val) == llvm_gen_be_error_t::unsupported_expr);

#if DO_IL_LOWERING
          // `leck_internal_try`: failing `first_e` (`L4989`), `second_e == nullptr` (`L5007`), and terminated `first_e` (`thr_e` -> `L4931`)!
          ensure_expr_bb();
          thr_sup.expr = &rhs32;
          an_expr_node eh_try = {}; eh_try.kind = enk_lowered_eh_construct; eh_try.type = &void_e;
          eh_try.variant.lowered_eh.kind = leck_internal_try;
          eh_try.variant.lowered_eh.variant.try_and_catch_expr = &bad_thr_sub;
          assert(llvm_lower_expression(&eh_try, &val) == llvm_gen_be_error_t::unsupported_expr);
          eh_try.variant.lowered_eh.variant.try_and_catch_expr = &thr_e; // thr_e terminates current block!
          assert(llvm_lower_expression(&eh_try, &val) == llvm_gen_be_error_t::ok);
          ensure_expr_bb();
          a_dynamic_init try_dip2 = {}; try_dip2.kind = dik_constructor;
          an_expr_node throw_only = {}; throw_only.kind = enk_initializer; throw_only.type = &s32_e; throw_only.variant.initializer.dyn_init = &try_dip2;
          throw_only.next = nullptr; // second_e == nullptr!
          eh_try.variant.lowered_eh.variant.try_and_catch_expr = &throw_only;
          g_dyn_init_emit_invoke = 1;
          assert(llvm_lower_expression(&eh_try, &val) == llvm_gen_be_error_t::ok);
          g_dyn_init_emit_invoke = 0;

#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
          // `enk_result_of_overriding_function`: `current_function_scope == nullptr`, extra args, unmapped/null param, and `void` return!
          llvm::Function* void_under = llvm::Function::Create(
              llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {i32_t}, false),
              llvm::Function::ExternalLinkage, "void_under_fn", state.module.get());
          assert(void_under != nullptr);
          a_routine r_void_under = {}, r_void_wrap = {};
          r_void_under.source_corresp.name = (char*)"void_under_fn"; r_void_under.type = &rty_e;
          r_void_wrap.overriding_function_for_wrapper = &r_void_under;
          state.current_edg_routine = &r_void_wrap;
          state.current_function_scope = nullptr;

          llvm::Function* w_fn2 = llvm::Function::Create(
              llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {i32_t, i32_t}, false),
              llvm::Function::ExternalLinkage, "void_wrap_fn", state.module.get());
          state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "entry", w_fn2));
          an_expr_node ovr_void = {}; ovr_void.kind = enk_result_of_overriding_function; ovr_void.type = &void_e;
          assert(llvm_lower_expression(&ovr_void, &val) == llvm_gen_be_error_t::ok); // scope == nullptr & void return!

          a_variable wp_unmapped = {}, wp_null = {}, wp_ok = {}, wp_extra = {};
          wp_unmapped.next = &wp_null; wp_null.next = &wp_ok; wp_ok.next = &wp_extra;
          state.local_vars[&wp_null] = nullptr;
          state.local_vars[&wp_ok] = state.builder->CreateAlloca(i32_t);
          state.local_vars[&wp_extra] = state.builder->CreateAlloca(i32_t);
          a_scope wsc2 = {}; wsc2.variant.routine.parameters = &wp_unmapped;
          state.current_function_scope = &wsc2;
          assert(llvm_lower_expression(&ovr_void, &val) == llvm_gen_be_error_t::ok);
          { llvm::IRBuilder<> wb2(&w_fn2->back()); wb2.CreateRetVoid(); }
          state.current_function_scope = nullptr;
          state.current_edg_routine = nullptr;
          state.builder->SetInsertPoint(&ws10_fn->back());
          ensure_expr_bb();
#endif
#endif

          // Compound assignment on `_Complex int` & `_Complex float` with scalar float & int RHS, plus `char*` & untyped atomic pointer `+=`!
          a_type cplx_i32_e = {}; cplx_i32_e.kind = tk_complex; cplx_i32_e.size = 8;
          llvm::StructType* cplx_i32_ll = llvm::StructType::get(*state.context, {i32_t, i32_t});
          state.type_cache[&cplx_i32_e] = cplx_i32_ll;
          a_type cplx_f32_e = {}; cplx_f32_e.kind = tk_complex; cplx_f32_e.size = 8;
          llvm::StructType* cplx_f32_ll = llvm::StructType::get(*state.context, {f32_t, f32_t});
          state.type_cache[&cplx_f32_e] = cplx_f32_ll;

          a_variable vci = {}, vcf = {}; vci.type = &cplx_i32_e; vcf.type = &cplx_f32_e;
          state.local_vars[&vci] = state.builder->CreateAlloca(cplx_i32_ll);
          state.local_vars[&vcf] = state.builder->CreateAlloca(cplx_f32_ll);
          an_expr_node eci = {}, ecf = {}, s_int = {}, s_flt = {}, ca = {};
          eci.kind = enk_variable; eci.type = &cplx_i32_e; eci.variant.variable.ptr = &vci;
          ecf.kind = enk_variable; ecf.type = &cplx_f32_e; ecf.variant.variable.ptr = &vcf;
          s_int.kind = enk_constant; s_int.type = &s32_e;
          s_flt.kind = enk_constant; s_flt.type = &f32_e;
          ca.kind = enk_operation;
          for (an_expr_node* sr : {&s_int, &s_flt}) {
            eci.next = sr; ca.type = &cplx_i32_e; ca.variant.operation.operands = &eci;
            ca.variant.operation.kind = eok_add_assign;
            assert(llvm_lower_compound_assignment(&ca, &val) == llvm_gen_be_error_t::ok);
            ca.variant.operation.kind = eok_subtract_assign;
            assert(llvm_lower_compound_assignment(&ca, &val) == llvm_gen_be_error_t::ok);
            ecf.next = sr; ca.type = &cplx_f32_e; ca.variant.operation.operands = &ecf;
            ca.variant.operation.kind = eok_add_assign;
            assert(llvm_lower_compound_assignment(&ca, &val) == llvm_gen_be_error_t::ok);
          }

          a_type atom_p8_e = {}; atom_p8_e.kind = tk_typeref; atom_p8_e.variant.typeref.type = &ptr_s8_e; atom_p8_e.variant.typeref.qualifiers = TQ_C11_ATOMIC;
          a_type atom_pnull_e = {}; atom_pnull_e.kind = tk_typeref; atom_pnull_e.variant.typeref.type = &ptr_null_e; atom_pnull_e.variant.typeref.qualifiers = TQ_C11_ATOMIC;
          a_type atom_nonptr_e = {}; atom_nonptr_e.kind = tk_typeref; atom_nonptr_e.variant.typeref.type = &s64_e; atom_nonptr_e.variant.typeref.qualifiers = TQ_C11_ATOMIC;
          state.type_cache[&s64_e] = ptr_t; // simulate pointer LLVM type with non-tk_pointer EDG type at L5137!
          a_variable vap8 = {}, vapn = {}, vanp = {};
          vap8.type = &atom_p8_e; vapn.type = &atom_pnull_e; vanp.type = &atom_nonptr_e;
          state.local_vars[&vap8] = state.builder->CreateAlloca(ptr_t);
          state.local_vars[&vapn] = state.builder->CreateAlloca(ptr_t);
          state.local_vars[&vanp] = state.builder->CreateAlloca(ptr_t);
          for (a_variable* av : {&vap8, &vapn, &vanp}) {
            an_expr_node eav = {}; eav.kind = enk_variable; eav.type = av->type; eav.variant.variable.ptr = av; eav.next = &s_int;
            ca.type = &ptr_s8_e; ca.variant.operation.operands = &eav; ca.variant.operation.kind = eok_add_assign;
            assert(llvm_lower_compound_assignment(&ca, &val) == llvm_gen_be_error_t::ok);
          }
          state.type_cache[&s64_e] = i64_t;
        }
      }

      // --- 10.G (Workstream 12): Complete 100.00% branch & OOM closure for src/llvm_gen_be_expr.cpp ---
      {
        state.type_cache.clear();
        ensure_expr_bb();

        a_type s8_e = {}; s8_e.kind = tk_integer; s8_e.size = 1; s8_e.variant.integer.int_kind = ik_int;
        a_type s16_e = {}; s16_e.kind = tk_integer; s16_e.size = 2; s16_e.variant.integer.int_kind = ik_short;
        a_type s24_e = {}; s24_e.kind = tk_integer; s24_e.size = 3; s24_e.variant.integer.int_kind = ik_int;
        state.type_cache[&s24_e] = llvm::IntegerType::get(*state.context, 24);
        a_type s32_e = {}; s32_e.kind = tk_integer; s32_e.size = 4; s32_e.variant.integer.int_kind = ik_int;
        a_type s64_e = {}; s64_e.kind = tk_integer; s64_e.size = 8; s64_e.variant.integer.int_kind = ik_long_long;
        a_type s256_e = {}; s256_e.kind = tk_integer; s256_e.size = 32; s256_e.variant.integer.int_kind = ik_int;
        state.type_cache[&s256_e] = llvm::IntegerType::get(*state.context, 256);
        a_type enum_e = {}; enum_e.kind = tk_integer; enum_e.size = 2; enum_e.variant.integer.enum_type = TRUE;
        state.type_cache[&enum_e] = i16_t;
        a_type bool_e = {}; bool_e.kind = tk_integer; bool_e.size = 1; bool_e.variant.integer.bool_type = TRUE;
        a_type void_e = {}; void_e.kind = tk_void;
        a_type f32_e = {}; f32_e.kind = tk_float; f32_e.size = 4; f32_e.variant.float_kind = fk_float;
        a_type f64_e = {}; f64_e.kind = tk_float; f64_e.size = 8; f64_e.variant.float_kind = fk_double;
        a_type rty_e = {}; rty_e.kind = tk_routine;
        a_type ptr_s32_e = {}; ptr_s32_e.kind = tk_pointer; ptr_s32_e.size = 8; ptr_s32_e.variant.pointer.type = &s32_e;
        a_type ptr_void_e = {}; ptr_void_e.kind = tk_pointer; ptr_void_e.size = 8; ptr_void_e.variant.pointer.type = &void_e;
        a_type ptr_null_e = {}; ptr_null_e.kind = tk_pointer; ptr_null_e.size = 8; ptr_null_e.variant.pointer.type = nullptr;
        a_type ptr_ptr_null_e = {}; ptr_ptr_null_e.kind = tk_pointer; ptr_ptr_null_e.size = 8; ptr_ptr_null_e.variant.pointer.type = &ptr_null_e;
        a_type st16_e = {}; st16_e.kind = tk_struct; st16_e.size = 16; st16_e.alignment = 77;
        llvm::StructType* st16_ll = llvm::StructType::get(*state.context, {i64_t, i64_t});
        state.type_cache[&st16_e] = st16_ll;
        a_type ptr_st16_e = {}; ptr_st16_e.kind = tk_pointer; ptr_st16_e.size = 8; ptr_st16_e.variant.pointer.type = &st16_e;
        a_type st24_e = {}; st24_e.kind = tk_struct; st24_e.size = 24; st24_e.alignment = 77;
        llvm::StructType* st24_ll = llvm::StructType::get(*state.context, {i64_t, i64_t, i64_t});
        state.type_cache[&st24_e] = st24_ll;

        // 1. Bitfield L186 (`24`-bit non-power-of-2 declared container without straddle) & L193 (`128`-bit power-of-2 `> 64U`)!
        {
          a_type vol24_wrap = {}; vol24_wrap.kind = tk_typeref; vol24_wrap.variant.typeref.type = &s24_e; vol24_wrap.variant.typeref.qualifiers = TQ_VOLATILE;
          llvm::AllocaInst* bf16 = state.builder->CreateAlloca(llvm::ArrayType::get(i8_t, 16));
          a_field bf_24 = {}; bf_24.is_bit_field = TRUE; bf_24.type = &vol24_wrap; bf_24.offset_bit_remainder = 0; bf_24.bit_size = 4;
          assert(llvm_emit_bitfield_load(bf16, &bf_24, &s32_e, &val) == llvm_gen_be_error_t::ok); // 24 in [8,64], !straddle, !isPowerOf2_32!
          bf_24.type = nullptr; bf_24.bit_size = 128; // span_bits = 128 -> isPowerOf2_32(128) TRUE && 128 > 64U TRUE!
          assert(llvm_emit_bitfield_load(bf16, &bf_24, &s64_e, &val) == llvm_gen_be_error_t::ok);
        }

        // 2. LValue OOM sweeps (`MBR L646..L734`) & `sc_unspecified` + `is_thread_local` (`L682`):
        {
          // L646: ck_string constant lvalue OOM
          a_constant str_con = {}; str_con.kind = ck_string; str_con.type = &s32_e;
          an_expr_node e_str = {}; e_str.kind = enk_constant; e_str.type = &s32_e; e_str.variant.constant.ptr = &str_con;
          run_expr_oom([&]() noexcept {
            state.string_literal_cache.clear();
            return llvm_lower_lvalue_expression(&e_str, &val);
          });

          // L657: enk_routine lvalue OOM
          a_routine rt_lv = {}; rt_lv.source_corresp.name = (char*)"oom_lv_rt"; rt_lv.type = &rty_e;
          an_expr_node e_rt = {}; e_rt.kind = enk_routine; e_rt.type = &rty_e; e_rt.variant.routine.ptr = &rt_lv;
          run_expr_oom([&]() noexcept { return llvm_lower_lvalue_expression(&e_rt, &val); });

          // L674: sc_auto local OOM; L682, L685, L692: unnamed `sc_unspecified` + `is_thread_local` OOM; L699, L706: named external global OOM!
          run_expr_oom([&]() noexcept {
            a_variable va = {}; va.storage_class = sc_auto; va.type = &s32_e;
            state.local_vars.erase(&va);
            an_expr_node eva = {}; eva.kind = enk_variable; eva.type = &s32_e; eva.variant.variable.ptr = &va;
            return llvm_lower_lvalue_expression(&eva, &val);
          });
          run_expr_oom([&]() noexcept {
            a_variable vtls = {}; vtls.storage_class = sc_unspecified; vtls.is_thread_local = TRUE; vtls.type = &s32_e; vtls.source_corresp.name = nullptr;
            state.local_vars.erase(&vtls);
            an_expr_node evt = {}; evt.kind = enk_variable; evt.type = &s32_e; evt.variant.variable.ptr = &vtls;
            return llvm_lower_lvalue_expression(&evt, &val);
          });
          static int s_ext_idx = 0;
          run_expr_oom([&]() noexcept {
            std::string gname = "oom_ext_gv_" + std::to_string(++s_ext_idx);
            a_variable vext = {}; vext.storage_class = sc_extern; vext.type = &s32_e; vext.source_corresp.name = const_cast<char*>(gname.c_str());
            an_expr_node eve = {}; eve.kind = enk_variable; eve.type = &s32_e; eve.variant.variable.ptr = &vext;
            return llvm_lower_lvalue_expression(&eve, &val);
          });

          // L723 (compound literal static global OOM) & L734 (temp_init with dtor OOM)!
          a_routine tmp_dtor = {}; tmp_dtor.source_corresp.name = (char*)"_ZN3TmpD1Ev";
          a_dynamic_init dip_cl = {}; dip_cl.kind = dik_zero; dip_cl.destructor = &tmp_dtor; dip_cl.has_temporary_lifetime = TRUE;
          an_expr_node e_cl = {}; e_cl.kind = enk_temp_init; e_cl.type = &s32_e; e_cl.variant.init.dynamic_init = &dip_cl;
          run_expr_oom([&]() noexcept { return llvm_lower_lvalue_expression(&e_cl, &val); });
          dip_cl.static_temp = TRUE;
          run_expr_oom([&]() noexcept { return llvm_lower_lvalue_expression(&e_cl, &val); });
        }

        // 3. UBSan + TBAA + `_Complex` `eok_xconj` + `_Complex` `/=`/`*=` OOM sweeps (`L516..L552`, `L1011..L1223`, `L1184`, `L1595`, `L2185`, `L4604..L4615`, `L5293..L5299`):
        {
          a_type cplx_i32_e = {}; cplx_i32_e.kind = tk_complex; cplx_i32_e.size = 8;
          llvm::StructType* cplx_i32_ll = llvm::StructType::get(*state.context, {i32_t, i32_t});
          state.type_cache[&cplx_i32_e] = cplx_i32_ll;
          a_type cplx_f32_e = {}; cplx_f32_e.kind = tk_complex; cplx_f32_e.size = 8;
          llvm::StructType* cplx_f32_ll = llvm::StructType::get(*state.context, {f32_t, f32_t});
          state.type_cache[&cplx_f32_e] = cplx_f32_ll;

          an_expr_node ci1 = {}, cf1 = {}, cf2 = {}, c_op = {};
          ci1.kind = enk_constant; ci1.type = &cplx_i32_e;
          cf1.kind = enk_constant; cf1.type = &cplx_f32_e; cf1.next = &cf2;
          cf2.kind = enk_constant; cf2.type = &cplx_f32_e;
          c_op.kind = enk_operation; c_op.type = &cplx_i32_e; c_op.variant.operation.kind = eok_xconj; c_op.variant.operation.operands = &ci1;
          assert(llvm_lower_arithmetic_expression(&c_op, &val) == llvm_gen_be_error_t::ok); // L1184 neg_i1 on integer!
          c_op.type = &cplx_f32_e; c_op.variant.operation.kind = eok_divide; c_op.variant.operation.operands = &cf1;
          run_expr_oom([&]() noexcept { return llvm_lower_arithmetic_expression(&c_op, &val); }); // MBR L1080!

          // `_Complex float` `*=` and `/=` OOM sweeps (`MBR L5293` & `L5299`)!
          a_variable vcf = {}; vcf.type = &cplx_f32_e; state.local_vars[&vcf] = state.builder->CreateAlloca(cplx_f32_ll);
          an_expr_node lv_cf = {}; lv_cf.kind = enk_variable; lv_cf.type = &cplx_f32_e; lv_cf.variant.variable.ptr = &vcf; lv_cf.next = &cf2;
          an_expr_node ca_cf = {}; ca_cf.kind = enk_operation; ca_cf.type = &cplx_f32_e; ca_cf.variant.operation.operands = &lv_cf;
          ca_cf.variant.operation.kind = eok_multiply_assign;
          run_expr_oom([&]() noexcept { return llvm_lower_compound_assignment(&ca_cf, &val); });
          ca_cf.variant.operation.kind = eok_divide_assign;
          run_expr_oom([&]() noexcept { return llvm_lower_compound_assignment(&ca_cf, &val); });

          // Enable UBSan & O2 TBAA and run OOM sweeps on load (int & bool), negate, add, sub, mul, div, rem, shl, shr, float->int cast, call nonnull, and >16B/<=16B assign!
          state.enable_ubsan_checks = true;
          state.opt_level = llvm_opt_level_t::O2;
          a_variable vs = {}, vb = {}, vst24 = {};
          vs.type = &s32_e; vb.type = &bool_e; vst24.type = &st24_e;
          state.local_vars[&vs] = state.builder->CreateAlloca(i32_t);
          state.local_vars[&vb] = state.builder->CreateAlloca(i8_t);
          state.local_vars[&vst24] = state.builder->CreateAlloca(st24_ll);

          an_expr_node evs1 = {}, evs2 = {}, evb = {};
          evs1.kind = enk_variable; evs1.type = &s32_e; evs1.variant.variable.ptr = &vs;
          evs2.kind = enk_constant; evs2.type = &s32_e;
          evb.kind = enk_variable; evb.type = &bool_e; evb.variant.variable.ptr = &vb;
          run_expr_oom([&]() noexcept { return llvm_lower_expression(&evb, &val); }); // MBR L516, L547, L552!

          evs1.next = nullptr;
          an_expr_node u_ar = {}; u_ar.kind = enk_operation; u_ar.type = &s32_e; u_ar.variant.operation.operands = &evs1;
          u_ar.variant.operation.kind = eok_negate;
          run_expr_oom([&]() noexcept { return llvm_lower_arithmetic_expression(&u_ar, &val); }); // MBR L1011!
          evs1.next = &evs2;
          for (an_expr_operator_kind ak : {eok_add, eok_subtract, eok_multiply, eok_divide, eok_remainder, eok_shiftl, eok_shiftr}) {
            u_ar.variant.operation.kind = ak;
            run_expr_oom([&]() noexcept { return llvm_lower_arithmetic_expression(&u_ar, &val); });
          }

          an_expr_node ef_c = {}; ef_c.kind = enk_constant; ef_c.type = &f32_e;
          an_expr_node u_fc = {}; u_fc.kind = enk_operation; u_fc.type = &s32_e; u_fc.variant.operation.kind = eok_cast; u_fc.variant.operation.operands = &ef_c;
          run_expr_oom([&]() noexcept { return llvm_lower_cast_expression(&u_fc, &val); }); // MBR L1595!

          // UBSan __builtin_unreachable check (`MBR L2185`):
          run_expr_oom([&]() noexcept { return call_test_builtin("__builtin_unreachable", &void_e, {}); });
          ensure_expr_bb();

          // TBAA assign OOM sweeps (`MBR L4604` >16B struct memcpy & `MBR L4615` scalar store)!
          an_expr_node lv_s = {}, lv_24 = {}, rv_24 = {}, as_s = {}, as_24 = {};
          lv_s.kind = enk_variable; lv_s.is_lvalue = TRUE; lv_s.type = &s32_e; lv_s.variant.variable.ptr = &vs; lv_s.next = &evs2;
          as_s.kind = enk_operation; as_s.type = &s32_e; as_s.variant.operation.kind = eok_assign; as_s.variant.operation.operands = &lv_s;
          run_expr_oom([&]() noexcept { return llvm_lower_expression(&as_s, &val); });

          lv_24.kind = enk_variable; lv_24.is_lvalue = TRUE; lv_24.type = &st24_e; lv_24.variant.variable.ptr = &vst24; lv_24.next = &rv_24;
          rv_24.kind = enk_variable; rv_24.is_lvalue = FALSE; rv_24.type = &st24_e; rv_24.variant.variable.ptr = &vst24;
          as_24.kind = enk_operation; as_24.type = &st24_e; as_24.variant.operation.kind = eok_assign; as_24.variant.operation.operands = &lv_24;
          run_expr_oom([&]() noexcept { return llvm_lower_expression(&as_24, &val); });

          state.enable_ubsan_checks = false;
          state.opt_level = llvm_opt_level_t::O0;
        }

        // 4. Call ABI coercion, overloaded builtins, & atomic edge cases + OOM sweeps (`L1780..L3133`):
        {
          an_expr_node ei32 = {}, ei24 = {}, ei256 = {}, epv = {}, epn = {}, eppn = {};
          ei32.kind = enk_constant; ei32.type = &s32_e;
          ei24.kind = enk_constant; ei24.type = &s24_e;
          ei256.kind = enk_constant; ei256.type = &s256_e;
          a_variable vp = {}; vp.type = &ptr_void_e; state.local_vars[&vp] = state.builder->CreateAlloca(ptr_t);
          epv.kind = enk_variable; epv.type = &ptr_void_e; epv.variant.variable.ptr = &vp;
          epn.kind = enk_variable; epn.type = &ptr_null_e; epn.variant.variable.ptr = &vp;
          eppn.kind = enk_variable; eppn.type = &ptr_ptr_null_e; eppn.variant.variable.ptr = &vp;

          // `L1949` (`!op1`)
          an_expr_node empty_call = {}; empty_call.kind = enk_operation; empty_call.variant.operation.kind = eok_call; empty_call.variant.operation.operands = nullptr;
          assert(llvm_lower_call_expression(&empty_call, &val) == llvm_gen_be_error_t::invalid_argument);

          a_routine_type_supplement rts_c = {};
          a_type fn_c_edg = {}; fn_c_edg.kind = tk_routine; fn_c_edg.variant.routine.return_type = &s32_e; fn_c_edg.variant.routine.extra_info = &rts_c;
          empty_call.type = &s32_e;

          // `L2036`, `L2043`, `L2044`, `L2051`:
          // Param 0: `ptr addrspace(1)` vs `ptr` (`expected_ty->isPointerTy()` TRUE, `!arg_val->isPointerTy()` FALSE!)
          // Param 1: `ptr` vs `i32` (`!arg_val->isPointerTy()` TRUE, `arg_is_agg` FALSE!)
          // Param 2: `f64` vs `i32` (`arg_val->isIntegerTy()` TRUE, `expected_ty->isIntegerTy()` FALSE!)
          // Param 3..6: `i64, i64, i64, i64` vs `i256` (`arg_val->isIntegerTy()` TRUE, `expected_ty->isIntegerTy()` TRUE, `!arg_is_agg` FALSE!)
          // Param 7: `i64` vs untyped `i32` (`raw_arg_edg == nullptr` at L2044!)
          // Param 8: `i32` vs `enum_e` (`raw_arg_edg->kind == tk_integer` — wait: `tk_enum`!)
          // Param 9: `i32` vs `f32` (`arg_val->isFloatingPointTy()` TRUE, `expected_ty->isFloatingPointTy()` FALSE!)
          a_type real_enum_e = {}; real_enum_e.kind = tk_enum; real_enum_e.size = 2;
          state.type_cache[&real_enum_e] = i16_t;
          llvm::Type* ptr_as1 = llvm::PointerType::get(*state.context, 1);
          llvm::FunctionType* edge_fty = llvm::FunctionType::get(
              i32_t, {ptr_as1, ptr_t, f64_t, i64_t, i64_t, i64_t, i64_t, i64_t, i32_t, i32_t}, false);
          llvm::Function::Create(edge_fty, llvm::Function::ExternalLinkage, "fn_edge_coercion", state.module.get());
          a_routine rt_edge = {}; rt_edge.source_corresp.name = (char*)"fn_edge_coercion"; rt_edge.type = &fn_c_edg;
          an_expr_node eo0 = {}, ea0 = {}, ea1 = {}, ea2 = {}, ea3 = {}, ea4 = {}, ea5 = {}, ea6 = {};
          eo0.kind = enk_routine; eo0.type = &fn_c_edg; eo0.variant.routine.ptr = &rt_edge; eo0.next = &ea0;
          ea0.kind = enk_constant; ea0.type = &ptr_s32_e; ea0.next = &ea1; // ptr -> ptr addrspace(1): bitcast/store!
          ea1.kind = enk_constant; ea1.type = &s32_e; ea1.next = &ea2; // i32 -> ptr!
          ea2.kind = enk_constant; ea2.type = &s32_e; ea2.next = &ea3; // i32 -> f64!
          ea3.kind = enk_constant; ea3.type = &s256_e; ea3.next = &ea4; // i256 (agg) -> 4 x i64!
          ea4.kind = enk_type_operand; ea4.type = nullptr; ea4.next = &ea5; // untyped i32 -> i64!
          ea5.kind = enk_constant; ea5.type = &real_enum_e; ea5.next = &ea6; // tk_enum i16 -> i32!
          ea6.kind = enk_constant; ea6.type = &f32_e; ea6.next = nullptr; // f32 -> i32!
          empty_call.variant.operation.operands = &eo0;
          assert(llvm_lower_call_expression(&empty_call, &val) == llvm_gen_be_error_t::ok);

          // Overloaded & scalar builtins + OOM sweeps (`L1780`, `L1867`, `L1879`, `L2269`, `L2283`, `L2759`, `MBR L1778..L2760`, `MBR L3903`):
          assert(call_test_builtin("__builtin_clz", nullptr, {&ei32}) == llvm_gen_be_error_t::ok); // L1780 expr_edg_type == nullptr!
          run_expr_oom([&]() noexcept { return call_test_builtin("__builtin_clz", &s16_e, {&ei32}); }); // MBR L1778, L1782!
          assert(call_test_builtin("__builtin_constant_p", &s32_e, {}) == llvm_gen_be_error_t::ok); // L1867 args.empty()!
          run_expr_oom([&]() noexcept { return call_test_builtin("__builtin_constant_p", &s16_e, {&ei32}); }); // MBR L1873!
          assert(call_test_builtin("__builtin_bswap32", &s32_e, {}) == llvm_gen_be_error_t::ok); // L1879 args.empty()!
          run_expr_oom([&]() noexcept { return call_test_builtin("__builtin_bswap32", &s16_e, {&ei32}); }); // MBR L1891!
          assert(call_test_builtin("__builtin_add_overflow_p", nullptr, {&ei32, &ei32, &ei32}) == llvm_gen_be_error_t::ok); // L2269!
          run_expr_oom([&]() noexcept { return call_test_builtin("__builtin_add_overflow", &bool_e, {&ei32, &ei32, &epv}); }); // MBR L2254!
          run_expr_oom([&]() noexcept { return call_test_builtin("__builtin_add_overflow_p", &bool_e, {&ei32, &ei32, &ei32}); }); // MBR L2269!
          assert(call_test_builtin("__builtin_stdc_has_single_bit", nullptr, {&ei32}) == llvm_gen_be_error_t::ok); // L2283!
          run_expr_oom([&]() noexcept { return call_test_builtin("__builtin_stdc_has_single_bit", &bool_e, {&ei32}); }); // MBR L2283!

          // `__builtin_clear_padding` (`MBR L2347`), `__builtin_bit_cast` (`MBR L2446`), `__builtin_huge_valf` (`MBR L2483`),
          // `__builtin_prefetch` (`MBR L2561`), `__builtin_isinf_sign` (`MBR L2598`), `__builtin_fpclassify` (`MBR L2603`),
          // `__builtin_expect_with_probability` (`MBR L2733`), `__atomic_is_lock_free` (`L2759` & `MBR L2760`), `__atomic_test_and_set` (`MBR L2779`)!
          a_field pad_f = {}; pad_f.type = &s32_e; pad_f.offset = 0;
          a_base_class pad_bc = {}; pad_bc.type = &st16_e; pad_bc.offset = 0;
          a_class_type_supplement pad_ext = {}; pad_ext.base_classes = &pad_bc;
          a_type pad_st_e = {}; pad_st_e.kind = tk_struct; pad_st_e.size = 24; pad_st_e.variant.class_struct_union.field_list = &pad_f; pad_st_e.variant.class_struct_union.extra_info = &pad_ext;
          a_type ptr_pad_e = {}; ptr_pad_e.kind = tk_pointer; ptr_pad_e.size = 8; ptr_pad_e.variant.pointer.type = &pad_st_e;
          an_expr_node ep_pad = {}; ep_pad.kind = enk_variable; ep_pad.type = &ptr_pad_e; ep_pad.variant.variable.ptr = &vp;
          run_expr_oom([&]() noexcept { return call_test_builtin("__builtin_clear_padding", &void_e, {&ep_pad}); }); // MBR L2347, L5454, L5477!
          run_expr_oom([&]() noexcept { return call_test_builtin("__builtin_bit_cast", &f32_e, {&ei32}); }); // MBR L2446!
          run_expr_oom([&]() noexcept { return call_test_builtin("__builtin_huge_valf", &f32_e, {}); }); // MBR L2483!
          run_expr_oom([&]() noexcept { return call_test_builtin("__builtin_prefetch", &void_e, {&epv, &ei32, &ei32}); }); // MBR L2561!
          an_expr_node ef32 = {}; ef32.kind = enk_constant; ef32.type = &f32_e;
          run_expr_oom([&]() noexcept { return call_test_builtin("__builtin_isinf_sign", &s32_e, {&ef32}); }); // MBR L2598!
          run_expr_oom([&]() noexcept { return call_test_builtin("__builtin_fpclassify", &s32_e, {&ei32, &ei32, &ei32, &ei32, &ei32, &ef32}); }); // MBR L2603!
          run_expr_oom([&]() noexcept { return call_test_builtin("__builtin_expect_with_probability", &s32_e, {&ei32, &ei32, &ef32}); }); // MBR L2733!
          assert(call_test_builtin("__atomic_is_lock_free", nullptr, {&ei32, &epv}) == llvm_gen_be_error_t::ok); // L2759!
          run_expr_oom([&]() noexcept { return call_test_builtin("__atomic_is_lock_free", &bool_e, {&ei32, &epv}); }); // MBR L2760!
          run_expr_oom([&]() noexcept { return call_test_builtin("__atomic_test_and_set", &bool_e, {&epv, &ei32}); }); // MBR L2779!

          // Atomics remaining source & OOM branches (`L1805`, `L1833`, `L1838`, `L2829..L3133`):
          assert(call_test_builtin("__atomic_load_n", &s24_e, {&epv, &ei32}) == llvm_gen_be_error_t::ok); // L1805 sz=3 !isPowerOf2_64!
          assert(call_test_builtin("__atomic_load_n", &s256_e, {&epv, &ei32}) == llvm_gen_be_error_t::ok); // L1805 sz=32 > 16!
          run_expr_oom([&]() noexcept { return call_test_builtin("__atomic_load_n", &s32_e, {&epv, &ei32}); }); // MBR L2962!
          run_expr_oom([&]() noexcept { return call_test_builtin("__atomic_store_n", &void_e, {&epv, &ei32, &ei32}); }); // MBR L2997!
          run_expr_oom([&]() noexcept { return call_test_builtin("__atomic_exchange_n", &s32_e, {&epv, &ei32, &ei32}); }); // MBR L3061!

          // `L1833:41` (`epn` directly has `pt->variant.pointer.type == nullptr`) & `L1838` (`cur` is `eok_unary_plus`, NOT `eok_address_of`)!
          an_expr_node uplus_nonptr = {}; uplus_nonptr.kind = enk_operation; uplus_nonptr.type = &s32_e; uplus_nonptr.variant.operation.kind = eok_unary_plus; uplus_nonptr.variant.operation.operands = &ei32;
          assert(call_test_builtin("__atomic_load", &void_e, {&epn, &epn, &ei32}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__atomic_load", &void_e, {&epv, &uplus_nonptr, &ei32}) == llvm_gen_be_error_t::atomic_lowering_failed);

          // `L2939` & `L3023`: 16-byte struct (`st16_e`, `!is_nat` TRUE && `isPowerOf2_64(16)` TRUE && `sz <= 8` FALSE)!
          an_expr_node ep_st16 = {}; ep_st16.kind = enk_variable; ep_st16.type = &ptr_st16_e; ep_st16.variant.variable.ptr = &vp;
          assert(call_test_builtin("__atomic_load", &void_e, {&ep_st16, &ep_st16, &ei32}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__atomic_store", &void_e, {&ep_st16, &ep_st16, &ei32}) == llvm_gen_be_error_t::ok);

          // Float `fetch_add` edge cases (`L2829..L2833`) & pointer `fetch_add` edge cases (`L2844..L2868`)!
          an_expr_node cast_null_inner = {}; cast_null_inner.kind = enk_operation; cast_null_inner.type = &s64_e; cast_null_inner.variant.operation.kind = eok_cast; cast_null_inner.variant.operation.operands = nullptr;
          // Note: `cast_null_inner` cannot be lowered by `args` loop, so test `rhs_node` as `ei32` (`enk_constant`, `rhs_node->kind == enk_operation` FALSE at L2830!) + OOM sweep on `eok_cast(&ef32)` (`MBR L2827, L2833, L2853`)!
          assert(call_test_builtin("__atomic_fetch_add", &f32_e, {&epv, &ei32, &ei32}) == llvm_gen_be_error_t::ok);
          an_expr_node cast_ef32 = {}; cast_ef32.kind = enk_operation; cast_ef32.type = &s64_e; cast_ef32.variant.operation.kind = eok_cast; cast_ef32.variant.operation.operands = &ef32;
          run_expr_oom([&]() noexcept { return call_test_builtin("__atomic_fetch_add", &f32_e, {&epv, &cast_ef32, &ei32}); });

          // Pointer `fetch_add` OOM (`MBR L2860`) + untyped `op1->next` (`L2844`), non-ptr `op1->next` (`L2845`), `ptr_null_e` (`L2845`), 2-arg `__atomic_fetch_add` (`L2852`), `__c11_atomic_fetch_add` with `ptr_s32_e` (`L2864`) and `ptr_ptr_null_e` (`L2865` & `L2868`)!
          an_expr_node ep_untyped = epv; ep_untyped.type = nullptr; ep_untyped.is_lvalue = TRUE;
          an_expr_node ep_s32_typed = epv; ep_s32_typed.type = &s32_e; ep_s32_typed.is_lvalue = TRUE;
          assert(call_test_builtin("__atomic_fetch_add", &ptr_s32_e, {&ep_untyped, &ei32}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__atomic_fetch_add", &ptr_s32_e, {&ep_s32_typed, &ei32, &ei32}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__atomic_fetch_add", &ptr_s32_e, {&epn, &ei32, &ei32}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__c11_atomic_fetch_add", &ptr_s32_e, {&epv, &ei32, &ei32}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__c11_atomic_fetch_add", &ptr_s32_e, {&eppn, &ei32, &ei32}) == llvm_gen_be_error_t::ok);
          run_expr_oom([&]() noexcept { return call_test_builtin("__c11_atomic_fetch_add", &ptr_s32_e, {&eppn, &ei32, &ei32}); });

          // 2-arg & 6-arg `__atomic_compare_exchange` with `has_sz_arg` & untyped `exp_arg_node` (`L3122-3124`) & OOM (`MBR L3133`)!
          assert(call_test_builtin("__atomic_compare_exchange", &bool_e, {&ei32, &epv, &epv, &epv}) == llvm_gen_be_error_t::ok); // exp_arg_node->type != nullptr & 4 args
          assert(call_test_builtin("__atomic_compare_exchange", &bool_e, {&ei32, &epv, &ep_untyped, &epv, &ei32, &ei32}) == llvm_gen_be_error_t::ok);
          run_expr_oom([&]() noexcept { return call_test_builtin("__atomic_compare_exchange_n", &bool_e, {&epv, &epv, &ei32, &ei32, &ei32, &ei32}); });
        }

        // 5. Type traits remaining branches (`L3412..L3713` + `MBR L3688` + `MBR L3903`):
        {
          auto eval_t2 = [&](a_builtin_operation_kind k, a_type_ptr t1, a_type_ptr t2 = nullptr, bool has_op2 = true) noexcept -> llvm_gen_be_error_t {
            an_expr_node o1 = {}, o2 = {}, be = {};
            o1.kind = enk_type_operand; o1.variant.type_operand.type = t1;
            if (has_op2) { o2.kind = enk_type_operand; o2.variant.type_operand.type = t2; o1.next = &o2; }
            be.kind = enk_builtin_operation; be.type = &s32_e; be.variant.builtin_operation.kind = k;
            be.variant.builtin_operation.operands = &o1;
            return llvm_lower_builtin_operation(&be, &val);
          };

          a_type uni_b = {}; uni_b.kind = tk_union; uni_b.size = 4;
          a_type base_target = {}; base_target.kind = tk_struct; base_target.size = 4;
          a_type base_other = {}; base_other.kind = tk_struct; base_other.size = 4;
          a_base_class bc_u = {}, bc_d1 = {}, bc_d2 = {};
          bc_u.type = &uni_b; bc_u.next = nullptr;
          bc_d1.type = &base_target; bc_d1.next_direct = &bc_d2; bc_d1.next = &bc_d2;
          bc_d2.type = &base_other; bc_d2.next_direct = nullptr; bc_d2.next = nullptr;
          a_class_type_supplement ext_b1 = {}, ext_b2 = {};
          ext_b1.base_classes = &bc_u;
          ext_b2.direct_base_classes = &bc_d1; ext_b2.base_classes = &bc_d1;
          a_type cls_u_base = {}, cls_2dir = {};
          cls_u_base.kind = tk_struct; cls_u_base.size = 4; cls_u_base.variant.class_struct_union.extra_info = &ext_b1;
          cls_2dir.kind = tk_struct; cls_2dir.size = 4; cls_2dir.variant.class_struct_union.extra_info = &ext_b2;
          assert(eval_t2(bok_is_base_of, &base_target, &cls_u_base) == llvm_gen_be_error_t::ok); // L3412 bt->kind == tk_union!
          assert(eval_t2(bok_is_base_of, &base_target, &cls_2dir) == llvm_gen_be_error_t::ok); // L3420 !found false on 2nd direct base!

          // `L3507` (non-virtual dtor) & `L3514` (`bc->type == nullptr` in both direct_base_classes and base_classes)
          a_routine nonvirt_dtor = {}; nonvirt_dtor.special_kind = sfk_destructor; nonvirt_dtor.is_virtual = FALSE;
          a_scope sc_nvd = {}; sc_nvd.routines = &nonvirt_dtor;
          a_base_class bc_null_vd = {}; bc_null_vd.type = nullptr;
          a_class_type_supplement ext_nvd = {}; ext_nvd.assoc_scope = &sc_nvd; ext_nvd.direct_base_classes = &bc_null_vd; ext_nvd.base_classes = &bc_null_vd;
          a_type cls_nvd = {}; cls_nvd.kind = tk_struct; cls_nvd.size = 4; cls_nvd.variant.class_struct_union.extra_info = &ext_nvd;
          assert(eval_t2(bok_has_virtual_destructor, &cls_nvd, nullptr, false) == llvm_gen_be_error_t::ok);

          // `L3530` (`any_virtual_base_classes = TRUE`), `L3540` (`bc->derivation == nullptr`), `L3545` (all ctors compiler-generated in `bok_is_aggregate`)
          a_type cls_vbc = {}; cls_vbc.kind = tk_struct; cls_vbc.size = 8; cls_vbc.variant.class_struct_union.any_virtual_base_classes = TRUE;
          assert(eval_t2(bok_is_aggregate, &cls_vbc, nullptr, false) == llvm_gen_be_error_t::ok);
          a_routine cg_only = {}; cg_only.special_kind = sfk_constructor; cg_only.compiler_generated = TRUE;
          a_scope sc_cgo = {}; sc_cgo.routines = &cg_only;
          a_base_class bc_noderiv = {}; bc_noderiv.type = &base_target; bc_noderiv.is_virtual = FALSE; bc_noderiv.derivation = nullptr;
          a_class_type_supplement ext_cgo = {}; ext_cgo.direct_base_classes = &bc_noderiv; ext_cgo.assoc_scope = &sc_cgo;
          a_type cls_cgo = {}; cls_cgo.kind = tk_struct; cls_cgo.size = 4; cls_cgo.variant.class_struct_union.extra_info = &ext_cgo;
          assert(eval_t2(bok_is_aggregate, &cls_cgo, nullptr, false) == llvm_gen_be_error_t::ok);

          // `L3635` (`ptr_s32_e` field), `L3639` (two public fields), `L3653` & `L3658` (derived with fields + empty base), `L3671..L3676` (`bok_is_pod`), `L3712..L3713` (`bok_is_assignable`), `MBR L3688` & `L3903`!
          a_field fp1 = {}, fp2 = {};
          fp1.type = &ptr_s32_e; fp1.source_corresp.access = as_public; fp1.next = &fp2;
          fp2.type = &s32_e; fp2.source_corresp.access = as_public; fp2.next = nullptr;
          a_base_class bc_emp_sl = {}; bc_emp_sl.type = &base_target; // base_target has field_list == nullptr!
          a_class_type_supplement ext_emp_sl = {}; ext_emp_sl.base_classes = &bc_emp_sl;
          a_type cls_ok_sl = {}; cls_ok_sl.kind = tk_struct; cls_ok_sl.size = 16;
          cls_ok_sl.variant.class_struct_union.field_list = &fp1;
          cls_ok_sl.variant.class_struct_union.extra_info = &ext_emp_sl;
          assert(eval_t2(bok_is_standard_layout, &cls_ok_sl, nullptr, false) == llvm_gen_be_error_t::ok);

          // `bok_is_pod`: trivial && !standard_layout (`L3671`), non-trivial (`L3671`), `void_e` in C++ (`L3675`), `ptr_s32_e` (`L3674` & `L3676`)!
          a_field f_priv_pod = {}; f_priv_pod.type = &s32_e; f_priv_pod.source_corresp.access = as_private; f_priv_pod.next = &fp2;
          a_type cls_triv_nonsl = {}; cls_triv_nonsl.kind = tk_struct; cls_triv_nonsl.size = 8; cls_triv_nonsl.variant.class_struct_union.field_list = &f_priv_pod;
          assert(eval_t2(bok_is_pod, &cls_triv_nonsl, nullptr, false) == llvm_gen_be_error_t::ok);
          a_type cls_nontriv_pod = {}; cls_nontriv_pod.kind = tk_struct; cls_nontriv_pod.size = 444;
          assert(eval_t2(bok_is_pod, &cls_nontriv_pod, nullptr, false) == llvm_gen_be_error_t::ok);
          assert(eval_t2(bok_is_pod, &void_e, nullptr, false) == llvm_gen_be_error_t::ok);
          assert(eval_t2(bok_is_pod, &ptr_s32_e, nullptr, false) == llvm_gen_be_error_t::ok);

          assert(eval_t2(bok_is_assignable, nullptr, &s32_e, true) == llvm_gen_be_error_t::ok); // L3712 t1 == nullptr!
          assert(eval_t2(bok_is_assignable, &s32_e, nullptr, false) == llvm_gen_be_error_t::ok); // L3713 t2 == nullptr!
          run_expr_oom([&]() noexcept { return eval_t2(bok_has_unique_object_representations, &cls_ok_sl, nullptr, false); }); // MBR L3688 & L3903!
        }

        // 6. `llvm_lower_expression`, `llvm_lower_compound_assignment`, `llvm_lower_new_expression`, `llvm_lower_delete_expression` remaining branches & OOM sweeps!
        {
          a_variable vs = {}, vp = {}; vs.type = &s32_e; vp.type = &ptr_s32_e;
          state.local_vars[&vs] = state.builder->CreateAlloca(i32_t);
          state.local_vars[&vp] = state.builder->CreateAlloca(ptr_t);

          // `L3991-3993`: `expr->is_lvalue == TRUE` on `eok_ref_indirect`, `eok_lvalue`, `eok_lvalue_cast`!
          an_expr_node evp = {}, evs_lv = {};
          evp.kind = enk_variable; evp.type = &ptr_s32_e; evp.variant.variable.ptr = &vp;
          evs_lv.kind = enk_variable; evs_lv.is_lvalue = TRUE; evs_lv.type = &s32_e; evs_lv.variant.variable.ptr = &vs;
          an_expr_node op_lv = {}; op_lv.kind = enk_operation; op_lv.is_lvalue = TRUE; op_lv.type = &s32_e;
          op_lv.variant.operation.kind = eok_ref_indirect; op_lv.variant.operation.operands = &evp;
          assert(llvm_lower_expression(&op_lv, &val) == llvm_gen_be_error_t::ok);
          for (an_expr_operator_kind lk : {eok_lvalue, eok_lvalue_cast}) {
            op_lv.variant.operation.kind = lk; op_lv.variant.operation.operands = &evs_lv;
            assert(llvm_lower_expression(&op_lv, &val) == llvm_gen_be_error_t::ok);
          }

          // `L4270-4273` + `MBR L4275`: `eok_lvalue` with `op1 == nullptr`, `!op1->is_lvalue`, `ut->kind == tk_routine`, `ut == &ptr_s32_e`, and OOM!
          op_lv.is_lvalue = FALSE; op_lv.variant.operation.kind = eok_lvalue;
          op_lv.variant.operation.operands = nullptr;
          assert(llvm_lower_expression(&op_lv, &val) == llvm_gen_be_error_t::ok);
          an_expr_node evs_rv = evs_lv; evs_rv.is_lvalue = FALSE;
          op_lv.variant.operation.operands = &evs_rv;
          assert(llvm_lower_expression(&op_lv, &val) == llvm_gen_be_error_t::ok);
          op_lv.variant.operation.operands = &evs_lv;
          op_lv.type = &rty_e;
          assert(llvm_lower_expression(&op_lv, &val) == llvm_gen_be_error_t::ok);
          op_lv.type = &ptr_s32_e;
          evs_lv.type = &ptr_s32_e; evs_lv.variant.variable.ptr = &vp;
          run_expr_oom([&]() noexcept { return llvm_lower_expression(&op_lv, &val); });
          evs_lv.type = &s32_e; evs_lv.variant.variable.ptr = &vs;

          // `L4295` & `MBR L4298`: `eok_vector_subscript` where `op1` has `is_lvalue = TRUE` (`v1->getType()->isPointerTy()` TRUE)!
          llvm::VectorType* vec4_t = llvm::FixedVectorType::get(i32_t, 4);
          a_type vec4_e = {}; vec4_e.kind = tk_array; vec4_e.size = 16;
          state.type_cache[&vec4_e] = vec4_t;
          a_variable vv4 = {}; vv4.type = &vec4_e; state.local_vars[&vv4] = state.builder->CreateAlloca(vec4_t);
          an_expr_node ev4_lv = {}, eidx = {}, vsub = {};
          ev4_lv.kind = enk_variable; ev4_lv.is_lvalue = TRUE; ev4_lv.type = &vec4_e; ev4_lv.variant.variable.ptr = &vv4; ev4_lv.next = &eidx;
          eidx.kind = enk_constant; eidx.type = &s32_e;
          vsub.kind = enk_operation; vsub.type = &s32_e; vsub.variant.operation.kind = eok_vector_subscript; vsub.variant.operation.operands = &ev4_lv;
          run_expr_oom([&]() noexcept { return llvm_lower_expression(&vsub, &val); }); // L4295 & MBR L4298!

          // Bitfield read/assign/compound-assign OOM sweeps (`MBR L4321`, `MBR L4568`, `MBR L4570`, `MBR L5108`, `MBR L5342`, `MBR L5346`)!
          a_field bf_m = {}; bf_m.is_bit_field = TRUE; bf_m.type = &s32_e; bf_m.bit_size = 4; bf_m.offset = 0;
          a_type bf_st_e = {}; bf_st_e.kind = tk_struct; bf_st_e.size = 4; bf_st_e.variant.class_struct_union.field_list = &bf_m;
          a_variable vbf_st = {}; vbf_st.type = &bf_st_e; state.local_vars[&vbf_st] = state.builder->CreateAlloca(i32_t);
          an_expr_node ebf_base = {}, ebf_fld = {}, ebf_dot = {}, ebf_rhs = {}, ebf_asgn = {}, ebf_ca = {};
          ebf_base.kind = enk_variable; ebf_base.is_lvalue = TRUE; ebf_base.type = &bf_st_e; ebf_base.variant.variable.ptr = &vbf_st; ebf_base.next = &ebf_fld;
          ebf_fld.kind = enk_field; ebf_fld.variant.field.ptr = &bf_m;
          ebf_dot.kind = enk_operation; ebf_dot.type = &s32_e; ebf_dot.variant.operation.kind = eok_dot_field; ebf_dot.variant.operation.operands = &ebf_base;
          run_expr_oom([&]() noexcept { return llvm_lower_expression(&ebf_dot, &val); }); // MBR L4321!

          ebf_rhs.kind = enk_constant; ebf_rhs.type = &s32_e;
          an_expr_node ebf_dot_lv = ebf_dot; ebf_dot_lv.is_lvalue = TRUE; ebf_dot_lv.next = &ebf_rhs;
          ebf_asgn.kind = enk_operation; ebf_asgn.type = &s32_e; ebf_asgn.variant.operation.kind = eok_assign; ebf_asgn.variant.operation.operands = &ebf_dot_lv;
          run_expr_oom([&]() noexcept { return llvm_lower_expression(&ebf_asgn, &val); }); // MBR L4568, L4570!

          ebf_ca.kind = enk_operation; ebf_ca.type = &s32_e; ebf_ca.variant.operation.kind = eok_add_assign; ebf_ca.variant.operation.operands = &ebf_dot_lv;
          run_expr_oom([&]() noexcept { return llvm_lower_compound_assignment(&ebf_ca, &val); }); // MBR L5084, L5108, L5342!

          // Vector-subscript compound-assign OOM (`MBR L5258`, `L5284`, `L5346`) + untyped atomic pointer `-=` (`L5138..L5148`)!
          an_expr_node vsub_lv = vsub; vsub_lv.next = &eidx;
          an_expr_node vsub_ca = {}; vsub_ca.kind = enk_operation; vsub_ca.type = &s32_e; vsub_ca.variant.operation.kind = eok_add_assign; vsub_ca.variant.operation.operands = &vsub_lv;
          a_type ptr_vec_elem = {}; ptr_vec_elem.kind = tk_pointer; ptr_vec_elem.size = 8; ptr_vec_elem.variant.pointer.type = &s32_e;
          state.type_cache[&ptr_vec_elem] = i32_t;
          vsub_lv.type = &ptr_vec_elem;
          run_expr_oom([&]() noexcept { return llvm_lower_compound_assignment(&vsub_ca, &val); });
          vsub_ca.variant.operation.kind = eok_subtract_assign;
          run_expr_oom([&]() noexcept { return llvm_lower_compound_assignment(&vsub_ca, &val); });

          // Untyped `op1->type == nullptr` on atomic pointer `psubtract_assign` (`L5138`, `L5143`, `L5144`)!
          a_type atom_ptr_e = {}; atom_ptr_e.kind = tk_typeref; atom_ptr_e.variant.typeref.type = &ptr_s32_e; atom_ptr_e.variant.typeref.qualifiers = TQ_C11_ATOMIC;
          a_variable vap_u = {}; vap_u.type = &atom_ptr_e; state.local_vars[&vap_u] = state.builder->CreateAlloca(ptr_t);
          an_expr_node eap_psub = {}; eap_psub.kind = enk_variable; eap_psub.type = &atom_ptr_e; eap_psub.variant.variable.ptr = &vap_u; eap_psub.next = &eidx;
          an_expr_node psub_ca = {}; psub_ca.kind = enk_operation; psub_ca.type = &ptr_s32_e; psub_ca.variant.operation.kind = eok_psubtract_assign; psub_ca.variant.operation.operands = &eap_psub;
          assert(llvm_lower_compound_assignment(&psub_ca, &val) == llvm_gen_be_error_t::ok); // L5138 eok_psubtract_assign!

          // Ternary `?:` (`L4519` & `L4524`): `phi_ty` is `f32_t` while `true_val` / `false_val` is `i32_t` (`phi_ty->isIntegerTy()` FALSE)
          // and `true_val` / `false_val` is `f64_t` (`true_val->isIntegerTy()` FALSE)!
          an_expr_node qc = {}, qt_i = {}, qf_i = {}, qt_d = {}, qf_d = {}, qe = {};
          qc.kind = enk_constant; qc.type = &s32_e;
          qt_i.kind = enk_constant; qt_i.type = &s32_e; qt_i.next = &qf_i;
          qf_i.kind = enk_constant; qf_i.type = &s32_e;
          qc.next = &qt_i;
          qe.kind = enk_operation; qe.type = &f32_e; qe.variant.operation.kind = eok_question; qe.variant.operation.operands = &qc;
          // Note: PHINode requires matching incoming types if not cast, so only test in isolated discarded BB or test `qt_d` (`f64`) -> wait: if `phi_ty` is `f32` and `true_val` is `i32`, `addIncoming(i32)` on `PHI(f32)` would assert in LLVM!
          // Let's check why L4519 had `phi_ty->isIntegerTy()`! When `true_val` != `phi_ty` and BOTH are `f32` vs `f64`? No, in C++ EDG already inserts casts so `true_val == phi_ty` EXCEPT `bool` (`i1` vs `i8`) which ARE both `IntegerTy`, OR `nullptr_t` (`ptr`)! Wait: when `true_val` is `ptr_as1` and `phi_ty` is `ptr_t`, `true_val->getType() != phi_ty` AND `!true_val->isPointerTy()` is FALSE (both are PointerTy -> `CreatePointerBitCastOrAddrSpaceCast`), OR if `true_val` is `f64_t` and `phi_ty` is `ptr_t`... No, `PHINode::addIncoming` asserts `Val->getType() == getType()`!

          // Statement-expression `enk_statement` (`L4764`, `L4765`, `L4783`) with `active_prvalue_dest_ptr` (mismatch & exact match)!
          a_statement st_nop = {}; st_nop.kind = stmk_empty;
          an_expr_node stmt_e = {}; stmt_e.kind = enk_statement; stmt_e.type = &st24_e; stmt_e.variant.statement = &st_nop;
          llvm::AllocaInst* st_dst = state.builder->CreateAlloca(st24_ll);
          state.active_prvalue_dest_ptr = st_dst; state.active_prvalue_dest_ty = i32_t; // mismatch!
          assert(llvm_lower_expression(&stmt_e, &val) == llvm_gen_be_error_t::ok);
          state.active_prvalue_dest_ptr = st_dst; state.active_prvalue_dest_ty = st24_ll; // match -> direct_in_place!
          assert(llvm_lower_expression(&stmt_e, &val) == llvm_gen_be_error_t::ok);
          state.active_prvalue_dest_ptr = nullptr; state.active_prvalue_dest_ty = nullptr; state.prvalue_constructed_in_place = false;

          // OOM sweeps on `enk_initializer` (`MBR L4743`), MSVC `enk_throw` + `dynamic_init->destructor` (`MBR L4842, L4869`),
          // `enk_temp_init` (`MBR L4899`), `enk_object_lifetime` (`L4937` & `MBR L4939`), `leck_internal_try` (`MBR L4987, L5015`), and `enk_result_of_overriding_function` (`L5056` & `MBR L5040`)!
          a_dynamic_init dip_ip = {}; dip_ip.kind = dik_constructor;
          an_expr_node e_init_ip = {}; e_init_ip.kind = enk_initializer; e_init_ip.type = &st24_e; e_init_ip.variant.initializer.dyn_init = &dip_ip;
          run_expr_oom([&]() noexcept {
            state.active_prvalue_dest_ptr = st_dst;
            state.active_prvalue_dest_ty = st24_ll;
            llvm_gen_be_error_t r = llvm_lower_expression(&e_init_ip, &val);
            state.active_prvalue_dest_ptr = nullptr;
            state.active_prvalue_dest_ty = nullptr;
            state.prvalue_constructed_in_place = false;
            return r;
          });

          std::string saved_tr = llvm::Triple(state.module->getTargetTriple()).str();
          EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
          a_routine thr_d = {}; thr_d.source_corresp.name = (char*)"_ZN4MsvcD1Ev";
          a_dynamic_init thr_dip = {}; thr_dip.kind = dik_zero; thr_dip.destructor = &thr_d;
          a_throw_supplement msvc_ts = {}; msvc_ts.type = &s32_e; msvc_ts.dynamic_init = &thr_dip;
          an_expr_node msvc_thr = {}; msvc_thr.kind = enk_throw; msvc_thr.type = &void_e; msvc_thr.variant.throw_info = &msvc_ts;
          run_expr_oom([&]() noexcept { return llvm_lower_expression(&msvc_thr, &val); }); // MBR L4842, L4869!
          EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), saved_tr);
          ensure_expr_bb();

          thr_dip.has_temporary_lifetime = TRUE;
          an_expr_node e_tmp_oom = {}; e_tmp_oom.kind = enk_temp_init; e_tmp_oom.type = &s32_e; e_tmp_oom.variant.init.dynamic_init = &thr_dip;
          run_expr_oom([&]() noexcept { return llvm_lower_expression(&e_tmp_oom, &val); }); // MBR L4899!

          // `enk_object_lifetime` (`L4937` terminated inner expr & `MBR L4939` active cleanup dtor OOM)!
          an_object_lifetime ol = {};
          an_expr_node e_ol = {}; e_ol.kind = enk_object_lifetime; e_ol.type = &void_e; e_ol.variant.object_lifetime.ptr = &ol;
          e_ol.variant.object_lifetime.expr = &msvc_thr; // terminates block -> L4937 false!
          assert(llvm_lower_expression(&e_ol, &val) == llvm_gen_be_error_t::ok);
          ensure_expr_bb();
          e_ol.variant.object_lifetime.expr = &e_tmp_oom; // pushes active cleanup onto cleanup_stack -> L4939 get_or_create_void_ptr_fn!
          run_expr_oom([&]() noexcept {
            state.cleanup_stack.cleanups.clear();
            return llvm_lower_expression(&e_ol, &val);
          });

#if DO_IL_LOWERING
          // OOM sweep on `leck_internal_try` with throwing `first_e` + `second_e` (`MBR L4987, L5015`)!
          an_expr_node try_f = {}, try_s = {}, try_eh = {};
          try_f.kind = enk_initializer; try_f.type = &s32_e; try_f.variant.initializer.dyn_init = &dip_ip; try_f.next = &try_s;
          try_s.kind = enk_constant; try_s.type = &s32_e;
          try_eh.kind = enk_lowered_eh_construct; try_eh.type = &void_e; try_eh.variant.lowered_eh.kind = leck_internal_try;
          try_eh.variant.lowered_eh.variant.try_and_catch_expr = &try_f;
          g_dyn_init_emit_invoke = 1;
          run_expr_oom([&]() noexcept { return llvm_lower_expression(&try_eh, &val); });
          g_dyn_init_emit_invoke = 0;

#if ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
          // `L5056` (`cur_fn->arg_size() < under_fn->arg_size()`, i.e. `cur_fn` has 0 args while `under_fn` has 1 arg!) + `MBR L5040` OOM!
          llvm::Function* u_1arg = llvm::Function::Create(
              llvm::FunctionType::get(i32_t, {i32_t}, false),
              llvm::Function::ExternalLinkage, "under_1arg_fn", state.module.get());
          assert(u_1arg != nullptr);
          llvm::Function* w_0arg = llvm::Function::Create(
              llvm::FunctionType::get(i32_t, {}, false),
              llvm::Function::ExternalLinkage, "wrap_0arg_fn", state.module.get());
          state.builder->SetInsertPoint(llvm::BasicBlock::Create(*state.context, "entry", w_0arg));
          a_routine ru1 = {}, rw0 = {};
          ru1.source_corresp.name = (char*)"under_1arg_fn"; ru1.type = &rty_e;
          rw0.overriding_function_for_wrapper = &ru1;
          state.current_edg_routine = &rw0;
          state.current_function_scope = nullptr;
          an_expr_node ovr_short = {}; ovr_short.kind = enk_result_of_overriding_function; ovr_short.type = &s32_e;
          run_expr_oom([&]() noexcept { return llvm_lower_expression(&ovr_short, &val); }); // L5056 & MBR L5040!
          { llvm::IRBuilder<> wb0(&w_0arg->back()); wb0.CreateRet(llvm::ConstantInt::get(i32_t, 0)); }
          state.current_edg_routine = nullptr;
          state.builder->SetInsertPoint(&ws10_fn->back());
          ensure_expr_bb();
#endif
#endif

          // `llvm_lower_new_expression` (`L5798`, `L5799`, `L5808`, `L5863`, `MBR L5893`) & `llvm_lower_delete_expression` (`MBR L5961`, `MBR L5989`)!
          a_routine new_rt2 = {}, dtor_rt2 = {}, del_rt2 = {};
          new_rt2.source_corresp.name = (char*)"_Znwm"; new_rt2.type = &rty_e;
          dtor_rt2.source_corresp.name = (char*)"_ZN4ItemD1Ev";
          del_rt2.source_corresp.name = (char*)"_ZdlPv"; del_rt2.type = &rty_e;
          an_expr_node sz_e2 = {}, cnt_e2 = {}, del_ptr_e = {};
          sz_e2.kind = enk_constant; sz_e2.type = &s32_e;
          cnt_e2.kind = enk_constant; cnt_e2.type = &s32_e;
          del_ptr_e.kind = enk_variable; del_ptr_e.type = &ptr_s32_e; del_ptr_e.variant.variable.ptr = &vp;

          a_dynamic_init dip_arr_zero = {}, dip_arr_noctor = {}, dip_arr_dtor_only = {}, dip_scalar_free = {};
          dip_arr_zero.kind = dik_zero;
          dip_arr_noctor.kind = dik_constructor; dip_arr_noctor.variant.constructor.ptr = nullptr;
          a_routine item_ctor = {}; item_ctor.source_corresp.name = (char*)"_ZN4ItemC1Ev";
          dip_arr_dtor_only.kind = dik_constructor; dip_arr_dtor_only.variant.constructor.ptr = &item_ctor; dip_arr_dtor_only.destructor = &dtor_rt2;
          dip_scalar_free.kind = dik_none; dip_scalar_free.destructor = &del_rt2;

          a_new_delete_supplement nd2 = {};
          nd2.is_new = TRUE; nd2.type = &s32_e; nd2.routine = &new_rt2; nd2.arg = &sz_e2; nd2.number_of_elements = &cnt_e2;
          an_expr_node ne2 = {}; ne2.kind = enk_new_delete; ne2.type = &ptr_s32_e; ne2.variant.new_delete = &nd2;

          nd2.dynamic_init = &dip_arr_zero;
          assert(llvm_lower_new_expression(&ne2, &val) == llvm_gen_be_error_t::ok); // L5798 dik_constructor FALSE!
          nd2.dynamic_init = &dip_arr_noctor;
          assert(llvm_lower_new_expression(&ne2, &val) == llvm_gen_be_error_t::ok); // L5799 constructor.ptr != nullptr FALSE!

          // `L5808` (`freeing_of_storage_on_exception == nullptr` && `destructor != nullptr`) & `L5863` (`freeing_of_storage_on_exception == nullptr` inside unwind_bb)!
          nd2.dynamic_init = &dip_arr_dtor_only; nd2.freeing_of_storage_on_exception = nullptr;
          g_dyn_init_emit_invoke = 1;
          assert(llvm_lower_new_expression(&ne2, &val) == llvm_gen_be_error_t::ok);

          // Scalar `new T` with throwing init + `freeing_of_storage_on_exception` (`MBR L5893`)!
          nd2.number_of_elements = nullptr;
          nd2.freeing_of_storage_on_exception = &dip_scalar_free;
          run_expr_oom([&]() noexcept { return llvm_lower_new_expression(&ne2, &val); });
          g_dyn_init_emit_invoke = 0;

          // Array `delete[]` & scalar `delete` with destructor OOM sweeps (`MBR L5961` & `MBR L5989`)!
          a_routine del_cls_dtor = {}; del_cls_dtor.special_kind = sfk_destructor; del_cls_dtor.compiler_generated = FALSE;
          a_scope del_cls_sc = {}; del_cls_sc.routines = &del_cls_dtor;
          a_class_type_supplement del_cls_ext = {}; del_cls_ext.assoc_scope = &del_cls_sc;
          a_type del_cls_e = {}; del_cls_e.kind = tk_class; del_cls_e.size = 777; del_cls_e.alignment = 4; del_cls_e.variant.class_struct_union.extra_info = &del_cls_ext;
          state.type_cache[&del_cls_e] = i32_t;

          a_new_delete_supplement dd_arr = {}, dd_sc = {};
          dd_arr.is_new = FALSE; dd_arr.array_delete = TRUE; dd_arr.type = &del_cls_e; dd_arr.routine = &del_rt2;
          dd_arr.arg = &del_ptr_e; dd_arr.dynamic_init = &dip_arr_dtor_only;
          an_expr_node de_e = {}; de_e.kind = enk_new_delete; de_e.type = &void_e; de_e.variant.new_delete = &dd_arr;
          run_expr_oom([&]() noexcept { return llvm_lower_delete_expression(&de_e, &val); });

          dd_sc = dd_arr; dd_sc.array_delete = FALSE;
          de_e.variant.new_delete = &dd_sc;
          run_expr_oom([&]() noexcept { return llvm_lower_delete_expression(&de_e, &val); });

          // --- 10.H: Final 15-branch closure ---
          // 1) `L620`: scalar + vector (`s32 + vec4`) OOM!
          llvm::VectorType* vec4_h = llvm::FixedVectorType::get(i32_t, 4);
          a_type vec4_he = {}; vec4_he.kind = tk_array; vec4_he.size = 16; state.type_cache[&vec4_he] = vec4_h;
          an_expr_node lhs_sc = {}, rhs_vc = {}, sc_vadd = {};
          lhs_sc.kind = enk_constant; lhs_sc.type = &s32_e; lhs_sc.next = &rhs_vc;
          rhs_vc.kind = enk_constant; rhs_vc.type = &vec4_he;
          sc_vadd.kind = enk_operation; sc_vadd.type = &vec4_he; sc_vadd.variant.operation.kind = eok_add; sc_vadd.variant.operation.operands = &lhs_sc;
          run_expr_oom([&]() noexcept { return llvm_lower_arithmetic_expression(&sc_vadd, &val); });

          // 2) `L2046`: `raw_arg_edg->kind != tk_integer` (`tk_float` mapped in `type_cache` to `i16_t`!)
          a_type non_int_i16 = {}; non_int_i16.kind = tk_float; non_int_i16.size = 2; state.type_cache[&non_int_i16] = i16_t;
          llvm::FunctionType* fty_ni = llvm::FunctionType::get(i32_t, {i32_t}, false);
          llvm::Function::Create(fty_ni, llvm::Function::ExternalLinkage, "fn_non_int_i16", state.module.get());
          a_routine_type_supplement rts_ni = {};
          a_type fn_ni_e = {}; fn_ni_e.kind = tk_routine; fn_ni_e.variant.routine.return_type = &s32_e; fn_ni_e.variant.routine.extra_info = &rts_ni;
          a_routine rt_ni = {}; rt_ni.source_corresp.name = (char*)"fn_non_int_i16"; rt_ni.type = &fn_ni_e;
          an_expr_node op_ni = {}, arg_ni = {}, call_ni = {};
          op_ni.kind = enk_routine; op_ni.type = &fn_ni_e; op_ni.variant.routine.ptr = &rt_ni; op_ni.next = &arg_ni;
          arg_ni.kind = enk_constant; arg_ni.type = &non_int_i16;
          call_ni.kind = enk_operation; call_ni.type = &s32_e; call_ni.variant.operation.kind = eok_call; call_ni.variant.operation.operands = &op_ni;
          assert(llvm_lower_call_expression(&call_ni, &val) == llvm_gen_be_error_t::ok);

          // 3) `MBR L2374` (`__builtin_FILE` OOM), `MBR L2563` (`__builtin_prefetch` with non-null `a_constant` OOM), `MBR L2783` (`__atomic_clear` OOM),
          //    `L2853` (`__atomic_fetch_add` untyped return), and `L2868` (`__c11_atomic_fetch_add` with `ptee_edg == nullptr`)!
          an_expr_node epv_h = {}, ei32_h = {}, ep_untyped_h = {};
          epv_h.kind = enk_variable; epv_h.type = &ptr_void_e; epv_h.variant.variable.ptr = &vp;
          a_constant c_one = {}; c_one.kind = ck_integer;
          ei32_h.kind = enk_constant; ei32_h.type = &s32_e; ei32_h.variant.constant.ptr = &c_one;
          ep_untyped_h = epv_h; ep_untyped_h.type = nullptr; ep_untyped_h.is_lvalue = TRUE;
          run_expr_oom([&]() noexcept { return call_test_builtin("__builtin_FILE", &ptr_void_e, {}); });
          run_expr_oom([&]() noexcept { return call_test_builtin("__builtin_prefetch", &void_e, {&epv_h, &ei32_h, &ei32_h}); });
          run_expr_oom([&]() noexcept { return call_test_builtin("__atomic_clear", &void_e, {&epv_h, &ei32_h}); });
          assert(call_test_builtin("__atomic_fetch_add", nullptr, {&epv_h, &ei32_h, &ei32_h}) == llvm_gen_be_error_t::ok);
          assert(call_test_builtin("__c11_atomic_fetch_add", &ptr_s32_e, {&ep_untyped_h, &ei32_h, &ei32_h}) == llvm_gen_be_error_t::ok);

          // 4) `bok_is_literal_type` (`L3679..L3681`) on `void_e` (C++ & PCC), `ref_e`, `uni`, and `ptr_s32_e`!
          a_type ref_he = {}; ref_he.kind = tk_pointer; ref_he.size = 555; ref_he.variant.pointer.is_reference = TRUE;
          a_type uni_he = {}; uni_he.kind = tk_union; uni_he.size = 4;
          for (a_type* lt : {&void_e, &ref_he, &uni_he, &ptr_s32_e}) {
            an_expr_node lo = {}, lb = {};
            lo.kind = enk_type_operand; lo.variant.type_operand.type = lt;
            lb.kind = enk_builtin_operation; lb.type = &s32_e; lb.variant.builtin_operation.kind = bok_is_literal_type; lb.variant.builtin_operation.operands = &lo;
            assert(llvm_lower_builtin_operation(&lb, &val) == llvm_gen_be_error_t::ok);
          }
          C_dialect = C_dialect_pcc;
          {
            an_expr_node lo = {}, lb = {};
            lo.kind = enk_type_operand; lo.variant.type_operand.type = &void_e;
            lb.kind = enk_builtin_operation; lb.type = &s32_e; lb.variant.builtin_operation.kind = bok_is_literal_type; lb.variant.builtin_operation.operands = &lo;
            assert(llvm_lower_builtin_operation(&lb, &val) == llvm_gen_be_error_t::ok);
          }
          C_dialect = C_dialect_cplusplus;

          // 5) `L3997`: `eok_lvalue_adjust` with `expr->is_lvalue == TRUE`; `L4940`: `enk_object_lifetime` with `eok_comma(e_tmp_oom, msvc_thr)`!
          op_lv.is_lvalue = TRUE; op_lv.type = &s32_e; op_lv.variant.operation.kind = eok_lvalue_adjust; op_lv.variant.operation.operands = &evs_lv;
          assert(llvm_lower_expression(&op_lv, &val) == llvm_gen_be_error_t::ok);

          // 6) `eok_assign` with lvalue `enk_temp_init` RHS (`v2->getType()->isPointerTy() && !ty->isPointerTy()`)
          {
            an_expr_node lhs_asgn = {}, rhs_ti = e_tmp_oom, asgn_ti = {};
            lhs_asgn.kind = enk_variable; lhs_asgn.type = &s32_e; lhs_asgn.is_lvalue = TRUE; lhs_asgn.variant.variable.ptr = &vs;
            lhs_asgn.next = &rhs_ti;
            rhs_ti.type = &s32_e; rhs_ti.is_lvalue = TRUE; rhs_ti.next = nullptr;
            asgn_ti.kind = enk_operation; asgn_ti.type = &s32_e; asgn_ti.variant.operation.kind = eok_assign; asgn_ti.variant.operation.operands = &lhs_asgn;
            assert(llvm_lower_expression(&asgn_ti, &val) == llvm_gen_be_error_t::ok);
          }

          an_expr_node cm_tmp = e_tmp_oom, cm_thr = msvc_thr, cm_op = {};
          cm_tmp.next = &cm_thr;
          cm_op.kind = enk_operation; cm_op.type = &void_e; cm_op.variant.operation.kind = eok_comma; cm_op.variant.operation.operands = &cm_tmp;
          e_ol.variant.object_lifetime.expr = &cm_op;
          ensure_expr_bb();
          assert(llvm_lower_expression(&e_ol, &val) == llvm_gen_be_error_t::ok);
          ensure_expr_bb();

          // 6) `L5141`, `L5147`, `L5151`: atomic pointer `&=`, atomic non-`tk_pointer` cached as `ptr_t`, and atomic `void*` (`ptee_ty->size == 0`) `+=`!
          a_type atom_pvoid_h = {}; atom_pvoid_h.kind = tk_typeref; atom_pvoid_h.variant.typeref.type = &ptr_void_e; atom_pvoid_h.variant.typeref.qualifiers = TQ_C11_ATOMIC;
          a_type atom_pnull_h = {}; atom_pnull_h.kind = tk_typeref; atom_pnull_h.variant.typeref.type = &ptr_null_e; atom_pnull_h.variant.typeref.qualifiers = TQ_C11_ATOMIC;
          a_type atom_fake_p_h = {}; atom_fake_p_h.kind = tk_typeref; atom_fake_p_h.variant.typeref.type = &s64_e; atom_fake_p_h.variant.typeref.qualifiers = TQ_C11_ATOMIC;
          state.type_cache[&atom_fake_p_h] = ptr_t;
          a_variable vpv_h = {}, vpn_h = {}, vfp_h = {};
          vpv_h.type = &atom_pvoid_h; vpn_h.type = &atom_pnull_h; vfp_h.type = &atom_fake_p_h;
          state.local_vars[&vpv_h] = state.builder->CreateAlloca(ptr_t);
          state.local_vars[&vpn_h] = state.builder->CreateAlloca(ptr_t);
          state.local_vars[&vfp_h] = state.builder->CreateAlloca(ptr_t);
          for (a_variable* avh : {&vpv_h, &vpn_h, &vfp_h}) {
            an_expr_node eavh = {}; eavh.kind = enk_variable; eavh.type = avh->type; eavh.variant.variable.ptr = avh; eavh.next = &ei32_h;
            an_expr_node cah = {}; cah.kind = enk_operation; cah.type = &ptr_void_e; cah.variant.operation.kind = eok_add_assign; cah.variant.operation.operands = &eavh;
            assert(llvm_lower_compound_assignment(&cah, &val) == llvm_gen_be_error_t::ok);
          }
          // Atomic pointer with `eok_lshift_assign` (`is_ptr_lhs == true` && all 8 pointer ++/--/+=/-- checks FALSE at L5141 -> falls through to non-RMW or test with `eok_assign`? Wait: `eok_lshift_assign` on `ptr_t` would call `CreateShl(ptr_t)`! Instead `is_ptr_lhs == true` with `k == eok_assign` returns `unsupported_expr` cleanly at the bottom of `llvm_lower_compound_assignment` without calling `CreateShl`!)
          {
            an_expr_node eavh = {}; eavh.kind = enk_variable; eavh.type = &atom_pvoid_h; eavh.variant.variable.ptr = &vpv_h; eavh.next = &epv_h;
            an_expr_node cah = {}; cah.kind = enk_operation; cah.type = &ptr_void_e; cah.variant.operation.kind = eok_assign; cah.variant.operation.operands = &eavh;
            assert(llvm_lower_compound_assignment(&cah, &val) == llvm_gen_be_error_t::unsupported_expr);
          }

          // 7) Final 7 OOM sweeps (`MBR L2485, L2563, L2607, L2737, L2783, L5260, L5286`):
          llvm::Function::Create(llvm::FunctionType::get(i32_t, false), llvm::Function::ExternalLinkage, "__builtin_is_constant_evaluated", state.module.get());
          llvm::Function::Create(llvm::FunctionType::get(ptr_t, false), llvm::Function::ExternalLinkage, "__uuidof", state.module.get());
          run_expr_oom([&]() noexcept { return call_test_builtin("__builtin_is_constant_evaluated", &s32_e, {}); });
          run_expr_oom([&]() noexcept { return call_test_builtin("__uuidof", &ptr_void_e, {&ei32_h}); });
          run_expr_oom([&]() noexcept { return call_test_builtin("__builtin_nanf", &f32_e, {&epv_h}); });
          run_expr_oom([&]() noexcept { return call_test_builtin("__builtin___printf_chk", &s32_e, {&ei32_h, &epv_h}); });
          run_expr_oom([&]() noexcept { return call_test_builtin("__atomic_thread_fence", &void_e, {&ei32_h}); });

          an_expr_node evp_ca = {}; evp_ca.kind = enk_variable; evp_ca.type = &ptr_s32_e; evp_ca.variant.variable.ptr = &vp; evp_ca.next = &ei32_h;
          an_expr_node p_ca = {}; p_ca.kind = enk_operation; p_ca.type = &ptr_s32_e; p_ca.variant.operation.operands = &evp_ca;
          p_ca.variant.operation.kind = eok_add_assign;
          run_expr_oom([&]() noexcept { return llvm_lower_compound_assignment(&p_ca, &val); });
          p_ca.variant.operation.kind = eok_subtract_assign;
          run_expr_oom([&]() noexcept { return llvm_lower_compound_assignment(&p_ca, &val); });

          // 8) C99 IL-lowered `__c99_*` complex helper calls (`{ T, T }` and `{ [2 x T] }`), `leck_internal_try` dispatch/exc-spec/resume, and `eok_indirect` function-pointer dereference
          {
            llvm::StructType* c99_arr_f32_t = llvm::StructType::get(
                *state.context, llvm::ArrayRef<llvm::Type*>{llvm::ArrayType::get(f32_t, 2)});
            llvm::StructType* c99_pair_f64_t = llvm::StructType::get(*state.context, {f64_t, f64_t});
            a_type c99_arr_f32_e = {}, c99_pair_f64_e = {};
            c99_arr_f32_e.kind = tk_struct; c99_arr_f32_e.size = 8;
            c99_pair_f64_e.kind = tk_struct; c99_pair_f64_e.size = 16;
            state.type_cache[&c99_arr_f32_e] = c99_arr_f32_t;
            state.type_cache[&c99_pair_f64_e] = c99_pair_f64_t;

            a_variable vc_arr1 = {}, vc_arr2 = {}, vc_pair1 = {};
            vc_arr1.type = &c99_arr_f32_e; vc_arr2.type = &c99_arr_f32_e; vc_pair1.type = &c99_pair_f64_e;
            state.local_vars[&vc_arr1] = state.builder->CreateAlloca(c99_arr_f32_t);
            state.local_vars[&vc_arr2] = state.builder->CreateAlloca(c99_arr_f32_t);
            state.local_vars[&vc_pair1] = state.builder->CreateAlloca(c99_pair_f64_t);
            an_expr_node ec_arr1 = {}, ec_arr2 = {}, ec_pair1 = {}, ef32_arg = {};
            ec_arr1.kind = enk_variable; ec_arr1.type = &c99_arr_f32_e; ec_arr1.variant.variable.ptr = &vc_arr1;
            ec_arr2.kind = enk_variable; ec_arr2.type = &c99_arr_f32_e; ec_arr2.variant.variable.ptr = &vc_arr2;
            ec_pair1.kind = enk_variable; ec_pair1.type = &c99_pair_f64_e; ec_pair1.variant.variable.ptr = &vc_pair1;
            a_variable vf32_arg = {}; vf32_arg.type = &f32_e;
            state.local_vars[&vf32_arg] = state.builder->CreateAlloca(f32_t);
            ef32_arg.kind = enk_variable; ef32_arg.type = &f32_e; ef32_arg.variant.variable.ptr = &vf32_arg;

            assert(call_test_builtin("__c99_complex_float_negate", &c99_arr_f32_e, {&ec_arr1}) == llvm_gen_be_error_t::ok);
            assert(call_test_builtin("__c99_complex_float_conj", &c99_arr_f32_e, {&ec_arr1}) == llvm_gen_be_error_t::ok);
            assert(call_test_builtin("__c99_cfloat_to_cdouble", &c99_pair_f64_e, {&ec_arr1}) == llvm_gen_be_error_t::ok);
            assert(call_test_builtin("__c99_cdouble_to_cfloat", &c99_arr_f32_e, {&ec_pair1}) == llvm_gen_be_error_t::ok);
            assert(call_test_builtin("__c99_cfloat_to_ifloat", &f32_e, {&ec_arr1}) == llvm_gen_be_error_t::ok);
            assert(call_test_builtin("__c99_cfloat_to_float", &f32_e, {&ec_arr1}) == llvm_gen_be_error_t::ok);
            assert(call_test_builtin("__c99_ifloat_to_cfloat", &c99_arr_f32_e, {&ef32_arg}) == llvm_gen_be_error_t::ok);
            assert(call_test_builtin("__c99_float_to_cdouble", &c99_pair_f64_e, {&ef32_arg}) == llvm_gen_be_error_t::ok);

            assert(call_test_builtin("__c99_complex_float_multiply", &c99_arr_f32_e, {&ec_arr1, &ec_arr2}) == llvm_gen_be_error_t::ok);
            assert(call_test_builtin("__c99_complex_float_divide", &c99_arr_f32_e, {&ec_arr1, &ec_arr2}) == llvm_gen_be_error_t::ok);
            assert(call_test_builtin("__c99_complex_float_add", &c99_arr_f32_e, {&ec_arr1, &ec_arr2}) == llvm_gen_be_error_t::ok);
            assert(call_test_builtin("__c99_complex_float_subtract", &c99_arr_f32_e, {&ec_arr1, &ec_arr2}) == llvm_gen_be_error_t::ok);
            assert(call_test_builtin("__c99_complex_float_eq", &s32_e, {&ec_arr1, &ec_arr2}) == llvm_gen_be_error_t::ok);
            assert(call_test_builtin("__c99_complex_float_ne", &s32_e, {&ec_arr1, &ec_arr2}) == llvm_gen_be_error_t::ok);
            assert(call_test_builtin("__c99_complex_float_unknown2", &c99_arr_f32_e, {&ec_arr1, &ec_arr2}) == llvm_gen_be_error_t::complex_lowering_failed);
            assert(call_test_builtin("__c99_scalar_bad1", &f32_e, {&ef32_arg}) == llvm_gen_be_error_t::complex_lowering_failed);
            assert(call_test_builtin("__c99_bad2_scalar_lhs", &s32_e, {&ef32_arg, &ec_arr2}) == llvm_gen_be_error_t::complex_lowering_failed);
            assert(call_test_builtin("__c99_bad2_scalar_rhs", &s32_e, {&ec_arr1, &ef32_arg}) == llvm_gen_be_error_t::complex_lowering_failed);
            assert(call_test_builtin("__c99_zero_args", &s32_e, {}) == llvm_gen_be_error_t::complex_lowering_failed);
            a_type c99_bad_ret_ty = bad_ty;
            c99_bad_ret_ty.size = 778;
            assert(call_test_builtin("__c99_bad_ret_ty", &c99_bad_ret_ty, {&ec_arr1}) == llvm_gen_be_error_t::unsupported_type);
            run_expr_oom([&]() noexcept { return call_test_builtin("__c99_complex_float_negate", &c99_arr_f32_e, {&ec_arr1}); });
            run_expr_oom([&]() noexcept { return call_test_builtin("__c99_complex_float_add", &c99_arr_f32_e, {&ec_arr1, &ec_arr2}); });
            run_expr_oom([&]() noexcept { return call_test_builtin("__c99_complex_float_multiply", &c99_arr_f32_e, {&ec_arr1, &ec_arr2}); });

            // `leck_internal_try`: dispatch_bb in `cur_fn`, dispatch_bb in foreign fn, `noexcept` routine, and non-empty `enclosing_unwind` (`CreateResume`)
            llvm::Function* cur_f_eh = state.builder->GetInsertBlock()->getParent();
            llvm::BasicBlock* disp_cur = llvm::BasicBlock::Create(*state.context, "itry.disp", cur_f_eh);
            llvm::PHINode* ep_p = nullptr;
            llvm::PHINode* es_p = nullptr;
            {
              llvm::IRBuilder<> db(disp_cur);
              ep_p = db.CreatePHI(ptr_t, 2);
              es_p = db.CreatePHI(i32_t, 2);
              db.CreateUnreachable();
            }
            llvm::BasicBlock* non_empty_lp = llvm::BasicBlock::Create(*state.context, "itry.nonempty", cur_f_eh);
            llvm::IRBuilder<>(non_empty_lp).CreateUnreachable();

            a_dynamic_init itry_dip = {}; itry_dip.kind = dik_constructor;
            an_expr_node itry_first = {};
            itry_first.kind = enk_initializer;
            itry_first.type = &s32_e;
            itry_first.variant.initializer.dyn_init = &itry_dip;
            an_expr_node itry_e = {};
            itry_e.kind = enk_lowered_eh_construct;
            itry_e.type = &void_e;
            itry_e.variant.lowered_eh.kind = leck_internal_try;
            itry_e.variant.lowered_eh.variant.try_and_catch_expr = &itry_first;
            g_dyn_init_emit_invoke = 1;

            state.current_landing_pads.push_back(non_empty_lp);
            state.current_lpad_dispatch_bbs.push_back(disp_cur);
            state.current_lpad_exc_ptr_phis.push_back(ep_p);
            state.current_lpad_exc_sel_phis.push_back(es_p);
            assert(llvm_lower_expression(&itry_e, &val) == llvm_gen_be_error_t::ok);
            run_expr_oom([&]() noexcept {
              ensure_expr_bb();
              return llvm_lower_expression(&itry_e, &val);
            });
            ensure_expr_bb();
            state.current_lpad_dispatch_bbs.pop_back();
            state.current_lpad_exc_ptr_phis.pop_back();
            state.current_lpad_exc_sel_phis.pop_back();

            // Foreign function dispatch_bb (`getParent() != cur_fn` -> falls through to non-empty `enclosing_unwind` -> `CreateResume`)
            llvm::Function* foreign_f = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false), llvm::Function::ExternalLinkage, "foreign_eh_f", state.module.get());
            llvm::BasicBlock* foreign_disp = llvm::BasicBlock::Create(*state.context, "fdisp", foreign_f);
            llvm::IRBuilder<>(foreign_disp).CreateUnreachable();
            state.current_lpad_dispatch_bbs.push_back(foreign_disp);
            assert(llvm_lower_expression(&itry_e, &val) == llvm_gen_be_error_t::ok);
            state.current_lpad_dispatch_bbs.pop_back();
            foreign_f->eraseFromParent();

            // `noexcept` `current_edg_routine` (`spec_handled == true`) + OOM
            an_exception_specification itry_noexc_spec = {}; itry_noexc_spec.is_noexcept = TRUE;
            a_routine_type_supplement itry_rs = {}; itry_rs.exception_specification = &itry_noexc_spec;
            a_type itry_rty = {}; itry_rty.kind = tk_routine; itry_rty.variant.routine.extra_info = &itry_rs;
            a_routine itry_rout = {}; itry_rout.type = &itry_rty;
            state.current_edg_routine = &itry_rout;
            assert(llvm_lower_expression(&itry_e, &val) == llvm_gen_be_error_t::ok);
            run_expr_oom([&]() noexcept {
              ensure_expr_bb();
              return llvm_lower_expression(&itry_e, &val);
            });
            ensure_expr_bb();
            state.current_edg_routine = nullptr;
            state.current_landing_pads.pop_back();
            g_dyn_init_emit_invoke = 0;

            // `eok_indirect` function-pointer dereference & null-type/null-pointee edge cases
            a_type fn_t_ind = {}; fn_t_ind.kind = tk_routine; fn_t_ind.variant.routine.return_type = &s32_e;
            a_type pfn_t_ind = {}; pfn_t_ind.kind = tk_pointer; pfn_t_ind.size = 8; pfn_t_ind.variant.pointer.type = &fn_t_ind;
            a_variable v_pfn = {}; v_pfn.type = &pfn_t_ind;
            state.local_vars[&v_pfn] = state.builder->CreateAlloca(ptr_t);
            an_expr_node e_pfn = {}; e_pfn.kind = enk_variable; e_pfn.type = &pfn_t_ind; e_pfn.variant.variable.ptr = &v_pfn;
            an_expr_node e_ind_fn = {}; e_ind_fn.kind = enk_operation; e_ind_fn.type = &pfn_t_ind; e_ind_fn.variant.operation.kind = eok_indirect; e_ind_fn.variant.operation.operands = &e_pfn;
            assert(llvm_lower_expression(&e_ind_fn, &val) == llvm_gen_be_error_t::ok);

            // `ind_op->type == nullptr`, `ind_op_ty->kind != tk_pointer`, and `ind_op_ty->variant.pointer.type == nullptr`
            llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false), llvm::Function::ExternalLinkage, "ind_notype_fn", state.module.get());
            a_routine ind_notype_rt = {}; ind_notype_rt.source_corresp.name = (char*)"ind_notype_fn";
            an_expr_node e_notype_op = {}; e_notype_op.kind = enk_routine; e_notype_op.type = nullptr; e_notype_op.variant.routine.ptr = &ind_notype_rt;
            e_ind_fn.type = &s32_e; e_ind_fn.variant.operation.operands = &e_notype_op;
            assert(llvm_lower_expression(&e_ind_fn, &val) == llvm_gen_be_error_t::ok);

            a_type fake_ptr_st = {}; fake_ptr_st.kind = tk_struct; fake_ptr_st.size = 8;
            state.type_cache[&fake_ptr_st] = ptr_t;
            a_variable v_intptr = {}; v_intptr.type = &fake_ptr_st;
            state.local_vars[&v_intptr] = state.builder->CreateAlloca(ptr_t);
            an_expr_node e_nonptr_op = {}; e_nonptr_op.kind = enk_variable; e_nonptr_op.type = &fake_ptr_st; e_nonptr_op.variant.variable.ptr = &v_intptr;
            e_ind_fn.variant.operation.operands = &e_nonptr_op;
            assert(llvm_lower_expression(&e_ind_fn, &val) == llvm_gen_be_error_t::ok);

            an_expr_node e_nullptr_ptee = {}; e_nullptr_ptee.kind = enk_variable; e_nullptr_ptee.type = &ptr_null_e; e_nullptr_ptee.variant.variable.ptr = &v_pfn;
            e_ind_fn.variant.operation.operands = &e_nullptr_ptee;
            assert(llvm_lower_expression(&e_ind_fn, &val) == llvm_gen_be_error_t::ok);

            // MSVC rethrow with active landing pad (`CreateInvoke` on `_CxxThrowException`) + OOM sweep
            llvm::Triple prev_t = llvm::Triple(state.module->getTargetTriple());
            EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
            state.current_landing_pads.push_back(non_empty_lp);
            an_expr_node msvc_rethrow = {}; msvc_rethrow.kind = enk_throw; msvc_rethrow.type = &void_e;
            ensure_expr_bb();
            assert(llvm_lower_expression(&msvc_rethrow, &val) == llvm_gen_be_error_t::ok);
            run_expr_oom([&]() noexcept {
              ensure_expr_bb();
              return llvm_lower_expression(&msvc_rethrow, &val);
            });
            ensure_expr_bb();
            state.current_landing_pads.pop_back();
            EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), prev_t);

            // Stage-2 full branch & OOM sweep for remaining llvm_gen_be_expr.cpp paths
            {
              a_type s2_i32 = {}; s2_i32.kind = tk_integer; s2_i32.size = 4; s2_i32.variant.integer.int_kind = ik_int;
              a_type s2_u32 = {}; s2_u32.kind = tk_integer; s2_u32.size = 4; s2_u32.variant.integer.int_kind = ik_unsigned_int;
              a_type s2_i64 = {}; s2_i64.kind = tk_integer; s2_i64.size = 8; s2_i64.variant.integer.int_kind = ik_long;
              a_type s2_f32 = {}; s2_f32.kind = tk_float; s2_f32.size = 4; s2_f32.variant.float_kind = fk_float;
              a_type s2_f64 = {}; s2_f64.kind = tk_float; s2_f64.size = 8; s2_f64.variant.float_kind = fk_double;
              a_type s2_void = {}; s2_void.kind = tk_void;
              a_type s2_bad_ty = {}; s2_bad_ty.kind = static_cast<a_type_kind>(99);
              a_type s2_ptr_i32 = {}; s2_ptr_i32.kind = tk_pointer; s2_ptr_i32.size = 8; s2_ptr_i32.variant.pointer.type = &s2_i32;

              a_type s2_v4i32 = {}; s2_v4i32.kind = tk_array; s2_v4i32.size = 16;
              a_type s2_v2i32 = {}; s2_v2i32.kind = tk_array; s2_v2i32.size = 8;
              a_type s2_v4f32 = {}; s2_v4f32.kind = tk_array; s2_v4f32.size = 16;
              a_type s2_v2f32 = {}; s2_v2f32.kind = tk_array; s2_v2f32.size = 8;
              state.type_cache[&s2_v4i32] = llvm::FixedVectorType::get(llvm::Type::getInt32Ty(*state.context), 4);
              state.type_cache[&s2_v2i32] = llvm::FixedVectorType::get(llvm::Type::getInt32Ty(*state.context), 2);
              state.type_cache[&s2_v4f32] = llvm::FixedVectorType::get(llvm::Type::getFloatTy(*state.context), 4);
              state.type_cache[&s2_v2f32] = llvm::FixedVectorType::get(llvm::Type::getFloatTy(*state.context), 2);

              int_kind_is_signed[ik_int] = TRUE;
              int_kind_is_signed[ik_long] = TRUE;
              int_kind_is_signed[ik_unsigned_int] = FALSE;

              llvm::Value* lval_out = nullptr;

              // 1) Aggregate constant lvalue (L794-L811): !expr->type, bad type, bad const, OOM, and ok
              a_constant agg_c_ok = {}, agg_c_bad = {};
              agg_c_ok.kind = ck_aggregate; agg_c_ok.type = &s2_i32;
              agg_c_bad.kind = ck_aggregate; agg_c_bad.type = &s2_bad_ty;
              an_expr_node e_agg_lv = {};
              e_agg_lv.kind = enk_constant;
              e_agg_lv.variant.constant.ptr = &agg_c_ok;
              e_agg_lv.type = nullptr;
              assert(llvm_lower_lvalue_expression(&e_agg_lv, &lval_out) == llvm_gen_be_error_t::unsupported_expr);
              e_agg_lv.type = &s2_bad_ty;
              assert(llvm_lower_lvalue_expression(&e_agg_lv, &lval_out) != llvm_gen_be_error_t::ok);
              e_agg_lv.type = &s2_i32;
              a_constant inner_bad_c = {}; inner_bad_c.kind = ck_aggregate; inner_bad_c.source_corresp.decl_position.seq = 888; // fails evaluate_constant stub!
              a_type s2_arr1 = {}; s2_arr1.kind = tk_array; s2_arr1.size = 4; s2_arr1.variant.array.element_type = &s2_i32; s2_arr1.variant.array.variant.number_of_elements = 1;
              e_agg_lv.type = &s2_arr1;
              e_agg_lv.variant.constant.ptr = &inner_bad_c;
              assert(llvm_lower_lvalue_expression(&e_agg_lv, &lval_out) != llvm_gen_be_error_t::ok);
              e_agg_lv.variant.constant.ptr = &agg_c_ok;
              agg_c_ok.type = &s2_arr1;
              run_expr_oom([&]() noexcept { return llvm_lower_lvalue_expression(&e_agg_lv, &lval_out); });
              assert(llvm_lower_lvalue_expression(&e_agg_lv, &lval_out) == llvm_gen_be_error_t::ok);

              // 2) vla_ptr_slots with and without active insert block (L831) + unnamed static local with initk_static (L851-854)
              ensure_expr_bb();
              a_variable vla_v = {}; vla_v.type = &s2_i32;
              llvm::AllocaInst* vla_slot_alloca = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
              state.local_vars[&vla_v] = vla_slot_alloca;
              state.vla_ptr_slots.insert(&vla_v);
              an_expr_node e_vla_v = {}; e_vla_v.kind = enk_variable; e_vla_v.type = &s2_i32; e_vla_v.variant.variable.ptr = &vla_v;
              assert(llvm_lower_lvalue_expression(&e_vla_v, &lval_out) == llvm_gen_be_error_t::ok);
              llvm::BasicBlock* saved_bb_s2 = state.builder->GetInsertBlock();
              state.builder->ClearInsertionPoint();
              assert(llvm_lower_lvalue_expression(&e_vla_v, &lval_out) == llvm_gen_be_error_t::ok);
              state.builder->SetInsertPoint(saved_bb_s2);
              state.vla_ptr_slots.erase(&vla_v);

              a_constant c_int_ok = {}; c_int_ok.kind = ck_integer; c_int_ok.type = &s2_i32;
              a_variable anon_st1 = {}, anon_st2 = {}, anon_st3 = {};
              anon_st1.storage_class = sc_static; anon_st1.type = &s2_i32; anon_st1.init_kind = initk_static; anon_st1.initializer.constant = nullptr;
              an_expr_node e_anon_st = {}; e_anon_st.kind = enk_variable; e_anon_st.type = &s2_i32; e_anon_st.variant.variable.ptr = &anon_st1;
              assert(llvm_lower_lvalue_expression(&e_anon_st, &lval_out) == llvm_gen_be_error_t::ok);

              anon_st2.storage_class = sc_static; anon_st2.type = &s2_i32; anon_st2.init_kind = initk_static; anon_st2.initializer.constant = &inner_bad_c;
              e_anon_st.variant.variable.ptr = &anon_st2;
              assert(llvm_lower_lvalue_expression(&e_anon_st, &lval_out) != llvm_gen_be_error_t::ok);

              anon_st3.storage_class = sc_static; anon_st3.type = &s2_i32; anon_st3.init_kind = initk_static; anon_st3.initializer.constant = &c_int_ok;
              e_anon_st.variant.variable.ptr = &anon_st3;
              assert(llvm_lower_lvalue_expression(&e_anon_st, &lval_out) == llvm_gen_be_error_t::ok);

              // 3) Lvalue eok_bassign / eok_assign (L1016), eok_vector_subscript (L1021/1031), and subscript op2 null type / non-int idx (L1060-1066)
              a_variable lv_var = {}, vec_var = {};
              lv_var.source_corresp.name = "s2_lv_var"; lv_var.type = &s2_i32;
              vec_var.source_corresp.name = "s2_vec_var"; vec_var.type = &s2_v4i32;
              state.local_vars[&lv_var] = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));
              state.local_vars[&vec_var] = state.builder->CreateAlloca(llvm::FixedVectorType::get(llvm::Type::getInt32Ty(*state.context), 4));

              an_expr_node e_lhs_v = {}, e_rhs_c = {}, e_asgn_lv = {};
              e_lhs_v.kind = enk_variable; e_lhs_v.type = &s2_i32; e_lhs_v.variant.variable.ptr = &lv_var; e_lhs_v.next = &e_rhs_c;
              e_rhs_c.kind = enk_constant; e_rhs_c.type = &s2_i32; e_rhs_c.variant.constant.ptr = &c_int_ok;
              e_asgn_lv.kind = enk_operation; e_asgn_lv.type = &s2_i32; e_asgn_lv.variant.operation.operands = &e_lhs_v;
              e_asgn_lv.variant.operation.kind = eok_bassign;
              assert(llvm_lower_lvalue_expression(&e_asgn_lv, &lval_out) == llvm_gen_be_error_t::ok);
              e_asgn_lv.variant.operation.kind = eok_assign;
              assert(llvm_lower_lvalue_expression(&e_asgn_lv, &lval_out) == llvm_gen_be_error_t::ok);

              // Error in eok_assign rvalue lowering (L1018)
              an_expr_node e_bad_rhs = {}; e_bad_rhs.kind = enk_constant; e_bad_rhs.type = &s2_bad_ty;
              e_lhs_v.next = &e_bad_rhs;
              assert(llvm_lower_lvalue_expression(&e_asgn_lv, &lval_out) != llvm_gen_be_error_t::ok);
              e_lhs_v.next = &e_rhs_c;

              // eok_vector_subscript lvalue + op2->type == nullptr + non-integer index (pointer index!) + op2 == nullptr
              an_expr_node e_vec_base = {}, e_vec_idx = {}, e_vsub_lv = {};
              e_vec_base.kind = enk_variable; e_vec_base.type = &s2_v4i32; e_vec_base.variant.variable.ptr = &vec_var; e_vec_base.next = &e_vec_idx;
              e_vec_idx.kind = enk_constant; e_vec_idx.type = &s2_i32; e_vec_idx.variant.constant.ptr = &c_int_ok;
              e_vsub_lv.kind = enk_operation; e_vsub_lv.type = &s2_i32; e_vsub_lv.variant.operation.kind = eok_vector_subscript; e_vsub_lv.variant.operation.operands = &e_vec_base;
              assert(llvm_lower_lvalue_expression(&e_vsub_lv, &lval_out) == llvm_gen_be_error_t::ok);

              // Non-integer v2 (pointer) & op2->type != tk_integer & op2->type == nullptr & op2 == nullptr
              a_constant c_null_ptr = {}; c_null_ptr.kind = ck_integer; c_null_ptr.type = &s2_ptr_i32;
              e_vec_idx.type = &s2_ptr_i32; e_vec_idx.variant.constant.ptr = nullptr; // ConstantPointerNull -> !isIntegerTy()
              assert(llvm_lower_lvalue_expression(&e_vsub_lv, &lval_out) == llvm_gen_be_error_t::ok);
              // op2 == nullptr on subscript: llvm_lower_expression(nullptr) returns invalid_argument unless handled; wait: check L1052!
              // At L1052 llvm_lower_expression(op2, &v2) fails if op2 == nullptr! Wait: can op2 ever be nullptr at L1060? NO! Because L1052 already ran `llvm_lower_expression(op2, &v2)` which returns `invalid_argument` if `!op2`!
              // What about op2->type == nullptr? `enk_variable` with `lv_var` (`local_vars[&lv_var]` returns `i32` if loaded? `load_from_lvalue_ptr` needs `expr->type`!).
              // What expression succeeds in `llvm_lower_expression(op2, &v2)` when `op2->type == nullptr`?! `enk_operation` `eok_comma` where `op2->type == nullptr` and `operands->next` has `type = &s2_i32`!!
              an_expr_node c_comma_idx = {}, c_lhs_d = {}, c_rhs_d = {};
              c_lhs_d.kind = enk_constant; c_lhs_d.type = &s2_i32; c_lhs_d.next = &c_rhs_d;
              c_rhs_d.kind = enk_constant; c_rhs_d.type = &s2_i32;
              c_comma_idx.kind = enk_operation; c_comma_idx.variant.operation.kind = eok_comma; c_comma_idx.type = nullptr; c_comma_idx.variant.operation.operands = &c_lhs_d;
              e_vec_base.next = &c_comma_idx;
              assert(llvm_lower_lvalue_expression(&e_vsub_lv, &lval_out) == llvm_gen_be_error_t::ok);

              // 4) eok_padd with null idx type & non-integer idx (L1317, L1333, L1336)
              a_variable ptr_var_s2 = {}; ptr_var_s2.source_corresp.name = "s2_ptr_v"; ptr_var_s2.type = &s2_ptr_i32;
              state.local_vars[&ptr_var_s2] = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
              an_expr_node e_pbase = {}, e_padd = {};
              e_pbase.kind = enk_variable; e_pbase.type = &s2_ptr_i32; e_pbase.variant.variable.ptr = &ptr_var_s2;
              e_pbase.next = &c_comma_idx; // op1->next->type == nullptr!
              e_padd.kind = enk_operation; e_padd.type = &s2_ptr_i32; e_padd.variant.operation.kind = eok_padd; e_padd.variant.operation.operands = &e_pbase;
              assert(llvm_lower_expression(&e_padd, &val) == llvm_gen_be_error_t::ok);

              // Swapped padd (`idx + ptr`) with `op1->type == nullptr` (`c_comma_idx + e_pbase`)
              c_comma_idx.next = &e_pbase;
              e_pbase.next = nullptr;
              e_padd.variant.operation.operands = &c_comma_idx;
              assert(llvm_lower_expression(&e_padd, &val) == llvm_gen_be_error_t::ok);
              c_comma_idx.next = nullptr;

              // Non-integer idx_val (`ptr + ptr` where idx_val is pointer -> `!idx_val->getType()->isIntegerTy()`)
              an_expr_node e_pidx_ptr = {}; e_pidx_ptr.kind = enk_constant; e_pidx_ptr.type = &s2_ptr_i32;
              e_pbase.next = &e_pidx_ptr;
              e_padd.variant.operation.operands = &e_pbase;
              assert(llvm_lower_expression(&e_padd, &val) == llvm_gen_be_error_t::ok);

              // 5) Vector comparisons, eok_vector_not, eok_vector_land, eok_vector_lor, scalar->vector casts, and eok_vector_fill
              a_variable vf_var = {}; vf_var.source_corresp.name = "s2_vf_var"; vf_var.type = &s2_v4f32;
              state.local_vars[&vf_var] = state.builder->CreateAlloca(llvm::FixedVectorType::get(llvm::Type::getFloatTy(*state.context), 4));
              an_expr_node vi1 = {}, vi2 = {}, vf1 = {}, vf2 = {}, vop = {};
              vi1.kind = enk_variable; vi1.type = &s2_v4i32; vi1.variant.variable.ptr = &vec_var; vi1.next = &vi2;
              vi2.kind = enk_variable; vi2.type = &s2_v4i32; vi2.variant.variable.ptr = &vec_var;
              vf1.kind = enk_variable; vf1.type = &s2_v4f32; vf1.variant.variable.ptr = &vf_var; vf1.next = &vf2;
              vf2.kind = enk_variable; vf2.type = &s2_v4f32; vf2.variant.variable.ptr = &vf_var;

              vop.kind = enk_operation;
              const an_expr_operator_kind vec_cmp_ops[] = {
                  eok_vector_eq, eok_vector_ne, eok_vector_lt, eok_vector_le,
                  eok_vector_gt, eok_vector_ge, eok_vector_not, eok_vector_land, eok_vector_lor
              };
              for (an_expr_operator_kind vok : vec_cmp_ops) {
                vop.variant.operation.kind = vok;
                vop.type = &s2_v4i32;
                vop.variant.operation.operands = &vi1;
                assert(llvm_lower_expression(&vop, &val) == llvm_gen_be_error_t::ok);
                vop.variant.operation.operands = &vf1;
                assert(llvm_lower_expression(&vop, &val) == llvm_gen_be_error_t::ok);
              }
              // Cover L1624 false branches: expr_llvm_ty non-vector (`i1`) and expr_llvm_ty == `<4 x i1>`
              a_type s2_i1_sc = {}; s2_i1_sc.kind = tk_integer; s2_i1_sc.size = 1;
              state.type_cache[&s2_i1_sc] = llvm::Type::getInt1Ty(*state.context);
              a_type s2_v4i1 = {}; s2_v4i1.kind = tk_array; s2_v4i1.size = 4;
              state.type_cache[&s2_v4i1] = llvm::FixedVectorType::get(llvm::Type::getInt1Ty(*state.context), 4);
              vop.variant.operation.kind = eok_vector_eq;
              vop.variant.operation.operands = &vi1;
              vop.type = &s2_i1_sc; // !expr_llvm_ty->isVectorTy() && getIntegerBitWidth() == 1
              assert(llvm_lower_expression(&vop, &val) == llvm_gen_be_error_t::ok);
              vop.type = &s2_v4i1; // expr_llvm_ty == (*out_val)->getType()
              assert(llvm_lower_expression(&vop, &val) == llvm_gen_be_error_t::ok);

              // Error branches in eok_vector_land/lor (v1 err, v2 err, expr->type err)
              an_expr_node v_bad_op = {}; v_bad_op.kind = enk_constant; v_bad_op.type = &s2_bad_ty;
              vop.variant.operation.kind = eok_vector_land;
              vop.type = &s2_v4i32;
              vop.variant.operation.operands = &v_bad_op;
              assert(llvm_lower_expression(&vop, &val) != llvm_gen_be_error_t::ok);
              vi1.next = &v_bad_op;
              vop.variant.operation.operands = &vi1;
              assert(llvm_lower_expression(&vop, &val) != llvm_gen_be_error_t::ok);
              vi1.next = &vi2;
              vop.type = &s2_bad_ty;
              assert(llvm_lower_expression(&vop, &val) != llvm_gen_be_error_t::ok);

              // Scalar FP & Int -> Vector casts (L1838-1848 & L1868-1878: bitcast equal bits vs splat to FP/int elements, signed & unsigned)
              an_expr_node sc_src = {}, vec_cast = {};
              vec_cast.kind = enk_operation; vec_cast.variant.operation.kind = eok_cast; vec_cast.variant.operation.operands = &sc_src;
              sc_src.kind = enk_constant;
              // f64 (64b) -> v2i32 (64b bitcast), v4f32 (splat FP), v4i32 (splat FPToSI)
              sc_src.type = &s2_f64;
              vec_cast.type = &s2_v2i32; assert(llvm_lower_expression(&vec_cast, &val) == llvm_gen_be_error_t::ok);
              vec_cast.type = &s2_v4f32; assert(llvm_lower_expression(&vec_cast, &val) == llvm_gen_be_error_t::ok);
              vec_cast.type = &s2_v4i32; assert(llvm_lower_expression(&vec_cast, &val) == llvm_gen_be_error_t::ok);
              // i64 (64b) -> v2i32 (64b bitcast), s32 -> v4f32 (SIToFP splat), u32 -> v4f32 (UIToFP splat), s64 -> v4i32 (IntCast splat)
              sc_src.type = &s2_i64;
              vec_cast.type = &s2_v2i32; assert(llvm_lower_expression(&vec_cast, &val) == llvm_gen_be_error_t::ok);
              vec_cast.type = &s2_v4i32; assert(llvm_lower_expression(&vec_cast, &val) == llvm_gen_be_error_t::ok);
              sc_src.type = &s2_i32;
              vec_cast.type = &s2_v4f32; assert(llvm_lower_expression(&vec_cast, &val) == llvm_gen_be_error_t::ok);
              sc_src.type = &s2_u32;
              vec_cast.type = &s2_v4f32; assert(llvm_lower_expression(&vec_cast, &val) == llvm_gen_be_error_t::ok);

              // eok_vector_fill (L4767-4788): op1 err, bad vec_ty, non-vector dst, matching elem, int->int (signed, unsigned, null op1->type, non-int op1->type), fp->fp, int->fp
              an_expr_node vfill = {};
              vfill.kind = enk_operation; vfill.variant.operation.kind = eok_vector_fill;
              vfill.variant.operation.operands = &v_bad_op; vfill.type = &s2_v4i32;
              assert(llvm_lower_expression(&vfill, &val) != llvm_gen_be_error_t::ok);
              vfill.variant.operation.operands = &sc_src; sc_src.type = &s2_i32;
              vfill.type = &s2_bad_ty;
              assert(llvm_lower_expression(&vfill, &val) != llvm_gen_be_error_t::ok);
              vfill.type = &s2_i32; // !fv_ty -> unsupported_expr
              assert(llvm_lower_expression(&vfill, &val) == llvm_gen_be_error_t::unsupported_expr);
              vfill.type = &s2_v4i32; // exact i32 == elem_ty
              assert(llvm_lower_expression(&vfill, &val) == llvm_gen_be_error_t::ok);
              sc_src.type = &s2_i64; // signed int -> i32
              assert(llvm_lower_expression(&vfill, &val) == llvm_gen_be_error_t::ok);
              sc_src.type = &s2_u32;
              a_type s2_v2i64 = {}; s2_v2i64.kind = tk_array; s2_v2i64.size = 16;
              state.type_cache[&s2_v2i64] = llvm::FixedVectorType::get(llvm::Type::getInt64Ty(*state.context), 2);
              vfill.type = &s2_v2i64; // unsigned int -> i64
              assert(llvm_lower_expression(&vfill, &val) == llvm_gen_be_error_t::ok);
              // op1->type == nullptr (`c_comma_idx` returns i32 into v2i64!)
              vfill.variant.operation.operands = &c_comma_idx;
              assert(llvm_lower_expression(&vfill, &val) == llvm_gen_be_error_t::ok);
              vfill.variant.operation.operands = &sc_src;
              // fp -> fp (`f64` -> `v4f32`) and int -> fp (`i32` -> `v4f32`)
              sc_src.type = &s2_f64; vfill.type = &s2_v4f32;
              assert(llvm_lower_expression(&vfill, &val) == llvm_gen_be_error_t::ok);
              sc_src.type = &s2_i32; vfill.type = &s2_v4f32;
              assert(llvm_lower_expression(&vfill, &val) == llvm_gen_be_error_t::ok);

              // 6) Call ABI & builtins: edg_supplies_sret_param (`value_returned_as_parameter`), packed_bytes == 0,
              //    demand_defined_routines, C11 atomics (`__c11_atomic_init`, `__c11_atomic_*_fence`, `__scoped_atomic_fetch_add`, `__atomic_add_fetch`),
              //    and `__builtin_operator_new`, `__builtin_operator_delete`, `__builtin_char_memchr`
              {
                a_routine_type_supplement edg_sret_supp = {};
                edg_sret_supp.value_returned_as_parameter = TRUE;
                a_param_type ep1 = {}, ep2 = {};
                ep1.type = &s2_ptr_i32; ep1.next = &ep2;
                ep2.type = &s2_ptr_i32;
                edg_sret_supp.param_type_list = &ep1;
                a_type edg_sret_rty = {};
                edg_sret_rty.kind = tk_routine;
                edg_sret_rty.variant.routine.return_type = &s2_void;
                edg_sret_rty.variant.routine.extra_info = &edg_sret_supp;

                a_routine r_edg_sret = {};
                r_edg_sret.source_corresp.name = "s2_edg_sret_def_fn";
                r_edg_sret.type = &edg_sret_rty;
                r_edg_sret.function_def_number = 1; // also covers L2392 demand_defined_routines!

                an_expr_node cal_n = {}, arg_sret_n = {}, arg_this_n = {}, call_sret_e = {};
                cal_n.kind = enk_routine; cal_n.type = &edg_sret_rty; cal_n.variant.routine.ptr = &r_edg_sret; cal_n.next = &arg_sret_n;
                arg_sret_n.kind = enk_constant; arg_sret_n.type = &s2_ptr_i32; arg_sret_n.next = &arg_this_n;
                arg_this_n.kind = enk_constant; arg_this_n.type = &s2_ptr_i32;
                call_sret_e.kind = enk_operation; call_sret_e.type = &s2_void; call_sret_e.variant.operation.kind = eok_call; call_sret_e.variant.operation.operands = &cal_n;
                assert(llvm_lower_expression(&call_sret_e, &val) == llvm_gen_be_error_t::ok);

                // Also call with 0 args (`op1->next == nullptr`) when `edg_supplies_sret_param == true`
                cal_n.next = nullptr;
                assert(llvm_lower_expression(&call_sret_e, &val) == llvm_gen_be_error_t::ok);

                // Zero-sized expected param (`[0 x i32]`, `packed_bytes == 0` at L2343!)
                llvm::ArrayType* zarr_llvm = llvm::ArrayType::get(llvm::Type::getInt32Ty(*state.context), 0);
                llvm::Function::Create(
                    llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {zarr_llvm}, false),
                    llvm::GlobalValue::ExternalLinkage, "s2_zero_sized_param_fn", state.module.get());
                a_routine r_zparam = {}; r_zparam.source_corresp.name = "s2_zero_sized_param_fn";
                an_expr_node z_cal = {}, z_arg = {}, z_call = {};
                z_cal.kind = enk_routine; z_cal.type = &s2_ptr_i32; z_cal.variant.routine.ptr = &r_zparam; z_cal.next = &z_arg;
                z_arg.kind = enk_constant; z_arg.type = &s2_i32;
                z_call.kind = enk_operation; z_call.type = &s2_void; z_call.variant.operation.kind = eok_call; z_call.variant.operation.operands = &z_cal;
                assert(llvm_lower_expression(&z_call, &val) == llvm_gen_be_error_t::ok);

                // C11 atomics & builtin runtime rewrites
                auto call_named_builtin = [&](const char* bname, std::initializer_list<an_expr_node*> bargs, a_type_ptr ret_t) noexcept -> llvm_gen_be_error_t {
                  a_routine br = {};
                  br.source_corresp.name = bname;
                  br.type = &edg_sret_rty;
                  an_expr_node bcal = {};
                  bcal.kind = enk_routine; bcal.type = &s2_ptr_i32; bcal.variant.routine.ptr = &br;
                  an_expr_node* prev = &bcal;
                  for (an_expr_node* a : bargs) {
                    a->next = nullptr;
                    prev->next = a;
                    prev = a;
                  }
                  an_expr_node bc = {};
                  bc.kind = enk_operation; bc.type = ret_t; bc.variant.operation.kind = eok_call; bc.variant.operation.operands = &bcal;
                  return llvm_lower_expression(&bc, &val);
                };

                an_expr_node ba_ptr = {}, ba_i1 = {}, ba_i2 = {}, ba_i3 = {};
                ba_ptr.kind = enk_variable; ba_ptr.type = &s2_ptr_i32; ba_ptr.variant.variable.ptr = &ptr_var_s2;
                a_constant c_ord_seq = {}; c_ord_seq.kind = ck_integer; c_ord_seq.type = &s2_i32;
                ba_i1.kind = enk_constant; ba_i1.type = &s2_i32; ba_i1.variant.constant.ptr = &c_ord_seq;
                ba_i2.kind = enk_constant; ba_i2.type = &s2_i32; ba_i2.variant.constant.ptr = &c_ord_seq;
                ba_i3.kind = enk_constant; ba_i3.type = &s2_i32; ba_i3.variant.constant.ptr = &c_ord_seq;
                // Give ba_ptr a valid pointer value via address of lv_var (`enk_constant` null ptr is valid too!)
                an_expr_node ba_pvalid = {}; ba_pvalid.kind = enk_constant; ba_pvalid.type = &s2_ptr_i32;
                // Allocate real alloca for atomic stores/fetches via eok_address_of `lv_var`
                an_expr_node ba_lv_var = {}, ba_addr = {};
                ba_lv_var.kind = enk_variable; ba_lv_var.type = &s2_i32; ba_lv_var.variant.variable.ptr = &lv_var;
                ba_addr.kind = enk_operation; ba_addr.type = &s2_ptr_i32; ba_addr.variant.operation.kind = eok_address_of; ba_addr.variant.operation.operands = &ba_lv_var;

                assert(call_named_builtin("__c11_atomic_init", {&ba_addr}, &s2_i32) == llvm_gen_be_error_t::ok); // args.size() < 2 -> falls through to lib_sym rewrite!
                assert(call_named_builtin("__c11_atomic_init", {&ba_addr, &ba_i1}, &s2_i32) == llvm_gen_be_error_t::ok);
                assert(call_named_builtin("__c11_atomic_thread_fence", {&ba_i1}, &s2_i32) == llvm_gen_be_error_t::ok);
                assert(call_named_builtin("__c11_atomic_signal_fence", {&ba_i1}, &s2_i32) == llvm_gen_be_error_t::ok);
                assert(call_named_builtin("__scoped_atomic_fetch_add", {&ba_addr, &ba_i1, &ba_i2}, &s2_i32) == llvm_gen_be_error_t::ok);
                assert(call_named_builtin("__sync_add_and_fetch", {&ba_addr, &ba_i1}, &s2_i32) == llvm_gen_be_error_t::ok);

                // __builtin_operator_new (1 & 2 args), __builtin_operator_delete (1, 2, 3 args), __builtin_char_memchr, plus dead_bi with non-empty uses & dead_bi == nullptr
                assert(call_named_builtin("__builtin_operator_new", {&ba_i1}, &s2_ptr_i32) == llvm_gen_be_error_t::ok);
                assert(call_named_builtin("__builtin_operator_new", {&ba_i1, &ba_i2}, &s2_ptr_i32) == llvm_gen_be_error_t::ok);
                assert(call_named_builtin("__builtin_operator_delete", {&ba_pvalid}, &s2_void) == llvm_gen_be_error_t::ok);
                assert(call_named_builtin("__builtin_operator_delete", {&ba_pvalid, &ba_i1}, &s2_void) == llvm_gen_be_error_t::ok);
                assert(call_named_builtin("__builtin_operator_delete", {&ba_pvalid, &ba_i1, &ba_i2}, &s2_void) == llvm_gen_be_error_t::ok);
                assert(call_named_builtin("__builtin_char_memchr", {&ba_pvalid, &ba_i1, &ba_i2}, &s2_ptr_i32) == llvm_gen_be_error_t::ok);

                // Pre-create __builtin_char_memchr with a live use so `dead_bi->use_empty()` is False (L3592)!
                llvm::Function* pinned_bi = llvm::Function::Create(
                    llvm::FunctionType::get(llvm::PointerType::getUnqual(*state.context), false),
                    llvm::GlobalValue::ExternalLinkage, "__builtin_char_memchr", state.module.get());
                llvm::GlobalVariable* pin_user = nullptr;
                assert(llvm_create_global_variable(
                    state.module.get(), llvm::PointerType::getUnqual(*state.context), false,
                    llvm::GlobalValue::InternalLinkage, pinned_bi, "pin_bi_use", &pin_user) == llvm_gen_be_error_t::ok);
                assert(call_named_builtin("__builtin_char_memchr", {&ba_pvalid, &ba_i1, &ba_i2}, &s2_ptr_i32) == llvm_gen_be_error_t::ok);

                // Indirect function pointer call (`op1->kind != enk_routine` at L3574!) and `callee_ty == nullptr` (`void` indirect call)
                llvm::AllocaInst* fn_ptr_slot = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
                state.builder->CreateStore(pinned_bi, fn_ptr_slot);
                a_variable ind_fn_v = {}; ind_fn_v.source_corresp.name = "s2_ind_fn_v"; ind_fn_v.type = &s2_ptr_i32;
                state.local_vars[&ind_fn_v] = fn_ptr_slot;
                an_expr_node ind_cal = {}, ind_call = {};
                ind_cal.kind = enk_variable; ind_cal.type = &s2_ptr_i32; ind_cal.variant.variable.ptr = &ind_fn_v;
                ind_call.kind = enk_operation; ind_call.type = &s2_ptr_i32; ind_call.variant.operation.kind = eok_call; ind_call.variant.operation.operands = &ind_cal;
                assert(llvm_lower_expression(&ind_call, &val) == llvm_gen_be_error_t::ok);
              }

              // 7) EH push_destructors (`!lpad_used && disp_used`, `enclosing_unwind && !lp_inst`)
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
              {
                // Make an inner `enk_Conditional_cleanup` (`ecok_push_destructors`) contain a nested `ecok_push_destructors`
                // that invokes in the inner one (`lpad_used` on inner) and then branches to the outer `current_lpad_dispatch_bbs`
                // WITHOUT invoking directly to the outer `internal_lpad` -> outer has `lpad_used == false && disp_used == true`!!
                a_routine throw_r = {}; throw_r.source_corresp.name = "__s2_may_throw";
                a_type throw_rty = {}; throw_rty.kind = tk_routine; throw_rty.variant.routine.return_type = &s2_void;
                throw_r.type = &throw_rty;
                an_expr_node t_cal = {}, t_call = {};
                t_cal.kind = enk_routine; t_cal.type = &throw_rty; t_cal.variant.routine.ptr = &throw_r;
                t_call.kind = enk_operation; t_call.type = &s2_void; t_call.variant.operation.kind = eok_call; t_call.variant.operation.operands = &t_cal;

                an_eh_construct_supplement inner_ehs = {}, outer_ehs = {};
                an_expr_node inner_eh_e = {}, outer_eh_e = {};
                inner_ehs.kind = ecok_push_destructors;
                inner_ehs.first_expr = &t_call;
                inner_eh_e.kind = enk_eh_construct; inner_eh_e.type = &s2_void; inner_eh_e.variant.eh_construct = &inner_ehs;

                outer_ehs.kind = ecok_push_destructors;
                outer_ehs.first_expr = &inner_eh_e;
                outer_eh_e.kind = enk_eh_construct; outer_eh_e.type = &s2_void; outer_eh_e.variant.eh_construct = &outer_ehs;

                // Case A: no enclosing_unwind -> hits `!lpad_used && disp_used` (`L4436 F`, `L4452 F`, `L4485 F`)
                ensure_expr_bb();
                assert(llvm_lower_expression(&outer_eh_e, &val) == llvm_gen_be_error_t::ok);

                // Case B: with `enclosing_unwind != nullptr` (empty lpad block) -> hits `enclosing_unwind && lp_inst` where `lp_inst == nullptr` (`L4485 F`!)
                llvm::Function* cur_f_eh = state.builder->GetInsertBlock()->getParent();
                llvm::BasicBlock* enc_empty_lp = llvm::BasicBlock::Create(*state.context, "s2.enc.lp", cur_f_eh);
                state.current_landing_pads.push_back(enc_empty_lp);
                ensure_expr_bb();
                assert(llvm_lower_expression(&outer_eh_e, &val) == llvm_gen_be_error_t::ok);
                state.current_landing_pads.pop_back();
              }
#endif

              // 8) Ternary `eok_question` branches (terminated arms, unreachable arms, void/null expr->type, unsized expr_llvm_ty, pointer/int/fp/struct phi coercions)
              {
                a_routine unreach_r = {}; unreach_r.source_corresp.name = "__builtin_unreachable";
                a_type unreach_rty = {}; unreach_rty.kind = tk_routine; unreach_rty.variant.routine.return_type = &s2_void;
                unreach_r.type = &unreach_rty;
                an_expr_node u_cal1 = {}, u_call1 = {}, u_cal2 = {}, u_call2 = {};
                u_cal1.kind = enk_routine; u_cal1.type = &unreach_rty; u_cal1.variant.routine.ptr = &unreach_r;
                u_call1.kind = enk_operation; u_call1.type = &s2_void; u_call1.variant.operation.kind = eok_call; u_call1.variant.operation.operands = &u_cal1;
                u_cal2.kind = enk_routine; u_cal2.type = &unreach_rty; u_cal2.variant.routine.ptr = &unreach_r;
                u_call2.kind = enk_operation; u_call2.type = &s2_void; u_call2.variant.operation.kind = eok_call; u_call2.variant.operation.operands = &u_cal2;

                // Note `__builtin_unreachable` emits `unreachable` AND switches to a fresh `unreachable.after` block with 0 predecessors -> `true_unreachable == true` & `false_unreachable == true`!
                // What about `true_terminated == true` directly (e.g. `enk_throw` rethrow which terminates current BB without creating a new BB!)?
                // Both arms terminated via enk_statement (`stmk_goto` emits CreateUnreachable in stub) + unreachable arms via `__builtin_unreachable`!
                a_statement term_s1 = {}, term_s2 = {};
                term_s1.kind = stmk_goto; term_s2.kind = stmk_goto;
                an_expr_node stmt_term_arm1 = {}, stmt_term_arm2 = {};
                stmt_term_arm1.kind = enk_statement; stmt_term_arm1.type = &s2_void; stmt_term_arm1.variant.statement = &term_s1;
                stmt_term_arm2.kind = enk_statement; stmt_term_arm2.type = &s2_void; stmt_term_arm2.variant.statement = &term_s2;

                an_expr_node q_cond = {}, q_e = {};
                q_cond.kind = enk_constant; q_cond.type = &s2_i32; q_cond.variant.constant.ptr = &c_int_ok;
                q_e.kind = enk_operation; q_e.variant.operation.kind = eok_question; q_e.variant.operation.operands = &q_cond;

                // Both arms unreachable (`__builtin_unreachable` creates fresh BB with 0 preds -> `true_unreachable` & `false_unreachable`!)
                q_cond.next = &u_call1; u_call1.next = &u_call2; u_call2.next = nullptr;
                q_e.type = &s2_i32;
                ensure_expr_bb();
                assert(llvm_lower_expression(&q_e, &val) == llvm_gen_be_error_t::ok);

                // Both arms directly terminated (`stmt_term_arm1` & `stmt_term_arm2` -> `true_terminated` & `false_terminated`!)
                q_cond.next = &stmt_term_arm1; stmt_term_arm1.next = &stmt_term_arm2; stmt_term_arm2.next = nullptr;
                ensure_expr_bb();
                assert(llvm_lower_expression(&q_e, &val) == llvm_gen_be_error_t::ok);

                // `expr->type == nullptr` (`q_ty == nullptr` -> L5126 F, L5127 F!)
                an_expr_node q_i32_a = {}, q_i32_b = {};
                q_i32_a.kind = enk_constant; q_i32_a.type = &s2_i32; q_i32_a.variant.constant.ptr = &c_int_ok;
                q_i32_b.kind = enk_constant; q_i32_b.type = &s2_i32; q_i32_b.variant.constant.ptr = &c_int_ok;
                q_cond.next = &q_i32_a; q_i32_a.next = &q_i32_b; q_i32_b.next = nullptr;
                q_e.type = nullptr;
                ensure_expr_bb();
                assert(llvm_lower_expression(&q_e, &val) == llvm_gen_be_error_t::ok);

                // Arm 1 = i64, Arm 2 = nullptr (`vla_null_arm` -> `false_val == nullptr` at L5140:46 F!)
                an_expr_node vla_null_arm = {}, ptr_arm = {}, i64_arm = {};
                vla_null_arm.kind = enk_vla_dealloc; vla_null_arm.type = &s2_void;
                ptr_arm.kind = enk_constant; ptr_arm.type = &s2_ptr_i32;
                i64_arm.kind = enk_constant; i64_arm.type = &s2_i64; i64_arm.variant.constant.ptr = &c_int_ok;
                q_cond.next = &i64_arm; i64_arm.next = &vla_null_arm; vla_null_arm.next = nullptr;
                q_e.type = &s2_i64;
                ensure_expr_bb();
                assert(llvm_lower_expression(&q_e, &val) == llvm_gen_be_error_t::ok);

                // Unsized `expr_llvm_ty` (`tk_routine` lowers to `FunctionType` which has `!expr_llvm_ty->isSized()`)
                a_type unsized_fn_ty = {}; unsized_fn_ty.kind = tk_routine; unsized_fn_ty.variant.routine.return_type = &s2_void;
                q_e.type = &unsized_fn_ty;
                q_cond.next = &vla_null_arm; vla_null_arm.next = &ptr_arm; ptr_arm.next = nullptr;
                ensure_expr_bb();
                assert(llvm_lower_expression(&q_e, &val) == llvm_gen_be_error_t::ok);

                // `coerce_phi_arm` conversions:
                // a) int -> ptr (`true_val` = ptr -> `phi_ty` = ptr, `false_val` = i64 -> `CreateIntToPtr` at L5147!)
                q_e.type = &s2_i64;
                q_cond.next = &ptr_arm; ptr_arm.next = &i64_arm; i64_arm.next = nullptr;
                ensure_expr_bb();
                assert(llvm_lower_expression(&q_e, &val) == llvm_gen_be_error_t::ok);

                // b) ptr -> int? Note if either arm is ptr, lines 5134-5138 force `phi_ty = ptr`! What if `true_val` is FP (`f32`) and `false_val` is `f64` with `expr->type = &s2_f64` -> `CreateFPCast` at L5153!
                an_expr_node f32_arm = {}, f64_arm = {};
                f32_arm.kind = enk_constant; f32_arm.type = &s2_f32;
                f64_arm.kind = enk_constant; f64_arm.type = &s2_f64;
                q_e.type = &s2_f64;
                q_cond.next = &f32_arm; f32_arm.next = &f64_arm; f64_arm.next = nullptr;
                ensure_expr_bb();
                assert(llvm_lower_expression(&q_e, &val) == llvm_gen_be_error_t::ok);

                // c) `<4 x i32>` vs `i64` (`isIntOrIntVectorTy` false when one is vector and one is scalar? No, BOTH are `isIntOrIntVectorTy`! To skip L5143..5152 and reach L5155/alloca fallback: `f64` arm with `expr->type = &s2_i64`!)
                q_e.type = &s2_i64;
                q_cond.next = &f64_arm; f64_arm.next = &i64_arm; i64_arm.next = nullptr;
                ensure_expr_bb();
                assert(llvm_lower_expression(&q_e, &val) == llvm_gen_be_error_t::ok);

                // d) `!v->getType()->isSized()` in `coerce_phi_arm` (`v` is `Function*`? No, `Function*` is `PointerType`; `MetadataAsValue` or `void` call `CallInst*` has `void` type which is `!isSized()`!)
                a_routine void_ext_r = {}; void_ext_r.source_corresp.name = "__builtin_trap"; void_ext_r.type = &unreach_rty;
                an_expr_node vc_cal = {}, vc_call = {};
                vc_cal.kind = enk_routine; vc_cal.type = &unreach_rty; vc_cal.variant.routine.ptr = &void_ext_r;
                vc_call.kind = enk_operation; vc_call.type = &s2_void; vc_call.variant.operation.kind = eok_call; vc_call.variant.operation.operands = &vc_cal;
                q_e.type = &s2_i64;
                q_cond.next = &vc_call; vc_call.next = &i64_arm; i64_arm.next = nullptr;
                ensure_expr_bb();
                assert(llvm_lower_expression(&q_e, &val) == llvm_gen_be_error_t::ok);
              }

              // 9) eok_bassign edge branches (`expr->type == nullptr`, void `v2` -> `v2 != nullptr && isVoidTy()`, `op1->next != nullptr` with `v2 == nullptr`, and cleanup stack when block is already terminated at L5620!)
              {
                a_routine void_ext_r2 = {}; void_ext_r2.source_corresp.name = "__builtin_trap";
                an_expr_node vcal2 = {}, vcall2 = {};
                vcal2.kind = enk_routine; vcal2.type = nullptr; vcal2.variant.routine.ptr = &void_ext_r2;
                vcall2.kind = enk_operation; vcall2.type = &s2_void; vcall2.variant.operation.kind = eok_call; vcall2.variant.operation.operands = &vcal2;

                an_expr_node ba_lhs_p = {}, ba_e = {};
                ba_lhs_p.kind = enk_constant; ba_lhs_p.type = &s2_ptr_i32;
                ba_e.kind = enk_operation; ba_e.variant.operation.kind = eok_assign; ba_e.variant.operation.operands = &ba_lhs_p;

                // void `v2` (`!v2->getType()->isVoidTy()` False at L5231, then `v2 != nullptr` True at L5297!)
                ba_e.type = &s2_i32;
                ba_lhs_p.next = &vcall2; vcall2.next = nullptr;
                ensure_expr_bb();
                assert(llvm_lower_expression(&ba_e, &val) == llvm_gen_be_error_t::ok);

                // `expr->type == nullptr` (L5216 F) + `op1->next != nullptr` with `v2 == nullptr` (`enk_vla_dealloc` -> L5297 F & L5304 T!)
                an_expr_node vla_rhs_null = {}; vla_rhs_null.kind = enk_vla_dealloc; vla_rhs_null.type = &s2_void;
                ba_e.type = nullptr;
                ba_lhs_p.next = &vla_rhs_null;
                ensure_expr_bb();
                assert(llvm_lower_expression(&ba_e, &val) == llvm_gen_be_error_t::unsupported_expr);

                // `enk_object_lifetime` cleanup loop when block IS terminated (`stmk_goto` terminates current BB after `enk_temp_init` pushes cleanup -> L5620 False!)
                an_object_lifetime s2_ol = {};
                a_statement ol_term_s = {}; ol_term_s.kind = stmk_goto;
                an_expr_node ol_term_e = {}, ol_expr = {};
                ol_term_e.kind = enk_statement; ol_term_e.type = &s2_void; ol_term_e.variant.statement = &ol_term_s;
                a_routine s2_dtor_r = {}; s2_dtor_r.source_corresp.name = "__s2_tmp_dtor";
                a_dynamic_init s2_tmp_dip = {}; s2_tmp_dip.kind = dik_zero; s2_tmp_dip.destructor = &s2_dtor_r;
                an_expr_node ol_tmp = {}, ol_comma = {};
                ol_tmp.kind = enk_temp_init; ol_tmp.type = &s2_i32; ol_tmp.variant.init.dynamic_init = &s2_tmp_dip; ol_tmp.next = &ol_term_e;
                ol_comma.kind = enk_operation; ol_comma.type = &s2_void; ol_comma.variant.operation.kind = eok_comma; ol_comma.variant.operation.operands = &ol_tmp;
                ol_expr.kind = enk_object_lifetime; ol_expr.type = &s2_void;
                ol_expr.variant.object_lifetime.ptr = &s2_ol;
                ol_expr.variant.object_lifetime.expr = &ol_comma;
                ensure_expr_bb();
                assert(llvm_lower_expression(&ol_expr, &val) == llvm_gen_be_error_t::ok);
                ensure_expr_bb();

                // Compound pointer assignments (+=, -=, atomic +=) with `op2->type == nullptr` (`c_comma_idx`) and non-integer `op2->type` (`&s2_ptr_i32`)
                an_expr_node cp_lhs = {}, cp_op = {};
                cp_lhs.kind = enk_variable; cp_lhs.type = &s2_ptr_i32; cp_lhs.variant.variable.ptr = &ptr_var_s2;
                cp_op.kind = enk_operation; cp_op.type = &s2_ptr_i32; cp_op.variant.operation.operands = &cp_lhs;

                for (an_expr_operator_kind cpk : {eok_padd_assign, eok_psubtract_assign}) {
                  cp_op.variant.operation.kind = cpk;
                  cp_lhs.next = &c_comma_idx; c_comma_idx.next = nullptr;
                  assert(llvm_lower_compound_assignment(&cp_op, &val) == llvm_gen_be_error_t::ok);
                  an_expr_node cp_ptr_rhs = {}; cp_ptr_rhs.kind = enk_constant; cp_ptr_rhs.type = &s2_ptr_i32;
                  cp_lhs.next = &cp_ptr_rhs;
                  assert(llvm_lower_compound_assignment(&cp_op, &val) == llvm_gen_be_error_t::ok);
                }
                cp_op.variant.operation.kind = eok_pre_decr;
                cp_lhs.next = nullptr; // op2 == nullptr on pointer decrement!
                assert(llvm_lower_compound_assignment(&cp_op, &val) == llvm_gen_be_error_t::ok);
                a_type atomic_ptr_i32 = {};
                atomic_ptr_i32.kind = tk_typeref;
                atomic_ptr_i32.variant.typeref.qualifiers = TQ_C11_ATOMIC;
                atomic_ptr_i32.variant.typeref.type = &s2_ptr_i32;
                cp_lhs.type = &atomic_ptr_i32;
                cp_op.variant.operation.kind = eok_padd_assign;
                cp_lhs.next = &c_comma_idx;
                assert(llvm_lower_compound_assignment(&cp_op, &val) == llvm_gen_be_error_t::ok);
                an_expr_node cp_ptr_rhs2 = {}; cp_ptr_rhs2.kind = enk_constant; cp_ptr_rhs2.type = &s2_ptr_i32;
                cp_lhs.next = &cp_ptr_rhs2;
                assert(llvm_lower_compound_assignment(&cp_op, &val) == llvm_gen_be_error_t::ok);

                // Final Stage-2 edge branches:
                // a) `eok_padd` with BOTH `op1->type == nullptr` and `op1->next->type == nullptr` (`L1321 F` & `L1326 F`!)
                an_expr_node c_comma_idx2 = c_comma_idx;
                c_comma_idx.next = &c_comma_idx2;
                c_comma_idx2.next = nullptr;
                e_padd.variant.operation.operands = &c_comma_idx;
                assert(llvm_lower_expression(&e_padd, &val) == llvm_gen_be_error_t::ok);
                c_comma_idx.next = nullptr;

                // b) `resolve_atomic_pointee_llvm_type` with `eok_parens` having `operands == nullptr` (`cur` becomes `nullptr` -> `L2091 F, L2100 F, L2107 F`!)
                //    and `ptr_arg_node->type == nullptr` (`L2111:7 F`!)
                an_expr_node parens_null_op = {};
                parens_null_op.kind = enk_operation;
                parens_null_op.variant.operation.kind = eok_parens;
                parens_null_op.type = nullptr;
                parens_null_op.variant.operation.operands = nullptr;
                an_expr_node ai_ptr = {}, ai_ord = {};
                ai_ptr.kind = enk_constant; ai_ptr.type = &s2_ptr_i32;
                ai_ord.kind = enk_constant; ai_ord.type = &s2_i32; ai_ord.variant.constant.ptr = &c_int_ok;
                assert(call_test_builtin("__atomic_load", &s2_void, {&ai_ptr, &parens_null_op, &ai_ord}) == llvm_gen_be_error_t::atomic_lowering_failed);

                // c) `packed_bytes == 0` (`[0 x i32]`) on `wasm32` target ABI (`L2343 T`!)
                llvm::Triple saved_tr_s2 = llvm::Triple(state.module->getTargetTriple());
                EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "wasm32-unknown-unknown");
                a_type zarr_edg = {}; zarr_edg.kind = tk_array; zarr_edg.size = 0; zarr_edg.variant.array.element_type = &s2_i32; zarr_edg.variant.array.variant.number_of_elements = 0;
                llvm::Type* zarr_llvm_ty = llvm::ArrayType::get(llvm::Type::getInt32Ty(*state.context), 0);
                state.type_cache[&zarr_edg] = zarr_llvm_ty;
                state.module->getOrInsertFunction(
                    "wasm_zarr_callee",
                    llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {zarr_llvm_ty}, false));
                a_param_type z_pt = {}; z_pt.type = &zarr_edg;
                a_routine_type_supplement z_supp = {}; z_supp.param_type_list = &z_pt;
                a_type z_rty = {}; z_rty.kind = tk_routine; z_rty.variant.routine.return_type = &s2_void; z_rty.variant.routine.extra_info = nullptr; // 0 EDG params -> overridden by 1-param [0 x i32] existing_fn!
                a_routine z_fn_r = {}; z_fn_r.source_corresp.name = "wasm_zarr_callee"; z_fn_r.type = &z_rty;
                an_expr_node z_fn_op = {}, z_arg_op = {}, z_call_op = {};
                z_fn_op.kind = enk_routine; z_fn_op.type = &z_rty; z_fn_op.variant.routine.ptr = &z_fn_r; z_fn_op.next = &z_arg_op;
                z_arg_op.kind = enk_constant; z_arg_op.type = &s2_i32;
                z_call_op.kind = enk_operation; z_call_op.type = &s2_void; z_call_op.variant.operation.kind = eok_call; z_call_op.variant.operation.operands = &z_fn_op;
                assert(llvm_lower_expression(&z_call_op, &val) == llvm_gen_be_error_t::ok);
                EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), saved_tr_s2);

                // d) Unnamed routine (`source_corresp.name == nullptr`), `__builtin_test_custom_unhandled_call`, and fresh `__builtin_unseen_xyz` (`dead_bi == nullptr`)
                a_routine r_no_name = {}; r_no_name.source_corresp.name = nullptr; r_no_name.type = &z_rty;
                an_expr_node nn_cal = {}, nn_call = {};
                nn_cal.kind = enk_routine; nn_cal.type = &s2_ptr_i32; nn_cal.variant.routine.ptr = &r_no_name;
                // Pre-bind unnamed function in module? When name==nullptr, enk_routine returns error unless handled; wait: `source_corresp.name = ""` is non-null, while `nullptr` in `enk_routine` returns `unsupported_expr`? Let's check: if `nn_cal` is `enk_routine` with `name == nullptr`, `llvm_lower_expression(op1)` fails BEFORE L3579 unless `local_vars`? No, `enk_routine` checks `if (!rout->source_corresp.name)`! Wait: so `op1->variant.routine.ptr->source_corresp.name` is ALWAYS non-null at L3579 too!
                assert(call_test_builtin("__builtin_test_custom_unhandled_call", &s2_i32, {}) == llvm_gen_be_error_t::ok);
                assert(call_test_builtin("__builtin_custom_fresh_lib", &s2_i32, {}) == llvm_gen_be_error_t::ok);

                // e) `__atomic_exchange_n` (2-arg `!has_sz_arg` -> `args.size() > 2` False, and 3-arg `has_sz_arg` -> `args.size() > 3` False!)
                an_expr_node ba_lv2 = {}, ba_addr2 = {};
                ba_lv2.kind = enk_variable; ba_lv2.type = &s2_i32; ba_lv2.variant.variable.ptr = &lv_var;
                ba_addr2.kind = enk_operation; ba_addr2.type = &s2_ptr_i32; ba_addr2.variant.operation.kind = eok_address_of; ba_addr2.variant.operation.operands = &ba_lv2;
                assert(call_test_builtin("__atomic_exchange_n", &s2_i32, {&ba_addr2, &ai_ord}) != llvm_gen_be_error_t::invalid_argument);
                an_expr_node ai_sz4 = {}; ai_sz4.kind = enk_constant; ai_sz4.type = &s2_i32; ai_sz4.variant.constant.ptr = &c_int_ok;
                assert(call_test_builtin("__atomic_exchange_n", &s2_i32, {&ai_sz4, &ba_addr2, &ai_ord}) != llvm_gen_be_error_t::invalid_argument);

                // f) OOM sweeps on `__builtin_constant_p` (L2145) & `__builtin_bit_cast` (L2772)
                a_type fresh_bc_ty1 = {}, fresh_bc_ty2 = {};
                fresh_bc_ty1.kind = tk_integer; fresh_bc_ty1.size = 4;
                fresh_bc_ty2.kind = tk_integer; fresh_bc_ty2.size = 4;
                for (int b = 0; b < 8; ++b) {
                  state.type_cache.erase(&fresh_bc_ty1);
                  ensure_expr_bb();
                  g_llvm_be_oom_countdown = b;
                  llvm_gen_be_error_t r = call_test_builtin("__builtin_constant_p", &fresh_bc_ty1, {&ai_ord});
                  g_llvm_be_oom_countdown = -1;
                  if (r == llvm_gen_be_error_t::ok) break;
                }
                for (int b = 0; b < 8; ++b) {
                  state.type_cache.erase(&fresh_bc_ty2);
                  ensure_expr_bb();
                  g_llvm_be_oom_countdown = b;
                  llvm_gen_be_error_t r = call_test_builtin("__builtin_bit_cast", &fresh_bc_ty2, {&ai_ord});
                  g_llvm_be_oom_countdown = -1;
                  if (r == llvm_gen_be_error_t::ok) break;
                }

                // g) `enk_lowered_eh_construct` (`leck_internal_try`): `!lpad_used && disp_used` (`L4440 F`, `L4456 F`, `L4489 F`!)
#if DO_IL_LOWERING
                {
                  a_routine leh_r = {}; leh_r.source_corresp.name = "__s2_leh_may_throw";
                  a_type leh_rty = {}; leh_rty.kind = tk_routine; leh_rty.variant.routine.return_type = &s2_void;
                  leh_r.type = &leh_rty;
                  an_expr_node leh_cal = {}, leh_call = {};
                  leh_cal.kind = enk_routine; leh_cal.type = &leh_rty; leh_cal.variant.routine.ptr = &leh_r;
                  leh_call.kind = enk_operation; leh_call.type = &s2_void; leh_call.variant.operation.kind = eok_call; leh_call.variant.operation.operands = &leh_cal;

                  an_expr_node inner_leh = {}, outer_leh = {};
                  inner_leh.kind = enk_lowered_eh_construct;
                  inner_leh.type = &s2_void;
                  inner_leh.variant.lowered_eh.kind = leck_internal_try;
                  inner_leh.variant.lowered_eh.variant.try_and_catch_expr = &leh_call;

                  outer_leh.kind = enk_lowered_eh_construct;
                  outer_leh.type = &s2_void;
                  outer_leh.variant.lowered_eh.kind = leck_internal_try;
                  outer_leh.variant.lowered_eh.variant.try_and_catch_expr = &inner_leh;

                  ensure_expr_bb();
                  assert(llvm_lower_expression(&outer_leh, &val) == llvm_gen_be_error_t::ok);

                  llvm::Function* cur_f_leh = state.builder->GetInsertBlock()->getParent();
                  llvm::BasicBlock* enc_lp_leh = llvm::BasicBlock::Create(*state.context, "s2.leh.enclp", cur_f_leh);
                  state.current_landing_pads.push_back(enc_lp_leh);
                  ensure_expr_bb();
                  assert(llvm_lower_expression(&outer_leh, &val) == llvm_gen_be_error_t::ok);
                  state.current_landing_pads.pop_back();
                }
#endif

                // h) `eok_vector_fill` FP->int (`sval` = `f32`, `elem_ty` = `i32` -> `L4781 F` & `L4785 F`!) and non-int `op1->type` (`&s2_ptr_i32` on `eok_comma` returning `i64` -> `L4782:33 F`!)
                sc_src.type = &s2_f32;
                vfill.variant.operation.operands = &sc_src;
                vfill.type = &s2_v4i32;
                ensure_expr_bb();
                assert(llvm_lower_expression(&vfill, &val) == llvm_gen_be_error_t::ok);

                an_expr_node c_ptr_typed_int = c_comma_idx;
                c_ptr_typed_int.type = &s2_ptr_i32; // skip_typerefs(op1->type)->kind == tk_pointer (!= tk_integer), while value is i32 -> v2i64!
                vfill.variant.operation.operands = &c_ptr_typed_int;
                vfill.type = &s2_v2i64;
                assert(llvm_lower_expression(&vfill, &val) == llvm_gen_be_error_t::ok);

                // i) `eok_question`: multi-BB arms (`true_end_bb != true_bb` with `hasNPredecessors(0)` False -> `L5095 F` & `L5116 F`), non-ptr `false_val` (`L5140:46 F`), and `i64` -> `f64` coercion (`L5150:51 F`)!
                an_expr_node sub_q1 = {}, sub_q2 = {}, sub_c1 = {}, sub_a1 = {}, sub_b1 = {}, sub_c2 = {}, sub_a2 = {}, sub_b2 = {}, top_q = {}, top_cond = {};
                sub_c1.kind = enk_constant; sub_c1.type = &s2_i32; sub_c1.variant.constant.ptr = &c_int_ok; sub_c1.next = &sub_a1;
                sub_a1.kind = enk_constant; sub_a1.type = &s2_i64; sub_a1.variant.constant.ptr = &c_int_ok; sub_a1.next = &sub_b1;
                sub_b1.kind = enk_constant; sub_b1.type = &s2_i64; sub_b1.variant.constant.ptr = &c_int_ok;
                sub_q1.kind = enk_operation; sub_q1.type = &s2_i64; sub_q1.variant.operation.kind = eok_question; sub_q1.variant.operation.operands = &sub_c1;

                sub_c2.kind = enk_constant; sub_c2.type = &s2_i32; sub_c2.variant.constant.ptr = &c_int_ok; sub_c2.next = &sub_a2;
                sub_a2.kind = enk_constant; sub_a2.type = &s2_f64; sub_a2.next = &sub_b2;
                sub_b2.kind = enk_constant; sub_b2.type = &s2_f64;
                sub_q2.kind = enk_operation; sub_q2.type = &s2_f64; sub_q2.variant.operation.kind = eok_question; sub_q2.variant.operation.operands = &sub_c2;

                top_cond.kind = enk_constant; top_cond.type = &s2_i32; top_cond.variant.constant.ptr = &c_int_ok; top_cond.next = &sub_q1;
                sub_q1.next = &sub_q2; sub_q2.next = nullptr;
                top_q.kind = enk_operation; top_q.type = &s2_f64; top_q.variant.operation.kind = eok_question; top_q.variant.operation.operands = &top_cond;
                ensure_expr_bb();
                assert(llvm_lower_expression(&top_q, &val) == llvm_gen_be_error_t::ok);

                // j) `eok_assign`: `op1->next == nullptr` (`L5219 F` & `L5304 F`), and `constructed_in_place == true` (`L5231 F` & `L5297 T`!)
                an_expr_node ba_lhs_only = {}, ba_no_rhs = {};
                ba_lhs_only.kind = enk_constant; ba_lhs_only.type = &s2_ptr_i32; ba_lhs_only.next = nullptr;
                ba_no_rhs.kind = enk_operation; ba_no_rhs.type = &s2_i32; ba_no_rhs.variant.operation.kind = eok_assign; ba_no_rhs.variant.operation.operands = &ba_lhs_only;
                ensure_expr_bb();
                assert(llvm_lower_expression(&ba_no_rhs, &val) == llvm_gen_be_error_t::unsupported_expr);

                a_field s2_st_fld = {}; s2_st_fld.type = &s2_i32;
                a_type s2_st_ty = {}; s2_st_ty.kind = tk_struct; s2_st_ty.size = 4; s2_st_ty.variant.class_struct_union.field_list = &s2_st_fld;
                a_dynamic_init cip_dip = {}; cip_dip.kind = dik_zero;
                an_expr_node cip_rhs = {}, ba_cip = {}, ba_st_ptr = {};
                cip_rhs.kind = enk_temp_init; cip_rhs.type = &s2_st_ty; cip_rhs.variant.init.dynamic_init = &cip_dip;
                ba_st_ptr.kind = enk_constant; ba_st_ptr.type = &s2_ptr_i32; ba_st_ptr.next = &cip_rhs;
                ba_cip.kind = enk_operation; ba_cip.type = &s2_st_ty; ba_cip.variant.operation.kind = eok_assign; ba_cip.variant.operation.operands = &ba_st_ptr;
                ensure_expr_bb();
                g_force_cip_in_dyn_init = 1;
                assert(llvm_lower_expression(&ba_cip, &val) == llvm_gen_be_error_t::ok);
                g_force_cip_in_dyn_init = 0;

                // k) `enk_object_lifetime` with active cleanup when block IS terminated (`L5624 F`!)
                ScopeCleanup manual_sc = {};
                manual_sc.is_active = false;
                a_statement term_s_ol = {}; term_s_ol.kind = stmk_goto;
                an_expr_node term_e_ol = {}; term_e_ol.kind = enk_statement; term_e_ol.type = &s2_void; term_e_ol.variant.statement = &term_s_ol;
                // Push cleanup inside `llvm_lower_statement` on `stmk_for` before emitting `CreateUnreachable`!
                a_statement push_and_term_s = {}; push_and_term_s.kind = stmk_for;
                term_e_ol.variant.statement = &push_and_term_s;
                an_object_lifetime s2_ol2 = {};
                an_expr_node ol_expr2 = {};
                ol_expr2.kind = enk_object_lifetime; ol_expr2.type = &s2_void;
                ol_expr2.variant.object_lifetime.ptr = &s2_ol2;
                ol_expr2.variant.object_lifetime.expr = &term_e_ol;
                ensure_expr_bb();
                assert(llvm_lower_expression(&ol_expr2, &val) == llvm_gen_be_error_t::ok);
                ensure_expr_bb();

                // l) Null-type binary arithmetic op (`expr->type == nullptr`), unsigned `__builtin_add_overflow`, and `eok_vector_question`
                {
                  a_constant c_lhs_nt = {}, c_rhs_nt = {};
                  c_lhs_nt.kind = ck_integer; c_lhs_nt.type = &s2_i32;
                  c_rhs_nt.kind = ck_integer; c_rhs_nt.type = &s2_i32;
                  an_expr_node e_lhs_nt = {}, e_rhs_nt = {}, e_bin_nt = {};
                  e_lhs_nt.kind = enk_constant; e_lhs_nt.type = &s2_i32; e_lhs_nt.variant.constant.ptr = &c_lhs_nt; e_lhs_nt.next = &e_rhs_nt;
                  e_rhs_nt.kind = enk_constant; e_rhs_nt.type = &s2_i32; e_rhs_nt.variant.constant.ptr = &c_rhs_nt;
                  e_bin_nt.kind = enk_operation;
                  e_bin_nt.type = nullptr;
                  e_bin_nt.variant.operation.kind = eok_add;
                  e_bin_nt.variant.operation.operands = &e_lhs_nt;
                  assert(llvm_lower_arithmetic_expression(&e_bin_nt, &val) == llvm_gen_be_error_t::ok);

                  int_kind_is_signed[ik_unsigned_int] = FALSE;
                  a_type ovf_bool_ty = {}; ovf_bool_ty.kind = tk_integer; ovf_bool_ty.size = 1;
                  state.type_cache[&ovf_bool_ty] = llvm::Type::getInt1Ty(*state.context);
                  a_routine ovf_u_rout = {};
                  ovf_u_rout.source_corresp.name = "__builtin_add_overflow";
                  a_type ovf_u_rty = {}; ovf_u_rty.kind = tk_routine; ovf_u_rty.variant.routine.return_type = &ovf_bool_ty;
                  an_expr_node ovf_cal = {}, ovf_a1 = {}, ovf_a2 = {}, ovf_a3 = {}, ovf_call = {};
                  a_variable ovf_out_v = {}; ovf_out_v.source_corresp.name = "ovf_u_out"; ovf_out_v.type = &s2_u32;
                  a_type u32_ptr_ty = {}; u32_ptr_ty.kind = tk_pointer; u32_ptr_ty.size = 8; u32_ptr_ty.variant.pointer.type = &s2_u32;
                  state.type_cache[&u32_ptr_ty] = llvm::PointerType::getUnqual(*state.context);
                  ovf_cal.kind = enk_routine; ovf_cal.type = &ovf_u_rty; ovf_cal.variant.routine.ptr = &ovf_u_rout; ovf_cal.next = &ovf_a1;
                  ovf_a1.kind = enk_constant; ovf_a1.type = &s2_u32; ovf_a1.variant.constant.ptr = &c_lhs_nt; ovf_a1.next = &ovf_a2;
                  ovf_a2.kind = enk_constant; ovf_a2.type = &s2_u32; ovf_a2.variant.constant.ptr = &c_rhs_nt; ovf_a2.next = &ovf_a3;
                  ovf_a3.kind = enk_variable; ovf_a3.type = &u32_ptr_ty; ovf_a3.is_lvalue = FALSE; ovf_a3.variant.variable.ptr = &ovf_out_v;
                  state.local_vars[&ovf_out_v] = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
                  state.builder->CreateStore(state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context)), state.local_vars[&ovf_out_v]);
                  ovf_call.kind = enk_operation; ovf_call.type = &ovf_bool_ty; ovf_call.variant.operation.kind = eok_call; ovf_call.variant.operation.operands = &ovf_cal;
                  assert(llvm_lower_expression(&ovf_call, &val) == llvm_gen_be_error_t::ok);

#if GNU_VECTOR_TYPES_ALLOWED
                  a_constant vc_c = {}; vc_c.kind = ck_integer; vc_c.type = &s2_v2i32;
                  an_expr_node vq_cond = {}, vq_t = {}, vq_f = {}, vq_bad = {}, vq_e = {};
                  vq_cond.kind = enk_constant; vq_cond.type = &s2_v2i32; vq_cond.variant.constant.ptr = &vc_c; vq_cond.next = &vq_t;
                  vq_t.kind = enk_constant; vq_t.type = &s2_v2i32; vq_t.variant.constant.ptr = &vc_c; vq_t.next = &vq_f;
                  vq_f.kind = enk_constant; vq_f.type = &s2_v2i32; vq_f.variant.constant.ptr = &vc_c;
                  vq_e.kind = enk_operation; vq_e.type = &s2_v2i32; vq_e.variant.operation.kind = eok_vector_question; vq_e.variant.operation.operands = &vq_cond;
                  assert(llvm_lower_expression(&vq_e, &val) == llvm_gen_be_error_t::ok);
                  vq_bad.kind = static_cast<an_expr_node_kind>(999);
                  vq_cond.next = &vq_bad;
                  vq_bad.next = &vq_f;
                  assert(llvm_lower_expression(&vq_e, &val) != llvm_gen_be_error_t::ok);
#endif
                }
              }
            }
          }
        }
      }
    }
  }

  printf("All tests passed!\n");
  return 0;
}
namespace edg {
  /**
   * @brief Test stub or helper for `llvm_lower_statement`.
   * @details Provides mock or helper behavior for `llvm_lower_statement` within `tests/test_llvm_expressions.cpp`.
   * @param[in] stmt Parameter `stmt` for `llvm_lower_statement`.
   * @return Result of `llvm_lower_statement` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_statement(a_statement_ptr stmt) noexcept {
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    if (stmt && stmt->kind == stmk_return) return llvm_gen_be_error_t::unsupported_stmt;
    if (stmt && stmt->kind == stmk_goto) {
      be_state->builder->CreateUnreachable();
    }
    if (stmt && stmt->kind == stmk_for) {
      ScopeCleanup sc = {};
      sc.is_active = false;
      be_state->cleanup_stack.cleanups.push_back(sc);
      be_state->builder->CreateUnreachable();
    }
    return llvm_gen_be_error_t::ok;
  }
  /**
   * @brief Test stub or helper for `llvm_emit_dynamic_init`.
   * @details Provides mock or helper behavior for `llvm_emit_dynamic_init` within `tests/test_llvm_expressions.cpp`.
   * @param[in] dip Parameter `dip` for `llvm_emit_dynamic_init`.
   * @param[in] ptr Parameter `ptr` for `llvm_emit_dynamic_init`.
   * @param[in] obj_type Parameter `obj_type` for `llvm_emit_dynamic_init`.
   * @return Result of `llvm_emit_dynamic_init` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_emit_dynamic_init(a_dynamic_init_ptr dip, llvm::Value* ptr, llvm::Type* obj_type) noexcept {
    if (!dip || !ptr) return llvm_gen_be_error_t::invalid_argument;
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    if (g_dyn_init_emit_invoke && !be_state->current_landing_pads.empty()) {
      llvm::Function* pfn = be_state->builder->GetInsertBlock()->getParent();
      llvm::BasicBlock* cont_bb = llvm::BasicBlock::Create(*be_state->context, "dyn.inv.cont", pfn);
      llvm::FunctionCallee inv_fn = be_state->module->getOrInsertFunction(
          "__test_throwing_ctor", llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false));
      be_state->builder->CreateInvoke(inv_fn, cont_bb, be_state->current_landing_pads.back(), {});
      be_state->builder->SetInsertPoint(cont_bb);
    }
    if (dip->kind == dik_none || (dip->variable && dip->kind != dik_constant)) return llvm_gen_be_error_t::unsupported_expr;
    if (g_force_cip_in_dyn_init) {
      be_state->prvalue_constructed_in_place = true;
    }
    if (dip->kind == dik_expression && dip->variant.expression) {
      llvm::Value* init_val = nullptr;
      llvm_gen_be_error_t err = llvm_lower_expression(dip->variant.expression, &init_val);
      if (err != llvm_gen_be_error_t::ok) return err;
      if (init_val) {
        be_state->builder->CreateStore(init_val, ptr);
        return llvm_gen_be_error_t::ok;
      }
    }
    if (obj_type && obj_type->isSized()) be_state->builder->CreateStore(llvm::Constant::getNullValue(obj_type), ptr);
    return llvm_gen_be_error_t::ok;
  }
}
