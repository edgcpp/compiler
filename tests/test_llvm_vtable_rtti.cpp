/**
 * @file test_llvm_vtable_rtti.cpp
 * @ingroup llvm_gen_be_tests
 * @brief Unit tests for the LLVM backend vtable and RTTI generation subsystem.
 * @details Exhaustively validates Itanium and MSVC ABI virtual method tables, VTT arrays,
 * construction vtables, adjustment thunks, RTTI descriptors, dynamic_cast, and typeid.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_rtti.h"
#include "llvm_gen_be_vtable.h"
#include "llvm_gen_be_ptrmem.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

using namespace edg;

LLVMBackendState* edg::be_state = nullptr;

namespace edg {
  char* g_mangled_fn_name = nullptr; ///< Configurable return value for `get_mangled_function_name`.
  char* g_mangled_ti_name = nullptr; ///< Configurable override for `mangled_typeinfo_name` (when g_use_custom_ti_name != 0).
  int g_use_custom_ti_name = 0; ///< When non-zero, `mangled_typeinfo_name` returns `g_mangled_ti_name`.
  char* g_mangled_vtbl_override = (char*)"_ZTVmock"; ///< Configurable return value for `mangled_vtbl_name`.
  char* g_mangled_vtt_override = (char*)"_ZTTmock"; ///< Configurable return value for `mangled_virtual_table_table_name`.
  a_boolean g_is_ref_type = FALSE; ///< Configurable return value for `is_reference_type`.
  int g_vr_fail_expr = 0; ///< Countdown of calls before `llvm_lower_expression` fails.
  int g_vr_expr_mode = 0; ///< Controls `llvm_lower_expression` output (0=ptr, 1=struct{ptr,i64}, 2=i64, 3=null, 4=ptr then i64, 5=i64 then ptr).
  int g_vr_fail_type = 0; ///< When non-zero, `get_llvm_type` returns `unsupported_type`.
  a_routine_ptr g_key_func = nullptr; ///< Configurable return value for `vtbl_decider_function_for_class`.

/**
 * @brief Stub implementation of `get_mangled_function_name`.
 * @details Returns `g_mangled_fn_name` for vtable/RTTI unit tests.
 * @param[in] routine The EDG routine pointer.
 * @return `g_mangled_fn_name`.
 */
