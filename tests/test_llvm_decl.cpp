/**
 * @file test_llvm_decl.cpp
 * @ingroup llvm_gen_be_tests
 * @brief Unit tests for declaration lowering, TLS attributes, thunks, and C1/C2/D0/D1/D2 aliases.
 * @details Exercises `llvm_lower_global_variable`, `llvm_lower_function_declarations`, and `llvm_lower_function_definitions`.
 */

#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_abi_x86_32.h"
#include "llvm_gen_be_abi_arm32.h"
#include "llvm_gen_be_abi_wasm.h"

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
   * @details Provides mock or helper behavior for `is_bool_type` within `tests/test_llvm_decl.cpp`.
   * @param[in] ty Parameter `ty` for `is_bool_type`.
   * @return Result of `is_bool_type` (a_boolean).
   */
  a_boolean is_bool_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `is_class_struct_union_type`.
   * @details Provides mock or helper behavior for `is_class_struct_union_type` within `tests/test_llvm_decl.cpp`.
   * @param[in] ty Parameter `ty` for `is_class_struct_union_type`.
   * @return Result of `is_class_struct_union_type` (a_boolean).
   */
  a_boolean is_class_struct_union_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `f_alignment_of_type`.
   * @details Provides mock or helper behavior for `f_alignment_of_type` within `tests/test_llvm_decl.cpp`.
   * @param[in] ty Parameter `ty` for `f_alignment_of_type`.
   * @return Result of `f_alignment_of_type` (a_targ_alignment).
   */
  a_targ_alignment f_alignment_of_type(a_type_ptr ty) { return 1; }
  /**
   * @brief Test stub or helper for `is_trivially_copyable_type`.
   * @details Provides mock or helper behavior for `is_trivially_copyable_type` within `tests/test_llvm_decl.cpp`.
   * @param[in] ty Parameter `ty` for `is_trivially_copyable_type`.
   * @return Result of `is_trivially_copyable_type` (a_boolean).
   */
  a_boolean is_trivially_copyable_type(a_type_ptr ty) { return TRUE; }
  /**
   * @brief Test stub for `type_is_nonreal`.
   * @details Returns `FALSE` in standalone decl unit tests.
   * @param[in] ty Frontend type pointer.
   * @return `FALSE`.
   */
  a_boolean type_is_nonreal(a_type_ptr ty) { return ty ? FALSE : FALSE; }
  /**
   * @brief Test stub for `il_lowering_needed`.
   * @details Returns `FALSE` in standalone decl unit tests.
   * @return `FALSE`.
   */
  a_boolean il_lowering_needed(void) { return FALSE; }
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
   * @details Provides mock or helper behavior for `conv_seq_to_file_and_line` within `tests/test_llvm_decl.cpp`.
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
   * @details Provides mock or helper behavior for `mangled_typeinfo_name` within `tests/test_llvm_decl.cpp`.
   * @param[in] type Parameter `type` for `mangled_typeinfo_name`.
   * @return Result of `mangled_typeinfo_name` (char*).
   */
  char* mangled_typeinfo_name(a_type_ptr type) { static char buf[] = "_ZTIi"; return buf; }
  /**
   * @brief Test stub or helper for `f_get_type_qualifiers`.
   * @details Provides mock or helper behavior for `f_get_type_qualifiers` within `tests/test_llvm_decl.cpp`.
   * @param[in] type Parameter `type` for `f_get_type_qualifiers`.
   * @param[in] x Parameter `x` for `f_get_type_qualifiers`.
   * @return Result of `f_get_type_qualifiers` (a_type_qualifier_set).
   */
  a_type_qualifier_set f_get_type_qualifiers(a_type_ptr type, a_boolean x) { return 0; }
  /**
   * @brief Test stub or helper for `is_reference_type`.
   * @details Provides mock or helper behavior for `is_reference_type` within `tests/test_llvm_decl.cpp`.
   * @param[in] ty Parameter `ty` for `is_reference_type`.
   * @return Result of `is_reference_type` (a_boolean).
   */
  a_boolean is_reference_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `is_empty_class_type`.
   * @details Provides mock or helper behavior for `is_empty_class_type` within `tests/test_llvm_decl.cpp`.
   * @param[in] ty Parameter `ty` for `is_empty_class_type`.
   * @return Result of `is_empty_class_type` (a_boolean).
   */
  a_boolean is_empty_class_type(a_type_ptr ty) { return ty && ty->variant.class_struct_union.is_empty_class; }
  a_byte_boolean int_kind_is_signed[ik_last] = {0};

  a_boolean sanitize_address = FALSE; a_boolean sanitize_thread = FALSE; a_boolean sanitize_memory = FALSE; a_boolean sanitize_undefined = FALSE; a_boolean sanitize_hwaddress = FALSE; int stack_protector_mode = 0; a_boolean use_safestack = FALSE; a_boolean use_shadowcallstack = FALSE;

  an_il_header il_header = {};
  a_module_ptr trans_unit_module = nullptr;

  /**
   * @brief Test stub or helper for `evaluate_constant`.
   * @details Provides mock or helper behavior for `evaluate_constant` within `tests/test_llvm_decl.cpp`.
   * @param[in] con Parameter `con` for `evaluate_constant`.
   * @param[in] expected_ty Parameter `expected_ty` for `evaluate_constant`.
   * @param[out] out_const Parameter `out_const` for `evaluate_constant`.
   * @return Result of `evaluate_constant` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t evaluate_constant(a_constant_ptr con, llvm::Type* expected_ty, llvm::Constant** out_const) noexcept {
      *out_const = llvm::Constant::getNullValue(expected_ty);
      return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `pop_lexical_block`.
   * @details Provides mock or helper behavior for `pop_lexical_block` within `tests/test_llvm_decl.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `pop_lexical_block`.
   * @return Result of `pop_lexical_block` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t pop_lexical_block(llvm_gen_be_debug_state_t* arg_0) noexcept { return llvm_gen_be_error_t::ok; }
  static a_scope g_test_routine_scope = {};
  /**
   * @brief Test stub or helper for `scope_for_routine`.
   * @details Provides mock or helper behavior for `scope_for_routine` within `tests/test_llvm_decl.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `scope_for_routine`.
   * @return Result of `scope_for_routine` (a_scope_ptr).
   */
  a_scope_ptr scope_for_routine(a_routine_ptr arg_0) {
    if (arg_0 && arg_0->function_def_number == 1) return &g_test_routine_scope;
    return nullptr;
  }
  /**
   * @brief Test stub or helper for `create_di_subprogram`.
   * @details Provides mock or helper behavior for `create_di_subprogram` within `tests/test_llvm_decl.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `create_di_subprogram`.
   * @param[in] arg_1 Parameter `arg_1` for `create_di_subprogram`.
   * @param[in] arg_2 Parameter `arg_2` for `create_di_subprogram`.
   * @param[out] arg_3 Parameter `arg_3` for `create_di_subprogram`.
   * @return Result of `create_di_subprogram` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t create_di_subprogram(llvm_gen_be_debug_state_t* arg_0, a_routine_ptr arg_1, llvm::Function* arg_2, llvm::DISubprogram** arg_3) noexcept { return llvm_gen_be_error_t::ok; }
  /**
   * @brief Test stub or helper for `get_variable_initializer`.
   * @details Provides mock or helper behavior for `get_variable_initializer` within `tests/test_llvm_decl.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `get_variable_initializer`.
   * @param[in] arg_1 Parameter `arg_1` for `get_variable_initializer`.
   * @param[in] k Parameter `k` for `get_variable_initializer`.
   * @param[in] ptr Parameter `ptr` for `get_variable_initializer`.
   */
  void get_variable_initializer(a_variable_ptr arg_0, a_scope_ptr arg_1, an_init_kind* k, an_initializer_ptr* ptr) {
      *k = initk_none;
      *ptr = nullptr;
  }
  /**
   * @brief Test stub or helper for `emit_dbg_declare_for_variable`.
   * @details Provides mock or helper behavior for `emit_dbg_declare_for_variable` within `tests/test_llvm_decl.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `emit_dbg_declare_for_variable`.
   * @param[in] arg_1 Parameter `arg_1` for `emit_dbg_declare_for_variable`.
   * @param[in] arg_2 Parameter `arg_2` for `emit_dbg_declare_for_variable`.
   * @return Result of `emit_dbg_declare_for_variable` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t emit_dbg_declare_for_variable(llvm_gen_be_debug_state_t* arg_0, a_variable_ptr arg_1, llvm::AllocaInst* arg_2) noexcept { return llvm_gen_be_error_t::ok; }

  /**
   * @brief Test stub for `emit_dbg_global_variable` in `tests/test_llvm_decl.cpp`.
   * @details No-op stub returning `llvm_gen_be_error_t::ok`.
   * @param[in] arg_0 Debug state pointer.
   * @param[in] arg_1 EDG variable pointer.
   * @param[in] arg_2 LLVM GlobalVariable pointer.
   * @return llvm_gen_be_error_t::ok.
   */
  llvm_gen_be_error_t emit_dbg_global_variable(llvm_gen_be_debug_state_t* arg_0, a_variable_ptr arg_1, llvm::GlobalVariable* arg_2) noexcept {
    (void)arg_0; (void)arg_1; (void)arg_2;
    return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub for `emit_scope_imported_entities` in `tests/test_llvm_decl.cpp`.
   * @details Sets `*out_count = 0` and returns `llvm_gen_be_error_t::ok`.
   * @param[in] dbg_state Debug state pointer.
   * @param[in] scope EDG scope pointer.
   * @param[out] out_count Receives the number of emitted imported entities.
   * @return llvm_gen_be_error_t::ok.
   */
  llvm_gen_be_error_t emit_scope_imported_entities(
      llvm_gen_be_debug_state_t* dbg_state,
      a_scope_ptr scope,
      unsigned* out_count) noexcept {
    (void)dbg_state;
    (void)scope;
    if (out_count) *out_count = 0;
    return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `llvm_lower_global_variable`.
   * @details Provides mock or helper behavior for `llvm_lower_global_variable` within `tests/test_llvm_decl.cpp`.
   * @param[in] var Parameter `var` for `llvm_lower_global_variable`.
   * @return Result of `llvm_lower_global_variable` (llvm_gen_be_error_t).
   */
  extern llvm_gen_be_error_t llvm_lower_global_variable(a_variable_ptr var) noexcept;
}

/**
 * @brief Entry point for the declaration lowering unit test executable.
 * @details Tests TLS attributes, section attributes, virtual thunks, and constructor/destructor aliases.
 * @return 0 on success.
 */
int main(void) {
  printf("Running test_llvm_decl...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_decl", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  a_type int_ty = {}; int_ty.kind = tk_integer;
  a_variable var = {};
  var.type = &int_ty;
  a_source_correspondence corresp = {};
  corresp.name = "test_var";
  var.source_corresp = corresp;
  var.storage_class = sc_static;
  var.is_thread_local = TRUE;
  
  an_attribute attr = {};
  attr.kind = ak_section;
  an_attribute_arg arg = {};
  arg.kind = aak_token;
  arg.variant.token = "\"my_section\"";
  attr.arguments = &arg;
  
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
  an_attribute tls_attr = {};
  tls_attr.kind = ak_tls_model;
  an_attribute_arg tls_arg = {};
  tls_arg.kind = aak_constant;
  a_constant tls_const = {};
  tls_const.kind = ck_string;
  tls_const.variant.string.value = "initial-exec";
  tls_arg.variant.constant = &tls_const;
  tls_attr.arguments = &tls_arg;
  attr.next = &tls_attr;
#endif
  
  var.source_corresp.attributes = &attr;

  assert(llvm_lower_global_variable(&var) == llvm_gen_be_error_t::ok);
  
  llvm::GlobalVariable* gv = state.module->getNamedGlobal("test_var");
  assert(gv != nullptr);
  assert(gv->getLinkage() == llvm::GlobalValue::InternalLinkage);
#if THREAD_LOCAL_STORAGE_SPECIFIER_ALLOWED
  assert(gv->getThreadLocalMode() == llvm::GlobalValue::InitialExecTLSModel);
#endif
  assert(gv->getSection() == "my_section");


  
  
  // Test Thunk Generation
  a_routine target_routine = {};
  target_routine.source_corresp.name = "_Z5targetv";
  a_type ptr_ty = {};
  ptr_ty.kind = tk_pointer;
  ptr_ty.variant.pointer.type = &int_ty;

  a_type target_func_ty = {};
  target_func_ty.kind = tk_routine;
  a_routine_type_supplement target_supp = {};
  target_func_ty.variant.routine.extra_info = &target_supp;
  target_func_ty.variant.routine.return_type = &int_ty;
  
  a_param_type param1 = {};
  param1.type = &ptr_ty; // the 'this' pointer
  target_supp.param_type_list = &param1;
  
  target_routine.type = &target_func_ty;
  
  a_routine thunk_routine = {};
  thunk_routine.source_corresp.name = "_ZTv0_n24_5targetv";
  thunk_routine.type = &target_func_ty; 
  thunk_routine.overriding_function_for_wrapper = &target_routine; 
  thunk_routine.delta = 16;
  thunk_routine.vcall_index = -24; 
  thunk_routine.return_delta = 8; 
  
  a_routine c1_ctor = {};
  c1_ctor.source_corresp.name = "_ZN1CC1Ev";
  c1_ctor.type = &target_func_ty;
  c1_ctor.function_def_number = 1;

  a_variable this_var = {};
  this_var.source_corresp.name = "this";
  this_var.type = &ptr_ty;
  edg::g_test_routine_scope.variant.routine.parameters = &this_var;

  a_field ctor_field = {};
  ctor_field.offset = 0;
  ctor_field.type = &int_ty;
  a_dynamic_init field_dip = {};
  field_dip.kind = dik_zero;
  a_constructor_init c_init = {};
  c_init.kind = cik_field;
  c_init.variant.field = &ctor_field;
  c_init.initializer = &field_dip;
  edg::g_test_routine_scope.variant.routine.constructor_inits = &c_init;
  
  a_routine c2_ctor = {};
  c2_ctor.source_corresp.name = "_ZN1CC2Ev";
  c2_ctor.type = &target_func_ty;
  c2_ctor.primary_ctor_or_dtor = &c1_ctor;
  c1_ctor.ctor_dtor_kind = cdk_complete;
  c2_ctor.ctor_dtor_kind = cdk_subobject;
  c2_ctor.is_alias_entry = TRUE;

  a_routine d1_dtor = {};
  d1_dtor.source_corresp.name = "_ZN1CD1Ev";
  d1_dtor.type = &target_func_ty;

  a_routine d0_dtor = {};
  d0_dtor.source_corresp.name = "_ZN1CD0Ev";
  d0_dtor.type = &target_func_ty;
  d0_dtor.primary_ctor_or_dtor = &d1_dtor;
  d1_dtor.ctor_dtor_kind = cdk_complete;
  d0_dtor.is_alias_entry = TRUE;
  d0_dtor.ctor_dtor_kind = cdk_deleting;

  a_scope mock_scope = {};
  mock_scope.routines = &target_routine;
  target_routine.next = &thunk_routine;
  thunk_routine.next = &c1_ctor;
  c1_ctor.next = &c2_ctor;
  c2_ctor.next = &d1_dtor;
  d1_dtor.next = &d0_dtor;
  
  a_scope_ptr old_scope = il_header.primary_scope;
  il_header.primary_scope = &mock_scope;
  
  assert(llvm_lower_function_declarations() == llvm_gen_be_error_t::ok);
  assert(llvm_lower_function_definitions() == llvm_gen_be_error_t::ok);
  
  llvm::Function* target_f = state.module->getFunction("_Z5targetv");
  assert(target_f != nullptr);
  
  llvm::Function* thunk_f = state.module->getFunction("_ZTv0_n24_5targetv");
  assert(thunk_f != nullptr);
  assert(!thunk_f->empty());
  
  llvm::Function* c1_f = state.module->getFunction("_ZN1CC1Ev");
  assert(c1_f != nullptr);
  
  llvm::Function* c2_f = state.module->getFunction("_ZN1CC2Ev");
  assert(c2_f != nullptr);
  assert(!c2_f->empty());

  llvm::Function* d1_f = state.module->getFunction("_ZN1CD1Ev");
  assert(d1_f != nullptr);
  
  llvm::Function* d0_f = state.module->getFunction("_ZN1CD0Ev");
  assert(d0_f != nullptr);
  assert(!d0_f->empty());
  
  // Verify operator delete is called
  llvm::Function* del_f = state.module->getFunction("_ZdlPv");
  assert(del_f != nullptr);

  il_header.primary_scope = old_scope;

  // Test X86 32-bit ABI
  EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "i386-unknown-linux-gnu");
  llvm::FunctionType* x86_func_ty = nullptr;
  llvm::AttributeList x86_attrs;
  assert(build_x86_32_function_type(&target_func_ty, &x86_func_ty, &x86_attrs) == llvm_gen_be_error_t::ok);
  assert(x86_func_ty != nullptr);

  // Test ARM 32-bit ABI
  EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "armv7-unknown-linux-gnueabihf");
  llvm::FunctionType* arm_func_ty = nullptr;
  llvm::AttributeList arm_attrs;
  assert(build_arm32_function_type(&target_func_ty, &arm_func_ty, &arm_attrs) == llvm_gen_be_error_t::ok);
  assert(arm_func_ty != nullptr);
  
  // Test WASM ABI
  EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "wasm32-unknown-unknown");
  llvm::FunctionType* wasm_func_ty = nullptr;
  llvm::AttributeList wasm_attrs;
  assert(build_wasm_function_type(&target_func_ty, &wasm_func_ty, &wasm_attrs) == llvm_gen_be_error_t::ok);
  assert(wasm_func_ty != nullptr);

  printf("PASS\n");
  return 0;
}

