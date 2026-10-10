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
  a_boolean is_bool_type(a_type_ptr ty) { return FALSE; }
  a_targ_alignment f_alignment_of_type(a_type_ptr ty) { return 1; }
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
  
  a_number_buffer fp_to_string(a_float_kind fk, an_internal_float_value * val, a_boolean* pos_inf, a_boolean* neg_inf, a_boolean* nan) {
    if (val->bytes[0] == 1) *pos_inf = TRUE;
    else if (val->bytes[0] == 2) *neg_inf = TRUE;
    else if (val->bytes[0] == 3) *nan = TRUE;
    else return a_number_buffer("3.14");
    return a_number_buffer("");
  }
}

int main() {
  LLVMBackendState state;
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
  dummy_const.variant.integer_value = 42;
  
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
  dummy_const.kind = ck_address;
  dummy_const.variant.address.kind = abk_label;
  dummy_const.variant.address.variant.label = reinterpret_cast<a_label_ptr>(0x1234);
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
  elem1.variant.integer_value = 1;
  elem1.next = &elem2;
  elem2.kind = ck_integer;
  elem2.type = &dummy_int_ty;
  elem2.variant.integer_value = 2;
  dummy_const.variant.aggregate.first_constant = &elem1;
  
  llvm::ArrayType* arr_ty = llvm::ArrayType::get(i32_ty, 3);
  assert(llvm_const_from_aggregate(&dummy_const, arr_ty, &out) == llvm_gen_be_error_t::ok);
  assert(out->getType() == arr_ty);
  
  llvm::StructType* st_ty = llvm::StructType::get(*state.context, {i32_ty, i32_ty, i32_ty});
  assert(llvm_const_from_aggregate(&dummy_const, st_ty, &out) == llvm_gen_be_error_t::ok);
  assert(out->getType() == st_ty);

  fprintf(stderr, "Test aggregate passed\n");

  // Test nulls and invalid args
  assert(llvm_const_from_integer(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_float(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_string(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_aggregate(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(llvm_const_from_address(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  assert(evaluate_constant(nullptr, nullptr, nullptr) == llvm_gen_be_error_t::invalid_argument);
  
  assert(evaluate_constant(nullptr, i32_ty, &out) == llvm_gen_be_error_t::ok);
  assert(out->isNullValue());
  
  dummy_const.kind = static_cast<a_constant_repr_kind>(99);
  assert(evaluate_constant(&dummy_const, i32_ty, &out) == llvm_gen_be_error_t::unsupported_expr);
  
  printf("All passed\n");
  return 0;
}
namespace edg {
  void free_general(void*, unsigned long) {}
  char* alloc_general(unsigned long) { return nullptr; }
  void assertion_failed(char const*, int, char const*, char const*, char const*) { abort(); }
  void insufficient_address_space() { abort(); }
}
