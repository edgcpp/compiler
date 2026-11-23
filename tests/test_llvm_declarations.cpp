/**
 * @file test_llvm_declarations.cpp
 * @ingroup llvm_gen_be_tests
 * @brief Unit tests for the LLVM backend declarations and globals subsystem.
 * @details Validates function coverage of llvm_lower_global_variable,
 * llvm_lower_function_prototype, llvm_lower_function_body, global ctors/dtors, and TLS models.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_abi_aapcs64.h"
#include "llvm_gen_be_abi_sysv_x86_64.h"
#include "llvm_gen_be_abi_win64.h"
#include "llvm_gen_be_abi_x86_32.h"
#include "llvm_gen_be_abi_arm32.h"
#include "llvm_gen_be_abi_wasm.h"
#include "llvm_gen_be_abi_riscv64.h"
#include "llvm_gen_be_abi_loongarch64.h"
#include "llvm_gen_be_abi_mips.h"
#include "llvm_gen_be_abi_ppc64.h"
#include "llvm_gen_be_abi_s390x.h"
#include "llvm_gen_be_abi_sparc.h"
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
  /**
   * @brief Test stub or helper for `is_bool_type`.
   * @details Provides mock or helper behavior for `is_bool_type` within `tests/test_llvm_declarations.cpp`.
   * @param[in] ty Parameter `ty` for `is_bool_type`.
   * @return Result of `is_bool_type` (a_boolean).
   */
  a_boolean is_bool_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `f_alignment_of_type`.
   * @details Provides mock or helper behavior for `f_alignment_of_type` within `tests/test_llvm_declarations.cpp`.
   * @param[in] ty Parameter `ty` for `f_alignment_of_type`.
   * @return Result of `f_alignment_of_type` (a_targ_alignment).
   */
  a_targ_alignment f_alignment_of_type(a_type_ptr ty) { return 1; }
  /**
   * @brief Test stub or helper for `is_trivially_copyable_type`.
   * @details Provides mock or helper behavior for `is_trivially_copyable_type` within `tests/test_llvm_declarations.cpp`.
   * @param[in] ty Parameter `ty` for `is_trivially_copyable_type`.
   * @return Result of `is_trivially_copyable_type` (a_boolean).
   */
  a_boolean is_trivially_copyable_type(a_type_ptr ty) { return TRUE; }
  int g_decl_type_nonreal = 0; ///< Configurable return value for `type_is_nonreal`.
  int g_decl_il_lowering = 0; ///< Configurable return value for `il_lowering_needed`.
  /**
   * @brief Test stub for `type_is_nonreal`.
   * @details Returns `TRUE` when `g_decl_type_nonreal` is non-zero and `ty != nullptr`, else `FALSE`.
   * @param[in] ty Frontend type pointer.
   * @return `a_boolean` indicating whether `ty` is nonreal.
   */
  a_boolean type_is_nonreal(a_type_ptr ty) { return (ty && g_decl_type_nonreal) ? TRUE : FALSE; }
  /**
   * @brief Test stub for `il_lowering_needed`.
   * @details Returns `TRUE` when `g_decl_il_lowering` is non-zero, else `FALSE`.
   * @return `a_boolean` indicating whether IL lowering is active.
   */
  a_boolean il_lowering_needed(void) { return g_decl_il_lowering ? TRUE : FALSE; }
  unsigned int targ_char_bit = 8;
  a_targ_size_t targ_sizeof_short = 2;
  a_targ_size_t targ_sizeof_int = 4;
  a_targ_size_t targ_sizeof_long = 8;
  a_targ_size_t targ_sizeof_long_long = 8;
  a_targ_size_t targ_sizeof_pointer = 8;
  a_boolean targ_little_endian = TRUE;
  a_C_dialect C_dialect = static_cast<a_C_dialect>(0);
  int g_decl_omp_scan_active = 0; ///< When non-zero, `conv_seq_to_file_and_line` populates file/line outputs.
  const char* g_decl_omp_short_fn = nullptr; ///< Optional short file name returned by `conv_seq_to_file_and_line`.
  const char* g_decl_omp_full_fn = nullptr; ///< Optional full file name returned by `conv_seq_to_file_and_line`.
  int g_decl_const_diff_ty = 0; ///< When non-zero, `evaluate_constant` returns `[2 x i32] zeroinitializer`.
  int g_decl_proto_nonempty_attrs = 0; ///< When non-zero, `llvm_build_target_abi_function_type` populates `*out_attrs`.
  /**
   * @brief Test stub or helper for `conv_seq_to_file_and_line`.
   * @details Provides mock or helper behavior for `conv_seq_to_file_and_line` within `tests/test_llvm_declarations.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `conv_seq_to_file_and_line`.
   * @param[out] arg_1 Parameter `arg_1` for `conv_seq_to_file_and_line`.
   * @param[out] arg_2 Parameter `arg_2` for `conv_seq_to_file_and_line`.
   * @param[in] arg_3 Parameter `arg_3` for `conv_seq_to_file_and_line`.
   * @param[in] arg_4 Parameter `arg_4` for `conv_seq_to_file_and_line`.
   * @return Result of `conv_seq_to_file_and_line` (a_source_file_ptr).
   */
  a_source_file_ptr conv_seq_to_file_and_line(a_seq_number arg_0, a_const_char** arg_1, a_const_char** arg_2, a_line_number* arg_3, a_boolean* arg_4) {
    if (!g_decl_omp_scan_active) return nullptr;
    if (arg_1) *arg_1 = g_decl_omp_short_fn;
    if (arg_2) *arg_2 = g_decl_omp_full_fn;
    if (arg_3) *arg_3 = static_cast<a_line_number>(arg_0);
    if (arg_4) *arg_4 = FALSE;
    return nullptr;
  }
  /**
   * @brief Test stub or helper for `mangled_typeinfo_name`.
   * @details Provides mock or helper behavior for `mangled_typeinfo_name` within `tests/test_llvm_declarations.cpp`.
   * @param[in] type Parameter `type` for `mangled_typeinfo_name`.
   * @return Result of `mangled_typeinfo_name` (char*).
   */
  char* mangled_typeinfo_name(a_type_ptr type) { return (char*)"_ZTIi"; }
  a_type_qualifier_set g_decl_tqual = 0; ///< Configurable return value for `f_get_type_qualifiers`.
  int g_decl_fail_const = 0; ///< When non-zero, `evaluate_constant` returns `constant_eval_failure`.
  int g_decl_fail_type = 0; ///< Countdown of calls before `get_llvm_type` returns `unsupported_type`.
  int g_decl_fail_proto = 0; ///< When non-zero, `build_*_function_type` returns `abi_classification_failed`.
  int g_decl_null_proto = 0; ///< When non-zero, `build_*_function_type` sets `*out_ty = nullptr`.
  int g_decl_proto_mode = 0; ///< Prototype shape (0=void(), 1=void(ptr), 2=i32(ptr,i32), 3=void(ptr,i64,i64,i32)).
  int g_decl_fail_ret_info = 0; ///< When non-zero, `compute_*_return_info` returns `abi_classification_failed`.
  bool g_decl_sret_ret_info = false; ///< Value stored into `*out_sret` by `compute_*_return_info`.
  an_init_kind g_decl_init_kind = initk_none; ///< Initializer kind returned by `get_variable_initializer`.
  an_initializer_ptr g_decl_init_ptr = nullptr; ///< Initializer pointer returned by `get_variable_initializer`.
  a_scope_ptr g_decl_scope_for_rout = nullptr; ///< Scope returned by `scope_for_routine`.
  int g_decl_fail_di_sub = 0; ///< When non-zero, `create_di_subprogram` returns `di_metadata_failure`.
  int g_decl_fail_dbg_decl = 0; ///< Countdown of calls before `emit_dbg_declare_for_variable` returns `di_metadata_failure`.
  int g_decl_fail_dbg_global = 0; ///< When non-zero, `emit_dbg_global_variable` returns `di_metadata_failure`.
  int g_decl_fail_pop_lex = 0; ///< When non-zero, `pop_lexical_block` returns `di_metadata_failure`.
  int g_decl_fail_vtbl = 0; ///< When non-zero, `llvm_emit_vtable` returns `vtable_generation_failure`.
  int g_decl_fail_vtt = 0; ///< When non-zero, `llvm_emit_vtt` returns `vtable_generation_failure`.
  int g_decl_fail_vbtbl = 0; ///< When non-zero, `llvm_emit_msvc_vbtable` returns `vtable_generation_failure`.
  int g_decl_fail_expr = 0; ///< When non-zero, `llvm_lower_expression` returns `unsupported_expr`.

  /**
   * @brief Test stub or helper for `f_get_type_qualifiers`.
   * @details Provides mock or helper behavior for `f_get_type_qualifiers` within `tests/test_llvm_declarations.cpp`.
   * @param[in] type Parameter `type` for `f_get_type_qualifiers`.
   * @param[in] x Parameter `x` for `f_get_type_qualifiers`.
   * @return Result of `f_get_type_qualifiers` (a_type_qualifier_set).
   */
  a_type_qualifier_set f_get_type_qualifiers(a_type_ptr type, a_boolean x) { (void)type; (void)x; return g_decl_tqual; }
  /**
   * @brief Test stub or helper for `is_reference_type`.
   * @details Provides mock or helper behavior for `is_reference_type` within `tests/test_llvm_declarations.cpp`.
   * @param[in] ty Parameter `ty` for `is_reference_type`.
   * @return Result of `is_reference_type` (a_boolean).
   */
  a_boolean is_reference_type(a_type_ptr ty) { (void)ty; return FALSE; }
  /**
   * @brief Test stub or helper for `is_empty_class_type`.
   * @details Provides mock or helper behavior for `is_empty_class_type` within `tests/test_llvm_declarations.cpp`.
   * @param[in] ty Parameter `ty` for `is_empty_class_type`.
   * @return Result of `is_empty_class_type` (a_boolean).
   */
  a_boolean is_empty_class_type(a_type_ptr ty) { return ty && ty->variant.class_struct_union.is_empty_class; }
  /**
   * @brief Test stub or helper for `evaluate_constant`.
   * @details Provides mock or helper behavior for `evaluate_constant` within `tests/test_llvm_declarations.cpp`.
   * @param[in] con Parameter `con` for `evaluate_constant`.
   * @param[in] ty Parameter `ty` for `evaluate_constant`.
   * @param[out] out_const Parameter `out_const` for `evaluate_constant`.
   * @return Result of `evaluate_constant` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t evaluate_constant(a_constant_ptr con, llvm::Type* ty, llvm::Constant** out_const) noexcept { 
    (void)con;
    if (g_decl_fail_const) return llvm_gen_be_error_t::constant_eval_failure;
    if (g_decl_const_diff_ty) {
      llvm::ArrayType* arr_ty = llvm::ArrayType::get(llvm::Type::getInt32Ty(*be_state->context), 2);
      *out_const = llvm::ConstantAggregateZero::get(arr_ty);
      return llvm_gen_be_error_t::ok;
    }
    if (ty->isIntegerTy()) *out_const = llvm::ConstantInt::get(ty, 1);
    else if (ty->isFloatingPointTy()) *out_const = llvm::ConstantFP::get(ty, 1.0);
    else *out_const = llvm::Constant::getNullValue(ty);
    return llvm_gen_be_error_t::ok; 
  }
  a_byte_boolean int_kind_is_signed[ik_last] = {0};

  a_boolean sanitize_address = FALSE; a_boolean sanitize_thread = FALSE; a_boolean sanitize_memory = FALSE; a_boolean sanitize_undefined = FALSE; a_boolean sanitize_hwaddress = FALSE; int stack_protector_mode = 0; a_boolean use_safestack = FALSE; a_boolean use_shadowcallstack = FALSE;

  /**
   * @brief Helper to construct a mock LLVM FunctionType based on `g_decl_proto_mode`.
   * @details Used by all 12 target ABI prototype builder stubs in `tests/test_llvm_declarations.cpp`.
   * @param[out] out_ty Receives the constructed `llvm::FunctionType*`.
   * @return llvm_gen_be_error_t::ok or `abi_classification_failed`.
   */
  static llvm_gen_be_error_t make_mock_proto(llvm::FunctionType** out_ty) noexcept {
    if (g_decl_fail_proto) return llvm_gen_be_error_t::abi_classification_failed;
    if (g_decl_null_proto) {
      *out_ty = nullptr;
      return llvm_gen_be_error_t::ok;
    }
    llvm::Type* void_ty = llvm::Type::getVoidTy(*be_state->context);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
    if (g_decl_proto_mode == 1) {
      *out_ty = llvm::FunctionType::get(void_ty, {ptr_ty}, false);
    } else if (g_decl_proto_mode == 2) {
      *out_ty = llvm::FunctionType::get(i32_ty, {ptr_ty, i32_ty}, false);
    } else if (g_decl_proto_mode == 3) {
      *out_ty = llvm::FunctionType::get(void_ty, {ptr_ty, i64_ty, i64_ty, i32_ty}, false);
    } else if (g_decl_proto_mode == 4) {
      *out_ty = llvm::FunctionType::get(ptr_ty, {ptr_ty, i32_ty}, false);
    } else if (g_decl_proto_mode == 5) {
      *out_ty = llvm::FunctionType::get(ptr_ty, false);
    } else if (g_decl_proto_mode == 6) {
      *out_ty = llvm::FunctionType::get(ptr_ty, {i32_ty}, false);
    } else if (g_decl_proto_mode == 7) {
      *out_ty = llvm::FunctionType::get(void_ty, {ptr_ty, ptr_ty}, false);
    } else if (g_decl_proto_mode == 8) {
      *out_ty = llvm::FunctionType::get(ptr_ty, {ptr_ty, ptr_ty}, false);
    } else if (g_decl_proto_mode == 9) {
      *out_ty = llvm::FunctionType::get(void_ty, {ptr_ty, ptr_ty, i32_ty}, false);
    } else {
      *out_ty = llvm::FunctionType::get(void_ty, false);
    }
    return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub or helper for `build_wasm_function_type`.
   * @details Provides mock or helper behavior for `build_wasm_function_type` within `tests/test_llvm_declarations.cpp`.
   * @param[in] ty Parameter `ty` for `build_wasm_function_type`.
   * @param[out] out_ty Parameter `out_ty` for `build_wasm_function_type`.
   * @param[out] out_attrs Parameter `out_attrs` for `build_wasm_function_type`.
   * @return Result of `build_wasm_function_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t build_wasm_function_type(a_type_ptr ty, llvm::FunctionType** out_ty, llvm::AttributeList* out_attrs) noexcept {
      (void)ty; (void)out_attrs; return make_mock_proto(out_ty);
  }
  /**
   * @brief Test stub or helper for `build_arm32_function_type`.
   * @details Provides mock or helper behavior for `build_arm32_function_type` within `tests/test_llvm_declarations.cpp`.
   * @param[in] ty Parameter `ty` for `build_arm32_function_type`.
   * @param[out] out_ty Parameter `out_ty` for `build_arm32_function_type`.
   * @param[out] out_attrs Parameter `out_attrs` for `build_arm32_function_type`.
   * @return Result of `build_arm32_function_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t build_arm32_function_type(a_type_ptr ty, llvm::FunctionType** out_ty, llvm::AttributeList* out_attrs) noexcept {
      (void)ty; (void)out_attrs; return make_mock_proto(out_ty);
  }
  /**
   * @brief Test stub or helper for `build_x86_32_function_type`.
   * @details Provides mock or helper behavior for `build_x86_32_function_type` within `tests/test_llvm_declarations.cpp`.
   * @param[in] ty Parameter `ty` for `build_x86_32_function_type`.
   * @param[out] out_ty Parameter `out_ty` for `build_x86_32_function_type`.
   * @param[out] out_attrs Parameter `out_attrs` for `build_x86_32_function_type`.
   * @return Result of `build_x86_32_function_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t build_x86_32_function_type(a_type_ptr ty, llvm::FunctionType** out_ty, llvm::AttributeList* out_attrs) noexcept {
      (void)ty; (void)out_attrs; return make_mock_proto(out_ty);
  }
  /**
   * @brief Test stub or helper for `build_riscv64_function_type`.
   * @details Provides mock or helper behavior for `build_riscv64_function_type` within `tests/test_llvm_declarations.cpp`.
   * @param[in] ty Parameter `ty` for `build_riscv64_function_type`.
   * @param[out] out_ty Parameter `out_ty` for `build_riscv64_function_type`.
   * @param[out] out_attrs Parameter `out_attrs` for `build_riscv64_function_type`.
   * @return Result of `build_riscv64_function_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t build_riscv64_function_type(a_type_ptr ty, llvm::FunctionType** out_ty, llvm::AttributeList* out_attrs) noexcept {
      (void)ty; (void)out_attrs; return make_mock_proto(out_ty);
  }
  /**
   * @brief Test stub or helper for `build_win64_function_type`.
   * @details Provides mock or helper behavior for `build_win64_function_type` within `tests/test_llvm_declarations.cpp`.
   * @param[in] ty Parameter `ty` for `build_win64_function_type`.
   * @param[out] out_ty Parameter `out_ty` for `build_win64_function_type`.
   * @param[out] out_attrs Parameter `out_attrs` for `build_win64_function_type`.
   * @return Result of `build_win64_function_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t build_win64_function_type(a_type_ptr ty, llvm::FunctionType** out_ty, llvm::AttributeList* out_attrs) noexcept {
      (void)ty; (void)out_attrs; return make_mock_proto(out_ty);
  }
  /**
   * @brief Test stub or helper for `build_loongarch64_function_type`.
   * @details Provides mock or helper behavior for `build_loongarch64_function_type` within `tests/test_llvm_declarations.cpp`.
   * @param[in] ty Parameter `ty` for `build_loongarch64_function_type`.
   * @param[out] out_ty Parameter `out_ty` for `build_loongarch64_function_type`.
   * @param[out] out_attrs Parameter `out_attrs` for `build_loongarch64_function_type`.
   * @return Result of `build_loongarch64_function_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t build_loongarch64_function_type(a_type_ptr ty, llvm::FunctionType** out_ty, llvm::AttributeList* out_attrs) noexcept {
      (void)ty; (void)out_attrs; return make_mock_proto(out_ty);
  }
  /**
   * @brief Test stub or helper for `build_mips_function_type`.
   * @details Provides mock or helper behavior for `build_mips_function_type` within `tests/test_llvm_declarations.cpp`.
   * @param[in] ty Parameter `ty` for `build_mips_function_type`.
   * @param[out] out_ty Parameter `out_ty` for `build_mips_function_type`.
   * @param[out] out_attrs Parameter `out_attrs` for `build_mips_function_type`.
   * @return Result of `build_mips_function_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t build_mips_function_type(a_type_ptr ty, llvm::FunctionType** out_ty, llvm::AttributeList* out_attrs) noexcept {
      (void)ty; (void)out_attrs; return make_mock_proto(out_ty);
  }
  /**
   * @brief Test stub or helper for `llvm_build_target_abi_function_type`.
   * @details Delegates to `make_mock_proto` within `tests/test_llvm_declarations.cpp`.
   * @param[in] routine_edg_type Routine type descriptor.
   * @param[in] is_variadic_call Variadic call flag.
   * @param[in] num_fixed_args Number of fixed parameters.
   * @param[in] call_arg_types Call-site argument types.
   * @param[out] out_fn_ty Receives mock `FunctionType*`.
   * @param[out] out_attrs Receives mock `AttributeList`.
   * @return Result of `make_mock_proto`.
   */
  llvm_gen_be_error_t llvm_build_target_abi_function_type(
      a_type_ptr routine_edg_type,
      bool is_variadic_call,
      unsigned num_fixed_args,
      const std::vector<a_type_ptr>& call_arg_types,
      llvm::FunctionType** out_fn_ty,
      llvm::AttributeList* out_attrs) noexcept {
      (void)routine_edg_type; (void)is_variadic_call; (void)num_fixed_args;
      (void)call_arg_types;
      llvm_gen_be_error_t err = make_mock_proto(out_fn_ty);
      if (err == llvm_gen_be_error_t::ok && g_decl_proto_nonempty_attrs && out_attrs && be_state && be_state->context) {
        *out_attrs = out_attrs->addFnAttribute(
            *be_state->context, llvm::Attribute::get(*be_state->context, llvm::Attribute::NoUnwind));
      }
      return err;
  }
  /**
   * @brief Test stub or helper for `build_ppc64_function_type`.
   * @details Provides mock or helper behavior for `build_ppc64_function_type` within `tests/test_llvm_declarations.cpp`.
   * @param[in] ty Parameter `ty` for `build_ppc64_function_type`.
   * @param[out] out_ty Parameter `out_ty` for `build_ppc64_function_type`.
   * @param[out] out_attrs Parameter `out_attrs` for `build_ppc64_function_type`.
   * @return Result of `build_ppc64_function_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t build_ppc64_function_type(a_type_ptr ty, llvm::FunctionType** out_ty, llvm::AttributeList* out_attrs) noexcept {
      (void)ty; (void)out_attrs; return make_mock_proto(out_ty);
  }
  /**
   * @brief Test stub or helper for `build_s390x_function_type`.
   * @details Provides mock or helper behavior for `build_s390x_function_type` within `tests/test_llvm_declarations.cpp`.
   * @param[in] ty Parameter `ty` for `build_s390x_function_type`.
   * @param[out] out_ty Parameter `out_ty` for `build_s390x_function_type`.
   * @param[out] out_attrs Parameter `out_attrs` for `build_s390x_function_type`.
   * @return Result of `build_s390x_function_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t build_s390x_function_type(a_type_ptr ty, llvm::FunctionType** out_ty, llvm::AttributeList* out_attrs) noexcept {
      (void)ty; (void)out_attrs; return make_mock_proto(out_ty);
  }
  /**
   * @brief Test stub or helper for `build_sparc_function_type`.
   * @details Provides mock or helper behavior for `build_sparc_function_type` within `tests/test_llvm_declarations.cpp`.
   * @param[in] ty Parameter `ty` for `build_sparc_function_type`.
   * @param[out] out_ty Parameter `out_ty` for `build_sparc_function_type`.
   * @param[out] out_attrs Parameter `out_attrs` for `build_sparc_function_type`.
   * @return Result of `build_sparc_function_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t build_sparc_function_type(a_type_ptr ty, llvm::FunctionType** out_ty, llvm::AttributeList* out_attrs) noexcept {
      (void)ty; (void)out_attrs; return make_mock_proto(out_ty);
  }
  /**
   * @brief Test stub or helper for `compute_win64_return_info`.
   * @details Provides mock or helper behavior for `compute_win64_return_info` within `tests/test_llvm_declarations.cpp`.
   * @param[in] ty Parameter `ty` for `compute_win64_return_info`.
   * @param[in] is_vectorcall Parameter `is_vectorcall` for `compute_win64_return_info`.
   * @param[out] out_info Parameter `out_info` for `compute_win64_return_info`.
   * @param[out] out_sret Parameter `out_sret` for `compute_win64_return_info`.
   * @return Result of `compute_win64_return_info` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t compute_win64_return_info(a_type_ptr ty, bool is_vectorcall, struct win64_arg_info_t* out_info, bool* out_sret) noexcept {
      (void)ty;
      (void)is_vectorcall;
      (void)out_info;
      if (g_decl_fail_ret_info) return llvm_gen_be_error_t::abi_classification_failed;
      if (out_sret) *out_sret = g_decl_sret_ret_info;
      return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `get_llvm_type`.
   * @details Provides mock or helper behavior for `get_llvm_type` within `tests/test_llvm_declarations.cpp`.
   * @param[in] ty Parameter `ty` for `get_llvm_type`.
   * @param[out] out_ty Parameter `out_ty` for `get_llvm_type`.
   * @return Result of `get_llvm_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_llvm_type(a_type_ptr ty, llvm::Type** out_ty) noexcept {
    if (g_decl_fail_type < 0) return llvm_gen_be_error_t::unsupported_type;
    if (g_decl_fail_type > 0) {
      --g_decl_fail_type;
      if (g_decl_fail_type == 0) return llvm_gen_be_error_t::unsupported_type;
    }
    if (ty && ty->kind == tk_pointer) {
      *out_ty = llvm::PointerType::getUnqual(*be_state->context);
    } else if (ty && ty->kind == tk_struct) {
      llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
      *out_ty = llvm::StructType::get(*be_state->context, {i64_ty, i64_ty});
    } else {
      *out_ty = llvm::Type::getInt32Ty(*be_state->context);
    }
    return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `get_variable_initializer`.
   * @details Provides mock or helper behavior for `get_variable_initializer` within `tests/test_llvm_declarations.cpp`.
   * @param[in] var Parameter `var` for `get_variable_initializer`.
   * @param[in] scope Parameter `scope` for `get_variable_initializer`.
   * @param[in] kind Parameter `kind` for `get_variable_initializer`.
   * @param[in] init Parameter `init` for `get_variable_initializer`.
   */
  void get_variable_initializer(a_variable_ptr var, a_scope_ptr scope, an_init_kind* kind, an_initializer_ptr* init) {
      (void)var; (void)scope;
      *kind = g_decl_init_kind;
      *init = g_decl_init_ptr;
  }
  
  int g_decl_coro_cleanup_in_expr = 0; ///< When non-zero, `llvm_lower_expression` creates `current_coro_cleanup_bb` in the active function.

  std::vector<a_routine_ptr> g_decl_demand_seed; ///< Routines pushed to `demand_defined_routines` during `llvm_lower_expression`.
  int g_decl_fail_proto_after_seed = 0; ///< When non-zero, arms `g_decl_fail_proto = 1` once `g_decl_demand_seed` is pushed.

  /**
   * @brief Test stub or helper for `llvm_lower_expression`.
   * @details Provides mock or helper behavior for `llvm_lower_expression` within `tests/test_llvm_declarations.cpp`.
   * @param[in] expr Parameter `expr` for `llvm_lower_expression`.
   * @param[out] out_val Parameter `out_val` for `llvm_lower_expression`.
   * @return Result of `llvm_lower_expression` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_expression(an_expr_node* expr, llvm::Value** out_val) noexcept {
    (void)expr;
    if (!g_decl_demand_seed.empty() && be_state) {
      for (a_routine_ptr dr : g_decl_demand_seed) {
        be_state->demand_defined_routines.push_back(dr);
      }
      g_decl_demand_seed.clear();
      if (g_decl_fail_proto_after_seed) {
        g_decl_fail_proto = 1;
      }
    }
    if (g_decl_fail_expr) return llvm_gen_be_error_t::unsupported_expr;
    if (g_decl_coro_cleanup_in_expr && be_state && be_state->builder && be_state->builder->GetInsertBlock()) {
      llvm::Function* cur_f = be_state->builder->GetInsertBlock()->getParent();
      be_state->current_coro_cleanup_bb = llvm::BasicBlock::Create(*be_state->context, "coro.cleanup", cur_f);
    }
    if (out_val) *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 1);
    return llvm_gen_be_error_t::ok;
  }
  /**
   * @brief Test stub or helper for `llvm_lower_asm_stmt`.
   * @details Provides mock or helper behavior for `llvm_lower_asm_stmt` within `tests/test_llvm_declarations.cpp`.
   * @param[in] stmt Parameter `stmt` for `llvm_lower_asm_stmt`.
   * @return Result of `llvm_lower_asm_stmt` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_asm_stmt(a_statement_ptr stmt) noexcept { (void)stmt; return llvm_gen_be_error_t::ok; }
  /**
   * @brief Test stub or helper for `llvm_lower_coroutine_stmt`.
   * @details Provides mock or helper behavior for `llvm_lower_coroutine_stmt` within `tests/test_llvm_declarations.cpp`.
   * @param[in] stmt Parameter `stmt` for `llvm_lower_coroutine_stmt`.
   * @return Result of `llvm_lower_coroutine_stmt` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_coroutine_stmt(a_statement_ptr stmt) noexcept { (void)stmt; return llvm_gen_be_error_t::ok; }
  /**
   * @brief Test stub or helper for `llvm_lower_coroutine_return_stmt`.
   * @details Provides mock or helper behavior for `llvm_lower_coroutine_return_stmt` within `tests/test_llvm_declarations.cpp`.
   * @param[in] stmt Parameter `stmt` for `llvm_lower_coroutine_return_stmt`.
   * @return Result of `llvm_lower_coroutine_return_stmt` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_coroutine_return_stmt(a_statement_ptr stmt) noexcept { (void)stmt; return llvm_gen_be_error_t::ok; }
  
  /**
   * @brief Test stub or helper for `get_di_location`.
   * @details Provides mock or helper behavior for `get_di_location` within `tests/test_llvm_declarations.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `get_di_location`.
   * @param[in] arg_1 Parameter `arg_1` for `get_di_location`.
   * @param[in] arg_2 Parameter `arg_2` for `get_di_location`.
   * @param[out] arg_3 Parameter `arg_3` for `get_di_location`.
   * @return Result of `get_di_location` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_di_location(llvm_gen_be_debug_state_t* arg_0, a_source_position arg_1, llvm::DIScope* arg_2, llvm::DILocation** arg_3) noexcept { (void)arg_0; (void)arg_1; (void)arg_2; (void)arg_3; return llvm_gen_be_error_t::ok; }

  /**
   * @brief Test stub or helper for `build_aapcs64_function_type`.
   * @details Provides mock or helper behavior for `build_aapcs64_function_type` within `tests/test_llvm_declarations.cpp`.
   * @param[in] ty Parameter `ty` for `build_aapcs64_function_type`.
   * @param[out] out_ty Parameter `out_ty` for `build_aapcs64_function_type`.
   * @param[out] out_attrs Parameter `out_attrs` for `build_aapcs64_function_type`.
   * @return Result of `build_aapcs64_function_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t build_aapcs64_function_type(a_type_ptr ty, llvm::FunctionType** out_ty, llvm::AttributeList* out_attrs) noexcept {
      (void)ty; (void)out_attrs; return make_mock_proto(out_ty);
  }
  
  /**
   * @brief Test stub or helper for `build_sysv_function_type`.
   * @details Provides mock or helper behavior for `build_sysv_function_type` within `tests/test_llvm_declarations.cpp`.
   * @param[in] ty Parameter `ty` for `build_sysv_function_type`.
   * @param[out] out_ty Parameter `out_ty` for `build_sysv_function_type`.
   * @param[out] out_attrs Parameter `out_attrs` for `build_sysv_function_type`.
   * @return Result of `build_sysv_function_type` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t build_sysv_function_type(a_type_ptr ty, llvm::FunctionType** out_ty, llvm::AttributeList* out_attrs) noexcept {
      (void)ty; (void)out_attrs; return make_mock_proto(out_ty);
  }
  
  /**
   * @brief Test stub or helper for `compute_aapcs64_return_info`.
   * @details Provides mock or helper behavior for `compute_aapcs64_return_info` within `tests/test_llvm_declarations.cpp`.
   * @param[in] ty Parameter `ty` for `compute_aapcs64_return_info`.
   * @param[out] out_info Parameter `out_info` for `compute_aapcs64_return_info`.
   * @param[out] out_sret Parameter `out_sret` for `compute_aapcs64_return_info`.
   * @return Result of `compute_aapcs64_return_info` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t compute_aapcs64_return_info(a_type_ptr ty, aapcs64_arg_info_t* out_info, bool* out_sret) noexcept {
      (void)ty; (void)out_info;
      if (g_decl_fail_ret_info) return llvm_gen_be_error_t::abi_classification_failed;
      *out_sret = g_decl_sret_ret_info;
      return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `compute_sysv_return_info`.
   * @details Provides mock or helper behavior for `compute_sysv_return_info` within `tests/test_llvm_declarations.cpp`.
   * @param[in] ty Parameter `ty` for `compute_sysv_return_info`.
   * @param[out] out_info Parameter `out_info` for `compute_sysv_return_info`.
   * @param[out] out_sret Parameter `out_sret` for `compute_sysv_return_info`.
   * @return Result of `compute_sysv_return_info` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t compute_sysv_return_info(a_type_ptr ty, x86_64_abi_arg_info_t* out_info, bool* out_sret) noexcept {
      (void)ty; (void)out_info;
      if (g_decl_fail_ret_info) return llvm_gen_be_error_t::abi_classification_failed;
      *out_sret = g_decl_sret_ret_info;
      return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub for `classify_x86_32_argument` in `tests/test_llvm_declarations.cpp`.
   * @details Populates `out_info->is_indirect` with `g_decl_sret_ret_info`.
   * @param[in] ty EDG type pointer.
   * @param[out] out_info Output `x86_32_arg_info_t`.
   * @return llvm_gen_be_error_t::ok or `abi_classification_failed`.
   */
  llvm_gen_be_error_t classify_x86_32_argument(a_type_ptr ty, x86_32_arg_info_t* out_info) noexcept {
      (void)ty;
      if (g_decl_fail_ret_info) return llvm_gen_be_error_t::abi_classification_failed;
      out_info->is_indirect = g_decl_sret_ret_info;
      return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub for `classify_arm32_argument` in `tests/test_llvm_declarations.cpp`.
   * @details Populates `out_info` fields for ARM32 return classification testing.
   * @param[in] ty EDG type pointer.
   * @param[out] out_info Output `arm32_arg_info_t`.
   * @return llvm_gen_be_error_t::ok or `abi_classification_failed`.
   */
  llvm_gen_be_error_t classify_arm32_argument(a_type_ptr ty, arm32_arg_info_t* out_info) noexcept {
      if (g_decl_fail_ret_info) return llvm_gen_be_error_t::abi_classification_failed;
      out_info->is_indirect = g_decl_sret_ret_info;
      out_info->is_hfa = (ty && ty->kind == tk_float);
      out_info->size = ty ? ty->size : 4;
      return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub for `compute_riscv64_return_info` in `tests/test_llvm_declarations.cpp`.
   * @details Populates `*out_sret` with `g_decl_sret_ret_info`.
   * @param[in] ty EDG return type pointer.
   * @param[out] out_info Output `riscv64_arg_info_t`.
   * @param[out] out_sret Output sret flag.
   * @return llvm_gen_be_error_t::ok or `abi_classification_failed`.
   */
  llvm_gen_be_error_t compute_riscv64_return_info(a_type_ptr ty, riscv64_arg_info_t* out_info, bool* out_sret) noexcept {
      (void)ty; (void)out_info;
      if (g_decl_fail_ret_info) return llvm_gen_be_error_t::abi_classification_failed;
      *out_sret = g_decl_sret_ret_info;
      return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub for `compute_loongarch64_return_info` in `tests/test_llvm_declarations.cpp`.
   * @details Populates `*out_sret` with `g_decl_sret_ret_info`.
   * @param[in] ty EDG return type pointer.
   * @param[out] out_info Output `loongarch64_arg_info_t`.
   * @param[out] out_sret Output sret flag.
   * @return llvm_gen_be_error_t::ok or `abi_classification_failed`.
   */
  llvm_gen_be_error_t compute_loongarch64_return_info(a_type_ptr ty, loongarch64_arg_info_t* out_info, bool* out_sret) noexcept {
      (void)ty; (void)out_info;
      if (g_decl_fail_ret_info) return llvm_gen_be_error_t::abi_classification_failed;
      *out_sret = g_decl_sret_ret_info;
      return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub for `compute_mips_return_info` in `tests/test_llvm_declarations.cpp`.
   * @details Populates `*out_sret` with `g_decl_sret_ret_info`.
   * @param[in] ty EDG return type pointer.
   * @param[in] is_soft_float Soft-float flag.
   * @param[in] is_n64 N64 ABI flag.
   * @param[out] out_info Output `mips_arg_info_t`.
   * @param[out] out_sret Output sret flag.
   * @return llvm_gen_be_error_t::ok or `abi_classification_failed`.
   */
  llvm_gen_be_error_t compute_mips_return_info(a_type_ptr ty, bool is_soft_float, bool is_n64, mips_arg_info_t* out_info, bool* out_sret) noexcept {
      (void)ty; (void)is_soft_float; (void)is_n64; (void)out_info;
      if (g_decl_fail_ret_info) return llvm_gen_be_error_t::abi_classification_failed;
      *out_sret = g_decl_sret_ret_info;
      return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub for `compute_ppc64_return_info` in `tests/test_llvm_declarations.cpp`.
   * @details Populates `*out_sret` with `g_decl_sret_ret_info`.
   * @param[in] ty EDG return type pointer.
   * @param[in] is_elfv2 ELFv2 ABI flag.
   * @param[out] out_info Output `ppc64_arg_info_t`.
   * @param[out] out_sret Output sret flag.
   * @return llvm_gen_be_error_t::ok or `abi_classification_failed`.
   */
  llvm_gen_be_error_t compute_ppc64_return_info(a_type_ptr ty, bool is_elfv2, ppc64_arg_info_t* out_info, bool* out_sret) noexcept {
      (void)ty; (void)is_elfv2; (void)out_info;
      if (g_decl_fail_ret_info) return llvm_gen_be_error_t::abi_classification_failed;
      *out_sret = g_decl_sret_ret_info;
      return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub for `compute_s390x_return_info` in `tests/test_llvm_declarations.cpp`.
   * @details Populates `*out_sret` with `g_decl_sret_ret_info`.
   * @param[in] ty EDG return type pointer.
   * @param[out] out_info Output `s390x_arg_info_t`.
   * @param[out] out_sret Output sret flag.
   * @return llvm_gen_be_error_t::ok or `abi_classification_failed`.
   */
  llvm_gen_be_error_t compute_s390x_return_info(a_type_ptr ty, s390x_arg_info_t* out_info, bool* out_sret) noexcept {
      (void)ty; (void)out_info;
      if (g_decl_fail_ret_info) return llvm_gen_be_error_t::abi_classification_failed;
      *out_sret = g_decl_sret_ret_info;
      return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub for `compute_sparc_return_info` in `tests/test_llvm_declarations.cpp`.
   * @details Populates `*out_sret` with `g_decl_sret_ret_info`.
   * @param[in] ty EDG return type pointer.
   * @param[in] is_v9 SPARC V9 flag.
   * @param[out] out_info Output `sparc_arg_info_t`.
   * @param[out] out_sret Output sret flag.
   * @return llvm_gen_be_error_t::ok or `abi_classification_failed`.
   */
  llvm_gen_be_error_t compute_sparc_return_info(a_type_ptr ty, bool is_v9, sparc_arg_info_t* out_info, bool* out_sret) noexcept {
      (void)ty; (void)is_v9; (void)out_info;
      if (g_decl_fail_ret_info) return llvm_gen_be_error_t::abi_classification_failed;
      *out_sret = g_decl_sret_ret_info;
      return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub for `classify_wasm_argument` in `tests/test_llvm_declarations.cpp`.
   * @details Populates `out_info->is_indirect` with `g_decl_sret_ret_info`.
   * @param[in] ty EDG return type pointer.
   * @param[out] out_info Output `wasm_arg_info_t`.
   * @return llvm_gen_be_error_t::ok or `abi_classification_failed`.
   */
  llvm_gen_be_error_t classify_wasm_argument(a_type_ptr ty, wasm_arg_info_t* out_info) noexcept {
      (void)ty;
      if (g_decl_fail_ret_info) return llvm_gen_be_error_t::abi_classification_failed;
      out_info->is_indirect = g_decl_sret_ret_info;
      return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `scope_for_routine`.
   * @details Provides mock or helper behavior for `scope_for_routine` within `tests/test_llvm_declarations.cpp`.
   * @param[in] r Parameter `r` for `scope_for_routine`.
   * @return Result of `scope_for_routine` (a_scope_ptr).
   */
  a_scope_ptr scope_for_routine(a_routine_ptr r) { (void)r; return g_decl_scope_for_rout; }
  
  /**
   * @brief Test stub or helper for `get_typeinfo_global`.
   * @details Provides mock or helper behavior for `get_typeinfo_global` within `tests/test_llvm_declarations.cpp`.
   * @param[in] type Parameter `type` for `get_typeinfo_global`.
   * @param[out] out_ti Parameter `out_ti` for `get_typeinfo_global`.
   * @return Result of `get_typeinfo_global` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t get_typeinfo_global(a_type_ptr type, llvm::Constant** out_ti) noexcept {
    (void)type;
    *out_ti = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
    return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub or helper for `emit_dbg_declare_for_variable`.
   * @details Provides mock or helper behavior for `emit_dbg_declare_for_variable` within `tests/test_llvm_declarations.cpp`.
   * @param[in] dbg Parameter `dbg` for `emit_dbg_declare_for_variable`.
   * @param[in] var Parameter `var` for `emit_dbg_declare_for_variable`.
   * @param[in] alloca_inst Parameter `alloca_inst` for `emit_dbg_declare_for_variable`.
   * @return Result of `emit_dbg_declare_for_variable` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t emit_dbg_declare_for_variable(llvm_gen_be_debug_state_t* dbg, a_variable_ptr var, llvm::AllocaInst* alloca_inst) noexcept {
    (void)dbg; (void)var; (void)alloca_inst;
    if (g_decl_fail_dbg_decl > 0) {
      --g_decl_fail_dbg_decl;
      if (g_decl_fail_dbg_decl == 0) return llvm_gen_be_error_t::di_metadata_failure;
    }
    return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub for `emit_dbg_global_variable` in `tests/test_llvm_declarations.cpp`.
   * @details Simulates success or `di_metadata_failure` when `g_decl_fail_dbg_global` is set.
   * @param[in] dbg Debug state pointer.
   * @param[in] var EDG variable pointer.
   * @param[in] gvar LLVM GlobalVariable pointer.
   * @return llvm_gen_be_error_t::ok or `di_metadata_failure`.
   */
  llvm_gen_be_error_t emit_dbg_global_variable(llvm_gen_be_debug_state_t* dbg, a_variable_ptr var, llvm::GlobalVariable* gvar) noexcept {
    (void)dbg; (void)var; (void)gvar;
    if (g_decl_fail_dbg_global) return llvm_gen_be_error_t::di_metadata_failure;
    return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `create_di_subprogram`.
   * @details Provides mock or helper behavior for `create_di_subprogram` within `tests/test_llvm_declarations.cpp`.
   * @param[in] dbg Parameter `dbg` for `create_di_subprogram`.
   * @param[in] routine Parameter `routine` for `create_di_subprogram`.
   * @param[in] fn Parameter `fn` for `create_di_subprogram`.
   * @param[out] out_subprogram Parameter `out_subprogram` for `create_di_subprogram`.
   * @return Result of `create_di_subprogram` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t create_di_subprogram(llvm_gen_be_debug_state_t* dbg, a_routine_ptr routine, llvm::Function* fn, llvm::DISubprogram** out_subprogram) noexcept {
    (void)dbg; (void)routine; (void)fn;
    if (g_decl_fail_di_sub) return llvm_gen_be_error_t::di_metadata_failure;
    *out_subprogram = nullptr;
    return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `push_lexical_block`.
   * @details Provides mock or helper behavior for `push_lexical_block` within `tests/test_llvm_declarations.cpp`.
   * @param[in] dbg Parameter `dbg` for `push_lexical_block`.
   * @param[in] pos Parameter `pos` for `push_lexical_block`.
   * @param[out] out_block Parameter `out_block` for `push_lexical_block`.
   * @return Result of `push_lexical_block` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t push_lexical_block(llvm_gen_be_debug_state_t* dbg, a_source_position pos, llvm::DILexicalBlock** out_block) noexcept {
    (void)dbg; (void)pos; (void)out_block;
    return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `pop_lexical_block`.
   * @details Provides mock or helper behavior for `pop_lexical_block` within `tests/test_llvm_declarations.cpp`.
   * @param[in] dbg Parameter `dbg` for `pop_lexical_block`.
   * @return Result of `pop_lexical_block` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t pop_lexical_block(llvm_gen_be_debug_state_t* dbg) noexcept {
    (void)dbg;
    if (g_decl_fail_pop_lex) return llvm_gen_be_error_t::di_metadata_failure;
    return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub for `emit_scope_imported_entities` in `tests/test_llvm_declarations.cpp`.
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
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    if (out_count) *out_count = 0;
    return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub for `llvm_emit_vtable` in `tests/test_llvm_declarations.cpp`.
   * @details Creates a `_ZTV_test_poly` global in the active LLVM module.
   * @param[in] type The EDG class type pointer.
   * @param[out] out_const Receives the vtable global constant pointer.
   * @return llvm_gen_be_error_t::ok.
   */
  llvm_gen_be_error_t llvm_emit_vtable(a_type_ptr type, llvm::Constant** out_const) noexcept {
    (void)type;
    if (g_decl_fail_vtbl) return llvm_gen_be_error_t::vtable_generation_failure;
    if (!out_const) return llvm_gen_be_error_t::invalid_argument;
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::GlobalVariable* gv = be_state->module->getNamedGlobal("_ZTV_test_poly");
    if (!gv) {
      llvm::ArrayType* arr_ty = llvm::ArrayType::get(ptr_ty, 4);
      gv = llvm::cast<llvm::GlobalVariable>(
          be_state->module->getOrInsertGlobal("_ZTV_test_poly", arr_ty));
      gv->setConstant(true);
      gv->setLinkage(llvm::GlobalValue::LinkOnceODRLinkage);
      gv->setInitializer(llvm::ConstantAggregateZero::get(arr_ty));
    }
    *out_const = gv;
    return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub for `llvm_emit_vtt` in `tests/test_llvm_declarations.cpp`.
   * @details Creates a `_ZTT_test_poly` global in the active LLVM module.
   * @param[in] type The EDG class type pointer.
   * @param[out] out_const Receives the VTT global constant pointer.
   * @return llvm_gen_be_error_t::ok.
   */
  llvm_gen_be_error_t llvm_emit_vtt(a_type_ptr type, llvm::Constant** out_const) noexcept {
    (void)type;
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    if (g_decl_fail_vtt > 0 && --g_decl_fail_vtt == 0) {
      return llvm_gen_be_error_t::vtable_generation_failure;
    }
    if (!out_const) return llvm_gen_be_error_t::invalid_argument;
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::GlobalVariable* gv = be_state->module->getNamedGlobal("_ZTT_test_poly");
    if (!gv) {
      llvm::ArrayType* arr_ty = llvm::ArrayType::get(ptr_ty, 2);
      gv = llvm::cast<llvm::GlobalVariable>(
          be_state->module->getOrInsertGlobal("_ZTT_test_poly", arr_ty));
      gv->setConstant(true);
      gv->setLinkage(llvm::GlobalValue::LinkOnceODRLinkage);
      gv->setInitializer(llvm::ConstantAggregateZero::get(arr_ty));
    }
    *out_const = gv;
    return llvm_gen_be_error_t::ok;
  }

  /**
   * @brief Test stub for `llvm_emit_msvc_vbtable` in `tests/test_llvm_declarations.cpp`.
   * @details Creates a `??_8test_poly7B@` global in the active LLVM module.
   * @param[in] type The EDG class type pointer.
   * @param[out] out_const Receives the vbtable global constant pointer.
   * @return llvm_gen_be_error_t::ok.
   */
  llvm_gen_be_error_t llvm_emit_msvc_vbtable(a_type_ptr type, llvm::Constant** out_const) noexcept {
    (void)type;
    if (g_decl_fail_vbtbl) return llvm_gen_be_error_t::vtable_generation_failure;
    if (!out_const) return llvm_gen_be_error_t::invalid_argument;
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
    llvm::GlobalVariable* gv = be_state->module->getNamedGlobal("??_8test_poly7B@");
    if (!gv) {
      llvm::ArrayType* arr_ty = llvm::ArrayType::get(i32_ty, 2);
      gv = llvm::cast<llvm::GlobalVariable>(
          be_state->module->getOrInsertGlobal("??_8test_poly7B@", arr_ty));
      gv->setConstant(true);
      gv->setLinkage(llvm::GlobalValue::LinkOnceODRLinkage);
      gv->setInitializer(llvm::ConstantAggregateZero::get(arr_ty));
    }
    *out_const = gv;
    return llvm_gen_be_error_t::ok;
  }
  
  an_il_header il_header = {};
  a_module_ptr trans_unit_module = nullptr;
  /**
   * @brief Test stub or helper for `llvm_lower_global_variable`.
   * @details Provides mock or helper behavior for `llvm_lower_global_variable` within `tests/test_llvm_declarations.cpp`.
   * @param[in] var Parameter `var` for `llvm_lower_global_variable`.
   * @return Result of `llvm_lower_global_variable` (llvm_gen_be_error_t).
   */
  extern llvm_gen_be_error_t llvm_lower_global_variable(a_variable_ptr var) noexcept;
  /**
   * @brief Forward declaration of `llvm_lower_function_prototype` for unit testing.
   * @details Lowers a single EDG routine declaration into an LLVM Function prototype.
   * @param[in] routine The EDG routine to lower.
   * @return llvm_gen_be_error_t::ok on success, or an error code.
   */
  extern llvm_gen_be_error_t llvm_lower_function_prototype(a_routine_ptr routine) noexcept;
  /**
   * @brief Forward declaration of `llvm_lower_function_body` for unit testing.
   * @details Lowers a single EDG routine definition body into LLVM IR.
   * @param[in] routine The EDG routine to lower.
   * @return llvm_gen_be_error_t::ok on success, or an error code.
   */
  extern llvm_gen_be_error_t llvm_lower_function_body(a_routine_ptr routine) noexcept;
}

/**
 * @brief Recreates a function prototype in the test module with the specified name.
 * @details Removes any existing `llvm::Function` with @p name from `be_state->module`, updates
 *          `io_routine->source_corresp.name`, and lowers the function prototype.
 * @param[in,out] io_routine Pointer to the EDG routine descriptor to update and lower.
 * @param[in] name New symbol name for the routine.
 * @return llvm_gen_be_error_t::ok on success, or an error code from `llvm_lower_function_prototype`.
 */
static llvm_gen_be_error_t recreate_test_fn_body(
    a_routine_ptr io_routine,
    const char* name) noexcept {
  io_routine->source_corresp.name = name;
  if (auto* old_f = be_state->module->getFunction(name)) {
    old_f->eraseFromParent();
  }
  return llvm_lower_function_prototype(io_routine);
}

/**
 * @brief Entry point for the declarations and C++20 modules unit test executable.
 * @details Tests globals, function prototypes/bodies, global ctors/dtors, C++20 modules, and nested scopes.
 * @return 0 on success.
 */
int main(void) {
  printf("Running test_llvm_declarations...\n");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_decl", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  a_scope root_scope = {};
  edg::il_header.primary_scope = &root_scope;

  // 1. Test global variable with TLS models
  a_variable gvar = {};
  gvar.source_corresp.name = "my_global";
  gvar.storage_class = sc_extern;
  gvar.is_thread_local = TRUE;
  root_scope.variables = &gvar;

  assert(llvm_lower_global_variables() == llvm_gen_be_error_t::ok);
  llvm::GlobalVariable* gv = state.module->getNamedGlobal("my_global");
  assert(gv != nullptr);
  assert(gv->isThreadLocal());
  assert(gv->getThreadLocalMode() == llvm::GlobalValue::GeneralDynamicTLSModel);

  // 2. Test function prototype
  a_routine fn = {};
  fn.source_corresp.name = "my_func";
  fn.storage_class = sc_extern;
  root_scope.routines = &fn;
  
  a_type fn_ty = {};
  fn_ty.kind = tk_routine;
  fn.type = &fn_ty;
  
  assert(llvm_lower_function_declarations() == llvm_gen_be_error_t::ok);
  assert(state.module->getFunction("my_func") != nullptr);

  // 3. Test function body (including constructor_inits)
  a_variable this_param = {};
  this_param.source_corresp.name = "this";
  a_type ptr_ty = {};
  ptr_ty.kind = tk_pointer;
  this_param.type = &ptr_ty;
  root_scope.variant.routine.parameters = &this_param;

  a_field ctor_field = {};
  ctor_field.offset = 0;
  ctor_field.type = &fn_ty;
  a_dynamic_init field_dip = {};
  field_dip.kind = dik_zero;
  a_constructor_init c_init = {};
  c_init.kind = cik_field;
  c_init.variant.field = &ctor_field;
  c_init.initializer = &field_dip;
  root_scope.variant.routine.constructor_inits = &c_init;

  fn.function_def_number = 1;
  a_function_def_descr def_table[2] = {};
  edg::il_header.function_def_table = def_table;
  assert(llvm_lower_function_definitions() == llvm_gen_be_error_t::ok);
  root_scope.variant.routine.constructor_inits = nullptr;
  root_scope.variant.routine.parameters = nullptr;
  
  // 4. Test global ctors and dtors
  fn.is_initialization_routine = TRUE;
  fn.is_finalization_routine = TRUE;
  assert(llvm_lower_global_ctors_and_dtors() == llvm_gen_be_error_t::ok);
  assert(state.module->getNamedGlobal("llvm.global_ctors") != nullptr);
  assert(state.module->getNamedGlobal("llvm.global_dtors") != nullptr);

  // 5. Test MSVC ctors / dtors
  if (auto* c = state.module->getNamedGlobal("llvm.global_ctors")) c->eraseFromParent();
  if (auto* d = state.module->getNamedGlobal("llvm.global_dtors")) d->eraseFromParent();
  EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
  
  assert(llvm_lower_global_ctors_and_dtors() == llvm_gen_be_error_t::ok);
  assert(state.module->getNamedGlobal("llvm.used") != nullptr);

  // 6. Test C++20 Module metadata, imports, and module init routine
  a_module mod_unit = {};
  mod_unit.kind = mk_unit_partition;
  mod_unit.resolved_file = "math_part.bmi";
  mod_unit.variant.unit_partition.name = "math:part";
  edg::trans_unit_module = &mod_unit;

  a_module imp_mod = {};
  imp_mod.kind = mk_unit;
  imp_mod.resolved_file = "core.bmi";
  imp_mod.variant.unit.name = "core";
  a_module_import_decl imp_decl = {};
  imp_decl.module_info = &imp_mod;
  edg::il_header.imported_modules = &imp_decl;

  assert(llvm_lower_module_metadata() == llvm_gen_be_error_t::ok);
  assert(state.module->getNamedMetadata("edg.cpp20.module") != nullptr);
  assert(state.module->getNamedMetadata("edg.cpp20.imports") != nullptr);
  assert(state.module->getGlobalVariable("__module_init_guard_math_part", true) != nullptr);
  assert(state.module->getFunction("__module_init_math_part") != nullptr);
  assert(state.module->getFunction("__module_init_core") != nullptr);

  // 7. Test module-internal linkage on non-exported entities
  a_variable mod_internal_var = {};
  mod_internal_var.source_corresp.name = "mod_internal_v";
  mod_internal_var.source_corresp.name_linkage = nlk_internal;
  mod_internal_var.storage_class = sc_unspecified;
  assert(llvm_lower_global_variable(&mod_internal_var) == llvm_gen_be_error_t::ok);
  llvm::GlobalVariable* mod_int_gv = state.module->getGlobalVariable("mod_internal_v", true);
  assert(mod_int_gv != nullptr && mod_int_gv->getLinkage() == llvm::GlobalValue::InternalLinkage);

  // 8. Test namespace and nested class scope traversal for variables and routines
  a_variable ns_var = {};
  ns_var.source_corresp.name = "_ZN2ns6ns_varE";
  ns_var.storage_class = sc_unspecified;

  a_routine ns_unused_rout = {};
  ns_unused_rout.source_corresp.name = "_ZN2ns9ns_unusedEv";
  ns_unused_rout.type = &fn_ty;

  a_routine ns_rout = {};
  ns_rout.source_corresp.name = "_ZN2ns7ns_funcEv";
  ns_rout.source_corresp.referenced = TRUE;
  ns_rout.type = &fn_ty;
  ns_rout.next = &ns_unused_rout;

  a_variable cls_static_var = {};
  cls_static_var.source_corresp.name = "_ZN2ns3Cls8s_memberE";
  cls_static_var.storage_class = sc_unspecified;

  a_routine cls_method = {};
  cls_method.source_corresp.name = "_ZN2ns3Cls6methodEv";
  cls_method.source_corresp.referenced = TRUE;
  cls_method.type = &fn_ty;

  a_scope cls_scope = {};
  cls_scope.variables = &cls_static_var;
  cls_scope.routines = &cls_method;

  a_class_type_supplement cls_supp = {};
  cls_supp.assoc_scope = &cls_scope;
  cls_supp.highest_virtual_function_number = 1;
  a_type cls_ty = {};
  cls_ty.kind = tk_class;
  cls_ty.variant.class_struct_union.any_virtual_functions_including_in_base_classes = TRUE;
  cls_ty.variant.class_struct_union.any_virtual_base_classes = TRUE;
  cls_ty.variant.class_struct_union.extra_info = &cls_supp;

  a_scope ns_scope = {};
  ns_scope.variables = &ns_var;
  ns_scope.routines = &ns_rout;
  ns_scope.types = &cls_ty;

  a_namespace ns_entry = {};
  ns_entry.is_namespace_alias = FALSE;
  ns_entry.variant.assoc_scope = &ns_scope;
  root_scope.namespaces = &ns_entry;

  EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
  assert(llvm_lower_global_variables() == llvm_gen_be_error_t::ok);
  assert(state.module->getNamedGlobal("_ZN2ns6ns_varE") != nullptr);
  assert(state.module->getNamedGlobal("_ZN2ns3Cls8s_memberE") != nullptr);
  assert(state.module->getNamedGlobal("_ZTV_test_poly") != nullptr);
  assert(state.module->getNamedGlobal("_ZTT_test_poly") != nullptr);

  assert(llvm_lower_function_declarations() == llvm_gen_be_error_t::ok);
  assert(state.module->getFunction("_ZN2ns7ns_funcEv") != nullptr);
  assert(state.module->getFunction("_ZN2ns3Cls6methodEv") != nullptr);

  // =========================================================================
  // 9. Exhaustive branch coverage for src/llvm_gen_be_decl.cpp
  // =========================================================================
  {
    // A. Global variable attributes, initializers, TLS models, comdats, and error branches
    assert(llvm_lower_global_variable(nullptr) == llvm_gen_be_error_t::ok);
    a_variable unnamed_var = {};
    assert(llvm_lower_global_variable(&unnamed_var) == llvm_gen_be_error_t::ok);

    a_variable gv_test = {};
    gv_test.source_corresp.name = "gv_test_1";
    g_decl_fail_type = 1;
    assert(llvm_lower_global_variable(&gv_test) == llvm_gen_be_error_t::unsupported_type);
    g_decl_fail_type = 0;

    // Linkage combinations: sc_static, is_inline, is_weak, is_weakref
    gv_test.storage_class = sc_static;
    gv_test.is_inline = TRUE;
    gv_test.is_weak = TRUE;
    assert(llvm_lower_global_variable(&gv_test) == llvm_gen_be_error_t::ok);
    gv_test.is_weak = FALSE;
    gv_test.is_weakref = TRUE;
    gv_test.source_corresp.name = "gv_test_weakref";
    assert(llvm_lower_global_variable(&gv_test) == llvm_gen_be_error_t::ok);
    gv_test.is_weakref = FALSE;

    // Initializers: initk_static (error & ok), initk_dynamic (dip==nullptr, dip->kind!=dik_constant, follows_an_exec_statement==TRUE, dik_constant && !follows)
    a_constant dummy_con = {};
    an_initializer init_u = {};
    init_u.constant = &dummy_con;
    g_decl_init_kind = initk_static;
    g_decl_init_ptr = &init_u;
    g_decl_fail_const = 1;
    gv_test.source_corresp.name = "gv_static_init";
    assert(llvm_lower_global_variable(&gv_test) == llvm_gen_be_error_t::constant_eval_failure);
    g_decl_fail_const = 0;
    assert(llvm_lower_global_variable(&gv_test) == llvm_gen_be_error_t::ok);

    g_decl_init_kind = initk_dynamic;
    init_u.dynamic = nullptr;
    gv_test.source_corresp.name = "gv_dyn_null";
    assert(llvm_lower_global_variable(&gv_test) == llvm_gen_be_error_t::ok);

    a_dynamic_init dip_gv = {};
    init_u.dynamic = &dip_gv;
    dip_gv.kind = dik_expression;
    gv_test.source_corresp.name = "gv_dyn_expr";
    assert(llvm_lower_global_variable(&gv_test) == llvm_gen_be_error_t::ok);

    dip_gv.kind = dik_constant;
    dip_gv.follows_an_exec_statement = TRUE;
    gv_test.source_corresp.name = "gv_dyn_follows";
    assert(llvm_lower_global_variable(&gv_test) == llvm_gen_be_error_t::ok);

    dip_gv.follows_an_exec_statement = FALSE;
    gv_test.source_corresp.name = "gv_dyn_const";
    assert(llvm_lower_global_variable(&gv_test) == llvm_gen_be_error_t::ok);
    g_decl_init_kind = initk_none;
    g_decl_init_ptr = nullptr;

    // When a Function with the same name already exists in module -> getNamedGlobal returns nullptr (if (gvar) is false)
    gv_test.source_corresp.name = "my_func";
    assert(llvm_lower_global_variable(&gv_test) == llvm_gen_be_error_t::ok);

    // Attributes on GlobalVariable (extract_attr_string, ak_section, ak_visibility, ak_dllexport, ak_dllimport, ak_cold, ak_hot, ak_naked, ak_tls_model)
    a_constant c_str_sec = {}; c_str_sec.kind = ck_string; c_str_sec.variant.string.value = ".sec_c"; c_str_sec.variant.string.length = 7;
    a_constant c_non_str = {}; c_non_str.kind = ck_integer;
    an_attribute_arg aa_con_ok = {}; aa_con_ok.kind = aak_constant; aa_con_ok.variant.constant = &c_str_sec;
    an_attribute_arg aa_con_null = {}; aa_con_null.kind = aak_constant; aa_con_null.variant.constant = nullptr;
    an_attribute_arg aa_con_int = {}; aa_con_int.kind = aak_constant; aa_con_int.variant.constant = &c_non_str;
    an_attribute_arg aa_tok_quoted = {}; aa_tok_quoted.kind = aak_token; aa_tok_quoted.variant.token = "\".sec_q\"";
    an_attribute_arg aa_tok_unq = {}; aa_tok_unq.kind = aak_raw_token; aa_tok_unq.variant.token = "hidden";
    an_attribute_arg aa_tok_null = {}; aa_tok_null.kind = aak_token; aa_tok_null.variant.token = nullptr;
    an_attribute_arg aa_other = {}; aa_other.kind = static_cast<an_attribute_arg_kind>(99);
    an_attribute_arg aa_vis_prot = {}; aa_vis_prot.kind = aak_token; aa_vis_prot.variant.token = "\"protected\"";
    an_attribute_arg aa_vis_def = {}; aa_vis_def.kind = aak_token; aa_vis_def.variant.token = "default";
    an_attribute_arg aa_vis_unk = {}; aa_vis_unk.kind = aak_token; aa_vis_unk.variant.token = "unknown";
    an_attribute_arg aa_tls_gd = {}; aa_tls_gd.kind = aak_token; aa_tls_gd.variant.token = "global-dynamic";
    an_attribute_arg aa_tls_ld = {}; aa_tls_ld.kind = aak_token; aa_tls_ld.variant.token = "local-dynamic";
    an_attribute_arg aa_tls_ie = {}; aa_tls_ie.kind = aak_token; aa_tls_ie.variant.token = "initial-exec";
    an_attribute_arg aa_tls_le = {}; aa_tls_le.kind = aak_token; aa_tls_le.variant.token = "local-exec";
    an_attribute_arg aa_tls_unk = {}; aa_tls_unk.kind = aak_token; aa_tls_unk.variant.token = "other-tls";

    an_attribute at1 = {}, at2 = {}, at3 = {}, at4 = {}, at5 = {}, at6 = {}, at7 = {}, at8 = {},
                 at12 = {}, at13 = {}, at14 = {}, at15 = {}, at16 = {}, at17 = {},
                 at_used = {}, at_alias_empty = {}, at_alias_miss = {}, at_alias_ok = {},
                 at_ifunc_empty = {}, at_ifunc_miss = {}, at_ifunc_ok = {};
    an_attribute_arg aa_alias_tgt = {}; aa_alias_tgt.kind = aak_token; aa_alias_tgt.variant.token = "my_global";
    an_attribute_arg aa_alias_miss = {}; aa_alias_miss.kind = aak_token; aa_alias_miss.variant.token = "nonexistent_sym";
    an_attribute_arg aa_ifunc_tgt = {}; aa_ifunc_tgt.kind = aak_token; aa_ifunc_tgt.variant.token = "my_func";
    at1.kind = ak_section; at1.arguments = nullptr; at1.next = &at2;
    at2.kind = ak_section; at2.arguments = &aa_con_null; at2.next = &at3;
    at3.kind = ak_section; at3.arguments = &aa_con_int; at3.next = &at4;
    at4.kind = ak_section; at4.arguments = &aa_con_ok; at4.next = &at5;
    at5.kind = ak_section; at5.arguments = &aa_tok_quoted; at5.next = &at6;
    at6.kind = ak_section; at6.arguments = &aa_tok_null; at6.next = &at7;
    at7.kind = ak_section; at7.arguments = &aa_other; at7.next = &at8;
    at8.kind = ak_section; at8.arguments = &aa_tok_unq; at8.next = &at12;
    at12.kind = ak_dllexport; at12.next = &at13;
    at13.kind = ak_dllimport; at13.next = &at14;
    at14.kind = ak_cold; at14.next = &at15;
    at15.kind = ak_hot; at15.next = &at16;
    at16.kind = ak_naked; at16.next = &at_used;
    at_used.kind = ak_used; at_used.next = &at_alias_empty;
    at_alias_empty.kind = ak_alias; at_alias_empty.arguments = nullptr; at_alias_empty.next = &at_alias_miss;
    at_alias_miss.kind = ak_alias; at_alias_miss.arguments = &aa_alias_miss; at_alias_miss.next = &at_alias_ok;
    at_alias_ok.kind = ak_alias; at_alias_ok.arguments = &aa_alias_tgt; at_alias_ok.next = &at_ifunc_empty;
    at_ifunc_empty.kind = ak_ifunc; at_ifunc_empty.arguments = nullptr; at_ifunc_empty.next = &at_ifunc_miss;
    at_ifunc_miss.kind = ak_ifunc; at_ifunc_miss.arguments = &aa_alias_miss; at_ifunc_miss.next = &at_ifunc_ok;
    at_ifunc_ok.kind = ak_ifunc; at_ifunc_ok.arguments = &aa_ifunc_tgt; at_ifunc_ok.next = &at17;
    at17.kind = ak_unused; at17.next = nullptr;

    if (auto* u = state.module->getGlobalVariable("llvm.used")) u->eraseFromParent();
    gv_test.source_corresp.name = "gv_attrs_inline";
    gv_test.source_corresp.attributes = &at1;
    gv_test.is_thread_local = TRUE;
    gv_test.is_inline = TRUE;
    gv_test.comdat_group = nullptr;
    assert(llvm_lower_global_variable(&gv_test) == llvm_gen_be_error_t::ok);

    gv_test.source_corresp.name = "gv_attrs_comdat_grp";
    gv_test.comdat_group = (char*)"custom_comdat";
    assert(llvm_lower_global_variable(&gv_test) == llvm_gen_be_error_t::ok);

    // B. lower_variables_in_scope & llvm_lower_global_variables edge & error branches
    edg::il_header.primary_scope = nullptr;
    assert(llvm_lower_global_variables() == llvm_gen_be_error_t::ok);
    edg::il_header.primary_scope = &root_scope;

    // Error from llvm_lower_global_variable inside lower_variables_in_scope
    g_decl_fail_type = 1;
    assert(llvm_lower_global_variables() == llvm_gen_be_error_t::unsupported_type);
    g_decl_fail_type = 0;

    // Error from nested namespace in lower_variables_in_scope, plus namespace alias and null assoc_scope
    a_namespace ns_alias = {}, ns_empty = {};
    ns_alias.is_namespace_alias = TRUE; ns_alias.next = &ns_empty;
    ns_empty.is_namespace_alias = FALSE; ns_empty.variant.assoc_scope = nullptr; ns_empty.next = &ns_entry;
    root_scope.namespaces = &ns_alias;
    g_decl_fail_type = 2; // 1st for root_scope.variables (gvar), 2nd for ns_scope.variables (ns_var)
    assert(llvm_lower_global_variables() == llvm_gen_be_error_t::unsupported_type);
    g_decl_fail_type = 0;

    // Error from llvm_emit_vtable and llvm_emit_vtt in lower_variables_in_scope
    g_decl_fail_vtbl = 1;
    assert(llvm_lower_global_variables() == llvm_gen_be_error_t::vtable_generation_failure);
    g_decl_fail_vtbl = 0;

    g_decl_fail_vtt = 1;
    assert(llvm_lower_global_variables() == llvm_gen_be_error_t::vtable_generation_failure);
    g_decl_fail_vtt = 0;

    // MSVC target in lower_variables_in_scope:
    // 1) any_virtual_functions == TRUE && any_virtual_base_classes == TRUE -> skips llvm_emit_msvc_vbtable
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    assert(llvm_lower_global_variables() == llvm_gen_be_error_t::ok);
    // 2) any_virtual_functions == FALSE && any_virtual_base_classes == TRUE -> calls llvm_emit_msvc_vbtable (error & ok)
    cls_ty.variant.class_struct_union.any_virtual_functions_including_in_base_classes = FALSE;
    g_decl_fail_vbtbl = 1;
    assert(llvm_lower_global_variables() == llvm_gen_be_error_t::vtable_generation_failure);
    g_decl_fail_vbtbl = 0;
    assert(llvm_lower_global_variables() == llvm_gen_be_error_t::ok);

    // Non-polymorphic struct, union without extra_info, union with extra_info but null assoc_scope, placeholder scope, non-class type (tk_integer), and error in class assoc_scope
    a_type st_plain = {}, un_no_extra = {}, un_no_scope = {}, un_placeholder = {}, int_tp = {};
    a_class_type_supplement un_supp = {}, ph_supp = {};
    a_scope ph_scope = {};
    ph_scope.is_placeholder_scope = TRUE;
    ph_supp.assoc_scope = &ph_scope;
    st_plain.kind = tk_struct; st_plain.next = &un_no_extra;
    un_no_extra.kind = tk_union; un_no_extra.next = &un_no_scope;
    un_no_scope.kind = tk_union; un_no_scope.variant.class_struct_union.extra_info = &un_supp; un_no_scope.next = &un_placeholder;
    un_placeholder.kind = tk_union; un_placeholder.variant.class_struct_union.extra_info = &ph_supp; un_placeholder.next = &int_tp;
    int_tp.kind = tk_integer; int_tp.next = nullptr;
    cls_ty.next = &st_plain;
    g_decl_fail_type = 3; // 1st gvar, 2nd ns_var, 3rd cls_static_var
    assert(llvm_lower_global_variables() == llvm_gen_be_error_t::unsupported_type);
    g_decl_fail_type = 0;
    assert(llvm_lower_global_variables() == llvm_gen_be_error_t::ok);

    // C. llvm_lower_function_prototype, lower_routine_decls_in_scope, llvm_lower_function_declarations
    edg::il_header.primary_scope = nullptr;
    assert(llvm_lower_function_declarations() == llvm_gen_be_error_t::ok);
    edg::il_header.primary_scope = &root_scope;

    assert(llvm_lower_function_prototype(nullptr) == llvm_gen_be_error_t::ok);
    a_routine unnamed_rout = {};
    assert(llvm_lower_function_prototype(&unnamed_rout) == llvm_gen_be_error_t::ok);

    // Test all 12 target triples in llvm_lower_function_prototype (including aarch64-pc-windows-msvc)
    const char* triples[] = {
      "x86_64-pc-windows-msvc",
      "aarch64-pc-windows-msvc",
      "aarch64-unknown-linux-gnu",
      "i686-unknown-linux-gnu",
      "armv7-unknown-linux-gnueabihf",
      "thumbv7-unknown-linux-gnueabihf",
      "riscv64-unknown-linux-gnu",
      "riscv32-unknown-linux-gnu",
      "loongarch64-unknown-linux-gnu",
      "loongarch32-unknown-linux-gnu",
      "mips64-unknown-linux-gnu",
      "powerpc64le-unknown-linux-gnu",
      "powerpc-unknown-linux-gnu",
      "s390x-ibm-linux-gnu",
      "sparc-unknown-linux-gnu",
      "sparcv9-unknown-linux-gnu",
      "sparcel-unknown-linux-gnu",
      "wasm32-unknown-unknown",
      "x86_64-unknown-linux-gnu"
    };
    for (size_t i = 0; i < sizeof(triples) / sizeof(triples[0]); ++i) {
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), triples[i]);
      a_routine r_abi = {};
      std::string rname = "fn_abi_" + std::to_string(i);
      r_abi.source_corresp.name = rname.c_str();
      r_abi.type = &fn_ty;
      assert(llvm_lower_function_prototype(&r_abi) == llvm_gen_be_error_t::ok);
    }

    // Prototype error & null func_ty
    a_routine r_proto = {};
    r_proto.source_corresp.name = "fn_proto_test";
    r_proto.type = &fn_ty;
    g_decl_fail_proto = 1;
    assert(llvm_lower_function_prototype(&r_proto) == llvm_gen_be_error_t::abi_classification_failed);
    g_decl_fail_proto = 0;
    g_decl_null_proto = 1;
    assert(llvm_lower_function_prototype(&r_proto) == llvm_gen_be_error_t::ok);
    g_decl_null_proto = 0;

    // Linkage & attributes on function prototype:
    // function_def_number != 0 with sc_static, nlk_internal, is_inline (with & without use_comdat), has_internal_linkage_attribute,
    // always_inline, never_inline, sanitizers, calling conventions, does_not_return, restrict params
    r_proto.function_def_number = 1;
    r_proto.storage_class = sc_static;
    r_proto.source_corresp.attributes = &at14; // ak_cold, ak_hot, ak_naked, ak_unused on Function!
    r_proto.always_inline = TRUE;
    r_proto.never_inline = TRUE;
    sanitize_address = TRUE;
    sanitize_thread = TRUE;
    sanitize_memory = TRUE;
    sanitize_undefined = TRUE;
    assert(llvm_lower_function_prototype(&r_proto) == llvm_gen_be_error_t::ok);
    sanitize_address = FALSE;
    sanitize_thread = FALSE;
    sanitize_memory = FALSE;
    sanitize_undefined = FALSE;

    r_proto.storage_class = sc_unspecified;
    r_proto.source_corresp.name_linkage = nlk_internal;
    r_proto.source_corresp.name = "fn_proto_nlk_int";
    assert(llvm_lower_function_prototype(&r_proto) == llvm_gen_be_error_t::ok);

    r_proto.source_corresp.name_linkage = nlk_external;
    r_proto.is_inline = TRUE;
    r_proto.use_comdat = FALSE;
    r_proto.source_corresp.name = "fn_proto_inline_nocomdat";
    assert(llvm_lower_function_prototype(&r_proto) == llvm_gen_be_error_t::ok);

    r_proto.use_comdat = TRUE;
    r_proto.has_internal_linkage_attribute = TRUE;
    r_proto.source_corresp.name = "fn_proto_use_comdat";
    assert(llvm_lower_function_prototype(&r_proto) == llvm_gen_be_error_t::ok);

    // Calling conventions, does_not_return, and restrict params
    a_type non_rout_ty = {}; non_rout_ty.kind = tk_integer;
    r_proto.type = &non_rout_ty;
    r_proto.source_corresp.name = "fn_non_rout_ty";
    assert(llvm_lower_function_prototype(&r_proto) == llvm_gen_be_error_t::ok);

    a_routine_type_supplement r_supp = {};
    fn_ty.variant.routine.extra_info = &r_supp;
    r_proto.type = &fn_ty;
    a_param_type pt1 = {}, pt2 = {};
    pt1.type = nullptr; pt1.next = &pt2;
    pt2.type = &non_rout_ty; pt2.next = nullptr;
    r_supp.param_type_list = &pt1;

    g_decl_proto_mode = 2; // 2 args
    int ccs[] = {cc_stdcall, cc_fastcall, cc_thiscall, cc_vectorcall, cc_default};
    for (int i = 0; i < 5; ++i) {
      r_supp.calling_convention = static_cast<a_calling_convention>(ccs[i]);
      r_supp.does_not_return = (i % 2 == 0) ? TRUE : FALSE;
      g_decl_tqual = (i % 2 == 0) ? TQ_RESTRICT : 0;
      std::string cname = "fn_cc_" + std::to_string(i);
      r_proto.source_corresp.name = cname.c_str();
      assert(llvm_lower_function_prototype(&r_proto) == llvm_gen_be_error_t::ok);
    }
    g_decl_tqual = 0;
    g_decl_proto_mode = 0;
    fn_ty.variant.routine.extra_info = nullptr;

    // Traverse all scopes & types in lower_routine_decls_in_scope (including st_plain, un_no_extra, un_no_scope, int_tp)
    assert(llvm_lower_function_declarations() == llvm_gen_be_error_t::ok);

    // Error propagation in lower_routine_decls_in_scope (root, namespace, class)
    g_decl_fail_proto = 1;
    assert(llvm_lower_function_declarations() == llvm_gen_be_error_t::abi_classification_failed);
    g_decl_fail_proto = 0;

    root_scope.routines = nullptr;
    g_decl_fail_proto = 1;
    assert(llvm_lower_function_declarations() == llvm_gen_be_error_t::abi_classification_failed);
    g_decl_fail_proto = 0;

    ns_scope.routines = nullptr;
    g_decl_fail_proto = 1;
    assert(llvm_lower_function_declarations() == llvm_gen_be_error_t::abi_classification_failed);
    g_decl_fail_proto = 0;
    root_scope.routines = &fn;
    ns_scope.routines = &ns_rout;

    // D. llvm_lower_thunk_body, lower_single_routine_definition, C1/D1/D0 aliases, TLS init wrappers
    edg::il_header.primary_scope = nullptr;
    assert(llvm_lower_function_definitions() == llvm_gen_be_error_t::ok);
    edg::il_header.primary_scope = &root_scope;

    // 1) C1/D0 alias entries
    a_routine base_dtor = {}; base_dtor.source_corresp.name = "_ZN1CD2Ev"; base_dtor.type = &fn_ty;
    g_decl_proto_mode = 1; // void(ptr)
    assert(llvm_lower_function_prototype(&base_dtor) == llvm_gen_be_error_t::ok);

    a_routine d1_alias = {};
    d1_alias.source_corresp.name = "_ZN1CD1Ev";
    d1_alias.primary_ctor_or_dtor = &base_dtor;
    d1_alias.is_alias_entry = TRUE;
    d1_alias.ctor_dtor_kind = cdk_complete;
    d1_alias.type = &fn_ty;
    assert(llvm_lower_function_prototype(&d1_alias) == llvm_gen_be_error_t::ok);

    a_routine d0_alias = {};
    d0_alias.source_corresp.name = "_ZN1CD0Ev";
    d0_alias.primary_ctor_or_dtor = &base_dtor;
    d0_alias.is_alias_entry = TRUE;
    d0_alias.ctor_dtor_kind = cdk_deleting;
    d0_alias.type = &fn_ty;
    assert(llvm_lower_function_prototype(&d0_alias) == llvm_gen_be_error_t::ok);

    // D0 alias with 0 args and non-void return alias
    g_decl_proto_mode = 0; // void()
    a_routine d0_noarg = {};
    d0_noarg.source_corresp.name = "_ZN1CD0Ev_noarg";
    a_routine base_noarg = {}; base_noarg.source_corresp.name = "_ZN1CD2Ev_noarg"; base_noarg.type = &fn_ty;
    assert(llvm_lower_function_prototype(&base_noarg) == llvm_gen_be_error_t::ok);
    d0_noarg.primary_ctor_or_dtor = &base_noarg;
    d0_noarg.is_alias_entry = TRUE;
    d0_noarg.ctor_dtor_kind = cdk_deleting;
    d0_noarg.type = &fn_ty;
    assert(llvm_lower_function_prototype(&d0_noarg) == llvm_gen_be_error_t::ok);

    g_decl_proto_mode = 2; // i32(ptr, i32)
    a_routine c1_nonvoid = {};
    c1_nonvoid.source_corresp.name = "_ZN1CC1Ev_ret";
    a_routine c2_nonvoid = {}; c2_nonvoid.source_corresp.name = "_ZN1CC2Ev_ret"; c2_nonvoid.type = &fn_ty;
    assert(llvm_lower_function_prototype(&c2_nonvoid) == llvm_gen_be_error_t::ok);
    c1_nonvoid.primary_ctor_or_dtor = &c2_nonvoid;
    c1_nonvoid.is_alias_entry = TRUE;
    c1_nonvoid.type = &fn_ty;
    assert(llvm_lower_function_prototype(&c1_nonvoid) == llvm_gen_be_error_t::ok);

    // Alias where target_func is missing, alias where target_func is found but alias_func is missing, and non-alias with primary_ctor_or_dtor
    a_routine bad_target = {}; bad_target.source_corresp.name = "missing_primary_target";
    a_routine alias_missing_tgt = {};
    alias_missing_tgt.source_corresp.name = "_ZN1CD1Ev_miss";
    alias_missing_tgt.primary_ctor_or_dtor = &bad_target;
    alias_missing_tgt.is_alias_entry = TRUE;

    a_routine alias_missing_self = {};
    alias_missing_self.source_corresp.name = "_ZN1CD1Ev_self_miss";
    alias_missing_self.primary_ctor_or_dtor = &base_dtor; // target_func exists, alias_func does not!
    alias_missing_self.is_alias_entry = TRUE;

    a_routine non_alias_cdtor = {};
    non_alias_cdtor.source_corresp.name = "_ZN1CC2Ev_body";
    non_alias_cdtor.primary_ctor_or_dtor = &base_dtor;
    non_alias_cdtor.is_alias_entry = FALSE;
    non_alias_cdtor.function_def_number = 0;

    d1_alias.next = &d0_alias;
    d0_alias.next = &d0_noarg;
    d0_noarg.next = &c1_nonvoid;
    c1_nonvoid.next = &alias_missing_tgt;
    alias_missing_tgt.next = &alias_missing_self;
    alias_missing_self.next = &non_alias_cdtor;
    non_alias_cdtor.next = nullptr;
    root_scope.routines = &d1_alias;
    root_scope.namespaces = nullptr;
    assert(llvm_lower_function_definitions() == llvm_gen_be_error_t::ok);
    // Second call exercises !alias_func->empty()!
    assert(llvm_lower_function_definitions() == llvm_gen_be_error_t::ok);

    // 2) TLS init aliases (Itanium with new & existing __tls_init and __cxa_thread_atexit, and MSVC .CRT$XDU)
    g_decl_proto_mode = 0;
    a_routine tls_alias1 = {}, tls_alias2 = {}, tls_alias_miss = {};
    tls_alias1.source_corresp.name = "_ZTW3var1"; tls_alias1.is_tls_init_alias = TRUE; tls_alias1.type = &fn_ty;
    tls_alias2.source_corresp.name = "_ZTW3var2"; tls_alias2.is_tls_init_alias = TRUE; tls_alias2.type = &fn_ty;
    tls_alias_miss.source_corresp.name = "_ZTW_uncreated"; tls_alias_miss.is_tls_init_alias = TRUE;
    assert(llvm_lower_function_prototype(&tls_alias1) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_prototype(&tls_alias2) == llvm_gen_be_error_t::ok);
    tls_alias1.next = &tls_alias2;
    tls_alias2.next = &tls_alias_miss;
    tls_alias_miss.next = nullptr;
    root_scope.routines = &tls_alias1;
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
    assert(llvm_lower_function_definitions() == llvm_gen_be_error_t::ok);
    // Call again to hit !alias_func->empty() on tls_alias1
    assert(llvm_lower_function_definitions() == llvm_gen_be_error_t::ok);

    // MSVC TLS init alias
    a_routine tls_alias_msvc = {};
    tls_alias_msvc.source_corresp.name = "_ZTW_msvc"; tls_alias_msvc.is_tls_init_alias = TRUE; tls_alias_msvc.type = &fn_ty;
    assert(llvm_lower_function_prototype(&tls_alias_msvc) == llvm_gen_be_error_t::ok);
    root_scope.routines = &tls_alias_msvc;
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    assert(llvm_lower_function_definitions() == llvm_gen_be_error_t::ok);

    // 3) Thunk bodies (llvm_lower_thunk_body)
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
    a_routine thunk_unnamed = {}, thunk_uncreated = {}, thunk_bad_tgt = {}, thunk_noarg = {}, thunk_void = {}, thunk_cov1 = {}, thunk_cov2 = {};
    a_routine ovr_unnamed = {}, ovr_void = {}, ovr_cov = {};
    ovr_unnamed.source_corresp.name = nullptr;
    ovr_void.source_corresp.name = "_ZN1D4void_fnEPv"; ovr_void.type = &fn_ty;
    ovr_cov.source_corresp.name = "_ZN1D6cov_fnEPvi"; ovr_cov.type = &fn_ty;
    g_decl_proto_mode = 1; // void(ptr)
    assert(llvm_lower_function_prototype(&ovr_void) == llvm_gen_be_error_t::ok);
    g_decl_proto_mode = 4; // ptr(ptr, i32)
    assert(llvm_lower_function_prototype(&ovr_cov) == llvm_gen_be_error_t::ok);

    thunk_unnamed.source_corresp.name = nullptr;
    thunk_unnamed.overriding_function_for_wrapper = &ovr_void;
    thunk_uncreated.source_corresp.name = "_ZTv_uncreated";
    thunk_uncreated.overriding_function_for_wrapper = &ovr_void;

    // Thunk with missing target_func -> code_gen_failure
    g_decl_proto_mode = 1;
    thunk_bad_tgt.source_corresp.name = "_ZTv_bad_tgt";
    thunk_bad_tgt.overriding_function_for_wrapper = &ovr_unnamed;
    thunk_bad_tgt.type = &fn_ty;
    assert(llvm_lower_function_prototype(&thunk_bad_tgt) == llvm_gen_be_error_t::ok);
    root_scope.routines = &thunk_bad_tgt;
    assert(llvm_lower_function_definitions() == llvm_gen_be_error_t::code_gen_failure);
    // Clean up partially-created thunk_bad_tgt function from module
    state.module->getFunction("_ZTv_bad_tgt")->eraseFromParent();

    // Thunk with 0 args
    g_decl_proto_mode = 0;
    thunk_noarg.source_corresp.name = "_ZTv_noarg";
    thunk_noarg.overriding_function_for_wrapper = &ovr_void;
    thunk_noarg.type = &fn_ty;
    assert(llvm_lower_function_prototype(&thunk_noarg) == llvm_gen_be_error_t::ok);

    // Void thunk with delta == 0, vcall_index == 0, return_delta == 0, vbase_index == 0
    g_decl_proto_mode = 1;
    thunk_void.source_corresp.name = "_ZTv_void";
    thunk_void.overriding_function_for_wrapper = &ovr_void;
    thunk_void.type = &fn_ty;
    assert(llvm_lower_function_prototype(&thunk_void) == llvm_gen_be_error_t::ok);

    // Non-void thunk with delta != 0, vcall_index != 0, extra arg, return_delta == 0, vbase_index == 0 (hits CreateRet(call))
    g_decl_proto_mode = 4; // ptr(ptr, i32)
    thunk_cov1.source_corresp.name = "_ZTv_nonvoid_nocov";
    thunk_cov1.overriding_function_for_wrapper = &ovr_cov;
    thunk_cov1.delta = -8;
    thunk_cov1.vcall_index = 1;
    thunk_cov1.type = &fn_ty;
    assert(llvm_lower_function_prototype(&thunk_cov1) == llvm_gen_be_error_t::ok);

    // Covariant return thunk with return_delta != 0 and vbase_index != 0, one with vbase_index != 0 && return_delta == 0, and one with return_delta != 0 && vbase_index == 0
    thunk_cov2.source_corresp.name = "_ZTch_cov_both";
    thunk_cov2.overriding_function_for_wrapper = &ovr_cov;
    thunk_cov2.return_delta = 16;
    thunk_cov2.vbase_index = 2;
    thunk_cov2.type = &fn_ty;
    assert(llvm_lower_function_prototype(&thunk_cov2) == llvm_gen_be_error_t::ok);

    a_routine thunk_cov3 = {};
    thunk_cov3.source_corresp.name = "_ZTch_cov_vbase_only";
    thunk_cov3.overriding_function_for_wrapper = &ovr_cov;
    thunk_cov3.return_delta = 0;
    thunk_cov3.vbase_index = 2;
    thunk_cov3.type = &fn_ty;
    assert(llvm_lower_function_prototype(&thunk_cov3) == llvm_gen_be_error_t::ok);

    a_routine thunk_cov4 = {};
    thunk_cov4.source_corresp.name = "_ZTch_cov_nv_only";
    thunk_cov4.overriding_function_for_wrapper = &ovr_cov;
    thunk_cov4.return_delta = 16;
    thunk_cov4.vbase_index = 0;
    thunk_cov4.type = &fn_ty;
    assert(llvm_lower_function_prototype(&thunk_cov4) == llvm_gen_be_error_t::ok);

    thunk_unnamed.next = &thunk_uncreated;
    thunk_uncreated.next = &thunk_noarg;
    thunk_noarg.next = &thunk_void;
    thunk_void.next = &thunk_cov1;
    thunk_cov1.next = &thunk_cov2;
    thunk_cov2.next = &thunk_cov3;
    thunk_cov3.next = &thunk_cov4;
    thunk_cov4.next = nullptr;
    root_scope.routines = &thunk_unnamed;
    assert(llvm_lower_function_definitions() == llvm_gen_be_error_t::ok);
    // Call again to hit !func->empty() on lowered thunks
    assert(llvm_lower_function_definitions() == llvm_gen_be_error_t::ok);

    // E. llvm_lower_function_body, llvm_lower_constructor_inits, llvm_emit_ctor_dtor_vptr_init
    assert(llvm_lower_function_body(nullptr) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&unnamed_rout) == llvm_gen_be_error_t::ok);
    a_routine fn_uncreated = {}; fn_uncreated.source_corresp.name = "uncreated_body_fn"; fn_uncreated.function_def_number = 1;
    assert(llvm_lower_function_body(&fn_uncreated) == llvm_gen_be_error_t::ok);

    // func_scope == nullptr
    g_decl_proto_mode = 3; // void(ptr, i64, i64, i32)
    a_routine fn_body = {};
    fn_body.source_corresp.name = "fn_body_1";
    fn_body.function_def_number = 1;
    fn_body.type = &fn_ty;
    def_table[1].scope = nullptr;
    g_decl_scope_for_rout = nullptr;
    assert(llvm_lower_function_prototype(&fn_body) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);

    // Now set g_decl_scope_for_rout = &body_scope
    a_scope body_scope = {};
    g_decl_scope_for_rout = &body_scope;

    // Debug info errors in llvm_lower_function_body: create_di_subprogram, emit_dbg_declare (param & lvar), pop_lexical_block
    llvm_gen_be_debug_state_t dbg_st = {};
    state.dbg_state = &dbg_st;

    assert(recreate_test_fn_body(&fn_body, "fn_dbg_err1") == llvm_gen_be_error_t::ok);
    g_decl_fail_di_sub = 1;
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::di_metadata_failure);
    g_decl_fail_di_sub = 0;

    // Pragmas in func_scope and primary_scope, labels in func_scope
    a_statement dummy_pragma_stmt = {}; dummy_pragma_stmt.kind = stmk_empty;
    a_pragma p_valid = {}, p_no_ent = {}, p_no_txt = {};
    p_valid.entity.ptr = reinterpret_cast<char*>(&dummy_pragma_stmt); p_valid.pragma_text = (char*)"omp barrier"; p_valid.next = &p_no_ent;
    p_no_ent.entity.ptr = nullptr; p_no_ent.pragma_text = (char*)"omp barrier"; p_no_ent.next = &p_no_txt;
    p_no_txt.entity.ptr = reinterpret_cast<char*>(&dummy_pragma_stmt); p_no_txt.pragma_text = nullptr; p_no_txt.next = nullptr;
    body_scope.pragmas = &p_valid;
    root_scope.pragmas = &p_valid;

    a_label scope_lbl = {};
    body_scope.labels = &scope_lbl;

    // Parameters:
    // 1) 3 EDG params on 4-arg LLVM function:
    //    p_byval (ptr -> i32), p_split (struct {i64,i64} -> consumes arg1 AND arg2!), p_exact (i32 -> consumes arg3)
    a_type int_t = {}, struct_t = {}, ptr_t = {};
    int_t.kind = tk_integer;
    struct_t.kind = tk_struct;
    ptr_t.kind = tk_pointer; ptr_t.variant.pointer.type = &cls_ty;

    a_variable p_byval = {}, p_split = {}, p_exact = {}, p_extra = {};
    p_byval.source_corresp.name = "p_byval"; p_byval.type = &int_t; p_byval.next = &p_split;
    p_split.source_corresp.name = nullptr; p_split.type = &struct_t; p_split.next = &p_exact;
    p_exact.source_corresp.name = "p_exact"; p_exact.type = &int_t; p_exact.next = nullptr; // 3 params -> p_split splits across 2 args!
    body_scope.variant.routine.parameters = &p_byval;

    a_variable lv_named = {}, lv_unnamed = {};
    lv_named.source_corresp.name = "lv_named"; lv_named.type = &int_t; lv_named.next = &lv_unnamed;
    lv_unnamed.source_corresp.name = nullptr; lv_unnamed.type = &int_t; lv_unnamed.next = nullptr;
    body_scope.nonstatic_variables = &lv_named;

    // Test param get_llvm_type error, param emit_dbg_declare error, lvar get_llvm_type error, lvar emit_dbg_declare error, pop_lexical_block error
    assert(recreate_test_fn_body(&fn_body, "fn_param_ty_err") == llvm_gen_be_error_t::ok);
    g_decl_fail_type = 1;
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::unsupported_type);
    g_decl_fail_type = 0;

    assert(recreate_test_fn_body(&fn_body, "fn_param_dbg_err") == llvm_gen_be_error_t::ok);
    g_decl_fail_dbg_decl = 1;
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::di_metadata_failure);
    g_decl_fail_dbg_decl = 0;

    assert(recreate_test_fn_body(&fn_body, "fn_lvar_ty_err") == llvm_gen_be_error_t::ok);
    g_decl_fail_type = 4; // after 3 params -> fails on lv_named!
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::unsupported_type);
    g_decl_fail_type = 0;

    assert(recreate_test_fn_body(&fn_body, "fn_lvar_dbg_err") == llvm_gen_be_error_t::ok);
    g_decl_fail_dbg_decl = 4; // after 3 params -> fails on lv_named!
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::di_metadata_failure);
    g_decl_fail_dbg_decl = 0;

    assert(recreate_test_fn_body(&fn_body, "fn_pop_lex_err") == llvm_gen_be_error_t::ok);
    g_decl_fail_pop_lex = 1;
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::di_metadata_failure);
    g_decl_fail_pop_lex = 0;

    // Also test with p_extra appended (so remaining_edg_params == 2 and (func->arg_size() - arg_idx) > remaining_edg_params is false)
    // and with ByVal attribute on arg 0!
    p_exact.next = &p_extra;
    p_extra.source_corresp.name = "p_extra"; p_extra.type = &int_t; p_extra.next = nullptr;
    assert(recreate_test_fn_body(&fn_body, "fn_dbg_ok") == llvm_gen_be_error_t::ok);
    state.module->getFunction("fn_dbg_ok")->getArg(0)->addAttr(llvm::Attribute::getWithByValType(*state.context, llvm::Type::getInt32Ty(*state.context)));
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);
    state.dbg_state = nullptr;
    assert(recreate_test_fn_body(&fn_body, "fn_nodbg_lvars") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);

    // Test sret on func, routine->type == nullptr, routine->type != tk_routine, il_header.primary_scope == nullptr, and aarch64-pc-windows-msvc in llvm_lower_function_body
    body_scope.variant.routine.parameters = nullptr;
    body_scope.nonstatic_variables = nullptr;
    assert(recreate_test_fn_body(&fn_body, "fn_already_sret") == llvm_gen_be_error_t::ok);
    state.module->getFunction("fn_already_sret")->getArg(0)->addAttr(llvm::Attribute::getWithStructRetType(*state.context, llvm::Type::getInt32Ty(*state.context)));
    edg::il_header.primary_scope = nullptr;
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);
    edg::il_header.primary_scope = &root_scope;

    fn_body.type = &int_t;
    assert(recreate_test_fn_body(&fn_body, "fn_non_rout_rtype") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);
    fn_body.type = &fn_ty;

    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    assert(recreate_test_fn_body(&fn_body, "fn_win64_ret_err") == llvm_gen_be_error_t::ok);
    g_decl_fail_ret_info = 1;
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::abi_classification_failed);
    g_decl_fail_ret_info = 0;
    g_decl_sret_ret_info = true;
    assert(recreate_test_fn_body(&fn_body, "fn_win64_sret") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);
    g_decl_sret_ret_info = false;

    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "aarch64-pc-windows-msvc");
    assert(recreate_test_fn_body(&fn_body, "fn_win_arm64_ret") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);

    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");

    // Test constructor_inits in llvm_lower_function_body:
    // 1) this_var == nullptr
    a_constructor_init ci_null_dip = {}, ci_field_null = {}, ci_field_ok = {}, ci_base_null = {}, ci_base_ok = {}, ci_vbase_ok = {}, ci_deleg = {}, ci_unk = {};
    body_scope.variant.routine.constructor_inits = &ci_null_dip;
    assert(recreate_test_fn_body(&fn_body, "fn_ci_no_this") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);

    // 2) this_var not in local_vars (when func has 0 args!) -> internal_inconsistency
    g_decl_proto_mode = 0;
    a_variable this_v = {}; this_v.source_corresp.name = "this"; this_v.type = &ptr_t;
    body_scope.variant.routine.parameters = &this_v;
    assert(recreate_test_fn_body(&fn_body, "fn_ci_no_alloca") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::internal_inconsistency);

    // Also test llvm_emit_ctor_dtor_vptr_init when this_alloca == nullptr (constructor_inits == nullptr, special_kind == sfk_constructor, 0 args!)
    body_scope.variant.routine.constructor_inits = nullptr;
    fn_body.special_kind = sfk_constructor;
    assert(recreate_test_fn_body(&fn_body, "fn_vptr_no_alloca") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);
    fn_body.special_kind = static_cast<a_special_function_kind>(0);
    body_scope.variant.routine.constructor_inits = &ci_null_dip;

    // 3) Full constructor_inits chain with this_v in local_vars (including dip without destructor AND dip with destructor)
    g_decl_proto_mode = 1; // void(ptr)
    a_routine dtor_ci = {}; dtor_ci.source_corresp.name = "_ZN1CD1Ev";
    a_dynamic_init dip_ci = {}, dip_ci_nodtor = {};
    dip_ci.kind = dik_expression;
    an_expr_node ci_expr = {}; dip_ci.variant.expression = &ci_expr;
    dip_ci.destructor = &dtor_ci;
    dip_ci_nodtor.kind = dik_none;
    dip_ci_nodtor.destructor = nullptr;
    a_field ci_f = {}; ci_f.offset = 4; ci_f.type = &int_t;
    a_base_class ci_b = {}; ci_b.offset = 8; ci_b.type = &int_t;

    ci_null_dip.initializer = nullptr; ci_null_dip.next = &ci_field_null;
    ci_field_null.initializer = &dip_ci; ci_field_null.kind = cik_field; ci_field_null.variant.field = nullptr; ci_field_null.next = &ci_field_ok;
    ci_field_ok.initializer = &dip_ci; ci_field_ok.kind = cik_field; ci_field_ok.variant.field = &ci_f; ci_field_ok.next = &ci_base_null;
    ci_base_null.initializer = &dip_ci; ci_base_null.kind = cik_direct_base_class; ci_base_null.variant.base_class = nullptr; ci_base_null.next = &ci_base_ok;
    ci_base_ok.initializer = &dip_ci; ci_base_ok.kind = cik_direct_base_class; ci_base_ok.variant.base_class = &ci_b; ci_base_ok.next = &ci_vbase_ok;
    ci_vbase_ok.initializer = &dip_ci_nodtor; ci_vbase_ok.kind = cik_virtual_base_class; ci_vbase_ok.variant.base_class = &ci_b; ci_vbase_ok.next = &ci_deleg;
    ci_deleg.initializer = &dip_ci; ci_deleg.kind = cik_delegation; ci_deleg.next = &ci_unk;
    ci_unk.initializer = &dip_ci; ci_unk.kind = static_cast<a_constructor_init_kind>(99); ci_unk.next = nullptr;

    // Error on field get_llvm_type, base get_llvm_type, and llvm_emit_dynamic_init
    assert(recreate_test_fn_body(&fn_body, "fn_ci_field_ty_err") == llvm_gen_be_error_t::ok);
    g_decl_fail_type = 2; // 1st this_v, 2nd ci_f
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::unsupported_type);
    g_decl_fail_type = 0;

    assert(recreate_test_fn_body(&fn_body, "fn_ci_base_ty_err") == llvm_gen_be_error_t::ok);
    g_decl_fail_type = 3; // 1st this_v, 2nd ci_f, 3rd ci_b
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::unsupported_type);
    g_decl_fail_type = 0;

    assert(recreate_test_fn_body(&fn_body, "fn_ci_dyn_err") == llvm_gen_be_error_t::ok);
    g_decl_fail_expr = 1;
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::unsupported_expr);
    g_decl_fail_expr = 0;

    // Also test llvm_emit_ctor_dtor_vptr_init:
    // - Secondary bases in Itanium vtable init (virtual direct base, primary base, secondary polymorphic base with & without extra_info, non-polymorphic base)
    a_type sec_poly1 = {}, sec_poly2 = {}, sec_nonpoly = {};
    a_class_type_supplement sec_supp1 = {}; sec_supp1.highest_virtual_function_number = 2;
    sec_poly1.kind = tk_class; sec_poly1.variant.class_struct_union.any_virtual_functions_including_in_base_classes = TRUE; sec_poly1.variant.class_struct_union.extra_info = &sec_supp1;
    sec_poly2.kind = tk_struct; sec_poly2.variant.class_struct_union.any_virtual_functions_including_in_base_classes = TRUE; sec_poly2.variant.class_struct_union.extra_info = nullptr;
    sec_nonpoly.kind = tk_class;

    a_base_class db_virt = {}, db_nonvirt = {};
    db_virt.is_virtual = TRUE; db_virt.next_direct = &db_nonvirt;
    db_nonvirt.is_virtual = FALSE; db_nonvirt.next_direct = nullptr;
    cls_supp.direct_base_classes = &db_virt;

    a_base_class b_prim = {}, b_sec1 = {}, b_sec2 = {}, b_nonpoly = {};
    b_prim.type = &sec_poly1; b_prim.offset = 0; b_prim.next = &b_sec1;
    b_sec1.type = &sec_poly1; b_sec1.offset = 8; b_sec1.next = &b_sec2;
    b_sec2.type = &sec_poly2; b_sec2.offset = 16; b_sec2.next = &b_nonpoly;
    b_nonpoly.type = &sec_nonpoly; b_nonpoly.offset = 24; b_nonpoly.next = nullptr;
    cls_supp.virtual_function_info_base_class = &b_prim;
    cls_supp.base_classes = &b_prim;
    cls_ty.variant.class_struct_union.any_virtual_functions_including_in_base_classes = TRUE;
    cls_ty.variant.class_struct_union.any_virtual_base_classes = TRUE;

    assert(recreate_test_fn_body(&fn_body, "fn_ci_all_ok") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);
    body_scope.variant.routine.constructor_inits = nullptr;

    // Test C2/D2 subobject constructor with VTT parameter (and VTT/vtable error branches)
    fn_body.special_kind = sfk_constructor;
    fn_body.ctor_dtor_kind = cdk_subobject;
    a_variable vtt_p = {}; vtt_p.source_corresp.name = "vtt"; vtt_p.type = &ptr_t;
    this_v.next = &vtt_p;
    g_decl_proto_mode = 3; // 4 args so both this_v and vtt_p get allocas!

    assert(recreate_test_fn_body(&fn_body, "_ZN1CC2Ev_vtt_err") == llvm_gen_be_error_t::ok);
    g_decl_fail_vtt = 1;
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::vtable_generation_failure);
    g_decl_fail_vtt = 0;

    assert(recreate_test_fn_body(&fn_body, "_ZN1CC2Ev_vtt_ok") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);

    // Subobject ctor with vtt_p not in local_vars (1-arg function), and subobject ctor when any_virtual_base_classes == FALSE
    g_decl_proto_mode = 1;
    assert(recreate_test_fn_body(&fn_body, "_ZN1CC2Ev_vtt_no_alloca") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);

    cls_ty.variant.class_struct_union.any_virtual_base_classes = FALSE;
    assert(recreate_test_fn_body(&fn_body, "_ZN1CC2Ev_no_vbase") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);
    cls_ty.variant.class_struct_union.any_virtual_base_classes = TRUE;

    // Itanium ctor with any_virtual_functions == FALSE && any_virtual_base_classes == TRUE (and cls_ty.kind == tk_struct!)
    this_v.next = nullptr;
    fn_body.ctor_dtor_kind = cdk_complete;
    cls_ty.kind = tk_struct;
    cls_ty.variant.class_struct_union.any_virtual_functions_including_in_base_classes = FALSE;
    assert(recreate_test_fn_body(&fn_body, "_ZN1CC1Ev_vbonly_struct") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);
    cls_ty.kind = tk_class;
    cls_ty.variant.class_struct_union.any_virtual_functions_including_in_base_classes = TRUE;

    // Subobject ctor without VTT param (falls through to primary vtable init) + vtable error
    assert(recreate_test_fn_body(&fn_body, "_ZN1CD2Ev_vtbl_err") == llvm_gen_be_error_t::ok);
    g_decl_fail_vtbl = 1;
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::vtable_generation_failure);
    g_decl_fail_vtbl = 0;

    // Class with supp == nullptr in vtable init
    cls_ty.variant.class_struct_union.extra_info = nullptr;
    assert(recreate_test_fn_body(&fn_body, "_ZN1CD2Ev_no_supp") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);
    cls_ty.variant.class_struct_union.extra_info = &cls_supp;

    // Test llvm_emit_ctor_dtor_variants for sfk_destructor ("D2E" -> "D1E" and "D0E", existing D1/D0, 0-arg D2, non-void return C2/D2, primary_ctor_or_dtor != nullptr)
    fn_body.special_kind = sfk_destructor;
    assert(recreate_test_fn_body(&fn_body, "_ZN1CD2Ev_unlowered") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);
    assert(state.module->getFunction("_ZN1CD1Ev_unlowered") != nullptr);
    assert(state.module->getFunction("_ZN1CD0Ev_unlowered") != nullptr);
    // Re-run with existing D1/D0 functions to cover already-exists branches
    assert(recreate_test_fn_body(&fn_body, "_ZN1CD2Ev_unlowered") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);
    // Dtor without "D2E" in name
    assert(recreate_test_fn_body(&fn_body, "_ZN1CD1Ev_complete_only") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);
    // C2 with existing C1 function
    fn_body.special_kind = sfk_constructor;
    assert(recreate_test_fn_body(&fn_body, "_ZN1CC2Ev_vtt_no_alloca") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);
    // C2 and D2 with non-void return type (g_decl_proto_mode == 2 returns i32, 2 args) and 0-arg D2 (g_decl_proto_mode == 0)
    body_scope.variant.routine.parameters = nullptr;
    g_decl_proto_mode = 2;
    assert(recreate_test_fn_body(&fn_body, "_ZN1CC2Ev_ret_i32") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);
    fn_body.special_kind = sfk_destructor;
    assert(recreate_test_fn_body(&fn_body, "_ZN1CD2Ev_ret_i32_2arg") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);
    g_decl_proto_mode = 0;
    assert(recreate_test_fn_body(&fn_body, "_ZN1CD2Ev_void_0arg") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);
    // primary_ctor_or_dtor != nullptr early return
    a_routine dummy_prim = {};
    fn_body.primary_ctor_or_dtor = &dummy_prim;
    assert(recreate_test_fn_body(&fn_body, "_ZN1CD2Ev_has_prim") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);
    fn_body.primary_ctor_or_dtor = nullptr;
    fn_body.special_kind = sfk_constructor;
    g_decl_proto_mode = 1;
    body_scope.variant.routine.parameters = &this_v;

    // MSVC ctor/dtor vptr and vbptr init:
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    fn_body.ctor_dtor_kind = cdk_complete;
    assert(recreate_test_fn_body(&fn_body, "fn_msvc_ctor_vtbl_err") == llvm_gen_be_error_t::ok);
    g_decl_fail_vtbl = 1;
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::vtable_generation_failure);
    g_decl_fail_vtbl = 0;

    assert(recreate_test_fn_body(&fn_body, "fn_msvc_ctor_vbtbl_err") == llvm_gen_be_error_t::ok);
    g_decl_fail_vbtbl = 1;
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::vtable_generation_failure);
    g_decl_fail_vbtbl = 0;

    assert(recreate_test_fn_body(&fn_body, "fn_msvc_ctor_both") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);

    // MSVC ctor with virtual functions ONLY (any_virtual_base_classes == FALSE)
    cls_ty.variant.class_struct_union.any_virtual_base_classes = FALSE;
    assert(recreate_test_fn_body(&fn_body, "fn_msvc_ctor_vfonly") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);
    cls_ty.variant.class_struct_union.any_virtual_base_classes = TRUE;

    // MSVC ctor with virtual bases ONLY (vbptr_off == 0)
    cls_ty.variant.class_struct_union.any_virtual_functions_including_in_base_classes = FALSE;
    assert(recreate_test_fn_body(&fn_body, "fn_msvc_ctor_vbonly") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);
    cls_ty.variant.class_struct_union.any_virtual_functions_including_in_base_classes = TRUE;

    // MSVC deleting dtor (is_dtor == true -> skips vbtable init)
    fn_body.special_kind = sfk_destructor;
    fn_body.ctor_dtor_kind = cdk_deleting;
    assert(recreate_test_fn_body(&fn_body, "fn_msvc_dtor") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);

    // Early exits in llvm_emit_ctor_dtor_vptr_init:
    // this_var->type == nullptr, non-pointer this_var->type, pointer to null, pointer to non-class, non-polymorphic class
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
    this_v.type = nullptr;
    assert(recreate_test_fn_body(&fn_body, "fn_vptr_null_ty") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);

    this_v.type = &int_t;
    assert(recreate_test_fn_body(&fn_body, "fn_vptr_nonptr_ty") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);

    ptr_t.variant.pointer.type = nullptr;
    this_v.type = &ptr_t;
    assert(recreate_test_fn_body(&fn_body, "fn_vptr_null_ptee") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);

    ptr_t.variant.pointer.type = &int_t;
    assert(recreate_test_fn_body(&fn_body, "fn_vptr_int_ptee") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);

    ptr_t.variant.pointer.type = &sec_nonpoly;
    assert(recreate_test_fn_body(&fn_body, "fn_vptr_nonpoly") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);

    fn_body.special_kind = static_cast<a_special_function_kind>(0);
    fn_body.ctor_dtor_kind = static_cast<a_ctor_or_dtor_kind>(0);
    body_scope.variant.routine.parameters = nullptr;

    // Exception specification on routine (exception_specification == nullptr, throw_any == TRUE, throw_any == FALSE) + assoc_block + escaped_allocas + non-void unreachable
    an_exception_specification exc_spec = {};
    r_supp.exception_specification = nullptr;
    fn_ty.variant.routine.extra_info = &r_supp;
    assert(recreate_test_fn_body(&fn_body, "fn_no_exc_spec") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);

    r_supp.exception_specification = &exc_spec;
    exc_spec.throw_any = TRUE;
    a_statement body_stmt = {}; body_stmt.kind = stmk_empty;
    body_scope.assoc_block = &body_stmt;

    // escaped_allocas when entry_bb has NO terminator yet
    llvm::Value* esc_dummy = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*state.context));
    state.escaped_allocas.push_back(esc_dummy);
    assert(recreate_test_fn_body(&fn_body, "fn_exc_any_esc_noterm") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);

    // escaped_allocas when entry_bb ALREADY has a terminator (via stmk_return), plus throw_any == FALSE and non-void return type (CreateUnreachable on lpad block!)
    exc_spec.throw_any = FALSE;
    body_stmt.kind = stmk_return;
    body_stmt.expr = nullptr;
    g_decl_proto_mode = 2; // returns i32!
    state.escaped_allocas.push_back(esc_dummy);
    assert(recreate_test_fn_body(&fn_body, "fn_exc_spec_esc_term") == llvm_gen_be_error_t::ok);
    // Note: stmk_return with expr == nullptr emits ret void in a function returning i32 -> triggers verifyFunction failure!
    // Let's also test with dbg_state active so verifyFunction failure path with dbg_state is covered!
    state.dbg_state = &dbg_st;
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::verification_failure);
    state.module->getFunction("fn_exc_spec_esc_term")->eraseFromParent();
    state.dbg_state = nullptr;

    // Now with valid return expression and empty body (hits CreateUnreachable for non-void function without terminator!)
    body_stmt.kind = stmk_empty;
    assert(recreate_test_fn_body(&fn_body, "fn_exc_spec_ok") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::ok);

    // assoc_block error
    body_stmt.kind = static_cast<a_statement_kind>(999);
    assert(recreate_test_fn_body(&fn_body, "fn_assoc_blk_err") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&fn_body) == llvm_gen_be_error_t::unsupported_stmt);
    state.module->getFunction("fn_assoc_blk_err")->eraseFromParent();
    state.current_landing_pads.clear();
    r_supp.exception_specification = nullptr;
    fn_ty.variant.routine.extra_info = nullptr;
    body_scope.assoc_block = nullptr;
    g_decl_proto_mode = 0;

    // Error propagation in lower_routine_defs_in_scope (namespace & class, including ns_alias and ns_empty!)
    g_decl_scope_for_rout = &body_scope;
    body_scope.assoc_block = &body_stmt; // invalid stmt kind 999
    root_scope.routines = nullptr;
    root_scope.namespaces = &ns_alias;
    ns_rout.function_def_number = 1;
    assert(llvm_lower_function_definitions() == llvm_gen_be_error_t::unsupported_stmt);
    ns_rout.function_def_number = 0;
    cls_method.function_def_number = 1;
    assert(llvm_lower_function_definitions() == llvm_gen_be_error_t::unsupported_stmt);
    cls_method.function_def_number = 0;
    body_scope.assoc_block = nullptr;
    assert(llvm_lower_function_definitions() == llvm_gen_be_error_t::ok);

    // F. llvm_lower_global_ctors_and_dtors:
    //    - primary_scope == nullptr
    //    - unnamed routine, routine not in module, ctor-only routine, dtor-only routine, non-ctor/dtor routine, and appending to existing llvm.used on MSVC
    edg::il_header.primary_scope = nullptr;
    assert(llvm_lower_global_ctors_and_dtors() == llvm_gen_be_error_t::ok);
    edg::il_header.primary_scope = &root_scope;

    a_routine r_ctor_only = {}, r_dtor_only = {}, r_sti_only = {}, r_std_only = {}, r_missing_fn = {}, r_neither = {};
    llvm::Function* sti_mock_fn = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false),
        llvm::GlobalValue::InternalLinkage, "__sti___mock", state.module.get());
    llvm::BasicBlock::Create(*state.context, "entry", sti_mock_fn);
    llvm::IRBuilder<>( &sti_mock_fn->getEntryBlock() ).CreateRetVoid();
    llvm::Function* std_mock_fn = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false),
        llvm::GlobalValue::InternalLinkage, "__std___mock", state.module.get());
    llvm::BasicBlock::Create(*state.context, "entry", std_mock_fn);
    llvm::IRBuilder<>( &std_mock_fn->getEntryBlock() ).CreateRetVoid();
    r_ctor_only.source_corresp.name = "my_func";
    r_ctor_only.is_initialization_routine = TRUE;
    r_ctor_only.is_finalization_routine = FALSE;
    r_ctor_only.next = &r_dtor_only;
    r_dtor_only.source_corresp.name = "my_func";
    r_dtor_only.is_initialization_routine = FALSE;
    r_dtor_only.is_finalization_routine = TRUE;
    r_dtor_only.next = &r_sti_only;
    r_sti_only.source_corresp.name = "__sti___mock";
    r_sti_only.next = &r_std_only;
    r_std_only.source_corresp.name = "__std___mock";
    r_std_only.next = &unnamed_rout;
    unnamed_rout.next = &r_missing_fn;
    r_missing_fn.source_corresp.name = "missing_ctor_fn";
    r_missing_fn.is_initialization_routine = TRUE;
    r_missing_fn.next = &r_neither;
    r_neither.source_corresp.name = "my_func";
    r_neither.next = nullptr;
    root_scope.routines = &r_ctor_only;

    // Test on MSVC (appends to existing llvm.used created in section 5!)
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    assert(llvm_lower_global_ctors_and_dtors() == llvm_gen_be_error_t::ok);

    // Test on MSVC with no ctors/dtors (used_vars.empty() == true) and on ELF with no ctors/dtors
    root_scope.routines = &r_neither;
    assert(llvm_lower_global_ctors_and_dtors() == llvm_gen_be_error_t::ok);
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
    assert(llvm_lower_global_ctors_and_dtors() == llvm_gen_be_error_t::ok);
    unnamed_rout.next = nullptr;
    r_missing_fn.next = nullptr;

    // G. llvm_lower_module_metadata:
    //    - imported_modules: module_info == nullptr, mk_unit_partition, mk_header_unit (with null resolved_file), unknown kind, empty name
    //    - trans_unit_module: nullptr, mk_unit (with null resolved_file), unknown kind, empty name
    //    - primary_scope == nullptr vs != nullptr
    //    - exported_syms: static/internal/unnamed vs exported routines & variables
    //    - existing guard_gv & existing non-empty mod_init_fn
    //    - initialization routines in primary_scope: unnamed init routine, r_fn == mod_init_fn, r_fn->arg_size() > 0, r_fn->arg_size() == 0
    a_module imp_part = {}, imp_hdr = {}, imp_unk = {}, imp_empty = {};
    imp_part.kind = mk_unit_partition; imp_part.variant.unit_partition.name = "mod:sub"; imp_part.resolved_file = nullptr;
    imp_hdr.kind = mk_header_unit; imp_hdr.variant.header_unit.name = "<vector>"; imp_hdr.resolved_file = nullptr;
    imp_unk.kind = static_cast<a_module_kind>(99);
    imp_empty.kind = mk_unit; imp_empty.variant.unit.name = "";

    a_module_import_decl id_null = {}, id_part = {}, id_hdr = {}, id_unk = {}, id_empty = {};
    id_null.module_info = nullptr; id_null.next = &id_part;
    id_part.module_info = &imp_part; id_part.next = &id_hdr;
    id_hdr.module_info = &imp_hdr; id_hdr.next = &id_unk;
    id_unk.module_info = &imp_unk; id_unk.next = &id_empty;
    id_empty.module_info = &imp_empty; id_empty.next = nullptr;
    edg::il_header.imported_modules = &id_null;

    // trans_unit_module == nullptr
    edg::trans_unit_module = nullptr;
    assert(llvm_lower_module_metadata() == llvm_gen_be_error_t::ok);

    // trans_unit_module with unknown kind & empty name
    edg::trans_unit_module = &imp_unk;
    assert(llvm_lower_module_metadata() == llvm_gen_be_error_t::ok);

    // trans_unit_module with primary_scope == nullptr
    a_module no_scope_mod = {};
    no_scope_mod.kind = mk_unit;
    no_scope_mod.variant.unit.name = "noscope.mod";
    edg::trans_unit_module = &no_scope_mod;
    edg::il_header.primary_scope = nullptr;
    assert(llvm_lower_module_metadata() == llvm_gen_be_error_t::ok);
    edg::il_header.primary_scope = &root_scope;

    // trans_unit_module with mk_unit, null resolved_file, exported & non-exported variables/routines, and init routines
    a_module main_mod = {};
    main_mod.kind = mk_unit;
    main_mod.variant.unit.name = "my.mod";
    main_mod.resolved_file = nullptr;
    edg::trans_unit_module = &main_mod;

    a_variable v_exp = {}, v_stat = {}, v_int = {};
    v_exp.source_corresp.name = "exp_var"; v_exp.storage_class = sc_unspecified; v_exp.next = &v_stat;
    v_stat.source_corresp.name = "stat_var"; v_stat.storage_class = sc_static; v_stat.next = &v_int;
    v_int.source_corresp.name = "int_var"; v_int.source_corresp.name_linkage = nlk_internal; v_int.next = &unnamed_var;
    unnamed_var.next = nullptr;
    root_scope.variables = &v_exp;

    a_routine r_exp = {}, r_stat = {}, r_int = {}, r_unnamed_init = {}, r_self = {}, r_with_arg = {};
    r_exp.source_corresp.name = "my_func"; // 0-arg init routine in module!
    r_exp.is_initialization_routine = TRUE;
    r_exp.next = &r_stat;
    r_stat.source_corresp.name = "stat_fn"; r_stat.storage_class = sc_static; r_stat.next = &r_int;
    r_int.source_corresp.name = "int_fn"; r_int.source_corresp.name_linkage = nlk_internal; r_int.next = &r_unnamed_init;
    r_unnamed_init.source_corresp.name = nullptr; r_unnamed_init.is_initialization_routine = TRUE; r_unnamed_init.next = &r_self;
    r_self.source_corresp.name = "__module_init_my_mod"; r_self.is_initialization_routine = TRUE; r_self.next = &r_with_arg;
    r_with_arg.source_corresp.name = "_ZN1CD2Ev"; // 1-arg function in module!
    r_with_arg.is_initialization_routine = TRUE; r_with_arg.next = &r_missing_fn;
    root_scope.routines = &r_exp;

    assert(llvm_lower_module_metadata() == llvm_gen_be_error_t::ok);
    // Call a second time to hit existing guard_gv, existing mod_init_fn, and !mod_init_fn->empty()!
    assert(llvm_lower_module_metadata() == llvm_gen_be_error_t::ok);

    // H. Phase 4 & 6 coverage:
    //    - MSVC vtordisp & covariant return thunks in llvm_lower_thunk_body
    //    - Sub-VTT passing in llvm_lower_constructor_inits (normal Call, Invoke with lpad, and dip->destructor != nullptr)
    //    - Synthesized global dynamic initializers (__cxx_global_var_init.<var>, inline COMDAT guard, __cxa_atexit, MSVC atexit)
    //    - Synthesized thread_local dynamic initializers (__tls_init, _ZTW<var>, __cxa_thread_atexit, MSVC _register_thread_local_exe_atexit_callback)
    state.cleanup_stack.cleanups.clear();
    state.current_landing_pads.clear();
    state.landing_pad_cleanup_depths.clear();
    state.active_funclet_pads.clear();
    a_type local_int_ty = {};
    local_int_ty.kind = tk_integer;
    local_int_ty.size = 4;

    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    g_decl_proto_mode = 4; // ptr(ptr, i32)
    a_routine msvc_thunk = {};
    msvc_thunk.source_corresp.name = "msvc_vtordisp_cov_thunk";
    msvc_thunk.type = &fn_ty;
    msvc_thunk.overriding_function_for_wrapper = &ovr_cov;
    msvc_thunk.vcall_index = 1;
    msvc_thunk.delta = 4;
    msvc_thunk.vbase_index = 1;
    msvc_thunk.return_delta = 8;
    assert(llvm_lower_function_prototype(&msvc_thunk) == llvm_gen_be_error_t::ok);
    root_scope.routines = &msvc_thunk;
    root_scope.namespaces = nullptr;
    root_scope.types = nullptr;
    assert(llvm_lower_function_definitions() == llvm_gen_be_error_t::ok);

    // Sub-VTT passing in llvm_lower_constructor_inits on Itanium
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
    a_type vbase_cls = {};
    vbase_cls.kind = tk_class;
    vbase_cls.variant.class_struct_union.any_virtual_base_classes = TRUE;
    a_type ptr_vbase_cls = {};
    ptr_vbase_cls.kind = tk_pointer;
    ptr_vbase_cls.variant.pointer.type = &vbase_cls;

    a_routine base_c2_rout = {}, base_c2_dtor = {};
    base_c2_rout.source_corresp.name = "_ZN4BaseC2EPPv";
    base_c2_rout.type = &fn_ty;
    base_c2_dtor.source_corresp.name = "_ZN4BaseD2Ev";
    g_decl_proto_mode = 2; // 2 args
    assert(llvm_lower_function_prototype(&base_c2_rout) == llvm_gen_be_error_t::ok);
    llvm::Function* base_c2_llvm = state.module->getFunction("_ZN4BaseC2EPPv");
    // Adjust 2nd arg to ptr so {target_ptr, sub_vtt_ptr} matches signature
    base_c2_llvm->eraseFromParent();
    llvm::Type* ptr_llvm_ty = llvm::PointerType::getUnqual(*state.context);
    llvm::FunctionType* c2_fty = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {ptr_llvm_ty, ptr_llvm_ty}, false);
    base_c2_llvm = llvm::Function::Create(c2_fty, llvm::GlobalValue::ExternalLinkage, "_ZN4BaseC2EPPv", state.module.get());

    a_base_class sub_vtt_base = {};
    sub_vtt_base.type = &vbase_cls;
    sub_vtt_base.offset = 8;

    a_dynamic_init sub_vtt_dip1 = {}, sub_vtt_dip2 = {}, sub_vtt_dip_null_ptr = {}, sub_vtt_dip_unnamed = {}, sub_vtt_dip_1arg = {}, sub_vtt_dip_has_args = {};
    sub_vtt_dip1.kind = dik_constructor;
    sub_vtt_dip1.variant.constructor.ptr = &base_c2_rout;
    sub_vtt_dip1.destructor = &base_c2_dtor;
    sub_vtt_dip2.kind = dik_constructor;
    sub_vtt_dip2.variant.constructor.ptr = &base_c2_rout;
    sub_vtt_dip2.destructor = nullptr;

    a_routine r_unnamed_c2 = {}, r_1arg_c2 = {};
    r_unnamed_c2.source_corresp.name = nullptr;
    r_1arg_c2.source_corresp.name = "_ZN4BaseC2E_1arg";
    llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), {ptr_llvm_ty}, false),
                           llvm::GlobalValue::ExternalLinkage, "_ZN4BaseC2E_1arg", state.module.get());
    an_expr_node dummy_c2_arg = {}; dummy_c2_arg.kind = enk_constant; dummy_c2_arg.type = &local_int_ty;
    sub_vtt_dip_null_ptr.kind = dik_constructor; sub_vtt_dip_null_ptr.variant.constructor.ptr = nullptr;
    sub_vtt_dip_unnamed.kind = dik_constructor; sub_vtt_dip_unnamed.variant.constructor.ptr = &r_unnamed_c2;
    sub_vtt_dip_1arg.kind = dik_constructor; sub_vtt_dip_1arg.variant.constructor.ptr = &r_1arg_c2;
    sub_vtt_dip_has_args.kind = dik_constructor; sub_vtt_dip_has_args.variant.constructor.ptr = &r_unnamed_c2; sub_vtt_dip_has_args.variant.constructor.args = &dummy_c2_arg;

    a_constructor_init ci_vtt1 = {}, ci_vtt2 = {}, ci_vtt3 = {}, ci_vtt4 = {}, ci_vtt5 = {}, ci_vtt6 = {};
    ci_vtt1.kind = cik_direct_base_class; ci_vtt1.variant.base_class = &sub_vtt_base; ci_vtt1.initializer = &sub_vtt_dip1; ci_vtt1.next = &ci_vtt2;
    ci_vtt2.kind = cik_virtual_base_class; ci_vtt2.variant.base_class = &sub_vtt_base; ci_vtt2.initializer = &sub_vtt_dip2; ci_vtt2.next = &ci_vtt3;
    ci_vtt3.kind = cik_direct_base_class; ci_vtt3.variant.base_class = &sub_vtt_base; ci_vtt3.initializer = &sub_vtt_dip_null_ptr; ci_vtt3.next = &ci_vtt4;
    ci_vtt4.kind = cik_direct_base_class; ci_vtt4.variant.base_class = &sub_vtt_base; ci_vtt4.initializer = &sub_vtt_dip_unnamed; ci_vtt4.next = &ci_vtt5;
    ci_vtt5.kind = cik_direct_base_class; ci_vtt5.variant.base_class = &sub_vtt_base; ci_vtt5.initializer = &sub_vtt_dip_1arg; ci_vtt5.next = &ci_vtt6;
    ci_vtt6.kind = cik_direct_base_class; ci_vtt6.variant.base_class = &sub_vtt_base; ci_vtt6.initializer = &sub_vtt_dip_has_args; ci_vtt6.next = nullptr;

    a_variable c1_this_param = {};
    c1_this_param.source_corresp.name = "this";
    c1_this_param.type = &ptr_vbase_cls;

    a_scope c1_scope = {};
    c1_scope.variant.routine.parameters = &c1_this_param;
    c1_scope.variant.routine.constructor_inits = &ci_vtt1;
    g_decl_scope_for_rout = &c1_scope;

    a_routine der_c1_rout = {};
    der_c1_rout.source_corresp.name = "_ZN3DerC1Ev";
    der_c1_rout.type = &fn_ty;
    der_c1_rout.function_def_number = 1;
    der_c1_rout.special_kind = sfk_constructor;
    der_c1_rout.ctor_dtor_kind = cdk_complete;
    g_decl_proto_mode = 1; // void(ptr)
    assert(llvm_lower_function_prototype(&der_c1_rout) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&der_c1_rout) == llvm_gen_be_error_t::ok);

    // Also test when base_c2_llvm is nounwind, and when routine->ctor_dtor_kind == cdk_subobject, and when this_var->type == nullptr or non-pointer or pointer-to-null
    base_c2_llvm->setDoesNotThrow();
    a_routine der_c1_nw = der_c1_rout;
    der_c1_nw.source_corresp.name = "_ZN3DerC1Ev_nw";
    assert(llvm_lower_function_prototype(&der_c1_nw) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&der_c1_nw) == llvm_gen_be_error_t::ok);

    c1_scope.variant.routine.constructor_inits = &ci_vtt3;
    a_routine der_c2_sub = der_c1_rout;
    der_c2_sub.source_corresp.name = "_ZN3DerC2Ev_sub";
    der_c2_sub.ctor_dtor_kind = cdk_subobject;
    assert(llvm_lower_function_prototype(&der_c2_sub) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&der_c2_sub) == llvm_gen_be_error_t::ok);

    c1_this_param.type = nullptr;
    der_c1_nw.source_corresp.name = "_ZN3DerC1Ev_noty";
    assert(llvm_lower_function_prototype(&der_c1_nw) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&der_c1_nw) == llvm_gen_be_error_t::ok);
    c1_this_param.type = &local_int_ty;
    der_c1_nw.source_corresp.name = "_ZN3DerC1Ev_intty";
    assert(llvm_lower_function_prototype(&der_c1_nw) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&der_c1_nw) == llvm_gen_be_error_t::ok);
    a_type ptr_null_ptee = {}; ptr_null_ptee.kind = tk_pointer; ptr_null_ptee.variant.pointer.type = nullptr;
    c1_this_param.type = &ptr_null_ptee;
    der_c1_nw.source_corresp.name = "_ZN3DerC1Ev_nullptee";
    assert(llvm_lower_function_prototype(&der_c1_nw) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&der_c1_nw) == llvm_gen_be_error_t::ok);
    c1_this_param.type = &ptr_vbase_cls;
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    der_c1_nw.source_corresp.name = "_ZN3DerC1Ev_msvc";
    assert(llvm_lower_function_prototype(&der_c1_nw) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&der_c1_nw) == llvm_gen_be_error_t::ok);
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");

    // Global & Thread-Local Dynamic Initialization synthesis (Itanium & MSVC)
    state.global_dynamic_inits.clear();
    if (llvm::Function* old_tls = state.module->getFunction("__tls_init")) old_tls->eraseFromParent();
    root_scope.routines = nullptr; // no pre-lowered __sti__ routine!
    an_expr_node dyn_expr_node = {};
    dyn_expr_node.kind = enk_constant;
    dyn_expr_node.type = &local_int_ty;

    a_routine g_dtor_rout = {}, g_dtor_unnamed = {};
    g_dtor_rout.source_corresp.name = "_ZN3ObjD1Ev";
    g_dtor_unnamed.source_corresp.name = nullptr;

    a_dynamic_init g_dip_with_dtor = {}, g_dip_unnamed_dtor = {}, g_dip_no_dtor = {};
    g_dip_with_dtor.kind = dik_expression;
    g_dip_with_dtor.variant.expression = &dyn_expr_node;
    g_dip_with_dtor.destructor = &g_dtor_rout;
    g_dip_unnamed_dtor.kind = dik_constant;
    g_dip_unnamed_dtor.follows_an_exec_statement = FALSE;
    g_dip_unnamed_dtor.destructor = &g_dtor_unnamed;
    g_dip_no_dtor.kind = dik_expression;
    g_dip_no_dtor.variant.expression = &dyn_expr_node;
    g_dip_no_dtor.destructor = nullptr;

    a_variable gv_inline_mangled = {}, gv_inline_plain = {}, gv_normal = {}, gv_tls_mangled = {}, gv_tls_plain = {}, gv_missing = {};
    gv_inline_mangled.source_corresp.name = "_Z10inline_var";
    gv_inline_mangled.type = &local_int_ty;
    gv_inline_mangled.is_inline = TRUE;

    gv_inline_plain.source_corresp.name = "plain_inline_var";
    gv_inline_plain.type = &local_int_ty;
    gv_inline_plain.is_inline = TRUE;

    gv_normal.source_corresp.name = "normal_dyn_var";
    gv_normal.type = &local_int_ty;

    gv_tls_mangled.source_corresp.name = "_Z7tls_var";
    gv_tls_mangled.type = &local_int_ty;
    gv_tls_mangled.is_thread_local = TRUE;

    gv_tls_plain.source_corresp.name = "plain_tls_var";
    gv_tls_plain.type = &local_int_ty;
    gv_tls_plain.is_thread_local = TRUE;

    gv_missing.source_corresp.name = "uncreated_gv";
    gv_missing.type = &local_int_ty;

    an_initializer dyn_init_u = {};
    dyn_init_u.dynamic = &g_dip_with_dtor;
    g_decl_init_kind = initk_dynamic;
    g_decl_init_ptr = &dyn_init_u;

    // Remove __dso_handle so get_or_create_dso_handle creates it fresh
    if (llvm::GlobalVariable* old_dso = state.module->getGlobalVariable("__dso_handle")) {
      old_dso->eraseFromParent();
    }
    assert(llvm_lower_global_variable(&gv_inline_mangled) == llvm_gen_be_error_t::ok);
    dyn_init_u.dynamic = &g_dip_unnamed_dtor;
    assert(llvm_lower_global_variable(&gv_inline_plain) == llvm_gen_be_error_t::ok);
    dyn_init_u.dynamic = &g_dip_no_dtor;
    assert(llvm_lower_global_variable(&gv_normal) == llvm_gen_be_error_t::ok);
    dyn_init_u.dynamic = &g_dip_with_dtor;
    assert(llvm_lower_global_variable(&gv_tls_mangled) == llvm_gen_be_error_t::ok);
    dyn_init_u.dynamic = &g_dip_unnamed_dtor;
    assert(llvm_lower_global_variable(&gv_tls_plain) == llvm_gen_be_error_t::ok);
    // Also add a TLS variable with no destructor and a pre-existing _ZTW wrapper
    a_variable gv_tls_nodtor = {};
    gv_tls_nodtor.source_corresp.name = "tls_nodtor_var";
    gv_tls_nodtor.type = &local_int_ty;
    gv_tls_nodtor.is_thread_local = TRUE;
    dyn_init_u.dynamic = &g_dip_no_dtor;
    assert(llvm_lower_global_variable(&gv_tls_nodtor) == llvm_gen_be_error_t::ok);
    llvm::Function::Create(llvm::FunctionType::get(ptr_llvm_ty, false),
                           llvm::GlobalValue::WeakODRLinkage, "_ZTW14tls_nodtor_var", state.module.get());
    // Add an entry whose GlobalVariable does not exist in module, and one where init_fn already exists & is non-empty
    state.global_dynamic_inits.push_back({&gv_missing, &g_dip_no_dtor});
    // Pre-remove comdat from gv_inline_plain to cover comdat == nullptr branch
    if (llvm::GlobalVariable* g_plain = state.module->getNamedGlobal("plain_inline_var")) {
      g_plain->setComdat(nullptr);
    }

    assert(llvm_lower_global_ctors_and_dtors() == llvm_gen_be_error_t::ok);
    assert(state.module->getFunction("_ZTW7tls_var") != nullptr);
    assert(state.module->getFunction("_ZTW13plain_tls_var") != nullptr);

    // Re-add entries when __cxx_global_var_init._Z10inline_var and _ZGV10inline_var and __tls_init already exist!
    state.global_dynamic_inits.push_back({&gv_inline_mangled, &g_dip_with_dtor});
    state.global_dynamic_inits.push_back({&gv_tls_mangled, &g_dip_with_dtor});
    // Also add a new variable where __cxx_global_var_init already exists as an empty declaration and _ZGV already exists
    a_variable gv_inline_existing = {};
    gv_inline_existing.source_corresp.name = "_Z12existing_inl";
    gv_inline_existing.type = &local_int_ty;
    gv_inline_existing.is_inline = TRUE;
    assert(llvm_lower_global_variable(&gv_inline_existing) == llvm_gen_be_error_t::ok);
    llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false),
                           llvm::GlobalValue::InternalLinkage, "__cxx_global_var_init._Z12existing_inl", state.module.get());
    llvm::GlobalVariable* zgv_inl = llvm::cast<llvm::GlobalVariable>(
        state.module->getOrInsertGlobal("_ZGV12existing_inl", llvm::Type::getInt64Ty(*state.context)));
    zgv_inl->setConstant(false);
    zgv_inl->setLinkage(llvm::GlobalValue::LinkOnceODRLinkage);
    zgv_inl->setInitializer(llvm::ConstantInt::get(llvm::Type::getInt64Ty(*state.context), 0));
    assert(zgv_inl != nullptr);
    assert(llvm_lower_global_ctors_and_dtors() == llvm_gen_be_error_t::ok);

    // Test on MSVC target (synthesizing atexit stub and _register_thread_local_exe_atexit_callback)
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    if (llvm::Function* old_tls = state.module->getFunction("__tls_init")) old_tls->eraseFromParent();
    a_variable gv_ms_dyn = {}, gv_ms_dyn2 = {}, gv_ms_tls = {};
    gv_ms_dyn.source_corresp.name = "ms_dyn_var";
    gv_ms_dyn.type = &local_int_ty;
    gv_ms_dyn2.source_corresp.name = "ms_dyn_var2";
    gv_ms_dyn2.type = &local_int_ty;
    gv_ms_tls.source_corresp.name = "ms_tls_var";
    gv_ms_tls.type = &local_int_ty;
    gv_ms_tls.is_thread_local = TRUE;
    dyn_init_u.dynamic = &g_dip_with_dtor;
    assert(llvm_lower_global_variable(&gv_ms_dyn) == llvm_gen_be_error_t::ok);
    // Pre-create __dtor_ms_dyn_var2 to cover existing atexit_stub branch
    llvm::Function* existing_stub = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false),
        llvm::GlobalValue::InternalLinkage, "__dtor_ms_dyn_var2", state.module.get());
    llvm::BasicBlock* stub_b = llvm::BasicBlock::Create(*state.context, "entry", existing_stub);
    llvm::IRBuilder<>(stub_b).CreateRetVoid();
    assert(llvm_lower_global_variable(&gv_ms_dyn2) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_global_variable(&gv_ms_tls) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_global_ctors_and_dtors() == llvm_gen_be_error_t::ok);

    // Error propagation in synthesize_tls_dynamic_init and global var init in llvm_lower_global_ctors_and_dtors
    if (llvm::Function* old_tls = state.module->getFunction("__tls_init")) old_tls->eraseFromParent();
    g_decl_fail_expr = 1;
    state.global_dynamic_inits.push_back({&gv_ms_tls, &g_dip_with_dtor});
    assert(llvm_lower_global_ctors_and_dtors() == llvm_gen_be_error_t::unsupported_expr);
    state.global_dynamic_inits.clear();

    a_variable gv_err_var = {};
    gv_err_var.source_corresp.name = "err_dyn_var";
    gv_err_var.type = &local_int_ty;
    g_decl_fail_expr = 0;
    assert(llvm_lower_global_variable(&gv_err_var) == llvm_gen_be_error_t::ok);
    g_decl_fail_expr = 1;
    assert(llvm_lower_global_ctors_and_dtors() == llvm_gen_be_error_t::unsupported_expr);
    g_decl_fail_expr = 0;
    state.global_dynamic_inits.clear();

    // Test emit_dbg_global_variable when be_state->dbg_state != nullptr (ok and error)
    llvm_gen_be_debug_state_t mock_dbg_st = {};
    state.dbg_state = &mock_dbg_st;
    gv_err_var.source_corresp.name = "dbg_gv_ok";
    assert(llvm_lower_global_variable(&gv_err_var) == llvm_gen_be_error_t::ok);
    g_decl_fail_dbg_global = 1;
    gv_err_var.source_corresp.name = "dbg_gv_fail";
    assert(llvm_lower_global_variable(&gv_err_var) == llvm_gen_be_error_t::di_metadata_failure);
    g_decl_fail_dbg_global = 0;
    state.dbg_state = nullptr;

    // Test Phase 8 function attributes, calling conventions, and noexcept in llvm_lower_function_prototype
    an_attribute fn_attrs[7] = {};
    an_attribute_kind fn_attr_kinds[7] = {
        ak_const, ak_pure, ak_noreturn, ak_nothrow,
        ak_warn_unused_result, ak_nodiscard, ak_alloc_size
    };
    for (int i = 0; i < 7; ++i) {
      fn_attrs[i].kind = fn_attr_kinds[i];
      fn_attrs[i].next = (i + 1 < 7) ? &fn_attrs[i + 1] : nullptr;
    }
    const char* ext_attr_tokens[8] = {
        "returns_twice", "alloc_align", "regcall", "preserve_most",
        "preserve_all", "aarch64_vector_pcs", "aarch64_sve_pcs", "other_unrecognized"
    };
    an_attribute ext_attrs[8] = {};
    an_attribute_arg ext_args[8] = {};
    for (int i = 0; i < 8; ++i) {
      ext_args[i].kind = aak_token;
      ext_args[i].variant.token = ext_attr_tokens[i];
      ext_attrs[i].kind = ak_unrecognized;
      ext_attrs[i].arguments = &ext_args[i];
      ext_attrs[i].next = (i + 1 < 8) ? &ext_attrs[i + 1] : nullptr;
    }
    fn_attrs[6].next = &ext_attrs[0];
    // Also include a non-function attribute on a GlobalVariable to hit the dyn_cast<Function>(global) == nullptr branch
    gv_err_var.source_corresp.name = "gv_with_fn_attrs";
    gv_err_var.source_corresp.attributes = &fn_attrs[0];
    assert(llvm_lower_global_variable(&gv_err_var) == llvm_gen_be_error_t::ok);
    gv_err_var.source_corresp.attributes = nullptr;

    an_exception_specification noexcept_spec = {};
    noexcept_spec.is_noexcept = TRUE;
    a_routine_type_supplement p8_supp = {};
    p8_supp.exception_specification = &noexcept_spec;
    fn_ty.variant.routine.extra_info = &p8_supp;

    a_routine p8_rout = {};
    p8_rout.source_corresp.name = "p8_attr_func";
    p8_rout.source_corresp.attributes = &fn_attrs[0];
    p8_rout.type = &fn_ty;
    p8_rout.is_pure = TRUE;
    p8_rout.has_nodiscard_attribute = TRUE;
    g_decl_proto_mode = 1; // 1 arg for alloc_size(0)
    assert(llvm_lower_function_prototype(&p8_rout) == llvm_gen_be_error_t::ok);
    // Also test when exception_specification is non-null but is_noexcept == FALSE, and when attr->kind on Function is none of the above
    noexcept_spec.is_noexcept = FALSE;
    an_attribute attr_unhandled_fn = {};
    attr_unhandled_fn.kind = ak_unused;
    p8_rout.source_corresp.name = "p8_attr_func2";
    p8_rout.source_corresp.attributes = &attr_unhandled_fn;
    assert(llvm_lower_function_prototype(&p8_rout) == llvm_gen_be_error_t::ok);
    fn_ty.variant.routine.extra_info = nullptr;
    g_decl_proto_mode = 0;

    // Test Phases 14 & 15:
    // 1. llvm_get_mangled_variable_name via llvm_lower_global_variable
    a_variable m_var = {};
    m_var.type = &local_int_ty;
    a_scope file_sc = {};
    file_sc.kind = sck_file;
    m_var.source_corresp.name = "";
    m_var.source_corresp.parent_scope = &file_sc;
    assert(llvm_lower_global_variable(&m_var) == llvm_gen_be_error_t::ok);
    assert(state.module->getNamedGlobal("anon_var") != nullptr);

    state.local_vars.erase(&m_var);
    m_var.source_corresp.name = "plain_v";
    m_var.source_corresp.parent_scope = &file_sc;
    assert(llvm_lower_global_variable(&m_var) == llvm_gen_be_error_t::ok);
    assert(state.module->getNamedGlobal("plain_v") != nullptr);

    a_namespace ns_outer = {};
    ns_outer.source_corresp.name = "outer";
    ns_outer.source_corresp.parent_scope = &file_sc;
    a_scope sc_ns_outer = {};
    sc_ns_outer.kind = sck_namespace;
    sc_ns_outer.variant.assoc_namespace = &ns_outer;

    a_namespace ns_anon = {};
    ns_anon.source_corresp.name = "";
    ns_anon.source_corresp.parent_scope = &sc_ns_outer;
    a_scope sc_ns_anon = {};
    sc_ns_anon.kind = sck_namespace;
    sc_ns_anon.variant.assoc_namespace = &ns_anon;

    a_scope sc_ns_null = {};
    sc_ns_null.kind = sck_namespace;
    sc_ns_null.variant.assoc_namespace = nullptr;
    sc_ns_null.parent = &sc_ns_anon;

    a_type cls_named = {};
    cls_named.kind = tk_class;
    cls_named.source_corresp.name = "MyWidget";
    cls_named.source_corresp.parent_scope = &sc_ns_null;
    a_scope sc_cls_named = {};
    sc_cls_named.kind = sck_class_struct_union;
    sc_cls_named.variant.assoc_type = &cls_named;

    a_type cls_empty = {};
    cls_empty.kind = tk_class;
    cls_empty.source_corresp.name = "";
    cls_empty.source_corresp.parent_scope = &sc_cls_named;
    a_scope sc_cls_empty = {};
    sc_cls_empty.kind = sck_class_struct_union;
    sc_cls_empty.variant.assoc_type = &cls_empty;

    a_scope sc_cls_null = {};
    sc_cls_null.kind = sck_class_struct_union;
    sc_cls_null.variant.assoc_type = nullptr;
    sc_cls_null.parent = &sc_cls_empty;

    state.local_vars.erase(&m_var);
    m_var.source_corresp.parent_scope = &sc_cls_null;
    assert(llvm_lower_global_variable(&m_var) == llvm_gen_be_error_t::ok);
    assert(state.module->getNamedGlobal("_ZN5outer12_GLOBAL__N_112_GLOBAL__N_18MyWidget9AnonClass9AnonClass7plain_vE") != nullptr);

    state.local_vars.erase(&m_var);
    m_var.source_corresp.parent_scope = &sc_ns_outer;
    assert(llvm_lower_global_variable(&m_var) == llvm_gen_be_error_t::ok);
    assert(state.module->getNamedGlobal("_ZN5outer7plain_vE") != nullptr);
    m_var.source_corresp.parent_scope = &sc_cls_null;

    state.local_vars.erase(&m_var);
    m_var.source_corresp.name_has_been_mangled = TRUE;
    assert(llvm_lower_global_variable(&m_var) == llvm_gen_be_error_t::ok);
    m_var.source_corresp.name_has_been_mangled = FALSE;
    state.local_vars.erase(&m_var);
    m_var.source_corresp.name_linkage = nlk_external;
    assert(llvm_lower_global_variable(&m_var) == llvm_gen_be_error_t::ok);
    m_var.source_corresp.name_linkage = nlk_cplusplus_external;

    state.local_vars.erase(&m_var);
    m_var.source_corresp.name = "_ZN3foo3barE";
    assert(llvm_lower_global_variable(&m_var) == llvm_gen_be_error_t::ok);
    assert(state.module->getNamedGlobal("_ZN3foo3barE") != nullptr);

    state.local_vars.erase(&m_var);
    m_var.source_corresp.name = "_NotZ";
    assert(llvm_lower_global_variable(&m_var) == llvm_gen_be_error_t::ok);

    state.local_vars.erase(&m_var);
    m_var.source_corresp.name = "?msvc_var@@3HA";
    assert(llvm_lower_global_variable(&m_var) == llvm_gen_be_error_t::ok);
    assert(state.module->getNamedGlobal("?msvc_var@@3HA") != nullptr);

    // 2. Prototype instantiation / nonreal / explicit_instantiation on variables and functions
    a_variable var_proto = {};
    var_proto.source_corresp.name = "var_proto_skip";
    var_proto.type = &local_int_ty;
    var_proto.is_prototype_instantiation = TRUE;
    assert(llvm_lower_global_variable(&var_proto) == llvm_gen_be_error_t::ok);
    var_proto.is_prototype_instantiation = FALSE;
    var_proto.is_nonreal = TRUE;
    assert(llvm_lower_global_variable(&var_proto) == llvm_gen_be_error_t::ok);
    var_proto.is_nonreal = FALSE;
    var_proto.explicit_instantiation = TRUE;
    var_proto.source_corresp.name = "var_explicit_inst";
    assert(llvm_lower_global_variable(&var_proto) == llvm_gen_be_error_t::ok);
    assert(state.module->getNamedGlobal("var_explicit_inst")->getLinkage() == llvm::GlobalValue::WeakODRLinkage);

    a_routine rout_proto_inst = {};
    rout_proto_inst.source_corresp.name = "rout_proto_skip";
    rout_proto_inst.type = &fn_ty;
    rout_proto_inst.is_prototype_instantiation = TRUE;
    assert(llvm_lower_function_prototype(&rout_proto_inst) == llvm_gen_be_error_t::ok);

    rout_proto_inst.is_prototype_instantiation = FALSE;
    rout_proto_inst.function_def_number = 1;
    rout_proto_inst.explicit_instantiation = TRUE;
    rout_proto_inst.source_corresp.name = "rout_explicit_inst";
    assert(llvm_lower_function_prototype(&rout_proto_inst) == llvm_gen_be_error_t::ok);
    assert(state.module->getFunction("rout_explicit_inst")->getLinkage() == llvm::GlobalValue::WeakODRLinkage);

    rout_proto_inst.explicit_instantiation = FALSE;
    rout_proto_inst.is_template_function = TRUE;
    rout_proto_inst.source_corresp.name = "rout_tmpl_inst";
    assert(llvm_lower_function_prototype(&rout_proto_inst) == llvm_gen_be_error_t::ok);
    assert(state.module->getFunction("rout_tmpl_inst")->getLinkage() == llvm::GlobalValue::LinkOnceODRLinkage);

    // 3. Visited scopes deduplication, nested scopes (scope->scopes), region_scope_entry, and function-local static vars
    a_scope sub_sc = {};
    a_variable sub_var = {};
    sub_var.source_corresp.name = "sub_scope_var";
    sub_var.type = &local_int_ty;
    sub_sc.variables = &sub_var;
    sub_sc.scopes = &sub_sc; // self-loop to exercise visited_scopes deduplication!

    a_routine sub_rout_proto = {};
    sub_rout_proto.source_corresp.name = "sub_rout_proto_skip";
    sub_rout_proto.type = &fn_ty;
    sub_rout_proto.is_prototype_instantiation = TRUE;
    sub_sc.routines = &sub_rout_proto;

    root_scope.variables = nullptr;
    root_scope.namespaces = nullptr;
    root_scope.types = nullptr;
    root_scope.routines = nullptr;
    root_scope.scopes = &sub_sc;
    edg::il_header.primary_scope = &root_scope;

    assert(llvm_lower_global_variables() == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_declarations() == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_definitions() == llvm_gen_be_error_t::ok);

    // Error propagation through scope->scopes in lower_variables_in_scope, lower_routine_decls_in_scope, lower_routine_defs_in_scope
    sub_rout_proto.is_prototype_instantiation = FALSE;
    sub_rout_proto.source_corresp.name = "sub_rout_err";
    sub_rout_proto.source_corresp.referenced = TRUE;
    sub_rout_proto.function_def_number = 1;
    a_scope sub_fn_sc = {};
    a_statement bad_stmt = {};
    bad_stmt.kind = static_cast<a_statement_kind>(999);
    sub_fn_sc.assoc_block = &bad_stmt;
    g_decl_scope_for_rout = &sub_fn_sc;
    g_decl_fail_type = 1;
    assert(llvm_lower_global_variables() == llvm_gen_be_error_t::unsupported_type);
    g_decl_fail_type = 0;
    g_decl_fail_proto = 1;
    assert(llvm_lower_function_declarations() == llvm_gen_be_error_t::abi_classification_failed);
    g_decl_fail_proto = 0;
    assert(llvm_lower_function_prototype(&sub_rout_proto) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_definitions() == llvm_gen_be_error_t::unsupported_stmt);
    g_decl_scope_for_rout = nullptr;
    root_scope.scopes = nullptr;

    // 4. get_scope_for_routine_definition via il_header.region_scope_entry and function-local static variables in func_scope->variables
    a_scope reg_fn_sc = {};
    a_variable fn_static_var = {};
    fn_static_var.source_corresp.name = "fn_local_static_v";
    fn_static_var.type = &local_int_ty;
    fn_static_var.storage_class = sc_static;
    a_variable fn_auto_var = {};
    fn_auto_var.source_corresp.name = "fn_local_auto_v";
    fn_auto_var.type = &local_int_ty;
    fn_auto_var.storage_class = sc_auto;
    fn_static_var.next = &fn_auto_var;
    reg_fn_sc.variables = &fn_static_var;

    a_scope_ptr reg_table[4] = {nullptr, nullptr, &reg_fn_sc, nullptr};
    a_function_def_descr* saved_fdt = edg::il_header.function_def_table;
    a_scope_ptr* saved_rse = edg::il_header.region_scope_entry;
    edg::il_header.function_def_table = nullptr;
    edg::il_header.region_scope_entry = reg_table;

    a_routine reg_rout = {};
    reg_rout.source_corresp.name = "reg_scoped_func";
    reg_rout.type = &fn_ty;
    reg_rout.function_def_number = 1;
    reg_rout.memory_region = 2;
    assert(llvm_lower_function_prototype(&reg_rout) == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&reg_rout) == llvm_gen_be_error_t::ok);
    assert(state.module->getNamedGlobal("fn_local_static_v") != nullptr);

    // Also test error in function-local static variable lowering and null region_scope_entry fallback
    if (llvm::Function* rf = state.module->getFunction("reg_scoped_func")) rf->deleteBody();
    g_decl_fail_type = 1;
    assert(llvm_lower_function_body(&reg_rout) == llvm_gen_be_error_t::unsupported_type);
    g_decl_fail_type = 0;

    edg::il_header.region_scope_entry = nullptr;
    assert(llvm_lower_function_body(&reg_rout) == llvm_gen_be_error_t::ok);
    reg_rout.memory_region = NULL_region_number;
    assert(llvm_lower_function_body(&reg_rout) == llvm_gen_be_error_t::ok);

    edg::il_header.function_def_table = saved_fdt;
    edg::il_header.region_scope_entry = saved_rse;

    // 5. Phase 16 sanitizer & stack-protector attributes in llvm_lower_function_prototype
    sanitize_hwaddress = TRUE;
    use_safestack = TRUE;
    use_shadowcallstack = TRUE;
    stack_protector_mode = 1;
    p8_rout.source_corresp.name = "p16_sp1";
    assert(llvm_lower_function_prototype(&p8_rout) == llvm_gen_be_error_t::ok);
    stack_protector_mode = 2;
    p8_rout.source_corresp.name = "p16_sp2";
    assert(llvm_lower_function_prototype(&p8_rout) == llvm_gen_be_error_t::ok);
    stack_protector_mode = 3;
    p8_rout.source_corresp.name = "p16_sp3";
    assert(llvm_lower_function_prototype(&p8_rout) == llvm_gen_be_error_t::ok);
    sanitize_hwaddress = FALSE;
    use_safestack = FALSE;
    use_shadowcallstack = FALSE;
    stack_protector_mode = 0;

    // 6. Phase 18 enable_ubsan_checks missing return handler in llvm_lower_function_body
    state.enable_ubsan_checks = true;
    a_type nonvoid_rty = {};
    nonvoid_rty.kind = tk_routine;
    nonvoid_rty.variant.routine.return_type = &local_int_ty;
    a_routine mr_rout = {};
    mr_rout.source_corresp.name = "ubsan_missing_ret_fn";
    mr_rout.type = &nonvoid_rty;
    mr_rout.function_def_number = 1;
    a_scope mr_sc = {};
    g_decl_scope_for_rout = &mr_sc;
    g_decl_proto_mode = 2;
    assert(llvm_lower_function_prototype(&mr_rout) == llvm_gen_be_error_t::ok);
    g_decl_proto_mode = 0;
    assert(llvm_lower_function_body(&mr_rout) == llvm_gen_be_error_t::ok);
    assert(state.module->getFunction("__ubsan_handle_missing_return") != nullptr);
    g_decl_scope_for_rout = nullptr;
    state.enable_ubsan_checks = false;

    // 7. Phase 20: is_consteval skipping, nested block scope in llvm_lower_function_body, and declaration linkage normalization
    a_routine ce_rout = {};
    ce_rout.source_corresp.name = "ce_rout_skip";
    ce_rout.type = &fn_ty;
    ce_rout.is_consteval = TRUE;
    assert(llvm_lower_function_prototype(&ce_rout) == llvm_gen_be_error_t::ok);

    a_scope nested_blk_sc = {}, nested_exp_proto_sc = {};
    nested_exp_proto_sc.is_expansion_prototype = TRUE;
    nested_blk_sc.next = &nested_exp_proto_sc;
    reg_fn_sc.scopes = &nested_blk_sc;
    reg_fn_sc.variables = nullptr;
    g_decl_scope_for_rout = &reg_fn_sc;
    if (llvm::Function* rf = state.module->getFunction("reg_scoped_func")) rf->deleteBody();
    assert(llvm_lower_function_body(&reg_rout) == llvm_gen_be_error_t::ok);
    reg_fn_sc.scopes = nullptr;
    g_decl_scope_for_rout = nullptr;

    llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false),
        llvm::GlobalValue::InternalLinkage, "decl_internal_norm", state.module.get());
    llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false),
        llvm::GlobalValue::WeakAnyLinkage, "decl_weak_norm", state.module.get());
    root_scope.routines = &ce_rout;
    edg::il_header.primary_scope = &root_scope;
    assert(llvm_lower_function_definitions() == llvm_gen_be_error_t::ok);
    llvm_gen_be_debug_state_t fn_defs_dbg = {};
    state.dbg_state = &fn_defs_dbg;
    assert(llvm_lower_function_definitions() == llvm_gen_be_error_t::ok);
    state.dbg_state = nullptr;
    root_scope.routines = nullptr;

    // 8. Phase 5: Unlowered Itanium & MSVC C++ ABI entity mangling tests
    std::string mangled_out;
    a_source_correspondence m_scp = {};
    assert(llvm_mangle_unlowered_entity(nullptr, nullptr, false, &mangled_out) ==
           llvm_gen_be_error_t::invalid_argument);
    assert(llvm_mangle_unlowered_entity(&m_scp, nullptr, false, nullptr) ==
           llvm_gen_be_error_t::invalid_argument);

    // Already-mangled or prefixed names
    m_scp.name = "_Z3foov";
    assert(llvm_mangle_unlowered_entity(&m_scp, nullptr, false, &mangled_out) == llvm_gen_be_error_t::ok);
    assert(mangled_out == "_Z3foov");
    m_scp.name = "?foo@@YAXXZ";
    assert(llvm_mangle_unlowered_entity(&m_scp, nullptr, true, &mangled_out) == llvm_gen_be_error_t::ok);
    assert(mangled_out == "?foo@@YAXXZ");
    m_scp.name = "pre_mangled";
    m_scp.name_has_been_mangled = TRUE;
    assert(llvm_mangle_unlowered_entity(&m_scp, nullptr, false, &mangled_out) == llvm_gen_be_error_t::ok);
    assert(mangled_out == "pre_mangled");
    m_scp.name_has_been_mangled = FALSE;

    // Nested scopes: std namespace, anonymous namespace, named class, anonymous class
    a_namespace std_ns = {};
    std_ns.source_corresp.name = "std";
    a_scope std_sc = {};
    std_sc.kind = sck_namespace;
    std_sc.variant.assoc_namespace = &std_ns;

    a_scope anon_ns_sc = {};
    anon_ns_sc.kind = sck_namespace;
    anon_ns_sc.variant.assoc_namespace = nullptr;
    anon_ns_sc.parent = &std_sc;

    a_type named_cls = {};
    named_cls.kind = tk_class;
    named_cls.source_corresp.name = "MyVec";
    named_cls.source_corresp.parent_scope = &anon_ns_sc;
    a_scope cls_sc = {};
    cls_sc.kind = sck_class_struct_union;
    cls_sc.variant.assoc_type = &named_cls;

    a_scope anon_cls_sc = {};
    anon_cls_sc.kind = sck_class_struct_union;
    anon_cls_sc.variant.assoc_type = nullptr;
    anon_cls_sc.parent = &cls_sc;

    a_scope mangle_file_sc = {};
    mangle_file_sc.kind = sck_file;
    anon_cls_sc.parent = &cls_sc;
    std_ns.source_corresp.parent_scope = &mangle_file_sc;

    m_scp.name = nullptr; // "anon"
    m_scp.parent_scope = &anon_cls_sc;
    assert(llvm_mangle_unlowered_entity(&m_scp, &named_cls, false, &mangled_out) == llvm_gen_be_error_t::ok);
    assert(llvm_mangle_unlowered_entity(&m_scp, nullptr, true, &mangled_out) == llvm_gen_be_error_t::ok);

    // Template function with parameter types, qualifiers, pointers/references, and S_ / S0_ substitutions
    m_scp.name = "operator<";
    assert(llvm_mangle_unlowered_entity(&m_scp, nullptr, false, &mangled_out) == llvm_gen_be_error_t::ok);
    m_scp.name = "push<int>";
    a_type r_ty = {};
    r_ty.kind = tk_routine;
    assert(llvm_mangle_unlowered_entity(&m_scp, &r_ty, true, &mangled_out) == llvm_gen_be_error_t::ok);
    assert(llvm_mangle_unlowered_entity(&m_scp, &r_ty, false, &mangled_out) == llvm_gen_be_error_t::ok);

    a_routine_type_supplement mangle_r_supp = {};
    r_ty.variant.routine.extra_info = &mangle_r_supp;
    assert(llvm_mangle_unlowered_entity(&m_scp, &r_ty, false, &mangled_out) == llvm_gen_be_error_t::ok);

    a_type t_void = {}; t_void.kind = tk_void;
    a_type t_bool = {}; t_bool.kind = tk_integer; t_bool.variant.integer.bool_type = TRUE;
    a_type t_int_kinds[12] = {};
    an_integer_kind iks[12] = {
        ik_char, ik_signed_char, ik_unsigned_char, ik_short, ik_unsigned_short,
        ik_int, ik_unsigned_int, ik_long, ik_unsigned_long, ik_long_long, ik_unsigned_long_long,
        static_cast<an_integer_kind>(99)
    };
    for (int i = 0; i < 12; ++i) {
      t_int_kinds[i].kind = tk_integer;
      t_int_kinds[i].variant.integer.int_kind = iks[i];
    }
    a_type t_flt = {}; t_flt.kind = tk_float; t_flt.variant.float_kind = fk_float;
    a_type t_dbl = {}; t_dbl.kind = tk_float; t_dbl.variant.float_kind = fk_double;
    a_type t_ldbl = {}; t_ldbl.kind = tk_float; t_ldbl.variant.float_kind = fk_long_double;
    a_type t_ptr = {}; t_ptr.kind = tk_pointer; t_ptr.variant.pointer.type = &named_cls;
    a_type t_lref = {}; t_lref.kind = tk_pointer; t_lref.variant.pointer.is_reference = TRUE; t_lref.variant.pointer.type = &named_cls;
    a_type t_rref = {}; t_rref.kind = tk_pointer; t_rref.variant.pointer.is_reference = TRUE; t_rref.variant.pointer.is_rvalue_reference = TRUE; t_rref.variant.pointer.type = nullptr;
    a_type t_anon_user = {}; t_anon_user.kind = tk_struct; t_anon_user.source_corresp.name = nullptr;

    a_param_type pts[23] = {};
    pts[0].type = nullptr;
    pts[1].type = &t_void;
    pts[2].type = &t_bool;
    for (int i = 0; i < 12; ++i) {
      pts[3 + i].type = &t_int_kinds[i];
    }
    pts[15].type = &t_flt;
    pts[16].type = &t_dbl;
    pts[17].type = &t_ldbl;
    pts[18].type = &t_ptr;
    pts[19].type = &t_ptr; // triggers S_ substitution!
    pts[20].type = &t_lref;
    pts[21].type = &t_lref; // triggers S0_ substitution!
    pts[22].type = &t_rref;
    a_param_type pt_anon = {};
    pt_anon.type = &t_anon_user;
    for (int i = 0; i < 22; ++i) {
      pts[i].next = &pts[i + 1];
    }
    pts[22].next = &pt_anon;
    mangle_r_supp.param_type_list = &pts[0];

    g_decl_tqual = TQ_CONST | TQ_VOLATILE | TQ_RESTRICT;
    m_scp.parent_scope = nullptr;
    assert(llvm_mangle_unlowered_entity(&m_scp, &r_ty, false, &mangled_out) == llvm_gen_be_error_t::ok);
    g_decl_tqual = 0;
    assert(llvm_mangle_unlowered_entity(&m_scp, &r_ty, false, &mangled_out) == llvm_gen_be_error_t::ok);

    // Phase 3: Extended Itanium & MSVC type codes, operators, cv/ref qualifiers, and multi-argument template lists
    a_type t_nullptr = {}; t_nullptr.kind = tk_nullptr;
    a_type t_wchar = {}; t_wchar.kind = tk_integer; t_wchar.variant.integer.wchar_t_type = TRUE;
    a_type t_c8 = {}; t_c8.kind = tk_integer; t_c8.variant.integer.char8_t_type = TRUE;
    a_type t_c16 = {}; t_c16.kind = tk_integer; t_c16.variant.integer.char16_t_type = TRUE;
    a_type t_c32 = {}; t_c32.kind = tk_integer; t_c32.variant.integer.char32_t_type = TRUE;
    a_type t_i128 = {}; t_i128.kind = tk_integer; t_i128.size = 16; t_i128.variant.integer.explicitly_signed = TRUE;
    a_type t_u128 = {}; t_u128.kind = tk_integer; t_u128.size = 16; t_u128.variant.integer.explicitly_signed = FALSE;
    a_type t_f128 = {}; t_f128.kind = tk_float; t_f128.variant.float_kind = fk_float128;
    a_type t_f16 = {}; t_f16.kind = tk_float; t_f16.variant.float_kind = fk_float16;
    a_type t_fp16 = {}; t_fp16.kind = tk_float; t_fp16.variant.float_kind = fk_fp16;
    a_type t_bf16 = {}; t_bf16.kind = tk_float; t_bf16.variant.float_kind = fk_std_bfloat16;
    a_type t_cplx = {}; t_cplx.kind = tk_complex; t_cplx.variant.float_kind = fk_float;
    a_type t_arr = {}; t_arr.kind = tk_array; t_arr.variant.array.variant.number_of_elements = 4; t_arr.variant.array.element_type = &t_int_kinds[5];
    a_type t_fn_noparam = {}; t_fn_noparam.kind = tk_routine; t_fn_noparam.variant.routine.return_type = &t_void;
    a_param_type fn_p1 = {}; fn_p1.type = &t_int_kinds[5];
    a_routine_type_supplement fn_p1_supp = {}; fn_p1_supp.param_type_list = &fn_p1;
    a_type t_fn_withparam = {}; t_fn_withparam.kind = tk_routine; t_fn_withparam.variant.routine.return_type = &t_int_kinds[5]; t_fn_withparam.variant.routine.extra_info = &fn_p1_supp;
    a_type t_ptrmem = {}; t_ptrmem.kind = tk_ptr_to_member; t_ptrmem.variant.ptr_to_member.class_of_which_a_member = &named_cls; t_ptrmem.variant.ptr_to_member.type = &t_int_kinds[5];

    a_param_type ext_pts[16] = {};
    a_type_ptr ext_tys[16] = {
        &t_nullptr, &t_wchar, &t_c8, &t_c16, &t_c32, &t_i128, &t_u128,
        &t_f128, &t_f16, &t_fp16, &t_bf16, &t_cplx, &t_arr,
        &t_fn_noparam, &t_fn_withparam, &t_ptrmem
    };
    for (int i = 0; i < 16; ++i) {
      ext_pts[i].type = ext_tys[i];
      ext_pts[i].next = (i + 1 < 16) ? &ext_pts[i + 1] : nullptr;
    }
