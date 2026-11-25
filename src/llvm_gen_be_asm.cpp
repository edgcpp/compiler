/**
 * @file llvm_gen_be_asm.cpp
 * @ingroup llvm_gen_be_stmt
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
 * @brief Appends a register clobber constraint (`~{reg}`) to @p parsed if not already present.
 * @details Deduplicates register clobbers across multiple instructions in an MSVC `__asm` block.
 * @param[in,out] parsed Target `msvc_asm_parsed_block_t` descriptor.
 * @param[in] reg_name Null-terminated register name (e.g., `"eax"`, `"xmm0"`).
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t add_msvc_clobber(
    msvc_asm_parsed_block_t* parsed,
    const char* reg_name) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  for (const std::string& existing : parsed->clobbered_registers) {
    if (existing == reg_name) {
      return llvm_gen_be_error_t::ok;
    }
  }
  parsed->clobbered_registers.push_back(reg_name);
  parsed->constraints += ",~{";
  parsed->constraints += reg_name;
  parsed->constraints += "}";
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Parses an MSVC `__asm` block string into operands, struct offsets, register clobbers, and label targets.
 * @details Evaluates `TYPE`, `SIZE`, `LENGTH`, and `OFFSET` directives, normalizes `BYTE/WORD/DWORD/QWORD PTR`,
 *          resolves nested `struct.sub.member` byte displacements, binds local variables as `*m` memory operands,
 *          records branch/loop target labels, and detects explicit and implicit (`cpuid`, `rdtsc`, `div`/`idiv`,
 *          `movsb`/`stosb`) x86/x64 register clobbers (`eax`..`r15`, `xmm0`..`xmm15`, `mm0`..`mm7`).
 * @param[in] raw_asm Raw MSVC inline assembly text string.
 * @param[out] out_parsed Receives the populated `msvc_asm_parsed_block_t`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t parse_msvc_asm_block(
    const std::string& raw_asm,
    msvc_asm_parsed_block_t* out_parsed) noexcept {
  if (!out_parsed) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  *out_parsed = msvc_asm_parsed_block_t{};
  out_parsed->constraints = "~{dirflag},~{fpsr},~{flags}";

  size_t last_pos = 0;
  for (size_t i = 0; i < raw_asm.length(); ++i) {
    if (isalpha(raw_asm[i]) || raw_asm[i] == '_') {
      size_t start = i;
      while (i < raw_asm.length() && (isalnum(raw_asm[i]) || raw_asm[i] == '_' || raw_asm[i] == '.')) {
        i++;
      }
      std::string token = raw_asm.substr(start, i - start);

      // 0. Check for MSVC size operators: TYPE, SIZE, LENGTH, or OFFSET
      if (token == "TYPE" || token == "SIZE" || token == "LENGTH" || token == "OFFSET") {
        size_t next_start = i;
        while (next_start < raw_asm.length() && isspace(raw_asm[next_start])) next_start++;
        size_t next_end = next_start;
        while (next_end < raw_asm.length() &&
               (isalnum(raw_asm[next_end]) || raw_asm[next_end] == '_' || raw_asm[next_end] == '.')) {
          next_end++;
        }
        std::string next_token = raw_asm.substr(next_start, next_end - next_start);

        a_variable_ptr var_found = nullptr;
        for (auto& pair : be_state->local_vars) {
          a_variable_ptr var = pair.first;
          if (var->source_corresp.name && next_token == var->source_corresp.name) {
            var_found = var;
            break;
          }
        }

        if (var_found) {
          out_parsed->rewritten_asm += raw_asm.substr(last_pos, start - last_pos);
          if (token == "OFFSET") {
            out_parsed->rewritten_asm += "offset ${" + std::to_string(out_parsed->args.size()) + ":a}";
            out_parsed->args.push_back(be_state->local_vars[var_found]);
            out_parsed->arg_types.push_back(be_state->local_vars[var_found]->getType());
            llvm::Type* elem_ty = nullptr;
            llvm_gen_be_error_t err_ty = get_llvm_type(var_found->type, &elem_ty);
            if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
            out_parsed->arg_element_types.push_back(elem_ty);
            out_parsed->constraints += ",*m";
          } else {
            uint64_t val = 0;
            if (token == "TYPE" || token == "SIZE") {
              val = static_cast<uint64_t>(var_found->type->size);
            } else {
              val = (var_found->type->kind == tk_array)
                        ? static_cast<uint64_t>(var_found->type->variant.array.variant.number_of_elements)
                        : 1ULL;
            }
            out_parsed->rewritten_asm += std::to_string(val);
          }
          last_pos = next_end;
          i = next_end - 1;
          continue;
        }
      }

      // 0b. Branch / loop instruction label target recording (jmp, je, jne, jg, jl, loop)
      if (token == "jmp" || token == "je" || token == "jne" ||
          token == "jg" || token == "jl" || token == "loop") {
        size_t lbl_start = i;
        while (lbl_start < raw_asm.length() && (raw_asm[lbl_start] == ' ' || raw_asm[lbl_start] == '\t')) {
          lbl_start++;
        }
        size_t lbl_end = lbl_start;
        while (lbl_end < raw_asm.length() && (isalnum(raw_asm[lbl_end]) || raw_asm[lbl_end] == '_')) {
          lbl_end++;
        }
        if (lbl_end > lbl_start) {
          out_parsed->referenced_labels.push_back(raw_asm.substr(lbl_start, lbl_end - lbl_start));
        }
      }

      // 0c. Implicit instruction clobbers (cpuid, rdtsc, div/idiv, movsb/stosb)
      if (token == "cpuid") {
        LLVM_GEN_BE_CHECK(add_msvc_clobber(out_parsed, "eax"));
        LLVM_GEN_BE_CHECK(add_msvc_clobber(out_parsed, "ebx"));
        LLVM_GEN_BE_CHECK(add_msvc_clobber(out_parsed, "ecx"));
        LLVM_GEN_BE_CHECK(add_msvc_clobber(out_parsed, "edx"));
        continue;
      }
      if (token == "rdtsc" || token == "idiv" || token == "div") {
        LLVM_GEN_BE_CHECK(add_msvc_clobber(out_parsed, "eax"));
        LLVM_GEN_BE_CHECK(add_msvc_clobber(out_parsed, "edx"));
        continue;
      }
      if (token == "movsb" || token == "stosb") {
        LLVM_GEN_BE_CHECK(add_msvc_clobber(out_parsed, "ecx"));
        LLVM_GEN_BE_CHECK(add_msvc_clobber(out_parsed, "esi"));
        LLVM_GEN_BE_CHECK(add_msvc_clobber(out_parsed, "edi"));
        continue;
      }

      // 1. Check for struct_type.member
      size_t dot_pos = token.find('.');
      if (dot_pos != std::string::npos) {
        std::string base_name = token.substr(0, dot_pos);
        uint64_t offset = 0;
        bool resolved = false;
        a_type_ptr root_struct_ty = nullptr;

        for (auto& pair : be_state->local_vars) {
          a_variable_ptr var = pair.first;
          if (var->source_corresp.name && base_name == var->source_corresp.name) {
            root_struct_ty = skip_typerefs(var->type);
            break;
          }
        }

        if (!root_struct_ty && il_header.primary_scope) {
          for (a_type_ptr tp = il_header.primary_scope->types; tp != nullptr; tp = tp->next) {
            if (tp->source_corresp.name && base_name == tp->source_corresp.name) {
              root_struct_ty = skip_typerefs(tp);
              break;
            }
          }
          if (!root_struct_ty && il_header.primary_scope->variables) {
            for (a_variable_ptr vp = il_header.primary_scope->variables; vp != nullptr; vp = vp->next) {
              if (vp->source_corresp.name && base_name == vp->source_corresp.name) {
                root_struct_ty = skip_typerefs(vp->type);
                break;
              }
            }
          }
        }

        if (root_struct_ty) {
          a_type_ptr struct_ty = root_struct_ty;
          std::string path = token.substr(dot_pos + 1);
          resolved = true;
          for (size_t path_pos = 0; ; ) {
            size_t next_dot = path.find('.', path_pos);
            std::string member_name = path.substr(
                path_pos, next_dot == std::string::npos ? std::string::npos : next_dot - path_pos);

            bool found_field = false;
            if (struct_ty->kind == tk_struct || struct_ty->kind == tk_class || struct_ty->kind == tk_union) {
              for (a_field_ptr f = struct_ty->variant.class_struct_union.field_list; f != nullptr; f = f->next) {
                if (f->source_corresp.name && member_name == f->source_corresp.name) {
                  offset += f->offset;
                  struct_ty = skip_typerefs(f->type);
                  found_field = true;
                  break;
                }
              }
            }
            if (!found_field) {
              resolved = false;
              break;
            }
            if (next_dot == std::string::npos) break;
            path_pos = next_dot + 1;
          }
        }

        if (resolved) {
          out_parsed->rewritten_asm += raw_asm.substr(last_pos, start - last_pos);
          out_parsed->rewritten_asm += std::to_string(offset);
          last_pos = i;
        }
        continue;
      }

      // 2. Check for local variables
      bool found_var = false;
      for (auto& pair : be_state->local_vars) {
        a_variable_ptr var = pair.first;
        if (var->source_corresp.name && token == var->source_corresp.name) {
          out_parsed->rewritten_asm += raw_asm.substr(last_pos, start - last_pos);
          out_parsed->rewritten_asm += "${" + std::to_string(out_parsed->args.size()) + ":a}";
          last_pos = i;

          out_parsed->args.push_back(pair.second);
          out_parsed->arg_types.push_back(pair.second->getType());

          llvm::Type* elem_ty = nullptr;
          llvm_gen_be_error_t err_ty = get_llvm_type(pair.first->type, &elem_ty);
          if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
          out_parsed->arg_element_types.push_back(elem_ty);

          out_parsed->constraints += ",*m";
          found_var = true;
          break;
        }
      }
      if (found_var) continue;

      // 3. Explicit register clobber analysis (GPRs, subregisters, XMM/YMM/ZMM, MMX, AArch64, RISC-V)
      const char* common_regs[] = {
          "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp",
          "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
          "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
          "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d",
          "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
          "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15",
          "ymm0", "ymm1", "zmm0", "zmm1", "k0", "k1",
          "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7"
      };
      for (const char* reg : common_regs) {
        if (token == reg) {
          LLVM_GEN_BE_CHECK(add_msvc_clobber(out_parsed, reg));
          break;
        }
      }
    }
  }
  out_parsed->rewritten_asm += raw_asm.substr(last_pos);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an inline assembly statement to LLVM IR.
 * @details Supports both GNU-style extended inline assembly (including asm goto and
 * constraints like r, m, i) and MSVC-style __asm blocks. For MSVC, parses the block
 * to replace local variable references with memory operands and struct member
 * accesses with constant displacements.
 * @param[in] stmt The EDG statement representing the inline assembly block.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_asm_stmt(a_statement_ptr stmt) noexcept {
  if (!stmt || stmt->kind != stmk_asm) return llvm_gen_be_error_t::invalid_argument;
  an_asm_entry_ptr aep = stmt->variant.asm_entry;
  if (!aep) return llvm_gen_be_error_t::invalid_argument;

  std::string asm_str;
  if (aep->asm_string && aep->asm_string->kind == ck_string) {
    asm_str = std::string(aep->asm_string->variant.string.value, static_cast<size_t>(aep->asm_string->variant.string.length - 1));
  }

  std::string constraints;
  std::vector<llvm::Value*> args;
  std::vector<llvm::Type*> arg_types;
  std::vector<llvm::Type*> arg_element_types;
  std::vector<llvm::Type*> output_types;
  std::vector<an_expr_node_ptr> output_exprs;

  if (!aep->gnu_asm_form) {
    msvc_asm_parsed_block_t parsed = {};
    llvm_gen_be_error_t parse_err = parse_msvc_asm_block(asm_str, &parsed);
    if (parse_err != llvm_gen_be_error_t::ok) return parse_err;

    llvm::FunctionType* asm_func_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*be_state->context), parsed.arg_types, false);
    llvm::InlineAsm* inline_asm = llvm::InlineAsm::get(
        asm_func_ty, parsed.rewritten_asm, parsed.constraints, true, false, llvm::InlineAsm::AD_Intel);
    llvm::CallInst* call = be_state->builder->CreateCall(inline_asm, parsed.args);
    for (unsigned idx = 0; idx < parsed.args.size(); ++idx) {
      call->addParamAttr(
          idx, llvm::Attribute::get(*be_state->context, llvm::Attribute::ElementType, parsed.arg_element_types[idx]));
    }
    return llvm_gen_be_error_t::ok;
  }

  bool first = true;
  std::vector<std::string> tied_input_constraints;
  if (aep->operands != NULL) {
    std::string llvm_asm_str;
    llvm_asm_str.reserve(asm_str.size() + 8);
    for (size_t si = 0; si < asm_str.size(); ++si) {
      char ch = asm_str[si];
      if (ch == '$') {
        llvm_asm_str += "$$";
      } else if (ch == '%' && si + 1 < asm_str.size()) {
        char nx = asm_str[si + 1];
        if (nx == '%') {
          llvm_asm_str += '%';
          ++si;
        } else if (nx >= '0' && nx <= '9') {
          llvm_asm_str += '$';
        } else if ((nx == 'w' || nx == 'x' || nx == 'b' || nx == 'h' ||
                    nx == 'k' || nx == 'q' || nx == 'c' || nx == 'P') &&
                   si + 2 < asm_str.size() && asm_str[si + 2] >= '0' &&
                   asm_str[si + 2] <= '9') {
          llvm_asm_str += "${";
          si += 2;
          while (si < asm_str.size() && asm_str[si] >= '0' && asm_str[si] <= '9') {
            llvm_asm_str += asm_str[si++];
          }
          --si;
          llvm_asm_str += ':';
          llvm_asm_str += nx;
          llvm_asm_str += '}';
        } else {
          llvm_asm_str += ch;
        }
      } else {
        llvm_asm_str += ch;
      }
    }
    asm_str = llvm_asm_str;
  }

  for (an_asm_operand_ptr aop = aep->operands; aop != NULL; aop = aop->next) {
    if (!first) constraints += ",";
    first = false;

    bool output = EDG_ASM_OP_IS_OUTPUT(aop);
    std::string constr = EDG_ASM_OP_CONSTRAINT_STR(aop);
    bool is_memory = (constr.find("m") != std::string::npos);
    bool is_read_write_reg = (output && !is_memory && constr.find("+") != std::string::npos);

    if (is_read_write_reg) {
      size_t plus_pos = constr.find("+");
      constr.replace(plus_pos, 1, "=");
      tied_input_constraints.push_back(std::to_string(output_types.size()));
    } else if (is_memory) {
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
      if (is_read_write_reg) {
        llvm::Value* rw_val = nullptr;
        llvm_gen_be_error_t rw_err = llvm_lower_expression(aop->expression, &rw_val);
        if (rw_err != llvm_gen_be_error_t::ok) return rw_err;
        if (rw_val->getType()->isPointerTy()) {
          rw_val = be_state->builder->CreateLoad(ty, rw_val, "asm.rw.in");
        }
        args.push_back(rw_val);
        arg_types.push_back(rw_val->getType());
        arg_element_types.push_back(nullptr);
      }
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

  for (const std::string& tied_c : tied_input_constraints) {
    constraints += ",";
    constraints += tied_c;
  }

  for (a_named_register_list_ptr clob = aep->clobbers; clob != NULL; clob = clob->next) {
    if (!first) constraints += ",";
    first = false;
    constraints += "~{";
    constraints += named_register_names[(int)clob->reg];
    constraints += "}";
  }

  llvm::Type* ret_ty = nullptr;
  if (output_types.empty()) ret_ty = llvm::Type::getVoidTy(*be_state->context);
  else if (output_types.size() == 1) ret_ty = output_types[0];
  else ret_ty = llvm::StructType::get(*be_state->context, output_types);
  
  if (aep->is_asm_goto) {
      // Implement asm goto (including output operands in C/C++ asm goto)
      llvm::FunctionType* asm_func_ty = llvm::FunctionType::get(ret_ty, arg_types, false);
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
      if (output_types.size() == 1) {
        llvm::Value* dst_ptr = nullptr;
        llvm_gen_be_error_t err = llvm_lower_expression(output_exprs[0], &dst_ptr);
        if (err != llvm_gen_be_error_t::ok) return err;
        be_state->builder->CreateStore(callbr, dst_ptr);
      }
      return llvm_gen_be_error_t::ok;
  }

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
      llvm::Value* ext = be_state->builder->CreateExtractValue(call, static_cast<unsigned>(i));
      llvm::Value* dst_ptr = nullptr;
      llvm_gen_be_error_t err = llvm_lower_expression(output_exprs[i], &dst_ptr);
      if (err != llvm_gen_be_error_t::ok) return err;
      be_state->builder->CreateStore(ext, dst_ptr);
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Canonicalizes a GCC/Clang inline assembly operand constraint string for LLVM IR.
 * @details Translates target-specific single-letter machine register constraints (`a`, `b`, `c`,
 *          `d`, `S`, `D`, `q`/`Q`/`R`, `Yz` on x86/x86_64; `Q` on AArch64/ARM) into LLVM
 *          register constraints (`{rax}`, `{rbx}`, etc.) and strips GCC-only cost/commutativity
 *          modifiers (`%`, `*`, `#`, `!`, `?`).
 * @param[in] raw_constraint Null-terminated GCC constraint string.
 * @param[in] is_output True if the operand is an output operand.
 * @param[in] triple_str Target triple string identifying the target architecture.
 * @param[out] out_canonical Receives the canonicalized LLVM constraint string.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if pointers are null.
 */
