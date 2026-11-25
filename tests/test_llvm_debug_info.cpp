/**
 * @file test_llvm_debug_info.cpp
 * @ingroup llvm_gen_be_tests
 * @brief Unit tests for the LLVM backend debug info subsystem.
 * @details Validates function coverage of llvm_gen_be_debug.cpp,
 * llvm_gen_be_debug_scopes.cpp, and llvm_gen_be_debug_types.cpp.
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
  // Stubs
  a_boolean g_is_bool = FALSE; ///< Configurable return value for `is_bool_type`.
  int g_seq_mode = 0; ///< Controls `conv_seq_to_file_and_line` output (0=file only, 1=full_name, 2=both null).

  /**
   * @brief Test stub or helper for `is_bool_type`.
   * @details Provides mock or helper behavior for `is_bool_type` within `tests/test_llvm_debug_info.cpp`.
   * @param[in] ty Parameter `ty` for `is_bool_type`.
   * @return Result of `is_bool_type` (a_boolean).
   */
  a_boolean is_bool_type(a_type_ptr ty) {
    (void)ty;
    return g_is_bool;
  }
  /**
   * @brief Test stub or helper for `f_alignment_of_type`.
   * @details Provides mock or helper behavior for `f_alignment_of_type` within `tests/test_llvm_debug_info.cpp`.
   * @param[in] ty Parameter `ty` for `f_alignment_of_type`.
   * @return Result of `f_alignment_of_type` (a_targ_alignment).
   */
  a_targ_alignment f_alignment_of_type(a_type_ptr ty) { return 1; }
  /**
   * @brief Test stub or helper for `is_trivially_copyable_type`.
   * @details Provides mock or helper behavior for `is_trivially_copyable_type` within `tests/test_llvm_debug_info.cpp`.
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
   * @details Provides mock or helper behavior for `mangled_typeinfo_name` within `tests/test_llvm_debug_info.cpp`.
   * @param[in] type Parameter `type` for `mangled_typeinfo_name`.
   * @return Result of `mangled_typeinfo_name` (char*).
   */
  char* mangled_typeinfo_name(a_type_ptr type) { return (char*)"_ZTIi"; }
  /**
   * @brief Test stub or helper for `f_get_type_qualifiers`.
   * @details Provides mock or helper behavior for `f_get_type_qualifiers` within `tests/test_llvm_debug_info.cpp`.
   * @param[in] type Parameter `type` for `f_get_type_qualifiers`.
   * @param[in] x Parameter `x` for `f_get_type_qualifiers`.
   * @return Result of `f_get_type_qualifiers` (a_type_qualifier_set).
   */
  a_type_qualifier_set f_get_type_qualifiers(a_type_ptr type, a_boolean x) { return 0; }
  /**
   * @brief Test stub or helper for `is_reference_type`.
   * @details Provides mock or helper behavior for `is_reference_type` within `tests/test_llvm_debug_info.cpp`.
   * @param[in] ty Parameter `ty` for `is_reference_type`.
   * @return Result of `is_reference_type` (a_boolean).
   */
  a_boolean is_reference_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `is_empty_class_type`.
   * @details Provides mock or helper behavior for `is_empty_class_type` within `tests/test_llvm_debug_info.cpp`.
   * @param[in] ty Parameter `ty` for `is_empty_class_type`.
   * @return Result of `is_empty_class_type` (a_boolean).
   */
  a_boolean is_empty_class_type(a_type_ptr ty) { return ty && ty->variant.class_struct_union.is_empty_class; }
  a_byte_boolean int_kind_is_signed[ik_last] = {0};
  
  an_il_header il_header = {};

  /**
   * @brief Test stub or helper for `conv_seq_to_file_and_line`.
   * @details Provides mock or helper behavior for `conv_seq_to_file_and_line` within `tests/test_llvm_debug_info.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `conv_seq_to_file_and_line`.
   * @param[out] file Parameter `file` for `conv_seq_to_file_and_line`.
   * @param[out] arg_2 Parameter `arg_2` for `conv_seq_to_file_and_line`.
   * @param[in] line Parameter `line` for `conv_seq_to_file_and_line`.
   * @param[in] arg_4 Parameter `arg_4` for `conv_seq_to_file_and_line`.
   * @return Result of `conv_seq_to_file_and_line` (a_source_file_ptr).
   */
  a_source_file_ptr conv_seq_to_file_and_line(a_seq_number arg_0, a_const_char** file, a_const_char** arg_2, a_line_number* line, a_boolean* arg_4) {
      (void)arg_0;
      (void)arg_4;
      if (g_seq_mode == 1) {
          *file = "test.cpp";
          *arg_2 = "CMakeLists.txt";
      } else if (g_seq_mode == 2) {
          *file = nullptr;
          *arg_2 = nullptr;
      } else {
          *file = "test.cpp";
          *arg_2 = nullptr;
      }
      *line = 1;
      return nullptr;
  }
}

/**
 * @brief Entry point for the DWARF and CodeView debug information unit test suite.
 * @details Exercises `debug_info_init`, subprograms, lexical blocks, locations, variables, and rich C++ types.
 * @return 0 on success.
 */