char* get_mangled_function_name(a_routine_ptr routine) {
    (void)routine;
    return g_mangled_fn_name;
}

  /**
   * @brief Test stub or helper for `is_bool_type`.
   * @details Provides mock or helper behavior for `is_bool_type` within `tests/test_llvm_vtable_rtti.cpp`.
   * @param[in] ty Parameter `ty` for `is_bool_type`.
   * @return Result of `is_bool_type` (a_boolean).
   */
  a_boolean is_bool_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `f_alignment_of_type`.
   * @details Provides mock or helper behavior for `f_alignment_of_type` within `tests/test_llvm_vtable_rtti.cpp`.
   * @param[in] ty Parameter `ty` for `f_alignment_of_type`.
   * @return Result of `f_alignment_of_type` (a_targ_alignment).
   */
  a_targ_alignment f_alignment_of_type(a_type_ptr ty) { return 1; }
  /**
   * @brief Test stub or helper for `is_trivially_copyable_type`.
   * @details Provides mock or helper behavior for `is_trivially_copyable_type` within `tests/test_llvm_vtable_rtti.cpp`.
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
   * @details Provides mock or helper behavior for `conv_seq_to_file_and_line` within `tests/test_llvm_vtable_rtti.cpp`.
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
   * @details Provides mock or helper behavior for `mangled_typeinfo_name` within `tests/test_llvm_vtable_rtti.cpp`.
   * @param[in] type Parameter `type` for `mangled_typeinfo_name`.
   * @return Result of `mangled_typeinfo_name` (char*).
   */
  char* mangled_typeinfo_name(a_type_ptr type) {
    if (g_use_custom_ti_name) return g_mangled_ti_name;
    if (type && type->source_corresp.name) return const_cast<char*>(type->source_corresp.name);
    if (type && (type->kind == tk_class || type->kind == tk_struct)) return (char*)"_ZTI1C";
    if (type && type->kind == tk_pointer) return (char*)"_ZTIPi";
    return (char*)"_ZTIi";
  }

  /**
   * @brief Test stub or helper for `f_get_type_qualifiers`.
   * @details Provides mock or helper behavior for `f_get_type_qualifiers` within `tests/test_llvm_vtable_rtti.cpp`.
   * @param[in] type Parameter `type` for `f_get_type_qualifiers`.
   * @param[in] x Parameter `x` for `f_get_type_qualifiers`.
   * @return Result of `f_get_type_qualifiers` (a_type_qualifier_set).
   */
  a_type_qualifier_set f_get_type_qualifiers(a_type_ptr type, a_boolean x) { return 0; }
  /**
   * @brief Test stub or helper for `is_reference_type`.
   * @details Provides mock or helper behavior for `is_reference_type` within `tests/test_llvm_vtable_rtti.cpp`.
   * @param[in] ty Parameter `ty` for `is_reference_type`.
   * @return Result of `is_reference_type` (a_boolean).
   */
  a_boolean is_reference_type(a_type_ptr ty) { (void)ty; return g_is_ref_type; }
  /**
   * @brief Test stub or helper for `is_empty_class_type`.
   * @details Provides mock or helper behavior for `is_empty_class_type` within `tests/test_llvm_vtable_rtti.cpp`.
   * @param[in] ty Parameter `ty` for `is_empty_class_type`.
   * @return Result of `is_empty_class_type` (a_boolean).
   */
  a_boolean is_empty_class_type(a_type_ptr ty) { return ty && ty->variant.class_struct_union.is_empty_class; }
  /** @brief Controls whether `llvm_lower_global_variable` skips creating a named global. */
  static bool g_skip_ti_gv_creation = false;

  /**
   * @brief Test stub for `llvm_lower_global_variable`.
   * @details Creates a placeholder external `GlobalVariable` when `var` is named so RTTI lookups succeed in unit tests.
   * @param[in] var Frontend variable pointer.
   * @return `llvm_gen_be_error_t::ok` on success.
   */
  llvm_gen_be_error_t llvm_lower_global_variable(a_variable_ptr var) noexcept {
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    if (!g_skip_ti_gv_creation && var && var->source_corresp.name && be_state && be_state->module &&
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
  a_byte_boolean int_kind_is_signed[ik_last] = {0};

  /**
   * @brief Test stub or helper for `llvm_lower_expression`.
   * @details Provides mock or helper behavior for `llvm_lower_expression` within `tests/test_llvm_vtable_rtti.cpp`.
   * @param[in] expr Parameter `expr` for `llvm_lower_expression`.
   * @param[out] out_val Parameter `out_val` for `llvm_lower_expression`.
   * @return Result of `llvm_lower_expression` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_expression(an_expr_node_ptr expr, llvm::Value** out_val) noexcept {
    (void)expr;
    if (!out_val) return llvm_gen_be_error_t::invalid_argument;
    if (g_vr_fail_expr < 0) return llvm_gen_be_error_t::unsupported_expr;
    if (g_vr_fail_expr > 0) {
      --g_vr_fail_expr;
      if (g_vr_fail_expr == 0) return llvm_gen_be_error_t::unsupported_expr;
    }
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
    if (g_vr_expr_mode == 1) {
      llvm::StructType* st = llvm::StructType::get(*be_state->context, {ptr_ty, i64_ty});
      *out_val = llvm::ConstantStruct::getNullValue(st);
    } else if (g_vr_expr_mode == 2) {
      *out_val = llvm::ConstantInt::get(i64_ty, 8);
    } else if (g_vr_expr_mode == 3) {
      *out_val = nullptr;
    } else if (g_vr_expr_mode == 4) {
      *out_val = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
      g_vr_expr_mode = 2;
    } else if (g_vr_expr_mode == 5) {
      *out_val = llvm::ConstantInt::get(i64_ty, 8);
      g_vr_expr_mode = 0;
    } else if (g_vr_expr_mode == 6) {
      *out_val = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
      g_vr_expr_mode = 3;
    } else if (g_vr_expr_mode == 7) {
      llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
      llvm::StructType* st3 = llvm::StructType::get(*be_state->context, {ptr_ty, i32_ty, i32_ty});
      *out_val = llvm::ConstantStruct::getNullValue(st3);
    } else {
      *out_val = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
    }
    return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub or helper for `get_llvm_type`.
   * @details Provides mock or helper behavior for `get_llvm_type` within `tests/test_llvm_vtable_rtti.cpp`.
   * @param[in] edg_type Parameter `edg_type` for `get_llvm_type`.
   * @param[out] out_type Parameter `out_type` for `get_llvm_type`.
   * @return Result of `get_llvm_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_llvm_type(a_type_ptr edg_type, llvm::Type** out_type) noexcept {
    if (!out_type) return llvm_gen_be_error_t::invalid_argument;
    if (g_vr_fail_type || (edg_type && edg_type->alignment == 255)) {
      return llvm_gen_be_error_t::unsupported_type;
    }
    if (edg_type && edg_type->kind == tk_void) {
      *out_type = llvm::Type::getVoidTy(*be_state->context);
      return llvm_gen_be_error_t::ok;
    }
    if (edg_type && edg_type->kind == tk_routine) {
      llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
      llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
      if (edg_type->size == 1) {
        *out_type = llvm::FunctionType::get(ptr_ty, {ptr_ty, i32_ty}, false);
      } else if (edg_type->size == 2) {
        *out_type = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), {ptr_ty}, true);
      } else if (edg_type->size == 3) {
        *out_type = llvm::FunctionType::get(i32_ty, {}, false);
      } else if (edg_type->size == 4) {
        *out_type = llvm::FunctionType::get(i32_ty, {i32_ty}, false);
      } else if (edg_type->size == 5) {
        *out_type = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), {ptr_ty, ptr_ty}, false);
      } else if (edg_type->size == 6) {
        *out_type = llvm::FunctionType::get(ptr_ty, {ptr_ty}, true);
      } else {
        *out_type = llvm::FunctionType::get(i32_ty, {ptr_ty}, false);
      }
    } else {
      *out_type = llvm::PointerType::getUnqual(*be_state->context);
    }
    return llvm_gen_be_error_t::ok;
  }
  /**
   * @brief Test stub or helper for `llvm_build_target_abi_function_type`.
   * @details Delegates to `get_llvm_type` within `tests/test_llvm_vtable_rtti.cpp`.
   * @param[in] routine_edg_type Routine type descriptor.
   * @param[in] is_variadic_call Variadic call flag.
   * @param[in] num_fixed_args Number of fixed parameters.
   * @param[in] call_arg_types Call-site argument types.
   * @param[out] out_fn_ty Receives mock `FunctionType*`.
   * @param[out] out_attrs Receives mock `AttributeList`.
   * @return `llvm_gen_be_error_t::ok` on success, or error code from `get_llvm_type`.
   */
  llvm_gen_be_error_t llvm_build_target_abi_function_type(
      a_type_ptr routine_edg_type,
      bool is_variadic_call,
      unsigned num_fixed_args,
      const std::vector<a_type_ptr>& call_arg_types,
      llvm::FunctionType** out_fn_ty,
      llvm::AttributeList* out_attrs) noexcept {
    (void)is_variadic_call; (void)num_fixed_args; (void)call_arg_types;
    if (!out_fn_ty || !out_attrs) return llvm_gen_be_error_t::invalid_argument;
    llvm::Type* rty = nullptr;
    LLVM_GEN_BE_CHECK(get_llvm_type(routine_edg_type, &rty));
    *out_fn_ty = llvm::dyn_cast_or_null<llvm::FunctionType>(rty);
    *out_attrs = llvm::AttributeList();
    if (routine_edg_type && routine_edg_type->size == 5) {
      *out_attrs = out_attrs->addParamAttribute(
          *be_state->context, 0,
          llvm::Attribute::getWithStructRetType(*be_state->context, llvm::Type::getInt64Ty(*be_state->context)));
    }
    return llvm_gen_be_error_t::ok;
  }
  /**
   * @brief Test stub or helper for `vtbl_decider_function_for_class`.
   * @details Provides mock or helper behavior for `vtbl_decider_function_for_class` within `tests/test_llvm_vtable_rtti.cpp`.
   * @param[in] class_type Parameter `class_type` for `vtbl_decider_function_for_class`.
   * @param[in] unknown Parameter `unknown` for `vtbl_decider_function_for_class`.
   * @return Result of `vtbl_decider_function_for_class` (a_routine_ptr).
   */
  a_routine_ptr vtbl_decider_function_for_class(a_type_ptr class_type, a_boolean* unknown) {
      (void)class_type;
      if (unknown) *unknown = FALSE;
      return g_key_func;
  }
}


namespace edg {
  int g_use_override_for_bcp = 0; ///< When non-zero, `mangled_vtbl_name` returns `g_mangled_vtbl_override` even when `bcp` is non-null.
  /**
   * @brief Test stub or helper for `mangled_vtbl_name`.
   * @details Provides mock or helper behavior for `mangled_vtbl_name` within `tests/test_llvm_vtable_rtti.cpp`.
   * @param[in] class_type Parameter `class_type` for `mangled_vtbl_name`.
   * @param[in] bcp Parameter `bcp` for `mangled_vtbl_name`.
   * @param[in] ctor_bcp Parameter `ctor_bcp` for `mangled_vtbl_name`.
   * @return Result of `mangled_vtbl_name` (char*).
   */
  char* mangled_vtbl_name(a_type_ptr class_type, a_base_class_ptr bcp, a_base_class_ptr ctor_bcp) {
      (void)class_type;
      (void)ctor_bcp;
      if (bcp && !g_use_override_for_bcp) return (char*)"_ZTCmock";
      return g_mangled_vtbl_override;
  }
  /**
   * @brief Test stub or helper for `mangled_virtual_table_table_name`.
   * @details Provides mock or helper behavior for `mangled_virtual_table_table_name` within `tests/test_llvm_vtable_rtti.cpp`.
   * @param[in] type Parameter `type` for `mangled_virtual_table_table_name`.
   * @return Result of `mangled_virtual_table_table_name` (char*).
   */
  char* mangled_virtual_table_table_name(a_type_ptr type) {
      (void)type;
      return g_mangled_vtt_override;
  }
}

/**
 * @brief Entry point for the vtable, VTT, RTTI, and pointer-to-member unit test suite.
 * @details Exercises Itanium and MSVC vtables, VTTs, construction vtables, thunks, `typeid`, `dynamic_cast`, and member pointers.
 * @return 0 on success.
 */
int main(void) {
  printf("Running test_llvm_vtable_rtti...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_rtti", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);

  // 1. Basic RTTI tests
  a_type basic_ty = {};
  basic_ty.kind = tk_integer;
  llvm::Constant* ti = nullptr;
  assert(get_typeinfo_global(&basic_ty, &ti) == llvm_gen_be_error_t::ok);

  a_type ptr_ty = {};
  ptr_ty.kind = tk_pointer;
  ptr_ty.variant.pointer.type = &basic_ty;
  assert(get_typeinfo_global(&ptr_ty, &ti) == llvm_gen_be_error_t::ok);

  a_type class_ty = {};
  class_ty.kind = tk_class;
  a_class_type_supplement extra = {};
  class_ty.variant.class_struct_union.extra_info = &extra;
  assert(get_typeinfo_global(&class_ty, &ti) == llvm_gen_be_error_t::ok);

  a_base_class base = {};
  base.type = &basic_ty;
  extra.direct_base_classes = &base;
  base.is_virtual = FALSE;
  assert(get_typeinfo_global(&class_ty, &ti) == llvm_gen_be_error_t::ok);

  a_base_class base2 = {};
  base2.type = &basic_ty;
  base.next_direct = &base2;
  
  // Set up base_classes list with a virtual base for vbtable test
  a_base_class vbase1 = {};
  vbase1.type = &basic_ty;
  vbase1.is_virtual = TRUE;
  vbase1.offset = 16;
  extra.base_classes = &vbase1;
  
  assert(get_typeinfo_global(&class_ty, &ti) == llvm_gen_be_error_t::ok);

  // 2. VTable builder tests
  llvm_vtable_builder_t builder;
  assert(llvm_vtable_builder_init(nullptr, &class_ty, 0, true) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_vtable_builder_init(&builder, nullptr, 0, true) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_vtable_builder_init(&builder, &class_ty, 0, true) == llvm_gen_be_error_t::ok);

  assert(llvm_vtable_builder_compute(nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_vtable_builder_compute(&builder) == llvm_gen_be_error_t::ok);

  // Add virtual base to test vbase and vcall offset calculations
  a_base_class vbase = {};
  vbase.type = &basic_ty;
  vbase.is_virtual = TRUE;
  vbase.offset = 16;
  base2.next_direct = &vbase;

  assert(llvm_vtable_builder_init(&builder, &class_ty, 0, true) == llvm_gen_be_error_t::ok);
  assert(llvm_vtable_builder_compute(&builder) == llvm_gen_be_error_t::ok);
  assert(!builder.vbase_offsets.empty() && builder.vbase_offsets[0] == 16);
  assert(!builder.vcall_offsets.empty() && builder.vcall_offsets[0] == -16);

  // 3. Thunk emission tests
  llvm_thunk_info_t thunk = {};
  llvm::Function* thunk_fn = nullptr;
  assert(llvm_emit_thunk(nullptr, &thunk_fn) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_thunk(&thunk, nullptr) == llvm_gen_be_error_t::invalid_argument);

  // Non-virtual 'this' delta adjustment
  thunk.this_offset = -8;
  thunk.is_virtual = false;
  thunk.return_offset = 0;
  assert(llvm_emit_thunk(&thunk, &thunk_fn) == llvm_gen_be_error_t::ok);
  assert(thunk_fn != nullptr);

  // Virtual base thunk with vcall offset and covariant return adjustment
  llvm_thunk_info_t vthunk = {};
  vthunk.this_offset = -8;
  vthunk.is_virtual = true;
  vthunk.vcall_offset_index = -3;
  vthunk.return_offset = 16;
  llvm::Function* vthunk_fn = nullptr;
  assert(llvm_emit_thunk(&vthunk, &vthunk_fn) == llvm_gen_be_error_t::ok);
  assert(vthunk_fn != nullptr);

  // 4. Primary and secondary vtable emission
  llvm::Constant* vtable = nullptr;
  assert(llvm_emit_vtable(nullptr, &vtable) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_vtable(&class_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_vtable(&basic_ty, &vtable) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_vtable(&class_ty, &vtable) == llvm_gen_be_error_t::ok);

  // 5. Virtual call dispatch
  llvm::Value* obj = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
  a_routine routine = {};
  routine.number.virtual_function = 2;
  llvm::Value* func_ptr = nullptr;
  assert(llvm_lower_virtual_dispatch(nullptr, &routine, &func_ptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_virtual_dispatch(obj, nullptr, &func_ptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_virtual_dispatch(obj, &routine, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_virtual_dispatch(obj, &routine, &func_ptr) == llvm_gen_be_error_t::ok);
  assert(func_ptr != nullptr);

  // 6. MSVC ABI tests (vbtable and virtual dispatch)
  llvm::Constant* vbtable = nullptr;
  assert(llvm_emit_msvc_vbtable(nullptr, &vbtable) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_msvc_vbtable(&class_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_msvc_vbtable(&basic_ty, &vbtable) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_msvc_vbtable(&class_ty, &vbtable) == llvm_gen_be_error_t::ok);
  assert(vbtable != nullptr);

  llvm::Value* msvc_func_ptr = nullptr;
  assert(llvm_lower_msvc_virtual_dispatch(nullptr, &routine, &msvc_func_ptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_msvc_virtual_dispatch(obj, nullptr, &msvc_func_ptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_msvc_virtual_dispatch(obj, &routine, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_msvc_virtual_dispatch(obj, &routine, &msvc_func_ptr) == llvm_gen_be_error_t::ok);
  assert(msvc_func_ptr != nullptr);

  // 7. VTT and Construction VTable emission
  extra.virtual_function_info_base_class = &vbase1;
  llvm::Constant* vtt = nullptr;
  assert(llvm_emit_vtt(nullptr, &vtt) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_vtt(&class_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_vtt(&basic_ty, &vtt) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_vtt(&class_ty, &vtt) == llvm_gen_be_error_t::ok);
  assert(vtt != nullptr);

  a_type class_b = {};
  class_b.kind = tk_class;
  a_class_type_supplement extra_b = {};
  class_b.variant.class_struct_union.extra_info = &extra_b;
  extra_b.highest_virtual_function_number = 1;

  llvm::Constant* cvtable = nullptr;
  a_base_class null_type_bc = {};
  assert(llvm_emit_construction_vtable(nullptr, &class_b, &cvtable) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_construction_vtable(&class_ty, static_cast<a_type_ptr>(nullptr), &cvtable) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_construction_vtable(&class_ty, &class_b, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_construction_vtable(nullptr, &null_type_bc, &cvtable) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_construction_vtable(&class_ty, static_cast<a_base_class_ptr>(nullptr), &cvtable) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_construction_vtable(&class_ty, &null_type_bc, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_construction_vtable(&class_ty, &null_type_bc, &cvtable) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_construction_vtable(&class_ty, &class_b, &cvtable) == llvm_gen_be_error_t::ok);
  assert(cvtable != nullptr);

  // 8. Multiple inheritance secondary vtable emission
  a_base_class secondary_base = {};
  secondary_base.type = &class_b;
  secondary_base.offset = 8;
  secondary_base.is_virtual = FALSE;

  extra.base_classes = &secondary_base;
  extra.virtual_function_info_base_class = nullptr;

  llvm::Constant* mi_vtable = nullptr;
  assert(llvm_emit_vtable(&class_ty, &mi_vtable) == llvm_gen_be_error_t::ok);
  assert(mi_vtable != nullptr);

  // 9. Additional RTTI type coverage
  llvm::Constant* vmi_rtti = nullptr;
  assert(get_typeinfo_global(&class_ty, &vmi_rtti) == llvm_gen_be_error_t::ok);

  a_type ptrmem_ty = {};
  ptrmem_ty.kind = tk_ptr_to_member;
  ptrmem_ty.variant.ptr_to_member.type = &basic_ty;
  ptrmem_ty.variant.ptr_to_member.class_of_which_a_member = &class_ty;
  llvm::Constant* ptrmem_rtti = nullptr;
  assert(get_typeinfo_global(&ptrmem_ty, &ptrmem_rtti) == llvm_gen_be_error_t::ok);

  a_type func_ty = {};
  func_ty.kind = tk_routine;
  llvm::Constant* func_rtti = nullptr;
  assert(get_typeinfo_global(&func_ty, &func_rtti) == llvm_gen_be_error_t::ok);

  a_type enum_ty = {};
  enum_ty.kind = tk_enum;
  llvm::Constant* enum_rtti = nullptr;
  assert(get_typeinfo_global(&enum_ty, &enum_rtti) == llvm_gen_be_error_t::ok);

  // 10. Phase 2: MSVC RTTI Complete Object Locator & BaseClassDescriptor tests (non-virtual & virtual bases)
  a_type vfunc_rout_ty = {};
  vfunc_rout_ty.kind = tk_routine;
  a_routine vfunc_rout = {};
  vfunc_rout.is_virtual = TRUE;
  vfunc_rout.number.virtual_function = 1;
  vfunc_rout.type = &vfunc_rout_ty;
  vfunc_rout.source_corresp.name = (char*)"_ZN1C4virtEv";
  a_scope class_assoc_scope = {};
  class_assoc_scope.routines = &vfunc_rout;
  extra.assoc_scope = &class_assoc_scope;
  extra.highest_virtual_function_number = 1;

  llvm_vtable_builder_t vfunc_builder;
  assert(llvm_vtable_builder_init(&vfunc_builder, &class_ty, 0, true) == llvm_gen_be_error_t::ok);
  assert(llvm_vtable_builder_compute(&vfunc_builder) == llvm_gen_be_error_t::ok);
  assert(vfunc_builder.method_slots.size() == 1);
  llvm::Function* vslot_fn = llvm::dyn_cast<llvm::Function>(vfunc_builder.method_slots[0]);
  assert(vslot_fn != nullptr);
  assert(vslot_fn->getFunctionType()->getReturnType()->isIntegerTy(32));

  // Set up both a non-virtual base and a virtual base in extra.base_classes
  secondary_base.has_public_derivation = TRUE;
  secondary_base.is_virtual = FALSE;
  secondary_base.offset = 8;
  vbase1.has_public_derivation = FALSE;
  vbase1.is_virtual = TRUE;
  vbase1.offset = 16;
  secondary_base.next = &vbase1;
  vbase1.next = nullptr;
  extra.base_classes = &secondary_base;

  llvm::Constant* col = nullptr;
  assert(llvm_emit_msvc_rtti_complete_object_locator(nullptr, &col) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_msvc_rtti_complete_object_locator(&class_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_msvc_rtti_complete_object_locator(&basic_ty, &col) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_msvc_rtti_complete_object_locator(&class_ty, &col) == llvm_gen_be_error_t::ok);
  assert(col != nullptr);

  // Verify generated MSVC BaseClassDescriptors for non-virtual base (pdisp == -1) and virtual base (pdisp >= 0, vdisp > 0)
  bool found_nonvirt_bcd = false;
  bool found_virt_bcd = false;
  for (llvm::GlobalVariable& gv : state.module->globals()) {
    if (gv.getName().starts_with("??_R1") && gv.hasInitializer()) {
      if (auto* cs = llvm::dyn_cast<llvm::ConstantStruct>(gv.getInitializer())) {
        if (cs->getNumOperands() >= 6) {
          auto* mdisp_c = llvm::dyn_cast<llvm::ConstantInt>(cs->getOperand(2));
          auto* pdisp_c = llvm::dyn_cast<llvm::ConstantInt>(cs->getOperand(3));
          auto* vdisp_c = llvm::dyn_cast<llvm::ConstantInt>(cs->getOperand(4));
          if (mdisp_c && pdisp_c && vdisp_c) {
            if (mdisp_c->getSExtValue() == 8 && pdisp_c->getSExtValue() == -1 && vdisp_c->getSExtValue() == 0) {
              found_nonvirt_bcd = true;
            }
            if (pdisp_c->getSExtValue() >= 0 && vdisp_c->getSExtValue() > 0) {
              found_virt_bcd = true;
            }
          }
        }
      }
    }
  }
  assert(found_nonvirt_bcd);
  assert(found_virt_bcd);

  // 11. Phase 2: dynamic_cast expression tests
  an_expr_node sub_op = {};
  sub_op.type = &class_ty;
  an_expr_node dyn_cast_node = {};
  dyn_cast_node.kind = enk_operation;
  dyn_cast_node.variant.operation.kind = eok_dynamic_cast;
  dyn_cast_node.variant.operation.operands = &sub_op;
  dyn_cast_node.type = &class_b;

  llvm::Value* cast_val = nullptr;
  assert(llvm_lower_dynamic_cast(nullptr, &cast_val) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_dynamic_cast(&dyn_cast_node, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_dynamic_cast(&dyn_cast_node, &cast_val) == llvm_gen_be_error_t::ok);
  assert(cast_val != nullptr);

  // Reference dynamic cast with bad_cast check
  dyn_cast_node.variant.operation.kind = eok_ref_dynamic_cast;
  llvm::Value* ref_cast_val = nullptr;
  assert(llvm_lower_dynamic_cast(&dyn_cast_node, &ref_cast_val) == llvm_gen_be_error_t::ok);
  assert(ref_cast_val != nullptr);

  // 12. Phase 2: typeid expression tests
  an_expr_node type_op = {};
  type_op.kind = enk_type_operand;
  type_op.variant.type_operand.type = &class_ty;

  an_expr_node typeid_node = {};
  typeid_node.kind = enk_typeid;
  typeid_node.variant.typeid_info.type_with_opt_expr = &type_op;
  typeid_node.variant.typeid_info.is_dynamic = FALSE;

  llvm::Value* ti_val = nullptr;
  assert(llvm_lower_typeid(nullptr, &ti_val) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_typeid(&typeid_node, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_typeid(&typeid_node, &ti_val) == llvm_gen_be_error_t::ok);
  assert(ti_val != nullptr);

  // Polymorphic dynamic typeid(expr)
  an_expr_node obj_expr = {};
  obj_expr.type = &class_ty;
  type_op.next = &obj_expr;
  typeid_node.variant.typeid_info.is_dynamic = TRUE;

  llvm::Value* dyn_ti_val = nullptr;
  assert(llvm_lower_typeid(&typeid_node, &dyn_ti_val) == llvm_gen_be_error_t::ok);
  assert(dyn_ti_val != nullptr);

  
  // 13. Pointer-to-member lowering tests
  a_type pm_ty = {};
  pm_ty.kind = tk_ptr_to_member;
  pm_ty.variant.ptr_to_member.class_of_which_a_member = &class_ty;
  pm_ty.variant.ptr_to_member.type = &basic_ty;
  
  llvm::Type* pm_llvm_ty = nullptr;
  assert(llvm_type_from_ptr_to_member(&pm_ty, &pm_llvm_ty) == llvm_gen_be_error_t::ok);
  assert(pm_llvm_ty != nullptr);
  
  an_expr_node pm_expr = {};
  pm_expr.type = &pm_ty;
  llvm::Constant* pm_const = nullptr;
  
  

  a_constant ptrmem_const = {};
  ptrmem_const.kind = ck_ptr_to_member;
  assert(llvm_const_from_ptr_to_member(&ptrmem_const, pm_llvm_ty, &pm_const) == llvm_gen_be_error_t::ok);

  llvm::Value* pm_call_val = nullptr;
  assert(llvm_lower_pm_call(&pm_expr, &pm_call_val) == llvm_gen_be_error_t::invalid_argument);

  llvm::Value* pm_field_val = nullptr;
  assert(llvm_lower_pm_field_access(&pm_expr, &pm_field_val) == llvm_gen_be_error_t::invalid_argument);

  llvm::Value* pm_cast_val = nullptr;
  assert(llvm_lower_pm_cast(&pm_expr, &pm_cast_val) == llvm_gen_be_error_t::invalid_argument);

  // Test valid pointer-to-member base-to-derived cast across class hierarchy
  a_type pm_derived_ty = {};
  pm_derived_ty.kind = tk_ptr_to_member;
  pm_derived_ty.variant.ptr_to_member.class_of_which_a_member = &class_ty;
  pm_derived_ty.variant.ptr_to_member.type = &basic_ty;

  a_type pm_base_ty = {};
  pm_base_ty.kind = tk_ptr_to_member;
  pm_base_ty.variant.ptr_to_member.class_of_which_a_member = &class_b;
  pm_base_ty.variant.ptr_to_member.type = &basic_ty;

  an_expr_node pm_sub_op = {};
  pm_sub_op.type = &pm_base_ty;
  an_expr_node pm_cast_expr = {};
  pm_cast_expr.kind = enk_operation;
  pm_cast_expr.variant.operation.kind = eok_pm_base_class_cast;
  pm_cast_expr.variant.operation.operands = &pm_sub_op;
  pm_cast_expr.type = &pm_derived_ty;
  assert(llvm_lower_pm_cast(&pm_cast_expr, &pm_cast_val) == llvm_gen_be_error_t::ok);

  // Test MSVC ??_7 vftable emission with populated virtual method slots
  EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
  llvm::Constant* msvc_vftable = nullptr;
  assert(llvm_emit_vtable(&class_ty, &msvc_vftable) == llvm_gen_be_error_t::ok);
  assert(msvc_vftable != nullptr);

  llvm::Value* pm_cmp_val = nullptr;
  assert(llvm_lower_pm_comparison(&pm_expr, &pm_cmp_val) == llvm_gen_be_error_t::invalid_argument);

  // 14. Exhaustive branch coverage for llvm_gen_be_ptrmem.cpp, llvm_gen_be_rtti.cpp, and llvm_gen_be_vtable.cpp
  {
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(*state.context);
    llvm::Type* llvm_ptr_ty = llvm::PointerType::getUnqual(*state.context);
    llvm::StructType* pm_fn_st = llvm::StructType::get(*state.context, {llvm_ptr_ty, i64_ty});

    a_type tref_pm = {}; tref_pm.kind = tk_typeref; tref_pm.variant.typeref.type = &pm_ty;
    a_type tref_rout = {}; tref_rout.kind = tk_typeref; tref_rout.variant.typeref.type = &func_ty;
    a_type tref_cls = {}; tref_cls.kind = tk_typeref; tref_cls.variant.typeref.type = &class_ty;

    // --- llvm_gen_be_ptrmem.cpp ---
    // 1. llvm_type_from_ptr_to_member: null args, non-ptrmem kind, typerefs, Itanium & MSVC (data, SI, MI, VI, supp==nullptr)
    assert(llvm_type_from_ptr_to_member(nullptr, &pm_llvm_ty) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_type_from_ptr_to_member(&pm_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_type_from_ptr_to_member(&basic_ty, &pm_llvm_ty) == llvm_gen_be_error_t::invalid_argument);

    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
    pm_ty.variant.ptr_to_member.type = nullptr;
    assert(llvm_type_from_ptr_to_member(&tref_pm, &pm_llvm_ty) == llvm_gen_be_error_t::ok);
    pm_ty.variant.ptr_to_member.type = &tref_rout;
    pm_ty.variant.ptr_to_member.class_of_which_a_member = &tref_cls;
    assert(llvm_type_from_ptr_to_member(&tref_pm, &pm_llvm_ty) == llvm_gen_be_error_t::ok); // Itanium func

    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    pm_ty.variant.ptr_to_member.type = &basic_ty;
    assert(llvm_type_from_ptr_to_member(&pm_ty, &pm_llvm_ty) == llvm_gen_be_error_t::ok); // MSVC data

    pm_ty.variant.ptr_to_member.type = &tref_rout;
    a_type cls_no_supp = {}; cls_no_supp.kind = tk_class;
    pm_ty.variant.ptr_to_member.class_of_which_a_member = &cls_no_supp;
    assert(llvm_type_from_ptr_to_member(&pm_ty, &pm_llvm_ty) == llvm_gen_be_error_t::ok); // MSVC SI (supp == nullptr, base_count == 0)

    cls_no_supp.variant.class_struct_union.any_virtual_base_classes = TRUE;
    assert(llvm_type_from_ptr_to_member(&pm_ty, &pm_llvm_ty) == llvm_gen_be_error_t::ok); // MSVC VI (base_count == 0, any_virtual_base_classes == TRUE)

    pm_ty.variant.ptr_to_member.class_of_which_a_member = &tref_cls;
    class_ty.variant.class_struct_union.any_virtual_base_classes = FALSE;
    assert(llvm_type_from_ptr_to_member(&pm_ty, &pm_llvm_ty) == llvm_gen_be_error_t::ok); // MSVC MI (base_count > 1, !any_virtual_base_classes)
    class_ty.variant.class_struct_union.any_virtual_base_classes = TRUE;
    assert(llvm_type_from_ptr_to_member(&pm_ty, &pm_llvm_ty) == llvm_gen_be_error_t::ok); // MSVC VI (base_count > 1, any_virtual_base_classes)

    // 2. llvm_const_from_ptr_to_member: null args, non-ck_ptr_to_member, function ptr (null r, virtual r, non-virtual named/unnamed r, casting_base_class), data ptr (null f, non-null f, casting_base_class)
    assert(llvm_const_from_ptr_to_member(nullptr, i64_ty, &pm_const) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_ptr_to_member(&ptrmem_const, i64_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);

    ptrmem_const.kind = ck_integer;
    assert(llvm_const_from_ptr_to_member(&ptrmem_const, pm_fn_st, &pm_const) == llvm_gen_be_error_t::ok);
    assert(llvm_const_from_ptr_to_member(&ptrmem_const, i64_ty, &pm_const) == llvm_gen_be_error_t::ok);
    assert(llvm_const_from_ptr_to_member(&ptrmem_const, nullptr, &pm_const) == llvm_gen_be_error_t::ok);

    ptrmem_const.kind = ck_ptr_to_member;
    ptrmem_const.variant.ptr_to_member.is_function_ptr = TRUE;
    ptrmem_const.variant.ptr_to_member.variant.routine = nullptr;
    assert(llvm_const_from_ptr_to_member(&ptrmem_const, pm_fn_st, &pm_const) == llvm_gen_be_error_t::ok);
    assert(llvm_const_from_ptr_to_member(&ptrmem_const, llvm_ptr_ty, &pm_const) == llvm_gen_be_error_t::ok);
    assert(llvm_const_from_ptr_to_member(&ptrmem_const, nullptr, &pm_const) == llvm_gen_be_error_t::ok);

    llvm::Type* i32_ty = llvm::Type::getInt32Ty(*state.context);
    llvm::StructType* msvc_pm3_st = llvm::StructType::get(*state.context, {llvm_ptr_ty, i32_ty, i32_ty});
    a_base_class cast_base = {}; cast_base.offset = 16;
    ptrmem_const.variant.ptr_to_member.casting_base_class = &cast_base;
    a_routine pm_virt_r = {}, pm_nonvirt_r = {};
    pm_virt_r.number.virtual_function = 2;
    ptrmem_const.variant.ptr_to_member.variant.routine = &pm_virt_r;
    assert(llvm_const_from_ptr_to_member(&ptrmem_const, pm_fn_st, &pm_const) == llvm_gen_be_error_t::ok);
    assert(llvm_const_from_ptr_to_member(&ptrmem_const, llvm_ptr_ty, &pm_const) == llvm_gen_be_error_t::ok);
    assert(llvm_const_from_ptr_to_member(&ptrmem_const, msvc_pm3_st, &pm_const) == llvm_gen_be_error_t::ok);

    ptrmem_const.variant.ptr_to_member.casting_base_class = nullptr;
    pm_nonvirt_r.number.virtual_function = 0;
    pm_nonvirt_r.source_corresp.name = (char*)"my_pm_fn";
    ptrmem_const.variant.ptr_to_member.variant.routine = &pm_nonvirt_r;
    assert(llvm_const_from_ptr_to_member(&ptrmem_const, nullptr, &pm_const) == llvm_gen_be_error_t::ok);
    pm_nonvirt_r.source_corresp.name = nullptr;
    assert(llvm_const_from_ptr_to_member(&ptrmem_const, pm_fn_st, &pm_const) == llvm_gen_be_error_t::ok);

    // Test ARM64/ARM/Thumb Itanium ABI pointer-to-member-function constant (virtual and non-virtual)
    const char* arm_pm_triples[] = {
        "aarch64-unknown-linux-gnu",
        "armv7-unknown-linux-gnueabihf",
        "thumbv7-unknown-linux-gnueabihf",
        "x86_64-unknown-linux-gnu"
    };
    for (const char* atr : arm_pm_triples) {
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), atr);
      ptrmem_const.variant.ptr_to_member.casting_base_class = &cast_base;
      ptrmem_const.variant.ptr_to_member.variant.routine = &pm_virt_r;
      assert(llvm_const_from_ptr_to_member(&ptrmem_const, pm_fn_st, &pm_const) == llvm_gen_be_error_t::ok);
      ptrmem_const.variant.ptr_to_member.variant.routine = &pm_nonvirt_r;
      assert(llvm_const_from_ptr_to_member(&ptrmem_const, pm_fn_st, &pm_const) == llvm_gen_be_error_t::ok);
    }
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");

    ptrmem_const.variant.ptr_to_member.is_function_ptr = FALSE;
    ptrmem_const.variant.ptr_to_member.variant.field = nullptr;
    assert(llvm_const_from_ptr_to_member(&ptrmem_const, nullptr, &pm_const) == llvm_gen_be_error_t::ok);

    a_field pm_field = {}; pm_field.offset = 12;
    ptrmem_const.variant.ptr_to_member.variant.field = &pm_field;
    assert(llvm_const_from_ptr_to_member(&ptrmem_const, i64_ty, &pm_const) == llvm_gen_be_error_t::ok);
    ptrmem_const.variant.ptr_to_member.casting_base_class = &cast_base;
    assert(llvm_const_from_ptr_to_member(&ptrmem_const, nullptr, &pm_const) == llvm_gen_be_error_t::ok);

    // 3. llvm_lower_pm_call: null args, missing op2, op1/op2/arg errors, StructType pm_val vs pointer pm_val, 3-word MSVC StructType, and invoke unwinding
    assert(llvm_lower_pm_call(nullptr, &pm_call_val) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_pm_call(&pm_expr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    an_expr_node pm_op1 = {}, pm_op2 = {}, pm_arg3 = {};
    pm_expr.variant.operation.operands = &pm_op1;
    assert(llvm_lower_pm_call(&pm_expr, &pm_call_val) == llvm_gen_be_error_t::invalid_argument); // op2 == nullptr
    pm_op1.next = &pm_op2;
    pm_op2.next = &pm_arg3;

    g_vr_expr_mode = 1; // StructType { ptr, i64 }
    assert(llvm_lower_pm_call(&pm_expr, &pm_call_val) == llvm_gen_be_error_t::ok);
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "aarch64-unknown-linux-gnu");
    assert(llvm_lower_pm_call(&pm_expr, &pm_call_val) == llvm_gen_be_error_t::ok);
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "armv7-unknown-linux-gnueabihf");
    assert(llvm_lower_pm_call(&pm_expr, &pm_call_val) == llvm_gen_be_error_t::ok);
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "thumbv7-unknown-linux-gnueabihf");
    assert(llvm_lower_pm_call(&pm_expr, &pm_call_val) == llvm_gen_be_error_t::ok);
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    g_vr_expr_mode = 7; // StructType { ptr, i32, i32 } (MSVC 3-word)
    assert(llvm_lower_pm_call(&pm_expr, &pm_call_val) == llvm_gen_be_error_t::ok);
    g_vr_expr_mode = 0; // PointerType
    assert(llvm_lower_pm_call(&pm_expr, &pm_call_val) == llvm_gen_be_error_t::ok);

    llvm::BasicBlock* dummy_lpad = llvm::BasicBlock::Create(*state.context, "dummy_lpad", f);
    state.current_landing_pads.push_back(dummy_lpad);
    assert(llvm_lower_pm_call(&pm_expr, &pm_call_val) == llvm_gen_be_error_t::ok);
    state.current_landing_pads.clear();

    g_vr_fail_expr = 1;
    assert(llvm_lower_pm_call(&pm_expr, &pm_call_val) == llvm_gen_be_error_t::unsupported_expr);
    g_vr_fail_expr = 2;
    assert(llvm_lower_pm_call(&pm_expr, &pm_call_val) == llvm_gen_be_error_t::unsupported_expr);
    g_vr_fail_expr = 3;
    assert(llvm_lower_pm_call(&pm_expr, &pm_call_val) == llvm_gen_be_error_t::unsupported_expr);
    g_vr_fail_expr = 0;
    pm_op2.next = nullptr;

    // 4. llvm_lower_pm_field_access: null args, missing op2, op1/op2 errors, v1/v2 null, v1 ptr, v2 ptr, neither ptr
    assert(llvm_lower_pm_field_access(nullptr, &pm_field_val) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_pm_field_access(&pm_expr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    pm_op1.next = nullptr;
    assert(llvm_lower_pm_field_access(&pm_expr, &pm_field_val) == llvm_gen_be_error_t::invalid_argument);
    pm_op1.next = &pm_op2;

    g_vr_fail_expr = 1;
    assert(llvm_lower_pm_field_access(&pm_expr, &pm_field_val) == llvm_gen_be_error_t::unsupported_expr);
    g_vr_fail_expr = 2;
    assert(llvm_lower_pm_field_access(&pm_expr, &pm_field_val) == llvm_gen_be_error_t::unsupported_expr);
    g_vr_fail_expr = 0;

    g_vr_expr_mode = 3; // v1 == nullptr
    assert(llvm_lower_pm_field_access(&pm_expr, &pm_field_val) == llvm_gen_be_error_t::unsupported_expr);
    g_vr_expr_mode = 6; // v1 != nullptr, v2 == nullptr
    assert(llvm_lower_pm_field_access(&pm_expr, &pm_field_val) == llvm_gen_be_error_t::unsupported_expr);
    g_vr_expr_mode = 4; // v1 is ptr, v2 is i64
    assert(llvm_lower_pm_field_access(&pm_expr, &pm_field_val) == llvm_gen_be_error_t::ok);
    g_vr_expr_mode = 5; // v1 is i64, v2 is ptr
    assert(llvm_lower_pm_field_access(&pm_expr, &pm_field_val) == llvm_gen_be_error_t::ok);
    g_vr_expr_mode = 2; // both i64 -> unsupported_expr
    assert(llvm_lower_pm_field_access(&pm_expr, &pm_field_val) == llvm_gen_be_error_t::unsupported_expr);
    g_vr_expr_mode = 0;

    // 5. llvm_lower_pm_cast & find_base_class_offset:
    assert(llvm_lower_pm_cast(nullptr, &pm_cast_val) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_pm_cast(&pm_cast_expr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    g_vr_fail_expr = 1;
    assert(llvm_lower_pm_cast(&pm_cast_expr, &pm_cast_val) == llvm_gen_be_error_t::unsupported_expr);
    g_vr_fail_expr = 0;

    // Test find_base_class_offset: derived == target, supp == nullptr, non-matching b->type in base_classes & direct_base_classes,
    // direct match in direct_base_classes, multi-level recursive match (D -> M -> B), and no match; plus StructType, IntegerType, and null sub_val
    a_type st_root = {}, st_mid = {}, st_leaf = {}, st_unrel = {};
    st_root.kind = tk_struct; st_mid.kind = tk_struct; st_leaf.kind = tk_struct; st_unrel.kind = tk_struct;
    a_type tref_root = {}; tref_root.kind = tk_typeref; tref_root.variant.typeref.type = &st_root;
    a_class_type_supplement supp_mid = {}, supp_leaf = {};
    st_mid.variant.class_struct_union.extra_info = &supp_mid;
    st_leaf.variant.class_struct_union.extra_info = &supp_leaf;

    a_base_class db_null = {}, db_mid = {}, db_root = {}, b_null = {};
    b_null.type = &basic_ty;
    supp_leaf.base_classes = &b_null;
    db_null.type = &basic_ty; db_null.next_direct = &db_mid;
    db_mid.type = &st_mid; db_mid.offset = 8;
    supp_leaf.direct_base_classes = &db_null;
    db_root.type = &tref_root; db_root.offset = 4;
    supp_mid.direct_base_classes = &db_root;

    pm_derived_ty.variant.ptr_to_member.class_of_which_a_member = &tref_root; // wait: leaf vs root
    a_type tref_leaf = {}; tref_leaf.kind = tk_typeref; tref_leaf.variant.typeref.type = &st_leaf;
    pm_derived_ty.variant.ptr_to_member.class_of_which_a_member = &tref_leaf;
    pm_base_ty.variant.ptr_to_member.class_of_which_a_member = &tref_root;

    a_type tref_pm_der = {}; tref_pm_der.kind = tk_typeref; tref_pm_der.variant.typeref.type = &pm_derived_ty;
    a_type tref_pm_base = {}; tref_pm_base.kind = tk_typeref; tref_pm_base.variant.typeref.type = &pm_base_ty;
    pm_cast_expr.type = &tref_pm_der;
    pm_sub_op.type = &tref_pm_base;

    g_vr_expr_mode = 1; // StructType sub_val
    pm_cast_expr.variant.operation.kind = eok_pm_derived_class_cast;
    assert(llvm_lower_pm_cast(&pm_cast_expr, &pm_cast_val) == llvm_gen_be_error_t::ok);

    // Same class (derived_cls == target_base) + IntegerType sub_val
    g_vr_expr_mode = 2; // IntegerType sub_val
    pm_base_ty.variant.ptr_to_member.class_of_which_a_member = &tref_leaf;
    assert(llvm_lower_pm_cast(&pm_cast_expr, &pm_cast_val) == llvm_gen_be_error_t::ok);

    // Unrelated class with supp == nullptr + null sub_val
    g_vr_expr_mode = 3; // sub_val == nullptr
    pm_base_ty.variant.ptr_to_member.class_of_which_a_member = &st_unrel;
    assert(llvm_lower_pm_cast(&pm_cast_expr, &pm_cast_val) == llvm_gen_be_error_t::ok);
    g_vr_expr_mode = 0;

    // Non-class d_cls / s_cls, non-ptrmem dest/src, and null expr->type / sub->type
    pm_base_ty.variant.ptr_to_member.class_of_which_a_member = &basic_ty;
    assert(llvm_lower_pm_cast(&pm_cast_expr, &pm_cast_val) == llvm_gen_be_error_t::ok);
    pm_derived_ty.variant.ptr_to_member.class_of_which_a_member = &basic_ty;
    assert(llvm_lower_pm_cast(&pm_cast_expr, &pm_cast_val) == llvm_gen_be_error_t::ok);
    pm_sub_op.type = &basic_ty;
    assert(llvm_lower_pm_cast(&pm_cast_expr, &pm_cast_val) == llvm_gen_be_error_t::ok);
    pm_cast_expr.type = &basic_ty;
    assert(llvm_lower_pm_cast(&pm_cast_expr, &pm_cast_val) == llvm_gen_be_error_t::ok);
    pm_sub_op.type = nullptr;
    assert(llvm_lower_pm_cast(&pm_cast_expr, &pm_cast_val) == llvm_gen_be_error_t::ok);
    pm_cast_expr.type = nullptr;
    assert(llvm_lower_pm_cast(&pm_cast_expr, &pm_cast_val) == llvm_gen_be_error_t::ok);

    // 6. llvm_lower_pm_comparison: null args, op1/op2 errors, StructType (eq & ne), IntegerType (eq & ne), null v1/v2
    assert(llvm_lower_pm_comparison(nullptr, &pm_cmp_val) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_lower_pm_comparison(&pm_expr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    pm_op1.next = nullptr;
    assert(llvm_lower_pm_comparison(&pm_expr, &pm_cmp_val) == llvm_gen_be_error_t::invalid_argument);
    pm_op1.next = &pm_op2;

    g_vr_fail_expr = 1;
    assert(llvm_lower_pm_comparison(&pm_expr, &pm_cmp_val) == llvm_gen_be_error_t::unsupported_expr);
    g_vr_fail_expr = 2;
    assert(llvm_lower_pm_comparison(&pm_expr, &pm_cmp_val) == llvm_gen_be_error_t::unsupported_expr);
    g_vr_fail_expr = 0;

    g_vr_expr_mode = 1; // StructType
    pm_expr.variant.operation.kind = eok_eq;
    assert(llvm_lower_pm_comparison(&pm_expr, &pm_cmp_val) == llvm_gen_be_error_t::ok);
    pm_expr.variant.operation.kind = eok_ne;
    assert(llvm_lower_pm_comparison(&pm_expr, &pm_cmp_val) == llvm_gen_be_error_t::ok);
    for (const char* atr_cmp : arm_pm_triples) {
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), atr_cmp);
      assert(llvm_lower_pm_comparison(&pm_expr, &pm_cmp_val) == llvm_gen_be_error_t::ok);
    }
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");

    g_vr_expr_mode = 2; // IntegerType
    pm_expr.variant.operation.kind = eok_eq;
    assert(llvm_lower_pm_comparison(&pm_expr, &pm_cmp_val) == llvm_gen_be_error_t::ok);
    pm_expr.variant.operation.kind = eok_ne;
    assert(llvm_lower_pm_comparison(&pm_expr, &pm_cmp_val) == llvm_gen_be_error_t::ok);

    g_vr_expr_mode = 3; // v1 == nullptr
    assert(llvm_lower_pm_comparison(&pm_expr, &pm_cmp_val) == llvm_gen_be_error_t::unsupported_expr);
    g_vr_expr_mode = 6; // v1 != nullptr, v2 == nullptr
    assert(llvm_lower_pm_comparison(&pm_expr, &pm_cmp_val) == llvm_gen_be_error_t::unsupported_expr);
    g_vr_expr_mode = 0;

    // --- llvm_gen_be_rtti.cpp ---
    // 1. get_typeinfo_global: null out_const, null type, typeref, null mangled name, MSVC vtable_name,
    //    tk_struct, extra == nullptr, 1 virtual direct base, VMI public & virtual flags, enum_type == TRUE
    assert(get_typeinfo_global(&basic_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(get_typeinfo_global(nullptr, &ti) == llvm_gen_be_error_t::ok);

    g_use_custom_ti_name = 1;
    g_mangled_ti_name = nullptr; // mangled == nullptr -> "_ZTI_unknown"
    enum_ty.variant.integer.enum_type = TRUE;
    assert(get_typeinfo_global(&enum_ty, &ti) == llvm_gen_be_error_t::ok);
    // Second call hits existing GlobalVariable for "_ZTI_unknown"!
    assert(get_typeinfo_global(&enum_ty, &ti) == llvm_gen_be_error_t::ok);

    // 1 virtual direct base (__vmi_class_type_info with base_count == 1 && is_virtual == TRUE), plus public derivation
    a_type v1_cls = {}; v1_cls.kind = tk_struct; v1_cls.variant.class_struct_union.any_virtual_base_classes = TRUE;
    a_class_type_supplement v1_supp = {};
    a_base_class v1_db = {}; v1_db.type = &basic_ty; v1_db.is_virtual = TRUE; v1_db.has_public_derivation = TRUE; v1_db.offset = 8;
    v1_supp.direct_base_classes = &v1_db;
    v1_cls.variant.class_struct_union.extra_info = &v1_supp;
    g_mangled_ti_name = (char*)"_ZTI_v1_cls";
    assert(get_typeinfo_global(&v1_cls, &ti) == llvm_gen_be_error_t::ok);

    // 1 non-virtual direct base (__si_class_type_info), plus tk_ptr_to_member and tk_routine with unique mangled names
    v1_db.is_virtual = FALSE;
    g_mangled_ti_name = (char*)"_ZTI_si_cls";
    assert(get_typeinfo_global(&v1_cls, &ti) == llvm_gen_be_error_t::ok);
    v1_db.is_virtual = TRUE;

    g_mangled_ti_name = (char*)"_ZTI_ptrmem_unique";
    assert(get_typeinfo_global(&ptrmem_ty, &ti) == llvm_gen_be_error_t::ok);

    g_mangled_ti_name = (char*)"_ZTI_routine_unique";
    assert(get_typeinfo_global(&func_ty, &ti) == llvm_gen_be_error_t::ok);

    a_type float_ty = {}; float_ty.kind = tk_float;
    g_mangled_ti_name = (char*)"_ZTIf_unique";
    assert(get_typeinfo_global(&float_ty, &ti) == llvm_gen_be_error_t::ok);

    a_type arr_rtti_ty = {}; arr_rtti_ty.kind = tk_array;
    g_mangled_ti_name = (char*)"_ZTIA4_i_unique";
    assert(get_typeinfo_global(&arr_rtti_ty, &ti) == llvm_gen_be_error_t::ok);

    // Class with extra == nullptr
    g_mangled_ti_name = (char*)"_ZTI_no_extra_cls";
    assert(get_typeinfo_global(&cls_no_supp, &ti) == llvm_gen_be_error_t::ok);

    // 2. MSVC RTTI descriptors: mangle negative > 10, ".?AV...", ".X", nullptr mangled, cached globals,
    //    multiple virtual bases in base_classes (vb_idx++), virtual base with extra==nullptr / base_classes==nullptr,
    //    tgt_type == nullptr, tk_struct in COL
    a_type msvc_col_st = {}; msvc_col_st.kind = tk_struct;
    a_class_type_supplement msvc_col_supp = {};
    msvc_col_st.variant.class_struct_union.extra_info = &msvc_col_supp;
    a_base_class mb1 = {}, mb2 = {}, mb3 = {}, mb4 = {};
    mb1.type = &cls_no_supp; mb1.is_virtual = FALSE; mb1.offset = static_cast<a_targ_size_t>(-32); mb1.has_public_derivation = FALSE; mb1.next = &mb2;
    mb2.type = &basic_ty; mb2.is_virtual = TRUE; mb2.has_public_derivation = TRUE; mb2.next = &mb3;
    mb3.type = &v1_cls; mb3.is_virtual = TRUE; mb3.has_public_derivation = FALSE; mb3.next = &mb4;
    mb4.type = &msvc_col_st; mb4.is_virtual = FALSE; mb4.has_public_derivation = TRUE; mb4.offset = 0; mb4.next = nullptr; // matches self BCD (mdisp=0, pdisp=-1, vdisp=0, attr=0x40)!
    msvc_col_supp.base_classes = &mb1;

    g_mangled_ti_name = (char*)".?AVMsvcStruct@@";
    a_type tref_msvc_col = {}; tref_msvc_col.kind = tk_typeref; tref_msvc_col.variant.typeref.type = &msvc_col_st;
    assert(llvm_emit_msvc_rtti_complete_object_locator(&tref_msvc_col, &col) == llvm_gen_be_error_t::ok);
    // Second call hits cached COL global!
    assert(llvm_emit_msvc_rtti_complete_object_locator(&tref_msvc_col, &col) == llvm_gen_be_error_t::ok);

    // Test when mangled starts with '.' but mangled[1] != '?', and when mangled == nullptr, and when extra == nullptr
    g_mangled_ti_name = (char*)".X";
    a_type col_no_extra = {}; col_no_extra.kind = tk_class;
    assert(llvm_emit_msvc_rtti_complete_object_locator(&col_no_extra, &col) == llvm_gen_be_error_t::ok);

    g_mangled_ti_name = nullptr;
    state.module->getNamedGlobal("??_R4class6B@");
    assert(llvm_emit_msvc_rtti_complete_object_locator(&col_no_extra, &col) == llvm_gen_be_error_t::ok);
    g_use_custom_ti_name = 0;

    // 3. llvm_lower_dynamic_cast: null sub_expr, sub_expr error, MSVC pointer & ref dynamic_cast,
    //    Itanium pointer/reference types, direct_base_classes fallback, virtual/public/multiple-public/private/ambiguous/0 matches
    an_expr_node dc_empty = {};
    assert(llvm_lower_dynamic_cast(&dc_empty, &cast_val) == llvm_gen_be_error_t::invalid_argument);
    g_vr_fail_expr = 1;
    assert(llvm_lower_dynamic_cast(&dyn_cast_node, &cast_val) == llvm_gen_be_error_t::unsupported_expr);
    g_vr_fail_expr = 0;

    // MSVC dynamic_cast (pointer and reference)
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    dyn_cast_node.variant.operation.kind = eok_dynamic_cast;
    assert(llvm_lower_dynamic_cast(&dyn_cast_node, &cast_val) == llvm_gen_be_error_t::ok);
    dyn_cast_node.variant.operation.kind = eok_ref_dynamic_cast;
    assert(llvm_lower_dynamic_cast(&dyn_cast_node, &cast_val) == llvm_gen_be_error_t::ok);

    // Itanium dynamic_cast offset_hint cases
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
    dyn_cast_node.variant.operation.kind = eok_dynamic_cast;
    a_type ptr_to_src = {}, ptr_to_dst = {};
    ptr_to_src.kind = tk_pointer; ptr_to_src.variant.pointer.type = &st_root;
    ptr_to_dst.kind = tk_pointer; ptr_to_dst.variant.pointer.type = &st_mid;
    sub_op.type = &ptr_to_src;
    dyn_cast_node.type = &ptr_to_dst;

    // Case A0: d_supp->base_classes != nullptr (exercises the base_classes list branch)
    supp_mid.base_classes = &db_root;
    db_root.next = nullptr;
    db_root.is_virtual = FALSE; db_root.has_public_derivation = TRUE; db_root.offset = 12;
    assert(llvm_lower_dynamic_cast(&dyn_cast_node, &cast_val) == llvm_gen_be_error_t::ok);
    supp_mid.base_classes = nullptr;

    // Case A: d_supp->base_classes == nullptr, uses d_supp->direct_base_classes -> 1 public nonvirt match (offset_hint = pub_off)
    assert(llvm_lower_dynamic_cast(&dyn_cast_node, &cast_val) == llvm_gen_be_error_t::ok);

    // Case B: virtual match (virt_matches > 0 -> offset_hint = -1), plus g_is_ref_type = TRUE
    g_is_ref_type = TRUE;
    ptr_to_src.kind = tk_class; ptr_to_dst.kind = tk_class;
    db_root.is_virtual = TRUE;
    assert(llvm_lower_dynamic_cast(&dyn_cast_node, &cast_val) == llvm_gen_be_error_t::ok);
    g_is_ref_type = FALSE;
    sub_op.type = &st_root;
    dyn_cast_node.type = &st_mid;

    // Case C: > 1 public nonvirt matches (offset_hint = -3), plus non-matching b->type
    a_base_class db_root2 = {}, db_other = {};
    db_root.is_virtual = FALSE; db_root.has_public_derivation = TRUE; db_root.next_direct = &db_root2;
    db_root2.type = &st_root; db_root2.is_virtual = FALSE; db_root2.has_public_derivation = TRUE; db_root2.next_direct = &db_other;
    db_other.type = &basic_ty; db_other.next_direct = nullptr;
    assert(llvm_lower_dynamic_cast(&dyn_cast_node, &cast_val) == llvm_gen_be_error_t::ok);

    // Case D: 1 public + 1 private match (pub_nonvirt_matches == 1 && priv_matches == 1 -> neither branch, offset_hint = -1)
    db_root2.has_public_derivation = FALSE;
    assert(llvm_lower_dynamic_cast(&dyn_cast_node, &cast_val) == llvm_gen_be_error_t::ok);

    // Case E: only private match (pub_nonvirt_matches == 0 && priv_matches > 0 -> offset_hint = -2)
    db_root.has_public_derivation = FALSE;
    assert(llvm_lower_dynamic_cast(&dyn_cast_node, &cast_val) == llvm_gen_be_error_t::ok);

    // Case F: 0 matches, list == nullptr, d_supp == nullptr, non-class s_cls / d_cls, null src_type / dst_type
    supp_mid.direct_base_classes = &db_other; db_other.next_direct = nullptr;
    assert(llvm_lower_dynamic_cast(&dyn_cast_node, &cast_val) == llvm_gen_be_error_t::ok);
    supp_mid.direct_base_classes = nullptr;
    assert(llvm_lower_dynamic_cast(&dyn_cast_node, &cast_val) == llvm_gen_be_error_t::ok);
    st_mid.variant.class_struct_union.extra_info = nullptr;
    assert(llvm_lower_dynamic_cast(&dyn_cast_node, &cast_val) == llvm_gen_be_error_t::ok);
    dyn_cast_node.type = &basic_ty;
    assert(llvm_lower_dynamic_cast(&dyn_cast_node, &cast_val) == llvm_gen_be_error_t::ok);
    sub_op.type = &basic_ty;
    assert(llvm_lower_dynamic_cast(&dyn_cast_node, &cast_val) == llvm_gen_be_error_t::ok);
    dyn_cast_node.type = nullptr;
    assert(llvm_lower_dynamic_cast(&dyn_cast_node, &cast_val) == llvm_gen_be_error_t::ok);
    sub_op.type = nullptr;
    assert(llvm_lower_dynamic_cast(&dyn_cast_node, &cast_val) == llvm_gen_be_error_t::ok);

    // Case G: dynamic_cast<void*>(expr) on Itanium
    a_type void_dc_ty = {}; void_dc_ty.kind = tk_void;
    a_type void_ptr_dc_ty = {}; void_ptr_dc_ty.kind = tk_pointer; void_ptr_dc_ty.variant.pointer.type = &void_dc_ty;
    sub_op.type = &ptr_to_src;
    dyn_cast_node.type = &void_ptr_dc_ty;
    assert(llvm_lower_dynamic_cast(&dyn_cast_node, &cast_val) == llvm_gen_be_error_t::ok);

    // 4. llvm_lower_typeid: null type_with_expr, is_dynamic with next == nullptr, expr error, and MSVC __RTtypeid
    an_expr_node tid_empty = {};
    assert(llvm_lower_typeid(&tid_empty, &ti_val) == llvm_gen_be_error_t::invalid_argument);
    g_vr_fail_expr = 1;
    assert(llvm_lower_typeid(&typeid_node, &dyn_ti_val) == llvm_gen_be_error_t::unsupported_expr);
    g_vr_fail_expr = 0;

    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    assert(llvm_lower_typeid(&typeid_node, &dyn_ti_val) == llvm_gen_be_error_t::ok);
    type_op.next = nullptr; // is_dynamic == TRUE && type_with_expr->next == nullptr
    assert(llvm_lower_typeid(&typeid_node, &dyn_ti_val) == llvm_gen_be_error_t::ok);

    // 5. get_typeinfo_global reference unwrapping & type->typeinfo_var branches
    {
      llvm::Constant* ref_ti_out = nullptr;
      a_type ref_valid = {};
      ref_valid.kind = tk_pointer;
      ref_valid.variant.pointer.is_reference = TRUE;
      ref_valid.variant.pointer.type = &basic_ty;
      assert(get_typeinfo_global(&ref_valid, &ref_ti_out) == llvm_gen_be_error_t::ok);
      assert(ref_ti_out != nullptr);

      ref_valid.variant.pointer.type = nullptr;
      assert(get_typeinfo_global(&ref_valid, &ref_ti_out) == llvm_gen_be_error_t::ok);
      assert(ref_ti_out != nullptr && ref_ti_out->isNullValue());

      a_variable ti_var_test = {};
      a_type ty_with_tivar = {};
      ty_with_tivar.kind = tk_class;
      ty_with_tivar.typeinfo_var = &ti_var_test;
      ti_var_test.source_corresp.name = nullptr; // typeinfo_var != nullptr && name == nullptr
      assert(get_typeinfo_global(&ty_with_tivar, &ref_ti_out) == llvm_gen_be_error_t::ok);

      ti_var_test.source_corresp.name = "_ZTI_custom_tivar_missing";
      g_skip_ti_gv_creation = true; // lowered_ti == nullptr branch
      assert(get_typeinfo_global(&ty_with_tivar, &ref_ti_out) == llvm_gen_be_error_t::ok);
      g_skip_ti_gv_creation = false;

      ti_var_test.source_corresp.name = "_ZTI_custom_tivar_present";
      g_llvm_be_oom_countdown = 1; // step 0 passes get_typeinfo_global entry, step 1 fails inside llvm_lower_global_variable
      assert(get_typeinfo_global(&ty_with_tivar, &ref_ti_out) == llvm_gen_be_error_t::out_of_memory);
      g_llvm_be_oom_countdown = -1;
      assert(get_typeinfo_global(&ty_with_tivar, &ref_ti_out) == llvm_gen_be_error_t::ok);
      assert(ref_ti_out == state.module->getNamedGlobal("_ZTI_custom_tivar_present"));
    }

    // --- llvm_gen_be_vtable.cpp ---
    // 1. gather_vfuncs_for_class & llvm_vtable_builder_compute:
    //    null builder->class_type, tk_struct class_type, supp == nullptr, base with b->type == nullptr,
    //    base with error, assoc_scope with routines == nullptr, non-virtual routine, vnum <= 0, vnum > num_vfuncs,
    //    routine with r->type == nullptr, r->type non-FunctionType, r->type error, unnamed routine, mangled routine name,
    //    and thunk-Triggering routines (overriding_function_for_wrapper, delta, vcall_index, return_delta, vbase_index)
    llvm_vtable_builder_t empty_b = {};
    assert(llvm_vtable_builder_compute(&empty_b) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_vtable_builder_init(&empty_b, &cls_no_supp, 0, true) == llvm_gen_be_error_t::ok);
    assert(llvm_vtable_builder_compute(&empty_b) == llvm_gen_be_error_t::ok); // supp == nullptr

    a_type st_vt = {}; st_vt.kind = tk_struct;
    a_class_type_supplement supp_vt = {};
    st_vt.variant.class_struct_union.extra_info = &supp_vt;
    a_scope scope_empty_rt = {};
    supp_vt.assoc_scope = &scope_empty_rt; // routines == nullptr
    assert(llvm_vtable_builder_init(&empty_b, &st_vt, 0, true) == llvm_gen_be_error_t::ok);
    assert(llvm_vtable_builder_compute(&empty_b) == llvm_gen_be_error_t::ok);

    a_routine r_nonvirt = {}, r_v0 = {}, r_vbig = {}, r_untyped = {}, r_nonfn_ty = {}, r_wrap = {}, r_delta = {}, r_vcall = {}, r_rdelta = {}, r_vbase = {};
    r_nonvirt.is_virtual = FALSE; r_nonvirt.next = &r_v0;
    r_v0.is_virtual = TRUE; r_v0.number.virtual_function = 0; r_v0.next = &r_vbig;
    r_vbig.is_virtual = TRUE; r_vbig.number.virtual_function = 99; r_vbig.next = &r_untyped;
    r_untyped.is_virtual = TRUE; r_untyped.number.virtual_function = 1; r_untyped.type = nullptr; r_untyped.source_corresp.name = nullptr; r_untyped.next = &r_nonfn_ty;
    r_nonfn_ty.is_virtual = TRUE; r_nonfn_ty.number.virtual_function = 2; r_nonfn_ty.type = &basic_ty; r_nonfn_ty.source_corresp.name = (char*)"v2"; r_nonfn_ty.next = &r_wrap;
    r_wrap.is_virtual = TRUE; r_wrap.number.virtual_function = 3; r_wrap.overriding_function_for_wrapper = &r_nonfn_ty; r_wrap.next = &r_delta;
    r_delta.is_virtual = TRUE; r_delta.number.virtual_function = 4; r_delta.delta = 8; r_delta.next = &r_vcall;
    r_vcall.is_virtual = TRUE; r_vcall.number.virtual_function = 5; r_vcall.vcall_index = 1; r_vcall.next = &r_rdelta;
    r_rdelta.is_virtual = TRUE; r_rdelta.number.virtual_function = 6; r_rdelta.return_delta = -8; r_rdelta.next = &r_vbase;
    r_vbase.is_virtual = TRUE; r_vbase.number.virtual_function = 7; r_vbase.vbase_index = 2; r_vbase.next = nullptr;

    scope_empty_rt.routines = &r_nonvirt;
    supp_vt.highest_virtual_function_number = 7;
    a_base_class vt_b_null = {}; vt_b_null.type = nullptr;
    supp_vt.direct_base_classes = &vt_b_null;
    g_mangled_fn_name = (char*)"_ZN_mangled_vfunc";
    assert(llvm_vtable_builder_init(&empty_b, &st_vt, 0, true) == llvm_gen_be_error_t::ok);
    assert(llvm_vtable_builder_compute(&empty_b) == llvm_gen_be_error_t::ok);
    g_mangled_fn_name = nullptr;
    assert(llvm_vtable_builder_compute(&empty_b) == llvm_gen_be_error_t::ok);

    // Test get_llvm_type error in gather_vfuncs_for_class (both direct and via base class)
    a_type bad_rt_ty = {}; bad_rt_ty.kind = tk_routine; bad_rt_ty.alignment = 255;
    r_nonfn_ty.type = &bad_rt_ty;
    assert(llvm_vtable_builder_compute(&empty_b) == llvm_gen_be_error_t::unsupported_type);
    a_type child_vt = {}; child_vt.kind = tk_class;
    a_class_type_supplement child_supp = {};
    a_base_class child_db = {}; child_db.type = &st_vt;
    child_supp.direct_base_classes = &child_db;
    child_supp.highest_virtual_function_number = 7;
    child_vt.variant.class_struct_union.extra_info = &child_supp;
    assert(llvm_vtable_builder_init(&empty_b, &child_vt, 0, true) == llvm_gen_be_error_t::ok);
    assert(llvm_vtable_builder_compute(&empty_b) == llvm_gen_be_error_t::unsupported_type);
    r_nonfn_ty.type = &basic_ty;

    // 2. llvm_emit_thunk: existing function cache hit, is_virtual == true && vcall_offset_index == -1,
    //    this_offset == 0, return_offset == 0 && return_vbase_offset_index != -1, target_routine with null name
    assert(llvm_emit_thunk(&thunk, &thunk_fn) == llvm_gen_be_error_t::ok); // cache hit!
    llvm_thunk_info_t t_edge = {};
    t_edge.is_virtual = true;
    t_edge.vcall_offset_index = -1;
    t_edge.this_offset = 0;
    t_edge.return_offset = 0;
    t_edge.return_vbase_offset_index = 1;
    a_routine unnamed_target = {}; unnamed_target.source_corresp.name = nullptr;
    t_edge.target_routine = &unnamed_target;
    assert(llvm_emit_thunk(&t_edge, &thunk_fn) == llvm_gen_be_error_t::ok);

    // Thunk with multi-parameter target routine (size == 1), variadic void-returning target routine (size == 2),
    // 0-param routine (size == 3), non-pointer param0 routine (size == 4), non-function type, and failing get_llvm_type
    a_type rt_multi = {}, rt_vararg_void = {}, rt_0param = {}, rt_nonptr0 = {}, rt_fail_ty = {};
    rt_multi.kind = tk_routine; rt_multi.size = 1;
    rt_vararg_void.kind = tk_routine; rt_vararg_void.size = 2;
    rt_0param.kind = tk_routine; rt_0param.size = 3;
    rt_nonptr0.kind = tk_routine; rt_nonptr0.size = 4;
    rt_fail_ty.kind = tk_routine; rt_fail_ty.alignment = 255;

    a_routine tgt_multi = {}, tgt_vararg = {}, tgt_0param = {}, tgt_nonptr0 = {}, tgt_nonfn = {}, tgt_fail = {};
    tgt_multi.source_corresp.name = (char*)"fn_multi"; tgt_multi.type = &rt_multi;
    tgt_vararg.source_corresp.name = (char*)"fn_vararg"; tgt_vararg.type = &rt_vararg_void;
    tgt_0param.source_corresp.name = (char*)"fn_0param"; tgt_0param.type = &rt_0param;
    tgt_nonptr0.source_corresp.name = (char*)"fn_nonptr0"; tgt_nonptr0.type = &rt_nonptr0;
    tgt_nonfn.source_corresp.name = (char*)"fn_nonfn"; tgt_nonfn.type = &basic_ty;
    tgt_fail.source_corresp.name = (char*)"fn_fail"; tgt_fail.type = &rt_fail_ty;

    llvm_thunk_info_t t_multi = {};
    t_multi.this_offset = 8;
    t_multi.return_offset = 8;
    t_multi.return_vbase_offset_index = -1;
    t_multi.target_routine = &tgt_multi;
    assert(llvm_emit_thunk(&t_multi, &thunk_fn) == llvm_gen_be_error_t::ok);
    assert(thunk_fn->arg_size() == 2);

    t_multi.target_routine = &tgt_vararg;
    assert(llvm_emit_thunk(&t_multi, &thunk_fn) == llvm_gen_be_error_t::ok);
    assert(thunk_fn->isVarArg());

    t_multi.target_routine = &tgt_0param;
    assert(llvm_emit_thunk(&t_multi, &thunk_fn) == llvm_gen_be_error_t::ok);

    t_multi.target_routine = &tgt_nonptr0;
    assert(llvm_emit_thunk(&t_multi, &thunk_fn) == llvm_gen_be_error_t::ok);

    t_multi.target_routine = &tgt_nonfn;
    assert(llvm_emit_thunk(&t_multi, &thunk_fn) == llvm_gen_be_error_t::ok);

    t_multi.target_routine = &tgt_fail;
    assert(llvm_emit_thunk(&t_multi, &thunk_fn) == llvm_gen_be_error_t::ok);

    // 3. llvm_emit_msvc_vbtable & emit_sub_vbtable:
    //    tk_struct, cached vbtable, supp == nullptr, subobject vbtable (with mangled == nullptr and != nullptr,
    //    subobj extra_info == nullptr, non-matching virtual base in complete_supp)
    assert(llvm_emit_msvc_vbtable(&class_ty, &vbtable) == llvm_gen_be_error_t::ok); // cache hit!
    g_use_custom_ti_name = 1;
    g_mangled_ti_name = nullptr;
    assert(llvm_emit_msvc_vbtable(&cls_no_supp, &vbtable) == llvm_gen_be_error_t::ok); // supp == nullptr, mangled == nullptr

    a_type sub_vb_cls = {}; sub_vb_cls.kind = tk_struct;
    sub_vb_cls.variant.class_struct_union.any_virtual_base_classes = TRUE;
    a_class_type_supplement sub_vb_supp = {};
    a_base_class sub_vb_entry = {}; sub_vb_entry.is_virtual = TRUE; sub_vb_entry.type = &st_unrel; // not in complete_supp!
    sub_vb_supp.base_classes = &sub_vb_entry;
    sub_vb_cls.variant.class_struct_union.extra_info = &sub_vb_supp;

    a_type comp_vb_cls = {}; comp_vb_cls.kind = tk_struct;
    comp_vb_cls.variant.class_struct_union.any_virtual_base_classes = TRUE;
    a_class_type_supplement comp_vb_supp = {};
    a_base_class cb1 = {}, cb2 = {};
    cb1.is_virtual = FALSE; cb1.type = &sub_vb_cls; cb1.offset = 8; cb1.next = &cb2;
    cb2.is_virtual = TRUE; cb2.type = &basic_ty; cb2.offset = 24; cb2.next = nullptr;
    comp_vb_supp.base_classes = &cb1;
    comp_vb_cls.variant.class_struct_union.extra_info = &comp_vb_supp;

    g_mangled_ti_name = (char*)"CompVB";
    assert(llvm_emit_msvc_vbtable(&comp_vb_cls, &vbtable) == llvm_gen_be_error_t::ok);
    g_use_custom_ti_name = 0;

    // 4. llvm_emit_vtable (MSVC and Itanium):
    //    cached MSVC vftable, MSVC vftable with key_func (function_def_number == 0 and != 0), any_virtual_base_classes == TRUE,
    //    and error in llvm_vtable_builder_compute;
    //    Itanium vtable with cached global, mangled_vtbl_name == nullptr, key_func, secondary vtable with virtual methods and error
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    assert(llvm_emit_vtable(&class_ty, &msvc_vftable) == llvm_gen_be_error_t::ok); // cached hit!

    a_routine kf = {}; kf.function_def_number = 0;
    g_key_func = &kf;
    g_use_custom_ti_name = 1;
    g_mangled_ti_name = nullptr;
    a_base_class cb_zero = {};
    cb2.next = &cb_zero;
    cb_zero.is_virtual = FALSE; cb_zero.offset = 0; cb_zero.type = &sub_vb_cls; cb_zero.next = nullptr;
    assert(llvm_emit_vtable(&comp_vb_cls, &msvc_vftable) == llvm_gen_be_error_t::ok);
    cb2.next = nullptr;

    kf.function_def_number = 1;
    g_mangled_ti_name = (char*)"CompVB_ext";
    // Pre-create secondary vftable to cover existing getNamedValue branch
    llvm::GlobalVariable* pre_vftbl = llvm::cast<llvm::GlobalVariable>(
        state.module->getOrInsertGlobal("??_7CompVB_ext6BCompVB_ext@", llvm::Type::getInt32Ty(*state.context)));
    pre_vftbl->setConstant(true);
    pre_vftbl->setLinkage(llvm::GlobalValue::ExternalLinkage);
    pre_vftbl->setInitializer(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 0));
    assert(pre_vftbl != nullptr);
    assert(llvm_emit_vtable(&comp_vb_cls, &msvc_vftable) == llvm_gen_be_error_t::ok);

    g_mangled_ti_name = (char*)"NoSupp_msvc";
    cls_no_supp.variant.class_struct_union.any_virtual_base_classes = FALSE;
    assert(llvm_emit_vtable(&cls_no_supp, &msvc_vftable) == llvm_gen_be_error_t::ok);
    cls_no_supp.variant.class_struct_union.any_virtual_base_classes = TRUE;

    g_mangled_ti_name = (char*)"CompVB_err";
    g_vr_fail_type = 1;
    assert(llvm_emit_vtable(&st_vt, &msvc_vftable) == llvm_gen_be_error_t::unsupported_type);
    g_vr_fail_type = 0;
    g_use_custom_ti_name = 0;

    // Itanium vtable
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
    assert(llvm_emit_vtable(&class_ty, &mi_vtable) == llvm_gen_be_error_t::ok); // cached "_ZTVmock" hit!
    g_mangled_vtbl_override = nullptr; // "_ZTV_unknown"
    // Give st_vt a secondary base that has virtual functions, a virtual secondary base with more slots than st_vt,
    // a primary base, an offset==0 base, a null-type base, and an invalid-type base
    a_type wide_sec_cls = {}; wide_sec_cls.kind = tk_class;
    a_class_type_supplement wide_sec_supp = {};
    wide_sec_supp.highest_virtual_function_number = 9;
    wide_sec_cls.variant.class_struct_union.extra_info = &wide_sec_supp;
    supp_vt.highest_virtual_function_number = 8; // slot 7 is ConstantPointerNull, and wide_sec_cls has 9 slots (> 8)!

    a_base_class sec_b1 = {}, sec_b_wide = {}, sec_b2 = {}, sec_b3 = {}, sec_b4 = {};
    sec_b1.type = &class_ty; sec_b1.offset = 8; sec_b1.next = &sec_b_wide;
    sec_b_wide.type = &wide_sec_cls; sec_b_wide.offset = 12; sec_b_wide.is_virtual = TRUE; sec_b_wide.next = &sec_b2;
    sec_b2.type = &basic_ty; sec_b2.offset = 16; sec_b2.next = &sec_b3; // compute fails on basic_ty!
    sec_b3.type = nullptr; sec_b3.offset = 24; sec_b3.next = &sec_b4;
    sec_b4.type = &class_ty; sec_b4.offset = 0; sec_b4.next = nullptr;
    supp_vt.base_classes = &sec_b1;
    supp_vt.virtual_function_info_base_class = &sec_b4;
    kf.function_def_number = 0;
    assert(llvm_emit_vtable(&st_vt, &mi_vtable) == llvm_gen_be_error_t::ok);
    supp_vt.highest_virtual_function_number = 7;

    g_mangled_vtbl_override = (char*)"_ZTV_kf_def";
    kf.function_def_number = 1;
    assert(llvm_emit_vtable(&cls_no_supp, &mi_vtable) == llvm_gen_be_error_t::ok);

    g_mangled_vtbl_override = (char*)"_ZTV_err";
    g_vr_fail_type = 1;
    assert(llvm_emit_vtable(&st_vt, &mi_vtable) == llvm_gen_be_error_t::unsupported_type);
    g_vr_fail_type = 0;
    g_key_func = nullptr;

    // 5. llvm_emit_vtt & llvm_emit_construction_vtable:
    //    cached VTT, m_vtt == nullptr, primary vtable failure, supp == nullptr,
    //    polymorphic non-primary base in direct_base_classes and base_classes (both already_added and new),
    //    and construction vtable failing when b->type is invalid!
    assert(llvm_emit_vtt(&class_ty, &vtt) == llvm_gen_be_error_t::ok); // cached "_ZTTmock" hit!
    g_mangled_vtt_override = nullptr; // "_ZTT_unknown"
    g_mangled_vtbl_override = (char*)"_ZTV_vtt_fail";
    g_vr_fail_type = 1;
    assert(llvm_emit_vtt(&st_vt, &vtt) == llvm_gen_be_error_t::vtable_generation_failure);
    g_vr_fail_type = 0;

    assert(llvm_emit_vtt(&cls_no_supp, &vtt) == llvm_gen_be_error_t::ok); // supp == nullptr

    g_mangled_vtt_override = (char*)"_ZTT_poly_vtt";
    g_mangled_vtbl_override = (char*)"_ZTV_poly_vtt";
    a_type poly_base1 = {}, poly_base2 = {};
    poly_base1.kind = tk_class; poly_base1.source_corresp.name = (char*)"PB1";
    poly_base1.variant.class_struct_union.any_virtual_functions_including_in_base_classes = TRUE;
    a_class_type_supplement pb1_supp = {};
    a_base_class pb1_vb = {}; pb1_vb.is_virtual = TRUE; pb1_vb.type = &cls_no_supp; pb1_vb.offset = 8;
    pb1_supp.direct_base_classes = &pb1_vb;
    poly_base1.variant.class_struct_union.extra_info = &pb1_supp;

    poly_base2.kind = tk_class; poly_base2.source_corresp.name = (char*)"PB2";
    poly_base2.variant.class_struct_union.any_virtual_functions_including_in_base_classes = TRUE;

    a_type vtt_cls = {}; vtt_cls.kind = tk_struct; vtt_cls.source_corresp.name = (char*)"VttCls";
    a_class_type_supplement vtt_supp = {};
    a_base_class vd1 = {}, vd2 = {}, vd3 = {}, vd4 = {};
    vd1.type = &poly_base1; vd1.offset = 8; vd1.next_direct = &vd2;
    vd2.type = &basic_ty; vd2.is_virtual = TRUE; vd2.next_direct = &vd3; // construction vtable fails on basic_ty!
    vd3.type = nullptr; vd3.is_virtual = TRUE; vd3.next_direct = &vd4;
    vd4.type = &poly_base1; vd4.next_direct = nullptr;
    vtt_supp.virtual_function_info_base_class = &vd4; // vd4 is primary!
    vtt_supp.direct_base_classes = &vd1;

    a_base_class vb_list1 = {}, vb_list2 = {}, vb_list3 = {}, vb_list4 = {};
    vb_list1.type = &poly_base1; vb_list1.offset = 8; vb_list1.next = &vb_list2; // already_added == true!
    vb_list2.type = &poly_base2; vb_list2.offset = 16; vb_list2.next = &vb_list3; // already_added == false!
    vb_list3.type = &basic_ty; vb_list3.is_virtual = TRUE; vb_list3.next = &vb_list4; // c_err != ok!
    vb_list4.type = &cls_no_supp; vb_list4.is_virtual = FALSE; vb_list4.next = nullptr; // !is_virtual && !is_poly_sub!
    vtt_supp.base_classes = &vb_list1;
    vtt_cls.variant.class_struct_union.extra_info = &vtt_supp;
    assert(llvm_emit_vtt(&vtt_cls, &vtt) == llvm_gen_be_error_t::ok);

    // llvm_emit_construction_vtable with der_supp == nullptr, m_der/m_base == nullptr, and direct_base_classes offset match
    g_use_custom_ti_name = 1;
    g_mangled_ti_name = nullptr;
    assert(llvm_emit_construction_vtable(&cls_no_supp, &poly_base1, &cvtable) == llvm_gen_be_error_t::ok);
    g_use_custom_ti_name = 0;

    vtt_supp.base_classes = nullptr; // forces search in direct_base_classes!
    poly_base2.source_corresp.name = (char*)"PB2_direct";
    vd1.type = &poly_base2;
    assert(llvm_emit_construction_vtable(&vtt_cls, &poly_base2, &cvtable) == llvm_gen_be_error_t::ok);

    // 6. llvm_lower_virtual_dispatch & llvm_lower_msvc_virtual_dispatch with virtual_function == 0 and vcall_index != 0
    routine.number.virtual_function = 0;
    assert(llvm_lower_virtual_dispatch(obj, &routine, &func_ptr) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_msvc_virtual_dispatch(obj, &routine, &msvc_func_ptr) == llvm_gen_be_error_t::ok);
    routine.vcall_index = -3;
    assert(llvm_lower_virtual_dispatch(obj, &routine, &func_ptr) == llvm_gen_be_error_t::ok);
    routine.vcall_index = 1;
    assert(llvm_lower_msvc_virtual_dispatch(obj, &routine, &msvc_func_ptr) == llvm_gen_be_error_t::ok);
    routine.vcall_index = 0;

    // 7. Diamond inheritance construction vtable: base_type found in der_supp->base_classes, and relative vbase_offset adjustment
    a_type vbase_shared = {};
    vbase_shared.kind = tk_class;
    vbase_shared.source_corresp.name = (char*)"VShared";
    a_base_class d_vb_entry = {};
    d_vb_entry.is_virtual = TRUE;
    d_vb_entry.type = &vbase_shared;
    d_vb_entry.offset = 32;

    a_base_class b_nvb_entry = {};
    b_nvb_entry.is_virtual = FALSE;
    b_nvb_entry.type = &vbase_shared;
    b_nvb_entry.offset = 0;

    a_base_class b_vb_entry = {};
    b_vb_entry.is_virtual = TRUE;
    b_vb_entry.type = &vbase_shared;
    b_vb_entry.offset = 16;
    b_vb_entry.next_direct = &b_nvb_entry;

    a_type mid_base = {};
    mid_base.kind = tk_class;
    mid_base.source_corresp.name = (char*)"MidBase";
    a_class_type_supplement mid_supp = {};
    mid_supp.direct_base_classes = &b_vb_entry;
    mid_supp.highest_virtual_function_number = 1;
    mid_base.variant.class_struct_union.extra_info = &mid_supp;

    a_base_class d_mid_entry = {};
    d_mid_entry.is_virtual = FALSE;
    d_mid_entry.type = &mid_base;
    d_mid_entry.offset = 8;
    d_mid_entry.next = &d_vb_entry;

    a_type diamond_der = {};
    diamond_der.kind = tk_class;
    diamond_der.source_corresp.name = (char*)"DiamondDer";
    a_class_type_supplement diamond_supp = {};
    diamond_supp.base_classes = &d_mid_entry;
    diamond_der.variant.class_struct_union.extra_info = &diamond_supp;

    assert(llvm_emit_construction_vtable(&diamond_der, &mid_base, &cvtable) == llvm_gen_be_error_t::ok);
    assert(cvtable != nullptr);

    // Also test llvm_emit_vtt on diamond_der (which has an indirect virtual base in base_classes AND a direct virtual base of a different type)
    a_type vbase_other = {};
    vbase_other.kind = tk_class;
    vbase_other.source_corresp.name = (char*)"VOther";
    a_base_class d_direct_vb = {};
    d_direct_vb.is_virtual = TRUE;
    d_direct_vb.type = &vbase_other;
    d_direct_vb.offset = 24;
    diamond_supp.direct_base_classes = &d_direct_vb;
    g_mangled_vtt_override = (char*)"_ZTT_diamond_der";
    g_mangled_vtbl_override = (char*)"_ZTV_diamond_der";
    assert(llvm_emit_vtt(&diamond_der, &vtt) == llvm_gen_be_error_t::ok);

    // Phase 9: Test Mach-O (no COMDAT attached) and COFF (Comdat::Largest) vtable/VTT/ctor_vtable/RTTI emission
    llvm::Constant* vtbl_p9 = nullptr;
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "aarch64-apple-darwin");
    g_mangled_vtbl_override = (char*)"_ZTV_macho_cls";
    g_mangled_vtt_override = (char*)"_ZTT_macho_cls";
    assert(llvm_emit_vtable(&diamond_der, &vtbl_p9) == llvm_gen_be_error_t::ok);
    assert(llvm_emit_vtt(&diamond_der, &vtt) == llvm_gen_be_error_t::ok);
    a_base_class macho_mid_base = d_mid_entry;
    macho_mid_base.offset = 32;
    assert(llvm_emit_construction_vtable(&diamond_der, &macho_mid_base, &cvtable) == llvm_gen_be_error_t::ok);
    a_type macho_rtti_ty = {};
    macho_rtti_ty.kind = tk_class;
    macho_rtti_ty.source_corresp.name = (char*)"MachoRttiCls";
    llvm::Constant* macho_ti = nullptr;
    assert(get_typeinfo_global(&macho_rtti_ty, &macho_ti) == llvm_gen_be_error_t::ok);

    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-w64-windows-gnu");
    g_mangled_vtbl_override = (char*)"_ZTV_coff_cls";
    assert(llvm_emit_vtable(&diamond_der, &vtbl_p9) == llvm_gen_be_error_t::ok);
    a_type coff_rtti_ty = {};
    coff_rtti_ty.kind = tk_class;
    coff_rtti_ty.source_corresp.name = (char*)"CoffRttiCls";
    llvm::Constant* coff_ti = nullptr;
    assert(get_typeinfo_global(&coff_rtti_ty, &coff_ti) == llvm_gen_be_error_t::ok);
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");

    // Phase 4: Test llvm_emit_msvc_rtti_type_descriptor and llvm_emit_msvc_vtordisp_thunk
    llvm::GlobalVariable* r0_gv = nullptr;
    assert(llvm_emit_msvc_rtti_type_descriptor(nullptr, &r0_gv) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_emit_msvc_rtti_type_descriptor(&coff_rtti_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);

    g_use_custom_ti_name = 1;
    g_mangled_ti_name = (char*)".?AVMsvcR0Cls@@";
    // First call when ??_7type_info@@6B@ might or might not exist; remove it first to exercise !ti_vtbl branch
    if (llvm::GlobalVariable* old_ti_vtbl = state.module->getNamedGlobal("??_7type_info@@6B@")) {
      old_ti_vtbl->eraseFromParent();
    }
    assert(llvm_emit_msvc_rtti_type_descriptor(&coff_rtti_ty, &r0_gv) == llvm_gen_be_error_t::ok);
    assert(r0_gv != nullptr);
    // Second call hits existing cached R0 global
    assert(llvm_emit_msvc_rtti_type_descriptor(&coff_rtti_ty, &r0_gv) == llvm_gen_be_error_t::ok);

    g_mangled_ti_name = (char*)"PlainR0Cls";
    assert(llvm_emit_msvc_rtti_type_descriptor(&coff_rtti_ty, &r0_gv) == llvm_gen_be_error_t::ok);
    g_mangled_ti_name = nullptr;
    assert(llvm_emit_msvc_rtti_type_descriptor(&coff_rtti_ty, &r0_gv) == llvm_gen_be_error_t::ok);
    g_use_custom_ti_name = 0;

    llvm::Function* vtordisp_fn = nullptr;
    assert(llvm_emit_msvc_vtordisp_thunk(nullptr, 4, 8, &vtordisp_fn) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_emit_msvc_vtordisp_thunk(&routine, 4, 8, nullptr) == llvm_gen_be_error_t::invalid_argument);
    routine.source_corresp.name = (char*)"?vfunc@Derived@@UEAAXXZ";
    assert(llvm_emit_msvc_vtordisp_thunk(&routine, 4, 8, &vtordisp_fn) == llvm_gen_be_error_t::ok);
    assert(vtordisp_fn != nullptr);
    // Cache hit on second call
    assert(llvm_emit_msvc_vtordisp_thunk(&routine, 4, 8, &vtordisp_fn) == llvm_gen_be_error_t::ok);
    // Unnamed target routine
    assert(llvm_emit_msvc_vtordisp_thunk(&unnamed_target, 4, 0, &vtordisp_fn) == llvm_gen_be_error_t::ok);

    // Phase 7: ck_address (abk_routine and abk_field) in llvm_const_from_ptr_to_member
    a_constant addr_pm_con = {};
    addr_pm_con.kind = ck_address;
    addr_pm_con.variant.address.kind = abk_routine;
    addr_pm_con.variant.address.variant.routine = &routine;
    addr_pm_con.variant.address.offset = 16;
    llvm::StructType* pm2_st = llvm::StructType::get(*state.context, {llvm_ptr_ty, i64_ty});
    llvm::StructType* pm3_st = llvm::StructType::get(*state.context, {llvm_ptr_ty, i32_ty, i32_ty});
    llvm::Constant* pm_addr_out = nullptr;
    assert(llvm_const_from_ptr_to_member(&addr_pm_con, pm2_st, &pm_addr_out) == llvm_gen_be_error_t::ok);
    assert(llvm_const_from_ptr_to_member(&addr_pm_con, pm3_st, &pm_addr_out) == llvm_gen_be_error_t::ok);
    assert(llvm_const_from_ptr_to_member(&addr_pm_con, llvm_ptr_ty, &pm_addr_out) == llvm_gen_be_error_t::ok);
    assert(llvm_const_from_ptr_to_member(&addr_pm_con, nullptr, &pm_addr_out) == llvm_gen_be_error_t::ok);
    addr_pm_con.variant.address.variant.routine = &unnamed_target;
    assert(llvm_const_from_ptr_to_member(&addr_pm_con, pm2_st, &pm_addr_out) == llvm_gen_be_error_t::ok);
    addr_pm_con.variant.address.variant.routine = nullptr;
    assert(llvm_const_from_ptr_to_member(&addr_pm_con, pm2_st, &pm_addr_out) == llvm_gen_be_error_t::ok);

    a_field pm_addr_fld = {};
    pm_addr_fld.offset = 12;
    a_subobject_path pm_subpath = {};
    pm_subpath.is_offset = FALSE;
    pm_subpath.is_base_class = FALSE;
    pm_subpath.variant.field = &pm_addr_fld;
    addr_pm_con.variant.address.kind = abk_variable;
    addr_pm_con.variant.address.subobject_path = &pm_subpath;
    addr_pm_con.variant.address.offset = 4;
    assert(llvm_const_from_ptr_to_member(&addr_pm_con, i64_ty, &pm_addr_out) == llvm_gen_be_error_t::ok);
    assert(llvm_const_from_ptr_to_member(&addr_pm_con, nullptr, &pm_addr_out) == llvm_gen_be_error_t::ok);
    pm_subpath.variant.field = nullptr;
    assert(llvm_const_from_ptr_to_member(&addr_pm_con, i64_ty, &pm_addr_out) == llvm_gen_be_error_t::ok);
    pm_subpath.is_base_class = TRUE;
    assert(llvm_const_from_ptr_to_member(&addr_pm_con, i64_ty, &pm_addr_out) == llvm_gen_be_error_t::ok);
    pm_subpath.is_offset = TRUE;
    assert(llvm_const_from_ptr_to_member(&addr_pm_con, i64_ty, &pm_addr_out) == llvm_gen_be_error_t::ok);
    addr_pm_con.variant.address.subobject_path = nullptr;
    assert(llvm_const_from_ptr_to_member(&addr_pm_con, i64_ty, &pm_addr_out) == llvm_gen_be_error_t::ok);

    // Phase 10: Complete branch & OOM coverage across ptrmem.cpp, rtti.cpp, and vtable.cpp
    {
      // 1) ptrmem.cpp: void return type in llvm_lower_pm_call (call + invoke) & get_llvm_type failure
      a_type pm_void_ty = {};
      pm_void_ty.kind = tk_void;
      an_expr_node pm_obj_e = {}, pm_fn_e = {}, pm_call_e = {};
      pm_obj_e.next = &pm_fn_e;
      pm_fn_e.next = nullptr;
      pm_call_e.kind = enk_operation;
      pm_call_e.type = &pm_void_ty;
      pm_call_e.variant.operation.kind = eok_dot_pm_call;
      pm_call_e.variant.operation.operands = &pm_obj_e;
      llvm::Value* pm_v_out = nullptr;

      g_vr_expr_mode = 1;
      g_vr_fail_type = 1;
      assert(llvm_lower_pm_call(&pm_call_e, &pm_v_out) == llvm_gen_be_error_t::unsupported_type);
      g_vr_fail_type = 0;

      assert(llvm_lower_pm_call(&pm_call_e, &pm_v_out) == llvm_gen_be_error_t::ok);
      llvm::Function* cur_vr_fn = state.builder->GetInsertBlock()->getParent();
      llvm::BasicBlock* dummy_lpad = llvm::BasicBlock::Create(*state.context, "pm.lpad", cur_vr_fn);
      state.current_landing_pads.push_back(dummy_lpad);
      assert(llvm_lower_pm_call(&pm_call_e, &pm_v_out) == llvm_gen_be_error_t::ok);
      state.current_landing_pads.pop_back();

      // 2) ptrmem.cpp: ARM/AArch64/Thumb PMF cast + indirect base OOM sweeps
      a_type pm_root = {}, pm_mid = {}, pm_leaf = {};
      pm_root.kind = tk_class; pm_mid.kind = tk_class; pm_leaf.kind = tk_class;
      a_base_class bc_mid_root = {}, bc_leaf_mid = {};
      bc_mid_root.type = &pm_root; bc_mid_root.offset = 8;
      bc_leaf_mid.type = &pm_mid; bc_leaf_mid.offset = 16;
      a_class_type_supplement supp_mid_pm = {}, supp_leaf_pm = {};
      supp_mid_pm.direct_base_classes = &bc_mid_root;
      supp_leaf_pm.direct_base_classes = &bc_leaf_mid;
      pm_mid.variant.class_struct_union.extra_info = &supp_mid_pm;
      pm_leaf.variant.class_struct_union.extra_info = &supp_leaf_pm;

      a_type pm_src_ty = {}, pm_dst_ty = {};
      pm_src_ty.kind = tk_ptr_to_member; pm_src_ty.variant.ptr_to_member.class_of_which_a_member = &pm_root;
      pm_dst_ty.kind = tk_ptr_to_member; pm_dst_ty.variant.ptr_to_member.class_of_which_a_member = &pm_leaf;
      an_expr_node pm_cast_sub = {}, pm_cast_e = {};
      pm_cast_sub.type = &pm_src_ty;
      pm_cast_e.kind = enk_operation;
      pm_cast_e.type = &pm_dst_ty;
      pm_cast_e.variant.operation.kind = eok_pm_derived_class_cast;
      pm_cast_e.variant.operation.operands = &pm_cast_sub;

      const char* pm_triples[] = {
          "aarch64-unknown-linux-gnu",
          "armv7-unknown-linux-gnueabihf",
          "thumbv7-unknown-linux-gnueabihf",
          "x86_64-unknown-linux-gnu",
      };
      for (const char* tr : pm_triples) {
        EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), tr);
        assert(llvm_lower_pm_cast(&pm_cast_e, &pm_v_out) == llvm_gen_be_error_t::ok);
      }
      for (int b = 0; b < 8; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t rpm = llvm_lower_pm_cast(&pm_cast_e, &pm_v_out);
        g_llvm_be_oom_countdown = -1;
        if (rpm == llvm_gen_be_error_t::ok) break;
      }
      // Also sweep reverse direction (s_cls = leaf, d_cls = root) so first find_base_class_offset finds it or fails
      pm_cast_sub.type = &pm_dst_ty;
      pm_cast_e.type = &pm_src_ty;
      for (int b = 0; b < 8; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t rpm = llvm_lower_pm_cast(&pm_cast_e, &pm_v_out);
        g_llvm_be_oom_countdown = -1;
        if (rpm == llvm_gen_be_error_t::ok) break;
      }
      g_vr_expr_mode = 0;

      // 3) vtable.cpp: sret virtual thunk (size == 5), construction vtable name branches, VTT address-point variants
      a_type rt_sret = {}; rt_sret.kind = tk_routine; rt_sret.size = 5;
      a_routine tgt_sret = {}; tgt_sret.source_corresp.name = (char*)"fn_sret_thunk"; tgt_sret.type = &rt_sret;
      llvm_thunk_info_t t_sret = {};
      t_sret.this_offset = 8;
      t_sret.return_vbase_offset_index = -1;
      t_sret.target_routine = &tgt_sret;
      assert(llvm_emit_thunk(&t_sret, &thunk_fn) == llvm_gen_be_error_t::ok);

      g_use_override_for_bcp = 1;
      g_mangled_vtbl_override = (char*)"_ZTCRealCtorVtbl";
      assert(llvm_emit_construction_vtable(&diamond_der, &macho_mid_base, &cvtable) == llvm_gen_be_error_t::ok);
      g_mangled_vtbl_override = (char*)"PlainNonZTC";
      macho_mid_base.offset = 36;
      assert(llvm_emit_construction_vtable(&diamond_der, &macho_mid_base, &cvtable) == llvm_gen_be_error_t::ok);

      g_mangled_vtbl_override = nullptr;
      g_use_custom_ti_name = 1;
      char* ti_cases[] = {nullptr, (char*)"", (char*)"_ZTI", (char*)"PlainName"};
      for (int idx = 0; idx < 4; ++idx) {
        g_mangled_ti_name = ti_cases[idx];
        macho_mid_base.offset = static_cast<a_targ_size_t>(40 + idx * 8);
        assert(llvm_emit_construction_vtable(&diamond_der, &macho_mid_base, &cvtable) == llvm_gen_be_error_t::ok);
      }
      g_use_custom_ti_name = 0;
      g_use_override_for_bcp = 0;

      // VTT compute_vtable_address_point with non-GlobalVariable (MSVC GEP ConstantExpr), ArrayType GlobalVariable, and Int64 GlobalVariable
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
      g_mangled_vtt_override = (char*)"_ZTT_msvc_gep_vtbl";
      assert(llvm_emit_vtt(&diamond_der, &vtt) == llvm_gen_be_error_t::ok);
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");

      llvm::ArrayType* arr4_ptr_ty = llvm::ArrayType::get(llvm_ptr_ty, 4);
      llvm::GlobalVariable* arr_vtbl_gv = nullptr;
      assert(llvm_create_global_variable(
          state.module.get(), arr4_ptr_ty, true, llvm::GlobalValue::ExternalLinkage,
          llvm::Constant::getNullValue(arr4_ptr_ty), "_ZTV_arr_vtbl", &arr_vtbl_gv) == llvm_gen_be_error_t::ok);
      g_mangled_vtbl_override = (char*)"_ZTV_arr_vtbl";
      g_mangled_vtt_override = (char*)"_ZTT_arr_vtbl";
      assert(llvm_emit_vtt(&diamond_der, &vtt) == llvm_gen_be_error_t::ok);

      llvm::GlobalVariable* int_vtbl_gv = nullptr;
      assert(llvm_create_global_variable(
          state.module.get(), i64_ty, true, llvm::GlobalValue::ExternalLinkage,
          llvm::ConstantInt::get(i64_ty, 0), "_ZTV_int_vtbl", &int_vtbl_gv) == llvm_gen_be_error_t::ok);
      g_mangled_vtbl_override = (char*)"_ZTV_int_vtbl";
      g_mangled_vtt_override = (char*)"_ZTT_int_vtbl";
      assert(llvm_emit_vtt(&diamond_der, &vtt) == llvm_gen_be_error_t::ok);

      // 4) OOM sweeps across rtti.cpp and vtable.cpp using fresh Modules so caches/globals never short-circuit
      a_type rtti_si = {}, rtti_ptr_t = {}, rtti_ptm_t = {};
      rtti_si.kind = tk_class; rtti_si.source_corresp.name = (char*)"_ZTI_oom_si";
      a_base_class si_bc = {}; si_bc.type = &pm_root; si_bc.has_public_derivation = TRUE;
      a_class_type_supplement si_supp = {}; si_supp.direct_base_classes = &si_bc;
      rtti_si.variant.class_struct_union.extra_info = &si_supp;

      rtti_ptr_t.kind = tk_pointer; rtti_ptr_t.source_corresp.name = (char*)"_ZTI_oom_ptr";
      rtti_ptr_t.variant.pointer.type = &pm_root;

      rtti_ptm_t.kind = tk_ptr_to_member; rtti_ptm_t.source_corresp.name = (char*)"_ZTI_oom_ptm";
      rtti_ptm_t.variant.ptr_to_member.type = &basic_ty;
      rtti_ptm_t.variant.ptr_to_member.class_of_which_a_member = &pm_mid;

      a_type rtti_vmi = {}; rtti_vmi.kind = tk_class; rtti_vmi.source_corresp.name = (char*)"_ZTI_oom_vmi";
      a_base_class vmi_b1 = {}, vmi_b2 = {};
      vmi_b1.type = &pm_root; vmi_b1.next_direct = &vmi_b2;
      vmi_b2.type = &pm_mid; vmi_b2.next_direct = nullptr;
      a_class_type_supplement vmi_supp = {}; vmi_supp.direct_base_classes = &vmi_b1;
      rtti_vmi.variant.class_struct_union.extra_info = &vmi_supp;

      auto saved_mod = std::move(state.module);
      auto reset_fresh_mod = [&](const char* tr) noexcept -> llvm_gen_be_error_t {
        state.module = std::make_unique<llvm::Module>("oom_vr", *state.context);
        EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), tr);
        llvm::Function* tf = llvm::Function::Create(cur_vr_fn->getFunctionType(), llvm::Function::ExternalLinkage, "oom_f", state.module.get());
        llvm::BasicBlock* tbb = llvm::BasicBlock::Create(*state.context, "entry", tf);
        state.builder->SetInsertPoint(tbb);
        state.emitted_rtti.clear();
        state.emitted_vtables.clear();
        return llvm_gen_be_error_t::ok;
      };

      a_type_ptr rtti_sweep_tys[] = {&rtti_si, &rtti_vmi, &rtti_ptr_t, &rtti_ptm_t};
      for (a_type_ptr rty_it : rtti_sweep_tys) {
        for (int b = 0; b < 16; ++b) {
          assert(reset_fresh_mod("x86_64-unknown-linux-gnu") == llvm_gen_be_error_t::ok);
          llvm::Constant* ti_c = nullptr;
          g_llvm_be_oom_countdown = b;
          llvm_gen_be_error_t rr = get_typeinfo_global(rty_it, &ti_c);
          g_llvm_be_oom_countdown = -1;
          if (rr == llvm_gen_be_error_t::ok) break;
        }
      }

      // MSVC COL, BCD, CHD, R0 OOM sweeps
      vmi_b1.next = &vmi_b2; vmi_b2.next = nullptr;
      vmi_supp.base_classes = &vmi_b1;
      for (int b = 0; b < 96; ++b) {
        assert(reset_fresh_mod("x86_64-pc-windows-msvc") == llvm_gen_be_error_t::ok);
        llvm::Constant* col_c = nullptr;
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t rr = llvm_emit_msvc_rtti_complete_object_locator(&rtti_vmi, &col_c);
        g_llvm_be_oom_countdown = -1;
        if (rr == llvm_gen_be_error_t::ok) break;
      }
      for (int b = 0; b < 8; ++b) {
        assert(reset_fresh_mod("x86_64-pc-windows-msvc") == llvm_gen_be_error_t::ok);
        llvm::GlobalVariable* r0_out = nullptr;
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t rr = llvm_emit_msvc_rtti_type_descriptor(&rtti_si, &r0_out);
        g_llvm_be_oom_countdown = -1;
        if (rr == llvm_gen_be_error_t::ok) break;
      }

      // dynamic_cast and typeid OOM sweeps
      a_type ptr_si = {}, ptr_vmi = {};
      ptr_si.kind = tk_pointer; ptr_si.variant.pointer.type = &rtti_si;
      ptr_vmi.kind = tk_pointer; ptr_vmi.variant.pointer.type = &rtti_vmi;
      an_expr_node dc_op = {}, dc_expr = {};
      dc_op.type = &ptr_si;
      dc_expr.kind = enk_operation; dc_expr.type = &ptr_vmi;
      dc_expr.variant.operation.kind = eok_dynamic_cast;
      dc_expr.variant.operation.operands = &dc_op;
      for (int b = 0; b < 16; ++b) {
        assert(reset_fresh_mod("x86_64-unknown-linux-gnu") == llvm_gen_be_error_t::ok);
        llvm::Value* dc_v = nullptr;
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t rr = llvm_lower_dynamic_cast(&dc_expr, &dc_v);
        g_llvm_be_oom_countdown = -1;
        if (rr == llvm_gen_be_error_t::ok) break;
      }

      an_expr_node tid_op = {}, tid_expr = {};
      tid_op.variant.type_operand.type = &rtti_si;
      tid_expr.kind = enk_typeid; tid_expr.type = &rtti_si;
      tid_expr.variant.typeid_info.type_with_opt_expr = &tid_op;
      for (int b = 0; b < 8; ++b) {
        assert(reset_fresh_mod("x86_64-unknown-linux-gnu") == llvm_gen_be_error_t::ok);
        llvm::Value* tid_v = nullptr;
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t rr = llvm_lower_typeid(&tid_expr, &tid_v);
        g_llvm_be_oom_countdown = -1;
        if (rr == llvm_gen_be_error_t::ok) break;
      }

      // vtable, VTT, construction vtable, and MSVC vbtable OOM sweeps
      // Setup a class with a virtual method requiring thunk adjustment + secondary base
      a_routine vt_r1 = {}; vt_r1.source_corresp.name = (char*)"vt_fn1"; vt_r1.is_virtual = TRUE; vt_r1.number.virtual_function = 1; vt_r1.type = &rt_multi; vt_r1.delta = 8;
      a_scope vt_sc = {}; vt_sc.routines = &vt_r1;
      a_type vt_base1 = {}, vt_base2 = {}, vt_der = {};
      vt_base1.kind = tk_class; vt_base1.source_corresp.name = (char*)"_ZTI_vt_b1";
      vt_base2.kind = tk_class; vt_base2.source_corresp.name = (char*)"_ZTI_vt_b2";
      vt_base2.variant.class_struct_union.any_virtual_functions_including_in_base_classes = TRUE;
      vt_base2.variant.class_struct_union.any_virtual_base_classes = TRUE;
      vt_der.kind = tk_class; vt_der.source_corresp.name = (char*)"_ZTI_vt_der";
      vt_der.variant.class_struct_union.any_virtual_base_classes = TRUE;
      a_class_type_supplement vt_s1 = {}, vt_s2 = {}, vt_sd = {};
      vt_s1.assoc_scope = &vt_sc; vt_s1.highest_virtual_function_number = 1;
      vt_s2.assoc_scope = &vt_sc; vt_s2.highest_virtual_function_number = 1;
      a_base_class vt_vb = {}; vt_vb.is_virtual = TRUE; vt_vb.type = &vt_base1; vt_vb.offset = 16;
      vt_s2.base_classes = &vt_vb; vt_s2.direct_base_classes = &vt_vb;
      vt_base1.variant.class_struct_union.extra_info = &vt_s1;
      vt_base2.variant.class_struct_union.extra_info = &vt_s2;
      a_base_class vt_db1 = {}, vt_db2 = {};
      vt_db1.type = &vt_base1; vt_db1.offset = 16; vt_db1.is_virtual = TRUE; vt_db1.next_direct = &vt_db2; vt_db1.next = &vt_db2;
      vt_db2.type = &vt_base2; vt_db2.offset = 8; vt_db2.is_virtual = FALSE;
      vt_sd.direct_base_classes = &vt_db1; vt_sd.base_classes = &vt_db1;
      vt_sd.assoc_scope = &vt_sc; vt_sd.highest_virtual_function_number = 1;
      vt_der.variant.class_struct_union.extra_info = &vt_sd;

      g_mangled_vtbl_override = (char*)"_ZTV_oom_der";
      g_mangled_vtt_override = (char*)"_ZTT_oom_der";
      for (int b = 0; b < 64; ++b) {
        assert(reset_fresh_mod("x86_64-unknown-linux-gnu") == llvm_gen_be_error_t::ok);
        llvm::Constant* out_c = nullptr;
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t rr = llvm_emit_vtt(&vt_der, &out_c);
        g_llvm_be_oom_countdown = -1;
        assert(rr == llvm_gen_be_error_t::ok || rr != llvm_gen_be_error_t::invalid_argument);
      }
      for (int b = 0; b < 32; ++b) {
        assert(reset_fresh_mod("x86_64-unknown-linux-gnu") == llvm_gen_be_error_t::ok);
        llvm::Constant* out_c = nullptr;
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t rr = llvm_emit_construction_vtable(&vt_der, &vt_db2, &out_c);
        g_llvm_be_oom_countdown = -1;
        if (rr == llvm_gen_be_error_t::ok) break;
      }
      for (int b = 0; b < 48; ++b) {
        assert(reset_fresh_mod("x86_64-unknown-linux-gnu") == llvm_gen_be_error_t::ok);
        llvm::Constant* out_c = nullptr;
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t rr = llvm_emit_vtable(&vt_der, &out_c);
        g_llvm_be_oom_countdown = -1;
        if (rr == llvm_gen_be_error_t::ok) break;
      }
      for (int b = 0; b < 64; ++b) {
        assert(reset_fresh_mod("x86_64-pc-windows-msvc") == llvm_gen_be_error_t::ok);
        llvm::Constant* out_c = nullptr;
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t rr = llvm_emit_vtable(&vt_der, &out_c);
        g_llvm_be_oom_countdown = -1;
        if (rr == llvm_gen_be_error_t::ok) break;
      }
      state.module = std::move(saved_mod);
      state.builder->SetInsertPoint(&cur_vr_fn->back());

      // Variadic thunk with covariant return adjustment (thunk_ty->isVarArg() && needs_covariant_adj)
      {
        a_routine_type_supplement var_rts = {};
        var_rts.has_ellipsis = TRUE;
        a_type ptr_ret_t = {};
        ptr_ret_t.kind = tk_pointer;
        ptr_ret_t.size = 8;
        a_type var_rty = {};
        var_rty.kind = tk_routine;
        var_rty.size = 6;
        var_rty.variant.routine.return_type = &ptr_ret_t;
        var_rty.variant.routine.extra_info = &var_rts;
        a_routine var_cov_rt = {};
        var_cov_rt.source_corresp.name = (char*)"_ZN3Cov7var_covEz";
        var_cov_rt.type = &var_rty;
        llvm_thunk_info_t var_cov_info = {};
        var_cov_info.this_offset = -8;
        var_cov_info.vcall_offset_index = -1;
        var_cov_info.return_offset = 8;
        var_cov_info.return_vbase_offset_index = -1;
        var_cov_info.target_routine = &var_cov_rt;
        var_cov_info.is_virtual = false;
        llvm::Function* var_cov_thk = nullptr;
        assert(llvm_emit_thunk(&var_cov_info, &var_cov_thk) == llvm_gen_be_error_t::ok);
        assert(var_cov_thk != nullptr);
      }

      // OOM sweep on non-virtual llvm_const_from_ptr_to_member
      {
        a_routine nv_oom_r = {};
        nv_oom_r.source_corresp.name = (char*)"_ZN3Foo3barEv";
        a_constant nv_oom_con = {};
        nv_oom_con.kind = ck_ptr_to_member;
        nv_oom_con.variant.ptr_to_member.is_function_ptr = TRUE;
        nv_oom_con.variant.ptr_to_member.variant.routine = &nv_oom_r;
        for (int b = 0; b < 8; ++b) {
          llvm::Constant* nv_out = nullptr;
          g_llvm_be_oom_countdown = b;
          llvm_gen_be_error_t nr = llvm_const_from_ptr_to_member(&nv_oom_con, pm_fn_st, &nv_out);
          g_llvm_be_oom_countdown = -1;
          if (nr == llvm_gen_be_error_t::ok) break;
        }
      }
    }
  }

  printf("PASS\n");
  return 0;
}
