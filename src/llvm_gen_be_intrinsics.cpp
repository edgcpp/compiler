/**
 * @file llvm_gen_be_intrinsics.cpp
 * @ingroup llvm_gen_be_expr
 * @brief Target architecture SIMD and system intrinsic lowering implementation for the EDG LLVM backend.
 * @details Implements lowering for x86/x86_64 (MMX, SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2, AVX,
 *          AVX2, AVX-512, AMX, BMI, AES, SHA), ARM/AArch64 (NEON, SVE, SVE2, CRC32, Crypto,
 *          barriers, system registers), RISC-V (RVV, Zbb/Zbc/Zbs bitmanip), PowerPC (Altivec,
 *          VSX), s390x, LoongArch (LSX, LASX), MIPS, SPARC, and WebAssembly SIMD128 builtins.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "basic_hdrs.h"

#if BACK_END_IS_LLVM_GEN_BE

#include "llvm_gen_be_intrinsics.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_type.h"
#include <llvm/ADT/SmallVector.h>
#include <llvm/Config/llvm-config.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Module.h>

BEGIN_EDG_NAMESPACE

/**
 * @brief Coerces an LLVM value to match an expected LLVM intrinsic parameter type when bit-widths match.
 * @details Handles XMM/YMM/ZMM vector bitcasts (e.g., `<4 x i32>` to `<2 x i64>` for AES-NI) and
 *          integer width casts so registered LLVM intrinsics pass `llvm::verifyModule` cleanly.
 * @param[in] val Input operand value.
 * @param[in] expected_ty Expected LLVM parameter type from the intrinsic declaration.
 * @param[out] out_coerced Receives the coerced `llvm::Value*`.
 * @return `llvm_gen_be_error_t::ok` on success, or `builtin_lowering_failed` if incompatible.
 */
static llvm_gen_be_error_t coerce_intrinsic_operand(
    llvm::Value* val,
    llvm::Type* expected_ty,
    llvm::Value** out_coerced) noexcept {
  if (val->getType() == expected_ty) {
    *out_coerced = val;
    return llvm_gen_be_error_t::ok;
  }
  if (val->getType()->isIntegerTy() && expected_ty->isIntegerTy()) {
    *out_coerced = be_state->builder->CreateIntCast(val, expected_ty, false);
    return llvm_gen_be_error_t::ok;
  }
  if (val->getType()->isFloatingPointTy() && expected_ty->isFloatingPointTy()) {
    *out_coerced = be_state->builder->CreateFPCast(val, expected_ty);
    return llvm_gen_be_error_t::ok;
  }
  unsigned src_bits = static_cast<unsigned>(val->getType()->getPrimitiveSizeInBits());
  unsigned dst_bits = static_cast<unsigned>(expected_ty->getPrimitiveSizeInBits());
  if (src_bits != 0 && src_bits == dst_bits) {
    *out_coerced = be_state->builder->CreateBitCast(val, expected_ty);
    return llvm_gen_be_error_t::ok;
  }
  if (val->getType()->isPointerTy() && expected_ty->isPointerTy()) {
    *out_coerced = be_state->builder->CreatePointerBitCastOrAddrSpaceCast(val, expected_ty);
    return llvm_gen_be_error_t::ok;
  }
  return llvm_gen_be_error_t::builtin_lowering_failed;
}

/**
 * @brief Validates and emits a registered LLVM intrinsic call via `lookupIntrinsicID` and `EDG_LLVM_GET_INTRINSIC_DECL`.
 * @details Never fabricates unchecked `@llvm.*` declarations that would fail `llvm::verifyModule`;
 *          deduces overloaded type arguments from `llvm::Intrinsic::getIntrinsicInfoTableEntries`
 *          and returns `llvm_gen_be_error_t::builtin_lowering_failed` if the intrinsic is unknown.
 * @param[in] llvm_intr_name Candidate LLVM intrinsic name.
 * @param[in] args Lowered LLVM argument values.
 * @param[in] callee_ty Lowered LLVM FunctionType of the builtin (may be null).
 * @param[out] out_val Receives the created `CallInst`.
 * @return `llvm_gen_be_error_t::ok` on success, or `llvm_gen_be_error_t::builtin_lowering_failed` if invalid.
 */
static llvm_gen_be_error_t emit_validated_llvm_intrinsic(
    const std::string& llvm_intr_name,
    const std::vector<llvm::Value*>& args,
    llvm::FunctionType* callee_ty,
    llvm::Value** out_val) noexcept {
  llvm::Intrinsic::ID iid = EDG_LLVM_LOOKUP_INTRINSIC_ID(llvm_intr_name);
  if (iid == llvm::Intrinsic::not_intrinsic) {
    std::string dotted = llvm_intr_name;
    for (size_t i = 0; i < dotted.size(); ++i) {
      if (dotted[i] == '_') dotted[i] = '.';
    }
    iid = EDG_LLVM_LOOKUP_INTRINSIC_ID(dotted);
  }
  if (iid == llvm::Intrinsic::not_intrinsic) {
    return llvm_gen_be_error_t::builtin_lowering_failed;
  }

  llvm::SmallVector<llvm::Type*, 4> overload_tys;
  if (llvm::Intrinsic::isOverloaded(iid)) {
    llvm::SmallVector<llvm::Intrinsic::IITDescriptor, 8> table;
    llvm::Intrinsic::getIntrinsicInfoTableEntries(iid, table);
    llvm::Type* ret_ty = callee_ty
        ? callee_ty->getReturnType()
        : (args.empty() ? llvm::Type::getVoidTy(*be_state->context) : args[0]->getType());

    auto pick_type_for_slot = [&](size_t desc_idx) noexcept -> llvm::Type* {
      if (desc_idx == 0) {
        return ret_ty->isVoidTy() && !args.empty() ? args[0]->getType() : ret_ty;
      }
      size_t arg_pos = desc_idx - 1;
      if (arg_pos < args.size()) {
        return args[arg_pos]->getType();
      }
      return ret_ty;
    };

    for (size_t d = 0; d < table.size(); ++d) {
#if LLVM_VERSION_MAJOR >= 20
      if (table[d].Kind == llvm::Intrinsic::IITDescriptor::Overloaded) {
        unsigned arg_num = table[d].getOverloadIndex();
#else
      if (table[d].Kind == llvm::Intrinsic::IITDescriptor::Argument) {
        unsigned arg_num = table[d].getArgumentNumber();
#endif
        llvm::Type* picked = pick_type_for_slot(d);
        if (picked->isVoidTy()) {
          return llvm_gen_be_error_t::builtin_lowering_failed;
        }
        while (overload_tys.size() <= arg_num) {
          overload_tys.push_back(llvm::Type::getInt32Ty(*be_state->context));
        }
        overload_tys[arg_num] = picked;
      }
    }
  }

  llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), iid, overload_tys);
  llvm::FunctionType* fn_ty = fn->getFunctionType();
  if (args.size() != fn_ty->getNumParams()) {
    return llvm_gen_be_error_t::builtin_lowering_failed;
  }

  std::vector<llvm::Value*> coerced_args;
  coerced_args.reserve(args.size());
  for (size_t i = 0; i < args.size(); ++i) {
    llvm::Value* c_arg = nullptr;
    LLVM_GEN_BE_CHECK(coerce_intrinsic_operand(args[i], fn_ty->getParamType(static_cast<unsigned>(i)), &c_arg));
    coerced_args.push_back(c_arg);
  }
  *out_val = be_state->builder->CreateCall(fn, coerced_args);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an x86/x86_64 target builtin (`__builtin_ia32_*`) into LLVM IR.
 * @details Handles SSE/AVX/AVX-512 packed floating-point and integer arithmetic, comparisons,
 *          unaligned loads/stores, saturating arithmetic (`paddsb`, `psubsb`, `paddusw`, `psubusw`),
 *          vector shuffles/blends/extracts/inserts, nontemporal and masked vector loads/stores, FMA,
 *          and validated x86 system/crypto/bit-manipulation intrinsics (`pause`, `mfence`, `sfence`,
 *          `lfence`, `clflush`, `rdtsc`, `crc32`, `aes`, `sha`, `bmi`, `amx`).
 * @param[in] expr Pointer to the EDG call expression node (may be null for direct synthetic calls).
 * @param[in] bfk The EDG `a_builtin_function_kind` code for the target builtin.
 * @param[in] builtin_name Null-terminated C string name of the builtin routine.
 * @param[in] args Lowered LLVM argument values for the call.
 * @param[in] callee_ty Lowered LLVM FunctionType of the target builtin (may be null).
 * @param[out] out_val Receives the lowered `llvm::Value*` when handled.
 * @param[out] out_handled Receives `true` if the builtin was recognized and lowered.
 * @return `llvm_gen_be_error_t::ok` on success, or `builtin_lowering_failed` if unrecognized/invalid.
 */
