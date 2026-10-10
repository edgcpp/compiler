/**
 * @file llvm_gen_be_eh.cpp
 * @brief Exception Handling subsystem for the EDG LLVM backend.
 * @details Implements C++ exception lowering for both Itanium ABI and MSVC SEH.
 *
 * Part of the EDG Compiler Project.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_eh.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Intrinsics.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

struct CleanupAction {
  a_routine_ptr dtor;
  llvm::Value* obj_ptr;
};

// Thread-local or state-managed cleanup stack (simplified for stub)
static std::vector<CleanupAction> active_cleanups;

/**
 * @brief llvm_emit_eh_cleanup
 * @param[in] _p param
 * @return llvm_gen_be_error_t::ok
 */
llvm_gen_be_error_t llvm_emit_eh_cleanup(a_routine_ptr dtor, llvm::Value* obj_ptr) noexcept {
  if (!dtor || !obj_ptr) return llvm_gen_be_error_t::inva/**
 * @brief llvm_lower_try_block_stmt
 * @param[in] _p param
 * @return llvm_gen_be_error_t::ok
 */
lid_argument;
  active_cleanups.push_back({dtor, obj_ptr});
  return llvm_gen_be_error_t::ok;
}
llvm_gen_be_error_t llvm_lower_microsoft_try_stmt(a_statement_ptr stmt) noexcept {
  if (!stmt || stmt->kind != stmk_microsoft_try) return llvm_gen_be_error_t::invalid_argument;
  
  a_microsoft_try_supplement_ptr sup = stmt->variant.microsoft_try;
  llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
  llvm::Type* int32_ty = llvm::Type::getInt32Ty(*be_state->context);
  
  llvm::FunctionCallee pers_fn = be_state->module->getOrInsertFunction("__C_specific_handler",
      llvm::FunctionType::get(int32_ty, true));
  func->setPersonalityFn(llvm::cast<llvm::Constant>(pers_fn.getCallee()));

  llvm::BasicBlock* try_bb = llvm::BasicBlock::Create(*be_state->context, "ms.try", func);
  llvm::BasicBlock* catch_dispatch_bb = llvm::BasicBlock::Create(*be_state->context, "ms.dispatch", func);
  llvm::BasicBlock* end_try_bb = llvm::BasicBlock::Create(*be_state->context, "ms.end");

  be_state->builder->CreateBr(try_bb);
  be_state->builder->SetInsertPoint(try_bb);

  be_state->current_landing_pads.push_back(catch_dispatch_bb);
  if (sup->guarded_statement) {
    llvm_gen_be_error_t err = llvm_lower_statement(sup->guarded_statement);
    if (err != llvm_gen_be_error_t::ok) return err;
  }
  be_state->current_landing_pads.pop_back();

  if (!be_state->builder->GetInsertBlock()->getTerminatorOrNull()) {
    be_state->builder->CreateBr(end_try_bb);
  }

  be_state->builder->SetInsertPoint(catch_dispatch_bb);
  
  if (sup->except_expr) {
      // __try / __except
      llvm::CatchSwitchInst* catch_switch = be_state->builder->CreateCatchSwitch(
          llvm::ConstantTokenNone::get(*be_state->context), nullptr, 1);
          
      llvm::BasicBlock* catch_bb = llvm::BasicBlock::Create(*be_state->context, "ms.catch", func);
      catch_switch->addHandler(catch_bb);
      
      be_state->builder->SetInsertPoint(catch_bb);
      
      // Filter function pointer (dummy for now)
      llvm::Constant* filter_fn = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
      llvm::CatchPadInst* catch_pad = be_state->builder->CreateCatchPad(catch_switch, {filter_fn});
      
      if (sup->cleanup_statement) {
          llvm_gen_be_error_t err = llvm_lower_statement(sup->cleanup_statement);
          if (err != llvm_gen_be_error_t::ok) return err;
      }
      
      be_state->builder->CreateCatchRet(catch_pad, end_try_bb);
  } else {
      // __try / __finally
      llvm::CleanupPadInst* cleanuppad = be_state->builder->CreateCleanupPad(llvm::ConstantTokenNone::get(*be_state->context), {});
      
      if (sup->cleanup_statement) {
          llvm_gen_be_error_t err = llvm_lower_statement(sup->cleanup_statement);
          if (err != llvm_gen_be_error_t::ok) return err;
      }
      
      be_state->builder->CreateCleanupRet(cleanuppad, nullptr);
  }

  func->insert(func->end(), end_try_bb);
  be_state->builder->SetInsertPoint(end_try_bb);
  
  return llvm_gen_be_error_t::ok;
}


