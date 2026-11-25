/**
 * @file llvm_gen_be_asm.h
 * @ingroup llvm_gen_be_stmt
 * @brief Inline Assembly subsystem for the EDG LLVM backend.
 * @details Implements inline assembly lowering for GNU-style asm and MSVC-style __asm.
 *
 * Part of the EDG Compiler Project.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_ASM_H
#define LLVM_GEN_BE_ASM_H 1

#include "basic_hdrs.h"
#include "llvm_gen_be_error.h"
#include <string>
#include <vector>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @struct msvc_asm_parsed_block_t
 * @brief Holds the rewritten assembly text, memory operands, clobbers, and labels for an MSVC `__asm` block.
 * @details Populated by `parse_msvc_asm_block` and consumed by `llvm_lower_asm_stmt` when creating
 *          an Intel-dialect `llvm::InlineAsm` call instruction.
 */
struct msvc_asm_parsed_block_t {
  std::string rewritten_asm;                      ///< Rewritten assembly text with `${N:a}` operand placeholders and constant offsets.
  std::string constraints;                        ///< Constructed LLVM constraint and clobber string (`*m`, `~{reg}`).
  std::vector<llvm::Value*> args;                 ///< Bound local variable pointer `llvm::Value*` arguments.
  std::vector<llvm::Type*> arg_types;             ///< LLVM types of `args`.
  std::vector<llvm::Type*> arg_element_types;     ///< Pointee element types for `*m` memory operands.
  std::vector<std::string> clobbered_registers;   ///< Explicitly and implicitly clobbered x86/x64 register names.
  std::vector<std::string> referenced_labels;     ///< Intra-function label names targeted by jump/loop instructions.
};

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
    msvc_asm_parsed_block_t* out_parsed) noexcept;

/**
 * @brief Lowers an inline assembly statement to LLVM IR.
 * @details Supports both GNU-style extended inline assembly (including asm goto and
 * constraints like r, m, i) and MSVC-style __asm blocks. For MSVC, parses the block
 * to replace local variable references with memory operands and struct member
 * accesses with constant displacements.
 * @param[in] stmt The EDG statement representing the inline assembly block.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_asm_stmt(a_statement_ptr stmt) noexcept;

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
    std::string* out_canonical) noexcept;

END_EDG_NAMESPACE
#endif

#endif /* LLVM_GEN_BE_ASM_H */