int main(void) {
  printf("Running test_llvm_debug_info...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_debug", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  // Test debug_info_init, debug_info_finalize, debug_info_cleanup, and get_or_create_di_file null/edge cases
  assert(debug_info_init(nullptr, state.module.get(), "test.cpp", "edg", false) == llvm_gen_be_error_t::invalid_argument);
  assert(debug_info_init(&be_state->dbg_state, nullptr, "test.cpp", "edg", false) == llvm_gen_be_error_t::invalid_argument);
  assert(debug_info_init(&be_state->dbg_state, state.module.get(), nullptr, "edg", false) == llvm_gen_be_error_t::invalid_argument);
  assert(debug_info_finalize(nullptr) == llvm_gen_be_error_t::invalid_argument);
  llvm_gen_be_debug_state_t empty_dbg = {};
  assert(debug_info_finalize(&empty_dbg) == llvm_gen_be_error_t::invalid_argument);
  assert(debug_info_cleanup(nullptr) == llvm_gen_be_error_t::invalid_argument);
  llvm_gen_be_debug_state_t* null_dbg_ptr = nullptr;
  assert(debug_info_cleanup(&null_dbg_ptr) == llvm_gen_be_error_t::invalid_argument);
  llvm_gen_be_debug_state_t* no_builder_dbg = new (std::nothrow) llvm_gen_be_debug_state_t();
  assert(no_builder_dbg != nullptr);
  assert(debug_info_cleanup(&no_builder_dbg) == llvm_gen_be_error_t::ok);

  // Test Windows MSVC debug_info_init branch + existing file MD5 checksum in get_or_create_di_file
  {
    llvm::Module msvc_mod("msvc_debug", *state.context);
    EDG_LLVM_SET_TARGET_TRIPLE(&msvc_mod, "x86_64-pc-windows-msvc");
    llvm_gen_be_debug_state_t* msvc_dbg = nullptr;
    assert(debug_info_init(&msvc_dbg, &msvc_mod, "CMakeLists.txt", ".", true) == llvm_gen_be_error_t::ok);
    assert(msvc_dbg->is_codeview == true);
    llvm::DIFile* cached_f = nullptr;
    assert(get_or_create_di_file(nullptr, "CMakeLists.txt", &cached_f) == llvm_gen_be_error_t::invalid_argument);
    assert(get_or_create_di_file(&empty_dbg, "CMakeLists.txt", &cached_f) == llvm_gen_be_error_t::invalid_argument);
    assert(get_or_create_di_file(msvc_dbg, nullptr, &cached_f) == llvm_gen_be_error_t::invalid_argument);
    assert(get_or_create_di_file(msvc_dbg, "CMakeLists.txt", nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(get_or_create_di_file(msvc_dbg, "CMakeLists.txt", &cached_f) == llvm_gen_be_error_t::ok); // cache hit!
    assert(debug_info_cleanup(&msvc_dbg) == llvm_gen_be_error_t::ok);
  }

  llvm_gen_be_error_t err = debug_info_init(&be_state->dbg_state, state.module.get(), "test.cpp", "edg", false);
  assert(err == llvm_gen_be_error_t::ok);

#if TARG_MICROSOFT
  assert(state.module->getModuleFlag("CodeView") != nullptr);
#else
  assert(state.module->getModuleFlag("Dwarf Version") != nullptr);
#endif

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);

  a_routine r = {};
  r.source_corresp.name = "test_func";
  llvm::DISubprogram* sp = nullptr;
  err = create_di_subprogram(be_state->dbg_state, &r, f, &sp);
  assert(err == llvm_gen_be_error_t::ok);

  llvm::DILexicalBlock* block = nullptr;
  a_source_position pos = {};
  err = push_lexical_block(be_state->dbg_state, pos, &block);
  assert(err == llvm_gen_be_error_t::ok);

  llvm::DILocation* loc = nullptr;
  err = get_di_location(be_state->dbg_state, pos, block, &loc);
  assert(err == llvm_gen_be_error_t::ok);

  llvm::AllocaInst* alloca = state.builder->CreateAlloca(llvm::Type::getInt32Ty(*state.context));
  a_variable var = {};
  var.source_corresp.name = "my_var";
  a_type ty = {};
  ty.kind = tk_integer;
  var.type = &ty;
  err = emit_dbg_declare_for_variable(be_state->dbg_state, &var, alloca);
  assert(err == llvm_gen_be_error_t::ok);

  
  // Phase 8.1 Rich DWARF C++ Type Descriptors Tests
  llvm::DIType* base_di_ty = nullptr;
  err = get_or_create_di_type(be_state->dbg_state, &ty, &base_di_ty);
  assert(err == llvm_gen_be_error_t::ok);
  
  a_type derived_ty = {};
  derived_ty.kind = tk_class;
  derived_ty.size = 16;
  
  a_field dummy_field = {};
  dummy_field.is_bit_field = TRUE;
  dummy_field.bit_size = 3;
  dummy_field.offset = 4;
  dummy_field.type = &ty;
  derived_ty.variant.class_struct_union.field_list = &dummy_field;
  
  a_class_type_supplement dummy_supp = {};
  a_base_class dummy_base = {};
  dummy_base.type = &ty;
  dummy_base.is_virtual = TRUE;
  dummy_supp.direct_base_classes = &dummy_base;
  derived_ty.variant.class_struct_union.extra_info = &dummy_supp;

  llvm::DIType* derived_di_ty = nullptr;
  err = get_or_create_di_type(be_state->dbg_state, &derived_ty, &derived_di_ty);
  assert(err == llvm_gen_be_error_t::ok);
  
  llvm::DIDerivedType* inheritance = nullptr;
  err = createInheritance(be_state->dbg_state, derived_di_ty, base_di_ty, 0, 0, llvm::DINode::FlagZero, &inheritance);
  assert(err == llvm_gen_be_error_t::ok);
  
  llvm::DISubprogram* method = nullptr;
  llvm::DISubroutineType* method_ty = be_state->dbg_state->builder->createSubroutineType(
      be_state->dbg_state->builder->getOrCreateTypeArray(llvm::ArrayRef<llvm::Metadata*>()));
  err = createMethod(be_state->dbg_state, sp, "my_method", "_ZN1A9my_methodEv", be_state->dbg_state->compile_unit->getFile(), 
                     10, method_ty, 0, 0, nullptr, llvm::DINode::FlagZero, llvm::DISubprogram::SPFlagZero, &method);
  assert(err == llvm_gen_be_error_t::ok);
  
  llvm::DINamespace* ns = nullptr;
  err = createNameSpace(be_state->dbg_state, be_state->dbg_state->compile_unit, "my_namespace", true, &ns);
  assert(err == llvm_gen_be_error_t::ok);
  
  llvm::DITemplateTypeParameter* ttp = nullptr;
  err = createTemplateTypeParameter(be_state->dbg_state, ns, "T", base_di_ty, false, &ttp);
  assert(err == llvm_gen_be_error_t::ok);
  
  llvm::Metadata* tv_val = llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 42));
  llvm::DITemplateValueParameter* tvp = nullptr;
  err = createTemplateValueParameter(be_state->dbg_state, ns, "V", base_di_ty, false, tv_val, &tvp);
  assert(err == llvm_gen_be_error_t::ok);
  
  llvm::DICompositeType* enum_ty = nullptr;
  llvm::SmallVector<llvm::Metadata*, 1> elements;
  elements.push_back(be_state->dbg_state->builder->createEnumerator("A", 0, true));
  err = createEnumerationType(be_state->dbg_state, ns, "my_enum", be_state->dbg_state->compile_unit->getFile(), 20, 32, 32, 
                              be_state->dbg_state->builder->getOrCreateArray(elements), base_di_ty, "_ZTI7my_enum", true, &enum_ty);
  assert(err == llvm_gen_be_error_t::ok);

uint64_t expr_ops[] = { llvm::dwarf::DW_OP_deref, llvm::dwarf::DW_OP_plus_uconst, 4 };
assert(emit_dbg_value_for_variable(be_state->dbg_state, &var, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 1), alloca, nullptr, expr_ops, 3) == llvm_gen_be_error_t::ok);

assert(emit_dbg_assign_for_variable(be_state->dbg_state, &var, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 1), alloca, alloca) == llvm_gen_be_error_t::ok);

// Test emit_dbg_global_variable (null checks, bad type, named/unnamed, static/internal/external, seq modes 0, 1, 2)
llvm::GlobalVariable* test_gv = llvm::cast<llvm::GlobalVariable>(
    state.module->getOrInsertGlobal("dbg_gv", llvm::Type::getInt32Ty(*state.context)));
test_gv->setConstant(false);
test_gv->setLinkage(llvm::GlobalValue::ExternalLinkage);
test_gv->setInitializer(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 0));
assert(test_gv != nullptr);
assert(emit_dbg_global_variable(nullptr, &var, test_gv) == llvm_gen_be_error_t::invalid_argument);
assert(emit_dbg_global_variable(be_state->dbg_state, nullptr, test_gv) == llvm_gen_be_error_t::invalid_argument);
assert(emit_dbg_global_variable(be_state->dbg_state, &var, nullptr) == llvm_gen_be_error_t::invalid_argument);
a_variable gv_no_ty = {};
gv_no_ty.type = nullptr;
assert(emit_dbg_global_variable(be_state->dbg_state, &gv_no_ty, test_gv) == llvm_gen_be_error_t::invalid_argument);
assert(emit_dbg_global_variable(be_state->dbg_state, &var, test_gv) == llvm_gen_be_error_t::ok);
a_variable gv_anon = {};
gv_anon.type = &ty;
gv_anon.source_corresp.name = nullptr;
gv_anon.storage_class = sc_static;
gv_anon.source_corresp.decl_position.seq = 1;
g_seq_mode = 0;
assert(emit_dbg_global_variable(be_state->dbg_state, &gv_anon, test_gv) == llvm_gen_be_error_t::ok);
gv_anon.storage_class = sc_unspecified;
gv_anon.source_corresp.name_linkage = nlk_internal;
g_seq_mode = 1;
assert(emit_dbg_global_variable(be_state->dbg_state, &gv_anon, test_gv) == llvm_gen_be_error_t::ok);
g_seq_mode = 2;
assert(emit_dbg_global_variable(be_state->dbg_state, &gv_anon, test_gv) == llvm_gen_be_error_t::ok);
g_seq_mode = 0;