llvm_gen_be_error_t llvm_lower_try_block_stmt(a_statement_ptr stmt) noexcept {
  if (!stmt || stmt->kind != stmk_try_block) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
  llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Type* int32_ty = llvm::Type::getInt32Ty(*be_state->context);

  // Check target triple for MSVC ABI
  bool is_msvc = be_state->module->getTargetTriple().getTriple().find("windows-msvc") != std::string::npos;

  if (is_msvc) {
      llvm::FunctionCallee pers_fn = be_state->module->getOrInsertFunction("__CxxFrameHandler3",
          llvm::FunctionType::get(int32_ty, true));
      func->setPersonalityFn(llvm::cast<llvm::Constant>(pers_fn.getCallee()));

      llvm::BasicBlock* try_bb = llvm::BasicBlock::Create(*be_state->context, "try", func);
      llvm::BasicBlock* catch_dispatch_bb = llvm::BasicBlock::Create(*be_state->context, "catch.dispatch", func);
      llvm::BasicBlock* end_try_bb = llvm::BasicBlock::Create(*be_state->context, "try.end");

      be_state->builder->CreateBr(try_bb);
      be_state->builder->SetInsertPoint(try_bb);

      be_state->current_landing_pads.push_back(catch_dispatch_bb);
      if (stmt->variant.try_block->statement) {
        llvm_gen_be_error_t err = llvm_lower_statement(stmt->variant.try_block->statement);
        if (err != llvm_gen_be_error_t::ok) return err;
      }
      be_state->current_landing_pads.pop_back();

      if (!be_state->builder->GetInsertBlock()->getTerminatorOrNull()) {
        be_state->builder->CreateBr(end_try_bb);
      }

      be_state->builder->SetInsertPoint(catch_dispatch_bb);
      llvm::CatchSwitchInst* catch_switch = be_state->builder->CreateCatchSwitch(
          llvm::ConstantTokenNone::get(*be_state->context), nullptr, 1);

      for (a_handler_ptr h = stmt->variant.try_block->handlers; h; h = h->next) {
        llvm::BasicBlock* catch_bb = llvm::BasicBlock::Create(*be_state->context, "catch", func);
        catch_switch->addHandler(catch_bb);

        be_state->builder->SetInsertPoint(catch_bb);
        
        llvm::Constant* typeinfo_ptr = nullptr;
        if (h->parameter) {
             llvm_gen_be_error_t err_ti = get_typeinfo_global(h->parameter->type, &typeinfo_ptr);
             if (err_ti != llvm_gen_be_error_t::ok) return err_ti;
        } else {
             typeinfo_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty)); // Catch all
        }

        llvm::CatchPadInst* catch_pad = be_state->builder->CreateCatchPad(catch_switch, {typeinfo_ptr, llvm::ConstantInt::get(int32_ty, 0), llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty))});

        if (h->statement) {
          llvm_gen_be_error_t err = llvm_lower_statement(h->statement);
          if (err != llvm_gen_be_error_t::ok) return err;
        }

        be_state->builder->CreateCatchRet(catch_pad, end_try_bb);
      }

      // Optional Cleanup Pad for unwinding
      if (!active_cleanups.empty()) {
          llvm::BasicBlock* cleanup_bb = llvm::BasicBlock::Create(*be_state->context, "cleanup", func);
          be_state->builder->SetInsertPoint(cleanup_bb);
          llvm::CleanupPadInst* cleanuppad = be_state->builder->CreateCleanupPad(llvm::ConstantTokenNone::get(*be_state->context), {});
          // Emit destructors...
          active_cleanups.clear();
          be_state->builder->CreateCleanupRet(cleanuppad, nullptr);
      }

      func->insert(func->end(), end_try_bb);
      be_state->builder->SetInsertPoint(end_try_bb);

  } else {
      // Itanium ABI
      llvm::FunctionCallee pers_fn = be_state->module->getOrInsertFunction("__gxx_personality_v0",
          llvm::FunctionType::get(int32_ty, true));
      func->setPersonalityFn(llvm::cast<llvm::Constant>(pers_fn.getCallee()));

      llvm::BasicBlock* try_bb = llvm::BasicBlock::Create(*be_state->context, "try", func);
      llvm::BasicBlock* lpad_bb = llvm::BasicBlock::Create(*be_state->context, "lpad", func);
      llvm::BasicBlock* end_try_bb = llvm::BasicBlock::Create(*be_state->context, "try.end");

      be_state->builder->CreateBr(try_bb);
      be_state->builder->SetInsertPoint(try_bb);

      be_state->current_landing_pads.push_back(lpad_bb);
      if (stmt->variant.try_block->statement) {
        llvm_gen_be_error_t err = llvm_lower_statement(stmt->variant.try_block->statement);
        if (err != llvm_gen_be_error_t::ok) return err;
      }
      be_state->current_landing_pads.pop_back();

      if (!be_state->builder->GetInsertBlock()->getTerminatorOrNull()) {
        be_state->builder->CreateBr(end_try_bb);
      }

      be_state->builder->SetInsertPoint(lpad_bb);
      llvm::StructType* lpad_ty = llvm::StructType::get(*be_state->context, {int8_ptr_ty, int32_ty});
      llvm::LandingPadInst* lpad = be_state->builder->CreateLandingPad(lpad_ty, 0);

      bool has_catch_all = false;
      for (a_handler_ptr h = stmt->variant.try_block->handlers; h; h = h->next) {
        if (!h->parameter) {
          has_catch_all = true;
          lpad->addClause(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty)));
        } else {
          llvm::Constant* typeinfo_ptr = nullptr;
          llvm_gen_be_error_t err_ti = get_typeinfo_global(h->parameter->type, &typeinfo_ptr);
          if (err_ti != llvm_gen_be_error_t::ok) return err_ti;
          lpad->addClause(typeinfo_ptr);
        }
      }
      lpad->setCleanup(true);

      llvm::Value* exc_ptr = be_state->builder->CreateExtractValue(lpad, 0, "exc_ptr");
      llvm::Value* exc_sel = be_state->builder->CreateExtractValue(lpad, 1, "exc_sel");

      llvm::FunctionCallee begin_catch_fn = be_state->module->getOrInsertFunction("__cxa_begin_catch",
         llvm::FunctionType::get(int8_ptr_ty, {int8_ptr_ty}, false));
      llvm::FunctionCallee end_catch_fn = be_state->module->getOrInsertFunction("__cxa_end_catch",
         llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false));
      llvm::FunctionCallee typeid_fn = be_state->module->getOrInsertFunction("llvm.eh.typeid.for",
         llvm::FunctionType::get(int32_ty, {int8_ptr_ty}, false));

      llvm::BasicBlock* resume_bb = llvm::BasicBlock::Create(*be_state->context, "resume", func);
      llvm::BasicBlock* current_dispatch_bb = be_state->builder->GetInsertBlock();

      for (a_handler_ptr h = stmt->variant.try_block->handlers; h; h = h->next) {
        llvm::BasicBlock* catch_bb = llvm::BasicBlock::Create(*be_state->context, "catch", func);
        llvm::BasicBlock* next_dispatch_bb = llvm::BasicBlock::Create(*be_state->context, "catch.fallthrough", func);

        be_state->builder->SetInsertPoint(current_dispatch_bb);
        
        if (!h->parameter) {
          be_state->builder->CreateBr(catch_bb);
        } else {
          llvm::Constant* typeinfo_ptr = nullptr;
          llvm_gen_be_error_t err_ti = get_typeinfo_global(h->parameter->type, &typeinfo_ptr);
          if (err_ti != llvm_gen_be_error_t::ok) return err_ti;
          llvm::Value* typeid_val = be_state->builder->CreateCall(typeid_fn, {typeinfo_ptr});
          llvm::Value* cmp = be_state->builder->CreateICmpEQ(exc_sel, typeid_val);
          be_state->builder->CreateCondBr(cmp, catch_bb, next_dispatch_bb);
        }

        be_state->builder->SetInsertPoint(catch_bb);
        be_state->builder->CreateCall(begin_catch_fn, {exc_ptr});

        if (h->statement) {
          llvm_gen_be_error_t err = llvm_lower_statement(h->statement);
          if (err != llvm_gen_be_error_t::ok) return err;
        }

        be_state->builder->CreateCall(end_catch_fn);
        if (!be_state->builder->GetInsertBlock()->getTerminatorOrNull()) {
          be_state->builder->CreateBr(end_try_bb);
        }

        current_dispatch_bb = next_dispatch_bb;
      }

      be_state->builder->SetInsertPoint(current_dispatch_bb);
      be_state->builder->CreateBr(resume_bb);

      be_state->builder->SetInsertPoint(resume_bb);
      be_state->builder->CreateResume(lpad);

      func->insert(func->end(), end_try_bb);
      be_state->builder->SetInsertPoint(end_try_bb);
  }
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