llvm_gen_be_error_t llvm_lower_x86_builtin(
    [[maybe_unused]] an_expr_node_ptr expr,
    [[maybe_unused]] a_builtin_function_kind bfk,
    const char* builtin_name,
    const std::vector<llvm::Value*>& args,
    llvm::FunctionType* callee_ty,
    llvm::Value** out_val,
    bool* out_handled) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(builtin_name);
  LLVM_GEN_BE_CHECK_NOT_NULL(out_val);
  LLVM_GEN_BE_CHECK_NOT_NULL(out_handled);
  *out_handled = false;

  llvm::StringRef rname = llvm::StringRef(builtin_name);
  if (!rname.starts_with("__builtin_ia32_")) {
    return llvm_gen_be_error_t::ok;
  }
  *out_handled = true;
  llvm::StringRef sub = rname.substr(15);

  llvm::StringRef norm_sub = sub;
  if (norm_sub.starts_with("sse_") || norm_sub.starts_with("avx_")) {
    norm_sub = norm_sub.substr(4);
  } else if (norm_sub.starts_with("sse2_") || norm_sub.starts_with("avx2_")) {
    norm_sub = norm_sub.substr(5);
  }

  // 1. Packed FP arithmetic: addps/addpd, subps/subpd, mulps/mulpd, divps/divpd (128/256/512-bit)
  if (args.size() >= 2 &&
      (norm_sub.starts_with("addp") || norm_sub.starts_with("add_p") ||
       norm_sub.starts_with("subp") || norm_sub.starts_with("sub_p") ||
       norm_sub.starts_with("mulp") || norm_sub.starts_with("mul_p") ||
       norm_sub.starts_with("divp") || norm_sub.starts_with("div_p")) &&
      args[0]->getType()->isFPOrFPVectorTy()) {
    if (norm_sub.starts_with("add")) {
      *out_val = be_state->builder->CreateFAdd(args[0], args[1], "x86.fadd");
    } else if (norm_sub.starts_with("sub")) {
      *out_val = be_state->builder->CreateFSub(args[0], args[1], "x86.fsub");
    } else if (norm_sub.starts_with("mul")) {
      *out_val = be_state->builder->CreateFMul(args[0], args[1], "x86.fmul");
    } else {
      *out_val = be_state->builder->CreateFDiv(args[0], args[1], "x86.fdiv");
    }
    return llvm_gen_be_error_t::ok;
  }

  // 1b. Native packed integer arithmetic, bitwise, and comparison ops
  if (args.size() >= 2 && args[0]->getType()->isIntOrIntVectorTy()) {
    if (norm_sub.starts_with("paddb") || norm_sub.starts_with("paddw") ||
        norm_sub.starts_with("paddd") || norm_sub.starts_with("paddq")) {
      *out_val = be_state->builder->CreateAdd(args[0], args[1], "x86.padd");
      return llvm_gen_be_error_t::ok;
    }
    if (norm_sub.starts_with("psubb") || norm_sub.starts_with("psubw") ||
        norm_sub.starts_with("psubd") || norm_sub.starts_with("psubq")) {
      *out_val = be_state->builder->CreateSub(args[0], args[1], "x86.psub");
      return llvm_gen_be_error_t::ok;
    }
    if (norm_sub.starts_with("pmullw") || norm_sub.starts_with("pmulld") ||
        norm_sub.starts_with("pmullq")) {
      *out_val = be_state->builder->CreateMul(args[0], args[1], "x86.pmull");
      return llvm_gen_be_error_t::ok;
    }
    if (norm_sub.starts_with("pandn")) {
      *out_val = be_state->builder->CreateAnd(
          be_state->builder->CreateNot(args[0]), args[1], "x86.pandn");
      return llvm_gen_be_error_t::ok;
    }
    if (norm_sub.starts_with("pand")) {
      *out_val = be_state->builder->CreateAnd(args[0], args[1], "x86.pand");
      return llvm_gen_be_error_t::ok;
    }
    if (norm_sub.starts_with("por")) {
      *out_val = be_state->builder->CreateOr(args[0], args[1], "x86.por");
      return llvm_gen_be_error_t::ok;
    }
    if (norm_sub.starts_with("pxor")) {
      *out_val = be_state->builder->CreateXor(args[0], args[1], "x86.pxor");
      return llvm_gen_be_error_t::ok;
    }
    if (norm_sub.starts_with("pcmpeqb") || norm_sub.starts_with("pcmpeqw") ||
        norm_sub.starts_with("pcmpeqd") || norm_sub.starts_with("pcmpeqq")) {
      llvm::Value* cmp = be_state->builder->CreateICmpEQ(args[0], args[1]);
      *out_val = be_state->builder->CreateSExt(cmp, args[0]->getType(), "x86.pcmpeq");
      return llvm_gen_be_error_t::ok;
    }
    if (norm_sub.starts_with("pcmpgtb") || norm_sub.starts_with("pcmpgtw") ||
        norm_sub.starts_with("pcmpgtd") || norm_sub.starts_with("pcmpgtq")) {
      llvm::Value* cmp = be_state->builder->CreateICmpSGT(args[0], args[1]);
      *out_val = be_state->builder->CreateSExt(cmp, args[0]->getType(), "x86.pcmpgt");
      return llvm_gen_be_error_t::ok;
    }
  }

  // 1c. Packed FP comparisons (cmpps / cmppd with immediate predicate) and unaligned vector loads/stores
  if ((norm_sub.starts_with("cmpps") || norm_sub.starts_with("cmppd")) &&
      args.size() >= 3 && args[0]->getType()->isVectorTy()) {
    auto* vty = llvm::cast<llvm::FixedVectorType>(args[0]->getType());
    uint64_t pred_imm = 0;
    if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(args[2])) {
      pred_imm = ci->getZExtValue() & 0x7u;
    }
    static const llvm::CmpInst::Predicate kPredMap[8] = {
        llvm::CmpInst::FCMP_OEQ, llvm::CmpInst::FCMP_OLT,
        llvm::CmpInst::FCMP_OLE, llvm::CmpInst::FCMP_UNO,
        llvm::CmpInst::FCMP_UNE, llvm::CmpInst::FCMP_UGE,
        llvm::CmpInst::FCMP_UGT, llvm::CmpInst::FCMP_ORD};
    llvm::Value* fcmp = be_state->builder->CreateFCmp(kPredMap[pred_imm], args[0], args[1]);
    unsigned lane_bits = static_cast<unsigned>(vty->getElementType()->getPrimitiveSizeInBits());
    llvm::Type* mask_ivty = llvm::FixedVectorType::get(
        llvm::Type::getIntNTy(*be_state->context, lane_bits), vty->getNumElements());
    llvm::Value* sext_m = be_state->builder->CreateSExt(fcmp, mask_ivty);
    *out_val = be_state->builder->CreateBitCast(sext_m, vty, "x86.cmpfp");
    return llvm_gen_be_error_t::ok;
  }
  if ((norm_sub.starts_with("loadups") || norm_sub.starts_with("loadupd") ||
       norm_sub.starts_with("loaddqu") || norm_sub.starts_with("lddqu")) &&
      !args.empty() && args[0]->getType()->isPointerTy()) {
    llvm::Type* load_ty = callee_ty
        ? callee_ty->getReturnType()
        : static_cast<llvm::Type*>(llvm::FixedVectorType::get(llvm::Type::getFloatTy(*be_state->context), 4));
    llvm::LoadInst* ld = be_state->builder->CreateLoad(load_ty, args[0], "x86.loadu");
    ld->setAlignment(llvm::Align(1));
    *out_val = ld;
    return llvm_gen_be_error_t::ok;
  }
  if ((norm_sub.starts_with("storeups") || norm_sub.starts_with("storeupd") ||
       norm_sub.starts_with("storedqu")) &&
      args.size() >= 2 && args[0]->getType()->isPointerTy()) {
    llvm::StoreInst* st = be_state->builder->CreateStore(args[1], args[0]);
    st->setAlignment(llvm::Align(1));
    *out_val = st;
    return llvm_gen_be_error_t::ok;
  }

  // 2. Packed FP sqrt, max, min, fma (sqrtps/sqrtpd, maxps/maxpd, minps/minpd, vfmadd*)
  if (norm_sub.starts_with("sqrtp") && !args.empty()) {
    llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(
        be_state->module.get(), llvm::Intrinsic::sqrt, {args[0]->getType()});
    *out_val = be_state->builder->CreateCall(fn, {args[0]});
    return llvm_gen_be_error_t::ok;
  }
  if ((norm_sub.starts_with("maxp") || norm_sub.starts_with("minp")) && args.size() >= 2) {
    llvm::Intrinsic::ID iid = norm_sub.starts_with("maxp") ? llvm::Intrinsic::maxnum : llvm::Intrinsic::minnum;
    llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(
        be_state->module.get(), iid, {args[0]->getType()});
    *out_val = be_state->builder->CreateCall(fn, {args[0], args[1]});
    return llvm_gen_be_error_t::ok;
  }
  if (norm_sub.starts_with("vfmadd") && args.size() >= 3) {
    llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(
        be_state->module.get(), llvm::Intrinsic::fma, {args[0]->getType()});
    *out_val = be_state->builder->CreateCall(fn, {args[0], args[1], args[2]});
    return llvm_gen_be_error_t::ok;
  }

  // 3. Packed integer saturating arithmetic: paddsb/paddsw, psubsb/psubsw, paddusb/paddusw, psubusb/psubusw
  if ((norm_sub.starts_with("paddsb") || norm_sub.starts_with("paddsw") ||
       norm_sub.starts_with("psubsb") || norm_sub.starts_with("psubsw") ||
       norm_sub.starts_with("paddusb") || norm_sub.starts_with("paddusw") ||
       norm_sub.starts_with("psubusb") || norm_sub.starts_with("psubusw")) &&
      args.size() >= 2) {
    llvm::Intrinsic::ID iid = llvm::Intrinsic::sadd_sat;
    if (norm_sub.starts_with("psubs")) {
      iid = llvm::Intrinsic::ssub_sat;
    } else if (norm_sub.starts_with("paddus")) {
      iid = llvm::Intrinsic::uadd_sat;
    } else if (norm_sub.starts_with("psubus")) {
      iid = llvm::Intrinsic::usub_sat;
    }
    llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(
        be_state->module.get(), iid, {args[0]->getType()});
    *out_val = be_state->builder->CreateCall(fn, {args[0], args[1]});
    return llvm_gen_be_error_t::ok;
  }

  // 4. Vector extract / insert / shuffle / blend builtins: vec_ext_*, vec_set_*, pshufd*, shufps*, shufpd*, pblendw*
  if (sub.starts_with("vec_ext_") && args.size() >= 2) {
    *out_val = be_state->builder->CreateExtractElement(args[0], args[1], "x86.vecext");
    return llvm_gen_be_error_t::ok;
  }
  if (sub.starts_with("vec_set_") && args.size() >= 3) {
    *out_val = be_state->builder->CreateInsertElement(args[0], args[1], args[2], "x86.vecset");
    return llvm_gen_be_error_t::ok;
  }
  if ((norm_sub.starts_with("pshufd") || norm_sub.starts_with("shufps") ||
       norm_sub.starts_with("shufpd") || norm_sub.starts_with("pblendw")) &&
      args.size() >= 2 && args[0]->getType()->isVectorTy()) {
    auto* vty = llvm::cast<llvm::FixedVectorType>(args[0]->getType());
    unsigned nel = vty->getNumElements();
    uint64_t imm = 0;
    if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(args.back())) {
      imm = ci->getZExtValue();
    }
    std::vector<int> mask(nel, 0);
    if (norm_sub.starts_with("pblendw")) {
      for (unsigned i = 0; i < nel; ++i) {
        bool pick_rhs = ((imm >> (i % 8)) & 0x1u) != 0;
        mask[i] = static_cast<int>(pick_rhs ? (nel + i) : i);
      }
    } else if (norm_sub.starts_with("shufpd")) {
      for (unsigned i = 0; i < nel; ++i) {
        unsigned base = (i & ~1u) + ((i & 1u) ? nel : 0u);
        mask[i] = static_cast<int>(base + ((imm >> i) & 0x1u));
      }
    } else {
      for (unsigned i = 0; i < nel; ++i) {
        unsigned lane = (imm >> ((i % 4) * 2)) & 0x3u;
        if (norm_sub.starts_with("shufps") && i >= nel / 2) {
          lane += nel;
        }
        mask[i] = static_cast<int>(lane);
      }
    }
    llvm::Value* rhs = (args.size() >= 3) ? args[1] : llvm::PoisonValue::get(vty);
    *out_val = be_state->builder->CreateShuffleVector(args[0], rhs, mask, "x86.shuf");
    return llvm_gen_be_error_t::ok;
  }

  // 5. Streaming / nontemporal stores (movntps, movntpd, movntdq, movnti) and masked loads/stores
  if (sub.starts_with("movnt") && args.size() >= 2) {
    llvm::StoreInst* st = be_state->builder->CreateStore(args[1], args[0]);
    llvm::MDNode* nt_md = llvm::MDNode::get(
        *be_state->context,
        llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 1)));
    st->setMetadata(llvm::LLVMContext::MD_nontemporal, nt_md);
    *out_val = st;
    return llvm_gen_be_error_t::ok;
  }
  if (sub.starts_with("maskload") && args.size() >= 2) {
    auto* mask_vty = llvm::cast<llvm::VectorType>(args[1]->getType());
    llvm::Value* zero_v = llvm::Constant::getNullValue(mask_vty);
    llvm::Value* bool_mask = be_state->builder->CreateICmpSLT(args[1], zero_v, "x86.mask");
    llvm::Value* passthru = llvm::Constant::getNullValue(mask_vty);
    *out_val = be_state->builder->CreateMaskedLoad(mask_vty, args[0], llvm::Align(16), bool_mask, passthru);
    return llvm_gen_be_error_t::ok;
  }
  if (sub.starts_with("maskstore") && args.size() >= 3) {
    auto* mask_vty = llvm::cast<llvm::VectorType>(args[1]->getType());
    llvm::Value* zero_v = llvm::Constant::getNullValue(mask_vty);
    llvm::Value* bool_mask = be_state->builder->CreateICmpSLT(args[1], zero_v, "x86.mask");
    *out_val = be_state->builder->CreateMaskedStore(args[2], args[0], llvm::Align(16), bool_mask);
    return llvm_gen_be_error_t::ok;
  }

  // 6. System, fence, cycle counter, BMI1/BMI2, and AVX-512 k-mask builtins
  if (sub == "pause") {
    return emit_validated_llvm_intrinsic("llvm.x86.sse2.pause", args, callee_ty, out_val);
  }
  if (sub == "sfence") {
    return emit_validated_llvm_intrinsic("llvm.x86.sse.sfence", args, callee_ty, out_val);
  }
  if (sub == "mfence" || sub == "lfence") {
    return emit_validated_llvm_intrinsic("llvm.x86.sse2." + sub.str(), args, callee_ty, out_val);
  }
  if (sub == "vzeroall" || sub == "avx_vzeroall") {
    return emit_validated_llvm_intrinsic("llvm.x86.avx.vzeroall", {}, callee_ty, out_val);
  }
  if (sub == "rdtsc") {
    llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(
        be_state->module.get(), llvm::Intrinsic::readcyclecounter);
    *out_val = be_state->builder->CreateCall(fn);
    return llvm_gen_be_error_t::ok;
  }
  if ((sub.starts_with("tzcnt") || sub.starts_with("lzcnt")) && !args.empty()) {
    llvm::Intrinsic::ID iid = sub.starts_with("tzcnt") ? llvm::Intrinsic::cttz : llvm::Intrinsic::ctlz;
    llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(
        be_state->module.get(), iid, {args[0]->getType()});
    *out_val = be_state->builder->CreateCall(fn, {args[0], be_state->builder->getFalse()});
    return llvm_gen_be_error_t::ok;
  }
  if (sub.starts_with("kand") && args.size() >= 2) {
    *out_val = be_state->builder->CreateAnd(args[0], args[1], "x86.kand");
    return llvm_gen_be_error_t::ok;
  }
  if (sub.starts_with("kor") && !sub.starts_with("kortest") && args.size() >= 2) {
    *out_val = be_state->builder->CreateOr(args[0], args[1], "x86.kor");
    return llvm_gen_be_error_t::ok;
  }
  if (sub.starts_with("kortestc") && args.size() >= 2) {
    llvm::Value* kor_v = be_state->builder->CreateOr(args[0], args[1]);
    llvm::Value* all_ones = llvm::ConstantInt::getSigned(kor_v->getType(), -1);
    llvm::Value* is_all = be_state->builder->CreateICmpEQ(kor_v, all_ones);
    *out_val = be_state->builder->CreateZExt(is_all, llvm::Type::getInt32Ty(*be_state->context), "x86.kortestc");
    return llvm_gen_be_error_t::ok;
  }
  if (sub.starts_with("kxnor") && args.size() >= 2) {
    *out_val = be_state->builder->CreateNot(be_state->builder->CreateXor(args[0], args[1]), "x86.kxnor");
    return llvm_gen_be_error_t::ok;
  }
  if (sub.starts_with("kadd") && args.size() >= 2) {
    *out_val = be_state->builder->CreateAdd(args[0], args[1], "x86.kadd");
    return llvm_gen_be_error_t::ok;
  }
  if ((sub.starts_with("pdep") || sub.starts_with("pext")) &&
      args.size() >= 2 && args[0]->getType()->isIntegerTy()) {
#if LLVM_VERSION_MAJOR >= 20
    std::string gen_name = sub.starts_with("pdep") ? "llvm.pdep" : "llvm.pext";
    return emit_validated_llvm_intrinsic(gen_name, args, callee_ty, out_val);
#else
    bool is_64 = args[0]->getType()->isIntegerTy(64);
    std::string bmi_stem = sub.starts_with("pdep") ? "pdep." : "pext.";
    return emit_validated_llvm_intrinsic(
        "llvm.x86.bmi." + bmi_stem + (is_64 ? "64" : "32"), args, callee_ty, out_val);
#endif
  }
  if ((sub.starts_with("bextr") || sub.starts_with("bzhi")) &&
      args.size() >= 2 && args[0]->getType()->isIntegerTy()) {
    bool is_64 = args[0]->getType()->isIntegerTy(64);
    std::string bmi_stem = sub.starts_with("bextr") ? "bextr." : "bzhi.";
    return emit_validated_llvm_intrinsic(
        "llvm.x86.bmi." + bmi_stem + (is_64 ? "64" : "32"), args, callee_ty, out_val);
  }
  if (sub.starts_with("crc32") && args.size() >= 2) {
    std::string crc_name = "llvm.x86.sse42.crc32.32.32";
    if (sub.ends_with("qi")) crc_name = "llvm.x86.sse42.crc32.32.8";
    else if (sub.ends_with("hi")) crc_name = "llvm.x86.sse42.crc32.32.16";
    else if (sub.ends_with("di")) crc_name = "llvm.x86.sse42.crc32.64.64";
    return emit_validated_llvm_intrinsic(crc_name, args, callee_ty, out_val);
  }
  if (sub.starts_with("aes") && sub.ends_with("128")) {
    std::string aes_stem = sub.substr(0, sub.size() - 3).str();
    return emit_validated_llvm_intrinsic("llvm.x86.aesni." + aes_stem, args, callee_ty, out_val);
  }

  // 7. Validated x86 table intrinsic lookup (avx512, avx2, avx, amx, sse, aes, sha)
  std::string intr_name;
  if (sub.starts_with("avx512_")) {
    intr_name = "llvm.x86.avx512." + sub.substr(7).str();
  } else if (sub.starts_with("avx2_")) {
    intr_name = "llvm.x86.avx2." + sub.substr(5).str();
  } else if (sub.starts_with("avx_")) {
    intr_name = "llvm.x86.avx." + sub.substr(4).str();
  } else if (sub.starts_with("amx_")) {
    intr_name = "llvm.x86." + sub.substr(4).str();
  } else if (sub.starts_with("aes")) {
    intr_name = "llvm.x86.aesni." + sub.str();
  } else if (sub.starts_with("sha")) {
    intr_name = "llvm.x86." + sub.str();
  } else {
    intr_name = "llvm.x86." + sub.str();
  }
  return emit_validated_llvm_intrinsic(intr_name, args, callee_ty, out_val);
}

