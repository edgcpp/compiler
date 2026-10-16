/**
 * @file llvm_gen_be_const.cpp
 * @brief Constant evaluator subsystem for the EDG LLVM backend.
 * @details Translates EDG front-end constant structures (a_constant_ptr) into LLVM IR
 * constants (llvm::Constant*).
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_const.h"

#include <type_traits>
#include <vector>

#include "basic_hdrs.h"
#include "fe_common.h"

#include "float_pt.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_ptrmem.h"
#include "target.h"

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

llvm_gen_be_error_t llvm_const_from_integer(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!con || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  bool is_unsigned = (con->type->kind == tk_integer || con->type->kind == tk_enum) ? 
                       !int_kind_is_signed[con->type->variant.integer.int_kind] : false;
  uint64_t low = (uint64_t)con->variant.integer_value;
  uint64_t high = 0;
#if INT128_EXTENSIONS_ALLOWED
  if (sizeof(an_integer_value) > 8) {
    high = (uint64_t)(con->variant.integer_value >> 64);
  }
#endif
  
  unsigned bit_width = expected_ty->getIntegerBitWidth();
  llvm::APInt api(128, {low, high});
  if (bit_width != 128) {
    if (is_unsigned) {
       api = api.zextOrTrunc(bit_width);
    } else {
       api = api.sextOrTrunc(bit_width);
    }
  }

  *out_const = llvm::ConstantInt::get(expected_ty->getContext(), api);
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_const_from_float(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!con || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
 
  }

  a_boolean pos_inf = FALSE, neg_inf = FALSE, nan = FALSE;
  a_float_kind fk = con->type->variant.float_kind;
  a_number_buffer hex_str = fp_to_string(
      fk, &con->variant.float_value, &pos_inf, &neg_inf, &nan);

  if (pos_inf) {
    *out_const = llvm::ConstantFP::getInfinity(expected_ty, false);
  } else if (neg_inf) {
    *out_const = llvm::ConstantFP::getInfinity(expected_ty, true);
  } else if (nan) {
    *out_const = llvm::ConstantFP::getQNaN(expected_ty);
  } else {
    llvm::StringRef str_ref(hex_str.as_temp_characters());
    *out_const = llvm::ConstantFP::get(expected_ty, str_ref);
  }

  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_const_from_string(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!con || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
 
  }

  a_targ_size_t len = con->variant.string.length;
  const char* chars = con->variant.string.value;

  llvm::Type* actual_array_ty = nullptr;
  llvm_gen_be_error_t err = llvm_type_from_edg_type(con->type, &actual_array_ty);
  if (err != llvm_gen_be_error_t::ok) return err;

  llvm::Type* elem_ty = nullptr;
  if (actual_array_ty->isArrayTy()) {
    elem_ty = actual_array_ty->getArrayElementType();
  } else {
    // Try to recover gracefully by treating it as an i8 array.
    elem_ty = llvm::Type::getInt8Ty(*be_state->context);
  }

  llvm::Constant* arr = nullptr;
  if (elem_ty->isIntegerTy(16)) {
    llvm::ArrayRef<uint16_t> wide_chars(reinterpret_cast<const uint16_t*>(chars), len / 2);
    arr = llvm::ConstantDataArray::get(*be_state->context, wide_chars);
  } else if (elem_ty->isIntegerTy(32)) {
    llvm::ArrayRef<uint32_t> wide_chars(reinterpret_cast<const uint32_t*>(chars), len / 4);
    arr = llvm::ConstantDataArray::get(*be_state->context, wide_chars);
  } else {
    arr = llvm::ConstantDataArray::get(*be_state->context, llvm::ArrayRef<uint8_t>(reinterpret_cast<const uint8_t*>(chars), len));
  }

  if (expected_ty->isPointerTy()) {
    std::string str_key(chars, len);
    if (be_state->string_literal_cache.count(str_key)) {
      *out_const = be_state->string_literal_cache[str_key];
      return llvm_gen_be_error_t::ok;
    }
    llvm::GlobalVariable* gv = new llvm::GlobalVariable(
        *be_state->module, arr->getType(), true,
        llvm::GlobalValue::PrivateLinkage, arr, ".str");
    be_state->string_literal_cache[str_key] = gv;
    *out_const = gv;
  } else {
    *out_const = arr;
  }
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_const_from_address(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!con || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
 
  }

  llvm::Constant* base = nullptr;

  if (con->variant.address.kind == abk_label) {
    a_label_ptr label = con->variant.address.variant.label;
    llvm::BasicBlock* label_bb = nullptr;
    if (be_state->label_blocks.count(label)) {
      label_bb = be_state->label_blocks[label];
    } else {
      llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
      label_bb = llvm::BasicBlock::Create(*be_state->context, "label", func);
      be_state->label_blocks[label] = label_bb;
    }
    llvm::Function* func = label_bb->getParent();
    if (!func) {
      func = be_state->builder->GetInsertBlock()->getParent();
    }
    base = llvm::BlockAddress::get(func, label_bb);
  } else if (con->variant.address.kind == abk_variable) {
    a_variable_ptr var = con->variant.address.variant.variable;
    if (!var || !var->source_corresp.name) {
      *out_const = llvm::Constant::getNullValue(expected_ty);
      return llvm_gen_be_error_t::ok;
    }
    llvm::GlobalVariable* gvar = be_state->module->getNamedGlobal(var->source_corresp.name);
    if (!gvar) {
      llvm::Type* var_ty = nullptr;
      llvm_gen_be_error_t err = get_llvm_type(var->type, &var_ty);
      if (err != llvm_gen_be_error_t::ok) return err;
      gvar = llvm::cast<llvm::GlobalVariable>(be_state->module->getOrInsertGlobal(var->source_corresp.name, var_ty));
    }
    base = gvar;
  } else if (con->variant.address.kind == abk_routine) {
    a_routine_ptr routine = con->variant.address.variant.routine;
    if (!routine || !routine->source_corresp.name) {
      *out_const = llvm::Constant::getNullValue(expected_ty);
      return llvm_gen_be_error_t::ok;
    }
    llvm::Function* func = be_state->module->getFunction(routine->source_corresp.name);
    if (!func) {
      llvm::FunctionType* func_ty = nullptr;
      llvm_gen_be_error_t err = llvm_type_from_routine(routine->type, &func_ty);
      if (err != llvm_gen_be_error_t::ok) return err;
      func = llvm::Function::Create(func_ty, llvm::GlobalValue::ExternalLinkage, routine->source_corresp.name, be_state->module.get());
    }
    base = func;
  } else if (con->variant.address.kind == abk_constant) {
    a_constant_ptr target = con->variant.address.variant.constant;
    // Specifically evaluate the target constant, expecting its own pointer type.
    llvm::Type* target_ty = nullptr;
    llvm_gen_be_error_t err = get_llvm_type(target->type, &target_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
    llvm::Type* target_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    err = evaluate_constant(target, target_ptr_ty, &base);
    if (err != llvm_gen_be_error_t::ok) return err;
  } else {
    *out_const = llvm::Constant::getNullValue(expected_ty);
    return llvm_gen_be_error_t::ok;
  }

  if (base && con->variant.address.offset != 0) {
    llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
    llvm::Constant* offset_val = llvm::ConstantInt::get(be_state->module->getDataLayout().getIntPtrType(*be_state->context), con->variant.address.offset);
    base = llvm::ConstantExpr::getGetElementPtr(i8_ty, base, offset_val);
  }
  
  if (base) {
    *out_const = base;
  } else {
    *out_const = llvm::Constant::getNullValue(expected_ty);
  }
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_const_from_aggregate(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!con || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
 
  }

  if (expected_ty->isArrayTy()) {
    std::vector<llvm::Constant*> elems;
    llvm::Type* elem_ty = expected_ty->getArrayElementType();
    for (a_constant_ptr c = con->variant.aggregate.first_constant; c != nullptr; c = c->next) {
      llvm::Constant* elem_const = nullptr;
      llvm_gen_be_error_t err = evaluate_constant(c, elem_ty, &elem_const);
      if (err != llvm_gen_be_error_t::ok) return err;
      elems.push_back(elem_const);
    }
    unsigned expected_elems = expected_ty->getArrayNumElements();
    while (elems.size() < expected_elems) {
       elems.push_back(llvm::Constant::getNullValue(elem_ty));
    }
    *out_const = llvm::ConstantArray::get(llvm::cast<llvm::ArrayType>(expected_ty), elems);
    return llvm_gen_be_error_t::ok;
  } else if (expected_ty->isStructTy()) {
    llvm::StructType* st_ty = llvm::cast<llvm::StructType>(expected_ty);
    if (con->type->kind == tk_union) {
       // Evaluate the active member, but since we must match st_ty (which was built from the largest member),
       // and LLVM doesn't easily support mixed-type unions without byte buffers, we zero initialize it
       // for this implementation unless the types exactly match.
       llvm::Constant* active_val = nullptr;
       a_constant_ptr c = con->variant.aggregate.first_constant;
       if (c && st_ty->getNumElements() > 0) {
           llvm::Type* target_ty = nullptr;
           llvm_gen_be_error_t err = get_llvm_type(c->type, &target_ty);
           if (err == llvm_gen_be_error_t::ok && target_ty == st_ty->getElementType(0)) {
               err = evaluate_constant(c, target_ty, &active_val);
           }
       }
       std::vector<llvm::Constant*> elems;
       if (active_val) {
           elems.push_back(active_val);
       } else if (st_ty->getNumElements() > 0) {
           elems.push_back(llvm::Constant::getNullValue(st_ty->getElementType(0)));
       }
       for (unsigned i = 1; i < st_ty->getNumElements(); i++) {
           elems.push_back(llvm::Constant::getNullValue(st_ty->getElementType(i)));
       }
       *out_const = llvm::ConstantStruct::get(st_ty, elems);
       return llvm_gen_be_error_t::ok;
    }
    
    std::vector<llvm::Constant*> elems;
    unsigned i = 0;
    for (a_constant_ptr c = con->variant.aggregate.first_constant; c != nullptr; c = c->next) {
      if (i < st_ty->getNumElements()) {
         llvm::Constant* elem_const = nullptr;
         llvm_gen_be_error_t err = evaluate_constant(c, st_ty->getElementType(i), &elem_const);
         if (err != llvm_gen_be_error_t::ok) return err;
         elems.push_back(elem_const);
         i++;
      }
    }
    while (i < st_ty->getNumElements()) {
       elems.push_back(llvm::Constant::getNullValue(st_ty->getElementType(i)));
       i++;
    }
    *out_const = llvm::ConstantStruct::get(st_ty, elems);
    return llvm_gen_be_error_t::ok;
  }

  *out_const = llvm::Constant::getNullValue(expected_ty);
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t evaluate_constant(
    a_constant_ptr con,
    llvm::Type* expected_ty,
    llvm::Constant** out_const) noexcept {
  if (!out_const) {
    return llvm_gen_be_error_t::invalid_argument;
 
  }
  if (!con) {
    *out_const = llvm::Constant::getNullValue(expected_ty);
    return llvm_gen_be_error_t::ok;
  }

  switch (con->kind) {
    case ck_integer:
      return llvm_const_from_integer(con, expected_ty, out_const);
    case ck_float:
      return llvm_const_from_float(con, expected_ty, out_const);
    case ck_string:
      return llvm_const_from_string(con, expected_ty, out_const);
    case ck_address:
      return llvm_const_from_address(con, expected_ty, out_const);
    case ck_aggregate:
      return llvm_const_from_aggregate(con, expected_ty, out_const);
    case ck_ptr_to_member:
      return llvm_const_from_ptr_to_member(con, expected_ty, out_const);
    default:
      *out_const = llvm::Constant::getNullValue(expected_ty);
      return llvm_gen_be_error_t::unsupported_expr;
  }
}

END_EDG_NAMESPACE
#endif
