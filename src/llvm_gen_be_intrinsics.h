/**
 * @file llvm_gen_be_intrinsics.h
 * @ingroup llvm_gen_be_expr
 * @brief Target architecture SIMD and system intrinsic lowering subsystem for the EDG LLVM backend.
 * @details Declares target-specific builtin lowering functions for x86/x86_64 (MMX, SSE, AVX,
 *          AVX2, AVX-512, AMX, BMI, AES, SHA), ARM/AArch64 (NEON, SVE, SVE2, CRC32, Crypto,
 *          barriers, system registers), RISC-V (RVV, Zbb/Zbc/Zbs bitmanip), PowerPC (Altivec,
 *          VSX), s390x, LoongArch (LSX, LASX), MIPS, SPARC, and WebAssembly SIMD128.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_INTRINSICS_H
#define LLVM_GEN_BE_INTRINSICS_H 1

#include "basic_hdrs.h"

#if BACK_END_IS_LLVM_GEN_BE

#include "il_def.h"
#include "llvm_gen_be_error.h"
#include <string>
#include <vector>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Value.h>

BEGIN_EDG_NAMESPACE

#if !BUILTIN_FUNCTIONS_ENABLED
/** @brief Fallback builtin-function kind integer typedef when EDG builtin functions are disabled. */
typedef unsigned short a_builtin_function_kind;
#endif

