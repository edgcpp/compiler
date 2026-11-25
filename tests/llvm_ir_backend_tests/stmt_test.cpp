/**
 * @file stmt_test.cpp
 * @ingroup llvm_gen_be_tests
 * @brief Unit tests for the LLVM backend statement lowering.
 * @details Validates function coverage of stmk_try_block and stmk_microsoft_try execution.
 */

#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_eh.h"

#include <cassert>
#include <cstdio>
#include <memory>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

using namespace edg;

LLVMBackendState* edg::be_state = nullptr;

namespace edg {
  /**
   * @brief Stub checking whether an EDG type is a boolean type.
   * @details Returns FALSE in statement test harness.
   * @param[in] ty The EDG type pointer.
   * @return Always FALSE.
   */
  a_boolean is_bool_type(a_type_ptr ty) { (void)ty; return FALSE; }

  /**
   * @brief Stub returning the byte alignment of an EDG type.
   * @details Returns 1 in statement test harness.
   * @param[in] ty The EDG type pointer.
   * @return Alignment in bytes (1).
   */
  a_targ_alignment f_alignment_of_type(a_type_ptr ty) { (void)ty; return 1; }

  /**
   * @brief Stub checking whether an EDG type is trivially copyable.
   * @details Returns TRUE in statement test harness.
   * @param[in] ty The EDG type pointer.
   * @return Always TRUE.
   */
  a_boolean is_trivially_copyable_type(a_type_ptr ty) { (void)ty; return TRUE; }

  unsigned int targ_char_bit = 8;
  a_targ_size_t targ_sizeof_short = 2;
  a_targ_size_t targ_sizeof_int = 4;
  a_targ_size_t targ_sizeof_long = 8;
  a_targ_size_t targ_sizeof_long_long = 8;
  a_targ_size_t targ_sizeof_pointer = 8;
  a_boolean targ_little_endian = TRUE;
  a_C_dialect C_dialect = static_cast<a_C_dialect>(0);

  /**
   * @brief Stub converting sequence numbers to file and line coordinates.
   * @details Returns nullptr in statement test harness.
   * @param[in] seq The sequence number.
   * @param[out] fn File name out-pointer.
   * @param[out] full_fn Full file name out-pointer.
   * @param[out] line Line number out-pointer.
   * @param[out] eos End-of-source out-pointer.
   * @return Always nullptr.
   */
  a_source_file_ptr conv_seq_to_file_and_line(a_seq_number seq, a_const_char** fn, a_const_char** full_fn, a_line_number* line, a_boolean* eos) {
    (void)seq; (void)fn; (void)full_fn; (void)line; (void)eos;
    return nullptr;
  }

  /**
   * @brief Stub returning a mangled typeinfo symbol name.
   * @details Returns "_ZTIi" in statement test harness.
   * @param[in] type The EDG type pointer.
   * @return Mangled typeinfo string pointer.
   */
  char* mangled_typeinfo_name(a_type_ptr type) { (void)type; return (char*)"_ZTIi"; }

  /**
   * @brief Stub retrieving CVR type qualifiers for an EDG type.
   * @details Returns 0 in statement test harness.
   * @param[in] type The EDG type pointer.
   * @param[in] x Flag parameter.
   * @return Empty qualifier set (0).
   */
  a_type_qualifier_set f_get_type_qualifiers(a_type_ptr type, a_boolean x) { (void)type; (void)x; return 0; }

  /**
   * @brief Stub checking whether an EDG type is a reference type.
   * @details Returns FALSE in statement test harness.
   * @param[in] ty The EDG type pointer.
   * @return Always FALSE.
   */
  a_boolean is_reference_type(a_type_ptr ty) { (void)ty; return FALSE; }
  /**
   * @brief Stub checking whether EDG IL lowering was enabled.
   * @details Returns FALSE in standalone statement unit tests.
   * @return Always FALSE.
   */
  a_boolean il_lowering_needed(void) { return FALSE; }

  /**
   * @brief Stub checking whether an EDG type is an empty class type.
   * @details Returns FALSE in statement test harness.
   * @param[in] ty The EDG type pointer.
   * @return Always FALSE.
   */
  a_boolean is_empty_class_type(a_type_ptr ty) { (void)ty; return FALSE; }