#if GNU_EXTENSIONS_ALLOWED && GNU_VECTOR_TYPES_ALLOWED
    a_type t_int_sz4 = t_int_kinds[5]; t_int_sz4.size = 4;
    a_type t_vec_sized = {}; t_vec_sized.kind = tk_vector; t_vec_sized.size = 16; t_vec_sized.variant.vector.element_type = &t_int_sz4;
    a_type t_vec_unsized = {}; t_vec_unsized.kind = tk_vector; t_vec_unsized.size = 0; t_vec_unsized.variant.vector.element_type = nullptr;
    a_param_type vec_pt1 = {}, vec_pt2 = {};
    vec_pt1.type = &t_vec_sized; vec_pt1.next = &vec_pt2;
    vec_pt2.type = &t_vec_unsized; vec_pt2.next = nullptr;
    ext_pts[15].next = &vec_pt1;
#endif
    mangle_r_supp.param_type_list = &ext_pts[0];
    m_scp.name = "ext_types<int, 42, -7, Foo>";
    m_scp.parent_scope = &cls_sc;
    mangle_r_supp.qualifiers = TQ_CONST | TQ_VOLATILE;
    mangle_r_supp.ref_qualifiers = rqk_lvalue;
    assert(llvm_mangle_unlowered_entity(&m_scp, &r_ty, false, &mangled_out) == llvm_gen_be_error_t::ok);
    mangle_r_supp.qualifiers = 0;
    mangle_r_supp.ref_qualifiers = rqk_rvalue;
    assert(llvm_mangle_unlowered_entity(&m_scp, &r_ty, false, &mangled_out) == llvm_gen_be_error_t::ok);
    mangle_r_supp.ref_qualifiers = rqk_default;
    a_type r_ty_nosupp = {}; r_ty_nosupp.kind = tk_routine;
    assert(llvm_mangle_unlowered_entity(&m_scp, &r_ty_nosupp, false, &mangled_out) == llvm_gen_be_error_t::ok);

    // All overloaded operators in encode_itanium_operator_or_name
    const char* op_names[39] = {
        "operator+", "operator-", "operator*", "operator/",
        "operator%", "operator^", "operator&", "operator|",
        "operator~", "operator!", "operator=", "operator<",
        "operator>", "operator+=", "operator-=", "operator*=",
        "operator/=", "operator%=", "operator^=", "operator&=",
        "operator|=", "operator<<", "operator>>", "operator<<=",
        "operator>>=", "operator==", "operator!=", "operator<=",
        "operator>=", "operator<=>", "operator&&", "operator||",
        "operator++", "operator--", "operator,", "operator->*",
        "operator->", "operator()", "operator[]",
    };
    for (int i = 0; i < 39; ++i) {
      m_scp.name = op_names[i];
      assert(llvm_mangle_unlowered_entity(&m_scp, &r_ty_nosupp, false, &mangled_out) == llvm_gen_be_error_t::ok);
    }
    const char* mem_op_names[19] = {
        "operator new", "operator new[]", "operator delete", "operator delete[]", "<leading_lt>",
        "operator?", "operator+unary", "operator-unary", "operator&unary", "operator*unary",
        "C1", "C2", "C3", "D0", "D1", "D2",
        "operator\"\"_km", "operator int", "operator bool"
    };
    for (int i = 0; i < 19; ++i) {
      m_scp.name = mem_op_names[i];
      assert(llvm_mangle_unlowered_entity(&m_scp, &r_ty_nosupp, false, &mangled_out) == llvm_gen_be_error_t::ok);
    }
    m_scp.name = "operator Foo";
    assert(llvm_mangle_unlowered_entity(&m_scp, &r_ty_nosupp, false, &mangled_out) == llvm_gen_be_error_t::ok);
    m_scp.name = "tmpl_fn<T_, T0_>";
    assert(llvm_mangle_unlowered_entity(&m_scp, &r_ty_nosupp, false, &mangled_out) == llvm_gen_be_error_t::ok);

    // Local function scope (_ZZ...E) and standard substitutions (Sa, Sb, Ss, Si, So, Sd)
    a_routine outer_fn_r = {};
    outer_fn_r.source_corresp.name = "outer_func";
    a_scope fn_local_sc = {};
    fn_local_sc.kind = sck_function;
    fn_local_sc.variant.routine.ptr = &outer_fn_r;
    fn_local_sc.parent = &mangle_file_sc;
    m_scp.parent_scope = &fn_local_sc;
    m_scp.name = "inner_local";
    assert(llvm_mangle_unlowered_entity(&m_scp, &r_ty_nosupp, false, &mangled_out) == llvm_gen_be_error_t::ok);
    fn_local_sc.variant.routine.ptr = nullptr;
    assert(llvm_mangle_unlowered_entity(&m_scp, &r_ty_nosupp, false, &mangled_out) == llvm_gen_be_error_t::ok);

    const char* std_sub_names[6] = {
        "std::allocator", "std::basic_string", "std::string",
        "std::istream", "std::ostream", "std::iostream"
    };
    for (int i = 0; i < 6; ++i) {
      std_ns.source_corresp.name = std_sub_names[i];
      m_scp.parent_scope = &std_sc;
      m_scp.name = "mem";
      assert(llvm_mangle_unlowered_entity(&m_scp, &r_ty_nosupp, false, &mangled_out) == llvm_gen_be_error_t::ok);
    }
    std_ns.source_corresp.name = "std";

    // MSVC type codes, free functions vs member functions (const & non-const), and class vs global data members
    a_param_type msvc_pts[22] = {};
    a_type_ptr msvc_tys[22] = {
        nullptr, &t_void, &t_bool, &t_int_kinds[0], &t_int_kinds[1], &t_int_kinds[2],
        &t_int_kinds[3], &t_int_kinds[4], &t_int_kinds[5], &t_int_kinds[6],
        &t_int_kinds[7], &t_int_kinds[8], &t_int_kinds[9], &t_int_kinds[10],
        &t_flt, &t_dbl, &t_ptr, &t_ptr, &t_lref, &t_rref, &named_cls, &t_anon_user
    };
    for (int i = 0; i < 22; ++i) {
      msvc_pts[i].type = msvc_tys[i];
      msvc_pts[i].next = (i + 1 < 22) ? &msvc_pts[i + 1] : nullptr;
    }
    mangle_r_supp.param_type_list = &msvc_pts[0];
    r_ty.variant.routine.return_type = &t_int_kinds[5];
    m_scp.name = "msvc_fn";
    m_scp.parent_scope = &cls_sc;
    mangle_r_supp.qualifiers = TQ_CONST;
    assert(llvm_mangle_unlowered_entity(&m_scp, &r_ty, true, &mangled_out) == llvm_gen_be_error_t::ok);
    mangle_r_supp.qualifiers = 0;
    for (a_calling_convention cc_v : {cc_cdecl, cc_stdcall, cc_thiscall, cc_fastcall, cc_vectorcall}) {
      mangle_r_supp.calling_convention = cc_v;
      assert(llvm_mangle_unlowered_entity(&m_scp, &r_ty, true, &mangled_out) == llvm_gen_be_error_t::ok);
    }
    mangle_r_supp.calling_convention = cc_default;
    m_scp.access = as_private;
    assert(llvm_mangle_unlowered_entity(&m_scp, &r_ty, true, &mangled_out) == llvm_gen_be_error_t::ok);
    m_scp.access = as_protected;
    assert(llvm_mangle_unlowered_entity(&m_scp, &r_ty, true, &mangled_out) == llvm_gen_be_error_t::ok);
    m_scp.access = as_public;

    const char* msvc_special_names[14] = {
        "C1", "C2", "D1", "D2", "~MyVec", "D0", "D_vec",
        "operator new", "operator delete", "operator new[]", "operator delete[]",
        "operator=", "operator==", "tpl_m<Foo>"
    };
    for (int i = 0; i < 14; ++i) {
      m_scp.name = msvc_special_names[i];
      assert(llvm_mangle_unlowered_entity(&m_scp, &r_ty, true, &mangled_out) == llvm_gen_be_error_t::ok);
    }
    m_scp.name = "msvc_fn";
    assert(llvm_mangle_unlowered_entity(&m_scp, &r_ty_nosupp, true, &mangled_out) == llvm_gen_be_error_t::ok);
    m_scp.parent_scope = nullptr;
    mangle_r_supp.param_type_list = nullptr;
    assert(llvm_mangle_unlowered_entity(&m_scp, &r_ty, true, &mangled_out) == llvm_gen_be_error_t::ok);
    assert(llvm_mangle_unlowered_entity(&m_scp, &named_cls, true, &mangled_out) == llvm_gen_be_error_t::ok);

    // 9. Phase 7: ak_no_sanitize_address and no_sanitize* function attribute exclusions
    an_attribute nosan_attrs[6] = {};
    an_attribute_arg nosan_args[5] = {};
    const char* nosan_names[5] = {
        "no_sanitize", "no_sanitize_address", "no_sanitize_thread",
        "no_sanitize_memory", "no_sanitize_undefined"
    };
    nosan_attrs[0].kind = ak_no_sanitize_address;
    nosan_attrs[0].next = &nosan_attrs[1];
    for (int i = 0; i < 5; ++i) {
      nosan_args[i].kind = aak_token;
      nosan_args[i].variant.token = const_cast<char*>(nosan_names[i]);
      nosan_attrs[1 + i].kind = ak_unrecognized;
      nosan_attrs[1 + i].arguments = &nosan_args[i];
      if (i < 4) nosan_attrs[1 + i].next = &nosan_attrs[2 + i];
    }
    a_routine nosan_rout = {};
    nosan_rout.source_corresp.name = "nosan_test_fn";
    nosan_rout.type = &fn_ty;
    nosan_rout.source_corresp.attributes = &nosan_attrs[0];
    assert(llvm_lower_function_prototype(&nosan_rout) == llvm_gen_be_error_t::ok);

    // 10. Phase 9 & Phase 10: Mach-O COMDAT omission and C++20 module BMI/global_ctors registration
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "aarch64-apple-darwin");
    a_variable macho_comdat_var = {};
    macho_comdat_var.source_corresp.name = "macho_inline_var";
    macho_comdat_var.type = &t_int_kinds[0];
    macho_comdat_var.storage_class = sc_unspecified;
    macho_comdat_var.is_inline = TRUE;
    macho_comdat_var.comdat_group = (char*)"macho_grp";
    assert(llvm_lower_global_variable(&macho_comdat_var) == llvm_gen_be_error_t::ok);

    a_routine macho_comdat_rout = {};
    macho_comdat_rout.source_corresp.name = "macho_inline_fn";
    macho_comdat_rout.type = &fn_ty;
    macho_comdat_rout.function_def_number = 1;
    macho_comdat_rout.is_inline = TRUE;
    macho_comdat_rout.use_comdat = TRUE;
    assert(llvm_lower_function_prototype(&macho_comdat_rout) == llvm_gen_be_error_t::ok);

    a_module macho_mod_desc = {};
    macho_mod_desc.kind = mk_unit;
    macho_mod_desc.variant.unit.name = (char*)"MachoMod";
    edg::trans_unit_module = &macho_mod_desc;
    assert(llvm_lower_module_metadata() == llvm_gen_be_error_t::ok);
    assert(llvm_lower_global_ctors_and_dtors() == llvm_gen_be_error_t::ok);

    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    assert(llvm_lower_global_ctors_and_dtors() == llvm_gen_be_error_t::ok);
    edg::trans_unit_module = nullptr;
    assert(llvm_lower_global_ctors_and_dtors() == llvm_gen_be_error_t::ok);
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
    assert(llvm_lower_global_ctors_and_dtors() == llvm_gen_be_error_t::ok);

    // 11. Phase 1: Reverse-order member/base destructor synthesis in ~D(), sized D0, class-specific operator delete, and MSVC ??_G
    g_decl_proto_mode = 1; // void(ptr)
    a_routine m_dtor_rout = {}, m_dtor_existing = {}, m_other_rout = {};
    m_other_rout.special_kind = sfk_none;
    m_other_rout.next = &m_dtor_rout;
    m_dtor_rout.special_kind = sfk_destructor;
    m_dtor_rout.source_corresp.name = "_ZN3SubD1Ev";
    m_dtor_rout.source_corresp.name_has_been_mangled = TRUE;
    m_dtor_existing.special_kind = sfk_destructor;
    m_dtor_existing.source_corresp.name = "_ZN3SubD1Ev_exist";
    m_dtor_existing.source_corresp.name_has_been_mangled = TRUE;
    m_dtor_existing.type = &fn_ty;
    assert(llvm_lower_function_prototype(&m_dtor_existing) == llvm_gen_be_error_t::ok);
    state.module->getFunction("_ZN3SubD1Ev_exist")->setDoesNotThrow();

    a_scope sub_cls_sc = {}, sub_cls_exist_sc = {}, sub_cls_nodtor_sc = {};
    sub_cls_sc.routines = &m_other_rout;
    sub_cls_exist_sc.routines = &m_dtor_existing;
    a_routine m_only_other = {};
    m_only_other.special_kind = sfk_none;
    sub_cls_nodtor_sc.routines = &m_only_other;

    a_class_type_supplement sub_cls_supp = {}, sub_cls_exist_supp = {}, sub_cls_nodtor_supp = {}, sub_cls_nullsc_supp = {};
    sub_cls_supp.assoc_scope = &sub_cls_sc;
    sub_cls_exist_supp.assoc_scope = &sub_cls_exist_sc;
    sub_cls_nodtor_supp.assoc_scope = &sub_cls_nodtor_sc;
    sub_cls_nullsc_supp.assoc_scope = nullptr;

    a_type sub_cls_ty = {}, sub_cls_exist_ty = {}, sub_cls_nodtor_ty = {}, sub_cls_noextra_ty = {}, sub_cls_nullsc_ty = {}, sub_triv_ty = {};
    sub_cls_ty.kind = tk_class; sub_cls_ty.size = 8;
    sub_cls_ty.variant.class_struct_union.extra_info = &sub_cls_supp;

    sub_cls_exist_ty.kind = tk_struct; sub_cls_exist_ty.size = 0; // exercises elem_sz == 0 -> 1 fallback!
    sub_cls_exist_ty.variant.class_struct_union.extra_info = &sub_cls_exist_supp;

    sub_cls_nodtor_ty.kind = tk_class; sub_cls_nodtor_ty.size = 4;
    sub_cls_nodtor_ty.variant.class_struct_union.extra_info = &sub_cls_nodtor_supp;

    sub_cls_noextra_ty.kind = tk_class; sub_cls_noextra_ty.size = 4;
    sub_cls_noextra_ty.variant.class_struct_union.extra_info = nullptr;

    sub_cls_nullsc_ty.kind = tk_class; sub_cls_nullsc_ty.size = 4;
    sub_cls_nullsc_ty.variant.class_struct_union.extra_info = &sub_cls_nullsc_supp;

    sub_triv_ty.kind = tk_integer; sub_triv_ty.size = 4;

    a_type arr_sub_ty = {}, arr_zero_ty = {}, arr_null_elem_ty = {};
    arr_sub_ty.kind = tk_array;
    arr_sub_ty.variant.array.element_type = &sub_cls_exist_ty;
    arr_sub_ty.variant.array.variant.number_of_elements = 2;
    arr_zero_ty.kind = tk_array;
    arr_zero_ty.variant.array.element_type = &sub_cls_ty;
    arr_zero_ty.variant.array.variant.number_of_elements = 0;
    arr_null_elem_ty.kind = tk_array;
    arr_null_elem_ty.variant.array.element_type = nullptr;

    a_field df_null = {}, df_triv = {}, df_nodtor = {}, df_noextra = {}, df_nullsc = {}, df_arr_null = {}, df_arr_zero = {}, df_arr = {}, df_cls = {};
    df_null.type = nullptr; df_null.next = &df_triv;
    df_triv.type = &sub_triv_ty; df_triv.next = &df_nodtor;
    df_nodtor.type = &sub_cls_nodtor_ty; df_nodtor.next = &df_noextra;
    df_noextra.type = &sub_cls_noextra_ty; df_noextra.next = &df_nullsc;
    df_nullsc.type = &sub_cls_nullsc_ty; df_nullsc.next = &df_arr_null;
    df_arr_null.type = &arr_null_elem_ty; df_arr_null.next = &df_arr_zero;
    df_arr_zero.type = &arr_zero_ty; df_arr_zero.offset = 4; df_arr_zero.next = &df_arr;
    df_arr.type = &arr_sub_ty; df_arr.offset = 8; df_arr.next = &df_cls;
    df_cls.type = &sub_cls_ty; df_cls.offset = 24; df_cls.next = nullptr;

    a_base_class db_v = {}, db_null = {}, db_triv = {}, db_nodtor = {}, db_ok = {};
    db_v.is_virtual = TRUE; db_v.type = &sub_cls_ty; db_v.next_direct = &db_null;
    db_null.is_virtual = FALSE; db_null.type = nullptr; db_null.next_direct = &db_triv;
    db_triv.is_virtual = FALSE; db_triv.type = &sub_triv_ty; db_triv.next_direct = &db_nodtor;
    db_nodtor.is_virtual = FALSE; db_nodtor.type = &sub_cls_nodtor_ty; db_nodtor.next_direct = &db_ok;
    db_ok.is_virtual = FALSE; db_ok.type = &sub_cls_ty; db_ok.offset = 32; db_ok.next_direct = nullptr;

    a_base_class vb_nv = {}, vb_null = {}, vb_triv = {}, vb_nodtor = {}, vb_ok = {};
    vb_nv.is_virtual = FALSE; vb_nv.type = &sub_cls_ty; vb_nv.next = &vb_null;
    vb_null.is_virtual = TRUE; vb_null.type = nullptr; vb_null.next = &vb_triv;
    vb_triv.is_virtual = TRUE; vb_triv.type = &sub_triv_ty; vb_triv.next = &vb_nodtor;
    vb_nodtor.is_virtual = TRUE; vb_nodtor.type = &sub_cls_nodtor_ty; vb_nodtor.next = &vb_ok;
    vb_ok.is_virtual = TRUE; vb_ok.type = &sub_cls_ty; vb_ok.offset = 48; vb_ok.next = nullptr;

    a_routine custom_del_r = {}, other_op_r = {}, unnamed_op_r = {}, custom_del_mangled_r = {};
    other_op_r.special_kind = sfk_operator; other_op_r.source_corresp.name = "operator new"; other_op_r.next = &unnamed_op_r;
    unnamed_op_r.special_kind = sfk_operator; unnamed_op_r.source_corresp.name = nullptr; unnamed_op_r.next = &custom_del_r;
    custom_del_r.special_kind = sfk_operator; custom_del_r.source_corresp.name = "operator delete";
    custom_del_r.source_corresp.name_has_been_mangled = TRUE;
    custom_del_mangled_r.special_kind = sfk_operator; custom_del_mangled_r.source_corresp.name = "_ZN6DerClsdlEPv";
    custom_del_mangled_r.source_corresp.name_has_been_mangled = TRUE;

    a_scope der_cls_sc = {};
    der_cls_sc.routines = &other_op_r;
    a_class_type_supplement der_cls_supp = {};
    der_cls_supp.assoc_scope = &der_cls_sc;
    der_cls_supp.direct_base_classes = &db_v;
    der_cls_supp.base_classes = &vb_nv;

    a_type der_cls_ty = {};
    der_cls_ty.kind = tk_class;
    der_cls_ty.size = 64;
    der_cls_ty.source_corresp.name = "DerCls";
    der_cls_ty.variant.class_struct_union.field_list = &df_null;
    der_cls_ty.variant.class_struct_union.extra_info = &der_cls_supp;
    der_cls_sc.kind = sck_class_struct_union;
    der_cls_sc.variant.assoc_type = &der_cls_ty;

    a_type der_ptr_ty = {};
    der_ptr_ty.kind = tk_pointer;
    der_ptr_ty.variant.pointer.type = &der_cls_ty;
    a_variable der_this_v = {};
    der_this_v.source_corresp.name = "this";
    der_this_v.type = &der_ptr_ty;

    a_scope dtor_fn_sc = {};
    dtor_fn_sc.variant.routine.parameters = &der_this_v;
    g_decl_scope_for_rout = &dtor_fn_sc;

    a_routine der_dtor = {};
    der_dtor.special_kind = sfk_destructor;
    der_dtor.ctor_dtor_kind = cdk_complete;
    der_dtor.type = &fn_ty;
    der_dtor.function_def_number = 1;
    der_dtor.source_corresp.parent_scope = &der_cls_sc;

    // 1) Complete dtor (D1) with fields + direct bases + virtual bases (triggers invoke when >1 cleanup!)
    assert(recreate_test_fn_body(&der_dtor, "_ZN6DerClsD1Ev") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&der_dtor) == llvm_gen_be_error_t::ok);

    // 2) Subobject dtor (D2) with custom operator delete -> emits D1 and D0 calling custom operator delete
    der_dtor.ctor_dtor_kind = cdk_subobject;
    assert(recreate_test_fn_body(&der_dtor, "_ZN6DerClsD2Ev_cust") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&der_dtor) == llvm_gen_be_error_t::ok);
    assert(state.module->getFunction("_ZN6DerClsD0Ev_cust") != nullptr);

    // Also test llvm_find_class_operator_delete with mangled "dlPv" name
    unnamed_op_r.next = &custom_del_mangled_r;
    assert(recreate_test_fn_body(&der_dtor, "_ZN6DerClsD2Ev_cust_mangled") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&der_dtor) == llvm_gen_be_error_t::ok);

    // 3) Subobject dtor (D2) without custom operator delete on sized class -> D0 calls _ZdlPvm
    unnamed_op_r.next = nullptr;
    assert(recreate_test_fn_body(&der_dtor, "_ZN6DerClsD2Ev_sized") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&der_dtor) == llvm_gen_be_error_t::ok);
    assert(state.module->getFunction("_ZdlPvm") != nullptr);

    // Also test when owner_class_tp has extra_info == nullptr or assoc_scope == nullptr or non-class kind in llvm_find_class_operator_delete
    der_cls_supp.assoc_scope = nullptr;
    assert(recreate_test_fn_body(&der_dtor, "_ZN6DerClsD2Ev_nullsc") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&der_dtor) == llvm_gen_be_error_t::ok);
    der_cls_ty.variant.class_struct_union.extra_info = nullptr;
    assert(recreate_test_fn_body(&der_dtor, "_ZN6DerClsD2Ev_noextra") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&der_dtor) == llvm_gen_be_error_t::ok);
    der_cls_ty.kind = tk_union;
    assert(recreate_test_fn_body(&der_dtor, "_ZN6DerClsD2Ev_union") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&der_dtor) == llvm_gen_be_error_t::ok);
    der_cls_ty.kind = tk_class;
    der_cls_ty.variant.class_struct_union.extra_info = &der_cls_supp;
    der_cls_supp.assoc_scope = &der_cls_sc;

    // 4) MSVC scalar deleting destructor (??_G) with default ??3@YAXPEAX@Z, with custom operator delete, and unnamed owner class
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
    assert(recreate_test_fn_body(&der_dtor, "??1DerCls@@QEAA@XZ") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&der_dtor) == llvm_gen_be_error_t::ok);
    assert(state.module->getFunction("??_GDerCls@@UEAAPEAXI@Z") != nullptr);
    // Call again when ??_GDerCls@@UEAAPEAXI@Z already exists!
    assert(recreate_test_fn_body(&der_dtor, "??1DerCls@@QEAA@XZ") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&der_dtor) == llvm_gen_be_error_t::ok);

    // MSVC dtor with custom operator delete and unnamed owner class (and size == 0)
    unnamed_op_r.next = &custom_del_r;
    der_cls_ty.source_corresp.name = nullptr;
    der_cls_ty.size = 0;
    assert(recreate_test_fn_body(&der_dtor, "??1AnonMsvc@@QEAA@XZ") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&der_dtor) == llvm_gen_be_error_t::ok);
    assert(state.module->getFunction("??_G??1AnonMsvc@@QEAA@XZ@@UEAAPEAXI@Z") != nullptr);
    assert(state.module->getFunction("??_E??1AnonMsvc@@QEAA@XZ@@UEAAPEAXI@Z") != nullptr);
    der_cls_ty.size = 64;

    // MSVC constructor (special_kind == sfk_constructor -> skips ??_G synthesis)
    der_dtor.special_kind = sfk_constructor;
    assert(recreate_test_fn_body(&der_dtor, "??0AnonMsvc@@QEAA@XZ") == llvm_gen_be_error_t::ok);
    assert(llvm_lower_function_body(&der_dtor) == llvm_gen_be_error_t::ok);
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
    g_decl_scope_for_rout = nullptr;

    // 12. Phase 3: Uniform llvm_compute_target_return_info across all 11 target ABIs
    bool sret_res = false;
    llvm::Type* coerced_res = nullptr;
    assert(llvm_compute_target_return_info(&local_int_ty, false, nullptr, &coerced_res) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_compute_target_return_info(&local_int_ty, false, &sret_res, nullptr) == llvm_gen_be_error_t::invalid_argument);

    const char* abi_triples[] = {
        "x86_64-pc-windows-msvc",
        "aarch64-unknown-linux-gnu",
        "i386-unknown-linux-gnu",
        "armv7-unknown-linux-gnueabihf",
        "thumbv7-unknown-linux-gnueabihf",
        "riscv64-unknown-linux-gnu",
        "riscv32-unknown-elf",
        "loongarch64-unknown-linux-gnu",
        "loongarch32-unknown-linux-gnu",
        "mips64-unknown-linux-gnu",
        "mips-unknown-linux-gnu",
        "powerpc64le-unknown-linux-gnu",
        "powerpc-unknown-linux-gnu",
        "s390x-ibm-linux-gnu",
        "sparc-unknown-linux-gnu",
        "sparcv9-sun-solaris2.11",
        "sparcel-unknown-linux-gnu",
        "wasm32-unknown-unknown",
        "x86_64-unknown-linux-gnu"
    };
    for (const char* tr : abi_triples) {
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), tr);
      assert(llvm_compute_target_return_info(&local_int_ty, false, &sret_res, &coerced_res) == llvm_gen_be_error_t::ok);
    }
    // ARM32 branches: is_indirect == true, is_hfa == true, size > 4, size <= 4
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "armv7-unknown-linux-gnueabihf");
    g_decl_sret_ret_info = true;
    assert(llvm_compute_target_return_info(&local_int_ty, false, &sret_res, &coerced_res) == llvm_gen_be_error_t::ok);
    assert(sret_res);
    g_decl_sret_ret_info = false;
    a_type arm_hfa_ty = {}; arm_hfa_ty.kind = tk_float; arm_hfa_ty.size = 16;
    assert(llvm_compute_target_return_info(&arm_hfa_ty, false, &sret_res, &coerced_res) == llvm_gen_be_error_t::ok);
    assert(!sret_res);
    a_type arm_big_ty = {}; arm_big_ty.kind = tk_struct; arm_big_ty.size = 8;
    assert(llvm_compute_target_return_info(&arm_big_ty, false, &sret_res, &coerced_res) == llvm_gen_be_error_t::ok);
    assert(sret_res);

    // Phase 9: Test gpu_kernel and global attributes across NVPTX, AMDGPU, and SPIR-V
    an_attribute_arg gpu_arg = {};
    gpu_arg.kind = aak_token;
    gpu_arg.variant.token = "gpu_kernel";
    an_attribute gpu_attr = {};
    gpu_attr.kind = ak_unrecognized;
    gpu_attr.arguments = &gpu_arg;
    a_routine gpu_rout = {};
    gpu_rout.type = der_dtor.type;
    gpu_rout.source_corresp.attributes = &gpu_attr;

    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "nvptx64-nvidia-cuda");
    gpu_rout.source_corresp.name = "nvptx_kern";
    assert(llvm_lower_function_prototype(&gpu_rout) == llvm_gen_be_error_t::ok);

    gpu_arg.variant.token = "global";
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "amdgcn-amd-amdhsa");
    gpu_rout.source_corresp.name = "amdgpu_kern";
    assert(llvm_lower_function_prototype(&gpu_rout) == llvm_gen_be_error_t::ok);

    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "spirv64-unknown-unknown");
    gpu_rout.source_corresp.name = "spirv_kern";
    assert(llvm_lower_function_prototype(&gpu_rout) == llvm_gen_be_error_t::ok);
    EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");

    // Test ak_alloc_size on a multi-argument function (F->arg_size() > 1)
    an_attribute attr_alloc_sz = {};
    attr_alloc_sz.kind = ak_alloc_size;
    a_routine rout_realloc = {};
    rout_realloc.source_corresp.name = "test_realloc_fn";
    rout_realloc.type = &fn_ty;
    rout_realloc.source_corresp.attributes = &attr_alloc_sz;
    g_decl_proto_mode = 2;
    assert(llvm_lower_function_prototype(&rout_realloc) == llvm_gen_be_error_t::ok);
    g_decl_proto_mode = 0;

    // Test tentative global definition replacement when existing global has a different type and existing uses
    llvm::GlobalVariable* old_tentative = llvm::cast<llvm::GlobalVariable>(
        state.module->getOrInsertGlobal("tentative_arr_var", llvm::Type::getInt8Ty(*state.context)));
    old_tentative->setConstant(false);
    old_tentative->setLinkage(llvm::GlobalValue::InternalLinkage);
    old_tentative->setInitializer(llvm::ConstantInt::get(llvm::Type::getInt8Ty(*state.context), 0));
    assert(old_tentative != nullptr);
    llvm::GlobalVariable* user_gv = llvm::cast<llvm::GlobalVariable>(
        state.module->getOrInsertGlobal("tentative_arr_user", llvm::PointerType::getUnqual(*state.context)));
    user_gv->setConstant(false);
    user_gv->setLinkage(llvm::GlobalValue::InternalLinkage);
    user_gv->setInitializer(old_tentative);
    assert(user_gv != nullptr);
    a_variable completed_var = {};
    completed_var.source_corresp.name = "tentative_arr_var";
    completed_var.storage_class = sc_static;
    completed_var.type = &local_int_ty;
    a_constant comp_con = {};
    an_initializer comp_init = {};
    comp_init.constant = &comp_con;
    g_decl_init_kind = initk_static;
    g_decl_init_ptr = &comp_init;
    assert(llvm_lower_global_variable(&completed_var) == llvm_gen_be_error_t::ok);
    g_decl_init_kind = initk_none;
    g_decl_init_ptr = nullptr;

    // =========================================================================
    // Wave 3: Exhaustive branch, region, and OOM coverage for llvm_gen_be_decl.cpp
    // =========================================================================
    {
      // 1) scan_omp_pragmas_in_stmt_tree via fopenmp_enabled + assoc_block
      const char* omp_tmp_path = "omp_scan_test_tmp.cpp";
      {
        FILE* fp = std::fopen(omp_tmp_path, "wb");
        assert(fp != nullptr);
        const char* src_txt =
            "   \r\n"
            "#pragma once\r\n"
            "int x = 0;\n"
            "#pragma omp critical\n"
            "   \n"
            "x++;\n"
            "int no_pragma = 1;\n"
            "no_pragma++;";
        std::fwrite(src_txt, 1, std::strlen(src_txt), fp);
        std::fclose(fp);
      }

      a_statement omp_s0 = {}, omp_salready = {}, omp_sl1 = {}, omp_sl2 = {}, omp_sl3 = {};
      a_statement omp_sfor = {}, omp_sfor_body = {}, omp_swhile = {}, omp_sdowhile = {};
      a_statement omp_sif = {}, omp_ssw = {}, omp_sexpr = {}, omp_sexpr2 = {}, omp_sblk = {};
      omp_s0.kind = stmk_expr; omp_s0.position.seq = 0; omp_s0.next = &omp_salready;
      omp_salready.kind = stmk_expr; omp_salready.position.seq = 6; omp_salready.next = &omp_sl1;
      omp_sl1.kind = stmk_expr; omp_sl1.position.seq = 1; omp_sl1.next = &omp_sl2;
      omp_sl2.kind = stmk_expr; omp_sl2.position.seq = 2; omp_sl2.next = &omp_sl3;
      omp_sl3.kind = stmk_expr; omp_sl3.position.seq = 3; omp_sl3.next = &omp_sfor;
      omp_sfor_body.kind = stmk_expr; omp_sfor_body.position.seq = 6; // duplicate claim_key!
      omp_sfor.kind = stmk_for; omp_sfor.position.seq = 6; omp_sfor.variant.for_loop.statement = &omp_sfor_body; omp_sfor.next = &omp_swhile;
      omp_sexpr2.kind = stmk_expr; omp_sexpr2.position.seq = 8;
      omp_ssw.kind = stmk_switch; omp_ssw.position.seq = 99; omp_ssw.variant.switch_stmt.body_statement = &omp_sexpr2;
      omp_sexpr.kind = stmk_expr; omp_sexpr.position.seq = 6;
      omp_sif.kind = stmk_if; omp_sif.variant.if_stmt.then_statement = &omp_ssw; omp_sif.variant.if_stmt.else_statement = &omp_sexpr;
      omp_sdowhile.kind = stmk_end_test_while; omp_sdowhile.variant.loop_statement = &omp_sif;
      omp_swhile.kind = stmk_while; omp_swhile.variant.loop_statement = &omp_sdowhile;
      omp_sblk.kind = stmk_block; omp_sblk.variant.block.statements = &omp_s0;

      a_type omp_rty = {};
      omp_rty.kind = tk_routine;
      omp_rty.variant.routine.return_type = &local_int_ty;
      a_scope omp_sc = {};
      omp_sc.assoc_block = &omp_sblk;
      a_routine omp_rout = {};
      omp_rout.type = &omp_rty;
      omp_rout.function_def_number = 1;
      g_decl_scope_for_rout = &omp_sc;
      state.fopenmp_enabled = true;
      g_decl_fail_ret_info = 1;
      g_decl_omp_scan_active = 1;

      // (a) null full_fn & short_fn
      g_decl_omp_full_fn = nullptr; g_decl_omp_short_fn = nullptr;
      assert(recreate_test_fn_body(&omp_rout, "omp_scan_fn_a") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&omp_rout) == llvm_gen_be_error_t::abi_classification_failed);

      // (b) empty full_fn & empty short_fn
      g_decl_omp_full_fn = ""; g_decl_omp_short_fn = "";
      assert(recreate_test_fn_body(&omp_rout, "omp_scan_fn_b") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&omp_rout) == llvm_gen_be_error_t::abi_classification_failed);

      // (c) non-empty full_fn pointing to nonexistent file (!buf_or_err)
      g_decl_omp_full_fn = "nonexistent_omp_scan_xyz.cpp"; g_decl_omp_short_fn = omp_tmp_path;
      assert(recreate_test_fn_body(&omp_rout, "omp_scan_fn_c") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&omp_rout) == llvm_gen_be_error_t::abi_classification_failed);

      // (d) null full_fn, valid short_fn -> loads file, attaches #pragma omp critical
      state.omp_pragmas.clear();
      state.omp_pragmas[&omp_salready] = "omp barrier";
      g_decl_omp_full_fn = nullptr; g_decl_omp_short_fn = omp_tmp_path;
      assert(recreate_test_fn_body(&omp_rout, "omp_scan_fn_d") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&omp_rout) == llvm_gen_be_error_t::abi_classification_failed);
      assert(omp_sfor.has_associated_pragma == TRUE);

      // (e) second run hits omp_source_file_cache
      state.omp_pragmas.clear();
      state.omp_pragmas[&omp_salready] = "omp barrier";
      assert(recreate_test_fn_body(&omp_rout, "omp_scan_fn_e") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&omp_rout) == llvm_gen_be_error_t::abi_classification_failed);

      // (f) OOM sweep across scan_omp_pragmas_in_stmt_tree (covers LLVM_GEN_BE_NEW + all recursive checks)
      for (int b = 0; b < 50; ++b) {
        state.omp_pragmas.clear();
        assert(recreate_test_fn_body(&omp_rout, "omp_scan_fn_oom") == llvm_gen_be_error_t::ok);
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_lower_function_body(&omp_rout);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::abi_classification_failed) break;
      }
      // Also test fopenmp_enabled == true with assoc_block == nullptr
      omp_sc.assoc_block = nullptr;
      assert(recreate_test_fn_body(&omp_rout, "omp_scan_fn_nullblk") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&omp_rout) == llvm_gen_be_error_t::abi_classification_failed);

      g_decl_omp_scan_active = 0;
      g_decl_fail_ret_info = 0;
      state.fopenmp_enabled = false;
      state.omp_pragmas.clear();
      state.omp_source_file_cache.clear();
      g_decl_omp_full_fn = nullptr;
      g_decl_omp_short_fn = nullptr;
      std::remove(omp_tmp_path);

      // 2) llvm_lower_global_variable: had_existing_gvar == true on eval failure, diff llvm_init type, and OOM sweeps
      llvm::GlobalVariable* pre_gv = llvm::cast<llvm::GlobalVariable>(
          state.module->getOrInsertGlobal("pre_existing_fail_gv", llvm::Type::getInt32Ty(*state.context)));
      pre_gv->setConstant(false);
      pre_gv->setLinkage(llvm::GlobalValue::ExternalLinkage);
      assert(pre_gv != nullptr);
      a_variable fail_existing_var = {};
      fail_existing_var.source_corresp.name = "pre_existing_fail_gv";
      fail_existing_var.type = &local_int_ty;
      g_decl_init_kind = initk_static;
      g_decl_init_ptr = &comp_init;
      g_decl_fail_const = 1;
      assert(llvm_lower_global_variable(&fail_existing_var) == llvm_gen_be_error_t::constant_eval_failure);
      assert(state.module->getNamedGlobal("pre_existing_fail_gv") != nullptr);
      // Also test when a Function occupies sym_name (!had_existing_gvar == true, gvar == nullptr)
      llvm::Function::Create(
          llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false),
          llvm::GlobalValue::ExternalLinkage, "fn_occupies_gv_name", state.module.get());
      a_variable fail_fn_coll_var = {};
      fail_fn_coll_var.source_corresp.name = "fn_occupies_gv_name";
      fail_fn_coll_var.type = &local_int_ty;
      assert(llvm_lower_global_variable(&fail_fn_coll_var) == llvm_gen_be_error_t::constant_eval_failure);
      // Also test fresh variable (`!had_existing_gvar && gvar` True at L991!)
      a_variable fail_fresh_var = {};
      fail_fresh_var.source_corresp.name = "fresh_fail_const_gv";
      fail_fresh_var.type = &local_int_ty;
      assert(llvm_lower_global_variable(&fail_fresh_var) == llvm_gen_be_error_t::constant_eval_failure);
      assert(state.module->getNamedGlobal("fresh_fail_const_gv") == nullptr);
      g_decl_fail_const = 0;
      assert(llvm_lower_global_variable(&fail_fn_coll_var) == llvm_gen_be_error_t::ok); // covers if (gvar) False at L1021!

      // llvm_init->getType() != gvar->getValueType() (L960 true + OOM on L962)
      a_variable diff_init_var = {};
      diff_init_var.source_corresp.name = "diff_init_ty_gv";
      diff_init_var.type = &local_int_ty;
      g_decl_const_diff_ty = 1;
      for (int b = 0; b < 15; ++b) {
        state.local_vars.erase(&diff_init_var);
        if (auto* old_g = state.module->getNamedGlobal("diff_init_ty_gv")) old_g->eraseFromParent();
        // Pre-insert mismatching i8 global to also sweep L914 OOM in the same loop!
        llvm::GlobalVariable* diff_seed = llvm::cast<llvm::GlobalVariable>(
            state.module->getOrInsertGlobal("diff_init_ty_gv", llvm::Type::getInt8Ty(*state.context)));
        diff_seed->setConstant(false);
        diff_seed->setLinkage(llvm::GlobalValue::ExternalLinkage);
        assert(diff_seed != nullptr);
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_lower_global_variable(&diff_init_var);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }
      g_decl_const_diff_ty = 0;
      g_decl_init_kind = initk_none;
      g_decl_init_ptr = nullptr;

      // Attribute & TLS OOM sweeps on global variable and function prototype
      llvm::Function* dummy_resolver = llvm::Function::Create(
          llvm::FunctionType::get(llvm::PointerType::getUnqual(*state.context), false),
          llvm::GlobalValue::ExternalLinkage, "w3_ifunc_resolver", state.module.get());
      (void)dummy_resolver;
      an_attribute_arg arg_sec = {}, arg_alias = {}, arg_ifunc = {}, arg_unrec = {};
      arg_sec.kind = aak_token; arg_sec.variant.token = "\".mysec\"";
      arg_alias.kind = aak_token; arg_alias.variant.token = "\"pre_existing_fail_gv\"";
      arg_ifunc.kind = aak_token; arg_ifunc.variant.token = "\"w3_ifunc_resolver\"";
      arg_unrec.kind = aak_token; arg_unrec.variant.token = "\"returns_twice\"";

      an_attribute a_sec = {}, a_used = {}, a_alias = {}, a_ifunc = {}, a_unrec = {};
      a_sec.kind = ak_section; a_sec.arguments = &arg_sec; a_sec.next = &a_used;
      a_used.kind = ak_used; a_used.next = nullptr;

      a_variable attr_oom_var = {};
      attr_oom_var.source_corresp.name = "w3_attr_oom_gv";
      attr_oom_var.type = &local_int_ty;
      attr_oom_var.is_thread_local = TRUE;
      attr_oom_var.source_corresp.attributes = &a_sec;
      for (int b = 0; b < 20; ++b) {
        state.local_vars.erase(&attr_oom_var);
        if (auto* old_g = state.module->getNamedGlobal("w3_attr_oom_gv")) old_g->eraseFromParent();
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_lower_global_variable(&attr_oom_var);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }

      a_alias.kind = ak_alias; a_alias.arguments = &arg_alias; a_alias.next = nullptr;
      attr_oom_var.source_corresp.name = "w3_alias_oom_gv";
      attr_oom_var.is_thread_local = FALSE;
      attr_oom_var.source_corresp.attributes = &a_alias;
      for (int b = 0; b < 10; ++b) {
        state.local_vars.erase(&attr_oom_var);
        if (auto* old_a = state.module->getNamedValue("w3_alias_oom_gv")) old_a->eraseFromParent();
        if (auto* old_d = state.module->getNamedValue("w3_alias_oom_gv.decl")) old_d->eraseFromParent();
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_lower_global_variable(&attr_oom_var);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }

      a_ifunc.kind = ak_ifunc; a_ifunc.arguments = &arg_ifunc; a_ifunc.next = &a_unrec;
      a_unrec.kind = ak_unrecognized; a_unrec.arguments = &arg_unrec; a_unrec.next = nullptr;
      a_param_type pt_null = {}, pt_nonnull = {};
      pt_null.type = nullptr; pt_null.next = &pt_nonnull;
      pt_nonnull.type = &local_int_ty; pt_nonnull.next = nullptr;
      a_routine_type_supplement proto_supp = {};
      proto_supp.param_type_list = &pt_null;
      a_type proto_rty = {};
      proto_rty.kind = tk_routine;
      proto_rty.variant.routine.extra_info = &proto_supp;
      a_routine proto_oom_r = {};
      proto_oom_r.source_corresp.name = "w3_ifunc_oom_fn";
      proto_oom_r.type = &proto_rty;
      proto_oom_r.source_corresp.attributes = &a_ifunc;
      g_decl_proto_mode = 2;
      g_decl_tqual = TQ_RESTRICT;
      for (int b = 0; b < 15; ++b) {
        if (auto* old_v = state.module->getNamedValue("w3_ifunc_oom_fn")) old_v->eraseFromParent();
        if (auto* old_d = state.module->getNamedValue("w3_ifunc_oom_fn.decl")) old_d->eraseFromParent();
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_lower_function_prototype(&proto_oom_r);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }
      g_decl_tqual = 0;
      g_decl_proto_mode = 0;

      // 3) Mangling OOM sweeps (Itanium & MSVC across compound parameter types, operators, templates, qualifiers)
      a_type m_int = {}, m_cplx = {}, m_ptr = {}, m_arr = {}, m_sub_fn = {}, m_ptm = {};
      m_int.kind = tk_integer; m_int.variant.integer.int_kind = ik_int; m_int.size = 4;
      m_cplx.kind = tk_complex; m_cplx.variant.float_kind = fk_double;
      m_ptr.kind = tk_pointer; m_ptr.variant.pointer.type = &m_int;
      m_arr.kind = tk_array; m_arr.variant.array.variant.number_of_elements = 4; m_arr.variant.array.element_type = &m_int;
      a_param_type m_sub_pt = {}; m_sub_pt.type = &m_int;
      a_routine_type_supplement m_sub_supp = {}; m_sub_supp.param_type_list = &m_sub_pt;
      m_sub_fn.kind = tk_routine; m_sub_fn.variant.routine.return_type = &m_int; m_sub_fn.variant.routine.extra_info = &m_sub_supp;
      m_ptm.kind = tk_ptr_to_member; m_ptm.variant.ptr_to_member.class_of_which_a_member = &m_int; m_ptm.variant.ptr_to_member.type = &m_int;

      a_param_type mp1 = {}, mp2 = {}, mp3 = {}, mp4 = {}, mp5 = {};
      mp1.type = &m_cplx; mp1.next = &mp2;
      mp2.type = &m_ptr; mp2.next = &mp3;
      mp3.type = &m_arr; mp3.next = &mp4;
      mp4.type = &m_sub_fn; mp4.next = &mp5;
      mp5.type = &m_ptm; mp5.next = nullptr;
      a_routine_type_supplement m_fn_supp = {};
      m_fn_supp.qualifiers = TQ_CONST | TQ_VOLATILE;
      m_fn_supp.param_type_list = &mp1;
      a_type m_fn_ty = {};
      m_fn_ty.kind = tk_routine;
      m_fn_ty.variant.routine.return_type = &m_ptr;
      m_fn_ty.variant.routine.extra_info = &m_fn_supp;

      a_type m_cls_ty = {}; m_cls_ty.kind = tk_class; m_cls_ty.source_corresp.name = "MCls";
      a_scope m_cls_sc = {}; m_cls_sc.kind = sck_class_struct_union; m_cls_sc.variant.assoc_type = &m_cls_ty;
      a_source_correspondence m_scp = {};
      m_scp.name = "tpl_fn<int, 4>";
      m_scp.parent_scope = &m_cls_sc;
      std::string mangled_out;
      g_decl_tqual = TQ_CONST | TQ_VOLATILE | TQ_RESTRICT;
      for (int b = 0; b < 25; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_mangle_unlowered_entity(&m_scp, &m_fn_ty, false, &mangled_out);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }
      m_scp.name = "plain_fn";
      for (int b = 0; b < 25; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_mangle_unlowered_entity(&m_scp, &m_fn_ty, false, &mangled_out);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }
      // MSVC function and data mangling OOM sweeps
      for (int b = 0; b < 20; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_mangle_unlowered_entity(&m_scp, &m_fn_ty, true, &mangled_out);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }
      for (int b = 0; b < 10; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_mangle_unlowered_entity(&m_scp, &m_ptr, true, &mangled_out);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }
      g_decl_tqual = 0;

      // 4) Constructor base C2 VTT calls (unseen base_ctor_fn, b_attrs empty/non-empty, invoke + cleanup) & OOM
      a_routine sub_dtor_r = {};
      sub_dtor_r.special_kind = sfk_destructor;
      sub_dtor_r.source_corresp.name = "_ZN4VSubD1Ev";
      sub_dtor_r.type = &fn_ty;
      a_routine op_del_r = {};
      op_del_r.source_corresp.name = "operator delete";
      op_del_r.type = &fn_ty;
      sub_dtor_r.next = &op_del_r;

      a_scope vsub_assoc = {};
      vsub_assoc.routines = &sub_dtor_r;
      a_class_type_supplement vsub_supp = {};
      vsub_supp.assoc_scope = &vsub_assoc;
      a_type vsub_cls = {};
      vsub_cls.kind = tk_class;
      vsub_cls.size = 8;
      vsub_cls.source_corresp.name = "VSub";
      vsub_cls.variant.class_struct_union.extra_info = &vsub_supp;

      a_type arr_vsub_ty = {};
      arr_vsub_ty.kind = tk_array;
      arr_vsub_ty.size = 16;
      arr_vsub_ty.variant.array.variant.number_of_elements = 2;
      arr_vsub_ty.variant.array.element_type = &vsub_cls;

      a_field vf_arr = {}, vf_single = {};
      vf_arr.type = &arr_vsub_ty; vf_arr.offset = 8; vf_arr.next = &vf_single;
      vf_single.type = &vsub_cls; vf_single.offset = 24; vf_single.next = nullptr;

      a_base_class vbc_virt = {}, vbc_dir = {};
      vbc_virt.is_virtual = TRUE; vbc_virt.type = &vsub_cls; vbc_virt.offset = 32; vbc_virt.next = nullptr;
      vbc_dir.is_virtual = FALSE; vbc_dir.type = &vsub_cls; vbc_dir.offset = 0; vbc_dir.next_direct = nullptr;

      a_class_type_supplement vder_supp = {};
      vder_supp.assoc_scope = &vsub_assoc;
      vder_supp.base_classes = &vbc_virt;
      vder_supp.direct_base_classes = &vbc_dir;

      a_type vder_cls = {};
      vder_cls.kind = tk_class;
      vder_cls.size = 40;
      vder_cls.source_corresp.name = "VDer";
      vder_cls.variant.class_struct_union.any_virtual_base_classes = TRUE;
      vder_cls.variant.class_struct_union.field_list = &vf_arr;
      vder_cls.variant.class_struct_union.extra_info = &vder_supp;

      a_type ptr_vder = {};
      ptr_vder.kind = tk_pointer;
      ptr_vder.variant.pointer.type = &vder_cls;

      a_variable vthis_p = {}, vvtt_p = {}, vextra_p = {};
      vthis_p.source_corresp.name = "this"; vthis_p.type = &ptr_vder; vthis_p.next = &vvtt_p;
      vvtt_p.source_corresp.name = "__vtt_parm"; vvtt_p.type = &ptr_vder; vvtt_p.next = &vextra_p;
      vextra_p.source_corresp.name = "extra"; vextra_p.type = &local_int_ty; vextra_p.next = nullptr;

      // Base C2 constructor inits: null c2_r, null type, fail proto, null proto, throwing 2-arg (with cleanup), noexcept 2-arg
      a_routine c2_notype = {}, c2_failp = {}, c2_nullp = {}, c2_throw = {}, c2_noexc = {};
      c2_notype.source_corresp.name = "unseen_c2_notype"; c2_notype.type = nullptr;
      c2_failp.source_corresp.name = "unseen_c2_failp"; c2_failp.type = &fn_ty;
      c2_nullp.source_corresp.name = "unseen_c2_nullp"; c2_nullp.type = &fn_ty;
      c2_throw.source_corresp.name = "unseen_c2_throw"; c2_throw.type = &fn_ty;
      c2_noexc.source_corresp.name = "unseen_c2_noexc"; c2_noexc.type = &fn_ty;

      a_dynamic_init dip_null_c2 = {}, dip_notype_c2 = {}, dip_throw_c2 = {}, dip_noexc_c2 = {}, dip_field = {};
      dip_null_c2.kind = dik_constructor; dip_null_c2.variant.constructor.ptr = nullptr;
      dip_notype_c2.kind = dik_constructor; dip_notype_c2.variant.constructor.ptr = &c2_notype;
      dip_throw_c2.kind = dik_constructor; dip_throw_c2.variant.constructor.ptr = &c2_throw; dip_throw_c2.destructor = &sub_dtor_r;
      dip_noexc_c2.kind = dik_constructor; dip_noexc_c2.variant.constructor.ptr = &c2_noexc; dip_noexc_c2.destructor = nullptr;
      dip_field.kind = dik_zero; dip_field.destructor = &sub_dtor_r;

      a_constructor_init ci1 = {}, ci2 = {}, ci3 = {}, ci4 = {}, ci5 = {};
      ci1.kind = cik_virtual_base_class; ci1.variant.base_class = &vbc_virt; ci1.initializer = &dip_null_c2; ci1.next = &ci2;
      ci2.kind = cik_direct_base_class; ci2.variant.base_class = &vbc_dir; ci2.initializer = &dip_notype_c2; ci2.next = &ci3;
      ci3.kind = cik_direct_base_class; ci3.variant.base_class = &vbc_dir; ci3.initializer = &dip_throw_c2; ci3.next = &ci4;
      ci4.kind = cik_field; ci4.variant.field = &vf_single; ci4.initializer = &dip_field; ci4.next = nullptr;

      a_scope vctor_sc = {};
      vctor_sc.variant.routine.parameters = &vthis_p;
      vctor_sc.variant.routine.constructor_inits = &ci1;
      g_decl_scope_for_rout = &vctor_sc;

      an_exception_specification ctor_exc_spec = {};
      ctor_exc_spec.throw_any = FALSE;
      ctor_exc_spec.is_noexcept = FALSE;
      a_routine_type_supplement ctor_rsupp = {};
      ctor_rsupp.exception_specification = &ctor_exc_spec;
      a_type ctor_rty = {};
      ctor_rty.kind = tk_routine;
      ctor_rty.variant.routine.return_type = &local_int_ty;
      ctor_rty.variant.routine.extra_info = &ctor_rsupp;

      a_scope vder_parent_sc = {};
      vder_parent_sc.kind = sck_class_struct_union;
      vder_parent_sc.variant.assoc_type = &vder_cls;

      a_routine vctor_r = {};
      vctor_r.special_kind = sfk_constructor;
      vctor_r.ctor_dtor_kind = cdk_complete;
      vctor_r.function_def_number = 1;
      vctor_r.type = &ctor_rty;
      vctor_r.source_corresp.parent_scope = &vder_parent_sc;

      // Fail proto & null proto during unseen base C2 creation
      g_decl_proto_mode = 7; // void(ptr, ptr)
      assert(recreate_test_fn_body(&vctor_r, "_ZN4VDerC1Ev_fp") == llvm_gen_be_error_t::ok);
      dip_notype_c2.variant.constructor.ptr = &c2_failp;
      g_decl_fail_proto = 1;
      assert(llvm_lower_function_body(&vctor_r) == llvm_gen_be_error_t::ok);
      g_decl_fail_proto = 0;

      assert(recreate_test_fn_body(&vctor_r, "_ZN4VDerC1Ev_np") == llvm_gen_be_error_t::ok);
      dip_notype_c2.variant.constructor.ptr = &c2_nullp;
      g_decl_null_proto = 1;
      assert(llvm_lower_function_body(&vctor_r) == llvm_gen_be_error_t::ok);
      g_decl_null_proto = 0;
      dip_notype_c2.variant.constructor.ptr = &c2_notype;

      // Non-empty b_attrs on unseen base C2 (sets NoUnwind -> CreateCall path)
      ci3.next = &ci5;
      ci5.kind = cik_direct_base_class; ci5.variant.base_class = &vbc_dir; ci5.initializer = &dip_noexc_c2; ci5.next = nullptr;
      g_decl_proto_nonempty_attrs = 1;
      assert(recreate_test_fn_body(&vctor_r, "_ZN4VDerC1Ev_attr") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&vctor_r) == llvm_gen_be_error_t::ok);
      g_decl_proto_nonempty_attrs = 0;
      ci3.next = &ci4;

      // OOM sweep across constructor body (covers VTT emit, base C2 invoke lpad, eh cleanup, field cleanup, exc_spec_lpad_bb)
      for (int b = 0; b < 80; ++b) {
        state.cleanup_stack.cleanups.clear();
        state.current_landing_pads.clear();
        if (auto* f_old = state.module->getFunction("unseen_c2_throw")) f_old->eraseFromParent();
        assert(recreate_test_fn_body(&vctor_r, "_ZN4VDerC1Ev_oom") == llvm_gen_be_error_t::ok);
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_lower_function_body(&vctor_r);
        g_llvm_be_oom_countdown = -1;
        state.cleanup_stack.cleanups.clear();
        state.current_landing_pads.clear();
        if (r == llvm_gen_be_error_t::ok) break;
      }

      // 5) Destructor cleanups OOM sweep + C2/D2 VTT variant synthesis (void & ptr return, parent_scope edge cases)
      a_scope vdtor_sc = {};
      vdtor_sc.variant.routine.parameters = &vthis_p;
      g_decl_scope_for_rout = &vdtor_sc;
      a_routine vdtor_r = {};
      vdtor_r.special_kind = sfk_destructor;
      vdtor_r.ctor_dtor_kind = cdk_complete;
      vdtor_r.function_def_number = 1;
      vdtor_r.type = &ctor_rty;
      vdtor_r.source_corresp.parent_scope = &vder_parent_sc;

      g_decl_proto_mode = 7; // void(ptr, ptr) -> d2_has_vtt == true
      for (int b = 0; b < 80; ++b) {
        state.cleanup_stack.cleanups.clear();
        state.current_landing_pads.clear();
        if (auto* f1 = state.module->getFunction("_ZN4VDerD1Ev")) f1->eraseFromParent();
        if (auto* f0 = state.module->getFunction("_ZN4VDerD0Ev")) f0->eraseFromParent();
        assert(recreate_test_fn_body(&vdtor_r, "_ZN4VDerD2Ev") == llvm_gen_be_error_t::ok);
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_lower_function_body(&vdtor_r);
        g_llvm_be_oom_countdown = -1;
        state.cleanup_stack.cleanups.clear();
        state.current_landing_pads.clear();
        if (r == llvm_gen_be_error_t::ok) break;
      }
      // Also sweep complete destructor D1E (is_complete_dtor == true -> covers vb_dtor OOM at L1825!)
      for (int b = 0; b < 80; ++b) {
        state.cleanup_stack.cleanups.clear();
        state.current_landing_pads.clear();
        assert(recreate_test_fn_body(&vdtor_r, "_ZN4VDerD1Ev_comp_oom") == llvm_gen_be_error_t::ok);
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_lower_function_body(&vdtor_r);
        g_llvm_be_oom_countdown = -1;
        state.cleanup_stack.cleanups.clear();
        state.current_landing_pads.clear();
        if (r == llvm_gen_be_error_t::ok) break;
      }
      // Same D2E with g_decl_fail_vtt = 2 (covers L2105 error return), ptr return (mode 8), and 1-arg D2E (mode 1)
      vdtor_r.type = &fn_ty;
      g_decl_proto_mode = 7;
      g_decl_fail_vtt = 2;
      assert(recreate_test_fn_body(&vdtor_r, "_ZN4VDerD2Ev_vtt_fail") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&vdtor_r) == llvm_gen_be_error_t::vtable_generation_failure);
      g_decl_fail_vtt = 0;

      g_decl_proto_mode = 8;
      if (auto* f1 = state.module->getFunction("_ZN4VDerD1Ev_ptr")) f1->eraseFromParent();
      if (auto* f0 = state.module->getFunction("_ZN4VDerD0Ev_ptr")) f0->eraseFromParent();
      assert(recreate_test_fn_body(&vdtor_r, "_ZN4VDerD2Ev_ptr") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&vdtor_r) == llvm_gen_be_error_t::ok);

      g_decl_proto_mode = 1;
      assert(recreate_test_fn_body(&vdtor_r, "_ZN4VDerD2Ev_1arg") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&vdtor_r) == llvm_gen_be_error_t::ok);

      // parent_scope->kind != sck_class_struct_union AND parent_scope->variant.assoc_type == nullptr
      a_scope file_parent_sc = {}; file_parent_sc.kind = sck_file;
      vdtor_r.source_corresp.parent_scope = &file_parent_sc;
      assert(recreate_test_fn_body(&vdtor_r, "_ZN4FileD2Ev") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&vdtor_r) == llvm_gen_be_error_t::ok);

      a_scope null_assoc_sc = {}; null_assoc_sc.kind = sck_class_struct_union; null_assoc_sc.variant.assoc_type = nullptr;
      vdtor_r.source_corresp.parent_scope = &null_assoc_sc;
      assert(recreate_test_fn_body(&vdtor_r, "_ZN8NullAsscD2Ev") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&vdtor_r) == llvm_gen_be_error_t::ok);
      vdtor_r.source_corresp.parent_scope = &vder_parent_sc;

      // C2E VTT variant synthesis across all parameter/scope combinations + OOM
      a_scope c2_sc = {};
      c2_sc.variant.routine.parameters = &vthis_p;
      g_decl_scope_for_rout = &c2_sc;
      a_routine vc2_r = {};
      vc2_r.special_kind = sfk_constructor;
      vc2_r.ctor_dtor_kind = cdk_subobject;
      vc2_r.function_def_number = 1;
      vc2_r.type = &fn_ty;
      vc2_r.source_corresp.parent_scope = &vder_parent_sc;

      // mode 9: void(ptr, ptr, i32) -> arg_size() == 3, pi = 2 < 3 loop runs, void return!
      g_decl_proto_mode = 9;
      for (int b = 0; b < 20; ++b) {
        if (auto* c1_old = state.module->getFunction("_ZN4VDerC1Ei")) c1_old->eraseFromParent();
        assert(recreate_test_fn_body(&vc2_r, "_ZN4VDerC2Ei") == llvm_gen_be_error_t::ok);
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_lower_function_body(&vc2_r);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }
      // mode 8: ptr(ptr, ptr) -> non-void C1 return!
      g_decl_proto_mode = 8;
      assert(recreate_test_fn_body(&vc2_r, "_ZN4VDerC2Ev_ptr") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&vc2_r) == llvm_gen_be_error_t::ok);

      // C2E guard permutations: arg_size() < 2, non-"vtt" name, null name, null p0->next, null p0
      g_decl_proto_mode = 1; // arg_size() == 1
      assert(recreate_test_fn_body(&vc2_r, "_ZN4VDerC2Ev_1a") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&vc2_r) == llvm_gen_be_error_t::ok);

      g_decl_proto_mode = 7;
      vvtt_p.source_corresp.name = "plain_arg";
      assert(recreate_test_fn_body(&vc2_r, "_ZN4VDerC2Ev_nv") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&vc2_r) == llvm_gen_be_error_t::ok);

      vvtt_p.source_corresp.name = nullptr;
      assert(recreate_test_fn_body(&vc2_r, "_ZN4VDerC2Ev_nn") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&vc2_r) == llvm_gen_be_error_t::ok);

      vthis_p.next = nullptr;
      assert(recreate_test_fn_body(&vc2_r, "_ZN4VDerC2Ev_np1") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&vc2_r) == llvm_gen_be_error_t::ok);

      c2_sc.variant.routine.parameters = nullptr;
      assert(recreate_test_fn_body(&vc2_r, "_ZN4VDerC2Ev_np0") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&vc2_r) == llvm_gen_be_error_t::ok);

      // 6) Coroutine cleanup BB fallthrough & ARM64 ctor/dtor ptr return fallback & compiler_generated aliases
      a_statement ret_void_s = {}; ret_void_s.kind = stmk_return;
      a_scope coro_sc = {};
      g_decl_scope_for_rout = &coro_sc;
      g_decl_proto_mode = 0;
      a_routine coro_fn_r = {};
      coro_fn_r.type = &fn_ty;
      coro_fn_r.function_def_number = 1;

      // (a) current_coro_cleanup_bb in a different function
      llvm::Function* other_coro_f = llvm::Function::Create(
          llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false),
          llvm::GlobalValue::ExternalLinkage, "other_coro_f", state.module.get());
      llvm::BasicBlock* other_coro_bb = llvm::BasicBlock::Create(*state.context, "other.cleanup", other_coro_f);
      llvm::ReturnInst::Create(*state.context, other_coro_bb);
      state.current_coro_cleanup_bb = other_coro_bb;
      assert(recreate_test_fn_body(&coro_fn_r, "coro_diff_parent_fn") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&coro_fn_r) == llvm_gen_be_error_t::ok);

      // (b) current_coro_cleanup_bb in same func, unterminated insert block -> CreateBr
      an_expr_node coro_dummy_e = {};
      a_statement coro_expr_s = {}, coro_blk_s = {};
      coro_expr_s.kind = stmk_expr; coro_expr_s.expr = &coro_dummy_e;
      coro_sc.assoc_block = &coro_expr_s;
      g_decl_coro_cleanup_in_expr = 1;
      assert(recreate_test_fn_body(&coro_fn_r, "coro_same_unterm_fn") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&coro_fn_r) == llvm_gen_be_error_t::ok);
      assert(state.current_coro_cleanup_bb == nullptr);

      // (c) current_coro_cleanup_bb in same func, ALREADY terminated insert block
      coro_expr_s.next = &ret_void_s;
      coro_blk_s.kind = stmk_block; coro_blk_s.variant.block.statements = &coro_expr_s;
      coro_sc.assoc_block = &coro_blk_s;
      assert(recreate_test_fn_body(&coro_fn_r, "coro_same_term_fn") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&coro_fn_r) == llvm_gen_be_error_t::ok);
      g_decl_coro_cleanup_in_expr = 0;
      coro_expr_s.next = nullptr;
      coro_sc.assoc_block = nullptr;

      // ARM64 ctor/dtor pointer return fallback: mode 5 = ptr(), mode 6 = ptr(i32), mode 4 = ptr(ptr, i32)
      a_routine ptr_ctor_r = {};
      ptr_ctor_r.special_kind = sfk_constructor;
      ptr_ctor_r.primary_ctor_or_dtor = &coro_fn_r; // skip variant synthesis
      ptr_ctor_r.function_def_number = 1;
      ptr_ctor_r.type = &fn_ty;
      g_decl_proto_mode = 5;
      assert(recreate_test_fn_body(&ptr_ctor_r, "ctor_ptr_0arg") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&ptr_ctor_r) == llvm_gen_be_error_t::ok);
      g_decl_proto_mode = 6;
      assert(recreate_test_fn_body(&ptr_ctor_r, "ctor_ptr_i32arg") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&ptr_ctor_r) == llvm_gen_be_error_t::ok);
      g_decl_proto_mode = 4;
      assert(recreate_test_fn_body(&ptr_ctor_r, "ctor_ptr_thisarg") == llvm_gen_be_error_t::ok);
      assert(llvm_lower_function_body(&ptr_ctor_r) == llvm_gen_be_error_t::ok);
      g_decl_proto_mode = 0;

      // lower_single_routine_definition: compiler_generated alias permutations + MSVC TLS alias OOM + scope OOM
      a_routine prim_undef0 = {}, prim_def = {}, prim_defnum = {};
      prim_undef0.source_corresp.name = "prim_undef0_fn"; prim_undef0.defined = FALSE; prim_undef0.function_def_number = 0;
      prim_def.source_corresp.name = "prim_def_fn"; prim_def.defined = TRUE; prim_def.function_def_number = 0;
      prim_defnum.source_corresp.name = "prim_defnum_fn"; prim_defnum.defined = FALSE; prim_defnum.function_def_number = 1;

      a_routine al1 = {}, al2 = {}, al3 = {}, tls_al = {};
      al1.source_corresp.name = "al1_fn"; al1.type = &fn_ty; al1.is_alias_entry = TRUE; al1.compiler_generated = TRUE; al1.primary_ctor_or_dtor = &prim_undef0; al1.next = &al2;
      al2.source_corresp.name = "al2_fn"; al2.type = &fn_ty; al2.is_alias_entry = TRUE; al2.compiler_generated = TRUE; al2.primary_ctor_or_dtor = &prim_def; al2.next = &al3;
      al3.source_corresp.name = "al3_fn"; al3.type = &fn_ty; al3.is_alias_entry = TRUE; al3.compiler_generated = TRUE; al3.primary_ctor_or_dtor = &prim_defnum; al3.next = &tls_al;
      tls_al.source_corresp.name = "msvc_tls_alias_fn"; tls_al.is_tls_init_alias = TRUE; tls_al.type = &fn_ty;

      a_scope w3_prim_sc = {};
      w3_prim_sc.routines = &al1;
      w3_prim_sc.types = &vder_cls;
      il_header.primary_scope = &w3_prim_sc;
      llvm_gen_be_debug_state_t w3_dbg_st = {};
      state.dbg_state = &w3_dbg_st;
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");

      for (int b = 0; b < 15; ++b) {
        if (auto* f_old = state.module->getFunction("msvc_tls_alias_fn")) f_old->eraseFromParent();
        if (auto* cb_old = state.module->getFunction("__tls_callback")) cb_old->eraseFromParent();
        if (auto* gv_old = state.module->getNamedGlobal("__tls_cb")) gv_old->eraseFromParent();
        assert(llvm_lower_function_prototype(&tls_al) == llvm_gen_be_error_t::ok);
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_lower_function_definitions();
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }
      for (int b = 0; b < 10; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_lower_global_variables();
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }
      for (int b = 0; b < 10; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_lower_function_declarations();
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }
      state.dbg_state = nullptr;

      // 7) Global ctors/dtors (Itanium & MSVC, TLS + inline + dtor + llvm.used) and C++20 module metadata OOM sweeps
      a_variable gd_tls_v = {}, gd_inl_v = {};
      gd_tls_v.source_corresp.name = "w3_gd_tls_v"; gd_tls_v.is_thread_local = TRUE; gd_tls_v.type = &local_int_ty;
      gd_inl_v.source_corresp.name = "w3_gd_inl_v"; gd_inl_v.is_inline = TRUE; gd_inl_v.type = &local_int_ty;
      a_dynamic_init gd_dip = {};
      gd_dip.kind = dik_zero;
      gd_dip.destructor = &sub_dtor_r;

      a_routine prio_ctor_r = {}, prio_dtor_r = {};
      prio_ctor_r.source_corresp.name = "w3_prio_ctor"; prio_ctor_r.is_finalization_routine = FALSE;
      prio_dtor_r.source_corresp.name = "w3_prio_dtor"; prio_dtor_r.is_finalization_routine = TRUE;
      prio_ctor_r.type = &fn_ty; prio_dtor_r.type = &fn_ty;
      prio_ctor_r.next = &prio_dtor_r;

      for (const char* tr : {"x86_64-unknown-linux-gnu", "x86_64-pc-windows-msvc"}) {
        for (int pass = 0; pass < 2; ++pass) {
          for (int b = 0; b < 50; ++b) {
            auto saved_m = std::move(state.module);
            state.module = std::make_unique<llvm::Module>("gd_oom_mod", *state.context);
            EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), tr);
            if (pass == 1) {
              llvm::Type* u_pty = llvm::PointerType::getUnqual(*state.context);
              llvm::ArrayType* u_aty = llvm::ArrayType::get(u_pty, 1);
              llvm::GlobalVariable* seed_gv = llvm::cast<llvm::GlobalVariable>(
                  state.module->getOrInsertGlobal("seed_used_gv", llvm::Type::getInt8Ty(*state.context)));
              seed_gv->setConstant(false);
              seed_gv->setLinkage(llvm::GlobalValue::InternalLinkage);
              seed_gv->setInitializer(llvm::ConstantInt::get(llvm::Type::getInt8Ty(*state.context), 0));
              assert(seed_gv != nullptr);
              llvm::GlobalVariable* used_gv = llvm::cast<llvm::GlobalVariable>(
                  state.module->getOrInsertGlobal("llvm.used", u_aty));
              used_gv->setConstant(false);
              used_gv->setLinkage(llvm::GlobalValue::AppendingLinkage);
              used_gv->setInitializer(llvm::ConstantArray::get(u_aty, {seed_gv}));
              assert(used_gv != nullptr);
            }
            llvm::GlobalVariable* tls_gv = llvm::cast<llvm::GlobalVariable>(
                state.module->getOrInsertGlobal("w3_gd_tls_v", llvm::Type::getInt32Ty(*state.context)));
            tls_gv->setConstant(false);
            tls_gv->setLinkage(llvm::GlobalValue::ExternalLinkage);
            tls_gv->setInitializer(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 0));
            assert(tls_gv != nullptr);
            llvm::GlobalVariable* inl_gv = llvm::cast<llvm::GlobalVariable>(
                state.module->getOrInsertGlobal("w3_gd_inl_v", llvm::Type::getInt32Ty(*state.context)));
            inl_gv->setConstant(false);
            inl_gv->setLinkage(llvm::GlobalValue::LinkOnceODRLinkage);
            inl_gv->setInitializer(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 0));
            assert(inl_gv != nullptr);
            llvm::Function::Create(
                llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false),
                llvm::GlobalValue::ExternalLinkage, "w3_prio_dtor", state.module.get());
            w3_prim_sc.routines = &prio_dtor_r;
            il_header.primary_scope = &w3_prim_sc;
            state.global_dynamic_inits.clear();
            state.global_dynamic_inits.push_back({&gd_tls_v, &gd_dip});
            state.global_dynamic_inits.push_back({&gd_inl_v, &gd_dip});

            g_llvm_be_oom_countdown = b;
            llvm_gen_be_error_t r = llvm_lower_global_ctors_and_dtors();
            g_llvm_be_oom_countdown = -1;
            state.module = std::move(saved_m);
            if (r == llvm_gen_be_error_t::ok) break;
          }
        }
      }
      state.global_dynamic_inits.clear();

      // C++20 module metadata OOM sweep
      a_module imp_mod_info = {}, tu_mod_info = {};
      imp_mod_info.kind = mk_unit; imp_mod_info.variant.unit.name = (char*)"imp.mod";
      a_module_import_decl imp_decl = {}; imp_decl.module_info = &imp_mod_info;
      tu_mod_info.kind = mk_unit; tu_mod_info.variant.unit.name = (char*)"my:mod";
      il_header.imported_modules = &imp_decl;
      il_header.primary_scope = nullptr;
      trans_unit_module = &tu_mod_info;
      for (int b = 0; b < 20; ++b) {
        auto saved_m = std::move(state.module);
        state.module = std::make_unique<llvm::Module>("cpp20_oom_mod", *state.context);
        EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_lower_module_metadata();
        g_llvm_be_oom_countdown = -1;
        state.module = std::move(saved_m);
        if (r == llvm_gen_be_error_t::ok) break;
      }
      il_header.imported_modules = nullptr;
      trans_unit_module = nullptr;

      // Unused static inline header routine filtering when il_header.function_def_table != nullptr
      a_function_def_descr dummy_fdef_tbl[2] = {};
      il_header.function_def_table = dummy_fdef_tbl;
      a_type si_ret_v = {}; si_ret_v.kind = tk_void;
      a_routine_type_supplement si_rts = {};
      a_type si_rty = {}; si_rty.kind = tk_routine;
      si_rty.variant.routine.return_type = &si_ret_v;
      si_rty.variant.routine.extra_info = &si_rts;
      a_routine r_non_inl = {}, r_ext_inl = {}, r_ref_si = {}, r_unused_si = {};
      r_non_inl.source_corresp.name = "si_non_inl";
      r_non_inl.type = &si_rty;
      r_non_inl.is_inline = FALSE;
      r_non_inl.next = &r_ext_inl;
      r_ext_inl.source_corresp.name = "si_ext_inl";
      r_ext_inl.type = &si_rty;
      r_ext_inl.is_inline = TRUE;
      r_ext_inl.storage_class = sc_extern;
      r_ext_inl.next = &r_ref_si;
      r_ref_si.source_corresp.name = "si_referenced_static";
      r_ref_si.type = &si_rty;
      r_ref_si.is_inline = TRUE;
      r_ref_si.storage_class = sc_static;
      r_ref_si.source_corresp.referenced = TRUE;