err = pop_lexical_block(be_state->dbg_state);
assert(err == llvm_gen_be_error_t::ok);

  // Test get_or_create_di_type on tk_enum, tk_void, and tk_routine
  a_type enum_ast_ty = {};
  enum_ast_ty.kind = tk_enum;
  enum_ast_ty.size = 4;
  enum_ast_ty.alignment = 4;
  enum_ast_ty.source_corresp.name = (char*)"Color";
  a_constant enum_c1 = {};
  enum_c1.source_corresp.name = (char*)"Red";
  enum_ast_ty.variant.integer.enum_info.constant_list = &enum_c1;
  llvm::DIType* enum_di_ty = nullptr;
  assert(get_or_create_di_type(be_state->dbg_state, &enum_ast_ty, &enum_di_ty) == llvm_gen_be_error_t::ok);
  assert(enum_di_ty != nullptr);

  a_type void_ast_ty = {};
  void_ast_ty.kind = tk_void;
  llvm::DIType* void_di_ty = nullptr;
  assert(get_or_create_di_type(be_state->dbg_state, &void_ast_ty, &void_di_ty) == llvm_gen_be_error_t::ok);
  assert(void_di_ty != nullptr);

  a_type rout_ast_ty = {};
  rout_ast_ty.kind = tk_routine;
  rout_ast_ty.variant.routine.return_type = &ty;
  a_routine_type_supplement rout_supp = {};
  a_param_type rout_param = {};
  rout_param.type = &ty;
  rout_supp.param_type_list = &rout_param;
  rout_ast_ty.variant.routine.extra_info = &rout_supp;
  llvm::DIType* rout_di_ty = nullptr;
  assert(get_or_create_di_type(be_state->dbg_state, &rout_ast_ty, &rout_di_ty) == llvm_gen_be_error_t::ok);
  assert(rout_di_ty != nullptr);

  // Test build_pdb_symbol_records (null checks, valid function/block scopes, and uninitialized debug state)
  llvm::DIScope* pdb_node = nullptr;
  a_scope func_scope = {};
  func_scope.kind = sck_function;
  func_scope.variant.routine.ptr = &r;
  func_scope.nonstatic_variables = &var;
  assert(build_pdb_symbol_records(nullptr, &pdb_node) == llvm_gen_be_error_t::invalid_argument);
  assert(build_pdb_symbol_records(&func_scope, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(build_pdb_symbol_records(&func_scope, &pdb_node) == llvm_gen_be_error_t::ok);
  assert(pdb_node != nullptr);
  assert(state.module->getModuleFlag("CodeView") != nullptr);

  a_scope block_scope = {};
  block_scope.kind = sck_block;
  block_scope.variables = &var;
  llvm::DIScope* pdb_block_node = nullptr;
  assert(build_pdb_symbol_records(&block_scope, &pdb_block_node) == llvm_gen_be_error_t::ok);
  assert(pdb_block_node != nullptr);

  llvm_gen_be_debug_state_t* saved_dbg = be_state->dbg_state;
  be_state->dbg_state = nullptr;
  assert(build_pdb_symbol_records(&func_scope, &pdb_node) == llvm_gen_be_error_t::di_metadata_failure);
  be_state->dbg_state = saved_dbg;

  // Test build_dwarf_variable_fragment and apply_instruction_debug_loc
  llvm::DIExpression* frag_expr = nullptr;
  assert(build_dwarf_variable_fragment(nullptr, 0, 32, &frag_expr) == llvm_gen_be_error_t::invalid_argument);
  assert(build_dwarf_variable_fragment(&ty, 0, 32, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(build_dwarf_variable_fragment(&ty, 0, 32, &frag_expr) == llvm_gen_be_error_t::ok);
  assert(frag_expr != nullptr);

  a_source_position inst_pos = {};
  inst_pos.seq = 10;
  inst_pos.column = 4;
  assert(apply_instruction_debug_loc(nullptr, inst_pos) == llvm_gen_be_error_t::invalid_argument);
  assert(apply_instruction_debug_loc(alloca, inst_pos) == llvm_gen_be_error_t::ok);

  // Additional branch coverage for debug_scopes.cpp, debug_types.cpp, and debug.cpp
  {
    // get_di_location null checks & seq != 0
    assert(get_di_location(nullptr, inst_pos, sp, &loc) == llvm_gen_be_error_t::invalid_argument);
    assert(get_di_location(be_state->dbg_state, inst_pos, nullptr, &loc) == llvm_gen_be_error_t::invalid_argument);
    assert(get_di_location(be_state->dbg_state, inst_pos, sp, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(get_di_location(be_state->dbg_state, inst_pos, sp, &loc) == llvm_gen_be_error_t::ok);

    // pop_lexical_block to empty stack, then test empty-stack branches
    assert(pop_lexical_block(nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(pop_lexical_block(be_state->dbg_state) == llvm_gen_be_error_t::ok); // pops sp
    assert(be_state->dbg_state->scope_stack.empty());
    assert(pop_lexical_block(be_state->dbg_state) == llvm_gen_be_error_t::invalid_argument);
    assert(push_lexical_block(nullptr, inst_pos, &block) == llvm_gen_be_error_t::invalid_argument);
    assert(push_lexical_block(be_state->dbg_state, inst_pos, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(push_lexical_block(be_state->dbg_state, inst_pos, &block) == llvm_gen_be_error_t::invalid_argument); // empty stack
    assert(apply_instruction_debug_loc(alloca, inst_pos) == llvm_gen_be_error_t::ok); // empty stack early return

    // apply_instruction_debug_loc when be_state->dbg_state == nullptr
    llvm_gen_be_debug_state_t* saved = be_state->dbg_state;
    be_state->dbg_state = nullptr;
    assert(apply_instruction_debug_loc(alloca, inst_pos) == llvm_gen_be_error_t::ok);
    be_state->dbg_state = saved;

    // Emit variable declare/value/assign with empty scope_stack, parameter=TRUE, null name, and seq modes 0, 1, 2
    a_variable param_var = {};
    param_var.is_parameter = TRUE;
    param_var.type = &ty;
    param_var.source_corresp.decl_position.seq = 1;
    g_seq_mode = 0;
    assert(emit_dbg_declare_for_variable(be_state->dbg_state, &param_var, alloca) == llvm_gen_be_error_t::ok);
    assert(emit_dbg_value_for_variable(be_state->dbg_state, &param_var, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 2), nullptr, bb, nullptr, 0) == llvm_gen_be_error_t::ok);
    g_seq_mode = 1;
    assert(emit_dbg_declare_for_variable(be_state->dbg_state, &param_var, alloca) == llvm_gen_be_error_t::ok);
    assert(emit_dbg_value_for_variable(be_state->dbg_state, &param_var, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 2), nullptr, bb, nullptr, 0) == llvm_gen_be_error_t::ok);
    g_seq_mode = 2;
    assert(emit_dbg_declare_for_variable(be_state->dbg_state, &param_var, alloca) == llvm_gen_be_error_t::ok);
    assert(emit_dbg_value_for_variable(be_state->dbg_state, &param_var, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 2), nullptr, bb, expr_ops, 0) == llvm_gen_be_error_t::ok);

    g_seq_mode = 0;
    assert(emit_dbg_assign_for_variable(be_state->dbg_state, &param_var, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 1), llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 0), alloca) == llvm_gen_be_error_t::ok);
    g_seq_mode = 1;
    assert(emit_dbg_assign_for_variable(be_state->dbg_state, &param_var, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 1), alloca, alloca) == llvm_gen_be_error_t::ok);
    g_seq_mode = 2;
    assert(emit_dbg_assign_for_variable(be_state->dbg_state, &param_var, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 1), alloca, alloca) == llvm_gen_be_error_t::ok);

    // Null argument & error checks on emit_dbg_declare / value / assign
    a_variable bad_var = {};
    bad_var.type = nullptr;
    assert(emit_dbg_declare_for_variable(nullptr, &var, alloca) == llvm_gen_be_error_t::invalid_argument);
    assert(emit_dbg_declare_for_variable(be_state->dbg_state, nullptr, alloca) == llvm_gen_be_error_t::invalid_argument);
    assert(emit_dbg_declare_for_variable(be_state->dbg_state, &var, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(emit_dbg_declare_for_variable(be_state->dbg_state, &bad_var, alloca) == llvm_gen_be_error_t::invalid_argument);

    llvm::Value* c_one = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 1);
    assert(emit_dbg_value_for_variable(nullptr, &var, c_one, alloca, nullptr, nullptr, 0) == llvm_gen_be_error_t::invalid_argument);
    assert(emit_dbg_value_for_variable(be_state->dbg_state, nullptr, c_one, alloca, nullptr, nullptr, 0) == llvm_gen_be_error_t::invalid_argument);
    assert(emit_dbg_value_for_variable(be_state->dbg_state, &var, nullptr, alloca, nullptr, nullptr, 0) == llvm_gen_be_error_t::invalid_argument);
    assert(emit_dbg_value_for_variable(be_state->dbg_state, &var, c_one, nullptr, nullptr, nullptr, 0) == llvm_gen_be_error_t::invalid_argument);
    assert(emit_dbg_value_for_variable(be_state->dbg_state, &bad_var, c_one, alloca, nullptr, nullptr, 0) == llvm_gen_be_error_t::invalid_argument);

    assert(emit_dbg_assign_for_variable(nullptr, &var, c_one, alloca, alloca) == llvm_gen_be_error_t::invalid_argument);
    assert(emit_dbg_assign_for_variable(be_state->dbg_state, nullptr, c_one, alloca, alloca) == llvm_gen_be_error_t::invalid_argument);
    assert(emit_dbg_assign_for_variable(be_state->dbg_state, &var, nullptr, alloca, alloca) == llvm_gen_be_error_t::invalid_argument);
    assert(emit_dbg_assign_for_variable(be_state->dbg_state, &var, c_one, nullptr, alloca) == llvm_gen_be_error_t::invalid_argument);
    assert(emit_dbg_assign_for_variable(be_state->dbg_state, &var, c_one, alloca, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(emit_dbg_assign_for_variable(be_state->dbg_state, &bad_var, c_one, alloca, alloca) == llvm_gen_be_error_t::invalid_argument);

    // create_di_subprogram and push_lexical_block with seq != 0 across seq modes 0, 1, 2, static storage, "main", and null name
    assert(create_di_subprogram(nullptr, &r, f, &sp) == llvm_gen_be_error_t::invalid_argument);
    assert(create_di_subprogram(be_state->dbg_state, nullptr, f, &sp) == llvm_gen_be_error_t::invalid_argument);
    assert(create_di_subprogram(be_state->dbg_state, &r, nullptr, &sp) == llvm_gen_be_error_t::invalid_argument);
    assert(create_di_subprogram(be_state->dbg_state, &r, f, nullptr) == llvm_gen_be_error_t::invalid_argument);

    a_routine r_main = {};
    r_main.source_corresp.name = (char*)"main";
    r_main.source_corresp.decl_position.seq = 1;
    r_main.storage_class = sc_static;
    r_main.type = &rout_ast_ty; // routine->type is tk_routine -> DISubroutineType!
    g_seq_mode = 0;
    assert(create_di_subprogram(be_state->dbg_state, &r_main, f, &sp) == llvm_gen_be_error_t::ok);
    assert(push_lexical_block(be_state->dbg_state, inst_pos, &block) == llvm_gen_be_error_t::ok);
    g_seq_mode = 1;
    r_main.source_corresp.name = nullptr;
    r_main.type = &ty; // routine->type is non-routine -> dyn_cast_or_null returns nullptr -> fallback!
    assert(create_di_subprogram(be_state->dbg_state, &r_main, f, &sp) == llvm_gen_be_error_t::ok);
    r_main.type = nullptr;
    assert(push_lexical_block(be_state->dbg_state, inst_pos, &block) == llvm_gen_be_error_t::ok);
    g_seq_mode = 2;
    assert(create_di_subprogram(be_state->dbg_state, &r_main, f, &sp) == llvm_gen_be_error_t::ok);
    assert(push_lexical_block(be_state->dbg_state, inst_pos, &block) == llvm_gen_be_error_t::ok);
    g_seq_mode = 0;

    // get_or_create_di_type: null checks, typeref, bool/signed/unsigned, scoped enum, routine void/no-extra, float, pointer, 2D array, struct/union, non-bitfield member, non-virtual base, default
    llvm::DIType* dt_out = nullptr;
    assert(get_or_create_di_type(nullptr, &ty, &dt_out) == llvm_gen_be_error_t::invalid_argument);
    assert(get_or_create_di_type(be_state->dbg_state, nullptr, &dt_out) == llvm_gen_be_error_t::invalid_argument);
    assert(get_or_create_di_type(be_state->dbg_state, &ty, nullptr) == llvm_gen_be_error_t::invalid_argument);

    a_type tref_ty = {};
    tref_ty.kind = tk_typeref;
    tref_ty.variant.typeref.type = &ty;
    assert(get_or_create_di_type(be_state->dbg_state, &tref_ty, &dt_out) == llvm_gen_be_error_t::ok);

    a_type named_tref_ty = {};
    named_tref_ty.kind = tk_typeref;
    named_tref_ty.source_corresp.name = (char*)"MyIntAlias";
    named_tref_ty.variant.typeref.type = &ty;
    assert(get_or_create_di_type(be_state->dbg_state, &named_tref_ty, &dt_out) == llvm_gen_be_error_t::ok);
    assert(dt_out->getTag() == llvm::dwarf::DW_TAG_typedef);

    // Qualified typeref (const, volatile, restrict, atomic)
    a_type cvra_tref_ty = {};
    cvra_tref_ty.kind = tk_typeref;
    cvra_tref_ty.variant.typeref.qualifiers = TQ_CONST | TQ_VOLATILE | TQ_RESTRICT | TQ_C11_ATOMIC;
    cvra_tref_ty.variant.typeref.type = &ty;
    assert(get_or_create_di_type(be_state->dbg_state, &cvra_tref_ty, &dt_out) == llvm_gen_be_error_t::ok);

    // nullptr_t type
    a_type nullptr_ast_ty = {};
    nullptr_ast_ty.kind = tk_nullptr;
    assert(get_or_create_di_type(be_state->dbg_state, &nullptr_ast_ty, &dt_out) == llvm_gen_be_error_t::ok);

    // Bool integer type and signed/unsigned char & UTF integer types
    a_type bool_int_ty = {};
    bool_int_ty.kind = tk_integer;
    bool_int_ty.size = 1;
    bool_int_ty.alignment = 1;
    g_is_bool = TRUE;
    assert(get_or_create_di_type(be_state->dbg_state, &bool_int_ty, &dt_out) == llvm_gen_be_error_t::ok);
    g_is_bool = FALSE;

    a_type char_ty = {}, schar_ty = {}, uchar_ty = {}, utf8_ty = {}, utf16_ty = {}, utf32_ty = {};
    char_ty.kind = tk_integer;
    char_ty.size = 1;
    char_ty.alignment = 1;
    char_ty.variant.integer.int_kind = ik_char;
    char_ty.source_corresp.name = (char*)"char";
    assert(get_or_create_di_type(be_state->dbg_state, &char_ty, &dt_out) == llvm_gen_be_error_t::ok);

    schar_ty.kind = tk_integer;
    schar_ty.size = 1;
    schar_ty.alignment = 1;
    schar_ty.variant.integer.int_kind = ik_signed_char;
    assert(get_or_create_di_type(be_state->dbg_state, &schar_ty, &dt_out) == llvm_gen_be_error_t::ok);

    uchar_ty.kind = tk_integer;
    uchar_ty.size = 1;
    uchar_ty.alignment = 1;
    uchar_ty.variant.integer.int_kind = ik_unsigned_char;
    assert(get_or_create_di_type(be_state->dbg_state, &uchar_ty, &dt_out) == llvm_gen_be_error_t::ok);

    utf8_ty.kind = tk_integer;
    utf8_ty.size = 1;
    utf8_ty.alignment = 1;
    utf8_ty.variant.integer.int_kind = ik_unsigned_short;
    utf8_ty.variant.integer.char8_t_type = TRUE;
    assert(get_or_create_di_type(be_state->dbg_state, &utf8_ty, &dt_out) == llvm_gen_be_error_t::ok);

    utf16_ty.kind = tk_integer;
    utf16_ty.size = 2;
    utf16_ty.alignment = 2;
    utf16_ty.variant.integer.int_kind = ik_unsigned_short;
    utf16_ty.variant.integer.char16_t_type = TRUE;
    assert(get_or_create_di_type(be_state->dbg_state, &utf16_ty, &dt_out) == llvm_gen_be_error_t::ok);

    utf32_ty.kind = tk_integer;
    utf32_ty.size = 4;
    utf32_ty.alignment = 4;
    utf32_ty.variant.integer.int_kind = ik_unsigned_int;
    utf32_ty.variant.integer.char32_t_type = TRUE;
    assert(get_or_create_di_type(be_state->dbg_state, &utf32_ty, &dt_out) == llvm_gen_be_error_t::ok);

    a_type uint_plain_ty = {};
    uint_plain_ty.kind = tk_integer;
    uint_plain_ty.size = 4;
    uint_plain_ty.alignment = 4;
    uint_plain_ty.variant.integer.int_kind = ik_unsigned_int;
    assert(get_or_create_di_type(be_state->dbg_state, &uint_plain_ty, &dt_out) == llvm_gen_be_error_t::ok);

    int_kind_is_signed[ik_int] = TRUE;
    a_type signed_enum_ty = {};
    signed_enum_ty.kind = tk_integer;
    signed_enum_ty.variant.integer.int_kind = ik_int;
    signed_enum_ty.variant.integer.enum_type = TRUE;
    signed_enum_ty.variant.integer.is_scoped_enum = TRUE;
    a_scope enum_scope = {};
    a_constant anon_enum_c = {};
    anon_enum_c.source_corresp.name = nullptr;
    assert(llvm_set_edg_sint64(&anon_enum_c.variant.integer_value, -42) == llvm_gen_be_error_t::ok);
    enum_scope.constants = &anon_enum_c;
    signed_enum_ty.variant.integer.enum_info.assoc_scope = &enum_scope;
    assert(get_or_create_di_type(be_state->dbg_state, &signed_enum_ty, &dt_out) == llvm_gen_be_error_t::ok);

    // Scoped enum with null assoc_scope
    a_type scoped_null_ty = {};
    scoped_null_ty.kind = tk_integer;
    scoped_null_ty.variant.integer.enum_type = TRUE;
    scoped_null_ty.variant.integer.is_scoped_enum = TRUE;
    scoped_null_ty.variant.integer.enum_info.assoc_scope = nullptr;
    assert(get_or_create_di_type(be_state->dbg_state, &scoped_null_ty, &dt_out) == llvm_gen_be_error_t::ok);

    // Routine with void return and null extra_info, and routine with null return_type and null param type
    a_type rout_void_ty = {};
    rout_void_ty.kind = tk_routine;
    rout_void_ty.variant.routine.return_type = &void_ast_ty;
    rout_void_ty.variant.routine.extra_info = nullptr;
    assert(get_or_create_di_type(be_state->dbg_state, &rout_void_ty, &dt_out) == llvm_gen_be_error_t::ok);

    a_type rout_nullret_ty = {};
    rout_nullret_ty.kind = tk_routine;
    rout_nullret_ty.variant.routine.return_type = nullptr;
    a_param_type null_pt = {};
    null_pt.type = nullptr;
    a_routine_type_supplement null_pt_supp = {};
    null_pt_supp.param_type_list = &null_pt;
    rout_nullret_ty.variant.routine.extra_info = &null_pt_supp;
    assert(get_or_create_di_type(be_state->dbg_state, &rout_nullret_ty, &dt_out) == llvm_gen_be_error_t::ok);

    // Float, complex, fixed-point, pointer, lvalue/rvalue reference, ptr-to-member, 2D array, struct, union, and unknown type kind
    a_type fp_ast_ty = {};
    fp_ast_ty.kind = tk_float;
    fp_ast_ty.size = 4;
    assert(get_or_create_di_type(be_state->dbg_state, &fp_ast_ty, &dt_out) == llvm_gen_be_error_t::ok);

#if C99_IL_EXTENSIONS_SUPPORTED
    a_type cplx_ast_ty = {}, cplx_zero_ty = {};
    cplx_ast_ty.kind = tk_complex;
    cplx_ast_ty.size = 8;
    assert(get_or_create_di_type(be_state->dbg_state, &cplx_ast_ty, &dt_out) == llvm_gen_be_error_t::ok);
    cplx_zero_ty.kind = tk_complex;
    cplx_zero_ty.size = 0;
    assert(get_or_create_di_type(be_state->dbg_state, &cplx_zero_ty, &dt_out) == llvm_gen_be_error_t::ok);
#endif
#if FIXED_POINT_ALLOWED
    a_type fx_s_ty = {}, fx_u_ty = {};
    fx_s_ty.kind = tk_fixed_point;
    fx_s_ty.size = 4;
    fx_s_ty.variant.fixed_point.is_unsigned = FALSE;
    assert(get_or_create_di_type(be_state->dbg_state, &fx_s_ty, &dt_out) == llvm_gen_be_error_t::ok);
    fx_u_ty.kind = tk_fixed_point;
    fx_u_ty.size = 0;
    fx_u_ty.variant.fixed_point.is_unsigned = TRUE;
    assert(get_or_create_di_type(be_state->dbg_state, &fx_u_ty, &dt_out) == llvm_gen_be_error_t::ok);
#endif

    a_type ptr_ast_ty = {}, lref_ast_ty = {}, rref_ast_ty = {}, p2m_ast_ty = {}, p2m_cls_ty = {};
    ptr_ast_ty.kind = tk_pointer;
    ptr_ast_ty.variant.pointer.type = &fp_ast_ty;
    assert(get_or_create_di_type(be_state->dbg_state, &ptr_ast_ty, &dt_out) == llvm_gen_be_error_t::ok);
    lref_ast_ty.kind = tk_pointer;
    lref_ast_ty.variant.pointer.type = &fp_ast_ty;
    lref_ast_ty.variant.pointer.is_reference = TRUE;
    lref_ast_ty.variant.pointer.is_rvalue_reference = FALSE;
    assert(get_or_create_di_type(be_state->dbg_state, &lref_ast_ty, &dt_out) == llvm_gen_be_error_t::ok);
    rref_ast_ty.kind = tk_pointer;
    rref_ast_ty.variant.pointer.type = &fp_ast_ty;
    rref_ast_ty.variant.pointer.is_reference = TRUE;
    rref_ast_ty.variant.pointer.is_rvalue_reference = TRUE;
    assert(get_or_create_di_type(be_state->dbg_state, &rref_ast_ty, &dt_out) == llvm_gen_be_error_t::ok);

    p2m_ast_ty.kind = tk_ptr_to_member;
    p2m_ast_ty.variant.ptr_to_member.type = &fp_ast_ty;
    p2m_ast_ty.variant.ptr_to_member.class_of_which_a_member = nullptr;
    assert(get_or_create_di_type(be_state->dbg_state, &p2m_ast_ty, &dt_out) == llvm_gen_be_error_t::ok);
    p2m_cls_ty.kind = tk_ptr_to_member;
    p2m_cls_ty.variant.ptr_to_member.type = &fp_ast_ty;
    p2m_cls_ty.variant.ptr_to_member.class_of_which_a_member = &ty;
    assert(get_or_create_di_type(be_state->dbg_state, &p2m_cls_ty, &dt_out) == llvm_gen_be_error_t::ok);

    a_type arr1_ty = {}, arr2_ty = {};
    arr1_ty.kind = tk_array;
    arr1_ty.size = 16;
    arr1_ty.variant.array.element_type = &fp_ast_ty;
    arr1_ty.variant.array.variant.number_of_elements = 4;
    arr2_ty.kind = tk_array;
    arr2_ty.size = 32;
    arr2_ty.variant.array.element_type = &arr1_ty;
    arr2_ty.variant.array.variant.number_of_elements = 2;
    assert(get_or_create_di_type(be_state->dbg_state, &arr2_ty, &dt_out) == llvm_gen_be_error_t::ok);

    a_type st_ast_ty = {}, un_ast_ty = {};
    st_ast_ty.kind = tk_struct;
    st_ast_ty.size = 8;
    st_ast_ty.alignment = 4;
    st_ast_ty.source_corresp.name = (char*)"MyStruct";
    a_field norm_f1 = {}, norm_f2 = {}, bf_named = {};
    norm_f1.is_bit_field = FALSE;
    norm_f1.source_corresp.name = (char*)"f1";
    norm_f1.type = &fp_ast_ty;
    norm_f1.next = &norm_f2;
    norm_f2.is_bit_field = FALSE;
    norm_f2.source_corresp.name = nullptr;
    norm_f2.type = &fp_ast_ty;
    norm_f2.offset = 4;
    norm_f2.next = &bf_named;
    bf_named.is_bit_field = TRUE;
    bf_named.source_corresp.name = (char*)"bf";
    bf_named.bit_size = 2;
    bf_named.offset = 4;
    bf_named.type = &ty;
    st_ast_ty.variant.class_struct_union.field_list = &norm_f1;
    a_base_class nonvirt_base = {}, b_pub = {}, b_prot = {}, b_priv = {}, b_other_acc = {}, b_has_pub = {};
    a_base_class_derivation deriv_pub = {}, deriv_prot = {}, deriv_priv = {}, deriv_none = {};
    deriv_pub.access = as_public;
    deriv_prot.access = as_protected;
    deriv_priv.access = as_private;
    deriv_none.access = as_inaccessible;
    nonvirt_base.type = &ty;
    nonvirt_base.is_virtual = FALSE;
    nonvirt_base.next_direct = &b_pub;
    b_pub.type = &ty;
    b_pub.is_virtual = TRUE;
    b_pub.derivation = &deriv_pub;
    b_pub.next_direct = &b_prot;
    b_prot.type = &ty;
    b_prot.derivation = &deriv_prot;
    b_prot.next_direct = &b_priv;
    b_priv.type = &ty;
    b_priv.derivation = &deriv_priv;
    b_priv.next_direct = &b_other_acc;
    b_other_acc.type = &ty;
    b_other_acc.derivation = &deriv_none;
    b_other_acc.next_direct = &b_has_pub;
    b_has_pub.type = &ty;
    b_has_pub.has_public_derivation = TRUE;

    a_routine m_virt_pure = {}, m_virt_norm = {}, m_nonvirt_noty = {};
    m_virt_pure.source_corresp.name = "pure_vfunc";
    m_virt_pure.type = &rout_void_ty;
    m_virt_pure.is_virtual = TRUE;
    m_virt_pure.pure_virtual = TRUE;
    m_virt_pure.number.virtual_function = 1;
    m_virt_pure.next = &m_virt_norm;
    m_virt_norm.source_corresp.name = nullptr;
    m_virt_norm.type = &ty;
    m_virt_norm.is_virtual = TRUE;
    m_virt_norm.pure_virtual = FALSE;
    m_virt_norm.number.virtual_function = 0;
    m_virt_norm.next = &m_nonvirt_noty;
    m_nonvirt_noty.source_corresp.name = "regular_m";
    m_nonvirt_noty.type = nullptr;
    m_nonvirt_noty.is_virtual = FALSE;

    a_scope cls_m_scope = {};
    cls_m_scope.routines = &m_virt_pure;

    a_template_arg ta_ty = {}, ta_ty_named = {}, ta_ty_null = {}, ta_val = {}, ta_val_noty = {}, ta_val_null = {}, ta_tpl = {}, ta_other = {}, ta_unk = {};
    a_constant tc_val = {}, tc_noty = {};
    tc_val.type = &ty;
    tc_val.source_corresp.name = (char*)"N";
    assert(llvm_set_edg_sint64(&tc_val.variant.integer_value, 7) == llvm_gen_be_error_t::ok);
    tc_noty.type = nullptr;
    tc_noty.source_corresp.name = nullptr;
    assert(llvm_set_edg_sint64(&tc_noty.variant.integer_value, 9) == llvm_gen_be_error_t::ok);
    ta_ty.kind = tak_type;
    ta_ty.variant.type = &ty; // ty has name == nullptr -> "T"
    ta_ty.next = &ta_ty_named;
    ta_ty_named.kind = tak_type;
    ta_ty_named.variant.type = &char_ty; // char_ty has name == "char"
    ta_ty_named.next = &ta_ty_null;
    ta_ty_null.kind = tak_type;
    ta_ty_null.variant.type = nullptr;
    ta_ty_null.next = &ta_val;
    ta_val.kind = tak_nontype;
    ta_val.variant.constant = &tc_val; // tc_val has name == "N"
    ta_val.next = &ta_val_noty;
    ta_val_noty.kind = tak_nontype;
    ta_val_noty.variant.constant = &tc_noty; // tc_noty has name == nullptr -> "V"
    ta_val_noty.next = &ta_val_null;
    ta_val_null.kind = tak_nontype;
    ta_val_null.variant.constant = nullptr;
    ta_val_null.next = &ta_tpl;
    ta_tpl.kind = tak_template;
    ta_tpl.next = &ta_other;
    ta_other.kind = tak_start_of_pack_expansion;
    ta_other.next = &ta_unk;
    ta_unk.kind = static_cast<a_templ_arg_kind>(99);
    ta_unk.next = nullptr;

    a_class_type_supplement st_supp = {};
    st_supp.direct_base_classes = &nonvirt_base;
    st_supp.assoc_scope = &cls_m_scope;
    st_supp.template_arg_list = &ta_ty;
    st_ast_ty.variant.class_struct_union.extra_info = &st_supp;
    assert(get_or_create_di_type(be_state->dbg_state, &st_ast_ty, &dt_out) == llvm_gen_be_error_t::ok);

    llvm::DIImportedEntity* imp_decl = nullptr;
    llvm::DIImportedEntity* imp_mod = nullptr;
    assert(createImportedDeclaration(nullptr, ns, base_di_ty, nullptr, 1, "Alias", &imp_decl) == llvm_gen_be_error_t::invalid_argument);
    assert(createImportedDeclaration(be_state->dbg_state, nullptr, base_di_ty, nullptr, 1, "Alias", &imp_decl) == llvm_gen_be_error_t::invalid_argument);
    assert(createImportedDeclaration(be_state->dbg_state, ns, nullptr, nullptr, 1, "Alias", &imp_decl) == llvm_gen_be_error_t::invalid_argument);
    assert(createImportedDeclaration(be_state->dbg_state, ns, base_di_ty, nullptr, 1, "Alias", nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(createImportedDeclaration(be_state->dbg_state, ns, base_di_ty, be_state->dbg_state->compile_unit->getFile(), 1, "Alias", &imp_decl) == llvm_gen_be_error_t::ok);
    assert(createImportedDeclaration(be_state->dbg_state, ns, base_di_ty, be_state->dbg_state->compile_unit->getFile(), 1, nullptr, &imp_decl) == llvm_gen_be_error_t::ok);

    assert(createImportedModule(nullptr, sp, ns, nullptr, 1, &imp_mod) == llvm_gen_be_error_t::invalid_argument);
    assert(createImportedModule(be_state->dbg_state, nullptr, ns, nullptr, 1, &imp_mod) == llvm_gen_be_error_t::invalid_argument);
    assert(createImportedModule(be_state->dbg_state, sp, nullptr, nullptr, 1, &imp_mod) == llvm_gen_be_error_t::invalid_argument);
    assert(createImportedModule(be_state->dbg_state, sp, ns, nullptr, 1, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(createImportedModule(be_state->dbg_state, sp, ns, be_state->dbg_state->compile_unit->getFile(), 1, &imp_mod) == llvm_gen_be_error_t::ok);

    un_ast_ty.kind = tk_union;
    un_ast_ty.size = 4;
    un_ast_ty.alignment = 4;
    un_ast_ty.variant.class_struct_union.extra_info = nullptr;
    assert(get_or_create_di_type(be_state->dbg_state, &un_ast_ty, &dt_out) == llvm_gen_be_error_t::ok);

    a_type named_un_ty = {};
    named_un_ty.kind = tk_union;
    named_un_ty.size = 4;
    named_un_ty.alignment = 4;
    named_un_ty.source_corresp.name = (char*)"MyUnion";
    named_un_ty.variant.class_struct_union.field_list = &norm_f1;
    assert(get_or_create_di_type(be_state->dbg_state, &named_un_ty, &dt_out) == llvm_gen_be_error_t::ok);
    assert(dt_out->getTag() == llvm::dwarf::DW_TAG_union_type);

    a_type unk_ast_ty = {};
    unk_ast_ty.kind = static_cast<a_type_kind>(999);
    unk_ast_ty.size = 1;
    assert(get_or_create_di_type(be_state->dbg_state, &unk_ast_ty, &dt_out) == llvm_gen_be_error_t::ok);

    // Null checks on createInheritance, createMethod, createNameSpace, createTemplateTypeParameter, createTemplateValueParameter, createEnumerationType, build_dwarf_variable_fragment
    assert(createInheritance(nullptr, derived_di_ty, base_di_ty, 0, 0, llvm::DINode::FlagZero, &inheritance) == llvm_gen_be_error_t::invalid_argument);
    assert(createInheritance(be_state->dbg_state, nullptr, base_di_ty, 0, 0, llvm::DINode::FlagZero, &inheritance) == llvm_gen_be_error_t::invalid_argument);
    assert(createInheritance(be_state->dbg_state, derived_di_ty, nullptr, 0, 0, llvm::DINode::FlagZero, &inheritance) == llvm_gen_be_error_t::invalid_argument);
    assert(createInheritance(be_state->dbg_state, derived_di_ty, base_di_ty, 0, 0, llvm::DINode::FlagZero, nullptr) == llvm_gen_be_error_t::invalid_argument);

    assert(createMethod(nullptr, sp, "m", nullptr, nullptr, 1, method_ty, 0, 0, nullptr, llvm::DINode::FlagZero, llvm::DISubprogram::SPFlagZero, &method) == llvm_gen_be_error_t::invalid_argument);
    assert(createMethod(be_state->dbg_state, nullptr, "m", nullptr, nullptr, 1, method_ty, 0, 0, nullptr, llvm::DINode::FlagZero, llvm::DISubprogram::SPFlagZero, &method) == llvm_gen_be_error_t::invalid_argument);
    assert(createMethod(be_state->dbg_state, sp, nullptr, nullptr, nullptr, 1, method_ty, 0, 0, nullptr, llvm::DINode::FlagZero, llvm::DISubprogram::SPFlagZero, &method) == llvm_gen_be_error_t::invalid_argument);
    assert(createMethod(be_state->dbg_state, sp, "m", nullptr, nullptr, 1, nullptr, 0, 0, nullptr, llvm::DINode::FlagZero, llvm::DISubprogram::SPFlagZero, &method) == llvm_gen_be_error_t::invalid_argument);
    assert(createMethod(be_state->dbg_state, sp, "m", nullptr, nullptr, 1, method_ty, 0, 0, nullptr, llvm::DINode::FlagZero, llvm::DISubprogram::SPFlagZero, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(createMethod(be_state->dbg_state, sp, "m", nullptr, be_state->dbg_state->compile_unit->getFile(), 1, method_ty, 0, 0, nullptr, llvm::DINode::FlagZero, llvm::DISubprogram::SPFlagZero, &method) == llvm_gen_be_error_t::ok);

    assert(createNameSpace(nullptr, sp, "n", false, &ns) == llvm_gen_be_error_t::invalid_argument);
    assert(createNameSpace(be_state->dbg_state, nullptr, "n", false, &ns) == llvm_gen_be_error_t::invalid_argument);
    assert(createNameSpace(be_state->dbg_state, sp, nullptr, false, &ns) == llvm_gen_be_error_t::invalid_argument);
    assert(createNameSpace(be_state->dbg_state, sp, "n", false, nullptr) == llvm_gen_be_error_t::invalid_argument);

    assert(createTemplateTypeParameter(nullptr, ns, "T", base_di_ty, false, &ttp) == llvm_gen_be_error_t::invalid_argument);
    assert(createTemplateTypeParameter(be_state->dbg_state, nullptr, "T", base_di_ty, false, &ttp) == llvm_gen_be_error_t::invalid_argument);
    assert(createTemplateTypeParameter(be_state->dbg_state, ns, nullptr, base_di_ty, false, &ttp) == llvm_gen_be_error_t::invalid_argument);
    assert(createTemplateTypeParameter(be_state->dbg_state, ns, "T", nullptr, false, &ttp) == llvm_gen_be_error_t::invalid_argument);
    assert(createTemplateTypeParameter(be_state->dbg_state, ns, "T", base_di_ty, false, nullptr) == llvm_gen_be_error_t::invalid_argument);

    assert(createTemplateValueParameter(nullptr, ns, "V", base_di_ty, false, tv_val, &tvp) == llvm_gen_be_error_t::invalid_argument);
    assert(createTemplateValueParameter(be_state->dbg_state, nullptr, "V", base_di_ty, false, tv_val, &tvp) == llvm_gen_be_error_t::invalid_argument);
    assert(createTemplateValueParameter(be_state->dbg_state, ns, nullptr, base_di_ty, false, tv_val, &tvp) == llvm_gen_be_error_t::invalid_argument);
    assert(createTemplateValueParameter(be_state->dbg_state, ns, "V", nullptr, false, tv_val, &tvp) == llvm_gen_be_error_t::invalid_argument);
    assert(createTemplateValueParameter(be_state->dbg_state, ns, "V", base_di_ty, false, nullptr, &tvp) == llvm_gen_be_error_t::invalid_argument);
    assert(createTemplateValueParameter(be_state->dbg_state, ns, "V", base_di_ty, false, tv_val, nullptr) == llvm_gen_be_error_t::invalid_argument);

    assert(createEnumerationType(nullptr, ns, "E", nullptr, 1, 32, 32, llvm::DINodeArray(), base_di_ty, nullptr, false, &enum_ty) == llvm_gen_be_error_t::invalid_argument);
    assert(createEnumerationType(be_state->dbg_state, nullptr, "E", nullptr, 1, 32, 32, llvm::DINodeArray(), base_di_ty, nullptr, false, &enum_ty) == llvm_gen_be_error_t::invalid_argument);
    assert(createEnumerationType(be_state->dbg_state, ns, nullptr, nullptr, 1, 32, 32, llvm::DINodeArray(), base_di_ty, nullptr, false, &enum_ty) == llvm_gen_be_error_t::invalid_argument);
    assert(createEnumerationType(be_state->dbg_state, ns, "E", nullptr, 1, 32, 32, llvm::DINodeArray(), nullptr, nullptr, false, &enum_ty) == llvm_gen_be_error_t::invalid_argument);
    assert(createEnumerationType(be_state->dbg_state, ns, "E", nullptr, 1, 32, 32, llvm::DINodeArray(), base_di_ty, nullptr, false, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(createEnumerationType(be_state->dbg_state, ns, "E", be_state->dbg_state->compile_unit->getFile(), 1, 32, 32, llvm::DINodeArray(), base_di_ty, nullptr, false, &enum_ty) == llvm_gen_be_error_t::ok);

    // build_dwarf_variable_fragment with null be_state and null dbg_state
    LLVMBackendState* saved_be = be_state;
    be_state = nullptr;
    assert(build_dwarf_variable_fragment(&ty, 0, 32, &frag_expr) == llvm_gen_be_error_t::di_metadata_failure);
    assert(build_pdb_symbol_records(&func_scope, &pdb_node) == llvm_gen_be_error_t::di_metadata_failure);
    be_state = saved_be;
    be_state->dbg_state = nullptr;
    assert(build_dwarf_variable_fragment(&ty, 0, 32, &frag_expr) == llvm_gen_be_error_t::di_metadata_failure);
    be_state->dbg_state = saved;

    // build_pdb_symbol_records with empty scope_stack, static function with null name, namespace scope, file scope, and unnamed/untyped variables
    be_state->dbg_state->scope_stack.clear();
    a_variable v_unnamed = {}, v_untyped = {};
    v_unnamed.type = &ty;
    v_unnamed.source_corresp.name = nullptr;
    v_unnamed.next = &v_untyped;
    v_untyped.type = nullptr;

    func_scope.variant.routine.ptr = &r_main; // r_main has name == nullptr and storage_class == sc_static
    v_unnamed.source_corresp.decl_position.seq = 1;
    func_scope.variables = &v_unnamed;
    func_scope.nonstatic_variables = &v_unnamed;
    assert(build_pdb_symbol_records(&func_scope, &pdb_node) == llvm_gen_be_error_t::ok);
    v_unnamed.source_corresp.decl_position.seq = 0;

    // Phase 8 tests: emit_di_macro_info and emit_scope_imported_entities
    llvm::DIMacro* mac_out = nullptr;
    assert(emit_di_macro_info(nullptr, 1, "FOO", "1", false, &mac_out) == llvm_gen_be_error_t::invalid_argument);
    assert(emit_di_macro_info(&empty_dbg, 1, "FOO", "1", false, &mac_out) == llvm_gen_be_error_t::invalid_argument);
    assert(emit_di_macro_info(be_state->dbg_state, 1, nullptr, "1", false, &mac_out) == llvm_gen_be_error_t::invalid_argument);
    assert(emit_di_macro_info(be_state->dbg_state, 1, "FOO", "1", false, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(emit_di_macro_info(be_state->dbg_state, 10, "FOO", "1", false, &mac_out) == llvm_gen_be_error_t::ok);
    assert(emit_di_macro_info(be_state->dbg_state, 11, "FOO", nullptr, true, &mac_out) == llvm_gen_be_error_t::ok);

    unsigned imp_cnt = 0;
    assert(emit_scope_imported_entities(nullptr, &func_scope, &imp_cnt) == llvm_gen_be_error_t::invalid_argument);
    assert(emit_scope_imported_entities(be_state->dbg_state, nullptr, &imp_cnt) == llvm_gen_be_error_t::invalid_argument);
    assert(emit_scope_imported_entities(be_state->dbg_state, &func_scope, nullptr) == llvm_gen_be_error_t::invalid_argument);
    a_namespace imp_ns1 = {}, imp_ns2 = {};
    imp_ns1.source_corresp.name = "std";
    imp_ns1.next = &imp_ns2;
    imp_ns2.source_corresp.name = nullptr;
    func_scope.namespaces = &imp_ns1;
    assert(emit_scope_imported_entities(be_state->dbg_state, &func_scope, &imp_cnt) == llvm_gen_be_error_t::ok);
    assert(imp_cnt == 2);
    func_scope.namespaces = nullptr;

    a_scope func_no_r = {};
    func_no_r.kind = sck_function;
    func_no_r.variant.routine.ptr = nullptr;
    assert(build_pdb_symbol_records(&func_no_r, &pdb_node) == llvm_gen_be_error_t::ok);

    a_namespace ns_sym = {};
    ns_sym.source_corresp.name = (char*)"pdb_ns";
    a_scope ns_scope_pdb = {};
    ns_scope_pdb.kind = sck_namespace;
    ns_scope_pdb.variant.assoc_namespace = &ns_sym;
    assert(build_pdb_symbol_records(&ns_scope_pdb, &pdb_node) == llvm_gen_be_error_t::ok);
    ns_sym.source_corresp.name = nullptr;
    assert(build_pdb_symbol_records(&ns_scope_pdb, &pdb_node) == llvm_gen_be_error_t::ok);
    ns_scope_pdb.variant.assoc_namespace = nullptr;
    assert(build_pdb_symbol_records(&ns_scope_pdb, &pdb_node) == llvm_gen_be_error_t::ok);

    // Phase 6: emit_debug_common_block and create_inlined_at_debug_location
    llvm::DICommonBlock* common_blk = nullptr;
    assert(emit_debug_common_block(nullptr, sp, nullptr, "COM1", nullptr, 5, &common_blk) == llvm_gen_be_error_t::invalid_argument);
    assert(emit_debug_common_block(&empty_dbg, sp, nullptr, "COM1", nullptr, 5, &common_blk) == llvm_gen_be_error_t::invalid_argument);
    assert(emit_debug_common_block(be_state->dbg_state, sp, nullptr, nullptr, nullptr, 5, &common_blk) == llvm_gen_be_error_t::invalid_argument);
    assert(emit_debug_common_block(be_state->dbg_state, sp, nullptr, "COM1", nullptr, 5, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(emit_debug_common_block(be_state->dbg_state, sp, nullptr, "COM1", be_state->dbg_state->compile_unit->getFile(), 5, &common_blk) == llvm_gen_be_error_t::ok);
    assert(common_blk != nullptr);
    assert(emit_debug_common_block(be_state->dbg_state, nullptr, nullptr, "COM2", nullptr, 6, &common_blk) == llvm_gen_be_error_t::ok);

    llvm::DILocation* inlined_loc = nullptr;
    assert(create_inlined_at_debug_location(nullptr, 20, 4, sp, loc, &inlined_loc) == llvm_gen_be_error_t::invalid_argument);
    assert(create_inlined_at_debug_location(be_state->dbg_state, 20, 4, nullptr, loc, &inlined_loc) == llvm_gen_be_error_t::invalid_argument);
    assert(create_inlined_at_debug_location(be_state->dbg_state, 20, 4, sp, nullptr, &inlined_loc) == llvm_gen_be_error_t::invalid_argument);
    assert(create_inlined_at_debug_location(be_state->dbg_state, 20, 4, sp, loc, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(create_inlined_at_debug_location(be_state->dbg_state, 20, 4, sp, loc, &inlined_loc) == llvm_gen_be_error_t::ok);
    assert(inlined_loc != nullptr && inlined_loc->getInlinedAt() == loc);

    // Deterministic OOM countdown sweeps across debug_scopes, debug_types, and debug.cpp
    be_state->dbg_state->scope_stack.push_back(sp);
    g_llvm_be_oom_countdown = 0;
    assert(apply_instruction_debug_loc(alloca, inst_pos) == llvm_gen_be_error_t::out_of_memory);
    g_llvm_be_oom_countdown = -1;

    g_seq_mode = 0;
    r_main.type = &rout_ast_ty;
    for (int b = 0; b < 8; ++b) {
      g_llvm_be_oom_countdown = b;
      llvm_gen_be_error_t rs = create_di_subprogram(be_state->dbg_state, &r_main, f, &sp);
      g_llvm_be_oom_countdown = -1;
      if (rs == llvm_gen_be_error_t::ok) break;
    }
    g_llvm_be_oom_countdown = 0;
    assert(push_lexical_block(be_state->dbg_state, inst_pos, &block) == llvm_gen_be_error_t::out_of_memory);
    g_llvm_be_oom_countdown = -1;

    func_scope.variables = &v_unnamed;
    func_scope.nonstatic_variables = &v_unnamed;
    for (int b = 0; b < 6; ++b) {
      g_llvm_be_oom_countdown = b;
      llvm_gen_be_error_t rpdb = build_pdb_symbol_records(&func_scope, &pdb_node);
      g_llvm_be_oom_countdown = -1;
      if (rpdb == llvm_gen_be_error_t::ok) break;
    }

    func_scope.namespaces = &imp_ns1;
    for (int b = 0; b < 6; ++b) {
      g_llvm_be_oom_countdown = b;
      llvm_gen_be_error_t rimp = emit_scope_imported_entities(be_state->dbg_state, &func_scope, &imp_cnt);
      g_llvm_be_oom_countdown = -1;
      if (rimp == llvm_gen_be_error_t::ok) break;
    }
    func_scope.namespaces = nullptr;

    a_type_ptr di_sweep_tys[] = {&rout_ast_ty, &ptr_ast_ty, &p2m_cls_ty, &arr2_ty, &st_ast_ty};
    for (a_type_ptr dty : di_sweep_tys) {
      for (int b = 0; b < 24; ++b) {
        be_state->dbg_state->di_types_map.clear();
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t rdt = get_or_create_di_type(be_state->dbg_state, dty, &dt_out);
        g_llvm_be_oom_countdown = -1;
        if (rdt == llvm_gen_be_error_t::ok) break;
      }
    }

    param_var.source_corresp.decl_position.seq = 1;
    g_seq_mode = 0;
    llvm::GlobalVariable* dbg_gv = nullptr;
    assert(llvm_create_global_variable(
        state.module.get(), llvm::Type::getInt32Ty(*state.context), false,
        llvm::GlobalValue::ExternalLinkage, nullptr, "dbg_oom_gv", &dbg_gv) == llvm_gen_be_error_t::ok);
    for (int b = 0; b < 4; ++b) {
      g_llvm_be_oom_countdown = b;
      llvm_gen_be_error_t rd1 = emit_dbg_declare_for_variable(be_state->dbg_state, &param_var, alloca);
      g_llvm_be_oom_countdown = -1;
      if (rd1 == llvm_gen_be_error_t::ok) break;
    }
    for (int b = 0; b < 4; ++b) {
      g_llvm_be_oom_countdown = b;
      llvm_gen_be_error_t rd2 = emit_dbg_value_for_variable(be_state->dbg_state, &param_var, c_one, alloca, nullptr, nullptr, 0);
      g_llvm_be_oom_countdown = -1;
      if (rd2 == llvm_gen_be_error_t::ok) break;
    }
    for (int b = 0; b < 4; ++b) {
      g_llvm_be_oom_countdown = b;
      llvm_gen_be_error_t rd3 = emit_dbg_assign_for_variable(be_state->dbg_state, &param_var, c_one, alloca, alloca);
      g_llvm_be_oom_countdown = -1;
      if (rd3 == llvm_gen_be_error_t::ok) break;
    }
    for (int b = 0; b < 4; ++b) {
      g_llvm_be_oom_countdown = b;
      llvm_gen_be_error_t rd4 = emit_dbg_global_variable(be_state->dbg_state, &param_var, dbg_gv);
      g_llvm_be_oom_countdown = -1;
      if (rd4 == llvm_gen_be_error_t::ok) break;
    }
  }

  err = debug_info_finalize(be_state->dbg_state);
  assert(err == llvm_gen_be_error_t::ok);

#if FIXED_POINT_ALLOWED
  {
    llvm::DIType* fx_di = nullptr;
    a_type fx_dbg_s = {}, fx_dbg_u = {};
    fx_dbg_s.kind = tk_fixed_point;
    fx_dbg_s.size = 4;
    fx_dbg_s.variant.fixed_point.is_unsigned = FALSE;
    assert(get_or_create_di_type(be_state->dbg_state, &fx_dbg_s, &fx_di) == llvm_gen_be_error_t::ok);
    fx_dbg_u.kind = tk_fixed_point;
    fx_dbg_u.size = 0;
    fx_dbg_u.variant.fixed_point.is_unsigned = TRUE;
    assert(get_or_create_di_type(be_state->dbg_state, &fx_dbg_u, &fx_di) == llvm_gen_be_error_t::ok);
  }
#endif

  err = debug_info_cleanup(&be_state->dbg_state);
  assert(err == llvm_gen_be_error_t::ok);
  assert(be_state->dbg_state == nullptr);

  // Test debug_info_init OOM branches + Mach-O target triple with split_dwarf_file set
  llvm::Module macho_mod("macho_dbg", *be_state->context);
  EDG_LLVM_SET_TARGET_TRIPLE(&macho_mod, "aarch64-apple-darwin");
  for (int b = 0; b < 6; ++b) {
    g_llvm_be_oom_countdown = b;
    llvm_gen_be_error_t rinit = debug_info_init(&be_state->dbg_state, &macho_mod, "test.cpp", "/tmp", false);
    g_llvm_be_oom_countdown = -1;
    if (rinit == llvm_gen_be_error_t::ok) {
      assert(debug_info_cleanup(&be_state->dbg_state) == llvm_gen_be_error_t::ok);
      break;
    }
  }
  be_state->split_dwarf_file = "macho.dwo";
  assert(debug_info_init(&be_state->dbg_state, &macho_mod, "test.cpp", "/tmp", false) == llvm_gen_be_error_t::ok);
  assert(debug_info_cleanup(&be_state->dbg_state) == llvm_gen_be_error_t::ok);
  be_state->split_dwarf_file.clear();

  printf("PASS\n");
  return 0;
}