  /**
   * @brief Stub evaluating an EDG constant into an LLVM Constant.
   * @details Produces a non-zero integer/float or null constant matching @p ty.
   * @param[in] con The EDG constant pointer.
   * @param[in] ty Expected LLVM type.
   * @param[out] out_const Receives the LLVM Constant pointer.
   * @return llvm_gen_be_error_t::ok.
   */
  llvm_gen_be_error_t evaluate_constant(a_constant_ptr con, llvm::Type* ty, llvm::Constant** out_const) noexcept {
    (void)con;
    if (ty->isIntegerTy()) *out_const = llvm::ConstantInt::get(ty, 1);
    else if (ty->isFloatingPointTy()) *out_const = llvm::ConstantFP::get(ty, 1.0);
    else *out_const = llvm::Constant::getNullValue(ty);
    return llvm_gen_be_error_t::ok; 
  }
  a_byte_boolean int_kind_is_signed[ik_last] = {0};
  
  /**
   * @brief Stub lowering an EDG type into an LLVM Type.
   * @details Returns i32 in statement test harness.
   * @param[in] ty The EDG type pointer.
   * @param[out] out_ty Receives the lowered LLVM Type pointer.
   * @return llvm_gen_be_error_t::ok.
   */
  llvm_gen_be_error_t get_llvm_type(a_type_ptr ty, llvm::Type** out_ty) noexcept {
    (void)ty;
    *out_ty = llvm::Type::getInt32Ty(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Stub returning a typeinfo global constant for an EDG type.
   * @details Returns a null pointer constant in statement test harness.
   * @param[in] type The EDG type pointer.
   * @param[out] out_ti Receives the typeinfo constant pointer.
   * @return llvm_gen_be_error_t::ok.
   */
  llvm_gen_be_error_t get_typeinfo_global(a_type_ptr type, llvm::Constant** out_ti) noexcept {
    (void)type;
    *out_ti = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
    return llvm_gen_be_error_t::ok;
  }

}

/**
 * @brief Executes the statement lowering unit test suite.
 * @details Exercises try/catch, SEH try, scope cleanups, static local guard init, and dynamic init kinds.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t run_stmt_tests(void) noexcept {
  printf("Running stmt_test...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_stmt", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);

  a_statement stmt_try = {};
  stmt_try.kind = stmk_try_block;
  a_try_supplement try_block = {};
  stmt_try.variant.try_block = &try_block;
  
  a_handler handler = {};
  try_block.handlers = &handler;
  a_variable param = {};
  a_type ty = {};
  ty.kind = tk_integer;
  param.type = &ty;
  handler.parameter = &param;
  
  a_statement body = {};
  body.kind = stmk_empty;
  try_block.statement = &body;
  handler.statement = &body;

  // Test lowering stmk_try_block
  LLVM_GEN_BE_CHECK(llvm_lower_statement(&stmt_try));

  // Test MSVC try
  EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
  llvm::BasicBlock* bb2 = llvm::BasicBlock::Create(*state.context, "entry2", f);
  state.builder->SetInsertPoint(bb2);

  a_statement stmt_ms_try = {};
  stmt_ms_try.kind = stmk_microsoft_try;
  a_microsoft_try_supplement ms_try_block = {};
  stmt_ms_try.variant.microsoft_try = &ms_try_block;
  
  // Test lowering stmk_microsoft_try
  LLVM_GEN_BE_CHECK(llvm_lower_statement(&stmt_ms_try));

  // Test ScopeCleanupStack via Return
  a_routine dtor_rout = {};
  dtor_rout.source_corresp.name = "_ZN1CD1Ev";
  llvm::Value* obj = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));
  
  ScopeCleanup cleanup = {};
  cleanup.dtor_routine = &dtor_rout;
  cleanup.object_ptr = obj;
  cleanup.is_active = true;
  LLVM_GEN_BE_CHECK(llvm_cleanup_stack_push(&state.cleanup_stack, &cleanup));
  
  a_statement return_stmt = {};
  return_stmt.kind = stmk_return;
  LLVM_GEN_BE_CHECK(llvm_lower_statement(&return_stmt));
  size_t c_size = 0;
  LLVM_GEN_BE_CHECK(llvm_cleanup_stack_size(&state.cleanup_stack, &c_size));
  assert(c_size == 1);
  state.cleanup_stack.cleanups.clear();
  
  // Test ScopeCleanupStack via Block
  a_statement block_stmt = {};
  block_stmt.kind = stmk_block;
  a_block block_sup = {};
  block_stmt.variant.block.extra_info = &block_sup;
  block_stmt.variant.block.statements = &return_stmt; // reusing return as body for simplicity, we just want to see it pop
  
  // push again
  LLVM_GEN_BE_CHECK(llvm_lower_statement(&block_stmt));
  LLVM_GEN_BE_CHECK(llvm_cleanup_stack_size(&state.cleanup_stack, &c_size));
  assert(c_size == 0); // Block handles popping of its own scope cleanups!
  
  // Test local static thread-safe guard initialization
  a_statement init_stmt = {};
  init_stmt.kind = stmk_init;
  a_dynamic_init dip = {};
  a_variable var = {};
  var.storage_class = sc_static;
  var.source_corresp.name = "_ZZ4funcE1x";
  dip.variable = &var;
  dip.kind = dik_expression;
  an_expr_node init_expr = {};
  dip.variant.expression = &init_expr;
  init_stmt.variant.dynamic_init = &dip;

  a_routine dtor_rout2 = {};
  dtor_rout2.source_corresp.name = "_ZN1CD1Ev";
  dip.destructor = &dtor_rout2;

  LLVM_GEN_BE_CHECK(llvm_lower_statement(&init_stmt));
  assert(state.module->getGlobalVariable("_ZGVZ4funcE1x", true) != nullptr);

  // Test unmangled static local guard name generation via enclosing function scope
  a_variable unmangled_var = {};
  unmangled_var.storage_class = sc_static;
  unmangled_var.source_corresp.name = (char*)"counter";
  dip.variable = &unmangled_var;
  LLVM_GEN_BE_CHECK(llvm_lower_statement(&init_stmt));
  assert(state.module->getGlobalVariable("_ZGVZ9test_funcvE7counter", true) != nullptr);

  // Test dik_nonconstant_aggregate
  a_dynamic_init agg_init;
  memset(&agg_init, 0, sizeof(agg_init));
  agg_init.kind = dik_nonconstant_aggregate;
  llvm::Value* ptr_val = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*state.context));
  llvm::Type* obj_ty = llvm::Type::getInt32Ty(*state.context);
  LLVM_GEN_BE_CHECK(llvm_emit_dynamic_init(&agg_init, ptr_val, obj_ty));

  // Test dik_constructor with default arguments and array element loop
  a_dynamic_init ctor_init;
  memset(&ctor_init, 0, sizeof(ctor_init));
  ctor_init.kind = dik_constructor;
  a_routine ctor_rout = {};
  ctor_rout.source_corresp.name = (char*)"_ZN1XC2Ev";
  a_type ctor_rout_ty = {};
  ctor_rout_ty.kind = tk_routine;
  a_routine_type_supplement ctor_rts = {};
  a_param_type def_param = {};
  an_expr_node def_arg_expr = {};
  def_arg_expr.kind = enk_constant;
  def_param.default_arg_expr = &def_arg_expr;
  ctor_rts.param_type_list = &def_param;
  ctor_rout_ty.variant.routine.extra_info = &ctor_rts;
  ctor_rout.type = &ctor_rout_ty;

  llvm::FunctionType* ctor_ft = llvm::FunctionType::get(
      llvm::Type::getVoidTy(*state.context),
      {llvm::PointerType::getUnqual(*state.context), llvm::Type::getInt32Ty(*state.context)},
      false);
  llvm::Function::Create(ctor_ft, llvm::Function::ExternalLinkage, "_ZN1XC2Ev", state.module.get());

  ctor_init.variant.constructor.ptr = &ctor_rout;
  llvm::AllocaInst* single_obj = state.builder->CreateAlloca(obj_ty);
  LLVM_GEN_BE_CHECK(llvm_emit_dynamic_init(&ctor_init, single_obj, obj_ty));

  // Array element loop via dik_constructor
  llvm::ArrayType* arr_obj_ty = llvm::ArrayType::get(obj_ty, 3);
  llvm::AllocaInst* arr_obj = state.builder->CreateAlloca(arr_obj_ty);
  LLVM_GEN_BE_CHECK(llvm_emit_dynamic_init(&ctor_init, arr_obj, arr_obj_ty));

  // Test dik_zero, dik_constant, dik_class_result_via_ctor
  a_dynamic_init zero_dip = {};
  zero_dip.kind = dik_zero;
  LLVM_GEN_BE_CHECK(llvm_emit_dynamic_init(&zero_dip, single_obj, obj_ty));

  a_dynamic_init const_dip = {};
  const_dip.kind = dik_constant;
  a_constant c_val = {};
  const_dip.variant.constant.ptr = &c_val;
  LLVM_GEN_BE_CHECK(llvm_emit_dynamic_init(&const_dip, single_obj, obj_ty));

  a_dynamic_init via_ctor_dip = {};
  via_ctor_dip.kind = dik_class_result_via_ctor;
  via_ctor_dip.variant.expression = &def_arg_expr;
  LLVM_GEN_BE_CHECK(llvm_emit_dynamic_init(&via_ctor_dip, single_obj, obj_ty));

  // Test llvm_get_routine_symbol_name for named, empty-named, and null-named routines
  const char* rname = nullptr;
  LLVM_GEN_BE_CHECK(llvm_get_routine_symbol_name(&ctor_rout, &rname));
  assert(strcmp(rname, "_ZN1XC2Ev") == 0);
  a_routine empty_rout = {};
  empty_rout.source_corresp.name = (char*)"";
  LLVM_GEN_BE_CHECK(llvm_get_routine_symbol_name(&empty_rout, &rname));
  assert(strcmp(rname, "anon_fn") == 0);
  a_routine null_rout = {};
  LLVM_GEN_BE_CHECK(llvm_get_routine_symbol_name(&null_rout, &rname));
  assert(strcmp(rname, "anon_fn") == 0);

  printf("PASS\n");
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Main entry point for the statement lowering test binary.
 * @details Invokes `run_stmt_tests` and asserts `llvm_gen_be_error_t::ok`.
 * @return 0 on success.
 */
int main(void) {
  llvm_gen_be_error_t err = run_stmt_tests();
  assert(err == llvm_gen_be_error_t::ok);
  return 0;
}

namespace edg {
  /**
   * @brief Stub lowering an EDG expression node in the statement test harness.
   * @details Produces a constant 32-bit integer 42.
   * @param[in] expr The EDG expression node pointer.
   * @param[out] out_val Receives the lowered LLVM Value pointer.
   * @return llvm_gen_be_error_t::ok.
   */
  llvm_gen_be_error_t llvm_lower_expression(an_expr_node* expr, llvm::Value** out_val) noexcept {
    (void)expr;
    if (out_val) *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 42);
    return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Stub lowering an inline assembly statement.
   * @details No-op returning ok.
   * @param[in] stmt The EDG statement node pointer.
   * @return llvm_gen_be_error_t::ok.
   */
  llvm_gen_be_error_t llvm_lower_asm_stmt(a_statement_ptr stmt) noexcept { (void)stmt; return llvm_gen_be_error_t::ok; }

  /**
   * @brief Stub lowering a coroutine statement.
   * @details No-op returning ok.
   * @param[in] stmt The EDG statement node pointer.
   * @return llvm_gen_be_error_t::ok.
   */
  llvm_gen_be_error_t llvm_lower_coroutine_stmt(a_statement_ptr stmt) noexcept { (void)stmt; return llvm_gen_be_error_t::ok; }

  /**
   * @brief Stub lowering a coroutine return statement.
   * @details No-op returning ok.
   * @param[in] stmt The EDG statement node pointer.
   * @return llvm_gen_be_error_t::ok.
   */
  llvm_gen_be_error_t llvm_lower_coroutine_return_stmt(a_statement_ptr stmt) noexcept { (void)stmt; return llvm_gen_be_error_t::ok; }
  
  // Debug stuff
  struct llvm_gen_be_debug_state_t;

  /**
   * @brief Stub creating a DWARF debug location.
   * @details No-op returning ok.
   * @param[in] dbg Debug state pointer.
   * @param[in] pos Source position.
   * @param[in] scope Enclosing DIScope pointer.
   * @param[out] out_loc Receives the DILocation pointer.
   * @return llvm_gen_be_error_t::ok.
   */
  llvm_gen_be_error_t get_di_location(llvm_gen_be_debug_state_t* dbg, a_source_position pos, llvm::DIScope* scope, llvm::DILocation** out_loc) noexcept {
    (void)dbg; (void)pos; (void)scope; (void)out_loc;
    return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Stub popping a DWARF lexical block scope.
   * @details No-op returning ok.
   * @param[in] dbg Debug state pointer.
   * @return llvm_gen_be_error_t::ok.
   */
  llvm_gen_be_error_t pop_lexical_block(llvm_gen_be_debug_state_t* dbg) noexcept { (void)dbg; return llvm_gen_be_error_t::ok; }

  /**
   * @brief Stub pushing a DWARF lexical block scope.
   * @details No-op returning ok.
   * @param[in] dbg Debug state pointer.
   * @param[in] pos Source position.
   * @param[out] out_block Receives the DILexicalBlock pointer.
   * @return llvm_gen_be_error_t::ok.
   */
  llvm_gen_be_error_t push_lexical_block(llvm_gen_be_debug_state_t* dbg, a_source_position pos, llvm::DILexicalBlock** out_block) noexcept {
    (void)dbg; (void)pos; (void)out_block;
    return llvm_gen_be_error_t::ok;
  }
}