#if MAINTAIN_NEEDED_FLAGS
      r_ref_si.definition_needed = TRUE;
#endif
      r_ref_si.next = &r_unused_si;
      r_unused_si.source_corresp.name = "si_unused_static";
      r_unused_si.type = &si_rty;
      r_unused_si.is_inline = TRUE;
      r_unused_si.storage_class = sc_static;
      r_unused_si.source_corresp.referenced = FALSE;
      r_unused_si.next = nullptr;
      a_scope si_scope = {};
      si_scope.kind = sck_file;
      si_scope.routines = &r_non_inl;
      il_header.primary_scope = &si_scope;
      for (int b = 0; b < 12; ++b) {
        auto saved_m = std::move(state.module);
        state.module = std::make_unique<llvm::Module>("si_decl_mod", *state.context);
        EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_lower_function_declarations();
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) {
          assert(state.module->getFunction("si_unused_static") == nullptr);
          assert(state.module->getFunction("si_referenced_static") != nullptr);
          state.module = std::move(saved_m);
          break;
        }
        state.module = std::move(saved_m);
      }
      for (int b = 0; b < 12; ++b) {
        auto saved_m = std::move(state.module);
        state.module = std::make_unique<llvm::Module>("si_def_mod", *state.context);
        EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_lower_function_definitions();
        g_llvm_be_oom_countdown = -1;
        state.module = std::move(saved_m);
        if (r == llvm_gen_be_error_t::ok) break;
      }
      il_header.function_def_table = nullptr;
      il_header.primary_scope = nullptr;

      a_type del_void_ty = {};
      del_void_ty.kind = tk_void;
      a_type del_rty = {};
      del_rty.kind = tk_routine;
      del_rty.variant.routine.return_type = &del_void_ty;
      a_routine del_direct = {};
      del_direct.source_corresp.name = "deleted_direct_fn";
      del_direct.type = &del_rty;
      del_direct.is_deleted = TRUE;
      assert(llvm_lower_function_prototype(&del_direct) == llvm_gen_be_error_t::ok);
      assert(state.module->getFunction("deleted_direct_fn") == nullptr);

      a_routine del_alias = {};
      del_alias.source_corresp.name = "deleted_alias_fn";
      del_alias.type = &del_rty;
      del_alias.primary_ctor_or_dtor = &del_direct;
      assert(llvm_lower_function_prototype(&del_alias) == llvm_gen_be_error_t::ok);
      assert(state.module->getFunction("deleted_alias_fn") == nullptr);

      a_routine pure_undef = {}, pure_def = {};
      pure_undef.source_corresp.name = "pure_undef_fn";
      pure_undef.type = &del_rty;
      pure_undef.pure_virtual = TRUE;
      pure_undef.function_def_number = NULL_function_def_number;
      assert(llvm_lower_function_prototype(&pure_undef) == llvm_gen_be_error_t::ok);
      assert(state.module->getFunction("pure_undef_fn") == nullptr);

      pure_def.source_corresp.name = "pure_defined_dtor_fn";
      pure_def.type = &del_rty;
      pure_def.pure_virtual = TRUE;
      pure_def.function_def_number = 1;
      assert(llvm_lower_function_prototype(&pure_def) == llvm_gen_be_error_t::ok);
      assert(state.module->getFunction("pure_defined_dtor_fn") != nullptr);

      a_routine comp_gen_fn = {};
      comp_gen_fn.source_corresp.name = "comp_gen_dtor_fn";
      comp_gen_fn.type = &del_rty;
      comp_gen_fn.function_def_number = 1;
      comp_gen_fn.compiler_generated = TRUE;
      llvm::Function::Create(
          llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false),
          llvm::GlobalValue::ExternalLinkage, "comp_gen_dtor_fn", state.module.get());
      assert(llvm_lower_function_prototype(&comp_gen_fn) == llvm_gen_be_error_t::ok);
      assert(state.module->getFunction("comp_gen_dtor_fn")->hasLinkOnceODRLinkage());

      a_routine zz_local_fn = {};
      zz_local_fn.source_corresp.name = "_ZZ1fvEN1LclEv";
      zz_local_fn.type = &del_rty;
      zz_local_fn.function_def_number = 1;
      llvm::Function::Create(
          llvm::FunctionType::get(llvm::Type::getInt32Ty(*state.context), false),
          llvm::GlobalValue::ExternalLinkage, "_ZZ1fvEN1LclEv", state.module.get());
      assert(llvm_lower_function_prototype(&zz_local_fn) == llvm_gen_be_error_t::ok);
      assert(state.module->getFunction("_ZZ1fvEN1LclEv")->hasLinkOnceODRLinkage());

      a_type zz_i32 = {};
      zz_i32.kind = tk_integer;
      zz_i32.size = 4;
      zz_i32.variant.integer.int_kind = ik_int;
      a_variable zz_var = {};
      zz_var.source_corresp.name = "_ZZ1fvE5table";
      zz_var.type = &zz_i32;
      zz_var.init_kind = initk_zero;
      assert(llvm_lower_global_variable(&zz_var) == llvm_gen_be_error_t::ok);
      assert(state.module->getNamedGlobal("_ZZ1fvE5table")->hasLinkOnceODRLinkage());

      // Cover remaining Stage-2 branches in llvm_gen_be_decl.cpp
      {
        // 1) var->is_parameter, var->is_pack, var->is_handler_param, and type_is_nonreal(var->type)
        a_variable v_skip = {};
        v_skip.source_corresp.name = "decl_skip_var";
        v_skip.type = &zz_i32;
        v_skip.is_parameter = TRUE;
        assert(llvm_lower_global_variable(&v_skip) == llvm_gen_be_error_t::ok);
        v_skip.is_parameter = FALSE;
        v_skip.is_pack = TRUE;
        assert(llvm_lower_global_variable(&v_skip) == llvm_gen_be_error_t::ok);
        v_skip.is_pack = FALSE;
        v_skip.is_handler_param = TRUE;
        assert(llvm_lower_global_variable(&v_skip) == llvm_gen_be_error_t::ok);
        v_skip.is_handler_param = FALSE;
        g_decl_type_nonreal = 1;
        assert(llvm_lower_global_variable(&v_skip) == llvm_gen_be_error_t::ok);
        g_decl_type_nonreal = 0;

        // 2) var->comdat_group != nullptr (when !var->is_inline)
        char fake_comdat[] = "grp";
        a_variable v_cdat = {};
        v_cdat.source_corresp.name = "decl_comdat_grp_var";
        v_cdat.type = &zz_i32;
        v_cdat.comdat_group = fake_comdat;
        v_cdat.init_kind = initk_zero;
        assert(llvm_lower_global_variable(&v_cdat) == llvm_gen_be_error_t::ok);

        // 3) is_fn_local_static across primary_scope sck_function/sck_block & parent_scope sck_function/sck_block/sck_file + collision + OOM
        a_scope saved_pri = {};
        a_scope* prev_pri = il_header.primary_scope;
        a_scope pri_fn_sc = {}, pri_blk_sc = {}, pri_file_sc = {}, par_fn_sc = {}, par_blk_sc = {}, par_ns_sc = {};
        pri_fn_sc.kind = sck_function;
        pri_blk_sc.kind = sck_block;
        pri_file_sc.kind = sck_file;
        par_fn_sc.kind = sck_function;
        par_blk_sc.kind = sck_block;
        a_namespace par_ns_ent = {};
        par_ns_ent.source_corresp.name = "pns";
        par_ns_ent.source_corresp.parent_scope = &pri_file_sc;
        par_ns_sc.kind = sck_namespace;
        par_ns_sc.variant.assoc_namespace = &par_ns_ent;

        a_variable v_fls1 = {}, v_fls2 = {}, v_fls3 = {}, v_fls4 = {}, v_fls5 = {};
        v_fls1.source_corresp.name = "fls_collision_sym";
        v_fls1.storage_class = sc_static;
        v_fls1.type = &zz_i32;
        il_header.primary_scope = &pri_fn_sc;
        assert(llvm_lower_global_variable(&v_fls1) == llvm_gen_be_error_t::ok); // creates fls_collision_sym

        // Second distinct static var with same name -> collision true! Test OOM at L966 then ok
        v_fls2.source_corresp.name = "fls_collision_sym";
        v_fls2.storage_class = sc_static;
        v_fls2.type = &zz_i32;
        il_header.primary_scope = &pri_blk_sc;
        g_llvm_be_oom_countdown = 1; // step 0 in llvm_get_mangled_variable_name, step 1 in llvm_create_global_variable
        assert(llvm_lower_global_variable(&v_fls2) == llvm_gen_be_error_t::out_of_memory);
        g_llvm_be_oom_countdown = -1;
        assert(llvm_lower_global_variable(&v_fls2) == llvm_gen_be_error_t::ok);

        il_header.primary_scope = &pri_file_sc;
        v_fls3.source_corresp.name = "fls_par_fn_sym";
        v_fls3.storage_class = sc_static;
        v_fls3.type = &zz_i32;
        v_fls3.source_corresp.parent_scope = &par_fn_sc;
        assert(llvm_lower_global_variable(&v_fls3) == llvm_gen_be_error_t::ok);

        v_fls4.source_corresp.name = "fls_par_blk_sym";
        v_fls4.storage_class = sc_static;
        v_fls4.type = &zz_i32;
        v_fls4.source_corresp.parent_scope = &par_blk_sc;
        assert(llvm_lower_global_variable(&v_fls4) == llvm_gen_be_error_t::ok);

        v_fls5.source_corresp.name = "fls_par_ns_sym";
        v_fls5.storage_class = sc_static;
        v_fls5.type = &zz_i32;
        v_fls5.source_corresp.parent_scope = &par_ns_sc;
        assert(llvm_lower_global_variable(&v_fls5) == llvm_gen_be_error_t::ok);
        il_header.primary_scope = prev_pri;

        // 4) Mach-O explicit_default_vis.count(gvar) != 0 and explicit_default_vis.count(func) != 0 (L1072 F & L1276 F)
        llvm::Triple saved_tr = llvm::Triple(state.module->getTargetTriple());
        EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "arm64-apple-macosx14.0.0");
        a_variable v_macho_exp = {};
        v_macho_exp.source_corresp.name = "macho_exp_default_gv";
        v_macho_exp.type = &zz_i32;
        v_macho_exp.is_inline = TRUE;
        v_macho_exp.init_kind = initk_zero;
        llvm::GlobalVariable* pre_macho_gv = llvm::cast<llvm::GlobalVariable>(
            state.module->getOrInsertGlobal("macho_exp_default_gv", llvm::Type::getInt32Ty(*state.context)));
        state.explicit_default_vis.insert(pre_macho_gv);
        assert(llvm_lower_global_variable(&v_macho_exp) == llvm_gen_be_error_t::ok);

        a_routine r_macho_exp = {};
        r_macho_exp.source_corresp.name = "macho_exp_default_fn";
        r_macho_exp.type = &del_rty;
        r_macho_exp.function_def_number = 1;
        r_macho_exp.use_comdat = TRUE; // also covers L1251 routine->use_comdat True!
        llvm::Function* pre_macho_fn = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false),
            llvm::GlobalValue::ExternalLinkage, "macho_exp_default_fn", state.module.get());
        state.explicit_default_vis.insert(pre_macho_fn);
        assert(llvm_lower_function_prototype(&r_macho_exp) == llvm_gen_be_error_t::ok);
        state.explicit_default_vis.clear();
        EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), saved_tr);

        // 5) routine->type == nullptr & type_is_nonreal(routine->type) in llvm_lower_function_prototype (L1211)
        a_routine r_null_ty = {};
        r_null_ty.source_corresp.name = "rout_with_null_type";
        r_null_ty.type = nullptr;
        g_decl_fail_proto = 1;
        assert(llvm_lower_function_prototype(&r_null_ty) == llvm_gen_be_error_t::abi_classification_failed);
        g_decl_fail_proto = 0;

        a_routine r_nonreal = {};
        r_nonreal.source_corresp.name = "rout_with_nonreal_type";
        r_nonreal.type = &del_rty;
        g_decl_type_nonreal = 1;
        assert(llvm_lower_function_prototype(&r_nonreal) == llvm_gen_be_error_t::ok);
        g_decl_type_nonreal = 0;

        // 6) Scope walkers: local sck_function/sck_block nonstatic skip, extern var pruning, edg_lowered_vtables,
        //    prototype/unspecialized template classes, nonreal class, sck_func_prototype subscopes,
        //    prune_unref routines, scope_orphaned_list_headers, and demand_defined_routines
        a_function_def_descr mock_fdd[4] = {};
        a_scope walk_pri = {}, walk_ns_sc = {}, walk_fn_sc = {}, walk_blk_sc = {}, walk_fproto_sc = {}, walk_exp_proto_sc = {};
        walk_pri.kind = sck_file;
        walk_ns_sc.kind = sck_namespace;
        walk_fn_sc.kind = sck_function;
        walk_blk_sc.kind = sck_block;
        walk_fproto_sc.kind = sck_func_prototype;
        walk_exp_proto_sc.kind = sck_block;
        walk_exp_proto_sc.is_expansion_prototype = TRUE;
        walk_pri.scopes = &walk_fn_sc;
        walk_fn_sc.next = &walk_blk_sc;
        walk_blk_sc.next = &walk_fproto_sc;
        walk_fproto_sc.next = &walk_exp_proto_sc;

        a_namespace walk_ns = {};
        walk_ns.source_corresp.name = "wns";
        walk_ns.variant.assoc_scope = &walk_ns_sc;
        walk_pri.namespaces = &walk_ns;

        a_variable wv_auto = {}, wv_static = {}, wv_ext_none = {}, wv_ext_zero = {}, wv_binding = {};
        wv_auto.source_corresp.name = "wv_auto_local"; wv_auto.storage_class = sc_auto; wv_auto.type = &zz_i32; wv_auto.next = &wv_static;
        wv_static.source_corresp.name = "wv_static_local"; wv_static.storage_class = sc_static; wv_static.type = &zz_i32;
        walk_fn_sc.variables = &wv_auto;

        wv_ext_none.source_corresp.name = "wv_ext_none"; wv_ext_none.storage_class = sc_extern; wv_ext_none.init_kind = initk_none; wv_ext_none.type = &zz_i32; wv_ext_none.next = &wv_ext_zero;
        wv_ext_zero.source_corresp.name = "wv_ext_zero"; wv_ext_zero.storage_class = sc_extern; wv_ext_zero.init_kind = initk_zero; wv_ext_zero.type = &zz_i32; wv_ext_zero.next = &wv_binding;
        wv_binding.source_corresp.name = "wv_binding"; wv_binding.init_kind = initk_binding; wv_binding.type = &zz_i32;
        walk_ns_sc.variables = &wv_ext_none;

        a_class_type_supplement cts_proto = {}, cts_tpl_unspec = {}, cts_tpl_spec = {}, cts_ok = {};
        a_scope csc_ok = {}; csc_ok.kind = sck_class_struct_union;
        cts_ok.assoc_scope = &csc_ok;
        a_type ct_nonreal = {}, ct_proto = {}, ct_tpl_unspec = {}, ct_tpl_spec = {}, ct_ok = {};
        ct_nonreal.kind = tk_class; ct_nonreal.variant.class_struct_union.is_nonreal_class = TRUE; ct_nonreal.next = &ct_proto;
        ct_proto.kind = tk_class; ct_proto.variant.class_struct_union.is_prototype_instantiation = TRUE; ct_proto.variant.class_struct_union.extra_info = &cts_proto; ct_proto.next = &ct_tpl_unspec;
        ct_tpl_unspec.kind = tk_class; ct_tpl_unspec.variant.class_struct_union.is_template_class = TRUE; ct_tpl_unspec.variant.class_struct_union.is_specialized = FALSE; ct_tpl_unspec.variant.class_struct_union.extra_info = &cts_tpl_unspec; ct_tpl_unspec.next = &ct_tpl_spec;
        ct_tpl_spec.kind = tk_class; ct_tpl_spec.variant.class_struct_union.is_template_class = TRUE; ct_tpl_spec.variant.class_struct_union.is_specialized = TRUE; ct_tpl_spec.variant.class_struct_union.extra_info = &cts_tpl_spec; ct_tpl_spec.next = &ct_ok;
        ct_ok.kind = tk_class; ct_ok.source_corresp.name = "WalkPolyClass"; ct_ok.variant.class_struct_union.any_virtual_functions = TRUE; ct_ok.variant.class_struct_union.extra_info = &cts_ok;
        walk_pri.types = &ct_nonreal;

        a_routine wr_unref = {}, wr_ref = {}, wr_def = {};
        wr_unref.source_corresp.name = "wr_ns_unref"; wr_unref.type = &del_rty; wr_unref.function_def_number = NULL_function_def_number; wr_unref.source_corresp.referenced = FALSE; wr_unref.next = &wr_ref;
        wr_ref.source_corresp.name = "wr_ns_ref"; wr_ref.type = &del_rty; wr_ref.function_def_number = NULL_function_def_number; wr_ref.source_corresp.referenced = TRUE; wr_ref.next = &wr_def;
        wr_def.source_corresp.name = "wr_ns_def"; wr_def.type = &del_rty; wr_def.function_def_number = 1;
        walk_ns_sc.routines = &wr_unref;

        // Orphaned scope list headers (all 3 ignore_routine_in_back_end arms: prototype, deduction_guide, consteval, plus valid & null)
        a_routine orph_ign_proto = {}, orph_ign_dg = {}, orph_ign_ce = {}, orph_ok_r = {};
        orph_ign_proto.is_prototype_instantiation = TRUE;
        orph_ign_dg.special_kind = static_cast<a_special_function_kind>(sfk_deduction_guide);
        orph_ign_ce.is_consteval = TRUE;
        a_type orph_t_nosc = {}, orph_t_sc = {};
        orph_t_nosc.kind = tk_integer; orph_t_nosc.next = &orph_t_sc;
        a_scope orph_csc = {}; orph_csc.kind = sck_class_struct_union;
        a_class_type_supplement orph_cts = {}; orph_cts.assoc_scope = &orph_csc;
        orph_t_sc.kind = tk_class; orph_t_sc.variant.class_struct_union.extra_info = &orph_cts;
        a_scope_orphaned_list_header solh_exp = {}, solh_p = {}, solh_d = {}, solh1 = {}, solh2 = {}, solh3 = {};
        solh_exp.in_expansion_prototype = TRUE; solh_exp.assoc_routine = &orph_ok_r; solh_exp.next = &solh_p;
        solh_p.assoc_routine = &orph_ign_proto; solh_p.next = &solh_d;
        solh_d.assoc_routine = &orph_ign_dg; solh_d.next = &solh1;
        solh1.assoc_routine = &orph_ign_ce; solh1.next = &solh2;
        solh2.assoc_routine = &orph_ok_r; solh2.orphaned_types = &orph_t_nosc; solh2.next = &solh3;
        solh3.assoc_routine = nullptr; solh3.orphaned_types = nullptr;

        // Also put routines on walk_pri (`is_primary == true` for L1406:24 F, plus all 3 ignore_routine_in_back_end arms at L1423!)
        a_routine pri_r_proto = {}, pri_r_dg = {}, pri_r_ce = {}, pri_r_ok = {};
        pri_r_proto.source_corresp.name = "pri_r_proto_fn"; pri_r_proto.type = &del_rty; pri_r_proto.is_prototype_instantiation = TRUE; pri_r_proto.next = &pri_r_dg;
        pri_r_dg.source_corresp.name = "pri_r_dg_fn"; pri_r_dg.type = &del_rty; pri_r_dg.special_kind = static_cast<a_special_function_kind>(sfk_deduction_guide); pri_r_dg.next = &pri_r_ce;
        pri_r_ce.source_corresp.name = "pri_r_ce_fn"; pri_r_ce.type = &del_rty; pri_r_ce.is_consteval = TRUE; pri_r_ce.next = &pri_r_ok;
        pri_r_ok.source_corresp.name = "pri_r_ok_fn"; pri_r_ok.type = &del_rty; pri_r_ok.function_def_number = NULL_function_def_number;
        walk_pri.routines = &pri_r_proto;

        il_header.primary_scope = &walk_pri;
        il_header.function_def_table = mock_fdd;
        il_header.scope_orphaned_list_headers = &solh_exp;

        state.visited_scopes.clear();
        assert(llvm_lower_global_variables() == llvm_gen_be_error_t::ok);
        state.visited_scopes.clear();
        g_decl_il_lowering = 1; // edg_lowered_vtables == true (L1149 F)
        assert(llvm_lower_global_variables() == llvm_gen_be_error_t::ok);
        g_decl_il_lowering = 0;

        state.visited_scopes.clear();
        g_decl_type_nonreal = 1; // !ignore_type_in_back_end(tp) False (L1098 F in get_valid_class_assoc_scope!)
        assert(llvm_lower_global_variables() == llvm_gen_be_error_t::ok);
        assert(llvm_lower_function_declarations() == llvm_gen_be_error_t::ok);
        g_decl_type_nonreal = 0;

        // Also test il_header.function_def_table == nullptr on walk_ns_sc (L1129 F & L1406 F)
        state.visited_scopes.clear();
        il_header.function_def_table = nullptr;
        assert(llvm_lower_global_variables() == llvm_gen_be_error_t::ok);
        assert(llvm_lower_function_declarations() == llvm_gen_be_error_t::ok);

        il_header.function_def_table = mock_fdd;
        g_decl_il_lowering = 1; // prune_unref == true in lower_routine_decls_in_scope!
        assert(llvm_lower_function_declarations() == llvm_gen_be_error_t::ok);
        // Also test il_header.function_def_table == nullptr while prune_unref == true (L1406:39 F)
        il_header.function_def_table = nullptr;
        assert(llvm_lower_function_declarations() == llvm_gen_be_error_t::ok);
        il_header.function_def_table = mock_fdd;
        g_decl_il_lowering = 0;

        // 7) demand_defined_routines in llvm_lower_function_definitions (L3045-L3052)
        //    plus local_static_variable_inits & value_returned_as_parameter in llvm_lower_function_body (L2470-L2508)
        an_expr_node dem_seed_expr = {};
        dem_seed_expr.kind = enk_constant;
        a_statement dem_body_s = {};
        dem_body_s.kind = stmk_expr;
        dem_body_s.expr = &dem_seed_expr;
        a_variable dem_lsv_ok = {}, dem_lsv_other = {};
        dem_lsv_ok.init_kind = initk_function_local;
        dem_lsv_other.init_kind = initk_zero;
        a_local_static_variable_init dem_lsi1 = {}, dem_lsi2 = {}, dem_lsi3 = {};
        dem_lsi1.variable = nullptr; dem_lsi1.next = &dem_lsi2;
        dem_lsi2.variable = &dem_lsv_other; dem_lsi2.next = &dem_lsi3;
        dem_lsi3.variable = &dem_lsv_ok; dem_lsi3.init_kind = initk_zero;

        a_scope dem_fn_scope = {};
        dem_fn_scope.kind = sck_function;
        dem_fn_scope.assoc_block = &dem_body_s;
        dem_fn_scope.local_static_variable_inits = &dem_lsi1;

        a_routine_type_supplement dem_rsupp = {};
        dem_rsupp.value_returned_as_parameter = TRUE;
        a_type dem_rty = {};
        dem_rty.kind = tk_routine;
        dem_rty.variant.routine.return_type = &zz_i32;
        dem_rty.variant.routine.extra_info = &dem_rsupp;

        a_routine dem_unnamed = {}, dem_nonempty = {}, dem_fresh = {}, dem_decl_only = {};
        dem_unnamed.source_corresp.name = nullptr;
        dem_nonempty.source_corresp.name = "dem_already_defined_fn";
        dem_nonempty.type = &dem_rty;
        dem_nonempty.function_def_number = 1;
        llvm::Function* pre_dem_fn = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false),
            llvm::GlobalValue::ExternalLinkage, "dem_already_defined_fn", state.module.get());
        llvm::ReturnInst::Create(*state.context, llvm::BasicBlock::Create(*state.context, "entry", pre_dem_fn));

        dem_decl_only.source_corresp.name = "dem_existing_empty_decl_fn";
        dem_decl_only.type = &dem_rty;
        dem_decl_only.function_def_number = 1;
        llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false),
            llvm::GlobalValue::ExternalLinkage, "dem_existing_empty_decl_fn", state.module.get());

        dem_fresh.source_corresp.name = "dem_fresh_fn";
        dem_fresh.type = &dem_rty;
        dem_fresh.function_def_number = 1;

        // Attach orphan routines to solh2 so L1472 & L3040 also run + sweep OOM across declarations & definitions!
        a_routine orph_sub_r = {};
        orph_sub_r.source_corresp.name = "orph_sub_fn";
        orph_sub_r.source_corresp.referenced = TRUE;
        orph_sub_r.type = &del_rty;
        orph_sub_r.function_def_number = 1;
        orph_csc.routines = &orph_sub_r;

        // Put a non-defined child routine inside dem_fn_scope (so lower_routine_decls_in_scope(fscope) takes an OOM step at L1428 without recursing!)
        // and a nested namespace inside dem_fn_scope with a defined routine whose fscope is nullptr? Wait: g_decl_scope_for_rout applies to all routines unless checked!
        // Let's put `fscope_decl_r` (function_def_number == NULL_function_def_number -> takes OOM step in decls at L1428 without recursing!)
        // AND `fscope_ns` -> `fscope_ns_sc` with `fscope_def_r`? Even simpler: in `lower_routine_defs_in_scope(fscope)`, `fscope->types` calls `extract_lowerable_class_scope` which calls `llvm_gen_be_step_oom_budget()`!
        a_type fscope_dummy_t = {};
        fscope_dummy_t.kind = tk_integer;
        dem_fn_scope.types = &fscope_dummy_t; // takes OOM step in BOTH lower_routine_decls_in_scope(fscope) AND lower_routine_defs_in_scope(fscope) without infinite recursion!

        g_decl_scope_for_rout = &dem_fn_scope;
        for (int b = 0; b < 55; ++b) {
          g_llvm_be_oom_countdown = b;
          llvm_gen_be_error_t r = llvm_lower_function_declarations();
          g_llvm_be_oom_countdown = -1;
          if (r == llvm_gen_be_error_t::ok) break;
        }
        for (int b = 0; b < 250; ++b) {
          if (auto* f1 = state.module->getFunction("dem_fresh_fn")) f1->eraseFromParent();
          if (auto* f2 = state.module->getFunction("dem_existing_empty_decl_fn")) f2->deleteBody();
          if (auto* f3 = state.module->getFunction("wr_ns_def")) f3->deleteBody();
          if (auto* f4 = state.module->getFunction("orph_sub_fn")) f4->deleteBody();
          dem_lsv_ok.init_kind = initk_function_local;
          g_decl_demand_seed = {nullptr, &dem_unnamed, &dem_nonempty, &dem_decl_only, &dem_fresh};
          g_llvm_be_oom_countdown = b;
          llvm_gen_be_error_t r = llvm_lower_function_definitions();
          g_llvm_be_oom_countdown = -1;
          g_decl_demand_seed.clear();
          if (r == llvm_gen_be_error_t::ok) break;
        }
        // Also cover prototype failure on demand_defined_routines at L3050!
        if (auto* f1 = state.module->getFunction("dem_fresh_fn")) f1->eraseFromParent();
        if (auto* f3 = state.module->getFunction("wr_ns_def")) f3->deleteBody();
        if (auto* f4 = state.module->getFunction("orph_sub_fn")) f4->deleteBody();
        g_decl_demand_seed = {&dem_fresh};
        g_decl_fail_proto_after_seed = 1;
        assert(llvm_lower_function_definitions() == llvm_gen_be_error_t::abi_classification_failed);
        g_decl_fail_proto_after_seed = 0;
        g_decl_fail_proto = 0;
        g_decl_demand_seed.clear();
        g_decl_scope_for_rout = nullptr;

        il_header.scope_orphaned_list_headers = nullptr;
        il_header.function_def_table = nullptr;
        il_header.primary_scope = prev_pri;
      }

