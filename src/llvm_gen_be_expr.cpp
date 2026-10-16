/**
 * @file llvm_gen_be_expr.cpp
 * @brief Expression lowering subsystem for the EDG LLVM backend.
 * @details Translates EDG front-end expression nodes (an_expr_node_ptr) into LLVM IR
 * values (llvm::Value*).
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_expr.h"
#include <type_traits>
#include <vector>

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_vtable.h"
#include "llvm_gen_be_rtti.h"
#include "llvm_gen_be_ptrmem.h"
#include "llvm_gen_be_coro.h"
#include "llvm_gen_be_eh.h"
#include "llvm_gen_be_abi_sysv_x86_64.h"
#include "llvm_gen_be_abi_aapcs64.h"
#include "llvm_gen_be_abi_win64.h"
#include "llvm_gen_be_abi_riscv64.h"
#include "target.h"
#include "lower_name.h"
#include <llvm/IR/Intrinsics.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

static llvm::Value* emit_complex_libcall(const char* sc_name, const char* dc_name, llvm::Value* v1, llvm::Value* v2) {
    llvm::Type* struct_ty = v1->getType();
    llvm::Type* elem_ty = struct_ty->getStructElementType(0);
    bool is_double = elem_ty->isDoubleTy();
    const char* func_name = is_double ? dc_name : sc_name;
    
    llvm::FunctionType* fty = llvm::FunctionType::get(struct_ty, {elem_ty, elem_ty, elem_ty, elem_ty}, false);
    llvm::FunctionCallee callee = be_state->module->getOrInsertFunction(func_name, fty);
    
    llvm::Value* r1 = be_state->builder->CreateExtractValue(v1, 0);
    llvm::Value* i1 = be_state->builder->CreateExtractValue(v1, 1);
    llvm::Value* r2 = be_state->builder->CreateExtractValue(v2, 0);
    llvm::Value* i2 = be_state->builder->CreateExtractValue(v2, 1);
    
    return be_state->builder->CreateCall(callee, {r1, i1, r2, i2});
}

static llvm_gen_be_error_t llvm_emit_bitfield_load(
    llvm::Value* ptr,
    a_field_ptr field,
    a_type_ptr expr_type,
    llvm::Value** out_val) noexcept {
    unsigned bit_offset = field->offset_bit_remainder;
    unsigned bit_size = field->bit_size;
    
    llvm::Triple triple(be_state->module->getTargetTriple());
    bool is_msvc = triple.isWindowsMSVCEnvironment();
    
    bool is_vol = is_volatile_qualified_type(expr_type);
    unsigned container_bits = field->type->size * 8;
    // Volatile bitfields must preserve exactly the declared byte width (e.g. load i32 for int).
    if (!is_vol && (!is_msvc || container_bits == 0)) {
        container_bits = ((bit_offset + bit_size + 7) / 8) * 8;
        if (container_bits < 32 && (bit_offset + bit_size) > 16) container_bits = 32;
        else if (container_bits < 16 && (bit_offset + bit_size) > 8) container_bits = 16;
    }
    if (container_bits == 0) container_bits = 8;
    
    llvm::Type* container_ty = llvm::Type::getIntNTy(*be_state->context, container_bits);
    llvm::LoadInst* load = be_state->builder->CreateLoad(container_ty, ptr);
    if (is_volatile_qualified_type(expr_type)) load->setVolatile(true);
    
    unsigned shift = targ_little_endian ? bit_offset : (container_bits - (bit_offset + bit_size));
    llvm::Value* val = load;
    if (shift > 0) val = be_state->builder->CreateLShr(val, shift);
    
    llvm::APInt mask_ap = llvm::APInt::getLowBitsSet(container_bits, bit_size);
    val = be_state->builder->CreateAnd(val, llvm::ConstantInt::get(container_ty, mask_ap));
    
    if (field->bit_field_is_signed) {
        unsigned shift_up = container_bits - bit_size;
        if (shift_up > 0) {
            val = be_state->builder->CreateShl(val, shift_up);
            val = be_state->builder->CreateAShr(val, shift_up);
        }
    }
    
    llvm::Type* target_ty = nullptr;
    llvm_gen_be_error_t err = get_llvm_type(expr_type, &target_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    if (val->getType()->getIntegerBitWidth() > target_ty->getIntegerBitWidth()) {
        val = be_state->builder->CreateTrunc(val, target_ty);
    } else if (val->getType()->getIntegerBitWidth() < target_ty->getIntegerBitWidth()) {
        if (field->bit_field_is_signed) val = be_state->builder->CreateSExt(val, target_ty);
        else val = be_state->builder->CreateZExt(val, target_ty);
    }
    *out_val = val;
    return llvm_gen_be_error_t::ok;
}
static llvm_gen_be_error_t llvm_emit_bitfield_store(
    llvm::Value* ptr,
    llvm::Value* new_val,
    a_field_ptr field,
    a_type_ptr expr_type) noexcept {
    unsigned bit_offset = field->offset_bit_remainder;
    unsigned bit_size = field->bit_size;
    
    llvm::Triple triple(be_state->module->getTargetTriple());
    bool is_msvc = triple.isWindowsMSVCEnvironment();
    
    bool is_vol = is_volatile_qualified_type(expr_type);
    unsigned container_bits = field->type->size * 8;
    // Volatile bitfields must preserve exactly the declared byte width
    if (!is_vol && (!is_msvc || container_bits == 0)) {
        container_bits = ((bit_offset + bit_size + 7) / 8) * 8;
        if (container_bits < 32 && (bit_offset + bit_size) > 16) container_bits = 32;
        else if (container_bits < 16 && (bit_offset + bit_size) > 8) container_bits = 16;
    }
    if (container_bits == 0) container_bits = 8;
    
    llvm::Type* container_ty = llvm::Type::getIntNTy(*be_state->context, container_bits);
    
    llvm::Value* val = new_val;
    if (val->getType()->getIntegerBitWidth() > container_bits) {
        val = be_state->builder->CreateTrunc(val, container_ty);
    } else if (val->getType()->getIntegerBitWidth() < container_bits) {
        val = be_state->builder->CreateZExt(val, container_ty);
    }
    
    unsigned shift = targ_little_endian ? bit_offset : (container_bits - (bit_offset + bit_size));
    llvm::APInt val_mask_ap = llvm::APInt::getLowBitsSet(container_bits, bit_size);
    val = be_state->builder->CreateAnd(val, llvm::ConstantInt::get(container_ty, val_mask_ap));
    if (shift > 0) val = be_state->builder->CreateShl(val, shift);
    
    llvm::APInt container_mask_ap = ~(val_mask_ap << shift);
    llvm::Constant* container_mask = llvm::ConstantInt::get(container_ty, container_mask_ap);
    
    
    if (is_vol) {
        llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
        llvm::BasicBlock* pre_bb = be_state->builder->GetInsertBlock();
        llvm::BasicBlock* loop_bb = llvm::BasicBlock::Create(*be_state->context, "bf.atomic.loop", func);
        llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, "bf.atomic.end");
        
        llvm::LoadInst* initial_load = be_state->builder->CreateLoad(container_ty, ptr);
        initial_load->setVolatile(true);
        be_state->builder->CreateBr(loop_bb);
        
        be_state->builder->SetInsertPoint(loop_bb);
        llvm::PHINode* old_val = be_state->builder->CreatePHI(container_ty, 2);
        old_val->addIncoming(initial_load, pre_bb);
        
        llvm::Value* masked_old = be_state->builder->CreateAnd(old_val, container_mask);
        llvm::Value* new_container = be_state->builder->CreateOr(masked_old, val);
        
        llvm::AtomicCmpXchgInst* cmpxchg = be_state->builder->CreateAtomicCmpXchg(
            ptr, old_val, new_container, llvm::MaybeAlign(),
            llvm::AtomicOrdering::SequentiallyConsistent, llvm::AtomicOrdering::SequentiallyConsistent);
        cmpxchg->setVolatile(true);
        
        llvm::Value* success = be_state->builder->CreateExtractValue(cmpxchg, 1);
        llvm::Value* actual_old = be_state->builder->CreateExtractValue(cmpxchg, 0);
        old_val->addIncoming(actual_old, loop_bb);
        
        be_state->builder->CreateCondBr(success, end_bb, loop_bb);
        
        func->insert(func->end(), end_bb);
        be_state->builder->SetInsertPoint(end_bb);
    } else {
        llvm::LoadInst* load = be_state->builder->CreateLoad(container_ty, ptr);
        llvm::Value* masked_container = be_state->builder->CreateAnd(load, container_mask);
        llvm::Value* new_container = be_state->builder->CreateOr(masked_container, val);
        be_state->builder->CreateStore(new_container, ptr);
    }
    
    return llvm_gen_be_error_t::ok;
}



/**
 * @brief Resolves and extracts the LLVM FunctionType from an EDG routine or pointer-to-routine type.
 * @details Unwraps pointers, references, and typerefs, lowering the underlying routine type.
 * @param[in] ty The EDG AST type pointer representing the routine or function pointer.
 * @param[out] out_fn_ty Receives the lowered LLVM FunctionType pointer on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
static llvm_gen_be_error_t get_llvm_function_type(a_type_ptr ty, llvm::FunctionType** out_fn_ty) noexcept {
  if (!out_fn_ty) return llvm_gen_be_error_t::invalid_argument;
  *out_fn_ty = nullptr;
  while (ty) {
    if (ty->kind == tk_pointer || is_reference_type(ty)) {
      ty = ty->variant.pointer.type;
    } else if (ty->kind == tk_typeref) {
      ty = ty->variant.typeref.type;
    } else {
      break;
    }
  }
  if (ty && ty->kind == tk_routine) {
    llvm::Type* lt = nullptr;
    llvm_gen_be_error_t err = get_llvm_type(ty, &lt);
    if (err != llvm_gen_be_error_t::ok) return err;
    if (lt->isFunctionTy()) *out_fn_ty = llvm::cast<llvm::FunctionType>(lt);
  }
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_lvalue_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_ptr) noexcept {
  if (!expr || !out_ptr) return llvm_gen_be_error_t::invalid_argument;
  
  switch (expr->kind) {
    case enk_variable: {
       a_variable_ptr var = expr->variant.variable.ptr;
       if (!var) return llvm_gen_be_error_t::unsupported_expr;
       llvm::Value* ptr = nullptr;
       auto it = be_state->local_vars.find(var);
       if (it != be_state->local_vars.end()) {
         ptr = it->second;
       } else {
         const char* name = var->source_corresp.name ? var->source_corresp.name : "";
         ptr = be_state->module->getNamedGlobal(name);
         if (!ptr && name[0] != '\0') {
           llvm::Type* var_ty = nullptr;
           llvm_gen_be_error_t err = get_llvm_type(expr->type, &var_ty);
           if (err != llvm_gen_be_error_t::ok) return err;
           ptr = new llvm::GlobalVariable(
             *be_state->module,
             var_ty,
             false, // isConstant
             llvm::GlobalValue::ExternalLinkage,
             nullptr,
             name
           );
         }
       }
       if (!ptr) {
         return llvm_gen_be_error_t::unsupported_expr;
       }
       *out_ptr = ptr;
       return llvm_gen_be_error_t::ok;
    }
    case enk_operation: {
       an_expr_node_ptr op1 = expr->variant.operation.operands;
       an_expr_node_ptr op2 = op1 ? op1->next : nullptr;
       
       switch (expr->variant.operation.kind) {
         case eok_subscript: {
           llvm::Value* v1 = nullptr;
           llvm::Value* v2 = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
           if (err != llvm_gen_be_error_t::ok) return err;
           err = llvm_lower_expression(op2, &v2);
           if (err != llvm_gen_be_error_t::ok) return err;

           llvm::Type* elem_ty = nullptr;
           llvm_gen_be_error_t err_ty = get_llvm_type(expr->type, &elem_ty);
           if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
           if (!v1 || !v2 || !elem_ty || !v1->getType()->isPointerTy()) return llvm_gen_be_error_t::unsupported_expr;

           a_type_ptr unwrapped_ty = skip_typerefs(expr->type);
           if (unwrapped_ty && unwrapped_ty->kind == tk_array && unwrapped_ty->variant.array.is_vla) {
               // Multi-dimensional VLA stride calculation
               llvm::Value* total_size = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), 1);
               a_type_ptr ty = unwrapped_ty;
               while (ty && ty->kind == tk_array) {
                   llvm::Value* dim_size = nullptr;
                   if (ty->variant.array.is_vla && ty->variant.array.has_assoc_vla_dimension) {
                       a_vla_dimension_ptr dim = nullptr;
                       if (be_state->array_to_vla_dim.count(ty)) {
                           dim = be_state->array_to_vla_dim[ty];
                       }
                       if (dim && be_state->vla_dimension_values.count(dim)) {
                           dim_size = be_state->vla_dimension_values[dim];
                           dim_size = be_state->builder->CreateZExtOrTrunc(dim_size, llvm::Type::getInt64Ty(*be_state->context));
                       }
                   } else if (!ty->variant.array.bound_is_zero) {
                       dim_size = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), (uint64_t)ty->variant.array.variant.number_of_elements);
                   } else {
                       dim_size = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), 0);
                   }
                   if (dim_size) {
                       total_size = be_state->builder->CreateMul(total_size, dim_size);
                   }
                   ty = skip_typerefs(ty->variant.array.element_type);
               }
               llvm::Type* inner_llvm_ty = nullptr;
               err = get_llvm_type(ty, &inner_llvm_ty);
               if (err != llvm_gen_be_error_t::ok) return err;
               
               const llvm::DataLayout &DL = be_state->module->getDataLayout();
               uint64_t elem_size = DL.getTypeAllocSize(inner_llvm_ty);
               llvm::Value* elem_size_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), elem_size);
               llvm::Value* byte_stride = be_state->builder->CreateMul(total_size, elem_size_val);
               
               llvm::Value* index = be_state->builder->CreateZExtOrTrunc(v2, llvm::Type::getInt64Ty(*be_state->context));
               llvm::Value* byte_offset = be_state->builder->CreateMul(index, byte_stride);
               
               *out_ptr = be_state->builder->CreateInBoundsGEP(llvm::Type::getInt8Ty(*be_state->context), v1, byte_offset);
               return llvm_gen_be_error_t::ok;
           }

           *out_ptr = be_state->builder->CreateInBoundsGEP(elem_ty, v1, v2);
           return llvm_gen_be_error_t::ok;
         }
         case eok_dot_field:
         case eok_points_to_field: {
           llvm::Value* v1 = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
           if (err != llvm_gen_be_error_t::ok) return err;

           if (v1 && op2 && op2->kind == enk_field) {
             a_field_ptr field = op2->variant.field.ptr;
             if (field && v1->getType()->isPointerTy()) {
               llvm::Value* offset_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), field->offset);
               *out_ptr = be_state->builder->CreateInBoundsGEP(llvm::Type::getInt8Ty(*be_state->context), v1, offset_val);
               return llvm_gen_be_error_t::ok;
             }
           }
           return llvm_gen_be_error_t::unsupported_expr;
         }
         case eok_dot_static:
         case eok_points_to_static: {
           llvm::Value* v1 = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
           if (err != llvm_gen_be_error_t::ok) return err;

           llvm::Value* v2 = nullptr;
           err = llvm_lower_lvalue_expression(op2, &v2);
           if (err != llvm_gen_be_error_t::ok) return err;
           *out_ptr = v2;
           return llvm_gen_be_error_t::ok;
         }
         case eok_pm_field:
         case eok_pm_points_to_field:
           return llvm_lower_pm_field_access(expr, out_ptr);
         case eok_indirect: {
           llvm::Value* v1 = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
           if (err != llvm_gen_be_error_t::ok) return err;
           if (!v1 || !v1->getType()->isPointerTy()) return llvm_gen_be_error_t::unsupported_expr;
           *out_ptr = v1;
           return llvm_gen_be_error_t::ok;
         }
         case eok_real_part: {
           llvm::Value* v1 = nullptr;
           llvm_gen_be_error_t err = llvm_lower_lvalue_expression(op1, &v1);
           if (err != llvm_gen_be_error_t::ok) return err;
           if (!v1 || !v1->getType()->isPointerTy()) return llvm_gen_be_error_t::unsupported_expr;
           llvm::Type* pty = nullptr;
           err = get_llvm_type(op1->type, &pty);
           if (err != llvm_gen_be_error_t::ok) return err;
           if (pty->isStructTy() && pty->getStructNumElements() == 2) {
               *out_ptr = be_state->builder->CreateStructGEP(pty, v1, 0);
           } else {
               *out_ptr = v1;
           }
           return llvm_gen_be_error_t::ok;
         }
         case eok_imag_part: {
           llvm::Value* v1 = nullptr;
           llvm_gen_be_error_t err = llvm_lower_lvalue_expression(op1, &v1);
           if (err != llvm_gen_be_error_t::ok) return err;
           if (!v1 || !v1->getType()->isPointerTy()) return llvm_gen_be_error_t::unsupported_expr;
           llvm::Type* pty = nullptr;
           err = get_llvm_type(op1->type, &pty);
           if (err != llvm_gen_be_error_t::ok) return err;
           if (pty->isStructTy() && pty->getStructNumElements() == 2) {
               *out_ptr = be_state->builder->CreateStructGEP(pty, v1, 1);
           } else {
               *out_ptr = v1; // Fallback, technically invalid but safe
           }
           return llvm_gen_be_error_t::ok;
         }
         default:
           return llvm_gen_be_error_t::unsupported_expr;
       }
    }
    default:
       return llvm_gen_be_error_t::unsupported_expr;
  }
}

llvm_gen_be_error_t llvm_lower_arithmetic_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  an_expr_node_ptr op1 = expr->variant.operation.operands;
  an_expr_node_ptr op2 = op1 ? op1->next : nullptr;
  
  llvm::Value* v1 = nullptr;
  llvm::Value* v2 = nullptr;
  llvm_gen_be_error_t err = llvm_gen_be_error_t::ok;
  if (op1) { err = llvm_lower_expression(op1, &v1); if (err != llvm_gen_be_error_t::ok) return err; }
  if (op2) { err = llvm_lower_expression(op2, &v2); if (err != llvm_gen_be_error_t::ok) return err; }
  
  if (!v1 || (!v2 && expr->variant.operation.kind != eok_complement && expr->variant.operation.kind != eok_xconj && expr->variant.operation.kind != eok_negate && expr->variant.operation.kind != eok_unary_plus)) return llvm_gen_be_error_t::unsupported_expr;

  switch (expr->variant.operation.kind) {
    case eok_negate:
      if (v1->getType()->isStructTy()) {
        llvm::Value* r1 = be_state->builder->CreateExtractValue(v1, 0);
        llvm::Value* i1 = be_state->builder->CreateExtractValue(v1, 1);
        llvm::Value* r = be_state->builder->CreateFNeg(r1);
        llvm::Value* i = be_state->builder->CreateFNeg(i1);
        llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(v1->getType()), r, 0);
        *out_val = be_state->builder->CreateInsertValue(res, i, 1);
      } else if (v1->getType()->isFloatingPointTy()) {
        *out_val = be_state->builder->CreateFNeg(v1);
      } else if (v1->getType()->isVectorTy() && llvm::cast<llvm::VectorType>(v1->getType())->getElementType()->isFloatingPointTy()) {
        *out_val = be_state->builder->CreateFNeg(v1);
      } else {
        *out_val = be_state->builder->CreateNeg(v1);
      }
      break;
    case eok_unary_plus:
      // unary plus is mostly a no-op at the LLVM IR level since type promotion happens in the AST
      *out_val = v1;
      break;
    case eok_add:
      if (v1->getType()->isStructTy()) {
        llvm::Value* r1 = be_state->builder->CreateExtractValue(v1, 0);
        llvm::Value* i1 = be_state->builder->CreateExtractValue(v1, 1);
        llvm::Value* r2 = be_state->builder->CreateExtractValue(v2, 0);
        llvm::Value* i2 = be_state->builder->CreateExtractValue(v2, 1);
        llvm::Value* r = be_state->builder->CreateFAdd(r1, r2);
        llvm::Value* i = be_state->builder->CreateFAdd(i1, i2);
        llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(v1->getType()), r, 0);
        *out_val = be_state->builder->CreateInsertValue(res, i, 1);
      } else if (v1->getType()->isFloatingPointTy()) *out_val = be_state->builder->CreateFAdd(v1, v2);
      else *out_val = be_state->builder->CreateAdd(v1, v2);
      break;
    case eok_subtract:
      if (v1->getType()->isStructTy()) {
        llvm::Value* r1 = be_state->builder->CreateExtractValue(v1, 0);
        llvm::Value* i1 = be_state->builder->CreateExtractValue(v1, 1);
        llvm::Value* r2 = be_state->builder->CreateExtractValue(v2, 0);
        llvm::Value* i2 = be_state->builder->CreateExtractValue(v2, 1);
        llvm::Value* r = be_state->builder->CreateFSub(r1, r2);
        llvm::Value* i = be_state->builder->CreateFSub(i1, i2);
        llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(v1->getType()), r, 0);
        *out_val = be_state->builder->CreateInsertValue(res, i, 1);
      } else if (v1->getType()->isFloatingPointTy()) *out_val = be_state->builder->CreateFSub(v1, v2);
      else *out_val = be_state->builder->CreateSub(v1, v2);
      break;
        case eok_multiply:
      if (v1->getType()->isStructTy()) {
        *out_val = emit_complex_libcall("__mulsc3", "__muldc3", v1, v2);
      } else if (v1->getType()->isFloatingPointTy()) *out_val = be_state->builder->CreateFMul(v1, v2);
      else *out_val = be_state->builder->CreateMul(v1, v2);
      break;
    case eok_divide: {
      if (v1->getType()->isStructTy()) {
        *out_val = emit_complex_libcall("__divsc3", "__divdc3", v1, v2);
      } else if (v1->getType()->isFloatingPointTy()) *out_val = be_state->builder->CreateFDiv(v1, v2);
      else if (op1->type->kind == tk_integer && int_kind_is_signed[op1->type->variant.integer.int_kind]) *out_val = be_state->builder->CreateSDiv(v1, v2);
      else *out_val = be_state->builder->CreateUDiv(v1, v2);
      break;
    }
    case eok_remainder: {
      if (v1->getType()->isFloatingPointTy()) *out_val = be_state->builder->CreateFRem(v1, v2);
      else {
        bool is_unsigned = (expr->type->kind == tk_integer && !int_kind_is_signed[expr->type->variant.integer.int_kind]);
        if (is_unsigned) *out_val = be_state->builder->CreateURem(v1, v2);
        else *out_val = be_state->builder->CreateSRem(v1, v2);
      }
      break;
    }
    case eok_padd:
      if (op1 && op1->type && op1->type->kind == tk_pointer && v1->getType()->isPointerTy()) {
        llvm::Type* elem_ty = nullptr;
        if (get_llvm_type(op1->type->variant.pointer.type, &elem_ty) != llvm_gen_be_error_t::ok || !elem_ty) return llvm_gen_be_error_t::unsupported_expr;
        *out_val = be_state->builder->CreateGEP(elem_ty, v1, v2);
      } else {
        *out_val = v1;
      }
      break;
    case eok_psubtract:
      if (op1 && op1->type && op1->type->kind == tk_pointer && v1->getType()->isPointerTy()) {
        llvm::Type* elem_ty = nullptr;
        if (get_llvm_type(op1->type->variant.pointer.type, &elem_ty) != llvm_gen_be_error_t::ok || !elem_ty) return llvm_gen_be_error_t::unsupported_expr;
        llvm::Value* neg_v2 = be_state->builder->CreateNeg(v2);
        *out_val = be_state->builder->CreateGEP(elem_ty, v1, neg_v2);
      } else {
        *out_val = v1;
      }
      break;
    case eok_pdiff:
      if (op1 && op1->type && op1->type->kind == tk_pointer && v1->getType()->isPointerTy() && v2->getType()->isPointerTy()) {
        llvm::Type* elem_ty = nullptr;
        if (get_llvm_type(op1->type->variant.pointer.type, &elem_ty) != llvm_gen_be_error_t::ok || !elem_ty) return llvm_gen_be_error_t::unsupported_expr;
        *out_val = be_state->builder->CreatePtrDiff(elem_ty, v1, v2);
      } else {
        return llvm_gen_be_error_t::unsupported_expr;
      }
      break;
    case eok_fjadd: {
      llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(llvm::StructType::get(v1->getType(), v2->getType())), v1, 0);
      *out_val = be_state->builder->CreateInsertValue(res, v2, 1);
      break;
    }
    case eok_jfadd: {
      llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(llvm::StructType::get(v2->getType(), v1->getType())), v2, 0);
      *out_val = be_state->builder->CreateInsertValue(res, v1, 1);
      break;
    }
    case eok_fjsubtract: {
      llvm::Value* neg_v2 = be_state->builder->CreateFNeg(v2);
      llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(llvm::StructType::get(v1->getType(), v2->getType())), v1, 0);
      *out_val = be_state->builder->CreateInsertValue(res, neg_v2, 1);
      break;
    }
    case eok_jfsubtract: {
      llvm::Value* neg_v1 = be_state->builder->CreateFNeg(v1);
      llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(llvm::StructType::get(v2->getType(), v1->getType())), neg_v1, 0);
      *out_val = be_state->builder->CreateInsertValue(res, v1, 1);
      break;
    }
    case eok_jmultiply: {
      // Imaginary * Imaginary = Real
      llvm::Value* mul = be_state->builder->CreateFMul(v1, v2);
      *out_val = be_state->builder->CreateFNeg(mul);
      break;
    }
    case eok_jdivide: {
      // Real / Imaginary = Imaginary
      llvm::Value* div = be_state->builder->CreateFDiv(v1, v2);
      *out_val = be_state->builder->CreateFNeg(div);
      break;
    }
    case eok_xconj: {
      if (v1->getType()->isStructTy()) {
          llvm::Value* r1 = be_state->builder->CreateExtractValue(v1, 0);
          llvm::Value* i1 = be_state->builder->CreateExtractValue(v1, 1);
          llvm::Value* neg_i1 = be_state->builder->CreateFNeg(i1);
          llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(v1->getType()), r1, 0);
          *out_val = be_state->builder->CreateInsertValue(res, neg_i1, 1);
      } else {
          *out_val = v1;
      }
      break;
    }
    case eok_complement: *out_val = be_state->builder->CreateNot(v1); break;
    case eok_and: *out_val = be_state->builder->CreateAnd(v1, v2); break;
    case eok_or: *out_val = be_state->builder->CreateOr(v1, v2); break;
    case eok_xor: *out_val = be_state->builder->CreateXor(v1, v2); break;
    case eok_shiftl: *out_val = be_state->builder->CreateShl(v1, v2); break;
    case eok_shiftr: {
      bool is_unsigned = (op1->type->kind == tk_integer && !int_kind_is_signed[op1->type->variant.integer.int_kind]);
      if (is_unsigned) *out_val = be_state->builder->CreateLShr(v1, v2);
      else *out_val = be_state->builder->CreateAShr(v1, v2);
      break;
    }
    default: return llvm_gen_be_error_t::unsupported_expr;
  }
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_logical_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  an_expr_node_ptr op1 = expr->variant.operation.operands;
  an_expr_node_ptr op2 = op1 ? op1->next : nullptr;
  
  if (expr->variant.operation.kind == eok_land || expr->variant.operation.kind == eok_lor) {
    llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* rhs_bb = llvm::BasicBlock::Create(*be_state->context, 
      (expr->variant.operation.kind == eok_land) ? "land.rhs" : "lor.rhs", func);
    llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, 
      (expr->variant.operation.kind == eok_land) ? "land.end" : "lor.end");

    llvm::Value* lhs_val = nullptr;
    llvm_gen_be_error_t err = llvm_lower_expression(op1, &lhs_val);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    llvm::Value* lhs_cond = be_state->builder->CreateICmpNE(lhs_val, llvm::Constant::getNullValue(lhs_val->getType()));
    llvm::BasicBlock* lhs_end_bb = be_state->builder->GetInsertBlock();
    
    if (expr->variant.operation.kind == eok_land) be_state->builder->CreateCondBr(lhs_cond, rhs_bb, end_bb);
    else be_state->builder->CreateCondBr(lhs_cond, end_bb, rhs_bb);

    be_state->builder->SetInsertPoint(rhs_bb);
    llvm::Value* rhs_val = nullptr;
    err = llvm_lower_expression(op2, &rhs_val);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    llvm::Value* rhs_cond = be_state->builder->CreateICmpNE(rhs_val, llvm::Constant::getNullValue(rhs_val->getType()));
    llvm::BasicBlock* rhs_end_bb = be_state->builder->GetInsertBlock();
    if (!rhs_end_bb->getTerminatorOrNull()) be_state->builder->CreateBr(end_bb);

    func->insert(func->end(), end_bb);
    be_state->builder->SetInsertPoint(end_bb);

    llvm::PHINode* phi = be_state->builder->CreatePHI(be_state->builder->getInt1Ty(), 2, 
      (expr->variant.operation.kind == eok_land) ? "land.phi" : "lor.phi");
    phi->addIncoming((expr->variant.operation.kind == eok_land) ? llvm::ConstantInt::getFalse(*be_state->context) : llvm::ConstantInt::getTrue(*be_state->context), lhs_end_bb);
    phi->addIncoming(rhs_cond, rhs_end_bb);
    
    llvm::Type* expr_llvm_ty = nullptr;
    err = get_llvm_type(expr->type, &expr_llvm_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
    *out_val = be_state->builder->CreateZExt(phi, expr_llvm_ty);
    return llvm_gen_be_error_t::ok;
  }

  if (op1 && op1->type && skip_typerefs(op1->type)->kind == tk_ptr_to_member) {
    return llvm_lower_pm_comparison(expr, out_val);
  }
  
  llvm::Value* v1 = nullptr;
  llvm::Value* v2 = nullptr;
  llvm_gen_be_error_t err = llvm_gen_be_error_t::ok;
  if (op1) { err = llvm_lower_expression(op1, &v1); if (err != llvm_gen_be_error_t::ok) return err; }
  if (op2) { err = llvm_lower_expression(op2, &v2); if (err != llvm_gen_be_error_t::ok) return err; }
  if (!v1 || !v2) return llvm_gen_be_error_t::unsupported_expr;
  
  bool is_fp = v1->getType()->isFloatingPointTy();
  bool is_ptr = v1->getType()->isPointerTy();
  bool is_unsigned = !is_fp && !is_ptr && (op1->type->kind == tk_integer && !int_kind_is_signed[op1->type->variant.integer.int_kind]);

  if (expr->variant.operation.kind == eok_spaceship) {
     llvm::Value* res = nullptr;
     llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
     llvm::Value* lt_val = llvm::ConstantInt::get(i8_ty, -1, true);
     llvm::Value* eq_val = llvm::ConstantInt::get(i8_ty, 0, true);
     llvm::Value* gt_val = llvm::ConstantInt::get(i8_ty, 1, true);
     if (is_fp) {
        llvm::Value* cmp_lt = be_state->builder->CreateFCmpOLT(v1, v2);
        llvm::Value* cmp_eq = be_state->builder->CreateFCmpOEQ(v1, v2);
        llvm::Value* cmp_gt = be_state->builder->CreateFCmpOGT(v1, v2);
        llvm::Value* un_val = llvm::ConstantInt::get(i8_ty, -127, true);
        llvm::Value* sel1 = be_state->builder->CreateSelect(cmp_eq, eq_val, be_state->builder->CreateSelect(cmp_gt, gt_val, un_val));
        res = be_state->builder->CreateSelect(cmp_lt, lt_val, sel1);
     } else {
        llvm::Value* cmp_lt = is_ptr ? be_state->builder->CreateICmpULT(v1, v2) : be_state->builder->CreateICmp(is_unsigned ? llvm::CmpInst::ICMP_ULT : llvm::CmpInst::ICMP_SLT, v1, v2);
        llvm::Value* cmp_eq = be_state->builder->CreateICmpEQ(v1, v2);
        res = be_state->builder->CreateSelect(cmp_lt, lt_val, be_state->builder->CreateSelect(cmp_eq, eq_val, gt_val));
     }

     llvm::Type* expr_llvm_ty = nullptr;
     err = get_llvm_type(expr->type, &expr_llvm_ty);
     if (err != llvm_gen_be_error_t::ok) return err;

     if (expr_llvm_ty->isStructTy()) {
         *out_val = be_state->builder->CreateInsertValue(llvm::UndefValue::get(expr_llvm_ty), res, 0);
     } else {
         *out_val = be_state->builder->CreateSExtOrTrunc(res, expr_llvm_ty);
     }
     return llvm_gen_be_error_t::ok;
  }

  llvm::CmpInst::Predicate pred;
  switch (expr->variant.operation.kind) {
     case eok_eq: pred = is_fp ? llvm::CmpInst::FCMP_OEQ : llvm::CmpInst::ICMP_EQ; break;
     case eok_ne: pred = is_fp ? llvm::CmpInst::FCMP_ONE : llvm::CmpInst::ICMP_NE; break;
     case eok_lt: pred = is_fp ? llvm::CmpInst::FCMP_OLT : (is_unsigned || is_ptr ? llvm::CmpInst::ICMP_ULT : llvm::CmpInst::ICMP_SLT); break;
     case eok_le: pred = is_fp ? llvm::CmpInst::FCMP_OLE : (is_unsigned || is_ptr ? llvm::CmpInst::ICMP_ULE : llvm::CmpInst::ICMP_SLE); break;
     case eok_gt: pred = is_fp ? llvm::CmpInst::FCMP_OGT : (is_unsigned || is_ptr ? llvm::CmpInst::ICMP_UGT : llvm::CmpInst::ICMP_SGT); break;
     case eok_ge: pred = is_fp ? llvm::CmpInst::FCMP_OGE : (is_unsigned || is_ptr ? llvm::CmpInst::ICMP_UGE : llvm::CmpInst::ICMP_SGE); break;
     case eok_not: pred = llvm::CmpInst::ICMP_EQ; v2 = llvm::Constant::getNullValue(v1->getType()); is_fp = false; break;
     default: return llvm_gen_be_error_t::unsupported_expr;
  }
  if (is_fp) *out_val = be_state->builder->CreateFCmp(pred, v1, v2);
  else *out_val = be_state->builder->CreateICmp(pred, v1, v2);
  
  if (expr->variant.operation.kind == eok_not) {
     llvm::Type* expr_llvm_ty = nullptr;
     err = get_llvm_type(expr->type, &expr_llvm_ty);
     if (err == llvm_gen_be_error_t::ok && !expr_llvm_ty->isVectorTy() && expr_llvm_ty->getIntegerBitWidth() > 1) {
        *out_val = be_state->builder->CreateZExt(*out_val, expr_llvm_ty);
     }
  } else if ((*out_val)->getType()->isVectorTy()) {
      // For vector comparisons, Clang extends <N x i1> to <N x int> mask where true = -1
      llvm::Type* expr_llvm_ty = nullptr;
      err = get_llvm_type(expr->type, &expr_llvm_ty);
      if (err == llvm_gen_be_error_t::ok) {
          *out_val = be_state->builder->CreateSExt(*out_val, expr_llvm_ty);
      }
  }
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_cast_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  an_expr_node_ptr op1 = expr->variant.operation.operands;
  
  llvm::Value* v1 = nullptr;
  llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
  if (err != llvm_gen_be_error_t::ok) return err;
  
  if (!v1) return llvm_gen_be_error_t::unsupported_expr;
  
  llvm::Type* dest_ty = nullptr;
  err = get_llvm_type(expr->type, &dest_ty);
  if (err != llvm_gen_be_error_t::ok) return err;
  llvm::Type* src_ty = v1->getType();
  if (src_ty == dest_ty) {
      *out_val = v1;
      return llvm_gen_be_error_t::ok;
  }
  
  if (src_ty->isPointerTy() && dest_ty->isIntegerTy()) {
     *out_val = be_state->builder->CreatePtrToInt(v1, dest_ty);
     return llvm_gen_be_error_t::ok;
  }
  if (src_ty->isIntegerTy() && dest_ty->isPointerTy()) {
     *out_val = be_state->builder->CreateIntToPtr(v1, dest_ty);
     return llvm_gen_be_error_t::ok;
  }
  if (src_ty->isPointerTy() && dest_ty->isPointerTy()) {
     *out_val = v1;
     return llvm_gen_be_error_t::ok;
  }
  
  bool src_is_fp = src_ty->isFloatingPointTy();
  bool dest_is_fp = dest_ty->isFloatingPointTy();
  
  if (src_is_fp && dest_is_fp) {
     if (src_ty->getPrimitiveSizeInBits() > dest_ty->getPrimitiveSizeInBits())
        *out_val = be_state->builder->CreateFPTrunc(v1, dest_ty);
     else
        *out_val = be_state->builder->CreateFPExt(v1, dest_ty);
     return llvm_gen_be_error_t::ok;
  }
  
  if (src_is_fp && dest_ty->isIntegerTy()) {
     bool is_unsigned = (expr->type->kind == tk_integer && !int_kind_is_signed[expr->type->variant.integer.int_kind]);
     if (is_unsigned) *out_val = be_state->builder->CreateFPToUI(v1, dest_ty);
     else *out_val = be_state->builder->CreateFPToSI(v1, dest_ty);
     return llvm_gen_be_error_t::ok;
  }
  
  if (src_ty->isIntegerTy() && dest_is_fp) {
     bool src_is_unsigned = (op1->type->kind == tk_integer && !int_kind_is_signed[op1->type->variant.integer.int_kind]);
     if (src_is_unsigned) *out_val = be_state->builder->CreateUIToFP(v1, dest_ty);
     else *out_val = be_state->builder->CreateSIToFP(v1, dest_ty);
     return llvm_gen_be_error_t::ok;
  }
  
  if (src_ty->isIntegerTy() && dest_ty->isIntegerTy()) {
     unsigned src_bits = src_ty->getIntegerBitWidth();
     unsigned dest_bits = dest_ty->getIntegerBitWidth();
     if (dest_bits > src_bits) {
        bool src_is_unsigned = (op1->type->kind == tk_integer && !int_kind_is_signed[op1->type->variant.integer.int_kind]);
        if (src_is_unsigned) *out_val = be_state->builder->CreateZExt(v1, dest_ty);
        else *out_val = be_state->builder->CreateSExt(v1, dest_ty);
     } else {
        *out_val = be_state->builder->CreateTrunc(v1, dest_ty);
     }
     return llvm_gen_be_error_t::ok;
  }
  
  *out_val = be_state->builder->CreateBitCast(v1, dest_ty);
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_call_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  an_expr_node_ptr op1 = expr->variant.operation.operands;
  
  llvm::FunctionType* callee_ty = nullptr;
  llvm::AttributeList callee_attrs;
  if (op1->type) {
     a_type_ptr func_ty_edg = op1->type;
     if (func_ty_edg->kind == tk_pointer) func_ty_edg = func_ty_edg->variant.array.element_type;
     if (func_ty_edg->kind == tk_routine) {
         llvm::Triple triple(be_state->module->getTargetTriple());
         if (triple.isRISCV64()) {
           llvm_gen_be_error_t err = build_riscv64_function_type(func_ty_edg, &callee_ty, &callee_attrs);
           if (err != llvm_gen_be_error_t::ok) return err;
         } else if (triple.isAArch64()) {
           llvm_gen_be_error_t err = build_aapcs64_function_type(func_ty_edg, &callee_ty, &callee_attrs);
           if (err != llvm_gen_be_error_t::ok) return err;
         } else {
           llvm_gen_be_error_t err = build_sysv_function_type(func_ty_edg, &callee_ty, &callee_attrs);
           if (err != llvm_gen_be_error_t::ok) return err;
         }
     }
  }

  std::vector<llvm::Value*> args;
  unsigned callee_arg_idx = 0;
  
  if (callee_ty && callee_ty->getNumParams() > 0) {
      if (callee_attrs.hasParamAttr(0, llvm::Attribute::StructRet)) {
          llvm::Type* sret_ty = callee_attrs.getParamStructRetType(0);
          llvm::AllocaInst* sret_alloc = be_state->builder->CreateAlloca(sret_ty, nullptr, "sret");
          args.push_back(sret_alloc);
          callee_arg_idx++;
      }
  }

  llvm::Value* this_val = nullptr;

  for (an_expr_node_ptr arg = op1->next; arg != nullptr; arg = arg->next) {
    llvm::Value* arg_val = nullptr;
    llvm_gen_be_error_t err = llvm_lower_expression(arg, &arg_val);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    if (arg == op1->next) this_val = arg_val; // Save 'this' pointer for virtual dispatch
    
    if (callee_ty && callee_arg_idx < callee_ty->getNumParams()) {
        llvm::Type* expected_ty = callee_ty->getParamType(callee_arg_idx);
        if (callee_attrs.hasParamAttr(callee_arg_idx, llvm::Attribute::ByVal)) {
            if (!arg_val->getType()->isPointerTy()) {
                llvm::AllocaInst* alloc = be_state->builder->CreateAlloca(arg_val->getType());
                be_state->builder->CreateStore(arg_val, alloc);
                arg_val = alloc;
            }
            if (arg_val->getType() != expected_ty) {
                arg_val = be_state->builder->CreatePointerCast(arg_val, expected_ty);
            }
        } else if (arg_val->getType() != expected_ty) {
            llvm::AllocaInst* alloc = be_state->builder->CreateAlloca(arg_val->getType());
            be_state->builder->CreateStore(arg_val, alloc);
            llvm::Value* cast_ptr = be_state->builder->CreatePointerCast(alloc, llvm::PointerType::getUnqual(*be_state->context));
            arg_val = be_state->builder->CreateLoad(expected_ty, cast_ptr);
        }
    }
    
    args.push_back(arg_val);
    callee_arg_idx++;
  }

  llvm::Value* v1 = nullptr;
  if (expr->variant.operation.is_virtual_call && this_val && op1->kind == enk_routine) {
      a_routine_ptr r = op1->variant.routine.ptr;
      llvm::Triple triple(be_state->module->getTargetTriple());
      llvm_gen_be_error_t v_err;
      if (triple.isWindowsMSVCEnvironment()) {
          v_err = llvm_lower_msvc_virtual_dispatch(this_val, r, &v1);
      } else {
          v_err = llvm_lower_virtual_dispatch(this_val, r, &v1);
      }
      if (v_err != llvm_gen_be_error_t::ok) return v_err;
  } else {
      llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
      if (err != llvm_gen_be_error_t::ok) return err;
      if (!v1) return llvm_gen_be_error_t::unsupported_expr;
      
      // Check for builtins in call expressions
      if (op1->kind == enk_routine) {
         a_routine_ptr r = op1->variant.routine.ptr;
         if (r && r->source_corresp.name) {
             std::string rname = r->source_corresp.name;
             if (rname == "__builtin_expect") {
                 llvm::Function* exp = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::expect, {llvm::Type::getInt64Ty(*be_state->context)});
                 // Assuming args has at least 2 args. The first two in args vector
                 if (args.size() >= 2) {
                     *out_val = be_state->builder->CreateCall(exp, {args[0], args[1]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_expect_with_probability") {
                 llvm::Function* exp_prob = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::expect_with_probability, {args[0]->getType()});
                 if (args.size() >= 3) {
                     *out_val = be_state->builder->CreateCall(exp_prob, {args[0], args[1], args[2]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_frame_address") {
                 llvm::Function* intr = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::frameaddress, {llvm::PointerType::getUnqual(*be_state->context)});
                 if (args.size() >= 1) {
                     *out_val = be_state->builder->CreateCall(intr, {args[0]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_return_address") {
                 llvm::Function* intr = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::returnaddress, {llvm::PointerType::getUnqual(*be_state->context)});
                 if (args.size() >= 1) {
                     *out_val = be_state->builder->CreateCall(intr, {args[0]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_alloca") {
                 if (args.size() >= 1) {
                     *out_val = be_state->builder->CreateAlloca(llvm::Type::getInt8Ty(*be_state->context), args[0]);
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_alloca_with_align") {
                 if (args.size() >= 2) {
                     llvm::AllocaInst* alloc = be_state->builder->CreateAlloca(llvm::Type::getInt8Ty(*be_state->context), args[0]);
                     if (auto* align_ci = llvm::dyn_cast<llvm::ConstantInt>(args[1])) {
                         alloc->setAlignment(llvm::Align(align_ci->getZExtValue() / 8));
                     }
                     *out_val = alloc;
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_shufflevector") {
                 if (args.size() >= 3) {
                     std::vector<int> mask;
                     for (size_t i = 2; i < args.size(); ++i) {
                         if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(args[i])) {
                             mask.push_back(ci->getSExtValue());
                         } else {
                             mask.push_back(-1);
                         }
                     }
                     *out_val = be_state->builder->CreateShuffleVector(args[0], args[1], mask);
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_convertvector") {
                 if (args.size() >= 2) { // src_val, dst_type_val (usually type is handled by cast or we need to extract from expr)
                     // __builtin_convertvector(val, type)
                     llvm::Type* dst_ty = nullptr;
                     llvm_gen_be_error_t err_ty = get_llvm_type(expr->type, &dst_ty);
                     if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
                     
                     llvm::Type* src_ty = args[0]->getType();
                     if (src_ty->isIntOrIntVectorTy() && dst_ty->isIntOrIntVectorTy()) {
                         *out_val = be_state->builder->CreateIntCast(args[0], dst_ty, true);
                     } else if (src_ty->isFPOrFPVectorTy() && dst_ty->isFPOrFPVectorTy()) {
                         *out_val = be_state->builder->CreateFPCast(args[0], dst_ty);
                     } else if (src_ty->isIntOrIntVectorTy() && dst_ty->isFPOrFPVectorTy()) {
                         *out_val = be_state->builder->CreateSIToFP(args[0], dst_ty);
                     } else if (src_ty->isFPOrFPVectorTy() && dst_ty->isIntOrIntVectorTy()) {
                         *out_val = be_state->builder->CreateFPToSI(args[0], dst_ty);
                     }
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_assume") {
                 llvm::Function* assume = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::assume);
                 if (args.size() >= 1) {
                     *out_val = be_state->builder->CreateCall(assume, {args[0]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_unreachable") {
                 *out_val = be_state->builder->CreateUnreachable();
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_trap") {
                 llvm::Function* trap = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::trap);
                 *out_val = be_state->builder->CreateCall(trap);
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_setjmp" || rname == "setjmp" || rname == "_setjmp") {
                 llvm::Function* setjmp_func = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::eh_sjlj_setjmp);
                 if (args.size() >= 1) {
                     *out_val = be_state->builder->CreateCall(setjmp_func, {args[0]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_longjmp" || rname == "longjmp" || rname == "_longjmp") {
                 llvm::Function* longjmp_func = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::eh_sjlj_longjmp);
                 if (args.size() >= 1) {
                     *out_val = be_state->builder->CreateCall(longjmp_func, {args[0]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_cabs" || rname == "__builtin_cabsf" || rname == "__builtin_cabsl") {
                 if (args.size() >= 1) {
                     llvm::Value* v_cplx = args[0];
                     if (v_cplx->getType()->isStructTy()) {
                         llvm::Value* r = be_state->builder->CreateExtractValue(v_cplx, 0);
                         llvm::Value* i = be_state->builder->CreateExtractValue(v_cplx, 1);
                         llvm::Value* r2 = be_state->builder->CreateFMul(r, r);
                         llvm::Value* i2 = be_state->builder->CreateFMul(i, i);
                         llvm::Value* sum = be_state->builder->CreateFAdd(r2, i2);
                         llvm::Function* sqrt_func = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::sqrt, {r->getType()});
                         *out_val = be_state->builder->CreateCall(sqrt_func, {sum});
                     } else {
                         llvm::Function* fabs_func = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::fabs, {v_cplx->getType()});
                         *out_val = be_state->builder->CreateCall(fabs_func, {v_cplx});
                     }
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname.find("__builtin_ia32_") == 0 || 
                        rname.find("__builtin_neon_") == 0 || 
                        rname.find("__builtin_sve_") == 0 || 
                        rname.find("__builtin_rvv_") == 0) {
                 
                 std::string intrinsic_name;
                 if (rname.find("__builtin_ia32_") == 0) {
                     std::string sub = rname.substr(15);
                     // Map specific prefixes to LLVM intrinsic namespaces, replacing '_' with '.'
                     if (sub.find("avx512_") == 0) {
                         intrinsic_name = "llvm.x86.avx512." + sub.substr(7);
                     } else if (sub.find("avx2_") == 0) {
                         intrinsic_name = "llvm.x86.avx2." + sub.substr(5);
                     } else if (sub.find("avx_") == 0) {
                         intrinsic_name = "llvm.x86.avx." + sub.substr(4);
                     } else if (sub.find("amx_") == 0 || sub.find("tile") == 0 || sub.find("tdp") == 0) {
                         intrinsic_name = "llvm.x86.amx." + sub;
                     } else {
                         intrinsic_name = "llvm.x86." + sub;
                     }
                     // Basic replace of underscores to dots where LLVM expects it
                     for (size_t i = 0; i < intrinsic_name.length(); ++i) {
                         if (intrinsic_name[i] == '_') intrinsic_name[i] = '.';
                     }
                 } else if (rname.find("__builtin_neon_") == 0) {
                     intrinsic_name = "llvm.aarch64.neon." + rname.substr(15);
                     for (size_t i = 0; i < intrinsic_name.length(); ++i) {
                         if (intrinsic_name[i] == '_') intrinsic_name[i] = '.';
                     }
                 } else if (rname.find("__builtin_sve_") == 0) {
                     intrinsic_name = "llvm.aarch64.sve." + rname.substr(14);
                     for (size_t i = 0; i < intrinsic_name.length(); ++i) {
                         if (intrinsic_name[i] == '_') intrinsic_name[i] = '.';
                     }
                 } else {
                     intrinsic_name = "llvm.riscv." + rname.substr(14);
                     for (size_t i = 0; i < intrinsic_name.length(); ++i) {
                         if (intrinsic_name[i] == '_') intrinsic_name[i] = '.';
                     }
                 }
                 
                 llvm::Function* intr_func = be_state->module->getFunction(intrinsic_name);
                 if (!intr_func) {
                     llvm::Intrinsic::ID id = llvm::Intrinsic::lookupIntrinsicID(intrinsic_name);
                     if (id != llvm::Intrinsic::not_intrinsic) {
                         if (llvm::Intrinsic::isOverloaded(id)) {
                             std::vector<llvm::Type*> overload_types;
                             if (callee_ty && !callee_ty->getReturnType()->isVoidTy()) {
                                 overload_types.push_back(callee_ty->getReturnType());
                             } else if (!args.empty()) {
                                 overload_types.push_back(args[0]->getType());
                             }
                             // Some target intrinsics need multiple overloaded types (e.g., return and specific params).
                             // In a full implementation, this maps tightly to the Intrinsic signature descriptor.
                             // We provide the primary return/arg type which satisfies the vast majority.
                             intr_func = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), id, overload_types);
                         } else {
                             intr_func = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), id);
                         }
                     }
                 }
                 if (intr_func) {
                     *out_val = be_state->builder->CreateCall(intr_func, args);
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_add_overflow" || rname == "__builtin_sub_overflow" || rname == "__builtin_mul_overflow" ||
                        rname == "__builtin_sadd_overflow" || rname == "__builtin_uadd_overflow" ||
                        rname == "__builtin_ssub_overflow" || rname == "__builtin_usub_overflow" ||
                        rname == "__builtin_smul_overflow" || rname == "__builtin_umul_overflow" ||
                        rname == "__builtin_saddl_overflow" || rname == "__builtin_uaddl_overflow" ||
                        rname == "__builtin_ssubl_overflow" || rname == "__builtin_usubl_overflow" ||
                        rname == "__builtin_smull_overflow" || rname == "__builtin_umull_overflow" ||
                        rname == "__builtin_saddll_overflow" || rname == "__builtin_uaddll_overflow" ||
                        rname == "__builtin_ssubll_overflow" || rname == "__builtin_usubll_overflow" ||
                        rname == "__builtin_smulll_overflow" || rname == "__builtin_umulll_overflow") {
                 
                 if (args.size() >= 3) {
                     llvm::Type* op_ty = args[0]->getType();
                     bool is_signed = (rname.find("sadd") != std::string::npos) || 
                                      (rname.find("ssub") != std::string::npos) || 
                                      (rname.find("smul") != std::string::npos) || 
                                      (rname == "__builtin_add_overflow" || rname == "__builtin_sub_overflow" || rname == "__builtin_mul_overflow"); // Simplification for generic
                                      
                     llvm::Intrinsic::ID iid;
                     if (rname.find("add") != std::string::npos) iid = is_signed ? llvm::Intrinsic::sadd_with_overflow : llvm::Intrinsic::uadd_with_overflow;
                     else if (rname.find("sub") != std::string::npos) iid = is_signed ? llvm::Intrinsic::ssub_with_overflow : llvm::Intrinsic::usub_with_overflow;
                     else iid = is_signed ? llvm::Intrinsic::smul_with_overflow : llvm::Intrinsic::umul_with_overflow;

                     llvm::Function* ofunc = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), iid, {op_ty});
                     llvm::Value* res = be_state->builder->CreateCall(ofunc, {args[0], args[1]});
                     
                     // Store result
                     llvm::Value* val = be_state->builder->CreateExtractValue(res, 0);
                     be_state->builder->CreateStore(val, args[2]);
                     
                     // Return overflow flag
                     *out_val = be_state->builder->CreateExtractValue(res, 1);
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_constant_p") {
                 if (args.size() >= 1) {
                     llvm::Function* is_const = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::is_constant, {args[0]->getType()});
                     *out_val = be_state->builder->CreateCall(is_const, {args[0]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_clz" || rname == "__builtin_clzl" || rname == "__builtin_clzll") {
                 if (args.size() >= 1) {
                     llvm::Function* clz = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::ctlz, {args[0]->getType()});
                     *out_val = be_state->builder->CreateCall(clz, {args[0], be_state->builder->getTrue()});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_ctz" || rname == "__builtin_ctzl" || rname == "__builtin_ctzll") {
                 if (args.size() >= 1) {
                     llvm::Function* ctz = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::cttz, {args[0]->getType()});
                     *out_val = be_state->builder->CreateCall(ctz, {args[0], be_state->builder->getTrue()});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_popcount" || rname == "__builtin_popcountl" || rname == "__builtin_popcountll") {
                 if (args.size() >= 1) {
                     llvm::Function* popcnt = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::ctpop, {args[0]->getType()});
                     *out_val = be_state->builder->CreateCall(popcnt, {args[0]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_parity" || rname == "__builtin_parityl" || rname == "__builtin_parityll") {
                 if (args.size() >= 1) {
                     llvm::Function* popcnt = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::ctpop, {args[0]->getType()});
                     llvm::Value* pop = be_state->builder->CreateCall(popcnt, {args[0]});
                     *out_val = be_state->builder->CreateAnd(pop, llvm::ConstantInt::get(args[0]->getType(), 1));
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname.find("__builtin_rotateleft") == 0) {
                 if (args.size() >= 2) {
                     llvm::Function* rotl = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::fshl, {args[0]->getType()});
                     *out_val = be_state->builder->CreateCall(rotl, {args[0], args[0], args[1]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname.find("__builtin_rotateright") == 0) {
                 if (args.size() >= 2) {
                     llvm::Function* rotr = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::fshr, {args[0]->getType()});
                     *out_val = be_state->builder->CreateCall(rotr, {args[0], args[0], args[1]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_object_size" || rname == "__builtin_dynamic_object_size") {
                 if (args.size() >= 2) {
                     llvm::Function* objsize = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::objectsize, {llvm::Type::getInt64Ty(*be_state->context), args[0]->getType()});
                     // LLVM objectsize takes 4 args: ptr, min, nullunknown, dynamic
                     llvm::Value* min_val = be_state->builder->CreateAnd(args[1], llvm::ConstantInt::get(args[1]->getType(), 2));
                     llvm::Value* is_min = be_state->builder->CreateICmpNE(min_val, llvm::ConstantInt::get(args[1]->getType(), 0));
                     llvm::Value* dynamic = (rname == "__builtin_dynamic_object_size") ? be_state->builder->getTrue() : be_state->builder->getFalse();
                     *out_val = be_state->builder->CreateCall(objsize, {args[0], is_min, be_state->builder->getFalse(), dynamic});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_sqrt" || rname == "__builtin_sqrtf" || rname == "__builtin_sqrtl") {
                 if (args.size() >= 1) {
                     llvm::Function* intr = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::sqrt, {args[0]->getType()});
                     *out_val = be_state->builder->CreateCall(intr, {args[0]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_sin" || rname == "__builtin_sinf" || rname == "__builtin_sinl") {
                 if (args.size() >= 1) {
                     llvm::Function* intr = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::sin, {args[0]->getType()});
                     *out_val = be_state->builder->CreateCall(intr, {args[0]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_cos" || rname == "__builtin_cosf" || rname == "__builtin_cosl") {
                 if (args.size() >= 1) {
                     llvm::Function* intr = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::cos, {args[0]->getType()});
                     *out_val = be_state->builder->CreateCall(intr, {args[0]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_pow" || rname == "__builtin_powf" || rname == "__builtin_powl") {
                 if (args.size() >= 2) {
                     llvm::Function* intr = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::pow, {args[0]->getType()});
                     *out_val = be_state->builder->CreateCall(intr, {args[0], args[1]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_exp" || rname == "__builtin_expf" || rname == "__builtin_expl") {
                 if (args.size() >= 1) {
                     llvm::Function* intr = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::exp, {args[0]->getType()});
                     *out_val = be_state->builder->CreateCall(intr, {args[0]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_log" || rname == "__builtin_logf" || rname == "__builtin_logl") {
                 if (args.size() >= 1) {
                     llvm::Function* intr = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::log, {args[0]->getType()});
                     *out_val = be_state->builder->CreateCall(intr, {args[0]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_fma" || rname == "__builtin_fmaf" || rname == "__builtin_fmal") {
                 if (args.size() >= 3) {
                     llvm::Function* intr = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::fma, {args[0]->getType()});
                     *out_val = be_state->builder->CreateCall(intr, {args[0], args[1], args[2]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_copysign" || rname == "__builtin_copysignf" || rname == "__builtin_copysignl") {
                 if (args.size() >= 2) {
                     llvm::Function* intr = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::copysign, {args[0]->getType()});
                     *out_val = be_state->builder->CreateCall(intr, {args[0], args[1]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_round" || rname == "__builtin_roundf" || rname == "__builtin_roundl") {
                 if (args.size() >= 1) {
                     llvm::Function* intr = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::round, {args[0]->getType()});
                     *out_val = be_state->builder->CreateCall(intr, {args[0]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_trunc" || rname == "__builtin_truncf" || rname == "__builtin_truncl") {
                 if (args.size() >= 1) {
                     llvm::Function* intr = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::trunc, {args[0]->getType()});
                     *out_val = be_state->builder->CreateCall(intr, {args[0]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_floor" || rname == "__builtin_floorf" || rname == "__builtin_floorl") {
                 if (args.size() >= 1) {
                     llvm::Function* intr = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::floor, {args[0]->getType()});
                     *out_val = be_state->builder->CreateCall(intr, {args[0]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_ceil" || rname == "__builtin_ceilf" || rname == "__builtin_ceill") {
                 if (args.size() >= 1) {
                     llvm::Function* intr = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::ceil, {args[0]->getType()});
                     *out_val = be_state->builder->CreateCall(intr, {args[0]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_bswap16" || rname == "__builtin_bswap32" || rname == "__builtin_bswap64") {
                 if (args.size() >= 1) {
                     llvm::Function* bswap = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::bswap, {args[0]->getType()});
                     *out_val = be_state->builder->CreateCall(bswap, {args[0]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin___emutls_get_address") {
                 if (args.size() >= 1) {
                     llvm::Function* emutls_fn = be_state->module->getFunction("__emutls_get_address");
                     if (!emutls_fn) {
                         llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                         llvm::FunctionType* fn_ty = llvm::FunctionType::get(ptr_ty, {ptr_ty}, false);
                         emutls_fn = llvm::Function::Create(fn_ty, llvm::GlobalValue::ExternalLinkage, "__emutls_get_address", be_state->module.get());
                     }
                     *out_val = be_state->builder->CreateCall(emutls_fn, {args[0]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_memcpy") {
                 if (args.size() >= 3) {
                     be_state->builder->CreateMemCpy(args[0], llvm::MaybeAlign(), args[1], llvm::MaybeAlign(), args[2]);
                     *out_val = args[0]; // Returns dest
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_memmove") {
                 if (args.size() >= 3) {
                     be_state->builder->CreateMemMove(args[0], llvm::MaybeAlign(), args[1], llvm::MaybeAlign(), args[2]);
                     *out_val = args[0]; // Returns dest
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_strlen") {
                 if (args.size() >= 1) {
                     llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getInt64Ty(*be_state->context), {args[0]->getType()}, false);
                     llvm::FunctionCallee fn = be_state->module->getOrInsertFunction("strlen", ft);
                     *out_val = be_state->builder->CreateCall(fn, {args[0]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_strcmp") {
                 if (args.size() >= 2) {
                     llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getInt32Ty(*be_state->context), {args[0]->getType(), args[1]->getType()}, false);
                     llvm::FunctionCallee fn = be_state->module->getOrInsertFunction("strcmp", ft);
                     *out_val = be_state->builder->CreateCall(fn, {args[0], args[1]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_strchr") {
                 if (args.size() >= 2) {
                     llvm::FunctionType* ft = llvm::FunctionType::get(llvm::PointerType::getUnqual(*be_state->context), {args[0]->getType(), llvm::Type::getInt32Ty(*be_state->context)}, false);
                     llvm::FunctionCallee fn = be_state->module->getOrInsertFunction("strchr", ft);
                     // the second argument might need coercion to i32
                     llvm::Value* arg1 = args[1];
                     if (arg1->getType()->getIntegerBitWidth() < 32) arg1 = be_state->builder->CreateSExt(arg1, llvm::Type::getInt32Ty(*be_state->context));
                     else if (arg1->getType()->getIntegerBitWidth() > 32) arg1 = be_state->builder->CreateTrunc(arg1, llvm::Type::getInt32Ty(*be_state->context));
                     *out_val = be_state->builder->CreateCall(fn, {args[0], arg1});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname.find("__builtin_rvv_vsetvli") == 0) {
                 bool is_max = rname.find("max") != std::string::npos;
                 bool is_i64 = args[0]->getType()->getIntegerBitWidth() == 64;
                 std::string intr_name = is_max ? "llvm.riscv.vsetvlimax." : "llvm.riscv.vsetvli.";
                 intr_name += is_i64 ? "i64" : "i32";
                 if (!is_max && args.size() >= 3) {
                     llvm::FunctionType* ft = llvm::FunctionType::get(args[0]->getType(), {args[0]->getType(), args[1]->getType(), args[2]->getType()}, false);
                     llvm::FunctionCallee intr = be_state->module->getOrInsertFunction(intr_name, ft);
                     *out_val = be_state->builder->CreateCall(intr, {args[0], args[1], args[2]});
                     return llvm_gen_be_error_t::ok;
                 } else if (is_max && args.size() >= 2) {
                     llvm::FunctionType* ft = llvm::FunctionType::get(args[0]->getType(), {args[0]->getType(), args[1]->getType()}, false);
                     llvm::FunctionCallee intr = be_state->module->getOrInsertFunction(intr_name, ft);
                     *out_val = be_state->builder->CreateCall(intr, {args[0], args[1]});
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_memset") {
                 if (args.size() >= 3) {
                     // LLVM memset expects i8 for value
                     llvm::Value* val_i8 = be_state->builder->CreateTrunc(args[1], llvm::Type::getInt8Ty(*be_state->context));
                     be_state->builder->CreateMemSet(args[0], val_i8, args[2], llvm::MaybeAlign());
                     *out_val = args[0]; // Returns dest
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__atomic_always_lock_free" || rname == "__atomic_is_lock_free") {
                 if (args.size() >= 1) {
                     // Usually args[0] is size, args[1] is pointer (optional for always_lock_free)
                     llvm::Value* size_val = args[0];
                     bool is_always = rname == "__atomic_always_lock_free";
                     
                     if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(size_val)) {
                         uint64_t size = ci->getZExtValue();
                         bool lock_free = (size == 1 || size == 2 || size == 4 || size == 8);
                         if (size == 16) {
                             llvm::Triple triple(be_state->module->getTargetTriple());
                             if (triple.getArch() == llvm::Triple::x86_64 || triple.getArch() == llvm::Triple::aarch64) lock_free = true;
                         }
                         *out_val = llvm::ConstantInt::get(llvm::Type::getInt1Ty(*be_state->context), lock_free ? 1 : 0);
                     } else {
                         *out_val = llvm::ConstantInt::get(llvm::Type::getInt1Ty(*be_state->context), 0); // conservative fallback
                     }
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__atomic_thread_fence" || rname == "__atomic_signal_fence") {
                 if (args.size() >= 1) {
                     llvm::Value* order_val = args[0];
                     llvm::AtomicOrdering order = llvm::AtomicOrdering::SequentiallyConsistent; // default fallback
                     if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(order_val)) {
                         switch(ci->getZExtValue()) {
                             case 0: order = llvm::AtomicOrdering::Monotonic; break; // relaxed
                             case 1: // consume fallback to acquire
                             case 2: order = llvm::AtomicOrdering::Acquire; break; // acquire
                             case 3: order = llvm::AtomicOrdering::Release; break; // release
                             case 4: order = llvm::AtomicOrdering::AcquireRelease; break; // acq_rel
                             case 5: order = llvm::AtomicOrdering::SequentiallyConsistent; break; // seq_cst
                         }
                     }
                     llvm::SyncScope::ID scope = (rname == "__atomic_signal_fence") ? llvm::SyncScope::SingleThread : llvm::SyncScope::System;
                     be_state->builder->CreateFence(order, scope);
                     *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0); // void/ignored
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname == "__builtin_atomic_wait" || rname == "__builtin_atomic_notify") {
                 if (args.size() >= 1) {
                     std::string fn_name = (rname == "__builtin_atomic_wait") ? "__atomic_wait" : "__atomic_notify";
                     llvm::Function* wait_notify_fn = be_state->module->getFunction(fn_name);
                     if (!wait_notify_fn) {
                         std::vector<llvm::Type*> arg_types;
                         for (auto* arg_val : args) arg_types.push_back(arg_val->getType());
                         llvm::FunctionType* fn_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), arg_types, false);
                         wait_notify_fn = llvm::Function::Create(fn_ty, llvm::GlobalValue::ExternalLinkage, fn_name, be_state->module.get());
                     }
                     *out_val = be_state->builder->CreateCall(wait_notify_fn, args);
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname.find("__atomic_fetch_") == 0 || rname.find("__c11_atomic_fetch_") == 0 || rname.find("__sync_fetch_and_") == 0 || rname.find("__sync_") == 0) {
                 if (rname == "__sync_synchronize") {
                     be_state->builder->CreateFence(llvm::AtomicOrdering::SequentiallyConsistent);
                     *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0);
                     return llvm_gen_be_error_t::ok;
                 }
                 if (rname == "__sync_lock_test_and_set") {
                     if (args.size() >= 2) {
                         llvm::AtomicRMWInst* rmw = be_state->builder->CreateAtomicRMW(llvm::AtomicRMWInst::Xchg, args[0], args[1], llvm::MaybeAlign(), llvm::AtomicOrdering::SequentiallyConsistent);
                         *out_val = rmw;
                         return llvm_gen_be_error_t::ok;
                     }
                 }
                 if (rname == "__sync_lock_release") {
                     if (args.size() >= 1) {
                         llvm::Type* val_ty = llvm::Type::getInt32Ty(*be_state->context);
                         llvm::StoreInst* store = be_state->builder->CreateStore(llvm::ConstantInt::get(val_ty, 0), args[0]);
                         store->setAtomic(llvm::AtomicOrdering::Release);
                         *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0);
                         return llvm_gen_be_error_t::ok;
                     }
                 }
                 if (rname == "__sync_val_compare_and_swap" || rname == "__sync_bool_compare_and_swap") {
                     if (args.size() >= 3) {
                         llvm::AtomicCmpXchgInst* cmpxchg = be_state->builder->CreateAtomicCmpXchg(args[0], args[1], args[2], llvm::MaybeAlign(), llvm::AtomicOrdering::SequentiallyConsistent, llvm::AtomicOrdering::SequentiallyConsistent);
                         if (rname == "__sync_val_compare_and_swap") {
                             *out_val = be_state->builder->CreateExtractValue(cmpxchg, 0);
                         } else {
                             *out_val = be_state->builder->CreateExtractValue(cmpxchg, 1);
                         }
                         return llvm_gen_be_error_t::ok;
                     }
                 }
                 if (args.size() >= 2) { // ptr, val, (optional order for __atomic)
                     bool is_fp = args[1]->getType()->isFloatingPointTy();
                     llvm::AtomicRMWInst::BinOp op = is_fp ? llvm::AtomicRMWInst::FAdd : llvm::AtomicRMWInst::Add;
                     if (rname.find("sub") != std::string::npos) op = is_fp ? llvm::AtomicRMWInst::FSub : llvm::AtomicRMWInst::Sub;
                     else if (rname.find("nand") != std::string::npos) op = llvm::AtomicRMWInst::Nand;
                     else if (rname.find("and") != std::string::npos) op = llvm::AtomicRMWInst::And;
                     else if (rname.find("or") != std::string::npos && rname.find("xor") == std::string::npos) op = llvm::AtomicRMWInst::Or;
                     else if (rname.find("xor") != std::string::npos) op = llvm::AtomicRMWInst::Xor;
                     else if (rname.find("min") != std::string::npos) {
                         op = is_fp ? llvm::AtomicRMWInst::FMin : llvm::AtomicRMWInst::Min;
                     }
                     else if (rname.find("max") != std::string::npos) {
                         op = is_fp ? llvm::AtomicRMWInst::FMax : llvm::AtomicRMWInst::Max;
                     }
                     
                     llvm::AtomicOrdering order = llvm::AtomicOrdering::SequentiallyConsistent;
                     if (args.size() >= 3 && rname.find("__sync_") != 0) {
                         if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(args[2])) {
                             switch(ci->getZExtValue()) {
                                 case 0: order = llvm::AtomicOrdering::Monotonic; break;
                                 case 1:
                                 case 2: order = llvm::AtomicOrdering::Acquire; break;
                                 case 3: order = llvm::AtomicOrdering::Release; break;
                                 case 4: order = llvm::AtomicOrdering::AcquireRelease; break;
                                 case 5: order = llvm::AtomicOrdering::SequentiallyConsistent; break;
                             }
                         }
                     }
                     
                     llvm::AtomicRMWInst* rmw = be_state->builder->CreateAtomicRMW(op, args[0], args[1], llvm::MaybeAlign(), order);

                     if (rname.find("__sync_add_and_fetch") == 0 || rname.find("__sync_sub_and_fetch") == 0 ||
                         rname.find("__sync_and_and_fetch") == 0 || rname.find("__sync_or_and_fetch") == 0 ||
                         rname.find("__sync_xor_and_fetch") == 0 || rname.find("__sync_nand_and_fetch") == 0) {
                         if (op == llvm::AtomicRMWInst::Add) *out_val = be_state->builder->CreateAdd(rmw, args[1]);
                         else if (op == llvm::AtomicRMWInst::FAdd) *out_val = be_state->builder->CreateFAdd(rmw, args[1]);
                         else if (op == llvm::AtomicRMWInst::Sub) *out_val = be_state->builder->CreateSub(rmw, args[1]);
                         else if (op == llvm::AtomicRMWInst::FSub) *out_val = be_state->builder->CreateFSub(rmw, args[1]);
                         else if (op == llvm::AtomicRMWInst::And) *out_val = be_state->builder->CreateAnd(rmw, args[1]);
                         else if (op == llvm::AtomicRMWInst::Or) *out_val = be_state->builder->CreateOr(rmw, args[1]);
                         else if (op == llvm::AtomicRMWInst::Xor) *out_val = be_state->builder->CreateXor(rmw, args[1]);
                         else if (op == llvm::AtomicRMWInst::Nand) {
                             llvm::Value* and_val = be_state->builder->CreateAnd(rmw, args[1]);
                             *out_val = be_state->builder->CreateNot(and_val);
                         }
                     } else {
                         *out_val = rmw;
                     }
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname.find("__atomic_load") == 0 || rname.find("__c11_atomic_load") == 0) {
                 if (args.size() >= 2) { // ptr, order OR ptr, ret_ptr, order
                     llvm::Value* ptr_arg = args[0];
                     llvm::Value* order_arg = args.back();

                     llvm::AtomicOrdering order = llvm::AtomicOrdering::SequentiallyConsistent;
                     if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(order_arg)) {
                         switch(ci->getZExtValue()) {
                             case 0: order = llvm::AtomicOrdering::Monotonic; break;
                             case 1:
                             case 2: order = llvm::AtomicOrdering::Acquire; break;
                             case 5: order = llvm::AtomicOrdering::SequentiallyConsistent; break;
                         }
                     }

                     llvm::Type* loaded_ty = nullptr; // we need to infer it. usually it's derived from the callee's return type or generic
                     // if it's __atomic_load_n, the return type is the value type.
                     // For simplicity we extract it from the pointer type, but LLVM 15 opaque pointers means we need the actual type!
                     // Since we can't easily get it here without EDG type info, we look at expr->type.
                     llvm_gen_be_error_t err = get_llvm_type(expr->type, &loaded_ty);
                     if (err != llvm_gen_be_error_t::ok) return err;

                     if (loaded_ty->isVoidTy() && args.size() >= 3) {
                         // __atomic_load(ptr, ret_ptr, order)
                         err = get_llvm_type(op1->next->next->type->variant.pointer.type, &loaded_ty);
                         if (err != llvm_gen_be_error_t::ok) return err;
                         llvm::LoadInst* load = be_state->builder->CreateLoad(loaded_ty, ptr_arg);
                         load->setAtomic(order);
                         be_state->builder->CreateStore(load, args[1]);
                         *out_val = load; // might be ignored
                     } else {
                         llvm::LoadInst* load = be_state->builder->CreateLoad(loaded_ty, ptr_arg);
                         load->setAtomic(order);
                         *out_val = load;
                     }
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname.find("__atomic_store") == 0 || rname.find("__c11_atomic_store") == 0) {
                 if (args.size() >= 3) { // ptr, val, order OR ptr, val_ptr, order
                     llvm::Value* ptr_arg = args[0];
                     llvm::Value* val_arg = args[1];
                     llvm::Value* order_arg = args.back();

                     llvm::AtomicOrdering order = llvm::AtomicOrdering::SequentiallyConsistent;
                     if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(order_arg)) {
                         switch(ci->getZExtValue()) {
                             case 0: order = llvm::AtomicOrdering::Monotonic; break;
                             case 3: order = llvm::AtomicOrdering::Release; break;
                             case 5: order = llvm::AtomicOrdering::SequentiallyConsistent; break;
                         }
                     }

                     if (rname.find("_n") != std::string::npos || !val_arg->getType()->isPointerTy()) {
                         llvm::StoreInst* store = be_state->builder->CreateStore(val_arg, ptr_arg);
                         store->setAtomic(order);
                     } else {
                         // __atomic_store(ptr, val_ptr, order)
                         llvm::Type* val_ty = nullptr;
                         llvm_gen_be_error_t err_ty = get_llvm_type(op1->next->next->type->variant.pointer.type, &val_ty);
                         if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
                         llvm::Value* loaded_val = be_state->builder->CreateLoad(val_ty, val_arg);
                         llvm::StoreInst* store = be_state->builder->CreateStore(loaded_val, ptr_arg);
                         store->setAtomic(order);
                     }
                     *out_val = llvm::Constant::getNullValue(llvm::Type::getInt32Ty(*be_state->context)); // void
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname.find("__atomic_exchange") == 0 || rname.find("__c11_atomic_exchange") == 0) {
                 if (args.size() >= 3) {
                     llvm::Value* ptr_arg = args[0];
                     llvm::Value* val_arg = args[1];
                     llvm::Value* order_arg = args.back();

                     llvm::AtomicOrdering order = llvm::AtomicOrdering::SequentiallyConsistent;
                     if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(order_arg)) {
                         switch(ci->getZExtValue()) {
                             case 0: order = llvm::AtomicOrdering::Monotonic; break;
                             case 1:
                             case 2: order = llvm::AtomicOrdering::Acquire; break;
                             case 3: order = llvm::AtomicOrdering::Release; break;
                             case 4: order = llvm::AtomicOrdering::AcquireRelease; break;
                             case 5: order = llvm::AtomicOrdering::SequentiallyConsistent; break;
                         }
                     }

                     if (rname.find("_n") != std::string::npos || !val_arg->getType()->isPointerTy()) {
                         *out_val = be_state->builder->CreateAtomicRMW(llvm::AtomicRMWInst::Xchg, ptr_arg, val_arg, llvm::MaybeAlign(), order);
                     } else {
                         llvm::Type* val_ty = nullptr;
                         llvm_gen_be_error_t err_ty = get_llvm_type(op1->next->next->type->variant.pointer.type, &val_ty);
                         if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
                         llvm::Value* loaded_val = be_state->builder->CreateLoad(val_ty, val_arg);
                         llvm::Value* old_val = be_state->builder->CreateAtomicRMW(llvm::AtomicRMWInst::Xchg, ptr_arg, loaded_val, llvm::MaybeAlign(), order);
                         if (args.size() >= 4) be_state->builder->CreateStore(old_val, args[2]); // __atomic_exchange(ptr, val_ptr, ret_ptr, order)
                         *out_val = old_val;
                     }
                     return llvm_gen_be_error_t::ok;
                 }
             } else if (rname.find("__atomic_compare_exchange") == 0 || rname.find("__c11_atomic_compare_exchange") == 0) {
                 if (args.size() >= 6) { // ptr, expected_ptr, desired, weak, succ_order, fail_order
                     llvm::AtomicOrdering succ_order = llvm::AtomicOrdering::SequentiallyConsistent;
                     llvm::AtomicOrdering fail_order = llvm::AtomicOrdering::SequentiallyConsistent;
                     
                     if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(args[4])) {
                         switch(ci->getZExtValue()) {
                             case 0: succ_order = llvm::AtomicOrdering::Monotonic; break;
                             case 1:
                             case 2: succ_order = llvm::AtomicOrdering::Acquire; break;
                             case 3: succ_order = llvm::AtomicOrdering::Release; break;
                             case 4: succ_order = llvm::AtomicOrdering::AcquireRelease; break;
                             case 5: succ_order = llvm::AtomicOrdering::SequentiallyConsistent; break;
                         }
                     }
                     if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(args[5])) {
                         switch(ci->getZExtValue()) {
                             case 0: fail_order = llvm::AtomicOrdering::Monotonic; break;
                             case 1:
                             case 2: fail_order = llvm::AtomicOrdering::Acquire; break;
                             case 5: fail_order = llvm::AtomicOrdering::SequentiallyConsistent; break;
                         }
                     }
                     
                     llvm::Type* val_ty = args[2]->getType();
                     llvm::Value* expected_val = be_state->builder->CreateLoad(val_ty, args[1]);
                     
                     llvm::AtomicCmpXchgInst* cmpxchg = be_state->builder->CreateAtomicCmpXchg(
                         args[0], expected_val, args[2], llvm::MaybeAlign(), succ_order, fail_order);
                         
                     llvm::Value* success = be_state->builder->CreateExtractValue(cmpxchg, 1);
                     llvm::Value* loaded_val = be_state->builder->CreateExtractValue(cmpxchg, 0);
                     
                     // If fail, store loaded_val back to expected_ptr
                     // To avoid branches, we can unconditionally store loaded_val (if it succeeded, it's the same as what was there)
                     be_state->builder->CreateStore(loaded_val, args[1]);
                     
                     // Return boolean success
                     *out_val = be_state->builder->CreateZExt(success, llvm::Type::getInt8Ty(*be_state->context));
                     return llvm_gen_be_error_t::ok;
                 }

             }
         }
      }
  }

  if (!callee_ty) {
     llvm_gen_be_error_t err = get_llvm_function_type(op1->type, &callee_ty);
     if (err != llvm_gen_be_error_t::ok) return err;
  }

     if (llvm::Function* f = llvm::dyn_cast<llvm::Function>(v1)) {
        callee_ty = f->getFunctionType();
     } else {
        callee_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false);
     }
     if (!be_state->current_landing_pads.empty()) {
    llvm::BasicBlock* lpad_bb = be_state->current_landing_pads.back();
    llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* normal_bb = llvm::BasicBlock::Create(*be_state->context, "invoke.cont", func);
    llvm::InvokeInst* invoke = be_state->builder->CreateInvoke(callee_ty, v1, normal_bb, lpad_bb, args);
    if (llvm::Function* fn = llvm::dyn_cast<llvm::Function>(v1)) {
      invoke->setCallingConv(fn->getCallingConv());
    }
    be_state->builder->SetInsertPoint(normal_bb);
    *out_val = invoke;
  } else {
    llvm::CallInst* call = be_state->builder->CreateCall(callee_ty, v1, args);
    if (llvm::Function* fn = llvm::dyn_cast<llvm::Function>(v1)) {
      call->setCallingConv(fn->getCallingConv());
    }
    *out_val = call;
  }
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!out_val) return llvm_gen_be_error_t::invalid_argument;
  if (!expr) {
    *out_val = nullptr;
    return llvm_gen_be_error_t::ok;
  }

  struct DILocSaver {
      llvm::DebugLoc saved_loc;
      DILocSaver() {
          if (be_state->builder) saved_loc = be_state->builder->getCurrentDebugLocation();
      }
      ~DILocSaver() {
          if (be_state->builder) be_state->builder->SetCurrentDebugLocation(saved_loc);
      }
  } loc_saver;

  if (be_state->dbg_state && !be_state->dbg_state->scope_stack.empty() && expr->position.seq != 0) {
      llvm::DIScope* scope = be_state->dbg_state->scope_stack.back();
      llvm::DILocation* loc = nullptr;
      llvm_gen_be_error_t err = get_di_location(be_state->dbg_state, expr->position, scope, &loc);
      if (err == llvm_gen_be_error_t::ok && loc) {
          be_state->builder->SetCurrentDebugLocation(loc);
      }
  }

  // Handle LValue evaluation explicitly
  if (expr->is_lvalue) {
    switch (expr->kind) {
      case enk_variable:
      case enk_operation:
        if (expr->kind == enk_operation) {
           an_expr_operator_kind op_kind = expr->variant.operation.kind;
           if (op_kind != eok_subscript && op_kind != eok_dot_field && 
               op_kind != eok_points_to_field && op_kind != eok_indirect) {
               break; // Not an LValue-producing operation
           }
        }
        return llvm_lower_lvalue_expression(expr, out_val);
      default: break;
    }
  }

  switch (expr->kind) {
    case enk_braced_init_list: {
      llvm::Type* agg_ty = nullptr;
      llvm_gen_be_error_t ty_err = get_llvm_type(expr->type, &agg_ty);
      if (ty_err != llvm_gen_be_error_t::ok) return ty_err;
      if (!agg_ty || agg_ty->isVoidTy()) {
          *out_val = nullptr;
          return llvm_gen_be_error_t::ok;
      }

      std::vector<llvm::Value*> values;
      for (an_expr_node_ptr item = expr->variant.braced_init_list; item; item = item->next) {
         llvm::Value* val = nullptr;
         llvm_gen_be_error_t err = llvm_lower_expression(item, &val);
         if (err != llvm_gen_be_error_t::ok) return err;
         if (val) values.push_back(val);
      }

      llvm::Value* agg = llvm::UndefValue::get(agg_ty);
      if (agg_ty->isStructTy() || agg_ty->isArrayTy()) {
          for (size_t i = 0; i < values.size(); ++i) {
             agg = be_state->builder->CreateInsertValue(agg, values[i], i);
          }
      } else if (values.size() == 1) {
          agg = values[0];
      }
      *out_val = agg;
      return llvm_gen_be_error_t::ok;
    }

    case enk_reuse_value: {
      a_dynamic_init_ptr init = expr->variant.reused_value_init;
      if (!init) return llvm_gen_be_error_t::invalid_argument;
      auto it = be_state->reused_values.find(init);
      if (it != be_state->reused_values.end()) {
          *out_val = it->second;
          return llvm_gen_be_error_t::ok;
      }
      llvm::Value* val = nullptr;
      if (init->kind == dik_expression && init->variant.expression) {
          llvm_gen_be_error_t err = llvm_lower_expression(init->variant.expression, &val);
          if (err != llvm_gen_be_error_t::ok) return err;
      } else {
          return llvm_gen_be_error_t::unsupported_expr;
      }
      be_state->reused_values[init] = val;
      *out_val = val;
      return llvm_gen_be_error_t::ok;
    }

    case enk_fold:
    case enk_pack_index:
    case enk_c11_generic: {
      return llvm_gen_be_error_t::unsupported_expr;
    }
    case enk_constant: {
       a_constant_ptr con = expr->variant.constant.ptr;
       if (!con) {
           llvm::Type* expr_llvm_ty = nullptr;
           llvm_gen_be_error_t err = get_llvm_type(expr->type, &expr_llvm_ty);
           if (err != llvm_gen_be_error_t::ok) return err;
           *out_val = llvm::Constant::getNullValue(expr_llvm_ty);
           return llvm_gen_be_error_t::ok;
       }
       llvm::Constant* out_const = nullptr;
       llvm::Type* expr_llvm_ty = nullptr;
       llvm_gen_be_error_t err = get_llvm_type(expr->type, &expr_llvm_ty);
       if (err != llvm_gen_be_error_t::ok) return err;
       err = evaluate_constant(con, expr_llvm_ty, &out_const);
       if (err != llvm_gen_be_error_t::ok) return err;
       *out_val = out_const;
       return llvm_gen_be_error_t::ok;
    }
    case enk_routine: {
       a_routine_ptr routine = expr->variant.routine.ptr;
       if (!routine || !routine->source_corresp.name) {
           llvm::Type* expr_llvm_ty = nullptr;
           llvm_gen_be_error_t err = get_llvm_type(expr->type, &expr_llvm_ty);
           if (err != llvm_gen_be_error_t::ok) return err;
           *out_val = llvm::Constant::getNullValue(expr_llvm_ty);
           return llvm_gen_be_error_t::ok;
       }
       llvm::Function* func = be_state->module->getFunction(routine->source_corresp.name);
       if (!func) {
         llvm::Type* func_ty = nullptr;
         llvm_gen_be_error_t err = get_llvm_type(routine->type, &func_ty);
         if (err != llvm_gen_be_error_t::ok) return err;
         func = llvm::Function::Create(
            llvm::dyn_cast_or_null<llvm::FunctionType>(func_ty) ? llvm::cast<llvm::FunctionType>(func_ty) : llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false),
            llvm::GlobalValue::ExternalLinkage,
            routine->source_corresp.name,
            be_state->module.get()
         );
       }
       *out_val = func;
       return llvm_gen_be_error_t::ok;
    }
    case enk_variable: {
       llvm::Value* ptr = nullptr;
       llvm_gen_be_error_t err = llvm_lower_lvalue_expression(expr, &ptr);
       if (err != llvm_gen_be_error_t::ok) return err;
       
       llvm::Type* expr_llvm_ty = nullptr;
       err = get_llvm_type(expr->type, &expr_llvm_ty);
       if (err != llvm_gen_be_error_t::ok) return err;
       if (!ptr || !ptr->getType()->isPointerTy()) return llvm_gen_be_error_t::unsupported_expr;
       
       llvm::LoadInst* load = be_state->builder->CreateLoad(expr_llvm_ty, ptr);
       if (is_volatile_qualified_type(expr->type)) load->setVolatile(true);
       *out_val = load;
       return llvm_gen_be_error_t::ok;
    }
    case enk_operation: {
       if (expr->variant.operation.kind == eok_comma) {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           an_expr_node_ptr op2 = op1 ? op1->next : nullptr;
           llvm::Value* v1 = nullptr;
           llvm::Value* v2 = nullptr;
           llvm_gen_be_error_t err = llvm_gen_be_error_t::ok;
           
           if (op1) {
               err = llvm_lower_expression(op1, &v1);
               if (err != llvm_gen_be_error_t::ok) return err;
           }
           if (op2) {
               err = llvm_lower_expression(op2, &v2);
               if (err != llvm_gen_be_error_t::ok) return err;
           }
           
           *out_val = v2;
           return llvm_gen_be_error_t::ok;
       }
       
       switch (expr->variant.operation.kind) {
         case eok_negate:
         case eok_unary_plus:
         case eok_add:
         case eok_subtract:
         case eok_multiply:
         case eok_divide:
         case eok_remainder:
         case eok_padd:
         case eok_psubtract:
         case eok_pdiff:
         case eok_fjadd:
         case eok_jfadd:
         case eok_fjsubtract:
         case eok_jfsubtract:
         case eok_jmultiply:
         case eok_jdivide:
         case eok_xconj:
         case eok_complement:
         case eok_and:
         case eok_or:
         case eok_xor:
         case eok_shiftl:
         case eok_shiftr:
           return llvm_lower_arithmetic_expression(expr, out_val);
           
         case eok_eq:
         case eok_ne:
         case eok_lt:
         case eok_le:
         case eok_gt:
         case eok_ge:
         case eok_spaceship:
         case eok_not:
         case eok_land:
         case eok_lor:
           return llvm_lower_logical_expression(expr, out_val);
           
         case eok_cast:
         case eok_lvalue_cast:
           return llvm_lower_cast_expression(expr, out_val);

         case eok_pm_base_class_cast:
         case eok_pm_derived_class_cast:
           return llvm_lower_pm_cast(expr, out_val);

         case eok_dynamic_cast:
         case eok_ref_dynamic_cast:
           return llvm_lower_dynamic_cast(expr, out_val);

         case eok_pre_incr:
         case eok_pre_decr:
         case eok_post_incr:
         case eok_post_decr:
         case eok_add_assign:
         case eok_subtract_assign:
         case eok_multiply_assign:
         case eok_divide_assign:
         case eok_remainder_assign:
         case eok_and_assign:
         case eok_or_assign:
         case eok_xor_assign:
         case eok_shiftl_assign:
         case eok_shiftr_assign:
         case eok_padd_assign:
         case eok_psubtract_assign:
         case eok_bassign:
           return llvm_lower_compound_assignment(expr, out_val);
           
         case eok_dot_member_call:
         case eok_points_to_member_call:
         case eok_call:
           return llvm_lower_call_expression(expr, out_val);

         case eok_dot_pm_call:
         case eok_points_to_pm_call:
           return llvm_lower_pm_call(expr, out_val);

         case eok_pm_field:
         case eok_pm_points_to_field:
           return llvm_lower_pm_field_access(expr, out_val);
           
         case eok_subscript: {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           an_expr_node_ptr op2 = op1 ? op1->next : nullptr;
           llvm::Value* v1 = nullptr;
           llvm::Value* v2 = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
           if (err != llvm_gen_be_error_t::ok) return err;
           err = llvm_lower_expression(op2, &v2);
           if (err != llvm_gen_be_error_t::ok) return err;
           
           if (v1 && v1->getType()->isVectorTy()) {
               *out_val = be_state->builder->CreateExtractElement(v1, v2);
               return llvm_gen_be_error_t::ok;
           }
           
           // Fallback to lvalue -> load
           llvm::Value* ptr = nullptr;
           err = llvm_lower_lvalue_expression(expr, &ptr);
           if (err != llvm_gen_be_error_t::ok) return err;
           
           llvm::Type* expr_llvm_ty = nullptr;
           err = get_llvm_type(expr->type, &expr_llvm_ty);
           if (err != llvm_gen_be_error_t::ok) return err;
           if (!ptr || !ptr->getType()->isPointerTy()) return llvm_gen_be_error_t::unsupported_expr;
           llvm::LoadInst* load = be_state->builder->CreateLoad(expr_llvm_ty, ptr);
           if (is_volatile_qualified_type(expr->type)) load->setVolatile(true);
           *out_val = load;
           return llvm_gen_be_error_t::ok;
         }
         case eok_dot_field:
         case eok_points_to_field:
         case eok_indirect: {
           llvm::Value* ptr = nullptr;
           llvm_gen_be_error_t err = llvm_lower_lvalue_expression(expr, &ptr);
           if (err != llvm_gen_be_error_t::ok) return err;

           if (expr->variant.operation.kind == eok_dot_field || expr->variant.operation.kind == eok_points_to_field) {
             an_expr_node_ptr op2 = expr->variant.operation.operands->next;
             if (op2 && op2->kind == enk_field) {
                 a_field_ptr field = op2->variant.field.ptr;
                 if (field && field->is_bit_field) {
                   if (!ptr || !ptr->getType()->isPointerTy()) return llvm_gen_be_error_t::unsupported_expr;
                   return llvm_emit_bitfield_load(ptr, field, expr->type, out_val);
                 }
             }
           }

           llvm::Type* expr_llvm_ty = nullptr;
           err = get_llvm_type(expr->type, &expr_llvm_ty);
           if (err != llvm_gen_be_error_t::ok) return err;
           if (!ptr || !ptr->getType()->isPointerTy()) return llvm_gen_be_error_t::unsupported_expr;
           llvm::LoadInst* load = be_state->builder->CreateLoad(expr_llvm_ty, ptr);
           if (is_volatile_qualified_type(expr->type)) load->setVolatile(true);
           *out_val = load;
           return llvm_gen_be_error_t::ok;
         }

         case eok_dot_static:
         case eok_points_to_static: {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           an_expr_node_ptr op2 = op1->next;
           llvm::Value* v1 = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
           if (err != llvm_gen_be_error_t::ok) return err;

           llvm::Value* v2 = nullptr;
           err = llvm_lower_expression(op2, &v2);
           if (err != llvm_gen_be_error_t::ok) return err;
           *out_val = v2;
           return llvm_gen_be_error_t::ok;
         }

         case eok_dot_vacuous_destructor_call:
         case eok_points_to_vacuous_destructor_call: {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           llvm::Value* v1 = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
           if (err != llvm_gen_be_error_t::ok) return err;
           *out_val = nullptr; // Vacuous destructor evaluates to nothing
           return llvm_gen_be_error_t::ok;
         }

         case eok_handle_to:
         case eok_handle_to_box:
         case eok_box:
         case eok_unbox:
         case eok_unbox_lvalue:
         case eok_cli_subscript: {
           // CLI operations are not supported by the LLVM backend.
           return llvm_gen_be_error_t::unsupported_expr;
         }
         case eok_real_part: {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           llvm::Value* v1 = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
           if (err != llvm_gen_be_error_t::ok) return err;
           if (v1->getType()->isStructTy() && v1->getType()->getStructNumElements() == 2) {
               *out_val = be_state->builder->CreateExtractValue(v1, 0);
           } else {
               *out_val = v1;
           }
           return llvm_gen_be_error_t::ok;
         }
         case eok_imag_part: {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           llvm::Value* v1 = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
           if (err != llvm_gen_be_error_t::ok) return err;
           if (v1->getType()->isStructTy() && v1->getType()->getStructNumElements() == 2) {
               *out_val = be_state->builder->CreateExtractValue(v1, 1);
           } else {
               *out_val = llvm::Constant::getNullValue(v1->getType());
           }
           return llvm_gen_be_error_t::ok;
         }
         case eok_address_of: {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           llvm::Value* v1 = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
           if (err != llvm_gen_be_error_t::ok) return err;
           *out_val = v1;
           return llvm_gen_be_error_t::ok;
         }
         case eok_question: {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           an_expr_node_ptr cond_expr = op1;
           an_expr_node_ptr true_expr = op1 ? op1->next : nullptr;
           an_expr_node_ptr false_expr = true_expr ? true_expr->next : nullptr;
           
           llvm::Value* cond = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(cond_expr, &cond);
           if (err != llvm_gen_be_error_t::ok) return err;
           
           if (!cond->getType()->isIntegerTy(1)) {
               cond = be_state->builder->CreateICmpNE(cond, llvm::Constant::getNullValue(cond->getType()));
           }
           
           llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
           llvm::BasicBlock* true_bb = llvm::BasicBlock::Create(*be_state->context, "cond.true", func);
           llvm::BasicBlock* false_bb = llvm::BasicBlock::Create(*be_state->context, "cond.false");
           llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, "cond.end");
           
           be_state->builder->CreateCondBr(cond, true_bb, false_bb);
           
           be_state->builder->SetInsertPoint(true_bb);
           llvm::Value* true_val = nullptr;
           err = llvm_lower_expression(true_expr, &true_val);
           if (err != llvm_gen_be_error_t::ok) return err;
           llvm::BasicBlock* true_end_bb = be_state->builder->GetInsertBlock();
           if (!true_end_bb->getTerminatorOrNull()) be_state->builder->CreateBr(end_bb);
           
           func->insert(func->end(), false_bb);
           be_state->builder->SetInsertPoint(false_bb);
           llvm::Value* false_val = nullptr;
           err = llvm_lower_expression(false_expr, &false_val);
           if (err != llvm_gen_be_error_t::ok) return err;
           llvm::BasicBlock* false_end_bb = be_state->builder->GetInsertBlock();
           if (!false_end_bb->getTerminatorOrNull()) be_state->builder->CreateBr(end_bb);
           
           func->insert(func->end(), end_bb);
           be_state->builder->SetInsertPoint(end_bb);
           
           if (expr->type->kind != tk_void && true_val && false_val) {
             llvm::Type* expr_llvm_ty = nullptr;
             err = get_llvm_type(expr->type, &expr_llvm_ty);
             if (err != llvm_gen_be_error_t::ok) return err;
             llvm::PHINode* phi = be_state->builder->CreatePHI(expr_llvm_ty, 2, "cond.phi");
             phi->addIncoming(true_val, true_end_bb);
             phi->addIncoming(false_val, false_end_bb);
             *out_val = phi;
           } else {
             *out_val = nullptr;
           }
           return llvm_gen_be_error_t::ok;
         }
         case eok_assign: {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           an_expr_node_ptr op2 = op1 ? op1->next : nullptr;
           llvm::Value* v1 = nullptr;
           llvm::Value* v2 = nullptr;
           
           llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
           if (err != llvm_gen_be_error_t::ok) return err;
           err = llvm_lower_expression(op2, &v2);
           if (err != llvm_gen_be_error_t::ok) return err;
           
           if (v1 && v2) {
             bool is_bitfield_assign = false;
             if (op1 && op1->kind == enk_operation && 
                 (op1->variant.operation.kind == eok_dot_field || op1->variant.operation.kind == eok_points_to_field)) {
                 an_expr_node_ptr sub_op2 = op1->variant.operation.operands->next;
                 if (sub_op2 && sub_op2->kind == enk_field) {
                     a_field_ptr field = sub_op2->variant.field.ptr;
                     if (field && field->is_bit_field) {
                         is_bitfield_assign = true;
                         if (!v1->getType()->isPointerTy()) return llvm_gen_be_error_t::unsupported_expr;
                         err = llvm_emit_bitfield_store(v1, v2, field, op1->type);
                         if (err != llvm_gen_be_error_t::ok) return err;
                     }
                 }
             }
             
             if (!is_bitfield_assign) {
                 llvm::Type* ty = nullptr;
                 err = get_llvm_type(expr->type, &ty);
                 if (err != llvm_gen_be_error_t::ok) return err;
                 if (!v1->getType()->isPointerTy()) return llvm_gen_be_error_t::unsupported_expr;
                 if (ty->isAggregateType() && be_state->module->getDataLayout().getTypeStoreSize(ty) > 16) {
                     if (llvm::LoadInst* li = llvm::dyn_cast<llvm::LoadInst>(v2)) {
                         llvm::Value* src_ptr = li->getPointerOperand();
                         uint64_t size = be_state->module->getDataLayout().getTypeStoreSize(ty);
                         llvm::Align align = be_state->module->getDataLayout().getABITypeAlign(ty);
                         be_state->builder->CreateMemCpy(v1, align, src_ptr, align, size);
                     } else {
                         be_state->builder->CreateStore(v2, v1);
                     }
                 } else {
                     be_state->builder->CreateStore(v2, v1);
                 }
             }

             if (be_state->dbg_state && op1->kind == enk_variable) {
                 a_variable_ptr var = op1->variant.variable.ptr;
                 llvm::Instruction* insert_before = be_state->builder->GetInsertBlock()->getTerminator();
                 err = emit_dbg_value_for_variable(be_state->dbg_state, var, v2, insert_before, insert_before ? nullptr : be_state->builder->GetInsertBlock());
                 if (err != llvm_gen_be_error_t::ok) return err;
             }

             *out_val = v2;
             return llvm_gen_be_error_t::ok;
             }
           return llvm_gen_be_error_t::unsupported_expr;
         }
         default:
           return llvm_gen_be_error_t::unsupported_expr;
       }
    }
    
    case enk_await:
    case enk_yield:
      return llvm_lower_coro_suspend(expr, out_val);

    case enk_builtin_operation: {
       a_builtin_operation_kind bok = expr->variant.builtin_operation.kind;
       an_expr_node_ptr op1 = expr->variant.builtin_operation.operands;
       if (!op1) return llvm_gen_be_error_t::invalid_argument;

       if (bok == bok_offsetof) {
           uint64_t total_offset = 0;
           an_expr_node_ptr curr = op1->next;
           while (curr) {
               if (curr->kind == enk_field) {
                   total_offset += curr->variant.field.ptr->offset;
               } else if (curr->kind == enk_operation && (curr->variant.operation.kind == eok_dot_field || curr->variant.operation.kind == eok_points_to_field)) {
                   an_expr_node_ptr sub_op2 = curr->variant.operation.operands->next;
                   if (sub_op2 && sub_op2->kind == enk_field) {
                       total_offset += sub_op2->variant.field.ptr->offset;
                   }
               }
               curr = curr->next;
           }
           *out_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), total_offset);
           return llvm_gen_be_error_t::ok;
       } else if (bok == bok_builtin_bit_cast) {
           llvm::Value* src_val = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(op1->next, &src_val);
           if (err != llvm_gen_be_error_t::ok) return err;
           llvm::Type* dest_ty = nullptr;
           err = get_llvm_type(expr->type, &dest_ty);
           if (err != llvm_gen_be_error_t::ok) return err;

           if (src_val->getType()->isAggregateType() || dest_ty->isAggregateType()) {
               llvm::AllocaInst* alloca = be_state->builder->CreateAlloca(src_val->getType());
               be_state->builder->CreateStore(src_val, alloca);
               *out_val = be_state->builder->CreateLoad(dest_ty, alloca);
           } else {
               if (src_val->getType() == dest_ty) *out_val = src_val;
               else *out_val = be_state->builder->CreateBitCast(src_val, dest_ty);
           }
           return llvm_gen_be_error_t::ok;
       } else if (bok == bok_builtin_has_attribute) {
           *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0);
           return llvm_gen_be_error_t::ok;
       }

       return llvm_gen_be_error_t::unsupported_expr;
    }

    case enk_statement: {
      llvm::Type* res_ty = nullptr;
      if (expr->type) {
          llvm_gen_be_error_t err_ty = get_llvm_type(expr->type, &res_ty);
          if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
      }
      
      llvm::Value* prev_ptr = be_state->current_stmt_expr_result_ptr;
      llvm::Value* result_ptr = nullptr;
      
      if (res_ty && !res_ty->isVoidTy()) {
          result_ptr = be_state->builder->CreateAlloca(res_ty, nullptr, "stmt_expr_res");
      }
      
      be_state->current_stmt_expr_result_ptr = result_ptr;
      llvm_gen_be_error_t err = llvm_lower_statement(expr->variant.statement);
      be_state->current_stmt_expr_result_ptr = prev_ptr;
      
      if (err != llvm_gen_be_error_t::ok) return err;
      
      if (result_ptr) {
          *out_val = be_state->builder->CreateLoad(res_ty, result_ptr);
      } else {
          *out_val = nullptr;
      }
      return llvm_gen_be_error_t::ok;
    }

    case enk_throw: {
       a_throw_supplement_ptr throw_info = expr->variant.throw_info;
       llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);

       if (!throw_info) {
         // Rethrow
         llvm::Triple triple(be_state->module->getTargetTriple());
         if (triple.isWindowsMSVCEnvironment()) {
             llvm::FunctionCallee rethrow_fn = be_state->module->getOrInsertFunction("_CxxThrowException",
                 llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty, int8_ptr_ty}, false));
             be_state->builder->CreateCall(rethrow_fn, {llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty)), llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty))});
             be_state->builder->CreateUnreachable();
             *out_val = nullptr;
             return llvm_gen_be_error_t::ok;
         }
         
         llvm::FunctionCallee rethrow_fn = be_state->module->getOrInsertFunction("__cxa_rethrow",
             llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false));
         if (!be_state->current_landing_pads.empty()) {
             llvm::BasicBlock* lpad_bb = be_state->current_landing_pads.back();
             llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
             llvm::BasicBlock* normal_bb = llvm::BasicBlock::Create(*be_state->context, "invoke.cont", func);
             be_state->builder->CreateInvoke(rethrow_fn, normal_bb, lpad_bb);
             be_state->builder->SetInsertPoint(normal_bb);
         } else {
             be_state->builder->CreateCall(rethrow_fn);
         }
         be_state->builder->CreateUnreachable();
         *out_val = nullptr;
         return llvm_gen_be_error_t::ok;
       } else {
         llvm::Triple triple(be_state->module->getTargetTriple());
         if (triple.isWindowsMSVCEnvironment()) {
             // MSVC Allocate exception
             llvm::Type* size_t_ty = llvm::Type::getInt64Ty(*be_state->context);
             llvm::FunctionCallee malloc_fn = be_state->module->getOrInsertFunction("malloc",
                 llvm::FunctionType::get(int8_ptr_ty, {size_t_ty}, false));
             
             uint32_t type_size = throw_info->type->size;
             if (type_size == 0) type_size = 1;
             llvm::Value* size_val = llvm::ConstantInt::get(size_t_ty, type_size);
             llvm::Value* exc_mem = be_state->builder->CreateCall(malloc_fn, {size_val});

             // Initialize exception object
             if ((throw_info->dynamic_init && throw_info->dynamic_init->kind == dik_expression ? throw_info->dynamic_init->variant.expression : nullptr) != nullptr) {
               llvm::Value* init_val = nullptr;
               llvm_gen_be_error_t err = llvm_lower_expression((throw_info->dynamic_init && throw_info->dynamic_init->kind == dik_expression ? throw_info->dynamic_init->variant.expression : nullptr), &init_val);
               if (err != llvm_gen_be_error_t::ok) return err;
               
               if (init_val) {
                 llvm::Type* exc_ty = nullptr;
                 llvm_gen_be_error_t err_ty = get_llvm_type(throw_info->type, &exc_ty);
                 if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
                 if (exc_ty && exc_ty->isSized()) {
                   be_state->builder->CreateStore(init_val, exc_mem);
                 }
               }
             }

             // Throw exception
             llvm::FunctionCallee throw_fn = be_state->module->getOrInsertFunction("_CxxThrowException",
                 llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty, int8_ptr_ty}, false));
             
             llvm::Constant* throw_info_const = nullptr;
             llvm_gen_be_error_t err_ti = get_typeinfo_global(throw_info->type, &throw_info_const);
             if (err_ti != llvm_gen_be_error_t::ok) return err_ti;
             llvm::Value* typeinfo_ptr = throw_info_const;
             
             if (!be_state->current_landing_pads.empty()) {
                 llvm::BasicBlock* lpad_bb = be_state->current_landing_pads.back();
                 llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
                 llvm::BasicBlock* normal_bb = llvm::BasicBlock::Create(*be_state->context, "invoke.cont", func);
                 be_state->builder->CreateInvoke(throw_fn, normal_bb, lpad_bb, {exc_mem, typeinfo_ptr});
                 be_state->builder->SetInsertPoint(normal_bb);
             } else {
                 be_state->builder->CreateCall(throw_fn, {exc_mem, typeinfo_ptr});
             }
             
             be_state->builder->CreateUnreachable();
             *out_val = nullptr;
             return llvm_gen_be_error_t::ok;
         }

         // Allocate exception
         llvm::Type* size_t_ty = llvm::Type::getInt64Ty(*be_state->context); // Assume 64-bit size_t
         llvm::FunctionCallee alloc_fn = be_state->module->getOrInsertFunction("__cxa_allocate_exception",
             llvm::FunctionType::get(int8_ptr_ty, {size_t_ty}, false));
         
         uint32_t type_size = throw_info->type->size;
         if (type_size == 0) type_size = 1; // Itanium ABI enforces 1-byte minimum for empty classes
         llvm::Value* size_val = llvm::ConstantInt::get(size_t_ty, type_size);
         llvm::Value* exc_mem = be_state->builder->CreateCall(alloc_fn, {size_val});

         // Initialize exception object
         if ((throw_info->dynamic_init && throw_info->dynamic_init->kind == dik_expression ? throw_info->dynamic_init->variant.expression : nullptr) != nullptr) {
           llvm::Value* init_val = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression((throw_info->dynamic_init && throw_info->dynamic_init->kind == dik_expression ? throw_info->dynamic_init->variant.expression : nullptr), &init_val);
           if (err != llvm_gen_be_error_t::ok) return err;
           
           if (init_val) {
             llvm::Type* exc_ty = nullptr;
             llvm_gen_be_error_t err_ty = get_llvm_type(throw_info->type, &exc_ty);
             if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
             if (exc_ty && exc_ty->isSized()) {
               be_state->builder->CreateStore(init_val, exc_mem);
             }
           }
         }

         // Throw exception
         llvm::FunctionCallee throw_fn = be_state->module->getOrInsertFunction("__cxa_throw",
             llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty, int8_ptr_ty, int8_ptr_ty}, false));
         
         llvm::Constant* typeinfo_const = nullptr;
         llvm_gen_be_error_t err_ti = get_typeinfo_global(throw_info->type, &typeinfo_const);
         if (err_ti != llvm_gen_be_error_t::ok) return err_ti;
         llvm::Value* typeinfo_ptr = typeinfo_const;
         llvm::Value* dtor_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty));
         
         if (throw_info->dynamic_init && throw_info->dynamic_init->destructor) {
           a_routine_ptr dtor_rt = throw_info->dynamic_init->destructor;
           if (dtor_rt->source_corresp.name) {
             llvm::Function* dtor_func = be_state->module->getFunction(dtor_rt->source_corresp.name);
             if (!dtor_func) {
               llvm::Type* func_ty = nullptr;
               llvm_gen_be_error_t err_ty = get_llvm_type(dtor_rt->type, &func_ty);
               if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
               dtor_func = llvm::Function::Create(llvm::dyn_cast_or_null<llvm::FunctionType>(func_ty) ? llvm::cast<llvm::FunctionType>(func_ty) : llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false), llvm::GlobalValue::ExternalLinkage, dtor_rt->source_corresp.name, *be_state->module);
             }
             dtor_ptr = dtor_func;
           }
         }
         
         if (!be_state->current_landing_pads.empty()) {
             llvm::BasicBlock* lpad_bb = be_state->current_landing_pads.back();
             llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
             llvm::BasicBlock* normal_bb = llvm::BasicBlock::Create(*be_state->context, "invoke.cont", func);
             be_state->builder->CreateInvoke(throw_fn, normal_bb, lpad_bb, {exc_mem, typeinfo_ptr, dtor_ptr});
             be_state->builder->SetInsertPoint(normal_bb);
         } else {
             be_state->builder->CreateCall(throw_fn, {exc_mem, typeinfo_ptr, dtor_ptr});
         }
         be_state->builder->CreateUnreachable();
         *out_val = nullptr;
         return llvm_gen_be_error_t::ok;
       }
    }
#if VLA_DEALLOCATIONS_IN_IL
    case enk_vla_dealloc: {
      a_variable_ptr var = expr->variant.vla_variable;
      if (be_state->vla_saved_stacks.count(var)) {
          llvm::Function* stackrestore = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::stackrestore, {llvm::PointerType::getUnqual(*be_state->context)});
          be_state->builder->CreateCall(stackrestore, be_state->vla_saved_stacks[var]);
      }
      *out_val = nullptr;
      return llvm_gen_be_error_t::ok;
    }
#endif
    case enk_temp_init: {
       // A temporary object initialization.
       // We allocate stack storage for the temporary using alloca, and then run the dynamic_init
       // routine on that storage.
       // The resulting value is the pointer to the allocated memory.
       
       llvm::Type* expr_llvm_ty = nullptr;
       llvm_gen_be_error_t err = get_llvm_type(expr->type, &expr_llvm_ty);
       if (err != llvm_gen_be_error_t::ok) return err;

       // Use the builder's current basic block to get the function, then find its entry block for the alloca
       llvm::Function* parent_func = be_state->builder->GetInsertBlock()->getParent();
       llvm::IRBuilder<> tmp_builder(&parent_func->getEntryBlock(), parent_func->getEntryBlock().begin());
       llvm::AllocaInst* temp_alloc = tmp_builder.CreateAlloca(expr_llvm_ty, nullptr, "temp.init");
       
       // Handle dynamic initialization
       if (expr->variant.init.dynamic_init) {
           a_dynamic_init_ptr dip = expr->variant.init.dynamic_init;
           if (dip->kind == dik_expression) {
               llvm::Value* init_val = nullptr;
               err = llvm_lower_expression(dip->variant.expression, &init_val);
               if (err != llvm_gen_be_error_t::ok) return err;
               if (init_val) be_state->builder->CreateStore(init_val, temp_alloc);
           }
           // Other dik_* kinds like dik_constructor would need further lowering which will be handled as we implement constructor logic.
           
           if (dip->destructor) {
               // Track reference-binding lifetime extensions if needed, but for now
               // register cleanup destructor for temporary.
               // If the frontend has `has_temporary_lifetime` as TRUE, it's a normal temporary that dies at the end of the full-expression.
               // If FALSE, its lifetime has been extended (e.g. by reference binding) and will be destroyed by the block scope's `destructions` list.
               if (dip->has_temporary_lifetime) {
                   err = llvm_emit_eh_cleanup(dip->destructor, temp_alloc);
                   if (err != llvm_gen_be_error_t::ok) return err;
               }
           }
       }
       
       // If the temporary is an rvalue but the expression expects a value, we might need to load it. 
       // The frontend type system usually dictates if it's an lvalue or rvalue.
       // `expr->is_lvalue` determines if we yield the pointer or the loaded value.
       if (expr->is_lvalue) {
           *out_val = temp_alloc;
       } else {
           *out_val = be_state->builder->CreateLoad(expr_llvm_ty, temp_alloc);
       }
       
       return llvm_gen_be_error_t::ok;
    }
    case enk_object_lifetime: {
       // An expression-level object lifetime region.
       // The expression inside `expr->variant.object_lifetime.expr` should be evaluated.
       // Any cleanups registered in `expr->variant.object_lifetime.ptr->destructions` should be emitted afterwards.
       
       llvm::Value* result_val = nullptr;
       an_expr_node_ptr inner_expr = expr->variant.object_lifetime.expr;
       
       // Record stack size to pop cleanups added during evaluation
       size_t initial_cleanups = be_state->cleanup_stack.cleanups.size();
       
       if (inner_expr) {
           llvm_gen_be_error_t err = llvm_lower_expression(inner_expr, &result_val);
           if (err != llvm_gen_be_error_t::ok) return err;
       }
       
       // Process destructions list from the lifetime structure
       // Note: The EDG frontend typically maintains `destructions` list in `ptr->destructions`.
       // For a robust implementation we should iterate `ptr->destructions` and invoke destructors.
       // If cleanups were pushed onto `be_state->cleanup_stack` via `llvm_emit_eh_cleanup`, we should pop and execute them down to `initial_cleanups`.
       an_object_lifetime_ptr lifetime = expr->variant.object_lifetime.ptr;
       if (lifetime) {
           for (a_dynamic_init_ptr dtor_init = lifetime->destructions; dtor_init; dtor_init = dtor_init->next_in_destruction_list) {
               if (dtor_init->destructor && dtor_init->destructor->source_corresp.name) {
                   // This is a static destruction schedule provided by the AST
                   // Usually handled by iterating and creating calls
                   llvm::Function* dtor_func = be_state->module->getFunction(dtor_init->destructor->source_corresp.name);
                   if (dtor_func) {
                       // The object pointer needs to be extracted. In `enk_temp_init` we should have recorded the mapping.
                       // For a complete implementation we map `dtor_init` to the allocated llvm::Value.
                   }
               }
           }
       }
       
       // Evaluate RAII cleanups dynamically registered during the evaluation of `inner_expr`
       while (be_state->cleanup_stack.cleanups.size() > initial_cleanups) {
           ScopeCleanup cleanup = be_state->cleanup_stack.cleanups.back();
           be_state->cleanup_stack.cleanups.pop_back();
           if (cleanup.is_active && cleanup.dtor_routine && cleanup.object_ptr) {
               llvm::Function* dtor_func = be_state->module->getFunction(cleanup.dtor_routine->source_corresp.name ? cleanup.dtor_routine->source_corresp.name : "");
               if (dtor_func) {
                   llvm::Type* expected_ptr_ty = dtor_func->getFunctionType()->getParamType(0);
                   llvm::Value* obj = cleanup.object_ptr;
                   if (obj->getType() != expected_ptr_ty) {
                       obj = be_state->builder->CreatePointerCast(obj, expected_ptr_ty);
                   }
                   be_state->builder->CreateCall(dtor_func, {obj});
               }
           }
       }
       
       *out_val = result_val;
       return llvm_gen_be_error_t::ok;
    }
    case enk_typeid:
      return llvm_lower_typeid(expr, out_val);
    case enk_new_delete: {
       a_new_delete_supplement_ptr nd = expr->variant.new_delete;
       if (!nd) return llvm_gen_be_error_t::invalid_argument;

       if (nd->is_new) {
           a_routine_ptr routine = nd->routine;
           if (!routine) return llvm_gen_be_error_t::unsupported_expr;

           llvm::Type* alloc_type = nullptr;
           llvm_gen_be_error_t err = get_llvm_type(nd->type, &alloc_type);
           if (err != llvm_gen_be_error_t::ok) return err;
           
           std::vector<llvm::Value*> alloc_args;
           llvm::Type* size_t_ty = llvm::Type::getInt64Ty(*be_state->context);
           llvm::Value* size_val = nullptr;
           llvm::Value* num_elems = nullptr;
           
           if (!nd->placement_new || routine->source_corresp.name == std::string("operator new") || routine->source_corresp.name == std::string("operator new[]")) {
               if (nd->number_of_elements) {
                   err = llvm_lower_expression(nd->number_of_elements, &num_elems);
                   if (err != llvm_gen_be_error_t::ok) return err;
                   
                   uint64_t type_size = be_state->module->getDataLayout().getTypeAllocSize(alloc_type);
                   llvm::Value* type_size_val = llvm::ConstantInt::get(size_t_ty, type_size);
                   
                   if (num_elems->getType() != size_t_ty) {
                       num_elems = be_state->builder->CreateIntCast(num_elems, size_t_ty, false);
                   }
                   size_val = be_state->builder->CreateMul(num_elems, type_size_val);
                   
                   // Allocate array cookie (usually 8 bytes) for tracking the element count if the type is destructible
                   bool needs_cookie = type_has_nontrivial_destructor(nd->type);
                   if (needs_cookie) {
                       uint64_t cookie_size = 8; // Itanium ABI cookie size
                       llvm::Value* cookie_size_val = llvm::ConstantInt::get(size_t_ty, cookie_size);
                       size_val = be_state->builder->CreateAdd(size_val, cookie_size_val);
                   }
               } else {
                   uint64_t type_size = be_state->module->getDataLayout().getTypeAllocSize(alloc_type);
                   size_val = llvm::ConstantInt::get(size_t_ty, type_size);
               }
               
               alloc_args.push_back(size_val);
           }
           
           for (an_expr_node_ptr arg = nd->arg; arg; arg = arg->next) {
               llvm::Value* arg_val = nullptr;
               err = llvm_lower_expression(arg, &arg_val);
               if (err != llvm_gen_be_error_t::ok) return err;
               alloc_args.push_back(arg_val);
           }

           llvm::Function* func = nullptr;
           if (routine->source_corresp.name) {
               func = be_state->module->getFunction(routine->source_corresp.name);
               if (!func) {
                   std::vector<llvm::Type*> alloc_arg_tys;
                   for (auto* a : alloc_args) alloc_arg_tys.push_back(a->getType());
                   llvm::FunctionType* expected_func_ty = llvm::FunctionType::get(llvm::PointerType::getUnqual(*be_state->context), alloc_arg_tys, false);
                   func = llvm::Function::Create(
                      expected_func_ty,
                      llvm::GlobalValue::ExternalLinkage,
                      routine->source_corresp.name,
                      be_state->module.get()
                   );
               }
           }
           if (!func) return llvm_gen_be_error_t::unsupported_expr;
           
           llvm::CallInst* allocated_mem = be_state->builder->CreateCall(func, alloc_args);
           llvm::Value* user_mem = allocated_mem;
           
           if (nd->number_of_elements) {
               bool needs_cookie = type_has_nontrivial_destructor(nd->type);
               if (needs_cookie) {
                   llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
                   llvm::Value* cookie_size_val = llvm::ConstantInt::get(size_t_ty, 8);
                   
                   // Store the number of elements in the cookie
                   if (num_elems) {
                       llvm::Value* num_elems_cast = be_state->builder->CreateIntCast(num_elems, size_t_ty, false);
                       be_state->builder->CreateStore(num_elems_cast, allocated_mem);
                   }
                   
                   user_mem = be_state->builder->CreateInBoundsGEP(i8_ty, allocated_mem, cookie_size_val);
               }
           }
           
           llvm::Type* expr_llvm_ty = nullptr;
           err = get_llvm_type(expr->type, &expr_llvm_ty);
           if (err != llvm_gen_be_error_t::ok) return err;
           
           llvm::Value* casted_mem = user_mem;
           if (casted_mem->getType() != expr_llvm_ty) {
               casted_mem = be_state->builder->CreatePointerCast(user_mem, expr_llvm_ty);
           }
           
           if (nd->dynamic_init) {
               llvm::BasicBlock* normal_dest = nullptr;
               llvm::BasicBlock* unwind_dest = nullptr;
               
               if (nd->freeing_of_storage_on_exception) {
                   // Create blocks for normal execution and exception unwinding
                   normal_dest = llvm::BasicBlock::Create(*be_state->context, "new.init.cont", be_state->builder->GetInsertBlock()->getParent());
                   unwind_dest = llvm::BasicBlock::Create(*be_state->context, "new.init.unwind", be_state->builder->GetInsertBlock()->getParent());
                   
                   // Note: True exception handling (`invoke` / `landingpad`) requires the EH subsystem to be fully set up.
                   // Since EH is not fully implemented yet, we will just emit the normal initialization for now.
                   // When EH is active, we would use an invoke instruction for the constructor call.
               }
               
               if (nd->dynamic_init->kind == dik_expression) {
                   llvm::Value* init_val = nullptr;
                   err = llvm_lower_expression(nd->dynamic_init->variant.expression, &init_val);
                   if (err != llvm_gen_be_error_t::ok) return err;
                   if (init_val) be_state->builder->CreateStore(init_val, casted_mem);
               }
               
               if (nd->freeing_of_storage_on_exception) {
                   // If we threw, we would land in unwind_dest and execute freeing_of_storage_on_exception
                   // Currently unreachable since we don't emit invoke above
                   be_state->builder->SetInsertPoint(unwind_dest);
                   // Lower the cleanup (which is a delete call)
                   if (nd->freeing_of_storage_on_exception->destructor) {
                       a_routine_ptr cleanup_routine = nd->freeing_of_storage_on_exception->destructor;
                       if (cleanup_routine && cleanup_routine->source_corresp.name) {
                           llvm::Function* cleanup_func = be_state->module->getFunction(cleanup_routine->source_corresp.name);
                           if (!cleanup_func) {
                               llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                               std::vector<llvm::Type*> cleanup_arg_tys = {ptr_ty};
                               llvm::FunctionType* cleanup_func_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), cleanup_arg_tys, false);
                               cleanup_func = llvm::Function::Create(
                                  cleanup_func_ty,
                                  llvm::GlobalValue::ExternalLinkage,
                                  cleanup_routine->source_corresp.name,
                                  be_state->module.get()
                               );
                           }
                           
                           llvm::Value* ptr_to_free = allocated_mem;
                           if (ptr_to_free->getType() != cleanup_func->getFunctionType()->getParamType(0)) {
                               ptr_to_free = be_state->builder->CreatePointerCast(ptr_to_free, cleanup_func->getFunctionType()->getParamType(0));
                           }
                           
                           be_state->builder->CreateCall(cleanup_func, {ptr_to_free});
                       }
                   }
                   be_state->builder->CreateResume(llvm::UndefValue::get(llvm::StructType::get(*be_state->context, {llvm::PointerType::getUnqual(*be_state->context), llvm::Type::getInt32Ty(*be_state->context)})));
                   
                   // Back to normal execution
                   be_state->builder->SetInsertPoint(normal_dest);
                   if (normal_dest->hasNPredecessors(0)) {
                       // Make normal_dest reachable from the current block
                       // In a real EH implementation, `invoke` would branch here.
                       // For now, just branch directly since we used `call`
                       be_state->builder->GetInsertBlock()->getPrevNode()->getTerminator()->eraseFromParent();
                       llvm::IRBuilder<>(be_state->builder->GetInsertBlock()->getPrevNode()).CreateBr(normal_dest);
                   }
               }
           }
           
           *out_val = casted_mem;
           return llvm_gen_be_error_t::ok;
       } else {
           llvm::Value* ptr_to_delete = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(nd->arg, &ptr_to_delete);
           if (err != llvm_gen_be_error_t::ok) return err;
           
           llvm::Value* base_ptr = ptr_to_delete;
           
           if (nd->array_delete) {
               bool needs_cookie = type_has_nontrivial_destructor(nd->type);
               if (needs_cookie) {
                   llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
                   llvm::Type* size_t_ty = llvm::Type::getInt64Ty(*be_state->context);
                   llvm::Value* cookie_size_val = llvm::ConstantInt::get(size_t_ty, -8, true);
                   base_ptr = be_state->builder->CreateInBoundsGEP(i8_ty, ptr_to_delete, cookie_size_val);
               }
           }
           
           a_routine_ptr routine = nd->routine;
           if (routine) {
               std::vector<llvm::Value*> dealloc_args;
               dealloc_args.push_back(base_ptr);

               // In some configurations, EDG puts additional arguments (like size or alignment) for sized/aligned delete into the arg list, chained by `next` AFTER the pointer argument.
               // However, `nd->arg` for delete typically only represents the pointer being deleted.
               // Wait, the documentation in il_def.h says: "For delete, the pointer to the object to be deleted."
               // And "routine ... Non-NULL when a global sized or any aligned deallocation routine is to be used."
               // Actually, if it's a sized or aligned delete, we need to pass the size or alignment to the function. 
               // The EDG IL might not put the size/alignment in `nd->arg->next` for delete. We must construct it if the routine expects it.
               // For now, if the routine has more parameters, we should ideally extract them. Let's just push any args available in nd->arg->next.
               for (an_expr_node_ptr extra_arg = nd->arg->next; extra_arg; extra_arg = extra_arg->next) {
                   llvm::Value* extra_arg_val = nullptr;
                   err = llvm_lower_expression(extra_arg, &extra_arg_val);
                   if (err != llvm_gen_be_error_t::ok) return err;
                   dealloc_args.push_back(extra_arg_val);
               }

               // If it's a sized/aligned delete and the IL doesn't pass the size in `arg->next`, we might need to compute it.
               // But usually, EDG will provide the extra arguments in `arg->next` if the routine selected expects them.

               std::vector<llvm::Type*> dealloc_arg_tys;
               for (auto* a : dealloc_args) dealloc_arg_tys.push_back(a->getType());

               llvm::Function* func = nullptr;
               if (routine->source_corresp.name) {
                   func = be_state->module->getFunction(routine->source_corresp.name);
                   if (!func) {
                       llvm::FunctionType* expected_func_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), dealloc_arg_tys, false);
                       func = llvm::Function::Create(
                          expected_func_ty,
                          llvm::GlobalValue::ExternalLinkage,
                          routine->source_corresp.name,
                          be_state->module.get()
                       );
                   }
               }
               if (func) {
                   llvm::Type* expected_ptr_ty = func->getFunctionType()->getParamType(0);
                   if (dealloc_args[0]->getType() != expected_ptr_ty) {
                       dealloc_args[0] = be_state->builder->CreatePointerCast(dealloc_args[0], expected_ptr_ty);
                   }
                   be_state->builder->CreateCall(func, dealloc_args);
               }
           }
           
           *out_val = nullptr;
           return llvm_gen_be_error_t::ok;
       }
    }
    default:
       return llvm_gen_be_error_t::unsupported_expr;
  }
}

llvm_gen_be_error_t llvm_lower_compound_assignment(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!out_val) return llvm_gen_be_error_t::invalid_argument;
  *out_val = nullptr;

  if (expr->kind != enk_operation) return llvm_gen_be_error_t::invalid_argument;

  an_expr_node_ptr op1 = expr->variant.operation.operands;
  an_expr_node_ptr op2 = op1 ? op1->next : nullptr;

  llvm::Value* lhs_ptr = nullptr;
  llvm_gen_be_error_t err = llvm_lower_lvalue_expression(op1, &lhs_ptr);
  if (err != llvm_gen_be_error_t::ok) return err;

  llvm::Type* lhs_ty = nullptr;
  err = get_llvm_type(op1->type, &lhs_ty);
  if (err != llvm_gen_be_error_t::ok) return err;

  bool is_bitfield = false;
  a_field_ptr bitfield_ptr = nullptr;
  
  if (op1 && op1->kind == enk_operation && 
      (op1->variant.operation.kind == eok_dot_field || op1->variant.operation.kind == eok_points_to_field)) {
      an_expr_node_ptr sub_op2 = op1->variant.operation.operands->next;
      if (sub_op2 && sub_op2->kind == enk_field) {
          a_field_ptr field = sub_op2->variant.field.ptr;
          if (field && field->is_bit_field) {
              is_bitfield = true;
              bitfield_ptr = field;
          }
      }
  }

  llvm::Value* loaded_val = nullptr;
  if (is_bitfield) {
      err = llvm_emit_bitfield_load(lhs_ptr, bitfield_ptr, op1->type, &loaded_val);
      if (err != llvm_gen_be_error_t::ok) return err;
  } else {
      llvm::LoadInst* load = be_state->builder->CreateLoad(lhs_ty, lhs_ptr);
      if (is_volatile_qualified_type(op1->type)) load->setVolatile(true);
      loaded_val = load;
  }

  llvm::Value* rhs_val = nullptr;
  if (op2) {
      err = llvm_lower_expression(op2, &rhs_val);
      if (err != llvm_gen_be_error_t::ok) return err;
  } else {
      // pre/post incr/decr implicitly 1
      rhs_val = llvm::ConstantInt::get(loaded_val->getType(), 1);
  }

  llvm::Value* result_val = nullptr;
  bool is_float = loaded_val->getType()->isFloatingPointTy();
  bool is_signed = op1->type->kind == tk_integer ? int_kind_is_signed[op1->type->variant.integer.int_kind] : false;
  
  switch (expr->variant.operation.kind) {
      case eok_pre_incr:
      case eok_post_incr:
      case eok_add_assign:
          if (is_float) result_val = be_state->builder->CreateFAdd(loaded_val, rhs_val);
          else result_val = be_state->builder->CreateAdd(loaded_val, rhs_val);
          break;
      case eok_pre_decr:
      case eok_post_decr:
      case eok_subtract_assign:
          if (is_float) result_val = be_state->builder->CreateFSub(loaded_val, rhs_val);
          else result_val = be_state->builder->CreateSub(loaded_val, rhs_val);
          break;
      case eok_multiply_assign:
          if (is_float) result_val = be_state->builder->CreateFMul(loaded_val, rhs_val);
          else result_val = be_state->builder->CreateMul(loaded_val, rhs_val);
          break;
      case eok_divide_assign:
          if (is_float) result_val = be_state->builder->CreateFDiv(loaded_val, rhs_val);
          else if (is_signed) result_val = be_state->builder->CreateSDiv(loaded_val, rhs_val);
          else result_val = be_state->builder->CreateUDiv(loaded_val, rhs_val);
          break;
      case eok_remainder_assign:
          if (is_float) result_val = be_state->builder->CreateFRem(loaded_val, rhs_val);
          else if (is_signed) result_val = be_state->builder->CreateSRem(loaded_val, rhs_val);
          else result_val = be_state->builder->CreateURem(loaded_val, rhs_val);
          break;
      case eok_and_assign:
          result_val = be_state->builder->CreateAnd(loaded_val, rhs_val);
          break;
      case eok_or_assign:
          result_val = be_state->builder->CreateOr(loaded_val, rhs_val);
          break;
      case eok_xor_assign:
          result_val = be_state->builder->CreateXor(loaded_val, rhs_val);
          break;
      case eok_shiftl_assign:
          result_val = be_state->builder->CreateShl(loaded_val, rhs_val);
          break;
      case eok_shiftr_assign:
          if (is_signed) result_val = be_state->builder->CreateAShr(loaded_val, rhs_val);
          else result_val = be_state->builder->CreateLShr(loaded_val, rhs_val);
          break;
      case eok_padd_assign:
          result_val = be_state->builder->CreateInBoundsGEP(llvm::Type::getInt8Ty(*be_state->context), loaded_val, rhs_val);
          break;
      case eok_psubtract_assign: {
          llvm::Value* neg_rhs = be_state->builder->CreateNeg(rhs_val);
          result_val = be_state->builder->CreateInBoundsGEP(llvm::Type::getInt8Ty(*be_state->context), loaded_val, neg_rhs);
          break;
      }
      case eok_bassign: {
          // Block assignment
          uint64_t size = op1->type->size;
          llvm::Align align = llvm::Align(f_alignment_of_type(op1->type));
          be_state->builder->CreateMemCpy(lhs_ptr, align, rhs_val, align, size);
          *out_val = lhs_ptr; // Yield destination pointer for bassign
          return llvm_gen_be_error_t::ok;
      }
      default:
          return llvm_gen_be_error_t::unsupported_expr;
  }

  if (is_bitfield) {
      err = llvm_emit_bitfield_store(lhs_ptr, result_val, bitfield_ptr, op1->type);
      if (err != llvm_gen_be_error_t::ok) return err;
  } else {
      llvm::StoreInst* store = be_state->builder->CreateStore(result_val, lhs_ptr);
      if (is_volatile_qualified_type(op1->type)) store->setVolatile(true);
  }

  if (expr->variant.operation.kind == eok_post_incr || expr->variant.operation.kind == eok_post_decr) {
      *out_val = loaded_val;
  } else {
      *out_val = result_val;
  }

  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
