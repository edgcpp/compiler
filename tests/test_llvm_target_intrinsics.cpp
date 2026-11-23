/**
 * @file test_llvm_target_intrinsics.cpp
 * @ingroup llvm_gen_be_tests
 * @brief Unit tests for the target architecture SIMD and system intrinsic lowering subsystem.
 * @details Validates `src/llvm_gen_be_intrinsics.cpp` across x86/x86_64, ARM/AArch64, RISC-V,
 *          PowerPC, s390x, LoongArch, WebAssembly, NVPTX, and AMDGPU target builtins, ensuring
 *          every emitted intrinsic passes `llvm::verifyModule` and invalid names return
 *          `llvm_gen_be_error_t::builtin_lowering_failed`.
 */

#include "llvm_gen_be_intrinsics.h"
#include "llvm_gen_be_internal.h"
#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>

using namespace edg;

LLVMBackendState* edg::be_state = nullptr;

/**
 * @brief Tests null-pointer argument validation across all target intrinsic entry points.
 * @details Exercises `LLVM_GEN_BE_CHECK_NOT_NULL` branches in `llvm_lower_x86_builtin`,
 *          `llvm_lower_arm_aarch64_builtin`, `llvm_lower_riscv_builtin`,
 *          `llvm_lower_other_arch_builtin`, and `llvm_lower_target_builtin`.
 * @param[out] out_passed Receives `true` when all error checks pass.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t test_intrinsic_null_checks(bool* out_passed) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(out_passed);
  *out_passed = false;
  llvm::Value* val = nullptr;
  bool handled = false;
  std::vector<llvm::Value*> empty_args;
  a_builtin_function_kind bfk = static_cast<a_builtin_function_kind>(0);

  assert(llvm_lower_x86_builtin(nullptr, bfk, nullptr, empty_args, nullptr, &val, &handled) ==
         llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_x86_builtin(nullptr, bfk, "__builtin_ia32_pause", empty_args, nullptr, nullptr, &handled) ==
         llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_x86_builtin(nullptr, bfk, "__builtin_ia32_pause", empty_args, nullptr, &val, nullptr) ==
         llvm_gen_be_error_t::invalid_argument);

  assert(llvm_lower_arm_aarch64_builtin(nullptr, bfk, nullptr, empty_args, nullptr, &val, &handled) ==
         llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_arm_aarch64_builtin(nullptr, bfk, "__builtin_arm_dmb", empty_args, nullptr, nullptr, &handled) ==
         llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_arm_aarch64_builtin(nullptr, bfk, "__builtin_arm_dmb", empty_args, nullptr, &val, nullptr) ==
         llvm_gen_be_error_t::invalid_argument);

  assert(llvm_lower_riscv_builtin(nullptr, bfk, nullptr, empty_args, nullptr, &val, &handled) ==
         llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_riscv_builtin(nullptr, bfk, "__builtin_rvv_vadd", empty_args, nullptr, nullptr, &handled) ==
         llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_riscv_builtin(nullptr, bfk, "__builtin_rvv_vadd", empty_args, nullptr, &val, nullptr) ==
         llvm_gen_be_error_t::invalid_argument);

  assert(llvm_lower_other_arch_builtin(nullptr, bfk, nullptr, empty_args, nullptr, &val, &handled) ==
         llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_other_arch_builtin(nullptr, bfk, "__builtin_wasm_add", empty_args, nullptr, nullptr, &handled) ==
         llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_other_arch_builtin(nullptr, bfk, "__builtin_wasm_add", empty_args, nullptr, &val, nullptr) ==
         llvm_gen_be_error_t::invalid_argument);

  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_pause", empty_args, nullptr, nullptr, &handled) ==
         llvm_gen_be_error_t::invalid_argument);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_pause", empty_args, nullptr, &val, nullptr) ==
         llvm_gen_be_error_t::invalid_argument);

  *out_passed = true;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Tests x86/x86_64 SIMD, saturating, comparison, load/store, shuffle/blend, and system builtins.
 * @details Exercises SSE/AVX/AVX-512 packed FP & integer ops, comparisons, unaligned loads/stores,
 *          saturating integer ops, shuffles/blends, nontemporal/masked loads/stores, BMI, AES/SHA/CRC32,
 *          and verifies `builtin_lowering_failed` on unknown x86 intrinsic names.
 * @param[out] out_passed Receives `true` when all x86 intrinsic tests pass.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t test_x86_intrinsics(bool* out_passed) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(out_passed);
  *out_passed = false;
  llvm::LLVMContext& ctx = *be_state->context;
  llvm::IRBuilder<>& b = *be_state->builder;
  a_builtin_function_kind bfk = static_cast<a_builtin_function_kind>(0);

  llvm::Type* f32_ty = llvm::Type::getFloatTy(ctx);
  llvm::Type* f64_ty = llvm::Type::getDoubleTy(ctx);
  llvm::Type* i8_ty = llvm::Type::getInt8Ty(ctx);
  llvm::Type* i16_ty = llvm::Type::getInt16Ty(ctx);
  llvm::Type* i32_ty = llvm::Type::getInt32Ty(ctx);
  llvm::Type* i64_ty = llvm::Type::getInt64Ty(ctx);
  llvm::Type* v4f32_ty = llvm::FixedVectorType::get(f32_ty, 4);
  llvm::Type* v2f64_ty = llvm::FixedVectorType::get(f64_ty, 2);
  llvm::Type* v4i32_ty = llvm::FixedVectorType::get(i32_ty, 4);
  llvm::Type* v8i16_ty = llvm::FixedVectorType::get(i16_ty, 8);

  llvm::Value* vf1 = llvm::Constant::getNullValue(v4f32_ty);
  llvm::Value* vf2 = llvm::Constant::getNullValue(v4f32_ty);
  llvm::Value* vf3 = llvm::Constant::getNullValue(v4f32_ty);
  llvm::Value* vd1 = llvm::Constant::getNullValue(v2f64_ty);
  llvm::Value* vd2 = llvm::Constant::getNullValue(v2f64_ty);
  llvm::Value* vi1 = llvm::Constant::getNullValue(v4i32_ty);
  llvm::Value* vi2 = llvm::Constant::getNullValue(v4i32_ty);
  llvm::Value* vw1 = llvm::Constant::getNullValue(v8i16_ty);
  llvm::Value* vw2 = llvm::Constant::getNullValue(v8i16_ty);
  llvm::Value* idx0 = llvm::ConstantInt::get(i32_ty, 0);
  llvm::Value* scalar_i8 = llvm::ConstantInt::get(i8_ty, 1);
  llvm::Value* scalar_i16 = llvm::ConstantInt::get(i16_ty, 2);
  llvm::Value* scalar_i32 = llvm::ConstantInt::get(i32_ty, 42);
  llvm::Value* scalar_i64 = llvm::ConstantInt::get(i64_ty, 42);
  llvm::Value* dynamic_i32 = b.CreateAlloca(i32_ty);
  llvm::Value* dyn_loaded = b.CreateLoad(i32_ty, dynamic_i32);
  llvm::Value* ptr_val = b.CreateAlloca(v4f32_ty);

  llvm::Value* out = nullptr;
  bool handled = false;

  const char* fp_bin_ops[] = {
      "__builtin_ia32_addps", "__builtin_ia32_subps",
      "__builtin_ia32_mulps", "__builtin_ia32_divps",
      "__builtin_ia32_sse_add_ps", "__builtin_ia32_avx_sub_ps"
  };
  for (const char* name : fp_bin_ops) {
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, name, {vf1, vf2}, nullptr, &out, &handled));
    assert(handled && out != nullptr);
  }

  const char* int_bin_ops[] = {
      "__builtin_ia32_paddd128", "__builtin_ia32_psubd128", "__builtin_ia32_pmulld128",
      "__builtin_ia32_pand128", "__builtin_ia32_pandn128", "__builtin_ia32_por128",
      "__builtin_ia32_pxor128", "__builtin_ia32_pcmpeqd128", "__builtin_ia32_pcmpgtd128"
  };
  for (const char* name : int_bin_ops) {
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, name, {vi1, vi2}, nullptr, &out, &handled));
    assert(handled && out != nullptr);
  }

  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_cmpps", {vf1, vf2, idx0}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_cmppd", {vd1, vd2, dyn_loaded}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_loadups", {ptr_val}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  llvm::FunctionType* ldu_ft = llvm::FunctionType::get(v4i32_ty, {ptr_val->getType()}, false);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_loaddqu", {ptr_val}, ldu_ft, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_storeups", {ptr_val, vf1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);

  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_sqrtps", {vf1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_maxps", {vf1, vf2}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_minps", {vf1, vf2}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_vfmaddps", {vf1, vf2, vf3}, nullptr, &out, &handled));
  assert(handled && out != nullptr);

  const char* sat_ops[] = {
      "__builtin_ia32_paddsb128", "__builtin_ia32_paddsw128",
      "__builtin_ia32_psubsb128", "__builtin_ia32_psubsw128",
      "__builtin_ia32_paddusb128", "__builtin_ia32_paddusw128",
      "__builtin_ia32_psubusb128", "__builtin_ia32_psubusw128"
  };
  for (const char* name : sat_ops) {
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, name, {vi1, vi2}, nullptr, &out, &handled));
    assert(handled && out != nullptr);
  }

  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_vec_ext_v4si", {vi1, idx0}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_vec_set_v4si", {vi1, scalar_i32, idx0}, nullptr, &out, &handled));
  assert(handled && out != nullptr);

  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_pshufd", {vi1, idx0}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_pshufd", {vi1, dyn_loaded}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_shufps", {vf1, vf2, idx0}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_shufpd", {vd1, vd2, llvm::ConstantInt::get(i32_ty, 1)}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_pblendw128", {vw1, vw2, llvm::ConstantInt::get(i32_ty, 0xAA)}, nullptr, &out, &handled));
  assert(handled && out != nullptr);

  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_movntps", {ptr_val, vf1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_maskloadps", {ptr_val, vi1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_maskstoreps", {ptr_val, vi1, vf1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);

  const char* sys_ops[] = {
      "__builtin_ia32_pause", "__builtin_ia32_mfence",
      "__builtin_ia32_sfence", "__builtin_ia32_lfence",
      "__builtin_ia32_vzeroall", "__builtin_ia32_rdtsc"
  };
  for (const char* name : sys_ops) {
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, name, {}, nullptr, &out, &handled));
    assert(handled && out != nullptr);
  }

  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_tzcnt_u32", {scalar_i32}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_lzcnt_u32", {scalar_i32}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_kandhi", {scalar_i32, scalar_i32}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_korhi", {scalar_i32, scalar_i32}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_kortestchi", {scalar_i32, scalar_i32}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_kxnorhi", {scalar_i32, scalar_i32}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_kaddhi", {scalar_i32, scalar_i32}, nullptr, &out, &handled));
  assert(handled && out != nullptr);

  const char* bmi_ops[] = {
      "__builtin_ia32_bextr_u32", "__builtin_ia32_bzhi_si",
      "__builtin_ia32_pdep_si", "__builtin_ia32_pext_si"
  };
  for (const char* name : bmi_ops) {
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, name, {scalar_i32, scalar_i32}, nullptr, &out, &handled));
    assert(handled && out != nullptr);
  }
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_bextr_u64", {scalar_i64, scalar_i64}, nullptr, &out, &handled));
  assert(handled && out != nullptr);

  // Real registered AES, SHA, CRC32, AMX, and SSE2 clflush intrinsics (with operand bitcast coercion!)
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_aesenc128", {vi1, vi2}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_aeskeygenassist128", {vi1, scalar_i8}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_sha256msg1", {vi1, vi2}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_crc32qi", {scalar_i32, scalar_i8}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_crc32hi", {scalar_i32, scalar_i16}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_crc32si", {scalar_i32, scalar_i32}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_crc32di", {scalar_i64, scalar_i64}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_amx_tilezero", {scalar_i8}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_sse2_clflush", {ptr_val}, nullptr, &out, &handled));
  assert(handled && out != nullptr);

  // Unknown/bogus x86 intrinsics must return builtin_lowering_failed rather than invalid @llvm.* decls
  const char* bad_x86_ops[] = {
      "__builtin_ia32_avx512_CustomOp",
      "__builtin_ia32_avx2_CustomOp",
      "__builtin_ia32_avx_CustomOp",
      "__builtin_ia32_totally_nonexistent"
  };
  for (const char* bad_name : bad_x86_ops) {
    assert(llvm_lower_target_builtin(nullptr, bfk, bad_name, {vi1, vi2}, nullptr, &out, &handled) ==
           llvm_gen_be_error_t::builtin_lowering_failed);
  }
  // Wrong arg count / incompatible operand type on known intrinsic returns builtin_lowering_failed
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_sha256msg1", {vi1}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_sha256msg1", {vi1, scalar_i32}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);

  // Additional x86 branch coverage: prefix norms, all arithmetic/saturating/shuffle/load/store/BMI/CRC/AES/SHA variants & fallthroughs
  const char* more_fp_ops[] = {
      "__builtin_ia32_add_ps", "__builtin_ia32_sub_ps", "__builtin_ia32_mulp_s", "__builtin_ia32_mul_ps",
      "__builtin_ia32_div_ps", "__builtin_ia32_avx2_addps"
  };
  for (const char* mop : more_fp_ops) {
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, mop, {vf1, vf2}, nullptr, &out, &handled));
    assert(handled);
  }
  const char* more_int_ops[] = {
      "__builtin_ia32_paddb128", "__builtin_ia32_paddw128", "__builtin_ia32_paddq128",
      "__builtin_ia32_psubb128", "__builtin_ia32_psubw128", "__builtin_ia32_psubq128",
      "__builtin_ia32_pmullw128", "__builtin_ia32_pmullq128",
      "__builtin_ia32_pcmpeqb128", "__builtin_ia32_pcmpeqw128", "__builtin_ia32_pcmpeqq128",
      "__builtin_ia32_pcmpgtb128", "__builtin_ia32_pcmpgtw128", "__builtin_ia32_pcmpgtq128",
      "__builtin_ia32_paddsw128", "__builtin_ia32_psubsw128", "__builtin_ia32_paddusb128", "__builtin_ia32_psubusb128",
      "__builtin_ia32_paddsb128", "__builtin_ia32_psubsb128", "__builtin_ia32_paddusw128", "__builtin_ia32_psubusw128"
  };
  for (const char* mop : more_int_ops) {
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, mop, {vw1, vw2}, nullptr, &out, &handled));
    assert(handled);
  }
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_loadupd", {ptr_val}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_lddqu", {ptr_val}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_storeupd", {ptr_val, vd1}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_storedqu", {ptr_val, vi1}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_sqrtps", {vf1}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_maxps", {vf1, vf2}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_minps", {vf1, vf2}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_vfmaddps", {vf1, vf2, vf3}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_vec_ext_v4si", {vi1, idx0}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_vec_set_v4si", {vi1, scalar_i32, idx0}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_pshufd", {vi1, idx0}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_pshufd", {vi1, dyn_loaded}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_shufps", {vf1, vf2, idx0}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_shufpd", {vd1, vd2, idx0}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_pblendw128", {vw1, vw2, scalar_i32}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_movntps", {ptr_val, vf1}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_maskloadd", {ptr_val, vi1}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_maskstored", {ptr_val, vi1, vi2}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_sfence", {}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_mfence", {}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_lfence", {}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_vzeroall", {}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_avx_vzeroall", {}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_rdtsc", {}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_tzcnt_u32", {scalar_i32}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_lzcnt_u32", {scalar_i32}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_kandhi", {scalar_i16, scalar_i16}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_korhi", {scalar_i16, scalar_i16}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_kortestchi", {scalar_i16, scalar_i16}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_kxnorhi", {scalar_i16, scalar_i16}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_kaddhi", {scalar_i16, scalar_i16}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_pdep_si", {scalar_i32, scalar_i32}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_pext_di", {scalar_i64, scalar_i64}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_bextr_u32", {scalar_i32, scalar_i32}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_bextr_u64", {scalar_i64, scalar_i64}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_bzhi_si", {scalar_i32, scalar_i32}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_crc32qi", {scalar_i32, scalar_i8}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_crc32hi", {scalar_i32, scalar_i16}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_aesenc128", {vi1, vi2}, nullptr, &out, &handled));

  // Exercise coerce_intrinsic_operand: int cast, FP cast, cross-addrspace pointer cast, and non-matching pointer/int failure
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_bextr_u32", {scalar_i32, scalar_i64}, nullptr, &out, &handled));
  llvm::Value* f32_s = llvm::ConstantFP::get(f32_ty, 1.0);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__nvvm_d2f_rn", {f32_s}, nullptr, &out, &handled));
  // Cross-address-space pointer coercion (addrspace(3) -> addrspace(0) on clflush)
  llvm::Value* as3_ptr = llvm::ConstantPointerNull::get(llvm::PointerType::get(ctx, 3));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_sse2_clflush", {as3_ptr}, nullptr, &out, &handled));
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_sse2_clflush", {scalar_i32}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_bextr_u32", {scalar_i32, ptr_val}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);
  llvm::Value* f64_s = llvm::ConstantFP::get(f64_ty, 1.0);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_bextr_u32", {scalar_i32, f64_s}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_arm_wsr64", {}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);

  // Exercise fallthrough branches when arg counts / operand types do not match fast-path native lowerings
  const char* x86_short_arg_fallthroughs[] = {
      "__builtin_ia32_addps", "__builtin_ia32_paddd", "__builtin_ia32_cmpps",
      "__builtin_ia32_loadups", "__builtin_ia32_storeups", "__builtin_ia32_sqrtps",
      "__builtin_ia32_maxps", "__builtin_ia32_vfmaddps", "__builtin_ia32_paddsb",
      "__builtin_ia32_vec_ext_v4si", "__builtin_ia32_vec_set_v4si", "__builtin_ia32_pshufd",
      "__builtin_ia32_movntps", "__builtin_ia32_maskloadd", "__builtin_ia32_maskstored",
      "__builtin_ia32_tzcnt_u32", "__builtin_ia32_kandhi", "__builtin_ia32_korhi",
      "__builtin_ia32_kortestzhi", "__builtin_ia32_kortestchi", "__builtin_ia32_kxnorhi",
      "__builtin_ia32_kaddhi", "__builtin_ia32_pdep_si", "__builtin_ia32_bextr_u32",
      "__builtin_ia32_crc32si", "__builtin_ia32_aeskeygenassist", "__builtin_ia32_sha1rnds4"
  };
  for (const char* sname : x86_short_arg_fallthroughs) {
    llvm_gen_be_error_t ft_err = llvm_lower_target_builtin(nullptr, bfk, sname, {}, nullptr, &out, &handled);
    assert(ft_err == llvm_gen_be_error_t::builtin_lowering_failed);
  }
  // Mismatched operand types on 2+/3+ arg checks
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_addps", {vi1, vi2}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_cmpps", {scalar_i32, scalar_i32, scalar_i32}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_loadups", {scalar_i32}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_storeups", {scalar_i32, vf1}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_pshufd", {scalar_i32, scalar_i32}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_pdep_si", {vf1, vf2}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_ia32_bextr_u32", {vf1, vf2}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);

  *out_passed = true;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Tests ARM/AArch64, RISC-V, PowerPC, s390x, LoongArch, WebAssembly, NVPTX, and AMDGPU target builtins.
 * @details Exercises NEON reductions and table intrinsics, SVE intrinsics, ARM system/barrier/hint/crc32/register
 *          builtins, RISC-V `vsetvli`/`vsetvlimax`/RVV/Zbb/Zbc (`rev8`, `clmul`, `clmulh`), and Altivec/VSX/s390/LSX/LASX/Wasm/NVPTX/AMDGCN builtins.
 * @param[out] out_passed Receives `true` when all non-x86 target intrinsic tests pass.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t test_non_x86_intrinsics(bool* out_passed) noexcept {
  LLVM_GEN_BE_CHECK_NOT_NULL(out_passed);
  *out_passed = false;
  llvm::LLVMContext& ctx = *be_state->context;
  a_builtin_function_kind bfk = static_cast<a_builtin_function_kind>(0);

  llvm::Type* i8_ty = llvm::Type::getInt8Ty(ctx);
  llvm::Type* i32_ty = llvm::Type::getInt32Ty(ctx);
  llvm::Type* i64_ty = llvm::Type::getInt64Ty(ctx);
  llvm::Type* f32_ty = llvm::Type::getFloatTy(ctx);
  llvm::Type* v16i8_ty = llvm::FixedVectorType::get(i8_ty, 16);
  llvm::Type* v4i32_ty = llvm::FixedVectorType::get(i32_ty, 4);
  llvm::Type* v2i64_ty = llvm::FixedVectorType::get(i64_ty, 2);
  llvm::Type* v4f32_ty = llvm::FixedVectorType::get(f32_ty, 4);
  llvm::Value* vb1 = llvm::Constant::getNullValue(v16i8_ty);
  llvm::Value* vi1 = llvm::Constant::getNullValue(v4i32_ty);
  llvm::Value* vf1 = llvm::Constant::getNullValue(v4f32_ty);
  llvm::Value* i32_v = llvm::ConstantInt::get(i32_ty, 1);
  llvm::Value* i64_v = llvm::ConstantInt::get(i64_ty, 1);

  llvm::Value* out = nullptr;
  bool handled = false;

  // ARM / AArch64 NEON, SVE, and system builtins
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vaddvq_s32", {vi1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vmaxvq_s32", {vi1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vaddq_s32", {vi1, vi1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vsubq_f32", {vf1, vf1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vmulq_s32", {vi1, vi1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  llvm::FunctionType* neon_ft = llvm::FunctionType::get(v2i64_ty, {v4i32_ty}, false);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_saddlp", {vi1}, neon_ft, &out, &handled));
  assert(handled && out != nullptr);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_custom_op", {vi1}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);

  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_sve_svadd_u", {vi1, vi1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_sve_svSub_u", {vf1, vf1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  llvm::Type* nxv16i1_ty = llvm::ScalableVectorType::get(llvm::Type::getInt1Ty(ctx), 16);
  llvm::FunctionType* ptrue_ft = llvm::FunctionType::get(nxv16i1_ty, {i32_ty}, false);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_sve_svptrue", {i32_v}, ptrue_ft, &out, &handled));
  assert(handled && out != nullptr);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_sve_custom_op", {vi1}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_sve_svcustom_op", {vi1}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__sve_svcntb", {}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__sve_svcnth", {}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__sve_svcntw", {}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__sve_svcntd", {}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  llvm::Type* nxv4i32_ty = llvm::ScalableVectorType::get(i32_ty, 4);
  llvm::FunctionType* svdup_ft = llvm::FunctionType::get(nxv4i32_ty, {i32_ty}, false);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__sve_svdup_s32", {i32_v}, svdup_ft, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__sve_svdup_s32", {i32_v}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__sve_svadd_s32", {vi1, vi1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);

  const char* arm_1arg_ops[] = {
      "__builtin_arm_dmb", "__builtin_arm_dsb", "__builtin_arm_isb",
      "__builtin_arm_yield", "__builtin_arm_wfe", "__builtin_arm_wfi", "__builtin_arm_sev",
      "__builtin_arm_rsr64", "__builtin_arm_wsr64"
  };
  for (const char* name : arm_1arg_ops) {
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, name, {i32_v}, nullptr, &out, &handled));
    assert(handled && out != nullptr);
  }
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_arm_crc32b", {i32_v, i32_v}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_arm_custom", {i32_v}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);

  // RISC-V builtins
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_rvv_vsetvli", {i32_v, i32_v, i32_v}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_rvv_vsetvli", {i64_v, i64_v, i64_v}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_rvv_vsetvlimax", {i32_v, i32_v}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_rvv_vadd_vv", {vi1, vi1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__riscv_vadd_vv", {vf1, vf1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_rvv_vcustom", {vi1}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_riscv_rev8", {i64_v}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_riscv_clmul", {i64_v, i64_v}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_riscv_clmulh", {i64_v, i64_v}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_riscv_orc_b", {i64_v}, nullptr, &out, &handled));
  assert(handled && out != nullptr);

  // PowerPC, s390x, LoongArch, WebAssembly, NVPTX, and AMDGPU builtins
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_altivec_vaddubm", {vb1, vb1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_vsx_xvaddsp", {vf1, vf1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_s390_vfaeb", {vb1, vb1, i32_v}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_lsx_vadd_b", {vb1, vb1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_lasx_xvadd_b", {vb1, vb1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_wasm_abs_i32x4", {vi1}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__nvvm_read_ptx_sreg_tid_x", {}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__nvvm_bar0", {}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_amdgcn_workitem_id_x", {}, nullptr, &out, &handled));
  assert(handled && out != nullptr);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_amdgcn_s_barrier", {}, nullptr, &out, &handled));
  assert(handled && out != nullptr);

  // Invalid other-arch names return builtin_lowering_failed
  const char* bad_other[] = {
      "__builtin_altivec_bad", "__builtin_vsx_bad", "__builtin_s390_bad",
      "__builtin_lsx_bad", "__builtin_lasx_bad", "__builtin_wasm_bad",
      "__nvvm_bad", "__builtin_amdgcn_bad"
  };
  for (const char* bname : bad_other) {
    assert(llvm_lower_target_builtin(nullptr, bfk, bname, {vi1}, nullptr, &out, &handled) ==
           llvm_gen_be_error_t::builtin_lowering_failed);
  }

  // Unhandled non-target builtin name returns handled = false
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_unknown_arch_op", {i32_v}, nullptr, &out, &handled));
  assert(!handled);

  // Cover remaining NEON, SVE, RISC-V, and other-arch FP/int & fallthrough branches + overloaded IIT slots
  {
    llvm::AllocaInst* mem_slot = be_state->builder->CreateAlloca(v4i32_ty, nullptr, "neon.mem");
    llvm::FunctionType* ft_i32 = llvm::FunctionType::get(i32_ty, {v4i32_ty, i32_ty}, false);
    llvm::Type* i64_ty_l = llvm::Type::getInt64Ty(ctx);
    llvm::Type* f32_ty_l = llvm::Type::getFloatTy(ctx);
    llvm::FunctionType* ft_i64 = llvm::FunctionType::get(i64_ty_l, {v4i32_ty, i32_ty}, false);
    llvm::FunctionType* ft_f32 = llvm::FunctionType::get(f32_ty_l, {v4i32_ty, i32_ty}, false);
    llvm::FunctionType* ft_v4i32 = llvm::FunctionType::get(v4i32_ty, {mem_slot->getType()}, false);
    llvm::Value* idx_zero = llvm::ConstantInt::get(i32_ty, 0);
    llvm::Value* f32_one = llvm::ConstantFP::get(f32_ty_l, 1.0);
    llvm::Value* dyn_tag = be_state->builder->CreateLoad(i32_ty, mem_slot, "dyn.tag");

    // vgetq_lane_ / vget_lane_: null callee_ty, matching callee_ty, int->int cast, int->fp bitcast, fp->fp bitcast, short args & non-vec fallthroughs
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vgetq_lane_i32", {vi1, idx_zero}, nullptr, &out, &handled));
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vget_lane_i32", {vi1, idx_zero}, ft_i32, &out, &handled));
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vget_lane_i32", {vi1, idx_zero}, ft_i64, &out, &handled));
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vget_lane_i32", {vi1, idx_zero}, ft_f32, &out, &handled));
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vget_lane_f32", {vf1, idx_zero}, ft_i32, &out, &handled));
    assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vget_lane_i32", {vi1}, nullptr, &out, &handled) ==
           llvm_gen_be_error_t::builtin_lowering_failed);
    assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vget_lane_i32", {idx_zero, idx_zero}, nullptr, &out, &handled) ==
           llvm_gen_be_error_t::builtin_lowering_failed);

    // vsetq_lane_ / vset_lane_: matching lane, int->int cast, fp->int bitcast, int->fp bitcast, short args & non-vec fallthroughs
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vsetq_lane_i32", {idx_zero, vi1, idx_zero}, nullptr, &out, &handled));
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vset_lane_i32", {i64_v, vi1, idx_zero}, nullptr, &out, &handled));
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vset_lane_i32", {f32_one, vi1, idx_zero}, nullptr, &out, &handled));
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vset_lane_f32", {idx_zero, vf1, idx_zero}, nullptr, &out, &handled));
    assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vset_lane_i32", {idx_zero, vi1}, nullptr, &out, &handled) ==
           llvm_gen_be_error_t::builtin_lowering_failed);
    assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vset_lane_i32", {idx_zero, idx_zero, idx_zero}, nullptr, &out, &handled) ==
           llvm_gen_be_error_t::builtin_lowering_failed);

    // vld1q_ / vld1_ & vst1q_ / vst1_
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vld1q_v", {mem_slot}, nullptr, &out, &handled));
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vld1_v", {mem_slot}, nullptr, &out, &handled));
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vld1_v", {mem_slot}, ft_v4i32, &out, &handled));
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vld1_v", {mem_slot}, ft_i32, &out, &handled));
    assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vld1_v", {}, nullptr, &out, &handled) ==
           llvm_gen_be_error_t::builtin_lowering_failed);
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vst1q_v", {mem_slot, vb1}, nullptr, &out, &handled));
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vst1_v", {mem_slot, vb1}, nullptr, &out, &handled));
    assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vst1_v", {mem_slot}, nullptr, &out, &handled) ==
           llvm_gen_be_error_t::builtin_lowering_failed);

    // vfmaq_ / vfma_: direct FP vector, byte vector with tags 41 (float), 42 (double), 40 (half), dynamic tag, mixed arg types, and non-FP scalar fallthrough
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vfmaq_v", {vf1, vf1, vf1}, nullptr, &out, &handled));
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vfma_v", {vf1, vb1, vb1}, nullptr, &out, &handled));
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vfmaq_v", {vb1, vb1, vb1, llvm::ConstantInt::get(i32_ty, 41)}, nullptr, &out, &handled));
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vfmaq_v", {vb1, vb1, vb1, llvm::ConstantInt::get(i32_ty, 42)}, nullptr, &out, &handled));
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vfmaq_v", {vb1, vb1, vb1, llvm::ConstantInt::get(i32_ty, 40)}, nullptr, &out, &handled));
    LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vfmaq_v", {vb1, vf1, vf1, dyn_tag}, nullptr, &out, &handled));
    assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vfma_v", {idx_zero, idx_zero, idx_zero}, nullptr, &out, &handled) ==
           llvm_gen_be_error_t::builtin_lowering_failed);
    assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vfma_v", {vf1, vf1}, nullptr, &out, &handled) ==
           llvm_gen_be_error_t::builtin_lowering_failed);
  }
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vaddq_f32", {vf1, vf1}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vsubq_s32", {vi1, vi1}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vmulq_f32", {vf1, vf1}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_sve_add_f32", {vf1, vf1}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_sve_svsub_s32", {vi1, vi1}, nullptr, &out, &handled));
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_sve_sub_s32", {vi1, vi1}, nullptr, &out, &handled));
  llvm::FunctionType* nonvec_ft = llvm::FunctionType::get(i32_ty, {i32_ty}, false);
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__sve_svdup_s32", {i32_v}, nonvec_ft, &out, &handled));
  // 2-arg vsetvli (not max, < 3 args -> falls through to vsetvlimax)
  LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(nullptr, bfk, "__builtin_rvv_vsetvli", {i32_v, i32_v}, nullptr, &out, &handled));

  // Overloaded intrinsics exercising callee_ty == nullptr (0 args & >=1 args), void return callee_ty, and arg_pos >= args.size()
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_sve_svptrue", {}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);
  llvm::FunctionType* void_ft = llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), {v4i32_ty}, false);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_saddlp", {vi1}, void_ft, &out, &handled) ==
         llvm_gen_be_error_t::ok || handled);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_saddlp", {}, void_ft, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);

  const char* nonx86_fallthroughs[] = {
      "__builtin_neon_vaddv", "__builtin_neon_vmaxv", "__builtin_neon_vadd",
      "__builtin_sve_svdup", "__builtin_sve_svadd",
      "__builtin_rvv_vsetvli", "__builtin_rvv_vadd", "__builtin_riscv_rev8",
      "__builtin_altivec_vadd", "__builtin_vsx_xvadd", "__builtin_lsx_vadd",
      "__builtin_lasx_xvadd", "__builtin_wasm_abs_"
  };
  for (const char* fname : nonx86_fallthroughs) {
    llvm_gen_be_error_t rft = llvm_lower_target_builtin(nullptr, bfk, fname, {}, nullptr, &out, &handled);
    assert(rft == llvm_gen_be_error_t::builtin_lowering_failed || rft == llvm_gen_be_error_t::ok);
  }
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_neon_vadd", {vi1, vf1}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_sve_svadd", {vi1, vf1}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_rvv_vadd", {vi1, vf1}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_altivec_vadd", {vf1, vf1}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_vsx_xvadd", {vi1, vi1}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_lsx_vadd", {vf1, vf1}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_lasx_xvadd", {vf1, vf1}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_wasm_abs_f32", {vf1}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);
  // Error propagation through top-level llvm_lower_target_builtin for arm & riscv
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_arm_custom_bad", {}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);
  assert(llvm_lower_target_builtin(nullptr, bfk, "__builtin_riscv_custom_bad", {}, nullptr, &out, &handled) ==
         llvm_gen_be_error_t::builtin_lowering_failed);

  *out_passed = true;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Entry point for the target architecture intrinsics unit test binary.
 * @details Initializes a synthetic LLVM module/function context, executes all target
 *          intrinsic test cases, and verifies the resulting module with `llvm::verifyModule`.
 * @return 0 on success.
 */
int main(void) {
  puts("Running test_llvm_target_intrinsics...");
  LLVMBackendState state = {};
  state.context = std::make_unique<llvm::LLVMContext>();
  state.module = std::make_unique<llvm::Module>("test_target_intrinsics", *state.context);
  state.builder = std::make_unique<llvm::IRBuilder<>>(*state.context);
  edg::be_state = &state;

  llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getVoidTy(*state.context), false);
  llvm::Function* f = llvm::Function::Create(
      ft, llvm::Function::ExternalLinkage, "test_intrinsics_fn", state.module.get());
  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*state.context, "entry", f);
  state.builder->SetInsertPoint(bb);

  bool passed = false;
  assert(test_intrinsic_null_checks(&passed) == llvm_gen_be_error_t::ok);
  assert(passed);

  assert(test_x86_intrinsics(&passed) == llvm_gen_be_error_t::ok);
  assert(passed);

  assert(test_non_x86_intrinsics(&passed) == llvm_gen_be_error_t::ok);
  assert(passed);

  state.builder->CreateRetVoid();
  assert(!llvm::verifyModule(*state.module, &llvm::errs()));
  puts("test_llvm_target_intrinsics PASSED!");
  return 0;
}