#if GNU_EXTENSIONS_ALLOWED && GNU_VISIBILITY_ATTRIBUTE_ALLOWED
      std::string orig_tr = state.module->getTargetTriple();
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "arm64-apple-darwin24.0.0");
      an_ELF_visibility_kind ev_kinds[] = {evk_default, evk_hidden, evk_internal, evk_protected};
      const char* ev_names[] = {"ev_def_fn", "ev_hid_fn", "ev_int_fn", "ev_prot_fn"};
      for (int ei = 0; ei < 4; ++ei) {
        a_routine ev_fn = {};
        ev_fn.source_corresp.name = ev_names[ei];
        ev_fn.type = &del_rty;
        ev_fn.ELF_visibility = ev_kinds[ei];
        assert(llvm_lower_function_prototype(&ev_fn) == llvm_gen_be_error_t::ok);
      }
      a_variable ev_gv = {};
      ev_gv.source_corresp.name = "ev_def_gv";
      ev_gv.type = &zz_i32;
      ev_gv.init_kind = initk_zero;
      ev_gv.ELF_visibility = evk_default;
      assert(llvm_lower_global_variable(&ev_gv) == llvm_gen_be_error_t::ok);
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), orig_tr);
#endif
      a_routine spec_tmpl_fn = {};
      spec_tmpl_fn.source_corresp.name = "spec_tmpl_fn";
      spec_tmpl_fn.type = &del_rty;
      spec_tmpl_fn.function_def_number = 1;
      spec_tmpl_fn.is_template_function = TRUE;
      spec_tmpl_fn.is_specialized = TRUE;
      assert(llvm_lower_function_prototype(&spec_tmpl_fn) == llvm_gen_be_error_t::ok);
      assert(state.module->getFunction("spec_tmpl_fn")->hasExternalLinkage());

