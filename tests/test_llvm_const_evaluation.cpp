/**
 * @file test_llvm_const_evaluation.cpp
 * @ingroup llvm_gen_be_tests
 * @brief Unit tests for the LLVM backend constant evaluator subsystem.
 * @details Validates integer, floating-point, string, address, and struct/array/union aggregate constants.
 */

#include "llvm_gen_be_const.h"
#include "llvm_gen_be_type.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_error.h"
#include "il.h"
#include "il_def.h"
#include "target.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Constants.h>
#include <llvm/Support/raw_ostream.h>
#include <cassert>

using namespace edg;

LLVMBackendState* edg::be_state = nullptr;

namespace edg {
  /**
   * @brief Test stub or helper for `is_bool_type`.
   * @details Provides mock or helper behavior for `is_bool_type` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] ty Parameter `ty` for `is_bool_type`.
   * @return Result of `is_bool_type` (a_boolean).
   */
  a_boolean is_bool_type(a_type_ptr ty) { return (ty && ty->variant.integer.bool_type) ? TRUE : FALSE; }
  /**
   * @brief Test stub or helper for `f_alignment_of_type`.
   * @details Provides mock or helper behavior for `f_alignment_of_type` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] ty Parameter `ty` for `f_alignment_of_type`.
   * @return Result of `f_alignment_of_type` (a_targ_alignment).
   */
  a_targ_alignment f_alignment_of_type(a_type_ptr ty) { return 1; }
  /**
   * @brief Test stub or helper for `is_trivially_copyable_type`.
   * @details Provides mock or helper behavior for `is_trivially_copyable_type` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] ty Parameter `ty` for `is_trivially_copyable_type`.
   * @return Result of `is_trivially_copyable_type` (a_boolean).
   */
  a_boolean is_trivially_copyable_type(a_type_ptr ty) { return TRUE; }
  unsigned int targ_char_bit = 8;
  a_targ_size_t targ_sizeof_short = 2;
  a_targ_size_t targ_sizeof_int = 4;
  a_targ_size_t targ_sizeof_long = 8;
  a_targ_size_t targ_sizeof_long_long = 8;
  a_targ_size_t targ_sizeof_float = 4;
  a_targ_size_t targ_sizeof_double = 8;
  a_targ_size_t targ_sizeof_long_double = 16;
  a_targ_size_t targ_sizeof_float80 = 10;
  a_targ_size_t targ_sizeof_float128 = 16;
  a_targ_size_t targ_sizeof_pointer = 8;
  a_byte_boolean int_kind_is_signed[ik_last]; // mock

  /**
   * @brief Test stub or helper for `is_reference_type`.
   * @details Provides mock or helper behavior for `is_reference_type` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] ty Parameter `ty` for `is_reference_type`.
   * @return Result of `is_reference_type` (a_boolean).
   */
  a_boolean is_reference_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `is_empty_class_type`.
   * @details Provides mock or helper behavior for `is_empty_class_type` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] ty Parameter `ty` for `is_empty_class_type`.
   * @return Result of `is_empty_class_type` (a_boolean).
   */
  a_boolean is_empty_class_type(a_type_ptr ty) { return ty && ty->variant.class_struct_union.is_empty_class; }
  /**
   * @brief Test stub or helper for `is_class_struct_union_type`.
   * @details Provides mock or helper behavior for `is_class_struct_union_type` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] ty Parameter `ty` for `is_class_struct_union_type`.
   * @return Result of `is_class_struct_union_type` (a_boolean).
   */
  a_boolean is_class_struct_union_type(a_type_ptr ty) { return FALSE; }
  /**
   * @brief Test stub or helper for `mangled_typeinfo_name`.
   * @details Provides mock or helper behavior for `mangled_typeinfo_name` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] type Parameter `type` for `mangled_typeinfo_name`.
   * @return Result of `mangled_typeinfo_name` (char*).
   */
  char* mangled_typeinfo_name(a_type_ptr type) { return (char*)"_ZTI_mock"; }
  /**
   * @brief Test stub or helper for `conv_seq_to_file_and_line`.
   * @details Provides mock or helper behavior for `conv_seq_to_file_and_line` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] seq_number Sequence number to resolve.
   * @param[out] file_name Short file name output pointer.
   * @param[out] full_name Full path output pointer.
   * @param[out] line_number Line number output pointer.
   * @param[out] at_end_of_source End-of-source flag output pointer.
   * @return Null `a_source_file_ptr` stub result.
   */
  a_source_file_ptr conv_seq_to_file_and_line(
      a_seq_number seq_number,
      a_const_char** file_name,
      a_const_char** full_name,
      a_line_number* line_number,
      a_boolean* at_end_of_source) {
    return (seq_number || file_name || full_name || line_number || at_end_of_source) ? nullptr : nullptr;
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
   * @brief Test stub or helper for `llvm_lower_expression`.
   * @details Provides mock or helper behavior for `llvm_lower_expression` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] expr Parameter `expr` for `llvm_lower_expression`.
   * @param[out] out_val Parameter `out_val` for `llvm_lower_expression`.
   * @return Result of `llvm_lower_expression` (llvm_gen_be_error_t).
   */
  llvm_gen_be_error_t llvm_lower_expression(an_expr_node_ptr expr, llvm::Value** out_val) noexcept {
    if (out_val) *out_val = nullptr;
    return llvm_gen_be_error_t::ok;
  }
  
  /**
   * @brief Test stub or helper for `fp_to_string`.
   * @details Provides mock or helper behavior for `fp_to_string` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] fk Parameter `fk` for `fp_to_string`.
   * @param[in] val Parameter `val` for `fp_to_string`.
   * @param[in] pos_inf Parameter `pos_inf` for `fp_to_string`.
   * @param[in] neg_inf Parameter `neg_inf` for `fp_to_string`.
   * @param[in] nan Parameter `nan` for `fp_to_string`.
   * @return Result of `fp_to_string` (a_number_buffer).
   */
  a_number_buffer fp_to_string(a_float_kind fk, an_internal_float_value * val, a_boolean* pos_inf, a_boolean* neg_inf, a_boolean* nan) {
    (void)fk;
    if (val->bytes[0] == 1) *pos_inf = TRUE;
    else if (val->bytes[0] == 2) *neg_inf = TRUE;
    else if (val->bytes[0] == 3) *nan = TRUE;
    else if (val->bytes[0] == 4) return a_number_buffer("-1.25");
    else if (val->bytes[0] == 5) return a_number_buffer("1.5e-20");
    else if (val->bytes[0] == 6) return a_number_buffer("2e+20");
    else if (val->bytes[0] == 7) return a_number_buffer("3e5");
    else return a_number_buffer("3.14");
    return a_number_buffer("");
  }
}

/**
 * @brief Entry point for the constant evaluation unit test suite.
 * @details Exercises all `llvm_const_from_*` and `evaluate_constant` code paths.
 * @return 0 on success.
 */
