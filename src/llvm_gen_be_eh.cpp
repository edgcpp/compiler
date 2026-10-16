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

/**
 * @brief Emits an RAII exception cleanup registration for a destructor call.
 * @details Appends a new scope cleanup record to the active cleanup stack.
 * @param[in] dtor The EDG routine pointer for the destructor.
 * @param[in] obj_ptr The LLVM value pointing to the object instance.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if pointers are null.
 */
llvm_gen_be_error_t llvm_emit_eh_cleanup(a_routine_ptr dtor, llvm::Value* obj_ptr) noexcept {
  if (!dtor || !obj_ptr) return llvm_gen_be_error_t::invalid_argument;
  ScopeCleanup cleanup = {};
  cleanup.dtor_routine = dtor;
  cleanup.object_ptr = obj_ptr;
  cleanup.is_active = true;
  be_state->cleanup_stack.cleanups.push_back(cleanup);
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_microsoft_try_stmt(a_statement_ptr stmt) noexcept {
  if (!stmt || stmt->kind != stmk_microsoft_try) return llvm_gen_be_error_t::invalid_argument;
  
  a_microsoft_try_supplement_ptr sup = stmt->variant.microsoft_try;
  llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
  llvm::Type* int32_ty = llvm::Type::getInt32Ty(*be_state->context);
  llvm::Type* void_ty = llvm::Type::getVoidTy(*be_state->context);
  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  
  llvm::FunctionCallee pers_fn = be_state->module->getOrInsertFunction("__C_specific_handler",
      llvm::FunctionType::get(int32_ty, true));
  func->setPersonalityFn(llvm::cast<llvm::Constant>(pers_fn.getCallee()));

  llvm::BasicBlock* try_bb = llvm::BasicBlock::Create(*be_state->context, "ms.try", func);
  llvm::BasicBlock* try_body_bb = llvm::BasicBlock::Create(*be_state->context, "ms.try.body", func);
  llvm::BasicBlock* catch_dispatch_bb = llvm::BasicBlock::Create(*be_state->context, "ms.dispatch", func);
  llvm::BasicBlock* end_try_bb = llvm::BasicBlock::Create(*be_state->context, "ms.end");

  be_state->builder->CreateBr(try_bb);
  be_state->builder->SetInsertPoint(try_bb);

  llvm::FunctionCallee seh_begin = be_state->module->getOrInsertFunction("llvm.seh.try.begin", llvm::FunctionType::get(void_ty, false));
  llvm::FunctionCallee seh_end = be_state->module->getOrInsertFunction("llvm.seh.try.end", llvm::FunctionType::get(void_ty, false));

  llvm::Function* fin_func = nullptr;
  if (!sup->except_expr && sup->cleanup_statement) {
      // Outline __finally block into a funclet
      llvm::FunctionType* fin_ft = llvm::FunctionType::get(void_ty, {llvm::Type::getInt8Ty(*be_state->context), ptr_ty}, false);
      fin_func = llvm::Function::Create(fin_ft, llvm::Function::InternalLinkage, func->getName() + ".fin$0", be_state->module.get());
      llvm::BasicBlock* fin_entry = llvm::BasicBlock::Create(*be_state->context, "entry", fin_func);
      
      llvm::IRBuilder<>::InsertPoint saved_ip = be_state->builder->saveIP();
      be_state->builder->SetInsertPoint(fin_entry);
      
      llvm::FunctionCallee localrecover_fn = be_state->module->getOrInsertFunction("llvm.localrecover",
         llvm::FunctionType::get(ptr_ty, {ptr_ty, ptr_ty, int32_ty}, false));
      
      llvm::Value* fp = fin_func->getArg(1);
      auto backup_vars = be_state->local_vars;
      for (auto& pair : backup_vars) {
          llvm::Value* orig_alloca = pair.second;
          auto it = be_state->escaped_var_indices.find(orig_alloca);
          int index = 0;
          if (it == be_state->escaped_var_indices.end()) {
              index = be_state->escaped_allocas.size();
              be_state->escaped_allocas.push_back(orig_alloca);
              be_state->escaped_var_indices[orig_alloca] = index;
          } else {
              index = it->second;
          }
          llvm::Value* recovered = be_state->builder->CreateCall(localrecover_fn, {
              llvm::cast<llvm::Constant>(func), fp, llvm::ConstantInt::get(int32_ty, index)
          });
          be_state->local_vars[pair.first] = be_state->builder->CreatePointerCast(recovered, orig_alloca->getType());
      }
      
      llvm_gen_be_error_t err = llvm_lower_statement(sup->cleanup_statement);
      
      be_state->local_vars = backup_vars;
      if (err != llvm_gen_be_error_t::ok) return err;
      be_state->builder->CreateRetVoid();
      be_state->builder->restoreIP(saved_ip);
      
      // Register it in the cleanup stack for normal non-local exits
      ScopeCleanup cleanup = {};
      cleanup.finally_func = fin_func;
      cleanup.is_active = true;
      be_state->cleanup_stack.cleanups.push_back(cleanup);
  }

  be_state->builder->CreateInvoke(seh_begin, try_body_bb, catch_dispatch_bb);
  
  be_state->builder->SetInsertPoint(try_body_bb);
  be_state->current_landing_pads.push_back(catch_dispatch_bb);
  if (sup->guarded_statement) {
    llvm_gen_be_error_t err = llvm_lower_statement(sup->guarded_statement);
    if (err != llvm_gen_be_error_t::ok) return err;
  }
  be_state->current_landing_pads.pop_back();

  if (fin_func) {
      be_state->cleanup_stack.cleanups.pop_back();
  }

  if (!be_state->builder->GetInsertBlock()->getTerminatorOrNull()) {
    llvm::BasicBlock* after_end_bb = llvm::BasicBlock::Create(*be_state->context, "ms.try.after", func);
    be_state->builder->CreateInvoke(seh_end, after_end_bb, catch_dispatch_bb);
    be_state->builder->SetInsertPoint(after_end_bb);
    if (fin_func) {
        llvm::FunctionCallee localaddr_fn = be_state->module->getOrInsertFunction("llvm.localaddress",
           llvm::FunctionType::get(ptr_ty, {}, false));
        llvm::Value* local_addr = be_state->builder->CreateCall(localaddr_fn, {});
        be_state->builder->CreateCall(fin_func, {
            llvm::ConstantInt::get(llvm::Type::getInt8Ty(*be_state->context), 0),
            local_addr
        });
    }
    be_state->builder->CreateBr(end_try_bb);
  }

  be_state->builder->SetInsertPoint(catch_dispatch_bb);
  
  llvm::Value* parent_pad = be_state->active_funclet_pads.empty() ? 
      static_cast<llvm::Value*>(llvm::ConstantTokenNone::get(*be_state->context)) : 
      be_state->active_funclet_pads.back();

  if (sup->except_expr) {
      // __try / __except
      llvm::CatchSwitchInst* catch_switch = be_state->builder->CreateCatchSwitch(
          parent_pad, nullptr, 1);
          
      llvm::BasicBlock* catch_bb = llvm::BasicBlock::Create(*be_state->context, "ms.catch", func);
      catch_switch->addHandler(catch_bb);
      
      // Out-of-line filter expression generator
      llvm::FunctionType* filter_ft = llvm::FunctionType::get(int32_ty, {ptr_ty, ptr_ty}, false);
      llvm::Function* filter_func = llvm::Function::Create(filter_ft, llvm::Function::InternalLinkage, func->getName() + ".filt$fe", be_state->module.get());
      llvm::BasicBlock* filter_entry = llvm::BasicBlock::Create(*be_state->context, "entry", filter_func);
      
      llvm::IRBuilder<>::InsertPoint saved_ip = be_state->builder->saveIP();
      be_state->builder->SetInsertPoint(filter_entry);
      
      llvm::FunctionCallee recoverfp_fn = be_state->module->getOrInsertFunction("llvm.eh.recoverfp",
         llvm::FunctionType::get(ptr_ty, {ptr_ty, ptr_ty}, false));
      llvm::Value* fp = be_state->builder->CreateCall(recoverfp_fn, {llvm::cast<llvm::Constant>(func), filter_func->getArg(1)});
      
      llvm::FunctionCallee localrecover_fn = be_state->module->getOrInsertFunction("llvm.localrecover",
         llvm::FunctionType::get(ptr_ty, {ptr_ty, ptr_ty, int32_ty}, false));

      auto backup_vars = be_state->local_vars;
      for (auto& pair : backup_vars) {
          llvm::Value* orig_alloca = pair.second;
          auto it = be_state->escaped_var_indices.find(orig_alloca);
          int index = 0;
          if (it == be_state->escaped_var_indices.end()) {
              index = be_state->escaped_allocas.size();
              be_state->escaped_allocas.push_back(orig_alloca);
              be_state->escaped_var_indices[orig_alloca] = index;
          } else {
              index = it->second;
          }
          llvm::Value* recovered = be_state->builder->CreateCall(localrecover_fn, {
              llvm::cast<llvm::Constant>(func), fp, llvm::ConstantInt::get(int32_ty, index)
          });
          // Cast the recovered pointer back to the original alloca type (or pointer type)
          be_state->local_vars[pair.first] = be_state->builder->CreatePointerCast(recovered, orig_alloca->getType());
      }
      
      llvm::Value* filter_val = nullptr;
      llvm_gen_be_error_t err = llvm_lower_expression(sup->except_expr, &filter_val);
      
      be_state->local_vars = backup_vars; // Restore

      if (err != llvm_gen_be_error_t::ok) return err;
      be_state->builder->CreateRet(filter_val);
      
      be_state->builder->restoreIP(saved_ip);
      
      be_state->builder->SetInsertPoint(catch_bb);
      llvm::CatchPadInst* catch_pad = be_state->builder->CreateCatchPad(catch_switch, {filter_func});
      
      be_state->active_funclet_pads.push_back(catch_pad);
      if (sup->cleanup_statement) {
          err = llvm_lower_statement(sup->cleanup_statement);
          if (err != llvm_gen_be_error_t::ok) return err;
      }
      be_state->active_funclet_pads.pop_back();
      
      be_state->builder->CreateCatchRet(catch_pad, end_try_bb);
  } else {
      // __try / __finally
      llvm::CleanupPadInst* cleanup_pad = be_state->builder->CreateCleanupPad(parent_pad, {});
      
      be_state->active_funclet_pads.push_back(cleanup_pad);
      if (fin_func) {
          llvm::FunctionCallee localaddr_fn = be_state->module->getOrInsertFunction("llvm.localaddress",
             llvm::FunctionType::get(ptr_ty, {}, false));
          llvm::Value* local_addr = be_state->builder->CreateCall(localaddr_fn, {});
          
          llvm::OperandBundleDef ob("funclet", cleanup_pad);
          llvm::CallInst* fin_call = be_state->builder->CreateCall(fin_func, {
              llvm::ConstantInt::get(llvm::Type::getInt8Ty(*be_state->context), 1),
              local_addr
          }, {ob});
      }
      be_state->active_funclet_pads.pop_back();
      
      be_state->builder->CreateCleanupRet(cleanup_pad, nullptr);
  }

  func->insert(func->end(), end_try_bb);
  be_state->builder->SetInsertPoint(end_try_bb);
  
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_eh_exceptioncode(llvm::Value** out_val) noexcept {
  if (!out_val) return llvm_gen_be_error_t::invalid_argument;
  auto it = be_state->builder->GetInsertBlock()->getFirstNonPHIIt();
  llvm::CatchPadInst* cpi = (it != be_state->builder->GetInsertBlock()->end()) ? llvm::dyn_cast<llvm::CatchPadInst>(&*it) : nullptr;
  llvm::Value* token = cpi ? static_cast<llvm::Value*>(cpi) : llvm::ConstantTokenNone::get(*be_state->context);
  llvm::FunctionCallee exc_code = be_state->module->getOrInsertFunction(
      "llvm.eh.exceptioncode", llvm::FunctionType::get(llvm::Type::getInt32Ty(*be_state->context), {token->getType()}, false));
  *out_val = be_state->builder->CreateCall(exc_code, {token});
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_eh_exceptioninfo(llvm::Value** out_val) noexcept {
  if (!out_val) return llvm_gen_be_error_t::invalid_argument;
  llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
  if (func->arg_size() >= 2) {
      *out_val = func->getArg(0); 
  } else {
      *out_val = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
  }
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_seh_setjmp(llvm::Value* jmp_buf, llvm::Value** out_val) noexcept {
  if (!jmp_buf || !out_val) return llvm_gen_be_error_t::invalid_argument;
  llvm::FunctionCallee setjmp_fn = be_state->module->getOrInsertFunction("_setjmp",
      llvm::FunctionType::get(llvm::Type::getInt32Ty(*be_state->context), {llvm::PointerType::getUnqual(*be_state->context), llvm::PointerType::getUnqual(*be_state->context)}, false));
  
  llvm::FunctionCallee frameaddr = be_state->module->getOrInsertFunction("llvm.frameaddress.p0",
      llvm::FunctionType::get(llvm::PointerType::getUnqual(*be_state->context), {llvm::Type::getInt32Ty(*be_state->context)}, false));
  llvm::Value* frame = be_state->builder->CreateCall(frameaddr, {llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0)});
  
  *out_val = be_state->builder->CreateCall(setjmp_fn, {jmp_buf, frame});
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_seh_longjmp(llvm::Value* jmp_buf, llvm::Value* val) noexcept {
  if (!jmp_buf || !val) return llvm_gen_be_error_t::invalid_argument;
  llvm::FunctionCallee longjmp_fn = be_state->module->getOrInsertFunction("longjmp",
      llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), {llvm::PointerType::getUnqual(*be_state->context), llvm::Type::getInt32Ty(*be_state->context)}, false));
  be_state->builder->CreateCall(longjmp_fn, {jmp_buf, val});
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_try_block_stmt(a_statement_ptr stmt) noexcept {
  if (!stmt || stmt->kind != stmk_try_block) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
  llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Type* int32_ty = llvm::Type::getInt32Ty(*be_state->context);

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
      
      llvm::Value* parent_pad = be_state->active_funclet_pads.empty() ? 
          static_cast<llvm::Value*>(llvm::ConstantTokenNone::get(*be_state->context)) : 
          be_state->active_funclet_pads.back();
          
      llvm::CatchSwitchInst* catch_switch = be_state->builder->CreateCatchSwitch(
          parent_pad, nullptr, 1);

      for (a_handler_ptr h = stmt->variant.try_block->handlers; h; h = h->next) {
        llvm::BasicBlock* catch_bb = llvm::BasicBlock::Create(*be_state->context, "catch", func);
        catch_switch->addHandler(catch_bb);

        be_state->builder->SetInsertPoint(catch_bb);
        
        llvm::Constant* typeinfo_ptr = nullptr;
        if (h->parameter) {
             llvm_gen_be_error_t err_ti = get_typeinfo_global(h->parameter->type, &typeinfo_ptr);
             if (err_ti != llvm_gen_be_error_t::ok) return err_ti;
        } else {
             typeinfo_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty)); 
        }

        llvm::CatchPadInst* catch_pad = be_state->builder->CreateCatchPad(catch_switch, {typeinfo_ptr, llvm::ConstantInt::get(int32_ty, 0), llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty))});

        be_state->active_funclet_pads.push_back(catch_pad);
        if (h->statement) {
          llvm_gen_be_error_t err = llvm_lower_statement(h->statement);
          if (err != llvm_gen_be_error_t::ok) return err;
        }
        be_state->active_funclet_pads.pop_back();

        be_state->builder->CreateCatchRet(catch_pad, end_try_bb);
      }

      if (!be_state->cleanup_stack.cleanups.empty()) {
          llvm::BasicBlock* cleanup_bb = llvm::BasicBlock::Create(*be_state->context, "cleanup", func);
          be_state->builder->SetInsertPoint(cleanup_bb);
          llvm::CleanupPadInst* cleanuppad = be_state->builder->CreateCleanupPad(parent_pad, {});
          
          be_state->active_funclet_pads.push_back(cleanuppad);
          be_state->cleanup_stack.cleanups.clear();
          be_state->active_funclet_pads.pop_back();
          
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

llvm_gen_be_error_t llvm_emit_exception_specification_lpad(
    a_routine_ptr routine,
    llvm::BasicBlock* lpad_bb) noexcept {
    if (!routine || !lpad_bb) return llvm_gen_be_error_t::invalid_argument;

    llvm::Function* func = lpad_bb->getParent();
    if (!func) return llvm_gen_be_error_t::invalid_argument;

    be_state->builder->SetInsertPoint(lpad_bb);
    llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::Type* int32_ty = llvm::Type::getInt32Ty(*be_state->context);

    bool is_msvc = be_state->module->getTargetTriple().getTriple().find("windows-msvc") != std::string::npos;

    if (is_msvc) {
        // MSVC doesn't natively use landingpad for C++ exception specifications the same way Itanium does,
        // but for completion, we'll emit a cleanuppad/terminate sequence here if needed.
        // Itanium is the primary target for exception specifications.
        llvm::FunctionCallee terminate_fn = be_state->module->getOrInsertFunction("?terminate@@YAXXZ",
            llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false));
        
        llvm::CatchSwitchInst* catch_switch = be_state->builder->CreateCatchSwitch(
            llvm::ConstantTokenNone::get(*be_state->context), nullptr, 1);
        llvm::BasicBlock* catch_bb = llvm::BasicBlock::Create(*be_state->context, "ms.terminate.catch", func);
        catch_switch->addHandler(catch_bb);
        
        be_state->builder->SetInsertPoint(catch_bb);
        be_state->builder->CreateCatchPad(catch_switch, {});
        be_state->builder->CreateCall(terminate_fn);
        be_state->builder->CreateUnreachable();
        return llvm_gen_be_error_t::ok;
    }

    // Itanium ABI
    llvm::FunctionCallee pers_fn = be_state->module->getOrInsertFunction("__gxx_personality_v0",
        llvm::FunctionType::get(int32_ty, true));
    func->setPersonalityFn(llvm::cast<llvm::Constant>(pers_fn.getCallee()));

    llvm::StructType* lpad_ty = llvm::StructType::get(*be_state->context, {int8_ptr_ty, int32_ty});
    llvm::LandingPadInst* lpad = be_state->builder->CreateLandingPad(lpad_ty, 1);
    lpad->setCleanup(false);

    a_type_ptr rtype = skip_typerefs(routine->type);
    an_exception_specification_ptr spec = rtype->variant.routine.extra_info->exception_specification;

    if (spec->is_noexcept) {
        // Filter [0 x i8*] zeroinitializer
        llvm::ArrayType* arr_ty = llvm::ArrayType::get(int8_ptr_ty, 0);
        llvm::Constant* filter_arr = llvm::ConstantArray::get(arr_ty, llvm::ArrayRef<llvm::Constant*>{});
        lpad->addClause(filter_arr);

        llvm::FunctionCallee terminate_fn = be_state->module->getOrInsertFunction("_ZSt9terminatev",
            llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false));
        be_state->builder->CreateCall(terminate_fn);
        be_state->builder->CreateUnreachable();
    } else {
        // throw(A, B) -> Filter array of types
        std::vector<llvm::Constant*> filter_types;
        for (an_exception_specification_type_ptr t = spec->variant.exception_specification_type_list; t; t = t->next) {
            llvm::Constant* typeinfo_ptr = nullptr;
            llvm_gen_be_error_t err_ti = get_typeinfo_global(t->type, &typeinfo_ptr);
            if (err_ti != llvm_gen_be_error_t::ok) return err_ti;
            filter_types.push_back(typeinfo_ptr);
        }

        llvm::ArrayType* arr_ty = llvm::ArrayType::get(int8_ptr_ty, filter_types.size());
        llvm::Constant* filter_arr = llvm::ConstantArray::get(arr_ty, filter_types);
        lpad->addClause(filter_arr);

        llvm::Value* exc_ptr = be_state->builder->CreateExtractValue(lpad, 0, "exc_ptr");
        llvm::FunctionCallee unexpected_fn = be_state->module->getOrInsertFunction("__cxa_call_unexpected",
            llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty}, false));
        be_state->builder->CreateCall(unexpected_fn, {exc_ptr});
        be_state->builder->CreateUnreachable();
    }

    return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