/**
 * @brief Lowers an ARM32 or AArch64 target builtin (`__builtin_neon_*`, `__builtin_sve_*`, `__builtin_arm_*`) into LLVM IR.
 * @details Handles AArch64 NEON vector arithmetic, saturating/rounding operations, horizontal reductions,
 *          SVE/SVE2 predicated scalable vector operations, CRC32, crypto extensions, memory barriers
 *          (`dmb`, `dsb`, `isb`), system registers (`rsr64`, `wsr64`), and hint instructions.
 * @param[in] expr Pointer to the EDG call expression node (may be null for direct synthetic calls).
 * @param[in] bfk The EDG `a_builtin_function_kind` code for the target builtin.
 * @param[in] builtin_name Null-terminated C string name of the builtin routine.
 * @param[in] args Lowered LLVM argument values for the call.
 * @param[in] callee_ty Lowered LLVM FunctionType of the target builtin (may be null).
 * @param[out] out_val Receives the lowered `llvm::Value*` when handled.
 * @param[out] out_handled Receives `true` if the builtin was recognized and lowered.
 * @return `llvm_gen_be_error_t::ok` on success, or `builtin_lowering_failed` if invalid.
 */
llvm_gen_be_error_t llvm_lower_arm_aarch64_builtin(
    [[maybe_unused]] an_expr_node_ptr expr,
    [[maybe_unused]] a_builtin_function_kind bfk,
    const char* builtin_name,
    const std::vector<llvm::Value*>& args,
    llvm::FunctionType* callee_ty,
    llvm::Value** out_val,
    bool* out_handled) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(builtin_name);
  LLVM_GEN_BE_CHECK_NOT_NULL(out_val);
  LLVM_GEN_BE_CHECK_NOT_NULL(out_handled);
  *out_handled = false;

  llvm::StringRef rname = llvm::StringRef(builtin_name);
  if (rname.starts_with("__builtin_neon_")) {
    *out_handled = true;
    llvm::StringRef sub = rname.substr(15);
    if ((sub.starts_with("vgetq_lane_") || sub.starts_with("vget_lane_")) &&
        args.size() >= 2 && args[0]->getType()->isVectorTy()) {
      llvm::Value* lane = be_state->builder->CreateExtractElement(args[0], args[1], "neon.get_lane");
      if (callee_ty && lane->getType() != callee_ty->getReturnType()) {
        llvm::Type* ret_ty = callee_ty->getReturnType();
        lane = (lane->getType()->isIntegerTy() && ret_ty->isIntegerTy())
                   ? be_state->builder->CreateIntCast(lane, ret_ty, true)
                   : be_state->builder->CreateBitCast(lane, ret_ty);
      }
      *out_val = lane;
      return llvm_gen_be_error_t::ok;
    }
    if ((sub.starts_with("vsetq_lane_") || sub.starts_with("vset_lane_")) &&
        args.size() >= 3 && args[1]->getType()->isVectorTy()) {
      llvm::Type* lane_ty = llvm::cast<llvm::VectorType>(args[1]->getType())->getElementType();
      llvm::Value* val = args[0];
      if (val->getType() != lane_ty) {
        val = (val->getType()->isIntegerTy() && lane_ty->isIntegerTy())
                  ? be_state->builder->CreateIntCast(val, lane_ty, false)
                  : be_state->builder->CreateBitCast(val, lane_ty);
      }
      *out_val = be_state->builder->CreateInsertElement(args[1], val, args[2], "neon.set_lane");
      return llvm_gen_be_error_t::ok;
    }
    if ((sub.starts_with("vld1q_") || sub.starts_with("vld1_")) && !args.empty()) {
      llvm::Type* load_vty = (callee_ty && callee_ty->getReturnType()->isVectorTy())
                                 ? callee_ty->getReturnType()
                                 : llvm::FixedVectorType::get(
                                       llvm::Type::getInt8Ty(*be_state->context),
                                       sub.starts_with("vld1q_") ? 16U : 8U);
      llvm::LoadInst* ld = be_state->builder->CreateLoad(load_vty, args[0], "neon.vld1");
      ld->setAlignment(llvm::Align(1));
      *out_val = ld;
      return llvm_gen_be_error_t::ok;
    }
    if ((sub.starts_with("vst1q_") || sub.starts_with("vst1_")) && args.size() >= 2) {
      llvm::StoreInst* st = be_state->builder->CreateStore(args[1], args[0]);
      st->setAlignment(llvm::Align(1));
      *out_val = st;
      return llvm_gen_be_error_t::ok;
    }
    if ((sub.starts_with("vfmaq_") || sub.starts_with("vfma_")) && args.size() >= 3) {
      llvm::Type* orig_ty = args[0]->getType();
      llvm::Type* fma_ty = orig_ty;
      if (!fma_ty->isFPOrFPVectorTy() && orig_ty->isVectorTy()) {
        unsigned total_bits = static_cast<unsigned>(orig_ty->getPrimitiveSizeInBits());
        uint64_t tag = 41;
        if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(args.back())) {
          tag = ci->getZExtValue();
        }
        llvm::Type* lane_fp = llvm::Type::getFloatTy(*be_state->context);
        if ((tag & 0x1FU) == 10U) {
          lane_fp = llvm::Type::getDoubleTy(*be_state->context);
        } else if ((tag & 0x1FU) == 8U) {
          lane_fp = llvm::Type::getHalfTy(*be_state->context);
        }
        unsigned lane_bits = static_cast<unsigned>(lane_fp->getPrimitiveSizeInBits());
        fma_ty = llvm::FixedVectorType::get(lane_fp, total_bits / lane_bits);
      }
      if (fma_ty->isFPOrFPVectorTy()) {
        llvm::Value* a0 = (orig_ty == fma_ty) ? args[0] : be_state->builder->CreateBitCast(args[0], fma_ty);
        llvm::Value* a1 = (args[1]->getType() == fma_ty) ? args[1] : be_state->builder->CreateBitCast(args[1], fma_ty);
        llvm::Value* a2 = (args[2]->getType() == fma_ty) ? args[2] : be_state->builder->CreateBitCast(args[2], fma_ty);
        llvm::Function* fma_fn = EDG_LLVM_GET_INTRINSIC_DECL(
            be_state->module.get(), llvm::Intrinsic::fma, {fma_ty});
        llvm::Value* res = be_state->builder->CreateCall(fma_fn, {a1, a2, a0}, "neon.vfma");
        *out_val = (orig_ty == fma_ty) ? res : be_state->builder->CreateBitCast(res, orig_ty);
        return llvm_gen_be_error_t::ok;
      }
    }
    if (sub.starts_with("vaddv") && !args.empty()) {
      *out_val = be_state->builder->CreateAddReduce(args[0]);
      return llvm_gen_be_error_t::ok;
    }
    if (sub.starts_with("vmaxv") && !args.empty()) {
      *out_val = be_state->builder->CreateIntMaxReduce(args[0], true);
      return llvm_gen_be_error_t::ok;
    }
    if ((sub.starts_with("vadd") || sub.starts_with("vsub") || sub.starts_with("vmul")) &&
        args.size() >= 2 && args[0]->getType() == args[1]->getType()) {
      bool fp = args[0]->getType()->isFPOrFPVectorTy();
      if (sub.starts_with("vadd")) *out_val = fp ? be_state->builder->CreateFAdd(args[0], args[1]) : be_state->builder->CreateAdd(args[0], args[1]);
      else if (sub.starts_with("vsub")) *out_val = fp ? be_state->builder->CreateFSub(args[0], args[1]) : be_state->builder->CreateSub(args[0], args[1]);
      else *out_val = fp ? be_state->builder->CreateFMul(args[0], args[1]) : be_state->builder->CreateMul(args[0], args[1]);
      return llvm_gen_be_error_t::ok;
    }
    return emit_validated_llvm_intrinsic("llvm.aarch64.neon." + sub.str(), args, callee_ty, out_val);
  }

  if (rname.starts_with("__builtin_sve_") || rname.starts_with("__sve_")) {
    *out_handled = true;
    llvm::StringRef sub = rname.starts_with("__builtin_sve_") ? rname.substr(14) : rname.substr(6);
    if (sub == "svcntb" || sub == "svcnth" || sub == "svcntw" || sub == "svcntd") {
      uint64_t base_elts = (sub == "svcntb") ? 16ULL : ((sub == "svcnth") ? 8ULL : ((sub == "svcntw") ? 4ULL : 2ULL));
      llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
      llvm::Function* vscale_fn = EDG_LLVM_GET_INTRINSIC_DECL(
          be_state->module.get(), llvm::Intrinsic::vscale, {i64_ty});
      llvm::Value* vscale_val = be_state->builder->CreateCall(vscale_fn, {});
      *out_val = be_state->builder->CreateMul(vscale_val, llvm::ConstantInt::get(i64_ty, base_elts), "sve.cnt");
      return llvm_gen_be_error_t::ok;
    }
    if (sub.starts_with("svdup") && !args.empty()) {
      if (callee_ty && callee_ty->getReturnType()->isVectorTy()) {
        auto* ret_vty = llvm::cast<llvm::VectorType>(callee_ty->getReturnType());
        *out_val = be_state->builder->CreateVectorSplat(ret_vty->getElementCount(), args[0], "sve.dup");
        return llvm_gen_be_error_t::ok;
      }
      *out_val = be_state->builder->CreateVectorSplat(llvm::ElementCount::getScalable(4), args[0], "sve.dup");
      return llvm_gen_be_error_t::ok;
    }
    if ((sub.starts_with("svadd") || sub.starts_with("add") ||
         sub.starts_with("svsub") || sub.starts_with("svSub") || sub.starts_with("sub")) &&
        args.size() >= 2 && args[0]->getType() == args[1]->getType()) {
      bool fp = args[0]->getType()->isFPOrFPVectorTy();
      if (sub.starts_with("svadd") || sub.starts_with("add")) {
        *out_val = fp ? be_state->builder->CreateFAdd(args[0], args[1]) : be_state->builder->CreateAdd(args[0], args[1]);
      } else {
        *out_val = fp ? be_state->builder->CreateFSub(args[0], args[1]) : be_state->builder->CreateSub(args[0], args[1]);
      }
      return llvm_gen_be_error_t::ok;
    }
    if (sub.starts_with("sv")) {
      std::string intr_name = "llvm.aarch64.sve." + sub.substr(2).str();
      return emit_validated_llvm_intrinsic(intr_name, args, callee_ty, out_val);
    }
    return emit_validated_llvm_intrinsic("llvm.aarch64.sve." + sub.str(), args, callee_ty, out_val);
  }

  if (rname.starts_with("__builtin_arm_")) {
    *out_handled = true;
    llvm::StringRef sub = rname.substr(14);
    if (sub == "dmb" || sub == "dsb" || sub == "isb") {
      return emit_validated_llvm_intrinsic("llvm.aarch64." + sub.str(), args, callee_ty, out_val);
    }
    if (sub == "yield" || sub == "wfe" || sub == "wfi" || sub == "sev") {
      uint32_t hint_imm = (sub == "yield") ? 1u : ((sub == "wfe") ? 2u : ((sub == "wfi") ? 3u : 4u));
      llvm::Value* imm_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), hint_imm);
      return emit_validated_llvm_intrinsic("llvm.aarch64.hint", {imm_val}, callee_ty, out_val);
    }
    if (sub.starts_with("crc32")) {
      return emit_validated_llvm_intrinsic("llvm.aarch64." + sub.str(), args, callee_ty, out_val);
    }
    if (sub == "rsr64") {
      llvm::MDNode* reg_md = llvm::MDNode::get(
          *be_state->context, llvm::MDString::get(*be_state->context, "sp"));
      llvm::Value* md_arg = llvm::MetadataAsValue::get(*be_state->context, reg_md);
      llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(
          be_state->module.get(), llvm::Intrinsic::read_register, {llvm::Type::getInt64Ty(*be_state->context)});
      *out_val = be_state->builder->CreateCall(fn, {md_arg});
      return llvm_gen_be_error_t::ok;
    }
    if (sub == "wsr64" && !args.empty()) {
      llvm::MDNode* reg_md = llvm::MDNode::get(
          *be_state->context, llvm::MDString::get(*be_state->context, "sp"));
      llvm::Value* md_arg = llvm::MetadataAsValue::get(*be_state->context, reg_md);
      llvm::Value* v64 = be_state->builder->CreateIntCast(
          args.back(), llvm::Type::getInt64Ty(*be_state->context), false);
      llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(
          be_state->module.get(), llvm::Intrinsic::write_register, {llvm::Type::getInt64Ty(*be_state->context)});
      *out_val = be_state->builder->CreateCall(fn, {md_arg, v64});
      return llvm_gen_be_error_t::ok;
    }
    return emit_validated_llvm_intrinsic("llvm.arm." + sub.str(), args, callee_ty, out_val);
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a RISC-V target builtin (`__builtin_rvv_*`, `__builtin_riscv_*`) into LLVM IR.
 * @details Handles RISC-V Vector (`vsetvli`, `vsetvlimax`, scalable RVV arithmetic) and
 *          Zbb/Zbc/Zbs bit-manipulation intrinsics (`clmul`, `clmulh`, `orc_b`, `rev8`).
 * @param[in] expr Pointer to the EDG call expression node (may be null for direct synthetic calls).
 * @param[in] bfk The EDG `a_builtin_function_kind` code for the target builtin.
 * @param[in] builtin_name Null-terminated C string name of the builtin routine.
 * @param[in] args Lowered LLVM argument values for the call.
 * @param[in] callee_ty Lowered LLVM FunctionType of the target builtin (may be null).
 * @param[out] out_val Receives the lowered `llvm::Value*` when handled.
 * @param[out] out_handled Receives `true` if the builtin was recognized and lowered.
 * @return `llvm_gen_be_error_t::ok` on success, or `builtin_lowering_failed` if invalid.
 */