#if MAINTAIN_NEEDED_FLAGS
      {
        a_scope* saved_pri_mnf = il_header.primary_scope;
        a_function_def_descr* saved_fdt_mnf = il_header.function_def_table;
        a_function_def_descr mnf_fdd[2] = {};
        a_statement mnf_ret = {};
        mnf_ret.kind = stmk_return;
        a_scope mnf_fn_sc = {};
        mnf_fn_sc.kind = sck_function;
        mnf_fn_sc.assoc_block = &mnf_ret;
        g_decl_scope_for_rout = &mnf_fn_sc;

        a_routine mnf_def_needed = {}, mnf_keep_il = {};
        mnf_def_needed.source_corresp.name = "mnf_def_needed_fn";
        mnf_def_needed.type = &del_rty;
        mnf_def_needed.function_def_number = 1;
        mnf_def_needed.definition_needed = TRUE;
        mnf_def_needed.next = &mnf_keep_il;

        mnf_keep_il.source_corresp.name = "mnf_keep_il_fn";
        mnf_keep_il.type = &del_rty;
        mnf_keep_il.function_def_number = 1;
        mnf_keep_il.keep_definition_in_il = TRUE;

        a_scope mnf_child_sc = {}, mnf_root_sc = {};
        mnf_child_sc.kind = sck_namespace;
        mnf_child_sc.routines = &mnf_def_needed;
        mnf_root_sc.kind = sck_file;
        mnf_root_sc.scopes = &mnf_child_sc;

        il_header.primary_scope = &mnf_root_sc;
        il_header.function_def_table = mnf_fdd;
        assert(llvm_lower_function_declarations() == llvm_gen_be_error_t::ok);
        assert(llvm_lower_function_definitions() == llvm_gen_be_error_t::ok);

        il_header.function_def_table = nullptr;
        if (auto* f1 = state.module->getFunction("mnf_def_needed_fn")) f1->deleteBody();
        if (auto* f2 = state.module->getFunction("mnf_keep_il_fn")) f2->deleteBody();
        assert(llvm_lower_function_definitions() == llvm_gen_be_error_t::ok);

        g_decl_scope_for_rout = nullptr;
        il_header.primary_scope = saved_pri_mnf;
        il_header.function_def_table = saved_fdt_mnf;
      }
