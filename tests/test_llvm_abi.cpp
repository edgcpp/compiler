/**
 * @file test_llvm_abi.cpp
 * @brief Unit tests for the LLVM backend ABI lowering subsystems.
 * @details Validates register classification, sret handling, and function type generation for System V AMD64, AAPCS64, and Win64.
 */

#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_abi_sysv_x86_64.h"
#include "llvm_gen_be_abi_aapcs64.h"
#include "llvm_gen_be_abi_win64.h"
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
  a_boolean is_trivially_copyable_type(a_type_ptr ty) { return TRUE; }
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
  
  
  
  
  llvm_gen_be_error_t llvm_type_from_edg_type(a_type_ptr ty, llvm::Type** out_ty) noexcept {
      return get_llvm_type(ty, out_ty);
  }
  llvm_gen_be_error_t llvm_type_from_float(a_type_ptr ty, llvm::Type** out_ty) noexcept {
      *out_ty = llvm::Type::getFloatTy(*be_state->context);
      return llvm_gen_be_error_t::ok;
  }

  llvm_gen_be_error_t get_llvm_type(a_type_ptr ty, llvm::Type** out_ty) noexcept {
    if (ty->kind == tk_integer) {
       *out_ty = llvm::Type::getInt32Ty(*be_state->context);
    } else if (ty->kind == tk_float) {
       *out_ty = llvm::Type::getFloatTy(*be_state->context);
    } else {
       *out_ty = llvm::Type::getInt32Ty(*be_state->context); // stub
    }
    return llvm_gen_be_error_t::ok;
  }
}

int main() {
  printf("Running test_llvm_abi...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_abi", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

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

  // SysV x86_64
  {
      x86_64_abi_arg_info_t info = {};
      assert(classify_sysv_argument(&int_ty, 0, &info) == llvm_gen_be_error_t::ok);
      assert(info.num_eightbytes == 1);
      assert(info.eightbyte_classes[0] == x86_64_abi_class_t::integer);

      info = {};
      assert(classify_sysv_argument(&float_ty, 0, &info) == llvm_gen_be_error_t::ok);
      assert(info.num_eightbytes == 1);
      assert(info.eightbyte_classes[0] == x86_64_abi_class_t::sse);

      info = {};
      assert(classify_sysv_argument(&struct_ty, 0, &info) == llvm_gen_be_error_t::ok);
      // int (integer) + float (sse) in same eightbyte -> integer
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
  }

  // AAPCS64
  {
      aapcs64_arg_info_t info = {};
      assert(classify_aapcs64_argument(&int_ty, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == aapcs64_abi_class_t::integer);

      info = {};
      assert(classify_aapcs64_argument(&float_ty, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == aapcs64_abi_class_t::float_vector);

      info = {};
      assert(classify_aapcs64_argument(&struct_ty, &info) == llvm_gen_be_error_t::ok);
      
      bool is_sret = false;
      assert(compute_aapcs64_return_info(&struct_ty, &info, &is_sret) == llvm_gen_be_error_t::ok);
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
      assert(build_aapcs64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      assert(fty != nullptr);
  }

  // Win64
  {
      win64_arg_info_t info = {};
      assert(classify_win64_argument(&int_ty, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == win64_abi_class_t::direct_integer);

      info = {};
      assert(classify_win64_argument(&float_ty, &info) == llvm_gen_be_error_t::ok);
      assert(info.abi_class == win64_abi_class_t::direct_float);

      info = {};
      assert(classify_win64_argument(&struct_ty, &info) == llvm_gen_be_error_t::ok);
      
      bool is_sret = false;
      assert(compute_win64_return_info(&struct_ty, &info, &is_sret) == llvm_gen_be_error_t::ok);
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
      assert(build_win64_function_type(&rty, &fty, &attrs) == llvm_gen_be_error_t::ok);
      assert(fty != nullptr);
  }

  printf("PASS\n");
  return 0;
}