llvm_gen_be_error_t llvm_lower_riscv_builtin(
    [[maybe_unused]] an_expr_node_ptr expr,
    [[maybe_unused]] a_builtin_function_kind bfk,
    const char* builtin_name,
    const std::vector<llvm::Value*>& args,
    llvm::FunctionType* callee_ty,
    llvm::Value** out_val,
    bool* out_handled) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(builtin_name);
  LLVM_GEN_BE_CHECK_NOT_NULL(out_val);
  LLVM_GEN_BE_CHECK_NOT_NULL(out_handled);
  *out_handled = false;

  llvm::StringRef rname = llvm::StringRef(builtin_name);
  if (rname.starts_with("__builtin_rvv_vsetvli") && args.size() >= 2) {
    *out_handled = true;
    bool is_max = rname.contains("max");
    llvm::Type* xlen_ty = args[0]->getType()->isIntegerTy(64)
                              ? llvm::Type::getInt64Ty(*be_state->context)
                              : llvm::Type::getInt32Ty(*be_state->context);
    if (!is_max && args.size() >= 3) {
      llvm::Intrinsic::ID iid = EDG_LLVM_LOOKUP_INTRINSIC_ID("llvm.riscv.vsetvli");
      llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), iid, {xlen_ty});
      llvm::Value* a0 = be_state->builder->CreateIntCast(args[0], xlen_ty, false);
      llvm::Value* a1 = be_state->builder->CreateIntCast(args[1], xlen_ty, false);
      llvm::Value* a2 = be_state->builder->CreateIntCast(args[2], xlen_ty, false);
      *out_val = be_state->builder->CreateCall(fn, {a0, a1, a2});
      return llvm_gen_be_error_t::ok;
    }
    llvm::Intrinsic::ID iid = EDG_LLVM_LOOKUP_INTRINSIC_ID("llvm.riscv.vsetvlimax");
    llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), iid, {xlen_ty});
    llvm::Value* a0 = be_state->builder->CreateIntCast(args[0], xlen_ty, false);
    llvm::Value* a1 = be_state->builder->CreateIntCast(args[1], xlen_ty, false);
    *out_val = be_state->builder->CreateCall(fn, {a0, a1});
    return llvm_gen_be_error_t::ok;
  }

  if (rname.starts_with("__builtin_rvv_") || rname.starts_with("__riscv_v")) {
    *out_handled = true;
    llvm::StringRef sub = rname.starts_with("__builtin_rvv_") ? rname.substr(14) : rname.substr(8);
    if (sub.starts_with("vadd") && args.size() >= 2 && args[0]->getType() == args[1]->getType()) {
      *out_val = args[0]->getType()->isFPOrFPVectorTy()
                     ? be_state->builder->CreateFAdd(args[0], args[1])
                     : be_state->builder->CreateAdd(args[0], args[1]);
      return llvm_gen_be_error_t::ok;
    }
    return emit_validated_llvm_intrinsic("llvm.riscv." + sub.str(), args, callee_ty, out_val);
  }

  if (rname.starts_with("__builtin_riscv_")) {
    *out_handled = true;
    llvm::StringRef sub = rname.substr(16);
    if (sub == "rev8" && !args.empty()) {
      llvm::Function* bswap = EDG_LLVM_GET_INTRINSIC_DECL(
          be_state->module.get(), llvm::Intrinsic::bswap, {args[0]->getType()});
      *out_val = be_state->builder->CreateCall(bswap, {args[0]});
      return llvm_gen_be_error_t::ok;
    }
#if LLVM_VERSION_MAJOR >= 19
    if (sub == "clmul") {
      return emit_validated_llvm_intrinsic("llvm.clmul", args, callee_ty, out_val);
    }
#endif
    return emit_validated_llvm_intrinsic("llvm.riscv." + sub.str(), args, callee_ty, out_val);
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers PowerPC, s390x, LoongArch, MIPS, SPARC, WebAssembly, NVPTX, and AMDGPU target builtins into LLVM IR.
 * @details Handles PowerPC Altivec/VSX (`__builtin_altivec_*`, `__builtin_vsx_*`), s390x vector
 *          (`__builtin_s390_*`), LoongArch LSX/LASX (`__builtin_lsx_*`, `__builtin_lasx_*`),
 *          WebAssembly SIMD128 (`__builtin_wasm_*`), NVPTX (`__nvvm_*`), and AMDGCN (`__builtin_amdgcn_*`) intrinsics.
 * @param[in] expr Pointer to the EDG call expression node (may be null for direct synthetic calls).
 * @param[in] bfk The EDG `a_builtin_function_kind` code for the target builtin.
 * @param[in] builtin_name Null-terminated C string name of the builtin routine.
 * @param[in] args Lowered LLVM argument values for the call.
 * @param[in] callee_ty Lowered LLVM FunctionType of the target builtin (may be null).
 * @param[out] out_val Receives the lowered `llvm::Value*` when handled.
 * @param[out] out_handled Receives `true` if the builtin was recognized and lowered.
 * @return `llvm_gen_be_error_t::ok` on success, or `builtin_lowering_failed` if invalid.
 */
llvm_gen_be_error_t llvm_lower_other_arch_builtin(
    [[maybe_unused]] an_expr_node_ptr expr,
    [[maybe_unused]] a_builtin_function_kind bfk,
    const char* builtin_name,
    const std::vector<llvm::Value*>& args,
    llvm::FunctionType* callee_ty,
    llvm::Value** out_val,
    bool* out_handled) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(builtin_name);
  LLVM_GEN_BE_CHECK_NOT_NULL(out_val);
  LLVM_GEN_BE_CHECK_NOT_NULL(out_handled);
  *out_handled = false;

  llvm::StringRef rname = llvm::StringRef(builtin_name);
  if (rname.starts_with("__builtin_altivec_")) {
    *out_handled = true;
    llvm::StringRef sub = rname.substr(18);
    if (sub.starts_with("vadd") && args.size() >= 2 && args[0]->getType()->isIntOrIntVectorTy()) {
      *out_val = be_state->builder->CreateAdd(args[0], args[1], "altivec.vadd");
      return llvm_gen_be_error_t::ok;
    }
    return emit_validated_llvm_intrinsic("llvm.ppc.altivec." + sub.str(), args, callee_ty, out_val);
  }
  if (rname.starts_with("__builtin_vsx_")) {
    *out_handled = true;
    llvm::StringRef sub = rname.substr(14);
    if (sub.starts_with("xvadd") && args.size() >= 2 && args[0]->getType()->isFPOrFPVectorTy()) {
      *out_val = be_state->builder->CreateFAdd(args[0], args[1], "vsx.xvadd");
      return llvm_gen_be_error_t::ok;
    }
    return emit_validated_llvm_intrinsic("llvm.ppc.vsx." + sub.str(), args, callee_ty, out_val);
  }
  if (rname.starts_with("__builtin_s390_")) {
    *out_handled = true;
    return emit_validated_llvm_intrinsic("llvm.s390." + rname.substr(15).str(), args, callee_ty, out_val);
  }
  if (rname.starts_with("__builtin_lsx_")) {
    *out_handled = true;
    llvm::StringRef sub = rname.substr(14);
    if (sub.starts_with("vadd") && args.size() >= 2 && args[0]->getType()->isIntOrIntVectorTy()) {
      *out_val = be_state->builder->CreateAdd(args[0], args[1], "lsx.vadd");
      return llvm_gen_be_error_t::ok;
    }
    return emit_validated_llvm_intrinsic("llvm.loongarch.lsx." + sub.str(), args, callee_ty, out_val);
  }
  if (rname.starts_with("__builtin_lasx_")) {
    *out_handled = true;
    llvm::StringRef sub = rname.substr(15);
    if (sub.starts_with("xvadd") && args.size() >= 2 && args[0]->getType()->isIntOrIntVectorTy()) {
      *out_val = be_state->builder->CreateAdd(args[0], args[1], "lasx.xvadd");
      return llvm_gen_be_error_t::ok;
    }
    return emit_validated_llvm_intrinsic("llvm.loongarch.lasx." + sub.str(), args, callee_ty, out_val);
  }
  if (rname.starts_with("__builtin_wasm_")) {
    *out_handled = true;
    llvm::StringRef sub = rname.substr(15);
    if (sub.starts_with("abs_") && !args.empty() && args[0]->getType()->isIntOrIntVectorTy()) {
      llvm::Function* abs_fn = EDG_LLVM_GET_INTRINSIC_DECL(
          be_state->module.get(), llvm::Intrinsic::abs, {args[0]->getType()});
      *out_val = be_state->builder->CreateCall(abs_fn, {args[0], be_state->builder->getFalse()});
      return llvm_gen_be_error_t::ok;
    }
    return emit_validated_llvm_intrinsic("llvm.wasm." + sub.str(), args, callee_ty, out_val);
  }
  if (rname.starts_with("__nvvm_")) {
    *out_handled = true;
    llvm::StringRef sub = rname.substr(7);
    if (sub == "bar0") {
#if LLVM_VERSION_MAJOR >= 20
      llvm::Value* id0 = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0);
      return emit_validated_llvm_intrinsic("llvm.nvvm.barrier.cta.sync.aligned.all", {id0}, callee_ty, out_val);
#else
      return emit_validated_llvm_intrinsic("llvm.nvvm.barrier0", {}, callee_ty, out_val);
#endif
    }
    return emit_validated_llvm_intrinsic("llvm.nvvm." + sub.str(), args, callee_ty, out_val);
  }
  if (rname.starts_with("__builtin_amdgcn_")) {
    *out_handled = true;
    return emit_validated_llvm_intrinsic("llvm.amdgcn." + rname.substr(17).str(), args, callee_ty, out_val);
  }

  return llvm_gen_be_error_t::ok;
}

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
 * @return `llvm_gen_be_error_t::ok` on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_target_builtin(
    an_expr_node_ptr expr,
    a_builtin_function_kind bfk,
    const std::string& rname,
    const std::vector<llvm::Value*>& args,
    llvm::FunctionType* callee_ty,
    llvm::Value** out_val,
    bool* out_handled) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(out_val);
  LLVM_GEN_BE_CHECK_NOT_NULL(out_handled);
  *out_handled = false;

  LLVM_GEN_BE_CHECK(
      llvm_lower_x86_builtin(expr, bfk, rname.c_str(), args, callee_ty, out_val, out_handled));
  if (*out_handled) return llvm_gen_be_error_t::ok;

  LLVM_GEN_BE_CHECK(
      llvm_lower_arm_aarch64_builtin(expr, bfk, rname.c_str(), args, callee_ty, out_val, out_handled));
  if (*out_handled) return llvm_gen_be_error_t::ok;

  LLVM_GEN_BE_CHECK(
      llvm_lower_riscv_builtin(expr, bfk, rname.c_str(), args, callee_ty, out_val, out_handled));
  if (*out_handled) return llvm_gen_be_error_t::ok;

  return llvm_lower_other_arch_builtin(expr, bfk, rname.c_str(), args, callee_ty, out_val, out_handled);
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_LLVM_GEN_BE */
