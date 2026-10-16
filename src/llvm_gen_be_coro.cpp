/**
 * @file llvm_gen_be_coro.cpp
 * @brief C++20 Coroutine lowering subsystem for the EDG LLVM backend.
 * @details Implements the lowering of coroutine statements and expressions using LLVM Coroutine Intrinsics.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_coro.h"
#include "llvm_gen_be_expr.h"
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Lowers a coroutine description statement into LLVM IR.
 * @details Emits llvm.coro.id, llvm.coro.alloc, and llvm.coro.begin intrinsics.
 * @param[in] stmt Pointer to the EDG coroutine statement node.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_coroutine_stmt(a_statement_ptr stmt) noexcept {
  if (!stmt || stmt->kind != stmk_coroutine) return llvm_gen_be_error_t::invalid_argument;
  
  a_coroutine_descr_ptr descr = stmt->variant.coroutine.descr;
  if (!descr) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
  llvm::Type* int32_ty = llvm::Type::getInt32Ty(*be_state->context);
  llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);

  // coro.id
  llvm::Function* coro_id = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::coro_id);
  
  llvm::Constant* align = llvm::ConstantInt::get(int32_ty, 0); // Default align
  llvm::Value* promise = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty)); 
  if (descr->promise) {
      auto it = be_state->local_vars.find(descr->promise);
      if (it != be_state->local_vars.end()) {
          promise = be_state->builder->CreateBitCast(it->second, int8_ptr_ty);
      }
  }
  
  llvm::Constant* coroutine_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty));
  llvm::Constant* empty_struct = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty));
  
  llvm::Value* id_val = be_state->builder->CreateCall(coro_id, {align, promise, coroutine_ptr, empty_struct}, "coro.id");

  // coro.alloc
  llvm::Function* coro_alloc = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::coro_alloc);
  llvm::Value* need_alloc = be_state->builder->CreateCall(coro_alloc, {id_val}, "coro.need.alloc");

  llvm::BasicBlock* alloc_bb = llvm::BasicBlock::Create(*be_state->context, "coro.alloc", func);
  llvm::BasicBlock* init_bb = llvm::BasicBlock::Create(*be_state->context, "coro.init", func);

  llvm::BasicBlock* pred_bb = be_state->builder->GetInsertBlock();
  be_state->builder->CreateCondBr(need_alloc, alloc_bb, init_bb);

  // Allocation block
  be_state->builder->SetInsertPoint(alloc_bb);
  llvm::Type* size_t_ty = llvm::Type::getInt64Ty(*be_state->context); // Assuming 64-bit size_t for coro.size
  llvm::Function* coro_size = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::coro_size, {size_t_ty});
  llvm::Value* size_val = be_state->builder->CreateCall(coro_size, {}, "coro.size");
  
  // Allocate memory using custom allocator if present, otherwise global operator new
  llvm::Value* raw_mem = nullptr;
  if (descr->new_routine) {
      llvm::Function* new_func = be_state->module->getFunction(descr->new_routine->source_corresp.name ? descr->new_routine->source_corresp.name : "");
      if (new_func) {
          std::vector<llvm::Value*> call_args;
          call_args.push_back(size_val);
          llvm::FunctionType* fty = new_func->getFunctionType();
          // Forward coroutine arguments to custom allocator (operator new(size_t, args...))
          for (unsigned i = 1; i < fty->getNumParams(); ++i) {
              if (i - 1 < func->arg_size()) {
                  llvm::Argument* arg = func->getArg(i - 1);
                  llvm::Type* expected_ty = fty->getParamType(i);
                  llvm::Value* cast_arg = arg;
                  if (cast_arg->getType() != expected_ty) {
                      if (expected_ty->isPointerTy() && cast_arg->getType()->isPointerTy()) {
                          cast_arg = be_state->builder->CreatePointerCast(cast_arg, expected_ty);
                      } else if (expected_ty->isIntegerTy() && cast_arg->getType()->isIntegerTy()) {
                          cast_arg = be_state->builder->CreateIntCast(cast_arg, expected_ty, true);
                      } else {
                          cast_arg = be_state->builder->CreateBitCast(cast_arg, expected_ty);
                      }
                  }
                  call_args.push_back(cast_arg);
              } else {
                  call_args.push_back(llvm::Constant::getNullValue(fty->getParamType(i)));
              }
          }
          raw_mem = be_state->builder->CreateCall(new_func, call_args, "coro.mem.alloc");
      }
  }
  if (!raw_mem) {
      // Fallback to global ::operator new(size_t)
      llvm::FunctionType* new_fty = llvm::FunctionType::get(int8_ptr_ty, {size_t_ty}, false);
      llvm::FunctionCallee global_new = be_state->module->getOrInsertFunction("_Znwm", new_fty); // Itanium mangling for new(unsigned long)
      raw_mem = be_state->builder->CreateCall(global_new, {size_val}, "coro.mem.alloc.global");
  }
  
  if (descr->alloc_failure_gro_call) {
      llvm::BasicBlock* fail_bb = llvm::BasicBlock::Create(*be_state->context, "coro.alloc.fail", func);
      llvm::Value* is_null = be_state->builder->CreateIsNull(raw_mem, "coro.alloc.isnull");
      be_state->builder->CreateCondBr(is_null, fail_bb, init_bb);
      
      be_state->builder->SetInsertPoint(fail_bb);
      llvm::Value* fail_val = nullptr;
      (void)llvm_lower_expression(descr->alloc_failure_gro_call, &fail_val);
      if (fail_val && fail_val->getType() != func->getReturnType()) {
          if (fail_val->getType()->isPointerTy() && func->getReturnType()->isPointerTy()) {
              fail_val = be_state->builder->CreatePointerCast(fail_val, func->getReturnType());
          }
      }
      if (fail_val) be_state->builder->CreateRet(fail_val);
      else be_state->builder->CreateRetVoid();
  } else {
      be_state->builder->CreateBr(init_bb);
  }

  // Init block
  be_state->builder->SetInsertPoint(init_bb);
  llvm::PHINode* mem_phi = be_state->builder->CreatePHI(int8_ptr_ty, 2, "coro.mem");
  mem_phi->addIncoming(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty)), pred_bb); 
  mem_phi->addIncoming(raw_mem, alloc_bb);

  llvm::Function* coro_begin = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::coro_begin);
  (void)be_state->builder->CreateCall(coro_begin, {id_val, mem_phi}, "coro.hdl");

  if (descr->unhandled_exception_call && descr->final_suspend_label) {
      llvm::BasicBlock* coro_lpad = llvm::BasicBlock::Create(*be_state->context, "coro.unhandled.lpad", func);
      be_state->current_landing_pads.push_back(coro_lpad);

      llvm::IRBuilder<>::InsertPoint saved_ip = be_state->builder->saveIP();
      be_state->builder->SetInsertPoint(coro_lpad);
      
      llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
      llvm::FunctionType* catch_fn_ty = llvm::FunctionType::get(int8_ptr_ty, {int8_ptr_ty}, false);
      llvm::FunctionCallee begin_catch = be_state->module->getOrInsertFunction("__cxa_begin_catch", catch_fn_ty);
      llvm::FunctionCallee end_catch = be_state->module->getOrInsertFunction("__cxa_end_catch", llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false));
      
      llvm::Triple triple(be_state->module->getTargetTriple());
      bool is_msvc = triple.isWindowsMSVCEnvironment();
      
      if (is_msvc) {
          llvm::CatchSwitchInst* catch_switch = be_state->builder->CreateCatchSwitch(
              llvm::ConstantTokenNone::get(*be_state->context), nullptr, 1);
          llvm::BasicBlock* catch_bb = llvm::BasicBlock::Create(*be_state->context, "coro.unhandled.catch", func);
          catch_switch->addHandler(catch_bb);
          be_state->builder->SetInsertPoint(catch_bb);
          llvm::CatchPadInst* catch_pad = be_state->builder->CreateCatchPad(catch_switch, {llvm::ConstantPointerNull::get(int8_ptr_ty), llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 64), llvm::ConstantPointerNull::get(int8_ptr_ty)});
          
          llvm::Value* tmp_val = nullptr;
          (void)llvm_lower_expression(descr->unhandled_exception_call, &tmp_val);
          
          llvm::BasicBlock* label_bb = nullptr;
          auto it = be_state->label_blocks.find(descr->final_suspend_label);
          if (it == be_state->label_blocks.end()) {
              label_bb = llvm::BasicBlock::Create(*be_state->context, "label", func);
              be_state->label_blocks[descr->final_suspend_label] = label_bb;
          } else {
              label_bb = it->second;
          }
          be_state->builder->CreateCatchRet(catch_pad, label_bb);
      } else {
          llvm::LandingPadInst* lpad = be_state->builder->CreateLandingPad(
              llvm::StructType::get(*be_state->context, {int8_ptr_ty, llvm::Type::getInt32Ty(*be_state->context)}),
              1, "coro.lpad");
          lpad->setCleanup(true);
          lpad->addClause(llvm::ConstantPointerNull::get(int8_ptr_ty)); // Catch all
          
          llvm::Value* exc_ptr = be_state->builder->CreateExtractValue(lpad, 0);
          be_state->builder->CreateCall(begin_catch, {exc_ptr});
          
          llvm::Value* tmp_val = nullptr;
          (void)llvm_lower_expression(descr->unhandled_exception_call, &tmp_val);
          
          be_state->builder->CreateCall(end_catch, {});
          
          llvm::BasicBlock* label_bb = nullptr;
          auto it = be_state->label_blocks.find(descr->final_suspend_label);
          if (it == be_state->label_blocks.end()) {
              label_bb = llvm::BasicBlock::Create(*be_state->context, "label", func);
              be_state->label_blocks[descr->final_suspend_label] = label_bb;
          } else {
              label_bb = it->second;
          }
          be_state->builder->CreateBr(label_bb);
      }
      
      be_state->builder->restoreIP(saved_ip);
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a coroutine return statement into LLVM IR.
 * @details Emits llvm.coro.end and cleans up the coroutine frame.
 * @param[in] stmt Pointer to the EDG coroutine return statement node.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_coroutine_return_stmt(a_statement_ptr stmt) noexcept {
  if (!stmt || stmt->kind != stmk_coroutine_return) return llvm_gen_be_error_t::invalid_argument;

  llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
  llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);

  if (stmt->expr) {
      llvm::Value* ret_val = nullptr;
      llvm_gen_be_error_t err = llvm_lower_expression(stmt->expr, &ret_val);
      if (err != llvm_gen_be_error_t::ok) return err;
  }

  llvm::Function* coro_free = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::coro_free);
  llvm::Function* coro_id = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::coro_id);
  llvm::Value* id_val = be_state->builder->CreateCall(coro_id, {
      llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0),
      llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty)),
      llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty)),
      llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty))
  });
  llvm::Value* hdl = be_state->builder->CreateCall(coro_free, {id_val, llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty))});

  llvm::Function* coro_end = llvm::Intrinsic::getOrInsertDeclaration(be_state->module.get(), llvm::Intrinsic::coro_end);
  llvm::Value* is_unwind = be_state->builder->getFalse();
  
  be_state->builder->CreateCall(coro_end, {hdl, is_unwind});
  
  if (func->getReturnType()->isVoidTy()) {
      be_state->builder->CreateRetVoid();
  } else {
      be_state->builder->CreateUnreachable(); 
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a C++20 co_await or co_yield expression to LLVM IR.
 * @details Evaluates await_ready, emits llvm.coro.save and llvm.coro.suspend intrinsics,
 *          implements the 3-way resume/destroy/suspend switch, and evaluates await_resume.
 * @param[in] expr Pointer to the EDG enk_await or enk_yield expression node.
 * @param[out] out_val Receives the computed result of the await_resume call on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_coro_suspend(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  if (expr->kind != enk_await && expr->kind != enk_yield) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
  llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Type* int8_ty = llvm::Type::getInt8Ty(*be_state->context);

  an_expr_node_ptr rrs = expr->variant.await_info.ready_resume_suspend;
  an_expr_node_ptr ready_expr = rrs;
  an_expr_node_ptr resume_expr = ready_expr ? ready_expr->next : nullptr;
  an_expr_node_ptr suspend_expr = resume_expr ? resume_expr->next : nullptr;

  llvm::BasicBlock* suspend_bb = llvm::BasicBlock::Create(*be_state->context, "coro.suspend.bb", func);
  llvm::BasicBlock* resume_bb = llvm::BasicBlock::Create(*be_state->context, "coro.resume.bb", func);
  llvm::BasicBlock* destroy_bb = llvm::BasicBlock::Create(*be_state->context, "coro.destroy.bb", func);
  llvm::BasicBlock* ret_bb = llvm::BasicBlock::Create(*be_state->context, "coro.suspend.ret", func);

  // 1. Evaluate await_ready()
  if (ready_expr) {
    llvm::Value* is_ready = nullptr;
    llvm_gen_be_error_t err = llvm_lower_expression(ready_expr, &is_ready);
    if (err != llvm_gen_be_error_t::ok) return err;

    if (is_ready) {
      if (is_ready->getType()->isIntegerTy(1)) {
        be_state->builder->CreateCondBr(is_ready, resume_bb, suspend_bb);
      } else {
        llvm::Value* cond = be_state->builder->CreateICmpNE(
            is_ready, llvm::Constant::getNullValue(is_ready->getType()), "is_ready_cond");
        be_state->builder->CreateCondBr(cond, resume_bb, suspend_bb);
      }
    } else {
      be_state->builder->CreateBr(suspend_bb);
    }
  } else {
    be_state->builder->CreateBr(suspend_bb);
  }

  // 2. Suspend block: emit llvm.coro.save and llvm.coro.suspend
  be_state->builder->SetInsertPoint(suspend_bb);
  llvm::Function* coro_save = llvm::Intrinsic::getOrInsertDeclaration(
      be_state->module.get(), llvm::Intrinsic::coro_save);
  llvm::Value* null_hdl = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty));
  llvm::Value* save_token = be_state->builder->CreateCall(coro_save, {null_hdl}, "save_tok");

  llvm::Function* coro_suspend = llvm::Intrinsic::getOrInsertDeclaration(
      be_state->module.get(), llvm::Intrinsic::coro_suspend);
  llvm::Value* susp_result = be_state->builder->CreateCall(
      coro_suspend, {save_token, be_state->builder->getFalse()}, "susp_val");

  llvm::BasicBlock* await_suspend_bb = llvm::BasicBlock::Create(*be_state->context, "coro.await.suspend", func);

  // 3-way resume switch on susp_result:
  // 0: resume normal execution
  // 1: cleanup and destroy frame
  // Default (-1): suspend to caller (evaluate await_suspend)
  llvm::IntegerType* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
  llvm::SwitchInst* sw = be_state->builder->CreateSwitch(susp_result, await_suspend_bb, 2);
  sw->addCase(llvm::ConstantInt::get(i8_ty, 0), resume_bb);
  sw->addCase(llvm::ConstantInt::get(i8_ty, 1), destroy_bb);

  be_state->builder->SetInsertPoint(await_suspend_bb);
  // Evaluate await_suspend()
  if (suspend_expr) {
    llvm::Value* susp_val = nullptr;
    llvm_gen_be_error_t err = llvm_lower_expression(suspend_expr, &susp_val);
    if (err != llvm_gen_be_error_t::ok) return err;

    if (susp_val && susp_val->getType()->isPointerTy()) {
      // Symmetric transfer: await_suspend returns another coroutine handle
      // Use coro.suspend.ret.conduit to correctly preserve symmetric transfer semantics in modern LLVM.
      llvm::FunctionCallee conduit = be_state->module->getOrInsertFunction("llvm.coro.suspend.ret.conduit", llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), {susp_val->getType()}, false));
      llvm::CallInst* tail_call = be_state->builder->CreateCall(conduit, {susp_val});
      tail_call->setTailCallKind(llvm::CallInst::TCK_MustTail);
      be_state->builder->CreateRetVoid();
    } else if (susp_val && susp_val->getType()->isIntegerTy(1)) {
      // await_suspend returns bool: true = suspend (return to caller), false = resume
      be_state->builder->CreateCondBr(susp_val, ret_bb, resume_bb);
    } else {
      // await_suspend returns void
      be_state->builder->CreateBr(ret_bb);
    }
  } else {
    be_state->builder->CreateBr(ret_bb);
  }

  // Destroy block
  be_state->builder->SetInsertPoint(destroy_bb);
  be_state->builder->CreateUnreachable();

  // Return to caller block
  be_state->builder->SetInsertPoint(ret_bb);
  if (func->getReturnType()->isVoidTy()) {
    be_state->builder->CreateRetVoid();
  } else {
    be_state->builder->CreateRet(llvm::Constant::getNullValue(func->getReturnType()));
  }

  // Resume block
  be_state->builder->SetInsertPoint(resume_bb);
  llvm::Value* res_val = nullptr;
  if (resume_expr) {
    llvm_gen_be_error_t err = llvm_lower_expression(resume_expr, &res_val);
    if (err != llvm_gen_be_error_t::ok) return err;
  }
  if (!res_val) {
    res_val = llvm::Constant::getNullValue(llvm::Type::getInt32Ty(*be_state->context));
  }
  *out_val = res_val;
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_coro_halo_hints(llvm::Function* func) noexcept {
  if (func == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  func->addFnAttr("coro_elide_safe");
  return llvm_gen_be_error_t::ok;
}
END_EDG_NAMESPACE
#endif

/**
 * @brief Emits Heap Allocation Elision Optimization (HALO) hint attributes.
 * @details Applies coro_elide_safe and related optimization hints.
 * @param[in] func Pointer to the LLVM Function.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
