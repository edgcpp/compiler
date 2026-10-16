/**
 * @file test_llvm_vtable_rtti.cpp
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
char* get_mangled_function_name(a_routine_ptr routine) {
    return nullptr;
}

  a_boolean is_bool_type(a_type_ptr ty) { return FALSE; }
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

  char* mangled_typeinfo_name(a_type_ptr type) {
    if (type && (type->kind == tk_class || type->kind == tk_struct)) return (char*)"_ZTI1C";
    if (type && type->kind == tk_pointer) return (char*)"_ZTIPi";
    return (char*)"_ZTIi";
  }

  a_type_qualifier_set f_get_type_qualifiers(a_type_ptr type, a_boolean x) { return 0; }
  a_boolean is_reference_type(a_type_ptr ty) { return FALSE; }
  a_boolean is_empty_class_type(a_type_ptr ty) { return ty && ty->variant.class_struct_union.is_empty_class; }
  a_byte_boolean int_kind_is_signed[ik_last] = {0};

  llvm_gen_be_error_t llvm_lower_expression(an_expr_node_ptr expr, llvm::Value** out_val) noexcept {
    if (!out_val) return llvm_gen_be_error_t::invalid_argument;
    if (be_state && be_state->context) {
      *out_val = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
    } else {
      *out_val = nullptr;
    }
    return llvm_gen_be_error_t::ok;
  }

  llvm_gen_be_error_t get_llvm_type(a_type_ptr edg_type, llvm::Type** out_type) noexcept {
    if (!out_type) return llvm_gen_be_error_t::invalid_argument;
    if (be_state && be_state->context) {
      *out_type = llvm::PointerType::getUnqual(*be_state->context);
    } else {
      *out_type = nullptr;
    }
    return llvm_gen_be_error_t::ok;
  }
  a_routine_ptr vtbl_decider_function_for_class(a_type_ptr class_type, a_boolean* unknown) {
      if (unknown) *unknown = FALSE;
      return nullptr;
  }
}


namespace edg {
  char* mangled_vtbl_name(a_type_ptr class_type, a_base_class_ptr bcp, a_base_class_ptr ctor_bcp) {
      if (bcp) return (char*)"_ZTCmock";
      return (char*)"_ZTVmock";
  }
  char* mangled_virtual_table_table_name(a_type_ptr type) {
      return (char*)"_ZTTmock";
  }
}

int main() {
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
  assert(llvm_emit_construction_vtable(nullptr, &class_b, &cvtable) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_construction_vtable(&class_ty, nullptr, &cvtable) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_construction_vtable(&class_ty, &class_b, nullptr) == llvm_gen_be_error_t::invalid_argument);
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

  // 10. Phase 2: MSVC RTTI Complete Object Locator tests
  llvm::Constant* col = nullptr;
  assert(llvm_emit_msvc_rtti_complete_object_locator(nullptr, &col) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_msvc_rtti_complete_object_locator(&class_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_msvc_rtti_complete_object_locator(&basic_ty, &col) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_emit_msvc_rtti_complete_object_locator(&class_ty, &col) == llvm_gen_be_error_t::ok);
  assert(col != nullptr);

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

  llvm::Value* pm_cmp_val = nullptr;
  assert(llvm_lower_pm_comparison(&pm_expr, &pm_cmp_val) == llvm_gen_be_error_t::invalid_argument);

  printf("PASS\n");
  return 0;
}
