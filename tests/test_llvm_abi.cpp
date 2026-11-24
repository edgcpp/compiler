/**
 * @file test_llvm_abi.cpp
 * @ingroup llvm_gen_be_tests
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
  /**
   * @brief Test stub or helper for `is_bool_type`.
   * @details Provides mock or helper behavior for `is_bool_type` within `tests/test_llvm_abi.cpp`.
   * @param[in] ty Parameter `ty` for `is_bool_type`.
   * @return Result of `is_bool_type` (a_boolean).
   */
  a_boolean is_bool_type(a_type_ptr ty) { return ty->kind == tk_integer && ty->variant.integer.int_kind == ik_char; }
  /**
   * @brief Test stub or helper for `f_alignment_of_type`.
   * @details Provides mock or helper behavior for `f_alignment_of_type` within `tests/test_llvm_abi.cpp`.
   * @param[in] ty Parameter `ty` for `f_alignment_of_type`.
   * @return Result of `f_alignment_of_type` (a_targ_alignment).
   */
  a_targ_alignment f_alignment_of_type(a_type_ptr ty) { return (ty && ty->alignment > 0) ? ty->alignment : 4; }
  /**
   * @brief Test stub or helper for `is_trivially_copyable_type`.
   * @details Provides mock or helper behavior for `is_trivially_copyable_type` within `tests/test_llvm_abi.cpp`.
   * @param[in] ty Parameter `ty` for `is_trivially_copyable_type`.
   * @return Result of `is_trivially_copyable_type` (a_boolean).
   */
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

  /**
   * @brief Test stub or helper for `is_reference_type`.
   * @details Provides mock or helper behavior for `is_reference_type` within `tests/test_llvm_abi.cpp`.
   * @param[in] ty Parameter `ty` for `is_reference_type`.
   * @return Result of `is_reference_type` (a_boolean).
   */
  a_boolean is_reference_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `is_empty_class_type`.
   * @details Provides mock or helper behavior for `is_empty_class_type` within `tests/test_llvm_abi.cpp`.
   * @param[in] ty Parameter `ty` for `is_empty_class_type`.
   * @return Result of `is_empty_class_type` (a_boolean).
   */
  a_boolean is_empty_class_type(a_type_ptr ty) { return ty && ty->variant.class_struct_union.is_empty_class; }
  /**
   * @brief Test stub or helper for `is_class_struct_union_type`.
   * @details Provides mock or helper behavior for `is_class_struct_union_type` within `tests/test_llvm_abi.cpp`.
   * @param[in] ty Parameter `ty` for `is_class_struct_union_type`.
   * @return Result of `is_class_struct_union_type` (a_boolean).
   */
  a_boolean is_class_struct_union_type(a_type_ptr ty) { return ty && (ty->kind == tk_class || ty->kind == tk_struct || ty->kind == tk_union); }
  /**
   * @brief Test stub or helper for `llvm_lower_expression`.
   * @details Provides mock or helper behavior for `llvm_lower_expression` within `tests/test_llvm_abi.cpp`.
   * @param[in] expr Parameter `expr` for `llvm_lower_expression`.
   * @param[out] out_val Parameter `out_val` for `llvm_lower_expression`.
   * @return Result of `llvm_lower_expression` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_expression(an_expr_node_ptr expr, llvm::Value** out_val) noexcept {
    if (out_val) *out_val = nullptr;
    return llvm_gen_be_error_t::ok;
  }
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
   * @brief Test stub or helper for `llvm_type_from_edg_type`.
   * @details Provides mock or helper behavior for `llvm_type_from_edg_type` within `tests/test_llvm_abi.cpp`.
   * @param[in] ty Parameter `ty` for `llvm_type_from_edg_type`.
   * @param[out] out_ty Parameter `out_ty` for `llvm_type_from_edg_type`.
   * @return Result of `llvm_type_from_edg_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_type_from_edg_type(a_type_ptr ty, llvm::Type** out_ty) noexcept {
      return get_llvm_type(ty, out_ty);
  }
  /**
   * @brief Test stub or helper for `llvm_type_from_float`.
   * @details Provides mock or helper behavior for `llvm_type_from_float` within `tests/test_llvm_abi.cpp`.
   * @param[in] ty Parameter `ty` for `llvm_type_from_float`.
   * @param[out] out_ty Parameter `out_ty` for `llvm_type_from_float`.
   * @return Result of `llvm_type_from_float` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_type_from_float(a_type_ptr ty, llvm::Type** out_ty) noexcept {
      *out_ty = llvm::Type::getFloatTy(*be_state->context);
      return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub or helper for `get_llvm_type`.
   * @details Provides mock or helper behavior for `get_llvm_type` within `tests/test_llvm_abi.cpp`.
   * @param[in] ty Parameter `ty` for `get_llvm_type`.
   * @param[out] out_ty Parameter `out_ty` for `get_llvm_type`.
   * @return Result of `get_llvm_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_llvm_type(a_type_ptr ty, llvm::Type** out_ty) noexcept {
    if (!out_ty) return llvm_gen_be_error_t::invalid_argument;
    if (!ty) {
       *out_ty = llvm::Type::getInt32Ty(*be_state->context);
       return llvm_gen_be_error_t::ok;
    }
    if (ty->alignment == 255) {
       return llvm_gen_be_error_t::unsupported_type;
    }
    if (ty->alignment == 254) {
       *out_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false);
    } else if (ty->kind == tk_void) {
       *out_ty = llvm::Type::getVoidTy(*be_state->context);
    } else if (ty->kind == tk_integer) {
       *out_ty = llvm::Type::getInt32Ty(*be_state->context);
    } else if (ty->kind == tk_float) {
       *out_ty = (ty->size == 8) ? llvm::Type::getDoubleTy(*be_state->context) : llvm::Type::getFloatTy(*be_state->context);
    } else {
       *out_ty = llvm::Type::getInt32Ty(*be_state->context);
    }
    return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub or helper for `mangled_typeinfo_name`.
   * @details Provides mock or helper behavior for `mangled_typeinfo_name` within `tests/test_llvm_abi.cpp`.
   * @param[in] type Parameter `type` for `mangled_typeinfo_name`.
   * @return Result of `mangled_typeinfo_name` (char*).
   */
  char* mangled_typeinfo_name(a_type_ptr type) {
    return (char*)"_ZTI_mock";
  }
}

/**
 * @brief Entry point for the multi-target ABI lowering unit test suite.
 * @details Exercises all 12 target ABI classification, function type, and varargs routines.
 * @return 0 on success.
 */