int main(void) {
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_mod", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm::FunctionType* fty = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* func = llvm::Function::Create(fty, llvm::GlobalValue::ExternalLinkage, "test_func", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", func);
  state.builder->SetInsertPoint(bb);

  a_type dummy_int_ty;
  memset(&dummy_int_ty, 0, sizeof(dummy_int_ty));
  dummy_int_ty.kind = tk_integer;
  dummy_int_ty.variant.integer.int_kind = ik_int;

  a_constant dummy_const;
  memset(&dummy_const, 0, sizeof(dummy_const));

  llvm::Type* i32_ty = llvm::Type::getInt32Ty(*state.context);
  llvm::Constant* out = nullptr;

  dummy_const.kind = ck_integer;
  dummy_const.type = &dummy_int_ty;
  assert(llvm_set_edg_int128(nullptr, 0ULL, 0ULL) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_set_edg_sint64(nullptr, 0) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_set_edg_sint64(&dummy_const.variant.integer_value, 42) == llvm_gen_be_error_t::ok);

  uint64_t ext_low = 0;
  uint64_t ext_high = 0;
  assert(llvm_extract_edg_int128(dummy_const.variant.integer_value, true, nullptr, &ext_high) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_extract_edg_int128(dummy_const.variant.integer_value, true, &ext_low, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_extract_edg_int128(dummy_const.variant.integer_value, true, &ext_low, &ext_high) == llvm_gen_be_error_t::ok);
  assert(ext_low == 42 && ext_high == 0);
  
  assert(llvm_const_from_integer(&dummy_const, i32_ty, &out) == llvm_gen_be_error_t::ok);
  assert(llvm::cast<llvm::ConstantInt>(out)->getZExtValue() == 42);

  fprintf(stderr, "Test integer passed\n");

  // Test evaluate_constant
  assert(evaluate_constant(&dummy_const, i32_ty, &out) == llvm_gen_be_error_t::ok);
  assert(llvm::cast<llvm::ConstantInt>(out)->getZExtValue() == 42);

  dummy_const.kind = ck_float;
  a_type dummy_fp_ty;
  memset(&dummy_fp_ty, 0, sizeof(dummy_fp_ty));
  dummy_fp_ty.kind = tk_float;
  dummy_fp_ty.variant.float_kind = fk_float;
  dummy_const.type = &dummy_fp_ty;
  dummy_const.variant.float_value.bytes[0] = 0; // fallback to 3.14
  llvm::Type* f32_ty = llvm::Type::getFloatTy(*state.context);
  assert(llvm_const_from_float(&dummy_const, f32_ty, &out) == llvm_gen_be_error_t::ok);
  
  dummy_const.variant.float_value.bytes[0] = 1; // pos inf
  assert(llvm_const_from_float(&dummy_const, f32_ty, &out) == llvm_gen_be_error_t::ok);
  assert(llvm::cast<llvm::ConstantFP>(out)->isInfinity());
  
  dummy_const.variant.float_value.bytes[0] = 2; // neg inf
  assert(llvm_const_from_float(&dummy_const, f32_ty, &out) == llvm_gen_be_error_t::ok);
  assert(llvm::cast<llvm::ConstantFP>(out)->isInfinity());

  dummy_const.variant.float_value.bytes[0] = 3; // nan
  assert(llvm_const_from_float(&dummy_const, f32_ty, &out) == llvm_gen_be_error_t::ok);
  assert(llvm::cast<llvm::ConstantFP>(out)->isNaN());
  
  fprintf(stderr, "Test float passed\n");

  // Test address
  a_label main_lbl_1 = {}, main_lbl_2 = {};
  dummy_const.kind = ck_address;
  dummy_const.variant.address.kind = abk_label;
  dummy_const.variant.address.variant.label = &main_lbl_1;
  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*state.context);
  assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);
  
  dummy_const.variant.address.kind = static_cast<an_address_base_kind>(99);
  assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);
  assert(out->isNullValue());

  fprintf(stderr, "Test address passed\n");

  // Test string
  dummy_const.kind = ck_string;
  a_type dummy_arr_ty;
  memset(&dummy_arr_ty, 0, sizeof(dummy_arr_ty));
  dummy_arr_ty.kind = tk_array;
  dummy_arr_ty.variant.array.element_type = &dummy_int_ty;
  dummy_arr_ty.variant.array.variant.number_of_elements = 4;
  dummy_const.type = &dummy_arr_ty;
  dummy_const.variant.string.length = 4;
  dummy_const.variant.string.value = "test";
  assert(llvm_const_from_string(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);
  assert(llvm::isa<llvm::GlobalVariable>(out));
  
  fprintf(stderr, "Test string passed\n");

  // Test aggregate
  dummy_const.kind = ck_aggregate;
  a_constant elem1, elem2;
  memset(&elem1, 0, sizeof(elem1));
  memset(&elem2, 0, sizeof(elem2));
  elem1.kind = ck_integer;
  elem1.type = &dummy_int_ty;
  assert(llvm_set_edg_sint64(&elem1.variant.integer_value, 1) == llvm_gen_be_error_t::ok);
  elem1.next = &elem2;
  elem2.kind = ck_integer;
  elem2.type = &dummy_int_ty;
  assert(llvm_set_edg_sint64(&elem2.variant.integer_value, 2) == llvm_gen_be_error_t::ok);
  dummy_const.variant.aggregate.first_constant = &elem1;
  
  llvm::ArrayType* arr_ty = llvm::ArrayType::get(i32_ty, 3);
  assert(llvm_const_from_aggregate(&dummy_const, arr_ty, &out) == llvm_gen_be_error_t::ok);
  assert(out->getType() == arr_ty);

  llvm::FixedVectorType* vec_ty = llvm::FixedVectorType::get(i32_ty, 4);
  assert(llvm_const_from_aggregate(&dummy_const, vec_ty, &out) == llvm_gen_be_error_t::ok);
  assert(out->getType() == vec_ty);
  a_constant bad_vec_elem = {};
  bad_vec_elem.kind = static_cast<a_constant_repr_kind>(99);
  dummy_const.variant.aggregate.first_constant = &bad_vec_elem;
  assert(llvm_const_from_aggregate(&dummy_const, vec_ty, &out) == llvm_gen_be_error_t::unsupported_expr);
  dummy_const.variant.aggregate.first_constant = &elem1;
  
  llvm::StructType* st_ty = llvm::StructType::get(*state.context, {i32_ty, i32_ty, i32_ty});
  assert(llvm_const_from_aggregate(&dummy_const, st_ty, &out) == llvm_gen_be_error_t::ok);
  assert(out->getType() == st_ty);
  
  // Test Union Aggregate
  a_type dummy_union_ty;
  memset(&dummy_union_ty, 0, sizeof(dummy_union_ty));
  dummy_union_ty.kind = tk_union;
  dummy_const.type = &dummy_union_ty;
  
  llvm::Type* un_elems[] = {llvm::Type::getDoubleTy(*state.context)};
  llvm::StructType* un_ty = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>(un_elems));
  assert(llvm_const_from_aggregate(&dummy_const, un_ty, &out) == llvm_gen_be_error_t::ok);

  // Test Union Aggregate with smaller integer member coerced into i64 or [8 x i8]
  llvm::Type* un_i64_elems[] = {llvm::Type::getInt64Ty(*state.context)};
  llvm::StructType* un_i64_ty = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>(un_i64_elems));
  assert(llvm_const_from_aggregate(&dummy_const, un_i64_ty, &out) == llvm_gen_be_error_t::ok);

  llvm::Type* un_bytes_elems[] = {llvm::ArrayType::get(llvm::Type::getInt8Ty(*state.context), 8)};
  llvm::StructType* un_bytes_ty = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>(un_bytes_elems));
  assert(llvm_const_from_aggregate(&dummy_const, un_bytes_ty, &out) == llvm_gen_be_error_t::ok);
  out->print(llvm::errs()); llvm::errs() << "\n";

  // Test Struct Aggregate with Bitfields and Synthetic Padding
  a_field bf1 = {};
  bf1.is_bit_field = TRUE;
  bf1.offset = 0;
  bf1.offset_bit_remainder = 0;
  bf1.bit_size = 3;
  bf1.type = &dummy_int_ty;
  a_field bf2 = {};
  bf2.is_bit_field = TRUE;
  bf2.offset = 0;
  bf2.offset_bit_remainder = 3;
  bf2.bit_size = 5;
  bf2.type = &dummy_int_ty;
  a_field normal_f = {};
  normal_f.is_bit_field = FALSE;
  normal_f.offset = 4;
  normal_f.type = &dummy_int_ty;
  bf1.next = &bf2;
  bf2.next = &normal_f;

  a_type bf_struct_ty = {};
  bf_struct_ty.kind = tk_struct;
  bf_struct_ty.variant.class_struct_union.field_list = &bf1;
  dummy_const.type = &bf_struct_ty;

  a_constant bf_c1 = {};
  bf_c1.kind = ck_integer;
  bf_c1.type = &dummy_int_ty;
  assert(llvm_set_edg_sint64(&bf_c1.variant.integer_value, 5) == llvm_gen_be_error_t::ok);
  a_constant bf_c2 = {};
  bf_c2.kind = ck_integer;
  bf_c2.type = &dummy_int_ty;
  assert(llvm_set_edg_sint64(&bf_c2.variant.integer_value, 3) == llvm_gen_be_error_t::ok);
  a_constant bf_c3 = {};
  bf_c3.kind = ck_integer;
  bf_c3.type = &dummy_int_ty;
  assert(llvm_set_edg_sint64(&bf_c3.variant.integer_value, 99) == llvm_gen_be_error_t::ok);
  bf_c1.next = &bf_c2;
  bf_c2.next = &bf_c3;
  dummy_const.variant.aggregate.first_constant = &bf_c1;

  llvm::Type* i8_ty = llvm::Type::getInt8Ty(*state.context);
  llvm::ArrayType* pad3_ty = llvm::ArrayType::get(i8_ty, 3);
  llvm::StructType* bf_llvm_st = llvm::StructType::get(*state.context, {i8_ty, pad3_ty, i32_ty});
  assert(llvm_const_from_aggregate(&dummy_const, bf_llvm_st, &out) == llvm_gen_be_error_t::ok);
  assert(out->getType() == bf_llvm_st);
  
  fprintf(stderr, "Test aggregate passed\n");

  // Test nulls and invalid args
  assert(llvm_const_from_integer(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_integer(&dummy_const, i32_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_float(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_float(&dummy_const, f32_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_string(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_string(&dummy_const, ptr_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_aggregate(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_aggregate(&dummy_const, st_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_address(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_address(&dummy_const, ptr_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(evaluate_constant(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  
  assert(evaluate_constant(nullptr, i32_ty, &out) == llvm_gen_be_error_t::ok);
  assert(out->isNullValue());
  
  dummy_const.kind = static_cast<a_constant_repr_kind>(99);
  assert(evaluate_constant(&dummy_const, i32_ty, &out) == llvm_gen_be_error_t::unsupported_expr);

  // Additional branch coverage tests for integer constants (signed, 128-bit, tk_enum, non-int kind)
  {
    int_kind_is_signed[ik_int] = TRUE;
    dummy_const.kind = ck_integer;
    dummy_const.type = &dummy_int_ty;
    assert(llvm_set_edg_sint64(&dummy_const.variant.integer_value, -7) == llvm_gen_be_error_t::ok);
    assert(llvm_const_from_integer(&dummy_const, i32_ty, &out) == llvm_gen_be_error_t::ok);
    assert(llvm_const_from_integer(&dummy_const, llvm::Type::getInt128Ty(*state.context), &out) == llvm_gen_be_error_t::ok);

    a_type enum_ty = {};
    enum_ty.kind = tk_enum;
    enum_ty.variant.integer.int_kind = ik_int;
    dummy_const.type = &enum_ty;
    assert(llvm_const_from_integer(&dummy_const, i32_ty, &out) == llvm_gen_be_error_t::ok);

    dummy_const.type = &dummy_fp_ty; // neither tk_integer nor tk_enum
    assert(llvm_const_from_integer(&dummy_const, i32_ty, &out) == llvm_gen_be_error_t::ok);
  }

  // Additional branch coverage tests for string constants (i8, i16, i32, cache hit, non-pointer expected_ty, non-array type, error)
  {
    a_type char_ty = {}, char16_ty = {}, err_ty = {};
    char_ty.kind = tk_integer;
    char_ty.variant.integer.int_kind = ik_char;
    char16_ty.kind = tk_integer;
    char16_ty.variant.integer.int_kind = ik_unsigned_short;
    err_ty.kind = static_cast<a_type_kind>(999);

    a_type str8_arr_ty = {}, str16_arr_ty = {}, non_arr_ty = {};
    str8_arr_ty.kind = tk_array;
    str8_arr_ty.variant.array.element_type = &char_ty;
    str8_arr_ty.variant.array.variant.number_of_elements = 4;

    str16_arr_ty.kind = tk_array;
    str16_arr_ty.variant.array.element_type = &char16_ty;
    str16_arr_ty.variant.array.variant.number_of_elements = 2;

    non_arr_ty.kind = tk_pointer;

    dummy_const.kind = ck_string;
    dummy_const.variant.string.length = 4;
    dummy_const.variant.string.value = "test"; // hits string_literal_cache from earlier test!
    dummy_const.type = &str8_arr_ty;
    assert(evaluate_constant(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    // Non-pointer expected_ty returns ConstantDataArray directly
    llvm::Type* i8_arr4_ty = llvm::ArrayType::get(i8_ty, 4);
    assert(llvm_const_from_string(&dummy_const, i8_arr4_ty, &out) == llvm_gen_be_error_t::ok);

    // UTF-16 string
    dummy_const.type = &str16_arr_ty;
    assert(llvm_const_from_string(&dummy_const, i8_arr4_ty, &out) == llvm_gen_be_error_t::ok);

    // Non-array con->type recovers gracefully as i8
    dummy_const.type = &non_arr_ty;
    assert(llvm_const_from_string(&dummy_const, i8_arr4_ty, &out) == llvm_gen_be_error_t::ok);

    // Error in con->type
    dummy_const.type = &err_ty;
    assert(llvm_const_from_string(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::unsupported_type);
  }

  // Additional branch coverage tests for address constants (abk_label cached & unparented, abk_variable, abk_routine, abk_constant, offset != 0)
  {
    dummy_const.kind = ck_address;
    dummy_const.variant.address.offset = 8;
    // Cached label hit
    dummy_const.variant.address.kind = abk_label;
    dummy_const.variant.address.variant.label = &main_lbl_1;
    assert(evaluate_constant(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    // Unparented label_bb in label_blocks
    a_label_ptr unparented_lbl = &main_lbl_2;
    llvm::BasicBlock* unparented_bb = llvm::BasicBlock::Create(*state.context, "unparented_lbl");
    llvm::ReturnInst::Create(*state.context, nullptr, unparented_bb);
    state.label_blocks[unparented_lbl] = unparented_bb;
    dummy_const.variant.address.variant.label = unparented_lbl;
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);
    dummy_const.variant.address.offset = 0;

    // abk_variable: null var, null name, invalid type, new global, existing global
    dummy_const.variant.address.kind = abk_variable;
    dummy_const.variant.address.variant.variable = nullptr;
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    a_variable addr_var = {};
    dummy_const.variant.address.variant.variable = &addr_var;
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    a_type bad_ty = {};
    bad_ty.kind = static_cast<a_type_kind>(999);
    state.type_cache.clear();
    addr_var.source_corresp.name = (char*)"addr_gvar";
    addr_var.type = &bad_ty;
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::unsupported_type);

    addr_var.type = &dummy_int_ty;
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);
    // Second call hits existing GlobalVariable in module
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    // abk_routine: null routine, null name, invalid type, new function, existing function
    dummy_const.variant.address.kind = abk_routine;
    dummy_const.variant.address.variant.routine = nullptr;
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    a_routine addr_rout = {};
    dummy_const.variant.address.variant.routine = &addr_rout;
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    a_type rout_ty = {};
    rout_ty.kind = tk_routine;
    rout_ty.variant.routine.return_type = &bad_ty;
    addr_rout.source_corresp.name = (char*)"addr_fn";
    addr_rout.type = &rout_ty;
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::unsupported_type);

    rout_ty.variant.routine.return_type = &dummy_int_ty;
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);
    // Second call hits existing Function in module
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    // abk_constant: invalid target->type, invalid target constant, valid target constant
    dummy_const.variant.address.kind = abk_constant;
    a_constant target_con = {};
    target_con.kind = static_cast<a_constant_repr_kind>(99);
    target_con.type = &bad_ty;
    dummy_const.variant.address.variant.constant = &target_con;
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::unsupported_type);

    target_con.type = &dummy_int_ty;
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::unsupported_expr);

    target_con.kind = ck_string;
    target_con.variant.string.length = 2;
    target_con.variant.string.value = "x";
    assert(llvm_const_from_address(&dummy_const, ptr_ty, &out) == llvm_gen_be_error_t::ok);
  }

  // Additional branch coverage for evaluate_constant(ck_float, ck_aggregate, ck_ptr_to_member) and aggregate edge cases
  {
    dummy_const.kind = ck_float;
    dummy_const.type = &dummy_fp_ty;
    dummy_const.variant.float_value.bytes[0] = 0;
    assert(evaluate_constant(&dummy_const, f32_ty, &out) == llvm_gen_be_error_t::ok);

    // ck_ptr_to_member via evaluate_constant
    a_type p2m_ty = {};
    p2m_ty.kind = tk_ptr_to_member;
    p2m_ty.size = 8;
    p2m_ty.variant.ptr_to_member.type = &dummy_int_ty;
    a_field p2m_field = {};
    p2m_field.offset = 4;
    a_constant p2m_con = {};
    p2m_con.kind = ck_ptr_to_member;
    p2m_con.type = &p2m_ty;
    p2m_con.variant.ptr_to_member.is_function_ptr = FALSE;
    p2m_con.variant.ptr_to_member.variant.field = &p2m_field;
    assert(evaluate_constant(&p2m_con, llvm::Type::getInt64Ty(*state.context), &out) == llvm_gen_be_error_t::ok);

    // Aggregate with non-array, non-struct expected_ty
    a_constant agg_con = {};
    agg_con.kind = ck_aggregate;
    agg_con.type = &dummy_int_ty;
    assert(evaluate_constant(&agg_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    // Array aggregate with invalid element constant
    a_constant bad_elem = {};
    bad_elem.kind = static_cast<a_constant_repr_kind>(99);
    bad_elem.type = &dummy_int_ty;
    agg_con.variant.aggregate.first_constant = &bad_elem;
    assert(llvm_const_from_aggregate(&agg_con, arr_ty, &out) == llvm_gen_be_error_t::unsupported_expr);

    // Union aggregate edge cases:
    // 1. c == nullptr and empty struct {}, plus c != nullptr with empty struct {}
    a_type un_edg_ty = {};
    un_edg_ty.kind = tk_union;
    agg_con.type = &un_edg_ty;
    agg_con.variant.aggregate.first_constant = nullptr;
    llvm::StructType* empty_st_ty = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>{});
    assert(llvm_const_from_aggregate(&agg_con, empty_st_ty, &out) == llvm_gen_be_error_t::ok);
    agg_con.variant.aggregate.first_constant = &elem1;
    assert(llvm_const_from_aggregate(&agg_con, empty_st_ty, &out) == llvm_gen_be_error_t::ok);

    // 2. Union with multi-element StructType { i32, [4 x i8] } and exact type match (target_ty == elem0_ty)
    llvm::StructType* multi_un_st = llvm::StructType::get(*state.context, {i32_ty, llvm::ArrayType::get(i8_ty, 4)});
    agg_con.variant.aggregate.first_constant = &elem1;
    elem1.next = nullptr;
    assert(llvm_const_from_aggregate(&agg_con, multi_un_st, &out) == llvm_gen_be_error_t::ok);

    // 3. Union where c->type is invalid, and where c is invalid (errors percolate)
    a_type bad_ty = {};
    bad_ty.kind = static_cast<a_type_kind>(999);
    a_constant un_bad_type_c = {};
    un_bad_type_c.kind = ck_integer;
    un_bad_type_c.type = &bad_ty;
    agg_con.variant.aggregate.first_constant = &un_bad_type_c;
    assert(llvm_const_from_aggregate(&agg_con, multi_un_st, &out) == llvm_gen_be_error_t::unsupported_type);

    agg_con.variant.aggregate.first_constant = &bad_elem;
    assert(llvm_const_from_aggregate(&agg_con, multi_un_st, &out) == llvm_gen_be_error_t::unsupported_expr);

    // 4. Union where active_val is float (non-integer) and elem0_ty is [8 x i8] or [2 x i32]
    a_constant fp_c = {};
    fp_c.kind = ck_float;
    fp_c.type = &dummy_fp_ty;
    agg_con.variant.aggregate.first_constant = &fp_c;
    assert(llvm_const_from_aggregate(&agg_con, un_bytes_ty, &out) == llvm_gen_be_error_t::ok);
    llvm::Type* i32_arr2_elem[] = {llvm::ArrayType::get(i32_ty, 2)};
    llvm::StructType* un_i32arr_st = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>(i32_arr2_elem));
    assert(llvm_const_from_aggregate(&agg_con, un_i32arr_st, &out) == llvm_gen_be_error_t::ok);

    // 5. Union where elem0_ty is [2 x i8] (smaller than 4-byte i32 active_val -> b < num_bytes stops loop)
    agg_con.variant.aggregate.first_constant = &elem1;
    llvm::Type* i8_arr2_elem[] = {llvm::ArrayType::get(i8_ty, 2)};
    llvm::StructType* un_2bytes_st = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>(i8_arr2_elem));
    assert(llvm_const_from_aggregate(&agg_con, un_2bytes_st, &out) == llvm_gen_be_error_t::ok);

    // Struct/class aggregate edge cases:
    // 1. tk_class with real tk_array field, non-i8 array field, bitfield overflow (bit_off + bit_sz > unit_bits),
    //    bit_sz == 0, non-integer bitfield constant, c == nullptr with remaining field, and field == nullptr with trailing pad
    a_type cls_agg_ty = {};
    cls_agg_ty.kind = tk_class;
    a_type i8_arr_edg_ty = {};
    i8_arr_edg_ty.kind = tk_array;
    a_field cf_arr = {}, cf_bf1 = {}, cf_bf0 = {}, cf_bf_over = {}, cf_extra = {};
    cf_arr.type = &i8_arr_edg_ty;
    cf_arr.next = &cf_bf1;

    cf_bf1.is_bit_field = TRUE;
    cf_bf1.offset = 4;
    cf_bf1.offset_bit_remainder = 0;
    cf_bf1.bit_size = 4;
    cf_bf1.type = &dummy_int_ty;
    cf_bf1.next = &cf_bf0;

    cf_bf0.is_bit_field = TRUE;
    cf_bf0.offset = 4;
    cf_bf0.offset_bit_remainder = 4;
    cf_bf0.bit_size = 0; // bit_sz == 0
    cf_bf0.type = &dummy_int_ty;
    cf_bf0.next = &cf_bf_over;

    cf_bf_over.is_bit_field = TRUE;
    cf_bf_over.offset = 5; // bit_off = 8, bit_sz = 4 -> 12 > 8 (unit_bits), breaks!
    cf_bf_over.offset_bit_remainder = 0;
    cf_bf_over.bit_size = 4;
    cf_bf_over.type = &dummy_int_ty;
    cf_bf_over.next = &cf_extra;

    cf_extra.is_bit_field = FALSE;
    cf_extra.type = &dummy_int_ty;
    cf_extra.next = nullptr;

    cls_agg_ty.variant.class_struct_union.field_list = &cf_arr;
    agg_con.type = &cls_agg_ty;

    llvm::ArrayType* pad2_ty = llvm::ArrayType::get(i8_ty, 2);
    llvm::ArrayType* arr_i32_1 = llvm::ArrayType::get(i32_ty, 1);
    llvm::StructType* complex_cls_st = llvm::StructType::get(
        *state.context, {pad2_ty, i8_ty, i8_ty, arr_i32_1, pad2_ty, i32_ty});

    a_constant cc_arr = {}, cc_bf1 = {}, cc_bf0_c = {};
    cc_arr.kind = ck_aggregate;
    cc_arr.type = &i8_arr_edg_ty;
    cc_arr.next = &cc_bf1;
    cc_bf1.kind = ck_float; // c->kind != ck_integer
    cc_bf1.type = &dummy_fp_ty;
    cc_bf1.next = &cc_bf0_c;
    cc_bf0_c.kind = ck_integer;
    cc_bf0_c.type = &dummy_int_ty;
    cc_bf0_c.next = nullptr; // c becomes nullptr before cf_bf_over and cf_extra!
    agg_con.variant.aggregate.first_constant = &cc_arr;
    assert(llvm_const_from_aggregate(&agg_con, complex_cls_st, &out) == llvm_gen_be_error_t::ok);

    // Test bitfield loop ending when field becomes nullptr (last field is a bitfield) and remaining elements are i32_ty and pad2_ty
    cf_bf1.next = nullptr;
    llvm::StructType* tail_bf_st = llvm::StructType::get(*state.context, {pad2_ty, i8_ty, i32_ty, pad2_ty});
    cc_bf1.next = &elem1;
    elem1.next = &elem2;
    elem2.next = nullptr;
    assert(llvm_const_from_aggregate(&agg_con, tail_bf_st, &out) == llvm_gen_be_error_t::ok);

    // Test error propagation inside field != nullptr non-bitfield evaluate_constant
    cf_arr.next = nullptr;
    agg_con.variant.aggregate.first_constant = &bad_elem;
    assert(llvm_const_from_aggregate(&agg_con, tail_bf_st, &out) == llvm_gen_be_error_t::unsupported_expr);

    // Test field == nullptr struct loop with excess constants and error propagation
    cls_agg_ty.variant.class_struct_union.field_list = nullptr;
    llvm::Type* one_i32_elem[] = {i32_ty};
    llvm::StructType* one_elem_st = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>(one_i32_elem));
    elem1.next = &elem2;
    elem2.next = nullptr;
    agg_con.variant.aggregate.first_constant = &elem1;
    assert(llvm_const_from_aggregate(&agg_con, one_elem_st, &out) == llvm_gen_be_error_t::ok);

    agg_con.variant.aggregate.first_constant = &bad_elem;
    assert(llvm_const_from_aggregate(&agg_con, one_elem_st, &out) == llvm_gen_be_error_t::unsupported_expr);

    // Phase 1 Tests: ck_complex, ck_imaginary, ck_label_difference, ck_stack_offset, ck_dynamic_init, ck_void
    elem1.next = nullptr;
    elem2.next = nullptr;
    a_type dummy_ptr_ty = {};
    dummy_ptr_ty.kind = tk_pointer;
    dummy_ptr_ty.size = 8;
    a_variable dummy_var = {};
    dummy_var.source_corresp.name = "my_dummy_var";
    dummy_var.type = &dummy_int_ty;

    assert(llvm_const_from_complex(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_complex(&dummy_const, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_complex(&dummy_const, f32_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
    a_constant cplx_con = {};
    cplx_con.kind = ck_complex;
    cplx_con.type = &dummy_fp_ty;
    cplx_con.variant.complex_value = nullptr;
    assert(llvm_const_from_complex(&cplx_con, f32_ty, &out) == llvm_gen_be_error_t::invalid_argument);

    an_internal_complex_value cplx_val = {};
    cplx_val.real.bytes[0] = 0; // normal
    cplx_val.imag.bytes[0] = 1; // +inf
    cplx_con.variant.complex_value = &cplx_val;
    llvm::StructType* cplx_st_ty = llvm::StructType::get(*state.context, {f32_ty, f32_ty});
    assert(evaluate_constant(&cplx_con, cplx_st_ty, &out) == llvm_gen_be_error_t::ok);
    // Non-struct expected_ty fallback in llvm_const_from_complex
    assert(llvm_const_from_complex(&cplx_con, f32_ty, &out) == llvm_gen_be_error_t::ok);
    // 1-element struct expected_ty fallback in llvm_const_from_complex
    assert(llvm_const_from_complex(&cplx_con, one_elem_st, &out) == llvm_gen_be_error_t::ok);
    // Invalid float_kind in non-struct expected_ty fallback
    a_type bad_fp_ty = {};
    bad_fp_ty.kind = tk_float;
    bad_fp_ty.variant.float_kind = static_cast<a_float_kind>(99);
    cplx_con.type = &bad_fp_ty;
    assert(llvm_const_from_complex(&cplx_con, f32_ty, &out) == llvm_gen_be_error_t::unsupported_type);
    cplx_con.type = &dummy_fp_ty;

    // ck_imaginary
    assert(llvm_const_from_imaginary(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_imaginary(&dummy_const, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_imaginary(&dummy_const, f32_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
    a_constant imag_con = {};
    imag_con.kind = ck_imaginary;
    imag_con.type = &dummy_fp_ty;
    imag_con.variant.float_value.bytes[0] = 0;
    assert(evaluate_constant(&imag_con, f32_ty, &out) == llvm_gen_be_error_t::ok);
    assert(evaluate_constant(&imag_con, cplx_st_ty, &out) == llvm_gen_be_error_t::ok);
    assert(evaluate_constant(&imag_con, one_elem_st, &out) == llvm_gen_be_error_t::ok);
    imag_con.type = &bad_fp_ty;
    assert(evaluate_constant(&imag_con, one_elem_st, &out) == llvm_gen_be_error_t::unsupported_type);
    imag_con.type = &dummy_fp_ty;

    // ck_label_difference
    assert(llvm_const_from_label_difference(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_label_difference(&dummy_const, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_label_difference(&dummy_const, i32_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
    a_constant lbl_diff = {};
    lbl_diff.kind = ck_label_difference;
    lbl_diff.type = &dummy_int_ty;
    lbl_diff.variant.label_difference.to_address = nullptr;
    lbl_diff.variant.label_difference.from_address = nullptr;
    assert(llvm_const_from_label_difference(&lbl_diff, i32_ty, &out) == llvm_gen_be_error_t::invalid_argument);

    a_label lbl_a = {}, lbl_b = {};
    a_constant addr_a = {}, addr_b = {};
    addr_a.kind = ck_address;
    addr_a.type = &dummy_ptr_ty;
    addr_a.variant.address.kind = abk_label;
    addr_a.variant.address.variant.label = &lbl_a;
    addr_b.kind = ck_address;
    addr_b.type = &dummy_ptr_ty;
    addr_b.variant.address.kind = abk_label;
    addr_b.variant.address.variant.label = &lbl_b;

    lbl_diff.variant.label_difference.to_address = &addr_b;
    assert(llvm_const_from_label_difference(&lbl_diff, i32_ty, &out) == llvm_gen_be_error_t::invalid_argument);
    lbl_diff.variant.label_difference.from_address = &addr_a;
    assert(evaluate_constant(&lbl_diff, i32_ty, &out) == llvm_gen_be_error_t::ok);
    assert(evaluate_constant(&lbl_diff, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    // Error propagation in label_difference
    lbl_diff.variant.label_difference.to_address = &bad_elem;
    assert(llvm_const_from_label_difference(&lbl_diff, i32_ty, &out) == llvm_gen_be_error_t::unsupported_expr);
    lbl_diff.variant.label_difference.to_address = &addr_b;
    lbl_diff.variant.label_difference.from_address = &bad_elem;
    assert(llvm_const_from_label_difference(&lbl_diff, i32_ty, &out) == llvm_gen_be_error_t::unsupported_expr);

    // ck_stack_offset
    assert(llvm_const_from_stack_offset(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_stack_offset(&dummy_const, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_stack_offset(&dummy_const, i32_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
    a_constant stk_con = {};
    stk_con.type = &dummy_int_ty;
    assert(llvm_set_edg_sint64(&stk_con.variant.integer_value, 16) == llvm_gen_be_error_t::ok);
    assert(llvm_const_from_stack_offset(&stk_con, i32_ty, &out) == llvm_gen_be_error_t::ok);
    assert(llvm_const_from_stack_offset(&stk_con, ptr_ty, &out) == llvm_gen_be_error_t::ok);
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
    stk_con.kind = ck_stack_offset;
    stk_con.variant.stack_offset.offset = 24;
    assert(evaluate_constant(&stk_con, i32_ty, &out) == llvm_gen_be_error_t::ok);
#endif

    // ck_dynamic_init
    assert(llvm_const_from_dynamic_init(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_dynamic_init(&dummy_const, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_dynamic_init(&dummy_const, i32_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);
    a_constant dyn_con = {};
    dyn_con.kind = ck_dynamic_init;
    dyn_con.type = &dummy_int_ty;
    dyn_con.variant.dynamic_init.ptr = nullptr;
    assert(evaluate_constant(&dyn_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    a_dynamic_init dip_node = {};
    dip_node.kind = dik_zero;
    dyn_con.variant.dynamic_init.ptr = &dip_node;
    assert(evaluate_constant(&dyn_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    dip_node.kind = dik_constant;
    dip_node.variant.constant.ptr = nullptr;
    assert(evaluate_constant(&dyn_con, i32_ty, &out) == llvm_gen_be_error_t::ok);
    dip_node.variant.constant.ptr = &elem1;
    assert(evaluate_constant(&dyn_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    an_expr_node dyn_expr = {};
    dip_node.kind = dik_expression;
    dip_node.variant.expression = nullptr;
    assert(evaluate_constant(&dyn_con, i32_ty, &out) == llvm_gen_be_error_t::ok);
    dyn_expr.kind = enk_operation;
    dip_node.variant.expression = &dyn_expr;
    assert(evaluate_constant(&dyn_con, i32_ty, &out) == llvm_gen_be_error_t::ok);
    dyn_expr.kind = enk_constant;
    dyn_expr.variant.constant.ptr = nullptr;
    assert(evaluate_constant(&dyn_con, i32_ty, &out) == llvm_gen_be_error_t::ok);
    dyn_expr.variant.constant.ptr = &elem1;
    assert(evaluate_constant(&dyn_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    // ck_void
    a_constant void_con = {};
    void_con.kind = ck_void;
    void_con.type = &dummy_int_ty;
    assert(evaluate_constant(&void_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    // abk_temporary, abk_typeid, abk_uuidof, abk_param_ref, and subobject_path
    a_constant tmp_addr = {};
    tmp_addr.kind = ck_address;
    tmp_addr.type = &dummy_ptr_ty;
    tmp_addr.variant.address.kind = abk_temporary;
    tmp_addr.variant.address.variant.constant = nullptr;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    a_constant bad_tmp_target = {};
    bad_tmp_target.kind = ck_integer;
    bad_tmp_target.type = &bad_ty;
    tmp_addr.variant.address.variant.constant = &bad_tmp_target;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::unsupported_type);

    bad_tmp_target.type = &dummy_int_ty;
    bad_tmp_target.kind = static_cast<a_constant_repr_kind>(99);
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::unsupported_expr);

    tmp_addr.variant.address.variant.constant = &elem1;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);
    // Second lookup hits temporary_constant_cache
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    // abk_typeid
    tmp_addr.variant.address.kind = abk_typeid;
    tmp_addr.variant.address.variant.type = &dummy_int_ty;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    // abk_uuidof: nullptr type, anonymous class type, named class with uuid_string, enum with uuid_string, cached lookup
    tmp_addr.variant.address.kind = abk_uuidof;
    tmp_addr.variant.address.variant.type = nullptr;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);
    // Repeat to hit existing GlobalVariable cache
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    a_type uuid_anon_ty = {};
    uuid_anon_ty.kind = tk_pointer;
    uuid_anon_ty.source_corresp.name = "";
    tmp_addr.variant.address.variant.type = &uuid_anon_ty;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    a_type uuid_null_name_ty = {};
    uuid_null_name_ty.kind = tk_struct;
    uuid_null_name_ty.source_corresp.name = nullptr;
    state.module->getNamedGlobal("__uuidof_anon")->eraseFromParent();
    tmp_addr.variant.address.variant.type = &uuid_null_name_ty;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    a_type uuid_cls_ty = {};
    uuid_cls_ty.kind = tk_class;
    uuid_cls_ty.source_corresp.name = "MyComClass";
    a_class_type_supplement uuid_cls_supp = {};
    uuid_cls_supp.uuid_string = "{01234567-89ab-CDEF-fedc-ba9876543210}";
    uuid_cls_ty.variant.class_struct_union.extra_info = nullptr;
    tmp_addr.variant.address.variant.type = &uuid_cls_ty;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    uuid_cls_ty.source_corresp.name = "MyComClassWithUuid";
    uuid_cls_ty.variant.class_struct_union.extra_info = &uuid_cls_supp;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    a_type uuid_enum_ty = {};
    uuid_enum_ty.kind = tk_integer;
    uuid_enum_ty.source_corresp.name = "MyUuidEnum";
    uuid_enum_ty.variant.integer.enum_type = FALSE;
    tmp_addr.variant.address.variant.type = &uuid_enum_ty;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    uuid_enum_ty.source_corresp.name = "MyUuidEnum2";
    uuid_enum_ty.variant.integer.enum_type = TRUE;
    uuid_enum_ty.variant.integer.extra_info = nullptr;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    char enum_uuid_buf[] = "11223344";
    an_integer_type_supplement uuid_enum_supp = {};
    uuid_enum_supp.uuid_string = enum_uuid_buf; // shorter than 32 hex chars to hit *p == '\0'
    uuid_enum_ty.source_corresp.name = "MyUuidEnum3";
    uuid_enum_ty.variant.integer.extra_info = &uuid_enum_supp;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    // abk_param_ref: param_num == 0, param_num > size, non-Constant param, Constant param
    tmp_addr.variant.address.kind = abk_param_ref;
    tmp_addr.variant.address.variant.param_ref.param_num = 0;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    tmp_addr.variant.address.variant.param_ref.param_num = 5;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);

    llvm::Value* non_const_inst = state.builder->CreateAlloca(i32_ty);
    llvm::GlobalVariable* const_param_gv = llvm::cast<llvm::GlobalVariable>(
        state.module->getOrInsertGlobal("const_param_gv", i32_ty));
    state.current_function_params = {non_const_inst, const_param_gv};
    tmp_addr.variant.address.variant.param_ref.param_num = 1;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);
    tmp_addr.variant.address.variant.param_ref.param_num = 2;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);
    state.current_function_params.clear();

    // subobject_path on address constant when offset == 0
    a_base_class sub_bc = {};
    sub_bc.offset = 8;
    a_type sub_elem_int_ty = dummy_int_ty;
    sub_elem_int_ty.size = 4;
    a_type sub_zero_int_ty = dummy_int_ty;
    sub_zero_int_ty.size = 0;
    a_type sub_arr_ty = {};
    sub_arr_ty.kind = tk_array;
    sub_arr_ty.variant.array.element_type = &sub_elem_int_ty;
    a_field sub_f_arr = {}, sub_f_scalar = {}, sub_f_nullty = {};
    sub_f_arr.offset = 4;
    sub_f_arr.type = &sub_arr_ty;
    sub_f_scalar.offset = 2;
    sub_f_scalar.type = &sub_zero_int_ty;
    sub_f_nullty.offset = 1;
    sub_f_nullty.type = nullptr;

    a_subobject_path sp1 = {}, sp2 = {}, sp3 = {}, sp4 = {}, sp5 = {}, sp6 = {}, sp6b = {}, sp7 = {};
    sp1.is_base_class = TRUE;
    sp1.variant.base_class = &sub_bc;
    sp1.next = &sp2;

    sp2.is_base_class = TRUE;
    sp2.variant.base_class = nullptr;
    sp2.next = &sp3;

    sp3.is_offset = TRUE; // curr_elem_ty == nullptr -> elem_sz = 1
    sp3.variant.ptr_offset = 3;
    sp3.next = &sp4;

    sp4.variant.field = &sub_f_arr; // sets curr_elem_ty = sub_elem_int_ty (size 4 > 0)
    sp4.next = &sp5;

    sp5.is_offset = TRUE; // curr_elem_ty->size == 4
    sp5.variant.ptr_offset = 2;
    sp5.next = &sp6;

    sp6.variant.field = &sub_f_scalar; // sets curr_elem_ty = sub_zero_int_ty (size 0)
    sp6.next = &sp6b;

    sp6b.is_offset = TRUE; // curr_elem_ty != nullptr && curr_elem_ty->size == 0
    sp6b.variant.ptr_offset = 1;
    sp6b.next = &sp7;

    sp7.variant.field = &sub_f_nullty;
    sp7.next = nullptr;

    a_subobject_path sp_null_field = {};
    sp_null_field.variant.field = nullptr;
    sp7.next = &sp_null_field;

    tmp_addr.variant.address.offset = 0;
    tmp_addr.variant.address.subobject_path = &sp1;
    assert(llvm_const_from_address(&tmp_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);
    tmp_addr.variant.address.subobject_path = nullptr;

    // Array designator (single, generic single, generic range, generic reversed range, null subscript, trailing designator) & ck_init_repeat
    a_constant arr_agg = {};
    arr_agg.kind = ck_aggregate;
    arr_agg.type = &sub_arr_ty;

    a_constant d_non_gen = {}, d_val1 = {}, d_gen_single = {}, d_val2 = {},
               d_gen_range = {}, d_val3 = {}, d_gen_rev = {}, d_val4 = {},
               d_gen_null = {}, d_val5 = {}, c_rep = {}, d_trail = {};
    a_constant sub_idx1 = {}, sub_idx2_lo = {}, sub_idx2_hi = {}, sub_rev_lo = {}, sub_rev_hi = {};
    sub_idx1.kind = ck_integer;
    assert(llvm_set_edg_sint64(&sub_idx1.variant.integer_value, 0) == llvm_gen_be_error_t::ok);
    sub_idx2_lo.kind = ck_integer;
    assert(llvm_set_edg_sint64(&sub_idx2_lo.variant.integer_value, 1) == llvm_gen_be_error_t::ok);
    sub_idx2_hi.kind = ck_integer;
    assert(llvm_set_edg_sint64(&sub_idx2_hi.variant.integer_value, 2) == llvm_gen_be_error_t::ok);
    sub_idx2_lo.next = &sub_idx2_hi;
    sub_rev_lo.kind = ck_integer;
    assert(llvm_set_edg_sint64(&sub_rev_lo.variant.integer_value, 2) == llvm_gen_be_error_t::ok);
    sub_rev_hi.kind = ck_integer;
    assert(llvm_set_edg_sint64(&sub_rev_hi.variant.integer_value, 1) == llvm_gen_be_error_t::ok);
    sub_rev_lo.next = &sub_rev_hi;

    d_non_gen.kind = ck_designator;
    d_non_gen.variant.designator.is_field_designator = FALSE;
    d_non_gen.variant.designator.is_generic = FALSE;
    d_non_gen.variant.designator.variant.array_element = 10; // idx >= expected_elems (4)
    d_non_gen.next = &d_val1;

    d_val1.kind = ck_integer;
    d_val1.type = &dummy_int_ty;
    assert(llvm_set_edg_sint64(&d_val1.variant.integer_value, 11) == llvm_gen_be_error_t::ok);
    d_val1.next = &d_gen_single;

    d_gen_single.kind = ck_designator;
    d_gen_single.variant.designator.is_field_designator = FALSE;
    d_gen_single.variant.designator.is_generic = TRUE;
    d_gen_single.variant.designator.variant.subscript = &sub_idx1;
    d_gen_single.next = &d_val2;

    d_val2.kind = ck_integer;
    d_val2.type = &dummy_int_ty;
    assert(llvm_set_edg_sint64(&d_val2.variant.integer_value, 22) == llvm_gen_be_error_t::ok);
    d_val2.next = &d_gen_range;

    d_gen_range.kind = ck_designator;
    d_gen_range.variant.designator.is_field_designator = FALSE;
    d_gen_range.variant.designator.is_generic = TRUE;
    d_gen_range.variant.designator.variant.subscript = &sub_idx2_lo;
    d_gen_range.next = &d_val3;

    d_val3.kind = ck_integer;
    d_val3.type = &dummy_int_ty;
    assert(llvm_set_edg_sint64(&d_val3.variant.integer_value, 33) == llvm_gen_be_error_t::ok);
    d_val3.next = &d_gen_rev;

    d_gen_rev.kind = ck_designator;
    d_gen_rev.variant.designator.is_field_designator = FALSE;
    d_gen_rev.variant.designator.is_generic = TRUE;
    d_gen_rev.variant.designator.variant.subscript = &sub_rev_lo;
    d_gen_rev.next = &d_val4;

    d_val4.kind = ck_integer;
    d_val4.type = &dummy_int_ty;
    assert(llvm_set_edg_sint64(&d_val4.variant.integer_value, 44) == llvm_gen_be_error_t::ok);
    d_val4.next = &d_gen_null;

    d_gen_null.kind = ck_designator;
    d_gen_null.variant.designator.is_field_designator = FALSE;
    d_gen_null.variant.designator.is_generic = TRUE;
    d_gen_null.variant.designator.variant.subscript = nullptr;
    d_gen_null.next = &d_val5;

    d_val5.kind = ck_integer;
    d_val5.type = &dummy_int_ty;
    assert(llvm_set_edg_sint64(&d_val5.variant.integer_value, 55) == llvm_gen_be_error_t::ok);
    d_val5.next = &c_rep;

    a_constant extra_arr_elem = {};
    extra_arr_elem.kind = ck_integer;
    extra_arr_elem.type = &dummy_int_ty;
    assert(llvm_set_edg_sint64(&extra_arr_elem.variant.integer_value, 66) == llvm_gen_be_error_t::ok);
    extra_arr_elem.next = &d_trail;

    c_rep.kind = ck_init_repeat;
    c_rep.variant.init_repeat.count = 5; // exceeds expected_elems (4) so cur_idx < expected_elems is both true and false!
    c_rep.variant.init_repeat.constant = &elem1;
    elem1.next = nullptr;
    c_rep.next = &extra_arr_elem; // regular element when cur_idx >= expected_elems!

    d_trail.kind = ck_designator;
    d_trail.variant.designator.is_field_designator = FALSE;
    d_trail.next = nullptr; // c->next == nullptr break!

    llvm::ArrayType* arr4_ty = llvm::ArrayType::get(i32_ty, 4);
    arr_agg.variant.aggregate.first_constant = &d_non_gen;
    assert(llvm_const_from_aggregate(&arr_agg, arr4_ty, &out) == llvm_gen_be_error_t::ok);

    // Error propagation in array designator value and ck_init_repeat
    d_non_gen.next = &bad_elem;
    assert(llvm_const_from_aggregate(&arr_agg, arr4_ty, &out) == llvm_gen_be_error_t::unsupported_expr);
    c_rep.variant.init_repeat.constant = &bad_elem;
    arr_agg.variant.aggregate.first_constant = &c_rep;
    assert(llvm_const_from_aggregate(&arr_agg, arr4_ty, &out) == llvm_gen_be_error_t::unsupported_expr);

    // Union with ck_designator and pointer-to-integer coercion + pointer-to-non-integer fallback
    a_constant u_desig = {}, u_ptr_c = {};
    u_desig.kind = ck_designator;
    u_desig.variant.designator.is_field_designator = TRUE;
    u_desig.next = &u_ptr_c;
    u_ptr_c.kind = ck_address;
    u_ptr_c.type = &dummy_ptr_ty;
    u_ptr_c.variant.address.kind = abk_variable;
    u_ptr_c.variant.address.variant.variable = &dummy_var;
    agg_con.type = &un_edg_ty;
    agg_con.variant.aggregate.first_constant = &u_desig;
    assert(llvm_const_from_aggregate(&agg_con, multi_un_st, &out) == llvm_gen_be_error_t::ok);
    assert(llvm_const_from_aggregate(&agg_con, un_bytes_ty, &out) == llvm_gen_be_error_t::ok);

    // Struct with initializes_empty_object, field designator (direct & generic by name), and designator past end
    a_field sf1 = {}, sf2 = {}, sf3 = {};
    sf1.source_corresp.name = "f1";
    sf1.type = &dummy_int_ty;
    sf1.next = &sf2;
    sf2.source_corresp.name = nullptr;
    sf2.type = &dummy_int_ty;
    sf2.next = &sf3;
    sf3.source_corresp.name = "f3";
    sf3.type = &dummy_int_ty;
    sf3.next = nullptr;

    a_type st_desig_edg_ty = {};
    st_desig_edg_ty.kind = tk_struct;
    st_desig_edg_ty.variant.class_struct_union.field_list = &sf1;
    llvm::StructType* st3_ty = llvm::StructType::get(*state.context, {i32_ty, i32_ty, i32_ty});

    a_constant c_empty = {}, c_fdesig_gen = {}, c_fval = {}, c_fdesig_direct = {},
               c_fdesig_nullname = {}, c_fdesig_nomatch = {};
    c_empty.kind = ck_aggregate;
#if DO_IL_LOWERING
    c_empty.initializes_empty_object = TRUE;
#endif
    c_empty.next = &c_fdesig_gen;

    c_fdesig_gen.kind = ck_designator;
    c_fdesig_gen.variant.designator.is_field_designator = TRUE;
    c_fdesig_gen.variant.designator.is_generic = TRUE;
    c_fdesig_gen.variant.designator.variant.field_name = "f3";
    c_fdesig_gen.next = &c_fval;

    c_fval.kind = ck_integer;
    c_fval.type = &dummy_int_ty;
    assert(llvm_set_edg_sint64(&c_fval.variant.integer_value, 99) == llvm_gen_be_error_t::ok);
    c_fval.next = nullptr;

    agg_con.type = &st_desig_edg_ty;
    agg_con.variant.aggregate.first_constant = &c_empty;
    assert(llvm_const_from_aggregate(&agg_con, st3_ty, &out) == llvm_gen_be_error_t::ok);

    // Direct field designator & designator exceeding st_ty->getNumElements()
    c_fdesig_direct.kind = ck_designator;
    c_fdesig_direct.variant.designator.is_field_designator = TRUE;
    c_fdesig_direct.variant.designator.is_generic = FALSE;
    c_fdesig_direct.variant.designator.variant.field = &sf3;
    c_fdesig_direct.next = &c_fval;
    agg_con.variant.aggregate.first_constant = &c_fdesig_direct;
    assert(llvm_const_from_aggregate(&agg_con, one_elem_st, &out) == llvm_gen_be_error_t::ok);

    // Generic field designator with null field_name and non-matching field_name
    c_fdesig_nullname.kind = ck_designator;
    c_fdesig_nullname.variant.designator.is_field_designator = TRUE;
    c_fdesig_nullname.variant.designator.is_generic = TRUE;
    c_fdesig_nullname.variant.designator.variant.field_name = nullptr;
    c_fdesig_nullname.next = &c_fdesig_nomatch;

    c_fdesig_nomatch.kind = ck_designator;
    c_fdesig_nomatch.variant.designator.is_field_designator = TRUE;
    c_fdesig_nomatch.variant.designator.is_generic = TRUE;
    c_fdesig_nomatch.variant.designator.variant.field_name = "nonexistent";
    c_fdesig_nomatch.next = &c_fval;
    c_fval.next = nullptr;
    agg_con.variant.aggregate.first_constant = &c_fdesig_nullname;
    assert(llvm_const_from_aggregate(&agg_con, st3_ty, &out) == llvm_gen_be_error_t::ok);

    // ck_init_repeat inside struct aggregate (valid repeat, count < elements, count exceeding elements, field==nullptr, and error propagation)
    a_constant st_rep = {};
    st_rep.kind = ck_init_repeat;
    st_rep.variant.init_repeat.count = 1;
    st_rep.variant.init_repeat.constant = &c_fval;
    st_rep.next = nullptr;
    agg_con.variant.aggregate.first_constant = &st_rep;
    assert(llvm_const_from_aggregate(&agg_con, st3_ty, &out) == llvm_gen_be_error_t::ok);
    st_rep.variant.init_repeat.count = 5;
    sf2.next = nullptr; // only 2 EDG fields for 3-element st3_ty so field becomes nullptr during repeat!
    assert(llvm_const_from_aggregate(&agg_con, st3_ty, &out) == llvm_gen_be_error_t::ok);
    st_rep.variant.init_repeat.constant = &bad_elem;
    assert(llvm_const_from_aggregate(&agg_con, st3_ty, &out) == llvm_gen_be_error_t::unsupported_expr);
    sf2.next = &sf3;

    // Test pointer-typed ck_integer constants in llvm_const_from_integer (low==0 && high==0, low==0 && high!=0, low!=0)
    a_constant ptr_int_con = {};
    ptr_int_con.kind = ck_integer;
    ptr_int_con.type = &dummy_int_ty;
    assert(llvm_set_edg_sint64(&ptr_int_con.variant.integer_value, 0) == llvm_gen_be_error_t::ok);
    assert(llvm_const_from_integer(&ptr_int_con, ptr_ty, &out) == llvm_gen_be_error_t::ok);
    assert(llvm_set_edg_sint64(&ptr_int_con.variant.integer_value, 0x1000) == llvm_gen_be_error_t::ok);
    assert(llvm_const_from_integer(&ptr_int_con, ptr_ty, &out) == llvm_gen_be_error_t::ok);
#if INT128_EXTENSIONS_ALLOWED
    assert(llvm_set_edg_int128(&ptr_int_con.variant.integer_value, 0ULL, 1ULL) == llvm_gen_be_error_t::ok);
    assert(llvm_const_from_integer(&ptr_int_con, ptr_ty, &out) == llvm_gen_be_error_t::ok);
#endif

    // Phase 2.1: ScalableVectorType constants in llvm_const_from_aggregate (empty, non-empty splat, and error)
    llvm::Type* sv4i32_ty = llvm::ScalableVectorType::get(i32_ty, 4);
    agg_con.variant.aggregate.first_constant = nullptr;
    assert(llvm_const_from_aggregate(&agg_con, sv4i32_ty, &out) == llvm_gen_be_error_t::ok);
    c_fval.next = nullptr;
    agg_con.variant.aggregate.first_constant = &c_fval;
    assert(llvm_const_from_aggregate(&agg_con, sv4i32_ty, &out) == llvm_gen_be_error_t::ok);
    agg_con.variant.aggregate.first_constant = &bad_elem;
    assert(llvm_const_from_aggregate(&agg_con, sv4i32_ty, &out) == llvm_gen_be_error_t::unsupported_expr);

    // Section 7.1: ck_stack_offset and ck_template_param in evaluate_constant and llvm_const_from_template_param
    a_constant stack_off_con = {};
#if DO_IL_LOWERING && GENERATE_EH_TABLES && !DO_FULL_PORTABLE_EH_LOWERING
    stack_off_con.kind = ck_stack_offset;
    stack_off_con.variant.stack_offset.offset = 32;
    assert(evaluate_constant(&stack_off_con, i32_ty, &out) == llvm_gen_be_error_t::ok);
#else
    assert(llvm_set_edg_sint64(&stack_off_con.variant.integer_value, 32) == llvm_gen_be_error_t::ok);
    assert(llvm_const_from_stack_offset(&stack_off_con, i32_ty, &out) == llvm_gen_be_error_t::ok);
#endif

    assert(llvm_const_from_template_param(nullptr, i32_ty, &out) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_template_param(&c_fval, nullptr, &out) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_const_from_template_param(&c_fval, i32_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);

    a_constant tp_con = {};
    tp_con.kind = ck_template_param;
    tp_con.variant.template_param.kind = tpck_dependent_constant;
    tp_con.variant.template_param.variant.constant = &c_fval;
    assert(evaluate_constant(&tp_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    tp_con.variant.template_param.kind = tpck_address;
    tp_con.variant.template_param.variant.constant = &c_fval;
    assert(evaluate_constant(&tp_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    an_expr_node tp_expr = {};
    tp_expr.kind = enk_constant;
    tp_expr.variant.constant.ptr = &c_fval;
    tp_con.variant.template_param.kind = tpck_expression;
    tp_con.variant.template_param.variant.expr = &tp_expr;
    assert(evaluate_constant(&tp_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    tp_con.variant.template_param.kind = tpck_sizeof;
    tp_con.variant.template_param.variant.templ_sizeof.type = &dummy_int_ty;
    assert(evaluate_constant(&tp_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    tp_con.variant.template_param.kind = tpck_datasizeof;
    assert(evaluate_constant(&tp_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    tp_con.variant.template_param.kind = tpck_alignof;
    assert(evaluate_constant(&tp_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    tp_con.variant.template_param.kind = tpck_param;
    assert(evaluate_constant(&tp_con, i32_ty, &out) == llvm_gen_be_error_t::constant_eval_failure);

#if FIXED_POINT_ALLOWED
    // Workstream 9: Test ck_fixed_point constant evaluation (with integer expected_ty, non-integer expected_ty, null expected_ty, and invalid con->type)
    a_type fx_con_ty = {};
    fx_con_ty.kind = tk_fixed_point;
    fx_con_ty.size = 4;
    a_constant fx_con = {};
    fx_con.kind = ck_fixed_point;
    fx_con.type = &fx_con_ty;
    fx_con.variant.fixed_point_value = 0x4000;
    assert(evaluate_constant(&fx_con, i32_ty, &out) == llvm_gen_be_error_t::ok);
    assert(evaluate_constant(&fx_con, ptr_ty, &out) == llvm_gen_be_error_t::ok);
    assert(evaluate_constant(&fx_con, nullptr, &out) == llvm_gen_be_error_t::ok);
    a_type bad_fx_ty = {};
    bad_fx_ty.kind = static_cast<a_type_kind>(999);
    fx_con.type = &bad_fx_ty;
    assert(evaluate_constant(&fx_con, nullptr, &out) == llvm_gen_be_error_t::unsupported_type);
#endif

    // Phase 7: Test llvm_pack_constant_bitfield_bytes on little-endian, big-endian, and null arguments
    a_field* bf_cur = &bf1;
    a_constant* con_cur = &bf_c1;
    assert(llvm_pack_constant_bitfield_bytes(nullptr, &con_cur, i8_ty, &out) == llvm_gen_be_error_t::invalid_argument);
    a_field* null_bf_cur = nullptr;
    assert(llvm_pack_constant_bitfield_bytes(&null_bf_cur, &con_cur, i8_ty, &out) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_pack_constant_bitfield_bytes(&bf_cur, nullptr, i8_ty, &out) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_pack_constant_bitfield_bytes(&bf_cur, &con_cur, nullptr, &out) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_pack_constant_bitfield_bytes(&bf_cur, &con_cur, i8_ty, nullptr) == llvm_gen_be_error_t::invalid_argument);

    state.module->setDataLayout("E-m:e-i64:64-n32:64");
    bf_cur = &bf1;
    con_cur = &bf_c1;
    assert(llvm_pack_constant_bitfield_bytes(&bf_cur, &con_cur, i8_ty, &out) == llvm_gen_be_error_t::ok);
    state.module->setDataLayout("");

    // Test llvm_extract_edg_int128 helper for signed, unsigned, and invalid_argument cases
    uint64_t ext_lo = 0, ext_hi = 0;
    assert(llvm_extract_edg_int128(c_fval.variant.integer_value, true, nullptr, &ext_hi) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_extract_edg_int128(c_fval.variant.integer_value, true, &ext_lo, nullptr) == llvm_gen_be_error_t::invalid_argument);
    assert(llvm_extract_edg_int128(c_fval.variant.integer_value, true, &ext_lo, &ext_hi) == llvm_gen_be_error_t::ok);
    assert(ext_lo == 99 && ext_hi == 0);
    assert(llvm_extract_edg_int128(c_fval.variant.integer_value, false, &ext_lo, &ext_hi) == llvm_gen_be_error_t::ok);
    assert(ext_lo == 99 && ext_hi == 0);

    // Test ck_address with expected_ty->isIntegerTy() (e.g. pointer-to-integer constant expression)
    a_constant addr_str_c = {};
    addr_str_c.kind = ck_string;
    addr_str_c.variant.string.value = "ptr_int_str";
    addr_str_c.variant.string.length = 12;
    a_constant addr_int_con = {};
    addr_int_con.kind = ck_address;
    addr_int_con.variant.address.kind = abk_constant;
    addr_int_con.variant.address.variant.constant = &addr_str_c;
    assert(evaluate_constant(&addr_int_con, i32_ty, &out) == llvm_gen_be_error_t::ok);

    // =========================================================================
    // Wave 4: Exhaustive coverage for llvm_gen_be_const.cpp
    // =========================================================================
    {
      state.type_cache.clear();
      int_kind_is_signed[ik_int] = TRUE;
      int_kind_is_signed[ik_bit_precise] = TRUE;
      int_kind_is_signed[ik_unsigned_int] = FALSE;
      int_kind_is_signed[ik_unsigned_bit_precise] = FALSE;

      // 1) llvm_extract_edg_apint invalid args & OOM
      llvm::APInt ap_tmp(128, 0);
      assert(llvm_extract_edg_apint(dummy_const.variant.integer_value, 64, true, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(llvm_extract_edg_apint(dummy_const.variant.integer_value, 0, true, &ap_tmp) == llvm_gen_be_error_t::invalid_argument);
      for (int b = 0; b < 4; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_extract_edg_apint(dummy_const.variant.integer_value, 64, true, &ap_tmp);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }

      // 2) llvm_const_from_integer: null ctype, tk_nullptr (null/ptr/int expected_ty), null/non-int expected_ty, non-zero ptr int, bool_type (0 and non-0)
      llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*state.context);
      llvm::Type* i256_ty = llvm::IntegerType::get(*state.context, 256);

      a_constant c_notype = {}; c_notype.kind = ck_integer; c_notype.type = nullptr; assert(llvm_set_edg_sint64(&c_notype.variant.integer_value, 5) == llvm_gen_be_error_t::ok);
      assert(llvm_const_from_integer(&c_notype, nullptr, &out) == llvm_gen_be_error_t::invalid_argument);
      assert(llvm_const_from_integer(&c_notype, f32_ty, &out) == llvm_gen_be_error_t::invalid_argument);
      assert(llvm_const_from_integer(&c_notype, ptr_ty, &out) == llvm_gen_be_error_t::ok); // non-zero ptr int (L281 false)

      a_type nullptr_ty = {}; nullptr_ty.kind = tk_nullptr;
      a_constant c_nullp = {}; c_nullp.kind = ck_integer; c_nullp.type = &nullptr_ty;
      assert(llvm_const_from_integer(&c_nullp, nullptr, &out) == llvm_gen_be_error_t::ok);
      assert(llvm_const_from_integer(&c_nullp, ptr_ty, &out) == llvm_gen_be_error_t::ok);
      assert(llvm_const_from_integer(&c_nullp, i32_ty, &out) == llvm_gen_be_error_t::ok);

      a_type bool_edg_ty = {}; bool_edg_ty.kind = tk_integer; bool_edg_ty.variant.integer.bool_type = TRUE;
      a_constant c_bool0 = {}, c_bool1 = {};
      c_bool0.kind = ck_integer; c_bool0.type = &bool_edg_ty; assert(llvm_set_edg_sint64(&c_bool0.variant.integer_value, 0) == llvm_gen_be_error_t::ok);
      c_bool1.kind = ck_integer; c_bool1.type = &bool_edg_ty; assert(llvm_set_edg_sint64(&c_bool1.variant.integer_value, 7) == llvm_gen_be_error_t::ok);
      assert(llvm_const_from_integer(&c_bool0, llvm::Type::getInt1Ty(*state.context), &out) == llvm_gen_be_error_t::ok);
      assert(llvm_const_from_integer(&c_bool1, llvm::Type::getInt1Ty(*state.context), &out) == llvm_gen_be_error_t::ok);

      // OOM sweep on 64-bit integer (hits L278)
      g_llvm_be_oom_countdown = 0;
      assert(llvm_const_from_integer(&c_notype, i32_ty, &out) == llvm_gen_be_error_t::out_of_memory);
      g_llvm_be_oom_countdown = -1;

      // 3) try_fold_wide_int_expr across all unary/binary/constant/fallback paths (bit_width = 256)
      a_type sint_ty = {}, uint_ty = {}, bp_s_ty = {}, bp_u_ty = {}, bp_noext_ty = {}, sz0_int_ty = {}, flt_ty = {};
      sint_ty.kind = tk_integer; sint_ty.size = 4; sint_ty.variant.integer.int_kind = ik_int;
      uint_ty.kind = tk_integer; uint_ty.size = 4; uint_ty.variant.integer.int_kind = ik_unsigned_int;
      an_integer_type_supplement bp_supp_200 = {}, bp_supp_0 = {};
      bp_supp_200.bit_width = 200; bp_supp_0.bit_width = 0;
      bp_s_ty.kind = tk_integer; bp_s_ty.variant.integer.int_kind = ik_bit_precise; bp_s_ty.variant.integer.extra_info = &bp_supp_200;
      bp_u_ty.kind = tk_integer; bp_u_ty.variant.integer.int_kind = ik_unsigned_bit_precise; bp_u_ty.variant.integer.extra_info = &bp_supp_0; bp_u_ty.size = 8;
      bp_noext_ty.kind = tk_integer; bp_noext_ty.variant.integer.int_kind = ik_bit_precise; bp_noext_ty.variant.integer.extra_info = nullptr; bp_noext_ty.size = 0;
      sz0_int_ty.kind = tk_integer; sz0_int_ty.size = 0; sz0_int_ty.variant.integer.int_kind = ik_unsigned_int;
      flt_ty.kind = tk_float; flt_ty.variant.float_kind = fk_double;

      a_constant leaf_c1 = {}, leaf_c0 = {}, leaf_flt_c = {}, nested_c_ok = {}, nested_c_fail = {}, self_c = {};
      leaf_c1.kind = ck_integer; leaf_c1.type = &sint_ty; assert(llvm_set_edg_sint64(&leaf_c1.variant.integer_value, 9) == llvm_gen_be_error_t::ok);
      leaf_c0.kind = ck_integer; leaf_c0.type = &uint_ty; assert(llvm_set_edg_sint64(&leaf_c0.variant.integer_value, 0) == llvm_gen_be_error_t::ok);
      leaf_flt_c.kind = ck_float; leaf_flt_c.type = &flt_ty;

      an_expr_node e_leaf1 = {}, e_leaf2 = {}, e_leaf0 = {}, e_var = {}, e_null_const = {}, e_flt_const = {}, e_self_const = {};
      e_leaf1.kind = enk_constant; e_leaf1.type = &bp_s_ty; e_leaf1.variant.constant.ptr = &leaf_c1;
      e_leaf2.kind = enk_constant; e_leaf2.type = &bp_u_ty; e_leaf2.variant.constant.ptr = &leaf_c1;
      e_leaf0.kind = enk_constant; e_leaf0.type = &uint_ty; e_leaf0.variant.constant.ptr = &leaf_c0;
      e_var.kind = enk_variable; e_var.type = nullptr; // non-operation, null type -> folded = false
      e_null_const.kind = enk_constant; e_null_const.type = &flt_ty; e_null_const.variant.constant.ptr = nullptr;
      e_flt_const.kind = enk_constant; e_flt_const.type = &sint_ty; e_flt_const.variant.constant.ptr = &leaf_flt_c;

      self_c.kind = ck_integer; assert(llvm_set_edg_sint64(&self_c.variant.integer_value, 3) == llvm_gen_be_error_t::ok); self_c.expr = &e_self_const;
      e_self_const.kind = enk_constant; e_self_const.type = &sint_ty; e_self_const.variant.constant.ptr = &self_c;

      an_expr_node e_neg_inner = {};
      e_neg_inner.kind = enk_operation; e_neg_inner.type = &sint_ty;
      e_neg_inner.variant.operation.kind = eok_negate; e_neg_inner.variant.operation.operands = &e_leaf1;
      nested_c_ok.kind = ck_integer; assert(llvm_set_edg_sint64(&nested_c_ok.variant.integer_value, 1) == llvm_gen_be_error_t::ok); nested_c_ok.expr = &e_neg_inner;
      nested_c_fail.kind = ck_integer; assert(llvm_set_edg_sint64(&nested_c_fail.variant.integer_value, 2) == llvm_gen_be_error_t::ok); nested_c_fail.expr = &e_var;

      an_expr_node e_nested_ok = {}, e_nested_fail = {};
      e_nested_ok.kind = enk_constant; e_nested_ok.type = &sint_ty; e_nested_ok.variant.constant.ptr = &nested_c_ok;
      e_nested_fail.kind = enk_constant; e_nested_fail.type = &uint_ty; e_nested_fail.variant.constant.ptr = &nested_c_fail;

      auto eval_wide_expr = [&](an_expr_node_ptr ex) noexcept {
        a_constant wc = {};
        wc.kind = ck_integer;
        wc.type = &sint_ty;
        assert(llvm_set_edg_sint64(&wc.variant.integer_value, 11) == llvm_gen_be_error_t::ok);
        wc.expr = ex;
        assert(llvm_const_from_integer(&wc, i256_ty, &out) == llvm_gen_be_error_t::ok);
      };

      eval_wide_expr(&e_var);
      eval_wide_expr(&e_null_const);
      eval_wide_expr(&e_flt_const);
      eval_wide_expr(&e_self_const);
      eval_wide_expr(&e_nested_ok);
      eval_wide_expr(&e_nested_fail);

      // Operation with null operands
      an_expr_node e_no_ops = {}; e_no_ops.kind = enk_operation; e_no_ops.variant.operation.operands = nullptr;
      eval_wide_expr(&e_no_ops);

      // Unary ops (eok_cast, eok_negate, eok_complement) across all sub_ty variants + !f1
      for (an_expr_operator_kind uok : {eok_cast, eok_negate, eok_complement}) {
        for (a_type_ptr sty : {static_cast<a_type_ptr>(nullptr), &flt_ty, &bp_s_ty, &bp_u_ty, &bp_noext_ty, &sint_ty, &uint_ty, &sz0_int_ty}) {
          an_expr_node op1_u = e_leaf1;
          op1_u.type = sty;
          an_expr_node eu = {};
          eu.kind = enk_operation; eu.type = &sint_ty;
          eu.variant.operation.kind = uok; eu.variant.operation.operands = &op1_u;
          eval_wide_expr(&eu);
        }
      }
      an_expr_node eu_unfolded = {};
      eu_unfolded.kind = enk_operation; eu_unfolded.variant.operation.kind = eok_cast; eu_unfolded.variant.operation.operands = &e_var;
      eval_wide_expr(&eu_unfolded);

      // Binary ops: !op2, !f1, !f2, and all operators (signed & unsigned, zero & non-zero rhs)
      an_expr_node op1_only = e_leaf1; op1_only.next = nullptr;
      an_expr_node eb_noop2 = {}; eb_noop2.kind = enk_operation; eb_noop2.variant.operation.kind = eok_add; eb_noop2.variant.operation.operands = &op1_only;
      eval_wide_expr(&eb_noop2);

      an_expr_node op1_bad = e_var, op2_good = e_leaf2; op1_bad.next = &op2_good;
      an_expr_node eb_bad1 = {}; eb_bad1.kind = enk_operation; eb_bad1.variant.operation.kind = eok_add; eb_bad1.variant.operation.operands = &op1_bad;
      eval_wide_expr(&eb_bad1);

      an_expr_node op1_good = e_leaf1, op2_bad = e_var; op1_good.next = &op2_bad;
      an_expr_node eb_bad2 = {}; eb_bad2.kind = enk_operation; eb_bad2.variant.operation.kind = eok_add; eb_bad2.variant.operation.operands = &op1_good;
      eval_wide_expr(&eb_bad2);

      for (an_expr_operator_kind bok : {eok_add, eok_subtract, eok_multiply, eok_divide, eok_remainder,
                                        eok_shiftl, eok_shiftr, eok_and, eok_or, eok_xor, eok_assign}) {
        for (a_type_ptr bty : {&sint_ty, &uint_ty}) {
          for (an_expr_node* rhs_src : {&e_leaf2, &e_leaf0}) {
            an_expr_node lhs_n = e_leaf1, rhs_n = *rhs_src;
            lhs_n.next = &rhs_n; rhs_n.next = nullptr;
            an_expr_node eb = {};
            eb.kind = enk_operation; eb.type = bty;
            eb.variant.operation.kind = bok; eb.variant.operation.operands = &lhs_n;
            eval_wide_expr(&eb);
          }
        }
      }

      // OOM sweeps across wide int folding (nested constant, unary, binary, fallback extract_apint)
      for (an_expr_node_ptr oom_ex : {&e_nested_ok, &e_nested_fail, &e_neg_inner, &e_var}) {
        a_constant wc = {}; wc.kind = ck_integer; wc.type = &sint_ty; assert(llvm_set_edg_sint64(&wc.variant.integer_value, 5) == llvm_gen_be_error_t::ok); wc.expr = oom_ex;
        for (int b = 0; b < 12; ++b) {
          g_llvm_be_oom_countdown = b;
          llvm_gen_be_error_t r = llvm_const_from_integer(&wc, i256_ty, &out);
          g_llvm_be_oom_countdown = -1;
          if (r == llvm_gen_be_error_t::ok) break;
        }
      }
      {
        an_expr_node lhs_n = e_leaf1, rhs_n = e_leaf2; lhs_n.next = &rhs_n;
        an_expr_node eb = {}; eb.kind = enk_operation; eb.type = &sint_ty;
        eb.variant.operation.kind = eok_add; eb.variant.operation.operands = &lhs_n;
        a_constant wc = {}; wc.kind = ck_integer; wc.type = &sint_ty; wc.expr = &eb;
        for (int b = 0; b < 12; ++b) {
          g_llvm_be_oom_countdown = b;
          llvm_gen_be_error_t r = llvm_const_from_integer(&wc, i256_ty, &out);
          g_llvm_be_oom_countdown = -1;
          if (r == llvm_gen_be_error_t::ok) break;
        }
      }

      // 4) Decimal floating-point BID/DPD encoding & build_decimal_fp_constant & OOM
      assert(llvm_encode_decimal_float_bid_dpd(4, false, false, 0, 1, 0, nullptr) == llvm_gen_be_error_t::invalid_argument);
      assert(llvm_encode_decimal_float_bid_dpd(3, false, false, 0, 1, 0, &out) == llvm_gen_be_error_t::unsupported_type);
      // byte_width = 4, 8, 16; biased_exp < 0, normal, > max; BID small & large_sig; negative & positive
      assert(llvm_encode_decimal_float_bid_dpd(4, false, true, -9999, 123, 0, &out) == llvm_gen_be_error_t::ok);
      assert(llvm_encode_decimal_float_bid_dpd(8, false, false, 9999, ~0ULL, ~0ULL, &out) == llvm_gen_be_error_t::ok);
      assert(llvm_encode_decimal_float_bid_dpd(16, false, false, 0, 42, 0, &out) == llvm_gen_be_error_t::ok);
      // DPD all 8 declet patterns (000, 008, 080, 088, 800, 808, 880, 888) and lead_d < 8 vs >= 8
      assert(llvm_encode_decimal_float_bid_dpd(8, true, false, 0, 800088080008000ULL, 0, &out) == llvm_gen_be_error_t::ok);
      assert(llvm_encode_decimal_float_bid_dpd(8, true, true, 0, 9888880808000000ULL, 0, &out) == llvm_gen_be_error_t::ok);
      for (int b = 0; b < 8; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_encode_decimal_float_bid_dpd(4, true, false, 0, 123, 0, &out);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }

      // llvm_const_from_float: null/non-FP expected_ty + _Decimal64 across x86_64, ppc64le, s390x and fp_to_string modes 3,4,5,6 + OOM
      a_constant flt_bad = {}; flt_bad.kind = ck_float; flt_bad.type = &flt_ty;
      assert(llvm_const_from_float(&flt_bad, nullptr, &out) == llvm_gen_be_error_t::invalid_argument);
      assert(llvm_const_from_float(&flt_bad, i32_ty, &out) == llvm_gen_be_error_t::invalid_argument);

      a_type dec64_tref = {};
      dec64_tref.kind = tk_typeref;
      dec64_tref.source_corresp.name = (char*)"_Decimal64";
      dec64_tref.variant.typeref.type = &flt_ty;
      a_constant dec_con = {};
      dec_con.kind = ck_float;
      dec_con.type = &dec64_tref;
      for (const char* tr : {"x86_64-unknown-linux-gnu", "powerpc64le-unknown-linux-gnu", "s390x-ibm-linux-gnu"}) {
        EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), tr);
        for (unsigned char mode : {static_cast<unsigned char>(3), static_cast<unsigned char>(4),
                                   static_cast<unsigned char>(5), static_cast<unsigned char>(6)}) {
          dec_con.variant.float_value.bytes[0] = mode;
          for (int b = 0; b < 10; ++b) {
            g_llvm_be_oom_countdown = b;
            llvm_gen_be_error_t r = llvm_const_from_float(&dec_con, llvm::Type::getInt64Ty(*state.context), &out);
            g_llvm_be_oom_countdown = -1;
            if (r == llvm_gen_be_error_t::ok) break;
          }
        }
      }
      EDG_LLVM_SET_TARGET_TRIPLE(state.module.get(), "x86_64-unknown-linux-gnu");

      // 5) String, Address (local_vars Constant & Alloca, temporary, typeid, uuidof, subobject_path) OOM sweeps
      a_type str_arr_edg = {}, char_edg = {};
      char_edg.kind = tk_integer; char_edg.size = 1; char_edg.variant.integer.int_kind = ik_char;
      str_arr_edg.kind = tk_array; str_arr_edg.size = 4;
      str_arr_edg.variant.array.variant.number_of_elements = 4; str_arr_edg.variant.array.element_type = &char_edg;
      a_constant str_oom_c = {};
      str_oom_c.kind = ck_string; str_oom_c.type = &str_arr_edg;
      str_oom_c.variant.string.value = "oom"; str_oom_c.variant.string.length = 4;
      for (int b = 0; b < 5; ++b) {
        state.string_literal_cache.clear();
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_const_from_string(&str_oom_c, ptr_ty, &out);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }

      // abk_variable with local_vars containing Constant* AND non-Constant AllocaInst* + OOM
      a_variable addr_v = {}; addr_v.source_corresp.name = (char*)"w4_addr_var"; addr_v.type = &sint_ty;
      llvm::GlobalVariable* addr_gv_c = llvm::cast<llvm::GlobalVariable>(
          state.module->getOrInsertGlobal("w4_local_const_gv", i32_ty));
      addr_gv_c->setConstant(false);
      addr_gv_c->setLinkage(llvm::GlobalValue::InternalLinkage);
      addr_gv_c->setInitializer(llvm::ConstantInt::get(i32_ty, 0));
      assert(addr_gv_c != nullptr);
      state.local_vars[&addr_v] = addr_gv_c;
      a_constant addr_v_con = {}; addr_v_con.kind = ck_address;
      addr_v_con.variant.address.kind = abk_variable; addr_v_con.variant.address.variant.variable = &addr_v;
      assert(llvm_const_from_address(&addr_v_con, ptr_ty, &out) == llvm_gen_be_error_t::ok);
      assert(out == addr_gv_c);

      llvm::AllocaInst* addr_alloca = state.builder->CreateAlloca(i32_ty);
      state.local_vars[&addr_v] = addr_alloca; // dyn_cast_or_null<Constant> == nullptr -> falls back to symbol name!
      for (int b = 0; b < 6; ++b) {
        if (auto* g = state.module->getNamedGlobal("w4_addr_var")) g->eraseFromParent();
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_const_from_address(&addr_v_con, ptr_ty, &out);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }
      state.local_vars.erase(&addr_v);

      // abk_temporary, abk_typeid, abk_uuidof, and subobject_path OOM sweeps
      a_subobject_path sp_node = {}; sp_node.is_offset = TRUE; sp_node.variant.ptr_offset = 4;
      a_constant tmp_inner = {}; tmp_inner.kind = ck_integer; tmp_inner.type = &sint_ty; assert(llvm_set_edg_sint64(&tmp_inner.variant.integer_value, 3) == llvm_gen_be_error_t::ok);
      a_constant addr_tmp_c = {}; addr_tmp_c.kind = ck_address;
      addr_tmp_c.variant.address.kind = abk_temporary; addr_tmp_c.variant.address.variant.constant = &tmp_inner;
      addr_tmp_c.variant.address.subobject_path = &sp_node;
      for (int b = 0; b < 10; ++b) {
        state.temporary_constant_cache.clear();
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_const_from_address(&addr_tmp_c, ptr_ty, &out);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }

      a_constant addr_ti_c = {}; addr_ti_c.kind = ck_address;
      addr_ti_c.variant.address.kind = abk_typeid; addr_ti_c.variant.address.variant.type = &sint_ty;
      for (int b = 0; b < 6; ++b) {
        if (auto* g = state.module->getNamedGlobal("_ZTI_mock")) g->eraseFromParent();
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_const_from_address(&addr_ti_c, ptr_ty, &out);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }

      a_class_type_supplement uuid_supp = {};
      uuid_supp.uuid_string = (char*)"01234567-89ab-CDEF-0123-456789abcdef";
      a_type uuid_cls = {}; uuid_cls.kind = tk_struct; uuid_cls.source_corresp.name = (char*)"W4Uuid";
      uuid_cls.variant.class_struct_union.extra_info = &uuid_supp;
      a_constant addr_uuid_c = {}; addr_uuid_c.kind = ck_address;
      addr_uuid_c.variant.address.kind = abk_uuidof; addr_uuid_c.variant.address.variant.type = &uuid_cls;
      for (int b = 0; b < 45; ++b) {
        if (auto* g = state.module->getNamedGlobal("__uuidof_W4Uuid")) g->eraseFromParent();
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_const_from_address(&addr_uuid_c, ptr_ty, &out);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }

      // 6) llvm_pack_constant_bitfield_bytes: non-int/non-i8-array error, [3 x i8] LE & BE, named/unnamed/0-width/implicit/non-int constants & OOM
      a_field bf_named = {}, bf_un1 = {}, bf_un2 = {}, bf_zero = {};
      bf_named.source_corresp.name = (char*)"bf1"; bf_named.is_bit_field = TRUE; bf_named.bit_size = 4; bf_named.offset = 0; bf_named.next = &bf_un1;
      bf_un1.source_corresp.name = nullptr; bf_un1.is_bit_field = TRUE; bf_un1.bit_size = 4; bf_un1.offset = 0; bf_un1.offset_bit_remainder = 4; bf_un1.next = &bf_un2;
      bf_un2.source_corresp.name = (char*)""; bf_un2.is_bit_field = TRUE; bf_un2.bit_size = 4; bf_un2.offset = 1; bf_un2.next = &bf_zero;
      bf_zero.source_corresp.name = nullptr; bf_zero.is_bit_field = TRUE; bf_zero.bit_size = 0; bf_zero.offset = 2; bf_zero.next = nullptr;

      a_constant bfc1 = {}, bfc2 = {}, bfc3 = {};
      bfc1.kind = ck_integer; bfc1.type = &sint_ty; assert(llvm_set_edg_sint64(&bfc1.variant.integer_value, 3) == llvm_gen_be_error_t::ok); bfc1.next = &bfc2;
      bfc2.kind = ck_integer; bfc2.type = &sint_ty; assert(llvm_set_edg_sint64(&bfc2.variant.integer_value, 5) == llvm_gen_be_error_t::ok); bfc2.implicit_aggr_element = FALSE; bfc2.next = &bfc3;
      bfc3.kind = ck_float; bfc3.type = &flt_ty; bfc3.implicit_aggr_element = TRUE; bfc3.next = nullptr;

      a_field_ptr f_cur = &bf_named;
      a_constant_ptr c_cur = &bfc1;
      assert(llvm_pack_constant_bitfield_bytes(&f_cur, &c_cur, llvm::Type::getFloatTy(*state.context), &out) ==
             llvm_gen_be_error_t::bitfield_access_failed);
      assert(llvm_pack_constant_bitfield_bytes(&f_cur, &c_cur, llvm::ArrayType::get(llvm::Type::getFloatTy(*state.context), 2), &out) ==
             llvm_gen_be_error_t::bitfield_access_failed);

      llvm::Type* arr3_i8 = llvm::ArrayType::get(llvm::Type::getInt8Ty(*state.context), 3);
      for (const char* dl_str : {"e-m:e-p:64:64-i64:64-n32:64-S128", "E-m:e-p:64:64-i64:64-n32:64-S128"}) {
        state.module->setDataLayout(dl_str);
        f_cur = &bf_named; c_cur = &bfc1;
        bfc2.implicit_aggr_element = FALSE;
        assert(llvm_pack_constant_bitfield_bytes(&f_cur, &c_cur, arr3_i8, &out) == llvm_gen_be_error_t::ok);
        f_cur = &bf_named; c_cur = &bfc1;
        bfc2.implicit_aggr_element = TRUE;
        assert(llvm_pack_constant_bitfield_bytes(&f_cur, &c_cur, arr3_i8, &out) == llvm_gen_be_error_t::ok);
        f_cur = &bf_un1; c_cur = nullptr; // c == nullptr whileunnamed_bf == true (bf_named not in scan? no, scan from bf_un1 has no named; test from bf_named with c_cur = nullptr!)
        f_cur = &bf_named; c_cur = nullptr;
        assert(llvm_pack_constant_bitfield_bytes(&f_cur, &c_cur, arr3_i8, &out) == llvm_gen_be_error_t::ok);
      }
      state.module->setDataLayout("e-m:e-p:64:64-i64:64-n32:64-S128");
      for (int b = 0; b < 5; ++b) {
        f_cur = &bf_named; c_cur = &bfc1;
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_pack_constant_bitfield_bytes(&f_cur, &c_cur, arr3_i8, &out);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }

      // 7) Union serialization/deserialization (LE & BE, Zero/NullPtr/i4/FP/DataSeq/ConstantArray/ConstantStruct, Pointer/PPC_FP128/truncation/relocatable, designators & bitfields)
      a_type u_ty = {}; u_ty.kind = tk_union; u_ty.size = 16;
      a_field uf_anon = {}, uf_a = {}, uf_b = {}, uf_bf = {};
      uf_anon.source_corresp.name = nullptr; uf_anon.type = &sint_ty; uf_anon.next = &uf_a;
      uf_a.source_corresp.name = (char*)"u_a"; uf_a.type = &sint_ty; uf_a.next = &uf_b;
      uf_b.source_corresp.name = (char*)"u_b"; uf_b.type = &flt_ty; uf_b.next = &uf_bf;
      uf_bf.source_corresp.name = (char*)"u_bf"; uf_bf.type = &sint_ty; uf_bf.is_bit_field = TRUE; uf_bf.bit_size = 5; uf_bf.next = nullptr;
      u_ty.variant.class_struct_union.field_list = &uf_anon;

      llvm::StructType* st_ptr = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>{ptr_ty});
      llvm::StructType* st_dbl = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>{llvm::Type::getDoubleTy(*state.context)});
      llvm::StructType* st_i32 = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>{i32_ty});
      llvm::StructType* st_i8 = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>{llvm::Type::getInt8Ty(*state.context)});
      llvm::StructType* st_ppc = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>{llvm::Type::getPPC_FP128Ty(*state.context)});
      llvm::StructType* st_arr_ppc = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>{llvm::ArrayType::get(llvm::Type::getPPC_FP128Ty(*state.context), 1)});
      llvm::StructType* st_sub_ppc = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>{st_ppc});
      llvm::StructType* st_dbl_i64 = llvm::StructType::get(*state.context, {llvm::Type::getDoubleTy(*state.context), llvm::Type::getInt64Ty(*state.context)});

      // Designator permutations on union + bitfield active_field + null c->type
      a_constant ud1 = {}, ud2 = {}, ud3 = {}, ud4 = {}, uv_int = {}, uv_notype = {};
      uv_int.kind = ck_integer; uv_int.type = &sint_ty; assert(llvm_set_edg_sint64(&uv_int.variant.integer_value, 0x1234) == llvm_gen_be_error_t::ok);
      uv_notype.kind = ck_integer; uv_notype.type = nullptr; assert(llvm_set_edg_sint64(&uv_notype.variant.integer_value, 0) == llvm_gen_be_error_t::ok);
      ud1.kind = ck_designator; ud1.variant.designator.is_generic = FALSE; ud1.variant.designator.variant.field = nullptr; ud1.next = &uv_notype;
      ud2.kind = ck_designator; ud2.variant.designator.is_generic = TRUE; ud2.variant.designator.variant.field_name = nullptr; ud2.next = &uv_int;
      ud3.kind = ck_designator; ud3.variant.designator.is_generic = TRUE; ud3.variant.designator.variant.field_name = (char*)"u_b"; ud3.next = &flt_bad;
      ud4.kind = ck_designator; ud4.variant.designator.is_generic = FALSE; ud4.variant.designator.variant.field = &uf_bf; ud4.next = &uv_int;

      a_constant u_agg = {}; u_agg.kind = ck_aggregate; u_agg.type = &u_ty;
      for (a_constant_ptr dc : {&ud1, &ud2, &ud3, &ud4}) {
        u_agg.variant.aggregate.first_constant = dc;
        for (int b = 0; b < 10; ++b) {
          g_llvm_be_oom_countdown = b;
          llvm_gen_be_error_t r = llvm_const_from_aggregate(&u_agg, st_i32, &out);
          g_llvm_be_oom_countdown = -1;
          if (r == llvm_gen_be_error_t::ok) break;
        }
      }
      // Also test c->type == nullptr when active_field == nullptr (empty union field_list)
      u_ty.variant.class_struct_union.field_list = nullptr;
      u_agg.variant.aggregate.first_constant = &uv_notype;
      assert(llvm_const_from_aggregate(&u_agg, st_i32, &out) != llvm_gen_be_error_t::ok);
      u_ty.variant.class_struct_union.field_list = &uf_anon;

      // Serialize/deserialize across LE and BE:
      // - 4-bit _BitInt (rem < 8U), zero int -> st_ptr (all_zero == true), non-zero int -> st_ptr (all_zero == false),
      // - double -> small 4-byte union (base_off + b >= buf.size()),
      // - string [6 x i8] -> small 4-byte union (ConstantDataSequential),
      // - zero array [2 x i32] (ConstantAggregateZero) & null pointer (ConstantPointerNull),
      // - ConstantArray of structs & ConstantStruct of ints (both sub_ok == true and sub_ok == false with relocatable global, plus base_off >= buf.size()),
      // - ppc_fp128 / [1 x ppc_fp128] / { { ppc_fp128 } } (!ec / rebuilt == nullptr),
      // - relocatable ptr into { double, i64 } (active_sz = 8 < union_sz = 16 -> trailing padding!)
      an_integer_type_supplement bp4_supp = {}; bp4_supp.bit_width = 4;
      a_type bp4_ty = {}; bp4_ty.kind = tk_integer; bp4_ty.size = 1; bp4_ty.variant.integer.int_kind = ik_unsigned_bit_precise; bp4_ty.variant.integer.extra_info = &bp4_supp;
      a_constant c_bp4 = {}; c_bp4.kind = ck_integer; c_bp4.type = &bp4_ty; assert(llvm_set_edg_sint64(&c_bp4.variant.integer_value, 7) == llvm_gen_be_error_t::ok);

      a_type ptr_edg = {}; ptr_edg.kind = tk_pointer; ptr_edg.size = 8; ptr_edg.variant.pointer.type = &sint_ty;
      a_constant c_nullptr_val = {}; c_nullptr_val.kind = ck_integer; c_nullptr_val.type = &ptr_edg; assert(llvm_set_edg_sint64(&c_nullptr_val.variant.integer_value, 0) == llvm_gen_be_error_t::ok);

      a_type arr2_int_edg = {}; arr2_int_edg.kind = tk_array; arr2_int_edg.size = 8;
      arr2_int_edg.variant.array.variant.number_of_elements = 2; arr2_int_edg.variant.array.element_type = &sint_ty;
      a_constant c_zero_arr = {}; c_zero_arr.kind = ck_aggregate; c_zero_arr.type = &arr2_int_edg; c_zero_arr.variant.aggregate.first_constant = nullptr;

      a_type str6_edg = {}; str6_edg.kind = tk_array; str6_edg.size = 6;
      str6_edg.variant.array.variant.number_of_elements = 6; str6_edg.variant.array.element_type = &char_edg;
      a_constant c_str6 = {}; c_str6.kind = ck_string; c_str6.type = &str6_edg; c_str6.variant.string.value = "hello"; c_str6.variant.string.length = 6;

      // Struct { int, int } & Array [1 x { int, int }] -> produces ConstantStruct & ConstantArray (not ConstantDataSequential!)
      a_field sf1 = {}, sf2 = {};
      sf1.type = &sint_ty; sf1.offset = 0; sf1.next = &sf2;
      sf2.type = &sint_ty; sf2.offset = 4; sf2.next = nullptr;
      a_type pair_st_edg = {}; pair_st_edg.kind = tk_struct; pair_st_edg.size = 8; pair_st_edg.variant.class_struct_union.field_list = &sf1;
      a_constant sc1 = {}, sc2 = {};
      sc1.kind = ck_integer; sc1.type = &sint_ty; assert(llvm_set_edg_sint64(&sc1.variant.integer_value, 1) == llvm_gen_be_error_t::ok); sc1.next = &sc2;
      sc2.kind = ck_integer; sc2.type = &sint_ty; assert(llvm_set_edg_sint64(&sc2.variant.integer_value, 2) == llvm_gen_be_error_t::ok); sc2.next = nullptr;
      a_constant c_pair_st = {}; c_pair_st.kind = ck_aggregate; c_pair_st.type = &pair_st_edg; c_pair_st.variant.aggregate.first_constant = &sc1;

      a_type arr1_pair_edg = {}; arr1_pair_edg.kind = tk_array; arr1_pair_edg.size = 8;
      arr1_pair_edg.variant.array.variant.number_of_elements = 1; arr1_pair_edg.variant.array.element_type = &pair_st_edg;
      a_constant c_arr_pair = {}; c_arr_pair.kind = ck_aggregate; c_arr_pair.type = &arr1_pair_edg; c_arr_pair.variant.aggregate.first_constant = &c_pair_st;

      // Relocatable struct { ptr } & array [1 x ptr] -> sub_ok == false!
      a_field pf1 = {}; pf1.type = &ptr_edg; pf1.offset = 0;
      a_type ptr_st_edg = {}; ptr_st_edg.kind = tk_struct; ptr_st_edg.size = 8; ptr_st_edg.variant.class_struct_union.field_list = &pf1;
      a_constant c_reloc_ptr = addr_v_con; c_reloc_ptr.type = &ptr_edg;
      a_constant c_reloc_st = {}; c_reloc_st.kind = ck_aggregate; c_reloc_st.type = &ptr_st_edg; c_reloc_st.variant.aggregate.first_constant = &c_reloc_ptr;
      a_type arr1_ptr_edg = {}; arr1_ptr_edg.kind = tk_array; arr1_ptr_edg.size = 8;
      arr1_ptr_edg.variant.array.variant.number_of_elements = 1; arr1_ptr_edg.variant.array.element_type = &ptr_edg;
      a_constant c_reloc_arr = {}; c_reloc_arr.kind = ck_aggregate; c_reloc_arr.type = &arr1_ptr_edg; c_reloc_arr.variant.aggregate.first_constant = &c_reloc_ptr;

      for (const char* dl_str : {"e-m:e-p:64:64-i64:64-n32:64-S128", "E-m:e-p:64:64-i64:64-n32:64-S128"}) {
        state.module->setDataLayout(dl_str);
        u_ty.size = 16;
        u_agg.variant.aggregate.first_constant = &c_bp4;
        assert(llvm_const_from_aggregate(&u_agg, st_dbl, &out) == llvm_gen_be_error_t::ok);
        u_agg.variant.aggregate.first_constant = &c_nullptr_val;
        assert(llvm_const_from_aggregate(&u_agg, st_dbl, &out) == llvm_gen_be_error_t::ok);
        u_agg.variant.aggregate.first_constant = &c_zero_arr;
        assert(llvm_const_from_aggregate(&u_agg, st_dbl, &out) == llvm_gen_be_error_t::ok);
        u_agg.variant.aggregate.first_constant = &leaf_c0;
        assert(llvm_const_from_aggregate(&u_agg, st_ptr, &out) == llvm_gen_be_error_t::ok);
        u_agg.variant.aggregate.first_constant = &uv_int;
        for (int b = 0; b < 10; ++b) {
          g_llvm_be_oom_countdown = b;
          llvm_gen_be_error_t r = llvm_const_from_aggregate(&u_agg, st_ptr, &out);
          g_llvm_be_oom_countdown = -1;
          if (r == llvm_gen_be_error_t::ok) break;
        }
        u_agg.variant.aggregate.first_constant = &flt_bad;
        assert(llvm_const_from_aggregate(&u_agg, st_i32, &out) == llvm_gen_be_error_t::ok);

        // Small union (size 1 / 4) for truncation & base_off >= buf.size()
        u_ty.size = 4;
        u_agg.variant.aggregate.first_constant = &flt_bad; // 8-byte double into 4-byte buf!
        assert(llvm_const_from_aggregate(&u_agg, st_i32, &out) == llvm_gen_be_error_t::ok);
        u_agg.variant.aggregate.first_constant = &c_str6; // 6-byte string into 4-byte buf!
        assert(llvm_const_from_aggregate(&u_agg, st_i32, &out) == llvm_gen_be_error_t::ok);
        u_ty.size = 1;
        u_agg.variant.aggregate.first_constant = &c_pair_st; // field 1 at offset 4 >= buf.size() (1)!
        assert(llvm_const_from_aggregate(&u_agg, st_i8, &out) == llvm_gen_be_error_t::ok);

        u_ty.size = 16;
        for (a_constant_ptr src_c : {&c_pair_st, &c_arr_pair, &c_reloc_st, &c_reloc_arr, &c_reloc_ptr}) {
          u_agg.variant.aggregate.first_constant = src_c;
          for (int b = 0; b < 15; ++b) {
            g_llvm_be_oom_countdown = b;
            llvm_gen_be_error_t r = llvm_const_from_aggregate(&u_agg, st_dbl_i64, &out);
            g_llvm_be_oom_countdown = -1;
            if (r == llvm_gen_be_error_t::ok) break;
          }
        }
        // Deserialize into [2 x i32], { i32, i32 }, ppc_fp128, [1 x ppc_fp128], { { ppc_fp128 } } + OOM
        llvm::StructType* st_arr_i32 = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>{llvm::ArrayType::get(i32_ty, 2)});
        u_agg.variant.aggregate.first_constant = &flt_bad;
        for (llvm::StructType* dst_st : {st_arr_i32, st_ppc, st_arr_ppc, st_sub_ppc}) {
          for (int b = 0; b < 15; ++b) {
            g_llvm_be_oom_countdown = b;
            llvm_gen_be_error_t r = llvm_const_from_aggregate(&u_agg, dst_st, &out);
            g_llvm_be_oom_countdown = -1;
            if (r == llvm_gen_be_error_t::ok) break;
          }
        }
      }
      state.module->setDataLayout("e-m:e-p:64:64-i64:64-n32:64-S128");

      // 8) Array designator/init_repeat OOM + Struct EBO, designator padding/bitfield/overlap, tail slots, init_repeat, and ck_void(void)
      llvm::ArrayType* arr4_i32 = llvm::ArrayType::get(i32_ty, 4);
      a_constant ad_desig = {}, ad_val = {}, ad_rep = {};
      ad_desig.kind = ck_designator; ad_desig.variant.designator.variant.array_element = 1; ad_desig.next = &ad_val;
      ad_val.kind = ck_integer; ad_val.type = &sint_ty; assert(llvm_set_edg_sint64(&ad_val.variant.integer_value, 9) == llvm_gen_be_error_t::ok); ad_val.next = &ad_rep;
      ad_rep.kind = ck_init_repeat; ad_rep.variant.init_repeat.count = 2; ad_rep.variant.init_repeat.constant = &uv_int;
      a_constant arr_oom_agg = {}; arr_oom_agg.kind = ck_aggregate; arr_oom_agg.type = &arr2_int_edg;
      arr_oom_agg.variant.aggregate.first_constant = &ad_desig;
      for (int b = 0; b < 12; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_const_from_aggregate(&arr_oom_agg, arr4_i32, &out);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }

      // Struct EBO leading loop (is_optimized_empty_class, empty tk_struct & tk_class, base_class / constexpr_folding / empty agg / non-empty agg)
      a_type emp_st = {}, emp_cls = {};
      emp_st.kind = tk_struct; emp_st.size = 1; emp_st.variant.class_struct_union.is_empty_class = TRUE;
      emp_cls.kind = tk_class; emp_cls.size = 1; emp_cls.variant.class_struct_union.is_empty_class = TRUE;

      a_field ef1 = {}, ef2 = {}, ef3 = {}, ef4 = {}, ef_real = {};
      ef1.is_optimized_empty_class = TRUE; ef1.type = nullptr; ef1.offset = 0; ef1.next = &ef2;
      ef2.type = &emp_st; ef2.offset = 0; ef2.next = &ef3;
      ef3.type = &emp_cls; ef3.offset = 0; ef3.next = &ef4;
      ef4.is_optimized_empty_class = TRUE; ef4.type = &emp_st; ef4.offset = 0; ef4.next = &ef_real;
      ef_real.type = &sint_ty; ef_real.offset = 0; ef_real.next = nullptr;

      a_constant ec1 = {}, ec2 = {}, ec3 = {}, ec_real = {};
      ec1.kind = ck_integer; ec1.constant_for_base_class = TRUE; ec1.next = &ec2;
      ec2.kind = ck_integer; ec2.constant_for_base_class_from_constexpr_folding = TRUE; ec2.next = &ec3;
      ec3.kind = ck_aggregate; ec3.variant.aggregate.first_constant = nullptr; ec3.next = &ec_real;
      ec_real.kind = ck_integer; ec_real.type = &sint_ty; assert(llvm_set_edg_sint64(&ec_real.variant.integer_value, 77) == llvm_gen_be_error_t::ok); ec_real.next = nullptr;

      a_type ebo_host = {}; ebo_host.kind = tk_struct; ebo_host.size = 4; ebo_host.variant.class_struct_union.field_list = &ef1;
      a_constant ebo_agg = {}; ebo_agg.kind = ck_aggregate; ebo_agg.type = &ebo_host; ebo_agg.variant.aggregate.first_constant = &ec1;
      for (int b = 0; b < 12; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_const_from_aggregate(&ebo_agg, st_i32, &out);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }
      // Also with c == nullptr on EBO fields
      ebo_agg.variant.aggregate.first_constant = nullptr;
      assert(llvm_const_from_aggregate(&ebo_agg, st_i32, &out) == llvm_gen_be_error_t::ok);

      // Designator skip loop: skip EBO field, i8-array padding (normal & break when i >= NumElements), non-i8 gap,
      // overlapping empty_sub (offset < current_offset with offset+fsz > current_offset & <= current_offset), 0-width bitfield, and non-zero bitfields!
      a_field df_ebo = {}, df_bf1 = {}, df_bf2 = {}, df_bf0 = {}, df_ov_emp1 = {}, df_ov_emp2 = {}, df_gap = {}, df_tgt = {};
      df_ebo.type = &emp_st; df_ebo.offset = 0; df_ebo.next = &df_bf1;
      df_bf1.type = &sint_ty; df_bf1.is_bit_field = TRUE; df_bf1.bit_size = 4; df_bf1.offset = 0; df_bf1.next = &df_bf2;
      df_bf2.type = &sint_ty; df_bf2.is_bit_field = TRUE; df_bf2.bit_size = 4; df_bf2.offset = 0; df_bf2.next = &df_bf0;
      df_bf0.type = &sint_ty; df_bf0.is_bit_field = TRUE; df_bf0.bit_size = 0; df_bf0.offset = 1; df_bf0.next = &df_ov_emp1;
      emp_cls.size = 8; // offset 2 + 8 = 10 > current_offset (4) -> advances!
      df_ov_emp1.type = &emp_cls; df_ov_emp1.offset = 2; df_ov_emp1.next = &df_ov_emp2;
      df_ov_emp2.type = nullptr; df_ov_emp2.is_bit_field = TRUE; df_ov_emp2.bit_size = 2; df_ov_emp2.offset = 2; df_ov_emp2.next = &df_gap;
      df_gap.type = &sint_ty; df_gap.offset = 12; df_gap.next = &df_tgt;
      df_tgt.source_corresp.name = (char*)"tgt"; df_tgt.type = &sint_ty; df_tgt.offset = 20; df_tgt.next = nullptr;

      a_type desig_st_edg = {}; desig_st_edg.kind = tk_struct; desig_st_edg.size = 24;
      desig_st_edg.variant.class_struct_union.field_list = &df_ebo;
      a_constant sd_desig = {}, sd_val = {};
      sd_desig.kind = ck_designator; sd_desig.variant.designator.is_generic = FALSE; sd_desig.variant.designator.variant.field = &df_tgt; sd_desig.next = &sd_val;
      sd_val.kind = ck_integer; sd_val.type = &sint_ty; assert(llvm_set_edg_sint64(&sd_val.variant.integer_value, 55) == llvm_gen_be_error_t::ok);
      a_constant sd_agg = {}; sd_agg.kind = ck_aggregate; sd_agg.type = &desig_st_edg; sd_agg.variant.aggregate.first_constant = &sd_desig;

      llvm::Type* arr2_i8 = llvm::ArrayType::get(llvm::Type::getInt8Ty(*state.context), 2);
      llvm::StructType* st_desig_full = llvm::StructType::get(*state.context, {i32_ty, i32_ty, arr2_i8, i32_ty, i32_ty});
      for (int b = 0; b < 15; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_const_from_aggregate(&sd_agg, st_desig_full, &out);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }
      // Also trigger i >= st_ty->getNumElements() break inside field->offset > current_offset i8-array check (L1710)!
      a_field pad_f1 = {}, pad_f2 = {};
      pad_f1.type = &sint_ty; pad_f1.offset = 4; pad_f1.next = &pad_f2;
      pad_f2.type = &sint_ty; pad_f2.offset = 8; pad_f2.next = nullptr;
      a_type pad_st_edg = {}; pad_st_edg.kind = tk_struct; pad_st_edg.size = 12; pad_st_edg.variant.class_struct_union.field_list = &pad_f1;
      a_constant pad_desig = {}; pad_desig.kind = ck_designator; pad_desig.variant.designator.variant.field = &pad_f2; pad_desig.next = &sd_val;
      a_constant pad_agg = {}; pad_agg.kind = ck_aggregate; pad_agg.type = &pad_st_edg; pad_agg.variant.aggregate.first_constant = &pad_desig;
      llvm::StructType* st_only_pad = llvm::StructType::get(*state.context, llvm::ArrayRef<llvm::Type*>{arr2_i8});
      assert(llvm_const_from_aggregate(&pad_agg, st_only_pad, &out) == llvm_gen_be_error_t::ok);
      // And non-i8-array d_cand_ty when field->offset > current_offset (L1711)!
      llvm::StructType* st_non_i8_pad = llvm::StructType::get(*state.context, {i32_ty, i32_ty});
      assert(llvm_const_from_aggregate(&pad_agg, st_non_i8_pad, &out) == llvm_gen_be_error_t::ok);

      // Non-designator struct loop: overlapping empty_sub / bitfield (L1771-L1785), 0-width bitfield (L1788),
      // null field->type, i8-array padding slot (L1762, L1811), tail rem_ty OOM (L1797), and ck_init_repeat with null/non-zero field type (L1839-L1841)!
      a_field nf1 = {}, nf_ov1 = {}, nf_ov2 = {}, nf_ov3 = {}, nf_bf0 = {}, nf_nullty = {}, nf_rep1 = {}, nf_rep2 = {};
      nf1.type = &sint_ty; nf1.offset = 0; nf1.next = &nf_ov1;
      nf_ov1.type = &emp_cls; nf_ov1.offset = 2; nf_ov1.next = &nf_ov2; // 2 < 4, 2 + 8 = 10 > 4!
      nf_ov2.type = nullptr; nf_ov2.is_bit_field = TRUE; nf_ov2.bit_size = 2; nf_ov2.offset = 2; nf_ov2.next = &nf_ov3;
      emp_st.size = 1;
      nf_ov3.type = &emp_st; nf_ov3.offset = 2; nf_ov3.next = &nf_bf0; // 2 < 10, 2 + 1 <= 10!
      nf_bf0.type = &sint_ty; nf_bf0.is_bit_field = TRUE; nf_bf0.bit_size = 0; nf_bf0.offset = 10; nf_bf0.next = &nf_nullty;
      nf_nullty.type = nullptr; nf_nullty.offset = 12; nf_nullty.next = &nf_rep1;
      nf_rep1.type = &sint_ty; nf_rep1.offset = 16; nf_rep1.next = &nf_rep2;
      nf_rep2.type = nullptr; nf_rep2.offset = 20; nf_rep2.next = nullptr;

      a_constant nc1 = {}, nc_ov1 = {}, nc_ov2 = {}, nc_ov3 = {}, nc_null = {}, nc_rep = {}, nc_tail1 = {}, nc_tail2 = {};
      nc1.kind = ck_integer; nc1.type = &sint_ty; assert(llvm_set_edg_sint64(&nc1.variant.integer_value, 1) == llvm_gen_be_error_t::ok); nc1.next = &nc_ov1;
      nc_ov1.kind = ck_integer; nc_ov1.constant_for_base_class = TRUE; nc_ov1.next = &nc_ov2;
      nc_ov2.kind = ck_integer; nc_ov2.constant_for_base_class_from_constexpr_folding = TRUE; nc_ov2.next = &nc_ov3;
      nc_ov3.kind = ck_aggregate; nc_ov3.variant.aggregate.first_constant = nullptr; nc_ov3.next = &nc_null;
      nc_null.kind = ck_integer; nc_null.type = &sint_ty; assert(llvm_set_edg_sint64(&nc_null.variant.integer_value, 2) == llvm_gen_be_error_t::ok); nc_null.next = &nc_rep;
      nc_rep.kind = ck_init_repeat; nc_rep.variant.init_repeat.count = 2; nc_rep.variant.init_repeat.constant = &uv_int; nc_rep.next = &nc_tail1;
      nc_tail1.kind = ck_integer; nc_tail1.type = &sint_ty; assert(llvm_set_edg_sint64(&nc_tail1.variant.integer_value, 3) == llvm_gen_be_error_t::ok); nc_tail1.next = &nc_tail2;
      nc_tail2.kind = ck_integer; nc_tail2.type = &sint_ty; assert(llvm_set_edg_sint64(&nc_tail2.variant.integer_value, 4) == llvm_gen_be_error_t::ok); nc_tail2.next = nullptr;

      a_type nf_st_edg = {}; nf_st_edg.kind = tk_struct; nf_st_edg.size = 36; nf_st_edg.variant.class_struct_union.field_list = &nf1;
      a_constant nf_agg = {}; nf_agg.kind = ck_aggregate; nf_agg.type = &nf_st_edg; nf_agg.variant.aggregate.first_constant = &nc1;
      llvm::StructType* st_nf = llvm::StructType::get(
          *state.context, {i32_ty, i32_ty, arr2_i8, arr2_i8, i32_ty, i32_ty, i32_ty, arr2_i8, i32_ty, i32_ty});
      for (int b = 0; b < 25; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_const_from_aggregate(&nf_agg, st_nf, &out);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }

      // Array field at offset > current_offset where cand_elem_ty [2 x i8] matches/mismatches gap (L1763)!
      a_field af_gap1 = {}, af_gap2 = {};
      af_gap1.type = &str6_edg; af_gap1.offset = 2; af_gap1.next = &af_gap2; // gap = 2 == [2 x i8]!
      af_gap2.type = &str6_edg; af_gap2.offset = 12; af_gap2.next = nullptr; // gap = 4 != [2 x i8]!
      a_type af_st_edg = {}; af_st_edg.kind = tk_struct; af_st_edg.size = 20; af_st_edg.variant.class_struct_union.field_list = &af_gap1;
      a_constant af_agg = {}; af_agg.kind = ck_aggregate; af_agg.type = &af_st_edg; af_agg.variant.aggregate.first_constant = nullptr;
      llvm::Type* arr6_i8 = llvm::ArrayType::get(llvm::Type::getInt8Ty(*state.context), 6);
      llvm::StructType* st_af = llvm::StructType::get(*state.context, {arr2_i8, arr6_i8, arr2_i8, arr6_i8});
      assert(llvm_const_from_aggregate(&af_agg, st_af, &out) == llvm_gen_be_error_t::ok);

      // ck_void with void expected_ty (L1951 true!)
      a_constant void_c = {}; void_c.kind = ck_void;
      assert(evaluate_constant(&void_c, llvm::Type::getVoidTy(*state.context), &out) == llvm_gen_be_error_t::ok);
      assert(out == nullptr);

      // 9) Final branch closure for llvm_gen_be_const.cpp:
      // - L298: bit_width > 128 && con->expr == nullptr (signed negative -7 into i256)
      a_constant c_wide_noexpr = {};
      c_wide_noexpr.kind = ck_integer;
      c_wide_noexpr.type = &sint_ty;
      assert(llvm_set_edg_sint64(&c_wide_noexpr.variant.integer_value, -7LL) == llvm_gen_be_error_t::ok);
      c_wide_noexpr.expr = nullptr;
      assert(llvm_const_from_integer(&c_wide_noexpr, i256_ty, &out) == llvm_gen_be_error_t::ok);

      // - L528: decimal float exponent without '+' or '-' ("3e5", mode 7)
      dec_con.variant.float_value.bytes[0] = 7;
      assert(llvm_const_from_float(&dec_con, llvm::Type::getInt64Ty(*state.context), &out) == llvm_gen_be_error_t::ok);

      // - L1202: non-empty tk_struct passed to is_empty_subobject_type
      // - L1239: bitfield scan starting at unnamed empty-string "" bitfield followed by named bitfield
      a_field bf_empty_first = {}, bf_named_second = {};
      bf_empty_first.source_corresp.name = (char*)"";
      bf_empty_first.is_bit_field = TRUE; bf_empty_first.bit_size = 4; bf_empty_first.offset = 0;
      bf_empty_first.next = &bf_named_second;
      bf_named_second.source_corresp.name = (char*)"bf_named2";
      bf_named_second.is_bit_field = TRUE; bf_named_second.bit_size = 4; bf_named_second.offset = 0; bf_named_second.offset_bit_remainder = 4;
      bf_named_second.next = nullptr;
      f_cur = &bf_empty_first; c_cur = &bfc1;
      assert(llvm_pack_constant_bitfield_bytes(&f_cur, &c_cur, i32_ty, &out) == llvm_gen_be_error_t::ok);

      // - L1345: ConstantDataSequential where raw.size() (6) <= buf.size() (16) so b < raw.size() terminates
      u_ty.size = 16;
      u_agg.variant.aggregate.first_constant = &c_str6;
      assert(llvm_const_from_aggregate(&u_agg, st_dbl, &out) == llvm_gen_be_error_t::ok);

      // - L1574: Union generic designator with non-matching field name (scan walks to nullptr)
      a_constant ud_nomatch = {};
      ud_nomatch.kind = ck_designator;
      ud_nomatch.variant.designator.is_field_designator = TRUE;
      ud_nomatch.variant.designator.is_generic = TRUE;
      ud_nomatch.variant.designator.variant.field_name = (char*)"nonexistent_union_field";
      ud_nomatch.next = &uv_int;
      u_agg.variant.aggregate.first_constant = &ud_nomatch;
      assert(llvm_const_from_aggregate(&u_agg, st_i32, &out) == llvm_gen_be_error_t::ok);

      // - L1676 & L1202: EBO leading field with non-empty ck_aggregate constant AND non-empty tk_struct at same offset
      a_type non_emp_st = {};
      non_emp_st.kind = tk_struct; non_emp_st.size = 4; non_emp_st.variant.class_struct_union.is_empty_class = FALSE;
      a_field ef_lead1 = {}, ef_lead2 = {}, ef_lead3 = {};
      ef_lead1.type = &emp_st; ef_lead1.offset = 0; ef_lead1.next = &ef_lead2;
      ef_lead2.type = &non_emp_st; ef_lead2.offset = 0; ef_lead2.next = &ef_lead3; // non-empty breaks EBO loop (L1202 false)!
      ef_lead3.type = &sint_ty; ef_lead3.offset = 0; ef_lead3.next = nullptr;
      a_constant ec_nonempty_agg = {};
      ec_nonempty_agg.kind = ck_aggregate; ec_nonempty_agg.variant.aggregate.first_constant = &uv_int; ec_nonempty_agg.next = &ec_real;
      a_type ebo_host2 = {}; ebo_host2.kind = tk_struct; ebo_host2.size = 8; ebo_host2.variant.class_struct_union.field_list = &ef_lead1;
      a_constant ebo_agg2 = {}; ebo_agg2.kind = ck_aggregate; ebo_agg2.type = &ebo_host2; ebo_agg2.variant.aggregate.first_constant = &ec_nonempty_agg;
      llvm::StructType* st_two_i32 = llvm::StructType::get(*state.context, {i32_ty, i32_ty});
      assert(llvm_const_from_aggregate(&ebo_agg2, st_two_i32, &out) == llvm_gen_be_error_t::ok);

      // - Designator skip loop (L1697-L1738):
      //   Case A: external target_field not in field_head, last field is non-bitfield -> L1701 (field->next == nullptr) & L1697 (field == nullptr)
      a_field ext_target = {};
      a_constant d_ext = {}; d_ext.kind = ck_designator; d_ext.variant.designator.variant.field = &ext_target; d_ext.next = &sd_val;
      a_field da_only = {}; da_only.type = &sint_ty; da_only.offset = 0; da_only.next = nullptr;
      a_type da_st_edg = {}; da_st_edg.kind = tk_struct; da_st_edg.size = 8; da_st_edg.variant.class_struct_union.field_list = &da_only;
      a_constant da_agg = {}; da_agg.kind = ck_aggregate; da_agg.type = &da_st_edg; da_agg.variant.aggregate.first_constant = &d_ext;
      assert(llvm_const_from_aggregate(&da_agg, st_two_i32, &out) == llvm_gen_be_error_t::ok);

      //   Case B: d_cand_ty is [2 x i32] (isArrayTy true, isIntegerTy(8) false -> L1708) AND [2 x i8] with more elements (i < NumElements -> L1712 false)
      //           plus overlapping empty_sub with distinct offsets (L1717 empty_sub true, L1720 > current_offset true & false),
      //           0-width bitfield at offset == current_offset (L1728 true),
      //           bitfield followed by non-bitfield at offset < end_off (L1737 is_bit_field false),
      //           bitfield followed by bitfield at offset >= end_off (L1738 offset < end_off false),
      //           and last field is bitfield (L1737 field != nullptr false)!
      emp_cls.size = 8;
      emp_st.size = 1;
      a_field db1 = {}, db2_emp_grow = {}, db3_emp_small = {}, db4_bf0 = {}, db5_bf = {}, db6_nonbf = {}, db7_bf_far = {}, db8_bf_last = {};
      db1.type = &sint_ty; db1.offset = 2; db1.next = &db2_emp_grow; // offset 2 > 0: hits [2 x i8] pad with i < NumElements! Then consumes i32 (current_offset = 6)
      db2_emp_grow.type = &emp_cls; db2_emp_grow.offset = 3; db2_emp_grow.next = &db3_emp_small; // 3 < 6, 3+8=11 > 6 -> L1717 & L1720 TRUE! (current_offset = 11)
      db3_emp_small.type = &emp_st; db3_emp_small.offset = 5; db3_emp_small.next = &db4_bf0; // 5 < 11, 5+1=6 <= 11 -> L1720 FALSE!
      db4_bf0.type = &sint_ty; db4_bf0.is_bit_field = TRUE; db4_bf0.bit_size = 0; db4_bf0.offset = 11; db4_bf0.next = &db5_bf; // offset == 11 -> L1728 TRUE!
      db5_bf.type = &sint_ty; db5_bf.is_bit_field = TRUE; db5_bf.bit_size = 4; db5_bf.offset = 11; db5_bf.next = &db6_nonbf; // skipped_sz = 4 -> end_off = 15
      db6_nonbf.type = &sint_ty; db6_nonbf.is_bit_field = FALSE; db6_nonbf.offset = 12; db6_nonbf.next = &db7_bf_far; // L1737 field->is_bit_field FALSE! Then 12 < 15 non-emp -> skipped_sz = 4 (current_offset = 19)
      db7_bf_far.type = &sint_ty; db7_bf_far.is_bit_field = TRUE; db7_bf_far.bit_size = 4; db7_bf_far.offset = 19; db7_bf_far.next = &db8_bf_last; // skipped_sz = 4 -> end_off = 23
      db8_bf_last.type = &sint_ty; db8_bf_last.is_bit_field = TRUE; db8_bf_last.bit_size = 4; db8_bf_last.offset = 24; db8_bf_last.next = nullptr; // offset 24 >= 23 -> L1738 FALSE! Next iter: field->next == nullptr -> L1737 field != nullptr FALSE!

      a_type db_st_edg = {}; db_st_edg.kind = tk_struct; db_st_edg.size = 32; db_st_edg.variant.class_struct_union.field_list = &db1;
      a_constant db_agg = {}; db_agg.kind = ck_aggregate; db_agg.type = &db_st_edg; db_agg.variant.aggregate.first_constant = &d_ext;
      llvm::Type* arr2_i32 = llvm::ArrayType::get(i32_ty, 2);
      llvm::StructType* st_db1 = llvm::StructType::get(*state.context, {arr2_i8, i32_ty, i32_ty, i32_ty, i32_ty, i32_ty, i32_ty, i32_ty});
      assert(llvm_const_from_aggregate(&db_agg, st_db1, &out) == llvm_gen_be_error_t::ok);
      // Also run pad_agg against { [2 x i32], i32 } so d_cand_ty->isArrayTy() is true and isIntegerTy(8) is false (L1708)!
      llvm::StructType* st_arr_i32_pad = llvm::StructType::get(*state.context, {arr2_i32, i32_ty});
      assert(llvm_const_from_aggregate(&pad_agg, st_arr_i32_pad, &out) == llvm_gen_be_error_t::ok);

      // - Non-designator struct loop remaining branches (L1764, L1776, L1781-L1784, L1797, L1813):
      //   * L1764: field->offset > current_offset with cand_is_i8_arr: (a) field->is_bit_field TRUE, (b) non-bitfield sint_ty (field_ut->kind != tk_array TRUE)
      //   * L1776 & L1781-L1784: distinct offsets on overlapping empty_sub (offset+fsz > current_offset TRUE) with:
      //     (1) c == nullptr, (2) c is plain ck_integer (not base_class), (3) c is non-empty ck_aggregate
      //   * L1813: elem_ty is [2 x i8] at exact offset with cur_field_ut == &sint_ty (!cur_field_ut FALSE, kind != tk_array TRUE)
      //   * L1797: field == nullptr with c == nullptr AND with c != nullptr when rem_ty is [2 x i32]
      a_field ng_bf_gap = {}, ng_int_gap = {}, ng_ov_grow1 = {}, ng_ov_grow2 = {}, ng_pad_exact = {};
      ng_bf_gap.type = &sint_ty; ng_bf_gap.is_bit_field = TRUE; ng_bf_gap.bit_size = 4; ng_bf_gap.offset = 2; ng_bf_gap.next = &ng_int_gap; // L1764 field->is_bit_field TRUE! Then packs into i32 (current_offset = 6)
      ng_int_gap.type = &sint_ty; ng_int_gap.offset = 8; ng_int_gap.next = &ng_ov_grow1; // L1764 !field_ut FALSE & kind != tk_array TRUE! Then consumes i32 (current_offset = 12)
      ng_ov_grow1.type = &emp_cls; ng_ov_grow1.offset = 9; ng_ov_grow1.next = &ng_ov_grow2; // 9 < 12, 9+8=17 > 12 -> L1776 TRUE!
      ng_ov_grow2.type = &emp_cls; ng_ov_grow2.offset = 10; ng_ov_grow2.next = &ng_pad_exact; // 10 < 17, 10+8=18 > 17 -> L1776 TRUE!
      ng_pad_exact.type = &sint_ty; ng_pad_exact.offset = 18; ng_pad_exact.next = nullptr; // exact offset 18: first sees [2 x i8] (L1813 cur_field_ut != tk_array TRUE -> current_offset=20), then consumes i32!

      a_type ng_st_edg = {}; ng_st_edg.kind = tk_struct; ng_st_edg.size = 36; ng_st_edg.variant.class_struct_union.field_list = &ng_bf_gap;
      llvm::StructType* st_ng = llvm::StructType::get(
          *state.context, {arr2_i8, i32_ty, arr2_i8, i32_ty, i32_ty, i32_ty, arr2_i8, i32_ty, arr2_i32, i32_ty});

      // Run 1: c == nullptr (hits L1781 c == nullptr & L1797 c == nullptr)
      a_constant ng_agg = {}; ng_agg.kind = ck_aggregate; ng_agg.type = &ng_st_edg; ng_agg.variant.aggregate.first_constant = nullptr;
      assert(llvm_const_from_aggregate(&ng_agg, st_ng, &out) == llvm_gen_be_error_t::ok);

      // Run 2: constants with plain ck_integer on ng_ov_grow1 (L1782-1784 all false) and non-empty ck_aggregate on ng_ov_grow2 (L1784 first_constant == nullptr false), plus trailing c for [2 x i32] rem_ty (L1797 isIntegerTy(8) false)!
      a_constant ngc1 = {}, ngc2 = {}, ngc_plain = {}, ngc_nonemp = {}, ngc_pad = {}, ngc_rem_arr = {};
      ngc1.kind = ck_integer; ngc1.type = &sint_ty; assert(llvm_set_edg_sint64(&ngc1.variant.integer_value, 1) == llvm_gen_be_error_t::ok); ngc1.next = &ngc2;
      ngc2.kind = ck_integer; ngc2.type = &sint_ty; assert(llvm_set_edg_sint64(&ngc2.variant.integer_value, 2) == llvm_gen_be_error_t::ok); ngc2.next = &ngc_plain;
      ngc_plain.kind = ck_integer; ngc_plain.type = &sint_ty; assert(llvm_set_edg_sint64(&ngc_plain.variant.integer_value, 3) == llvm_gen_be_error_t::ok); ngc_plain.next = &ngc_pad; // NOT consumed by ng_ov_grow1 nor ng_ov_grow2? Wait: let's also test non-empty ck_aggregate in Run 3!
      ngc_pad.kind = ck_aggregate; ngc_pad.type = &arr2_int_edg; ngc_pad.variant.aggregate.first_constant = nullptr; ngc_pad.next = &ngc_rem_arr;
      ngc_rem_arr.kind = ck_integer; ngc_rem_arr.type = &sint_ty; assert(llvm_set_edg_sint64(&ngc_rem_arr.variant.integer_value, 9) == llvm_gen_be_error_t::ok); ngc_rem_arr.next = nullptr;
      ng_agg.variant.aggregate.first_constant = &ngc1;
      assert(llvm_const_from_aggregate(&ng_agg, st_ng, &out) == llvm_gen_be_error_t::ok);

      // Run 3: non-empty ck_aggregate at ng_ov_grow1 (hits L1784 c->kind == ck_aggregate TRUE && first_constant == nullptr FALSE)
      ngc2.next = &ngc_nonemp;
      ngc_nonemp.kind = ck_aggregate; ngc_nonemp.type = &sint_ty; ngc_nonemp.variant.aggregate.first_constant = &uv_int; ngc_nonemp.next = nullptr;
      llvm::StructType* st_ng_short = llvm::StructType::get(
          *state.context, {arr2_i8, i32_ty, arr2_i8, i32_ty, i32_ty, i32_ty});
      assert(llvm_const_from_aggregate(&ng_agg, st_ng_short, &out) == llvm_gen_be_error_t::ok);

      // Run 4:
      // - L1762 (!field_ut TRUE) & L1811 (!cur_field_ut TRUE): non-bitfield with type == nullptr at offset 2 > 0 facing { [2 x i8], [2 x i8], i32 }
      // - L1780 (c->constant_for_base_class TRUE): overlapping empty_sub at distinct offset with constant_for_base_class = TRUE
      // - L1795 (c == nullptr): leading EBO-only field skipped at L1668 leaving field == nullptr && c == nullptr!
      a_field nr_null_gap = {}, nr_ov_bc = {};
      nr_null_gap.type = nullptr; nr_null_gap.offset = 2; nr_null_gap.next = &nr_ov_bc; // offset 2 > 0: L1762 !field_ut TRUE (i=0 [2 x i8], cur_off=2); next iter: L1811 !cur_field_ut TRUE (i=1 [2 x i8], cur_off=4); next iter: consumes i=2 i32 (cur_off=8)!
      nr_ov_bc.type = &emp_cls; nr_ov_bc.offset = 3; nr_ov_bc.next = nullptr; // 3 < 8, 3+8=11 > 8: L1780 constant_for_base_class TRUE!
      a_constant nrc1 = {}, nrc_bc = {};
      nrc1.kind = ck_integer; nrc1.type = &sint_ty; assert(llvm_set_edg_sint64(&nrc1.variant.integer_value, 1) == llvm_gen_be_error_t::ok); nrc1.next = &nrc_bc;
      nrc_bc.kind = ck_integer; nrc_bc.constant_for_base_class = TRUE; nrc_bc.next = nullptr;
      a_type nr_st_edg = {}; nr_st_edg.kind = tk_struct; nr_st_edg.size = 16; nr_st_edg.variant.class_struct_union.field_list = &nr_null_gap;
      a_constant nr_agg = {}; nr_agg.kind = ck_aggregate; nr_agg.type = &nr_st_edg; nr_agg.variant.aggregate.first_constant = &nrc1;
      llvm::StructType* st_nr = llvm::StructType::get(*state.context, {arr2_i8, arr2_i8, i32_ty, i32_ty});
      assert(llvm_const_from_aggregate(&nr_agg, st_nr, &out) == llvm_gen_be_error_t::ok);

      a_field ebo_single = {}; ebo_single.is_optimized_empty_class = TRUE; ebo_single.type = nullptr; ebo_single.next = nullptr;
      a_type ebo_only_edg = {}; ebo_only_edg.kind = tk_struct; ebo_only_edg.size = 4; ebo_only_edg.variant.class_struct_union.field_list = &ebo_single;
      a_constant ebo_only_agg = {}; ebo_only_agg.kind = ck_aggregate; ebo_only_agg.type = &ebo_only_edg; ebo_only_agg.variant.aggregate.first_constant = nullptr;
      assert(llvm_const_from_aggregate(&ebo_only_agg, st_i32, &out) == llvm_gen_be_error_t::ok);

      // Trigger error percolation from llvm_pack_constant_bitfield_bytes inside struct llvm_const_from_aggregate
      a_field bf_err_f = {}; bf_err_f.type = &sint_ty; bf_err_f.is_bit_field = TRUE; bf_err_f.bit_size = 4; bf_err_f.offset = 0;
      a_type bf_err_st_edg = {}; bf_err_st_edg.kind = tk_struct; bf_err_st_edg.size = 4; bf_err_st_edg.variant.class_struct_union.field_list = &bf_err_f;
      a_constant bf_err_agg = {}; bf_err_agg.kind = ck_aggregate; bf_err_agg.type = &bf_err_st_edg; bf_err_agg.variant.aggregate.first_constant = &nrc1;
      assert(llvm_const_from_aggregate(&bf_err_agg, st_dbl, &out) == llvm_gen_be_error_t::bitfield_access_failed);

      // Unnamed abk_variable address constant branches (L1057-L1080) & ck_init_repeat (L1974-L1988)
      a_type bad_var_ty = {};
      bad_var_ty.kind = tk_integer;
      bad_var_ty.size = 99; // get_llvm_type returns unsupported_type!

      a_variable anon_v = {};
      anon_v.source_corresp.name = nullptr;
      a_constant anon_addr = {};
      anon_addr.kind = ck_address;
      anon_addr.variant.address.kind = abk_variable;
      anon_addr.variant.address.variant.variable = &anon_v;

      // 1) !var->type
      anon_v.type = nullptr;
      assert(llvm_const_from_address(&anon_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);
      state.local_vars.erase(&anon_v);

      // 2) get_llvm_type failure via OOM at step 1 (var->type uncached)
      a_type fresh_anon_ty = {};
      fresh_anon_ty.kind = tk_integer;
      fresh_anon_ty.size = 4;
      anon_v.type = &fresh_anon_ty;
      anon_v.init_kind = initk_none;
      for (int b = 0; b < 4; ++b) {
        state.type_cache.erase(&fresh_anon_ty);
        state.local_vars.erase(&anon_v);
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_const_from_address(&anon_addr, ptr_ty, &out);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }
      state.local_vars.erase(&anon_v);

      // 3) init_kind != initk_static (!anon_init True) + OOM on llvm_create_global_variable + ok
      anon_v.type = &sint_ty;
      anon_v.init_kind = initk_none;
      g_llvm_be_oom_countdown = 1; // sint_ty is cached so step 0=address, step 1=llvm_create_global_variable!
      assert(llvm_const_from_address(&anon_addr, ptr_ty, &out) == llvm_gen_be_error_t::out_of_memory);
      g_llvm_be_oom_countdown = -1;
      assert(llvm_const_from_address(&anon_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);
      state.local_vars.erase(&anon_v);

      // 4) init_kind == initk_static && initializer.constant == nullptr
      anon_v.init_kind = initk_static;
      anon_v.initializer.constant = nullptr;
      assert(llvm_const_from_address(&anon_addr, ptr_ty, &out) == llvm_gen_be_error_t::ok);
      state.local_vars.erase(&anon_v);

      // 5) init_kind == initk_static && evaluate_constant OOM error vs ok (!anon_init False)
      anon_v.initializer.constant = &nrc1;
      for (int b = 0; b < 6; ++b) {
        state.local_vars.erase(&anon_v);
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = llvm_const_from_address(&anon_addr, ptr_ty, &out);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }
      state.local_vars.erase(&anon_v);

      // ck_init_repeat via evaluate_constant (ArrayType OOM error, count < len, count > len, and non-ArrayType fallback)
      a_constant rep_con = {};
      rep_con.kind = ck_init_repeat;
      rep_con.variant.init_repeat.constant = &nrc1;
      rep_con.variant.init_repeat.count = 1;
      for (int b = 0; b < 4; ++b) {
        g_llvm_be_oom_countdown = b;
        llvm_gen_be_error_t r = evaluate_constant(&rep_con, arr2_i32, &out);
        g_llvm_be_oom_countdown = -1;
        if (r == llvm_gen_be_error_t::ok) break;
      }
      rep_con.variant.init_repeat.count = 1; // count < total_elems (2)
      assert(evaluate_constant(&rep_con, arr2_i32, &out) == llvm_gen_be_error_t::ok);
      rep_con.variant.init_repeat.count = 5; // count > total_elems (2)
      assert(evaluate_constant(&rep_con, arr2_i32, &out) == llvm_gen_be_error_t::ok);
      assert(evaluate_constant(&rep_con, i32_ty, &out) == llvm_gen_be_error_t::ok); // non-ArrayType
#if FIXED_POINT_ALLOWED
      a_type fx_con_ty = {};
      fx_con_ty.kind = tk_fixed_point;
      fx_con_ty.size = 4;
      a_constant fx_con = {};
      fx_con.kind = ck_fixed_point;
      fx_con.type = &fx_con_ty;
      assert(llvm_set_edg_sint64(&fx_con.variant.fixed_point_value, 1024) == llvm_gen_be_error_t::ok);
      assert(evaluate_constant(&fx_con, i32_ty, &out) == llvm_gen_be_error_t::ok);
      assert(evaluate_constant(&fx_con, nullptr, &out) == llvm_gen_be_error_t::ok);
      assert(evaluate_constant(&fx_con, llvm::Type::getFloatTy(*state.context), &out) == llvm_gen_be_error_t::ok);
      state.type_cache.erase(&fx_con_ty);
      fx_con_ty.kind = static_cast<a_type_kind>(999);
      assert(evaluate_constant(&fx_con, nullptr, &out) == llvm_gen_be_error_t::unsupported_type);
#endif
    }
  }
  
  printf("All passed\n");
  return 0;
}
namespace edg {
  /**
   * @brief Test stub or helper for `free_general`.
   * @details Provides mock or helper behavior for `free_general` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `free_general`.
   * @param[in] arg_1 Parameter `arg_1` for `free_general`.
   */
  void free_general(void* arg_0, unsigned long arg_1) {}
  /**
   * @brief Test stub or helper for `alloc_general`.
   * @details Provides mock or helper behavior for `alloc_general` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `alloc_general`.
   * @return Result of `alloc_general` (char*).
   */
  char* alloc_general(unsigned long arg_0) { return nullptr; }
  /**
   * @brief Test stub or helper for `assertion_failed`.
   * @details Provides mock or helper behavior for `assertion_failed` within `tests/test_llvm_const_evaluation.cpp`.
   * @param[in] arg_0 Parameter `arg_0` for `assertion_failed`.
   * @param[in] arg_1 Parameter `arg_1` for `assertion_failed`.
   * @param[in] arg_2 Parameter `arg_2` for `assertion_failed`.
   * @param[in] arg_3 Parameter `arg_3` for `assertion_failed`.
   * @param[in] arg_4 Parameter `arg_4` for `assertion_failed`.
   */
  void assertion_failed(char const* arg_0, int arg_1, char const* arg_2, char const* arg_3, char const* arg_4) {
    (void)arg_0; (void)arg_1; (void)arg_2; (void)arg_3; (void)arg_4;
    for (;;) {}
  }
  /**
   * @brief Test stub or helper for `insufficient_address_space`.
   * @details Provides mock or helper behavior for `insufficient_address_space` within `tests/test_llvm_const_evaluation.cpp`.
   */
  void insufficient_address_space() {
    for (;;) {}
  }
}
