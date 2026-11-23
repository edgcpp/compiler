/**
 * @file llvm_gen_be_eh.cpp
 * @ingroup llvm_gen_be_eh
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
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  ScopeCleanup cleanup = {};
  cleanup.dtor_routine = dtor;
  cleanup.object_ptr = obj_ptr;
  cleanup.is_active = true;
  be_state->cleanup_stack.cleanups.push_back(cleanup);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a Microsoft SEH `__try` / `__except` or `__try` / `__finally` statement.
 * @details Outlines filter expressions and `__finally` cleanup blocks into funclets using
 *          `llvm.localrecover` and `llvm.eh.recoverfp`, and emits `catchswitch` / `catchpad`
 *          or `cleanuppad` instructions for `__C_specific_handler`.
 * @param[in] stmt Pointer to the EDG statement node (`stmk_microsoft_try`).
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
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
  llvm::BasicBlock* leave_target_bb = llvm::BasicBlock::Create(*be_state->context, "ms.try.leave", func);
  llvm::BasicBlock* catch_dispatch_bb = llvm::BasicBlock::Create(*be_state->context, "ms.dispatch", func);
  llvm::BasicBlock* end_try_bb = llvm::BasicBlock::Create(*be_state->context, "ms.end");

  if (be_state->enable_async_seh) {
    LLVM_GEN_BE_CHECK(llvm_enable_async_seh_attributes(func));
  }

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
              index = static_cast<int>(be_state->escaped_allocas.size());
              be_state->escaped_allocas.push_back(orig_alloca);
              be_state->escaped_var_indices[orig_alloca] = index;
          } else {
              index = it->second;
          }
          llvm::Value* recovered = be_state->builder->CreateCall(localrecover_fn, {
              llvm::cast<llvm::Constant>(func), fp, llvm::ConstantInt::get(int32_ty, static_cast<uint64_t>(index))
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
  be_state->seh_leave_blocks.push_back(leave_target_bb);
  if (sup->guarded_statement) {
    llvm_gen_be_error_t err = llvm_lower_statement(sup->guarded_statement);
    if (err != llvm_gen_be_error_t::ok) {
      be_state->seh_leave_blocks.pop_back();
      be_state->current_landing_pads.pop_back();
      return err;
    }
  }
  be_state->seh_leave_blocks.pop_back();
  be_state->current_landing_pads.pop_back();

  if (fin_func) {
      be_state->cleanup_stack.cleanups.pop_back();
  }

  if (!EDG_LLVM_GET_TERMINATOR(be_state->builder->GetInsertBlock())) {
    be_state->builder->CreateBr(leave_target_bb);
  }

  be_state->builder->SetInsertPoint(leave_target_bb);
  {
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
              index = static_cast<int>(be_state->escaped_allocas.size());
              be_state->escaped_allocas.push_back(orig_alloca);
              be_state->escaped_var_indices[orig_alloca] = index;
          } else {
              index = it->second;
          }
          llvm::Value* recovered = be_state->builder->CreateCall(localrecover_fn, {
              llvm::cast<llvm::Constant>(func), fp, llvm::ConstantInt::get(int32_ty, static_cast<uint64_t>(index))
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
          
          llvm::OperandBundleDef ob{"funclet", cleanup_pad};
          be_state->builder->CreateCall(fin_func, {
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

/**
 * @brief Lowers the Microsoft SEH `_exception_code()` intrinsic.
 * @details Calls `@llvm.eh.exceptioncode` bound to the active `catchpad` token.
 * @param[out] out_val Receives the 32-bit exception code LLVM Value.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if out_val is null.
 */
