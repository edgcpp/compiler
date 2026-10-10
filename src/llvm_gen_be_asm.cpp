/**
 * @file llvm_gen_be_asm.cpp
 * @brief Inline Assembly subsystem for the EDG LLVM backend.
 * @details Implements inline assembly lowering for GNU-style asm and MSVC-style __asm.
 *
 * Part of the EDG Compiler Project.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_asm.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/InlineAsm.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief llvm_lower_asm_stmt
 * @param[in] _p param
 * @return llvm_gen_be_error_t::ok
 */
llvm_gen_be_error_t llvm_lower_asm_stmt(a_statement_ptr stmt) noexcept {
  if (!stmt || stmt->kind != stmk_asm) return llvm_gen_be_error_t::invalid_argument;
  an_asm_entry_ptr aep = stmt->variant.asm_entry;
  if (!aep) return llvm_gen_be_error_t::invalid_argument;

  std::string asm_str;
  if (aep->asm_string && aep->asm_string->kind == ck_string) {
    asm_str = std::string(aep->asm_string->variant.string.value, aep->asm_string->variant.string.length - 1);
  }

  std::string constraints;
  std::vector<llvm::Value*> args;
  std::vector<llvm::Type*> arg_types;
  std::vector<llvm::Type*> arg_element_types;
  std::vector<llvm::Type*> output_types;
  std::vector<an_expr_node_ptr> output_exprs;

  bool first = true;
  for (an_asm_operand_ptr aop = aep->operands; aop != NULL; aop = aop->next) {
    if (!first) constraints += ",";
    first = false;

    bool output = aop->is_output_operand;
    std::string constr = aop->constraints_string ? aop->constraints_string : "";
    bool is_memory = (constr.find("m") != std::string::npos);
    
    if (is_memory) {
      if (output && constr.find("*") == std::string::npos) {
        if (constr.find("=") != std::string::npos) constr.insert(constr.find("=") + 1, "*");
        else if (constr.find("+") != std::string::npos) constr.replace(constr.find("+"), 1, "*");
      } else if (!output && constr.find("*") == std::string::npos) {
        if (constr.find("+") != std::string::npos) constr.replace(constr.find("+"), 1, "*");
        else constr = "*" + constr;
      }
    }
    constraints += constr;

    if (output && !is_memory) {
      llvm::Type* ty = nullptr;
      llvm_gen_be_error_t err_ty = get_llvm_type(aop->expression->type, &ty);
      if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
      output_types.push_back(ty);
      output_exprs.push_back(aop->expression);
    } else {
      llvm::Value* arg_val = nullptr;
      llvm_gen_be_error_t err = llvm_lower_expression(aop->expression, &arg_val);
      if (err != llvm_gen_be_error_t::ok) return err;
      args.push_back(arg_val);
      arg_types.push_back(arg_val->getType());
      if (is_memory) {
         llvm::Type* mem_ty = nullptr;
         llvm_gen_be_error_t err_ty = get_llvm_type(aop->expression->type, &mem_ty);
         if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
         arg_element_types.push_back(mem_ty);
      } else {
         arg_element_types.push_back(nullptr);
      }
    }
  }

  for (a_named_register_list_ptr clob = aep->clobbers; clob != NULL; clob = clob->next) {
    if (!first) constraints += ",";
    first = false;
    constraints += "~{";
    constraints += named_register_names[(int)clob->reg];
    constraints += "}";
  }
  
  if (aep->is_asm_goto) {
      // Implement asm goto
      llvm::FunctionType* asm_func_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), arg_types, false);
      llvm::InlineAsm* inline_asm = llvm::InlineAsm::get(asm_func_ty, asm_str, constraints, true); // asm goto is always side-effecting
      
      llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
      llvm::BasicBlock* fallthrough_bb = llvm::BasicBlock::Create(*be_state->context, "asm.fallthrough", func);
      
      std::vector<llvm::BasicBlock*> indirect_dests;
      for (a_label_list_ptr lbl = aep->labels; lbl != NULL; lbl = lbl->next) {
          if (be_state->label_blocks.count(lbl->label)) {
              indirect_dests.push_back(be_state->label_blocks[lbl->label]);
          } else {
              // Should not happen in a well-formed function
              indirect_dests.push_back(fallthrough_bb);
          }
      }
      
      llvm::CallBrInst* callbr = be_state->builder->CreateCallBr(inline_asm, fallthrough_bb, indirect_dests, args);
      for (unsigned i = 0; i < args.size(); ++i) {
        if (arg_element_types[i]) {
           callbr->addParamAttr(i, llvm::Attribute::get(*be_state->context, llvm::Attribute::ElementType, arg_element_types[i]));
        }
      }
      be_state->builder->SetInsertPoint(fallthrough_bb);
      return llvm_gen_be_error_t::ok;
  }

  llvm::Type* ret_ty = nullptr;
  if (output_types.empty()) ret_ty = llvm::Type::getVoidTy(*be_state->context);
  else if (output_types.size() == 1) ret_ty = output_types[0];
  else ret_ty = llvm::StructType::get(*be_state->context, output_types);

  llvm::FunctionType* asm_func_ty = llvm::FunctionType::get(ret_ty, arg_types, false);
  llvm::InlineAsm* inline_asm = llvm::InlineAsm::get(asm_func_ty, asm_str, constraints, aep->is_volatile);
  llvm::CallInst* call = be_state->builder->CreateCall(inline_asm, args);
  for (unsigned i = 0; i < args.size(); ++i) {
    if (arg_element_types[i]) {
       call->addParamAttr(i, llvm::Attribute::get(*be_state->context, llvm::Attribute::ElementType, arg_element_types[i]));
    }
  }

  if (output_types.size() == 1) {
    llvm::Value* dst_ptr = nullptr;
    llvm_gen_be_error_t err = llvm_lower_expression(output_exprs[0], &dst_ptr);
    if (err != llvm_gen_be_error_t::ok) return err;
    be_state->builder->CreateStore(call, dst_ptr);
  } else if (output_types.size() > 1) {
    for (size_t i = 0; i < output_types.size(); ++i) {
      llvm::Value* ext = be_state->builder->CreateExtractValue(call, i);
      llvm::Value* dst_ptr = nullptr;
      llvm_gen_be_error_t err = llvm_lower_expression(output_exprs[i], &dst_ptr);
      if (err != llvm_gen_be_error_t::ok) return err;
      be_state->builder->CreateStore(ext, dst_ptr);
    }
  }
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