int main(void) {
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
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "aarch64-unknown-linux-gnu");
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

      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "arm64-apple-darwin");
      assert(classify_aapcs64_argument(&empty_cpp_struct, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == aapcs64_abi_class_t::no_class); // Darwin ignores it completely
      
      bool is_sret_val = false;
      assert(compute_aapcs64_return_info(&empty_cpp_struct, &info, &is_sret_val) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == aapcs64_abi_class_t::no_class); // Return value is ignored on both

      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "aarch64-unknown-linux-gnu");

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

      a_type sve_vec_ty = {};
      sve_vec_ty.kind = tk_scalable_vector;
      sve_vec_ty.size = 0;
      assert(classify_aapcs64_argument(&sve_vec_ty, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == aapcs64_abi_class_t::float_vector);
      assert(compute_aapcs64_return_info(&sve_vec_ty, &info, &is_sret) == llvm_gen_be_error_t::ok);
      assert(!is_sret && info.abi_class == aapcs64_abi_class_t::float_vector);

      a_type sve_cnt_ty = {};
      sve_cnt_ty.kind = tk_scalable_vector_count;
      sve_cnt_ty.size = 0;
      assert(classify_aapcs64_argument(&sve_cnt_ty, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == aapcs64_abi_class_t::float_vector);
      assert(compute_aapcs64_return_info(&sve_cnt_ty, &info, &is_sret) == llvm_gen_be_error_t::ok);
      assert(!is_sret && info.abi_class == aapcs64_abi_class_t::float_vector);
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

      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "arm64-apple-darwin");
      assert(build_aapcs64_va_list_type(&va_list_ty) == llvm_gen_be_error_t::ok);
      assert(va_list_ty->isPointerTy()); // Darwin uses char* for va_list
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "aarch64-unknown-linux-gnu");

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

      // ARM64EC thunk signature tests
      assert(build_arm64ec_thunk_signature(nullptr, &fty, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_arm64ec_thunk_signature(&rty, nullptr, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_arm64ec_thunk_signature(&rty, &fty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(build_arm64ec_thunk_signature(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      assert(fty != nullptr);
      assert(attrs.hasFnAttr("arm64ec-thunk"));

      rsupp.has_ellipsis = TRUE;
      assert(build_arm64ec_thunk_signature(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      assert(fty->isVarArg());
      assert(fty->getNumParams() == 2); // base param + variadic array pointer
      rsupp.has_ellipsis = FALSE;

      // Windows ARM64 variadic HFA <= 16B (coerced to integer) and > 16B (coerced to reference)
      a_type hfa2_flt = {};
      hfa2_flt.kind = tk_struct;
      hfa2_flt.size = 8;
      a_field hf2_1 = {}, hf2_2 = {};
      hf2_1.type = &float_ty; hf2_2.type = &float_ty; hf2_1.next = &hf2_2;
      hfa2_flt.variant.class_struct_union.field_list = &hf2_1;

      a_type dbl_abi_ty = {};
      dbl_abi_ty.kind = tk_float;
      dbl_abi_ty.size = 8;
      dbl_abi_ty.variant.float_kind = fk_double;
      a_type hfa3_dbl = {};
      hfa3_dbl.kind = tk_struct;
      hfa3_dbl.size = 24;
      a_field hd3_1 = {}, hd3_2 = {}, hd3_3 = {};
      hd3_1.type = &dbl_abi_ty; hd3_2.type = &dbl_abi_ty; hd3_3.type = &dbl_abi_ty;
      hd3_1.next = &hd3_2; hd3_2.next = &hd3_3;
      hfa3_dbl.variant.class_struct_union.field_list = &hd3_1;

      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "aarch64-pc-windows-msvc");
      rsupp.has_ellipsis = TRUE;
      a_param_type wp1 = {}, wp2 = {};
      wp1.type = &hfa2_flt; wp2.type = &hfa3_dbl; wp1.next = &wp2;
      rsupp.param_type_list = &wp1;
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      rsupp.has_ellipsis = FALSE;

      // DO_IL_LOWERING value_returned_as_parameter branches (Linux, Windows with/without this_class, non-ptr, null pointee, bad pointee)
      rsupp.value_returned_as_parameter = TRUE;
      a_type void_ret_ty = {}; void_ret_ty.kind = tk_void;
      rty.variant.routine.return_type = &void_ret_ty;

      // 1) first param is non-pointer (raw_param_ty->kind == tk_pointer False)
      wp1.type = &int_ty; wp1.next = nullptr;
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      // 2) first param is pointer with null pointee (variant.pointer.type != nullptr False)
      a_type ptr_sret = {}; ptr_sret.kind = tk_pointer; ptr_sret.size = 8; ptr_sret.variant.pointer.type = nullptr;
      wp1.type = &ptr_sret;
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      // 3) error branch inside llvm_type_from_edg_type on sret pointee (alignment == 255 -> unsupported_type)
      a_type bad_flt_ptee = {};
      bad_flt_ptee.kind = tk_integer;
      bad_flt_ptee.alignment = 255;
      ptr_sret.variant.pointer.type = &bad_flt_ptee;
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);

      // 4) Windows valid sret pointer with this_class == nullptr vs this_class != nullptr, plus trailing second param
      ptr_sret.variant.pointer.type = &hfa3_dbl;
      wp2.type = &int_ty; wp1.next = &wp2;
      rsupp.this_class = nullptr;
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      rsupp.this_class = &hfa3_dbl;
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      rsupp.this_class = nullptr;

      // 5) Linux valid sret pointer (is_windows False)
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "aarch64-unknown-linux-gnu");
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      rsupp.value_returned_as_parameter = FALSE;
      rsupp.param_type_list = &p1;
      rty.variant.routine.return_type = &int_ty;
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

      // __vectorcall HFA struct (3 floats = 12 bytes)
      a_type vec_hfa_ty = {};
      vec_hfa_ty.kind = tk_struct;
      vec_hfa_ty.size = 12;
      a_field vf1 = {}, vf2 = {}, vf3 = {};
      vf1.type = &float_ty; vf2.type = &float_ty; vf3.type = &float_ty;
      vf1.next = &vf2; vf2.next = &vf3;
      vec_hfa_ty.variant.class_struct_union.field_list = &vf1;

      info = {};
      assert(classify_win64_argument(&vec_hfa_ty, true, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == win64_abi_class_t::vectorcall_hva);
      assert(info.num_elements == 3);
      assert(!info.is_indirect);

      bool is_sret_hfa = false;
      assert(compute_win64_return_info(&vec_hfa_ty, true, &info, &is_sret_hfa) == llvm_gen_be_error_t::ok);
      assert(!is_sret_hfa);
      assert(info.abi_class == win64_abi_class_t::vectorcall_hva);

      // __vectorcall >4 elements falls back to indirect_by_pointer
      a_field vf4 = {}, vf5 = {};
      vf4.type = &float_ty; vf5.type = &float_ty;
      vf3.next = &vf4; vf4.next = &vf5;
      vec_hfa_ty.size = 20;
      info = {};
      assert(classify_win64_argument(&vec_hfa_ty, true, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == win64_abi_class_t::indirect_by_pointer);
      assert(info.is_indirect);
      vf3.next = nullptr;
      vec_hfa_ty.size = 12;

      // Mixed fields struct under __vectorcall falls back to direct_integer (size 8)
      info = {};
      assert(classify_win64_argument(&struct_ty, true, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == win64_abi_class_t::direct_integer);

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

#if MICROSOFT_EXTENSIONS_ALLOWED || GNU_X86_ATTRIBUTES_ALLOWED
      rsupp.calling_convention = cc_vectorcall;
      rty.variant.routine.return_type = &vec_hfa_ty;
      p1.type = &vec_hfa_ty;
      assert(build_win64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      assert(fty != nullptr);
      assert(attrs.hasParamAttr(0, llvm::Attribute::InReg));
      rsupp.calling_convention = cc_default;
      rty.variant.routine.return_type = &int_ty;
      p1.type = &struct_ty;
#endif

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

      // Test ARM32 hard-float HFA detection (detect_arm32_hfa / detect_arm32_hfa_internal)
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "armv7-unknown-linux-gnueabihf");
      a_type arm_hfa_ty = {};
      arm_hfa_ty.kind = tk_struct;
      arm_hfa_ty.size = 8;
      arm_hfa_ty.alignment = 4;
      a_field hf1 = {}, hf2 = {};
      hf1.type = &float_ty; hf2.type = &float_ty;
      hf1.next = &hf2;
      arm_hfa_ty.variant.class_struct_union.field_list = &hf1;

      info = {};
      assert(classify_arm32_argument(&arm_hfa_ty, &info) == llvm_gen_be_error_t::ok);
      assert(info.is_hfa);

      a_type arm_hfa_arr_ty = {};
      arm_hfa_arr_ty.kind = tk_array;
      arm_hfa_arr_ty.size = 12;
      arm_hfa_arr_ty.alignment = 4;
      arm_hfa_arr_ty.variant.array.element_type = &float_ty;
      arm_hfa_arr_ty.variant.array.variant.number_of_elements = 3;
      info = {};
      assert(classify_arm32_argument(&arm_hfa_arr_ty, &info) == llvm_gen_be_error_t::ok);
      assert(info.is_hfa);
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");

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

  // 13. Exhaustive edge-case and branch coverage across all 12 target ABIs
  {
    llvm::Value* va_ptr = state.builder->CreateAlloca(llvm::PointerType::getUnqual(*state.context));
    llvm::Value* out_val = nullptr;
    llvm::FunctionType* fty = nullptr;
    llvm::AttributeList attrs;

    a_type tref_int = {};
    tref_int.kind = tk_typeref;
    tref_int.variant.typeref.type = &int_ty;

    a_type tref_flt = {};
    tref_flt.kind = tk_typeref;
    tref_flt.variant.typeref.type = &float_ty;

    a_type ptr_ty = {};
    ptr_ty.kind = tk_pointer;
    ptr_ty.size = 8;
    ptr_ty.alignment = 8;

    a_type p2m_ty = {};
    p2m_ty.kind = tk_ptr_to_member;
    p2m_ty.size = 8;
    p2m_ty.alignment = 8;

    a_type void_ty = {};
    void_ty.kind = tk_void;
    void_ty.size = 0;

    a_type int1_ty = int_ty; int1_ty.size = 1; int1_ty.alignment = 1;
    a_type int128_ty = int_ty; int128_ty.size = 16; int128_ty.alignment = 16;
    a_type double_ty = float_ty; double_ty.variant.float_kind = fk_double; double_ty.size = 8; double_ty.alignment = 8;
    a_type ldouble_ty = float_ty; ldouble_ty.variant.float_kind = fk_long_double; ldouble_ty.size = 16; ldouble_ty.alignment = 16;
    a_type ldouble8_ty = float_ty; ldouble8_ty.variant.float_kind = fk_long_double; ldouble8_ty.size = 8; ldouble8_ty.alignment = 8;
    a_type f128_ty = float_ty; f128_ty.variant.float_kind = fk_float128; f128_ty.size = 16; f128_ty.alignment = 16;
    a_type stdf128_ty = float_ty; stdf128_ty.variant.float_kind = fk_std_float128; stdf128_ty.size = 16; stdf128_ty.alignment = 16;

    a_type bad_int_ty = int_ty; bad_int_ty.alignment = 255;
    a_type bad_float_ty = float_ty; bad_float_ty.alignment = 255;

    a_type st_sz0 = {}, st_sz1 = {}, st_sz2 = {}, st_sz4 = {}, st_sz8 = {}, st_sz12 = {}, st_sz16 = {}, st_sz32 = {}, st_sz128 = {};
    st_sz0.kind = tk_struct; st_sz0.size = 0;
    st_sz1.kind = tk_struct; st_sz1.size = 1; st_sz1.alignment = 1;
    st_sz2.kind = tk_struct; st_sz2.size = 2; st_sz2.alignment = 2;
    st_sz4.kind = tk_struct; st_sz4.size = 4; st_sz4.alignment = 4;
    st_sz8.kind = tk_struct; st_sz8.size = 8; st_sz8.alignment = 8;
    st_sz12.kind = tk_struct; st_sz12.size = 12; st_sz12.alignment = 4;
    st_sz16.kind = tk_struct; st_sz16.size = 16; st_sz16.alignment = 8;
    st_sz32.kind = tk_struct; st_sz32.size = 32; st_sz32.alignment = 8;
    st_sz128.kind = tk_struct; st_sz128.size = 128; st_sz128.alignment = 8;

    a_type bad_big_st = st_sz128; bad_big_st.alignment = 255;
    a_type bad_small_st = st_sz4; bad_small_st.alignment = 255;

    a_type union_ty = {};
    union_ty.kind = tk_union;
    union_ty.size = 8;
    union_ty.alignment = 4;

    a_field null_type_field = {};
    null_type_field.type = nullptr;
    a_type null_field_st = st_sz8;
    null_field_st.variant.class_struct_union.field_list = &null_type_field;

    a_field bad_flt_field = {};
    bad_flt_field.type = &bad_float_ty;
    a_type bad_flt_st = st_sz4;
    bad_flt_st.variant.class_struct_union.field_list = &bad_flt_field;

    a_field zbf = {}, flt_after_zbf = {};
    zbf.is_bit_field = TRUE; zbf.bit_size = 0; zbf.type = &int_ty; zbf.next = &flt_after_zbf;
    flt_after_zbf.type = &float_ty;
    a_type zbf_flt_st = st_sz4;
    zbf_flt_st.variant.class_struct_union.field_list = &zbf;

    a_field nzbf = {};
    nzbf.is_bit_field = TRUE; nzbf.bit_size = 4; nzbf.type = &int_ty;
    a_type nzbf_st = st_sz4;
    nzbf_st.variant.class_struct_union.field_list = &nzbf;

    a_field mixed_f1 = {}, mixed_f2 = {};
    mixed_f1.type = &float_ty; mixed_f1.next = &mixed_f2;
    mixed_f2.type = &double_ty; mixed_f2.offset = 8;
    a_type mixed_fp_st = st_sz16;
    mixed_fp_st.variant.class_struct_union.field_list = &mixed_f1;

    a_type r_no_supp = {};
    r_no_supp.kind = tk_routine;
    r_no_supp.variant.routine.return_type = &int_ty;
    r_no_supp.variant.routine.extra_info = nullptr;

    a_routine_type_supplement rsupp = {};
    a_type rty = {};
    rty.kind = tk_routine;
    rty.variant.routine.return_type = &int_ty;
    rty.variant.routine.extra_info = &rsupp;
    a_param_type p1 = {}, p2 = {};

    // --- s390x ---
    {
      s390x_arg_info_t info = {};
      assert(classify_s390x_argument(&tref_int, &info) == llvm_gen_be_error_t::ok);
      assert(classify_s390x_argument(&ptr_ty, &info) == llvm_gen_be_error_t::ok);
      assert(classify_s390x_argument(&st_sz1, &info) == llvm_gen_be_error_t::ok);
      assert(classify_s390x_argument(&st_sz2, &info) == llvm_gen_be_error_t::ok);
      assert(classify_s390x_argument(&st_sz4, &info) == llvm_gen_be_error_t::ok);
      assert(classify_s390x_argument(&st_sz8, &info) == llvm_gen_be_error_t::ok);
      assert(classify_s390x_argument(&st_sz12, &info) == llvm_gen_be_error_t::ok);
      bool sret = false;
      assert(compute_s390x_return_info(&bad_int_ty, &info, &sret) == llvm_gen_be_error_t::unsupported_type);
      assert(build_s390x_function_type(&rty, nullptr, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_s390x_function_type(&rty, &fty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(build_s390x_function_type(&r_no_supp, &fty, &attrs) == llvm_gen_be_error_t::ok);
      rty.variant.routine.return_type = &bad_int_ty;
      assert(build_s390x_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rty.variant.routine.return_type = &bad_big_st;
      assert(build_s390x_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rty.variant.routine.return_type = &int_ty;
      p1.type = &st_sz0; p1.next = &p2; p2.type = &bad_int_ty; p2.next = nullptr;
      rsupp.param_type_list = &p1;
      assert(build_s390x_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      p2.type = &bad_big_st;
      assert(build_s390x_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      assert(lower_s390x_va_arg(va_ptr, &bad_int_ty, &out_val) == llvm_gen_be_error_t::unsupported_type);
    }

    // --- sparc ---
    {
      sparc_arg_info_t info = {};
      assert(classify_sparc_argument(&tref_int, true, &info) == llvm_gen_be_error_t::ok);
      assert(classify_sparc_argument(&ptr_ty, true, &info) == llvm_gen_be_error_t::ok);
      bool sret = false;
      assert(compute_sparc_return_info(&bad_int_ty, true, &info, &sret) == llvm_gen_be_error_t::unsupported_type);
      assert(build_sparc_function_type(&rty, nullptr, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_sparc_function_type(&rty, &fty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(build_sparc_function_type(&r_no_supp, &fty, &attrs) == llvm_gen_be_error_t::ok);
      targ_sizeof_pointer = 4; // SPARC V8
      rty.variant.routine.return_type = &st_sz16;
      p1.type = &st_sz0; p1.next = &p2; p2.type = &st_sz8; p2.next = nullptr;
      rsupp.param_type_list = &p1;
      assert(build_sparc_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      targ_sizeof_pointer = 8;
      rty.variant.routine.return_type = &bad_int_ty;
      assert(build_sparc_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rty.variant.routine.return_type = &bad_big_st;
      assert(build_sparc_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rty.variant.routine.return_type = &int_ty;
      p1.type = &bad_int_ty; p1.next = nullptr;
      assert(build_sparc_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      p1.type = &bad_big_st;
      assert(build_sparc_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      assert(lower_sparc_va_arg(va_ptr, &bad_int_ty, &out_val) == llvm_gen_be_error_t::unsupported_type);
    }

    // --- wasm ---
    {
      wasm_arg_info_t info = {};
      assert(classify_wasm_argument(&tref_int, &info) == llvm_gen_be_error_t::ok);
      assert(classify_wasm_argument(&st_sz8, &info) == llvm_gen_be_error_t::ok);
      assert(classify_wasm_argument(&union_ty, &info) == llvm_gen_be_error_t::ok);
      assert(classify_wasm_argument(&nzbf_st, &info) == llvm_gen_be_error_t::ok);
      assert(info.is_indirect);

      // Single-scalar struct and nested single-scalar class for wasm
      a_field wf_flt = {}; wf_flt.type = &float_ty;
      a_type wasm_single_flt_st = st_sz4; wasm_single_flt_st.variant.class_struct_union.field_list = &wf_flt;
      assert(classify_wasm_argument(&wasm_single_flt_st, &info) == llvm_gen_be_error_t::ok);
      assert(!info.is_indirect);

      a_field wf_ptr = {}; wf_ptr.type = &ptr_ty;
      a_type wasm_single_ptr_cls = st_sz8; wasm_single_ptr_cls.kind = tk_class;
      wasm_single_ptr_cls.variant.class_struct_union.field_list = &wf_ptr;
      a_field wf_nested = {}; wf_nested.type = &wasm_single_ptr_cls;
      a_type wasm_nested_st = st_sz8; wasm_nested_st.variant.class_struct_union.field_list = &wf_nested;
      assert(classify_wasm_argument(&wasm_nested_st, &info) == llvm_gen_be_error_t::ok);
      assert(!info.is_indirect);

      assert(build_wasm_function_type(nullptr, &fty, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_wasm_function_type(&rty, nullptr, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_wasm_function_type(&rty, &fty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      r_no_supp.variant.routine.return_type = &void_ty;
      assert(build_wasm_function_type(&r_no_supp, &fty, &attrs) == llvm_gen_be_error_t::ok);
      r_no_supp.variant.routine.return_type = &wasm_nested_st;
      assert(build_wasm_function_type(&r_no_supp, &fty, &attrs) == llvm_gen_be_error_t::ok);
      r_no_supp.variant.routine.return_type = &int_ty;
      rty.variant.routine.return_type = &bad_int_ty;
      assert(build_wasm_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rty.variant.routine.return_type = &st_sz8; // indirect sret
      a_param_type wp_s = {}; wp_s.type = &wasm_single_flt_st; wp_s.next = &p2;
      p1.type = &int_ty; p1.next = &wp_s; p2.type = &void_ty; p2.next = nullptr;
      rsupp.param_type_list = &p1;
      assert(build_wasm_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      p1.type = &bad_int_ty;
      assert(build_wasm_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
    }

    // --- x86_32 ---
    {
      x86_32_arg_info_t info = {};
      assert(classify_x86_32_argument(&tref_int, &info) == llvm_gen_be_error_t::ok);
      assert(classify_x86_32_argument(&st_sz8, &info) == llvm_gen_be_error_t::ok);
      assert(classify_x86_32_argument(&union_ty, &info) == llvm_gen_be_error_t::ok);
      assert(classify_x86_32_argument(&st_sz16, &info) == llvm_gen_be_error_t::ok);
      assert(build_x86_32_function_type(nullptr, &fty, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_x86_32_function_type(&rty, nullptr, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_x86_32_function_type(&rty, &fty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(build_x86_32_function_type(&r_no_supp, &fty, &attrs) == llvm_gen_be_error_t::ok);

      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "i686-pc-windows-msvc");
      rsupp.calling_convention = cc_thiscall;
      rty.variant.routine.return_type = &st_sz16; // sret + is_msvc + is_thiscall
      a_param_type p3 = {}, p4 = {}, p5 = {};
      p1.type = &int_ty; p1.next = &p2;
      p2.type = &st_sz16; p2.next = &p3;
      p3.type = &void_ty; p3.next = nullptr;
      rsupp.param_type_list = &p1;
      assert(build_x86_32_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      // is_thiscall with indirect p1 (arg_idx == 0 && p_info.is_indirect)
      p1.type = &st_sz16;
      assert(build_x86_32_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      // is_msvc && !is_thiscall with sret, and coerced_type return (st_sz4)
      rsupp.calling_convention = cc_cdecl;
      assert(build_x86_32_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      rty.variant.routine.return_type = &st_sz4;
      assert(build_x86_32_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      // non-MSVC sret
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "i686-unknown-linux-gnu");
      rty.variant.routine.return_type = &st_sz16;
      assert(build_x86_32_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      // fastcall with indirect param, pointer, float, large int, and >2 regs
      rsupp.calling_convention = cc_fastcall;
      rty.variant.routine.return_type = &int_ty;
      a_param_type p0_ind = {};
      p0_ind.type = &st_sz16; p0_ind.next = &p1;
      p1.type = &ptr_ty; ptr_ty.size = 4; p1.next = &p2;
      p2.type = &float_ty; p2.next = &p3;
      p3.type = &int128_ty; p3.next = &p4;
      p4.type = &int_ty; p4.next = &p5;
      p5.type = &int_ty; p5.next = nullptr;
      rsupp.param_type_list = &p0_ind;
      assert(build_x86_32_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      ptr_ty.size = 8;

      rty.variant.routine.return_type = &bad_int_ty;
      assert(build_x86_32_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rty.variant.routine.return_type = &int_ty;
      p0_ind.type = &bad_int_ty;
      assert(build_x86_32_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rsupp.calling_convention = cc_default;
    }

    // --- arm32 ---
    {
      arm32_arg_info_t info = {};
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "armv7-unknown-none-eabihf");
      assert(classify_arm32_argument(&tref_int, &info) == llvm_gen_be_error_t::ok);
      assert(classify_arm32_argument(&float_ty, &info) == llvm_gen_be_error_t::ok);
      assert(classify_arm32_argument(&zbf_flt_st, &info) == llvm_gen_be_error_t::ok);
      assert(classify_arm32_argument(&nzbf_st, &info) == llvm_gen_be_error_t::ok);
      assert(classify_arm32_argument(&mixed_fp_st, &info) == llvm_gen_be_error_t::ok);
      assert(classify_arm32_argument(&null_field_st, &info) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_arm32_argument(&bad_flt_st, &info) == llvm_gen_be_error_t::unsupported_type);
      assert(classify_arm32_argument(&st_sz0, &info) == llvm_gen_be_error_t::ok);
      assert(classify_arm32_argument(&st_sz128, &info) == llvm_gen_be_error_t::ok);
      assert(classify_arm32_argument(&union_ty, &info) == llvm_gen_be_error_t::ok);

      a_type cls_arm = st_sz8; cls_arm.kind = tk_class;
      assert(classify_arm32_argument(&cls_arm, &info) == llvm_gen_be_error_t::ok);

      a_field tf_f = {}; tf_f.type = &tref_flt;
      a_type tref_flt_st = st_sz4; tref_flt_st.variant.class_struct_union.field_list = &tf_f;
      assert(classify_arm32_argument(&tref_flt_st, &info) == llvm_gen_be_error_t::ok);

      a_type arr_5flt = {}; arr_5flt.kind = tk_array; arr_5flt.size = 20; arr_5flt.alignment = 4;
      arr_5flt.variant.array.element_type = &float_ty; arr_5flt.variant.array.variant.number_of_elements = 5;
      assert(classify_arm32_argument(&arr_5flt, &info) == llvm_gen_be_error_t::ok);

      a_type arr_badflt = arr_5flt; arr_badflt.variant.array.element_type = &bad_float_ty;
      assert(classify_arm32_argument(&arr_badflt, &info) == llvm_gen_be_error_t::unsupported_type);

      a_type arr_int = arr_5flt; arr_int.variant.array.element_type = &int_ty;
      assert(classify_arm32_argument(&arr_int, &info) == llvm_gen_be_error_t::ok);

      a_type align8_rem1 = st_sz12; align8_rem1.alignment = 8;
      assert(classify_arm32_argument(&align8_rem1, &info) == llvm_gen_be_error_t::ok);
      a_type align8_rem2 = st_sz12; align8_rem2.size = 14; align8_rem2.alignment = 8; // rem_bytes = 6 -> rem_ints = 2
      assert(classify_arm32_argument(&align8_rem2, &info) == llvm_gen_be_error_t::ok);

      assert(build_arm32_function_type(nullptr, &fty, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_arm32_function_type(&rty, nullptr, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_arm32_function_type(&rty, &fty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(build_arm32_function_type(&r_no_supp, &fty, &attrs) == llvm_gen_be_error_t::ok);

      rty.variant.routine.return_type = &zbf_flt_st; // HFA return (size 4)
      a_type big_align1 = st_sz128; big_align1.alignment = 1; // p_info.align < 4
      a_param_type p3 = {}, p4 = {};
      p1.type = &st_sz128; p1.next = &p2;
      p2.type = &big_align1; p2.next = &p3;
      p3.type = &int_ty; p3.next = &p4;
      p4.type = &void_ty; p4.next = nullptr;
      rsupp.param_type_list = &p1;
      assert(build_arm32_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      rty.variant.routine.return_type = &cls_arm; // tk_class sret return
      assert(build_arm32_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      rty.variant.routine.return_type = &union_ty; // tk_union sret return
      assert(build_arm32_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      rty.variant.routine.return_type = &st_sz16; // tk_struct sret return
      assert(build_arm32_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "armv7-none-eabi"); // soft-float
      rty.variant.routine.return_type = &st_sz4; // coerced non-sret struct return
      assert(build_arm32_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "armv7-unknown-none-eabihf");
      rty.variant.routine.return_type = &bad_int_ty;
      assert(build_arm32_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rty.variant.routine.return_type = &bad_flt_st;
      assert(build_arm32_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rty.variant.routine.return_type = &int_ty;
      p1.type = &bad_int_ty;
      assert(build_arm32_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      p1.type = &bad_flt_st;
      assert(build_arm32_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
    }

    // --- loongarch64 ---
    {
      loongarch64_arg_info_t info = {};
      assert(classify_loongarch64_argument(&tref_int, &info) == llvm_gen_be_error_t::ok);
      assert(classify_loongarch64_argument(&ptr_ty, &info) == llvm_gen_be_error_t::ok);
      assert(classify_loongarch64_argument(&union_ty, &info) == llvm_gen_be_error_t::ok);
      assert(classify_loongarch64_argument(&st_sz8, &info) == llvm_gen_be_error_t::ok); // empty field_list -> fields.empty()
      assert(classify_loongarch64_argument(&null_field_st, &info) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_loongarch64_argument(&zbf_flt_st, &info) == llvm_gen_be_error_t::ok); // 2 floats with bitfields
      assert(classify_loongarch64_argument(&nzbf_st, &info) == llvm_gen_be_error_t::ok);

      a_field flt1_only = {}; flt1_only.type = &float_ty;
      a_type st_1flt = st_sz4; st_1flt.variant.class_struct_union.field_list = &flt1_only;
      assert(classify_loongarch64_argument(&st_1flt, &info) == llvm_gen_be_error_t::ok); // 1 float (size 4)

      a_field d1 = {}, d2 = {};
      d1.type = &double_ty; d2.type = &double_ty; d2.offset = 8;
      a_type st_1dbl = st_sz8; st_1dbl.variant.class_struct_union.field_list = &d1;
      assert(classify_loongarch64_argument(&st_1dbl, &info) == llvm_gen_be_error_t::ok); // 1 double (size 8)
      d1.next = &d2;
      a_type st_2dbl = st_sz16; st_2dbl.variant.class_struct_union.field_list = &d1;
      assert(classify_loongarch64_argument(&st_2dbl, &info) == llvm_gen_be_error_t::ok); // 2 doubles

      // float + ptr and double + int
      a_field fi_1 = {}, fi_2 = {};
      fi_1.type = &float_ty; fi_1.next = &fi_2; fi_2.type = &ptr_ty; fi_2.offset = 4;
      a_type st_fi = st_sz12; st_fi.variant.class_struct_union.field_list = &fi_1;
      assert(classify_loongarch64_argument(&st_fi, &info) == llvm_gen_be_error_t::ok);
      fi_1.type = &double_ty; fi_2.type = &int_ty; fi_2.offset = 8; st_fi.size = 12;
      assert(classify_loongarch64_argument(&st_fi, &info) == llvm_gen_be_error_t::ok);

      // int + double
      fi_1.type = &int_ty; fi_2.type = &double_ty;
      assert(classify_loongarch64_argument(&st_fi, &info) == llvm_gen_be_error_t::ok);

      // 1 int struct (size <= 8) and 2 int struct (size > 8)
      fi_1.next = nullptr; st_fi.size = 4;
      assert(classify_loongarch64_argument(&st_fi, &info) == llvm_gen_be_error_t::ok);
      fi_1.next = &fi_2; fi_2.type = &int_ty; st_fi.size = 16;
      assert(classify_loongarch64_argument(&st_fi, &info) == llvm_gen_be_error_t::ok);

      // struct containing array of floats, array of unions, and array of null_field_st
      a_type arr_2flt = {}; arr_2flt.kind = tk_array; arr_2flt.size = 8;
      arr_2flt.variant.array.element_type = &tref_flt; arr_2flt.variant.array.variant.number_of_elements = 2;
      a_field af = {}; af.type = &arr_2flt;
      a_type st_arr = st_sz8; st_arr.variant.class_struct_union.field_list = &af;
      assert(classify_loongarch64_argument(&st_arr, &info) == llvm_gen_be_error_t::ok);

      arr_2flt.variant.array.element_type = &union_ty;
      assert(classify_loongarch64_argument(&st_arr, &info) == llvm_gen_be_error_t::ok);

      arr_2flt.variant.array.element_type = nullptr;
      assert(classify_loongarch64_argument(&st_arr, &info) == llvm_gen_be_error_t::invalid_argument);

      bool sret = false;
      assert(compute_loongarch64_return_info(&bad_int_ty, &info, &sret) == llvm_gen_be_error_t::unsupported_type);
      assert(build_loongarch64_function_type(&rty, nullptr, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_loongarch64_function_type(&rty, &fty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(build_loongarch64_function_type(&r_no_supp, &fty, &attrs) == llvm_gen_be_error_t::ok);

      rty.variant.routine.return_type = &bad_int_ty;
      assert(build_loongarch64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rty.variant.routine.return_type = &bad_big_st;
      assert(build_loongarch64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rty.variant.routine.return_type = &int_ty;
      p1.type = &bad_int_ty; p1.next = nullptr; rsupp.param_type_list = &p1;
      assert(build_loongarch64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      p1.type = &bad_big_st;
      assert(build_loongarch64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      assert(lower_loongarch64_va_arg(va_ptr, &bad_int_ty, &out_val) == llvm_gen_be_error_t::unsupported_type);
    }

    // --- mips ---
    {
      mips_arg_info_t info = {};
      assert(classify_mips_argument(&tref_int, false, true, &info) == llvm_gen_be_error_t::ok);
      assert(classify_mips_argument(&ptr_ty, false, true, &info) == llvm_gen_be_error_t::ok);
      assert(classify_mips_argument(&float_ty, true, true, &info) == llvm_gen_be_error_t::ok);
      assert(classify_mips_argument(&struct_ty, true, true, &info) == llvm_gen_be_error_t::ok); // soft-float N64 struct

      // Union, struct, array, ptr, p2m, zero bitfield, non-scalar field, 2-int struct, 3-field struct, empty struct, error in flatten
      a_field mf1 = {}, mf2 = {}, mf3 = {};
      mf1.type = &ptr_ty; mf1.next = &mf2; mf2.type = &p2m_ty; mf2.offset = 8;
      a_type st_2int = st_sz16; st_2int.variant.class_struct_union.field_list = &mf1;
      assert(classify_mips_argument(&st_2int, false, true, &info) == llvm_gen_be_error_t::ok); // !has_fp

      mf2.next = &mf3; mf3.type = &float_ty;
      assert(classify_mips_argument(&st_2int, false, true, &info) == llvm_gen_be_error_t::ok); // fields.size() > 2

      assert(classify_mips_argument(&st_sz8, false, true, &info) == llvm_gen_be_error_t::ok); // fields.empty()
      assert(classify_mips_argument(&zbf_flt_st, false, true, &info) == llvm_gen_be_error_t::ok);
      assert(classify_mips_argument(&nzbf_st, false, true, &info) == llvm_gen_be_error_t::ok);
      assert(classify_mips_argument(&null_field_st, false, true, &info) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_mips_argument(&bad_flt_st, false, true, &info) == llvm_gen_be_error_t::unsupported_type);

      // Union and array in flatten_mips_struct
      a_field uf = {}; uf.type = &float_ty;
      union_ty.variant.class_struct_union.field_list = &uf;
      assert(classify_mips_argument(&union_ty, false, true, &info) == llvm_gen_be_error_t::ok);

      a_type arr_1flt = {}; arr_1flt.kind = tk_array; arr_1flt.size = 4;
      arr_1flt.variant.array.element_type = &float_ty; arr_1flt.variant.array.variant.number_of_elements = 1;
      assert(classify_mips_argument(&arr_1flt, false, true, &info) == llvm_gen_be_error_t::ok); // !is_class_struct_union_type in detect_mips_flattening
      a_field arr_f = {}; arr_f.type = &arr_1flt;
      a_type st_with_arr = st_sz4; st_with_arr.variant.class_struct_union.field_list = &arr_f;
      assert(classify_mips_argument(&st_with_arr, false, true, &info) == llvm_gen_be_error_t::ok);
      arr_1flt.variant.array.element_type = &null_field_st;
      assert(classify_mips_argument(&st_with_arr, false, true, &info) == llvm_gen_be_error_t::invalid_argument);

      a_field rout_f = {}; rout_f.type = &r_no_supp;
      a_type st_with_rout = st_sz4; st_with_rout.variant.class_struct_union.field_list = &rout_f;
      assert(classify_mips_argument(&st_with_rout, false, true, &info) == llvm_gen_be_error_t::ok);

      bool sret = false;
      assert(compute_mips_return_info(&bad_int_ty, false, true, &info, &sret) == llvm_gen_be_error_t::unsupported_type);
      assert(compute_mips_return_info(&int_ty, false, false, &info, &sret) == llvm_gen_be_error_t::ok);

      assert(build_mips_function_type(&rty, nullptr, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_mips_function_type(&rty, &fty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(build_mips_function_type(&r_no_supp, &fty, &attrs) == llvm_gen_be_error_t::ok);

      targ_sizeof_pointer = 4;
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "mips-unknown-linux-gnu");
      assert(build_mips_function_type(&r_no_supp, &fty, &attrs) == llvm_gen_be_error_t::ok);
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "mips64-unknown-linux-gnu");
      assert(build_mips_function_type(&r_no_supp, &fty, &attrs) == llvm_gen_be_error_t::ok);
      targ_sizeof_pointer = 8;

      rty.variant.routine.return_type = &bad_int_ty;
      assert(build_mips_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rty.variant.routine.return_type = &bad_big_st;
      assert(build_mips_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rty.variant.routine.return_type = &int_ty;
      p1.type = &st_sz0; p1.next = &p2; p2.type = &bad_int_ty; p2.next = nullptr; rsupp.param_type_list = &p1;
      assert(build_mips_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      p2.type = &bad_big_st;
      assert(build_mips_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      assert(lower_mips_va_arg(va_ptr, &bad_int_ty, &out_val) == llvm_gen_be_error_t::unsupported_type);
    }

    // --- ppc64 ---
    {
      ppc64_arg_info_t info = {};
      assert(classify_ppc64_argument(&tref_int, true, &info) == llvm_gen_be_error_t::ok);
      assert(classify_ppc64_argument(&ptr_ty, true, &info) == llvm_gen_be_error_t::ok);
      assert(classify_ppc64_argument(&zbf_flt_st, true, &info) == llvm_gen_be_error_t::ok);
      assert(classify_ppc64_argument(&nzbf_st, true, &info) == llvm_gen_be_error_t::ok);
      assert(classify_ppc64_argument(&mixed_fp_st, true, &info) == llvm_gen_be_error_t::ok);
      assert(classify_ppc64_argument(&null_field_st, true, &info) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_ppc64_argument(&bad_flt_st, true, &info) == llvm_gen_be_error_t::unsupported_type);
      assert(classify_ppc64_argument(&st_sz8, true, &info) == llvm_gen_be_error_t::ok); // 0 fields -> *out_count == 0
      assert(classify_ppc64_argument(&st_sz8, false, &info) == llvm_gen_be_error_t::ok); // is_elfv2 == false

      a_type arr_ppc = {}; arr_ppc.kind = tk_array; arr_ppc.size = 8;
      arr_ppc.variant.array.element_type = &tref_flt; arr_ppc.variant.array.variant.number_of_elements = 2;
      assert(classify_ppc64_argument(&arr_ppc, true, &info) == llvm_gen_be_error_t::ok);
      arr_ppc.variant.array.variant.number_of_elements = 0;
      assert(classify_ppc64_argument(&arr_ppc, true, &info) == llvm_gen_be_error_t::ok);
      arr_ppc.variant.array.variant.number_of_elements = 9;
      assert(classify_ppc64_argument(&arr_ppc, true, &info) == llvm_gen_be_error_t::ok);
      arr_ppc.variant.array.element_type = &int_ty;
      assert(classify_ppc64_argument(&arr_ppc, true, &info) == llvm_gen_be_error_t::ok);
      arr_ppc.variant.array.element_type = &bad_float_ty;
      assert(classify_ppc64_argument(&arr_ppc, true, &info) == llvm_gen_be_error_t::unsupported_type);

      // 9-float struct in is_hfa_hva (*out_count > 8)
      a_field pf[9] = {};
      for (int i = 0; i < 9; ++i) {
        pf[i].type = &float_ty;
        pf[i].next = (i + 1 < 9) ? &pf[i + 1] : nullptr;
      }
      a_type st_9flt = st_sz32; st_9flt.size = 36; st_9flt.variant.class_struct_union.field_list = &pf[0];
      assert(classify_ppc64_argument(&st_9flt, true, &info) == llvm_gen_be_error_t::ok);

      bool sret = false;
      assert(compute_ppc64_return_info(&bad_int_ty, true, &info, &sret) == llvm_gen_be_error_t::unsupported_type);
      assert(compute_ppc64_return_info(&int_ty, true, &info, &sret) == llvm_gen_be_error_t::ok);
      assert(compute_ppc64_return_info(&st_sz8, false, &info, &sret) == llvm_gen_be_error_t::ok);
      assert(compute_ppc64_return_info(&st_sz12, false, &info, &sret) == llvm_gen_be_error_t::ok);

      assert(build_ppc64_function_type(&rty, nullptr, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_ppc64_function_type(&rty, &fty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(build_ppc64_function_type(&r_no_supp, &fty, &attrs) == llvm_gen_be_error_t::ok);

      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "powerpc64-unknown-linux-gnu"); // ELFv1 (is_elfv2 == false)
      r_no_supp.variant.routine.return_type = &st_sz8; // non-sret, coerced_type == nullptr -> calls llvm_type_from_edg_type
      assert(build_ppc64_function_type(&r_no_supp, &fty, &attrs) == llvm_gen_be_error_t::ok);
      r_no_supp.variant.routine.return_type = &bad_small_st;
      assert(build_ppc64_function_type(&r_no_supp, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      r_no_supp.variant.routine.return_type = &int_ty;
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "powerpc64le-unknown-linux-gnu");

      rty.variant.routine.return_type = &bad_int_ty;
      assert(build_ppc64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rty.variant.routine.return_type = &bad_big_st;
      assert(build_ppc64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rty.variant.routine.return_type = &int_ty;
      p1.type = &st_sz0; p1.next = &p2; p2.type = &bad_int_ty; p2.next = nullptr; rsupp.param_type_list = &p1;
      assert(build_ppc64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      p2.type = &bad_big_st;
      assert(build_ppc64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      assert(lower_ppc64_va_arg(va_ptr, &bad_int_ty, &out_val) == llvm_gen_be_error_t::unsupported_type);
    }

    // --- riscv64 ---
    {
      riscv64_arg_info_t info = {};
      assert(classify_riscv64_argument(&tref_int, &info) == llvm_gen_be_error_t::ok);
      assert(classify_riscv64_argument(&union_ty, &info) == llvm_gen_be_error_t::ok);
      assert(classify_riscv64_argument(&zbf_flt_st, &info) == llvm_gen_be_error_t::ok);
      assert(classify_riscv64_argument(&null_field_st, &info) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_riscv64_argument(&bad_flt_st, &info) == llvm_gen_be_error_t::unsupported_type);
      assert(classify_riscv64_argument(&st_sz0, &info) == llvm_gen_be_error_t::ok);
      assert(classify_riscv64_argument(&st_sz8, &info) == llvm_gen_be_error_t::ok); // fields.empty()
      assert(classify_riscv64_argument(&st_sz32, &info) == llvm_gen_be_error_t::ok); // size > 16

      // Non-zero bitfield, nested union, array of floats, array of unions, array of null, non-scalar field, size > 8 field, 2-int struct, 3-field struct
      a_field nz_bf = {}; nz_bf.is_bit_field = TRUE; nz_bf.bit_size = 4; nz_bf.type = &int_ty;
      a_type st_nzbf = st_sz4; st_nzbf.variant.class_struct_union.field_list = &nz_bf;
      assert(classify_riscv64_argument(&st_nzbf, &info) == llvm_gen_be_error_t::ok);

      a_field uf_nest = {}; uf_nest.type = &union_ty;
      a_type st_un_nest = st_sz8; st_un_nest.variant.class_struct_union.field_list = &uf_nest;
      assert(classify_riscv64_argument(&st_un_nest, &info) == llvm_gen_be_error_t::ok);

      a_type arr_rv = {}; arr_rv.kind = tk_array; arr_rv.size = 8;
      arr_rv.variant.array.element_type = &float_ty; arr_rv.variant.array.variant.number_of_elements = 2;
      a_field arr_rv_f = {}; arr_rv_f.type = &arr_rv;
      a_type st_arr_rv = st_sz8; st_arr_rv.variant.class_struct_union.field_list = &arr_rv_f;
      assert(classify_riscv64_argument(&st_arr_rv, &info) == llvm_gen_be_error_t::ok);
      arr_rv.variant.array.element_type = &union_ty;
      assert(classify_riscv64_argument(&st_arr_rv, &info) == llvm_gen_be_error_t::ok);
      arr_rv.variant.array.element_type = nullptr;
      assert(classify_riscv64_argument(&st_arr_rv, &info) == llvm_gen_be_error_t::invalid_argument);

      a_field rout_rv_f = {}; rout_rv_f.type = &r_no_supp;
      a_type st_rout_rv = st_sz8; st_rout_rv.variant.class_struct_union.field_list = &rout_rv_f;
      assert(classify_riscv64_argument(&st_rout_rv, &info) == llvm_gen_be_error_t::ok);

      a_field f_big1 = {}, f_big2 = {}, f_big3 = {};
      f_big1.type = &float_ty; f_big1.next = &f_big2; f_big2.type = &ldouble_ty;
      a_type st_fbig = st_sz16; st_fbig.variant.class_struct_union.field_list = &f_big1;
      assert(classify_riscv64_argument(&st_fbig, &info) == llvm_gen_be_error_t::ok); // f_ty->size > 8 -> sizes_valid = false

      f_big1.type = &ptr_ty; f_big2.type = &p2m_ty;
      assert(classify_riscv64_argument(&st_fbig, &info) == llvm_gen_be_error_t::ok); // !has_fp

      f_big2.next = &f_big3; f_big3.type = &float_ty;
      assert(classify_riscv64_argument(&st_fbig, &info) == llvm_gen_be_error_t::ok); // fields.size() > 2

      bool sret = false;
      assert(compute_riscv64_return_info(nullptr, &info, &sret) == llvm_gen_be_error_t::invalid_argument);
      assert(compute_riscv64_return_info(&int_ty, nullptr, &sret) == llvm_gen_be_error_t::invalid_argument);
      assert(compute_riscv64_return_info(&int_ty, &info, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(compute_riscv64_return_info(&bad_flt_st, &info, &sret) == llvm_gen_be_error_t::unsupported_type);
      assert(compute_riscv64_return_info(&st_sz32, &info, &sret) == llvm_gen_be_error_t::ok);

      assert(build_riscv64_function_type(&r_no_supp, &fty, &attrs) == llvm_gen_be_error_t::ok);
      a_type non_triv_rv = {}; non_triv_rv.kind = tk_class; non_triv_rv.size = 99;
      int_kind_is_signed[ik_int] = TRUE;
      int_kind_is_signed[ik_char] = FALSE;
      a_type uint1_rv = int1_ty; uint1_rv.variant.integer.int_kind = ik_char;
      a_type int8_rv = int_ty; int8_rv.size = 8;
      a_param_type rvp1 = {}, rvp2 = {}, rvp3 = {}, rvp4 = {}, rvp5 = {};
      rvp1.type = &st_sz32; rvp1.next = &rvp2;
      rvp2.type = &non_triv_rv; rvp2.next = &rvp3;
      rvp3.type = &int_ty; rvp3.next = &rvp4;
      rvp4.type = &uint1_rv; rvp4.next = &rvp5;
      rvp5.type = &int8_rv; rvp5.next = &p2;
      p2.type = &float_ty; p2.next = nullptr;
      rsupp.param_type_list = &rvp1;
      rty.variant.routine.return_type = &uint1_rv; // unsigned <=4 return
      assert(build_riscv64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      rty.variant.routine.return_type = &int8_rv; // >4 integer return
      assert(build_riscv64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      rty.variant.routine.return_type = &float_ty; // non-integer return
      assert(build_riscv64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      rty.variant.routine.return_type = &st_sz32; // sret
      assert(build_riscv64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      rty.variant.routine.return_type = &struct_ty; // coerced_type return
      assert(build_riscv64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      rty.variant.routine.return_type = &bad_flt_st;
      assert(build_riscv64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rty.variant.routine.return_type = &bad_big_st;
      assert(build_riscv64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rty.variant.routine.return_type = &bad_int_ty;
      assert(build_riscv64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rty.variant.routine.return_type = &int_ty;
      p1.type = &bad_flt_st; p1.next = nullptr; rsupp.param_type_list = &p1;
      assert(build_riscv64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      p1.type = &bad_int_ty;
      assert(build_riscv64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      assert(lower_riscv64_va_arg(va_ptr, &bad_flt_st, &out_val) == llvm_gen_be_error_t::unsupported_type);
      assert(lower_riscv64_va_arg(va_ptr, &bad_int_ty, &out_val) == llvm_gen_be_error_t::unsupported_type);
      assert(lower_riscv64_va_arg(va_ptr, &st_sz32, &out_val) == llvm_gen_be_error_t::ok);
    }

    // --- win64 ---
    {
      win64_arg_info_t info = {};
      assert(classify_win64_argument(&tref_int, false, &info) == llvm_gen_be_error_t::ok);
      assert(classify_win64_argument(&int128_ty, false, &info) == llvm_gen_be_error_t::ok);
      assert(classify_win64_argument(&ptr_ty, false, &info) == llvm_gen_be_error_t::ok);
      assert(classify_win64_argument(&p2m_ty, false, &info) == llvm_gen_be_error_t::ok);
      assert(classify_win64_argument(&bad_float_ty, true, &info) == llvm_gen_be_error_t::unsupported_type);
      assert(classify_win64_argument(&union_ty, true, &info) == llvm_gen_be_error_t::ok);
      assert(classify_win64_argument(&st_sz1, false, &info) == llvm_gen_be_error_t::ok);
      assert(classify_win64_argument(&st_sz2, false, &info) == llvm_gen_be_error_t::ok);
      assert(classify_win64_argument(&st_sz4, false, &info) == llvm_gen_be_error_t::ok);
      assert(classify_win64_argument(&st_sz12, false, &info) == llvm_gen_be_error_t::ok);
      assert(classify_win64_argument(&void_ty, false, &info) == llvm_gen_be_error_t::ok);

      // vectorcall HVA internal branches
      a_type tref_flt = {}; tref_flt.kind = tk_typeref; tref_flt.variant.typeref.type = &float_ty;
      a_field w_tf = {}; w_tf.type = &tref_flt;
      a_type st_w_tref = st_sz4; st_w_tref.variant.class_struct_union.field_list = &w_tf;
      assert(classify_win64_argument(&st_w_tref, true, &info) == llvm_gen_be_error_t::ok);
      assert(classify_win64_argument(&zbf_flt_st, true, &info) == llvm_gen_be_error_t::ok);
      assert(classify_win64_argument(&nzbf_st, true, &info) == llvm_gen_be_error_t::ok);
      assert(classify_win64_argument(&mixed_fp_st, true, &info) == llvm_gen_be_error_t::ok);
      assert(classify_win64_argument(&st_sz8, true, &info) == llvm_gen_be_error_t::ok); // field_list == nullptr
      assert(classify_win64_argument(&null_field_st, true, &info) == llvm_gen_be_error_t::invalid_argument);
      assert(classify_win64_argument(&bad_flt_st, true, &info) == llvm_gen_be_error_t::unsupported_type);

      a_type arr_w = {}; arr_w.kind = tk_array; arr_w.size = 8;
      arr_w.variant.array.element_type = &float_ty; arr_w.variant.array.variant.number_of_elements = 2;
      assert(classify_win64_argument(&arr_w, true, &info) == llvm_gen_be_error_t::ok);
      arr_w.variant.array.variant.number_of_elements = 0;
      assert(classify_win64_argument(&arr_w, true, &info) == llvm_gen_be_error_t::ok);
      arr_w.variant.array.variant.number_of_elements = 2; arr_w.variant.array.element_type = &int_ty;
      assert(classify_win64_argument(&arr_w, true, &info) == llvm_gen_be_error_t::ok);
      arr_w.variant.array.element_type = &bad_float_ty;
      assert(classify_win64_argument(&arr_w, true, &info) == llvm_gen_be_error_t::unsupported_type);

      bool sret = false;
      assert(compute_win64_return_info(nullptr, false, &info, &sret) == llvm_gen_be_error_t::invalid_argument);
      assert(compute_win64_return_info(&int_ty, false, nullptr, &sret) == llvm_gen_be_error_t::invalid_argument);
      assert(compute_win64_return_info(&int_ty, false, &info, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(compute_win64_return_info(&bad_float_ty, true, &info, &sret) == llvm_gen_be_error_t::unsupported_type);
      assert(compute_win64_return_info(&st_sz16, false, &info, &sret) == llvm_gen_be_error_t::ok);

      assert(build_win64_function_type(nullptr, &fty, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_win64_function_type(&rty, nullptr, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_win64_function_type(&rty, &fty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      r_no_supp.variant.routine.return_type = &void_ty;
      assert(build_win64_function_type(&r_no_supp, &fty, &attrs) == llvm_gen_be_error_t::ok);

      a_type non_trivial_ty = {}; non_trivial_ty.kind = tk_class; non_trivial_ty.size = 99;
      rty.variant.routine.return_type = &st_sz16; // sret
      a_param_type p3 = {};
      p1.type = &non_trivial_ty; p1.next = &p2;
      p2.type = &st_sz16; p2.next = &p3;
      p3.type = &int_ty; p3.next = nullptr;
      rsupp.param_type_list = &p1;
      assert(build_win64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      rty.variant.routine.return_type = &st_sz4; // small struct return coerced to i32
      assert(build_win64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      a_type cls_sz4 = st_sz4; cls_sz4.kind = tk_class;
      rty.variant.routine.return_type = &cls_sz4; // small class return coerced to i32
      assert(build_win64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      rty.variant.routine.return_type = &bad_big_st;
      assert(build_win64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rty.variant.routine.return_type = &bad_int_ty;
      assert(build_win64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);

      rsupp.calling_convention = cc_vectorcall;
      rty.variant.routine.return_type = &bad_float_ty;
      assert(build_win64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      a_type bad_outer_hva = zbf_flt_st; bad_outer_hva.alignment = 255; // HVA leaf floats valid, outer struct alignment=255
      rty.variant.routine.return_type = &bad_outer_hva;
      assert(build_win64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);

      // 8 HVA params under vectorcall (exceeds 6 vector_regs_used), plus tk_class and tk_array HVA
      a_type cls_hva = zbf_flt_st; cls_hva.kind = tk_class;
      arr_w.variant.array.element_type = &float_ty; arr_w.variant.array.variant.number_of_elements = 2;
      rty.variant.routine.return_type = &float_ty;
      a_param_type wp[8] = {};
      wp[0].type = &cls_hva; wp[0].next = &wp[1];
      wp[1].type = &arr_w; wp[1].next = &wp[2];
      for (int i = 2; i < 8; ++i) { wp[i].type = &zbf_flt_st; wp[i].next = (i + 1 < 8) ? &wp[i + 1] : nullptr; }
      rsupp.param_type_list = &wp[0];
      assert(build_win64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      // Fail on 1st HVA param (in-reg) and 7th HVA param (overflow byval)
      wp[0].type = &bad_outer_hva;
      assert(build_win64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      wp[0].type = &cls_hva; wp[7].type = &bad_outer_hva;
      assert(build_win64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);

      wp[0].type = &bad_float_ty; wp[0].next = nullptr;
      assert(build_win64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);

      // Default calling convention with tk_struct, tk_class, tk_union (direct_integer), float_ty (direct_float), and bad_big_st (indirect error)
      rsupp.calling_convention = cc_default;
      a_param_type dp1 = {}, dp2 = {}, dp3 = {}, dp4 = {};
      dp1.type = &st_sz4; dp1.next = &dp2;
      dp2.type = &cls_sz4; dp2.next = &dp3;
      dp3.type = &union_ty; union_ty.size = 8; dp3.next = &dp4;
      dp4.type = &float_ty; dp4.next = nullptr;
      rsupp.param_type_list = &dp1;
      assert(build_win64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      dp1.type = &bad_big_st;
      assert(build_win64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      dp1.type = &bad_int_ty;
      assert(build_win64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);

      assert(lower_win64_va_arg(va_ptr, &bad_float_ty, &out_val) == llvm_gen_be_error_t::unsupported_type);
      assert(lower_win64_va_arg(va_ptr, &st_sz16, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_win64_va_arg(va_ptr, &bad_big_st, &out_val) == llvm_gen_be_error_t::unsupported_type);
    }

    // --- aapcs64 ---
    {
      uint32_t hfa_n = 0; llvm::Type* hfa_t = nullptr; bool is_hfa = false;
      assert(detect_hfa_hva(nullptr, &hfa_n, &hfa_t, &is_hfa) == llvm_gen_be_error_t::invalid_argument);
      assert(detect_hfa_hva(&float_ty, nullptr, &hfa_t, &is_hfa) == llvm_gen_be_error_t::invalid_argument);
      assert(detect_hfa_hva(&float_ty, &hfa_n, nullptr, &is_hfa) == llvm_gen_be_error_t::invalid_argument);
      assert(detect_hfa_hva(&float_ty, &hfa_n, &hfa_t, nullptr) == llvm_gen_be_error_t::invalid_argument);

      a_type tref_flt = {}; tref_flt.kind = tk_typeref; tref_flt.variant.typeref.type = &float_ty;
      assert(detect_hfa_hva(&tref_flt, &hfa_n, &hfa_t, &is_hfa) == llvm_gen_be_error_t::ok);
      assert(detect_hfa_hva(&bad_float_ty, &hfa_n, &hfa_t, &is_hfa) == llvm_gen_be_error_t::unsupported_type);
      assert(detect_hfa_hva(&zbf_flt_st, &hfa_n, &hfa_t, &is_hfa) == llvm_gen_be_error_t::ok);
      assert(detect_hfa_hva(&nzbf_st, &hfa_n, &hfa_t, &is_hfa) == llvm_gen_be_error_t::ok);
      assert(detect_hfa_hva(&mixed_fp_st, &hfa_n, &hfa_t, &is_hfa) == llvm_gen_be_error_t::ok);
      assert(detect_hfa_hva(&st_sz8, &hfa_n, &hfa_t, &is_hfa) == llvm_gen_be_error_t::ok); // 0 elements

      a_type arr_a64 = {}; arr_a64.kind = tk_array; arr_a64.size = 20;
      arr_a64.variant.array.element_type = &float_ty; arr_a64.variant.array.variant.number_of_elements = 5;
      assert(detect_hfa_hva(&arr_a64, &hfa_n, &hfa_t, &is_hfa) == llvm_gen_be_error_t::ok); // > 4 elements
      arr_a64.variant.array.element_type = &int_ty;
      assert(detect_hfa_hva(&arr_a64, &hfa_n, &hfa_t, &is_hfa) == llvm_gen_be_error_t::ok);
      arr_a64.variant.array.element_type = &bad_float_ty;
      assert(detect_hfa_hva(&arr_a64, &hfa_n, &hfa_t, &is_hfa) == llvm_gen_be_error_t::unsupported_type);

      aapcs64_arg_info_t info = {};
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "aarch64-unknown-linux-gnu");
      assert(classify_aapcs64_argument(&tref_int, &info) == llvm_gen_be_error_t::ok);
      assert(classify_aapcs64_argument(&ptr_ty, &info) == llvm_gen_be_error_t::ok);
      assert(classify_aapcs64_argument(&p2m_ty, &info) == llvm_gen_be_error_t::ok);
      assert(classify_aapcs64_argument(&int128_ty, &info) == llvm_gen_be_error_t::ok);
      assert(classify_aapcs64_argument(&zbf_flt_st, &info) == llvm_gen_be_error_t::ok); // HFA
      assert(classify_aapcs64_argument(&st_sz32, &info) == llvm_gen_be_error_t::ok); // size > 16
      assert(classify_aapcs64_argument(&bad_flt_st, &info) == llvm_gen_be_error_t::unsupported_type);

      a_type non_triv_st = {}; non_triv_st.kind = tk_struct; non_triv_st.size = 99;
      assert(classify_aapcs64_argument(&non_triv_st, &info) == llvm_gen_be_error_t::ok);

      bool sret = false;
      assert(compute_aapcs64_return_info(nullptr, &info, &sret) == llvm_gen_be_error_t::invalid_argument);
      assert(compute_aapcs64_return_info(&int_ty, nullptr, &sret) == llvm_gen_be_error_t::invalid_argument);
      assert(compute_aapcs64_return_info(&int_ty, &info, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(compute_aapcs64_return_info(&void_ty, &info, &sret) == llvm_gen_be_error_t::ok);
      assert(compute_aapcs64_return_info(&st_sz0, &info, &sret) == llvm_gen_be_error_t::ok);
      assert(compute_aapcs64_return_info(&st_sz32, &info, &sret) == llvm_gen_be_error_t::ok);
      assert(compute_aapcs64_return_info(&bad_flt_st, &info, &sret) == llvm_gen_be_error_t::unsupported_type);

      assert(build_aapcs64_function_type(nullptr, &fty, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_aapcs64_function_type(&rty, nullptr, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_aapcs64_function_type(&rty, &fty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(build_aapcs64_function_type(&r_no_supp, &fty, &attrs) == llvm_gen_be_error_t::ok);

      // Darwin SExt/ZExt, no_class return & param, sret return, HFA/float/int/reference params exceeding 8 regs
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "arm64-apple-darwin");
      a_type empty_st1 = st_sz1; empty_st1.variant.class_struct_union.is_empty_class = TRUE;
      r_no_supp.variant.routine.return_type = &empty_st1;
      assert(build_aapcs64_function_type(&r_no_supp, &fty, &attrs) == llvm_gen_be_error_t::ok);

      int_kind_is_signed[ik_int] = TRUE;
      rty.variant.routine.return_type = &int1_ty;
      a_param_type ap[22] = {};
      ap[0].type = &int1_ty; ap[0].next = &ap[1];
      int_kind_is_signed[ik_char] = FALSE;
      a_type uint1_ty = int1_ty; uint1_ty.variant.integer.int_kind = ik_char;
      ap[1].type = &uint1_ty; ap[1].next = &ap[2];
      ap[2].type = &empty_st1; ap[2].next = &ap[3];
      ap[3].type = &non_triv_st; ap[3].next = &ap[4];
      for (int i = 4; i < 13; ++i) { ap[i].type = &st_sz32; ap[i].next = &ap[i + 1]; }
      for (int i = 13; i < 16; ++i) { ap[i].type = &zbf_flt_st; ap[i].next = &ap[i + 1]; }
      for (int i = 16; i < 21; ++i) { ap[i].type = &float_ty; ap[i].next = (i + 1 < 21) ? &ap[i + 1] : nullptr; }
      rsupp.param_type_list = &ap[0];
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      rty.variant.routine.return_type = &uint1_ty;
      for (int i = 0; i < 10; ++i) { ap[i].type = &int_ty; ap[i].next = (i + 1 < 10) ? &ap[i + 1] : nullptr; }
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      rty.variant.routine.return_type = &float_ty; // Darwin non-integer return
      for (int i = 0; i < 10; ++i) { ap[i].type = &zbf_flt_st; ap[i].next = (i + 1 < 10) ? &ap[i + 1] : nullptr; }
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      for (int i = 0; i < 10; ++i) { ap[i].type = &float_ty; ap[i].next = (i + 1 < 10) ? &ap[i + 1] : nullptr; }
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      // AAPCS64 HFA return, 1-reg integer composite return, 2-reg integer composite return, array HFA param, and 2-reg integer composite param
      rty.variant.routine.return_type = &zbf_flt_st;
      a_type arr_hfa2 = {}; arr_hfa2.kind = tk_array; arr_hfa2.size = 8;
      arr_hfa2.variant.array.element_type = &float_ty; arr_hfa2.variant.array.variant.number_of_elements = 2;
      ap[0].type = &arr_hfa2; ap[0].next = &ap[1];
      ap[1].type = &st_sz16; ap[1].next = nullptr;
      rsupp.param_type_list = &ap[0];
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      rty.variant.routine.return_type = &st_sz8; // 1-element integer composite return
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      rty.variant.routine.return_type = &st_sz16; // 2-element integer composite return
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      rty.variant.routine.return_type = &arr_hfa2; // array return
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      rty.variant.routine.return_type = &st_sz32; // sret
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      rty.variant.routine.return_type = &bad_big_st;
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rty.variant.routine.return_type = &bad_flt_st;
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rty.variant.routine.return_type = &bad_int_ty;
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rty.variant.routine.return_type = &int_ty;
      ap[0].type = &bad_flt_st; ap[0].next = nullptr;
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      ap[0].type = &bad_int_ty;
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);

      assert(lower_aapcs64_va_arg(va_ptr, &bad_flt_st, &out_val) == llvm_gen_be_error_t::unsupported_type);
      assert(lower_aapcs64_va_arg(va_ptr, &bad_int_ty, &out_val) == llvm_gen_be_error_t::unsupported_type);
      assert(lower_aapcs64_va_arg(va_ptr, &st_sz32, &out_val) == llvm_gen_be_error_t::ok);

      r_no_supp.variant.routine.return_type = &int_ty;
      assert(build_arm64ec_thunk_signature(&r_no_supp, &fty, &attrs) == llvm_gen_be_error_t::ok);
      r_no_supp.variant.routine.return_type = &bad_int_ty;
      assert(build_arm64ec_thunk_signature(&r_no_supp, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      r_no_supp.variant.routine.return_type = &int_ty;
    }

    // --- sysv_x86_64 ---
    {
      x86_64_abi_arg_info_t info = {};
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
      assert(classify_sysv_argument(&tref_int, 0, &info) == llvm_gen_be_error_t::ok);
      assert(classify_sysv_argument(&ptr_ty, 0, &info) == llvm_gen_be_error_t::ok);
      assert(classify_sysv_argument(&p2m_ty, 0, &info) == llvm_gen_be_error_t::ok);
      assert(classify_sysv_argument(&int128_ty, 0, &info) == llvm_gen_be_error_t::ok);
      assert(classify_sysv_argument(&ldouble_ty, 0, &info) == llvm_gen_be_error_t::ok);
      assert(classify_sysv_argument(&ldouble8_ty, 0, &info) == llvm_gen_be_error_t::ok);
      assert(classify_sysv_argument(&f128_ty, 0, &info) == llvm_gen_be_error_t::ok);
      assert(classify_sysv_argument(&stdf128_ty, 0, &info) == llvm_gen_be_error_t::ok);
      assert(classify_sysv_argument(&st_sz0, 0, &info) == llvm_gen_be_error_t::ok);
      assert(classify_sysv_argument(&st_sz32, 0, &info) == llvm_gen_be_error_t::ok);
      assert(classify_sysv_argument(&null_field_st, 0, &info) == llvm_gen_be_error_t::invalid_argument);

      a_type non_triv_st = {}; non_triv_st.kind = tk_struct; non_triv_st.size = 99;
      assert(classify_sysv_argument(&non_triv_st, 0, &info) == llvm_gen_be_error_t::ok);

      // typeref inside struct (with alignment == 0 to hit f_alignment_of_type), float straddling 8-byte boundary, field with offset+size > 16, misaligned field, fallback kind
      a_field sf1 = {}, sf2 = {};
      tref_int.alignment = 0;
      sf1.type = &tref_int; sf1.offset = 0; sf1.next = &sf2;
      sf2.type = &double_ty; sf2.offset = 4; // aligned to 4 when double_ty.alignment = 4, straddles 8-byte boundary!
      double_ty.alignment = 4;
      a_type st_straddle = st_sz12; st_straddle.variant.class_struct_union.field_list = &sf1;
      assert(classify_sysv_argument(&st_straddle, 0, &info) == llvm_gen_be_error_t::ok);
      tref_int.alignment = 4;
      tref_int.alignment_set_explicitly = TRUE;
      assert(classify_sysv_argument(&st_straddle, 0, &info) == llvm_gen_be_error_t::ok);
      double_ty.alignment = 8;
      // Now with double_ty.alignment = 8 at offset 4, field_offset % alignment != 0 (unaligned field)!
      assert(classify_sysv_argument(&st_straddle, 0, &info) == llvm_gen_be_error_t::ok);

      // Field with offset + size > 16 inside a 16-byte struct
      sf2.type = &double_ty; sf2.offset = 16;
      st_straddle.size = 16;
      assert(classify_sysv_argument(&st_straddle, 0, &info) == llvm_gen_be_error_t::ok);

      // Array of 2 ints, and array of null_field_st
      a_type arr_sysv = {}; arr_sysv.kind = tk_array; arr_sysv.size = 8;
      arr_sysv.variant.array.element_type = &int_ty; arr_sysv.variant.array.variant.number_of_elements = 2;
      assert(classify_sysv_argument(&arr_sysv, 0, &info) == llvm_gen_be_error_t::ok);
      arr_sysv.variant.array.element_type = &null_field_st;
      assert(classify_sysv_argument(&arr_sysv, 0, &info) == llvm_gen_be_error_t::invalid_argument);

      // Unions testing all merge_classes and post-merge fixup branches:
      // 1. float + float (accum == field)
      sf1.type = &float_ty; sf1.offset = 0; sf1.next = &sf2; sf2.type = &float_ty; sf2.offset = 0;
      union_ty.size = 16; union_ty.variant.class_struct_union.field_list = &sf1;
      assert(classify_sysv_argument(&union_ty, 0, &info) == llvm_gen_be_error_t::ok);
      // 2. float + int (field == integer when accum == sse)
      sf2.type = &int_ty;
      assert(classify_sysv_argument(&union_ty, 0, &info) == llvm_gen_be_error_t::ok);
      // 3. ldouble + float (accum == x87, field == sse -> memory)
      sf1.type = &ldouble_ty; sf2.type = &float_ty;
      assert(classify_sysv_argument(&union_ty, 0, &info) == llvm_gen_be_error_t::ok);
      // 4. float + ldouble (accum == sse, field == x87 -> memory; and classes[1] has accum == no_class, field == x87up)
      sf1.type = &float_ty; sf2.type = &ldouble_ty;
      assert(classify_sysv_argument(&union_ty, 0, &info) == llvm_gen_be_error_t::ok);
      // 5. double_at_8 + ldouble (classes[1] has accum == sse, field == x87up -> memory)
      a_field d_at_8 = {}; d_at_8.type = &double_ty; d_at_8.offset = 8;
      a_type st_d8 = st_sz16; st_d8.variant.class_struct_union.field_list = &d_at_8;
      sf1.type = &st_d8; sf2.type = &ldouble_ty;
      assert(classify_sysv_argument(&union_ty, 0, &info) == llvm_gen_be_error_t::ok);
      // 6. ldouble + d_at_8 (classes[1] has accum == x87up, field == sse -> memory)
      sf1.type = &ldouble_ty; sf2.type = &st_d8;
      assert(classify_sysv_argument(&union_ty, 0, &info) == llvm_gen_be_error_t::ok);
      // 7. ldouble + int (classes[0] = integer, classes[1] = x87up -> post-merge x87up fixup!)
      sf1.type = &ldouble_ty; sf2.type = &int_ty;
      assert(classify_sysv_argument(&union_ty, 0, &info) == llvm_gen_be_error_t::ok);
      // 8. f128 + int (classes[0] = integer, classes[1] = sseup -> post-merge sseup fixup!)
      sf1.type = &f128_ty; sf2.type = &int_ty;
      assert(classify_sysv_argument(&union_ty, 0, &info) == llvm_gen_be_error_t::ok);
      // 9. f128 + d_at_8 (classes[1] has accum == sseup, field == sse -> *out_class = sse)
      sf1.type = &f128_ty; sf2.type = &st_d8;
      assert(classify_sysv_argument(&union_ty, 0, &info) == llvm_gen_be_error_t::ok);

      // Unsupported type kind with size = 4 hits fallback in classify_internal
      a_type unk_sz4 = {}; unk_sz4.kind = tk_routine; unk_sz4.size = 4;
      assert(classify_sysv_argument(&unk_sz4, 0, &info) == llvm_gen_be_error_t::ok);

      // Complex types: _Complex float (size 8), _Complex double (size 16), _Complex long double (size 32), _Complex __float80
      a_type cpx_flt = {}; cpx_flt.kind = tk_complex; cpx_flt.variant.float_kind = fk_float; cpx_flt.size = 8; cpx_flt.alignment = 4;
      a_type cpx_dbl = {}; cpx_dbl.kind = tk_complex; cpx_dbl.variant.float_kind = fk_double; cpx_dbl.size = 16; cpx_dbl.alignment = 8;
      a_type cpx_ldbl = {}; cpx_ldbl.kind = tk_complex; cpx_ldbl.variant.float_kind = fk_long_double; cpx_ldbl.size = 32; cpx_ldbl.alignment = 16;
      a_type cpx_ldbl16 = cpx_ldbl; cpx_ldbl16.size = 16; cpx_ldbl16.alignment = 8;
      a_type cpx_f80 = {}; cpx_f80.kind = tk_complex; cpx_f80.variant.float_kind = fk_float80; cpx_f80.size = 32; cpx_f80.alignment = 16;
      a_type f80_ty = float_ty; f80_ty.variant.float_kind = fk_float80; f80_ty.size = 16; f80_ty.alignment = 16;
      assert(classify_sysv_argument(&f80_ty, 0, &info) == llvm_gen_be_error_t::ok);
      assert(info.eightbyte_classes[0] == x86_64_abi_class_t::x87);
      assert(classify_sysv_argument(&cpx_flt, 0, &info) == llvm_gen_be_error_t::ok);
      assert(classify_sysv_argument(&cpx_dbl, 0, &info) == llvm_gen_be_error_t::ok);
      assert(classify_sysv_argument(&cpx_ldbl16, 0, &info) == llvm_gen_be_error_t::ok);
      assert(info.eightbyte_classes[0] == x86_64_abi_class_t::sse);
      assert(classify_sysv_argument(&cpx_f80, 0, &info) == llvm_gen_be_error_t::ok);
      assert(info.eightbyte_classes[0] == x86_64_abi_class_t::complex_x87);
      assert(classify_sysv_argument(&cpx_ldbl, 0, &info) == llvm_gen_be_error_t::ok);
      assert(info.eightbyte_classes[0] == x86_64_abi_class_t::complex_x87);

      // Exhaustive tests for is_sysv_complex_x87_type, IL-lowered _Complex_long_double / _Complex_float80,
      // Android x86_64 fp128 long double, user-defined 32B struct sret distinction, and OOM percolation.
      {
        bool is_cx87 = false;
        assert(is_sysv_complex_x87_type(nullptr, &is_cx87) == llvm_gen_be_error_t::invalid_argument);
        assert(is_sysv_complex_x87_type(&cpx_ldbl, nullptr) == llvm_gen_be_error_t::invalid_argument);
        g_llvm_be_oom_countdown = 0;
        assert(classify_sysv_argument(&cpx_ldbl, 0, &info) == llvm_gen_be_error_t::out_of_memory);
        g_llvm_be_oom_countdown = -1;

        // Lowered _Complex_long_double and _Complex_float80 structs (DO_IL_LOWERING=1 representation)
        a_type arr_ld2 = {};
        arr_ld2.kind = tk_array;
        arr_ld2.size = 32;
        arr_ld2.alignment = 16;
        arr_ld2.variant.array.element_type = &ldouble_ty;
        arr_ld2.variant.array.variant.number_of_elements = 2;
        a_field fld_vals = {};
        fld_vals.type = &arr_ld2;
        fld_vals.offset = 0;

        a_type lowered_cld = {};
        lowered_cld.kind = tk_struct;
        lowered_cld.size = 32;
        lowered_cld.alignment = 16;
        lowered_cld.source_corresp.name = "_Complex_long_double";
        lowered_cld.variant.class_struct_union.field_list = &fld_vals;

        a_type tref_lowered_cld = {};
        tref_lowered_cld.kind = tk_typeref;
        tref_lowered_cld.size = 32;
        tref_lowered_cld.alignment = 16;
        tref_lowered_cld.variant.typeref.type = &lowered_cld;

        bool sret_flag = true;
        assert(compute_sysv_return_info(&tref_lowered_cld, &info, &sret_flag) == llvm_gen_be_error_t::ok);
        assert(!sret_flag);
        assert(info.eightbyte_classes[0] == x86_64_abi_class_t::complex_x87);

        // User-written 32-byte struct { long double a[2]; } MUST still use MEMORY / sret
        a_type user_ld2_struct = lowered_cld;
        user_ld2_struct.source_corresp.name = "UserPairLD";
        assert(compute_sysv_return_info(&user_ld2_struct, &info, &sret_flag) == llvm_gen_be_error_t::ok);
        assert(sret_flag);

        // Unnamed 32-byte struct also uses MEMORY / sret
        user_ld2_struct.source_corresp.name = nullptr;
        assert(compute_sysv_return_info(&user_ld2_struct, &info, &sret_flag) == llvm_gen_be_error_t::ok);
        assert(sret_flag);

        // Lowered _Complex_long_double with size 16 (8-byte long double target) is NOT complex_x87
        a_type lowered_cld16 = lowered_cld;
        lowered_cld16.size = 16;
        assert(is_sysv_complex_x87_type(&lowered_cld16, &is_cx87) == llvm_gen_be_error_t::ok);
        assert(!is_cx87);

        // Lowered _Complex_float80 is complex_x87
        a_type lowered_cf80 = lowered_cld;
        lowered_cf80.source_corresp.name = "_Complex_float80";
        assert(compute_sysv_return_info(&lowered_cf80, &info, &sret_flag) == llvm_gen_be_error_t::ok);
        assert(!sret_flag);
        assert(info.eightbyte_classes[0] == x86_64_abi_class_t::complex_x87);

        // Android x86_64: 16-byte fk_long_double is IEEE fp128 (SSE/SSEUP), so _Complex long double is MEMORY/sret
        std::string saved_triple = llvm::Triple(be_state->module->getTargetTriple()).str();
        EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "x86_64-unknown-linux-android");
        assert(classify_sysv_argument(&ldouble_ty, 0, &info) == llvm_gen_be_error_t::ok);
        assert(info.eightbyte_classes[0] == x86_64_abi_class_t::sse);
        assert(info.eightbyte_classes[1] == x86_64_abi_class_t::sseup);
        assert(is_sysv_complex_x87_type(&cpx_ldbl, &is_cx87) == llvm_gen_be_error_t::ok);
        assert(!is_cx87);
        assert(compute_sysv_return_info(&tref_lowered_cld, &info, &sret_flag) == llvm_gen_be_error_t::ok);
        assert(sret_flag);
        EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), saved_triple);

        // Verify build_sysv_function_type on lowered _Complex_long_double sets {x86_fp80, x86_fp80} return (no sret) and byval align 16
        a_param_type param_cld = {};
        param_cld.type = &tref_lowered_cld;
        a_routine_type_supplement supp_cld = {};
        supp_cld.param_type_list = &param_cld;
        supp_cld.prototyped = TRUE;
        a_type fn_cld = {};
        fn_cld.kind = tk_routine;
        fn_cld.variant.routine.return_type = &tref_lowered_cld;
        fn_cld.variant.routine.extra_info = &supp_cld;
        llvm::FunctionType* cld_fty = nullptr;
        llvm::AttributeList cld_attrs;
        assert(build_sysv_function_type(&fn_cld, &cld_fty, &cld_attrs) == llvm_gen_be_error_t::ok);
        assert(cld_fty->getNumParams() == 1);
        assert(cld_fty->getReturnType()->isStructTy());
        assert(cld_attrs.getParamAttrs(0).getAlignment() == llvm::Align(16));
      }

      // Struct containing _Complex float to exercise classify_internal tk_complex SSE branch
      a_field cf_flt = {}; cf_flt.type = &cpx_flt; cf_flt.offset = 0;
      a_type st_cpx_flt = st_sz8; st_cpx_flt.variant.class_struct_union.field_list = &cf_flt;
      assert(classify_sysv_argument(&st_cpx_flt, 0, &info) == llvm_gen_be_error_t::ok);

      // Bitfields in struct and union (including 0-bit and >64-bit / multi-eightbyte bitfield)
      a_field bf0 = {}, bf1 = {}, bf2 = {};
      bf0.is_bit_field = TRUE; bf0.bit_size = 0; bf0.type = &int_ty; bf0.next = &bf1;
      bf1.is_bit_field = TRUE; bf1.bit_size = 16; bf1.offset = 7; bf1.offset_bit_remainder = 4; bf1.type = &int_ty; bf1.next = &bf2;
      bf2.is_bit_field = TRUE; bf2.bit_size = 64; bf2.offset = 12; bf2.offset_bit_remainder = 0; bf2.type = &int_ty; bf2.next = nullptr;
      a_type st_bf = st_sz16; st_bf.variant.class_struct_union.field_list = &bf0;
      assert(classify_sysv_argument(&st_bf, 0, &info) == llvm_gen_be_error_t::ok);
      a_type un_bf = union_ty; un_bf.size = 16; un_bf.variant.class_struct_union.field_list = &bf1;
      assert(classify_sysv_argument(&un_bf, 0, &info) == llvm_gen_be_error_t::ok);

      // Test build_sysv_coerced_type for all classifications and sizes
      llvm::Type* coerced_ty = nullptr;
      assert(build_sysv_coerced_type(nullptr, &info, &coerced_ty) == llvm_gen_be_error_t::invalid_argument);
      assert(build_sysv_coerced_type(&int_ty, nullptr, &coerced_ty) == llvm_gen_be_error_t::invalid_argument);
      assert(build_sysv_coerced_type(&int_ty, &info, nullptr) == llvm_gen_be_error_t::invalid_argument);

      // 1-eightbyte integer structs of sizes 1, 2, 4, 8
      a_field f_i1 = {}; f_i1.type = &int1_ty;
      st_sz1.variant.class_struct_union.field_list = &f_i1;
      assert(classify_sysv_argument(&st_sz1, 0, &info) == llvm_gen_be_error_t::ok);
      assert(build_sysv_coerced_type(&st_sz1, &info, &coerced_ty) == llvm_gen_be_error_t::ok);
      st_sz2.variant.class_struct_union.field_list = &f_i1;
      assert(classify_sysv_argument(&st_sz2, 0, &info) == llvm_gen_be_error_t::ok);
      assert(build_sysv_coerced_type(&st_sz2, &info, &coerced_ty) == llvm_gen_be_error_t::ok);
      st_sz4.variant.class_struct_union.field_list = &f_i1;
      assert(classify_sysv_argument(&st_sz4, 0, &info) == llvm_gen_be_error_t::ok);
      assert(build_sysv_coerced_type(&st_sz4, &info, &coerced_ty) == llvm_gen_be_error_t::ok);
      st_sz8.variant.class_struct_union.field_list = &f_i1;
      assert(classify_sysv_argument(&st_sz8, 0, &info) == llvm_gen_be_error_t::ok);
      assert(build_sysv_coerced_type(&st_sz8, &info, &coerced_ty) == llvm_gen_be_error_t::ok);

      // 1-eightbyte SSE structs of sizes 4 and 8
      a_field f_flt = {}; f_flt.type = &float_ty;
      a_type st_sse4 = st_sz4; st_sse4.variant.class_struct_union.field_list = &f_flt;
      assert(classify_sysv_argument(&st_sse4, 0, &info) == llvm_gen_be_error_t::ok);
      assert(build_sysv_coerced_type(&st_sse4, &info, &coerced_ty) == llvm_gen_be_error_t::ok);
      assert(classify_sysv_argument(&cpx_flt, 0, &info) == llvm_gen_be_error_t::ok);
      assert(build_sysv_coerced_type(&cpx_flt, &info, &coerced_ty) == llvm_gen_be_error_t::ok);

      // 2-eightbyte structs: {int, int} (size 12 & 16), {sse, sse} (size 12 & 16), x87/x87up, sse/sseup, complex_x87
      a_field f_lo_i = {}, f_hi_i = {};
      f_lo_i.type = &ptr_ty; f_lo_i.offset = 0; f_lo_i.next = &f_hi_i;
      f_hi_i.type = &int_ty; f_hi_i.offset = 8;
      a_type st_ii12 = st_sz12; st_ii12.alignment = 8; st_ii12.variant.class_struct_union.field_list = &f_lo_i;
      assert(classify_sysv_argument(&st_ii12, 0, &info) == llvm_gen_be_error_t::ok);
      assert(build_sysv_coerced_type(&st_ii12, &info, &coerced_ty) == llvm_gen_be_error_t::ok);
      a_type st_ii16 = st_sz16; st_ii16.variant.class_struct_union.field_list = &f_lo_i;
      assert(classify_sysv_argument(&st_ii16, 0, &info) == llvm_gen_be_error_t::ok);
      assert(build_sysv_coerced_type(&st_ii16, &info, &coerced_ty) == llvm_gen_be_error_t::ok);

      f_lo_i.type = &double_ty; f_hi_i.type = &float_ty;
      a_type st_ss12 = st_sz12; st_ss12.alignment = 8; st_ss12.variant.class_struct_union.field_list = &f_lo_i;
      assert(classify_sysv_argument(&st_ss12, 0, &info) == llvm_gen_be_error_t::ok);
      assert(build_sysv_coerced_type(&st_ss12, &info, &coerced_ty) == llvm_gen_be_error_t::ok);
      assert(classify_sysv_argument(&cpx_dbl, 0, &info) == llvm_gen_be_error_t::ok);
      assert(build_sysv_coerced_type(&cpx_dbl, &info, &coerced_ty) == llvm_gen_be_error_t::ok);

      assert(classify_sysv_argument(&ldouble_ty, 0, &info) == llvm_gen_be_error_t::ok);
      assert(build_sysv_coerced_type(&ldouble_ty, &info, &coerced_ty) == llvm_gen_be_error_t::ok);
      assert(classify_sysv_argument(&f128_ty, 0, &info) == llvm_gen_be_error_t::ok);
      assert(build_sysv_coerced_type(&f128_ty, &info, &coerced_ty) == llvm_gen_be_error_t::ok);
      assert(classify_sysv_argument(&cpx_ldbl, 0, &info) == llvm_gen_be_error_t::ok);
      assert(build_sysv_coerced_type(&cpx_ldbl, &info, &coerced_ty) == llvm_gen_be_error_t::ok);
      assert(classify_sysv_argument(&st_sz0, 0, &info) == llvm_gen_be_error_t::ok);
      assert(build_sysv_coerced_type(&st_sz0, &info, &coerced_ty) == llvm_gen_be_error_t::ok);
      assert(classify_sysv_argument(&st_sz32, 0, &info) == llvm_gen_be_error_t::ok);
      assert(build_sysv_coerced_type(&st_sz32, &info, &coerced_ty) == llvm_gen_be_error_t::ok);

      bool sret = false;
      assert(compute_sysv_return_info(&int_ty, nullptr, &sret) == llvm_gen_be_error_t::invalid_argument);
      assert(compute_sysv_return_info(&int_ty, &info, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(compute_sysv_return_info(&null_field_st, &info, &sret) == llvm_gen_be_error_t::invalid_argument);
      assert(compute_sysv_return_info(&st_sz32, &info, &sret) == llvm_gen_be_error_t::ok);
      assert(compute_sysv_return_info(&cpx_ldbl, &info, &sret) == llvm_gen_be_error_t::ok);
      assert(!sret);

      assert(build_sysv_function_type(nullptr, &fty, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_sysv_function_type(&rty, nullptr, &attrs) == llvm_gen_be_error_t::invalid_argument);
      assert(build_sysv_function_type(&rty, &fty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(build_sysv_function_type(&r_no_supp, &fty, &attrs) == llvm_gen_be_error_t::ok);

      int_kind_is_signed[ik_int] = TRUE;
      int_kind_is_signed[ik_char] = FALSE;
      a_type uint1_sysv = int1_ty; uint1_sysv.variant.integer.int_kind = ik_char;
      r_no_supp.variant.routine.return_type = &int1_ty;
      assert(build_sysv_function_type(&r_no_supp, &fty, &attrs) == llvm_gen_be_error_t::ok);
      r_no_supp.variant.routine.return_type = &uint1_sysv;
      assert(build_sysv_function_type(&r_no_supp, &fty, &attrs) == llvm_gen_be_error_t::ok);
      r_no_supp.variant.routine.return_type = &float_ty;
      assert(build_sysv_function_type(&r_no_supp, &fty, &attrs) == llvm_gen_be_error_t::ok);
      r_no_supp.variant.routine.return_type = &int_ty;

      rty.variant.routine.return_type = &st_sz32; // sret
      a_param_type sp_params[22] = {};
      sp_params[0].type = &non_triv_st; sp_params[0].next = &sp_params[1];
      sp_params[1].type = &st_sz32; sp_params[1].next = &sp_params[2];
      sp_params[2].type = &int1_ty; sp_params[2].next = &sp_params[3];
      sp_params[3].type = &uint1_sysv; sp_params[3].next = &sp_params[4];
      for (int i = 4; i < 10; ++i) { sp_params[i].type = &int_ty; sp_params[i].next = &sp_params[i + 1]; }
      sp_params[10].type = &st_sz8; sp_params[10].next = &sp_params[11]; // aggregate after GP regs exhausted -> byval
      for (int i = 11; i < 20; ++i) { sp_params[i].type = &float_ty; sp_params[i].next = &sp_params[i + 1]; }
      sp_params[20].type = &cpx_flt; sp_params[20].next = &sp_params[21]; // _Complex float after SSE regs exhausted -> byval
      sp_params[21].type = &float_ty; sp_params[21].next = nullptr;
      rsupp.param_type_list = &sp_params[0];
      assert(build_sysv_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      rty.variant.routine.return_type = &null_field_st;
      assert(build_sysv_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::invalid_argument);
      rty.variant.routine.return_type = &bad_big_st;
      assert(build_sysv_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rty.variant.routine.return_type = &bad_int_ty;
      assert(build_sysv_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      rty.variant.routine.return_type = &int_ty;
      sp_params[0].type = &null_field_st; sp_params[0].next = nullptr;
      assert(build_sysv_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::invalid_argument);
      sp_params[0].type = &bad_int_ty;
      assert(build_sysv_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);

      assert(lower_sysv_va_arg(va_ptr, &null_field_st, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_sysv_va_arg(va_ptr, &bad_int_ty, &out_val) == llvm_gen_be_error_t::unsupported_type);
      assert(lower_sysv_va_arg(va_ptr, &non_triv_st, &out_val) == llvm_gen_be_error_t::ok);

      // x86_32, arm32, and wasm va_* helpers
      assert(lower_x86_32_va_start(nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_x86_32_va_start(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_x86_32_va_start(va_ptr, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_x86_32_va_arg(nullptr, &int_ty, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_x86_32_va_arg(va_ptr, nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_x86_32_va_arg(va_ptr, &int_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_x86_32_va_arg(va_ptr, &bad_int_ty, &out_val) == llvm_gen_be_error_t::unsupported_type);
      assert(lower_x86_32_va_arg(va_ptr, &int_ty, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_x86_32_va_end(nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_x86_32_va_end(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_x86_32_va_end(va_ptr, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_x86_32_va_copy(nullptr, va_ptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_x86_32_va_copy(va_ptr, nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_x86_32_va_copy(va_ptr, va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_x86_32_va_copy(va_ptr, va_ptr, &out_val) == llvm_gen_be_error_t::ok);

      assert(lower_arm32_va_start(nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_arm32_va_start(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_arm32_va_start(va_ptr, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_arm32_va_arg(nullptr, &int_ty, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_arm32_va_arg(va_ptr, nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_arm32_va_arg(va_ptr, &int_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_arm32_va_arg(va_ptr, &bad_int_ty, &out_val) == llvm_gen_be_error_t::unsupported_type);
      assert(lower_arm32_va_arg(va_ptr, &int_ty, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_arm32_va_end(nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_arm32_va_end(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_arm32_va_end(va_ptr, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_arm32_va_copy(nullptr, va_ptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_arm32_va_copy(va_ptr, nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_arm32_va_copy(va_ptr, va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_arm32_va_copy(va_ptr, va_ptr, &out_val) == llvm_gen_be_error_t::ok);

      assert(lower_wasm_va_start(nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_wasm_va_start(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_wasm_va_start(va_ptr, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_wasm_va_arg(nullptr, &int_ty, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_wasm_va_arg(va_ptr, nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_wasm_va_arg(va_ptr, &int_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_wasm_va_arg(va_ptr, &bad_int_ty, &out_val) == llvm_gen_be_error_t::unsupported_type);
      assert(lower_wasm_va_arg(va_ptr, &int_ty, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_wasm_va_arg(va_ptr, &union_ty, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_s390x_va_arg(va_ptr, &st_sz16, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_sparc_va_arg(va_ptr, &st_sz128, &out_val) == llvm_gen_be_error_t::ok);
      targ_sizeof_pointer = 4;
      assert(lower_sparc_va_arg(va_ptr, &st_sz8, &out_val) == llvm_gen_be_error_t::ok);
      targ_sizeof_pointer = 8;
      assert(lower_ppc64_va_arg(va_ptr, &st_sz128, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_loongarch64_va_arg(va_ptr, &st_sz32, &out_val) == llvm_gen_be_error_t::ok);

      // Over-aligned va_arg tests across sysv, aapcs64, arm32, riscv64, ppc64, s390x
      a_type aligned32_int = int_ty;
      aligned32_int.alignment = 32;
      assert(lower_sysv_va_arg(va_ptr, &aligned32_int, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_aapcs64_va_arg(va_ptr, &aligned32_int, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_arm32_va_arg(va_ptr, &aligned32_int, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_arm32_va_arg(va_ptr, &st_sz128, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_riscv64_va_arg(va_ptr, &aligned32_int, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_ppc64_va_arg(va_ptr, &aligned32_int, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_s390x_va_arg(va_ptr, &aligned32_int, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_wasm_va_end(nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_wasm_va_end(va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_wasm_va_end(va_ptr, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_wasm_va_copy(nullptr, va_ptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_wasm_va_copy(va_ptr, nullptr, &out_val) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_wasm_va_copy(va_ptr, va_ptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(lower_wasm_va_copy(va_ptr, va_ptr, &out_val) == llvm_gen_be_error_t::ok);

      // Test llvm_compute_function_abi_info across all target architectures and return kinds
      llvm_abi_function_info_t abi_info = {};
      assert(llvm_compute_function_abi_info(nullptr, "x86_64-unknown-linux-gnu", &abi_info) == llvm_gen_be_error_t::invalid_argument);
      assert(llvm_compute_function_abi_info(&r_no_supp, "x86_64-unknown-linux-gnu", nullptr) == llvm_gen_be_error_t::invalid_argument);

      r_no_supp.variant.routine.return_type = &void_ty;
      assert(llvm_compute_function_abi_info(&r_no_supp, "x86_64-unknown-linux-gnu", &abi_info) == llvm_gen_be_error_t::ok);
      assert(abi_info.return_kind == llvm_abi_passing_kind_t::ignore_void);

      r_no_supp.variant.routine.return_type = &int_ty;
      assert(llvm_compute_function_abi_info(&r_no_supp, "x86_64-pc-windows-msvc", &abi_info) == llvm_gen_be_error_t::ok);
      assert(abi_info.return_kind == llvm_abi_passing_kind_t::direct);
      assert(llvm_compute_function_abi_info(&r_no_supp, "x86_64-unknown-linux-gnu", &abi_info) == llvm_gen_be_error_t::ok);
      assert(llvm_compute_function_abi_info(&r_no_supp, "i386-pc-windows-msvc", &abi_info) == llvm_gen_be_error_t::ok);
      assert(llvm_compute_function_abi_info(&r_no_supp, "aarch64-unknown-linux-gnu", &abi_info) == llvm_gen_be_error_t::ok);
      assert(llvm_compute_function_abi_info(&r_no_supp, "armv7-unknown-linux-gnueabihf", &abi_info) == llvm_gen_be_error_t::ok);
      assert(llvm_compute_function_abi_info(&r_no_supp, "thumbv7-unknown-linux-gnueabihf", &abi_info) == llvm_gen_be_error_t::ok);
      assert(llvm_compute_function_abi_info(&r_no_supp, "riscv64-unknown-linux-gnu", &abi_info) == llvm_gen_be_error_t::ok);
      assert(llvm_compute_function_abi_info(&r_no_supp, "loongarch64-unknown-linux-gnu", &abi_info) == llvm_gen_be_error_t::ok);
      assert(llvm_compute_function_abi_info(&r_no_supp, "mips64-unknown-linux-gnu", &abi_info) == llvm_gen_be_error_t::ok);
      assert(llvm_compute_function_abi_info(&r_no_supp, "powerpc64le-unknown-linux-gnu", &abi_info) == llvm_gen_be_error_t::ok);
      assert(llvm_compute_function_abi_info(&r_no_supp, "s390x-ibm-linux-gnu", &abi_info) == llvm_gen_be_error_t::ok);
      assert(llvm_compute_function_abi_info(&r_no_supp, "sparcv9-sun-solaris", &abi_info) == llvm_gen_be_error_t::ok);
      assert(llvm_compute_function_abi_info(&r_no_supp, "wasm32-unknown-wasi", &abi_info) == llvm_gen_be_error_t::ok);

      r_no_supp.variant.routine.return_type = &st_sz8;
      assert(llvm_compute_function_abi_info(&r_no_supp, "x86_64-unknown-linux-gnu", &abi_info) == llvm_gen_be_error_t::ok);
      assert(abi_info.return_kind == llvm_abi_passing_kind_t::coerced_register);

      r_no_supp.variant.routine.return_type = &st_sz32;
      assert(llvm_compute_function_abi_info(&r_no_supp, "x86_64-unknown-linux-gnu", &abi_info) == llvm_gen_be_error_t::ok);
      assert(abi_info.has_sret);
      assert(abi_info.return_kind == llvm_abi_passing_kind_t::indirect_sret);

      r_no_supp.variant.routine.return_type = &bad_int_ty;
      assert(llvm_compute_function_abi_info(&r_no_supp, "x86_64-unknown-linux-gnu", &abi_info) == llvm_gen_be_error_t::unsupported_type);
      r_no_supp.variant.routine.return_type = &int_ty;

      // Test build_s390x_va_list_type
      llvm::Type* s390_va_ty = nullptr;
      assert(build_s390x_va_list_type(nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(build_s390x_va_list_type(&s390_va_ty) == llvm_gen_be_error_t::ok);
      assert(s390_va_ty != nullptr && s390_va_ty->isArrayTy());

      // Test Windows ARM64 (aarch64-pc-windows-msvc) variadic HFA/float & instance method sret
      EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "aarch64-pc-windows-msvc");
      assert(build_aapcs64_va_list_type(&s390_va_ty) == llvm_gen_be_error_t::ok);
      assert(s390_va_ty->isPointerTy());
      assert(lower_aapcs64_va_arg(va_ptr, &int_ty, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_aapcs64_va_arg(va_ptr, &aligned32_int, &out_val) == llvm_gen_be_error_t::ok);

      a_type hfa2_flt = st_sz8;
      a_field hf1 = {}, hf2 = {};
      hf1.type = &float_ty; hf1.offset = 0; hf1.next = &hf2;
      hf2.type = &float_ty; hf2.offset = 4; hf2.next = nullptr;
      hfa2_flt.variant.class_struct_union.field_list = &hf1;

      a_type hfa4_dbl = st_sz32;
      a_field hd1 = {}, hd2 = {}, hd3 = {}, hd4 = {};
      hd1.type = &double_ty; hd1.offset = 0; hd1.next = &hd2;
      hd2.type = &double_ty; hd2.offset = 8; hd2.next = &hd3;
      hd3.type = &double_ty; hd3.offset = 16; hd3.next = &hd4;
      hd4.type = &double_ty; hd4.offset = 24; hd4.next = nullptr;
      hfa4_dbl.variant.class_struct_union.field_list = &hd1;

      // Windows ARM64 instance method returning >16B struct (sret in x1) and non-instance sret
      rty.variant.routine.return_type = &st_sz32;
      rsupp.this_class = &st_sz8;
      rsupp.has_ellipsis = FALSE;
      rsupp.param_type_list = nullptr;
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      rsupp.this_class = nullptr;
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      // Windows ARM64 variadic function returning HFA and taking HFA <= 16B, HFA > 16B, and float (up to >8 GPRs)
      rty.variant.routine.return_type = &hfa2_flt;
      rsupp.has_ellipsis = TRUE;
      a_param_type win_arm_params[11] = {};
      win_arm_params[0].type = &hfa2_flt; win_arm_params[0].next = &win_arm_params[1];
      win_arm_params[1].type = &hfa4_dbl; win_arm_params[1].next = &win_arm_params[2];
      for (int i = 2; i < 11; ++i) {
        win_arm_params[i].type = &float_ty;
        win_arm_params[i].next = (i + 1 < 11) ? &win_arm_params[i + 1] : nullptr;
      }
      rsupp.param_type_list = &win_arm_params[0];
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      rsupp.has_ellipsis = FALSE;

      // Test AAPCS64 Linux va_arg for HFA (num_regs > 1), float_vector (num_regs == 1), integer (size 0 and size 8), and over-aligned (>8B)
      EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "aarch64-unknown-linux-gnu");
      assert(lower_aapcs64_va_arg(va_ptr, &float_ty, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_aapcs64_va_arg(va_ptr, &hfa2_flt, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_aapcs64_va_arg(va_ptr, &st_sz0, &out_val) == llvm_gen_be_error_t::ok);
      // Test Darwin AAPCS64 va_arg
      EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "aarch64-apple-darwin");
      assert(lower_aapcs64_va_arg(va_ptr, &int_ty, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_aapcs64_va_arg(va_ptr, &aligned32_int, &out_val) == llvm_gen_be_error_t::ok);

      // Test SysV x86_64 va_arg in-function for 1-eightbyte GP, 1-eightbyte FP, 2-eightbyte {GP, GP}, 2-eightbyte {FP, FP}, 2-eightbyte {GP, FP}, memory (>16B and size 0), and over-aligned (>8B)
      EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "x86_64-unknown-linux-gnu");
      assert(lower_sysv_va_arg(va_ptr, &int_ty, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_sysv_va_arg(va_ptr, &float_ty, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_sysv_va_arg(va_ptr, &st_ii16, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_sysv_va_arg(va_ptr, &cpx_dbl, &out_val) == llvm_gen_be_error_t::ok);
      a_field f_mix1 = {}, f_mix2 = {};
      f_mix1.type = &ptr_ty; f_mix1.offset = 0; f_mix1.next = &f_mix2;
      f_mix2.type = &double_ty; f_mix2.offset = 8; f_mix2.next = nullptr;
      a_type st_if16 = st_sz16; st_if16.variant.class_struct_union.field_list = &f_mix1;
      assert(lower_sysv_va_arg(va_ptr, &st_if16, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_sysv_va_arg(va_ptr, &st_sz32, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_sysv_va_arg(va_ptr, &st_sz0, &out_val) == llvm_gen_be_error_t::ok);
      assert(lower_sysv_va_arg(va_ptr, &ldouble_ty, &out_val) == llvm_gen_be_error_t::ok);

      // Test detached basic block (cur_fn == nullptr) fallback in lower_sysv_va_arg and lower_aapcs64_va_arg
      {
        llvm::BasicBlock* saved_bb = be_state->builder->GetInsertBlock();
        llvm::BasicBlock* detached_bb = llvm::BasicBlock::Create(*be_state->context, "detached");
        be_state->builder->SetInsertPoint(detached_bb);
        assert(lower_sysv_va_arg(va_ptr, &int_ty, &out_val) == llvm_gen_be_error_t::ok);
        assert(lower_sysv_va_arg(va_ptr, &aligned32_int, &out_val) == llvm_gen_be_error_t::ok);
        EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "aarch64-unknown-linux-gnu");
        assert(lower_aapcs64_va_arg(va_ptr, &int_ty, &out_val) == llvm_gen_be_error_t::ok);
        assert(lower_aapcs64_va_arg(va_ptr, &aligned32_int, &out_val) == llvm_gen_be_error_t::ok);
        be_state->builder->SetInsertPoint(saved_bb);
        delete detached_bb;
      }

      // Test RISC-V 32-bit (riscv32) vs 64-bit (riscv64) and FPR/GPR exhaustion in build_riscv64_function_type
      EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "riscv32-unknown-linux-gnu");
      riscv64_arg_info_t rv_info = {};
      assert(classify_riscv64_argument(&st_sz8, &rv_info) == llvm_gen_be_error_t::ok);
      assert(classify_riscv64_argument(&st_sz16, &rv_info) == llvm_gen_be_error_t::ok);
      assert(rv_info.is_indirect == true); // > 8 bytes on RV32 is indirect

      rty.variant.routine.return_type = &st_sz32; // sret
      a_param_type rv_params[12] = {};
      for (int i = 0; i < 8; ++i) {
        rv_params[i].type = &float_ty;
        rv_params[i].next = &rv_params[i + 1];
      }
      rv_params[8].type = &hfa2_flt; rv_params[8].next = &rv_params[9]; // FPRs exhausted -> demoted to [2 x i32] on RV32
      rv_params[9].type = &float_ty; rv_params[9].next = &rv_params[10]; // FPR exhausted -> uses GPR
      rv_params[10].type = &st_sz8; rv_params[10].next = &rv_params[11]; // ArrayType coerced [2 x i32]
      rv_params[11].type = &st_sz32; rv_params[11].next = nullptr; // indirect
      rsupp.param_type_list = &rv_params[0];
      assert(build_riscv64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);

      EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "riscv64-unknown-linux-gnu");
      rty.variant.routine.return_type = &int_ty;
      for (int i = 0; i < 8; ++i) {
        rv_params[i].type = &int_ty;
        rv_params[i].next = &rv_params[i + 1];
      }
      rv_params[8].type = &st_if16; rv_params[8].next = &rv_params[9]; // GPRs exhausted for {ptr, double} -> demoted to [2 x i64]
      rv_params[9].type = &int_ty; rv_params[9].next = &rv_params[10];
      rv_params[10].type = &st_if16; rv_params[10].next = &rv_params[11];
      rv_params[11].type = &st_sz32; rv_params[11].next = nullptr;
      assert(build_riscv64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      // Also test when both GPRs and FPRs are available for {ptr, double}
      rv_params[0].type = &st_if16; rv_params[0].next = nullptr;
      assert(build_riscv64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "x86_64-unknown-linux-gnu");

      // Cover tk_nullptr across AAPCS64, RISC-V 64, SysV x86_64, and Win64
      a_type nullptr_ty = {};
      nullptr_ty.kind = tk_nullptr;
      nullptr_ty.size = 8;
      nullptr_ty.alignment = 8;

      aapcs64_arg_info_t aapcs_np_info = {};
      assert(classify_aapcs64_argument(&nullptr_ty, &aapcs_np_info) == llvm_gen_be_error_t::ok);
      a_type big_int32 = int_ty;
      big_int32.size = 32;
      assert(classify_aapcs64_argument(&big_int32, &aapcs_np_info) == llvm_gen_be_error_t::ok);

      a_field f_np = {};
      f_np.type = &nullptr_ty; f_np.offset = 0; f_np.next = nullptr;
      a_type st_np = st_sz8;
      st_np.variant.class_struct_union.field_list = &f_np;
      assert(classify_riscv64_argument(&st_np, &rv_info) == llvm_gen_be_error_t::ok);

      x86_64_abi_arg_info_t sysv_np_info = {};
      assert(classify_sysv_argument(&nullptr_ty, 0, &sysv_np_info) == llvm_gen_be_error_t::ok);

      win64_arg_info_t win_np_info = {};
      assert(classify_win64_argument(&nullptr_ty, false, &win_np_info) == llvm_gen_be_error_t::ok);

      // Cover AAPCS64 indirect va_arg on Windows, Linux, and detached BasicBlock, plus Darwin step==0
      EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "aarch64-pc-windows-msvc");
      assert(lower_aapcs64_va_arg(va_ptr, &st_sz32, &out_val) == llvm_gen_be_error_t::ok);
      EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "aarch64-unknown-linux-gnu");
      assert(lower_aapcs64_va_arg(va_ptr, &st_sz32, &out_val) == llvm_gen_be_error_t::ok);
      EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "aarch64-apple-darwin");
      assert(lower_aapcs64_va_arg(va_ptr, &st_sz0, &out_val) == llvm_gen_be_error_t::ok);
      {
        llvm::BasicBlock* saved_bb = be_state->builder->GetInsertBlock();
        llvm::BasicBlock* detached_bb = llvm::BasicBlock::Create(*be_state->context, "detached_ind");
        be_state->builder->SetInsertPoint(detached_bb);
        EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "aarch64-unknown-linux-gnu");
        assert(lower_aapcs64_va_arg(va_ptr, &st_sz32, &out_val) == llvm_gen_be_error_t::ok);
        EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "x86_64-pc-windows-msvc");
        assert(lower_win64_va_arg(va_ptr, &int_ty, &out_val) == llvm_gen_be_error_t::ok);
        assert(lower_win64_va_arg(va_ptr, &st_sz32, &out_val) == llvm_gen_be_error_t::ok);
        be_state->builder->SetInsertPoint(saved_bb);
        detached_bb->dropAllReferences();
        delete detached_bb;
      }
      EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "x86_64-pc-windows-msvc");
      assert(lower_win64_va_arg(va_ptr, &st_sz32, &out_val) == llvm_gen_be_error_t::ok);

      // Cover va_arg / function-type error propagation in arm32, loongarch64, ppc64, s390x, sparc, wasm, and sysv dispatcher
      a_field f_null_ty = {};
      f_null_ty.type = nullptr;
      a_type st_null_field = st_sz8;
      st_null_field.variant.class_struct_union.field_list = &f_null_ty;
      EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "armv7-unknown-linux-gnueabihf");
      assert(lower_arm32_va_arg(va_ptr, &st_null_field, &out_val) != llvm_gen_be_error_t::ok);
      assert(lower_loongarch64_va_arg(va_ptr, &st_null_field, &out_val) != llvm_gen_be_error_t::ok);
      assert(lower_ppc64_va_arg(va_ptr, &st_null_field, &out_val) != llvm_gen_be_error_t::ok);

      a_type bad_int_ty = int_ty;
      bad_int_ty.alignment = 255;
      a_type tref_bad = {};
      tref_bad.kind = tk_typeref;
      tref_bad.size = 4;
      tref_bad.alignment = 4;
      tref_bad.variant.typeref.type = &bad_int_ty;
      assert(lower_s390x_va_arg(va_ptr, &tref_bad, &out_val) == llvm_gen_be_error_t::unsupported_type);
      assert(lower_sparc_va_arg(va_ptr, &tref_bad, &out_val) == llvm_gen_be_error_t::unsupported_type);

      a_type st_bad_s390 = st_sz8;
      st_bad_s390.size = 4;
      st_bad_s390.alignment = 255;
      rty.variant.routine.return_type = &st_bad_s390;
      rsupp.param_type_list = nullptr;
      EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "s390x-ibm-linux-gnu");
      llvm_abi_function_info_t abi_out = {};
      assert(llvm_compute_function_abi_info(&rty, "s390x-ibm-linux-gnu", &abi_out) == llvm_gen_be_error_t::unsupported_type);

      // WebAssembly single-scalar struct where unwrapped field fails get_llvm_type
      a_field f_bad_wasm = {};
      f_bad_wasm.type = &bad_int_ty;
      a_type st_bad_wasm = st_sz8;
      st_bad_wasm.size = 4;
      st_bad_wasm.variant.class_struct_union.field_list = &f_bad_wasm;
      rty.variant.routine.return_type = &st_bad_wasm;
      rsupp.param_type_list = nullptr;
      assert(build_wasm_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);
      a_param_type wasm_p = {};
      wasm_p.type = &st_bad_wasm;
      wasm_p.next = nullptr;
      rty.variant.routine.return_type = &int_ty;
      rsupp.param_type_list = &wasm_p;
      assert(build_wasm_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::unsupported_type);

      // Deterministic OOM countdown sweeps across sysv_x86_64, wasm, win64, and x86_32
      a_field f_ok_wasm = {};
      f_ok_wasm.type = &int_ty;
      a_type st_ok_wasm = st_sz8;
      st_ok_wasm.size = 4;
      st_ok_wasm.variant.class_struct_union.field_list = &f_ok_wasm;

      EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "x86_64-unknown-linux-gnu");
      for (int b = 0; b < 32; ++b) {
        g_llvm_be_oom_countdown = b;
        rty.variant.routine.return_type = &st_if16;
        wasm_p.type = &st_if16;
        rsupp.param_type_list = &wasm_p;
        llvm_gen_be_error_t r1 = build_sysv_function_type(&rty, &fty, &attrs);
        g_llvm_be_oom_countdown = -1;
        if (r1 == llvm_gen_be_error_t::ok) break;
      }
      for (int b = 0; b < 16; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r2 = lower_sysv_va_arg(va_ptr, &st_if16, &out_val);
        g_llvm_be_oom_countdown = -1;
        if (r2 == llvm_gen_be_error_t::ok) break;
      }
      // Sweep merge_classes OOM branches across integer/x87/fp128/complex/straddling/unaligned fields in classify_sysv_argument
      a_type fp128_ty = float_ty;
      fp128_ty.size = 16;
      fp128_ty.alignment = 16;
      fp128_ty.variant.float_kind = fk_float128;
      a_field f_strad_i = {}, f_strad_f = {}, f_unalign = {}, f_bf = {};
      f_strad_i.type = &ptr_ty; f_strad_i.offset = 4; f_strad_i.next = nullptr;
      a_type st_strad_i = st_sz16; st_strad_i.variant.class_struct_union.field_list = &f_strad_i;
      f_strad_f.type = &double_ty; f_strad_f.offset = 4; f_strad_f.next = nullptr;
      a_type st_strad_f = st_sz16; st_strad_f.variant.class_struct_union.field_list = &f_strad_f;
      f_unalign.type = &int_ty; f_unalign.offset = 1; f_unalign.next = nullptr;
      a_type st_unalign = st_sz8; st_unalign.variant.class_struct_union.field_list = &f_unalign;
      f_bf.type = &int_ty; f_bf.offset = 0; f_bf.is_bit_field = TRUE; f_bf.bit_size = 5; f_bf.next = nullptr;
      a_type st_bf_oom = st_sz8; st_bf_oom.variant.class_struct_union.field_list = &f_bf;
      a_type_ptr sysv_sweep_tys[] = {&st_if16, &ldouble_ty, &fp128_ty, &cpx_dbl, &st_strad_i, &st_strad_f, &st_unalign, &st_bf_oom};
      for (a_type_ptr sty : sysv_sweep_tys) {
        for (int b = 0; b < 16; ++b) {
          g_llvm_be_oom_countdown = b;
          x86_64_abi_arg_info_t sinfo = {};
          llvm_gen_be_error_t rs = classify_sysv_argument(sty, 0, &sinfo);
          g_llvm_be_oom_countdown = -1;
          if (rs == llvm_gen_be_error_t::ok) break;
        }
      }

      EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "wasm32-unknown-unknown");
      rty.variant.routine.return_type = &st_ok_wasm;
      wasm_p.type = &st_ok_wasm;
      rsupp.param_type_list = &wasm_p;
      rsupp.has_ellipsis = FALSE;
      for (int b = 0; b < 24; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t rw = build_wasm_function_type(&rty, &fty, &attrs);
        g_llvm_be_oom_countdown = -1;
        if (rw == llvm_gen_be_error_t::ok) break;
      }
      for (int b = 0; b < 8; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t rw2 = lower_wasm_va_arg(va_ptr, &st_ok_wasm, &out_val);
        g_llvm_be_oom_countdown = -1;
        if (rw2 == llvm_gen_be_error_t::ok) break;
      }

      EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "x86_64-pc-windows-msvc");
      for (int b = 0; b < 8; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t rwin = lower_win64_va_arg(va_ptr, &int_ty, &out_val);
        g_llvm_be_oom_countdown = -1;
        if (rwin == llvm_gen_be_error_t::ok) break;
      }

      EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "i686-unknown-linux-gnu");
      rty.variant.routine.return_type = &int_ty;
      wasm_p.type = &int_ty;
      rsupp.param_type_list = &wasm_p;
      for (int b = 0; b < 16; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t rx86 = build_x86_32_function_type(&rty, &fty, &attrs);
        g_llvm_be_oom_countdown = -1;
        if (rx86 == llvm_gen_be_error_t::ok) break;
      }
      EDG_LLVM_SET_TARGET_TRIPLE(be_state->module.get(), "x86_64-unknown-linux-gnu");

      // Cover inline helpers in src/llvm_gen_be_internal.h
      {
        ScopeCleanupStack cstack = {};
        ScopeCleanup centry = {};
        g_llvm_be_oom_countdown = 0;
        assert(llvm_cleanup_stack_push(&cstack, &centry) == llvm_gen_be_error_t::out_of_memory);
        g_llvm_be_oom_countdown = -1;

        a_routine r_int = {};
        const char* sym_out = nullptr;
        g_llvm_be_oom_countdown = 0;
        assert(llvm_get_routine_symbol_name(&r_int, &sym_out) == llvm_gen_be_error_t::out_of_memory);
        g_llvm_be_oom_countdown = -1;

        llvm::Function* decl_fn = nullptr;
        assert(llvm_get_or_declare_routine(nullptr, &decl_fn) == llvm_gen_be_error_t::ok);
        char empty_str[] = "";
        r_int.source_corresp.name = empty_str;
        assert(llvm_get_or_declare_routine(&r_int, &decl_fn) == llvm_gen_be_error_t::ok);

        r_int.source_corresp.name = const_cast<char*>("fresh_proto_fn");
        g_llvm_be_oom_countdown = 0;
        assert(llvm_get_or_declare_routine(&r_int, &decl_fn) == llvm_gen_be_error_t::out_of_memory);
        g_llvm_be_oom_countdown = -1;

        r_int.type = &bad_int_ty;
        assert(llvm_get_or_declare_routine(&r_int, &decl_fn) == llvm_gen_be_error_t::unsupported_type);
        r_int.type = &int_ty; // non-FunctionType -> dyn_cast_or_null false branch
        assert(llvm_get_or_declare_routine(&r_int, &decl_fn) == llvm_gen_be_error_t::ok);
        assert(decl_fn == nullptr);
        a_type fn_proto_ty = rty;
        fn_proto_ty.alignment = 254;
        r_int.type = &fn_proto_ty; // FunctionType -> creates prototype
        assert(llvm_get_or_declare_routine(&r_int, &decl_fn) == llvm_gen_be_error_t::ok);
        assert(decl_fn != nullptr);

        a_variable v_int = {};
        g_llvm_be_oom_countdown = 0;
        assert(llvm_get_variable_symbol_name(&v_int, &sym_out) == llvm_gen_be_error_t::out_of_memory);
        g_llvm_be_oom_countdown = 0;
        assert(llvm_create_global_variable(be_state->module.get(), llvm::Type::getInt32Ty(*be_state->context), false, llvm::GlobalValue::ExternalLinkage, nullptr, "oom_gv") == llvm_gen_be_error_t::out_of_memory);
        g_llvm_be_oom_countdown = -1;
      }
    }
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