llvm_gen_be_error_t llvm_lower_eh_exceptioncode(llvm::Value** out_val) noexcept {
  if (!out_val) return llvm_gen_be_error_t::invalid_argument;
  auto it = be_state->builder->GetInsertBlock()->begin();
  llvm::CatchPadInst* cpi = (it != be_state->builder->GetInsertBlock()->end()) ? llvm::dyn_cast<llvm::CatchPadInst>(&*it) : nullptr;
  llvm::Value* token = cpi ? static_cast<llvm::Value*>(cpi) : llvm::ConstantTokenNone::get(*be_state->context);
  llvm::FunctionCallee exc_code = be_state->module->getOrInsertFunction(
      "llvm.eh.exceptioncode", llvm::FunctionType::get(llvm::Type::getInt32Ty(*be_state->context), {token->getType()}, false));
  *out_val = be_state->builder->CreateCall(exc_code, {token});
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers the Microsoft SEH `_exception_info()` intrinsic.
 * @details Retrieves the exception pointers structure from the filter funclet's first argument.
 * @param[out] out_val Receives the exception info pointer LLVM Value.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if out_val is null.
 */
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

/**
 * @brief Lowers a Windows SEH-aware `setjmp` call.
 * @details Passes the current frame address from `@llvm.frameaddress.p0` to `_setjmp`.
 * @param[in] jmp_buf The jump buffer pointer argument.
 * @param[out] out_val Receives the result of the `_setjmp` call.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if pointers are null.
 */
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

/**
 * @brief Lowers a Windows SEH-aware `longjmp` call.
 * @details Emits a call to `longjmp` with the jump buffer and status value.
 * @param[in] jmp_buf The jump buffer pointer argument.
 * @param[in] val The integer status value to restore.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if pointers are null.
 */
llvm_gen_be_error_t llvm_lower_seh_longjmp(llvm::Value* jmp_buf, llvm::Value* val) noexcept {
  if (!jmp_buf || !val) return llvm_gen_be_error_t::invalid_argument;
  llvm::FunctionCallee longjmp_fn = be_state->module->getOrInsertFunction("longjmp",
      llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), {llvm::PointerType::getUnqual(*be_state->context), llvm::Type::getInt32Ty(*be_state->context)}, false));
  be_state->builder->CreateCall(longjmp_fn, {jmp_buf, val});
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a C++ `try` / `catch` block statement (`stmk_try_block`).
 * @details Emits MSVC C++ EH (`__CxxFrameHandler3`, `catchswitch`, `catchpad`, `cleanuppad` with
 *          funclet-bound RAII destructor calls) on Windows MSVC targets, or Itanium C++ ABI EH
 *          (`__gxx_personality_v0`, `landingpad`, `__cxa_begin_catch`, `__cxa_end_catch`) otherwise,
 *          binding any caught exception parameter variable in `be_state->local_vars`.
 * @param[in] stmt Pointer to the EDG try-block statement node.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_try_block_stmt(a_statement_ptr stmt) noexcept {
  if (!stmt || stmt->kind != stmk_try_block) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
  llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Type* int32_ty = llvm::Type::getInt32Ty(*be_state->context);

  bool is_msvc = llvm::Triple(be_state->module->getTargetTriple()).getTriple().find("windows-msvc") != std::string::npos;

  if (is_msvc) {
      llvm::FunctionCallee pers_fn = be_state->module->getOrInsertFunction("__CxxFrameHandler3",
          llvm::FunctionType::get(int32_ty, true));
      func->setPersonalityFn(llvm::cast<llvm::Constant>(pers_fn.getCallee()));

      llvm::BasicBlock* try_bb = llvm::BasicBlock::Create(*be_state->context, "try", func);
      llvm::BasicBlock* catch_dispatch_bb = llvm::BasicBlock::Create(*be_state->context, "catch.dispatch", func);
      llvm::BasicBlock* end_try_bb = llvm::BasicBlock::Create(*be_state->context, "try.end");

      be_state->builder->CreateBr(try_bb);
      be_state->builder->SetInsertPoint(try_bb);

      size_t msvc_try_cleanup_depth = be_state->cleanup_stack.cleanups.size();
      be_state->current_landing_pads.push_back(catch_dispatch_bb);
      be_state->landing_pad_cleanup_depths.push_back(msvc_try_cleanup_depth);
      if (stmt->variant.try_block->statement) {
        llvm_gen_be_error_t err = llvm_lower_statement(stmt->variant.try_block->statement);
        if (err != llvm_gen_be_error_t::ok) return err;
      }
      be_state->current_landing_pads.pop_back();
      be_state->landing_pad_cleanup_depths.pop_back();

      if (!EDG_LLVM_GET_TERMINATOR(be_state->builder->GetInsertBlock())) {
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
        llvm::Value* catch_obj_slot = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty));
        if (h->parameter) {
             llvm_gen_be_error_t err_ti = get_typeinfo_global(h->parameter->type, &typeinfo_ptr);
             if (err_ti != llvm_gen_be_error_t::ok) return err_ti;
             auto it = be_state->local_vars.find(h->parameter);
             if (it != be_state->local_vars.end()) {
                 catch_obj_slot = it->second;
             } else {
                 llvm::Type* param_ty = nullptr;
                 llvm_gen_be_error_t ty_err = get_llvm_type(h->parameter->type, &param_ty);
                 if (ty_err != llvm_gen_be_error_t::ok) return ty_err;
                 if (param_ty->isVoidTy()) param_ty = int8_ptr_ty;
                 llvm::IRBuilder<> entry_builder(&func->getEntryBlock(), func->getEntryBlock().begin());
                 llvm::AllocaInst* catch_alloca = entry_builder.CreateAlloca(
                     param_ty, nullptr, h->parameter->source_corresp.name ? h->parameter->source_corresp.name : "catch.param");
                 be_state->local_vars[h->parameter] = catch_alloca;
                 catch_obj_slot = catch_alloca;
             }
        } else {
             typeinfo_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty)); 
        }

        llvm::CatchPadInst* catch_pad = be_state->builder->CreateCatchPad(
            catch_switch, {typeinfo_ptr, llvm::ConstantInt::get(int32_ty, 0), catch_obj_slot});

        be_state->active_funclet_pads.push_back(catch_pad);
        if (h->statement) {
          llvm_gen_be_error_t err = llvm_lower_statement(h->statement);
          if (err != llvm_gen_be_error_t::ok) return err;
        }
        be_state->active_funclet_pads.pop_back();

        if (!EDG_LLVM_GET_TERMINATOR(be_state->builder->GetInsertBlock())) {
          if (be_state->current_routine_is_ctor_or_dtor) {
            llvm::FunctionType* rethrow_ty = llvm::FunctionType::get(
                llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty, int8_ptr_ty}, false);
            llvm::FunctionCallee rethrow_fn = be_state->module->getOrInsertFunction("_CxxThrowException", rethrow_ty);
            llvm::OperandBundleDef cpad_bundle{"funclet", catch_pad};
            llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty));
            be_state->builder->CreateCall(rethrow_fn, {null_ptr, null_ptr}, {cpad_bundle});
          }
          be_state->builder->CreateCatchRet(catch_pad, end_try_bb);
        }
      }

      if (!be_state->cleanup_stack.cleanups.empty()) {
          llvm::BasicBlock* cleanup_bb = llvm::BasicBlock::Create(*be_state->context, "cleanup", func);
          be_state->builder->SetInsertPoint(cleanup_bb);
          llvm::CleanupPadInst* cleanuppad = be_state->builder->CreateCleanupPad(parent_pad, {});
          
          be_state->active_funclet_pads.push_back(cleanuppad);
          llvm::OperandBundleDef funclet_bundle{"funclet", cleanuppad};
          for (size_t i = be_state->cleanup_stack.cleanups.size(); i > 0; --i) {
              ScopeCleanup& cleanup = be_state->cleanup_stack.cleanups[i - 1];
              if (!cleanup.is_active) continue;
              if (cleanup.is_guard_abort) {
                  llvm::FunctionType* abort_ty = llvm::FunctionType::get(
                      llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty}, false);
                  llvm::FunctionCallee abort_func = be_state->module->getOrInsertFunction("_Init_thread_abort", abort_ty);
                  be_state->builder->CreateCall(abort_func, {cleanup.guard_abort_ptr}, {funclet_bundle});
              } else if (cleanup.finally_func) {
                  llvm::FunctionCallee localaddr_fn = be_state->module->getOrInsertFunction(
                      "llvm.localaddress", llvm::FunctionType::get(int8_ptr_ty, {}, false));
                  llvm::Value* local_addr = be_state->builder->CreateCall(localaddr_fn, {});
                  be_state->builder->CreateCall(cleanup.finally_func, {
                      llvm::ConstantInt::get(llvm::Type::getInt8Ty(*be_state->context), 1),
                      local_addr
                  }, {funclet_bundle});
              } else if (cleanup.dtor_routine) {
                  const char* dtor_name = cleanup.dtor_routine->source_corresp.name ?
                      cleanup.dtor_routine->source_corresp.name : "dtor";
                  llvm::FunctionType* dtor_ty = llvm::FunctionType::get(
                      llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty}, false);
                  llvm::FunctionCallee dtor_callee = be_state->module->getOrInsertFunction(dtor_name, dtor_ty);
                  be_state->builder->CreateCall(dtor_callee, {cleanup.object_ptr}, {funclet_bundle});
              }
          }
          if (be_state->cleanup_stack.cleanups.size() > msvc_try_cleanup_depth) {
              be_state->cleanup_stack.cleanups.resize(msvc_try_cleanup_depth);
          } else {
              be_state->cleanup_stack.cleanups.clear();
          }
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
      llvm::BasicBlock* lpad_dispatch_bb = llvm::BasicBlock::Create(*be_state->context, "lpad.dispatch", func);
      llvm::BasicBlock* end_try_bb = llvm::BasicBlock::Create(*be_state->context, "try.end");

      llvm::PHINode* exc_ptr = nullptr;
      llvm::PHINode* exc_sel = nullptr;
      {
        llvm::IRBuilder<> disp_b(lpad_dispatch_bb);
        exc_ptr = disp_b.CreatePHI(int8_ptr_ty, 2, "exc_ptr");
        exc_sel = disp_b.CreatePHI(int32_ty, 2, "exc_sel");
      }

      be_state->builder->CreateBr(try_bb);
      be_state->builder->SetInsertPoint(try_bb);

      size_t try_cleanup_depth = be_state->cleanup_stack.cleanups.size();
      be_state->current_landing_pads.push_back(lpad_bb);
      be_state->current_lpad_dispatch_bbs.push_back(lpad_dispatch_bb);
      be_state->current_lpad_exc_ptr_phis.push_back(exc_ptr);
      be_state->current_lpad_exc_sel_phis.push_back(exc_sel);
      be_state->current_lpad_handlers.push_back(stmt->variant.try_block->handlers);
      be_state->landing_pad_cleanup_depths.push_back(try_cleanup_depth);
      be_state->lowered_eh_cleanup_barriers.push_back(be_state->active_lowered_eh_cleanup);
      if (stmt->variant.try_block->statement) {
        llvm_gen_be_error_t err = llvm_lower_statement(stmt->variant.try_block->statement);
        if (err != llvm_gen_be_error_t::ok) {
          be_state->current_landing_pads.pop_back();
          be_state->current_lpad_dispatch_bbs.pop_back();
          be_state->current_lpad_exc_ptr_phis.pop_back();
          be_state->current_lpad_exc_sel_phis.pop_back();
          be_state->current_lpad_handlers.pop_back();
          be_state->landing_pad_cleanup_depths.pop_back();
          be_state->lowered_eh_cleanup_barriers.pop_back();
          return err;
        }
      }
      be_state->current_landing_pads.pop_back();
      be_state->current_lpad_dispatch_bbs.pop_back();
      be_state->current_lpad_exc_ptr_phis.pop_back();
      be_state->current_lpad_exc_sel_phis.pop_back();
      be_state->current_lpad_handlers.pop_back();
      be_state->landing_pad_cleanup_depths.pop_back();
      be_state->lowered_eh_cleanup_barriers.pop_back();

      if (!EDG_LLVM_GET_TERMINATOR(be_state->builder->GetInsertBlock())) {
        be_state->builder->CreateBr(end_try_bb);
      }

      bool lpad_has_preds = !lpad_bb->hasNPredecessors(0);
      be_state->builder->SetInsertPoint(lpad_bb);
      llvm::StructType* lpad_ty = llvm::StructType::get(*be_state->context, {int8_ptr_ty, int32_ty});

      for (size_t i = be_state->cleanup_stack.cleanups.size(); i > try_cleanup_depth; --i) {
        ScopeCleanup& cleanup = be_state->cleanup_stack.cleanups[i - 1];
        if (!cleanup.is_active || !lpad_has_preds) continue;
        if (cleanup.is_guard_abort) {
          llvm::FunctionType* abort_ty = llvm::FunctionType::get(
              llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty}, false);
          llvm::FunctionCallee abort_func = be_state->module->getOrInsertFunction("__cxa_guard_abort", abort_ty);
          be_state->builder->CreateCall(abort_func, {cleanup.guard_abort_ptr});
        } else if (cleanup.dtor_routine) {
          const char* dtor_name = cleanup.dtor_routine->source_corresp.name ?
              cleanup.dtor_routine->source_corresp.name : "dtor";
          llvm::FunctionType* dtor_ty = llvm::FunctionType::get(
              llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty}, false);
          llvm::FunctionCallee dtor_callee = be_state->module->getOrInsertFunction(dtor_name, dtor_ty);
          be_state->builder->CreateCall(dtor_callee, {cleanup.object_ptr});
        }
      }
      while (be_state->cleanup_stack.cleanups.size() > try_cleanup_depth) {
        be_state->cleanup_stack.cleanups.pop_back();
      }

      if (lpad_has_preds) {
        llvm::IRBuilder<> lpad_top_b(lpad_bb, lpad_bb->begin());
        llvm::LandingPadInst* lpad = lpad_top_b.CreateLandingPad(lpad_ty, 0);
        for (a_handler_ptr h = stmt->variant.try_block->handlers; h; h = h->next) {
          if (!h->parameter) {
            lpad->addClause(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty)));
          } else {
            llvm::Constant* typeinfo_ptr = nullptr;
            llvm_gen_be_error_t err_ti = get_typeinfo_global(h->parameter->type, &typeinfo_ptr);
            if (err_ti != llvm_gen_be_error_t::ok) return err_ti;
            lpad->addClause(typeinfo_ptr);
          }
        }
        lpad->setCleanup(true);
        llvm::Value* raw_ep = be_state->builder->CreateExtractValue(lpad, 0, "lpad.exc_ptr");
        llvm::Value* raw_es = be_state->builder->CreateExtractValue(lpad, 1, "lpad.exc_sel");
        exc_ptr->addIncoming(raw_ep, lpad_bb);
        exc_sel->addIncoming(raw_es, lpad_bb);
        be_state->builder->CreateBr(lpad_dispatch_bb);
      } else {
        for (a_handler_ptr h = stmt->variant.try_block->handlers; h; h = h->next) {
          if (h->parameter) {
            llvm::Constant* typeinfo_ptr = nullptr;
            llvm_gen_be_error_t err_ti = get_typeinfo_global(h->parameter->type, &typeinfo_ptr);
            if (err_ti != llvm_gen_be_error_t::ok) return err_ti;
          }
        }
        exc_ptr->addIncoming(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty)), lpad_bb);
        exc_sel->addIncoming(llvm::ConstantInt::get(int32_ty, 0), lpad_bb);
        be_state->builder->CreateBr(lpad_dispatch_bb);
      }

      be_state->builder->SetInsertPoint(lpad_dispatch_bb);

      llvm::FunctionCallee begin_catch_fn = be_state->module->getOrInsertFunction("__cxa_begin_catch",
         llvm::FunctionType::get(int8_ptr_ty, {int8_ptr_ty}, false));
      llvm::FunctionCallee end_catch_fn = be_state->module->getOrInsertFunction("__cxa_end_catch",
         llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false));
