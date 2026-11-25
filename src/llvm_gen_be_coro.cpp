/**
 * @file llvm_gen_be_coro.cpp
 * @ingroup llvm_gen_be_coro
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
 * @brief Emits frame-promoted parameter allocas and copies in the coroutine entry ramp.
 * @details Allocates coroutine frame slots for `this_param_copy` and `parameter_copies` before
 *          `llvm.coro.begin` and stores incoming function arguments into them so they survive across suspends.
 * @param[in] descr Pointer to the EDG coroutine descriptor.
 * @param[in] func Pointer to the enclosing LLVM coroutine function.
 * @param[out] out_copied_count Receives the number of parameter copies emitted into the coroutine frame.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_emit_coro_parameter_copies(
    a_coroutine_descr_ptr descr,
    llvm::Function* func,
    uint32_t* out_copied_count) noexcept {
  if (!descr || !func || !out_copied_count) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  unsigned param_idx = (func->arg_size() > 0 && func->getArg(0)->hasStructRetAttr()) ? 1U : 0U;
  if (descr->this_param_copy && param_idx < func->arg_size()) {
    llvm::Argument* this_arg = func->getArg(param_idx++);
    llvm::AllocaInst* this_copy_alloca = be_state->builder->CreateAlloca(
        this_arg->getType(), nullptr, "coro.this.copy");
    be_state->builder->CreateStore(this_arg, this_copy_alloca);
    be_state->local_vars[descr->this_param_copy] = this_copy_alloca;
  }
  for (a_variable_ptr p_copy = descr->parameter_copies; p_copy != nullptr; p_copy = p_copy->next) {
    if (p_copy == descr->promise || p_copy == descr->init_await_resume || p_copy == descr->handle) {
      break;
    }
    if (param_idx < func->arg_size()) {
      llvm::Argument* fn_arg = func->getArg(param_idx++);
      llvm::AllocaInst* param_copy_alloca = be_state->builder->CreateAlloca(
          fn_arg->getType(), nullptr, "coro.param.copy");
      be_state->builder->CreateStore(fn_arg, param_copy_alloca);
      be_state->local_vars[p_copy] = param_copy_alloca;
    }
  }
  *out_copied_count = param_idx;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers coroutine frame allocation via custom `promise_type::operator new` or global `operator new`.
 * @details Forwards coroutine function parameters to overloaded `operator new(size_t, Args...)` when present,
 *          or falls back to target-specific aligned/unaligned global `operator new`.
 * @param[in] descr Pointer to the EDG coroutine descriptor.
 * @param[in] func Pointer to the enclosing LLVM coroutine function.
 * @param[in] size_val Lowered `@llvm.coro.size` frame byte size value.
 * @param[in] prom_align Alignment requirement of the coroutine promise (0 if default-aligned).
 * @param[out] out_alloc_ptr Receives the allocated coroutine frame memory pointer.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_coro_promise_alloc(
    a_coroutine_descr_ptr descr,
    llvm::Function* func,
    llvm::Value* size_val,
    uint32_t prom_align,
    llvm::Value** out_alloc_ptr) noexcept {
  if (!descr || !func || !size_val || !out_alloc_ptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Type* size_t_ty = size_val->getType();
  llvm::Value* raw_mem = nullptr;

  if (descr->new_routine) {
    llvm::Function* new_func = nullptr;
    LLVM_GEN_BE_CHECK(llvm_get_or_declare_routine(descr->new_routine, &new_func));
    if (new_func) {
      std::vector<llvm::Value*> call_args;
      call_args.push_back(size_val);
      llvm::FunctionType* fty = new_func->getFunctionType();
      unsigned sret_off = (func->arg_size() > 0 && func->getArg(0)->hasStructRetAttr()) ? 1U : 0U;
      for (unsigned i = 1; i < fty->getNumParams(); ++i) {
        if (sret_off + i - 1 < func->arg_size()) {
          llvm::Argument* arg = func->getArg(sret_off + i - 1);
          llvm::Type* expected_ty = fty->getParamType(i);
          llvm::Value* cast_arg = arg;
          if (cast_arg->getType() != expected_ty) {
            if (expected_ty->isIntegerTy() && cast_arg->getType()->isIntegerTy()) {
              cast_arg = be_state->builder->CreateIntCast(cast_arg, expected_ty, true);
            } else {
              cast_arg = be_state->builder->CreateBitOrPointerCast(cast_arg, expected_ty);
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
    llvm::Triple target_triple = llvm::Triple(be_state->module->getTargetTriple());
    bool is_msvc_target = target_triple.isWindowsMSVCEnvironment();
    bool is_32bit_target = target_triple.isArch32Bit();
    if (prom_align > 16) {
      llvm::Function* coro_align = EDG_LLVM_GET_INTRINSIC_DECL(
          be_state->module.get(), llvm::Intrinsic::coro_align, {size_t_ty});
      llvm::Value* align_val = be_state->builder->CreateCall(coro_align, {}, "coro.align");
      const char* aligned_new_name =
          is_msvc_target ? "??2@YAPEAX_KW4align_val_t@std@@@Z" : "_ZnwmSt11align_val_t";
      llvm::FunctionType* aligned_new_fty = llvm::FunctionType::get(
          int8_ptr_ty, {size_t_ty, size_t_ty}, false);
      llvm::FunctionCallee global_aligned_new =
          be_state->module->getOrInsertFunction(aligned_new_name, aligned_new_fty);
      raw_mem = be_state->builder->CreateCall(
          global_aligned_new, {size_val, align_val}, "coro.mem.alloc.aligned");
    } else {
      const char* global_new_name = "_Znwm";
      if (is_msvc_target) {
        global_new_name = is_32bit_target ? "??2@YAPAXI@Z" : "??2@YAPEAX_K@Z";
      } else if (is_32bit_target) {
        global_new_name = "_Znwj";
      }
      llvm::FunctionType* new_fty = llvm::FunctionType::get(int8_ptr_ty, {size_t_ty}, false);
      llvm::FunctionCallee global_new =
          be_state->module->getOrInsertFunction(global_new_name, new_fty);
      raw_mem = be_state->builder->CreateCall(global_new, {size_val}, "coro.mem.alloc.global");
    }
  }
  *out_alloc_ptr = raw_mem;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits the coroutine unhandled-exception landing pad and catch handler.
 * @details Creates a landing pad (Itanium) or `catchswitch`/`catchpad` (MSVC) that invokes
 *          `promise.unhandled_exception()` and branches to the `final_suspend` label.
 * @param[in] descr Pointer to the EDG coroutine descriptor.
 * @param[in] func Pointer to the enclosing LLVM coroutine function.
 * @param[out] out_lpad_bb Receives the created unhandled-exception landing pad basic block.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_emit_coro_unhandled_exception_landing_pad(
    a_coroutine_descr_ptr descr,
    llvm::Function* func,
    llvm::BasicBlock** out_lpad_bb) noexcept {
  if (!descr || !func || !out_lpad_bb || !descr->unhandled_exception_call || !descr->final_suspend_label) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());

  llvm::BasicBlock* coro_lpad = llvm::BasicBlock::Create(*be_state->context, "coro.unhandled.lpad", func);
  be_state->current_landing_pads.push_back(coro_lpad);
  *out_lpad_bb = coro_lpad;

  llvm::IRBuilder<>::InsertPoint saved_ip = be_state->builder->saveIP();
  be_state->builder->SetInsertPoint(coro_lpad);

  llvm::PointerType* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::FunctionType* catch_fn_ty = llvm::FunctionType::get(int8_ptr_ty, {int8_ptr_ty}, false);
  llvm::FunctionCallee begin_catch = be_state->module->getOrInsertFunction("__cxa_begin_catch", catch_fn_ty);
  llvm::FunctionCallee end_catch = be_state->module->getOrInsertFunction(
      "__cxa_end_catch", llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false));

  llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
  bool is_msvc = triple.isWindowsMSVCEnvironment();

  if (is_msvc) {
    llvm::CatchSwitchInst* catch_switch = be_state->builder->CreateCatchSwitch(
        llvm::ConstantTokenNone::get(*be_state->context), nullptr, 1);
    llvm::BasicBlock* catch_bb = llvm::BasicBlock::Create(*be_state->context, "coro.unhandled.catch", func);
    catch_switch->addHandler(catch_bb);
    be_state->builder->SetInsertPoint(catch_bb);
    llvm::CatchPadInst* catch_pad = be_state->builder->CreateCatchPad(
        catch_switch,
        {llvm::ConstantPointerNull::get(int8_ptr_ty),
         llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 64),
         llvm::ConstantPointerNull::get(int8_ptr_ty)});

    llvm::Value* tmp_val = nullptr;
    llvm_gen_be_error_t uh_err = llvm_lower_expression(descr->unhandled_exception_call, &tmp_val);
    if (uh_err != llvm_gen_be_error_t::ok) {
      be_state->builder->restoreIP(saved_ip);
      return uh_err;
    }

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
    lpad->addClause(llvm::ConstantPointerNull::get(int8_ptr_ty));

    llvm::Value* exc_ptr = be_state->builder->CreateExtractValue(lpad, 0);
    be_state->builder->CreateCall(begin_catch, {exc_ptr});

    llvm::Value* tmp_val = nullptr;
    llvm_gen_be_error_t uh_err = llvm_lower_expression(descr->unhandled_exception_call, &tmp_val);
    if (uh_err != llvm_gen_be_error_t::ok) {
      be_state->builder->restoreIP(saved_ip);
      return uh_err;
    }

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
  return llvm_gen_be_error_t::ok;
}

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
  func->addFnAttr(llvm::Attribute::PresplitCoroutine);
  llvm::Type* int32_ty = llvm::Type::getInt32Ty(*be_state->context);
  llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);

  // coro.id
  llvm::Function* coro_id = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::coro_id);
  
  uint32_t prom_align = 0;
  if (descr->promise && descr->promise->type && descr->promise->type->alignment > 16) {
      prom_align = static_cast<uint32_t>(descr->promise->type->alignment);
  }
  llvm::Constant* align = llvm::ConstantInt::get(int32_ty, prom_align);
  llvm::Value* promise = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty)); 
  llvm::Value* promise_slot = nullptr;
  if (descr->promise) {
      auto it = be_state->local_vars.find(descr->promise);
      if (it == be_state->local_vars.end()) {
          llvm::Type* prom_ty = int32_ty;
          if (descr->promise->type) {
              LLVM_GEN_BE_CHECK(get_llvm_type(descr->promise->type, &prom_ty));
          }
          llvm::AllocaInst* prom_alloca = be_state->builder->CreateAlloca(prom_ty, nullptr, "coro.promise");
          be_state->local_vars[descr->promise] = prom_alloca;
          promise_slot = prom_alloca;
          promise = be_state->builder->CreateBitCast(prom_alloca, int8_ptr_ty);
      } else {
          promise_slot = it->second;
          promise = be_state->builder->CreateBitCast(it->second, int8_ptr_ty);
      }
  }

  // Copy parameters and implicit `this` into coroutine frame allocas
  uint32_t copied_params = 0;
  LLVM_GEN_BE_CHECK(llvm_emit_coro_parameter_copies(descr, func, &copied_params));
  
  llvm::Constant* coroutine_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty));
  llvm::Constant* empty_struct = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty));
  
  llvm::Value* id_val = be_state->builder->CreateCall(coro_id, {align, promise, coroutine_ptr, empty_struct}, "coro.id");

  // coro.alloc
  llvm::Function* coro_alloc = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::coro_alloc);
  llvm::Value* need_alloc = be_state->builder->CreateCall(coro_alloc, {id_val}, "coro.need.alloc");

  llvm::BasicBlock* alloc_bb = llvm::BasicBlock::Create(*be_state->context, "coro.alloc", func);
  llvm::BasicBlock* init_bb = llvm::BasicBlock::Create(*be_state->context, "coro.init", func);

  llvm::Type* size_t_ty = be_state->module->getDataLayout().getIntPtrType(*be_state->context);
  llvm::Function* coro_size = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::coro_size, {size_t_ty});
  llvm::Value* size_val = be_state->builder->CreateCall(coro_size, {}, "coro.size");

  llvm::BasicBlock* pred_bb = be_state->builder->GetInsertBlock();
  be_state->builder->CreateCondBr(need_alloc, alloc_bb, init_bb);

  // Allocation block
  be_state->builder->SetInsertPoint(alloc_bb);
  
  llvm::Value* raw_mem = nullptr;
  LLVM_GEN_BE_CHECK(llvm_lower_coro_promise_alloc(descr, func, size_val, prom_align, &raw_mem));
  
  if (descr->alloc_failure_gro_call) {
      llvm::BasicBlock* fail_bb = llvm::BasicBlock::Create(*be_state->context, "coro.alloc.fail", func);
      llvm::Value* is_null = be_state->builder->CreateIsNull(raw_mem, "coro.alloc.isnull");
      be_state->builder->CreateCondBr(is_null, fail_bb, init_bb);
      
      be_state->builder->SetInsertPoint(fail_bb);
      bool fail_has_sret = (func->arg_size() > 0 && func->getArg(0)->hasStructRetAttr());
      llvm::Value* prev_fail_dest = be_state->active_prvalue_dest_ptr;
      bool prev_fail_in_place = be_state->prvalue_constructed_in_place;
      if (fail_has_sret) {
          be_state->active_prvalue_dest_ptr = func->getArg(0);
          be_state->prvalue_constructed_in_place = false;
      }
      llvm::Value* fail_val = nullptr;
      llvm_gen_be_error_t fail_err = llvm_lower_expression(descr->alloc_failure_gro_call, &fail_val);
      bool fail_in_sret = fail_has_sret && be_state->prvalue_constructed_in_place;
      be_state->active_prvalue_dest_ptr = prev_fail_dest;
      be_state->prvalue_constructed_in_place = prev_fail_in_place;
      LLVM_GEN_BE_CHECK(fail_err);
      if (fail_has_sret) {
          if (fail_val && !fail_in_sret && !fail_val->getType()->isVoidTy()) {
              be_state->builder->CreateStore(fail_val, func->getArg(0));
          }
          be_state->builder->CreateRetVoid();
      } else if (fail_val && !func->getReturnType()->isVoidTy()) {
          llvm::Type* fn_ret_ty = func->getReturnType();
          if (fail_val->getType() != fn_ret_ty) {
              if (fail_val->getType()->isAggregateType() || fn_ret_ty->isAggregateType()) {
                  llvm::AllocaInst* fail_slot = be_state->builder->CreateAlloca(fn_ret_ty, nullptr, "coro.fail.slot");
                  be_state->builder->CreateStore(llvm::Constant::getNullValue(fn_ret_ty), fail_slot);
                  be_state->builder->CreateStore(fail_val, fail_slot);
                  fail_val = be_state->builder->CreateLoad(fn_ret_ty, fail_slot, "coro.fail.ret");
              } else {
                  fail_val = be_state->builder->CreateBitOrPointerCast(fail_val, fn_ret_ty);
              }
          }
          be_state->builder->CreateRet(fail_val);
      } else {
          be_state->builder->CreateRetVoid();
      }
  } else {
      be_state->builder->CreateBr(init_bb);
  }

  // Init block
  be_state->builder->SetInsertPoint(init_bb);
  llvm::PHINode* mem_phi = be_state->builder->CreatePHI(int8_ptr_ty, 2, "coro.mem");
  mem_phi->addIncoming(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty)), pred_bb); 
  mem_phi->addIncoming(raw_mem, alloc_bb);

  llvm::Function* coro_begin = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::coro_begin);
  llvm::Value* coro_hdl = be_state->builder->CreateCall(coro_begin, {id_val, mem_phi}, "coro.hdl");
  be_state->current_coro_handle = coro_hdl;
  be_state->current_coro_promise = promise;
  be_state->current_coro_gro_slot = nullptr;
  be_state->current_coro_final_suspend_bb = nullptr;
  be_state->current_coro_final_await_expr = descr->body_generated ? descr->final_suspend_call : nullptr;
  LLVM_GEN_BE_CHECK(llvm_lower_coro_halo_hints(func));

  if (promise_slot && descr->promise->type) {
    llvm::Type* prom_ty = nullptr;
    LLVM_GEN_BE_CHECK(get_llvm_type(descr->promise->type, &prom_ty));
    if (prom_ty->isSized()) {
      be_state->builder->CreateStore(llvm::Constant::getNullValue(prom_ty), promise_slot);
    }
  }
  if (descr->handle) {
    llvm::Value* hdl_slot = nullptr;
    auto hit = be_state->local_vars.find(descr->handle);
    if (hit != be_state->local_vars.end()) {
      hdl_slot = hit->second;
    } else if (descr->handle->type) {
      llvm::Type* hdl_ty = nullptr;
      LLVM_GEN_BE_CHECK(get_llvm_type(descr->handle->type, &hdl_ty));
      llvm::IRBuilder<> entry_b(&func->getEntryBlock(), func->getEntryBlock().begin());
      hdl_slot = entry_b.CreateAlloca(hdl_ty, nullptr, "coro.descr.hdl");
      be_state->local_vars[descr->handle] = hdl_slot;
    }
    if (hdl_slot) {
      be_state->builder->CreateStore(coro_hdl, hdl_slot);
    }
  }

  if (descr->get_return_object_call) {
    bool has_sret = (func->arg_size() > 0 && func->getArg(0)->hasStructRetAttr());
    llvm::Value* prev_dest = be_state->active_prvalue_dest_ptr;
    bool prev_in_place = be_state->prvalue_constructed_in_place;
    if (has_sret) {
      be_state->active_prvalue_dest_ptr = func->getArg(0);
      be_state->prvalue_constructed_in_place = false;
    }
    llvm::Value* gro_val = nullptr;
    llvm_gen_be_error_t gro_err = llvm_lower_expression(descr->get_return_object_call, &gro_val);
    bool constructed_in_sret = has_sret && be_state->prvalue_constructed_in_place;
    be_state->active_prvalue_dest_ptr = prev_dest;
    be_state->prvalue_constructed_in_place = prev_in_place;
    LLVM_GEN_BE_CHECK(gro_err);
    if (has_sret && gro_val && !constructed_in_sret && !gro_val->getType()->isVoidTy()) {
      be_state->builder->CreateStore(gro_val, func->getArg(0));
    } else if (has_sret && !constructed_in_sret) {
      an_expr_node_ptr gro_call = descr->get_return_object_call;
      while (gro_call->kind == enk_operation && gro_call->variant.operation.kind == eok_comma &&
             gro_call->variant.operation.operands && gro_call->variant.operation.operands->next) {
        gro_call = gro_call->variant.operation.operands->next;
      }
      if (gro_call->kind == enk_operation &&
          gro_call->variant.operation.kind == eok_call &&
          gro_call->variant.operation.operands && gro_call->variant.operation.operands->next) {
        an_expr_node_ptr sret_arg_node = gro_call->variant.operation.operands->next;
        llvm::Value* gro_tmp_ptr = nullptr;
        LLVM_GEN_BE_CHECK(llvm_lower_expression(sret_arg_node, &gro_tmp_ptr));
        llvm::Type* sret_elem_ty = func->getParamStructRetType(0);
        if (gro_tmp_ptr && gro_tmp_ptr->getType()->isPointerTy() &&
            gro_tmp_ptr != func->getArg(0) && sret_elem_ty->isSized()) {
          const llvm::DataLayout& dl = be_state->module->getDataLayout();
          uint64_t copy_sz = dl.getTypeAllocSize(sret_elem_ty);
          llvm::MaybeAlign sret_align = dl.getABITypeAlign(sret_elem_ty);
          be_state->builder->CreateMemCpy(func->getArg(0), sret_align, gro_tmp_ptr, sret_align, copy_sz);
        }
      }
    } else if (gro_val && !func->getReturnType()->isVoidTy()) {
      llvm::Type* fn_ret_ty = func->getReturnType();
      llvm::AllocaInst* gro_slot = be_state->builder->CreateAlloca(fn_ret_ty, nullptr, "coro.gro.slot");
      be_state->builder->CreateStore(llvm::Constant::getNullValue(fn_ret_ty), gro_slot);
      if (gro_val->getType() == fn_ret_ty) {
        be_state->builder->CreateStore(gro_val, gro_slot);
      } else if (gro_val->getType()->isAggregateType() || fn_ret_ty->isAggregateType()) {
        be_state->builder->CreateStore(gro_val, gro_slot);
      } else if (gro_val->getType()->isPointerTy() && fn_ret_ty->isIntegerTy()) {
        be_state->builder->CreateStore(be_state->builder->CreatePtrToInt(gro_val, fn_ret_ty), gro_slot);
      } else if (gro_val->getType()->isIntegerTy() && fn_ret_ty->isPointerTy()) {
        be_state->builder->CreateStore(be_state->builder->CreateIntToPtr(gro_val, fn_ret_ty), gro_slot);
      } else if (gro_val->getType()->isIntegerTy() && fn_ret_ty->isIntegerTy()) {
        be_state->builder->CreateStore(be_state->builder->CreateIntCast(gro_val, fn_ret_ty, false), gro_slot);
      } else {
        be_state->builder->CreateStore(be_state->builder->CreateBitOrPointerCast(gro_val, fn_ret_ty), gro_slot);
      }
      be_state->current_coro_gro_slot = gro_slot;
    }
  }
  llvm::BasicBlock* cleanup_bb = llvm::BasicBlock::Create(*be_state->context, "coro.cleanup", func);
  llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, "coro.end.bb", func);
  be_state->current_coro_cleanup_bb = cleanup_bb;
  be_state->current_coro_end_bb = end_bb;

  if (!descr->body_generated && descr->initial_suspend_call) {
    llvm::Value* init_susp_val = nullptr;
    LLVM_GEN_BE_CHECK(llvm_lower_expression(descr->initial_suspend_call, &init_susp_val));
  }
  if (descr->final_suspend_label) {
    auto it = be_state->label_blocks.find(descr->final_suspend_label);
    if (it == be_state->label_blocks.end()) {
      llvm::BasicBlock* fbb = descr->body_generated
                                  ? llvm::BasicBlock::Create(*be_state->context, "coro.final.suspend")
                                  : llvm::BasicBlock::Create(*be_state->context, "coro.final.suspend", func);
      be_state->current_coro_final_suspend_bb = fbb;
      be_state->label_blocks[descr->final_suspend_label] = fbb;
    } else {
      be_state->current_coro_final_suspend_bb = it->second;
    }
  }
  if (!descr->body_generated && descr->final_suspend_call && descr->final_suspend_label) {
    llvm::BasicBlock* final_bb = be_state->current_coro_final_suspend_bb;
    be_state->current_coro_suspend_bb = final_bb;
    llvm::IRBuilder<>::InsertPoint saved_fin_ip = be_state->builder->saveIP();
    be_state->builder->SetInsertPoint(final_bb);
    llvm::Value* fin_susp_val = nullptr;
    llvm_gen_be_error_t fin_err = llvm_lower_expression(descr->final_suspend_call, &fin_susp_val);
    be_state->builder->restoreIP(saved_fin_ip);
    be_state->current_coro_suspend_bb = nullptr;
    if (fin_err != llvm_gen_be_error_t::ok) return fin_err;
  }

  {
    llvm::IRBuilder<>::InsertPoint saved_cleanup_ip = be_state->builder->saveIP();
    be_state->builder->SetInsertPoint(cleanup_bb);
    llvm::Function* coro_free_fn = EDG_LLVM_GET_INTRINSIC_DECL(
        be_state->module.get(), llvm::Intrinsic::coro_free);
    llvm::Value* mem_to_free = be_state->builder->CreateCall(coro_free_fn, {id_val, coro_hdl}, "coro.mem.free");
    llvm::Value* need_free = be_state->builder->CreateIsNotNull(mem_to_free, "coro.need.free");
    llvm::BasicBlock* dealloc_bb = llvm::BasicBlock::Create(*be_state->context, "coro.dealloc", func);
    be_state->builder->CreateCondBr(need_free, dealloc_bb, end_bb);

    be_state->builder->SetInsertPoint(dealloc_bb);
    llvm::Function* del_fn = nullptr;
    if (descr->delete_routine) {
      LLVM_GEN_BE_CHECK(llvm_get_or_declare_routine(descr->delete_routine, &del_fn));
    }
    if (del_fn && del_fn->arg_size() >= 1) {
      std::vector<llvm::Value*> del_args = {mem_to_free};
      if (del_fn->arg_size() >= 2) {
        del_args.push_back(size_val);
      }
      if (del_fn->arg_size() >= 3) {
        llvm::Function* coro_align_fn = EDG_LLVM_GET_INTRINSIC_DECL(
            be_state->module.get(), llvm::Intrinsic::coro_align, {size_t_ty});
        del_args.push_back(be_state->builder->CreateCall(coro_align_fn, {}, "coro.del.align"));
      }
      be_state->builder->CreateCall(del_fn, del_args);
    } else if (!descr->delete_routine && prom_align > 16) {
      llvm::Triple target_triple = llvm::Triple(be_state->module->getTargetTriple());
      const char* aligned_del_name = target_triple.isWindowsMSVCEnvironment()
                                         ? "??3@YAXPEAX_KW4align_val_t@std@@@Z"
                                         : "_ZdlPvmSt11align_val_t";
      llvm::Function* coro_align_fn = EDG_LLVM_GET_INTRINSIC_DECL(
          be_state->module.get(), llvm::Intrinsic::coro_align, {size_t_ty});
      llvm::Value* align_val = be_state->builder->CreateCall(coro_align_fn, {}, "coro.del.align.global");
      llvm::FunctionType* aligned_del_fty = llvm::FunctionType::get(
          llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty, size_t_ty, size_t_ty}, false);
      llvm::FunctionCallee global_aligned_del =
          be_state->module->getOrInsertFunction(aligned_del_name, aligned_del_fty);
      be_state->builder->CreateCall(global_aligned_del, {mem_to_free, size_val, align_val});
    } else if (!descr->delete_routine) {
      llvm::Triple target_triple = llvm::Triple(be_state->module->getTargetTriple());
      const char* global_del_name = target_triple.isWindowsMSVCEnvironment() ? "??3@YAXPEAX@Z" : "_ZdlPv";
      llvm::FunctionType* del_fty = llvm::FunctionType::get(
          llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty}, false);
      llvm::FunctionCallee global_del = be_state->module->getOrInsertFunction(global_del_name, del_fty);
      be_state->builder->CreateCall(global_del, {mem_to_free});
    }
    be_state->builder->CreateBr(end_bb);

    be_state->builder->SetInsertPoint(end_bb);
    llvm::Function* coro_end_fn = EDG_LLVM_GET_INTRINSIC_DECL(
        be_state->module.get(), llvm::Intrinsic::coro_end);
#if LLVM_VERSION_MAJOR >= 17
    be_state->builder->CreateCall(
        coro_end_fn,
        {coro_hdl, be_state->builder->getFalse(), llvm::ConstantTokenNone::get(*be_state->context)});
#else
    be_state->builder->CreateCall(coro_end_fn, {coro_hdl, be_state->builder->getFalse()});
#endif
    if (func->getReturnType()->isVoidTy()) {
      be_state->builder->CreateRetVoid();
    } else if (be_state->current_coro_gro_slot) {
      llvm::Value* gro_end_ret = be_state->builder->CreateLoad(
          func->getReturnType(), be_state->current_coro_gro_slot, "coro.gro.end.ret");
      be_state->builder->CreateRet(gro_end_ret);
    } else {
      be_state->builder->CreateRet(llvm::Constant::getNullValue(func->getReturnType()));
    }
    be_state->builder->restoreIP(saved_cleanup_ip);
  }

  if (!descr->body_generated && descr->unhandled_exception_call && descr->final_suspend_label) {
    llvm::BasicBlock* lpad_bb = nullptr;
    LLVM_GEN_BE_CHECK(llvm_emit_coro_unhandled_exception_landing_pad(descr, func, &lpad_bb));
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a coroutine `co_return` statement into LLVM IR.
 * @details Evaluates `promise.return_void()` or `promise.return_value(expr)` when present on
 *          @p stmt and branches cleanly to the coroutine's `final_suspend` (or frame cleanup)
 *          block without emitting any duplicate `@llvm.coro.id` intrinsics.
 * @param[in] stmt Pointer to the EDG coroutine return statement node.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_coroutine_return_stmt(a_statement_ptr stmt) noexcept {
  if (!stmt || stmt->kind != stmk_coroutine_return) return llvm_gen_be_error_t::invalid_argument;

  llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();

  if (stmt->expr) {
    llvm::Value* ret_val = nullptr;
    llvm_gen_be_error_t err = llvm_lower_expression(stmt->expr, &ret_val);
    if (err != llvm_gen_be_error_t::ok) return err;
  }

  if (be_state->current_coro_final_suspend_bb &&
      be_state->current_coro_final_suspend_bb->getParent() == func) {
    be_state->builder->CreateBr(be_state->current_coro_final_suspend_bb);
  } else if (be_state->current_coro_cleanup_bb &&
             be_state->current_coro_cleanup_bb->getParent() == func) {
    be_state->builder->CreateBr(be_state->current_coro_cleanup_bb);
  } else if (func->getReturnType()->isVoidTy()) {
    be_state->builder->CreateRetVoid();
  } else if (be_state->current_coro_gro_slot &&
             llvm::cast<llvm::Instruction>(be_state->current_coro_gro_slot)->getFunction() == func) {
    llvm::Value* gro_ret = be_state->builder->CreateLoad(
        func->getReturnType(), be_state->current_coro_gro_slot, "coro.ret.gro");
    be_state->builder->CreateRet(gro_ret);
  } else {
    be_state->builder->CreateRet(llvm::Constant::getNullValue(func->getReturnType()));
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

  if (expr->variant.await_info.operand) {
    llvm::Value* awaiter_op_val = nullptr;
    LLVM_GEN_BE_CHECK(llvm_lower_expression(expr->variant.await_info.operand, &awaiter_op_val));
  }

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

  // 2. Suspend block: emit llvm.coro.save, evaluate await_suspend, then emit llvm.coro.suspend
  be_state->builder->SetInsertPoint(suspend_bb);
  llvm::Function* coro_save = EDG_LLVM_GET_INTRINSIC_DECL(
      be_state->module.get(), llvm::Intrinsic::coro_save);
  llvm::Value* null_hdl = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty));
  llvm::Value* save_token = be_state->builder->CreateCall(coro_save, {null_hdl}, "save_tok");

  llvm::BasicBlock* do_suspend_bb = llvm::BasicBlock::Create(*be_state->context, "coro.do.suspend", func);

  if (suspend_expr) {
    llvm::Value* susp_val = nullptr;
    llvm_gen_be_error_t err = llvm_lower_expression(suspend_expr, &susp_val);
    if (err != llvm_gen_be_error_t::ok) return err;

    llvm::Value* susp_hdl = nullptr;
    if (susp_val) {
      if (susp_val->getType()->isPointerTy()) {
        susp_hdl = susp_val;
      } else if (llvm::StructType* st = llvm::dyn_cast<llvm::StructType>(susp_val->getType())) {
        if (st->getNumElements() == 1 && st->getElementType(0)->isPointerTy()) {
          susp_hdl = be_state->builder->CreateExtractValue(susp_val, {0}, "coro.transfer.hdl");
        }
      } else if (susp_val->getType()->isIntegerTy(64) && suspend_expr->type &&
                 (suspend_expr->type->kind == tk_struct || suspend_expr->type->kind == tk_class)) {
        susp_hdl = be_state->builder->CreateIntToPtr(susp_val, int8_ptr_ty, "coro.transfer.i64hdl");
      }
    }
    if (susp_hdl) {
      llvm::Function* subfn = EDG_LLVM_GET_INTRINSIC_DECL(
          be_state->module.get(), llvm::Intrinsic::coro_subfn_addr);
      llvm::Value* resume_fn = be_state->builder->CreateCall(
          subfn, {susp_hdl, llvm::ConstantInt::get(int8_ty, 0)}, "coro.resume.fn");
      llvm::FunctionType* resume_fty = llvm::FunctionType::get(
          llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty}, false);
      llvm::CallInst* transfer_call = be_state->builder->CreateCall(resume_fty, resume_fn, {susp_hdl});
      transfer_call->setTailCall(true);
      be_state->builder->CreateBr(do_suspend_bb);
    } else if (susp_val && (susp_val->getType()->isIntegerTy(1) || susp_val->getType()->isIntegerTy(8))) {
      llvm::Value* susp_bool = susp_val->getType()->isIntegerTy(1)
                                   ? susp_val
                                   : be_state->builder->CreateICmpNE(
                                         susp_val, llvm::ConstantInt::get(susp_val->getType(), 0));
      be_state->builder->CreateCondBr(susp_bool, do_suspend_bb, resume_bb);
    } else {
      be_state->builder->CreateBr(do_suspend_bb);
    }
  } else {
    be_state->builder->CreateBr(do_suspend_bb);
  }

  be_state->builder->SetInsertPoint(do_suspend_bb);
  llvm::Function* coro_suspend = EDG_LLVM_GET_INTRINSIC_DECL(
      be_state->module.get(), llvm::Intrinsic::coro_suspend);
  bool is_final = (be_state->current_coro_suspend_bb != nullptr) ||
                  (be_state->current_coro_final_await_expr != nullptr &&
                   expr == be_state->current_coro_final_await_expr);
  llvm::Value* susp_result = be_state->builder->CreateCall(
      coro_suspend, {save_token, be_state->builder->getInt1(is_final)}, "susp_val");

  // 3-way resume switch on susp_result:
  // 0: resume normal execution
  // 1: cleanup and destroy frame
  // Default (-1): suspend and return to caller
  llvm::IntegerType* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
  llvm::SwitchInst* sw = be_state->builder->CreateSwitch(susp_result, ret_bb, 2);
  sw->addCase(llvm::ConstantInt::get(i8_ty, 0), resume_bb);
  sw->addCase(llvm::ConstantInt::get(i8_ty, 1), destroy_bb);

  // Destroy block: clean up coroutine frame and exit to ret_bb
  be_state->builder->SetInsertPoint(destroy_bb);
  if (!is_final) {
    for (a_dynamic_init_ptr dip = be_state->active_lowered_eh_cleanup;
         dip != nullptr;
         dip = dip->next_in_destruction_list
                   ? dip->next_in_destruction_list
                   : (dip->lifetime ? dip->lifetime->parent_destruction_sublist : nullptr)) {
      if (!dip->destructor || !dip->variable) {
        continue;
      }
      auto vit = be_state->local_vars.find(dip->variable);
      if (vit == be_state->local_vars.end() || !vit->second || vit->second == be_state->current_coro_promise) {
        continue;
      }
      llvm::Function* dtor_fn = nullptr;
      LLVM_GEN_BE_CHECK(llvm_get_or_declare_routine(dip->destructor, &dtor_fn));
      if (dtor_fn && dtor_fn->arg_size() >= 1) {
        std::vector<llvm::Value*> dtor_args;
        dtor_args.push_back(vit->second);
        if (dtor_fn->arg_size() >= 2) {
          llvm::Type* p1_ty = dtor_fn->getFunctionType()->getParamType(1);
          if (p1_ty->isIntegerTy()) {
            dtor_args.push_back(llvm::ConstantInt::get(p1_ty, 2));
          } else {
            dtor_args.push_back(llvm::Constant::getNullValue(p1_ty));
          }
        }
        be_state->builder->CreateCall(dtor_fn, dtor_args);
      }
    }
  }
  if (be_state->current_coro_cleanup_bb &&
      be_state->current_coro_cleanup_bb->getParent() == func) {
    be_state->builder->CreateBr(be_state->current_coro_cleanup_bb);
  } else {
    llvm::Function* coro_free_fn = EDG_LLVM_GET_INTRINSIC_DECL(
        be_state->module.get(), llvm::Intrinsic::coro_free);
    llvm::Value* active_hdl = (be_state->current_coro_handle &&
        llvm::cast<llvm::Instruction>(be_state->current_coro_handle)->getFunction() == func)
        ? be_state->current_coro_handle
        : null_hdl;
    be_state->builder->CreateCall(coro_free_fn, {llvm::ConstantTokenNone::get(*be_state->context), active_hdl});
    be_state->builder->CreateBr(ret_bb);
  }

  // Return to caller block: route through unified @llvm.coro.end block so CoroSplitPass splits .resume/.destroy exits
  be_state->builder->SetInsertPoint(ret_bb);
  if (be_state->current_coro_end_bb &&
      be_state->current_coro_end_bb->getParent() == func) {
    be_state->builder->CreateBr(be_state->current_coro_end_bb);
  } else {
    llvm::Function* coro_end_fn = EDG_LLVM_GET_INTRINSIC_DECL(
        be_state->module.get(), llvm::Intrinsic::coro_end);
#if LLVM_VERSION_MAJOR >= 17
    be_state->builder->CreateCall(
        coro_end_fn,
        {null_hdl, be_state->builder->getFalse(), llvm::ConstantTokenNone::get(*be_state->context)});
#else
    be_state->builder->CreateCall(coro_end_fn, {null_hdl, be_state->builder->getFalse()});
#endif
    if (func->getReturnType()->isVoidTy()) {
      be_state->builder->CreateRetVoid();
    } else if (be_state->current_coro_gro_slot &&
               llvm::cast<llvm::Instruction>(be_state->current_coro_gro_slot)->getFunction() == func) {
      llvm::Value* gro_ret = be_state->builder->CreateLoad(
          func->getReturnType(), be_state->current_coro_gro_slot, "coro.gro.ret");
      be_state->builder->CreateRet(gro_ret);
    } else {
      be_state->builder->CreateRet(llvm::Constant::getNullValue(func->getReturnType()));
    }
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

/**
 * @brief Emits Heap Allocation Elision Optimization (HALO) hint attributes.
 * @details Applies coro_elide_safe and related optimization hints.
 * @param[in] func Pointer to the LLVM Function.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_coro_halo_hints(llvm::Function* func) noexcept {
  if (func == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  func->addFnAttr("coro_elide_safe");
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits a call to the `@llvm.coro.noop()` intrinsic for `std::noop_coroutine()`.
 * @details Inserts a declaration for `@llvm.coro.noop` in the active module and emits a call returning
 *          the constant noop coroutine frame handle pointer.
 * @param[out] out_noop_hdl Receives the resulting `@llvm.coro.noop()` handle value.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if @p out_noop_hdl is null.
 */
llvm_gen_be_error_t llvm_lower_coro_noop(llvm::Value** out_noop_hdl) noexcept {
  if (out_noop_hdl == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  llvm::Function* coro_noop_fn = EDG_LLVM_GET_INTRINSIC_DECL(
      be_state->module.get(), llvm::Intrinsic::coro_noop);
  *out_noop_hdl = be_state->builder->CreateCall(coro_noop_fn, {}, "coro.noop.hdl");
  return llvm_gen_be_error_t::ok;
}
END_EDG_NAMESPACE
#endif
