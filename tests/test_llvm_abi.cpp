/**
 * @file test_llvm_abi.cpp
 * @brief Unit tests for the LLVM backend ABI lowering subsystems.
 * @details Validates register classification, sret handling, HFA/HVA, and function type generation
 * for System V AMD64, AAPCS64, Win64, ARM32, x86-32, RISC-V 64, and WebAssembly.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_abi_sysv_x86_64.h"
#include "llvm_gen_be_abi_aapcs64.h"
#include "llvm_gen_be_abi_win64.h"
#include "llvm_gen_be_abi_arm32.h"
#include "llvm_gen_be_abi_x86_32.h"
#include "llvm_gen_be_abi_riscv64.h"
#include "llvm_gen_be_abi_wasm.h"
#include "llvm_gen_be_abi_loongarch64.h"
#include "llvm_gen_be_abi_mips.h"
#include "llvm_gen_be_abi_ppc64.h"
#include "llvm_gen_be_abi_sparc.h"
#include "llvm_gen_be_abi_s390x.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

using namespace edg;

LLVMBackendState* edg::be_state = nullptr;

namespace edg {
  a_boolean is_bool_type(a_type_ptr ty) { return ty->kind == tk_integer && ty->variant.integer.int_kind == ik_char; }
  a_targ_alignment f_alignment_of_type(a_type_ptr ty) { return 4; }
  a_boolean is_trivially_copyable_type(a_type_ptr ty) { 
    if (ty && ty->size == 99) return FALSE;
    return TRUE; 
  }
  unsigned int targ_char_bit = 8;
  a_targ_size_t targ_sizeof_short = 2;
  a_targ_size_t targ_sizeof_int = 4;
  a_targ_size_t targ_sizeof_long = 8;
  a_targ_size_t targ_sizeof_long_long = 8;
  a_targ_size_t targ_sizeof_pointer = 8;
  a_targ_size_t targ_sizeof_float = 4;
  a_targ_size_t targ_sizeof_double = 8;
  a_targ_size_t targ_sizeof_long_double = 16;
  a_boolean targ_little_endian = TRUE;
  a_C_dialect C_dialect = static_cast<a_C_dialect>(0);

  a_boolean is_reference_type(a_type_ptr ty) { return FALSE; }
  a_boolean is_empty_class_type(a_type_ptr ty) { return ty && ty->variant.class_struct_union.is_empty_class; }
  a_boolean is_class_struct_union_type(a_type_ptr ty) { return ty && (ty->kind == tk_class || ty->kind == tk_struct || ty->kind == tk_union); }
  llvm_gen_be_error_t llvm_lower_expression(an_expr_node_ptr expr, llvm::Value** out_val) noexcept {
    if (out_val) *out_val = nullptr;
    return llvm_gen_be_error_t::ok;
  }

  llvm_gen_be_error_t llvm_type_from_edg_type(a_type_ptr ty, llvm::Type** out_ty) noexcept {
      return get_llvm_type(ty, out_ty);
  }
  llvm_gen_be_error_t llvm_type_from_float(a_type_ptr ty, llvm::Type** out_ty) noexcept {
      *out_ty = llvm::Type::getFloatTy(*be_state->context);
      return llvm_gen_be_error_t::ok;
  }

  llvm_gen_be_error_t get_llvm_type(a_type_ptr ty, llvm::Type** out_ty) noexcept {
    if (!out_ty) return llvm_gen_be_error_t::invalid_argument;
    if (!ty) {
       *out_ty = llvm::Type::getInt32Ty(*be_state->context);
       return llvm_gen_be_error_t::ok;
    }
    if (ty->kind == tk_integer) {
       *out_ty = llvm::Type::getInt32Ty(*be_state->context);
    } else if (ty->kind == tk_float) {
       *out_ty = llvm::Type::getFloatTy(*be_state->context);
    } else {
       *out_ty = llvm::Type::getInt32Ty(*be_state->context);
    }
    return llvm_gen_be_error_t::ok;
  }

  char* mangled_typeinfo_name(a_type_ptr type) {
    return (char*)"_ZTI_mock";
  }
}

int main() {
  printf("Running test_llvm_abi...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_abi", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* f = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);

  a_type int_ty = {};
  int_ty.kind = tk_integer;
  int_ty.variant.integer.int_kind = ik_int;
  int_ty.size = 4; int_ty.alignment = 4;

  a_type float_ty = {};
  float_ty.kind = tk_float;
  float_ty.variant.float_kind = fk_float;
  float_ty.size = 4; float_ty.alignment = 4;

  a_type struct_ty = {};
  struct_ty.kind = tk_class;
  struct_ty.size = 8; struct_ty.alignment = 4;
  a_field f1 = {};
  f1.type = &int_ty;
  f1.offset = 0;
  a_field f2 = {};
  f2.type = &float_ty;
  f2.offset = 4;
  f1.next = &f2;
  struct_ty.variant.class_struct_union.field_list = &f1;
  a_class_type_supplement extra = {};
  struct_ty.variant.class_struct_union.extra_info = &extra;

  // 1. SysV x86_64 ABI
  {
      x86_64_abi_arg_info_t info = {};
      assert(classify_sysv_argument(nullptr, 0, &info) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_sysv_argument(&int_ty, 0, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_sysv_argument(&int_ty, 0, &info) == llvm_gen_be_error_t::ok);
       assert(info.num_eightbytes == 1);
      assert(info.eightbyte_classes[0] == x86_64_abi_class_t::integer);

      info = {};
      assert(classify_sysv_argument(&float_ty, 0, &info) == llvm_gen_be_error_t::ok);
       assert(info.num_eightbytes == 1);
      assert(info.eightbyte_classes[0] == x86_64_abi_class_t::sse);

      info = {};
      assert(classify_sysv_argument(&struct_ty, 0, &info) == llvm_gen_be_error_t::ok);
       assert(info.num_eightbytes == 1);

      bool is_sret = false;
      assert(compute_sysv_return_info(&struct_ty, &info, &is_sret) == llvm_gen_be_error_t::ok);
      assert(is_sret == false);

      llvm::FunctionType* fty = nullptr;
      llvm::AttributeList attrs;
      a_type rty = {};
      rty.kind = tk_routine;
      a_routine_type_supplement rsupp = {};
      rty.variant.routine.extra_info = &rsupp;
      rty.variant.routine.return_type = &int_ty;
      a_param_type p1 = {};
      p1.type = &struct_ty;
      rsupp.param_type_list = &p1;
      assert(build_sysv_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      assert(fty != nullptr);

      // va_list lowering tests
      llvm::Type* va_list_ty = nullptr;
      assert(build_sysv_va_list_type(nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(build_sysv_va_list_type(&va_list_ty) == llvm_gen_be_error_t::ok);
      assert(va_list_ty != nullptr);
      assert(va_list_ty->isArrayTy());

      llvm::Value* va_ptr = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
      llvm::Value* out_val = nullptr;
      assert(lower_sysv_va_start(nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_sysv_va_start(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_sysv_va_start(va_ptr, &out_val) == llvm_gen_be_error_t::ok);
      assert(out_val != nullptr);

      assert(lower_sysv_va_arg(nullptr, &int_ty, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_sysv_va_arg(va_ptr, nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_sysv_va_arg(va_ptr, &int_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_sysv_va_arg(va_ptr, &int_ty, &out_val) == llvm_gen_be_error_t::ok);
      assert(out_val != nullptr);

      assert(lower_sysv_va_copy(nullptr, va_ptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_sysv_va_copy(va_ptr, nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_sysv_va_copy(va_ptr, va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_sysv_va_copy(va_ptr, va_ptr, &out_val) == llvm_gen_be_error_t::ok);
      assert(out_val != nullptr);

      assert(lower_sysv_va_end(nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_sysv_va_end(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_sysv_va_end(va_ptr, &out_val) == llvm_gen_be_error_t::ok);
      assert(out_val != nullptr);

      // Non-trivially copyable C++ class test
      a_type non_trivial_ty = {};
      non_trivial_ty.kind = tk_class;
      non_trivial_ty.size = 99; // To trigger is_trivially_copyable_type == FALSE
      
      info = {};
      assert(classify_sysv_argument(&non_trivial_ty, 0, &info) == llvm_gen_be_error_t::ok);
      assert(info.pass_in_memory == true);
      assert(info.pass_by_invisible_reference == true);

      // Flexible array member struct test
      a_type fam_struct_ty = {};
      fam_struct_ty.kind = tk_struct;
      fam_struct_ty.size = 4;
      a_field fam_f1 = {};
      fam_f1.type = &int_ty;
      fam_f1.offset = 0;
      a_field fam_f2 = {};
      a_type fam_array_ty = {};
      fam_array_ty.kind = tk_array;
      fam_array_ty.size = 0; fam_array_ty.alignment = 4; // Size 0 flexible array
      fam_f2.type = &fam_array_ty;
      fam_f2.offset = 4;
      fam_f1.next = &fam_f2;
      fam_struct_ty.variant.class_struct_union.field_list = &fam_f1;

      info = {};
      assert(classify_sysv_argument(&fam_struct_ty, 0, &info) == llvm_gen_be_error_t::ok);
       assert(info.num_eightbytes == 1);
      assert(info.eightbyte_classes[0] == x86_64_abi_class_t::integer);
  }

  // 2. AAPCS64 ABI
  {
      state.module->setTargetTriple(llvm::Triple("aarch64-unknown-linux-gnu"));
      aapcs64_arg_info_t info = {};
      assert(classify_aapcs64_argument(nullptr, &info) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_aapcs64_argument(&int_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_aapcs64_argument(&int_ty, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == aapcs64_abi_class_t::integer);


      // Non-trivially copyable test
      a_type non_trivial_ty = {};
      non_trivial_ty.kind = tk_class;
      non_trivial_ty.size = 99; // To trigger is_trivially_copyable_type == FALSE
      info = {};
      assert(classify_aapcs64_argument(&non_trivial_ty, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == aapcs64_abi_class_t::reference);
      assert(info.pass_by_invisible_reference == true);

      // C empty struct tests (size 0)
      a_type empty_c_struct = {};
      empty_c_struct.kind = tk_struct;
      empty_c_struct.size = 0;
      assert(classify_aapcs64_argument(&empty_c_struct, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == aapcs64_abi_class_t::no_class);

      // C++ empty struct tests (size 1)
      a_type empty_cpp_struct = {};
      empty_cpp_struct.kind = tk_class;
      empty_cpp_struct.size = 1;
      empty_cpp_struct.variant.class_struct_union.is_empty_class = TRUE;
      assert(classify_aapcs64_argument(&empty_cpp_struct, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == aapcs64_abi_class_t::integer); // Linux passes it as 1-byte integer

      state.module->setTargetTriple(llvm::Triple("arm64-apple-darwin"));
      assert(classify_aapcs64_argument(&empty_cpp_struct, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == aapcs64_abi_class_t::no_class); // Darwin ignores it completely
      
      bool is_sret_val = false;
      assert(compute_aapcs64_return_info(&empty_cpp_struct, &info, &is_sret_val) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == aapcs64_abi_class_t::no_class); // Return value is ignored on both

      state.module->setTargetTriple(llvm::Triple("aarch64-unknown-linux-gnu"));

      info = {};
      assert(classify_aapcs64_argument(&float_ty, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == aapcs64_abi_class_t::float_vector);

      info = {};
      assert(classify_aapcs64_argument(&struct_ty, &info) == llvm_gen_be_error_t::ok);

      bool is_sret = false;
      assert(compute_aapcs64_return_info(&struct_ty, &info, &is_sret) == llvm_gen_be_error_t::ok);
      assert(is_sret == false);

      // HFA/HVA test
      uint32_t num_hfa = 0;
      llvm::Type* hfa_elem = nullptr;
      bool is_hfa = false;
      assert(detect_hfa_hva(&float_ty, &num_hfa, &hfa_elem, &is_hfa) == llvm_gen_be_error_t::ok);
      assert(is_hfa && num_hfa == 1);

#if GNU_VECTOR_TYPES_ALLOWED
      a_type vector_ty = {};
      vector_ty.kind = tk_vector;
      vector_ty.size = 16;
      a_type elem_int_ty = int_ty;
      // We don't have the exact struct variant for tk_vector setup properly for the mock,
      // but the detect_hfa_hva function expects it to be either float or vector type 
      // and we just added vector size checking.
      assert(detect_hfa_hva(&vector_ty, &num_hfa, &hfa_elem, &is_hfa) == llvm_gen_be_error_t::ok);
      assert(is_hfa && num_hfa == 1);

      a_type large_vector_ty = {};
      large_vector_ty.kind = tk_vector;
      large_vector_ty.size = 32;
      assert(detect_hfa_hva(&large_vector_ty, &num_hfa, &hfa_elem, &is_hfa) == llvm_gen_be_error_t::ok);
      assert(is_hfa == false);
#endif

      llvm::FunctionType* fty = nullptr;
      llvm::AttributeList attrs;
      a_type rty = {};
      rty.kind = tk_routine;
      a_routine_type_supplement rsupp = {};
      rty.variant.routine.extra_info = &rsupp;
      rty.variant.routine.return_type = &int_ty;
      a_param_type p1 = {};
      p1.type = &struct_ty;
      rsupp.param_type_list = &p1;
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      assert(fty != nullptr);

      // va_list lowering tests
      llvm::Type* va_list_ty = nullptr;
      assert(build_aapcs64_va_list_type(nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(build_aapcs64_va_list_type(&va_list_ty) == llvm_gen_be_error_t::ok);
      assert(va_list_ty != nullptr);
      assert(va_list_ty->isArrayTy()); // Linux AAPCS64 uses struct array for va_list

      state.module->setTargetTriple(llvm::Triple("arm64-apple-darwin"));
      assert(build_aapcs64_va_list_type(&va_list_ty) == llvm_gen_be_error_t::ok);
      assert(va_list_ty->isPointerTy()); // Darwin uses char* for va_list
      state.module->setTargetTriple(llvm::Triple("aarch64-unknown-linux-gnu"));

      llvm::Value* va_ptr = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
      llvm::Value* out_val = nullptr;
      assert(lower_aapcs64_va_start(nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_aapcs64_va_start(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_aapcs64_va_start(va_ptr, &out_val) == llvm_gen_be_error_t::ok);
      assert(out_val != nullptr);

      assert(lower_aapcs64_va_arg(nullptr, &int_ty, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_aapcs64_va_arg(va_ptr, nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_aapcs64_va_arg(va_ptr, &int_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_aapcs64_va_arg(va_ptr, &int_ty, &out_val) == llvm_gen_be_error_t::ok);
      assert(out_val != nullptr);

      assert(lower_aapcs64_va_copy(nullptr, va_ptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_aapcs64_va_copy(va_ptr, nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_aapcs64_va_copy(va_ptr, va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_aapcs64_va_copy(va_ptr, va_ptr, &out_val) == llvm_gen_be_error_t::ok);
      assert(out_val != nullptr);

      assert(lower_aapcs64_va_end(nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_aapcs64_va_end(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_aapcs64_va_end(va_ptr, &out_val) == llvm_gen_be_error_t::ok);
      assert(out_val != nullptr);
  }

  // 3. Win64 ABI
  {
      win64_arg_info_t info = {};
      assert(classify_win64_argument(nullptr, false, &info) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_win64_argument(&int_ty, false, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_win64_argument(&int_ty, false, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == win64_abi_class_t::direct_integer);

      info = {};
      assert(classify_win64_argument(&float_ty, false, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == win64_abi_class_t::direct_float);
      
      info = {};
      assert(classify_win64_argument(&float_ty, true, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == win64_abi_class_t::vectorcall_hva);

      info = {};
      assert(classify_win64_argument(&struct_ty, false, &info) == llvm_gen_be_error_t::ok);

      bool is_sret = false;
      assert(compute_win64_return_info(&struct_ty, false, &info, &is_sret) == llvm_gen_be_error_t::ok);
      assert(is_sret == false);
      a_type non_trivial_ty = {};
      non_trivial_ty.kind = tk_class;
      non_trivial_ty.size = 99;
      info = {};
      assert(classify_win64_argument(&non_trivial_ty, false, &info) == llvm_gen_be_error_t::ok);
      assert(info.is_indirect == true);
      assert(info.pass_by_invisible_reference == true);


      llvm::FunctionType* fty = nullptr;
      llvm::AttributeList attrs;
      a_type rty = {};
      rty.kind = tk_routine;
      a_routine_type_supplement rsupp = {};
      rty.variant.routine.extra_info = &rsupp;
      rty.variant.routine.return_type = &int_ty;
      a_param_type p1 = {};
      p1.type = &struct_ty;
      rsupp.param_type_list = &p1;
      assert(build_win64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      assert(fty != nullptr);

      // va_list tests
      llvm::Type* va_list_ty = nullptr;
      assert(build_win64_va_list_type(nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(build_win64_va_list_type(&va_list_ty) == llvm_gen_be_error_t::ok);
      assert(va_list_ty != nullptr);

      llvm::Value* va_ptr = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
      llvm::Value* va_val = nullptr;
      assert(lower_win64_va_start(nullptr, &va_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_win64_va_start(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_win64_va_start(va_ptr, &va_val) == llvm_gen_be_error_t::ok);
      assert(va_val != nullptr);

      assert(lower_win64_va_arg(nullptr, &int_ty, &va_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_win64_va_arg(va_ptr, nullptr, &va_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_win64_va_arg(va_ptr, &int_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_win64_va_arg(va_ptr, &int_ty, &va_val) == llvm_gen_be_error_t::ok);
      assert(va_val != nullptr);

      assert(lower_win64_va_copy(nullptr, va_ptr, &va_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_win64_va_copy(va_ptr, nullptr, &va_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_win64_va_copy(va_ptr, va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_win64_va_copy(va_ptr, va_ptr, &va_val) == llvm_gen_be_error_t::ok);
      assert(va_val != nullptr);

      assert(lower_win64_va_end(nullptr, &va_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_win64_va_end(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_win64_va_end(va_ptr, &va_val) == llvm_gen_be_error_t::ok);
      assert(va_val != nullptr);
  }

  // 4. ARM32 ABI
  {
      arm32_arg_info_t info = {};
      assert(classify_arm32_argument(nullptr, &info) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_arm32_argument(&int_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_arm32_argument(&int_ty, &info) == llvm_gen_be_error_t::ok);
      assert(!info.is_indirect);

      info = {};
      assert(classify_arm32_argument(&float_ty, &info) == llvm_gen_be_error_t::ok);

      info = {};
      assert(classify_arm32_argument(&struct_ty, &info) == llvm_gen_be_error_t::ok);

      llvm::FunctionType* fty = nullptr;
      llvm::AttributeList attrs;
      a_type rty = {};
      rty.kind = tk_routine;
      a_routine_type_supplement rsupp = {};
      rty.variant.routine.extra_info = &rsupp;
      rty.variant.routine.return_type = &int_ty;
      a_param_type p1 = {};
      p1.type = &struct_ty;
      rsupp.param_type_list = &p1;
      assert(build_arm32_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      assert(fty != nullptr);
  }

  // 5. x86 32-bit ABI
  {
      x86_32_arg_info_t info = {};
      assert(classify_x86_32_argument(nullptr, &info) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_x86_32_argument(&int_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_x86_32_argument(&int_ty, &info) == llvm_gen_be_error_t::ok);
      assert(!info.is_indirect);

      llvm::FunctionType* fty = nullptr;
      llvm::AttributeList attrs;
      a_type rty = {};
      rty.kind = tk_routine;
      a_routine_type_supplement rsupp = {};
      rsupp.calling_convention = cc_fastcall;
      rty.variant.routine.extra_info = &rsupp;
      rty.variant.routine.return_type = &int_ty;
      a_param_type p1 = {};
      p1.type = &struct_ty;
      a_param_type p2 = {};
      p2.type = &int_ty;
      p1.next = &p2;
      rsupp.param_type_list = &p1;
      assert(build_x86_32_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      assert(fty != nullptr);
      
      // fastcall should put inreg on the int_ty
      assert(attrs.hasParamAttr(1, llvm::Attribute::InReg));
      
      // Test thiscall
      rsupp.calling_convention = cc_thiscall;
      assert(build_x86_32_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      assert(fty != nullptr);
      // Wait, struct_ty is > 8 bytes so it's passed indirectly, so param 0 is pointer, which gets InReg
      assert(attrs.hasParamAttr(0, llvm::Attribute::InReg));
  }

  // 6. WebAssembly ABI
  {
      wasm_arg_info_t info = {};
      assert(classify_wasm_argument(nullptr, &info) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_wasm_argument(&int_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_wasm_argument(&int_ty, &info) == llvm_gen_be_error_t::ok);
      assert(!info.is_indirect);

      assert(classify_wasm_argument(&struct_ty, &info) == llvm_gen_be_error_t::ok);
      assert(info.is_indirect);

      llvm::FunctionType* fty = nullptr;
      llvm::AttributeList attrs;
      a_type rty = {};
      rty.kind = tk_routine;
      a_routine_type_supplement rsupp = {};
      rty.variant.routine.extra_info = &rsupp;
      rty.variant.routine.return_type = &int_ty;
      a_param_type p1 = {};
      p1.type = &struct_ty;
      rsupp.param_type_list = &p1;
      assert(build_wasm_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      assert(fty != nullptr);
  }

  // 7. RISC-V 64-bit ABI
  {
      riscv64_arg_info_t info = {};
      assert(classify_riscv64_argument(nullptr, &info) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_riscv64_argument(&int_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_riscv64_argument(&int_ty, &info) == llvm_gen_be_error_t::ok);
      assert(!info.is_indirect);

      info = {};
      assert(classify_riscv64_argument(&struct_ty, &info) == llvm_gen_be_error_t::ok);

      bool is_sret = false;
      assert(compute_riscv64_return_info(&struct_ty, &info, &is_sret) == llvm_gen_be_error_t::ok);

      llvm::FunctionType* fty = nullptr;
      llvm::AttributeList attrs;
      a_type rty = {};
      rty.kind = tk_routine;
      a_routine_type_supplement rsupp = {};
      rty.variant.routine.extra_info = &rsupp;
      rty.variant.routine.return_type = &int_ty;
      a_param_type p1 = {};
      p1.type = &struct_ty;
      rsupp.param_type_list = &p1;

      assert(build_riscv64_function_type(nullptr, &fty, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_riscv64_function_type(&rty, nullptr, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_riscv64_function_type(&rty, &fty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(build_riscv64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      assert(fty != nullptr);

      // va_list tests
      llvm::Type* va_list_ty = nullptr;
      assert(build_riscv64_va_list_type(nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(build_riscv64_va_list_type(&va_list_ty) == llvm_gen_be_error_t::ok);
      assert(va_list_ty != nullptr);

      llvm::Value* va_ptr = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
      llvm::Value* va_val = nullptr;
      assert(lower_riscv64_va_start(nullptr, &va_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_riscv64_va_start(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_riscv64_va_start(va_ptr, &va_val) == llvm_gen_be_error_t::ok);
      assert(va_val != nullptr);

      assert(lower_riscv64_va_arg(nullptr, &int_ty, &va_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_riscv64_va_arg(va_ptr, nullptr, &va_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_riscv64_va_arg(va_ptr, &int_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_riscv64_va_arg(va_ptr, &int_ty, &va_val) == llvm_gen_be_error_t::ok);
      assert(va_val != nullptr);

      assert(lower_riscv64_va_copy(nullptr, va_ptr, &va_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_riscv64_va_copy(va_ptr, nullptr, &va_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_riscv64_va_copy(va_ptr, va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_riscv64_va_copy(va_ptr, va_ptr, &va_val) == llvm_gen_be_error_t::ok);
      assert(va_val != nullptr);

      assert(lower_riscv64_va_end(nullptr, &va_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_riscv64_va_end(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_riscv64_va_end(va_ptr, &va_val) == llvm_gen_be_error_t::ok);
      assert(va_val != nullptr);
  }

  // 8. LoongArch 64 ABI
  {
      loongarch64_arg_info_t info = {};
      assert(classify_loongarch64_argument(nullptr, &info) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_loongarch64_argument(&int_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_loongarch64_argument(&int_ty, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == loongarch64_abi_class_t::integer);

      info = {};
      assert(classify_loongarch64_argument(&float_ty, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == loongarch64_abi_class_t::float_class);

      info = {};
      assert(classify_loongarch64_argument(&struct_ty, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == loongarch64_abi_class_t::integer_float);

      bool is_sret = false;
      assert(compute_loongarch64_return_info(nullptr, &info, &is_sret) == llvm_gen_be_error_t::invalid_argument);
      assert(compute_loongarch64_return_info(&struct_ty, nullptr, &is_sret) == llvm_gen_be_error_t::invalid_argument);
      assert(compute_loongarch64_return_info(&struct_ty, &info, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(compute_loongarch64_return_info(&struct_ty, &info, &is_sret) == llvm_gen_be_error_t::ok);
      assert(is_sret == false);

      a_type big_struct_ty = {};
      big_struct_ty.kind = tk_class;
      big_struct_ty.size = 32;
      assert(classify_loongarch64_argument(&big_struct_ty, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == loongarch64_abi_class_t::memory);
      assert(info.is_indirect == true);
      assert(compute_loongarch64_return_info(&big_struct_ty, &info, &is_sret) == llvm_gen_be_error_t::ok);
      assert(is_sret == true);

      a_type empty_struct_ty = {};
      empty_struct_ty.kind = tk_class;
      empty_struct_ty.size = 0;
      assert(classify_loongarch64_argument(&empty_struct_ty, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == loongarch64_abi_class_t::no_class);

      a_type hfa_struct_ty = {};
      hfa_struct_ty.kind = tk_class;
      hfa_struct_ty.size = 8;
      a_field hfa_f1 = {}, hfa_f2 = {};
      hfa_f1.type = &float_ty; hfa_f2.type = &float_ty;
      hfa_f1.next = &hfa_f2;
      hfa_struct_ty.variant.class_struct_union.field_list = &hfa_f1;
      assert(classify_loongarch64_argument(&hfa_struct_ty, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == loongarch64_abi_class_t::float_float);

      llvm::FunctionType* fty = nullptr;
      llvm::AttributeList attrs;
      a_type rty = {};
      rty.kind = tk_routine;
      a_routine_type_supplement rsupp = {};
      rty.variant.routine.extra_info = &rsupp;
      rty.variant.routine.return_type = &int_ty;
      a_param_type p1 = {};
      p1.type = &struct_ty;
      a_param_type p2 = {};
      p2.type = &big_struct_ty;
      p1.next = &p2;
      a_param_type p3 = {};
      p3.type = &empty_struct_ty;
      p2.next = &p3;
      rsupp.param_type_list = &p1;
      assert(build_loongarch64_function_type(nullptr, &fty, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_loongarch64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      assert(fty != nullptr);

      rty.variant.routine.return_type = &big_struct_ty;
      assert(build_loongarch64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      rty.variant.routine.return_type = &empty_struct_ty;
      assert(build_loongarch64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      
      llvm::Value* va_ptr = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
      llvm::Value* out_val = nullptr;
      assert(lower_loongarch64_va_start(nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_loongarch64_va_start(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_loongarch64_va_start(va_ptr, &out_val) == llvm_gen_be_error_t::ok);

      assert(lower_loongarch64_va_arg(nullptr, &int_ty, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_loongarch64_va_arg(va_ptr, nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_loongarch64_va_arg(va_ptr, &int_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_loongarch64_va_arg(va_ptr, &int_ty, &out_val) == llvm_gen_be_error_t::ok);

      assert(lower_loongarch64_va_copy(nullptr, va_ptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_loongarch64_va_copy(va_ptr, nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_loongarch64_va_copy(va_ptr, va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_loongarch64_va_copy(va_ptr, va_ptr, &out_val) == llvm_gen_be_error_t::ok);

      assert(lower_loongarch64_va_end(nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_loongarch64_va_end(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_loongarch64_va_end(va_ptr, &out_val) == llvm_gen_be_error_t::ok);
  }

  // 9. MIPS ABI
  {
      mips_arg_info_t info = {};
      assert(classify_mips_argument(nullptr, false, true, &info) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_mips_argument(&int_ty, false, true, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_mips_argument(&int_ty, false, true, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == mips_abi_class_t::integer);

      info = {};
      assert(classify_mips_argument(&float_ty, false, true, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == mips_abi_class_t::float_class);

      info = {};
      // N64 struct <= 16 bytes is coerced
      assert(classify_mips_argument(&struct_ty, false, true, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == mips_abi_class_t::struct_coerced);

      info = {};
      // O32 struct is byval
      assert(classify_mips_argument(&struct_ty, false, false, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == mips_abi_class_t::struct_byval);

      bool is_sret = false;
      assert(compute_mips_return_info(nullptr, false, true, &info, &is_sret) == llvm_gen_be_error_t::invalid_argument);
      assert(compute_mips_return_info(&struct_ty, false, true, nullptr, &is_sret) == llvm_gen_be_error_t::invalid_argument);
      assert(compute_mips_return_info(&struct_ty, false, true, &info, nullptr) == llvm_gen_be_error_t::invalid_argument);
      
      // N64 return struct <= 16 bytes is not sret
      assert(compute_mips_return_info(&struct_ty, false, true, &info, &is_sret) == llvm_gen_be_error_t::ok);
      assert(is_sret == false);

      // O32 return struct <= 8 bytes is not sret, but it's coerced
      assert(compute_mips_return_info(&struct_ty, false, false, &info, &is_sret) == llvm_gen_be_error_t::ok);
      assert(is_sret == false);
      assert(info.abi_class == mips_abi_class_t::struct_coerced);

      a_type big_struct_ty = {};
      big_struct_ty.kind = tk_class;
      big_struct_ty.size = 32;
      assert(compute_mips_return_info(&big_struct_ty, false, true, &info, &is_sret) == llvm_gen_be_error_t::ok);
      assert(is_sret == true);

      assert(compute_mips_return_info(&big_struct_ty, false, false, &info, &is_sret) == llvm_gen_be_error_t::ok);
      assert(is_sret == true);

      a_type empty_struct_ty = {};
      empty_struct_ty.kind = tk_class;
      empty_struct_ty.size = 0;
      assert(classify_mips_argument(&empty_struct_ty, false, true, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == mips_abi_class_t::no_class);

      llvm::FunctionType* fty = nullptr;
      llvm::AttributeList attrs;
      a_type rty = {};
      rty.kind = tk_routine;
      a_routine_type_supplement rsupp = {};
      rty.variant.routine.extra_info = &rsupp;
      rty.variant.routine.return_type = &int_ty;
      a_param_type p1 = {};
      p1.type = &struct_ty;
      a_param_type p2 = {};
      p2.type = &big_struct_ty;
      p1.next = &p2;
      rsupp.param_type_list = &p1;
      
      assert(build_mips_function_type(nullptr, &fty, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_mips_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      assert(fty != nullptr);

      rty.variant.routine.return_type = &big_struct_ty;
      assert(build_mips_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      rty.variant.routine.return_type = &empty_struct_ty;
      assert(build_mips_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      
      llvm::Value* va_ptr = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
      llvm::Value* out_val = nullptr;
      assert(lower_mips_va_start(nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_mips_va_start(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_mips_va_start(va_ptr, &out_val) == llvm_gen_be_error_t::ok);

      assert(lower_mips_va_arg(nullptr, &int_ty, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_mips_va_arg(va_ptr, nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_mips_va_arg(va_ptr, &int_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_mips_va_arg(va_ptr, &int_ty, &out_val) == llvm_gen_be_error_t::ok);

      assert(lower_mips_va_copy(nullptr, va_ptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_mips_va_copy(va_ptr, nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_mips_va_copy(va_ptr, va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_mips_va_copy(va_ptr, va_ptr, &out_val) == llvm_gen_be_error_t::ok);

      assert(lower_mips_va_end(nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_mips_va_end(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_mips_va_end(va_ptr, &out_val) == llvm_gen_be_error_t::ok);
  }

  // 10. PPC64 ABI
  {
      ppc64_arg_info_t info = {};
      assert(classify_ppc64_argument(nullptr, true, &info) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_ppc64_argument(&int_ty, true, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_ppc64_argument(&int_ty, true, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == ppc64_abi_class_t::integer);

      info = {};
      assert(classify_ppc64_argument(&float_ty, true, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == ppc64_abi_class_t::float_class);

      info = {};
      // ELFv2 HFA
      a_type hfa_struct_ty = {};
      hfa_struct_ty.kind = tk_class;
      hfa_struct_ty.size = 16;
      a_field hfa_f1 = {}, hfa_f2 = {}, hfa_f3 = {}, hfa_f4 = {};
      hfa_f1.type = &float_ty; hfa_f2.type = &float_ty; hfa_f3.type = &float_ty; hfa_f4.type = &float_ty;
      hfa_f1.next = &hfa_f2; hfa_f2.next = &hfa_f3; hfa_f3.next = &hfa_f4;
      hfa_struct_ty.variant.class_struct_union.field_list = &hfa_f1;
      
      assert(classify_ppc64_argument(&hfa_struct_ty, true, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == ppc64_abi_class_t::hfa);

      info = {};
      // General struct ELFv2
      assert(classify_ppc64_argument(&struct_ty, true, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == ppc64_abi_class_t::struct_byval);

      bool is_sret = false;
      assert(compute_ppc64_return_info(nullptr, true, &info, &is_sret) == llvm_gen_be_error_t::invalid_argument);
      assert(compute_ppc64_return_info(&struct_ty, true, nullptr, &is_sret) == llvm_gen_be_error_t::invalid_argument);
      assert(compute_ppc64_return_info(&struct_ty, true, &info, nullptr) == llvm_gen_be_error_t::invalid_argument);
      
      assert(compute_ppc64_return_info(&struct_ty, true, &info, &is_sret) == llvm_gen_be_error_t::ok);
      assert(is_sret == false);

      a_type big_struct_ty = {};
      big_struct_ty.kind = tk_class;
      big_struct_ty.size = 128;
      assert(compute_ppc64_return_info(&big_struct_ty, true, &info, &is_sret) == llvm_gen_be_error_t::ok);
      assert(is_sret == true);

      assert(compute_ppc64_return_info(&big_struct_ty, false, &info, &is_sret) == llvm_gen_be_error_t::ok);
      assert(is_sret == true);

      a_type empty_struct_ty = {};
      empty_struct_ty.kind = tk_class;
      empty_struct_ty.size = 0;
      assert(classify_ppc64_argument(&empty_struct_ty, true, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == ppc64_abi_class_t::no_class);

      llvm::FunctionType* fty = nullptr;
      llvm::AttributeList attrs;
      a_type rty = {};
      rty.kind = tk_routine;
      a_routine_type_supplement rsupp = {};
      rty.variant.routine.extra_info = &rsupp;
      rty.variant.routine.return_type = &int_ty;
      a_param_type p1 = {};
      p1.type = &struct_ty;
      a_param_type p2 = {};
      p2.type = &big_struct_ty;
      p1.next = &p2;
      rsupp.param_type_list = &p1;
      
      assert(build_ppc64_function_type(nullptr, &fty, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_ppc64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      assert(fty != nullptr);

      rty.variant.routine.return_type = &big_struct_ty;
      assert(build_ppc64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      rty.variant.routine.return_type = &empty_struct_ty;
      assert(build_ppc64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      
      llvm::Value* va_ptr = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
      llvm::Value* out_val = nullptr;
      assert(lower_ppc64_va_start(nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_ppc64_va_start(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_ppc64_va_start(va_ptr, &out_val) == llvm_gen_be_error_t::ok);

      assert(lower_ppc64_va_arg(nullptr, &int_ty, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_ppc64_va_arg(va_ptr, nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_ppc64_va_arg(va_ptr, &int_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_ppc64_va_arg(va_ptr, &int_ty, &out_val) == llvm_gen_be_error_t::ok);

      assert(lower_ppc64_va_copy(nullptr, va_ptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_ppc64_va_copy(va_ptr, nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_ppc64_va_copy(va_ptr, va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_ppc64_va_copy(va_ptr, va_ptr, &out_val) == llvm_gen_be_error_t::ok);

      assert(lower_ppc64_va_end(nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_ppc64_va_end(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_ppc64_va_end(va_ptr, &out_val) == llvm_gen_be_error_t::ok);
  }

  // 11. SPARC ABI
  {
      sparc_arg_info_t info = {};
      assert(classify_sparc_argument(nullptr, true, &info) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_sparc_argument(&int_ty, true, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_sparc_argument(&int_ty, true, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == sparc_abi_class_t::integer);

      info = {};
      assert(classify_sparc_argument(&float_ty, true, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == sparc_abi_class_t::float_class);

      info = {};
      assert(classify_sparc_argument(&struct_ty, true, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == sparc_abi_class_t::struct_coerced);

      info = {};
      assert(classify_sparc_argument(&struct_ty, false, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == sparc_abi_class_t::struct_byval);

      bool is_sret = false;
      assert(compute_sparc_return_info(nullptr, true, &info, &is_sret) == llvm_gen_be_error_t::invalid_argument);
      assert(compute_sparc_return_info(&struct_ty, true, nullptr, &is_sret) == llvm_gen_be_error_t::invalid_argument);
      assert(compute_sparc_return_info(&struct_ty, true, &info, nullptr) == llvm_gen_be_error_t::invalid_argument);
      
      assert(compute_sparc_return_info(&struct_ty, true, &info, &is_sret) == llvm_gen_be_error_t::ok);
      assert(is_sret == false);

      a_type big_struct_ty = {};
      big_struct_ty.kind = tk_class;
      big_struct_ty.size = 64;
      assert(compute_sparc_return_info(&big_struct_ty, true, &info, &is_sret) == llvm_gen_be_error_t::ok);
      assert(is_sret == true);

      assert(compute_sparc_return_info(&big_struct_ty, false, &info, &is_sret) == llvm_gen_be_error_t::ok);
      assert(is_sret == true);

      a_type empty_struct_ty = {};
      empty_struct_ty.kind = tk_class;
      empty_struct_ty.size = 0;
      assert(classify_sparc_argument(&empty_struct_ty, true, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == sparc_abi_class_t::no_class);

      llvm::FunctionType* fty = nullptr;
      llvm::AttributeList attrs;
      a_type rty = {};
      rty.kind = tk_routine;
      a_routine_type_supplement rsupp = {};
      rty.variant.routine.extra_info = &rsupp;
      rty.variant.routine.return_type = &int_ty;
      a_param_type p1 = {};
      p1.type = &struct_ty;
      a_param_type p2 = {};
      p2.type = &big_struct_ty;
      p1.next = &p2;
      rsupp.param_type_list = &p1;
      
      assert(build_sparc_function_type(nullptr, &fty, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_sparc_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      assert(fty != nullptr);

      rty.variant.routine.return_type = &big_struct_ty;
      assert(build_sparc_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      rty.variant.routine.return_type = &empty_struct_ty;
      assert(build_sparc_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      
      llvm::Value* va_ptr = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
      llvm::Value* out_val = nullptr;
      assert(lower_sparc_va_start(nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_sparc_va_start(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_sparc_va_start(va_ptr, &out_val) == llvm_gen_be_error_t::ok);

      assert(lower_sparc_va_arg(nullptr, &int_ty, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_sparc_va_arg(va_ptr, nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_sparc_va_arg(va_ptr, &int_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_sparc_va_arg(va_ptr, &int_ty, &out_val) == llvm_gen_be_error_t::ok);

      assert(lower_sparc_va_copy(nullptr, va_ptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_sparc_va_copy(va_ptr, nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_sparc_va_copy(va_ptr, va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_sparc_va_copy(va_ptr, va_ptr, &out_val) == llvm_gen_be_error_t::ok);

      assert(lower_sparc_va_end(nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_sparc_va_end(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_sparc_va_end(va_ptr, &out_val) == llvm_gen_be_error_t::ok);
  }

  // 12. s390x ABI
  {
      s390x_arg_info_t info = {};
      assert(classify_s390x_argument(nullptr, &info) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_s390x_argument(&int_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_s390x_argument(&int_ty, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == s390x_abi_class_t::integer);

      info = {};
      assert(classify_s390x_argument(&float_ty, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == s390x_abi_class_t::float_class);

      info = {};
      assert(classify_s390x_argument(&struct_ty, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == s390x_abi_class_t::struct_coerced);

      bool is_sret = false;
      assert(compute_s390x_return_info(nullptr, &info, &is_sret) == llvm_gen_be_error_t::invalid_argument);
      assert(compute_s390x_return_info(&struct_ty, nullptr, &is_sret) == llvm_gen_be_error_t::invalid_argument);
      assert(compute_s390x_return_info(&struct_ty, &info, nullptr) == llvm_gen_be_error_t::invalid_argument);
      
      assert(compute_s390x_return_info(&struct_ty, &info, &is_sret) == llvm_gen_be_error_t::ok);
      assert(is_sret == false);

      a_type big_struct_ty = {};
      big_struct_ty.kind = tk_class;
      big_struct_ty.size = 16;
      assert(compute_s390x_return_info(&big_struct_ty, &info, &is_sret) == llvm_gen_be_error_t::ok);
      assert(is_sret == true);
      assert(info.abi_class == s390x_abi_class_t::memory);

      a_type empty_struct_ty = {};
      empty_struct_ty.kind = tk_class;
      empty_struct_ty.size = 0;
      assert(classify_s390x_argument(&empty_struct_ty, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == s390x_abi_class_t::no_class);

      llvm::FunctionType* fty = nullptr;
      llvm::AttributeList attrs;
      a_type rty = {};
      rty.kind = tk_routine;
      a_routine_type_supplement rsupp = {};
      rty.variant.routine.extra_info = &rsupp;
      rty.variant.routine.return_type = &int_ty;
      a_param_type p1 = {};
      p1.type = &struct_ty;
      a_param_type p2 = {};
      p2.type = &big_struct_ty;
      p1.next = &p2;
      rsupp.param_type_list = &p1;
      
      assert(build_s390x_function_type(nullptr, &fty, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_s390x_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      assert(fty != nullptr);

      rty.variant.routine.return_type = &big_struct_ty;
      assert(build_s390x_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      rty.variant.routine.return_type = &empty_struct_ty;
      assert(build_s390x_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      
      llvm::Value* va_ptr = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
      llvm::Value* out_val = nullptr;
      assert(lower_s390x_va_start(nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_s390x_va_start(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_s390x_va_start(va_ptr, &out_val) == llvm_gen_be_error_t::ok);

      assert(lower_s390x_va_arg(nullptr, &int_ty, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_s390x_va_arg(va_ptr, nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_s390x_va_arg(va_ptr, &int_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_s390x_va_arg(va_ptr, &int_ty, &out_val) == llvm_gen_be_error_t::ok);

      assert(lower_s390x_va_copy(nullptr, va_ptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_s390x_va_copy(va_ptr, nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_s390x_va_copy(va_ptr, va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_s390x_va_copy(va_ptr, va_ptr, &out_val) == llvm_gen_be_error_t::ok);

      assert(lower_s390x_va_end(nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_s390x_va_end(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_s390x_va_end(va_ptr, &out_val) == llvm_gen_be_error_t::ok);
  }

  printf("PASS\n");
  return 0;
}

namespace edg { 
extern a_byte_boolean int_kind_is_signed[ik_last]; 
a_byte_boolean int_kind_is_signed[ik_last] = {0}; 
}

// Extra test logic for non-trivially copyable and flexible array
// ... I will append a function to test it ...