/**
 * @brief Lowers an x86/x86_64 target builtin (`__builtin_ia32_*`) into LLVM IR.
 * @details Handles SSE/AVX/AVX-512 packed floating-point and integer arithmetic, saturating
 *          arithmetic (`paddsb`, `psubsb`, `paddusw`, `psubusw`), vector shuffles/blends/extracts/inserts,
 *          nontemporal and masked vector loads/stores, FMA, and x86 system/crypto/bit-manipulation
 *          intrinsics (`pause`, `mfence`, `sfence`, `lfence`, `clflush`, `rdtsc`, `crc32`, `aes`, `sha`).
 * @param[in] expr Pointer to the EDG call expression node (may be null for direct synthetic calls).
 * @param[in] bfk The EDG `a_builtin_function_kind` code for the target builtin.
 * @param[in] builtin_name Null-terminated C string name of the builtin routine.
 * @param[in] args Lowered LLVM argument values for the call.
 * @param[in] callee_ty Lowered LLVM FunctionType of the target builtin (may be null).
 * @param[out] out_val Receives the lowered `llvm::Value*` when handled.
 * @param[out] out_handled Receives `true` if the builtin was recognized and lowered.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_x86_builtin(
    an_expr_node_ptr expr,
    a_builtin_function_kind bfk,
    const char* builtin_name,
    const std::vector<llvm::Value*>& args,
    llvm::FunctionType* callee_ty,
    llvm::Value** out_val,
    bool* out_handled) noexcept;

/**
 * @brief Lowers an ARM32 or AArch64 target builtin (`__builtin_neon_*`, `__builtin_sve_*`, `__builtin_arm_*`) into LLVM IR.
 * @details Handles AArch64 NEON vector arithmetic, saturating/rounding operations, horizontal reductions,
 *          table lookups, SVE/SVE2 predicated scalable vector operations, CRC32, crypto extensions,
 *          memory barriers (`dmb`, `dsb`, `isb`), system registers (`rsr64`, `wsr64`), and hint instructions.
 * @param[in] expr Pointer to the EDG call expression node (may be null for direct synthetic calls).
 * @param[in] bfk The EDG `a_builtin_function_kind` code for the target builtin.
 * @param[in] builtin_name Null-terminated C string name of the builtin routine.
 * @param[in] args Lowered LLVM argument values for the call.
 * @param[in] callee_ty Lowered LLVM FunctionType of the target builtin (may be null).
 * @param[out] out_val Receives the lowered `llvm::Value*` when handled.
 * @param[out] out_handled Receives `true` if the builtin was recognized and lowered.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_arm_aarch64_builtin(
    an_expr_node_ptr expr,
    a_builtin_function_kind bfk,
    const char* builtin_name,
    const std::vector<llvm::Value*>& args,
    llvm::FunctionType* callee_ty,
    llvm::Value** out_val,
    bool* out_handled) noexcept;

/**
 * @brief Lowers a RISC-V target builtin (`__builtin_rvv_*`, `__builtin_riscv_*`) into LLVM IR.
 * @details Handles RISC-V Vector (`vsetvli`, `vsetvlimax`, scalable RVV arithmetic/load/store) and
 *          Zbb/Zbc/Zbs bit-manipulation and scalar cryptography intrinsics (`clmul`, `clmulh`, `orc_b`, `rev8`).
 * @param[in] expr Pointer to the EDG call expression node (may be null for direct synthetic calls).
 * @param[in] bfk The EDG `a_builtin_function_kind` code for the target builtin.
 * @param[in] builtin_name Null-terminated C string name of the builtin routine.
 * @param[in] args Lowered LLVM argument values for the call.
 * @param[in] callee_ty Lowered LLVM FunctionType of the target builtin (may be null).
 * @param[out] out_val Receives the lowered `llvm::Value*` when handled.
 * @param[out] out_handled Receives `true` if the builtin was recognized and lowered.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_riscv_builtin(
    an_expr_node_ptr expr,
    a_builtin_function_kind bfk,
    const char* builtin_name,
    const std::vector<llvm::Value*>& args,
    llvm::FunctionType* callee_ty,
    llvm::Value** out_val,
    bool* out_handled) noexcept;

/**
 * @brief Lowers PowerPC, s390x, LoongArch, MIPS, SPARC, and WebAssembly target builtins into LLVM IR.
 * @details Handles PowerPC Altivec/VSX (`__builtin_altivec_*`, `__builtin_vsx_*`), s390x vector
 *          (`__builtin_s390_*`), LoongArch LSX/LASX (`__builtin_lsx_*`, `__builtin_lasx_*`), and
 *          WebAssembly SIMD128 (`__builtin_wasm_*`) intrinsics.
 * @param[in] expr Pointer to the EDG call expression node (may be null for direct synthetic calls).
 * @param[in] bfk The EDG `a_builtin_function_kind` code for the target builtin.
 * @param[in] builtin_name Null-terminated C string name of the builtin routine.
 * @param[in] args Lowered LLVM argument values for the call.
 * @param[in] callee_ty Lowered LLVM FunctionType of the target builtin (may be null).
 * @param[out] out_val Receives the lowered `llvm::Value*` when handled.
 * @param[out] out_handled Receives `true` if the builtin was recognized and lowered.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_other_arch_builtin(
    an_expr_node_ptr expr,
    a_builtin_function_kind bfk,
    const char* builtin_name,
    const std::vector<llvm::Value*>& args,
    llvm::FunctionType* callee_ty,
    llvm::Value** out_val,
    bool* out_handled) noexcept;

/**
 * @brief Top-level dispatcher that lowers any target architecture builtin call into LLVM IR.
 * @details Inspects the routine name and `a_builtin_function_kind` on @p expr (or @p rname) and
 *          delegates to `llvm_lower_x86_builtin`, `llvm_lower_arm_aarch64_builtin`,
 *          `llvm_lower_riscv_builtin`, or `llvm_lower_other_arch_builtin`.
 * @param[in] expr Pointer to the EDG call expression node (may be null).
 * @param[in] bfk The EDG `a_builtin_function_kind` code for the target builtin.
 * @param[in] rname Name of the target builtin routine.
 * @param[in] args Lowered LLVM argument values.
 * @param[in] callee_ty Lowered LLVM FunctionType of the builtin (may be null).
 * @param[out] out_val Receives the resulting `llvm::Value*`.
 * @param[out] out_handled Receives `true` if the target builtin was lowered.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_target_builtin(
    an_expr_node_ptr expr,
    a_builtin_function_kind bfk,
    const std::string& rname,
    const std::vector<llvm::Value*>& args,
    llvm::FunctionType* callee_ty,
    llvm::Value** out_val,
    bool* out_handled) noexcept;

END_EDG_NAMESPACE

#endif /* BACK_END_IS_LLVM_GEN_BE */

#endif /* LLVM_GEN_BE_INTRINSICS_H */
