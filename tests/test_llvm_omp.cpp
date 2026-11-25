/**
 * @file test_llvm_omp.cpp
 * @ingroup llvm_gen_be_tests
 * @brief Unit tests for the LLVM backend OpenMP lowering subsystem.
 * @details Validates OpenMP parallel, worksharing, synchronization, tasking, reduction, and target offload lowering.
 */

#include "llvm_gen_be_omp.h"
#include "llvm_gen_be_internal.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/TargetSelect.h>

using namespace edg;

namespace edg {
  /**
   * @brief Test stub or helper for `get_mangled_function_name`.
   * @details Provides mock or helper behavior for `get_mangled_function_name` within `tests/test_llvm_omp.cpp`.
   * @param[in] routine Parameter `routine` for `get_mangled_function_name`.
   * @return Result of `get_mangled_function_name` (char*).
   */
  char* get_mangled_function_name(a_routine_ptr routine) { return nullptr; }
  a_byte_boolean int_kind_is_signed[ik_last] = {0};
  a_targ_size_t targ_sizeof_pointer = 8;
  /**
   * @brief Test stub for `il_lowering_needed`.
   * @details Returns `FALSE` in standalone OpenMP unit tests.
   * @return `FALSE`.
   */
  a_boolean il_lowering_needed(void) { return FALSE; }
  
  int g_omp_fail_type = 0; ///< When non-zero, `get_llvm_type` fails immediately.
  int g_omp_fail_type_countdown = 0; ///< Countdown of calls before `get_llvm_type` fails.
  int g_omp_rich_types = 0; ///< When non-zero, `get_llvm_type` and `llvm_lower_expression` honor `ty->kind`.
  int g_omp_fail_expr = 0; ///< Countdown of calls before `llvm_lower_expression` fails.
  int g_omp_expr_i64 = 0; ///< When non-zero, `llvm_lower_expression` returns `i64` instead of `i32`.
  int g_omp_expr_null_val = 0; ///< When non-zero, `llvm_lower_expression` returns `ok` with `*out_val = nullptr`.

