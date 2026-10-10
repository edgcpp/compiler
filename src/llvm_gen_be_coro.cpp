/**
 * @file llvm_gen_be_coro.cpp
 * @brief C++20 Coroutine lowering subsystem for the EDG LLVM backend.
 * @details Implements the lowering of coroutine statements and expressions using LLVM Coroutine Intrinsics.
 *
 * Part of the EDG Compiler Project.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_coro.h"
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Lowers a coroutine description statement into LLVM IR.
 * @details Emits llvm.coro.id, llvm.coro.alloc, and llvm.coro.begin intrinsics.
 * @param[in] stmt Pointer to the EDG coroutine statement node.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t llvm_lower_coroutine_stmt(a_statement_ptr stmt) noexcept {
  if (!stmt || stmt->kind != stmk_coroutine) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
  llvm::Type* int32_ty = llvm::Type::getInt32Ty(*be_state->context);
  llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);

  // coro.id
  llvm::Function* coro_id = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::coro_id);
  
  llvm::Constant* align = llvm::ConstantInt::get(int32_ty, 0); // Placeholder
  llvm::Constant* promise = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty)); // Placeholder
  llvm::Constant* coroutine_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty));
  llvm::Constant* empty_struct = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty));
  
  llvm::Value* id_val = be_state->builder->CreateCall(coro_id, {align, promise, coroutine_ptr, empty_struct}, "coro.id");

  // coro.alloc
  llvm::Function* coro_alloc = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::coro_alloc);
  llvm::Value* need_alloc = be_state->builder->CreateCall(coro_alloc, {id_val}, "coro.need.alloc");

  llvm::BasicBlock* alloc_bb = llvm::BasicBlock::Create(*be_state->context, "coro.alloc", func);
  llvm::BasicBlock* init_bb = llvm::BasicBlock::Create(*be_state->context, "coro.init", func);

  // Keep a dummy value for the PHI node's "other" block, we need a predecessor block
  llvm::BasicBlock* pred_bb = be_state->builder->GetInsertBlock();

  be_state->builder->CreateCondBr(need_alloc, alloc_bb, init_bb);

  // Allocation block
  be_state->builder->SetInsertPoint(alloc_bb);
  llvm::Function* coro_size = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::coro_size, {int32_ty});
  llvm::Value* size_val = be_state->builder->CreateCall(coro_size, {}, "coro.size");
  
  // Allocate memory (stub)
  // ... call operator new ...
  llvm::Value* raw_mem = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty)); // Stub raw memory
  be_state->builder->CreateBr(init_bb);

  // Init block
  be_state->builder->SetInsertPoint(init_bb);
  llvm::PHINode* mem_phi = be_state->builder->CreatePHI(int8_ptr_ty, 2, "coro.mem");
  mem_phi->addIncoming(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty)), pred_bb); // Stub
  mem_phi->addIncoming(raw_mem, alloc_bb);

  llvm::Function* coro_begin = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::coro_begin);
  llvm::Value* coro_hdl = be_state->builder->CreateCall(coro_begin, {id_val, mem_phi}, "coro.hdl");

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a coroutine return statement into LLVM IR.
 * @details Emits llvm.coro.end and cleans up the coroutine frame.
 * @param[in] stmt Pointer to the EDG coroutine return statement node.
 * @return llvm_gen_be_error_t::ok on success.
 */
llvm_gen_be_error_t llvm_lower_coroutine_return_stmt(a_statement_ptr stmt) noexcept {
  if (!stmt || stmt->kind != stmk_coroutine_return) return llvm_gen_be_error_t::invalid_argument;

  // Emits llvm.coro.end
  llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
  llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);

  llvm::Function* coro_end = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::coro_end);
  llvm::Value* hdl = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty)); // Need to retrieve actual handle
  llvm::Value* is_unwind = be_state->builder->getFalse();
  
  be_state->builder->CreateCall(coro_end, {hdl, is_unwind});
  
  if (func->getReturnType()->isVoidTy()) {
      be_state->builder->CreateRetVoid();
  } else {
      be_state->builder->CreateUnreachable(); // Stub
  }

  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