#if LLVM_VERSION_MAJOR >= 20
      llvm::Function* typeid_fn = EDG_LLVM_GET_INTRINSIC_DECL(
          be_state->module.get(), llvm::Intrinsic::eh_typeid_for, {int8_ptr_ty});
#else
      llvm::Function* typeid_fn = EDG_LLVM_GET_INTRINSIC_DECL(
          be_state->module.get(), llvm::Intrinsic::eh_typeid_for);
#endif

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
        llvm::Value* caught_adj_ptr = be_state->builder->CreateCall(begin_catch_fn, {exc_ptr});
        llvm::Value* param_slot = nullptr;

        ScopeCleanup end_catch_cleanup = {};
        end_catch_cleanup.is_active = true;
        end_catch_cleanup.is_end_catch = true;
        be_state->cleanup_stack.cleanups.push_back(end_catch_cleanup);

        bool lowered_catch_init = (h->parameter && h->dynamic_init && h->dynamic_init->kind == dik_none);
        if (h->parameter) {
          llvm::Type* param_ty = nullptr;
          llvm::Type* raw_param_ty = nullptr;
          llvm_gen_be_error_t ty_err = get_llvm_type(h->parameter->type, &raw_param_ty);
          if (ty_err != llvm_gen_be_error_t::ok) return ty_err;
          param_ty = raw_param_ty->isVoidTy() ? int8_ptr_ty : raw_param_ty;

          auto it = be_state->local_vars.find(h->parameter);
          if (it != be_state->local_vars.end()) {
            param_slot = it->second;
          } else {
            llvm::IRBuilder<> entry_builder(&func->getEntryBlock(), func->getEntryBlock().begin());
            param_slot = entry_builder.CreateAlloca(
                param_ty, nullptr, h->parameter->source_corresp.name ? h->parameter->source_corresp.name : "catch.param");
            be_state->local_vars[h->parameter] = param_slot;
          }

          if (!lowered_catch_init) {
            if (h->dynamic_init && h->dynamic_init->kind == dik_constructor &&
                h->dynamic_init->variant.constructor.ptr) {
              a_routine_ptr copy_ctor = h->dynamic_init->variant.constructor.ptr;
              const char* ctor_name = copy_ctor->source_corresp.name ? copy_ctor->source_corresp.name : "catch_copy_ctor";
              llvm::FunctionType* ctor_ty = llvm::FunctionType::get(
                  llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty, int8_ptr_ty}, false);
              llvm::FunctionCallee ctor_callee = be_state->module->getOrInsertFunction(ctor_name, ctor_ty);
              be_state->builder->CreateCall(ctor_callee, {param_slot, caught_adj_ptr});
            } else if (param_ty->isPointerTy()) {
              be_state->builder->CreateStore(caught_adj_ptr, param_slot);
            } else {
              llvm::Value* loaded_exc = be_state->builder->CreateLoad(param_ty, caught_adj_ptr, "caught.val");
              be_state->builder->CreateStore(loaded_exc, param_slot);
            }

            if (h->dynamic_init && h->dynamic_init->destructor) {
              ScopeCleanup catch_dtor_cleanup = {};
              catch_dtor_cleanup.is_active = true;
              catch_dtor_cleanup.dtor_routine = h->dynamic_init->destructor;
              catch_dtor_cleanup.object_ptr = param_slot;
              be_state->cleanup_stack.cleanups.push_back(catch_dtor_cleanup);
            }
          }
        }

        if (h->statement) {
          llvm::Value* caught_obj_addr = caught_adj_ptr;
          if (h->parameter && h->parameter->type) {
            a_type_ptr catch_ty = skip_typerefs(h->parameter->type);
            if (catch_ty->kind == tk_pointer &&
                catch_ty->variant.pointer.is_reference) {
              catch_ty = catch_ty->variant.pointer.type
                             ? skip_typerefs(catch_ty->variant.pointer.type)
                             : nullptr;
            }
            if (catch_ty && (catch_ty->kind == tk_pointer || catch_ty->kind == tk_nullptr)) {
              llvm::IRBuilder<> entry_b(&func->getEntryBlock(), func->getEntryBlock().begin());
              llvm::AllocaInst* ptr_addr_slot = entry_b.CreateAlloca(int8_ptr_ty, nullptr, "caught.ptr.addr");
              be_state->builder->CreateStore(caught_adj_ptr, ptr_addr_slot);
              caught_obj_addr = ptr_addr_slot;
            }
          }
          llvm::Value* prev_caught_ptr = be_state->active_caught_object_ptr;
          be_state->active_caught_object_ptr = caught_obj_addr;
          llvm_gen_be_error_t err = llvm_lower_statement(h->statement);
          be_state->active_caught_object_ptr = prev_caught_ptr;
          if (err != llvm_gen_be_error_t::ok) return err;
        }

        if (!lowered_catch_init && h->parameter && h->dynamic_init && h->dynamic_init->destructor) {
          be_state->cleanup_stack.cleanups.pop_back();
          if (!EDG_LLVM_GET_TERMINATOR(be_state->builder->GetInsertBlock())) {
            a_routine_ptr catch_dtor = h->dynamic_init->destructor;
            const char* dtor_name = catch_dtor->source_corresp.name ? catch_dtor->source_corresp.name : "catch_dtor";
            llvm::FunctionType* dtor_ty = llvm::FunctionType::get(
                llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty}, false);
            llvm::FunctionCallee dtor_callee = be_state->module->getOrInsertFunction(dtor_name, dtor_ty);
            be_state->builder->CreateCall(dtor_callee, {param_slot});
          }
        }

        // Pop end_catch_cleanup pushed upon entering catch_bb
        be_state->cleanup_stack.cleanups.pop_back();

        if (!EDG_LLVM_GET_TERMINATOR(be_state->builder->GetInsertBlock())) {
          if (be_state->current_routine_is_ctor_or_dtor) {
            llvm::FunctionType* rethrow_ty = llvm::FunctionType::get(
                llvm::Type::getVoidTy(*be_state->context), false);
            llvm::FunctionCallee rethrow_fn = be_state->module->getOrInsertFunction("__cxa_rethrow", rethrow_ty);
            be_state->builder->CreateCall(rethrow_fn);
          }
          be_state->builder->CreateCall(end_catch_fn);
          be_state->builder->CreateBr(end_try_bb);
        }

        current_dispatch_bb = next_dispatch_bb;
      }

      be_state->builder->SetInsertPoint(current_dispatch_bb);
      be_state->builder->CreateBr(resume_bb);

      be_state->builder->SetInsertPoint(resume_bb);
      if (!be_state->current_lpad_dispatch_bbs.empty() &&
          be_state->current_lpad_dispatch_bbs.back()->getParent() == func) {
          be_state->current_lpad_exc_ptr_phis.back()->addIncoming(exc_ptr, resume_bb);
          be_state->current_lpad_exc_sel_phis.back()->addIncoming(exc_sel, resume_bb);
          be_state->builder->CreateBr(be_state->current_lpad_dispatch_bbs.back());
      } else {
          llvm::Value* lpad_agg = llvm::UndefValue::get(lpad_ty);
          lpad_agg = be_state->builder->CreateInsertValue(lpad_agg, exc_ptr, 0);
          lpad_agg = be_state->builder->CreateInsertValue(lpad_agg, exc_sel, 1);
          be_state->builder->CreateResume(lpad_agg);
      }

      func->insert(func->end(), end_try_bb);
      be_state->builder->SetInsertPoint(end_try_bb);
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Retrieves or synthesizes the Itanium ABI `__clang_call_terminate` helper function.
 * @details Synthesizes a `linkonce_odr hidden` `noreturn` `nounwind` function `__clang_call_terminate(ptr)`
 *          that invokes `__cxa_begin_catch` followed by `_ZSt9terminatev` (`std::terminate`) and `unreachable`.
 * @param[out] out_fn Receives the `__clang_call_terminate` `llvm::Function*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_get_or_create_clang_call_terminate(
    llvm::Function** out_fn) noexcept {
    if (!out_fn || !be_state) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    llvm::Function* fn = be_state->module->getFunction("__clang_call_terminate");
    if (!fn) {
        llvm::Type* void_ty = llvm::Type::getVoidTy(*be_state->context);
        llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
        llvm::FunctionType* fn_ty = llvm::FunctionType::get(void_ty, {ptr_ty}, false);
        fn = llvm::Function::Create(
            fn_ty, llvm::GlobalValue::LinkOnceODRLinkage, "__clang_call_terminate", be_state->module.get());
        fn->setVisibility(llvm::GlobalValue::HiddenVisibility);
        fn->addFnAttr(llvm::Attribute::NoReturn);
        fn->addFnAttr(llvm::Attribute::NoUnwind);

        llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(*be_state->context, "entry", fn);
        llvm::IRBuilder<> term_builder(entry_bb);
        llvm::FunctionCallee begin_catch = be_state->module->getOrInsertFunction(
            "__cxa_begin_catch", llvm::FunctionType::get(ptr_ty, {ptr_ty}, false));
        term_builder.CreateCall(begin_catch, {fn->getArg(0)});
        llvm::FunctionCallee std_term = be_state->module->getOrInsertFunction(
            "_ZSt9terminatev", llvm::FunctionType::get(void_ty, false));
        llvm::CallInst* term_call = term_builder.CreateCall(std_term);
        term_call->setDoesNotReturn();
        term_call->setDoesNotThrow();
        term_builder.CreateUnreachable();
    }
    *out_fn = fn;
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Attaches Itanium `noexcept` / `throw(...)` filter clauses and termination/unexpected dispatch onto an existing landing pad.
 * @details Inspects @p routine for a non-`throw_any` exception specification. When present, appends the
 *          appropriate filter clause (`[0 x ptr]` for `noexcept`, or `[N x ptr]` typeinfo list for `throw(...)`)
 *          to @p lpad, emits `__clang_call_terminate` or `__cxa_call_unexpected`, terminates the block with
 *          `unreachable`, and sets `*out_handled = true`.
 * @param[in] routine Optional EDG routine whose exception specification is inspected.
 * @param[in,out] lpad LandingPadInst to attach filter clauses to.
 * @param[out] out_handled Receives `true` if a termination/unexpected sequence was emitted.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_emit_routine_exc_spec_handler(
    a_routine_ptr routine,
    llvm::LandingPadInst* lpad,
    bool* out_handled) noexcept {
    if (!lpad || !out_handled || !be_state) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    *out_handled = false;
    if (!routine || !routine->type) {
        return llvm_gen_be_error_t::ok;
    }
    a_type_ptr rtype = skip_typerefs(routine->type);
    if (rtype->kind != tk_routine || !rtype->variant.routine.extra_info ||
        !rtype->variant.routine.extra_info->exception_specification) {
        return llvm_gen_be_error_t::ok;
    }
    an_exception_specification_ptr spec = rtype->variant.routine.extra_info->exception_specification;
    if (spec->throw_any) {
        return llvm_gen_be_error_t::ok;
    }

    llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    if (spec->is_noexcept) {
        llvm::ArrayType* arr_ty = llvm::ArrayType::get(int8_ptr_ty, 0);
        llvm::Constant* filter_arr = llvm::ConstantArray::get(arr_ty, llvm::ArrayRef<llvm::Constant*>{});
        lpad->addClause(filter_arr);

        llvm::Value* exc_ptr = be_state->builder->CreateExtractValue(lpad, 0, "exc_ptr");
        llvm::Function* clang_term_fn = nullptr;
        LLVM_GEN_BE_CHECK(llvm_get_or_create_clang_call_terminate(&clang_term_fn));
        llvm::CallInst* call_inst = be_state->builder->CreateCall(clang_term_fn, {exc_ptr});
        call_inst->setDoesNotReturn();
        call_inst->setDoesNotThrow();
        be_state->builder->CreateUnreachable();
    } else {
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
    *out_handled = true;
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Appends the innermost enclosing `try` block's `catch` typeinfo clauses onto an inner chained `LandingPadInst`.
 * @details Ensures `__gxx_personality_v0` computes matching `@llvm.eh.typeid.for` selector values when an inner
 *          cleanup landing pad branches to `be_state->current_lpad_dispatch_bbs.back()`.
 * @param[in,out] lpad Chained inner `LandingPadInst` to populate with `catch` clauses.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_append_enclosing_try_clauses(
    llvm::LandingPadInst* lpad) noexcept {
    if (!lpad || !be_state) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    llvm::PointerType* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    if (!be_state->current_lpad_handlers.empty() &&
        be_state->current_lpad_handlers.back() != nullptr) {
        for (a_handler_ptr eh_h = be_state->current_lpad_handlers.back(); eh_h; eh_h = eh_h->next) {
            if (!eh_h->parameter) {
                lpad->addClause(llvm::ConstantPointerNull::get(ptr_ty));
            } else {
                llvm::Constant* ti = nullptr;
                LLVM_GEN_BE_CHECK(get_typeinfo_global(eh_h->parameter->type, &ti));
                lpad->addClause(ti);
            }
        }
    } else {
        lpad->addClause(llvm::ConstantPointerNull::get(ptr_ty));
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits a landing pad for `noexcept` or dynamic exception specifications (`throw(T1, T2)`).
 * @details Generates filter clauses on the landing pad and dispatches to `std::terminate`
 *          or `__cxa_call_unexpected` on specification violation.
 * @param[in] routine Pointer to the EDG routine node with an exception specification.
 * @param[in] lpad_bb Pointer to the landing pad basic block to populate.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_emit_exception_specification_lpad(
    a_routine_ptr routine,
    llvm::BasicBlock* lpad_bb) noexcept {
    if (!routine || !lpad_bb) return llvm_gen_be_error_t::invalid_argument;
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());

    llvm::Function* func = lpad_bb->getParent();
    if (!func) return llvm_gen_be_error_t::invalid_argument;

    be_state->builder->SetInsertPoint(lpad_bb);
    llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::Type* int32_ty = llvm::Type::getInt32Ty(*be_state->context);

    bool is_msvc = llvm::Triple(be_state->module->getTargetTriple()).getTriple().find("windows-msvc") != std::string::npos;

    if (is_msvc) {
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

    bool handled = false;
    return llvm_emit_routine_exc_spec_handler(routine, lpad, &handled);
}

/**
 * @brief Creates a chained cleanup landing pad block for active `ScopeCleanup` entries.
 * @details Emits an Itanium `landingpad cleanup` + `resume` or MSVC `cleanuppad` + `cleanupret`
 *          basic block that executes active destructors and guard aborts down to @p target_cleanup_depth.
 * @param[in] target_cleanup_depth Cleanup stack depth to unwind down to.
 * @param[in] next_lpad_bb Optional enclosing landing pad / unwind destination block.
 * @param[out] out_cleanup_lpad_bb Receives the created cleanup landing pad basic block.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_emit_cleanup_landing_pad(
    size_t target_cleanup_depth,
    llvm::BasicBlock* next_lpad_bb,
    llvm::BasicBlock** out_cleanup_lpad_bb) noexcept {
    if (!out_cleanup_lpad_bb) return llvm_gen_be_error_t::invalid_argument;
    *out_cleanup_lpad_bb = nullptr;
    if (!be_state || !be_state->builder || !be_state->builder->GetInsertBlock()) {
        return llvm_gen_be_error_t::invalid_argument;
    }

    llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
    llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::Type* int32_ty = llvm::Type::getInt32Ty(*be_state->context);
    bool is_msvc = llvm::Triple(be_state->module->getTargetTriple()).getTriple().find("windows-msvc") != std::string::npos;

    llvm::BasicBlock* cleanup_bb = llvm::BasicBlock::Create(*be_state->context, "eh.cleanup.lpad", func);
    llvm::IRBuilder<>::InsertPoint saved_ip = be_state->builder->saveIP();
    be_state->builder->SetInsertPoint(cleanup_bb);

    if (is_msvc) {
        if (!func->hasPersonalityFn()) {
            llvm::FunctionCallee pers_fn = be_state->module->getOrInsertFunction(
                "__CxxFrameHandler3", llvm::FunctionType::get(int32_ty, true));
            func->setPersonalityFn(llvm::cast<llvm::Constant>(pers_fn.getCallee()));
        }
        llvm::Value* parent_pad = be_state->active_funclet_pads.empty() ?
            static_cast<llvm::Value*>(llvm::ConstantTokenNone::get(*be_state->context)) :
            be_state->active_funclet_pads.back();
        llvm::CleanupPadInst* cpad = be_state->builder->CreateCleanupPad(parent_pad, {});
        llvm::OperandBundleDef funclet_bundle{"funclet", cpad};
        for (size_t i = be_state->cleanup_stack.cleanups.size(); i > target_cleanup_depth; --i) {
            ScopeCleanup& cleanup = be_state->cleanup_stack.cleanups[i - 1];
            if (!cleanup.is_active) continue;
            if (cleanup.is_guard_abort) {
                llvm::FunctionType* abort_ty = llvm::FunctionType::get(
                    llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty}, false);
                llvm::FunctionCallee abort_func = be_state->module->getOrInsertFunction("_Init_thread_abort", abort_ty);
                be_state->builder->CreateCall(abort_func, {cleanup.guard_abort_ptr}, {funclet_bundle});
            } else if (cleanup.finally_func) {
                llvm::FunctionCallee localaddr_fn = be_state->module->getOrInsertFunction(
                    "llvm.localaddress", llvm::FunctionType::get(int8_ptr_ty, {}, false));
                llvm::Value* local_addr = be_state->builder->CreateCall(localaddr_fn, {});
                be_state->builder->CreateCall(cleanup.finally_func, {
                    llvm::ConstantInt::get(llvm::Type::getInt8Ty(*be_state->context), 1),
                    local_addr
                }, {funclet_bundle});
            } else if (cleanup.dtor_routine) {
                const char* dtor_name = cleanup.dtor_routine->source_corresp.name ?
                    cleanup.dtor_routine->source_corresp.name : "dtor";
                llvm::FunctionType* dtor_ty = llvm::FunctionType::get(
                    llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty}, false);
                llvm::FunctionCallee dtor_callee = be_state->module->getOrInsertFunction(dtor_name, dtor_ty);
                be_state->builder->CreateCall(dtor_callee, {cleanup.object_ptr}, {funclet_bundle});
            }
        }
        be_state->builder->CreateCleanupRet(cpad, next_lpad_bb);
    } else {
        if (!func->hasPersonalityFn()) {
            llvm::FunctionCallee pers_fn = be_state->module->getOrInsertFunction(
                "__gxx_personality_v0", llvm::FunctionType::get(int32_ty, true));
            func->setPersonalityFn(llvm::cast<llvm::Constant>(pers_fn.getCallee()));
        }
        llvm::StructType* lpad_ty = llvm::StructType::get(*be_state->context, {int8_ptr_ty, int32_ty});
        llvm::LandingPadInst* lpad = be_state->builder->CreateLandingPad(lpad_ty, 0);
        lpad->setCleanup(true);
        unsigned pending_end_catches = 0;
        for (size_t i = be_state->cleanup_stack.cleanups.size(); i > target_cleanup_depth; --i) {
            ScopeCleanup& cleanup = be_state->cleanup_stack.cleanups[i - 1];
            if (!cleanup.is_active) continue;
            if (cleanup.is_guard_abort) {
                llvm::FunctionType* abort_ty = llvm::FunctionType::get(
                    llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty}, false);
                llvm::FunctionCallee abort_func = be_state->module->getOrInsertFunction("__cxa_guard_abort", abort_ty);
                be_state->builder->CreateCall(abort_func, {cleanup.guard_abort_ptr});
            } else if (cleanup.is_end_catch) {
                ++pending_end_catches;
            } else if (cleanup.dtor_routine) {
                const char* dtor_name = cleanup.dtor_routine->source_corresp.name ?
                    cleanup.dtor_routine->source_corresp.name : "dtor";
                llvm::FunctionType* dtor_ty = llvm::FunctionType::get(
                    llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty}, false);
                llvm::FunctionCallee dtor_callee = be_state->module->getOrInsertFunction(dtor_name, dtor_ty);
                be_state->builder->CreateCall(dtor_callee, {cleanup.object_ptr});
            }
        }
        a_dynamic_init_ptr barrier_dip = be_state->lowered_eh_cleanup_barriers.empty()
                                             ? nullptr
                                             : be_state->lowered_eh_cleanup_barriers.back();
        for (a_dynamic_init_ptr dip = be_state->active_lowered_eh_cleanup;
             dip != nullptr && dip != barrier_dip; ) {
            if (dip->destructor && dip->destructor->source_corresp.name) {
                llvm::Value* target_obj_ptr = nullptr;
                if (dip->variable) {
                    auto vit = be_state->local_vars.find(dip->variable);
                    if (vit != be_state->local_vars.end()) {
                        target_obj_ptr = vit->second;
                    }
                } else if (dip->is_constructor_init && be_state->current_function_scope) {
                    a_variable_ptr this_param = be_state->current_function_scope->variant.routine.parameters;
                    if (this_param && this_param->type) {
                        auto tit = be_state->local_vars.find(this_param);
                        a_type_ptr ptr_tp = skip_typerefs(this_param->type);
                        a_type_ptr cls_tp = (ptr_tp->kind == tk_pointer && ptr_tp->variant.pointer.type)
                                                ? skip_typerefs(ptr_tp->variant.pointer.type)
                                                : nullptr;
                        if (tit != be_state->local_vars.end() && tit->second && cls_tp &&
                            (cls_tp->kind == tk_class || cls_tp->kind == tk_struct)) {
                            auto extract_dtor_class_ty = [](a_routine_ptr dr) noexcept -> a_type_ptr {
                                if (!dr->type) return nullptr;
                                a_type_ptr rt = skip_typerefs(dr->type);
                                if (rt->kind != tk_routine || !rt->variant.routine.extra_info) return nullptr;
                                a_param_type_ptr p0 = rt->variant.routine.extra_info->param_type_list;
                                if (!p0 || !p0->type) return nullptr;
                                a_type_ptr pt = skip_typerefs(p0->type);
                                return (pt->kind == tk_pointer && pt->variant.pointer.type)
                                           ? skip_typerefs(pt->variant.pointer.type)
                                           : nullptr;
                            };
                            a_type_ptr target_sub_cls = extract_dtor_class_ty(dip->destructor);
                            std::vector<a_dynamic_init_ptr> same_cls_dips;
                            an_object_lifetime_ptr fn_lt = be_state->current_function_scope->lifetime;
                            for (a_dynamic_init_ptr cur_d = fn_lt ? fn_lt->destructions : dip;
                                 cur_d != nullptr; cur_d = cur_d->next_in_destruction_list) {
                                if (cur_d->is_constructor_init && cur_d->destructor &&
                                    extract_dtor_class_ty(cur_d->destructor) == target_sub_cls) {
                                    same_cls_dips.push_back(cur_d);
                                }
                            }
                            std::vector<uint64_t> sub_offsets;
                            auto* extra = cls_tp->variant.class_struct_union.extra_info;
                            bool is_c2 = be_state->current_edg_routine &&
                                         be_state->current_edg_routine->source_corresp.name &&
                                         llvm::StringRef(be_state->current_edg_routine->source_corresp.name).contains("C2E");
                            if (extra) {
                                if (!is_c2) {
                                    for (a_base_class_ptr bc = extra->base_classes; bc != nullptr; bc = bc->next) {
                                        a_type_ptr bt = bc->type ? skip_typerefs(bc->type) : nullptr;
                                        if (bc->is_virtual && bt == target_sub_cls) {
                                            sub_offsets.push_back(static_cast<uint64_t>(bc->offset));
                                        }
                                    }
                                }
                                for (a_base_class_ptr bc = extra->direct_base_classes; bc != nullptr; bc = bc->next_direct) {
                                    a_type_ptr bt = bc->type ? skip_typerefs(bc->type) : nullptr;
                                    if (!bc->is_virtual && bt == target_sub_cls) {
                                        sub_offsets.push_back(static_cast<uint64_t>(bc->offset));
                                    }
                                }
                            }
                            for (a_field_ptr f = cls_tp->variant.class_struct_union.field_list; f != nullptr; f = f->next) {
                                a_type_ptr ft = f->type ? skip_typerefs(f->type) : nullptr;
                                if (!f->is_bit_field && ft == target_sub_cls) {
                                    sub_offsets.push_back(static_cast<uint64_t>(f->offset));
                                }
                            }
                            for (size_t idx = 0; idx < same_cls_dips.size(); ++idx) {
                                if (same_cls_dips[idx] == dip) {
                                    size_t rev_idx = same_cls_dips.size() - 1 - idx;
                                    uint64_t off = (rev_idx < sub_offsets.size()) ? sub_offsets[rev_idx] : 0;
                                    llvm::Value* this_val = be_state->builder->CreateLoad(int8_ptr_ty, tit->second, "eh.this");
                                    target_obj_ptr = (off == 0)
                                                         ? this_val
                                                         : be_state->builder->CreateGEP(
                                                               llvm::Type::getInt8Ty(*be_state->context),
                                                               this_val,
                                                               llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), off));
                                    break;
                                }
                            }
                        }
                    }
                }
                if (target_obj_ptr) {
                    std::string dname = dip->destructor->source_corresp.name;
                    size_t d2_pos = dname.find("D2E");
                    llvm::Function* dfn = nullptr;
                    if (!dip->is_constructor_init && d2_pos != std::string::npos) {
                        std::string d1_name = dname;
                        d1_name[d2_pos + 1] = '1';
                        dfn = be_state->module->getFunction(d1_name);
                        if (dfn) dname = d1_name;
                    }
                    if (!dfn) {
                        LLVM_GEN_BE_CHECK(llvm_get_or_declare_routine(dip->destructor, &dfn));
                    }
                    if (dfn && dfn->arg_size() == 1) {
                        be_state->builder->CreateCall(dfn, {target_obj_ptr});
                    } else if (!dfn) {
                        llvm::FunctionType* dtor_ty = llvm::FunctionType::get(
                            llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty}, false);
                        llvm::FunctionCallee dtor_callee = be_state->module->getOrInsertFunction(dname, dtor_ty);
                        be_state->builder->CreateCall(dtor_callee, {target_obj_ptr});
                    }
                }
            }
            if (dip->next_in_destruction_list) {
                dip = dip->next_in_destruction_list;
            } else if (dip->lifetime) {
                dip = dip->lifetime->parent_destruction_sublist;
            } else {
                dip = nullptr;
            }
        }
        while (pending_end_catches > 0) {
            llvm::FunctionCallee end_catch_fn = be_state->module->getOrInsertFunction(
                "__cxa_end_catch",
                llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false));
            be_state->builder->CreateCall(end_catch_fn);
            --pending_end_catches;
        }
        if (next_lpad_bb && !be_state->current_lpad_dispatch_bbs.empty() &&
            be_state->current_lpad_dispatch_bbs.back()->getParent() == func) {
            LLVM_GEN_BE_CHECK(llvm_append_enclosing_try_clauses(lpad));
            llvm::Value* ep = be_state->builder->CreateExtractValue(lpad, 0);
            llvm::Value* es = be_state->builder->CreateExtractValue(lpad, 1);
            llvm::BasicBlock* cur_b = be_state->builder->GetInsertBlock();
            be_state->current_lpad_exc_ptr_phis.back()->addIncoming(ep, cur_b);
            be_state->current_lpad_exc_sel_phis.back()->addIncoming(es, cur_b);
            be_state->builder->CreateBr(be_state->current_lpad_dispatch_bbs.back());
        } else {
            bool spec_handled = false;
            if (next_lpad_bb) {
                LLVM_GEN_BE_CHECK(llvm_emit_routine_exc_spec_handler(
                    be_state->current_edg_routine, lpad, &spec_handled));
            }
            if (!spec_handled) {
                if (next_lpad_bb && next_lpad_bb->empty()) {
                    be_state->builder->CreateBr(next_lpad_bb);
                } else {
                    be_state->builder->CreateResume(lpad);
                }
            }
        }
    }

    be_state->builder->restoreIP(saved_ip);
    *out_cleanup_lpad_bb = cleanup_bb;
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Retrieves or lazily materializes the unwind destination basic block for the current cleanup and landing pad state.
 * @details If active `ScopeCleanup` entries exist above the innermost enclosing landing pad depth,
 *          emits a chained cleanup landing pad via `llvm_emit_cleanup_landing_pad`; otherwise returns
 *          the innermost enclosing landing pad (or `nullptr` if none).
 * @param[out] out_unwind_bb Receives the unwind destination basic block, or `nullptr` if no unwind handler is active.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_get_or_create_current_unwind_dest(
    llvm::BasicBlock** out_unwind_bb) noexcept {
    if (!out_unwind_bb) return llvm_gen_be_error_t::invalid_argument;
    *out_unwind_bb = nullptr;
    if (!be_state) return llvm_gen_be_error_t::invalid_argument;
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());

    llvm::BasicBlock* enclosing_lpad = be_state->current_landing_pads.empty()
                                           ? nullptr
                                           : be_state->current_landing_pads.back();
    size_t base_depth = (!be_state->landing_pad_cleanup_depths.empty() &&
                         be_state->landing_pad_cleanup_depths.back() <= be_state->cleanup_stack.cleanups.size())
                            ? be_state->landing_pad_cleanup_depths.back()
                            : 0;
    a_dynamic_init_ptr barrier_dip = be_state->lowered_eh_cleanup_barriers.empty()
                                         ? nullptr
                                         : be_state->lowered_eh_cleanup_barriers.back();

    bool has_active_cleanup = (be_state->active_lowered_eh_cleanup != barrier_dip);
    for (size_t i = be_state->cleanup_stack.cleanups.size(); !has_active_cleanup && i > base_depth; --i) {
        const ScopeCleanup& c = be_state->cleanup_stack.cleanups[i - 1];
        if (c.is_active && c.saved_stack_ptr == nullptr) {
            has_active_cleanup = true;
            break;
        }
    }

    if (has_active_cleanup) {
        return llvm_emit_cleanup_landing_pad(base_depth, enclosing_lpad, out_unwind_bb);
    }

    *out_unwind_bb = enclosing_lpad;
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a Microsoft SEH `__leave` statement inside a `__try` block.
 * @details Emits an unconditional branch to the innermost active `__try` leave continuation block
 *          stored in `be_state->seh_leave_blocks`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if no SEH `__try` block is active.
 */
llvm_gen_be_error_t llvm_lower_seh_leave(void) noexcept {
    if (!be_state || !be_state->builder || be_state->seh_leave_blocks.empty()) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    llvm::BasicBlock* target_bb = be_state->seh_leave_blocks.back();
    be_state->builder->CreateBr(target_bb);
    llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* dead_bb = llvm::BasicBlock::Create(*be_state->context, "ms.after.leave", func);
    be_state->builder->SetInsertPoint(dead_bb);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Retrieves or synthesizes the MSVC C++ EH `_ThrowInfo` (`_TI...`) global for a thrown type.
 * @details Emits `_CT??_R0...` (`_s__CatchableType`) descriptors for @p thrown_type and its public
 *          base classes, the `_CTA...` (`_s__CatchableTypeArray`) table, and the top-level `_TI...`
 *          (`_s__ThrowInfo`) descriptor referencing @p dtor_fn.
 * @param[in] thrown_type The EDG type of the thrown exception object.
 * @param[in] dtor_fn Optional destructor `llvm::Constant*` (or null pointer constant).
 * @param[out] out_throw_info Receives the `_TI...` `llvm::Constant*` global pointer.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_get_or_create_msvc_throw_info(
    a_type_ptr thrown_type,
    llvm::Constant* dtor_fn,
    llvm::Constant** out_throw_info) noexcept {
    if (!thrown_type || !out_throw_info || !be_state) {
        return llvm_gen_be_error_t::invalid_argument;
    }

    llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
    llvm::Constant* null_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty));
    if (!dtor_fn) {
        dtor_fn = null_ptr;
    }

    llvm::Constant* type_desc_const = nullptr;
    LLVM_GEN_BE_CHECK(get_typeinfo_global(thrown_type, &type_desc_const));

    uint32_t type_size = thrown_type->size ? static_cast<uint32_t>(thrown_type->size) : 1u;
    const char* raw_tname = thrown_type->source_corresp.name
                                ? thrown_type->source_corresp.name
                                : "type";
    std::string ct_name = std::string("_CT??_R0?AU") + raw_tname + "@@8";
    std::string cta_name = std::string("_CTA1?AU") + raw_tname + "@@";
    std::string ti_name = std::string("_TI1?AU") + raw_tname + "@@";

    llvm::GlobalVariable* ti_gv = be_state->module->getNamedGlobal(ti_name);
    if (!ti_gv) {
        llvm::StructType* ct_ty = llvm::StructType::get(
            *be_state->context,
            {i32_ty, int8_ptr_ty, i32_ty, i32_ty, i32_ty, i32_ty, int8_ptr_ty});
        llvm::Constant* ct_init = llvm::ConstantStruct::get(
            ct_ty,
            {llvm::ConstantInt::get(i32_ty, 0),
             type_desc_const,
             llvm::ConstantInt::get(i32_ty, 0),
             llvm::ConstantInt::get(i32_ty, static_cast<uint64_t>(static_cast<uint32_t>(-1))),
             llvm::ConstantInt::get(i32_ty, 0),
             llvm::ConstantInt::get(i32_ty, static_cast<uint64_t>(type_size)),
             null_ptr});
        llvm::GlobalVariable* ct_gv = nullptr;
        LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), ct_ty, true, llvm::GlobalValue::LinkOnceODRLinkage, ct_init, ct_name, &ct_gv));

        std::vector<llvm::Constant*> ct_list = {ct_gv};
        if (thrown_type->kind == tk_class || thrown_type->kind == tk_struct) {
            if (thrown_type->variant.class_struct_union.extra_info) {
                for (a_base_class_ptr bc = thrown_type->variant.class_struct_union.extra_info->base_classes;
                     bc != nullptr; bc = bc->next) {
                    if (bc->type) {
                        llvm::Constant* base_ti = nullptr;
                        LLVM_GEN_BE_CHECK(get_typeinfo_global(bc->type, &base_ti));
                        const char* bname = bc->type->source_corresp.name ? bc->type->source_corresp.name : "base";
                        std::string b_ct_name = std::string("_CT??_R0?AU") + bname + "@@8";
                        llvm::Constant* b_ct_init = llvm::ConstantStruct::get(
                            ct_ty,
                            {llvm::ConstantInt::get(i32_ty, 0),
                             base_ti,
                             llvm::ConstantInt::get(i32_ty, static_cast<uint32_t>(bc->offset)),
                             llvm::ConstantInt::get(i32_ty, static_cast<uint64_t>(static_cast<uint32_t>(-1))),
                             llvm::ConstantInt::get(i32_ty, 0),
                             llvm::ConstantInt::get(i32_ty, static_cast<uint32_t>(bc->type->size ? bc->type->size : 1)),
                             null_ptr});
                        llvm::GlobalVariable* b_ct_gv = nullptr;
                        LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), ct_ty, true, llvm::GlobalValue::LinkOnceODRLinkage, b_ct_init, b_ct_name, &b_ct_gv));
                        ct_list.push_back(b_ct_gv);
                    }
                }
            }
        }

        llvm::ArrayType* cta_arr_ty = llvm::ArrayType::get(int8_ptr_ty, ct_list.size());
        llvm::StructType* cta_ty = llvm::StructType::get(*be_state->context, {i32_ty, cta_arr_ty});
        llvm::Constant* cta_init = llvm::ConstantStruct::get(
            cta_ty,
            {llvm::ConstantInt::get(i32_ty, static_cast<uint32_t>(ct_list.size())),
             llvm::ConstantArray::get(cta_arr_ty, ct_list)});
        llvm::GlobalVariable* cta_gv = nullptr;
        LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), cta_ty, true, llvm::GlobalValue::LinkOnceODRLinkage, cta_init, cta_name, &cta_gv));

        llvm::StructType* ti_ty = llvm::StructType::get(
            *be_state->context, {i32_ty, int8_ptr_ty, int8_ptr_ty, int8_ptr_ty});
        llvm::Constant* ti_init = llvm::ConstantStruct::get(
            ti_ty,
            {llvm::ConstantInt::get(i32_ty, 0),
             dtor_fn,
             null_ptr,
             cta_gv});
        LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), ti_ty, true, llvm::GlobalValue::LinkOnceODRLinkage, ti_init, ti_name, &ti_gv));
    }
    *out_throw_info = ti_gv;
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Attaches MSVC `/EHa` asynchronous SEH attributes (`seh-asynch` and `non-call-exceptions`) to a function.
 * @details Configures @p func for hardware trap unwinding under `/EHa` asynchronous exception mode.
 * @param[in,out] func Pointer to the target `llvm::Function`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if @p func is null.
 */
llvm_gen_be_error_t llvm_enable_async_seh_attributes(llvm::Function* func) noexcept {
    if (!func) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    func->addFnAttr("seh-asynch", "true");
    func->addFnAttr("non-call-exceptions", "true");
    return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
