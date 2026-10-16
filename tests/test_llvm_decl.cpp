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
  a_type_qualifier_set f_get_type_qualifiers(a_type_ptr type, a_boolean x) { return 0; }
  a_boolean is_reference_type(a_type_ptr ty) { return FALSE; }
  a_boolean is_empty_class_type(a_type_ptr ty) { return ty && ty->variant.class_struct_union.is_empty_class; }
  a_byte_boolean int_kind_is_signed[ik_last] = {0};

  a_boolean sanitize_address = FALSE; a_boolean sanitize_thread = FALSE; a_boolean sanitize_memory = FALSE; a_boolean sanitize_undefined = FALSE; a_boolean sanitize_hwaddress = FALSE; int stack_protector_mode = 0; a_boolean use_safestack = FALSE; a_boolean use_shadowcallstack = FALSE;

  an_il_header il_header = {};

  llvm_gen_be_error_t evaluate_constant(a_constant_ptr con, llvm::Type* expected_ty, llvm::Constant** out_const) noexcept {
      *out_const = llvm::Constant::getNullValue(expected_ty);
      return llvm_gen_be_error_t::ok;
  }
  
  llvm_gen_be_error_t pop_lexical_block(llvm_gen_be_debug_state_t*) noexcept { return llvm_gen_be_error_t::ok; }
  a_scope_ptr scope_for_routine(a_routine_ptr) { return nullptr; }
  llvm_gen_be_error_t create_di_subprogram(llvm_gen_be_debug_state_t*, a_routine_ptr, llvm::Function*, llvm::DISubprogram**) noexcept { return llvm_gen_be_error_t::ok; }
  void get_variable_initializer(a_variable_ptr, a_scope_ptr, an_init_kind* k, an_initializer_ptr* ptr) {
      *k = initk_none;
      *ptr = nullptr;
  }
  llvm_gen_be_error_t emit_dbg_declare_for_variable(llvm_gen_be_debug_state_t*, a_variable_ptr, llvm::AllocaInst*) noexcept { return llvm_gen_be_error_t::ok; }
  
  extern llvm_gen_be_error_t llvm_lower_global_variable(a_variable_ptr var) noexcept;
}

int main() {
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
  state.module->setTargetTriple(llvm::Triple("i386-unknown-linux-gnu"));
  llvm::FunctionType* x86_func_ty = nullptr;
  llvm::AttributeList x86_attrs;
  assert(build_x86_32_function_type(&target_func_ty, &x86_func_ty, &x86_attrs) == llvm_gen_be_error_t::ok);
  assert(x86_func_ty != nullptr);

  // Test ARM 32-bit ABI
  state.module->setTargetTriple(llvm::Triple("armv7-unknown-linux-gnueabihf"));
  llvm::FunctionType* arm_func_ty = nullptr;
  llvm::AttributeList arm_attrs;
  assert(build_arm32_function_type(&target_func_ty, &arm_func_ty, &arm_attrs) == llvm_gen_be_error_t::ok);
  assert(arm_func_ty != nullptr);
  
  // Test WASM ABI
  state.module->setTargetTriple(llvm::Triple("wasm32-unknown-unknown"));
  llvm::FunctionType* wasm_func_ty = nullptr;
  llvm::AttributeList wasm_attrs;
  assert(build_wasm_function_type(&target_func_ty, &wasm_func_ty, &wasm_attrs) == llvm_gen_be_error_t::ok);
  assert(wasm_func_ty != nullptr);

  printf("PASS\n");
  return 0;
}namespace edg {
llvm_gen_be_error_t llvm_lower_expression(an_expr_node_ptr expr, llvm::Value** out_val) noexcept {
    *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0);
    return llvm_gen_be_error_t::ok;
}
  llvm_gen_be_error_t get_di_location(llvm_gen_be_debug_state_t*, a_source_position, llvm::DIScope*, llvm::DILocation**) noexcept { return llvm_gen_be_error_t::ok; }
  llvm_gen_be_error_t push_lexical_block(llvm_gen_be_debug_state_t*, a_source_position, llvm::DILexicalBlock**) noexcept { return llvm_gen_be_error_t::ok; }
  llvm_gen_be_error_t llvm_lower_asm_stmt(a_statement_ptr stmt) noexcept { return llvm_gen_be_error_t::ok; }
  llvm_gen_be_error_t llvm_lower_coroutine_stmt(a_statement_ptr stmt) noexcept { return llvm_gen_be_error_t::ok; }
  llvm_gen_be_error_t llvm_lower_coroutine_return_stmt(a_statement_ptr stmt) noexcept { return llvm_gen_be_error_t::ok; }
}