namespace edg {
  /**
   * @brief Stub implementation of `llvm_lower_expression` for declaration unit tests.
   * @details Produces a constant `i32 0` value.
   * @param[in] expr The EDG expression node.
   * @param[out] out_val Receives the lowered `llvm::Value*`.
   * @return llvm_gen_be_error_t::ok.
   */
  llvm_gen_be_error_t llvm_lower_expression(an_expr_node_ptr expr, llvm::Value** out_val) noexcept {
    (void)expr;
    *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0);
    return llvm_gen_be_error_t::ok;
  }
  /**
   * @brief Test stub or helper for `get_di_location`.
   * @details Provides mock or helper behavior for `get_di_location` within `tests/test_llvm_decl.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `get_di_location`.
   * @param[in] arg_1 Parameter `arg_1` for `get_di_location`.
   * @param[in] arg_2 Parameter `arg_2` for `get_di_location`.
   * @param[out] arg_3 Parameter `arg_3` for `get_di_location`.
   * @return Result of `get_di_location` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_di_location(llvm_gen_be_debug_state_t* arg_0, a_source_position arg_1, llvm::DIScope* arg_2, llvm::DILocation** arg_3) noexcept { return llvm_gen_be_error_t::ok; }
  /**
   * @brief Test stub or helper for `push_lexical_block`.
   * @details Provides mock or helper behavior for `push_lexical_block` within `tests/test_llvm_decl.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `push_lexical_block`.
   * @param[in] arg_1 Parameter `arg_1` for `push_lexical_block`.
   * @param[out] arg_2 Parameter `arg_2` for `push_lexical_block`.
   * @return Result of `push_lexical_block` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t push_lexical_block(llvm_gen_be_debug_state_t* arg_0, a_source_position arg_1, llvm::DILexicalBlock** arg_2) noexcept { return llvm_gen_be_error_t::ok; }
  /**
   * @brief Test stub or helper for `llvm_lower_asm_stmt`.
   * @details Provides mock or helper behavior for `llvm_lower_asm_stmt` within `tests/test_llvm_decl.cpp`.
   * @param[in] stmt Parameter `stmt` for `llvm_lower_asm_stmt`.
   * @return Result of `llvm_lower_asm_stmt` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_asm_stmt(a_statement_ptr stmt) noexcept { (void)stmt; return llvm_gen_be_error_t::ok; }
  /**
   * @brief Test stub or helper for `llvm_lower_coroutine_stmt`.
   * @details Provides mock or helper behavior for `llvm_lower_coroutine_stmt` within `tests/test_llvm_decl.cpp`.
   * @param[in] stmt Parameter `stmt` for `llvm_lower_coroutine_stmt`.
   * @return Result of `llvm_lower_coroutine_stmt` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_coroutine_stmt(a_statement_ptr stmt) noexcept { (void)stmt; return llvm_gen_be_error_t::ok; }
  /**
   * @brief Test stub or helper for `llvm_lower_coroutine_return_stmt`.
   * @details Provides mock or helper behavior for `llvm_lower_coroutine_return_stmt` within `tests/test_llvm_decl.cpp`.
   * @param[in] stmt Parameter `stmt` for `llvm_lower_coroutine_return_stmt`.
   * @return Result of `llvm_lower_coroutine_return_stmt` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_coroutine_return_stmt(a_statement_ptr stmt) noexcept { (void)stmt; return llvm_gen_be_error_t::ok; }

  /**
   * @brief Test stub for `llvm_emit_vtable`.
   * @details Emits or retrieves a mock `_ZTVmock` global variable in `tests/test_llvm_decl.cpp`.
   * @param[in] type The EDG class type pointer.
   * @param[out] out_const Receives the mock vtable constant pointer.
   * @return llvm_gen_be_error_t::ok.
   */
  llvm_gen_be_error_t llvm_emit_vtable(a_type_ptr type, llvm::Constant** out_const) noexcept {
    (void)type;
    if (!out_const) return llvm_gen_be_error_t::invalid_argument;
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    *out_const = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
    return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub for `llvm_emit_vtt`.
   * @details Emits or retrieves a mock `_ZTTmock` global variable in `tests/test_llvm_decl.cpp`.
   * @param[in] type The EDG class type pointer.
   * @param[out] out_const Receives the mock VTT constant pointer.
   * @return llvm_gen_be_error_t::ok.
   */
  llvm_gen_be_error_t llvm_emit_vtt(a_type_ptr type, llvm::Constant** out_const) noexcept {
    (void)type;
    if (!out_const) return llvm_gen_be_error_t::invalid_argument;
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    *out_const = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
    return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub for `llvm_emit_msvc_vbtable`.
   * @details Emits or retrieves a mock `??_8mock7B@` global variable in `tests/test_llvm_decl.cpp`.
   * @param[in] type The EDG class type pointer.
   * @param[out] out_const Receives the mock vbtable constant pointer.
   * @return llvm_gen_be_error_t::ok.
   */
  llvm_gen_be_error_t llvm_emit_msvc_vbtable(a_type_ptr type, llvm::Constant** out_const) noexcept {
    (void)type;
    if (!out_const) return llvm_gen_be_error_t::invalid_argument;
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    *out_const = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
    return llvm_gen_be_error_t::ok;
  }
}