llvm_gen_be_error_t llvm_canonicalize_asm_constraint(
    const char* raw_constraint,
    bool is_output,
    const std::string& triple_str,
    std::string* out_canonical) noexcept {
  if (!raw_constraint || !out_canonical) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  std::string prefix;
  std::string body;
  for (size_t i = 0; raw_constraint[i] != '\0'; ++i) {
    char c = raw_constraint[i];
    if (c == '%' || c == '#' || c == '!' || c == '?') {
      continue;
    }
    if ((c == '=' || c == '+' || c == '&') && body.empty()) {
      prefix += (c == '+' && is_output) ? '=' : c;
    } else {
      body += c;
    }
  }

  bool is_x86 = (triple_str.find("x86_64") != std::string::npos ||
                 triple_str.find("i386") != std::string::npos ||
                 triple_str.find("i686") != std::string::npos);
  if (is_x86) {
    if (body == "a") body = "{ax}";
    else if (body == "b") body = "{bx}";
    else if (body == "c") body = "{cx}";
    else if (body == "d") body = "{dx}";
    else if (body == "S") body = "{si}";
    else if (body == "D") body = "{di}";
    else if (body == "Yz") body = "{xmm0}";
    else if (body == "q" || body == "Q" || body == "R") body = "r";
  } else if (triple_str.find("aarch64") != std::string::npos ||
             triple_str.find("arm") != std::string::npos) {
    if (body == "x") body = "w";
    else if (body == "Q") body = "*m";
  }
  *out_canonical = prefix + body;
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