#endif

#if GNU_EXTENSIONS_ALLOWED && GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
      {
        a_scope* saved_pri_ip = il_header.primary_scope;
        std::string saved_tr_ip = state.module->getTargetTriple();
        a_gnu_routine_supplement ip_gnu_c = {}, ip_gnu_d = {};
        ip_gnu_c.ctor_priority = 200;
        ip_gnu_d.dtor_priority = 201;

        a_routine ip_ctor = {}, ip_dtor = {};
        ip_ctor.source_corresp.name = "ip_gnu_ctor_fn";
        ip_ctor.type = &del_rty;
        ip_ctor.has_ctor_priority = TRUE;
        ip_ctor.gnu_extra_info = &ip_gnu_c;
#if DO_IL_LOWERING
        ip_ctor.init_priority = 300;
#endif
        ip_ctor.next = &ip_dtor;

        ip_dtor.source_corresp.name = "ip_gnu_dtor_fn";
        ip_dtor.type = &del_rty;
        ip_dtor.has_dtor_priority = TRUE;
        ip_dtor.gnu_extra_info = &ip_gnu_d;
#if DO_IL_LOWERING
        ip_dtor.init_priority = 301;
#endif

        llvm::FunctionType* void_fty = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
        llvm::Function::Create(void_fty, llvm::GlobalValue::InternalLinkage, "ip_gnu_ctor_fn", state.module.get());
        llvm::Function::Create(void_fty, llvm::GlobalValue::InternalLinkage, "ip_gnu_dtor_fn", state.module.get());

        a_variable ip_var = {};
        ip_var.source_corresp.name = "ip_gnu_dyn_var";
        ip_var.type = &zz_i32;
        ip_var.init_kind = initk_dynamic;
        ip_var.init_priority = 400;
        a_dynamic_init ip_dip = {};
        ip_dip.kind = dik_constant;
        a_constant ip_con = {};
        ip_con.kind = ck_integer;
        ip_con.type = &zz_i32;
        ip_dip.variant.constant.ptr = &ip_con;
        ip_var.initializer.dynamic = &ip_dip;

        a_scope ip_sc = {};
        ip_sc.kind = sck_file;
        ip_sc.routines = &ip_ctor;
        il_header.primary_scope = &ip_sc;

        EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");
        llvm::GlobalVariable* ip_gv1 = llvm::cast<llvm::GlobalVariable>(
            state.module->getOrInsertGlobal("ip_gnu_dyn_var", llvm::Type::getInt32Ty(*state.context)));
        ip_gv1->setInitializer(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 0));
        state.local_vars[&ip_var] = ip_gv1;
        state.global_dynamic_inits.push_back({&ip_var, &ip_dip});
        assert(llvm_lower_global_ctors_and_dtors() == llvm_gen_be_error_t::ok);

        auto msvc_ip_mod = std::make_unique<llvm::Module>("msvc_ip_mod", *state.context);
        auto saved_mod_ip = std::move(state.module);
        state.module = std::move(msvc_ip_mod);
        EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-pc-windows-msvc");
        llvm::Function::Create(void_fty, llvm::GlobalValue::InternalLinkage, "ip_gnu_ctor_fn", state.module.get());
        llvm::Function::Create(void_fty, llvm::GlobalValue::InternalLinkage, "ip_gnu_dtor_fn", state.module.get());
        llvm::GlobalVariable* ip_gv2 = llvm::cast<llvm::GlobalVariable>(
            state.module->getOrInsertGlobal("ip_gnu_dyn_var", llvm::Type::getInt32Ty(*state.context)));
        ip_gv2->setInitializer(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*state.context), 0));
        state.local_vars[&ip_var] = ip_gv2;
        state.global_dynamic_inits.push_back({&ip_var, &ip_dip});
        assert(llvm_lower_global_ctors_and_dtors() == llvm_gen_be_error_t::ok);
        state.local_vars.erase(&ip_var);
        state.module = std::move(saved_mod_ip);

        EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), saved_tr_ip);
        il_header.primary_scope = saved_pri_ip;
      }
#endif
    }
  }

  printf("PASS\n");
  return 0;
}