  /**
   * @brief Test stub or helper for `get_llvm_type`.
   * @details Provides mock or helper behavior for `get_llvm_type` within `tests/test_llvm_omp.cpp`.
   * @param[in] ty Parameter `ty` for `get_llvm_type`.
   * @param[out] out_ty Parameter `out_ty` for `get_llvm_type`.
   * @return Result of `get_llvm_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_llvm_type(a_type_ptr ty, llvm::Type** out_ty) noexcept {
      if (g_omp_fail_type) return llvm_gen_be_error_t::unsupported_type;
      if (g_omp_fail_type_countdown > 0) {
          --g_omp_fail_type_countdown;
          if (g_omp_fail_type_countdown == 0) return llvm_gen_be_error_t::unsupported_type;
      }
      if (g_omp_rich_types && ty) {
          while (ty && ty->kind == tk_typeref) ty = ty->variant.typeref.type;
          if (!ty) {
              *out_ty = llvm::Type::getInt32Ty(*be_state->context);
              return llvm_gen_be_error_t::ok;
          }
          if (ty->kind == tk_void) {
              *out_ty = llvm::Type::getVoidTy(*be_state->context);
              return llvm_gen_be_error_t::ok;
          }
          if (ty->kind == tk_float) {
              *out_ty = (ty->size == 8) ? llvm::Type::getDoubleTy(*be_state->context)
                                        : llvm::Type::getFloatTy(*be_state->context);
              return llvm_gen_be_error_t::ok;
          }
          if (ty->kind == tk_pointer) {
              *out_ty = llvm::PointerType::getUnqual(*be_state->context);
              return llvm_gen_be_error_t::ok;
          }
          if (ty->kind == tk_array) {
              llvm::Type* elem_t = llvm::Type::getInt32Ty(*be_state->context);
              if (ty->variant.array.element_type) {
                  LLVM_GEN_BE_CHECK(get_llvm_type(ty->variant.array.element_type, &elem_t));
              }
              uint64_t cnt = (ty->size > 0) ? static_cast<uint64_t>(ty->size) : 4ULL;
              *out_ty = llvm::ArrayType::get(elem_t, cnt);
              return llvm_gen_be_error_t::ok;
          }
          if (ty->kind == tk_struct || ty->kind == tk_class || ty->kind == tk_union) {
              *out_ty = llvm::StructType::get(
                  *be_state->context,
                  {llvm::Type::getInt32Ty(*be_state->context), llvm::Type::getInt64Ty(*be_state->context)});
              return llvm_gen_be_error_t::ok;
          }
          if (ty->kind == tk_routine) {
              std::vector<llvm::Type*> ptypes;
              if (ty->variant.routine.extra_info) {
                  for (a_param_type_ptr p = ty->variant.routine.extra_info->param_type_list; p != nullptr; p = p->next) {
                      ptypes.push_back(llvm::PointerType::getUnqual(*be_state->context));
                  }
              }
              *out_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), ptypes, false);
              return llvm_gen_be_error_t::ok;
          }
          if (ty->kind == tk_integer && ty->size == 8) {
              *out_ty = llvm::Type::getInt64Ty(*be_state->context);
              return llvm_gen_be_error_t::ok;
          }
          if (ty->kind == tk_integer && ty->size == 1) {
              *out_ty = llvm::Type::getInt8Ty(*be_state->context);
              return llvm_gen_be_error_t::ok;
          }
      }
      *out_ty = llvm::Type::getInt32Ty(*be_state->context);
      return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `get_di_location`.
   * @details Provides mock or helper behavior for `get_di_location` within `tests/test_llvm_omp.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `get_di_location`.
   * @param[in] arg_1 Parameter `arg_1` for `get_di_location`.
   * @param[in] arg_2 Parameter `arg_2` for `get_di_location`.
   * @param[out] arg_3 Parameter `arg_3` for `get_di_location`.
   * @return Result of `get_di_location` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_di_location(llvm_gen_be_debug_state_t* arg_0, a_source_position arg_1, llvm::DIScope* arg_2, llvm::DILocation** arg_3) noexcept {
      return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `evaluate_constant`.
   * @details Provides mock or helper behavior for `evaluate_constant` within `tests/test_llvm_omp.cpp`.
   * @param[in] con Parameter `con` for `evaluate_constant`.
   * @param[in] ty Parameter `ty` for `evaluate_constant`.
   * @param[out] out_const Parameter `out_const` for `evaluate_constant`.
   * @return Result of `evaluate_constant` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t evaluate_constant(a_constant_ptr con, llvm::Type* ty, llvm::Constant** out_const) noexcept {
      *out_const = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0);
      return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `pop_lexical_block`.
   * @details Provides mock or helper behavior for `pop_lexical_block` within `tests/test_llvm_omp.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `pop_lexical_block`.
   * @return Result of `pop_lexical_block` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t pop_lexical_block(llvm_gen_be_debug_state_t* arg_0) noexcept { return llvm_gen_be_error_t::ok; }
  /**
   * @brief Test stub or helper for `push_lexical_block`.
   * @details Provides mock or helper behavior for `push_lexical_block` within `tests/test_llvm_omp.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `push_lexical_block`.
   * @param[in] arg_1 Parameter `arg_1` for `push_lexical_block`.
   * @param[out] arg_2 Parameter `arg_2` for `push_lexical_block`.
   * @return Result of `push_lexical_block` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t push_lexical_block(llvm_gen_be_debug_state_t* arg_0, a_source_position arg_1, llvm::DILexicalBlock** arg_2) noexcept { return llvm_gen_be_error_t::ok; }
  
  /**
   * @brief Test stub or helper for `get_typeinfo_global`.
   * @details Provides mock or helper behavior for `get_typeinfo_global` within `tests/test_llvm_omp.cpp`.
   * @param[in] ty Parameter `ty` for `get_typeinfo_global`.
   * @param[out] out_const Parameter `out_const` for `get_typeinfo_global`.
   * @return Result of `get_typeinfo_global` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_typeinfo_global(a_type_ptr ty, llvm::Constant** out_const) noexcept {
      *out_const = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
      return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `llvm_lower_asm_stmt`.
   * @details Provides mock or helper behavior for `llvm_lower_asm_stmt` within `tests/test_llvm_omp.cpp`.
   * @param[in] stmt Parameter `stmt` for `llvm_lower_asm_stmt`.
   * @return Result of `llvm_lower_asm_stmt` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_asm_stmt(a_statement_ptr stmt) noexcept { return llvm_gen_be_error_t::ok; }
  
  /**
   * @brief Test stub or helper for `llvm_lower_expression`.
   * @details Provides mock or helper behavior for `llvm_lower_expression` within `tests/test_llvm_omp.cpp`.
   * @param[in] expr Parameter `expr` for `llvm_lower_expression`.
   * @param[out] out_val Parameter `out_val` for `llvm_lower_expression`.
   * @return Result of `llvm_lower_expression` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_expression(an_expr_node_ptr expr, llvm::Value** out_val) noexcept {
      if (g_omp_fail_expr < 0) return llvm_gen_be_error_t::unsupported_expr;
      if (g_omp_fail_expr > 0) {
          --g_omp_fail_expr;
          if (g_omp_fail_expr == 0) return llvm_gen_be_error_t::unsupported_expr;
      }
      if (g_omp_expr_null_val > 0) {
          --g_omp_expr_null_val;
          if (g_omp_expr_null_val == 0) {
              *out_val = nullptr;
              return llvm_gen_be_error_t::ok;
          }
      }
      uint64_t cval = expr ? static_cast<uint64_t>(expr->position.column) : 0ULL;
      if (g_omp_rich_types && expr && expr->type) {
          llvm::Type* ety = nullptr;
          LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &ety));
          if (ety->isPointerTy()) {
              *out_val = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ety));
              return llvm_gen_be_error_t::ok;
          }
          if (ety->isFloatingPointTy()) {
              *out_val = llvm::ConstantFP::get(ety, static_cast<double>(cval));
              return llvm_gen_be_error_t::ok;
          }
          if (ety->isIntegerTy()) {
              *out_val = llvm::ConstantInt::get(ety, cval);
              return llvm_gen_be_error_t::ok;
          }
      }
      if (g_omp_expr_i64) {
          *out_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), cval);
      } else {
          *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), cval);
      }
      return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `llvm_lower_coroutine_stmt`.
   * @details Provides mock or helper behavior for `llvm_lower_coroutine_stmt` within `tests/test_llvm_omp.cpp`.
   * @param[in] stmt Parameter `stmt` for `llvm_lower_coroutine_stmt`.
   * @return Result of `llvm_lower_coroutine_stmt` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_coroutine_stmt(a_statement_ptr stmt) noexcept { return llvm_gen_be_error_t::ok; }
  /**
   * @brief Test stub or helper for `llvm_lower_coroutine_return_stmt`.
   * @details Provides mock or helper behavior for `llvm_lower_coroutine_return_stmt` within `tests/test_llvm_omp.cpp`.
   * @param[in] stmt Parameter `stmt` for `llvm_lower_coroutine_return_stmt`.
   * @return Result of `llvm_lower_coroutine_return_stmt` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_coroutine_return_stmt(a_statement_ptr stmt) noexcept { return llvm_gen_be_error_t::ok; }
}


LLVMBackendState* edg::be_state = nullptr;

/**
 * @brief Entry point for the OpenMP runtime lowering unit test suite.
 * @details Exercises all `lower_omp_*` runtime emission functions.
 * @return 0 on success.
 */
int main(void) {
  printf("Running test_llvm_omp...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_omp", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);

  a_statement stmt = {};
  stmt.kind = stmk_empty;
  stmt.position.seq = 42;
  stmt.position.column = 7;

  a_variable local_var = {};
  local_var.source_corresp.name = (char*)"shared_x";
  llvm::AllocaInst* local_alloca = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context), nullptr, "shared_x");
  state.local_vars[&local_var] = local_alloca;

  assert(lower_omp_parallel_region(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_parallel_region(&stmt) == llvm_gen_be_error_t::ok);
  llvm::Function* microtask = state.module->getFunction(".omp_microtask.");
  assert(microtask != nullptr && microtask->arg_size() == 3);

  assert(lower_omp_for_worksharing(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_for_worksharing(&stmt) == llvm_gen_be_error_t::ok);

  assert(lower_omp_critical(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_critical(&stmt) == llvm_gen_be_error_t::ok);
  assert(state.module->getGlobalVariable("gomp_critical_user_.var", true) != nullptr);

  assert(lower_omp_barrier(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_barrier(&stmt) == llvm_gen_be_error_t::ok);

  assert(lower_omp_target_region(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_target_region(&stmt) == llvm_gen_be_error_t::ok);
  assert(state.module->getFunction("__omp_offloading_42_test_func") != nullptr);
  assert(state.module->getNamedGlobal(".omp_offloading.entry.__omp_offloading_42_test_func") != nullptr);

  assert(lower_omp_task(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_task(&stmt) == llvm_gen_be_error_t::ok);
  assert(state.module->getFunction(".omp_task_entry.") != nullptr);

  assert(lower_omp_taskwait(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_taskwait(&stmt) == llvm_gen_be_error_t::ok);

  assert(lower_omp_taskgroup(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_taskgroup(&stmt) == llvm_gen_be_error_t::ok);

  assert(lower_omp_reduce(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_reduce(&stmt) == llvm_gen_be_error_t::ok);


  assert(lower_omp_sections(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_sections(&stmt) == llvm_gen_be_error_t::ok);

  assert(lower_omp_section(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_section(&stmt) == llvm_gen_be_error_t::ok);

  assert(lower_omp_single(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(lower_omp_single(&stmt) == llvm_gen_be_error_t::ok);

  // Test private and firstprivate clauses in parallel region + pragma dispatch via llvm_lower_statement
  a_type int_ty = {};
  int_ty.kind = tk_integer;
  a_variable priv_var = {};
  priv_var.source_corresp.name = (char*)"priv_v";
  priv_var.type = &int_ty;
  a_variable fpriv_var = {};
  fpriv_var.source_corresp.name = (char*)"fpriv_v";
  fpriv_var.type = &int_ty;
  state.local_vars[&priv_var] = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context), nullptr, "priv_v");
  state.local_vars[&fpriv_var] = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context), nullptr, "fpriv_v");

  stmt.has_associated_pragma = TRUE;
  state.omp_pragmas[&stmt] = "omp parallel private(priv_v) firstprivate(fpriv_v)";
  assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);

  // Test canonical stmk_for loop bound and induction variable extraction via pragma dispatch
  a_variable ind_var = {};
  ind_var.source_corresp.name = (char*)"i";
  ind_var.type = &int_ty;
  state.local_vars[&ind_var] = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context), nullptr, "i");

  an_expr_node lb_expr = {};
  lb_expr.kind = enk_constant;
  a_dynamic_init init_dip = {};
  init_dip.kind = dik_expression;
  init_dip.variable = &ind_var;
  init_dip.variant.expression = &lb_expr;
  a_statement init_stmt = {};
  init_stmt.kind = stmk_init;
  init_stmt.variant.dynamic_init = &init_dip;

  an_expr_node cmp_op1 = {};
  an_expr_node cmp_op2 = {};
  cmp_op1.next = &cmp_op2;
  an_expr_node cond_expr = {};
  cond_expr.kind = enk_operation;
  cond_expr.variant.operation.kind = eok_lt;
  cond_expr.variant.operation.operands = &cmp_op1;

  an_expr_node inc_expr = {};
  inc_expr.kind = enk_constant;

  a_statement body_stmt = {};
  body_stmt.kind = stmk_empty;

  a_for_loop for_extra = {};
  for_extra.initialization = &init_stmt;
  for_extra.increment = &inc_expr;

  a_statement for_stmt = {};
  for_stmt.kind = stmk_for;
  for_stmt.expr = &cond_expr;
  for_stmt.variant.for_loop.statement = &body_stmt;
  for_stmt.variant.for_loop.extra_info = &for_extra;
  for_stmt.has_associated_pragma = TRUE;
  state.omp_pragmas[&for_stmt] = "omp for";

  assert(llvm_lower_statement(&for_stmt) == llvm_gen_be_error_t::ok);

  // Test remaining directive strings through llvm_dispatch_omp_pragma_stmt
  const char* directives[] = {
      "omp critical", "omp barrier", "omp target", "omp taskwait",
      "omp taskgroup", "omp task", "omp reduction(+:shared_x)",
      "omp reduction(-:shared_x)", "omp reduction(*:shared_x)",
      "omp reduction(&:shared_x)", "omp reduction(&&:shared_x)",
      "omp reduction(|:shared_x)", "omp reduction(||:shared_x)",
      "omp reduction(^:shared_x)", "omp reduction(min:shared_x)",
      "omp reduction(max:shared_x)", "omp reduce",
      "omp for schedule(dynamic, 4)", "omp for schedule(guided, 2)",
      "omp for schedule(runtime)", "omp sections", "omp section",
      "omp single", "omp unknown_directive"
  };
  for (const char* dir : directives) {
      state.omp_pragmas[&stmt] = dir;
      stmt.has_associated_pragma = TRUE;
      assert(llvm_lower_statement(&stmt) == llvm_gen_be_error_t::ok);
  }

  // Exhaustive branch coverage for llvm_gen_be_omp.cpp
  {
    bool handled = false;
    assert(llvm_dispatch_omp_pragma_stmt(nullptr, &handled) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_dispatch_omp_pragma_stmt(&stmt, nullptr) == llvm_gen_be_error_t::invalid_argument);
    stmt.has_associated_pragma = FALSE;
    assert(llvm_dispatch_omp_pragma_stmt(&stmt, &handled) == llvm_gen_be_error_t::ok);
    stmt.has_associated_pragma = TRUE;
    state.omp_pragmas.erase(&stmt);
    assert(llvm_dispatch_omp_pragma_stmt(&stmt, &handled) == llvm_gen_be_error_t::ok); // directive == nullptr

    // Attribute-based directive text extraction (null args, non-token kind, null token, valid token)
    an_attribute attr1 = {}, attr2 = {}, attr3 = {}, attr4 = {};
    an_attribute_arg aarg2 = {}, aarg3 = {}, aarg4 = {};
    attr1.arguments = nullptr; attr1.next = &attr2;
    aarg2.kind = static_cast<an_attribute_arg_kind>(99); attr2.arguments = &aarg2; attr2.next = &attr3;
    aarg3.kind = aak_token; aarg3.variant.token = nullptr; attr3.arguments = &aarg3; attr3.next = &attr4;
    aarg4.kind = aak_token; aarg4.variant.token = (char*)"omp barrier"; attr4.arguments = &aarg4; attr4.next = nullptr;
    stmt.attributes = &attr1;
    assert(llvm_dispatch_omp_pragma_stmt(&stmt, &handled) == llvm_gen_be_error_t::ok);
    stmt.attributes = nullptr;
    stmt.has_associated_pragma = FALSE;

    // Empty function name and empty module source file name in get_or_create_ident_t
    state.module->setSourceFileName("");
    llvm::Function* anon_f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "", state.module.get());
    llvm::BasicBlock* anon_bb = llvm::BasicBlock::Create(*state.context, "entry", anon_f);
    state.builder->SetInsertPoint(anon_bb);
    assert(lower_omp_barrier(&stmt) == llvm_gen_be_error_t::ok);
    state.builder->SetInsertPoint(bb);

    // Parallel region: null var/alloca in local_vars, unnamed var, untyped private var, unclosed clause, multiple clauses, get_llvm_type error, terminated body, and body error
    state.local_vars[nullptr] = local_alloca;
    a_variable null_alloca_var = {}, unnamed_var = {}, untyped_priv_var = {};
    state.local_vars[&null_alloca_var] = nullptr;
    unnamed_var.source_corresp.name = nullptr;
    state.local_vars[&unnamed_var] = local_alloca;
    untyped_priv_var.source_corresp.name = (char*)"untyped_p";
    untyped_priv_var.type = nullptr;
    state.local_vars[&untyped_priv_var] = local_alloca;

    state.omp_pragmas[&stmt] = "private(other) private(untyped_p) firstprivate(unclosed";
    assert(lower_omp_parallel_region(&stmt) == llvm_gen_be_error_t::ok);

    // Body that terminates the block (stmk_return) inside parallel region, target region, and task
    a_statement ret_body = {};
    ret_body.kind = stmk_return;
    assert(lower_omp_parallel_region(&ret_body) == llvm_gen_be_error_t::ok);

    // Error when get_llvm_type fails on private var
    state.omp_pragmas[&stmt] = "private(priv_v)";
    g_omp_fail_type = 1;
    assert(lower_omp_parallel_region(&stmt) == llvm_gen_be_error_t::unsupported_type);
    g_omp_fail_type = 0;

    // Error when llvm_lower_statement fails (unsupported statement kind 99)
    a_statement bad_body = {};
    bad_body.kind = static_cast<a_statement_kind>(99);
    assert(lower_omp_parallel_region(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_for_worksharing(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_critical(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_target_region(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_task(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_taskgroup(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_reduce(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_sections(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_single(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);

    // Target region and task where kernel/task body terminates (stmk_return in current function too, so restoreIP to a fresh block afterwards)
    llvm::BasicBlock* tb1 = llvm::BasicBlock::Create(*state.context, "tb1", f);
    state.builder->SetInsertPoint(tb1);
    assert(lower_omp_target_region(&ret_body) == llvm_gen_be_error_t::ok);

    llvm::BasicBlock* tb2 = llvm::BasicBlock::Create(*state.context, "tb2", f);
    state.builder->SetInsertPoint(tb2);
    // For lower_omp_task, task_entry_fn returns i32, so use a block with an unreachable/br terminator
    a_label dummy_lbl = {};
    a_statement goto_body = {};
    goto_body.kind = stmk_goto;
    goto_body.variant.label.ptr = &dummy_lbl;
    assert(lower_omp_task(&goto_body) == llvm_gen_be_error_t::ok);

    llvm::BasicBlock* tb3 = llvm::BasicBlock::Create(*state.context, "tb3", f);
    state.builder->SetInsertPoint(tb3);

    // Worksharing for loop branches:
    // 1. fl->initialization == nullptr, stmt->expr == nullptr, fl->increment == nullptr
    for_stmt.has_associated_pragma = FALSE;
    for_extra.initialization = nullptr;
    for_stmt.expr = nullptr;
    for_extra.increment = nullptr;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);

    // 3. stmk_init with dynamic_init == nullptr, dik != dik_expression, expression == nullptr, i64 lb_expr, and expr error
    for_extra.initialization = &init_stmt;
    init_stmt.variant.dynamic_init = nullptr;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);
    init_stmt.variant.dynamic_init = &init_dip;
    init_dip.kind = dik_none;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);
    init_dip.kind = dik_expression;
    init_dip.variant.expression = nullptr;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);
    init_dip.variant.expression = &lb_expr;
    g_omp_expr_i64 = 1;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);
    g_omp_expr_i64 = 0;
    g_omp_fail_expr = 1;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::unsupported_expr);
    g_omp_fail_expr = 0;

    // 4. stmk_expr assignment initialization (eok_assign, lhs enk_variable & non-variable, rhs present & absent, i64, error, non-assign)
    a_statement expr_init_s = {};
    expr_init_s.kind = stmk_expr;
    for_extra.initialization = &expr_init_s;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok); // expr == nullptr

    an_expr_node assign_expr = {}, lhs_node = {}, rhs_node = {};
    assign_expr.kind = enk_constant;
    expr_init_s.expr = &assign_expr;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok); // kind != enk_operation

    assign_expr.kind = enk_operation;
    assign_expr.variant.operation.kind = eok_add;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok); // kind != eok_assign

    assign_expr.variant.operation.kind = eok_assign;
    assign_expr.variant.operation.operands = nullptr;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok); // lhs == nullptr

    lhs_node.kind = enk_constant;
    assign_expr.variant.operation.operands = &lhs_node;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok); // lhs != enk_variable, rhs == nullptr

    a_variable unmapped_ind = {};
    lhs_node.kind = enk_variable;
    lhs_node.variant.variable.ptr = &unmapped_ind; // ind_var not in local_vars!
    lhs_node.next = &rhs_node;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);

    g_omp_expr_i64 = 1;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);
    g_omp_expr_i64 = 0;
    g_omp_fail_expr = 1;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::unsupported_expr);
    g_omp_fail_expr = 0;

    // 5. Condition expression: kind != enk_operation, op1 == nullptr, op2 == nullptr, eok_le, i64 ub_expr, error
    for_extra.initialization = nullptr;
    cond_expr.kind = enk_constant;
    for_stmt.expr = &cond_expr;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);

    cond_expr.kind = enk_operation;
    cond_expr.variant.operation.operands = nullptr;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);

    cmp_op1.next = nullptr;
    cond_expr.variant.operation.operands = &cmp_op1;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);

    cmp_op1.next = &cmp_op2;
    cond_expr.variant.operation.kind = eok_le;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);

    g_omp_expr_i64 = 1;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);
    g_omp_expr_i64 = 0;
    g_omp_fail_expr = 1;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::unsupported_expr);
    g_omp_fail_expr = 0;

    // 6. Increment expression: i64 step_expr_val and error
    for_stmt.expr = nullptr;
    for_extra.increment = &inc_expr;
    g_omp_expr_i64 = 1;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);
    g_omp_expr_i64 = 0;
    g_omp_fail_expr = 1;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::unsupported_expr);
    g_omp_fail_expr = 0;

    // 7. Phase 10 directives: taskyield, flush, ordered, simd, teams distribute, target map(...), task depend(...)
    assert(lower_omp_taskyield(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_flush(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_ordered(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_simd(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_teams_distribute(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_taskloop(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_master(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_taskloop(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_master(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_taskloop(&goto_body) == llvm_gen_be_error_t::ok);
    assert(lower_omp_master(&stmt) == llvm_gen_be_error_t::ok);

    const char* p10_pragmas[] = {
        "omp taskyield",
        "omp flush",
        "omp ordered",
        "omp simd",
        "omp teams distribute",
        "omp taskloop",
        "omp master",
        "omp masked",
        "omp target update to(x)",
        "omp cancel parallel",
        "omp cancellation point parallel",
        "omp target map(to:x)",
        "omp target map(from:x)",
        "omp target map(alloc:x)",
        "omp task depend(in:x)",
        "omp task depend(out:x)",
        "omp task depend(inout:x)"
    };
    for (const char* p_str : p10_pragmas) {
      bool handled = false;
      stmt.has_associated_pragma = TRUE;
      state.omp_pragmas[&stmt] = p_str;
      assert(llvm_dispatch_omp_pragma_stmt(&stmt, &handled) == llvm_gen_be_error_t::ok);
      assert(handled);
    }
    state.omp_pragmas.erase(&stmt);
    stmt.has_associated_pragma = FALSE;

    // Test collapse(2) and lastprivate(i) on lower_omp_for_worksharing
    a_statement inner_for = {};
    inner_for.kind = stmk_for;
    inner_for.variant.for_loop.extra_info = &for_extra;
    inner_for.variant.for_loop.statement = &body_stmt;

    for_stmt.variant.for_loop.statement = &inner_for;
    for_extra.initialization = &init_stmt;
    init_dip.variable = &ind_var;
    state.omp_pragmas[&for_stmt] = "omp for collapse(2) lastprivate(i)";
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);

    // Also test collapse(2) on non-for statement, for with null body, and for with non-for body
    state.omp_pragmas[&stmt] = "omp for collapse(2) lastprivate(i)";
    assert(lower_omp_for_worksharing(&stmt) == llvm_gen_be_error_t::ok);
    state.omp_pragmas.erase(&stmt);

    for_stmt.variant.for_loop.statement = nullptr;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);
    for_stmt.variant.for_loop.statement = &body_stmt;
    // Also test lastprivate when ind_var is unmapped
    init_dip.variable = &unmapped_ind;
    assert(lower_omp_for_worksharing(&for_stmt) == llvm_gen_be_error_t::ok);
    init_dip.variable = &ind_var;
    state.omp_pragmas.erase(&for_stmt);

    // Error propagation in lower_omp_ordered, lower_omp_simd, lower_omp_teams_distribute (and terminated teams body)
    assert(lower_omp_ordered(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_simd(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_teams_distribute(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_teams_distribute(&ret_body) == llvm_gen_be_error_t::ok);

    // 8. Phase 19: EDG a_pragma_ptr AST node extraction in be_state->active_pragma_list and device offload bitcode
    a_pragma p_other = {}, p_null_txt = {}, p_match = {};
    p_other.entity.ptr = reinterpret_cast<char*>(&for_stmt);
    p_other.pragma_text = "omp barrier";
    p_null_txt.entity.ptr = reinterpret_cast<char*>(&stmt);
    p_null_txt.pragma_text = nullptr;
    p_match.entity.ptr = reinterpret_cast<char*>(&stmt);
    p_match.pragma_text = "omp barrier";
    p_other.next = &p_null_txt;
    p_null_txt.next = &p_match;
    state.active_pragma_list = &p_other;
    stmt.has_associated_pragma = TRUE;
    bool handled_ast = false;
    assert(llvm_dispatch_omp_pragma_stmt(&stmt, &handled_ast) == llvm_gen_be_error_t::ok);
    assert(handled_ast);
    p_null_txt.next = nullptr;
    assert(llvm_dispatch_omp_pragma_stmt(&stmt, &handled_ast) == llvm_gen_be_error_t::ok);
    assert(!handled_ast);
    stmt.has_associated_pragma = FALSE;
    state.active_pragma_list = nullptr;

    assert(llvm_emit_omp_device_offload_module(nullptr, "nvptx64-nvidia-cuda") == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_emit_omp_device_offload_module(state.module.get(), nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_emit_omp_device_offload_module(state.module.get(), "nvptx64-nvidia-cuda") == llvm_gen_be_error_t::ok);
    assert(llvm_emit_omp_device_offload_module(state.module.get(), "amdgcn-amd-amdhsa") == llvm_gen_be_error_t::ok);
    assert(state.module->getNamedGlobal("__llvm_offloading_device_bc") != nullptr);
    assert(state.module->getNamedGlobal(".omp_offloading.device_image") != nullptr);
    assert(state.module->getNamedGlobal(".omp_offloading.descriptor") != nullptr);
    assert(state.module->getFunction(".omp_offloading.descriptor_reg") != nullptr);
    assert(state.module->getFunction(".omp_offloading.descriptor_unreg") != nullptr);

    // Phase 2 OpenMP 5.x Device Offload & Fatbinary Bundling tests
    std::vector<omp_map_clause_entry_t> map_entries;
    assert(parse_omp_map_clauses(nullptr, &map_entries) == llvm_gen_be_error_t::invalid_argument);
    assert(parse_omp_map_clauses("map(to:x)", nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(parse_omp_map_clauses("map(unclosed", &map_entries) == llvm_gen_be_error_t::ok);
    assert(parse_omp_map_clauses(
               " to(shared_x) from(shared_x) map(tofrom: shared_x, , s.field) "
               "map(always, to: shared_x[2:16], shared_x[0:8], shared_x[a:b], shared_x[4], shared_x[4) "
               "map(shared_x[1:4]) map(from: unmapped_v) map(alloc: shared_x) "
               "map(delete: shared_x) map(shared_x) to(shared_x)",
               &map_entries) == llvm_gen_be_error_t::ok);
    assert(!map_entries.empty());

    assert(lower_omp_target_data(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_target_enter_data(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_target_exit_data(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_target_update(nullptr) == llvm_gen_be_error_t::invalid_argument);

    assert(lower_omp_target_data(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_target_data(&stmt) == llvm_gen_be_error_t::ok);
    assert(lower_omp_target_enter_data(&stmt) == llvm_gen_be_error_t::ok);
    assert(lower_omp_target_exit_data(&stmt) == llvm_gen_be_error_t::ok);
    assert(lower_omp_target_update(&stmt) == llvm_gen_be_error_t::ok);

    const char* p2_pragmas[] = {
        "omp target data map(tofrom: shared_x[2:16], s.field)",
        "omp target enter data map(to: shared_x)",
        "omp target exit data map(release: shared_x)",
        "omp target update from(shared_x)",
        "omp target nowait depend(in: shared_x) map(always, tofrom: shared_x[2:8])"
    };
    for (const char* p_str : p2_pragmas) {
      bool handled = false;
      stmt.has_associated_pragma = TRUE;
      state.omp_pragmas[&stmt] = p_str;
      assert(llvm_dispatch_omp_pragma_stmt(&stmt, &handled) == llvm_gen_be_error_t::ok);
      assert(handled);
    }
    state.omp_pragmas.erase(&stmt);
    stmt.has_associated_pragma = FALSE;

    // Test compile_omp_device_module with declare target function and empty/non-empty arch
    llvm::Function* decl_tgt_fn = llvm::Function::Create(
        ft, llvm::Function::ExternalLinkage, "dev_helper_fn", state.module.get());
    decl_tgt_fn->addFnAttr("omp_declare_target");
    llvm::SmallVector<char, 512> dev_img;
    assert(compile_omp_device_module(nullptr, "nvptx64-nvidia-cuda", "sm_80", &dev_img) ==
           llvm_gen_be_error_t::invalid_argument);
    assert(compile_omp_device_module(state.module.get(), "nvptx64-nvidia-cuda", "sm_80", nullptr) ==
           llvm_gen_be_error_t::invalid_argument);
    assert(compile_omp_device_module(state.module.get(), "nvptx64-nvidia-cuda", "", &dev_img) ==
           llvm_gen_be_error_t::ok);
    assert(!dev_img.empty());
    assert(compile_omp_device_module(state.module.get(), "amdgcn-amd-amdhsa", "gfx90a", &dev_img) ==
           llvm_gen_be_error_t::ok);
    assert(!dev_img.empty());

    // Test lower_omp_declare_simd, lower_omp_declare_reduction, and lower_omp_atomic (read, write, update, capture, compare) + omp for simd
    assert(lower_omp_declare_simd(nullptr, 0) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_declare_simd(f, 0) == llvm_gen_be_error_t::ok);
    assert(lower_omp_declare_simd(f, 8) == llvm_gen_be_error_t::ok);
    assert(f->hasFnAttribute("vector-function-abi-variant"));

    llvm::Function* udr_comb_fn = nullptr;
    llvm::Type* i32_llvm_ty = llvm::Type::getInt32Ty(*state.context);
    llvm::Type* f32_llvm_ty = llvm::Type::getFloatTy(*state.context);
    assert(lower_omp_declare_reduction(nullptr, i32_llvm_ty, &udr_comb_fn) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_declare_reduction("my_sum", nullptr, &udr_comb_fn) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_declare_reduction("my_sum", i32_llvm_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_declare_reduction("my_sum", i32_llvm_ty, &udr_comb_fn) == llvm_gen_be_error_t::ok);
    assert(udr_comb_fn != nullptr);
    assert(state.module->getFunction(".omp.user_reduction.init.my_sum") != nullptr);
    // Second call with same id reuses existing combiner and init functions
    assert(lower_omp_declare_reduction("my_sum", i32_llvm_ty, &udr_comb_fn) == llvm_gen_be_error_t::ok);
    // Floating-point reduction type exercises FP combiner branch
    assert(lower_omp_declare_reduction("my_fsum", f32_llvm_ty, &udr_comb_fn) == llvm_gen_be_error_t::ok);

    assert(lower_omp_atomic(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(lower_omp_atomic(&bad_body) == llvm_gen_be_error_t::unsupported_stmt);
    assert(lower_omp_atomic(&stmt) == llvm_gen_be_error_t::ok);

    const char* atomic_and_simd_pragmas[] = {
        "omp atomic read",
        "omp atomic write",
        "omp atomic update",
        "omp atomic capture",
        "omp atomic compare",
        "omp for simd"
    };
    for (const char* p_str : atomic_and_simd_pragmas) {
      bool handled = false;
      stmt.has_associated_pragma = TRUE;
      state.omp_pragmas[&stmt] = p_str;
      assert(llvm_dispatch_omp_pragma_stmt(&stmt, &handled) == llvm_gen_be_error_t::ok);
      assert(handled);
    }
    bad_body.has_associated_pragma = TRUE;
    state.omp_pragmas[&bad_body] = "omp for simd";
    bool bad_handled = false;
    assert(llvm_dispatch_omp_pragma_stmt(&bad_body, &bad_handled) == llvm_gen_be_error_t::unsupported_stmt);
    state.omp_pragmas.erase(&bad_body);
    bad_body.has_associated_pragma = FALSE;
  }

  // ===========================================================================
  // Wave 2: Exhaustive function, line, region, and branch coverage for omp
  // ===========================================================================
  {
    g_omp_rich_types = 1;
    int_kind_is_signed[ik_int] = TRUE;
    int_kind_is_signed[ik_long_long] = TRUE;
    int_kind_is_signed[ik_unsigned_int] = FALSE;
    int_kind_is_signed[ik_unsigned_long_long] = FALSE;

    auto run_oom_sweep = [&](auto&& fn) noexcept {
      for (int b = 0; b < 64; ++b) {
        llvm::BasicBlock* cur_b = state.builder->GetInsertBlock();
        if (!cur_b || EDG_LLVM_GET_TERMINATOR(cur_b)) {
          llvm::BasicBlock* fresh_bb = llvm::BasicBlock::Create(*state.context, "oom.fresh", f);
          state.builder->SetInsertPoint(fresh_bb);
        }
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = fn();
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }
    };

    auto ensure_open_bb = [&]() noexcept {
      llvm::BasicBlock* cur_b = state.builder->GetInsertBlock();
      if (!cur_b || EDG_LLVM_GET_TERMINATOR(cur_b)) {
        llvm::BasicBlock* fresh_bb = llvm::BasicBlock::Create(*state.context, "w2.bb", f);
        state.builder->SetInsertPoint(fresh_bb);
      }
    };

    // 1) Clause & identifier boundary edge cases + reductions + C++ privatization in lower_omp_parallel_region
    a_type int32_edg = {}; int32_edg.kind = tk_integer; int32_edg.size = 4; int32_edg.variant.integer.int_kind = ik_int;
    a_type uint32_edg = {}; uint32_edg.kind = tk_integer; uint32_edg.size = 4; uint32_edg.variant.integer.int_kind = ik_unsigned_int;
    a_type int64_edg = {}; int64_edg.kind = tk_integer; int64_edg.size = 8; int64_edg.variant.integer.int_kind = ik_long_long;
    a_type uint64_edg = {}; uint64_edg.kind = tk_integer; uint64_edg.size = 8; uint64_edg.variant.integer.int_kind = ik_unsigned_long_long;
    a_type char_edg = {}; char_edg.kind = tk_integer; char_edg.size = 1; char_edg.variant.integer.int_kind = ik_int;
    a_type flt_edg = {}; flt_edg.kind = tk_float; flt_edg.size = 4;
    a_type void_edg = {}; void_edg.kind = tk_void;
    a_type ptr_int_edg = {}; ptr_int_edg.kind = tk_pointer; ptr_int_edg.size = 8; ptr_int_edg.variant.pointer.type = &int32_edg;
    a_type ptr_char_edg = {}; ptr_char_edg.kind = tk_pointer; ptr_char_edg.size = 8; ptr_char_edg.variant.pointer.type = &char_edg;
    a_type ptr_void_edg = {}; ptr_void_edg.kind = tk_pointer; ptr_void_edg.size = 8; ptr_void_edg.variant.pointer.type = &void_edg;
    a_type ptr_null_edg = {}; ptr_null_edg.kind = tk_pointer; ptr_null_edg.size = 8; ptr_null_edg.variant.pointer.type = nullptr;
    a_type tref_int_edg = {}; tref_int_edg.kind = tk_typeref; tref_int_edg.variant.typeref.type = &int32_edg;
    a_type tref_uint_edg = {}; tref_uint_edg.kind = tk_typeref; tref_uint_edg.variant.typeref.type = &uint32_edg;

    llvm::Type* i32_t = llvm::Type::getInt32Ty(*state.context);
    llvm::Type* i64_t = llvm::Type::getInt64Ty(*state.context);
    llvm::Type* f32_t = llvm::Type::getFloatTy(*state.context);
    llvm::Type* ptr_t = llvm::PointerType::getUnqual(*state.context);

    // Test identifier boundary cases: empty name "", '$' and '9' and '_', prefix/suffix non-matches,
    // prev_ident on "firstprivate(" / "my_reduction(", colon modifier "private(mod: v$1)", unclosed parens
    {
      state.local_vars.clear();
      ensure_open_bb();
      a_variable v_empty_nm = {}, v_dollar = {}, v_untyped_red = {};
      v_empty_nm.source_corresp.name = (char*)"";
      v_empty_nm.type = &int32_edg;
      v_dollar.source_corresp.name = (char*)"v$9_a";
      v_dollar.type = &tref_int_edg;
      v_untyped_red.source_corresp.name = (char*)"u_red";
      v_untyped_red.type = nullptr;

      state.local_vars[&v_empty_nm] = state.builder->CreateAlloca(i32_t, nullptr, "v_emp");
      state.local_vars[&v_dollar] = state.builder->CreateAlloca(i32_t, nullptr, "v_dol");
      state.local_vars[&v_untyped_red] = state.builder->CreateAlloca(i32_t, nullptr, "u_red");

      a_statement p_stmt = {}; p_stmt.kind = stmk_empty;
      state.omp_pragmas[&p_stmt] =
          "omp parallel my_private(v$9_a) firstprivate(pre_v$9_a, v$9_a_post) "
          "private(mod: x1, v$9_a) my_reduction(+:u_red) reduction(nocolon) "
          "reduction(task, +: pre_u_red, u_red_post, u_red) reduction(+:unclosed";
      run_oom_sweep([&]() noexcept { return lower_omp_parallel_region(&p_stmt); });

      p_stmt.kind = stmk_empty;
      state.omp_pragmas[&p_stmt] = "omp parallel private(unclosed";
      assert(lower_omp_parallel_region(&p_stmt) == llvm_gen_be_error_t::ok);
      state.omp_pragmas.erase(&p_stmt);
    }

    // Test all reduction operators across signed int, unsigned int, float, and pointer
    {
      const char* ops[] = {"+", "-", "*", "&", "|", "^", "&&", "||", "min", "max", "custom_op"};
      for (const char* op : ops) {
        state.local_vars.clear();
        ensure_open_bb();
        a_variable vs = {}, vu = {}, vf = {}, vp = {};
        vs.source_corresp.name = (char*)"rs"; vs.type = &int32_edg;
        vu.source_corresp.name = (char*)"ru"; vu.type = &tref_uint_edg;
        vf.source_corresp.name = (char*)"rf"; vf.type = &flt_edg;
        vp.source_corresp.name = (char*)"rp"; vp.type = (std::string(op) == "+") ? &ptr_int_edg : &int32_edg;
        state.local_vars[&vs] = state.builder->CreateAlloca(i32_t, nullptr, "rs");
        state.local_vars[&vu] = state.builder->CreateAlloca(i32_t, nullptr, "ru");
        state.local_vars[&vf] = state.builder->CreateAlloca(f32_t, nullptr, "rf");
        state.local_vars[&vp] = state.builder->CreateAlloca((std::string(op) == "+") ? ptr_t : i32_t, nullptr, "rp");

        a_statement r_stmt = {}; r_stmt.kind = stmk_empty;
        std::string dir = std::string("omp parallel reduction(") + op + ": rs, ru, rf, rp)";
        state.omp_pragmas[&r_stmt] = dir.c_str();
        assert(lower_omp_parallel_region(&r_stmt) == llvm_gen_be_error_t::ok);
      }
      // Run OOM sweep on float * reduction (exercises CAS loop + emit_omp_reduction_binop OOM inside combiner & CAS)
      state.local_vars.clear();
      ensure_open_bb();
      a_variable vf_oom = {}; vf_oom.source_corresp.name = (char*)"rf_oom"; vf_oom.type = &flt_edg;
      state.local_vars[&vf_oom] = state.builder->CreateAlloca(f32_t, nullptr, "rf_oom");
      a_statement r_oom_stmt = {}; r_oom_stmt.kind = stmk_empty;
      state.omp_pragmas[&r_oom_stmt] = "omp parallel reduction(*: rf_oom)";
      run_oom_sweep([&]() noexcept { return lower_omp_parallel_region(&r_oom_stmt); });
    }

    // Test C++ class private / firstprivate constructors & destructors (both arg_size >= 1/2 and 0-arg fallback)
    {
      // Build EDG class with: deleted ctor, compiler-generated def/copy ctors, user-defined def/copy ctors,
      // non-ref 1-arg ctor, 2-arg ctor, compiler-generated dtor, and user-defined dtor
      a_param_type p_ref = {}, p_val = {}, p_two1 = {}, p_two2 = {};
      a_type ref_ty = {}; ref_ty.kind = tk_pointer; ref_ty.variant.pointer.is_reference = TRUE;
      p_ref.type = &ref_ty; p_ref.next = nullptr;
      p_val.type = &int32_edg; p_val.next = nullptr;
      p_two1.type = &ref_ty; p_two1.next = &p_two2; p_two2.type = &int32_edg; p_two2.next = nullptr;

      a_routine_type_supplement rts_0 = {}, rts_ref = {}, rts_val = {}, rts_two = {};
      rts_0.param_type_list = nullptr;
      rts_ref.param_type_list = &p_ref;
      rts_val.param_type_list = &p_val;
      rts_two.param_type_list = &p_two1;

      a_type rty_0 = {}, rty_ref = {}, rty_val = {}, rty_two = {};
      rty_0.kind = tk_routine; rty_0.variant.routine.extra_info = &rts_0;
      rty_ref.kind = tk_routine; rty_ref.variant.routine.extra_info = &rts_ref;
      rty_val.kind = tk_routine; rty_val.variant.routine.extra_info = &rts_val;
      rty_two.kind = tk_routine; rty_two.variant.routine.extra_info = &rts_two;

      // In llvm_get_or_declare_routine, routine->type param count determines Function arg count:
      // For def_ctor with 1 LLVM arg (`this`), give it 1 param in a separate routine for arg_size()>=1,
      // and 0 params (`rty_0`) for the `arg_size() == 0` fallback!
      a_routine r_del = {}, r_cg_def = {}, r_ud_def = {}, r_cg_cpy = {}, r_ud_cpy = {};
      a_routine r_nonref = {}, r_2arg = {}, r_other = {}, r_cg_dtor = {}, r_ud_dtor = {};
      r_del.special_kind = sfk_constructor; r_del.is_deleted = TRUE; r_del.next = &r_cg_def;
      r_cg_def.special_kind = sfk_constructor; r_cg_def.compiler_generated = TRUE; r_cg_def.type = &rty_0;
      r_cg_def.source_corresp.name = (char*)"cls_cg_def_ctor"; r_cg_def.next = &r_ud_def;
      r_ud_def.special_kind = sfk_constructor; r_ud_def.compiler_generated = FALSE; r_ud_def.type = &rty_0;
      r_ud_def.source_corresp.name = (char*)"cls_ud_def_ctor"; r_ud_def.next = &r_cg_cpy;
      r_cg_cpy.special_kind = sfk_constructor; r_cg_cpy.compiler_generated = TRUE; r_cg_cpy.type = &rty_ref;
      r_cg_cpy.source_corresp.name = (char*)"cls_cg_cpy_ctor"; r_cg_cpy.next = &r_ud_cpy;
      r_ud_cpy.special_kind = sfk_constructor; r_ud_cpy.compiler_generated = FALSE; r_ud_cpy.type = &rty_ref;
      r_ud_cpy.source_corresp.name = (char*)"cls_ud_cpy_ctor"; r_ud_cpy.next = &r_nonref;
      r_nonref.special_kind = sfk_constructor; r_nonref.type = &rty_val; r_nonref.next = &r_2arg;
      r_2arg.special_kind = sfk_constructor; r_2arg.type = &rty_two; r_2arg.next = &r_other;
      r_other.special_kind = sfk_none; r_other.next = &r_cg_dtor;
      r_cg_dtor.special_kind = sfk_destructor; r_cg_dtor.compiler_generated = TRUE; r_cg_dtor.next = &r_ud_dtor;
      r_ud_dtor.special_kind = sfk_destructor; r_ud_dtor.compiler_generated = FALSE; r_ud_dtor.type = &rty_0;
      r_ud_dtor.source_corresp.name = (char*)"cls_ud_dtor"; r_ud_dtor.next = nullptr;

      a_scope cls_scope = {}; cls_scope.routines = &r_del;
      a_class_type_supplement cls_extra = {}; cls_extra.assoc_scope = &cls_scope;
      a_type cls_ty = {}; cls_ty.kind = tk_class; cls_ty.size = 16; cls_ty.variant.class_struct_union.extra_info = &cls_extra;

      llvm::Type* cls_llvm_ty = nullptr;
      assert(get_llvm_type(&cls_ty, &cls_llvm_ty) == llvm_gen_be_error_t::ok);

      // Case A: Pre-create LLVM functions with 0 args so `arg_size() >= 1` / `>= 2` are FALSE (hits fallback!)
      llvm::FunctionType* void0_fty = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {}, false);
      llvm::Function::Create(void0_fty, llvm::GlobalValue::ExternalLinkage, "cls_ud_def_ctor", state.module.get());
      llvm::Function::Create(void0_fty, llvm::GlobalValue::ExternalLinkage, "cls_ud_cpy_ctor", state.module.get());
      llvm::Function::Create(void0_fty, llvm::GlobalValue::ExternalLinkage, "cls_ud_dtor", state.module.get());

      state.local_vars.clear();
      ensure_open_bb();
      a_variable vc_priv = {}, vc_fpriv = {};
      vc_priv.source_corresp.name = (char*)"obj_p"; vc_priv.type = &cls_ty;
      vc_fpriv.source_corresp.name = (char*)"obj_fp"; vc_fpriv.type = &cls_ty;
      state.local_vars[&vc_priv] = state.builder->CreateAlloca(cls_llvm_ty, nullptr, "obj_p");
      state.local_vars[&vc_fpriv] = state.builder->CreateAlloca(cls_llvm_ty, nullptr, "obj_fp");

      a_statement c_stmt = {}; c_stmt.kind = stmk_empty;
      state.omp_pragmas[&c_stmt] = "omp parallel private(obj_p) firstprivate(obj_fp)";
      assert(lower_omp_parallel_region(&c_stmt) == llvm_gen_be_error_t::ok);

      // Case B: Pre-create LLVM functions with >=1 / >=2 args so `arg_size() >= 1` / `>= 2` are TRUE + OOM sweep!
      r_ud_def.source_corresp.name = (char*)"cls_ud_def_ctor_1";
      r_ud_cpy.source_corresp.name = (char*)"cls_ud_cpy_ctor_2";
      r_ud_dtor.source_corresp.name = (char*)"cls_ud_dtor_1";
      llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {ptr_t}, false),
                             llvm::GlobalValue::ExternalLinkage, "cls_ud_def_ctor_1", state.module.get());
      llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {ptr_t, ptr_t}, false),
                             llvm::GlobalValue::ExternalLinkage, "cls_ud_cpy_ctor_2", state.module.get());
      llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {ptr_t}, false),
                             llvm::GlobalValue::ExternalLinkage, "cls_ud_dtor_1", state.module.get());
      run_oom_sweep([&]() noexcept { return lower_omp_parallel_region(&c_stmt); });

      // Case C: Class with NO extra_info / empty scope / compiler-generated second ctor (`!def_ctor` / `!copy_ctor` false)
      a_routine r_cg_def2 = r_cg_def; r_cg_def2.next = nullptr;
      r_cg_def.next = &r_cg_def2;
      a_routine r_cg_cpy2 = r_cg_cpy; r_cg_cpy2.next = nullptr;
      r_cg_def2.next = &r_cg_cpy; r_cg_cpy.next = &r_cg_cpy2;
      r_cg_def.source_corresp.name = nullptr; // null name falls back to bitwise/zero init!
      r_cg_cpy.source_corresp.name = nullptr;
      assert(lower_omp_parallel_region(&c_stmt) == llvm_gen_be_error_t::ok);

      // Case D: Class with null extra_info and non-routine ctor type
      a_routine r_not_rt = {}; r_not_rt.special_kind = sfk_constructor; r_not_rt.type = &int32_edg;
      cls_scope.routines = &r_not_rt;
      assert(lower_omp_parallel_region(&c_stmt) == llvm_gen_be_error_t::ok);
      cls_ty.variant.class_struct_union.extra_info = nullptr;
      assert(lower_omp_parallel_region(&c_stmt) == llvm_gen_be_error_t::ok);
    }

    // Combined parallel constructs: `omp parallel for`, `omp parallel for simd`, `omp parallel sections`
    {
      state.local_vars.clear();
      ensure_open_bb();
      a_statement comb_stmt = {}; comb_stmt.kind = stmk_empty;
      state.omp_pragmas[&comb_stmt] = "omp parallel for";
      assert(lower_omp_parallel_region(&comb_stmt) == llvm_gen_be_error_t::ok);
      state.omp_pragmas[&comb_stmt] = "omp parallel for simd";
      run_oom_sweep([&]() noexcept { return lower_omp_parallel_region(&comb_stmt); });
      state.omp_pragmas[&comb_stmt] = "omp parallel sections";
      assert(lower_omp_parallel_region(&comb_stmt) == llvm_gen_be_error_t::ok);
    }

    // 2) Canonical & non-canonical `lower_omp_for_worksharing` + `extract_omp_canonical_loop_dim`
    {
      auto make_canonical_for = [&](a_variable* iv, a_type_ptr iv_ty, an_expr_operator_kind cmp_k,
                                    an_expr_node_ptr inc_e, a_statement* body_s,
                                    a_for_loop* fl_out, a_statement* init_s_out,
                                    a_dynamic_init* dip_out, an_expr_node* lb_out,
                                    an_expr_node* cmp_out, an_expr_node* op1_out, an_expr_node* op2_out) noexcept -> a_statement {
        *lb_out = {}; lb_out->kind = enk_constant; lb_out->type = iv_ty; lb_out->position.column = 1;
        *dip_out = {}; dip_out->kind = dik_expression; dip_out->variable = iv; dip_out->variant.expression = lb_out;
        *init_s_out = {}; init_s_out->kind = stmk_init; init_s_out->variant.dynamic_init = dip_out;
        *op1_out = {}; op1_out->kind = enk_variable; op1_out->variant.variable.ptr = iv; op1_out->type = iv_ty;
        *op2_out = {}; op2_out->kind = enk_constant; op2_out->type = iv_ty; op2_out->position.column = 10;
        op1_out->next = op2_out;
        *cmp_out = {}; cmp_out->kind = enk_operation; cmp_out->variant.operation.kind = cmp_k; cmp_out->variant.operation.operands = op1_out;
        *fl_out = {}; fl_out->initialization = init_s_out; fl_out->increment = inc_e;
        a_statement fs = {}; fs.kind = stmk_for; fs.expr = cmp_out; fs.variant.for_loop.extra_info = fl_out; fs.variant.for_loop.statement = body_s;
        return fs;
      };

      a_statement empty_b = {}; empty_b.kind = stmk_empty;
      a_statement ret_b = {}; ret_b.kind = stmk_return;
      a_statement bad_b = {}; bad_b.kind = static_cast<a_statement_kind>(999);

      /**
       * @struct LoopCase
       * @brief Describes a canonical OpenMP `for` loop test configuration.
       * @details Pairs induction variable type, comparison operator, increment operator, and pragma text.
       */
      struct LoopCase {
        a_type_ptr ty; ///< Induction variable EDG type.
        an_expr_operator_kind cmp; ///< Loop condition relational operator.
        an_expr_operator_kind inc_op; ///< Loop increment operator kind.
        const char* pragma; ///< OpenMP pragma directive string.
      };
      an_expr_node step_c2 = {}; step_c2.kind = enk_constant; step_c2.type = &int32_edg; step_c2.position.column = 2;
      an_expr_node step_flt = {}; step_flt.kind = enk_constant; step_flt.type = &flt_edg; step_flt.position.column = 2;

      LoopCase cases[] = {
          {&int32_edg, eok_lt, eok_pre_incr, "omp for schedule(static, 4) lastprivate(iv)"},
          {&uint32_edg, eok_le, eok_post_incr, "omp for schedule(static, 0) nowait"},
          {&int64_edg, eok_gt, eok_pre_decr, "omp for schedule(dynamic, 2) lastprivate(iv)"},
          {&uint64_edg, eok_ge, eok_post_decr, "omp for schedule(guided)"},
          {&uint32_edg, eok_lt, eok_add_assign, "omp for schedule(static, 2)"},
          {&uint64_edg, eok_gt, eok_subtract_assign, "omp for schedule(runtime) nowait"},
          {&ptr_int_edg, eok_lt, eok_padd_assign, "omp for schedule(auto) lastprivate(iv)"},
          {&ptr_char_edg, eok_ge, eok_psubtract_assign, "omp for lastprivate(iv)"},
          {&ptr_null_edg, eok_le, eok_pre_incr, "omp for schedule(unclosed"},
      };

      for (const LoopCase& lc : cases) {
        state.local_vars.clear();
        ensure_open_bb();
        a_variable iv = {}; iv.source_corresp.name = (char*)"iv"; iv.type = lc.ty;
        llvm::Type* v_ll_ty = nullptr;
        assert(get_llvm_type(lc.ty, &v_ll_ty) == llvm_gen_be_error_t::ok);
        state.local_vars[&iv] = state.builder->CreateAlloca(v_ll_ty, nullptr, "iv");

        an_expr_node inc_op1 = {}, inc_op2 = step_c2, inc_e = {};
        inc_op1.kind = enk_variable; inc_op1.variant.variable.ptr = &iv;
        inc_op1.next = &inc_op2;
        inc_e.kind = enk_operation; inc_e.variant.operation.kind = lc.inc_op; inc_e.variant.operation.operands = &inc_op1;

        a_for_loop fl = {}; a_statement init_s = {}; a_dynamic_init dip = {};
        an_expr_node lb = {}, cmp = {}, op1 = {}, op2 = {};
        a_statement fs = make_canonical_for(&iv, lc.ty, lc.cmp, &inc_e, &empty_b, &fl, &init_s, &dip, &lb, &cmp, &op1, &op2);
        state.omp_pragmas[&fs] = lc.pragma;
        assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);
      }

      // Test increment forms: non-int +=, `i = i + 2`, `i = i - 2`, `p = p + 2`, `p = p - 2`, `i = i + flt`, `i = i * 2`, `i = c`, `eok_comma`, non-op constant `0` and `2`, non-op float
      {
        state.local_vars.clear();
        ensure_open_bb();
        a_variable iv = {}; iv.source_corresp.name = (char*)"iv"; iv.type = &int32_edg;
        // Also test non-AllocaInst var_slot (GlobalVariable) with iv.type != nullptr!
        llvm::GlobalVariable* gv_iv = nullptr;
        assert(llvm_create_global_variable(state.module.get(), i32_t, false, llvm::GlobalValue::InternalLinkage,
                                           llvm::ConstantInt::get(i32_t, 0), "gv_iv_slot", &gv_iv) == llvm_gen_be_error_t::ok);
        state.local_vars[&iv] = gv_iv;

        // 1. += with float RHS (sv is not IntegerTy)
        an_expr_node op_v = {}, inc_add_flt = {};
        op_v.kind = enk_variable; op_v.next = &step_flt;
        inc_add_flt.kind = enk_operation; inc_add_flt.variant.operation.kind = eok_add_assign; inc_add_flt.variant.operation.operands = &op_v;

        a_for_loop fl = {}; a_statement init_s = {}; a_dynamic_init dip = {};
        an_expr_node lb = {}, cmp = {}, op1 = {}, op2 = {};
        a_statement fs = make_canonical_for(&iv, &int32_edg, eok_lt, &inc_add_flt, &empty_b, &fl, &init_s, &dip, &lb, &cmp, &op1, &op2);
        assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);

        // Fail get_llvm_type on non-AllocaInst var_slot (call #1 is lb, call #2 is ind_var->type) and fail expr on += step (call #3)!
        g_omp_fail_type_countdown = 1;
        assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::unsupported_type);
        g_omp_fail_type_countdown = 2;
        assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::unsupported_type);
        g_omp_fail_expr = 3;
        assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::unsupported_expr);
        g_omp_fail_expr = 0;

        // Non-AllocaInst var_slot with iv.type == nullptr AND float raw_lb/raw_ub (hits to_i64 non-ptr/non-int fallback!)
        iv.type = nullptr;
        lb.type = &flt_edg;
        op2.type = &flt_edg;
        assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);
        iv.type = &int32_edg;

        // 2. `i = i + 2`, `i = i - 2`, `p = p + 2`, `p = p - 2`, `i = i + flt`, `i = i * 2`, `i = 2` (non-operation RHS)
        an_expr_operator_kind rk_ops[] = {eok_add, eok_subtract, eok_padd, eok_psubtract, eok_multiply};
        for (an_expr_operator_kind rk : rk_ops) {
          an_expr_node rop1 = {}, rop2 = step_c2, rhs_op = {}, asgn_lhs = {}, inc_asgn = {};
          rop1.kind = enk_variable; rop1.next = &rop2;
          rhs_op.kind = enk_operation; rhs_op.variant.operation.kind = rk; rhs_op.variant.operation.operands = &rop1;
          asgn_lhs.kind = enk_variable; asgn_lhs.next = &rhs_op;
          inc_asgn.kind = enk_operation; inc_asgn.variant.operation.kind = eok_assign; inc_asgn.variant.operation.operands = &asgn_lhs;
          fl.increment = &inc_asgn;
          assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);
          // Fail expression lowering on rop2 (4th expr in extract_omp_canonical_loop_dim: lb=1, ub=2, rop2=3)
          if (rk == eok_add) {
            g_omp_fail_expr = 3;
            assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::unsupported_expr);
            g_omp_fail_expr = 0;
            // Also test non-int rop2 (`step_flt`)
            rop1.next = &step_flt;
            assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);
            // Also test rop2 == nullptr
            rop1.next = nullptr;
            assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);
          }
        }

        // `i = step_c2` (eok_assign where iop2->kind != enk_operation) and `eok_assign` with iop1 == nullptr
        {
          an_expr_node asgn_lhs = {}, inc_asgn = {};
          asgn_lhs.kind = enk_variable; asgn_lhs.next = &step_c2;
          inc_asgn.kind = enk_operation; inc_asgn.variant.operation.kind = eok_assign; inc_asgn.variant.operation.operands = &asgn_lhs;
          fl.increment = &inc_asgn;
          assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);
          inc_asgn.variant.operation.operands = nullptr;
          assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);
        }

        // Other operation (`eok_comma`) + fail expr
        {
          an_expr_node inc_comma = {}; inc_comma.kind = enk_operation; inc_comma.variant.operation.kind = eok_comma;
          fl.increment = &inc_comma;
          assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);
          g_omp_fail_expr = 3;
          assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::unsupported_expr);
          g_omp_fail_expr = 0;
        }

        // Non-operation increment: integer `0`, integer `2`, float `step_flt`, and fail expr
        {
          an_expr_node inc_c0 = {}; inc_c0.kind = enk_constant; inc_c0.type = &int32_edg; inc_c0.position.column = 0;
          fl.increment = &inc_c0;
          assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);
          fl.increment = &step_c2;
          assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);
          fl.increment = &step_flt;
          assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);
          g_omp_fail_expr = 3;
          assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::unsupported_expr);
          g_omp_fail_expr = 0;
        }

        // Initialization via `stmk_expr` (`i = 0`, non-variable LHS, null operands, fail expr on rhs)
        {
          an_expr_node init_lhs = {}, init_rhs = {}, init_asgn = {};
          init_lhs.kind = enk_variable; init_lhs.variant.variable.ptr = &iv; init_lhs.next = &init_rhs;
          init_rhs.kind = enk_constant; init_rhs.type = &int32_edg; init_rhs.position.column = 0;
          init_asgn.kind = enk_operation; init_asgn.variant.operation.kind = eok_assign; init_asgn.variant.operation.operands = &init_lhs;
          a_statement st_init_e = {}; st_init_e.kind = stmk_expr; st_init_e.expr = &init_asgn;
          fl.initialization = &st_init_e;
          fl.increment = nullptr;
          assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);

          g_omp_fail_expr = 1;
          assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::unsupported_expr);
          g_omp_fail_expr = 0;

          init_lhs.kind = enk_constant; // non-variable LHS -> ind_var == nullptr (`!out_dim->var_ty` uses raw_lb->getType()!)
          init_lhs.next = &init_rhs;
          assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);

          init_asgn.variant.operation.operands = nullptr; // lhs == nullptr & raw_lb == nullptr (`!out_dim->var_ty` uses i32_ty!)
          assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);

          // stmk_init with dynamic_init == nullptr, and dik_zero (`dip->kind != dik_expression`)
          a_statement st_init_null = {}; st_init_null.kind = stmk_init; st_init_null.variant.dynamic_init = nullptr;
          fl.initialization = &st_init_null;
          assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);
          dip.kind = dik_zero;
          fl.initialization = &init_s;
          assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);

          // Condition op2 returning nullptr (`has_cond_op2 == false`), op1 == nullptr, expr->kind != enk_operation
          dip.kind = dik_expression;
          g_omp_expr_null_val = 2;
          assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);
          cmp.variant.operation.operands = nullptr;
          assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);
          an_expr_node non_op_cond = {}; non_op_cond.kind = enk_constant;
          fs.expr = &non_op_cond;
          assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);
        }
      }

      // Test `collapse(2)`, `collapse(0)`, standalone `omp for reduction(+:rvar)` (canonical & non-canonical, static & dynamic, terminated & failing body), and OOM sweep!
      {
        state.local_vars.clear();
        ensure_open_bb();
        a_variable iv1 = {}, iv2 = {}, rvar = {}, rvar_untyped = {};
        iv1.source_corresp.name = (char*)"iv1"; iv1.type = &int32_edg;
        iv2.source_corresp.name = (char*)"iv2"; iv2.type = &int32_edg;
        rvar.source_corresp.name = (char*)"rvar"; rvar.type = &int32_edg;
        rvar_untyped.source_corresp.name = (char*)"rvar_u"; rvar_untyped.type = nullptr;
        state.local_vars[&iv1] = state.builder->CreateAlloca(i32_t, nullptr, "iv1");
        state.local_vars[&iv2] = state.builder->CreateAlloca(i32_t, nullptr, "iv2");
        state.local_vars[&rvar] = state.builder->CreateAlloca(i32_t, nullptr, "rvar");
        state.local_vars[&rvar_untyped] = state.builder->CreateAlloca(i32_t, nullptr, "rvar_u");

        an_expr_node inc1 = {}, inc2 = {};
        inc1.kind = enk_operation; inc1.variant.operation.kind = eok_pre_incr;
        inc2.kind = enk_operation; inc2.variant.operation.kind = eok_pre_incr;

        a_for_loop fl1 = {}, fl2 = {};
        a_statement init1 = {}, init2 = {};
        a_dynamic_init dip1 = {}, dip2 = {};
        an_expr_node lb1 = {}, cmp1 = {}, op1_1 = {}, op2_1 = {};
        an_expr_node lb2 = {}, cmp2 = {}, op1_2 = {}, op2_2 = {};
        a_statement inner_for = make_canonical_for(&iv2, &int32_edg, eok_lt, &inc2, &empty_b, &fl2, &init2, &dip2, &lb2, &cmp2, &op1_2, &op2_2);
        a_statement outer_for = make_canonical_for(&iv1, &int32_edg, eok_lt, &inc1, &inner_for, &fl1, &init1, &dip1, &lb1, &cmp1, &op1_1, &op2_1);

        state.omp_pragmas[&outer_for] = "omp for collapse(2) reduction(+: rvar, rvar_u)";
        run_oom_sweep([&]() noexcept { return lower_omp_for_worksharing(&outer_for); });

        // `collapse(0)` and `collapse(unclosed`
        state.omp_pragmas[&outer_for] = "omp for collapse(0)";
        assert(lower_omp_for_worksharing(&outer_for) == llvm_gen_be_error_t::ok);
        state.omp_pragmas[&outer_for] = "omp for collapse(unclosed";
        assert(lower_omp_for_worksharing(&outer_for) == llvm_gen_be_error_t::ok);

        // Canonical static & dynamic with terminated body (`ret_b`) and error body (`bad_b`)
        inner_for.variant.for_loop.statement = &ret_b;
        state.omp_pragmas[&outer_for] = "omp for collapse(2) schedule(static)";
        assert(lower_omp_for_worksharing(&outer_for) == llvm_gen_be_error_t::ok);
        ensure_open_bb();
        state.omp_pragmas[&outer_for] = "omp for collapse(2) schedule(dynamic)";
        assert(lower_omp_for_worksharing(&outer_for) == llvm_gen_be_error_t::ok);

        ensure_open_bb();
        inner_for.variant.for_loop.statement = &bad_b;
        state.omp_pragmas[&outer_for] = "omp for collapse(2) schedule(static) reduction(+: rvar)";
        assert(lower_omp_for_worksharing(&outer_for) == llvm_gen_be_error_t::unsupported_stmt);
        ensure_open_bb();
        state.omp_pragmas[&outer_for] = "omp for collapse(2) schedule(dynamic) reduction(+: rvar)";
        assert(lower_omp_for_worksharing(&outer_for) == llvm_gen_be_error_t::unsupported_stmt);

        // Non-canonical `stmk_for` with null `innermost_body`, terminated `innermost_body`, and dynamic schedule
        ensure_open_bb();
        a_statement nc_for = {}; nc_for.kind = stmk_for; nc_for.variant.for_loop.statement = nullptr;
        state.omp_pragmas[&nc_for] = "omp for schedule(dynamic)";
        assert(lower_omp_for_worksharing(&nc_for) == llvm_gen_be_error_t::ok);
        nc_for.variant.for_loop.statement = &ret_b;
        state.omp_pragmas[&nc_for] = "omp for schedule(static) reduction(+: rvar)";
        assert(lower_omp_for_worksharing(&nc_for) == llvm_gen_be_error_t::ok);

        // Fail `get_llvm_type` on pointer pointee in `extract_omp_canonical_loop_dim` and standalone reduction `vty`
        ensure_open_bb();
        g_omp_fail_type_countdown = 1;
        state.omp_pragmas[&nc_for] = "omp for reduction(+: rvar)";
        assert(lower_omp_for_worksharing(&nc_for) == llvm_gen_be_error_t::unsupported_type);

        a_variable piv = {}; piv.source_corresp.name = (char*)"piv"; piv.type = &ptr_int_edg;
        state.local_vars[&piv] = state.builder->CreateAlloca(ptr_t, nullptr, "piv");
        a_statement ptr_for = make_canonical_for(&piv, &ptr_int_edg, eok_lt, &inc1, &empty_b, &fl1, &init1, &dip1, &lb1, &cmp1, &op1_1, &op2_1);
        g_omp_fail_type_countdown = 3; // 1: lb, 2: ub, 3: pointee_ty!
        assert(lower_omp_for_worksharing(&ptr_for) == llvm_gen_be_error_t::unsupported_type);
      }
    }

    // 3) `emit_omp_mapper_arrays` rich type/section/member/global/alloca/unmapped branches + OOM sweeps
    {
      state.local_vars.clear();
      ensure_open_bb();

      a_type arr_int_edg = {}; arr_int_edg.kind = tk_array; arr_int_edg.size = 8; arr_int_edg.variant.array.element_type = &int32_edg;
      a_type arr_noelem_edg = {}; arr_noelem_edg.kind = tk_array; arr_noelem_edg.size = 4; arr_noelem_edg.variant.array.element_type = nullptr;
      a_field sf_a = {}, sf_b = {}, sf_untyped = {};
      sf_a.source_corresp.name = (char*)"fa"; sf_a.offset = 0; sf_a.type = &int32_edg; sf_a.next = &sf_b;
      sf_b.source_corresp.name = (char*)"fb"; sf_b.offset = 4; sf_b.type = &int64_edg; sf_b.next = &sf_untyped;
      sf_untyped.source_corresp.name = (char*)"fu"; sf_untyped.offset = 12; sf_untyped.type = nullptr; sf_untyped.next = nullptr;
      a_type map_st_edg = {}; map_st_edg.kind = tk_struct; map_st_edg.size = 16; map_st_edg.variant.class_struct_union.field_list = &sf_a;

      llvm::Type* st_ll_ty = nullptr;
      assert(get_llvm_type(&map_st_edg, &st_ll_ty) == llvm_gen_be_error_t::ok);
      llvm::Type* arr_ll_ty = llvm::ArrayType::get(i32_t, 8);

      a_variable mv_ptr = {}, mv_vptr = {}, mv_arr = {}, mv_arr_ne = {}, mv_sc = {}, mv_st = {}, mv_untyped_arr = {}, mv_untyped_sc = {};
      mv_ptr.source_corresp.name = (char*)"mp"; mv_ptr.type = &ptr_int_edg;
      mv_vptr.source_corresp.name = (char*)"mvp"; mv_vptr.type = &ptr_void_edg;
      mv_arr.source_corresp.name = (char*)"ma"; mv_arr.type = &arr_int_edg;
      mv_arr_ne.source_corresp.name = (char*)"mane"; mv_arr_ne.type = &arr_noelem_edg;
      mv_sc.source_corresp.name = (char*)"msc"; mv_sc.type = &int64_edg;
      mv_st.source_corresp.name = (char*)"mst"; mv_st.type = &map_st_edg;
      mv_untyped_arr.source_corresp.name = (char*)"mua"; mv_untyped_arr.type = nullptr;
      mv_untyped_sc.source_corresp.name = (char*)"mus"; mv_untyped_sc.type = nullptr;

      state.local_vars[&mv_ptr] = state.builder->CreateAlloca(ptr_t, nullptr, "mp");
      state.local_vars[&mv_vptr] = state.builder->CreateAlloca(ptr_t, nullptr, "mvp");
      state.local_vars[&mv_arr] = state.builder->CreateAlloca(arr_ll_ty, nullptr, "ma");
      state.local_vars[&mv_arr_ne] = state.builder->CreateAlloca(arr_ll_ty, nullptr, "mane");
      state.local_vars[&mv_sc] = state.builder->CreateAlloca(i64_t, nullptr, "msc");
      state.local_vars[&mv_st] = state.builder->CreateAlloca(st_ll_ty, nullptr, "mst");
      state.local_vars[&mv_untyped_arr] = state.builder->CreateAlloca(arr_ll_ty, nullptr, "mua");
      state.local_vars[&mv_untyped_sc] = state.builder->CreateAlloca(i32_t, nullptr, "mus");

      llvm::GlobalVariable* mgv = nullptr;
      assert(llvm_create_global_variable(state.module.get(), i64_t, false, llvm::GlobalValue::ExternalLinkage,
                                         llvm::ConstantInt::get(i64_t, 0), "mgv_global", &mgv) == llvm_gen_be_error_t::ok);

      a_statement m_stmt = {}; m_stmt.kind = stmk_empty;
      state.omp_pragmas[&m_stmt] =
          "omp target enter data map(to: mp[2:8], mvp[0:4], ma[1:4], mane[0:2], msc[0:2], msc, "
          "mst.fb, mst.fu, mst.nomatch, mua[1:3], mus[0:2], mus, mgv_global[1:2], mgv_global, unk_arr[2:4], unk_sc)";
      run_oom_sweep([&]() noexcept { return lower_omp_target_enter_data(&m_stmt); });

      // Struct member with bracket in expr_text (`mst.fb[0]` when not parsed as array section: e.g. unclosed `[` so `is_array_section` is false!)
      state.omp_pragmas[&m_stmt] = "omp target enter data map(to: mst.fb[0, , ) map(unclosed";
      assert(lower_omp_target_enter_data(&m_stmt) == llvm_gen_be_error_t::ok);

      // Fail `get_llvm_type` inside `emit_omp_mapper_arrays` at each nested call site (1: var_llvm_ty, 2: pt_llvm_ty, 3: arr_elem_ty, 4: f_ty)
      state.omp_pragmas[&m_stmt] = "omp target enter data map(to: mp[0:2])";
      g_omp_fail_type_countdown = 1;
      assert(lower_omp_target_enter_data(&m_stmt) == llvm_gen_be_error_t::unsupported_type);
      g_omp_fail_type_countdown = 2;
      assert(lower_omp_target_enter_data(&m_stmt) == llvm_gen_be_error_t::unsupported_type);

      state.omp_pragmas[&m_stmt] = "omp target enter data map(to: ma[0:2])";
      g_omp_fail_type_countdown = 3; // 1: ma(tk_array), 2: ma->elem, 3: arr_elem_ty!
      assert(lower_omp_target_enter_data(&m_stmt) == llvm_gen_be_error_t::unsupported_type);

      state.omp_pragmas[&m_stmt] = "omp target enter data map(to: mst.fb)";
      g_omp_fail_type_countdown = 2; // 1: mst, 2: f_ty!
      assert(lower_omp_target_enter_data(&m_stmt) == llvm_gen_be_error_t::unsupported_type);
    }

    // 4) `compile_omp_device_module` & `llvm_emit_omp_device_offload_module` & OOM sweeps on all remaining OpenMP constructs
    {
      llvm::SmallVector<char, 512> dev_img;
      // Invalid non-GPU triple -> gpu_offload_failed
      assert(compile_omp_device_module(state.module.get(), "x86_64-unknown-linux-gnu", "generic", &dev_img) ==
             llvm_gen_be_error_t::gpu_offload_failed);

      // Add a declaration-only `__omp_offloading_decl_only` kernel AND a defined kernel with arguments
      llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "__omp_offloading_decl_only", state.module.get());
      llvm::FunctionType* k_arg_fty = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {ptr_t}, false);
      llvm::Function* k_with_arg = llvm::Function::Create(
          k_arg_fty, llvm::Function::InternalLinkage, "__omp_offloading_with_arg", state.module.get());
      k_with_arg->getArg(0)->setName("k_ptr");
      llvm::BasicBlock* k_bb = llvm::BasicBlock::Create(*state.context, "entry", k_with_arg);
      llvm::IRBuilder<> kb(k_bb);
      kb.CreateRetVoid();

      // Initialize all LLVM target infos so `registered_dev_target != nullptr` is also exercised!
      llvm::InitializeAllTargetInfos();
      run_oom_sweep([&]() noexcept {
        return compile_omp_device_module(state.module.get(), "nvptx64-nvidia-cuda", "sm_80", &dev_img);
      });
      assert(llvm_emit_omp_device_offload_module(state.module.get(), "amdgcn-amd-amdhsa") == llvm_gen_be_error_t::ok);
      run_oom_sweep([&]() noexcept {
        return llvm_emit_omp_device_offload_module(state.module.get(), "nvptx64-nvidia-cuda");
      });

      // OOM sweeps across all remaining OpenMP lowering functions and pragma dispatch branches
      a_statement s_ok = {}; s_ok.kind = stmk_empty; s_ok.position.seq = 77;
      state.omp_pragmas[&s_ok] = "omp target nowait depend(in:x) map(tofrom: msc)";
      run_oom_sweep([&]() noexcept { return lower_omp_target_region(&s_ok); });

      state.omp_pragmas[&s_ok] = "omp target data map(to: msc)";
      run_oom_sweep([&]() noexcept { return lower_omp_target_data(&s_ok); });
      state.omp_pragmas[&s_ok] = "omp target exit data map(from: msc)";
      run_oom_sweep([&]() noexcept { return lower_omp_target_exit_data(&s_ok); });
      state.omp_pragmas[&s_ok] = "omp target update to(msc)";
      run_oom_sweep([&]() noexcept { return lower_omp_target_update(&s_ok); });

      // Also test null directive path (`directive == nullptr`) in target data/enter/exit/update/region/task/reduce/master/atomic
      state.omp_pragmas.erase(&s_ok);
      run_oom_sweep([&]() noexcept { return lower_omp_critical(&s_ok); });
      run_oom_sweep([&]() noexcept { return lower_omp_barrier(&s_ok); });
      run_oom_sweep([&]() noexcept { return lower_omp_target_data(&s_ok); });
      run_oom_sweep([&]() noexcept { return lower_omp_target_enter_data(&s_ok); });
      run_oom_sweep([&]() noexcept { return lower_omp_target_exit_data(&s_ok); });
      run_oom_sweep([&]() noexcept { return lower_omp_target_update(&s_ok); });
      run_oom_sweep([&]() noexcept { return lower_omp_target_region(&s_ok); });
      run_oom_sweep([&]() noexcept { return lower_omp_task(&s_ok); });
      state.omp_pragmas[&s_ok] = "omp task depend(inout:x)";
      run_oom_sweep([&]() noexcept { return lower_omp_task(&s_ok); });
      state.omp_pragmas.erase(&s_ok);
      run_oom_sweep([&]() noexcept { return lower_omp_taskwait(&s_ok); });
      run_oom_sweep([&]() noexcept { return lower_omp_taskgroup(&s_ok); });
      run_oom_sweep([&]() noexcept { return lower_omp_reduce(&s_ok); });
      run_oom_sweep([&]() noexcept { return lower_omp_sections(&s_ok); });
      run_oom_sweep([&]() noexcept { return lower_omp_single(&s_ok); });
      run_oom_sweep([&]() noexcept { return lower_omp_taskyield(&s_ok); });
      run_oom_sweep([&]() noexcept { return lower_omp_flush(&s_ok); });
      run_oom_sweep([&]() noexcept { return lower_omp_ordered(&s_ok); });
      run_oom_sweep([&]() noexcept { return lower_omp_simd(&s_ok); });
      run_oom_sweep([&]() noexcept { return lower_omp_teams_distribute(&s_ok); });
      run_oom_sweep([&]() noexcept { return lower_omp_taskloop(&s_ok); });
      run_oom_sweep([&]() noexcept { return lower_omp_master(&s_ok); });
      run_oom_sweep([&]() noexcept { return lower_omp_atomic(&s_ok); });

      // OOM sweep on `llvm_dispatch_omp_pragma_stmt` for `omp for simd` and `omp cancel`
      run_oom_sweep([&]() noexcept {
        bool h = false;
        s_ok.has_associated_pragma = TRUE;
        state.omp_pragmas[&s_ok] = "omp for simd";
        return llvm_dispatch_omp_pragma_stmt(&s_ok, &h);
      });
      run_oom_sweep([&]() noexcept {
        bool h = false;
        s_ok.has_associated_pragma = TRUE;
        state.omp_pragmas[&s_ok] = "omp cancel parallel";
        return llvm_dispatch_omp_pragma_stmt(&s_ok, &h);
      });

      // Also exercise `get_or_create_ident_t` when `fn->getName()` is empty AND `M->getSourceFileName()` is non-empty!
      state.module->setSourceFileName("omp_src.cpp");
      llvm::Function* anon_fn = llvm::Function::Create(ft, llvm::Function::InternalLinkage, "", state.module.get());
      llvm::BasicBlock* anon_bb = llvm::BasicBlock::Create(*state.context, "entry", anon_fn);
      state.builder->SetInsertPoint(anon_bb);
      assert(lower_omp_flush(&s_ok) == llvm_gen_be_error_t::ok);

      // 5) Final branch closure for src/llvm_gen_be_omp.cpp:
      state.builder->SetInsertPoint(bb);
      ensure_open_bb();

      // - L169 ($ boundary), L189/L207 (partial match at very end of list_str), L288 (reduction( at index 0),
      //   L348 (tk_struct & null ut), L352 (extra->assoc_scope == nullptr), L359 (r->type == nullptr & rt->extra_info == nullptr),
      //   L367 (pt->type == nullptr & non-reference pointer p0), L847 (dtor->source_corresp.name == nullptr)
      {
        a_type tref_null = {}; tref_null.kind = tk_typeref; tref_null.variant.typeref.type = nullptr;
        a_class_type_supplement st_no_scope = {}; st_no_scope.assoc_scope = nullptr;
        a_type st_priv_edg = {}; st_priv_edg.kind = tk_struct; st_priv_edg.size = 16; st_priv_edg.variant.class_struct_union.extra_info = &st_no_scope;

        a_param_type p_null_t = {}, p_nonref_ptr = {};
        p_null_t.type = nullptr;
        p_nonref_ptr.type = &ptr_int_edg; // tk_pointer with is_reference == FALSE!
        a_routine_type_supplement rts_null_p = {}, rts_nonref_p = {};
        rts_null_p.param_type_list = &p_null_t;
        rts_nonref_p.param_type_list = &p_nonref_ptr;
        a_type rty_no_extra = {}, rty_null_p = {}, rty_nonref_p = {};
        rty_no_extra.kind = tk_routine; rty_no_extra.variant.routine.extra_info = nullptr;
        rty_null_p.kind = tk_routine; rty_null_p.variant.routine.extra_info = &rts_null_p;
        rty_nonref_p.kind = tk_routine; rty_nonref_p.variant.routine.extra_info = &rts_nonref_p;

        a_routine rc_notype = {}, rc_noextra = {}, rc_nullp = {}, rc_nonrefp = {}, rd_noname = {};
        rc_notype.special_kind = sfk_constructor; rc_notype.type = nullptr; rc_notype.next = &rc_noextra;
        rc_noextra.special_kind = sfk_constructor; rc_noextra.type = &rty_no_extra; rc_noextra.next = &rc_nullp;
        rc_nullp.special_kind = sfk_constructor; rc_nullp.type = &rty_null_p; rc_nullp.next = &rc_nonrefp;
        rc_nonrefp.special_kind = sfk_constructor; rc_nonrefp.type = &rty_nonref_p; rc_nonrefp.next = &rd_noname;
        rd_noname.special_kind = sfk_destructor; rd_noname.compiler_generated = FALSE; rd_noname.source_corresp.name = nullptr; rd_noname.next = nullptr;

        a_scope st2_scope = {}; st2_scope.routines = &rc_notype;
        a_class_type_supplement st2_extra = {}; st2_extra.assoc_scope = &st2_scope;
        a_type st2_edg = {}; st2_edg.kind = tk_struct; st2_edg.size = 16; st2_edg.variant.class_struct_union.extra_info = &st2_extra;

        llvm::Type* st_ll = nullptr;
        assert(get_llvm_type(&st_priv_edg, &st_ll) == llvm_gen_be_error_t::ok);

        state.local_vars.clear();
        ensure_open_bb();
        a_variable vx = {}, v_tref_null = {}, v_st1 = {}, v_st2 = {};
        vx.source_corresp.name = (char*)"x"; vx.type = &int32_edg;
        v_tref_null.source_corresp.name = (char*)"vtn"; v_tref_null.type = &tref_null;
        v_st1.source_corresp.name = (char*)"vst1"; v_st1.type = &st_priv_edg;
        v_st2.source_corresp.name = (char*)"vst2"; v_st2.type = &st2_edg;
        state.local_vars[&vx] = state.builder->CreateAlloca(i32_t, nullptr, "x");
        state.local_vars[&v_tref_null] = state.builder->CreateAlloca(i32_t, nullptr, "vtn");
        state.local_vars[&v_st1] = state.builder->CreateAlloca(st_ll, nullptr, "vst1");
        state.local_vars[&v_st2] = state.builder->CreateAlloca(st_ll, nullptr, "vst2");

        a_statement p2 = {}; p2.kind = stmk_empty;
        // "reduction(+: x)" at pos 0 (L288 pos == 0!), "private($x, x$, my_x)" (L169 '$' TRUE & L189/207 end-of-string partial match!)
        state.omp_pragmas[&p2] = "reduction(+: x) omp parallel private($x, x$, my_x) firstprivate(vtn, vst1, vst2)";
        assert(lower_omp_parallel_region(&p2) == llvm_gen_be_error_t::ok);
      }

      // - Canonical & standalone `lower_omp_for_worksharing` + `extract_omp_canonical_loop_dim` remaining branches:
      //   * L953: `state.local_vars[&iv] = nullptr`
      //   * L1001: `eok_add_assign` with `iop2 == nullptr`
      //   * L1004, L1015, L1027: `sv == nullptr` / `raw_inc == nullptr` (`g_omp_expr_null_val = 3`)
      //   * L1011: `eok_assign` where `iop2` is `enk_operation` with `rop1 == nullptr`
      //   * L1037: `has_cond_op2 && var_slot && loop_stmt->variant.for_loop.statement == nullptr`
      //   * L1050 & L1053: pointer `var_ty` with `iv.type == nullptr`, `iv.type == &int32_edg`, and `iv.type == &tref_null`
      //   * L1206 & L1217: standalone reduction with null key, null value, null name, unsigned int, and float!
      //   * L1512 & L1515 & L1542: canonical loop with unnamed `iv` in `lastprivate(other)` AND canonical `omp parallel for lastprivate(iv)` + OOM sweeps!
      {
        a_type tref_null = {}; tref_null.kind = tk_typeref; tref_null.variant.typeref.type = nullptr;
        a_statement body_e = {}; body_e.kind = stmk_empty;
        a_variable iv = {}; iv.source_corresp.name = (char*)"iv"; iv.type = &int32_edg;

        an_expr_node lb = {}, op1 = {}, op2 = {}, cmp = {}, inc_lhs = {}, inc_rhs = {}, inc_e = {};
        lb.kind = enk_constant; lb.type = &int32_edg; lb.position.column = 0;
        op1.kind = enk_variable; op1.variant.variable.ptr = &iv; op1.next = &op2;
        op2.kind = enk_constant; op2.type = &int32_edg; op2.position.column = 4;
        cmp.kind = enk_operation; cmp.variant.operation.kind = eok_lt; cmp.variant.operation.operands = &op1;
        inc_lhs.kind = enk_variable; inc_lhs.next = &inc_rhs;
        inc_rhs.kind = enk_constant; inc_rhs.type = &int32_edg; inc_rhs.position.column = 1;
        inc_e.kind = enk_operation; inc_e.variant.operation.kind = eok_add_assign; inc_e.variant.operation.operands = &inc_lhs;

        a_dynamic_init dip = {}; dip.kind = dik_expression; dip.variable = &iv; dip.variant.expression = &lb;
        a_statement init_s = {}; init_s.kind = stmk_init; init_s.variant.dynamic_init = &dip;
        a_for_loop fl = {}; fl.initialization = &init_s; fl.increment = &inc_e;
        a_statement fs = {}; fs.kind = stmk_for; fs.expr = &cmp; fs.variant.for_loop.extra_info = &fl; fs.variant.for_loop.statement = &body_e;

        // L953: `it != end() && it->second == nullptr`
        state.local_vars.clear();
        ensure_open_bb();
        state.local_vars[&iv] = nullptr;
        assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);

        state.local_vars[&iv] = state.builder->CreateAlloca(i32_t, nullptr, "iv");
        // OOM sweep on `+=` canonical loop (covers MBR file_id=4, 8 in extract_omp_canonical_loop_dim)
        run_oom_sweep([&]() noexcept { return lower_omp_for_worksharing(&fs); });

        // L1004: `eok_add_assign` with `sv == nullptr`
        g_omp_expr_null_val = 3;
        assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);
        // L1001: `eok_add_assign` with `iop2 == nullptr`
        inc_lhs.next = nullptr;
        assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);

        // L1011 & L1015: `eok_assign` where `iop2` is `enk_operation(eok_add)` with `rop1 == nullptr`, and with `sv == nullptr`
        an_expr_node add_rhs = {}, rop1 = {}, rop2 = {};
        add_rhs.kind = enk_operation; add_rhs.variant.operation.kind = eok_add; add_rhs.variant.operation.operands = nullptr;
        inc_lhs.next = &add_rhs;
        inc_e.variant.operation.kind = eok_assign;
        assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok); // rop1 == nullptr!

        rop1.kind = enk_variable; rop1.next = &rop2;
        rop2.kind = enk_constant; rop2.type = &int32_edg; rop2.position.column = 1;
        add_rhs.variant.operation.operands = &rop1;
        g_omp_expr_null_val = 3;
        assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok); // L1015 sv == nullptr!

        // L1027: non-operation increment with `raw_inc == nullptr`
        fl.increment = &rop2;
        g_omp_expr_null_val = 3;
        assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);

        // L1037: `has_cond_op2 && var_slot && statement == nullptr`
        fs.variant.for_loop.statement = nullptr;
        assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);
        fs.variant.for_loop.statement = &body_e;

        // L1050 & L1053 + MBR file_id=12: Pointer `var_slot` with `iv.type == nullptr`, `iv.type == &int32_edg`, `iv.type == &tref_null`, and OOM sweep on `ptr_int_edg`!
        state.local_vars[&iv] = state.builder->CreateAlloca(ptr_t, nullptr, "piv");
        lb.type = &ptr_int_edg; op2.type = &ptr_int_edg;
        iv.type = nullptr;
        assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);
        iv.type = &int32_edg;
        assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);
        iv.type = &tref_null;
        assert(lower_omp_for_worksharing(&fs) == llvm_gen_be_error_t::ok);
        iv.type = &ptr_int_edg;
        run_oom_sweep([&]() noexcept { return lower_omp_for_worksharing(&fs); });

        // L1206, L1217, L1512, L1515, L1542 + MBR file_id=10, 12, 14, 16 in `lower_omp_for_worksharing`:
        state.local_vars.clear();
        ensure_open_bb();
        iv.type = &int32_edg;
        lb.type = &int32_edg; op2.type = &int32_edg;
        state.local_vars[&iv] = state.builder->CreateAlloca(i32_t, nullptr, "iv");

        a_variable v_nullval = {}, v_noname = {}, v_ru = {}, v_rf = {};
        v_nullval.source_corresp.name = (char*)"vnv";
        v_noname.source_corresp.name = nullptr;
        v_ru.source_corresp.name = (char*)"ru_for"; v_ru.type = &uint32_edg;
        v_rf.source_corresp.name = (char*)"rf_for"; v_rf.type = &flt_edg;
        state.local_vars[nullptr] = state.builder->CreateAlloca(i32_t, nullptr, "nullk");
        state.local_vars[&v_nullval] = nullptr;
        state.local_vars[&v_noname] = state.builder->CreateAlloca(i32_t, nullptr, "noname");
        state.local_vars[&v_ru] = state.builder->CreateAlloca(i32_t, nullptr, "ru_for");
        state.local_vars[&v_rf] = state.builder->CreateAlloca(f32_t, nullptr, "rf_for");

        // Canonical `omp parallel for lastprivate(iv)` -> L1542 (`omp parallel` TRUE without `nowait`) + OOM sweep on `lastprivate`!
        state.omp_pragmas[&fs] = "omp parallel for lastprivate(iv)";
        run_oom_sweep([&]() noexcept { return lower_omp_for_worksharing(&fs); });

        // Unnamed `iv` (`iv.source_corresp.name = nullptr`) with `lastprivate(other)` -> L1512 false & L1515 false!
        iv.source_corresp.name = nullptr;
        state.omp_pragmas[&fs] = "omp for lastprivate(other) reduction(+: ru_for, rf_for)";
        run_oom_sweep([&]() noexcept { return lower_omp_for_worksharing(&fs); });

        // Also OOM sweep on `restore_standalone_reds(false)` error paths (non-canonical error, canonical static error, canonical dynamic error)!
        a_statement bad_inner = {}; bad_inner.kind = static_cast<a_statement_kind>(999);
        fs.variant.for_loop.statement = &bad_inner;
        for (int b = 0; b < 32; ++b) {
          ensure_open_bb();
          state.omp_pragmas[&fs] = "omp for schedule(static) reduction(+: ru_for)";
          g_llvm_be_oom_countdown = b;
          llvm_gen_be_error_t r = lower_omp_for_worksharing(&fs);
          assert(r != llvm_gen_be_error_t::ok);
          g_llvm_be_oom_countdown = -1;
        }
        for (int b = 0; b < 32; ++b) {
          ensure_open_bb();
          state.omp_pragmas[&fs] = "omp for schedule(dynamic) reduction(+: ru_for)";
          g_llvm_be_oom_countdown = b;
          llvm_gen_be_error_t r = lower_omp_for_worksharing(&fs);
          assert(r != llvm_gen_be_error_t::ok);
          g_llvm_be_oom_countdown = -1;
        }
        a_statement nc_bad = {}; nc_bad.kind = static_cast<a_statement_kind>(999);
        for (int b = 0; b < 32; ++b) {
          ensure_open_bb();
          state.omp_pragmas[&nc_bad] = "omp for reduction(+: ru_for)";
          g_llvm_be_oom_countdown = b;
          llvm_gen_be_error_t r = lower_omp_for_worksharing(&nc_bad);
          assert(r != llvm_gen_be_error_t::ok);
          g_llvm_be_oom_countdown = -1;
        }
      }

      // - `emit_omp_mapper_arrays` remaining branches (L1824, L1825, L1830, L1835, L1843-1844, L1852):
      {
        a_type tref_null = {}; tref_null.kind = tk_typeref; tref_null.variant.typeref.type = nullptr;
        a_field f_noname = {}, f_named = {};
        f_noname.source_corresp.name = nullptr; f_noname.next = &f_named;
        f_named.source_corresp.name = (char*)"m"; f_named.offset = 0; f_named.type = &int32_edg; f_named.next = nullptr;

        a_type cls_map = {}; cls_map.kind = tk_class; cls_map.size = 16; cls_map.variant.class_struct_union.field_list = &f_noname;
        a_type uni_map = {}; uni_map.kind = tk_union; uni_map.size = 16; uni_map.variant.class_struct_union.field_list = &f_noname;
        llvm::Type* cu_ll = nullptr;
        assert(get_llvm_type(&cls_map, &cu_ll) == llvm_gen_be_error_t::ok);

        state.local_vars.clear();
        ensure_open_bb();
        a_variable v_tn = {}, v_pnull_base = {}, v_pnull_elem = {}, v_cls = {}, v_uni = {}, v_int_dot = {};
        v_tn.source_corresp.name = (char*)"mtn"; v_tn.type = &tref_null;
        v_pnull_base.source_corresp.name = (char*)"mpnb"; v_pnull_base.type = &ptr_int_edg;
        v_pnull_elem.source_corresp.name = (char*)"mpne"; v_pnull_elem.type = &ptr_null_edg;
        v_cls.source_corresp.name = (char*)"mcls"; v_cls.type = &cls_map;
        v_uni.source_corresp.name = (char*)"muni"; v_uni.type = &uni_map;
        v_int_dot.source_corresp.name = (char*)"mint"; v_int_dot.type = &int32_edg;

        state.local_vars[&v_tn] = state.builder->CreateAlloca(i32_t, nullptr, "mtn");
        state.local_vars[&v_pnull_base] = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_t)); // base_val == null_ptr!
        state.local_vars[&v_pnull_elem] = state.builder->CreateAlloca(ptr_t, nullptr, "mpne");
        state.local_vars[&v_cls] = state.builder->CreateAlloca(cu_ll, nullptr, "mcls");
        state.local_vars[&v_uni] = state.builder->CreateAlloca(cu_ll, nullptr, "muni");
        state.local_vars[&v_int_dot] = state.builder->CreateAlloca(i32_t, nullptr, "mint");

        a_statement ms2 = {}; ms2.kind = stmk_empty;
        state.omp_pragmas[&ms2] =
            "omp target enter data map(to: mtn[0:2], mpnb[0:2], mpne[0:2], mtn.m, mcls.m, muni.m, mint.m)";
        assert(lower_omp_target_enter_data(&ms2) == llvm_gen_be_error_t::ok);
      }

      // - `compile_omp_device_module` non-kernel defined `omp_declare_target` function (`is_kernel == false` at L2183),
      //   PLUS clean-module OOM sweeps on `lower_omp_critical` and `llvm_emit_omp_device_offload_module`!
      {
        llvm::Function* def_helper = llvm::Function::Create(
            ft, llvm::Function::ExternalLinkage, "defined_declare_target_helper", state.module.get());
        def_helper->addFnAttr("omp_declare_target");
        llvm::BasicBlock* dh_bb = llvm::BasicBlock::Create(*state.context, "entry", def_helper);
        llvm::IRBuilder<> dhb(dh_bb);
        dhb.CreateRetVoid();
        assert(compile_omp_device_module(state.module.get(), "nvptx64-nvidia-cuda", "sm_80", &dev_img) == llvm_gen_be_error_t::ok);

        // Fresh module OOM sweeps so `!lock_gv` and `!desc_gv` creation OOM branches execute!
        for (int b = 0; b < 16; ++b) {
          LLVMBackendState clean_st = {};
          clean_st.context = std::make_unique<llvm::LLVMContext>();
          clean_st.module = std::make_unique<llvm::Module>("clean_omp", *clean_st.context);
          clean_st.builder = std::make_unique<llvm::IRBuilder<>>(*clean_st.context);
          llvm::Function* cf = llvm::Function::Create(
              llvm::FunctionType::get(llvm::Type::getVoidTy(*clean_st.context), false),
              llvm::Function::ExternalLinkage, "cf", clean_st.module.get());
          clean_st.builder->SetInsertPoint(llvm::BasicBlock::Create(*clean_st.context, "entry", cf));
          edg::be_state = &clean_st;
          a_statement cs = {}; cs.kind = stmk_empty;
          g_llvm_be_oom_countdown = b;
          llvm_gen_be_error_t r = lower_omp_critical(&cs);
          g_llvm_be_oom_countdown = -1;
          if (r == llvm_gen_be_error_t::ok) break;
        }
        for (int b = 0; b < 16; ++b) {
          LLVMBackendState clean_st = {};
          clean_st.context = std::make_unique<llvm::LLVMContext>();
          clean_st.module = std::make_unique<llvm::Module>("clean_offload", *clean_st.context);
          clean_st.builder = std::make_unique<llvm::IRBuilder<>>(*clean_st.context);
          edg::be_state = &clean_st;
          g_llvm_be_oom_countdown = b;
          llvm_gen_be_error_t r = llvm_emit_omp_device_offload_module(clean_st.module.get(), "nvptx64-nvidia-cuda");
          g_llvm_be_oom_countdown = -1;
          if (r == llvm_gen_be_error_t::ok) break;
        }
        edg::be_state = &state;
      }
    }
  }

  printf("PASS\n");
  return 0;
}
