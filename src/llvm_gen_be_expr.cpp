/**
 * @file llvm_gen_be_expr.cpp
 * @ingroup llvm_gen_be_expr
 * @brief Expression lowering subsystem for the EDG LLVM backend.
 * @details Translates EDG front-end expression nodes (an_expr_node_ptr) into LLVM IR
 * values (llvm::Value*).
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "llvm_gen_be_expr.h"
#include <type_traits>
#include <vector>

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_const.h"
#include "llvm_gen_be_type.h"
#include "llvm_gen_be_vtable.h"
#include "llvm_gen_be_rtti.h"
#include "llvm_gen_be_ptrmem.h"
#include "llvm_gen_be_coro.h"
#include "llvm_gen_be_eh.h"
#include "llvm_gen_be_intrinsics.h"
#include "llvm_gen_be_abi_sysv_x86_64.h"
#include "llvm_gen_be_abi_aapcs64.h"
#include "llvm_gen_be_abi_win64.h"
#include "llvm_gen_be_abi_x86_32.h"
#include "llvm_gen_be_abi_arm32.h"
#include "llvm_gen_be_abi_wasm.h"
#include "llvm_gen_be_abi_riscv64.h"
#include "llvm_gen_be_abi_loongarch64.h"
#include "llvm_gen_be_abi_mips.h"
#include "llvm_gen_be_abi_ppc64.h"
#include "llvm_gen_be_abi_s390x.h"
#include "llvm_gen_be_abi_sparc.h"
#include "target.h"
#include "lower_name.h"
#include <llvm/Demangle/Demangle.h>
#include <llvm/IR/Intrinsics.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Extracts the scalar element type, real part, and imaginary part from an LLVM `_Complex` struct.
 * @details Supports both unlowered `{ T, T }` and EDG C99 IL-lowered `{ [2 x T] }` representations.
 * @param[in] cplx_val Complex aggregate `llvm::Value*`.
 * @param[out] out_elem_ty Receives the underlying scalar component `llvm::Type*`.
 * @param[out] out_re Receives the extracted real scalar `llvm::Value*`.
 * @param[out] out_im Receives the extracted imaginary scalar `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t unpack_complex_pair(
    llvm::Value* cplx_val,
    llvm::Type** out_elem_ty,
    llvm::Value** out_re,
    llvm::Value** out_im) noexcept {
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    llvm::Type* st_ty = cplx_val->getType();
    llvm::Type* field0_ty = st_ty->getStructElementType(0);
    if (auto* arr_ty = llvm::dyn_cast<llvm::ArrayType>(field0_ty)) {
      *out_elem_ty = arr_ty->getElementType();
      *out_re = be_state->builder->CreateExtractValue(cplx_val, {0, 0});
      *out_im = be_state->builder->CreateExtractValue(cplx_val, {0, 1});
    } else {
      *out_elem_ty = field0_ty;
      *out_re = be_state->builder->CreateExtractValue(cplx_val, 0);
      *out_im = be_state->builder->CreateExtractValue(cplx_val, 1);
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Packs real and imaginary scalar values into an LLVM `_Complex` struct (`{ T, T }` or `{ [2 x T] }`).
 * @details Produces an aggregate matching @p target_struct_ty regardless of whether the target type
 *          uses unlowered `{ T, T }` or EDG C99 IL-lowered `{ [2 x T] }` representation.
 * @param[in] target_struct_ty Target LLVM complex struct type.
 * @param[in] re Real component scalar value.
 * @param[in] im Imaginary component scalar value.
 * @param[out] out_cplx Receives the packed complex aggregate value.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t pack_complex_pair(
    llvm::Type* target_struct_ty,
    llvm::Value* re,
    llvm::Value* im,
    llvm::Value** out_cplx) noexcept {
    llvm::Type* field0_ty = target_struct_ty->getStructElementType(0);
    if (field0_ty->isArrayTy()) {
      llvm::Value* res = be_state->builder->CreateInsertValue(
          llvm::UndefValue::get(target_struct_ty), re, {0, 0});
      *out_cplx = be_state->builder->CreateInsertValue(res, im, {0, 1});
    } else {
      llvm::Value* res = be_state->builder->CreateInsertValue(
          llvm::UndefValue::get(target_struct_ty), re, 0);
      *out_cplx = be_state->builder->CreateInsertValue(res, im, 1);
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits complex multiplication or division for floating-point (target ABI coerced) or GNU integer `_Complex`.
 * @details Coerces `_Complex float` returns to `<2 x float>` on SysV x86_64 and `i64` on Win64 x86_64,
 *          preserves `{ float, float }` HFA on AAPCS64, supports `_Float16` (`__mulhc3`/`__divhc3`),
 *          `x86_fp80` (`__mulxc3`/`__divxc3`), `fp128` (`__multc3`/`__divtc3`), and lowers GNU integer
 *          `_Complex` multiplication/division inline.
 * @param[in] sc_name Name of the single-precision runtime function (`__mulsc3` or `__divsc3`).
 * @param[in] dc_name Name of the double-precision runtime function (`__muldc3` or `__divdc3`).
 * @param[in] v1 First complex struct operand.
 * @param[in] v2 Second complex struct operand.
 * @param[out] out_val Receives the resulting complex struct LLVM Value.
 * @return llvm_gen_be_error_t::ok on success, or `complex_lowering_failed` on invalid operands.
 */
static llvm_gen_be_error_t emit_complex_libcall(
    const char* sc_name,
    const char* dc_name,
    llvm::Value* v1,
    llvm::Value* v2,
    llvm::Value** out_val) noexcept {
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    llvm::Type* struct_ty = v1->getType();
    llvm::Type* elem_ty = nullptr;
    llvm::Type* elem_ty2 = nullptr;
    llvm::Value* r1 = nullptr;
    llvm::Value* i1 = nullptr;
    llvm::Value* r2 = nullptr;
    llvm::Value* i2 = nullptr;
    LLVM_GEN_BE_CHECK(unpack_complex_pair(v1, &elem_ty, &r1, &i1));
    LLVM_GEN_BE_CHECK(unpack_complex_pair(v2, &elem_ty2, &r2, &i2));
    bool is_mul = (sc_name[2] == 'm');

    if (elem_ty->isIntegerTy()) {
      llvm::Value* r_out = nullptr;
      llvm::Value* i_out = nullptr;
      if (is_mul) {
        llvm::Value* ac = be_state->builder->CreateMul(r1, r2);
        llvm::Value* bd = be_state->builder->CreateMul(i1, i2);
        llvm::Value* ad = be_state->builder->CreateMul(r1, i2);
        llvm::Value* bc = be_state->builder->CreateMul(i1, r2);
        r_out = be_state->builder->CreateSub(ac, bd);
        i_out = be_state->builder->CreateAdd(ad, bc);
      } else {
        llvm::Value* r2_sq = be_state->builder->CreateMul(r2, r2);
        llvm::Value* i2_sq = be_state->builder->CreateMul(i2, i2);
        llvm::Value* denom = be_state->builder->CreateAdd(r2_sq, i2_sq);
        llvm::Value* num_r = be_state->builder->CreateAdd(
            be_state->builder->CreateMul(r1, r2),
            be_state->builder->CreateMul(i1, i2));
        llvm::Value* num_i = be_state->builder->CreateSub(
            be_state->builder->CreateMul(i1, r2),
            be_state->builder->CreateMul(r1, i2));
        r_out = be_state->builder->CreateSDiv(num_r, denom);
        i_out = be_state->builder->CreateSDiv(num_i, denom);
      }
      return pack_complex_pair(struct_ty, r_out, i_out, out_val);
    }

    const char* func_name = sc_name;
    if (elem_ty->isHalfTy()) {
      func_name = is_mul ? "__mulhc3" : "__divhc3";
    } else if (elem_ty->isDoubleTy()) {
      func_name = dc_name;
    } else if (elem_ty->isX86_FP80Ty()) {
      func_name = is_mul ? "__mulxc3" : "__divxc3";
    } else if (elem_ty->isFP128Ty() || elem_ty->isPPC_FP128Ty()) {
      func_name = is_mul ? "__multc3" : "__divtc3";
    }

    llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
    if (elem_ty->isFloatTy() && triple.getArch() == llvm::Triple::x86_64) {
      if (triple.isOSWindows()) {
        llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
        llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
        llvm::FunctionType* fty = llvm::FunctionType::get(i64_ty, {elem_ty, elem_ty, elem_ty, elem_ty}, false);
        llvm::FunctionCallee callee = be_state->module->getOrInsertFunction(func_name, fty);
        llvm::Value* packed = be_state->builder->CreateCall(callee, {r1, i1, r2, i2});
        llvm::Value* lo_i32 = be_state->builder->CreateTrunc(packed, i32_ty);
        llvm::Value* hi_i32 = be_state->builder->CreateTrunc(be_state->builder->CreateLShr(packed, 32), i32_ty);
        llvm::Value* r_res = be_state->builder->CreateBitCast(lo_i32, elem_ty);
        llvm::Value* i_res = be_state->builder->CreateBitCast(hi_i32, elem_ty);
        return pack_complex_pair(struct_ty, r_res, i_res, out_val);
      }
      llvm::Type* v2f32_ty = llvm::FixedVectorType::get(elem_ty, 2);
      llvm::FunctionType* fty = llvm::FunctionType::get(v2f32_ty, {elem_ty, elem_ty, elem_ty, elem_ty}, false);
      llvm::FunctionCallee callee = be_state->module->getOrInsertFunction(func_name, fty);
      llvm::Value* vec_res = be_state->builder->CreateCall(callee, {r1, i1, r2, i2});
      llvm::Value* r_res = be_state->builder->CreateExtractElement(vec_res, uint64_t(0));
      llvm::Value* i_res = be_state->builder->CreateExtractElement(vec_res, uint64_t(1));
      return pack_complex_pair(struct_ty, r_res, i_res, out_val);
    }

    llvm::StructType* pair_ty = llvm::StructType::get(*be_state->context, {elem_ty, elem_ty});
    llvm::FunctionType* fty = llvm::FunctionType::get(pair_ty, {elem_ty, elem_ty, elem_ty, elem_ty}, false);
    llvm::FunctionCallee callee = be_state->module->getOrInsertFunction(func_name, fty);
    llvm::Value* pair_res = be_state->builder->CreateCall(callee, {r1, i1, r2, i2});
    llvm::Value* r_res = be_state->builder->CreateExtractValue(pair_res, 0);
    llvm::Value* i_res = be_state->builder->CreateExtractValue(pair_res, 1);
    return pack_complex_pair(struct_ty, r_res, i_res, out_val);
}

/**
 * @brief Lowers EDG IL-lowered C99 `_Complex` runtime helper calls (`__c99_*`) into inline LLVM IR and compiler-rt/libgcc calls.
 * @details Intercepts `__c99_complex_*` (`negate`, `conj`, `add`, `subtract`, `multiply`, `divide`, `eq`, `ne`)
 *          and `__c99_*_to_*` complex/imaginary/real conversions synthesized by EDG IL lowering (`c99_complex.c`),
 *          preventing undefined external symbol references when linking against standard system runtimes.
 * @param[in] ret_edg_type Expected EDG result type of the call expression.
 * @param[in] rname Mangled/lowered C99 helper function name starting with `"__c99_"`.
 * @param[in] args Evaluated LLVM argument values.
 * @param[out] out_val Receives the lowered LLVM result value.
 * @return llvm_gen_be_error_t::ok on success, or `complex_lowering_failed` if unrecognized or malformed.
 */
static llvm_gen_be_error_t llvm_lower_c99_complex_runtime_call(
    a_type_ptr ret_edg_type,
    llvm::StringRef rname,
    const std::vector<llvm::Value*>& args,
    llvm::Value** out_val) noexcept {
    llvm::Type* res_ty = nullptr;
    LLVM_GEN_BE_CHECK(get_llvm_type(ret_edg_type, &res_ty));
    if (args.size() == 1 && args[0]->getType()->isStructTy()) {
      llvm::Type* elem_ty = nullptr;
      llvm::Value* r1 = nullptr;
      llvm::Value* i1 = nullptr;
      LLVM_GEN_BE_CHECK(unpack_complex_pair(args[0], &elem_ty, &r1, &i1));
      if (rname.ends_with("_negate")) {
        return pack_complex_pair(res_ty, be_state->builder->CreateFNeg(r1),
                                 be_state->builder->CreateFNeg(i1), out_val);
      }
      if (rname.ends_with("_conj")) {
        return pack_complex_pair(res_ty, r1, be_state->builder->CreateFNeg(i1), out_val);
      }
      if (res_ty->isStructTy()) {
        llvm::Type* dst_elem = res_ty->getStructElementType(0);
        if (auto* arr_ty = llvm::dyn_cast<llvm::ArrayType>(dst_elem)) {
          dst_elem = arr_ty->getElementType();
        }
        return pack_complex_pair(res_ty,
                                 be_state->builder->CreateFPCast(r1, dst_elem),
                                 be_state->builder->CreateFPCast(i1, dst_elem), out_val);
      }
      llvm::Value* part = rname.contains("_to_i") ? i1 : r1;
      *out_val = be_state->builder->CreateFPCast(part, res_ty);
      return llvm_gen_be_error_t::ok;
    }
    if (args.size() == 1 && res_ty->isStructTy()) {
      llvm::Type* dst_elem = res_ty->getStructElementType(0);
      if (auto* arr_ty = llvm::dyn_cast<llvm::ArrayType>(dst_elem)) {
        dst_elem = arr_ty->getElementType();
      }
      llvm::Value* scalar = be_state->builder->CreateFPCast(args[0], dst_elem);
      llvm::Value* zero = llvm::ConstantFP::get(dst_elem, 0.0);
      bool from_imag = rname.starts_with("__c99_i");
      return pack_complex_pair(res_ty, from_imag ? zero : scalar, from_imag ? scalar : zero, out_val);
    }
    if (args.size() == 2 && args[0]->getType()->isStructTy() && args[1]->getType()->isStructTy()) {
      if (rname.ends_with("_multiply")) {
        return emit_complex_libcall("__mulsc3", "__muldc3", args[0], args[1], out_val);
      }
      if (rname.ends_with("_divide")) {
        return emit_complex_libcall("__divsc3", "__divdc3", args[0], args[1], out_val);
      }
      llvm::Type* elem_ty = nullptr;
      llvm::Type* elem_ty2 = nullptr;
      llvm::Value* r1 = nullptr;
      llvm::Value* i1 = nullptr;
      llvm::Value* r2 = nullptr;
      llvm::Value* i2 = nullptr;
      LLVM_GEN_BE_CHECK(unpack_complex_pair(args[0], &elem_ty, &r1, &i1));
      LLVM_GEN_BE_CHECK(unpack_complex_pair(args[1], &elem_ty2, &r2, &i2));
      if (rname.ends_with("_add")) {
        return pack_complex_pair(res_ty, be_state->builder->CreateFAdd(r1, r2),
                                 be_state->builder->CreateFAdd(i1, i2), out_val);
      }
      if (rname.ends_with("_subtract")) {
        return pack_complex_pair(res_ty, be_state->builder->CreateFSub(r1, r2),
                                 be_state->builder->CreateFSub(i1, i2), out_val);
      }
      if (rname.ends_with("_eq") || rname.ends_with("_ne")) {
        llvm::Value* cmp = rname.ends_with("_eq")
            ? be_state->builder->CreateAnd(be_state->builder->CreateFCmpOEQ(r1, r2),
                                           be_state->builder->CreateFCmpOEQ(i1, i2))
            : be_state->builder->CreateOr(be_state->builder->CreateFCmpUNE(r1, r2),
                                          be_state->builder->CreateFCmpUNE(i1, i2));
        *out_val = be_state->builder->CreateZExt(cmp, res_ty);
        return llvm_gen_be_error_t::ok;
      }
    }
    return llvm_gen_be_error_t::complex_lowering_failed;
}

/**
 * @brief Checks whether an EDG type is `volatile`-qualified.
 * @details Queries `f_get_type_qualifiers` directly to determine volatile qualification.
 * @param[in] tp The EDG type pointer to inspect.
 * @param[out] out_vol Receives `true` if `tp` is volatile-qualified.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t check_type_is_volatile(a_type_ptr tp, bool* out_vol) noexcept {
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    *out_vol = (f_get_type_qualifiers(tp, TRUE) & TQ_VOLATILE) != 0;
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Computes the integer container bit width and byte-granular straddle flag for a bitfield access.
 * @details Uses the declared container width for non-straddling volatile/MSVC bitfields and flags
 *          straddling or non-power-of-2 spans so byte-granular loads/stores never read past struct tail boundaries.
 * @param[in] field Pointer to the EDG bitfield descriptor.
 * @param[in] is_vol Whether the access is volatile-qualified.
 * @param[out] out_bits Receives the computed container bit width.
 * @param[out] out_byte_wise Receives `true` when byte-granular access must be used to avoid tearing/over-read.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t compute_bitfield_container_bits(
    a_field_ptr field,
    bool is_vol,
    unsigned* out_bits,
    bool* out_byte_wise) noexcept {
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    unsigned bit_offset = field->offset_bit_remainder;
    unsigned bit_size = field->bit_size;
    llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
    bool is_msvc = triple.isWindowsMSVCEnvironment();
    a_type_ptr ft = field->type ? skip_typerefs(field->type) : nullptr;
    unsigned declared_bits = ft ? static_cast<unsigned>(ft->size * 8) : 0U;
    unsigned span_bits = ((bit_offset + bit_size + 7U) / 8U) * 8U;
    bool straddles_declared = (declared_bits > 0 && (bit_offset + bit_size) > declared_bits);

    if ((is_vol || is_msvc) && !straddles_declared &&
        declared_bits >= 8U && declared_bits <= 64U && llvm::isPowerOf2_32(declared_bits)) {
        *out_bits = declared_bits;
        *out_byte_wise = false;
        return llvm_gen_be_error_t::ok;
    }

    *out_bits = (span_bits > 0) ? span_bits : 8U;
    *out_byte_wise = straddles_declared || !llvm::isPowerOf2_32(*out_bits) || *out_bits > 64U;
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits a bitfield load and sign/zero extension from a container address.
 * @details Loads the underlying container integer (preserving single declared-width access for
 *          non-straddling volatile bitfields and using tail-safe byte assembly for straddling spans),
 *          shifts and masks the bitfield bits, and sign- or zero-extends to `expr_type`.
 * @param[in] ptr Pointer to the bitfield storage unit.
 * @param[in] field Pointer to the EDG bitfield descriptor.
 * @param[in] expr_type The target EDG expression type.
 * @param[out] out_val Receives the extracted bitfield value.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_emit_bitfield_load(
    llvm::Value* ptr,
    a_field_ptr field,
    a_type_ptr expr_type,
    llvm::Value** out_val) noexcept {
    if (!ptr || !field || !expr_type || !out_val) {
        return llvm_gen_be_error_t::bitfield_access_failed;
    }
    unsigned bit_offset = field->offset_bit_remainder;
    unsigned bit_size = field->bit_size;
    bool is_vol = false;
    LLVM_GEN_BE_CHECK(check_type_is_volatile(expr_type, &is_vol));
    if (!is_vol && field->type) {
        LLVM_GEN_BE_CHECK(check_type_is_volatile(field->type, &is_vol));
    }
    unsigned container_bits = 0;
    bool byte_wise = false;
    LLVM_GEN_BE_CHECK(compute_bitfield_container_bits(field, is_vol, &container_bits, &byte_wise));
    
    llvm::Type* container_ty = llvm::Type::getIntNTy(*be_state->context, container_bits);
    llvm::Value* val = nullptr;
    if (byte_wise) {
        llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
        llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
        unsigned num_bytes = container_bits / 8U;
        val = llvm::ConstantInt::get(container_ty, 0);
        for (unsigned b = 0; b < num_bytes; ++b) {
            llvm::Value* b_ptr = (b == 0)
                ? ptr
                : be_state->builder->CreateInBoundsGEP(i8_ty, ptr, llvm::ConstantInt::get(i64_ty, b));
            llvm::LoadInst* bl = be_state->builder->CreateLoad(i8_ty, b_ptr);
            if (is_vol) bl->setVolatile(true);
            llvm::Value* ext_b = be_state->builder->CreateZExt(bl, container_ty);
            unsigned sh = targ_little_endian ? (b * 8U) : ((num_bytes - 1U - b) * 8U);
            if (sh > 0) ext_b = be_state->builder->CreateShl(ext_b, sh);
            val = be_state->builder->CreateOr(val, ext_b);
        }
    } else {
        llvm::LoadInst* load = be_state->builder->CreateLoad(container_ty, ptr);
        if (is_vol) load->setVolatile(true);
        val = load;
    }
    
    unsigned shift = targ_little_endian ? bit_offset : (container_bits - (bit_offset + bit_size));
    if (shift > 0) val = be_state->builder->CreateLShr(val, shift);
    
    llvm::APInt mask_ap = llvm::APInt::getLowBitsSet(container_bits, bit_size);
    val = be_state->builder->CreateAnd(val, llvm::ConstantInt::get(container_ty, mask_ap));
    
    if (field->bit_field_is_signed && bit_size < container_bits) {
        unsigned shift_up = container_bits - bit_size;
        val = be_state->builder->CreateShl(val, shift_up);
        val = be_state->builder->CreateAShr(val, shift_up);
    }
    
    llvm::Type* target_ty = nullptr;
    llvm_gen_be_error_t err = get_llvm_type(expr_type, &target_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    if (is_bool_type(skip_typerefs(expr_type))) {
        llvm::Value* nz = be_state->builder->CreateICmpNE(val, llvm::ConstantInt::get(container_ty, 0));
        *out_val = target_ty->isIntegerTy(1) ? nz : be_state->builder->CreateZExt(nz, target_ty);
    } else {
        *out_val = be_state->builder->CreateIntCast(val, target_ty, field->bit_field_is_signed);
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits a bitfield read-modify-write store into a container address.
 * @details Masks and shifts `new_val` into the bitfield position and updates the container
 *          (preserving declared access width for volatile bitfields and using tail-safe byte
 *          stores for straddling or non-power-of-2 bitfield spans).
 * @param[in] ptr Pointer to the bitfield storage unit.
 * @param[in] new_val The new value to store into the bitfield.
 * @param[in] field Pointer to the EDG bitfield descriptor.
 * @param[in] expr_type The EDG expression type (checked for volatile qualification).
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_emit_bitfield_store(
    llvm::Value* ptr,
    llvm::Value* new_val,
    a_field_ptr field,
    a_type_ptr expr_type) noexcept {
    if (!ptr || !new_val || !field || !expr_type) {
        return llvm_gen_be_error_t::bitfield_access_failed;
    }
    unsigned bit_offset = field->offset_bit_remainder;
    unsigned bit_size = field->bit_size;
    bool is_vol = false;
    LLVM_GEN_BE_CHECK(check_type_is_volatile(expr_type, &is_vol));
    if (!is_vol && field->type) {
        LLVM_GEN_BE_CHECK(check_type_is_volatile(field->type, &is_vol));
    }
    unsigned container_bits = 0;
    bool byte_wise = false;
    LLVM_GEN_BE_CHECK(compute_bitfield_container_bits(field, is_vol, &container_bits, &byte_wise));
    
    llvm::Type* container_ty = llvm::Type::getIntNTy(*be_state->context, container_bits);
    llvm::Value* val = be_state->builder->CreateIntCast(new_val, container_ty, false);
    
    unsigned shift = targ_little_endian ? bit_offset : (container_bits - (bit_offset + bit_size));
    llvm::APInt val_mask_ap = llvm::APInt::getLowBitsSet(container_bits, bit_size);
    val = be_state->builder->CreateAnd(val, llvm::ConstantInt::get(container_ty, val_mask_ap));
    if (shift > 0) val = be_state->builder->CreateShl(val, shift);
    
    llvm::APInt container_mask_ap = ~(val_mask_ap << shift);
    llvm::Constant* container_mask = llvm::ConstantInt::get(container_ty, container_mask_ap);
    
    if (byte_wise) {
        llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
        llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
        unsigned num_bytes = container_bits / 8U;
        llvm::APInt shifted_field_mask = (val_mask_ap << shift);
        for (unsigned b = 0; b < num_bytes; ++b) {
            unsigned sh = targ_little_endian ? (b * 8U) : ((num_bytes - 1U - b) * 8U);
            uint8_t byte_fmask = static_cast<uint8_t>(shifted_field_mask.extractBitsAsZExtValue(8U, sh));
            if (byte_fmask == 0U) continue;
            llvm::Value* b_ptr = (b == 0)
                ? ptr
                : be_state->builder->CreateInBoundsGEP(i8_ty, ptr, llvm::ConstantInt::get(i64_ty, b));
            llvm::Value* new_byte = be_state->builder->CreateTrunc(
                (sh > 0) ? be_state->builder->CreateLShr(val, sh) : val, i8_ty);
            if (byte_fmask == 0xFFU) {
                llvm::StoreInst* bst = be_state->builder->CreateStore(new_byte, b_ptr);
                if (is_vol) bst->setVolatile(true);
            } else {
                llvm::LoadInst* old_b = be_state->builder->CreateLoad(i8_ty, b_ptr);
                if (is_vol) old_b->setVolatile(true);
                llvm::Value* kept_b = be_state->builder->CreateAnd(
                    old_b, llvm::ConstantInt::get(i8_ty, static_cast<uint8_t>(~byte_fmask)));
                llvm::Value* merged_b = be_state->builder->CreateOr(kept_b, new_byte);
                llvm::StoreInst* bst = be_state->builder->CreateStore(merged_b, b_ptr);
                if (is_vol) bst->setVolatile(true);
            }
        }
        return llvm_gen_be_error_t::ok;
    }

    if (is_vol) {
        llvm::LoadInst* vload = be_state->builder->CreateLoad(container_ty, ptr);
        vload->setVolatile(true);
        llvm::Value* masked_old = be_state->builder->CreateAnd(vload, container_mask);
        llvm::Value* new_container = be_state->builder->CreateOr(masked_old, val);
        llvm::StoreInst* vstore = be_state->builder->CreateStore(new_container, ptr);
        vstore->setVolatile(true);
    } else {
        llvm::LoadInst* load = be_state->builder->CreateLoad(container_ty, ptr);
        llvm::Value* masked_container = be_state->builder->CreateAnd(load, container_mask);
        llvm::Value* new_container = be_state->builder->CreateOr(masked_container, val);
        be_state->builder->CreateStore(new_container, ptr);
    }
    
    return llvm_gen_be_error_t::ok;
}



/**
 * @brief Resolves and extracts the LLVM FunctionType from an EDG routine or pointer-to-routine type.
 * @details Unwraps pointers, references, and typerefs, lowering the underlying routine type.
 * @param[in] ty The EDG AST type pointer representing the routine or function pointer.
 * @param[out] out_fn_ty Receives the lowered LLVM FunctionType pointer on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t get_llvm_function_type(a_type_ptr ty, llvm::FunctionType** out_fn_ty) noexcept {
  *out_fn_ty = nullptr;
  while (ty) {
    if (ty->kind == tk_pointer || is_reference_type(ty)) {
      ty = ty->variant.pointer.type;
    } else if (ty->kind == tk_typeref) {
      ty = ty->variant.typeref.type;
    } else {
      break;
    }
  }
  if (ty && ty->kind == tk_routine) {
    llvm::Type* lt = nullptr;
    llvm_gen_be_error_t err = get_llvm_type(ty, &lt);
    if (err != llvm_gen_be_error_t::ok) return err;
    *out_fn_ty = llvm::cast<llvm::FunctionType>(lt);
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits a structured UndefinedBehaviorSanitizer (UBSan) `TypeDescriptor` global constant.
 * @details Constructs `{ i16 TypeKind, i16 TypeInfo, [N x i8] TypeName }` describing @p edg_ty
 *          for UBSan type-mismatch, overflow, and float-cast-overflow diagnostics.
 * @param[in] edg_ty Pointer to the EDG type to describe (may be nullptr for unknown type).
 * @param[out] out_desc Receives the created `llvm::GlobalVariable*` for the UBSan `TypeDescriptor`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if @p out_desc is null.
 */
llvm_gen_be_error_t llvm_emit_ubsan_type_descriptor(
    a_type_ptr edg_ty,
    llvm::GlobalVariable** out_desc) noexcept {
  if (!out_desc) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  uint16_t tk_val = 0xffff;
  uint16_t ti_val = 0;
  const char* tname = "<unknown>";
  if (edg_ty) {
    if (edg_ty->source_corresp.name) {
      tname = edg_ty->source_corresp.name;
    }
    if (edg_ty->kind == tk_integer) {
      tk_val = 0x0000;
      ti_val = static_cast<uint16_t>((edg_ty->size > 0 ? 6 : 0) | 1);
    } else if (edg_ty->kind == tk_float) {
      tk_val = 0x0001;
      ti_val = static_cast<uint16_t>(edg_ty->size * 8);
    }
  }
  llvm::Type* i16_ty = llvm::Type::getInt16Ty(*be_state->context);
  llvm::Constant* name_arr = llvm::ConstantDataArray::getString(*be_state->context, tname);
  llvm::StructType* desc_ty = llvm::StructType::get(
      *be_state->context, {i16_ty, i16_ty, name_arr->getType()});
  llvm::Constant* desc_init = llvm::ConstantStruct::get(
      desc_ty, {llvm::ConstantInt::get(i16_ty, tk_val), llvm::ConstantInt::get(i16_ty, ti_val), name_arr});
  LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), desc_ty, true, llvm::GlobalValue::PrivateLinkage, desc_init, ".ubsan.type", out_desc));
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits a conditional UndefinedBehaviorSanitizer (UBSan) runtime handler call when @p cond is true.
 * @details Splits the current basic block into a `ubsan.fail` block that invokes @p handler_name
 *          with a source descriptor pointer and @p handler_args, followed by a `ubsan.cont` continuation block.
 * @param[in] handler_name Null-terminated name of the `__ubsan_handle_*` runtime function.
 * @param[in] cond Boolean fault condition (`i1` `llvm::Value*`).
 * @param[in] handler_args Additional `llvm::Value*` arguments passed to the UBSan handler after the descriptor pointer.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `handler_name` or `cond` is null.
 */
llvm_gen_be_error_t llvm_emit_ubsan_check(
    const char* handler_name,
    llvm::Value* cond,
    const std::vector<llvm::Value*>& handler_args) noexcept {
  if (!handler_name || !cond) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
  llvm::BasicBlock* fail_bb = llvm::BasicBlock::Create(*be_state->context, "ubsan.fail", func);
  llvm::BasicBlock* cont_bb = llvm::BasicBlock::Create(*be_state->context, "ubsan.cont", func);
  be_state->builder->CreateCondBr(cond, fail_bb, cont_bb);

  be_state->builder->SetInsertPoint(fail_bb);
  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
  uint32_t dbg_line = 1;
  uint32_t dbg_col = 1;
  if (llvm::DebugLoc dl = be_state->builder->getCurrentDebugLocation()) {
    dbg_line = dl.getLine();
    dbg_col = dl.getCol();
  }
  llvm::StructType* ubsan_loc_ty = llvm::StructType::get(*be_state->context, {ptr_ty, i32_ty, i32_ty});
  const std::string& src_fn = be_state->module->getSourceFileName();
  llvm::Constant* fn_str = be_state->builder->CreateGlobalString(
      !src_fn.empty() ? src_fn.c_str() : "<file>", ".ubsan.file");
  llvm::Constant* loc_init = llvm::ConstantStruct::get(
      ubsan_loc_ty, {fn_str, llvm::ConstantInt::get(i32_ty, dbg_line), llvm::ConstantInt::get(i32_ty, dbg_col)});
  llvm::GlobalVariable* loc_gv = nullptr;
  LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), ubsan_loc_ty, true, llvm::GlobalValue::PrivateLinkage, loc_init, ".ubsan.loc", &loc_gv));
  std::vector<llvm::Type*> param_tys = {ptr_ty};
  std::vector<llvm::Value*> call_args = {loc_gv};
  for (llvm::Value* a : handler_args) {
    param_tys.push_back(a->getType());
    call_args.push_back(a);
  }
  llvm::FunctionType* fty = llvm::FunctionType::get(
      llvm::Type::getVoidTy(*be_state->context), param_tys, false);
  llvm::FunctionCallee fn = be_state->module->getOrInsertFunction(handler_name, fty);
  be_state->builder->CreateCall(fn, call_args);
  be_state->builder->CreateBr(cont_bb);

  be_state->builder->SetInsertPoint(cont_bb);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits a conditional UBSan runtime handler call when @p cond is true.
 * @details Delegates to `llvm_emit_ubsan_check` with an empty extra-argument list.
 * @param[in] cond Boolean fault condition (`i1`).
 * @param[in] handler_name Name of the `__ubsan_handle_*` runtime function.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t emit_ubsan_check_branch(
    llvm::Value* cond,
    const char* handler_name) noexcept {
  return llvm_emit_ubsan_check(handler_name, cond, {});
}

/**
 * @brief Loads an rvalue from an lvalue pointer, preserving volatile, atomic, UBSan, and TBAA semantics.
 * @details Lowers `edg_ty` to an `llvm::Type*`, emits a `CreateLoad`, and attaches volatile/atomic/TBAA metadata.
 * @param[in] ptr The source memory address value.
 * @param[in] edg_ty The EDG type of the loaded value.
 * @param[out] out_val Receives the loaded `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t load_from_lvalue_ptr(
    llvm::Value* ptr,
    a_type_ptr edg_ty,
    llvm::Value** out_val) noexcept {
  llvm::Type* llvm_ty = nullptr;
  llvm_gen_be_error_t err = get_llvm_type(edg_ty, &llvm_ty);
  if (err != llvm_gen_be_error_t::ok) return err;
  if (be_state->enable_ubsan_checks) {
    llvm::Value* is_null = be_state->builder->CreateIsNull(ptr, "ubsan.null");
    LLVM_GEN_BE_CHECK(emit_ubsan_check_branch(is_null, "__ubsan_handle_type_mismatch_v1"));
  }
  a_type_qualifier_set quals = f_get_type_qualifiers(edg_ty, TRUE);
  if ((quals & TQ_C11_ATOMIC) != 0) {
    uint64_t size = be_state->module->getDataLayout().getTypeStoreSize(llvm_ty);
    if ((llvm_ty->isIntegerTy() || llvm_ty->isPointerTy()) &&
        llvm::isPowerOf2_64(size) && size <= 16) {
      llvm::LoadInst* load = be_state->builder->CreateLoad(llvm_ty, ptr);
      load->setAtomic(llvm::AtomicOrdering::SequentiallyConsistent);
      load->setAlignment(llvm::Align(size));
      if ((quals & TQ_VOLATILE) != 0) load->setVolatile(true);
      *out_val = load;
      return llvm_gen_be_error_t::ok;
    }
    llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::AllocaInst* tmp = be_state->builder->CreateAlloca(llvm_ty, nullptr, "atomic.load.tmp");
    llvm::FunctionType* fty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*be_state->context), {i64_ty, ptr_ty, ptr_ty, i32_ty}, false);
    llvm::FunctionCallee fn = be_state->module->getOrInsertFunction("__atomic_load", fty);
    be_state->builder->CreateCall(
        fn, {llvm::ConstantInt::get(i64_ty, size), ptr, tmp, llvm::ConstantInt::get(i32_ty, 5)});
    *out_val = be_state->builder->CreateLoad(llvm_ty, tmp);
    return llvm_gen_be_error_t::ok;
  }
  llvm::LoadInst* load = be_state->builder->CreateLoad(llvm_ty, ptr);
  if ((quals & TQ_VOLATILE) != 0) load->setVolatile(true);
  if (be_state->enable_ubsan_checks && is_bool_type(edg_ty) && llvm_ty->isIntegerTy(8)) {
    llvm::Value* is_invalid_bool = be_state->builder->CreateICmpUGT(
        load, llvm::ConstantInt::get(llvm_ty, 1), "ubsan.bool.invalid");
    LLVM_GEN_BE_CHECK(emit_ubsan_check_branch(is_invalid_bool, "__ubsan_handle_load_invalid_value"));
  }
  if (be_state->opt_level != llvm_opt_level_t::O0 &&
      (llvm_ty->isIntegerTy() || llvm_ty->isFloatingPointTy() || llvm_ty->isPointerTy())) {
    llvm::MDNode* tbaa_tag = nullptr;
    LLVM_GEN_BE_CHECK(llvm_get_tbaa_access_tag(edg_ty, edg_ty, 0, &tbaa_tag));
    load->setMetadata(llvm::LLVMContext::MD_tbaa, tbaa_tag);
  }
  *out_val = load;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Coerces a scalar value (`sc`) to the element type of `vec_ty` and splats it into a vector.
 * @details Handles floating-point (`FPCast` / `SIToFP`) and integer (`IntCast`) element conversions.
 * @param[in] sc Scalar `llvm::Value*` to broadcast.
 * @param[in] vec_ty Target `llvm::VectorType*`.
 * @param[out] out_splat Receives the broadcasted vector value.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t broadcast_scalar_to_vector(
    llvm::Value* sc,
    llvm::VectorType* vec_ty,
    llvm::Value** out_splat) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  llvm::Type* elem_ty = vec_ty->getElementType();
  if (sc->getType() != elem_ty) {
    if (elem_ty->isFloatingPointTy()) {
      sc = sc->getType()->isFloatingPointTy()
               ? be_state->builder->CreateFPCast(sc, elem_ty)
               : be_state->builder->CreateSIToFP(sc, elem_ty);
    } else {
      sc = be_state->builder->CreateIntCast(sc, elem_ty, true);
    }
  }
  *out_splat = be_state->builder->CreateVectorSplat(vec_ty->getElementCount(), sc);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers the first and (optional) second operands of a binary EDG expression.
 * @details Evaluates `op1` into `out_v1` and `op1->next` (if present) into `out_v2`.
 * @param[in] op1 Pointer to the first operand node.
 * @param[out] out_v1 Receives the lowered first operand value.
 * @param[out] out_v2 Receives the lowered second operand value (or nullptr if unary).
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t lower_binary_operands(
    an_expr_node_ptr op1,
    llvm::Value** out_v1,
    llvm::Value** out_v2) noexcept {
  *out_v1 = nullptr;
  *out_v2 = nullptr;
  llvm_gen_be_error_t err = llvm_lower_expression(op1, out_v1);
  if (err != llvm_gen_be_error_t::ok) return err;
  if (op1->next) {
    err = llvm_lower_expression(op1->next, out_v2);
    if (err != llvm_gen_be_error_t::ok) return err;
    llvm::Type* t1 = (*out_v1)->getType();
    llvm::Type* t2 = (*out_v2)->getType();
    if (t1->isVectorTy() && !t2->isVectorTy()) {
      LLVM_GEN_BE_CHECK(broadcast_scalar_to_vector(
          *out_v2, llvm::cast<llvm::VectorType>(t1), out_v2));
    } else if (!t1->isVectorTy() && t2->isVectorTy()) {
      LLVM_GEN_BE_CHECK(broadcast_scalar_to_vector(
          *out_v1, llvm::cast<llvm::VectorType>(t2), out_v1));
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an lvalue expression into its corresponding memory address (`llvm::Value*`).
 * @details Handles local/global variables, array subscripts (including VLAs), struct/union field
 *          selections, pointer-to-member field accesses, dereferences, and complex real/imag parts.
 * @param[in] expr Pointer to the EDG lvalue expression node.
 * @param[out] out_ptr Receives the resulting address pointer value.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_lvalue_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_ptr) noexcept {
  if (!expr || !out_ptr) return llvm_gen_be_error_t::invalid_argument;
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  
  switch (expr->kind) {
    case enk_constant: {
       a_constant_ptr con = expr->variant.constant.ptr;
       if (!con) {
           return llvm_gen_be_error_t::unsupported_expr;
       }
       if (con->kind == ck_string) {
           llvm::Constant* out_const = nullptr;
           LLVM_GEN_BE_CHECK(evaluate_constant(
               con,
               llvm::PointerType::getUnqual(*be_state->context),
               &out_const));
           *out_ptr = out_const;
           return llvm_gen_be_error_t::ok;
       }
       if (!expr->type || con->kind != ck_aggregate) {
           return llvm_gen_be_error_t::unsupported_expr;
       }
       llvm::Type* c_ty = nullptr;
       LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &c_ty));
       llvm::Constant* out_const = nullptr;
       LLVM_GEN_BE_CHECK(evaluate_constant(con, c_ty, &out_const));
       llvm::GlobalVariable* gv = nullptr;
       LLVM_GEN_BE_CHECK(llvm_create_global_variable(
           be_state->module.get(),
           out_const->getType(),
           true,
           llvm::GlobalValue::PrivateLinkage,
           out_const,
           ".const.lval",
           &gv));
       *out_ptr = gv;
       return llvm_gen_be_error_t::ok;
    }
    case enk_routine: {
       a_routine_ptr r = expr->variant.routine.ptr;
       if (!r) return llvm_gen_be_error_t::unsupported_expr;
       llvm::Function* fn = nullptr;
       LLVM_GEN_BE_CHECK(llvm_get_or_declare_routine(r, &fn));
       if (!fn) {
         return llvm_lower_expression(expr, out_ptr);
       }
       *out_ptr = fn;
       return llvm_gen_be_error_t::ok;
    }
    case enk_variable: {
       a_variable_ptr var = expr->variant.variable.ptr;
       if (!var) return llvm_gen_be_error_t::unsupported_expr;
       llvm::Value* ptr = nullptr;
       auto it = be_state->local_vars.find(var);
       if (it != be_state->local_vars.end()) {
         ptr = it->second;
         if (be_state->vla_ptr_slots.count(var) > 0 && be_state->builder->GetInsertBlock()) {
           ptr = be_state->builder->CreateLoad(
               llvm::PointerType::getUnqual(*be_state->context), ptr, "vla.ptr");
         }
       } else if ((var->storage_class == sc_auto || var->storage_class == sc_register) &&
                  var->type && be_state->builder->GetInsertBlock()) {
         llvm::Type* var_ty = nullptr;
         LLVM_GEN_BE_CHECK(get_llvm_type(var->type, &var_ty));
         llvm::Function* cur_fn = be_state->builder->GetInsertBlock()->getParent();
         llvm::IRBuilder<> entry_b(&cur_fn->getEntryBlock(), cur_fn->getEntryBlock().begin());
         llvm::AllocaInst* local_slot = entry_b.CreateAlloca(
             var_ty, nullptr, var->source_corresp.name ? var->source_corresp.name : "coro.local");
         be_state->local_vars[var] = local_slot;
         ptr = local_slot;
       } else if (!var->source_corresp.name &&
                  (var->storage_class == sc_static || var->is_thread_local) &&
                  var->type) {
         llvm::Type* var_ty = nullptr;
         LLVM_GEN_BE_CHECK(get_llvm_type(var->type, &var_ty));
         llvm::Constant* anon_init = nullptr;
         if (var->init_kind == initk_static && var->initializer.constant != nullptr) {
           LLVM_GEN_BE_CHECK(evaluate_constant(var->initializer.constant, var_ty, &anon_init));
         }
         if (!anon_init) {
           anon_init = llvm::Constant::getNullValue(var_ty);
         }
         static uint64_t s_unnamed_static_id = 0;
         std::string anon_name = ".edg.anon.static." + std::to_string(++s_unnamed_static_id);
         llvm::GlobalVariable* anon_gv = nullptr;
         llvm::GlobalValue::ThreadLocalMode tls_m = var->is_thread_local
                                                        ? llvm::GlobalValue::GeneralDynamicTLSModel
                                                        : llvm::GlobalValue::NotThreadLocal;
         LLVM_GEN_BE_CHECK(llvm_create_global_variable(
             be_state->module.get(), anon_init->getType(), false, llvm::GlobalValue::InternalLinkage,
             anon_init, anon_name, &anon_gv, tls_m));
         be_state->local_vars[var] = anon_gv;
         ptr = anon_gv;
       } else if (var->source_corresp.name) {
         const char* name = nullptr;
         LLVM_GEN_BE_CHECK(llvm_get_variable_symbol_name(var, &name));
         ptr = be_state->module->getNamedGlobal(name);
         if (!ptr) {
           llvm::Type* var_ty = nullptr;
           llvm_gen_be_error_t err = get_llvm_type(expr->type, &var_ty);
           if (err != llvm_gen_be_error_t::ok) return err;
           llvm::GlobalVariable* ptr_gv_alloc = nullptr;
           LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), var_ty, false, llvm::GlobalValue::ExternalLinkage, nullptr, name, &ptr_gv_alloc));
           ptr = ptr_gv_alloc;
         }
       } else {
         return llvm_gen_be_error_t::unsupported_expr;
       }
       *out_ptr = ptr;
       return llvm_gen_be_error_t::ok;
    }
    case enk_temp_init:
    case enk_lambda: {
       llvm::Type* expr_llvm_ty = nullptr;
       LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &expr_llvm_ty));
       a_dynamic_init_ptr dip = expr->variant.init.dynamic_init;
       llvm::Value* temp_storage = nullptr;
       if (dip && dip->static_temp) {
         llvm::GlobalVariable* temp_storage_gv_alloc = nullptr;
         LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), expr_llvm_ty, false, llvm::GlobalValue::InternalLinkage, llvm::Constant::getNullValue(expr_llvm_ty), ".compoundliteral", &temp_storage_gv_alloc));
         temp_storage = temp_storage_gv_alloc;
       } else {
         llvm::Function* parent_func = be_state->builder->GetInsertBlock()->getParent();
         llvm::IRBuilder<> tmp_builder(&parent_func->getEntryBlock(), parent_func->getEntryBlock().begin());
         temp_storage = tmp_builder.CreateAlloca(expr_llvm_ty, nullptr, "temp.lval");
       }
       if (dip) {
         be_state->reused_values[dip] = temp_storage;
         LLVM_GEN_BE_CHECK(llvm_emit_dynamic_init(dip, temp_storage, expr_llvm_ty));
         if (dip->destructor && dip->has_temporary_lifetime) {
           LLVM_GEN_BE_CHECK(llvm_emit_eh_cleanup(dip->destructor, temp_storage));
         }
       }
       *out_ptr = temp_storage;
       return llvm_gen_be_error_t::ok;
    }
    case enk_operation: {
       an_expr_node_ptr op1 = expr->variant.operation.operands;
       an_expr_node_ptr op2 = op1->next;
       an_expr_operator_kind k = expr->variant.operation.kind;

       if (k == eok_parens || k == eok_lvalue || k == eok_lvalue_adjust ||
           k == eok_lvalue_cast || k == eok_array_to_pointer) {
         return llvm_lower_lvalue_expression(op1, out_ptr);
       }

       if (k == eok_question) {
         an_expr_node_ptr true_expr = op2;
         an_expr_node_ptr false_expr = true_expr->next;
         llvm::Value* cond = nullptr;
         LLVM_GEN_BE_CHECK(llvm_lower_expression(op1, &cond));
         if (!cond->getType()->isIntegerTy(1)) {
           cond = be_state->builder->CreateICmpNE(cond, llvm::Constant::getNullValue(cond->getType()));
         }
         llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
         llvm::BasicBlock* true_bb = llvm::BasicBlock::Create(*be_state->context, "lcond.true", func);
         llvm::BasicBlock* false_bb = llvm::BasicBlock::Create(*be_state->context, "lcond.false");
         llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, "lcond.end");
         be_state->builder->CreateCondBr(cond, true_bb, false_bb);

         be_state->builder->SetInsertPoint(true_bb);
         llvm::Value* true_ptr = nullptr;
         LLVM_GEN_BE_CHECK(llvm_lower_lvalue_expression(true_expr, &true_ptr));
         llvm::BasicBlock* true_end_bb = be_state->builder->GetInsertBlock();
         be_state->builder->CreateBr(end_bb);

         func->insert(func->end(), false_bb);
         be_state->builder->SetInsertPoint(false_bb);
         llvm::Value* false_ptr = nullptr;
         LLVM_GEN_BE_CHECK(llvm_lower_lvalue_expression(false_expr, &false_ptr));
         llvm::BasicBlock* false_end_bb = be_state->builder->GetInsertBlock();
         be_state->builder->CreateBr(end_bb);

         func->insert(func->end(), end_bb);
         be_state->builder->SetInsertPoint(end_bb);
         llvm::PHINode* phi = be_state->builder->CreatePHI(
             llvm::PointerType::getUnqual(*be_state->context), 2, "lcond.phi");
         phi->addIncoming(true_ptr, true_end_bb);
         phi->addIncoming(false_ptr, false_end_bb);
         *out_ptr = phi;
         return llvm_gen_be_error_t::ok;
       }

       if (k == eok_pm_field || k == eok_pm_points_to_field) {
         return llvm_lower_pm_field_access(expr, out_ptr);
       }
       if (k == eok_real_part || k == eok_imag_part) {
         bool is_imag = (k == eok_imag_part);
         llvm::Value* v1 = nullptr;
         llvm_gen_be_error_t err = llvm_lower_lvalue_expression(op1, &v1);
         if (err != llvm_gen_be_error_t::ok) return err;
         llvm::Type* pty = nullptr;
         err = get_llvm_type(op1->type, &pty);
         if (err != llvm_gen_be_error_t::ok) return err;
         if (pty->isStructTy()) {
           *out_ptr = be_state->builder->CreateStructGEP(pty, v1, is_imag ? 1 : 0);
         } else if (!is_imag) {
           *out_ptr = v1;
         } else if (expr->type) {
           llvm::Type* elem_ty = nullptr;
           err = get_llvm_type(expr->type, &elem_ty);
           if (err != llvm_gen_be_error_t::ok) return err;
           if (elem_ty->isVoidTy()) return llvm_gen_be_error_t::unsupported_expr;
           *out_ptr = be_state->builder->CreateInBoundsGEP(
               elem_ty, v1, llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), 1));
         } else {
           return llvm_gen_be_error_t::unsupported_expr;
         }
         return llvm_gen_be_error_t::ok;
       }
       if (k == eok_dot_static || k == eok_points_to_static || k == eok_comma) {
         llvm::Value* v1 = nullptr;
         LLVM_GEN_BE_CHECK(llvm_lower_expression(op1, &v1));
         return llvm_lower_lvalue_expression(op2, out_ptr);
       }
       if (k == eok_call && expr->type) {
         llvm::Type* tmp_ty = nullptr;
         LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &tmp_ty));
         llvm::Type* alloc_ty = tmp_ty->isSized() ? tmp_ty : llvm::Type::getInt8Ty(*be_state->context);
         llvm::Function* cur_fn = be_state->builder->GetInsertBlock()->getParent();
         llvm::IRBuilder<> entry_b(&cur_fn->getEntryBlock(), cur_fn->getEntryBlock().begin());
         llvm::AllocaInst* tmp_slot = entry_b.CreateAlloca(alloc_ty, nullptr, "rval.call.tmp");
         llvm::Value* prev_dest = be_state->active_prvalue_dest_ptr;
         llvm::Type* prev_dest_ty = be_state->active_prvalue_dest_ty;
         bool prev_in_place = be_state->prvalue_constructed_in_place;
         be_state->active_prvalue_dest_ptr = tmp_slot;
         be_state->active_prvalue_dest_ty = tmp_ty;
         be_state->prvalue_constructed_in_place = false;
         llvm::Value* rval = nullptr;
         llvm_gen_be_error_t call_err = llvm_lower_expression(expr, &rval);
         bool in_place = be_state->prvalue_constructed_in_place;
         be_state->active_prvalue_dest_ptr = prev_dest;
         be_state->active_prvalue_dest_ty = prev_dest_ty;
         be_state->prvalue_constructed_in_place = prev_in_place;
         LLVM_GEN_BE_CHECK(call_err);
         if (!in_place && !rval->getType()->isVoidTy()) {
           be_state->builder->CreateStore(rval, tmp_slot);
         }
         *out_ptr = tmp_slot;
         return llvm_gen_be_error_t::ok;
       }
       if (k == eok_bassign || k == eok_assign) {
         llvm::Value* rv = nullptr;
         LLVM_GEN_BE_CHECK(llvm_lower_expression(expr, &rv));
         return llvm_lower_lvalue_expression(op1, out_ptr);
       }
       if (k != eok_subscript && k != eok_vector_subscript &&
           k != eok_dot_field && k != eok_points_to_field &&
           k != eok_indirect && k != eok_ref_indirect) {
         return llvm_gen_be_set_error(
             &be_state->err_context, llvm_gen_be_error_t::unsupported_expr,
             __FILE__, __LINE__, 0, "Unsupported lvalue op %d", static_cast<int>(k));
       }

       llvm::Value* v1 = nullptr;
       bool op1_is_array = (k == eok_subscript && skip_typerefs(op1->type)->kind == tk_array) ||
                           (k == eok_vector_subscript);
       if (k == eok_dot_field || op1_is_array) {
         LLVM_GEN_BE_CHECK(llvm_lower_lvalue_expression(op1, &v1));
       } else {
         LLVM_GEN_BE_CHECK(llvm_lower_expression(op1, &v1));
       }
       if (!v1->getType()->isPointerTy()) return llvm_gen_be_error_t::unsupported_expr;
       if (k == eok_indirect || k == eok_ref_indirect) {
         *out_ptr = v1;
         return llvm_gen_be_error_t::ok;
       }
       if (k == eok_dot_field || k == eok_points_to_field) {
         if (op2) {
           llvm::Value* offset_val = llvm::ConstantInt::get(
               llvm::Type::getInt64Ty(*be_state->context), static_cast<uint64_t>(op2->variant.field.ptr->offset));
           *out_ptr = be_state->builder->CreateInBoundsGEP(llvm::Type::getInt8Ty(*be_state->context), v1, offset_val);
           return llvm_gen_be_error_t::ok;
         }
         return llvm_gen_be_error_t::unsupported_expr;
       }

       llvm::Value* v2 = nullptr;
       llvm_gen_be_error_t err = llvm_lower_expression(op2, &v2);
       if (err != llvm_gen_be_error_t::ok) return err;
       llvm::Type* elem_ty = nullptr;
       err = get_llvm_type(expr->type, &elem_ty);
       if (err != llvm_gen_be_error_t::ok) return err;

       bool idx_is_signed = true;
       if (op2->type) {
         a_type_ptr idx_ty = skip_typerefs(op2->type);
         if (idx_ty->kind == tk_integer) {
           idx_is_signed = (int_kind_is_signed[idx_ty->variant.integer.int_kind] != FALSE);
         }
       }
       llvm::Value* idx64 = v2->getType()->isIntegerTy()
           ? be_state->builder->CreateIntCast(v2, llvm::Type::getInt64Ty(*be_state->context), idx_is_signed)
           : v2;

       a_type_ptr unwrapped_ty = skip_typerefs(expr->type);
       if (unwrapped_ty->kind == tk_array) {
           llvm::Value* total_size = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), 1);
           a_type_ptr ty = unwrapped_ty;
           while (ty->kind == tk_array) {
               llvm::Value* dim_size = nullptr;
               if (ty->variant.array.is_vla) {
                   a_vla_dimension_ptr dim = be_state->array_to_vla_dim[ty];
                   dim_size = be_state->builder->CreateZExtOrTrunc(
                       be_state->vla_dimension_values[dim], llvm::Type::getInt64Ty(*be_state->context));
               } else {
                   uint64_t n = ty->variant.array.bound_is_zero ? 0 : (uint64_t)ty->variant.array.variant.number_of_elements;
                   dim_size = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), n);
               }
               total_size = be_state->builder->CreateMul(total_size, dim_size);
               ty = skip_typerefs(ty->variant.array.element_type);
           }
           llvm::Type* inner_llvm_ty = nullptr;
           err = get_llvm_type(ty, &inner_llvm_ty);
           if (err != llvm_gen_be_error_t::ok) return err;
           
           const llvm::DataLayout &DL = be_state->module->getDataLayout();
           uint64_t elem_size = DL.getTypeAllocSize(inner_llvm_ty);
           llvm::Value* elem_size_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), elem_size);
           llvm::Value* byte_stride = be_state->builder->CreateMul(total_size, elem_size_val);
           
           llvm::Value* byte_offset = be_state->builder->CreateMul(idx64, byte_stride);
           
           *out_ptr = be_state->builder->CreateInBoundsGEP(llvm::Type::getInt8Ty(*be_state->context), v1, byte_offset);
           return llvm_gen_be_error_t::ok;
       }

       *out_ptr = be_state->builder->CreateInBoundsGEP(elem_ty, v1, idx64);
       return llvm_gen_be_error_t::ok;
    }
    default:
       return llvm_gen_be_set_error(
           &be_state->err_context, llvm_gen_be_error_t::unsupported_expr,
           __FILE__, __LINE__, 0, "Unsupported lvalue kind %d", static_cast<int>(expr->kind));
  }
}

/**
 * @brief Lowers an arithmetic, bitwise, pointer arithmetic, or complex arithmetic expression.
 * @details Supports integer, floating-point, vector, pointer, and C99 `_Complex` operations.
 * @param[in] expr Pointer to the EDG arithmetic operation expression node.
 * @param[out] out_val Receives the computed `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_arithmetic_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  an_expr_node_ptr op1 = expr->variant.operation.operands;
  llvm::Value* v1 = nullptr;
  llvm::Value* v2 = nullptr;
  LLVM_GEN_BE_CHECK(lower_binary_operands(op1, &v1, &v2));

#if FIXED_POINT_ALLOWED
  if (expr->type && skip_typerefs(expr->type)->kind == tk_fixed_point && v2 != nullptr) {
    unsigned frac_bits = 15;
    bool is_signed = true;
    bool is_sat = false;
    LLVM_GEN_BE_CHECK(llvm_get_fixed_point_scale(expr->type, &frac_bits, &is_signed, &is_sat));
    llvm::Type* fx_ty = v1->getType();
    llvm::Value* scale_arg = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), frac_bits);
    an_expr_operator_kind k = expr->variant.operation.kind;
    if (k == eok_add || k == eok_subtract) {
      if (is_sat) {
        llvm::Intrinsic::ID sat_id = (k == eok_add)
            ? (is_signed ? llvm::Intrinsic::sadd_sat : llvm::Intrinsic::uadd_sat)
            : (is_signed ? llvm::Intrinsic::ssub_sat : llvm::Intrinsic::usub_sat);
        llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), sat_id, {fx_ty});
        *out_val = be_state->builder->CreateCall(fn, {v1, v2});
      } else {
        *out_val = (k == eok_add) ? be_state->builder->CreateAdd(v1, v2)
                                  : be_state->builder->CreateSub(v1, v2);
      }
      return llvm_gen_be_error_t::ok;
    }
    if (k == eok_multiply || k == eok_divide) {
      llvm::Intrinsic::ID fx_id = (k == eok_multiply)
          ? (is_sat ? (is_signed ? llvm::Intrinsic::smul_fix_sat : llvm::Intrinsic::umul_fix_sat)
                    : (is_signed ? llvm::Intrinsic::smul_fix : llvm::Intrinsic::umul_fix))
          : (is_sat ? (is_signed ? llvm::Intrinsic::sdiv_fix_sat : llvm::Intrinsic::udiv_fix_sat)
                    : (is_signed ? llvm::Intrinsic::sdiv_fix : llvm::Intrinsic::udiv_fix));
      llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), fx_id, {fx_ty});
      *out_val = be_state->builder->CreateCall(fn, {v1, v2, scale_arg});
      return llvm_gen_be_error_t::ok;
    }
  }
#endif

  unsigned expr_dec_bytes = 0;
  LLVM_GEN_BE_CHECK(llvm_get_decimal_float_byte_width(expr->type, &expr_dec_bytes));
  if (expr_dec_bytes != 0) {
    an_expr_operator_kind dk = expr->variant.operation.kind;
    if (dk == eok_unary_plus) {
      *out_val = v1;
      return llvm_gen_be_error_t::ok;
    }
    if (dk == eok_negate) {
      unsigned bw = v1->getType()->getIntegerBitWidth();
      llvm::Value* sign_mask = llvm::ConstantInt::get(
          v1->getType(), llvm::APInt::getSignMask(bw));
      *out_val = be_state->builder->CreateXor(v1, sign_mask);
      return llvm_gen_be_error_t::ok;
    }
    if (dk == eok_add || dk == eok_subtract || dk == eok_multiply || dk == eok_divide) {
      return llvm_lower_decimal_float_binop(expr_dec_bytes, dk, v1, v2, out_val);
    }
  }

  switch (expr->variant.operation.kind) {
    case eok_negate:
      if (v1->getType()->isStructTy()) {
        llvm::Value* r1 = be_state->builder->CreateExtractValue(v1, 0);
        llvm::Value* i1 = be_state->builder->CreateExtractValue(v1, 1);
        bool fp_c = r1->getType()->isFloatingPointTy();
        llvm::Value* r = fp_c ? be_state->builder->CreateFNeg(r1) : be_state->builder->CreateNeg(r1);
        llvm::Value* i = fp_c ? be_state->builder->CreateFNeg(i1) : be_state->builder->CreateNeg(i1);
        llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(v1->getType()), r, 0);
        *out_val = be_state->builder->CreateInsertValue(res, i, 1);
      } else if (v1->getType()->isFPOrFPVectorTy()) {
        *out_val = be_state->builder->CreateFNeg(v1);
      } else if (be_state->enable_ubsan_checks && v1->getType()->isIntegerTy() &&
                 int_kind_is_signed[skip_typerefs(op1->type)->variant.integer.int_kind]) {
        llvm::Value* zero = llvm::ConstantInt::get(v1->getType(), 0);
        llvm::Function* intr = EDG_LLVM_GET_INTRINSIC_DECL(
            be_state->module.get(), llvm::Intrinsic::ssub_with_overflow, {v1->getType()});
        llvm::Value* pair = be_state->builder->CreateCall(intr, {zero, v1});
        llvm::Value* ovf = be_state->builder->CreateExtractValue(pair, 1);
        LLVM_GEN_BE_CHECK(llvm_emit_ubsan_check("__ubsan_handle_negate_overflow", ovf, {v1}));
        *out_val = be_state->builder->CreateExtractValue(pair, 0);
      } else {
        *out_val = be_state->builder->CreateNeg(v1);
      }
      break;
    case eok_unary_plus:
      *out_val = v1;
      break;
    case eok_add:
      if (v1->getType()->isStructTy()) {
        llvm::Value* r1 = be_state->builder->CreateExtractValue(v1, 0);
        llvm::Value* i1 = be_state->builder->CreateExtractValue(v1, 1);
        llvm::Value* r2 = be_state->builder->CreateExtractValue(v2, 0);
        llvm::Value* i2 = be_state->builder->CreateExtractValue(v2, 1);
        bool fp_c = r1->getType()->isFloatingPointTy();
        llvm::Value* r = fp_c ? be_state->builder->CreateFAdd(r1, r2) : be_state->builder->CreateAdd(r1, r2);
        llvm::Value* i = fp_c ? be_state->builder->CreateFAdd(i1, i2) : be_state->builder->CreateAdd(i1, i2);
        llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(v1->getType()), r, 0);
        *out_val = be_state->builder->CreateInsertValue(res, i, 1);
      } else if (v1->getType()->isFPOrFPVectorTy()) *out_val = be_state->builder->CreateFAdd(v1, v2);
      else if (be_state->enable_ubsan_checks && v1->getType()->isIntegerTy() &&
               int_kind_is_signed[skip_typerefs(op1->type)->variant.integer.int_kind]) {
        llvm::Function* intr = EDG_LLVM_GET_INTRINSIC_DECL(
            be_state->module.get(), llvm::Intrinsic::sadd_with_overflow, {v1->getType()});
        llvm::Value* pair = be_state->builder->CreateCall(intr, {v1, v2});
        llvm::Value* ovf = be_state->builder->CreateExtractValue(pair, 1);
        LLVM_GEN_BE_CHECK(emit_ubsan_check_branch(ovf, "__ubsan_handle_add_overflow"));
        *out_val = be_state->builder->CreateExtractValue(pair, 0);
      } else *out_val = be_state->builder->CreateAdd(v1, v2);
      break;
    case eok_subtract:
      if (v1->getType()->isStructTy()) {
        llvm::Value* r1 = be_state->builder->CreateExtractValue(v1, 0);
        llvm::Value* i1 = be_state->builder->CreateExtractValue(v1, 1);
        llvm::Value* r2 = be_state->builder->CreateExtractValue(v2, 0);
        llvm::Value* i2 = be_state->builder->CreateExtractValue(v2, 1);
        bool fp_c = r1->getType()->isFloatingPointTy();
        llvm::Value* r = fp_c ? be_state->builder->CreateFSub(r1, r2) : be_state->builder->CreateSub(r1, r2);
        llvm::Value* i = fp_c ? be_state->builder->CreateFSub(i1, i2) : be_state->builder->CreateSub(i1, i2);
        llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(v1->getType()), r, 0);
        *out_val = be_state->builder->CreateInsertValue(res, i, 1);
      } else if (v1->getType()->isFPOrFPVectorTy()) *out_val = be_state->builder->CreateFSub(v1, v2);
      else if (be_state->enable_ubsan_checks && v1->getType()->isIntegerTy() &&
               int_kind_is_signed[skip_typerefs(op1->type)->variant.integer.int_kind]) {
        llvm::Function* intr = EDG_LLVM_GET_INTRINSIC_DECL(
            be_state->module.get(), llvm::Intrinsic::ssub_with_overflow, {v1->getType()});
        llvm::Value* pair = be_state->builder->CreateCall(intr, {v1, v2});
        llvm::Value* ovf = be_state->builder->CreateExtractValue(pair, 1);
        LLVM_GEN_BE_CHECK(emit_ubsan_check_branch(ovf, "__ubsan_handle_sub_overflow"));
        *out_val = be_state->builder->CreateExtractValue(pair, 0);
      } else *out_val = be_state->builder->CreateSub(v1, v2);
      break;
    case eok_multiply:
      if (v1->getType()->isStructTy()) {
        LLVM_GEN_BE_CHECK(emit_complex_libcall("__mulsc3", "__muldc3", v1, v2, out_val));
      } else if (v1->getType()->isFPOrFPVectorTy()) *out_val = be_state->builder->CreateFMul(v1, v2);
      else if (be_state->enable_ubsan_checks && v1->getType()->isIntegerTy() &&
               int_kind_is_signed[skip_typerefs(op1->type)->variant.integer.int_kind]) {
        llvm::Function* intr = EDG_LLVM_GET_INTRINSIC_DECL(
            be_state->module.get(), llvm::Intrinsic::smul_with_overflow, {v1->getType()});
        llvm::Value* pair = be_state->builder->CreateCall(intr, {v1, v2});
        llvm::Value* ovf = be_state->builder->CreateExtractValue(pair, 1);
        LLVM_GEN_BE_CHECK(emit_ubsan_check_branch(ovf, "__ubsan_handle_mul_overflow"));
        *out_val = be_state->builder->CreateExtractValue(pair, 0);
      } else *out_val = be_state->builder->CreateMul(v1, v2);
      break;
    case eok_divide:
      if (v1->getType()->isStructTy()) {
        LLVM_GEN_BE_CHECK(emit_complex_libcall("__divsc3", "__divdc3", v1, v2, out_val));
      } else if (v1->getType()->isFPOrFPVectorTy()) *out_val = be_state->builder->CreateFDiv(v1, v2);
      else {
        if (be_state->enable_ubsan_checks && v2->getType()->isIntegerTy()) {
          llvm::Value* is_zero = be_state->builder->CreateICmpEQ(
              v2, llvm::ConstantInt::get(v2->getType(), 0));
          LLVM_GEN_BE_CHECK(emit_ubsan_check_branch(is_zero, "__ubsan_handle_divrem_overflow"));
        }
        if (int_kind_is_signed[skip_typerefs(op1->type)->variant.integer.int_kind]) *out_val = be_state->builder->CreateSDiv(v1, v2);
        else *out_val = be_state->builder->CreateUDiv(v1, v2);
      }
      break;
    case eok_remainder:
      if (v1->getType()->isFPOrFPVectorTy()) *out_val = be_state->builder->CreateFRem(v1, v2);
      else {
        if (be_state->enable_ubsan_checks && v2->getType()->isIntegerTy()) {
          llvm::Value* is_zero = be_state->builder->CreateICmpEQ(
              v2, llvm::ConstantInt::get(v2->getType(), 0));
          LLVM_GEN_BE_CHECK(emit_ubsan_check_branch(is_zero, "__ubsan_handle_divrem_overflow"));
        }
        if (!int_kind_is_signed[skip_typerefs(expr->type)->variant.integer.int_kind]) *out_val = be_state->builder->CreateURem(v1, v2);
        else *out_val = be_state->builder->CreateSRem(v1, v2);
      }
      break;
    case eok_gnu_min:
    case eok_gnu_max: {
      bool is_min = (expr->variant.operation.kind == eok_gnu_min);
      llvm::Value* cmp = nullptr;
      if (v1->getType()->isFPOrFPVectorTy()) {
        cmp = is_min ? be_state->builder->CreateFCmpOLT(v1, v2)
                     : be_state->builder->CreateFCmpOGT(v1, v2);
      } else {
        bool is_signed = int_kind_is_signed[skip_typerefs(op1->type)->variant.integer.int_kind];
        llvm::CmpInst::Predicate pred = is_min
            ? (is_signed ? llvm::CmpInst::ICMP_SLT : llvm::CmpInst::ICMP_ULT)
            : (is_signed ? llvm::CmpInst::ICMP_SGT : llvm::CmpInst::ICMP_UGT);
        cmp = be_state->builder->CreateICmp(pred, v1, v2);
      }
      *out_val = be_state->builder->CreateSelect(cmp, v1, v2);
      break;
    }
    case eok_padd:
    case eok_psubtract:
    case eok_pdiff: {
      a_type_ptr ptr_edg_ty = op1->type ? skip_typerefs(op1->type) : nullptr;
      a_type_ptr idx_edg_ty = op1->next->type ? skip_typerefs(op1->next->type) : nullptr;
      llvm::Value* base_ptr = v1;
      llvm::Value* idx_val = v2;
      if (!ptr_edg_ty || ptr_edg_ty->kind != tk_pointer) {
        ptr_edg_ty = op1->next->type ? skip_typerefs(op1->next->type) : nullptr;
        idx_edg_ty = op1->type ? skip_typerefs(op1->type) : nullptr;
        base_ptr = v2;
        idx_val = v1;
      }
      if (ptr_edg_ty && ptr_edg_ty->kind == tk_pointer) {
        llvm::Type* elem_ty = nullptr;
        if (get_llvm_type(ptr_edg_ty->variant.pointer.type, &elem_ty) != llvm_gen_be_error_t::ok) return llvm_gen_be_error_t::unsupported_expr;
        if (expr->variant.operation.kind == eok_pdiff) {
          *out_val = be_state->builder->CreatePtrDiff(elem_ty, v1, v2);
        } else {
          bool idx_signed = true;
          if (idx_edg_ty && idx_edg_ty->kind == tk_integer) {
            idx_signed = (int_kind_is_signed[idx_edg_ty->variant.integer.int_kind] != FALSE);
          }
          llvm::Value* idx64 = idx_val->getType()->isIntegerTy()
              ? be_state->builder->CreateIntCast(idx_val, llvm::Type::getInt64Ty(*be_state->context), idx_signed)
              : idx_val;
          llvm::Value* idx = (expr->variant.operation.kind == eok_psubtract) ? be_state->builder->CreateNeg(idx64) : idx64;
          *out_val = be_state->builder->CreateGEP(elem_ty, base_ptr, idx);
        }
      } else if (expr->variant.operation.kind == eok_pdiff) {
        return llvm_gen_be_error_t::unsupported_expr;
      } else {
        *out_val = v1;
      }
      break;
    }
    case eok_fjadd: {
      llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(llvm::StructType::get(v1->getType(), v2->getType())), v1, 0);
      *out_val = be_state->builder->CreateInsertValue(res, v2, 1);
      break;
    }
    case eok_jfadd: {
      llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(llvm::StructType::get(v2->getType(), v1->getType())), v2, 0);
      *out_val = be_state->builder->CreateInsertValue(res, v1, 1);
      break;
    }
    case eok_fjsubtract: {
      llvm::Value* neg_v2 = be_state->builder->CreateFNeg(v2);
      llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(llvm::StructType::get(v1->getType(), v2->getType())), v1, 0);
      *out_val = be_state->builder->CreateInsertValue(res, neg_v2, 1);
      break;
    }
    case eok_jfsubtract: {
      llvm::Value* neg_v1 = be_state->builder->CreateFNeg(v1);
      llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(llvm::StructType::get(v2->getType(), v1->getType())), neg_v1, 0);
      *out_val = be_state->builder->CreateInsertValue(res, v1, 1);
      break;
    }
    case eok_jmultiply: {
      llvm::Value* mul = be_state->builder->CreateFMul(v1, v2);
      *out_val = be_state->builder->CreateFNeg(mul);
      break;
    }
    case eok_jdivide: {
      llvm::Value* div = be_state->builder->CreateFDiv(v1, v2);
      *out_val = be_state->builder->CreateFNeg(div);
      break;
    }
    case eok_xconj: {
      if (v1->getType()->isStructTy()) {
          llvm::Value* r1 = be_state->builder->CreateExtractValue(v1, 0);
          llvm::Value* i1 = be_state->builder->CreateExtractValue(v1, 1);
          llvm::Value* neg_i1 = i1->getType()->isFloatingPointTy()
                                    ? be_state->builder->CreateFNeg(i1)
                                    : be_state->builder->CreateNeg(i1);
          llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(v1->getType()), r1, 0);
          *out_val = be_state->builder->CreateInsertValue(res, neg_i1, 1);
      } else {
          *out_val = v1;
      }
      break;
    }
    case eok_complement: {
      if (v1->getType()->isStructTy()) {
          llvm::Value* r1 = be_state->builder->CreateExtractValue(v1, 0);
          llvm::Value* i1 = be_state->builder->CreateExtractValue(v1, 1);
          llvm::Value* neg_i1 = i1->getType()->isFloatingPointTy()
                                    ? be_state->builder->CreateFNeg(i1)
                                    : be_state->builder->CreateNeg(i1);
          llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(v1->getType()), r1, 0);
          *out_val = be_state->builder->CreateInsertValue(res, neg_i1, 1);
      } else {
          *out_val = be_state->builder->CreateNot(v1);
      }
      break;
    }
    case eok_and: *out_val = be_state->builder->CreateAnd(v1, v2); break;
    case eok_or: *out_val = be_state->builder->CreateOr(v1, v2); break;
    case eok_xor: *out_val = be_state->builder->CreateXor(v1, v2); break;
    case eok_shiftl:
      if (be_state->enable_ubsan_checks && v1->getType()->isIntegerTy()) {
        unsigned bw = v1->getType()->getIntegerBitWidth();
        llvm::Value* oob = be_state->builder->CreateICmpUGE(v2, llvm::ConstantInt::get(v2->getType(), bw));
        LLVM_GEN_BE_CHECK(emit_ubsan_check_branch(oob, "__ubsan_handle_shift_out_of_bounds"));
      }
      *out_val = be_state->builder->CreateShl(v1, be_state->builder->CreateZExtOrTrunc(v2, v1->getType()));
      break;
    case eok_shiftr: {
      if (be_state->enable_ubsan_checks && v1->getType()->isIntegerTy()) {
        unsigned bw = v1->getType()->getIntegerBitWidth();
        llvm::Value* oob = be_state->builder->CreateICmpUGE(v2, llvm::ConstantInt::get(v2->getType(), bw));
        LLVM_GEN_BE_CHECK(emit_ubsan_check_branch(oob, "__ubsan_handle_shift_out_of_bounds"));
      }
      llvm::Value* sh_v2 = be_state->builder->CreateZExtOrTrunc(v2, v1->getType());
      if (!int_kind_is_signed[skip_typerefs(op1->type)->variant.integer.int_kind]) *out_val = be_state->builder->CreateLShr(v1, sh_v2);
      else *out_val = be_state->builder->CreateAShr(v1, sh_v2);
      break;
    }
    default: return llvm_gen_be_error_t::unsupported_expr;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a logical (`&&`, `||`, `!`) or relational (`==`, `!=`, `<`, `<=`, `>`, `>=`, `<=>`) expression.
 * @details Emits short-circuit basic blocks and PHI nodes for `&&`/`||`, three-way comparison selects for `<=>`,
 *          and `icmp`/`fcmp` instructions for scalar, pointer, and vector comparisons.
 * @param[in] expr Pointer to the EDG logical/relational expression node.
 * @param[out] out_val Receives the computed boolean or integer comparison result.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_logical_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  an_expr_node_ptr op1 = expr->variant.operation.operands;
  an_expr_node_ptr op2 = op1->next;
  
  if (skip_typerefs(op1->type)->kind == tk_ptr_to_member) {
    return llvm_lower_pm_comparison(expr, out_val);
  }

  llvm::Type* expr_llvm_ty = nullptr;
  llvm_gen_be_error_t err = get_llvm_type(expr->type, &expr_llvm_ty);
  if (err != llvm_gen_be_error_t::ok) return err;

  if (expr->variant.operation.kind == eok_land || expr->variant.operation.kind == eok_lor) {
    llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* rhs_bb = llvm::BasicBlock::Create(*be_state->context, 
      (expr->variant.operation.kind == eok_land) ? "land.rhs" : "lor.rhs", func);
    llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, 
      (expr->variant.operation.kind == eok_land) ? "land.end" : "lor.end");

    llvm::Value* lhs_val = nullptr;
    err = llvm_lower_expression(op1, &lhs_val);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    llvm::Value* lhs_cond = be_state->builder->CreateICmpNE(lhs_val, llvm::Constant::getNullValue(lhs_val->getType()));
    llvm::BasicBlock* lhs_end_bb = be_state->builder->GetInsertBlock();
    
    if (expr->variant.operation.kind == eok_land) be_state->builder->CreateCondBr(lhs_cond, rhs_bb, end_bb);
    else be_state->builder->CreateCondBr(lhs_cond, end_bb, rhs_bb);

    be_state->builder->SetInsertPoint(rhs_bb);
    llvm::Value* rhs_val = nullptr;
    err = llvm_lower_expression(op2, &rhs_val);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    llvm::Value* rhs_cond = be_state->builder->CreateICmpNE(rhs_val, llvm::Constant::getNullValue(rhs_val->getType()));
    llvm::BasicBlock* rhs_end_bb = be_state->builder->GetInsertBlock();
    be_state->builder->CreateBr(end_bb);

    func->insert(func->end(), end_bb);
    be_state->builder->SetInsertPoint(end_bb);

    llvm::PHINode* phi = be_state->builder->CreatePHI(be_state->builder->getInt1Ty(), 2, 
      (expr->variant.operation.kind == eok_land) ? "land.phi" : "lor.phi");
    phi->addIncoming((expr->variant.operation.kind == eok_land) ? llvm::ConstantInt::getFalse(*be_state->context) : llvm::ConstantInt::getTrue(*be_state->context), lhs_end_bb);
    phi->addIncoming(rhs_cond, rhs_end_bb);
    
    *out_val = be_state->builder->CreateZExt(phi, expr_llvm_ty);
    return llvm_gen_be_error_t::ok;
  }

  if (expr->variant.operation.kind == eok_vector_land ||
      expr->variant.operation.kind == eok_vector_lor) {
    llvm::Value* v1 = nullptr;
    llvm::Value* v2 = nullptr;
    LLVM_GEN_BE_CHECK(lower_binary_operands(op1, &v1, &v2));
    llvm::Value* c1 = v1->getType()->isFPOrFPVectorTy()
                          ? be_state->builder->CreateFCmpUNE(v1, llvm::Constant::getNullValue(v1->getType()))
                          : be_state->builder->CreateICmpNE(v1, llvm::Constant::getNullValue(v1->getType()));
    llvm::Value* c2 = v2->getType()->isFPOrFPVectorTy()
                          ? be_state->builder->CreateFCmpUNE(v2, llvm::Constant::getNullValue(v2->getType()))
                          : be_state->builder->CreateICmpNE(v2, llvm::Constant::getNullValue(v2->getType()));
    llvm::Value* mask = (expr->variant.operation.kind == eok_vector_land)
                            ? be_state->builder->CreateAnd(c1, c2)
                            : be_state->builder->CreateOr(c1, c2);
    *out_val = be_state->builder->CreateSExt(mask, expr_llvm_ty);
    return llvm_gen_be_error_t::ok;
  }

  llvm::Value* v1 = nullptr;
  llvm::Value* v2 = nullptr;
  err = lower_binary_operands(op1, &v1, &v2);
  if (err != llvm_gen_be_error_t::ok) return err;

  unsigned op1_dec_bytes = 0;
  LLVM_GEN_BE_CHECK(llvm_get_decimal_float_byte_width(op1->type, &op1_dec_bytes));
  if (op1_dec_bytes != 0 && v2 != nullptr) {
    return llvm_lower_decimal_float_cmp(
        op1_dec_bytes, expr->variant.operation.kind, v1, v2, expr_llvm_ty, out_val);
  }

  if (v1->getType()->isStructTy()) {
    llvm::Value* r1 = be_state->builder->CreateExtractValue(v1, 0);
    llvm::Value* i1 = be_state->builder->CreateExtractValue(v1, 1);
    llvm::Value* r2 = be_state->builder->CreateExtractValue(v2, 0);
    llvm::Value* i2 = be_state->builder->CreateExtractValue(v2, 1);
    bool elem_fp = r1->getType()->isFloatingPointTy();
    llvm::Value* cmp_i1 = nullptr;
    if (expr->variant.operation.kind == eok_eq) {
      llvm::Value* r_eq = elem_fp ? be_state->builder->CreateFCmpOEQ(r1, r2) : be_state->builder->CreateICmpEQ(r1, r2);
      llvm::Value* i_eq = elem_fp ? be_state->builder->CreateFCmpOEQ(i1, i2) : be_state->builder->CreateICmpEQ(i1, i2);
      cmp_i1 = be_state->builder->CreateAnd(r_eq, i_eq);
    } else {
      llvm::Value* r_ne = elem_fp ? be_state->builder->CreateFCmpUNE(r1, r2) : be_state->builder->CreateICmpNE(r1, r2);
      llvm::Value* i_ne = elem_fp ? be_state->builder->CreateFCmpUNE(i1, i2) : be_state->builder->CreateICmpNE(i1, i2);
      cmp_i1 = be_state->builder->CreateOr(r_ne, i_ne);
    }
    *out_val = (expr_llvm_ty->getIntegerBitWidth() > 1)
                   ? be_state->builder->CreateZExt(cmp_i1, expr_llvm_ty)
                   : cmp_i1;
    return llvm_gen_be_error_t::ok;
  }

  bool is_fp = v1->getType()->isFPOrFPVectorTy();
  bool is_ptr = v1->getType()->isPointerTy();
  a_type_ptr raw_op1_ty = skip_typerefs(op1->type);
  #if GNU_VECTOR_TYPES_ALLOWED
  if (raw_op1_ty->kind == tk_vector && raw_op1_ty->variant.vector.element_type) {
    raw_op1_ty = skip_typerefs(raw_op1_ty->variant.vector.element_type);
  }
  #endif
  bool is_unsigned = !is_fp && !is_ptr && raw_op1_ty->kind == tk_integer &&
                     !int_kind_is_signed[raw_op1_ty->variant.integer.int_kind];

  if (expr->variant.operation.kind == eok_spaceship) {
     llvm::Value* res = nullptr;
     llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
     llvm::Value* lt_val = llvm::ConstantInt::getSigned(i8_ty, -1);
     llvm::Value* eq_val = llvm::ConstantInt::getSigned(i8_ty, 0);
     llvm::Value* gt_val = llvm::ConstantInt::getSigned(i8_ty, 1);
     if (is_fp) {
        llvm::Value* cmp_lt = be_state->builder->CreateFCmpOLT(v1, v2);
        llvm::Value* cmp_eq = be_state->builder->CreateFCmpOEQ(v1, v2);
        llvm::Value* cmp_gt = be_state->builder->CreateFCmpOGT(v1, v2);
        llvm::Value* un_val = llvm::ConstantInt::getSigned(i8_ty, -127);
        llvm::Value* sel1 = be_state->builder->CreateSelect(cmp_eq, eq_val, be_state->builder->CreateSelect(cmp_gt, gt_val, un_val));
        res = be_state->builder->CreateSelect(cmp_lt, lt_val, sel1);
     } else {
        llvm::Value* cmp_lt = is_ptr ? be_state->builder->CreateICmpULT(v1, v2) : be_state->builder->CreateICmp(is_unsigned ? llvm::CmpInst::ICMP_ULT : llvm::CmpInst::ICMP_SLT, v1, v2);
        llvm::Value* cmp_eq = be_state->builder->CreateICmpEQ(v1, v2);
        res = be_state->builder->CreateSelect(cmp_lt, lt_val, be_state->builder->CreateSelect(cmp_eq, eq_val, gt_val));
     }

     if (expr_llvm_ty->isStructTy()) {
         *out_val = be_state->builder->CreateInsertValue(llvm::UndefValue::get(expr_llvm_ty), res, 0);
     } else {
         *out_val = be_state->builder->CreateSExtOrTrunc(res, expr_llvm_ty);
     }
     return llvm_gen_be_error_t::ok;
  }

  llvm::CmpInst::Predicate pred;
  switch (expr->variant.operation.kind) {
     case eok_eq:
     case eok_vector_eq:
       pred = is_fp ? llvm::CmpInst::FCMP_OEQ : llvm::CmpInst::ICMP_EQ;
       break;
     case eok_ne:
     case eok_vector_ne:
       pred = is_fp ? llvm::CmpInst::FCMP_UNE : llvm::CmpInst::ICMP_NE;
       break;
     case eok_lt:
     case eok_vector_lt:
       pred = is_fp ? llvm::CmpInst::FCMP_OLT : (is_unsigned || is_ptr ? llvm::CmpInst::ICMP_ULT : llvm::CmpInst::ICMP_SLT);
       break;
     case eok_le:
     case eok_vector_le:
       pred = is_fp ? llvm::CmpInst::FCMP_OLE : (is_unsigned || is_ptr ? llvm::CmpInst::ICMP_ULE : llvm::CmpInst::ICMP_SLE);
       break;
     case eok_gt:
     case eok_vector_gt:
       pred = is_fp ? llvm::CmpInst::FCMP_OGT : (is_unsigned || is_ptr ? llvm::CmpInst::ICMP_UGT : llvm::CmpInst::ICMP_SGT);
       break;
     case eok_ge:
     case eok_vector_ge:
       pred = is_fp ? llvm::CmpInst::FCMP_OGE : (is_unsigned || is_ptr ? llvm::CmpInst::ICMP_UGE : llvm::CmpInst::ICMP_SGE);
       break;
     case eok_not:
     case eok_vector_not:
       pred = is_fp ? llvm::CmpInst::FCMP_OEQ : llvm::CmpInst::ICMP_EQ;
       v2 = llvm::Constant::getNullValue(v1->getType());
       break;
     default: return llvm_gen_be_error_t::unsupported_expr;
  }
  if (is_fp) *out_val = be_state->builder->CreateFCmp(pred, v1, v2);
  else *out_val = be_state->builder->CreateICmp(pred, v1, v2);

  if ((*out_val)->getType()->isVectorTy()) {
     if (expr_llvm_ty->isVectorTy() && expr_llvm_ty != (*out_val)->getType()) {
        *out_val = be_state->builder->CreateSExt(*out_val, expr_llvm_ty);
     }
  } else if (expr_llvm_ty->getIntegerBitWidth() > 1) {
     *out_val = be_state->builder->CreateZExt(*out_val, expr_llvm_ty);
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a type cast expression (`eok_cast`, `eok_Pass_by_value`, etc.) into LLVM IR.
 * @details Handles int-to-ptr, ptr-to-int, fpext/fptrunc, fptosi/fptoui, sitofp/uitofp, sext/zext/trunc, and bitcast.
 * @param[in] expr Pointer to the EDG cast expression node.
 * @param[out] out_val Receives the converted `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_cast_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  an_expr_node_ptr op1 = expr->variant.operation.operands;
  
  llvm::Value* v1 = nullptr;
  llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
  if (err != llvm_gen_be_error_t::ok) return err;
  
  llvm::Type* dest_ty = nullptr;
  err = get_llvm_type(expr->type, &dest_ty);
  if (err != llvm_gen_be_error_t::ok) return err;
  if (dest_ty->isVoidTy()) {
      *out_val = nullptr;
      return llvm_gen_be_error_t::ok;
  }
  llvm::Type* src_ty = v1->getType();
  a_type_ptr raw_src_edg = op1->type ? skip_typerefs(op1->type) : nullptr;
  a_type_ptr raw_dst_edg = skip_typerefs(expr->type);

  unsigned src_dec_bytes = 0;
  unsigned dst_dec_bytes = 0;
  LLVM_GEN_BE_CHECK(llvm_get_decimal_float_byte_width(op1->type, &src_dec_bytes));
  LLVM_GEN_BE_CHECK(llvm_get_decimal_float_byte_width(expr->type, &dst_dec_bytes));
  if (src_dec_bytes != 0 || dst_dec_bytes != 0) {
    a_type_ptr int_edg = (src_dec_bytes == 0) ? raw_src_edg : raw_dst_edg;
    bool int_uns = (int_edg && int_edg->kind == tk_integer)
                       ? !int_kind_is_signed[int_edg->variant.integer.int_kind]
                       : false;
    return llvm_lower_decimal_float_cast(
        src_dec_bytes, dst_dec_bytes, int_uns, v1, dest_ty, out_val);
  }

  if (raw_dst_edg->kind == tk_nullptr) {
    *out_val = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(dest_ty));
    return llvm_gen_be_error_t::ok;
  }
  if (raw_src_edg && raw_src_edg->kind == tk_nullptr) {
    if (raw_dst_edg->kind == tk_ptr_to_member) {
      *out_val = dest_ty->isIntegerTy()
                     ? llvm::ConstantInt::getSigned(dest_ty, -1)
                     : llvm::Constant::getNullValue(dest_ty);
      return llvm_gen_be_error_t::ok;
    }
    if (dest_ty->isPointerTy()) {
      *out_val = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(dest_ty));
      return llvm_gen_be_error_t::ok;
    }
    *out_val = llvm::Constant::getNullValue(dest_ty);
    return llvm_gen_be_error_t::ok;
  }

  if (is_bool_type(raw_dst_edg)) {
    if (raw_src_edg && is_bool_type(raw_src_edg) && src_ty == dest_ty) {
      *out_val = v1;
      return llvm_gen_be_error_t::ok;
    }
    llvm::Value* nz = nullptr;
    if (src_ty->isFloatingPointTy()) {
      nz = be_state->builder->CreateFCmpUNE(v1, llvm::ConstantFP::get(src_ty, 0.0));
    } else if (src_ty->isStructTy()) {
      llvm::Value* r = be_state->builder->CreateExtractValue(v1, 0);
      llvm::Value* im = be_state->builder->CreateExtractValue(v1, 1);
      bool c_fp = r->getType()->isFloatingPointTy();
      llvm::Value* r_nz = c_fp ? be_state->builder->CreateFCmpUNE(r, llvm::ConstantFP::get(r->getType(), 0.0))
                               : be_state->builder->CreateICmpNE(r, llvm::ConstantInt::get(r->getType(), 0));
      llvm::Value* i_nz = c_fp ? be_state->builder->CreateFCmpUNE(im, llvm::ConstantFP::get(im->getType(), 0.0))
                               : be_state->builder->CreateICmpNE(im, llvm::ConstantInt::get(im->getType(), 0));
      nz = be_state->builder->CreateOr(r_nz, i_nz);
    } else {
      nz = be_state->builder->CreateICmpNE(v1, llvm::Constant::getNullValue(src_ty));
    }
    *out_val = dest_ty->isIntegerTy(1) ? nz : be_state->builder->CreateZExt(nz, dest_ty);
    return llvm_gen_be_error_t::ok;
  }

#if FIXED_POINT_ALLOWED
  bool src_is_fx = (raw_src_edg && raw_src_edg->kind == tk_fixed_point);
  bool dst_is_fx = (raw_dst_edg->kind == tk_fixed_point);
  if (src_is_fx || dst_is_fx) {
    unsigned src_f = 0, dst_f = 0;
    bool src_s = true, dst_s = true, src_sat = false, dst_sat = false;
    if (src_is_fx) LLVM_GEN_BE_CHECK(llvm_get_fixed_point_scale(raw_src_edg, &src_f, &src_s, &src_sat));
    if (dst_is_fx) LLVM_GEN_BE_CHECK(llvm_get_fixed_point_scale(raw_dst_edg, &dst_f, &dst_s, &dst_sat));

    if (src_is_fx && dst_is_fx) {
      unsigned common_bw = std::max(src_ty->getIntegerBitWidth(), dest_ty->getIntegerBitWidth());
      llvm::Type* wide_ty = llvm::IntegerType::get(*be_state->context, common_bw);
      llvm::Value* w = be_state->builder->CreateIntCast(v1, wide_ty, src_s);
      if (dst_f > src_f) {
        w = be_state->builder->CreateShl(w, llvm::ConstantInt::get(wide_ty, dst_f - src_f));
      } else if (src_f > dst_f) {
        w = src_s ? be_state->builder->CreateAShr(w, llvm::ConstantInt::get(wide_ty, src_f - dst_f))
                  : be_state->builder->CreateLShr(w, llvm::ConstantInt::get(wide_ty, src_f - dst_f));
      }
      *out_val = be_state->builder->CreateIntCast(w, dest_ty, src_s);
      return llvm_gen_be_error_t::ok;
    }
    if (src_is_fx && dest_ty->isFloatingPointTy()) {
      llvm::Value* fp_v = src_s ? be_state->builder->CreateSIToFP(v1, dest_ty)
                                : be_state->builder->CreateUIToFP(v1, dest_ty);
      llvm::Value* scale_fp = llvm::ConstantFP::get(dest_ty, static_cast<double>(1ULL << src_f));
      *out_val = be_state->builder->CreateFDiv(fp_v, scale_fp, "fx.to.fp");
      return llvm_gen_be_error_t::ok;
    }
    if (dst_is_fx && src_ty->isFloatingPointTy()) {
      llvm::Value* scale_fp = llvm::ConstantFP::get(src_ty, static_cast<double>(1ULL << dst_f));
      llvm::Value* scaled = be_state->builder->CreateFMul(v1, scale_fp, "fp.to.fx");
      *out_val = dst_s ? be_state->builder->CreateFPToSI(scaled, dest_ty)
                       : be_state->builder->CreateFPToUI(scaled, dest_ty);
      return llvm_gen_be_error_t::ok;
    }
    if (src_is_fx) {
      llvm::Value* unscaled = src_s
          ? be_state->builder->CreateAShr(v1, llvm::ConstantInt::get(src_ty, src_f))
          : be_state->builder->CreateLShr(v1, llvm::ConstantInt::get(src_ty, src_f));
      *out_val = be_state->builder->CreateIntCast(unscaled, dest_ty, src_s);
      return llvm_gen_be_error_t::ok;
    }
    bool int_s = int_kind_is_signed[raw_src_edg->variant.integer.int_kind];
    llvm::Value* cast_v = be_state->builder->CreateIntCast(v1, dest_ty, int_s);
    *out_val = be_state->builder->CreateShl(cast_v, llvm::ConstantInt::get(dest_ty, dst_f));
    return llvm_gen_be_error_t::ok;
  }
#endif
  if (src_ty == dest_ty) {
      *out_val = v1;
      return llvm_gen_be_error_t::ok;
  }

  // _Complex conversions ({ elem_ty, elem_ty })
  bool src_is_complex = src_ty->isStructTy();
  bool dest_is_complex = dest_ty->isStructTy();
  if (src_is_complex && dest_is_complex) {
      llvm::Type* dst_elem_ty = dest_ty->getStructElementType(0);
      llvm::Value* r = be_state->builder->CreateExtractValue(v1, 0);
      llvm::Value* i = be_state->builder->CreateExtractValue(v1, 1);
      bool src_elem_fp = r->getType()->isFloatingPointTy();
      bool dst_elem_fp = dst_elem_ty->isFloatingPointTy();
      if (src_elem_fp && dst_elem_fp) {
          r = be_state->builder->CreateFPCast(r, dst_elem_ty);
          i = be_state->builder->CreateFPCast(i, dst_elem_ty);
      } else if (!src_elem_fp && dst_elem_fp) {
          r = be_state->builder->CreateSIToFP(r, dst_elem_ty);
          i = be_state->builder->CreateSIToFP(i, dst_elem_ty);
      } else if (src_elem_fp) {
          r = be_state->builder->CreateFPToSI(r, dst_elem_ty);
          i = be_state->builder->CreateFPToSI(i, dst_elem_ty);
      } else {
          r = be_state->builder->CreateIntCast(r, dst_elem_ty, true);
          i = be_state->builder->CreateIntCast(i, dst_elem_ty, true);
      }
      llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(dest_ty), r, 0);
      *out_val = be_state->builder->CreateInsertValue(res, i, 1);
      return llvm_gen_be_error_t::ok;
  }
  if (dest_is_complex) {
      llvm::Type* dst_elem_ty = dest_ty->getStructElementType(0);
      llvm::Value* r = nullptr;
      if (dst_elem_ty->isFloatingPointTy()) {
          r = src_ty->isFloatingPointTy()
                  ? be_state->builder->CreateFPCast(v1, dst_elem_ty)
                  : be_state->builder->CreateSIToFP(v1, dst_elem_ty);
      } else {
          r = src_ty->isFloatingPointTy()
                  ? be_state->builder->CreateFPToSI(v1, dst_elem_ty)
                  : be_state->builder->CreateIntCast(v1, dst_elem_ty, true);
      }
      llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(dest_ty), r, 0);
      *out_val = be_state->builder->CreateInsertValue(res, llvm::Constant::getNullValue(dst_elem_ty), 1);
      return llvm_gen_be_error_t::ok;
  }
  if (src_is_complex) {
      v1 = be_state->builder->CreateExtractValue(v1, 0);
      src_ty = v1->getType();
      if (src_ty == dest_ty) {
          *out_val = v1;
          return llvm_gen_be_error_t::ok;
      }
  }
  
  if (src_ty->isPointerTy()) {
     if (dest_ty->isPointerTy()) {
        *out_val = be_state->builder->CreateAddrSpaceCast(v1, dest_ty, "addrspace.cast");
        return llvm_gen_be_error_t::ok;
     }
     *out_val = be_state->builder->CreatePtrToInt(v1, dest_ty);
     return llvm_gen_be_error_t::ok;
  }
  if (dest_ty->isPointerTy()) {
     *out_val = be_state->builder->CreateIntToPtr(v1, dest_ty);
     return llvm_gen_be_error_t::ok;
  }
  
  if (src_ty->isFloatingPointTy()) {
     if (dest_ty->isFloatingPointTy()) {
        *out_val = be_state->builder->CreateFPCast(v1, dest_ty);
     } else if (dest_ty->isVectorTy()) {
        if (src_ty->getPrimitiveSizeInBits() == dest_ty->getPrimitiveSizeInBits()) {
           *out_val = be_state->builder->CreateBitCast(v1, dest_ty);
        } else {
           auto* fv = llvm::cast<llvm::FixedVectorType>(dest_ty);
           llvm::Type* ety = fv->getElementType();
           llvm::Value* ev = ety->isFloatingPointTy()
                                 ? be_state->builder->CreateFPCast(v1, ety)
                                 : be_state->builder->CreateFPToSI(v1, ety);
           *out_val = be_state->builder->CreateVectorSplat(fv->getNumElements(), ev);
        }
     } else {
        if (be_state->enable_ubsan_checks) {
           llvm::Value* is_nan = be_state->builder->CreateFCmpUNO(v1, v1, "ubsan.fpcast.nan");
           LLVM_GEN_BE_CHECK(emit_ubsan_check_branch(is_nan, "__ubsan_handle_float_cast_overflow"));
        }
        if (!int_kind_is_signed[skip_typerefs(expr->type)->variant.integer.int_kind]) {
           *out_val = be_state->builder->CreateFPToUI(v1, dest_ty);
        } else {
           *out_val = be_state->builder->CreateFPToSI(v1, dest_ty);
        }
     }
     return llvm_gen_be_error_t::ok;
  }
  
  if (src_ty->isIntegerTy()) {
     bool src_is_unsigned = !int_kind_is_signed[skip_typerefs(op1->type)->variant.integer.int_kind];
     if (dest_ty->isFloatingPointTy()) {
        *out_val = src_is_unsigned ? be_state->builder->CreateUIToFP(v1, dest_ty)
                                   : be_state->builder->CreateSIToFP(v1, dest_ty);
     } else if (dest_ty->isVectorTy()) {
        if (src_ty->getPrimitiveSizeInBits() == dest_ty->getPrimitiveSizeInBits()) {
           *out_val = be_state->builder->CreateBitCast(v1, dest_ty);
        } else {
           auto* fv = llvm::cast<llvm::FixedVectorType>(dest_ty);
           llvm::Type* ety = fv->getElementType();
           llvm::Value* ev = ety->isFloatingPointTy()
                                 ? (src_is_unsigned ? be_state->builder->CreateUIToFP(v1, ety)
                                                    : be_state->builder->CreateSIToFP(v1, ety))
                                 : be_state->builder->CreateIntCast(v1, ety, !src_is_unsigned);
           *out_val = be_state->builder->CreateVectorSplat(fv->getNumElements(), ev);
        }
     } else {
        *out_val = be_state->builder->CreateIntCast(v1, dest_ty, !src_is_unsigned);
     }
     return llvm_gen_be_error_t::ok;
  }
  
  *out_val = be_state->builder->CreateBitCast(v1, dest_ty);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Decodes a C11/C++11 memory model integer constant into an `llvm::AtomicOrdering`.
 * @details Maps `0..5` to `Monotonic`, `Acquire`, `Release`, `AcquireRelease`, or `SequentiallyConsistent`,
 *          falling back to `SequentiallyConsistent` for release/acq_rel on load or failure orderings.
 * @param[in] order_val The lowered LLVM value for the memory order argument.
 * @param[in] is_load_or_fail True if decoding a load or cmpxchg failure ordering.
 * @param[out] out_order Receives the decoded `llvm::AtomicOrdering`.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t decode_atomic_ordering(
    llvm::Value* order_val,
    bool is_load_or_fail,
    llvm::AtomicOrdering* out_order) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  *out_order = llvm::AtomicOrdering::SequentiallyConsistent;
  if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(order_val)) {
    switch (ci->getZExtValue()) {
      case 0: *out_order = llvm::AtomicOrdering::Monotonic; break;
      case 1:
      case 2: *out_order = llvm::AtomicOrdering::Acquire; break;
      case 3:
        if (!is_load_or_fail) *out_order = llvm::AtomicOrdering::Release;
        break;
      case 4:
        if (!is_load_or_fail) *out_order = llvm::AtomicOrdering::AcquireRelease;
        break;
      default: break;
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits a runtime switch over a non-constant `memory_order` value for `__atomic_thread_fence` / `__atomic_signal_fence`.
 * @details Dispatches runtime `order_val` (`1`/`2` = `Acquire`, `3` = `Release`, `4` = `AcquireRelease`, default = `SequentiallyConsistent`)
 *          into dedicated basic blocks with the corresponding `llvm::FenceInst`.
 * @param[in] order_val Runtime integer `memory_order` value.
 * @param[in] scope Synchronization scope (`System` or `SingleThread`).
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t emit_dynamic_atomic_ordering_fence(
    llvm::Value* order_val,
    llvm::SyncScope::ID scope) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  llvm::Function* fn = be_state->builder->GetInsertBlock()->getParent();
  llvm::BasicBlock* acq_bb = llvm::BasicBlock::Create(*be_state->context, "fence.acq", fn);
  llvm::BasicBlock* rel_bb = llvm::BasicBlock::Create(*be_state->context, "fence.rel", fn);
  llvm::BasicBlock* ar_bb = llvm::BasicBlock::Create(*be_state->context, "fence.acqrel", fn);
  llvm::BasicBlock* sc_bb = llvm::BasicBlock::Create(*be_state->context, "fence.seqcst", fn);
  llvm::BasicBlock* done_bb = llvm::BasicBlock::Create(*be_state->context, "fence.done", fn);

  llvm::Type* ord_ty = order_val->getType();
  llvm::SwitchInst* sw = be_state->builder->CreateSwitch(order_val, sc_bb, 4);
  sw->addCase(llvm::cast<llvm::ConstantInt>(llvm::ConstantInt::get(ord_ty, 1)), acq_bb);
  sw->addCase(llvm::cast<llvm::ConstantInt>(llvm::ConstantInt::get(ord_ty, 2)), acq_bb);
  sw->addCase(llvm::cast<llvm::ConstantInt>(llvm::ConstantInt::get(ord_ty, 3)), rel_bb);
  sw->addCase(llvm::cast<llvm::ConstantInt>(llvm::ConstantInt::get(ord_ty, 4)), ar_bb);

  be_state->builder->SetInsertPoint(acq_bb);
  be_state->builder->CreateFence(llvm::AtomicOrdering::Acquire, scope);
  be_state->builder->CreateBr(done_bb);

  be_state->builder->SetInsertPoint(rel_bb);
  be_state->builder->CreateFence(llvm::AtomicOrdering::Release, scope);
  be_state->builder->CreateBr(done_bb);

  be_state->builder->SetInsertPoint(ar_bb);
  be_state->builder->CreateFence(llvm::AtomicOrdering::AcquireRelease, scope);
  be_state->builder->CreateBr(done_bb);

  be_state->builder->SetInsertPoint(sc_bb);
  be_state->builder->CreateFence(llvm::AtomicOrdering::SequentiallyConsistent, scope);
  be_state->builder->CreateBr(done_bb);

  be_state->builder->SetInsertPoint(done_bb);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Resolves true header/source filename, 1-based line, column, and human-readable function name for source-location builtins.
 * @details Uses EDG's `conv_seq_to_file_and_line` on `pos.seq` so `#include`d header call sites report the
 *          actual header path and 1-based line, and resolves an unmangled function name via `be_state->current_edg_routine`
 *          or `llvm::demangle`.
 * @param[in] pos EDG source position of the builtin expression.
 * @param[out] out_file Receives the resolved source/header filename.
 * @param[out] out_line Receives the 1-based line number.
 * @param[out] out_col Receives the 1-based column number.
 * @param[out] out_func Receives the unmangled human-readable function name.
 * @return `llvm_gen_be_error_t::ok` on success.
 */
static llvm_gen_be_error_t resolve_builtin_source_location(
    const a_source_position& pos,
    std::string* out_file,
    uint32_t* out_line,
    uint32_t* out_col,
    std::string* out_func) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  const std::string& mod_fn = be_state->module->getSourceFileName();
  *out_file = !mod_fn.empty() ? mod_fn : "<file>";
  *out_line = pos.seq ? static_cast<uint32_t>(pos.seq) : 1U;
  *out_col = pos.column ? static_cast<uint32_t>(pos.column) : 1U;
  if (pos.seq != 0) {
    a_const_char* short_fn = nullptr;
    a_const_char* full_fn = nullptr;
    a_line_number resolved_line = 0;
    a_boolean at_eos = FALSE;
    conv_seq_to_file_and_line(pos.seq, &short_fn, &full_fn, &resolved_line, &at_eos);
    if (full_fn != nullptr && full_fn[0] != '\0') {
      *out_file = full_fn;
    } else if (short_fn != nullptr && short_fn[0] != '\0') {
      *out_file = short_fn;
    }
    if (resolved_line != 0) {
      *out_line = static_cast<uint32_t>(resolved_line);
    }
  }

  std::string raw_fn;
  if (be_state->current_edg_routine != nullptr &&
      !be_state->current_edg_routine->source_corresp.name_has_been_mangled &&
      be_state->current_edg_routine->source_corresp.name != nullptr &&
      be_state->current_edg_routine->source_corresp.name[0] != '\0') {
    *out_func = be_state->current_edg_routine->source_corresp.name;
    return llvm_gen_be_error_t::ok;
  }
  if (be_state->builder->GetInsertBlock() &&
      be_state->builder->GetInsertBlock()->getParent()) {
    raw_fn = be_state->builder->GetInsertBlock()->getParent()->getName().str();
  }
  if (raw_fn.size() >= 2 && (raw_fn.compare(0, 2, "_Z") == 0 || raw_fn[0] == '?')) {
    std::string demangled = llvm::demangle(raw_fn);
    if (demangled != raw_fn) {
      size_t paren_pos = demangled.find('(');
      if (paren_pos != std::string::npos) {
        demangled.erase(paren_pos);
      }
      *out_func = demangled;
      return llvm_gen_be_error_t::ok;
    }
  }
  *out_func = raw_fn;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Coerces an integer builtin result to match the EDG expression's integer return type.
 * @details Ensures builtins such as `__builtin_clzll`, `__builtin_popcountll`, and `__builtin_constant_p`
 *          emit the exact `i32` (`int`) or target integer width expected by surrounding IR expressions.
 * @param[in] expr_edg_type EDG expression result type (may be null).
 * @param[in] val Raw integer `llvm::Value*` produced by the intrinsic.
 * @param[out] out_val Receives the width-adjusted `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on type lowering failure.
 */
static llvm_gen_be_error_t coerce_builtin_int_return(
    a_type_ptr expr_edg_type,
    llvm::Value* val,
    llvm::Value** out_val) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  *out_val = val;
  if (expr_edg_type && val->getType()->isIntegerTy()) {
    llvm::Type* ret_ty = nullptr;
    LLVM_GEN_BE_CHECK(get_llvm_type(expr_edg_type, &ret_ty));
    if (ret_ty->isIntegerTy() && ret_ty != val->getType()) {
      *out_val = be_state->builder->CreateIntCast(val, ret_ty, false);
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Determines whether an LLVM value type of byte size @p sz is natively lock-free in LLVM atomic IR instructions.
 * @details Returns `true` in @p out_is_native for integer, pointer, and floating-point types whose store size
 *          is a power of two in `[1, 16]`.
 * @param[in] ty Lowered LLVM value type.
 * @param[in] sz Byte store size of @p ty from `DataLayout`.
 * @param[out] out_is_native Receives `true` if native LLVM atomic instructions can operate on @p ty directly.
 * @return `llvm_gen_be_error_t::ok` on success.
 */
static llvm_gen_be_error_t check_native_atomic_value_type(
    llvm::Type* ty,
    uint64_t sz,
    bool* out_is_native) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  *out_is_native = (ty->isIntOrPtrTy() || ty->isFloatingPointTy()) &&
                   llvm::isPowerOf2_64(sz) && sz <= 16;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Resolves the true pointee LLVM type of a pointer argument passed to generic `__atomic_*` builtins.
 * @details Unwraps implicit `(void*)` `eok_cast` and `eok_parens` wrappers inserted by the EDG front end
 *          so `DataLayout::getTypeStoreSize` inspects the concrete underlying object type instead of `void`.
 * @param[in] ptr_arg_node EDG expression node for the pointer argument.
 * @param[out] out_ty Receives the lowered sized pointee `llvm::Type*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t resolve_atomic_pointee_llvm_type(
    an_expr_node_ptr ptr_arg_node,
    llvm::Type** out_ty) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  an_expr_node_ptr cur = ptr_arg_node;
  while (cur && cur->kind == enk_operation &&
         (cur->variant.operation.kind == eok_cast || cur->variant.operation.kind == eok_parens)) {
    a_type_ptr ct = cur->type ? skip_typerefs(cur->type) : nullptr;
    if (ct && ct->kind == tk_pointer && ct->variant.pointer.type &&
        skip_typerefs(ct->variant.pointer.type)->kind != tk_void) {
      break;
    }
    cur = cur->variant.operation.operands;
  }
  if (cur && cur->type) {
    a_type_ptr pt = skip_typerefs(cur->type);
    if (pt->kind == tk_pointer && pt->variant.pointer.type &&
        skip_typerefs(pt->variant.pointer.type)->kind != tk_void) {
      return get_llvm_type(pt->variant.pointer.type, out_ty);
    }
  }
  if (cur && cur->kind == enk_operation && cur->variant.operation.kind == eok_address_of &&
      cur->variant.operation.operands->type) {
    return get_llvm_type(cur->variant.operation.operands->type, out_ty);
  }
  if (ptr_arg_node->type && skip_typerefs(ptr_arg_node->type)->kind == tk_pointer) {
    *out_ty = llvm::Type::getInt8Ty(*be_state->context);
    return llvm_gen_be_error_t::ok;
  }
  return llvm_gen_be_error_t::atomic_lowering_failed;
}

/**
 * @brief Lowers a unary or binary overloaded math/bit-manipulation builtin into an LLVM intrinsic call.
 * @details Matches `rname` against known math and bit-manipulation builtin prefixes and emits the corresponding intrinsic call.
 * @param[in] expr_edg_type EDG result type of the builtin call expression (may be null).
 * @param[in] rname Name of the builtin routine.
 * @param[in] args Lowered LLVM argument values.
 * @param[out] out_val Receives the resulting call instruction value.
 * @param[out] out_handled Receives `true` if the builtin matched and was lowered.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t llvm_lower_overloaded_intrinsic_builtin(
    a_type_ptr expr_edg_type,
    llvm::StringRef rname,
    const std::vector<llvm::Value*>& args,
    llvm::Value** out_val,
    bool* out_handled) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  *out_handled = false;
  if (args.empty()) {
    return llvm_gen_be_error_t::ok;
  }
  if (rname == "__builtin_constant_p") {
    llvm::Function* intr = EDG_LLVM_GET_INTRINSIC_DECL(
        be_state->module.get(), llvm::Intrinsic::is_constant, {args[0]->getType()});
    llvm::Value* i1_res = be_state->builder->CreateCall(intr, {args[0]});
    llvm::Type* ret_ty = llvm::Type::getInt32Ty(*be_state->context);
    if (expr_edg_type) {
      LLVM_GEN_BE_CHECK(get_llvm_type(expr_edg_type, &ret_ty));
    }
    *out_val = ret_ty->isIntegerTy() ? be_state->builder->CreateIntCast(i1_res, ret_ty, false) : i1_res;
    *out_handled = true;
    return llvm_gen_be_error_t::ok;
  }
  if (rname.starts_with("__builtin_bswap") && args[0]->getType()->isIntegerTy()) {
    unsigned target_bits = args[0]->getType()->getIntegerBitWidth();
    if (rname == "__builtin_bswap16") target_bits = 16;
    else if (rname == "__builtin_bswap32") target_bits = 32;
    else if (rname == "__builtin_bswap64") target_bits = 64;
    llvm::Type* bswap_ty = llvm::Type::getIntNTy(*be_state->context, target_bits);
    llvm::Value* in_v = (args[0]->getType() == bswap_ty)
                            ? args[0]
                            : be_state->builder->CreateIntCast(args[0], bswap_ty, false);
    llvm::Function* intr = EDG_LLVM_GET_INTRINSIC_DECL(
        be_state->module.get(), llvm::Intrinsic::bswap, {bswap_ty});
    llvm::Value* swapped = be_state->builder->CreateCall(intr, {in_v});
    LLVM_GEN_BE_CHECK(coerce_builtin_int_return(expr_edg_type, swapped, out_val));
    *out_handled = true;
    return llvm_gen_be_error_t::ok;
  }
  static const std::pair<const char*, llvm::Intrinsic::ID> kUnaryEntries[] = {
    {"__builtin_sqrt", llvm::Intrinsic::sqrt},
    {"__builtin_sin", llvm::Intrinsic::sin},
    {"__builtin_cos", llvm::Intrinsic::cos},
    {"__builtin_exp", llvm::Intrinsic::exp},
    {"__builtin_log", llvm::Intrinsic::log},
    {"__builtin_round", llvm::Intrinsic::round},
    {"__builtin_rint", llvm::Intrinsic::rint},
    {"__builtin_nearbyint", llvm::Intrinsic::nearbyint},
    {"__builtin_trunc", llvm::Intrinsic::trunc},
    {"__builtin_floor", llvm::Intrinsic::floor},
    {"__builtin_ceil", llvm::Intrinsic::ceil},
    {"__builtin_fabs", llvm::Intrinsic::fabs},
    {"__builtin_bswap", llvm::Intrinsic::bswap},
    {"__builtin_bitreverse", llvm::Intrinsic::bitreverse},
    {"__builtin_reduce_or", llvm::Intrinsic::vector_reduce_or},
    {"__builtin_reduce_and", llvm::Intrinsic::vector_reduce_and},
    {"__builtin_reduce_xor", llvm::Intrinsic::vector_reduce_xor},
    {"__builtin_reduce_add", llvm::Intrinsic::vector_reduce_add},
    {"__builtin_reduce_mul", llvm::Intrinsic::vector_reduce_mul},
  };
  for (const auto& entry : kUnaryEntries) {
    if (rname.starts_with(entry.first) && !rname.starts_with("__builtin_expect")) {
      llvm::Function* intr = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), entry.second, {args[0]->getType()});
      *out_val = be_state->builder->CreateCall(intr, {args[0]});
      *out_handled = true;
      return llvm_gen_be_error_t::ok;
    }
  }
  static const std::pair<const char*, llvm::Intrinsic::ID> kBinaryEntries[] = {
    {"__builtin_pow", llvm::Intrinsic::pow},
    {"__builtin_copysign", llvm::Intrinsic::copysign},
    {"__builtin_fmin", llvm::Intrinsic::minnum},
    {"__builtin_fmax", llvm::Intrinsic::maxnum},
  };
  for (const auto& entry : kBinaryEntries) {
    if (rname.starts_with(entry.first)) {
      llvm::Function* intr = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), entry.second, {args[0]->getType()});
      *out_val = be_state->builder->CreateCall(intr, {args[0], args[1]});
      *out_handled = true;
      return llvm_gen_be_error_t::ok;
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a direct, indirect, virtual, or builtin function call expression into LLVM IR.
 * @details Handles target ABI parameter/return lowering (`sret`, `byval`, register coercion),
 *          compiler builtins, C11/C++11 atomics, virtual dispatch, and `invoke` unwinding.
 * @param[in] expr Pointer to the EDG call expression node.
 * @param[out] out_val Receives the call return `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_call_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  an_expr_node_ptr op1 = expr->variant.operation.operands;
  if (!op1) return llvm_gen_be_error_t::invalid_argument;
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  
  llvm::FunctionType* callee_ty = nullptr;
  llvm::AttributeList callee_attrs;
  std::vector<llvm::Type*> call_param_tys;
  bool is_c99_complex_helper = (op1->kind == enk_routine &&
                                llvm::StringRef(op1->variant.routine.ptr->source_corresp.name).starts_with("__c99_"));
  bool is_builtin_callee = (is_c99_complex_helper ||
                            (op1->kind == enk_routine &&
                             (llvm::StringRef(op1->variant.routine.ptr->source_corresp.name).starts_with("__builtin_") ||
                              llvm::StringRef(op1->variant.routine.ptr->source_corresp.name).starts_with("__atomic_") ||
                              llvm::StringRef(op1->variant.routine.ptr->source_corresp.name).starts_with("__c11_atomic_") ||
                              llvm::StringRef(op1->variant.routine.ptr->source_corresp.name).starts_with("__sync_"))));
  bool edg_supplies_sret_param = false;
  if (op1->type && !is_builtin_callee) {
     a_type_ptr func_ty_edg = skip_typerefs(op1->type);
     if (func_ty_edg->kind == tk_pointer) {
       func_ty_edg = skip_typerefs(func_ty_edg->variant.pointer.type);
     }
     if (func_ty_edg->kind == tk_routine) {
         unsigned num_fixed = 0;
         bool is_vararg_rout = false;
         if (func_ty_edg->variant.routine.extra_info != nullptr) {
           is_vararg_rout = (func_ty_edg->variant.routine.extra_info->has_ellipsis != FALSE);
#if DO_IL_LOWERING
           edg_supplies_sret_param = (func_ty_edg->variant.routine.extra_info->value_returned_as_parameter != FALSE);
#endif
           for (a_param_type_ptr p = func_ty_edg->variant.routine.extra_info->param_type_list;
                p != nullptr; p = p->next) {
             ++num_fixed;
           }
         }
         std::vector<a_type_ptr> call_arg_edg_types;
         for (an_expr_node_ptr a = op1->next; a != nullptr; a = a->next) {
           call_arg_edg_types.push_back(a->type);
         }
         LLVM_GEN_BE_CHECK(llvm_build_target_abi_call_schedule(
             func_ty_edg, is_vararg_rout, num_fixed, call_arg_edg_types,
             &callee_ty, &call_param_tys, &callee_attrs));
         if (op1->kind == enk_routine) {
           llvm::Function* existing_fn = be_state->module->getFunction(
               op1->variant.routine.ptr->source_corresp.name);
           if (existing_fn && !is_vararg_rout &&
               existing_fn->getFunctionType()->getNumParams() > callee_ty->getNumParams()) {
             callee_ty = existing_fn->getFunctionType();
             callee_attrs = existing_fn->getAttributes();
             call_param_tys.assign(callee_ty->param_begin(), callee_ty->param_end());
           }
         }
     }
  }

  std::vector<llvm::Value*> args;
  unsigned callee_arg_idx = 0;
  llvm::AllocaInst* sret_alloc = nullptr;
  llvm::Value* direct_sret_dest = nullptr;
  llvm::Type* sret_ty = nullptr;
  
  if (!edg_supplies_sret_param && callee_ty && callee_attrs.hasParamAttr(0, llvm::Attribute::StructRet)) {
      sret_ty = callee_attrs.getParamStructRetType(0);
      if (be_state->active_prvalue_dest_ptr != nullptr &&
          be_state->active_prvalue_dest_ty == sret_ty) {
          direct_sret_dest = be_state->active_prvalue_dest_ptr;
          be_state->active_prvalue_dest_ptr = nullptr;
          be_state->active_prvalue_dest_ty = nullptr;
          args.push_back(direct_sret_dest);
      } else {
          sret_alloc = be_state->builder->CreateAlloca(sret_ty, nullptr, "sret");
          args.push_back(sret_alloc);
      }
      callee_arg_idx++;
  }

  llvm::Value* this_val = nullptr;

  for (an_expr_node_ptr arg = op1->next; arg != nullptr; arg = arg->next) {
    llvm::Value* arg_val = nullptr;
    llvm_gen_be_error_t err = llvm_lower_expression(arg, &arg_val);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    if ((!edg_supplies_sret_param && arg == op1->next) ||
        (edg_supplies_sret_param && arg == op1->next->next)) {
      this_val = arg_val; // Save 'this' pointer for virtual dispatch
    }
    
    if (callee_ty && callee_arg_idx < call_param_tys.size()) {
        llvm::Type* expected_ty = call_param_tys[callee_arg_idx];
        a_type_ptr raw_arg_edg = arg->type ? skip_typerefs(arg->type) : nullptr;
        bool arg_is_agg = raw_arg_edg &&
                          (is_class_struct_union_type(raw_arg_edg) ||
                           raw_arg_edg->kind == tk_complex ||
                           raw_arg_edg->kind == tk_array ||
                           (raw_arg_edg->kind == tk_integer && raw_arg_edg->size > 16));
        if (callee_attrs.hasParamAttr(callee_arg_idx, llvm::Attribute::ByVal) ||
            (expected_ty->isPointerTy() && !arg_val->getType()->isPointerTy() && arg_is_agg)) {
            llvm::AllocaInst* alloc = be_state->builder->CreateAlloca(arg_val->getType());
            be_state->builder->CreateStore(arg_val, alloc);
            args.push_back(alloc);
            callee_arg_idx++;
            continue;
        } else if (arg_val->getType() != expected_ty) {
            if (arg_val->getType()->isIntegerTy() && expected_ty->isIntegerTy() && !arg_is_agg) {
                bool is_s = (raw_arg_edg && raw_arg_edg->kind == tk_integer && !raw_arg_edg->variant.integer.bool_type)
                                ? (int_kind_is_signed[raw_arg_edg->variant.integer.int_kind] != FALSE)
                                : false;
                args.push_back(be_state->builder->CreateIntCast(arg_val, expected_ty, is_s));
                callee_arg_idx++;
                continue;
            }
            if (arg_val->getType()->isFloatingPointTy() && expected_ty->isFloatingPointTy()) {
                args.push_back(be_state->builder->CreateFPCast(arg_val, expected_ty));
                callee_arg_idx++;
                continue;
            }
            const llvm::DataLayout& dl = be_state->module->getDataLayout();
            uint64_t packed_bytes = dl.getTypeAllocSize(expected_ty);
            if (packed_bytes == 0) {
                args.push_back(llvm::Constant::getNullValue(expected_ty));
                callee_arg_idx++;
                continue;
            }
            uint64_t total_arg_bytes = dl.getTypeAllocSize(arg_val->getType());
            llvm::Type* slot_ty = (packed_bytes > total_arg_bytes) ? expected_ty : arg_val->getType();
            llvm::AllocaInst* alloc = be_state->builder->CreateAlloca(slot_ty);
            if (packed_bytes > total_arg_bytes) {
                be_state->builder->CreateStore(llvm::Constant::getNullValue(slot_ty), alloc);
            }
            be_state->builder->CreateStore(arg_val, alloc);
            llvm::Value* first_piece = be_state->builder->CreateLoad(expected_ty, alloc);
            args.push_back(first_piece);
            callee_arg_idx++;

            while (packed_bytes < total_arg_bytes && callee_arg_idx < call_param_tys.size()) {
                llvm::Type* piece_ty = call_param_tys[callee_arg_idx++];
                llvm::Value* piece_ptr = be_state->builder->CreateInBoundsGEP(
                    llvm::Type::getInt8Ty(*be_state->context), alloc,
                    llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), packed_bytes), "call.arg.piece");
                args.push_back(be_state->builder->CreateLoad(piece_ty, piece_ptr));
                packed_bytes += dl.getTypeAllocSize(piece_ty);
            }
            continue;
        }
    }
    
    args.push_back(arg_val);
    callee_arg_idx++;
  }

  llvm::Value* v1 = nullptr;
  if (expr->variant.operation.is_virtual_call) {
      a_routine_ptr r = op1->variant.routine.ptr;
      llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
      llvm_gen_be_error_t v_err;
      if (triple.isWindowsMSVCEnvironment()) {
          v_err = llvm_lower_msvc_virtual_dispatch(this_val, r, &v1);
      } else {
          v_err = llvm_lower_virtual_dispatch(this_val, r, &v1);
      }
      if (v_err != llvm_gen_be_error_t::ok) return v_err;
  } else {
      if (is_c99_complex_helper) {
          return llvm_lower_c99_complex_runtime_call(
              expr->type, op1->variant.routine.ptr->source_corresp.name, args, out_val);
      }
      if (op1->kind == enk_routine &&
          op1->variant.routine.ptr->function_def_number != NULL_function_def_number) {
        be_state->demand_defined_routines.push_back(op1->variant.routine.ptr);
      }
      if (op1->kind == enk_routine && callee_ty &&
          !be_state->module->getFunction(op1->variant.routine.ptr->source_corresp.name)) {
          llvm::Function::Create(
              callee_ty, llvm::GlobalValue::ExternalLinkage,
              op1->variant.routine.ptr->source_corresp.name, be_state->module.get());
      }
      llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
      if (err != llvm_gen_be_error_t::ok) return err;
      
      // Check for builtins in call expressions
      if (op1->kind == enk_routine) {
             a_routine_ptr r = op1->variant.routine.ptr;
             std::string rname = r->source_corresp.name;
             bool ov_handled = false;
             LLVM_GEN_BE_CHECK(llvm_lower_overloaded_intrinsic_builtin(expr->type, rname, args, out_val, &ov_handled));
             if (ov_handled) return llvm_gen_be_error_t::ok;
             if (rname == "__builtin_expect") {
                 llvm::Type* exp_ty = args[0]->getType()->isIntegerTy()
                                          ? args[0]->getType()
                                          : llvm::Type::getInt64Ty(*be_state->context);
                 llvm::Value* a0 = args[0]->getType()->isPointerTy()
                                       ? be_state->builder->CreatePtrToInt(args[0], exp_ty)
                                       : args[0];
                 llvm::Value* a1 = args[1]->getType()->isPointerTy()
                                       ? be_state->builder->CreatePtrToInt(args[1], exp_ty)
                                       : be_state->builder->CreateIntCast(args[1], exp_ty, true);
                 llvm::Function* exp = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::expect, {exp_ty});
                 llvm::Value* exp_res = be_state->builder->CreateCall(exp, {a0, a1});
                 return coerce_builtin_int_return(expr->type, exp_res, out_val);
             } else if (rname == "__builtin_expect_with_probability") {
                 llvm::Function* exp_prob = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::expect_with_probability, {args[0]->getType()});
                 *out_val = be_state->builder->CreateCall(exp_prob, {args[0], args[1], args[2]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_frame_address") {
                 llvm::Function* intr = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::frameaddress, {llvm::PointerType::getUnqual(*be_state->context)});
                 *out_val = be_state->builder->CreateCall(intr, {args[0]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_return_address") {
                 llvm::Function* intr = EDG_LLVM_GET_RETADDR_INTRINSIC_DECL(be_state->module.get());
                 *out_val = be_state->builder->CreateCall(intr, {args[0]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_alloca") {
                 *out_val = be_state->builder->CreateAlloca(llvm::Type::getInt8Ty(*be_state->context), args[0]);
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_alloca_with_align") {
                 llvm::AllocaInst* alloc = be_state->builder->CreateAlloca(llvm::Type::getInt8Ty(*be_state->context), args[0]);
                 if (auto* align_ci = llvm::dyn_cast<llvm::ConstantInt>(args[1])) {
                     alloc->setAlignment(llvm::Align(align_ci->getZExtValue() / 8));
                 }
                 *out_val = alloc;
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_shufflevector") {
                 std::vector<int> mask;
                 for (size_t i = 2; i < args.size(); ++i) {
                     if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(args[i])) {
                         mask.push_back(static_cast<int>(ci->getSExtValue()));
                     } else {
                         mask.push_back(-1);
                     }
                 }
                 *out_val = be_state->builder->CreateShuffleVector(args[0], args[1], mask);
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_convertvector") {
                 llvm::Type* dst_ty = nullptr;
                 llvm_gen_be_error_t err_ty = get_llvm_type(expr->type, &dst_ty);
                 if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
                 
                 llvm::Type* src_ty = args[0]->getType();
                 if (src_ty->isIntOrIntVectorTy() && dst_ty->isIntOrIntVectorTy()) {
                     *out_val = be_state->builder->CreateIntCast(args[0], dst_ty, true);
                 } else if (src_ty->isFPOrFPVectorTy() && dst_ty->isFPOrFPVectorTy()) {
                     *out_val = be_state->builder->CreateFPCast(args[0], dst_ty);
                 } else if (src_ty->isIntOrIntVectorTy()) {
                     *out_val = be_state->builder->CreateSIToFP(args[0], dst_ty);
                 } else {
                     *out_val = be_state->builder->CreateFPToSI(args[0], dst_ty);
                 }
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_assume") {
                 llvm::Function* assume = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::assume);
                 llvm::Value* cond = args[0]->getType()->isIntegerTy(1)
                                         ? args[0]
                                         : be_state->builder->CreateICmpNE(
                                               args[0], llvm::Constant::getNullValue(args[0]->getType()));
                 *out_val = be_state->builder->CreateCall(assume, {cond});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_unreachable") {
                 if (be_state->enable_ubsan_checks) {
                     LLVM_GEN_BE_CHECK(emit_ubsan_check_branch(
                         be_state->builder->getTrue(), "__ubsan_handle_builtin_unreachable"));
                 }
                 llvm::Function* cur_fn = be_state->builder->GetInsertBlock()->getParent();
                 *out_val = be_state->builder->CreateUnreachable();
                 be_state->builder->SetInsertPoint(
                     llvm::BasicBlock::Create(*be_state->context, "unreachable.after", cur_fn));
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_trap") {
                 llvm::Function* trap = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::trap);
                 *out_val = be_state->builder->CreateCall(trap);
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).ends_with("setjmp")) {
                 llvm::Function* setjmp_func = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::eh_sjlj_setjmp);
                 *out_val = be_state->builder->CreateCall(setjmp_func, {args[0]});
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).ends_with("longjmp")) {
                 llvm::Function* longjmp_func = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::eh_sjlj_longjmp);
                 *out_val = be_state->builder->CreateCall(longjmp_func, {args[0]});
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_cabs")) {
                 llvm::Value* v_cplx = args[0];
                 if (v_cplx->getType()->isStructTy()) {
                     llvm::Value* r = be_state->builder->CreateExtractValue(v_cplx, 0);
                     llvm::Value* i = be_state->builder->CreateExtractValue(v_cplx, 1);
                     llvm::Value* r2 = be_state->builder->CreateFMul(r, r);
                     llvm::Value* i2 = be_state->builder->CreateFMul(i, i);
                     llvm::Value* sum = be_state->builder->CreateFAdd(r2, i2);
                     llvm::Function* sqrt_func = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::sqrt, {r->getType()});
                     *out_val = be_state->builder->CreateCall(sqrt_func, {sum});
                 } else {
                     llvm::Function* fabs_func = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::fabs, {v_cplx->getType()});
                     *out_val = be_state->builder->CreateCall(fabs_func, {v_cplx});
                 }
                 return llvm_gen_be_error_t::ok;
             }
             bool target_handled = false;
#if BUILTIN_FUNCTIONS_ENABLED
             a_builtin_function_kind bfk = static_cast<a_builtin_function_kind>(r->variant.builtin_function_kind);
#else
             a_builtin_function_kind bfk = 0;
#endif
             LLVM_GEN_BE_CHECK(llvm_lower_target_builtin(
                 expr, bfk, rname, args, callee_ty, out_val, &target_handled));
             if (target_handled) {
                 return llvm_gen_be_error_t::ok;
             }
             if (llvm::StringRef(rname).starts_with("__builtin_") &&
                        llvm::StringRef(rname).ends_with("_overflow")) {
                 llvm::Type* op_ty = args[0]->getType();
                 a_type_ptr first_edg_ty = skip_typerefs(op1->next->type);
                 bool generic_signed = int_kind_is_signed[first_edg_ty->variant.integer.int_kind];
                 bool is_signed = (rname.find("sadd") != std::string::npos) || 
                                  (rname.find("ssub") != std::string::npos) || 
                                  (rname.find("smul") != std::string::npos) || 
                                  ((rname == "__builtin_add_overflow" || rname == "__builtin_sub_overflow" || rname == "__builtin_mul_overflow") && generic_signed);
                                  
                 llvm::Intrinsic::ID iid;
                 if (rname.find("add") != std::string::npos) iid = is_signed ? llvm::Intrinsic::sadd_with_overflow : llvm::Intrinsic::uadd_with_overflow;
                 else if (rname.find("sub") != std::string::npos) iid = is_signed ? llvm::Intrinsic::ssub_with_overflow : llvm::Intrinsic::usub_with_overflow;
                 else iid = is_signed ? llvm::Intrinsic::smul_with_overflow : llvm::Intrinsic::umul_with_overflow;

                 llvm::Function* ofunc = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), iid, {op_ty});
                 llvm::Value* res = be_state->builder->CreateCall(ofunc, {args[0], args[1]});
                 
                 // Store result
                 llvm::Value* val = be_state->builder->CreateExtractValue(res, 0);
                 be_state->builder->CreateStore(val, args[2]);
                 
                 // Return overflow flag coerced to expression return type (i8 for bool)
                 llvm::Value* ovf_flag = be_state->builder->CreateExtractValue(res, 1);
                 llvm::Type* ovf_ret_ty = llvm::Type::getInt8Ty(*be_state->context);
                 if (expr->type) LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &ovf_ret_ty));
                 *out_val = ovf_ret_ty->isIntegerTy(1) ? ovf_flag : be_state->builder->CreateZExt(ovf_flag, ovf_ret_ty);
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_") &&
                        llvm::StringRef(rname).ends_with("_overflow_p")) {
                 llvm::Type* op_ty = args[0]->getType();
                 llvm::Intrinsic::ID iid = (rname.find("add") != std::string::npos)
                                               ? llvm::Intrinsic::sadd_with_overflow
                                               : ((rname.find("sub") != std::string::npos)
                                                      ? llvm::Intrinsic::ssub_with_overflow
                                                      : llvm::Intrinsic::smul_with_overflow);
                 llvm::Function* ofunc = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), iid, {op_ty});
                 llvm::Value* res = be_state->builder->CreateCall(ofunc, {args[0], args[1]});
                 llvm::Value* ovf_flag = be_state->builder->CreateExtractValue(res, 1);
                 llvm::Type* ovf_ret_ty = llvm::Type::getInt8Ty(*be_state->context);
                 if (expr->type) LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &ovf_ret_ty));
                 *out_val = ovf_ret_ty->isIntegerTy(1) ? ovf_flag : be_state->builder->CreateZExt(ovf_flag, ovf_ret_ty);
                 return llvm_gen_be_error_t::ok;
             } else if ((llvm::StringRef(rname).starts_with("__builtin_addc") ||
                         llvm::StringRef(rname).starts_with("__builtin_subc")) &&
                        args.size() >= 4) {
                 llvm::Type* op_ty = args[0]->getType();
                 llvm::Intrinsic::ID iid = llvm::StringRef(rname).starts_with("__builtin_addc")
                                               ? llvm::Intrinsic::uadd_with_overflow
                                               : llvm::Intrinsic::usub_with_overflow;
                 llvm::Function* ofunc = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), iid, {op_ty});
                 llvm::Value* step1 = be_state->builder->CreateCall(ofunc, {args[0], args[1]});
                 llvm::Value* mid = be_state->builder->CreateExtractValue(step1, 0);
                 llvm::Value* ov1 = be_state->builder->CreateZExt(
                     be_state->builder->CreateExtractValue(step1, 1), op_ty);
                 llvm::Value* step2 = be_state->builder->CreateCall(ofunc, {mid, args[2]});
                 llvm::Value* res_val = be_state->builder->CreateExtractValue(step2, 0);
                 llvm::Value* ov2 = be_state->builder->CreateZExt(
                     be_state->builder->CreateExtractValue(step2, 1), op_ty);
                 be_state->builder->CreateStore(be_state->builder->CreateAdd(ov1, ov2), args[3]);
                 *out_val = res_val;
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_popcount") ||
                        llvm::StringRef(rname).starts_with("__builtin_stdc_count_ones")) {
                 llvm::Function* pop = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::ctpop, {args[0]->getType()});
                 llvm::Value* pop_res = be_state->builder->CreateCall(pop, {args[0]});
                 return coerce_builtin_int_return(expr->type, pop_res, out_val);
             } else if (llvm::StringRef(rname).starts_with("__builtin_stdc_has_single_bit")) {
                 llvm::Type* arg_ty = args[0]->getType();
                 llvm::Function* pop = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::ctpop, {arg_ty});
                 llvm::Value* cnt = be_state->builder->CreateCall(pop, {args[0]});
                 llvm::Value* single = be_state->builder->CreateICmpEQ(cnt, llvm::ConstantInt::get(arg_ty, 1));
                 llvm::Type* sb_ret_ty = llvm::Type::getInt8Ty(*be_state->context);
                 if (expr->type) LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &sb_ret_ty));
                 *out_val = sb_ret_ty->isIntegerTy(1) ? single : be_state->builder->CreateZExt(single, sb_ret_ty);
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_stdc_bit_floor")) {
                 llvm::Type* arg_ty = args[0]->getType();
                 unsigned bw = arg_ty->getIntegerBitWidth();
                 llvm::Function* clz = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::ctlz, {arg_ty});
                 llvm::Value* lz = be_state->builder->CreateCall(clz, {args[0], be_state->builder->getFalse()});
                 llvm::Value* shift = be_state->builder->CreateSub(llvm::ConstantInt::get(arg_ty, bw - 1), lz);
                 llvm::Value* pow2 = be_state->builder->CreateShl(llvm::ConstantInt::get(arg_ty, 1), shift);
                 llvm::Value* is_zero = be_state->builder->CreateICmpEQ(args[0], llvm::ConstantInt::get(arg_ty, 0));
                 *out_val = be_state->builder->CreateSelect(is_zero, llvm::ConstantInt::get(arg_ty, 0), pow2);
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_stdc_bit_ceil")) {
                 llvm::Type* arg_ty = args[0]->getType();
                 unsigned bw = arg_ty->getIntegerBitWidth();
                 llvm::Value* sub1 = be_state->builder->CreateSub(args[0], llvm::ConstantInt::get(arg_ty, 1));
                 llvm::Function* clz = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::ctlz, {arg_ty});
                 llvm::Value* lz = be_state->builder->CreateCall(clz, {sub1, be_state->builder->getFalse()});
                 llvm::Value* shift = be_state->builder->CreateSub(llvm::ConstantInt::get(arg_ty, bw), lz);
                 llvm::Value* pow2 = be_state->builder->CreateShl(llvm::ConstantInt::get(arg_ty, 1), shift);
                 llvm::Value* le1 = be_state->builder->CreateICmpULE(args[0], llvm::ConstantInt::get(arg_ty, 1));
                 *out_val = be_state->builder->CreateSelect(le1, llvm::ConstantInt::get(arg_ty, 1), pow2);
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_stdc_count_zeros")) {
                 llvm::Value* inv = be_state->builder->CreateNot(args[0]);
                 llvm::Function* pop = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::ctpop, {args[0]->getType()});
                 *out_val = be_state->builder->CreateCall(pop, {inv});
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_stdc_bit_width")) {
                 llvm::Type* arg_ty = args[0]->getType();
                 llvm::Function* clz = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::ctlz, {arg_ty});
                 llvm::Value* lz = be_state->builder->CreateCall(clz, {args[0], be_state->builder->getFalse()});
                 *out_val = be_state->builder->CreateSub(
                     llvm::ConstantInt::get(arg_ty, arg_ty->getIntegerBitWidth()), lz);
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_stdc_first_leading_one") ||
                        llvm::StringRef(rname).starts_with("__builtin_stdc_first_leading_zero")) {
                 llvm::Type* arg_ty = args[0]->getType();
                 llvm::Value* in_v = llvm::StringRef(rname).ends_with("zero")
                                         ? be_state->builder->CreateNot(args[0])
                                         : args[0];
                 llvm::Function* clz = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::ctlz, {arg_ty});
                 llvm::Value* lz = be_state->builder->CreateCall(clz, {in_v, be_state->builder->getFalse()});
                 llvm::Value* is_zero = be_state->builder->CreateICmpEQ(in_v, llvm::ConstantInt::get(arg_ty, 0));
                 llvm::Value* idx1 = be_state->builder->CreateAdd(lz, llvm::ConstantInt::get(arg_ty, 1));
                 *out_val = be_state->builder->CreateSelect(is_zero, llvm::ConstantInt::get(arg_ty, 0), idx1);
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_stdc_first_trailing_one") ||
                        llvm::StringRef(rname).starts_with("__builtin_stdc_first_trailing_zero")) {
                 llvm::Type* arg_ty = args[0]->getType();
                 llvm::Value* in_v = llvm::StringRef(rname).ends_with("zero")
                                         ? be_state->builder->CreateNot(args[0])
                                         : args[0];
                 llvm::Function* ctz = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::cttz, {arg_ty});
                 llvm::Value* tz = be_state->builder->CreateCall(ctz, {in_v, be_state->builder->getFalse()});
                 llvm::Value* is_zero = be_state->builder->CreateICmpEQ(in_v, llvm::ConstantInt::get(arg_ty, 0));
                 llvm::Value* idx1 = be_state->builder->CreateAdd(tz, llvm::ConstantInt::get(arg_ty, 1));
                 *out_val = be_state->builder->CreateSelect(is_zero, llvm::ConstantInt::get(arg_ty, 0), idx1);
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_clear_padding") {
                 llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
                 a_type_ptr ptee_ty = op1->next->type->variant.pointer.type;
                 std::vector<uint8_t> pad_mask;
                 LLVM_GEN_BE_CHECK(llvm_compute_type_padding_mask(ptee_ty, &pad_mask));
                 for (size_t b = 0; b < pad_mask.size(); ++b) {
                     uint8_t m = pad_mask[b];
                     if (m == 0xFFu) continue;
                     llvm::Value* byte_ptr = be_state->builder->CreateInBoundsGEP(
                         i8_ty, args[0], llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), b));
                     if (m == 0x00u) {
                         be_state->builder->CreateStore(llvm::ConstantInt::get(i8_ty, 0), byte_ptr);
                     } else {
                         llvm::Value* old_b = be_state->builder->CreateLoad(i8_ty, byte_ptr);
                         llvm::Value* masked_b = be_state->builder->CreateAnd(old_b, llvm::ConstantInt::get(i8_ty, m));
                         be_state->builder->CreateStore(masked_b, byte_ptr);
                     }
                 }
                 be_state->builder->CreateMemSet(
                     args[0], llvm::ConstantInt::get(i8_ty, 0),
                     llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), 0), llvm::MaybeAlign());
                 *out_val = args[0];
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_FILE" || rname == "__builtin_FUNCTION" ||
                        rname == "__builtin_LINE" || rname == "__builtin_COLUMN") {
                 std::string sl_file;
                 std::string sl_func;
                 uint32_t sl_line = 1;
                 uint32_t sl_col = 1;
#if EXTRA_SOURCE_POSITIONS_IN_IL
                 const a_source_position& sl_pos =
                     (expr->position.seq != 0) ? expr->position : expr->expr_range.start;
#else /* !EXTRA_SOURCE_POSITIONS_IN_IL */
                 const a_source_position& sl_pos = expr->position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
                 LLVM_GEN_BE_CHECK(resolve_builtin_source_location(
                     sl_pos, &sl_file, &sl_line, &sl_col, &sl_func));
                 if (rname == "__builtin_FILE") {
                     *out_val = be_state->builder->CreateGlobalString(sl_file, ".file");
                 } else if (rname == "__builtin_FUNCTION") {
                     *out_val = be_state->builder->CreateGlobalString(sl_func, ".func");
                 } else if (rname == "__builtin_LINE") {
                     *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), sl_line);
                 } else {
                     *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), sl_col);
                 }
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_clz")) {
                 llvm::Function* clz = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::ctlz, {args[0]->getType()});
                 llvm::Value* lz = be_state->builder->CreateCall(clz, {args[0], be_state->builder->getTrue()});
                 return coerce_builtin_int_return(expr->type, lz, out_val);
             } else if (llvm::StringRef(rname).starts_with("__builtin_ctz")) {
                 llvm::Function* ctz = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::cttz, {args[0]->getType()});
                 llvm::Value* tz = be_state->builder->CreateCall(ctz, {args[0], be_state->builder->getTrue()});
                 return coerce_builtin_int_return(expr->type, tz, out_val);
             } else if (llvm::StringRef(rname).starts_with("__builtin_ffs")) {
                 llvm::Type* arg_ty = args[0]->getType();
                 llvm::Function* ctz = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::cttz, {arg_ty});
                 llvm::Value* tz = be_state->builder->CreateCall(ctz, {args[0], be_state->builder->getFalse()});
                 llvm::Value* tz_plus_1 = be_state->builder->CreateAdd(tz, llvm::ConstantInt::get(arg_ty, 1));
                 llvm::Value* is_zero = be_state->builder->CreateICmpEQ(args[0], llvm::ConstantInt::get(arg_ty, 0));
                 llvm::Value* ffs_res = be_state->builder->CreateSelect(is_zero, llvm::ConstantInt::get(arg_ty, 0), tz_plus_1);
                 return coerce_builtin_int_return(expr->type, ffs_res, out_val);
             } else if (llvm::StringRef(rname).starts_with("__builtin_clrsb")) {
                 llvm::Type* arg_ty = args[0]->getType();
                 unsigned bw = arg_ty->getIntegerBitWidth();
                 llvm::Value* sign_mask = be_state->builder->CreateAShr(args[0], llvm::ConstantInt::get(arg_ty, bw - 1));
                 llvm::Value* xored = be_state->builder->CreateXor(args[0], sign_mask);
                 llvm::Function* clz = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::ctlz, {arg_ty});
                 llvm::Value* lz = be_state->builder->CreateCall(clz, {xored, be_state->builder->getFalse()});
                 llvm::Value* clrsb_res = be_state->builder->CreateSub(lz, llvm::ConstantInt::get(arg_ty, 1));
                 return coerce_builtin_int_return(expr->type, clrsb_res, out_val);
             } else if (rname == "__builtin_launder") {
                 llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                 llvm::Function* launder = EDG_LLVM_GET_INTRINSIC_DECL(
                     be_state->module.get(), llvm::Intrinsic::launder_invariant_group, {ptr_ty});
                 *out_val = be_state->builder->CreateCall(launder, {args[0]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_coro_resume" && !args.empty()) {
                 llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(
                     be_state->module.get(), llvm::Intrinsic::coro_resume);
                 *out_val = be_state->builder->CreateCall(fn, {args[0]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_coro_destroy" && !args.empty()) {
                 llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(
                     be_state->module.get(), llvm::Intrinsic::coro_destroy);
                 *out_val = be_state->builder->CreateCall(fn, {args[0]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_coro_done" && !args.empty()) {
                 llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(
                     be_state->module.get(), llvm::Intrinsic::coro_done);
                 llvm::Value* done_i1 = be_state->builder->CreateCall(fn, {args[0]});
                 return coerce_builtin_int_return(expr->type, done_i1, out_val);
             } else if (rname == "__builtin_coro_promise" && args.size() >= 3) {
                 llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(
                     be_state->module.get(), llvm::Intrinsic::coro_promise);
                 llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
                 llvm::Value* al = be_state->builder->CreateIntCast(args[1], i32_ty, false);
                 llvm::Value* from = args[2]->getType()->isIntegerTy(1)
                                         ? args[2]
                                         : be_state->builder->CreateICmpNE(
                                               args[2], llvm::Constant::getNullValue(args[2]->getType()));
                 *out_val = be_state->builder->CreateCall(fn, {args[0], al, from});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_coro_noop") {
                 return llvm_lower_coro_noop(out_val);
             } else if (rname == "__builtin_bit_cast" && !args.empty()) {
                 llvm::Type* dst_ty = args.back()->getType();
                 if (expr->type) {
                     LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &dst_ty));
                 }
                 llvm::Value* src_v = args.back();
                 llvm::AllocaInst* tmp_slot = be_state->builder->CreateAlloca(dst_ty, nullptr, "bitcast.slot");
                 be_state->builder->CreateStore(src_v, tmp_slot);
                 *out_val = be_state->builder->CreateLoad(dst_ty, tmp_slot);
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_dump_struct") {
                 llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
                 llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                 llvm::FunctionType* printf_fty = llvm::FunctionType::get(i32_ty, {ptr_ty}, true);
                 llvm::Constant* fmt_str = be_state->builder->CreateGlobalString("struct {}\n", ".dump_fmt");
                 *out_val = be_state->builder->CreateCall(printf_fty, args[1], {fmt_str});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_assume_aligned") {
                 llvm::Function* assume_fn = EDG_LLVM_GET_INTRINSIC_DECL(
                     be_state->module.get(), llvm::Intrinsic::assume);
                 llvm::Value* cond = be_state->builder->getTrue();
                 std::vector<llvm::Value*> bundle_args = {args[0], args[1]};
                 if (args.size() > 2) {
                     bundle_args.push_back(args[2]);
                 }
                 llvm::OperandBundleDef align_bundle{"align", bundle_args};
                 be_state->builder->CreateCall(assume_fn, {cond}, {align_bundle});
                 *out_val = args[0];
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_choose_expr") {
                 bool cond_true = false;
                 if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(args[0])) {
                     cond_true = !ci->isZero();
                 }
                 *out_val = cond_true ? args[1] : args[2];
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_va_arg_pack" || rname == "__builtin_va_arg_pack_len" ||
                        rname == "__builtin_is_constant_evaluated") {
                 llvm::Type* ret_ty = llvm::Type::getInt32Ty(*be_state->context);
                 if (expr->type) {
                     LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &ret_ty));
                 }
                 *out_val = llvm::ConstantInt::get(ret_ty, 0);
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_nondeterministic_value") {
                 *out_val = be_state->builder->CreateFreeze(llvm::PoisonValue::get(args[0]->getType()));
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_addressof") {
                 return llvm_lower_lvalue_expression(op1->next, out_val);
             } else if (rname == "__builtin_complex") {
                 llvm::Type* cplx_ty = llvm::StructType::get(*be_state->context, {args[0]->getType(), args[1]->getType()});
                 llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(cplx_ty), args[0], 0);
                 *out_val = be_state->builder->CreateInsertValue(res, args[1], 1);
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_source_location") {
                 llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                 llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
                 llvm::StructType* sl_ty = llvm::StructType::get(*be_state->context, {ptr_ty, ptr_ty, i32_ty, i32_ty});
                 std::string sl_file;
                 std::string sl_func;
                 uint32_t sl_line = 1;
                 uint32_t sl_col = 1;
#if EXTRA_SOURCE_POSITIONS_IN_IL
                 const a_source_position& sl_pos =
                     (expr->position.seq != 0) ? expr->position : expr->expr_range.start;
#else /* !EXTRA_SOURCE_POSITIONS_IN_IL */
                 const a_source_position& sl_pos = expr->position;
#endif /* EXTRA_SOURCE_POSITIONS_IN_IL */
                 LLVM_GEN_BE_CHECK(resolve_builtin_source_location(
                     sl_pos, &sl_file, &sl_line, &sl_col, &sl_func));
                 llvm::Constant* fn_str = be_state->builder->CreateGlobalString(sl_file, ".file");
                 llvm::Constant* func_str = be_state->builder->CreateGlobalString(sl_func, ".func");
                 llvm::Constant* sl_init = llvm::ConstantStruct::get(
                     sl_ty,
                     {fn_str, func_str,
                      llvm::ConstantInt::get(i32_ty, sl_line),
                      llvm::ConstantInt::get(i32_ty, sl_col)});
                 llvm::GlobalVariable* sl_gv = nullptr;
                 LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), sl_ty, true, llvm::GlobalValue::PrivateLinkage, sl_init, ".src_loc", &sl_gv));
                 *out_val = sl_gv;
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_classify_type") {
                 an_expr_node_ptr arg_node = op1->next;
                 a_type_ptr target_ty = (arg_node && arg_node->kind == enk_type_operand)
                                            ? arg_node->variant.type_operand.type
                                            : (arg_node ? arg_node->type : nullptr);
                 int32_t cls_code = -1;
                 if (target_ty) {
                     a_type_ptr ut = skip_typerefs(target_ty);
                     llvm::Type* lt = nullptr;
                     llvm_gen_be_error_t l_err = get_llvm_type(ut, &lt);
                     if (l_err == llvm_gen_be_error_t::ok && lt->isVectorTy()) cls_code = 19;
                     else if (ut->kind == tk_void) cls_code = 0;
                     else if (ut->kind == tk_integer) {
                         if (ut->variant.integer.bool_type) cls_code = 4;
                         else if (ut->variant.integer.enum_type) cls_code = 3;
                         else if (ut->variant.integer.int_kind == ik_char ||
                                  ut->variant.integer.int_kind == ik_signed_char ||
                                  ut->variant.integer.int_kind == ik_unsigned_char) cls_code = 2;
                         else cls_code = 1;
                     } else if (ut->kind == tk_pointer) cls_code = ut->variant.pointer.is_reference ? 6 : 5;
                     else if (ut->kind == tk_ptr_to_member) {
                         a_type_ptr mty = ut->variant.ptr_to_member.type ? skip_typerefs(ut->variant.ptr_to_member.type) : nullptr;
                         cls_code = (mty && mty->kind == tk_routine) ? 11 : 7;
                     } else if (ut->kind == tk_float || ut->kind == tk_imaginary) cls_code = 8;
                     else if (ut->kind == tk_complex) cls_code = 9;
                     else if (ut->kind == tk_routine) cls_code = 10;
                     else if (ut->kind == tk_struct || ut->kind == tk_class) cls_code = 12;
                     else if (ut->kind == tk_union) cls_code = 13;
                     else if (ut->kind == tk_array) cls_code = 14;
                 }
                 *out_val = llvm::ConstantInt::getSigned(llvm::Type::getInt32Ty(*be_state->context), cls_code);
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__uuidof") {
                 an_expr_node_ptr arg_node = op1->next;
                 a_type_ptr target_ty = (arg_node && arg_node->kind == enk_type_operand)
                                            ? arg_node->variant.type_operand.type
                                            : (arg_node ? arg_node->type : nullptr);
                 a_constant uuid_con = {};
                 uuid_con.kind = ck_address;
                 uuid_con.type = expr->type;
                 uuid_con.variant.address.kind = abk_uuidof;
                 uuid_con.variant.address.variant.type = target_ty;
                 llvm::Constant* guid_gv = nullptr;
                 LLVM_GEN_BE_CHECK(evaluate_constant(
                     &uuid_con, llvm::PointerType::getUnqual(*be_state->context), &guid_gv));
                 *out_val = guid_gv;
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_prefetch") {
                 llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
                 llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                 llvm::Value* rw = (args.size() > 1) ? be_state->builder->CreateIntCast(args[1], i32_ty, false) : llvm::ConstantInt::get(i32_ty, 0);
                 llvm::Value* loc = (args.size() > 2) ? be_state->builder->CreateIntCast(args[2], i32_ty, false) : llvm::ConstantInt::get(i32_ty, 3);
                 llvm::Function* pref = EDG_LLVM_GET_INTRINSIC_DECL(
                     be_state->module.get(), llvm::Intrinsic::prefetch, {ptr_ty});
                 *out_val = be_state->builder->CreateCall(pref, {args[0], rw, loc, llvm::ConstantInt::get(i32_ty, 1)});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_clear_cache") {
#if LLVM_VERSION_MAJOR >= 19
                 llvm::Type* ptr_ty = args[0]->getType();
                 llvm::Function* cc = EDG_LLVM_GET_INTRINSIC_DECL(
                     be_state->module.get(), llvm::Intrinsic::clear_cache, {ptr_ty});
#else
                 llvm::Function* cc = EDG_LLVM_GET_INTRINSIC_DECL(
                     be_state->module.get(), llvm::Intrinsic::clear_cache);
#endif
                 *out_val = be_state->builder->CreateCall(cc, {args[0], args[1]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_readcyclecounter") {
                 llvm::Function* rcc = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::readcyclecounter);
                 *out_val = be_state->builder->CreateCall(rcc);
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_isnan")) {
                 llvm::Value* is_unord = be_state->builder->CreateFCmpUNO(args[0], args[0]);
                 *out_val = be_state->builder->CreateZExt(is_unord, llvm::Type::getInt32Ty(*be_state->context));
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_isunordered") {
                 llvm::Value* is_unord = be_state->builder->CreateFCmpUNO(args[0], args[1]);
                 *out_val = be_state->builder->CreateZExt(is_unord, llvm::Type::getInt32Ty(*be_state->context));
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_huge_val") ||
                        llvm::StringRef(rname).starts_with("__builtin_inf")) {
                 llvm::Type* fp_ret_ty = llvm::Type::getDoubleTy(*be_state->context);
                 if (expr->type) LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &fp_ret_ty));
                 *out_val = llvm::ConstantFP::getInfinity(fp_ret_ty, false);
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_nan")) {
                 llvm::Type* fp_ret_ty = llvm::Type::getDoubleTy(*be_state->context);
                 if (expr->type) LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &fp_ret_ty));
                 *out_val = llvm::ConstantFP::getNaN(fp_ret_ty, false);
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__yield") {
                 llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
                 llvm::Function* hint_fn = EDG_LLVM_GET_INTRINSIC_DECL(
                     be_state->module.get(), EDG_LLVM_LOOKUP_INTRINSIC_ID("llvm.aarch64.hint"));
                 *out_val = be_state->builder->CreateCall(hint_fn, {llvm::ConstantInt::get(i32_ty, 1)});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_memcmp" || rname == "__builtin_strncmp") {
                 const char* libc_name = (rname == "__builtin_memcmp") ? "memcmp" : "strncmp";
                 llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
                 llvm::FunctionType* ft = llvm::FunctionType::get(
                     i32_ty, {args[0]->getType(), args[1]->getType(), args[2]->getType()}, false);
                 llvm::FunctionCallee fn = be_state->module->getOrInsertFunction(libc_name, ft);
                 *out_val = be_state->builder->CreateCall(fn, {args[0], args[1], args[2]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_strcpy") {
                 llvm::FunctionType* ft = llvm::FunctionType::get(
                     args[0]->getType(), {args[0]->getType(), args[1]->getType()}, false);
                 llvm::FunctionCallee fn = be_state->module->getOrInsertFunction("strcpy", ft);
                 *out_val = be_state->builder->CreateCall(fn, {args[0], args[1]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_strncpy") {
                 llvm::FunctionType* ft = llvm::FunctionType::get(
                     args[0]->getType(), {args[0]->getType(), args[1]->getType(), args[2]->getType()}, false);
                 llvm::FunctionCallee fn = be_state->module->getOrInsertFunction("strncpy", ft);
                 *out_val = be_state->builder->CreateCall(fn, {args[0], args[1], args[2]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_isinf" || rname == "__builtin_isfinite" || rname == "__builtin_isnormal") {
                 unsigned mask = (rname == "__builtin_isinf") ? 516u : ((rname == "__builtin_isfinite") ? 504u : 264u);
                 llvm::Function* fpcls = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::is_fpclass, {args[0]->getType()});
                 llvm::Value* res = be_state->builder->CreateCall(fpcls, {args[0], llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), mask)});
                 *out_val = be_state->builder->CreateZExt(res, llvm::Type::getInt32Ty(*be_state->context));
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_signbit")) {
                 unsigned bw = static_cast<unsigned>(args[0]->getType()->getPrimitiveSizeInBits());
                 llvm::Type* int_ty = llvm::Type::getIntNTy(*be_state->context, bw);
                 llvm::Value* bits = be_state->builder->CreateBitCast(args[0], int_ty);
                 llvm::Value* is_neg = be_state->builder->CreateICmpSLT(bits, llvm::ConstantInt::get(int_ty, 0));
                 *out_val = be_state->builder->CreateZExt(is_neg, llvm::Type::getInt32Ty(*be_state->context));
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_fpclassify") {
                 // __builtin_fpclassify(fp_nan, fp_infinite, fp_normal, fp_subnormal, fp_zero, x)
                 llvm::Value* x = args[5];
                 llvm::Value* zero = llvm::ConstantFP::get(x->getType(), 0.0);
                 llvm::Value* is_nan = be_state->builder->CreateFCmpUNO(x, x);
                 llvm::Value* is_zero = be_state->builder->CreateFCmpOEQ(x, zero);
                 llvm::Function* fpcls = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::is_fpclass, {x->getType()});
                 llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
                 llvm::Value* is_inf = be_state->builder->CreateCall(fpcls, {x, llvm::ConstantInt::get(i32_ty, 516u)});
                 llvm::Value* is_norm = be_state->builder->CreateCall(fpcls, {x, llvm::ConstantInt::get(i32_ty, 264u)});
                 llvm::Value* sel = be_state->builder->CreateSelect(is_norm, args[2], args[3]);
                 sel = be_state->builder->CreateSelect(is_zero, args[4], sel);
                 sel = be_state->builder->CreateSelect(is_inf, args[1], sel);
                 *out_val = be_state->builder->CreateSelect(is_nan, args[0], sel);
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_parity")) {
                 llvm::Function* popcnt = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::ctpop, {args[0]->getType()});
                 llvm::Value* pop = be_state->builder->CreateCall(popcnt, {args[0]});
                 llvm::Value* par = be_state->builder->CreateAnd(pop, llvm::ConstantInt::get(args[0]->getType(), 1));
                 return coerce_builtin_int_return(expr->type, par, out_val);
             } else if (rname.find("__builtin_rotateleft") == 0 ||
                        rname.find("__builtin_stdc_rotate_left") == 0) {
                 llvm::Function* rotl = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::fshl, {args[0]->getType()});
                 *out_val = be_state->builder->CreateCall(rotl, {args[0], args[0], args[1]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname.find("__builtin_rotateright") == 0 ||
                        rname.find("__builtin_stdc_rotate_right") == 0) {
                 llvm::Function* rotr = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::fshr, {args[0]->getType()});
                 *out_val = be_state->builder->CreateCall(rotr, {args[0], args[0], args[1]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_object_size" || rname == "__builtin_dynamic_object_size") {
                 llvm::Function* objsize = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::objectsize, {llvm::Type::getInt64Ty(*be_state->context), args[0]->getType()});
                 // LLVM objectsize takes 4 args: ptr, min, nullunknown, dynamic
                 llvm::Value* min_val = be_state->builder->CreateAnd(args[1], llvm::ConstantInt::get(args[1]->getType(), 2));
                 llvm::Value* is_min = be_state->builder->CreateICmpNE(min_val, llvm::ConstantInt::get(args[1]->getType(), 0));
                 llvm::Value* dynamic = (rname == "__builtin_dynamic_object_size") ? be_state->builder->getTrue() : be_state->builder->getFalse();
                 *out_val = be_state->builder->CreateCall(objsize, {args[0], is_min, be_state->builder->getFalse(), dynamic});
                 return llvm_gen_be_error_t::ok;
             } else if (llvm::StringRef(rname).starts_with("__builtin_fma")) {
                 llvm::Function* intr = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), llvm::Intrinsic::fma, {args[0]->getType()});
                 *out_val = be_state->builder->CreateCall(intr, {args[0], args[1], args[2]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin___emutls_get_address") {
                 llvm::Function* emutls_fn = be_state->module->getFunction("__emutls_get_address");
                 if (!emutls_fn) {
                     llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                     llvm::FunctionType* fn_ty = llvm::FunctionType::get(ptr_ty, {ptr_ty}, false);
                     emutls_fn = llvm::Function::Create(fn_ty, llvm::GlobalValue::ExternalLinkage, "__emutls_get_address", be_state->module.get());
                 }
                 *out_val = be_state->builder->CreateCall(emutls_fn, {args[0]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_memcpy" || rname == "__builtin___memcpy_chk") {
                 be_state->builder->CreateMemCpy(args[0], llvm::MaybeAlign(), args[1], llvm::MaybeAlign(), args[2]);
                 *out_val = args[0]; // Returns dest
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_memmove" || rname == "__builtin___memmove_chk") {
                 be_state->builder->CreateMemMove(args[0], llvm::MaybeAlign(), args[1], llvm::MaybeAlign(), args[2]);
                 *out_val = args[0]; // Returns dest
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_strlen") {
                 llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getInt64Ty(*be_state->context), {args[0]->getType()}, false);
                 llvm::FunctionCallee fn = be_state->module->getOrInsertFunction("strlen", ft);
                 *out_val = be_state->builder->CreateCall(fn, {args[0]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_strcmp") {
                 llvm::FunctionType* ft = llvm::FunctionType::get(llvm::Type::getInt32Ty(*be_state->context), {args[0]->getType(), args[1]->getType()}, false);
                 llvm::FunctionCallee fn = be_state->module->getOrInsertFunction("strcmp", ft);
                 *out_val = be_state->builder->CreateCall(fn, {args[0], args[1]});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_strchr") {
                 llvm::FunctionType* ft = llvm::FunctionType::get(llvm::PointerType::getUnqual(*be_state->context), {args[0]->getType(), llvm::Type::getInt32Ty(*be_state->context)}, false);
                 llvm::FunctionCallee fn = be_state->module->getOrInsertFunction("strchr", ft);
                 llvm::Value* arg1 = args[1];
                 if (arg1->getType()->getIntegerBitWidth() < 32) arg1 = be_state->builder->CreateSExt(arg1, llvm::Type::getInt32Ty(*be_state->context));
                 else if (arg1->getType()->getIntegerBitWidth() > 32) arg1 = be_state->builder->CreateTrunc(arg1, llvm::Type::getInt32Ty(*be_state->context));
                 *out_val = be_state->builder->CreateCall(fn, {args[0], arg1});
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_memset" || rname == "__builtin___memset_chk") {
                 llvm::Value* val_i8 = be_state->builder->CreateTrunc(args[1], llvm::Type::getInt8Ty(*be_state->context));
                 be_state->builder->CreateMemSet(args[0], val_i8, args[2], llvm::MaybeAlign());
                 *out_val = args[0]; // Returns dest
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__va_start") {
                 return lower_win64_va_start(args[0], out_val);
             } else if (llvm::StringRef(rname).starts_with("__builtin___") &&
                        llvm::StringRef(rname).ends_with("_chk")) {
                 std::string chk_name = rname.substr(10);
                 llvm::Type* ret_ty = nullptr;
                 LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &ret_ty));
                 size_t fixed_cnt = args.size();
                 bool is_vararg_chk = false;
                 if (chk_name == "__snprintf_chk" && args.size() >= 5) {
                     fixed_cnt = 5;
                     is_vararg_chk = true;
                 } else if (chk_name == "__sprintf_chk" && args.size() >= 4) {
                     fixed_cnt = 4;
                     is_vararg_chk = true;
                 } else if (chk_name == "__fprintf_chk" && args.size() >= 3) {
                     fixed_cnt = 3;
                     is_vararg_chk = true;
                 } else if (chk_name == "__printf_chk" && args.size() >= 2) {
                     fixed_cnt = 2;
                     is_vararg_chk = true;
                 }
                 std::vector<llvm::Type*> param_tys;
                 for (size_t idx = 0; idx < fixed_cnt; ++idx) {
                     param_tys.push_back(args[idx]->getType());
                 }
                 llvm::FunctionType* chk_fty = llvm::FunctionType::get(ret_ty, param_tys, is_vararg_chk);
                 llvm::FunctionCallee fn = be_state->module->getOrInsertFunction(chk_name, chk_fty);
                 *out_val = be_state->builder->CreateCall(fn, args);
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__atomic_always_lock_free" || rname == "__atomic_is_lock_free") {
                 llvm::Type* lf_ret_ty = llvm::Type::getInt8Ty(*be_state->context);
                 if (expr->type) {
                     LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &lf_ret_ty));
                 }
                 llvm::Value* size_val = args[0];
                 if (auto* ci = llvm::dyn_cast<llvm::ConstantInt>(size_val)) {
                     uint64_t size = ci->getZExtValue();
                     bool lock_free = (size > 0 && size <= 8 && llvm::isPowerOf2_64(size));
                     if (size == 16) {
                         llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
                         if (triple.getArch() == llvm::Triple::x86_64 || triple.getArch() == llvm::Triple::aarch64) lock_free = true;
                     }
                     *out_val = llvm::ConstantInt::get(lf_ret_ty, lock_free ? 1 : 0);
                 } else {
                     *out_val = llvm::ConstantInt::get(lf_ret_ty, 0); // conservative fallback
                 }
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__c11_atomic_init" && args.size() >= 2) {
                 be_state->builder->CreateStore(args[1], args[0]);
                 *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0);
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__atomic_thread_fence" || rname == "__atomic_signal_fence" ||
                        rname == "__c11_atomic_thread_fence" || rname == "__c11_atomic_signal_fence") {
                 llvm::SyncScope::ID scope = (rname.find("signal_fence") != std::string::npos) ? llvm::SyncScope::SingleThread : llvm::SyncScope::System;
                 if (llvm::isa<llvm::ConstantInt>(args[0])) {
                     llvm::AtomicOrdering order = llvm::AtomicOrdering::SequentiallyConsistent;
                     LLVM_GEN_BE_CHECK(decode_atomic_ordering(args[0], false, &order));
                     be_state->builder->CreateFence(order, scope);
                 } else {
                     LLVM_GEN_BE_CHECK(emit_dynamic_atomic_ordering_fence(args[0], scope));
                 }
                 *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0); // void/ignored
                 return llvm_gen_be_error_t::ok;
             } else if (rname == "__builtin_atomic_wait" || rname == "__builtin_atomic_notify") {
                 std::string fn_name = (rname == "__builtin_atomic_wait") ? "__atomic_wait" : "__atomic_notify";
                 llvm::Function* wait_notify_fn = be_state->module->getFunction(fn_name);
                 if (!wait_notify_fn) {
                     std::vector<llvm::Type*> arg_types;
                     for (auto* arg_val : args) arg_types.push_back(arg_val->getType());
                     llvm::FunctionType* fn_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), arg_types, false);
                     wait_notify_fn = llvm::Function::Create(fn_ty, llvm::GlobalValue::ExternalLinkage, fn_name, be_state->module.get());
                 }
                 *out_val = be_state->builder->CreateCall(wait_notify_fn, args);
                 return llvm_gen_be_error_t::ok;
             } else if (rname.find("__c11_atomic_fetch_") == 0 ||
                        rname.find("__scoped_atomic_fetch_") == 0 ||
                        (rname.find("__atomic_") == 0 && rname.find("_fetch") != std::string::npos) ||
                        rname.find("__sync_") == 0) {
                 if (rname == "__sync_synchronize") {
                     be_state->builder->CreateFence(llvm::AtomicOrdering::SequentiallyConsistent);
                     *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0);
                     return llvm_gen_be_error_t::ok;
                 }
                 if (rname == "__sync_lock_test_and_set") {
                     llvm::AtomicRMWInst* rmw = be_state->builder->CreateAtomicRMW(llvm::AtomicRMWInst::Xchg, args[0], args[1], llvm::MaybeAlign(), llvm::AtomicOrdering::SequentiallyConsistent);
                     *out_val = rmw;
                     return llvm_gen_be_error_t::ok;
                 }
                 if (rname == "__sync_lock_release") {
                     llvm::Type* val_ty = llvm::Type::getInt32Ty(*be_state->context);
                     llvm::StoreInst* store = be_state->builder->CreateStore(llvm::ConstantInt::get(val_ty, 0), args[0]);
                     store->setAtomic(llvm::AtomicOrdering::Release);
                     *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0);
                     return llvm_gen_be_error_t::ok;
                 }
                 if (rname == "__sync_val_compare_and_swap" || rname == "__sync_bool_compare_and_swap") {
                     llvm::AtomicCmpXchgInst* cmpxchg = be_state->builder->CreateAtomicCmpXchg(args[0], args[1], args[2], llvm::MaybeAlign(), llvm::AtomicOrdering::SequentiallyConsistent, llvm::AtomicOrdering::SequentiallyConsistent);
                     if (rname == "__sync_val_compare_and_swap") {
                         *out_val = be_state->builder->CreateExtractValue(cmpxchg, 0);
                     } else {
                         *out_val = be_state->builder->CreateExtractValue(cmpxchg, 1);
                     }
                     return llvm_gen_be_error_t::ok;
                 }
                 llvm::Value* fetch_rhs = args[1];
                 if (!fetch_rhs->getType()->isFloatingPointTy() && expr->type) {
                     llvm::Type* expr_ret_ty = nullptr;
                     LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &expr_ret_ty));
                     if (expr_ret_ty->isFloatingPointTy()) {
                         an_expr_node_ptr rhs_node = op1->next->next;
                         if (rhs_node->kind == enk_operation &&
                             rhs_node->variant.operation.kind == eok_cast) {
                             LLVM_GEN_BE_CHECK(llvm_lower_expression(
                                 rhs_node->variant.operation.operands, &fetch_rhs));
                             if (fetch_rhs->getType() != expr_ret_ty && fetch_rhs->getType()->isFloatingPointTy()) {
                                 fetch_rhs = be_state->builder->CreateFPCast(fetch_rhs, expr_ret_ty);
                             }
                         } else {
                             fetch_rhs = be_state->builder->CreateSIToFP(fetch_rhs, expr_ret_ty);
                         }
                     }
                 }
                 bool is_fp = fetch_rhs->getType()->isFloatingPointTy();
                 a_type_ptr ptr_arg_edg = op1->next->type ? skip_typerefs(op1->next->type) : nullptr;
                 a_type_ptr ptee_edg = (ptr_arg_edg && ptr_arg_edg->kind == tk_pointer && ptr_arg_edg->variant.pointer.type)
                                           ? skip_typerefs(ptr_arg_edg->variant.pointer.type)
                                           : nullptr;
                 bool target_is_ptr = (ptee_edg && ptee_edg->kind == tk_pointer) ||
                                      (expr->type && skip_typerefs(expr->type)->kind == tk_pointer);

                 llvm::AtomicOrdering order = llvm::AtomicOrdering::SequentiallyConsistent;
                 if (rname.find("__sync_") != 0 && args.size() >= 3) {
                     LLVM_GEN_BE_CHECK(decode_atomic_ordering(args[2], false, &order));
                 }

                 if (target_is_ptr && (rname.find("add") != std::string::npos || rname.find("sub") != std::string::npos)) {
                     const llvm::DataLayout& dl = be_state->module->getDataLayout();
                     llvm::Type* intptr_ty = dl.getIntPtrType(*be_state->context);
                     llvm::Type* res_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                     if (expr->type) LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &res_ptr_ty));
                     llvm::Value* delta = args[1]->getType()->isPointerTy()
                                              ? be_state->builder->CreatePtrToInt(args[1], intptr_ty)
                                              : be_state->builder->CreateIntCast(args[1], intptr_ty, true);
                     if (rname.find("__c11_atomic_") == 0 && ptee_edg && ptee_edg->kind == tk_pointer) {
                         a_type_ptr inner_ptee = ptee_edg->variant.pointer.type
                                                     ? skip_typerefs(ptee_edg->variant.pointer.type)
                                                     : nullptr;
                         uint64_t elem_sz = (inner_ptee && inner_ptee->size > 0)
                                                ? static_cast<uint64_t>(inner_ptee->size)
                                                : 1ULL;
                         if (elem_sz > 1ULL) {
                             delta = be_state->builder->CreateMul(delta, llvm::ConstantInt::get(intptr_ty, elem_sz));
                         }
                     }
                     llvm::AtomicRMWInst::BinOp ptr_op =
                         (rname.find("sub") != std::string::npos) ? llvm::AtomicRMWInst::Sub : llvm::AtomicRMWInst::Add;
                     llvm::AtomicRMWInst* old_int = be_state->builder->CreateAtomicRMW(
                         ptr_op, args[0], delta, llvm::MaybeAlign(), order);
                     bool ptr_returns_new = llvm::StringRef(rname).ends_with("_and_fetch") ||
                                            (rname.find("__atomic_") == 0 && rname.find("atomic_fetch_") == std::string::npos);
                     if (ptr_returns_new) {
                         llvm::Value* new_int = (ptr_op == llvm::AtomicRMWInst::Sub)
                                                    ? be_state->builder->CreateSub(old_int, delta)
                                                    : be_state->builder->CreateAdd(old_int, delta);
                         *out_val = be_state->builder->CreateIntToPtr(new_int, res_ptr_ty);
                     } else {
                         *out_val = be_state->builder->CreateIntToPtr(old_int, res_ptr_ty);
                     }
                     return llvm_gen_be_error_t::ok;
                 }

                 llvm::AtomicRMWInst::BinOp op = is_fp ? llvm::AtomicRMWInst::FAdd : llvm::AtomicRMWInst::Add;
                 if (rname.find("_sub") != std::string::npos) op = is_fp ? llvm::AtomicRMWInst::FSub : llvm::AtomicRMWInst::Sub;
                 else if (rname.find("_nand") != std::string::npos) op = llvm::AtomicRMWInst::Nand;
                 else if (rname.find("_xor") != std::string::npos) op = llvm::AtomicRMWInst::Xor;
                 else if (rname.find("_or") != std::string::npos) op = llvm::AtomicRMWInst::Or;
                 else if (rname.find("_min") != std::string::npos) op = is_fp ? llvm::AtomicRMWInst::FMin : llvm::AtomicRMWInst::Min;
                 else if (rname.find("_max") != std::string::npos) op = is_fp ? llvm::AtomicRMWInst::FMax : llvm::AtomicRMWInst::Max;
                 else if (rname.find("_add") == std::string::npos) op = llvm::AtomicRMWInst::And;
                 
                 llvm::AtomicRMWInst* rmw = be_state->builder->CreateAtomicRMW(op, args[0], fetch_rhs, llvm::MaybeAlign(), order);

                 bool returns_new = llvm::StringRef(rname).ends_with("_and_fetch") ||
                                    (rname.find("__atomic_") == 0 && rname.find("atomic_fetch_") == std::string::npos);
                 if (returns_new) {
                     if (op == llvm::AtomicRMWInst::Add) *out_val = be_state->builder->CreateAdd(rmw, fetch_rhs);
                     else if (op == llvm::AtomicRMWInst::FAdd) *out_val = be_state->builder->CreateFAdd(rmw, fetch_rhs);
                     else if (op == llvm::AtomicRMWInst::Sub) *out_val = be_state->builder->CreateSub(rmw, fetch_rhs);
                     else if (op == llvm::AtomicRMWInst::FSub) *out_val = be_state->builder->CreateFSub(rmw, fetch_rhs);
                     else if (op == llvm::AtomicRMWInst::And) *out_val = be_state->builder->CreateAnd(rmw, fetch_rhs);
                     else if (op == llvm::AtomicRMWInst::Or) *out_val = be_state->builder->CreateOr(rmw, fetch_rhs);
                     else if (op == llvm::AtomicRMWInst::Xor) *out_val = be_state->builder->CreateXor(rmw, fetch_rhs);
                     else {
                         llvm::Value* and_val = be_state->builder->CreateAnd(rmw, fetch_rhs);
                         *out_val = be_state->builder->CreateNot(and_val);
                     }
                 } else {
                     *out_val = rmw;
                 }
                 return llvm_gen_be_error_t::ok;
             } else if (rname.find("atomic_load") != std::string::npos) {
                 bool has_sz_arg = args[0]->getType()->isIntegerTy();
                 if (args.size() < (has_sz_arg ? 3U : 2U)) {
                     return llvm_gen_be_error_t::atomic_lowering_failed;
                 }
                 llvm::Value* ptr_arg = has_sz_arg ? args[1] : args[0];
                 llvm::Value* ret_ptr_arg = has_sz_arg ? args[2] : args[1];
                 an_expr_node_ptr ret_ptr_node = has_sz_arg
                                                     ? op1->next->next->next
                                                     : op1->next->next;
                 llvm::AtomicOrdering order = llvm::AtomicOrdering::SequentiallyConsistent;
                 LLVM_GEN_BE_CHECK(decode_atomic_ordering(args.back(), true, &order));

                 llvm::Type* loaded_ty = nullptr;
                 llvm_gen_be_error_t err = get_llvm_type(expr->type, &loaded_ty);
                 if (err != llvm_gen_be_error_t::ok) return err;

                 if (loaded_ty->isVoidTy()) {
                     // __atomic_load([size,] ptr, ret_ptr, order)
                     err = resolve_atomic_pointee_llvm_type(ret_ptr_node, &loaded_ty);
                     if (err != llvm_gen_be_error_t::ok) return err;
                     uint64_t sz = be_state->module->getDataLayout().getTypeStoreSize(loaded_ty);
                     bool is_nat = false;
                     LLVM_GEN_BE_CHECK(check_native_atomic_value_type(loaded_ty, sz, &is_nat));
                     if (!is_nat && !(llvm::isPowerOf2_64(sz) && sz <= 8)) {
                         llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
                         llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
                         llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                         llvm::FunctionType* lib_ft = llvm::FunctionType::get(
                             llvm::Type::getVoidTy(*be_state->context), {i64_ty, ptr_ty, ptr_ty, i32_ty}, false);
                         llvm::FunctionCallee lib_fn = be_state->module->getOrInsertFunction("__atomic_load", lib_ft);
                         *out_val = be_state->builder->CreateCall(
                             lib_fn, {llvm::ConstantInt::get(i64_ty, sz), ptr_arg, ret_ptr_arg,
                                      be_state->builder->CreateIntCast(args.back(), i32_ty, false)});
                     } else {
                         llvm::Type* atom_ty = is_nat
                                                   ? loaded_ty
                                                   : llvm::Type::getIntNTy(*be_state->context, static_cast<unsigned>(sz * 8));
                         llvm::LoadInst* load = be_state->builder->CreateLoad(atom_ty, ptr_arg);
                         load->setAtomic(order);
                         load->setAlignment(llvm::Align(sz));
                         be_state->builder->CreateStore(load, ret_ptr_arg);
                         *out_val = load;
                     }
                 } else {
                     uint64_t sz = be_state->module->getDataLayout().getTypeStoreSize(loaded_ty);
                     bool is_nat = false;
                     LLVM_GEN_BE_CHECK(check_native_atomic_value_type(loaded_ty, sz, &is_nat));
                     if (!is_nat) {
                         llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
                         llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
                         llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                         llvm::AllocaInst* ret_tmp = be_state->builder->CreateAlloca(loaded_ty, nullptr, "atomic.load.tmp");
                         llvm::FunctionType* lib_ft = llvm::FunctionType::get(
                             llvm::Type::getVoidTy(*be_state->context), {i64_ty, ptr_ty, ptr_ty, i32_ty}, false);
                         llvm::FunctionCallee lib_fn = be_state->module->getOrInsertFunction("__atomic_load", lib_ft);
                         be_state->builder->CreateCall(
                             lib_fn, {llvm::ConstantInt::get(i64_ty, sz), ptr_arg, ret_tmp,
                                      be_state->builder->CreateIntCast(args.back(), i32_ty, false)});
                         *out_val = be_state->builder->CreateLoad(loaded_ty, ret_tmp);
                     } else {
                         llvm::LoadInst* load = be_state->builder->CreateLoad(loaded_ty, ptr_arg);
                         load->setAtomic(order);
                         load->setAlignment(llvm::Align(sz));
                         *out_val = load;
                     }
                 }
                 return llvm_gen_be_error_t::ok;
             } else if (rname.find("atomic_store") != std::string::npos) {
                 bool has_sz_arg = args[0]->getType()->isIntegerTy();
                 if (args.size() < (has_sz_arg ? 3U : 2U)) {
                     return llvm_gen_be_error_t::atomic_lowering_failed;
                 }
                 llvm::Value* ptr_arg = has_sz_arg ? args[1] : args[0];
                 llvm::Value* val_arg = has_sz_arg ? args[2] : args[1];
                 an_expr_node_ptr val_ptr_node = has_sz_arg
                                                     ? op1->next->next->next
                                                     : op1->next->next;
                 llvm::AtomicOrdering order = llvm::AtomicOrdering::SequentiallyConsistent;
                 LLVM_GEN_BE_CHECK(decode_atomic_ordering(args.back(), false, &order));

                 if (rname != "__atomic_store") {
                     llvm::Type* val_ty = val_arg->getType();
                     uint64_t sz = be_state->module->getDataLayout().getTypeStoreSize(val_ty);
                     bool is_nat = false;
                     LLVM_GEN_BE_CHECK(check_native_atomic_value_type(val_ty, sz, &is_nat));
                     if (!is_nat) {
                         llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
                         llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
                         llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                         llvm::AllocaInst* val_tmp = be_state->builder->CreateAlloca(val_ty, nullptr, "atomic.store.tmp");
                         be_state->builder->CreateStore(val_arg, val_tmp);
                         llvm::FunctionType* lib_ft = llvm::FunctionType::get(
                             llvm::Type::getVoidTy(*be_state->context), {i64_ty, ptr_ty, ptr_ty, i32_ty}, false);
                         llvm::FunctionCallee lib_fn = be_state->module->getOrInsertFunction("__atomic_store", lib_ft);
                         be_state->builder->CreateCall(
                             lib_fn, {llvm::ConstantInt::get(i64_ty, sz), ptr_arg, val_tmp,
                                      be_state->builder->CreateIntCast(args.back(), i32_ty, false)});
                     } else {
                         llvm::StoreInst* store = be_state->builder->CreateStore(val_arg, ptr_arg);
                         store->setAtomic(order);
                         store->setAlignment(llvm::Align(sz));
                     }
                 } else {
                     // __atomic_store([size,] ptr, val_ptr, order)
                     llvm::Type* val_ty = nullptr;
                     llvm_gen_be_error_t err_ty = resolve_atomic_pointee_llvm_type(val_ptr_node, &val_ty);
                     if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
                     uint64_t sz = be_state->module->getDataLayout().getTypeStoreSize(val_ty);
                     bool is_nat = false;
                     LLVM_GEN_BE_CHECK(check_native_atomic_value_type(val_ty, sz, &is_nat));
                     if (!is_nat && !(llvm::isPowerOf2_64(sz) && sz <= 8)) {
                         llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
                         llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
                         llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                         llvm::FunctionType* lib_ft = llvm::FunctionType::get(
                             llvm::Type::getVoidTy(*be_state->context), {i64_ty, ptr_ty, ptr_ty, i32_ty}, false);
                         llvm::FunctionCallee lib_fn = be_state->module->getOrInsertFunction("__atomic_store", lib_ft);
                         be_state->builder->CreateCall(
                             lib_fn, {llvm::ConstantInt::get(i64_ty, sz), ptr_arg, val_arg,
                                      be_state->builder->CreateIntCast(args.back(), i32_ty, false)});
                     } else {
                         llvm::Type* atom_ty = is_nat
                                                   ? val_ty
                                                   : llvm::Type::getIntNTy(*be_state->context, static_cast<unsigned>(sz * 8));
                         llvm::Value* loaded_val = be_state->builder->CreateLoad(atom_ty, val_arg);
                         llvm::StoreInst* store = be_state->builder->CreateStore(loaded_val, ptr_arg);
                         store->setAtomic(order);
                         store->setAlignment(llvm::Align(sz));
                     }
                 }
                 *out_val = llvm::Constant::getNullValue(llvm::Type::getInt32Ty(*be_state->context)); // void
                 return llvm_gen_be_error_t::ok;
             } else if (rname.find("atomic_exchange") != std::string::npos) {
                 bool has_sz_arg = args[0]->getType()->isIntegerTy();
                 const size_t min_xchg_args = (rname == "__atomic_exchange")
                                                  ? (has_sz_arg ? 4U : 3U)
                                                  : (has_sz_arg ? 3U : 2U);
                 if (args.size() < min_xchg_args) {
                     return llvm_gen_be_error_t::atomic_lowering_failed;
                 }
                 llvm::Value* ptr_arg = has_sz_arg ? args[1] : args[0];
                 llvm::Value* val_arg = has_sz_arg ? args[2] : args[1];
                 llvm::Value* ret_ptr_arg = (rname == "__atomic_exchange")
                                                ? (has_sz_arg ? args[3] : args[2])
                                                : nullptr;
                 an_expr_node_ptr val_ptr_node = has_sz_arg
                                                     ? op1->next->next->next
                                                     : op1->next->next;
                 llvm::AtomicOrdering order = llvm::AtomicOrdering::SequentiallyConsistent;
                 LLVM_GEN_BE_CHECK(decode_atomic_ordering(args.back(), false, &order));

                 if (rname != "__atomic_exchange") {
                     llvm::Type* val_ty = val_arg->getType();
                     uint64_t sz = be_state->module->getDataLayout().getTypeStoreSize(val_ty);
                     bool is_nat = false;
                     LLVM_GEN_BE_CHECK(check_native_atomic_value_type(val_ty, sz, &is_nat));
                     if (!is_nat) {
                         llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
                         llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
                         llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                         llvm::AllocaInst* val_tmp = be_state->builder->CreateAlloca(val_ty, nullptr, "atomic.xchg.val");
                         llvm::AllocaInst* ret_tmp = be_state->builder->CreateAlloca(val_ty, nullptr, "atomic.xchg.ret");
                         be_state->builder->CreateStore(val_arg, val_tmp);
                         llvm::FunctionType* lib_ft = llvm::FunctionType::get(
                             llvm::Type::getVoidTy(*be_state->context), {i64_ty, ptr_ty, ptr_ty, ptr_ty, i32_ty}, false);
                         llvm::FunctionCallee lib_fn = be_state->module->getOrInsertFunction("__atomic_exchange", lib_ft);
                         be_state->builder->CreateCall(
                             lib_fn, {llvm::ConstantInt::get(i64_ty, sz), ptr_arg, val_tmp, ret_tmp,
                                      be_state->builder->CreateIntCast(args.back(), i32_ty, false)});
                         *out_val = be_state->builder->CreateLoad(val_ty, ret_tmp);
                     } else {
                         *out_val = be_state->builder->CreateAtomicRMW(
                             llvm::AtomicRMWInst::Xchg, ptr_arg, val_arg, llvm::Align(sz), order);
                     }
                 } else {
                     llvm::Type* val_ty = nullptr;
                     llvm_gen_be_error_t err_ty = resolve_atomic_pointee_llvm_type(val_ptr_node, &val_ty);
                     if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
                     uint64_t sz = be_state->module->getDataLayout().getTypeStoreSize(val_ty);
                     bool is_nat = false;
                     LLVM_GEN_BE_CHECK(check_native_atomic_value_type(val_ty, sz, &is_nat));
                     if (!is_nat) {
                         llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
                         llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
                         llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                         llvm::FunctionType* lib_ft = llvm::FunctionType::get(
                             llvm::Type::getVoidTy(*be_state->context), {i64_ty, ptr_ty, ptr_ty, ptr_ty, i32_ty}, false);
                         llvm::FunctionCallee lib_fn = be_state->module->getOrInsertFunction("__atomic_exchange", lib_ft);
                         *out_val = be_state->builder->CreateCall(
                             lib_fn, {llvm::ConstantInt::get(i64_ty, sz), ptr_arg, val_arg, ret_ptr_arg,
                                      be_state->builder->CreateIntCast(args.back(), i32_ty, false)});
                     } else {
                         llvm::Value* loaded_val = be_state->builder->CreateLoad(val_ty, val_arg);
                         llvm::Value* old_val = be_state->builder->CreateAtomicRMW(
                             llvm::AtomicRMWInst::Xchg, ptr_arg, loaded_val, llvm::Align(sz), order);
                         be_state->builder->CreateStore(old_val, ret_ptr_arg); // __atomic_exchange([size,] ptr, val_ptr, ret_ptr, order)
                         *out_val = old_val;
                     }
                 }
                 return llvm_gen_be_error_t::ok;
             } else if (rname.find("atomic_compare_exchange") != std::string::npos) {
                 bool is_generic_ptr_desired = (rname == "__atomic_compare_exchange");
                 bool has_sz_arg = (is_generic_ptr_desired && args[0]->getType()->isIntegerTy());
                 llvm::Value* ptr_arg = has_sz_arg ? args[1] : args[0];
                 llvm::Value* exp_ptr_arg = has_sz_arg ? args[2] : args[1];
                 llvm::Value* des_arg = has_sz_arg ? args[3] : args[2];
                 llvm::Value* succ_ord_arg = has_sz_arg ? args[args.size() - 2]
                                                        : ((args.size() >= 6) ? args[4] : args[args.size() - 2]);
                 llvm::Value* fail_ord_arg = args.back();
                 llvm::AtomicOrdering succ_order = llvm::AtomicOrdering::SequentiallyConsistent;
                 llvm::AtomicOrdering fail_order = llvm::AtomicOrdering::SequentiallyConsistent;
                 LLVM_GEN_BE_CHECK(decode_atomic_ordering(succ_ord_arg, false, &succ_order));
                 LLVM_GEN_BE_CHECK(decode_atomic_ordering(fail_ord_arg, true, &fail_order));

                 llvm::Type* val_ty = nullptr;
                 an_expr_node_ptr exp_arg_node = has_sz_arg
                                                     ? op1->next->next->next
                                                     : op1->next->next;
                 if (exp_arg_node->type &&
                     skip_typerefs(exp_arg_node->type)->kind == tk_pointer) {
                     LLVM_GEN_BE_CHECK(resolve_atomic_pointee_llvm_type(exp_arg_node, &val_ty));
                 } else {
                     val_ty = des_arg->getType();
                 }

                 llvm::Type* res_bool_ty = llvm::Type::getInt8Ty(*be_state->context);
                 if (expr->type) {
                     LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &res_bool_ty));
                 }

                 uint64_t sz = be_state->module->getDataLayout().getTypeStoreSize(val_ty);
                 bool is_lock_free = false;
                 LLVM_GEN_BE_CHECK(check_native_atomic_value_type(val_ty, sz, &is_lock_free));
                 if (!is_lock_free) {
                     llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
                     llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
                     llvm::Type* i1_ty = llvm::Type::getInt1Ty(*be_state->context);
                     llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                     llvm::Value* desired_ptr = des_arg;
                     if (!is_generic_ptr_desired) {
                         llvm::AllocaInst* des_tmp = be_state->builder->CreateAlloca(val_ty, nullptr, "cmpxchg.desired.tmp");
                         be_state->builder->CreateStore(des_arg, des_tmp);
                         desired_ptr = des_tmp;
                     }
                     llvm::FunctionType* lib_ft = llvm::FunctionType::get(
                         i1_ty, {i64_ty, ptr_ty, ptr_ty, ptr_ty, i32_ty, i32_ty}, false);
                     llvm::FunctionCallee lib_fn = be_state->module->getOrInsertFunction("__atomic_compare_exchange", lib_ft);
                     llvm::Value* ok_i1 = be_state->builder->CreateCall(
                         lib_fn,
                         {llvm::ConstantInt::get(i64_ty, sz), ptr_arg, exp_ptr_arg, desired_ptr,
                          be_state->builder->CreateIntCast(succ_ord_arg, i32_ty, false),
                          be_state->builder->CreateIntCast(fail_ord_arg, i32_ty, false)});
                     *out_val = res_bool_ty->isIntegerTy(1)
                                    ? ok_i1
                                    : be_state->builder->CreateZExt(ok_i1, res_bool_ty);
                     return llvm_gen_be_error_t::ok;
                 }

                 llvm::Value* expected_val = be_state->builder->CreateLoad(val_ty, exp_ptr_arg);
                 llvm::Value* desired_val = is_generic_ptr_desired
                                                ? static_cast<llvm::Value*>(be_state->builder->CreateLoad(val_ty, des_arg))
                                                : des_arg;
                 llvm::Type* orig_val_ty = val_ty;

                 if (val_ty->isFloatingPointTy()) {
                     llvm::Type* int_ty = llvm::IntegerType::get(*be_state->context, static_cast<unsigned>(val_ty->getPrimitiveSizeInBits()));
                     expected_val = be_state->builder->CreateBitCast(expected_val, int_ty);
                     desired_val = be_state->builder->CreateBitCast(desired_val, int_ty);
                 }
                 
                 llvm::AtomicCmpXchgInst* cmpxchg = be_state->builder->CreateAtomicCmpXchg(
                     ptr_arg, expected_val, desired_val, llvm::Align(sz), succ_order, fail_order);
                 if (!has_sz_arg && args.size() >= 6) {
                     if (auto* weak_ci = llvm::dyn_cast<llvm::ConstantInt>(args[3])) {
                         if (!weak_ci->isZero()) cmpxchg->setWeak(true);
                     }
                 }
                     
                 llvm::Value* success = be_state->builder->CreateExtractValue(cmpxchg, 1);
                 llvm::Value* loaded_val = be_state->builder->CreateExtractValue(cmpxchg, 0);
                 if (orig_val_ty->isFloatingPointTy()) {
                     loaded_val = be_state->builder->CreateBitCast(loaded_val, orig_val_ty);
                 }
                 
                 // Store loaded_val back to expected_ptr ONLY on the failure path (C11/C++11 memory model)
                 llvm::Function* parent_fn = be_state->builder->GetInsertBlock()->getParent();
                 llvm::BasicBlock* fail_bb = llvm::BasicBlock::Create(*be_state->context, "cmpxchg.fail", parent_fn);
                 llvm::BasicBlock* done_bb = llvm::BasicBlock::Create(*be_state->context, "cmpxchg.end", parent_fn);
                 be_state->builder->CreateCondBr(success, done_bb, fail_bb);
                 be_state->builder->SetInsertPoint(fail_bb);
                 be_state->builder->CreateStore(loaded_val, exp_ptr_arg);
                 be_state->builder->CreateBr(done_bb);
                 be_state->builder->SetInsertPoint(done_bb);
                 
                 *out_val = res_bool_ty->isIntegerTy(1)
                                ? success
                                : be_state->builder->CreateZExt(success, res_bool_ty);
                 return llvm_gen_be_error_t::ok;
             }
      }
  }

  if (!callee_ty) {
     if (llvm::Function* f = llvm::dyn_cast<llvm::Function>(v1)) {
       callee_ty = f->getFunctionType();
     } else {
       std::vector<llvm::Type*> inferred_param_tys;
       for (llvm::Value* a : args) {
         inferred_param_tys.push_back(a->getType());
       }
       llvm::Type* inferred_ret_ty = llvm::Type::getVoidTy(*be_state->context);
       if (expr->type) {
         llvm_gen_be_error_t err = get_llvm_type(expr->type, &inferred_ret_ty);
         if (err != llvm_gen_be_error_t::ok) return err;
       }
       callee_ty = llvm::FunctionType::get(inferred_ret_ty, inferred_param_tys, false);
     }
  }

  if (op1->kind == enk_routine) {
    llvm::StringRef rref = op1->variant.routine.ptr->source_corresp.name;
    if (rref.starts_with("__builtin_") &&
        rref != "__builtin_test_custom_unhandled_call") {
      std::string lib_sym;
      if (rref == "__builtin_operator_new") {
        lib_sym = (args.size() >= 2) ? "_ZnwmSt11align_val_t" : "_Znwm";
      } else if (rref == "__builtin_operator_delete") {
        lib_sym = (args.size() >= 3) ? "_ZdlPvmSt11align_val_t"
                                     : ((args.size() == 2) ? "_ZdlPvSt11align_val_t" : "_ZdlPv");
      } else if (rref == "__builtin_char_memchr") {
        lib_sym = "memchr";
      } else {
        lib_sym = rref.substr(10).str();
      }
      llvm::Function* dead_bi = be_state->module->getFunction(rref);
      v1 = be_state->module->getOrInsertFunction(lib_sym, callee_ty).getCallee();
      if (dead_bi->use_empty()) {
        dead_bi->eraseFromParent();
      }
    }
  }

  llvm::CallBase* call_res = nullptr;
  llvm::Function* callee_fn = llvm::dyn_cast<llvm::Function>(v1);
  bool can_throw = !callee_fn || !callee_fn->doesNotThrow();
  llvm::BasicBlock* lpad_bb = nullptr;
  if (can_throw) {
    LLVM_GEN_BE_CHECK(llvm_get_or_create_current_unwind_dest(&lpad_bb));
  }
  if (lpad_bb) {
    llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* normal_bb = llvm::BasicBlock::Create(*be_state->context, "invoke.cont", func);
    call_res = be_state->builder->CreateInvoke(callee_ty, v1, normal_bb, lpad_bb, args);
    be_state->builder->SetInsertPoint(normal_bb);
  } else {
    call_res = be_state->builder->CreateCall(callee_ty, v1, args);
  }
  if (callee_fn) {
    call_res->setCallingConv(callee_fn->getCallingConv());
  }
  if (!callee_attrs.isEmpty()) {
    call_res->setAttributes(callee_attrs);
  }

  if (direct_sret_dest) {
    be_state->prvalue_constructed_in_place = true;
    *out_val = nullptr;
  } else if (sret_alloc) {
    *out_val = be_state->builder->CreateLoad(sret_ty, sret_alloc, "sret.load");
  } else if (expr->type && !call_res->getType()->isVoidTy()) {
    llvm::Type* expected_ret_ty = nullptr;
    llvm_gen_be_error_t err = get_llvm_type(expr->type, &expected_ret_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
    if (expected_ret_ty != call_res->getType()) {
      llvm::AllocaInst* unpack_alloc = be_state->builder->CreateAlloca(call_res->getType(), nullptr, "ret.unpack");
      be_state->builder->CreateStore(call_res, unpack_alloc);
      *out_val = be_state->builder->CreateLoad(expected_ret_ty, unpack_alloc, "ret.unpacked");
    } else {
      *out_val = call_res;
    }
  } else {
    *out_val = call_res;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Resolves a `va_list` expression operand to its underlying pointer or stack slot.
 * @details Uses `be_state->local_vars` directly when `op` is a local `va_list` variable,
 *          or lowers `op` as an expression otherwise.
 * @param[in] op Pointer to the EDG `va_list` expression operand.
 * @param[out] out_ptr Receives the resolved `va_list` pointer value.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
static llvm_gen_be_error_t resolve_va_list_ptr(
    an_expr_node_ptr op,
    llvm::Value** out_ptr) noexcept {
  if (op->kind == enk_variable) {
    *out_ptr = be_state->local_vars[op->variant.variable.ptr];
    return llvm_gen_be_error_t::ok;
  }
  return llvm_lower_expression(op, out_ptr);
}

/**
 * @brief Extracts a bitfield descriptor from a field-selection expression when applicable.
 * @details Inspects `expr` for `eok_dot_field` or `eok_points_to_field` targeting a bitfield.
 * @param[in] expr Pointer to the EDG expression node.
 * @param[out] out_field Receives the `a_field_ptr` if `expr` accesses a bitfield, or `nullptr`.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t extract_bitfield_target(
    an_expr_node_ptr expr,
    a_field_ptr* out_field) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  *out_field = nullptr;
  if (expr->kind == enk_operation &&
      (expr->variant.operation.kind == eok_dot_field ||
       expr->variant.operation.kind == eok_points_to_field)) {
    an_expr_node_ptr op2 = expr->variant.operation.operands->next;
    if (op2->variant.field.ptr->is_bit_field) {
      *out_field = op2->variant.field.ptr;
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Resolves or declares a `void(ptr)` destructor or cleanup function in the current module.
 * @details Looks up `routine->source_corresp.name` in `be_state->module` or creates an external
 *          `void(ptr)` declaration if not yet present.
 * @param[in] routine Pointer to the EDG destructor or cleanup routine.
 * @param[out] out_fn Receives the resolved `llvm::Function*`, or `nullptr` if unnamed/null.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t get_or_create_void_ptr_fn(
    a_routine_ptr routine,
    llvm::Function** out_fn) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  *out_fn = nullptr;
  if (routine) {
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::FunctionType* ft = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*be_state->context), {ptr_ty}, false);
    llvm::FunctionCallee callee = be_state->module->getOrInsertFunction(
        routine->source_corresp.name, ft);
    *out_fn = llvm::dyn_cast<llvm::Function>(callee.getCallee());
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Evaluates runtime fallback for type-trait `a_builtin_operation_kind` values (`bok_is_*`, `bok_has_*`).
 * @details Inspects the target EDG type operand(s) and produces a boolean/integer `1` or `0` result.
 * @param[in] bok The builtin operation kind.
 * @param[in] op1 Pointer to the first operand (`enk_type_operand` or expression).
 * @param[out] out_result Receives `1` if the trait holds, or `0` otherwise.
 * @param[out] out_handled Receives `true` if @p bok is a recognized type-trait builtin.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t evaluate_type_trait_builtin(
    a_builtin_operation_kind bok,
    an_expr_node_ptr op1,
    int32_t* out_result,
    bool* out_handled) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  *out_handled = true;
  *out_result = 0;
  a_type_ptr t1 = (op1->kind == enk_type_operand) ? op1->variant.type_operand.type : op1->type;
  if (t1) t1 = skip_typerefs(t1);
  an_expr_node_ptr op2 = op1->next;
  a_type_ptr t2 = (op2 && op2->kind == enk_type_operand) ? op2->variant.type_operand.type
                                                         : (op2 ? op2->type : nullptr);
  if (t2) t2 = skip_typerefs(t2);

  if (bok == bok_is_class) {
    *out_result = (t1 && (t1->kind == tk_class || t1->kind == tk_struct)) ? 1 : 0;
  } else if (bok == bok_is_union) {
    *out_result = (t1 && t1->kind == tk_union) ? 1 : 0;
  } else if (bok == bok_is_enum) {
    *out_result = (t1 && t1->kind == tk_integer && t1->variant.integer.enum_type) ? 1 : 0;
  } else if (bok == bok_is_empty) {
    *out_result = (t1 && (t1->kind == tk_class || t1->kind == tk_struct) &&
                   t1->variant.class_struct_union.field_list == nullptr &&
                   !t1->variant.class_struct_union.any_virtual_functions_including_in_base_classes)
                      ? 1
                      : 0;
  } else if (bok == bok_is_polymorphic) {
    *out_result = (t1 && (t1->kind == tk_class || t1->kind == tk_struct) &&
                   t1->variant.class_struct_union.any_virtual_functions)
                      ? 1
                      : 0;
  } else if (bok == bok_is_abstract) {
    *out_result = (t1 && (t1->kind == tk_class || t1->kind == tk_struct) &&
                   t1->variant.class_struct_union.abstract)
                      ? 1
                      : 0;
  } else if (bok == bok_is_final) {
    *out_result = (t1 && (t1->kind == tk_class || t1->kind == tk_struct) &&
                   t1->variant.class_struct_union.final)
                      ? 1
                      : 0;
  } else if (bok == bok_is_same || bok == bok_is_same_as) {
    *out_result = (t1 != nullptr && t1 == t2) ? 1 : 0;
  } else if (bok == bok_is_base_of || bok == bok_builtin_is_virtual_base_of) {
    if (t1 && t2 && (t1->kind == tk_class || t1->kind == tk_struct) &&
        (t2->kind == tk_class || t2->kind == tk_struct)) {
      if (t1 == t2 && bok == bok_is_base_of) {
        *out_result = 1;
      } else {
        std::vector<std::pair<a_type_ptr, bool>> worklist;
        std::vector<a_type_ptr> visited;
        worklist.push_back({t2, false});
        while (!worklist.empty()) {
          a_type_ptr cur_cls = worklist.back().first;
          bool via_virt = worklist.back().second;
          worklist.pop_back();
          if (!cur_cls->variant.class_struct_union.extra_info) continue;
          auto push_edge = [&](a_base_class_ptr bc) noexcept -> bool {
            if (!bc->type) return false;
            a_type_ptr bt = skip_typerefs(bc->type);
            bool edge_virt = via_virt || (bc->is_virtual != FALSE);
            if (bt == t1 && (bok == bok_is_base_of || edge_virt)) {
              *out_result = 1;
              return true;
            }
            bool seen = false;
            for (a_type_ptr v : visited) {
              if (v == bt && !edge_virt) { seen = true; break; }
            }
            if (!seen && (bt->kind == tk_class || bt->kind == tk_struct)) {
              visited.push_back(bt);
              worklist.push_back({bt, edge_virt});
            }
            return false;
          };
          bool found = false;
          for (a_base_class_ptr bc = cur_cls->variant.class_struct_union.extra_info->direct_base_classes;
               bc != nullptr && !found; bc = bc->next_direct) {
            found = push_edge(bc);
          }
          for (a_base_class_ptr bc = cur_cls->variant.class_struct_union.extra_info->base_classes;
               bc != nullptr && !found; bc = bc->next) {
            found = push_edge(bc);
          }
          if (found) break;
        }
      }
    }
  } else if (bok == bok_is_void) {
    *out_result = (t1 && t1->kind == tk_void) ? 1 : 0;
  } else if (bok == bok_is_integral) {
    *out_result = (t1 && t1->kind == tk_integer && !t1->variant.integer.enum_type) ? 1 : 0;
  } else if (bok == bok_is_floating_point) {
    *out_result = (t1 && t1->kind == tk_float) ? 1 : 0;
  } else if (bok == bok_is_arithmetic) {
    *out_result = (t1 && ((t1->kind == tk_integer && !t1->variant.integer.enum_type) || t1->kind == tk_float)) ? 1 : 0;
  } else if (bok == bok_is_fundamental) {
    *out_result = (t1 && (t1->kind == tk_void || t1->kind == tk_nullptr ||
                          (t1->kind == tk_integer && !t1->variant.integer.enum_type) ||
                          t1->kind == tk_float)) ? 1 : 0;
  } else if (bok == bok_is_compound) {
    *out_result = (t1 && !(t1->kind == tk_void || t1->kind == tk_nullptr ||
                           (t1->kind == tk_integer && !t1->variant.integer.enum_type) ||
                           t1->kind == tk_float)) ? 1 : 0;
  } else if (bok == bok_is_array) {
    *out_result = (t1 && t1->kind == tk_array) ? 1 : 0;
  } else if (bok == bok_is_bounded_array) {
    *out_result = (t1 && t1->kind == tk_array && t1->variant.array.variant.number_of_elements > 0) ? 1 : 0;
  } else if (bok == bok_is_unbounded_array) {
    *out_result = (t1 && t1->kind == tk_array && t1->variant.array.variant.number_of_elements == 0) ? 1 : 0;
  } else if (bok == bok_is_pointer) {
    *out_result = (t1 && t1->kind == tk_pointer && !t1->variant.pointer.is_reference) ? 1 : 0;
  } else if (bok == bok_is_lvalue_reference) {
    *out_result = (t1 && t1->kind == tk_pointer && t1->variant.pointer.is_reference &&
                   !t1->variant.pointer.is_rvalue_reference) ? 1 : 0;
  } else if (bok == bok_is_rvalue_reference) {
    *out_result = (t1 && t1->kind == tk_pointer && t1->variant.pointer.is_reference &&
                   t1->variant.pointer.is_rvalue_reference) ? 1 : 0;
  } else if (bok == bok_is_reference) {
    *out_result = (t1 && t1->kind == tk_pointer && t1->variant.pointer.is_reference) ? 1 : 0;
  } else if (bok == bok_is_member_pointer) {
    *out_result = (t1 && t1->kind == tk_ptr_to_member) ? 1 : 0;
  } else if (bok == bok_is_member_function_pointer) {
    *out_result = (t1 && t1->kind == tk_ptr_to_member && t1->variant.ptr_to_member.type &&
                   skip_typerefs(t1->variant.ptr_to_member.type)->kind == tk_routine) ? 1 : 0;
  } else if (bok == bok_is_member_object_pointer) {
    *out_result = (t1 && t1->kind == tk_ptr_to_member && t1->variant.ptr_to_member.type &&
                   skip_typerefs(t1->variant.ptr_to_member.type)->kind != tk_routine) ? 1 : 0;
  } else if (bok == bok_is_function) {
    *out_result = (t1 && t1->kind == tk_routine) ? 1 : 0;
  } else if (bok == bok_is_scalar) {
    *out_result = (t1 && (t1->kind == tk_integer || t1->kind == tk_float || t1->kind == tk_nullptr ||
                          t1->kind == tk_ptr_to_member ||
                          (t1->kind == tk_pointer && !t1->variant.pointer.is_reference))) ? 1 : 0;
  } else if (bok == bok_is_object) {
    *out_result = (t1 && t1->kind != tk_void && t1->kind != tk_routine &&
                   !(t1->kind == tk_pointer && t1->variant.pointer.is_reference)) ? 1 : 0;
  } else if (bok == bok_is_const) {
    a_type_ptr raw_t1 = (op1->kind == enk_type_operand) ? op1->variant.type_operand.type : op1->type;
    *out_result = (raw_t1 && (f_get_type_qualifiers(raw_t1, FALSE) & TQ_CONST) != 0) ? 1 : 0;
  } else if (bok == bok_is_volatile) {
    a_type_ptr raw_t1 = (op1->kind == enk_type_operand) ? op1->variant.type_operand.type : op1->type;
    *out_result = (raw_t1 && (f_get_type_qualifiers(raw_t1, FALSE) & TQ_VOLATILE) != 0) ? 1 : 0;
  } else if (bok == bok_is_signed) {
    *out_result = (t1 && (t1->kind == tk_float ||
                          (t1->kind == tk_integer && !t1->variant.integer.bool_type &&
                           int_kind_is_signed[t1->variant.integer.int_kind]))) ? 1 : 0;
  } else if (bok == bok_is_unsigned) {
    *out_result = (t1 && t1->kind == tk_integer &&
                   (t1->variant.integer.bool_type || !int_kind_is_signed[t1->variant.integer.int_kind])) ? 1 : 0;
  } else if (bok == bok_is_scoped_enum) {
    *out_result = (t1 && t1->kind == tk_integer && t1->variant.integer.enum_type &&
                   t1->variant.integer.is_scoped_enum) ? 1 : 0;
  } else if (bok == bok_has_virtual_destructor) {
    bool has_vd = false;
    if (t1 && (t1->kind == tk_class || t1->kind == tk_struct)) {
      std::vector<a_type_ptr> stack = {t1};
      while (!stack.empty() && !has_vd) {
        a_type_ptr cur = stack.back();
        stack.pop_back();
        auto* extra = cur->variant.class_struct_union.extra_info;
        if (!extra) continue;
        if (extra->assoc_scope) {
          for (a_routine_ptr r = extra->assoc_scope->routines; r != nullptr; r = r->next) {
            if (r->special_kind == sfk_destructor && r->is_virtual) {
              has_vd = true;
              break;
            }
          }
        }
        for (a_base_class_ptr bc = extra->direct_base_classes; bc != nullptr; bc = bc->next_direct) {
          if (bc->type) stack.push_back(skip_typerefs(bc->type));
        }
        for (a_base_class_ptr bc = extra->base_classes; bc != nullptr; bc = bc->next) {
          if (bc->type) stack.push_back(skip_typerefs(bc->type));
        }
      }
    }
    *out_result = has_vd ? 1 : 0;
  } else if (bok == bok_is_aggregate) {
    bool is_agg = false;
    if (t1) {
      if (t1->kind == tk_array) {
        is_agg = true;
      } else if (t1->kind == tk_union || t1->kind == tk_class || t1->kind == tk_struct) {
        is_agg = !t1->variant.class_struct_union.any_virtual_functions &&
                 !t1->variant.class_struct_union.any_virtual_functions_including_in_base_classes &&
                 !t1->variant.class_struct_union.any_virtual_base_classes &&
                 !t1->variant.class_struct_union.abstract;
        for (a_field_ptr f = t1->variant.class_struct_union.field_list; is_agg && f != nullptr; f = f->next) {
          if (f->source_corresp.access == as_private || f->source_corresp.access == as_protected) {
            is_agg = false;
          }
        }
        auto* extra = t1->variant.class_struct_union.extra_info;
        if (is_agg && extra) {
          for (a_base_class_ptr bc = extra->direct_base_classes; is_agg && bc != nullptr; bc = bc->next_direct) {
            if (bc->is_virtual || (bc->derivation && bc->derivation->access != as_public)) {
              is_agg = false;
            }
          }
          if (is_agg && extra->assoc_scope) {
            for (a_routine_ptr r = extra->assoc_scope->routines; r != nullptr; r = r->next) {
              if (r->special_kind == sfk_constructor && !r->compiler_generated) {
                is_agg = false;
                break;
              }
            }
          }
        }
      }
    }
    *out_result = is_agg ? 1 : 0;
  } else if (bok == bok_is_layout_compatible) {
    *out_result = (t1 && t2 && (t1 == t2 || (t1->kind == t2->kind && t1->size == t2->size))) ? 1 : 0;
  } else if (bok == bok_is_pointer_interconvertible_base_of) {
    *out_result = (t1 && t2 && (t1->kind == tk_class || t1->kind == tk_struct) &&
                   (t2->kind == tk_class || t2->kind == tk_struct) &&
                   !t2->variant.class_struct_union.any_virtual_functions) ? 1 : 0;
  } else if (bok == bok_reference_binds_to_temporary || bok == bok_reference_constructs_from_temporary ||
             bok == bok_reference_converts_from_temporary) {
    *out_result = (t1 && t2 && t1->kind == tk_pointer && t1->variant.pointer.is_reference &&
                   skip_typerefs(t1->variant.pointer.type) != t2) ? 1 : 0;
  } else if (bok == bok_array_rank) {
    int32_t rank = 0;
    a_type_ptr cur = t1;
    while (cur && cur->kind == tk_array) {
      rank++;
      cur = skip_typerefs(cur->variant.array.element_type);
    }
    *out_result = rank;
  } else if (bok == bok_array_extent) {
    uint64_t dim_idx = 0;
    if (op2 && op2->kind == enk_constant && op2->variant.constant.ptr) {
      dim_idx = EDG_INT_VAL_LOW64(op2->variant.constant.ptr->variant.integer_value);
    }
    a_type_ptr cur = t1;
    for (uint64_t d = 0; d < dim_idx && cur->kind == tk_array; ++d) {
      cur = skip_typerefs(cur->variant.array.element_type);
    }
    if (cur && cur->kind == tk_array) {
      *out_result = static_cast<int32_t>(cur->variant.array.variant.number_of_elements);
    }
  } else if (bok == bok_is_standard_layout || bok == bok_is_pod || bok == bok_is_literal_type ||
             bok == bok_is_trivially_copyable || bok == bok_is_trivial) {
    a_type_ptr elem = t1;
    while (elem && elem->kind == tk_array) {
      elem = skip_typerefs(elem->variant.array.element_type);
    }
    auto check_trivially_copyable_elem = [](a_type_ptr et) noexcept -> bool {
      if (!et || et->kind == tk_void || et->kind == tk_routine) return false;
      if (et->kind == tk_pointer && et->variant.pointer.is_reference) return false;
      if (et->kind == tk_class || et->kind == tk_struct || et->kind == tk_union) {
        return !et->variant.class_struct_union.any_virtual_functions &&
               !et->variant.class_struct_union.any_virtual_functions_including_in_base_classes &&
               !et->variant.class_struct_union.any_virtual_base_classes &&
               !type_has_nontrivial_destructor(et) &&
               (is_trivially_copyable_type(et) != FALSE);
      }
      return true;
    };
    auto check_trivial_elem = [&](a_type_ptr et) noexcept -> bool {
      if (!check_trivially_copyable_elem(et)) return false;
      if (et->kind == tk_class || et->kind == tk_struct || et->kind == tk_union) {
        auto* extra = et->variant.class_struct_union.extra_info;
        if (extra) {
          if (extra->has_field_initializer) return false;
          if (extra->assoc_scope) {
            for (a_routine_ptr r = extra->assoc_scope->routines; r != nullptr; r = r->next) {
              if (r->special_kind == sfk_constructor &&
                  !r->compiler_generated && !r->is_trivial_default_constructor) {
                return false;
              }
            }
          }
        }
      }
      return true;
    };
    auto check_standard_layout_elem = [](a_type_ptr et) noexcept -> bool {
      if (!et || et->kind == tk_void || et->kind == tk_routine) return false;
      if (et->kind == tk_pointer && et->variant.pointer.is_reference) return false;
      if (et->kind == tk_class || et->kind == tk_struct || et->kind == tk_union) {
        if (et->variant.class_struct_union.any_virtual_functions ||
            et->variant.class_struct_union.any_virtual_functions_including_in_base_classes ||
            et->variant.class_struct_union.any_virtual_base_classes) {
          return false;
        }
        bool seen_access = false;
        an_access_specifier first_access = as_public;
        for (a_field_ptr f = et->variant.class_struct_union.field_list; f != nullptr; f = f->next) {
          a_type_ptr ft = f->type ? skip_typerefs(f->type) : nullptr;
          if (ft && ft->kind == tk_pointer && ft->variant.pointer.is_reference) return false;
          if (!seen_access) {
            seen_access = true;
            first_access = static_cast<an_access_specifier>(f->source_corresp.access);
          } else if (static_cast<an_access_specifier>(f->source_corresp.access) != first_access) {
            return false;
          }
        }
        auto* extra = et->variant.class_struct_union.extra_info;
        if (extra) {
          unsigned classes_with_fields = (et->variant.class_struct_union.field_list != nullptr) ? 1u : 0u;
          for (a_base_class_ptr bc = extra->base_classes; bc != nullptr; bc = bc->next) {
            a_type_ptr bt = bc->type ? skip_typerefs(bc->type) : nullptr;
            if (bt && (bt->kind == tk_class || bt->kind == tk_struct)) {
              if (bt->variant.class_struct_union.any_virtual_functions ||
                  bt->variant.class_struct_union.any_virtual_base_classes) {
                return false;
              }
              if (bt->variant.class_struct_union.field_list != nullptr) {
                ++classes_with_fields;
              }
            }
          }
          if (classes_with_fields > 1u) return false;
        }
      }
      return true;
    };
    bool res_flag = false;
    if (bok == bok_is_trivially_copyable) {
      res_flag = check_trivially_copyable_elem(elem);
    } else if (bok == bok_is_trivial) {
      res_flag = check_trivial_elem(elem);
    } else if (bok == bok_is_standard_layout) {
      res_flag = check_standard_layout_elem(elem);
    } else if (bok == bok_is_pod) {
      res_flag = check_trivial_elem(elem) && check_standard_layout_elem(elem);
    } else {
      if (elem) {
        if (elem->kind == tk_void || (elem->kind == tk_pointer && elem->variant.pointer.is_reference)) {
          res_flag = (elem->kind != tk_void || C_dialect != C_dialect_pcc);
        } else if (elem->kind == tk_class || elem->kind == tk_struct || elem->kind == tk_union) {
          res_flag = !type_has_nontrivial_destructor(elem);
        } else {
          res_flag = (elem->kind != tk_routine);
        }
      }
    }
    *out_result = res_flag ? 1 : 0;
  } else if (bok == bok_has_unique_object_representations) {
    bool has_unique_rep = false;
    if (t1->size > 0 && t1->kind != tk_pointer) {
      std::vector<uint8_t> pad_mask;
      LLVM_GEN_BE_CHECK(llvm_compute_type_padding_mask(t1, &pad_mask));
      has_unique_rep = true;
      for (uint8_t mb : pad_mask) {
        if (mb != 0xFF) {
          has_unique_rep = false;
          break;
        }
      }
    }
    *out_result = has_unique_rep ? 1 : 0;
  } else if (bok == bok_is_trivially_constructible || bok == bok_is_constructible ||
             bok == bok_is_nothrow_constructible || bok == bok_is_destructible ||
             bok == bok_is_nothrow_destructible || bok == bok_is_trivially_destructible) {
    bool ok_trait = (t1->kind != tk_void && t1->kind != tk_routine &&
                     !(t1->kind == tk_class && t1->variant.class_struct_union.abstract));
    if (ok_trait && (bok == bok_is_trivially_constructible || bok == bok_is_trivially_destructible)) {
      ok_trait = !type_has_nontrivial_destructor(t1);
    }
    *out_result = ok_trait ? 1 : 0;
  } else if (bok == bok_is_assignable || bok == bok_is_trivially_assignable ||
             bok == bok_is_nothrow_assignable || bok == bok_is_trivially_copy_assignable ||
             bok == bok_is_assignable_no_precondition_check ||
             bok == bok_is_convertible_to || bok == bok_is_convertible ||
             bok == bok_is_nothrow_convertible) {
    bool ok_assign_or_conv = (t1 && t1->kind != tk_void &&
                              (!t2 || t2->kind != tk_void) &&
                              !type_has_nontrivial_destructor(t1));
    *out_result = ok_assign_or_conv ? 1 : 0;
  } else {
    *out_handled = false;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an EDG builtin operation node (`enk_builtin_operation`, `a_builtin_operation_kind`) into LLVM IR.
 * @details Supports `bok_offsetof`, `bok_intaddr`, `bok_builtin_bit_cast`, `bok_builtin_has_attribute`,
 *          `bok_types_compatible`, `bok_builtin_complex`, `bok_builtin_addressof`, `bok_builtin_shuffle`,
 *          `bok_builtin_shufflevector`, `bok_builtin_convertvector`, and runtime fallbacks for compiler
 *          type-trait `bok_*` operations.
 * @param[in] expr Pointer to the EDG `enk_builtin_operation` expression node.
 * @param[out] out_val Receives the lowered `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_builtin_operation(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  a_builtin_operation_kind bok = expr->variant.builtin_operation.kind;
  an_expr_node_ptr op1 = expr->variant.builtin_operation.operands;
  if (!op1) return llvm_gen_be_error_t::invalid_argument;

  if (bok == bok_offsetof) {
    uint64_t total_offset = 0;
    an_expr_node_ptr curr = op1->next;
    while (curr) {
      if (curr->kind == enk_field) {
        total_offset += curr->variant.field.ptr->offset;
      } else {
        an_expr_node_ptr sub_op2 = curr->variant.operation.operands->next;
        total_offset += sub_op2->variant.field.ptr->offset;
      }
      curr = curr->next;
    }
    *out_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), total_offset);
    return llvm_gen_be_error_t::ok;
  } else if (bok == bok_intaddr) {
    llvm::Value* addr_val = nullptr;
    LLVM_GEN_BE_CHECK(llvm_lower_expression(op1, &addr_val));
    llvm::Type* dst_ty = llvm::Type::getInt64Ty(*be_state->context);
    if (expr->type) {
      LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &dst_ty));
    }
    if (addr_val->getType()->isPointerTy()) {
      *out_val = be_state->builder->CreatePtrToInt(addr_val, dst_ty);
    } else {
      *out_val = be_state->builder->CreateIntCast(addr_val, dst_ty, false);
    }
    return llvm_gen_be_error_t::ok;
  } else if (bok == bok_builtin_bit_cast) {
    llvm::Value* src_val = nullptr;
    llvm_gen_be_error_t err = llvm_lower_expression(op1->next, &src_val);
    if (err != llvm_gen_be_error_t::ok) return err;
    llvm::Type* dest_ty = nullptr;
    err = get_llvm_type(expr->type, &dest_ty);
    if (err != llvm_gen_be_error_t::ok) return err;

    if (src_val->getType()->isAggregateType() || dest_ty->isAggregateType()) {
      llvm::AllocaInst* alloca = be_state->builder->CreateAlloca(src_val->getType());
      be_state->builder->CreateStore(src_val, alloca);
      *out_val = be_state->builder->CreateLoad(dest_ty, alloca);
    } else {
      if (src_val->getType() == dest_ty) *out_val = src_val;
      else *out_val = be_state->builder->CreateBitCast(src_val, dest_ty);
    }
    return llvm_gen_be_error_t::ok;
  } else if (bok == bok_builtin_has_attribute) {
    an_expr_node_ptr arg2 = op1->next;
    an_attribute_ptr target_attr = arg2 ? arg2->variant.constant.ptr->source_corresp.attributes : nullptr;

    an_expr_node_ptr subj = op1;
    while (subj->kind == enk_operation) {
      an_expr_operator_kind k = subj->variant.operation.kind;
      if (k == eok_parens) {
        subj = subj->variant.operation.operands;
      } else if (k == eok_dot_field) {
        subj = subj->variant.operation.operands->next;
      } else {
        break;
      }
    }

    an_attribute_ptr entity_attrs = nullptr;
    if (subj->kind == enk_variable) {
      entity_attrs = subj->variant.variable.ptr->source_corresp.attributes;
    } else if (subj->kind == enk_routine) {
      entity_attrs = subj->variant.routine.ptr->source_corresp.attributes;
    } else if (subj->kind == enk_field) {
      entity_attrs = subj->variant.field.ptr->source_corresp.attributes;
    } else if (subj->kind == enk_type_operand) {
      entity_attrs = subj->variant.type_operand.type->source_corresp.attributes;
    } else {
      entity_attrs = subj->type->source_corresp.attributes;
    }

    int32_t has_attr = 0;
    for (an_attribute_ptr ap = entity_attrs; ap != nullptr; ap = ap->next) {
      if (!target_attr || ap->kind == target_attr->kind) {
        has_attr = 1;
        break;
      }
    }
    *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), static_cast<uint64_t>(has_attr));
    return llvm_gen_be_error_t::ok;
  } else if (bok == bok_types_compatible) {
    an_expr_node_ptr op2 = op1->next;
    a_type_ptr t1 = (op1->kind == enk_type_operand) ? op1->variant.type_operand.type : op1->type;
    a_type_ptr t2 = (op2 && op2->kind == enk_type_operand) ? op2->variant.type_operand.type
                                                           : (op2 ? op2->type : nullptr);
    if (t1) t1 = skip_typerefs(t1);
    if (t2) t2 = skip_typerefs(t2);
    bool compat = (t1 != nullptr && t2 != nullptr &&
                   (t1 == t2 || (t1->kind == t2->kind && t1->size == t2->size)));
    *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), compat ? 1 : 0);
    return llvm_gen_be_error_t::ok;
  } else if (bok == bok_builtin_complex) {
    llvm::Value* real_v = nullptr;
    llvm::Value* imag_v = nullptr;
    LLVM_GEN_BE_CHECK(lower_binary_operands(op1, &real_v, &imag_v));
    llvm::Type* cplx_ty = llvm::StructType::get(*be_state->context, {real_v->getType(), imag_v->getType()});
    llvm::Value* res = be_state->builder->CreateInsertValue(llvm::UndefValue::get(cplx_ty), real_v, 0);
    *out_val = be_state->builder->CreateInsertValue(res, imag_v, 1);
    return llvm_gen_be_error_t::ok;
  } else if (bok == bok_builtin_addressof) {
    return llvm_lower_lvalue_expression(op1, out_val);
  } else if (bok == bok_builtin_shuffle || bok == bok_builtin_shufflevector) {
    llvm::Value* v1 = nullptr;
    LLVM_GEN_BE_CHECK(llvm_lower_expression(op1, &v1));
    an_expr_node_ptr op2 = op1->next;
    llvm::Value* v2 = nullptr;
    LLVM_GEN_BE_CHECK(llvm_lower_expression(op2, &v2));
    auto* v_ty = llvm::cast<llvm::VectorType>(v1->getType());
    unsigned num_elems = v_ty->getElementCount().getKnownMinValue();
    if (bok == bok_builtin_shufflevector && op2->next != nullptr) {
      std::vector<int> mask_indices;
      for (an_expr_node_ptr cur = op2->next; cur != nullptr; cur = cur->next) {
        llvm::Value* idx_v = nullptr;
        LLVM_GEN_BE_CHECK(llvm_lower_expression(cur, &idx_v));
        int m_idx = 0;
        if (auto* ci = llvm::dyn_cast_or_null<llvm::ConstantInt>(idx_v)) {
          m_idx = static_cast<int>(ci->getSExtValue());
        }
        mask_indices.push_back(m_idx);
      }
      *out_val = be_state->builder->CreateShuffleVector(v1, v2, mask_indices);
      return llvm_gen_be_error_t::ok;
    }
    llvm::Value* mask_val = v2;
    llvm::Value* vec2_val = llvm::PoisonValue::get(v1->getType());
    if (op2->next != nullptr) {
      vec2_val = v2;
      LLVM_GEN_BE_CHECK(llvm_lower_expression(op2->next, &mask_val));
    }
    std::vector<int> mask_indices(num_elems, 0);
    if (auto* cv = llvm::dyn_cast<llvm::Constant>(mask_val)) {
      for (unsigned i = 0; i < num_elems; ++i) {
        auto* elem_ci = llvm::cast<llvm::ConstantInt>(cv->getAggregateElement(i));
        mask_indices[i] = static_cast<int>(elem_ci->getZExtValue() % (num_elems * 2));
      }
    }
    *out_val = be_state->builder->CreateShuffleVector(v1, vec2_val, mask_indices);
    return llvm_gen_be_error_t::ok;
  } else if (bok == bok_builtin_convertvector) {
    llvm::Value* src_vec = nullptr;
    LLVM_GEN_BE_CHECK(llvm_lower_expression(op1, &src_vec));
    llvm::Type* dst_ty = nullptr;
    LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &dst_ty));
    llvm::Type* src_ty = src_vec->getType();
    if (src_ty->isIntOrIntVectorTy() && dst_ty->isIntOrIntVectorTy()) {
      *out_val = be_state->builder->CreateIntCast(src_vec, dst_ty, true);
    } else if (src_ty->isFPOrFPVectorTy() && dst_ty->isFPOrFPVectorTy()) {
      *out_val = be_state->builder->CreateFPCast(src_vec, dst_ty);
    } else if (src_ty->isIntOrIntVectorTy()) {
      *out_val = be_state->builder->CreateSIToFP(src_vec, dst_ty);
    } else {
      *out_val = be_state->builder->CreateFPToSI(src_vec, dst_ty);
    }
    return llvm_gen_be_error_t::ok;
  } else {
    int32_t trait_res = 0;
    bool trait_handled = false;
    LLVM_GEN_BE_CHECK(evaluate_type_trait_builtin(bok, op1, &trait_res, &trait_handled));
    if (trait_handled) {
      llvm::Type* ret_ty = llvm::Type::getInt32Ty(*be_state->context);
      if (expr->type) {
        LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &ret_ty));
      }
      *out_val = llvm::ConstantInt::get(ret_ty, static_cast<uint64_t>(trait_res));
      return llvm_gen_be_error_t::ok;
    }
  }

  return llvm_gen_be_set_error(
      &be_state->err_context, llvm_gen_be_error_t::unsupported_expr,
      __FILE__, __LINE__, 0, "Unsupported builtin operation kind %d",
      static_cast<int>(bok));
}

#if !BUILTIN_FUNCTIONS_ENABLED
/** @brief Fallback sentinel value for `enk_builtin_choose_expr` when EDG builtin functions are disabled. */
static constexpr an_expr_node_kind enk_builtin_choose_expr = static_cast<an_expr_node_kind>(250);
#endif

/**
 * @brief Lowers a GNU `__builtin_choose_expr` node (`enk_builtin_choose_expr`) into LLVM IR.
 * @details Evaluates either the second or third operand of `expr->variant.builtin_choose_expr`
 *          depending on `choose_first`.
 * @param[in] expr Pointer to the EDG `enk_builtin_choose_expr` expression node.
 * @param[out] out_val Receives the lowered `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if operands are missing.
 */
llvm_gen_be_error_t llvm_lower_builtin_choose_expr(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
#if BUILTIN_FUNCTIONS_ENABLED
  an_expr_node_ptr op1 = expr->variant.builtin_choose_expr.operands;
  if (!op1 || !op1->next || !op1->next->next) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  an_expr_node_ptr chosen = expr->variant.builtin_choose_expr.choose_first
                                ? op1->next
                                : op1->next->next;
  return llvm_lower_expression(chosen, out_val);
#else
  (void)expr;
  (void)out_val;
  return llvm_gen_be_error_t::invalid_argument;
#endif
}

/**
 * @brief Lowers an arbitrary EDG expression node (`an_expr_node_ptr`) into an `llvm::Value*`.
 * @details Dispatches constants, variables, operations, conditions, temporaries, `new`/`delete`,
 *          `throw`, `typeid`, `dynamic_cast`, coroutines, and statement expressions.
 * @param[in] expr Pointer to the EDG expression node to lower.
 * @param[out] out_val Receives the lowered `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!out_val) return llvm_gen_be_error_t::invalid_argument;
  if (!expr) {
    *out_val = nullptr;
    return llvm_gen_be_error_t::ok;
  }
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());

  /**
   * @struct DILocSaver
   * @brief RAII helper that saves and restores the active `IRBuilder` debug location.
   * @details Ensures nested sub-expression debug locations do not leak to outer expressions.
   */
  struct DILocSaver {
      llvm::DebugLoc saved_loc; ///< Saved debug location on entry.
      DILocSaver() noexcept : saved_loc(be_state->builder->getCurrentDebugLocation()) {} ///< Captures the current debug location.
      ~DILocSaver() noexcept { be_state->builder->SetCurrentDebugLocation(saved_loc); } ///< Restores the saved debug location.
  } loc_saver;

  if (be_state->dbg_state && expr->position.seq != 0) {
      llvm::DIScope* scope = be_state->dbg_state->scope_stack.back();
      llvm::DILocation* loc = nullptr;
      llvm_gen_be_error_t err = get_di_location(be_state->dbg_state, expr->position, scope, &loc);
      if (err == llvm_gen_be_error_t::ok && loc) {
          be_state->builder->SetCurrentDebugLocation(loc);
      }
  }

  // Handle LValue evaluation explicitly
  if (expr->is_lvalue) {
    switch (expr->kind) {
      case enk_variable:
      case enk_operation:
        if (expr->kind == enk_operation) {
           an_expr_operator_kind op_kind = expr->variant.operation.kind;
           if (op_kind != eok_subscript && op_kind != eok_dot_field &&
               op_kind != eok_points_to_field && op_kind != eok_indirect &&
               op_kind != eok_ref_indirect && op_kind != eok_parens &&
               op_kind != eok_lvalue && op_kind != eok_lvalue_adjust &&
               op_kind != eok_lvalue_cast) {
               break; // Not an LValue-producing operation
           }
        }
        return llvm_lower_lvalue_expression(expr, out_val);
      default: break;
    }
  }

#if DO_IL_LOWERING && !DO_FULL_PORTABLE_EH_LOWERING
  if (expr->kind == enk_lowered_eh_construct) {
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    switch (expr->variant.lowered_eh.kind) {
      case leck_caught_object_address:
        *out_val = be_state->active_caught_object_ptr
                       ? be_state->active_caught_object_ptr
                       : llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
        return llvm_gen_be_error_t::ok;
      case leck_thrown_object_address:
        *out_val = be_state->active_thrown_object_ptr
                       ? be_state->active_thrown_object_ptr
                       : llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
        return llvm_gen_be_error_t::ok;
      case leck_cleanup_state:
      case leck_unreachable_cleanup_state:
#if !GENERATE_EH_TABLES
        be_state->active_lowered_eh_cleanup = expr->variant.lowered_eh.variant.cleanup_ptr;
#endif
        *out_val = nullptr;
        return llvm_gen_be_error_t::ok;
      case leck_internal_try: {
        an_expr_node_ptr first_e = expr->variant.lowered_eh.variant.try_and_catch_expr;
        an_expr_node_ptr second_e = first_e ? first_e->next : nullptr;
        if (!first_e) {
          *out_val = nullptr;
          return llvm_gen_be_error_t::ok;
        }
        llvm::Function* cur_fn = be_state->builder->GetInsertBlock()->getParent();
        llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
        llvm::BasicBlock* enclosing_unwind = nullptr;
        LLVM_GEN_BE_CHECK(llvm_get_or_create_current_unwind_dest(&enclosing_unwind));
        llvm::BasicBlock* internal_lpad =
            llvm::BasicBlock::Create(*be_state->context, "eh.internal.try", cur_fn);
        llvm::BasicBlock* internal_dispatch =
            llvm::BasicBlock::Create(*be_state->context, "eh.internal.dispatch", cur_fn);
        llvm::IRBuilder<> disp_top(internal_dispatch);
        llvm::PHINode* ep_phi = disp_top.CreatePHI(ptr_ty, 2, "eh.int.ep");
        llvm::PHINode* es_phi = disp_top.CreatePHI(i32_ty, 2, "eh.int.es");

        be_state->current_landing_pads.push_back(internal_lpad);
        be_state->landing_pad_cleanup_depths.push_back(be_state->cleanup_stack.cleanups.size());
        be_state->current_lpad_dispatch_bbs.push_back(internal_dispatch);
        be_state->current_lpad_exc_ptr_phis.push_back(ep_phi);
        be_state->current_lpad_exc_sel_phis.push_back(es_phi);
        llvm::Value* first_v = nullptr;
        llvm_gen_be_error_t f_err = llvm_lower_expression(first_e, &first_v);
        be_state->current_lpad_exc_sel_phis.pop_back();
        be_state->current_lpad_exc_ptr_phis.pop_back();
        be_state->current_lpad_dispatch_bbs.pop_back();
        be_state->current_landing_pads.pop_back();
        be_state->landing_pad_cleanup_depths.pop_back();
        if (f_err != llvm_gen_be_error_t::ok) return f_err;

        bool lpad_used = !internal_lpad->hasNPredecessors(0);
        bool disp_used = (ep_phi->getNumIncomingValues() > 0);
        if (!lpad_used && !disp_used) {
          ep_phi->eraseFromParent();
          es_phi->eraseFromParent();
          internal_dispatch->eraseFromParent();
          internal_lpad->eraseFromParent();
          *out_val = nullptr;
          return llvm_gen_be_error_t::ok;
        }
        llvm::IRBuilder<>::InsertPoint saved_ip = be_state->builder->saveIP();
        if (!cur_fn->hasPersonalityFn()) {
          llvm::FunctionCallee pers = be_state->module->getOrInsertFunction(
              "__gxx_personality_v0", llvm::FunctionType::get(i32_ty, true));
          cur_fn->setPersonalityFn(llvm::cast<llvm::Constant>(pers.getCallee()));
        }
        llvm::StructType* lp_ty = llvm::StructType::get(*be_state->context, {ptr_ty, i32_ty});
        llvm::LandingPadInst* lp_inst = nullptr;
        if (lpad_used) {
          be_state->builder->SetInsertPoint(internal_lpad);
          lp_inst = be_state->builder->CreateLandingPad(lp_ty, 0);
          lp_inst->setCleanup(true);
          if (!be_state->current_lpad_dispatch_bbs.empty() &&
              be_state->current_lpad_dispatch_bbs.back()->getParent() == cur_fn) {
            LLVM_GEN_BE_CHECK(llvm_append_enclosing_try_clauses(lp_inst));
          }
          llvm::Value* lpad_ep = be_state->builder->CreateExtractValue(lp_inst, 0);
          llvm::Value* lpad_es = be_state->builder->CreateExtractValue(lp_inst, 1);
          ep_phi->addIncoming(lpad_ep, internal_lpad);
          es_phi->addIncoming(lpad_es, internal_lpad);
          be_state->builder->CreateBr(internal_dispatch);
        } else {
          internal_lpad->eraseFromParent();
        }

        be_state->builder->SetInsertPoint(internal_dispatch);
        if (second_e) {
          llvm::Value* sec_v = nullptr;
          LLVM_GEN_BE_CHECK(llvm_lower_expression(second_e, &sec_v));
        }
        if (enclosing_unwind && !be_state->current_lpad_dispatch_bbs.empty() &&
            be_state->current_lpad_dispatch_bbs.back()->getParent() == cur_fn) {
          llvm::BasicBlock* cur_b = be_state->builder->GetInsertBlock();
          be_state->current_lpad_exc_ptr_phis.back()->addIncoming(ep_phi, cur_b);
          be_state->current_lpad_exc_sel_phis.back()->addIncoming(es_phi, cur_b);
          be_state->builder->CreateBr(be_state->current_lpad_dispatch_bbs.back());
        } else {
          llvm::Value* packed_lp = llvm::PoisonValue::get(lp_ty);
          packed_lp = be_state->builder->CreateInsertValue(packed_lp, ep_phi, 0);
          packed_lp = be_state->builder->CreateInsertValue(packed_lp, es_phi, 1);
          bool spec_handled = false;
          if (enclosing_unwind && lp_inst) {
            LLVM_GEN_BE_CHECK(llvm_emit_routine_exc_spec_handler(
                be_state->current_edg_routine, lp_inst, &spec_handled));
          }
          if (!spec_handled) {
            if (enclosing_unwind && enclosing_unwind->empty()) {
              be_state->builder->CreateBr(enclosing_unwind);
            } else {
              be_state->builder->CreateResume(packed_lp);
            }
          }
        }
        be_state->builder->restoreIP(saved_ip);
        *out_val = nullptr;
        return llvm_gen_be_error_t::ok;
      }
      default:
        *out_val = nullptr;
        return llvm_gen_be_error_t::ok;
    }
  }
#endif

#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
  if (expr->kind == enk_result_of_overriding_function) {
    a_routine_ptr curr_r = be_state->current_edg_routine;
    a_routine_ptr under_r = curr_r ? curr_r->overriding_function_for_wrapper : nullptr;
    if (!under_r) {
      return llvm_gen_be_error_t::invalid_argument;
    }
    llvm::Function* under_fn = nullptr;
    LLVM_GEN_BE_CHECK(llvm_get_or_declare_routine(under_r, &under_fn));
    if (!under_fn) {
      return llvm_gen_be_error_t::invalid_argument;
    }
    llvm::Function* cur_fn = be_state->builder->GetInsertBlock()->getParent();
    std::vector<llvm::Value*> call_args;
    if (be_state->current_function_scope) {
      for (a_variable_ptr p = be_state->current_function_scope->variant.routine.parameters;
           p != nullptr && call_args.size() < under_fn->arg_size(); p = p->next) {
        auto pit = be_state->local_vars.find(p);
        if (pit != be_state->local_vars.end() && pit->second) {
          llvm::Type* p_ty = under_fn->getArg(static_cast<unsigned>(call_args.size()))->getType();
          call_args.push_back(be_state->builder->CreateLoad(p_ty, pit->second));
        }
      }
    }
    while (call_args.size() < under_fn->arg_size() && call_args.size() < cur_fn->arg_size()) {
      call_args.push_back(cur_fn->getArg(static_cast<unsigned>(call_args.size())));
    }
    llvm::CallInst* ci = be_state->builder->CreateCall(under_fn, call_args);
    *out_val = under_fn->getReturnType()->isVoidTy() ? nullptr : static_cast<llvm::Value*>(ci);
    return llvm_gen_be_error_t::ok;
  }
#endif

#if VLA_DEALLOCATIONS_IN_IL
  if (expr->kind == enk_vla_dealloc) {
    a_variable_ptr var = expr->variant.vla_variable;
    if (be_state->vla_saved_stacks.count(var)) {
      llvm::Function* stackrestore = EDG_LLVM_GET_INTRINSIC_DECL(
          be_state->module.get(), llvm::Intrinsic::stackrestore,
          {llvm::PointerType::getUnqual(*be_state->context)});
      be_state->builder->CreateCall(stackrestore, be_state->vla_saved_stacks[var]);
    }
    *out_val = nullptr;
    return llvm_gen_be_error_t::ok;
  }
#endif /* VLA_DEALLOCATIONS_IN_IL */

  switch (expr->kind) {
    case enk_braced_init_list: {
      llvm::Type* agg_ty = nullptr;
      llvm_gen_be_error_t ty_err = get_llvm_type(expr->type, &agg_ty);
      if (ty_err != llvm_gen_be_error_t::ok) return ty_err;
      if (agg_ty->isVoidTy()) {
          *out_val = nullptr;
          return llvm_gen_be_error_t::ok;
      }

      std::vector<llvm::Value*> values;
      for (an_expr_node_ptr item = expr->variant.braced_init_list; item; item = item->next) {
         llvm::Value* val = nullptr;
         llvm_gen_be_error_t err = llvm_lower_expression(item, &val);
         if (err != llvm_gen_be_error_t::ok) return err;
         values.push_back(val);
      }

      llvm::Value* agg = llvm::UndefValue::get(agg_ty);
      if (agg_ty->isAggregateType()) {
          for (size_t i = 0; i < values.size(); ++i) {
             agg = be_state->builder->CreateInsertValue(agg, values[i], static_cast<unsigned>(i));
          }
      } else if (values.size() == 1) {
          agg = values[0];
      }
      *out_val = agg;
      return llvm_gen_be_error_t::ok;
    }

    case enk_reuse_value: {
      a_dynamic_init_ptr init = expr->variant.reused_value_init;
      if (!init) return llvm_gen_be_error_t::invalid_argument;
      auto it = be_state->reused_values.find(init);
      if (it != be_state->reused_values.end()) {
          *out_val = it->second;
          return llvm_gen_be_error_t::ok;
      }
      if (init->kind == dik_expression || init->kind == dik_class_result_via_ctor) {
          llvm::Value* val = nullptr;
          llvm_gen_be_error_t err = llvm_lower_expression(init->variant.expression, &val);
          if (err != llvm_gen_be_error_t::ok) return err;
          be_state->reused_values[init] = val;
          *out_val = val;
          return llvm_gen_be_error_t::ok;
      }
      llvm::Type* expr_ty = nullptr;
      llvm_gen_be_error_t err = get_llvm_type(expr->type, &expr_ty);
      if (err != llvm_gen_be_error_t::ok) return err;
      llvm::Value* val = nullptr;
      if (init->kind == dik_zero) {
          val = llvm::Constant::getNullValue(expr_ty);
      } else if (init->kind == dik_constant) {
          llvm::Constant* con_val = nullptr;
          err = evaluate_constant(init->variant.constant.ptr, expr_ty, &con_val);
          if (err != llvm_gen_be_error_t::ok) return err;
          val = con_val;
      } else if (init->kind == dik_constructor || init->kind == dik_nonconstant_aggregate ||
                 init->kind == dik_bitwise_copy) {
          llvm::AllocaInst* tmp_alloca = be_state->builder->CreateAlloca(expr_ty, nullptr, "reuse.tmp");
          err = llvm_emit_dynamic_init(init, tmp_alloca, expr_ty);
          if (err != llvm_gen_be_error_t::ok) return err;
          val = be_state->builder->CreateLoad(expr_ty, tmp_alloca, "reuse.val");
      } else {
          return llvm_gen_be_error_t::unsupported_expr;
      }
      be_state->reused_values[init] = val;
      *out_val = val;
      return llvm_gen_be_error_t::ok;
    }

    case enk_c11_generic: {
      if (!expr->variant.c11_generic.result) {
        return llvm_gen_be_error_t::unsupported_expr;
      }
      return llvm_lower_expression(expr->variant.c11_generic.result, out_val);
    }

    case enk_fold:
      return llvm_lower_fold_expression(expr, out_val);

    case enk_pack_index: {
      an_expr_node_ptr pack_expr = expr->variant.pack_index.expr;
      if (!pack_expr) {
        return llvm_gen_be_error_t::unsupported_expr;
      }
      uint64_t target_idx = 0;
      if (expr->variant.pack_index.index_expr) {
        llvm::Value* idx_val = nullptr;
        llvm_gen_be_error_t err = llvm_lower_expression(expr->variant.pack_index.index_expr, &idx_val);
        if (err != llvm_gen_be_error_t::ok) return err;
        if (auto* ci = llvm::dyn_cast_or_null<llvm::ConstantInt>(idx_val)) {
          target_idx = ci->getZExtValue();
        }
      }
      an_expr_node_ptr selected = pack_expr;
      for (uint64_t i = 0; i < target_idx && selected->next != nullptr; ++i) {
        selected = selected->next;
      }
      return llvm_lower_expression(selected, out_val);
    }
    case enk_constant: {
       llvm::Type* expr_llvm_ty = nullptr;
       llvm_gen_be_error_t err = get_llvm_type(expr->type, &expr_llvm_ty);
       if (err != llvm_gen_be_error_t::ok) return err;
       a_constant_ptr con = expr->variant.constant.ptr;
       if (!con) {
           return llvm_materialize_null_constant(
               expr_llvm_ty, llvm::Type::getInt32Ty(*be_state->context), out_val);
       }
       llvm::Constant* out_const = nullptr;
       err = evaluate_constant(con, expr_llvm_ty, &out_const);
       if (err != llvm_gen_be_error_t::ok) return err;
       *out_val = out_const;
       return llvm_gen_be_error_t::ok;
    }
    case enk_routine: {
       a_routine_ptr routine = expr->variant.routine.ptr;
       if (!routine) {
           llvm::Type* expr_llvm_ty = nullptr;
           llvm_gen_be_error_t err = get_llvm_type(expr->type, &expr_llvm_ty);
           if (err != llvm_gen_be_error_t::ok) return err;
           return llvm_materialize_null_constant(
               expr_llvm_ty, llvm::PointerType::getUnqual(*be_state->context), out_val);
       }
       llvm::Function* func = be_state->module->getFunction(routine->source_corresp.name);
       if (!func) {
         llvm::Type* func_ty = nullptr;
         llvm_gen_be_error_t err = get_llvm_type(routine->type, &func_ty);
         if (err != llvm_gen_be_error_t::ok) return err;
         func = llvm::Function::Create(
            llvm::isa<llvm::FunctionType>(func_ty) ? llvm::cast<llvm::FunctionType>(func_ty) : llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false),
            llvm::GlobalValue::ExternalLinkage,
            routine->source_corresp.name,
            be_state->module.get()
         );
       }
       *out_val = func;
       return llvm_gen_be_error_t::ok;
    }
    case enk_variable: {
       llvm::Value* ptr = nullptr;
       llvm_gen_be_error_t err = llvm_lower_lvalue_expression(expr, &ptr);
       if (err != llvm_gen_be_error_t::ok) return err;
       return load_from_lvalue_ptr(ptr, expr->type, out_val);
    }
    case enk_operation: {
       if (expr->variant.operation.kind == eok_comma) {
           llvm::Value* saved_dest = be_state->active_prvalue_dest_ptr;
           llvm::Type* saved_dest_ty = be_state->active_prvalue_dest_ty;
           bool saved_in_place = be_state->prvalue_constructed_in_place;
           be_state->active_prvalue_dest_ptr = nullptr;
           be_state->active_prvalue_dest_ty = nullptr;
           be_state->prvalue_constructed_in_place = false;
           llvm::Value* v1 = nullptr;
           llvm_gen_be_error_t comma_err = llvm_lower_expression(expr->variant.operation.operands, &v1);
           be_state->active_prvalue_dest_ptr = saved_dest;
           be_state->active_prvalue_dest_ty = saved_dest_ty;
           be_state->prvalue_constructed_in_place = saved_in_place;
           LLVM_GEN_BE_CHECK(comma_err);
           return llvm_lower_expression(expr->variant.operation.operands->next, out_val);
       }
       
       switch (expr->variant.operation.kind) {
         case eok_negate:
         case eok_unary_plus:
         case eok_add:
         case eok_subtract:
         case eok_multiply:
         case eok_divide:
         case eok_remainder:
         case eok_padd:
         case eok_psubtract:
         case eok_pdiff:
         case eok_fjadd:
         case eok_jfadd:
         case eok_fjsubtract:
         case eok_jfsubtract:
         case eok_jmultiply:
         case eok_jdivide:
         case eok_xconj:
         case eok_complement:
         case eok_and:
         case eok_or:
         case eok_xor:
         case eok_shiftl:
         case eok_shiftr:
         case eok_gnu_min:
         case eok_gnu_max:
           return llvm_lower_arithmetic_expression(expr, out_val);
           
         case eok_eq:
         case eok_ne:
         case eok_lt:
         case eok_le:
         case eok_gt:
         case eok_ge:
         case eok_vector_eq:
         case eok_vector_ne:
         case eok_vector_lt:
         case eok_vector_le:
         case eok_vector_gt:
         case eok_vector_ge:
         case eok_spaceship:
         case eok_not:
         case eok_vector_not:
         case eok_land:
         case eok_lor:
         case eok_vector_land:
         case eok_vector_lor:
           return llvm_lower_logical_expression(expr, out_val);

         case eok_vector_fill: {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           llvm::Value* sval = nullptr;
           LLVM_GEN_BE_CHECK(llvm_lower_expression(op1, &sval));
           llvm::Type* vec_ty = nullptr;
           LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &vec_ty));
           auto* fv_ty = llvm::dyn_cast<llvm::FixedVectorType>(vec_ty);
           if (!fv_ty) return llvm_gen_be_error_t::unsupported_expr;
           llvm::Type* elem_ty = fv_ty->getElementType();
           if (sval->getType() != elem_ty) {
             if (elem_ty->isIntegerTy() && sval->getType()->isIntegerTy()) {
               bool sgn = op1->type && skip_typerefs(op1->type)->kind == tk_integer &&
                          int_kind_is_signed[skip_typerefs(op1->type)->variant.integer.int_kind];
               sval = be_state->builder->CreateIntCast(sval, elem_ty, sgn);
             } else if (elem_ty->isFloatingPointTy() && sval->getType()->isFloatingPointTy()) {
               sval = be_state->builder->CreateFPCast(sval, elem_ty);
             }
           }
           *out_val = be_state->builder->CreateVectorSplat(fv_ty->getNumElements(), sval, "vec.splat");
           return llvm_gen_be_error_t::ok;
         }
           
         case eok_cast:
         case eok_lvalue_cast:
           return llvm_lower_cast_expression(expr, out_val);

         case eok_pm_base_class_cast:
         case eok_pm_derived_class_cast:
           return llvm_lower_pm_cast(expr, out_val);

         case eok_dynamic_cast:
         case eok_ref_dynamic_cast:
           return llvm_lower_dynamic_cast(expr, out_val);

         case eok_pre_incr:
         case eok_pre_decr:
         case eok_post_incr:
         case eok_post_decr:
         case eok_add_assign:
         case eok_subtract_assign:
         case eok_multiply_assign:
         case eok_divide_assign:
         case eok_remainder_assign:
         case eok_and_assign:
         case eok_or_assign:
         case eok_xor_assign:
         case eok_shiftl_assign:
         case eok_shiftr_assign:
         case eok_padd_assign:
         case eok_psubtract_assign:
         case eok_bassign:
           return llvm_lower_compound_assignment(expr, out_val);
           
         case eok_dot_member_call:
         case eok_points_to_member_call:
         case eok_call:
           return llvm_lower_call_expression(expr, out_val);

         case eok_dot_pm_call:
         case eok_points_to_pm_call:
           return llvm_lower_pm_call(expr, out_val);

         case eok_dot_pm_func_ptr:
         case eok_points_to_pm_func_ptr: {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           llvm::Value* obj_val = nullptr;
           llvm::Value* pm_val = nullptr;
           LLVM_GEN_BE_CHECK(lower_binary_operands(op1, &obj_val, &pm_val));
           if (pm_val->getType()->isStructTy()) {
             llvm::Value* fn_or_off = be_state->builder->CreateExtractValue(pm_val, 0);
             *out_val = be_state->builder->CreateIntToPtr(
                 fn_or_off, llvm::PointerType::getUnqual(*be_state->context));
           } else {
             *out_val = pm_val;
           }
           return llvm_gen_be_error_t::ok;
         }

         case eok_parens:
           return llvm_lower_expression(expr->variant.operation.operands, out_val);
         case eok_lvalue:
         case eok_lvalue_adjust: {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           if (op1 && op1->is_lvalue && expr->type) {
             a_type_ptr ut = skip_typerefs(expr->type);
             if (ut->kind != tk_routine &&
                 !(ut->kind == tk_pointer && ut->variant.pointer.is_reference)) {
               llvm::Value* lptr = nullptr;
               LLVM_GEN_BE_CHECK(llvm_lower_lvalue_expression(op1, &lptr));
               return load_from_lvalue_ptr(lptr, expr->type, out_val);
             }
           }
           return llvm_lower_expression(op1, out_val);
         }

         case eok_pm_field:
         case eok_pm_points_to_field:
           return llvm_lower_pm_field_access(expr, out_val);
           
         case eok_subscript:
         case eok_vector_subscript: {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           llvm::Type* op1_ll_ty = nullptr;
           LLVM_GEN_BE_CHECK(get_llvm_type(op1->type, &op1_ll_ty));
           if (op1_ll_ty->isVectorTy()) {
             llvm::Value* v1 = nullptr;
             llvm::Value* v2 = nullptr;
             LLVM_GEN_BE_CHECK(llvm_lower_expression(op1, &v1));
             if (v1->getType()->isPointerTy()) {
               v1 = be_state->builder->CreateLoad(op1_ll_ty, v1);
             }
             LLVM_GEN_BE_CHECK(llvm_lower_expression(op1->next, &v2));
             *out_val = be_state->builder->CreateExtractElement(v1, v2);
             return llvm_gen_be_error_t::ok;
           }

           llvm::Value* ptr = nullptr;
           llvm_gen_be_error_t err = llvm_lower_lvalue_expression(expr, &ptr);
           if (err != llvm_gen_be_error_t::ok) return err;
           return load_from_lvalue_ptr(ptr, expr->type, out_val);
         }
         case eok_dot_field:
         case eok_points_to_field:
         case eok_ref_indirect:
         case eok_indirect: {
           llvm::Value* ptr = nullptr;
           llvm_gen_be_error_t err = llvm_lower_lvalue_expression(expr, &ptr);
           if (err != llvm_gen_be_error_t::ok) return err;
           an_expr_node_ptr ind_op = (expr->variant.operation.kind == eok_indirect)
                                         ? expr->variant.operation.operands
                                         : nullptr;
           a_type_ptr ind_op_ty = (ind_op && ind_op->type) ? skip_typerefs(ind_op->type) : nullptr;
           if (skip_typerefs(expr->type)->kind == tk_routine ||
               (ind_op_ty && ind_op_ty->kind == tk_pointer &&
                ind_op_ty->variant.pointer.type &&
                skip_typerefs(ind_op_ty->variant.pointer.type)->kind == tk_routine)) {
             *out_val = ptr;
             return llvm_gen_be_error_t::ok;
           }

           a_field_ptr bf_field = nullptr;
           LLVM_GEN_BE_CHECK(extract_bitfield_target(expr, &bf_field));
           if (bf_field) {
             return llvm_emit_bitfield_load(ptr, bf_field, expr->type, out_val);
           }
           return load_from_lvalue_ptr(ptr, expr->type, out_val);
         }

         case eok_dot_static:
         case eok_points_to_static: {
           llvm::Value* v1 = nullptr;
           return lower_binary_operands(expr->variant.operation.operands, &v1, out_val);
         }

         case eok_dot_vacuous_destructor_call:
         case eok_points_to_vacuous_destructor_call: {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           llvm::Value* v1 = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
           if (err != llvm_gen_be_error_t::ok) return err;
           *out_val = nullptr; // Vacuous destructor evaluates to nothing
           return llvm_gen_be_error_t::ok;
         }

         case eok_handle_to:
         case eok_handle_to_box:
         case eok_box:
         case eok_unbox:
         case eok_unbox_lvalue:
         case eok_cli_subscript: {
           // CLI operations are not supported by the LLVM backend.
           return llvm_gen_be_error_t::unsupported_expr;
         }
         case eok_va_start:
         case eok_va_start_single_operand:
         case eok_va_arg:
         case eok_va_end:
         case eok_va_copy: {
           an_expr_operator_kind vk = expr->variant.operation.kind;
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           llvm::Value* va_ptr = nullptr;
           LLVM_GEN_BE_CHECK(resolve_va_list_ptr(op1, &va_ptr));
           llvm::Value* src_ptr = nullptr;
           if (vk == eok_va_copy) {
             LLVM_GEN_BE_CHECK(resolve_va_list_ptr(op1->next, &src_ptr));
           }
           llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
           if (triple.isOSWindows()) {
             if (vk == eok_va_arg) return lower_win64_va_arg(va_ptr, expr->type, out_val);
             if (vk == eok_va_end) return lower_win64_va_end(va_ptr, out_val);
             if (vk == eok_va_copy) return lower_win64_va_copy(va_ptr, src_ptr, out_val);
             return lower_win64_va_start(va_ptr, out_val);
           } else if (triple.isAArch64()) {
             if (vk == eok_va_arg) return lower_aapcs64_va_arg(va_ptr, expr->type, out_val);
             if (vk == eok_va_end) return lower_aapcs64_va_end(va_ptr, out_val);
             if (vk == eok_va_copy) return lower_aapcs64_va_copy(va_ptr, src_ptr, out_val);
             return lower_aapcs64_va_start(va_ptr, out_val);
           } else if (triple.getArch() == llvm::Triple::x86) {
             if (vk == eok_va_arg) return lower_x86_32_va_arg(va_ptr, expr->type, out_val);
             if (vk == eok_va_end) return lower_x86_32_va_end(va_ptr, out_val);
             if (vk == eok_va_copy) return lower_x86_32_va_copy(va_ptr, src_ptr, out_val);
             return lower_x86_32_va_start(va_ptr, out_val);
           } else if (triple.isARM() || triple.isThumb()) {
             if (vk == eok_va_arg) return lower_arm32_va_arg(va_ptr, expr->type, out_val);
             if (vk == eok_va_end) return lower_arm32_va_end(va_ptr, out_val);
             if (vk == eok_va_copy) return lower_arm32_va_copy(va_ptr, src_ptr, out_val);
             return lower_arm32_va_start(va_ptr, out_val);
           } else if (triple.isRISCV()) {
             if (vk == eok_va_arg) return lower_riscv64_va_arg(va_ptr, expr->type, out_val);
             if (vk == eok_va_end) return lower_riscv64_va_end(va_ptr, out_val);
             if (vk == eok_va_copy) return lower_riscv64_va_copy(va_ptr, src_ptr, out_val);
             return lower_riscv64_va_start(va_ptr, out_val);
           } else if (triple.isLoongArch()) {
             if (vk == eok_va_arg) return lower_loongarch64_va_arg(va_ptr, expr->type, out_val);
             if (vk == eok_va_end) return lower_loongarch64_va_end(va_ptr, out_val);
             if (vk == eok_va_copy) return lower_loongarch64_va_copy(va_ptr, src_ptr, out_val);
             return lower_loongarch64_va_start(va_ptr, out_val);
           } else if (triple.isMIPS()) {
             if (vk == eok_va_arg) return lower_mips_va_arg(va_ptr, expr->type, out_val);
             if (vk == eok_va_end) return lower_mips_va_end(va_ptr, out_val);
             if (vk == eok_va_copy) return lower_mips_va_copy(va_ptr, src_ptr, out_val);
             return lower_mips_va_start(va_ptr, out_val);
           } else if (triple.isPPC()) {
             if (vk == eok_va_arg) return lower_ppc64_va_arg(va_ptr, expr->type, out_val);
             if (vk == eok_va_end) return lower_ppc64_va_end(va_ptr, out_val);
             if (vk == eok_va_copy) return lower_ppc64_va_copy(va_ptr, src_ptr, out_val);
             return lower_ppc64_va_start(va_ptr, out_val);
           } else if (triple.isSystemZ()) {
             if (vk == eok_va_arg) return lower_s390x_va_arg(va_ptr, expr->type, out_val);
             if (vk == eok_va_end) return lower_s390x_va_end(va_ptr, out_val);
             if (vk == eok_va_copy) return lower_s390x_va_copy(va_ptr, src_ptr, out_val);
             return lower_s390x_va_start(va_ptr, out_val);
           } else if (triple.isSPARC()) {
             if (vk == eok_va_arg) return lower_sparc_va_arg(va_ptr, expr->type, out_val);
             if (vk == eok_va_end) return lower_sparc_va_end(va_ptr, out_val);
             if (vk == eok_va_copy) return lower_sparc_va_copy(va_ptr, src_ptr, out_val);
             return lower_sparc_va_start(va_ptr, out_val);
           } else if (triple.isWasm()) {
             if (vk == eok_va_arg) return lower_wasm_va_arg(va_ptr, expr->type, out_val);
             if (vk == eok_va_end) return lower_wasm_va_end(va_ptr, out_val);
             if (vk == eok_va_copy) return lower_wasm_va_copy(va_ptr, src_ptr, out_val);
             return lower_wasm_va_start(va_ptr, out_val);
           }
           if (vk == eok_va_arg) return lower_sysv_va_arg(va_ptr, expr->type, out_val);
           if (vk == eok_va_end) return lower_sysv_va_end(va_ptr, out_val);
           if (vk == eok_va_copy) return lower_sysv_va_copy(va_ptr, src_ptr, out_val);
           return lower_sysv_va_start(va_ptr, out_val);
         }
         case eok_real_part:
         case eok_imag_part: {
           bool is_imag = (expr->variant.operation.kind == eok_imag_part);
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           llvm::Value* v1 = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(op1, &v1);
           if (err != llvm_gen_be_error_t::ok) return err;
           if (v1->getType()->isStructTy()) {
               *out_val = be_state->builder->CreateExtractValue(v1, is_imag ? 1 : 0);
           } else {
               *out_val = is_imag ? llvm::Constant::getNullValue(v1->getType()) : v1;
           }
           return llvm_gen_be_error_t::ok;
         }
         case eok_array_to_pointer: {
           return llvm_lower_lvalue_expression(expr->variant.operation.operands, out_val);
         }
         case eok_address_of: {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           if (op1->is_lvalue ||
               (op1->kind == enk_operation &&
                (op1->variant.operation.kind == eok_lvalue_adjust ||
                 op1->variant.operation.kind == eok_question))) {
             return llvm_lower_lvalue_expression(op1, out_val);
           }
           return llvm_lower_expression(op1, out_val);
         }
         case eok_question:
         case eok_vector_question: {
           an_expr_node_ptr cond_expr = expr->variant.operation.operands;
           an_expr_node_ptr true_expr = cond_expr->next;
           an_expr_node_ptr false_expr = true_expr->next;
           
           llvm::Value* cond = nullptr;
           llvm_gen_be_error_t err = llvm_lower_expression(cond_expr, &cond);
           if (err != llvm_gen_be_error_t::ok) return err;

           if (cond->getType()->isVectorTy()) {
               llvm::Value* tv = nullptr;
               llvm::Value* fv = nullptr;
               LLVM_GEN_BE_CHECK(lower_binary_operands(true_expr, &tv, &fv));
               llvm::Value* vcond = be_state->builder->CreateICmpNE(
                   cond, llvm::Constant::getNullValue(cond->getType()));
               *out_val = be_state->builder->CreateSelect(vcond, tv, fv);
               return llvm_gen_be_error_t::ok;
           }
           
           if (!cond->getType()->isIntegerTy(1)) {
               cond = be_state->builder->CreateICmpNE(cond, llvm::Constant::getNullValue(cond->getType()));
           }
           
           llvm::Value* saved_q_dest = be_state->active_prvalue_dest_ptr;
           llvm::Type* saved_q_dest_ty = be_state->active_prvalue_dest_ty;
           bool saved_q_in_place = be_state->prvalue_constructed_in_place;
           be_state->active_prvalue_dest_ptr = nullptr;
           be_state->active_prvalue_dest_ty = nullptr;
           be_state->prvalue_constructed_in_place = false;

           llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
           llvm::BasicBlock* true_bb = llvm::BasicBlock::Create(*be_state->context, "cond.true", func);
           llvm::BasicBlock* false_bb = llvm::BasicBlock::Create(*be_state->context, "cond.false");
           llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*be_state->context, "cond.end");
           
           be_state->builder->CreateCondBr(cond, true_bb, false_bb);
           
           be_state->builder->SetInsertPoint(true_bb);
           llvm::Value* true_val = nullptr;
           err = llvm_lower_expression(true_expr, &true_val);
           if (err != llvm_gen_be_error_t::ok) {
             be_state->active_prvalue_dest_ptr = saved_q_dest;
             be_state->active_prvalue_dest_ty = saved_q_dest_ty;
             be_state->prvalue_constructed_in_place = saved_q_in_place;
             return err;
           }
           llvm::BasicBlock* true_end_bb = be_state->builder->GetInsertBlock();
           bool true_unreachable =
               (true_end_bb != true_bb && true_end_bb->hasNPredecessors(0));
           bool true_terminated = (EDG_LLVM_GET_TERMINATOR(true_end_bb) != nullptr);
           if (!true_terminated) {
             if (true_unreachable) {
               be_state->builder->CreateUnreachable();
               true_terminated = true;
             } else {
               be_state->builder->CreateBr(end_bb);
             }
           }
           
           func->insert(func->end(), false_bb);
           be_state->builder->SetInsertPoint(false_bb);
           llvm::Value* false_val = nullptr;
           err = llvm_lower_expression(false_expr, &false_val);
           be_state->active_prvalue_dest_ptr = saved_q_dest;
           be_state->active_prvalue_dest_ty = saved_q_dest_ty;
           be_state->prvalue_constructed_in_place = saved_q_in_place;
           if (err != llvm_gen_be_error_t::ok) return err;
           llvm::BasicBlock* false_end_bb = be_state->builder->GetInsertBlock();
           bool false_unreachable =
               (false_end_bb != false_bb && false_end_bb->hasNPredecessors(0));
           bool false_terminated = (EDG_LLVM_GET_TERMINATOR(false_end_bb) != nullptr);
           if (!false_terminated) {
             if (false_unreachable) {
               be_state->builder->CreateUnreachable();
               false_terminated = true;
             } else {
               be_state->builder->CreateBr(end_bb);
             }
           }
           
           func->insert(func->end(), end_bb);
           be_state->builder->SetInsertPoint(end_bb);
           
           a_type_ptr q_ty = expr->type ? skip_typerefs(expr->type) : nullptr;
           if (q_ty && q_ty->kind != tk_void && (!true_terminated || !false_terminated)) {
             llvm::Type* expr_llvm_ty = nullptr;
             err = get_llvm_type(expr->type, &expr_llvm_ty);
             if (err != llvm_gen_be_error_t::ok) return err;
             llvm::Type* phi_ty = expr_llvm_ty->isSized()
                 ? expr_llvm_ty
                 : llvm::PointerType::getUnqual(*be_state->context);
             if (!true_terminated && true_val && true_val->getType()->isPointerTy()) {
               phi_ty = true_val->getType();
             } else if (!false_terminated && false_val && false_val->getType()->isPointerTy()) {
               phi_ty = false_val->getType();
             }
             auto coerce_phi_arm = [&](llvm::Value* v, llvm::BasicBlock* arm_bb) noexcept -> llvm::Value* {
               if (!v) return llvm::PoisonValue::get(phi_ty);
               if (v->getType() == phi_ty) return v;
               be_state->builder->SetInsertPoint(EDG_LLVM_GET_TERMINATOR(arm_bb));
               if (v->getType()->isIntOrIntVectorTy() && phi_ty->isIntOrIntVectorTy()) {
                 return be_state->builder->CreateZExtOrTrunc(v, phi_ty);
               }
               if (v->getType()->isIntegerTy() && phi_ty->isPointerTy()) {
                 return be_state->builder->CreateIntToPtr(v, phi_ty);
               }
               if (v->getType()->isFloatingPointTy() && phi_ty->isFloatingPointTy()) {
                 return be_state->builder->CreateFPCast(v, phi_ty);
               }
               if (!v->getType()->isSized()) {
                 return llvm::PoisonValue::get(phi_ty);
               }
               llvm::IRBuilder<> eb(&func->getEntryBlock(), func->getEntryBlock().begin());
               const llvm::DataLayout& dl = be_state->module->getDataLayout();
               uint64_t need_sz = std::max(dl.getTypeAllocSize(v->getType()), dl.getTypeAllocSize(phi_ty));
               llvm::AllocaInst* conv_slot = eb.CreateAlloca(
                   llvm::ArrayType::get(llvm::Type::getInt8Ty(*be_state->context), std::max<uint64_t>(1, need_sz)),
                   nullptr, "cond.phi.cast");
               be_state->builder->CreateMemSet(conv_slot, be_state->builder->getInt8(0), need_sz, llvm::MaybeAlign());
               be_state->builder->CreateStore(v, conv_slot);
               return be_state->builder->CreateLoad(phi_ty, conv_slot);
             };
             if (!true_terminated) {
               true_val = coerce_phi_arm(true_val, true_end_bb);
             }
             if (!false_terminated) {
               false_val = coerce_phi_arm(false_val, false_end_bb);
             }
             be_state->builder->SetInsertPoint(end_bb);
             llvm::PHINode* phi = be_state->builder->CreatePHI(phi_ty, 2, "cond.phi");
             if (!true_terminated) {
               phi->addIncoming(true_val, true_end_bb);
             }
             if (!false_terminated) {
               phi->addIncoming(false_val, false_end_bb);
             }
             *out_val = phi;
           } else {
             *out_val = nullptr;
           }
           return llvm_gen_be_error_t::ok;
         }
         case eok_assign: {
           an_expr_node_ptr op1 = expr->variant.operation.operands;
           if (op1->kind == enk_operation &&
               op1->variant.operation.kind == eok_vector_subscript) {
             an_expr_node_ptr sub_base = op1->variant.operation.operands;
             llvm::Value* vec_ptr = nullptr;
             LLVM_GEN_BE_CHECK(llvm_lower_lvalue_expression(sub_base, &vec_ptr));
             llvm::Value* idx_val = nullptr;
             LLVM_GEN_BE_CHECK(llvm_lower_expression(sub_base->next, &idx_val));
             llvm::Value* rhs_val = nullptr;
             LLVM_GEN_BE_CHECK(llvm_lower_expression(op1->next, &rhs_val));
             llvm::Type* vec_ty = nullptr;
             LLVM_GEN_BE_CHECK(get_llvm_type(sub_base->type, &vec_ty));
             llvm::Value* old_vec = be_state->builder->CreateLoad(vec_ty, vec_ptr);
             llvm::Value* new_vec = be_state->builder->CreateInsertElement(old_vec, rhs_val, idx_val);
             be_state->builder->CreateStore(new_vec, vec_ptr);
             *out_val = rhs_val;
             return llvm_gen_be_error_t::ok;
           }
           llvm::Value* v1 = nullptr;
           llvm::Value* v2 = nullptr;
           LLVM_GEN_BE_CHECK(llvm_lower_expression(op1, &v1));
           bool constructed_in_place = false;
           if (op1->next) {
             llvm::Value* prev_dest = be_state->active_prvalue_dest_ptr;
             llvm::Type* prev_dest_ty = be_state->active_prvalue_dest_ty;
             bool prev_in_place = be_state->prvalue_constructed_in_place;
             if (expr->type && is_class_struct_union_type(skip_typerefs(expr->type))) {
               llvm::Type* dty = nullptr;
               LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &dty));
               be_state->active_prvalue_dest_ptr = v1;
               be_state->active_prvalue_dest_ty = dty;
               be_state->prvalue_constructed_in_place = false;
             }
             llvm_gen_be_error_t r_err = llvm_lower_expression(op1->next, &v2);
             constructed_in_place = be_state->prvalue_constructed_in_place;
             be_state->active_prvalue_dest_ptr = prev_dest;
             be_state->active_prvalue_dest_ty = prev_dest_ty;
             be_state->prvalue_constructed_in_place = prev_in_place;
             LLVM_GEN_BE_CHECK(r_err);
           }

           if (v2 && !constructed_in_place && !v2->getType()->isVoidTy()) {
             a_field_ptr bf_field = nullptr;
             LLVM_GEN_BE_CHECK(extract_bitfield_target(op1, &bf_field));
             if (bf_field) {
                 LLVM_GEN_BE_CHECK(llvm_emit_bitfield_store(v1, v2, bf_field, op1->type));
             } else {
                 llvm::Type* ty = nullptr;
                 LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &ty));
                 if (v2->getType()->isPointerTy() && !ty->isPointerTy()) {
                     v2 = be_state->builder->CreateLoad(ty, v2);
                 }
                 a_type_qualifier_set lhs_quals = f_get_type_qualifiers(op1->type, TRUE);
                 if ((lhs_quals & TQ_C11_ATOMIC) != 0) {
                     uint64_t size = be_state->module->getDataLayout().getTypeStoreSize(ty);
                     if ((ty->isIntegerTy() || ty->isPointerTy()) &&
                         llvm::isPowerOf2_64(size) && size <= 16) {
                         llvm::StoreInst* st = be_state->builder->CreateStore(v2, v1);
                         st->setAtomic(llvm::AtomicOrdering::SequentiallyConsistent);
                         st->setAlignment(llvm::Align(size));
                         if ((lhs_quals & TQ_VOLATILE) != 0) st->setVolatile(true);
                     } else {
                         llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
                         llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
                         llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
                         llvm::AllocaInst* tmp = be_state->builder->CreateAlloca(ty, nullptr, "atomic.store.tmp");
                         be_state->builder->CreateStore(v2, tmp);
                         llvm::FunctionType* fty = llvm::FunctionType::get(
                             llvm::Type::getVoidTy(*be_state->context), {i64_ty, ptr_ty, ptr_ty, i32_ty}, false);
                         llvm::FunctionCallee fn = be_state->module->getOrInsertFunction("__atomic_store", fty);
                         be_state->builder->CreateCall(
                             fn, {llvm::ConstantInt::get(i64_ty, size), v1, tmp, llvm::ConstantInt::get(i32_ty, 5)});
                     }
                 } else if (be_state->module->getDataLayout().getTypeStoreSize(ty) > 16) {
                     if (llvm::LoadInst* li = llvm::dyn_cast<llvm::LoadInst>(v2)) {
                         llvm::Value* src_ptr = li->getPointerOperand();
                         uint64_t size = be_state->module->getDataLayout().getTypeStoreSize(ty);
                         llvm::Align align = be_state->module->getDataLayout().getABITypeAlign(ty);
                         llvm::CallInst* mc = be_state->builder->CreateMemCpy(v1, align, src_ptr, align, size);
                         if (be_state->opt_level != llvm_opt_level_t::O0) {
                             llvm::MDNode* tbaa_st = nullptr;
                             LLVM_GEN_BE_CHECK(llvm_get_tbaa_struct_node(expr->type, &tbaa_st));
                             mc->setMetadata(llvm::LLVMContext::MD_tbaa_struct, tbaa_st);
                         }
                     } else {
                         be_state->builder->CreateStore(v2, v1);
                     }
                 } else {
                     llvm::StoreInst* st = be_state->builder->CreateStore(v2, v1);
                     if (be_state->opt_level != llvm_opt_level_t::O0 &&
                         (ty->isIntegerTy() || ty->isFloatingPointTy() || ty->isPointerTy())) {
                         llvm::MDNode* tbaa_tag = nullptr;
                         LLVM_GEN_BE_CHECK(llvm_get_tbaa_access_tag(expr->type, expr->type, 0, &tbaa_tag));
                         st->setMetadata(llvm::LLVMContext::MD_tbaa, tbaa_tag);
                     }
                 }
             }

             if (be_state->dbg_state && op1->kind == enk_variable) {
                 a_variable_ptr var = op1->variant.variable.ptr;
                 LLVM_GEN_BE_CHECK(emit_dbg_value_for_variable(
                     be_state->dbg_state, var, v2, nullptr, be_state->builder->GetInsertBlock()));
             }

             *out_val = v2;
             return llvm_gen_be_error_t::ok;
           }
           if (constructed_in_place || v2 != nullptr) {
             *out_val = v1;
             return llvm_gen_be_error_t::ok;
           }
           return llvm_gen_be_set_error(
               &be_state->err_context, llvm_gen_be_error_t::unsupported_expr,
               __FILE__, __LINE__, 0, "Unsupported bassign rhs kind %d",
               op1->next ? static_cast<int>(op1->next->kind) : -1);
         }
         default:
           return llvm_gen_be_set_error(
               &be_state->err_context, llvm_gen_be_error_t::unsupported_expr,
               __FILE__, __LINE__, 0, "Unsupported operation kind %d",
               static_cast<int>(expr->variant.operation.kind));
       }
    }
    
    case enk_await:
    case enk_yield:
      return llvm_lower_coro_suspend(expr, out_val);

    case enk_builtin_operation:
      return llvm_lower_builtin_operation(expr, out_val);

    case enk_builtin_choose_expr:
      return llvm_lower_builtin_choose_expr(expr, out_val);

    case enk_condition: {
      a_condition_supplement_ptr cond_supp = expr->variant.condition;
      if (!cond_supp) return llvm_gen_be_error_t::invalid_argument;
      if (cond_supp->initialization) {
        LLVM_GEN_BE_CHECK(llvm_lower_statement(cond_supp->initialization));
      }
      if (cond_supp->dynamic_init) {
        a_variable_ptr cvar = cond_supp->dynamic_init->variable;
        llvm::Type* cvar_ty = nullptr;
        LLVM_GEN_BE_CHECK(get_llvm_type(cvar->type, &cvar_ty));
        llvm::Value* cvar_ptr = nullptr;
        auto it = be_state->local_vars.find(cvar);
        if (it != be_state->local_vars.end()) {
          cvar_ptr = it->second;
        } else {
          llvm::Function* parent_func = be_state->builder->GetInsertBlock()->getParent();
          llvm::IRBuilder<> entry_b(&parent_func->getEntryBlock(), parent_func->getEntryBlock().begin());
          cvar_ptr = entry_b.CreateAlloca(cvar_ty, nullptr, "cond.var");
          be_state->local_vars[cvar] = cvar_ptr;
        }
        LLVM_GEN_BE_CHECK(llvm_emit_dynamic_init(cond_supp->dynamic_init, cvar_ptr, cvar_ty));
      }
      return llvm_lower_expression(cond_supp->expr, out_val);
    }

    case enk_field: {
      if (!expr->variant.field.ptr) return llvm_gen_be_error_t::invalid_argument;
      *out_val = llvm::ConstantInt::get(
          llvm::Type::getInt64Ty(*be_state->context), static_cast<uint64_t>(expr->variant.field.ptr->offset));
      return llvm_gen_be_error_t::ok;
    }

    case enk_type_operand: {
      *out_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0);
      return llvm_gen_be_error_t::ok;
    }

    case enk_sizeof:
    case enk_alignof:
    case enk_datasizeof: {
      llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
      a_type_ptr target_ty = expr->variant.sizeof_info.is_type
                                 ? expr->variant.sizeof_info.variant.type
                                 : (expr->variant.sizeof_info.variant.expr
                                        ? expr->variant.sizeof_info.variant.expr->type
                                        : nullptr);
      if (expr->kind == enk_alignof) {
        uint64_t al = target_ty ? EDG_ALIGN_OF_TYPE(target_ty) : 1;
        *out_val = llvm::ConstantInt::get(i64_ty, al);
        return llvm_gen_be_error_t::ok;
      }
      if (target_ty) {
        a_type_ptr ut = skip_typerefs(target_ty);
        if (be_state->array_to_vla_dim.count(ut)) {
          llvm::Value* total_sz = llvm::ConstantInt::get(i64_ty, 1);
          a_type_ptr cur = ut;
          while (cur->kind == tk_array) {
            if (be_state->array_to_vla_dim.count(cur)) {
              a_vla_dimension_ptr dim = be_state->array_to_vla_dim[cur];
              if (be_state->vla_dimension_values.count(dim)) {
                llvm::Value* dval = be_state->builder->CreateZExtOrTrunc(
                    be_state->vla_dimension_values[dim], i64_ty);
                total_sz = be_state->builder->CreateMul(total_sz, dval);
              }
            } else if (cur->variant.array.variant.number_of_elements > 0) {
              total_sz = be_state->builder->CreateMul(
                  total_sz, llvm::ConstantInt::get(i64_ty, static_cast<uint64_t>(cur->variant.array.variant.number_of_elements)));
            }
            cur = skip_typerefs(cur->variant.array.element_type);
          }
          if (cur->size > 1) {
            total_sz = be_state->builder->CreateMul(total_sz, llvm::ConstantInt::get(i64_ty, static_cast<uint64_t>(cur->size)));
          }
          *out_val = total_sz;
          return llvm_gen_be_error_t::ok;
        }
        *out_val = llvm::ConstantInt::get(i64_ty, static_cast<uint64_t>(ut->size));
        return llvm_gen_be_error_t::ok;
      }
      *out_val = llvm::ConstantInt::get(i64_ty, 0);
      return llvm_gen_be_error_t::ok;
    }

    case enk_sizeof_pack:
      return llvm_lower_sizeof_pack(expr, out_val);

    case enk_initializer: {
      a_dynamic_init_ptr dip = expr->variant.initializer.dyn_init;
      if (!dip) return llvm_gen_be_error_t::invalid_argument;
      llvm::Type* expr_ty = nullptr;
      LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &expr_ty));
      if (be_state->active_prvalue_dest_ptr != nullptr &&
          be_state->active_prvalue_dest_ty == expr_ty) {
        llvm::Value* direct_dest = be_state->active_prvalue_dest_ptr;
        be_state->active_prvalue_dest_ptr = nullptr;
        be_state->active_prvalue_dest_ty = nullptr;
        LLVM_GEN_BE_CHECK(llvm_emit_dynamic_init(dip, direct_dest, expr_ty));
        be_state->prvalue_constructed_in_place = true;
        *out_val = nullptr;
        return llvm_gen_be_error_t::ok;
      }
      llvm::AllocaInst* tmp_alloca = be_state->builder->CreateAlloca(expr_ty, nullptr, "init.tmp");
      LLVM_GEN_BE_CHECK(llvm_emit_dynamic_init(dip, tmp_alloca, expr_ty));
      *out_val = be_state->builder->CreateLoad(expr_ty, tmp_alloca);
      return llvm_gen_be_error_t::ok;
    }

    case enk_statement: {
      llvm::Type* res_ty = nullptr;
      llvm_gen_be_error_t err_ty = get_llvm_type(expr->type, &res_ty);
      if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
      
      llvm::Value* prev_ptr = be_state->current_stmt_expr_result_ptr;
      llvm::Value* result_ptr = nullptr;
      bool direct_in_place = false;
      
      if (!res_ty->isVoidTy()) {
          if (be_state->active_prvalue_dest_ptr != nullptr &&
              be_state->active_prvalue_dest_ty == res_ty) {
              result_ptr = be_state->active_prvalue_dest_ptr;
              be_state->active_prvalue_dest_ptr = nullptr;
              be_state->active_prvalue_dest_ty = nullptr;
              direct_in_place = true;
          } else {
              result_ptr = be_state->builder->CreateAlloca(res_ty, nullptr, "stmt_expr_res");
          }
      }
      
      be_state->current_stmt_expr_result_ptr = result_ptr;
      be_state->stmt_expr_cleanup_depths.push_back(be_state->cleanup_stack.cleanups.size());
      llvm_gen_be_error_t err = llvm_lower_statement(expr->variant.statement);
      be_state->stmt_expr_cleanup_depths.pop_back();
      be_state->current_stmt_expr_result_ptr = prev_ptr;
      
      if (err != llvm_gen_be_error_t::ok) return err;
      
      if (direct_in_place) {
          be_state->prvalue_constructed_in_place = true;
          *out_val = nullptr;
      } else if (result_ptr) {
          *out_val = be_state->builder->CreateLoad(res_ty, result_ptr);
      } else {
          *out_val = nullptr;
      }
      return llvm_gen_be_error_t::ok;
    }

    case enk_throw: {
       a_throw_supplement_ptr throw_info = expr->variant.throw_info;
       llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
       llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
       bool is_msvc = triple.isWindowsMSVCEnvironment();

       if (!throw_info) {
         // Rethrow
         llvm::FunctionCallee rethrow_fn;
         std::vector<llvm::Value*> rethrow_args;
         if (is_msvc) {
             rethrow_fn = be_state->module->getOrInsertFunction("_CxxThrowException",
                 llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty, int8_ptr_ty}, false));
             llvm::Constant* null_p = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty));
             rethrow_args = {null_p, null_p};
         } else {
             rethrow_fn = be_state->module->getOrInsertFunction("__cxa_rethrow",
                 llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false));
         }
         llvm::BasicBlock* lpad_bb = nullptr;
         LLVM_GEN_BE_CHECK(llvm_get_or_create_current_unwind_dest(&lpad_bb));
         llvm::Function* rfunc = be_state->builder->GetInsertBlock()->getParent();
         if (lpad_bb) {
             llvm::BasicBlock* normal_bb = llvm::BasicBlock::Create(*be_state->context, "invoke.cont", rfunc);
             be_state->builder->CreateInvoke(rethrow_fn, normal_bb, lpad_bb, rethrow_args);
             be_state->builder->SetInsertPoint(normal_bb);
         } else {
             be_state->builder->CreateCall(rethrow_fn, rethrow_args);
         }
         be_state->builder->CreateUnreachable();
         be_state->builder->SetInsertPoint(
             llvm::BasicBlock::Create(*be_state->context, "rethrow.after", rfunc));
         *out_val = nullptr;
         return llvm_gen_be_error_t::ok;
       }

       llvm::Type* size_t_ty = llvm::Type::getInt64Ty(*be_state->context);
       llvm::FunctionCallee alloc_fn = be_state->module->getOrInsertFunction(
           is_msvc ? "malloc" : "__cxa_allocate_exception",
           llvm::FunctionType::get(int8_ptr_ty, {size_t_ty}, false));
       uint32_t type_size = static_cast<uint32_t>(throw_info->type->size);
       if (type_size == 0) type_size = 1;
       llvm::Value* size_val = llvm::ConstantInt::get(size_t_ty, type_size);
       llvm::Value* exc_mem = be_state->builder->CreateCall(alloc_fn, {size_val});

       llvm::Constant* dtor_const = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty));
       if (throw_info->dynamic_init) {
           llvm::Type* exc_ty = nullptr;
           llvm_gen_be_error_t err_ty = get_llvm_type(throw_info->type, &exc_ty);
           if (err_ty != llvm_gen_be_error_t::ok) return err_ty;
           llvm_gen_be_error_t err = llvm_emit_dynamic_init(throw_info->dynamic_init, exc_mem, exc_ty);
           if (err != llvm_gen_be_error_t::ok) return err;
           llvm::Function* dtor_fn = nullptr;
           LLVM_GEN_BE_CHECK(get_or_create_void_ptr_fn(throw_info->dynamic_init->destructor, &dtor_fn));
           if (dtor_fn) dtor_const = dtor_fn;
       } else {
           if (throw_info->expr) {
               llvm::Value* prev_thrown_ptr = be_state->active_thrown_object_ptr;
               be_state->active_thrown_object_ptr = exc_mem;
               llvm::Value* init_res = nullptr;
               llvm_gen_be_error_t init_err = llvm_lower_expression(throw_info->expr, &init_res);
               be_state->active_thrown_object_ptr = prev_thrown_ptr;
               if (init_err != llvm_gen_be_error_t::ok) return init_err;
           }
           if (throw_info->destructor) {
               llvm::Function* dtor_fn = nullptr;
               LLVM_GEN_BE_CHECK(get_or_create_void_ptr_fn(throw_info->destructor, &dtor_fn));
               dtor_const = dtor_fn;
           }
       }

       llvm::Constant* type_desc_const = nullptr;
       LLVM_GEN_BE_CHECK(get_typeinfo_global(throw_info->type, &type_desc_const));

       llvm::FunctionCallee throw_fn;
       std::vector<llvm::Value*> throw_args;
       if (is_msvc) {
           throw_fn = be_state->module->getOrInsertFunction("_CxxThrowException",
               llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty, int8_ptr_ty}, false));
           llvm::Constant* ti_gv = nullptr;
           LLVM_GEN_BE_CHECK(llvm_get_or_create_msvc_throw_info(throw_info->type, dtor_const, &ti_gv));
           throw_args = {exc_mem, ti_gv};
       } else {
           throw_fn = be_state->module->getOrInsertFunction("__cxa_throw",
               llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), {int8_ptr_ty, int8_ptr_ty, int8_ptr_ty}, false));
           throw_args = {exc_mem, type_desc_const, dtor_const};
       }

       llvm::BasicBlock* lpad_bb = nullptr;
       LLVM_GEN_BE_CHECK(llvm_get_or_create_current_unwind_dest(&lpad_bb));
       llvm::Function* tfunc = be_state->builder->GetInsertBlock()->getParent();
       if (lpad_bb) {
           llvm::BasicBlock* normal_bb = llvm::BasicBlock::Create(*be_state->context, "invoke.cont", tfunc);
           be_state->builder->CreateInvoke(throw_fn, normal_bb, lpad_bb, throw_args);
           be_state->builder->SetInsertPoint(normal_bb);
       } else {
           be_state->builder->CreateCall(throw_fn, throw_args);
       }
       be_state->builder->CreateUnreachable();
       be_state->builder->SetInsertPoint(
           llvm::BasicBlock::Create(*be_state->context, "throw.after", tfunc));
       *out_val = nullptr;
       return llvm_gen_be_error_t::ok;
    }
    case enk_temp_init:
    case enk_lambda: {
       llvm::Value* temp_storage = nullptr;
       LLVM_GEN_BE_CHECK(llvm_lower_lvalue_expression(expr, &temp_storage));
       if (expr->is_lvalue) {
           *out_val = temp_storage;
       } else {
           llvm::Type* expr_llvm_ty = nullptr;
           LLVM_GEN_BE_CHECK(get_llvm_type(expr->type, &expr_llvm_ty));
           *out_val = be_state->builder->CreateLoad(expr_llvm_ty, temp_storage);
       }
       return llvm_gen_be_error_t::ok;
    }
    case enk_object_lifetime: {
       llvm::Value* result_val = nullptr;
       an_expr_node_ptr inner_expr = expr->variant.object_lifetime.expr;
       
       size_t initial_cleanups = be_state->cleanup_stack.cleanups.size();
       
       if (inner_expr) {
           llvm_gen_be_error_t err = llvm_lower_expression(inner_expr, &result_val);
           if (err != llvm_gen_be_error_t::ok) return err;
       }
       
#if !DO_IL_LOWERING
       an_object_lifetime_ptr lifetime = expr->variant.object_lifetime.ptr;
       if (lifetime && !EDG_LLVM_GET_TERMINATOR(be_state->builder->GetInsertBlock())) {
           for (a_dynamic_init_ptr dtor_init = lifetime->destructions; dtor_init; dtor_init = dtor_init->next_in_destruction_list) {
               auto it = be_state->reused_values.find(dtor_init);
               llvm::Value* obj_ptr = (it != be_state->reused_values.end())
                                          ? it->second
                                          : (dtor_init->variable ? be_state->local_vars[dtor_init->variable] : nullptr);
               if (obj_ptr) {
                   llvm::Function* dtor_func = nullptr;
                   LLVM_GEN_BE_CHECK(get_or_create_void_ptr_fn(dtor_init->destructor, &dtor_func));
                   if (dtor_func) {
                       be_state->builder->CreateCall(dtor_func, {obj_ptr});
                   }
               }
           }
       }
#endif
       
       while (be_state->cleanup_stack.cleanups.size() > initial_cleanups) {
           ScopeCleanup cleanup = be_state->cleanup_stack.cleanups.back();
           be_state->cleanup_stack.cleanups.pop_back();
           if (!EDG_LLVM_GET_TERMINATOR(be_state->builder->GetInsertBlock())) {
               llvm::Function* dtor_func = nullptr;
               LLVM_GEN_BE_CHECK(get_or_create_void_ptr_fn(cleanup.dtor_routine, &dtor_func));
               be_state->builder->CreateCall(dtor_func, {cleanup.object_ptr});
           }
       }
       
       *out_val = result_val;
       return llvm_gen_be_error_t::ok;
    }
    case enk_typeid:
      return llvm_lower_typeid(expr, out_val);
    case enk_new_delete: {
       a_new_delete_supplement_ptr nd = expr->variant.new_delete;
       if (!nd) return llvm_gen_be_error_t::invalid_argument;
       if (nd->is_new) {
         return llvm_lower_new_expression(expr, out_val);
       }
       return llvm_lower_delete_expression(expr, out_val);
       }
       default:
       return llvm_gen_be_set_error(
          &be_state->err_context, llvm_gen_be_error_t::unsupported_expr,
          __FILE__, __LINE__, 0, "Unsupported expr kind %d",
          static_cast<int>(expr->kind));
       }
       }

/**
 * @brief Lowers a compound assignment (`+=`, `-=`, `*=`, etc.) or increment/decrement (`++`, `--`) expression.
 * @details Evaluates the target lvalue address once, loads the current value (supporting volatile and bitfields),
 *          computes the updated value, stores it back, and returns either the pre- or post-update value.
 * @param[in] expr Pointer to the EDG compound assignment or increment/decrement expression node.
 * @param[out] out_val Receives the resulting `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_compound_assignment(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!out_val) return llvm_gen_be_error_t::invalid_argument;
  *out_val = nullptr;

  if (expr->kind != enk_operation) return llvm_gen_be_error_t::invalid_argument;
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());

  an_expr_node_ptr op1 = expr->variant.operation.operands;
  an_expr_node_ptr op2 = op1->next;

  llvm::Value* lhs_ptr = nullptr;
  llvm::Value* vec_elem_idx = nullptr;
  llvm::Type* vec_container_ty = nullptr;
  llvm::Value* loaded_val = nullptr;
  a_field_ptr bitfield_ptr = nullptr;
  llvm_gen_be_error_t err = llvm_gen_be_error_t::ok;

  if (op1->kind == enk_operation &&
      op1->variant.operation.kind == eok_vector_subscript) {
      an_expr_node_ptr vbase = op1->variant.operation.operands;
      LLVM_GEN_BE_CHECK(llvm_lower_lvalue_expression(vbase, &lhs_ptr));
      LLVM_GEN_BE_CHECK(llvm_lower_expression(vbase->next, &vec_elem_idx));
      LLVM_GEN_BE_CHECK(get_llvm_type(vbase->type, &vec_container_ty));
      llvm::Value* full_vec = be_state->builder->CreateLoad(vec_container_ty, lhs_ptr);
      loaded_val = be_state->builder->CreateExtractElement(full_vec, vec_elem_idx);
  } else {
      err = llvm_lower_lvalue_expression(op1, &lhs_ptr);
      if (err != llvm_gen_be_error_t::ok) return err;

      LLVM_GEN_BE_CHECK(extract_bitfield_target(op1, &bitfield_ptr));

      if (bitfield_ptr) {
          err = llvm_emit_bitfield_load(lhs_ptr, bitfield_ptr, op1->type, &loaded_val);
      } else {
          err = load_from_lvalue_ptr(lhs_ptr, op1->type, &loaded_val);
      }
      if (err != llvm_gen_be_error_t::ok) return err;
  }

  llvm::Value* rhs_val = nullptr;
  if (op2) {
      err = llvm_lower_expression(op2, &rhs_val);
      if (err != llvm_gen_be_error_t::ok) return err;
  } else if (loaded_val->getType()->isPointerTy()) {
      rhs_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), 1);
  } else if (loaded_val->getType()->isFloatingPointTy()) {
      rhs_val = llvm::ConstantFP::get(loaded_val->getType(), 1.0);
  } else {
      // pre/post incr/decr implicitly 1
      rhs_val = llvm::ConstantInt::get(loaded_val->getType(), 1);
  }

  a_type_qualifier_set lhs_quals = f_get_type_qualifiers(op1->type, TRUE);
  if ((lhs_quals & TQ_C11_ATOMIC) != 0) {
    an_expr_operator_kind k = expr->variant.operation.kind;
    bool is_fp = loaded_val->getType()->isFloatingPointTy();
    bool is_ptr_lhs = loaded_val->getType()->isPointerTy();
    if (is_ptr_lhs && (k == eok_pre_incr || k == eok_post_incr || k == eok_add_assign ||
                       k == eok_padd_assign || k == eok_pre_decr || k == eok_post_decr ||
                       k == eok_subtract_assign || k == eok_psubtract_assign)) {
      bool is_sub = (k == eok_pre_decr || k == eok_post_decr ||
                     k == eok_subtract_assign || k == eok_psubtract_assign);
      bool rhs_signed = true;
      if (op2 && op2->type) {
        a_type_ptr r_ty = skip_typerefs(op2->type);
        if (r_ty->kind == tk_integer) {
          rhs_signed = (int_kind_is_signed[r_ty->variant.integer.int_kind] != FALSE);
        }
      }
      llvm::Type* intptr_ty = be_state->module->getDataLayout().getIntPtrType(*be_state->context);
      llvm::Value* delta = be_state->builder->CreateIntCast(rhs_val, intptr_ty, rhs_signed);
      a_type_ptr raw_p_ty = skip_typerefs(op1->type);
      if (raw_p_ty->kind == tk_pointer) {
        a_type_ptr ptee_ty = raw_p_ty->variant.pointer.type
                                 ? skip_typerefs(raw_p_ty->variant.pointer.type)
                                 : nullptr;
        uint64_t scale = (ptee_ty && ptee_ty->size > 0) ? static_cast<uint64_t>(ptee_ty->size) : 1ULL;
        if (scale > 1ULL) {
          delta = be_state->builder->CreateMul(delta, llvm::ConstantInt::get(intptr_ty, scale));
        }
      }
      llvm::AtomicRMWInst* old_i = be_state->builder->CreateAtomicRMW(
          is_sub ? llvm::AtomicRMWInst::Sub : llvm::AtomicRMWInst::Add,
          lhs_ptr, delta, llvm::MaybeAlign(), llvm::AtomicOrdering::SequentiallyConsistent);
      llvm::Value* old_p = be_state->builder->CreateIntToPtr(old_i, loaded_val->getType());
      if (k == eok_post_incr || k == eok_post_decr) {
        *out_val = old_p;
      } else {
        llvm::Value* signed_off = is_sub ? be_state->builder->CreateNeg(delta) : delta;
        *out_val = be_state->builder->CreateInBoundsGEP(
            llvm::Type::getInt8Ty(*be_state->context), old_p, signed_off);
      }
      return llvm_gen_be_error_t::ok;
    }
    bool has_atomic_rmw = false;
    llvm::AtomicRMWInst::BinOp rmw_op = llvm::AtomicRMWInst::Add;
    if (k == eok_pre_incr || k == eok_post_incr || k == eok_add_assign) {
      rmw_op = is_fp ? llvm::AtomicRMWInst::FAdd : llvm::AtomicRMWInst::Add;
      has_atomic_rmw = true;
    } else if (k == eok_pre_decr || k == eok_post_decr || k == eok_subtract_assign) {
      rmw_op = is_fp ? llvm::AtomicRMWInst::FSub : llvm::AtomicRMWInst::Sub;
      has_atomic_rmw = true;
    } else if (k == eok_and_assign) {
      rmw_op = llvm::AtomicRMWInst::And;
      has_atomic_rmw = true;
    } else if (k == eok_or_assign) {
      rmw_op = llvm::AtomicRMWInst::Or;
      has_atomic_rmw = true;
    } else if (k == eok_xor_assign) {
      rmw_op = llvm::AtomicRMWInst::Xor;
      has_atomic_rmw = true;
    }
    if (has_atomic_rmw) {
      llvm::AtomicRMWInst* old_v = be_state->builder->CreateAtomicRMW(
          rmw_op, lhs_ptr, rhs_val, llvm::MaybeAlign(), llvm::AtomicOrdering::SequentiallyConsistent);
      if (k == eok_post_incr || k == eok_post_decr) {
        *out_val = old_v;
      } else if (rmw_op == llvm::AtomicRMWInst::Add) {
        *out_val = be_state->builder->CreateAdd(old_v, rhs_val);
      } else if (rmw_op == llvm::AtomicRMWInst::FAdd) {
        *out_val = be_state->builder->CreateFAdd(old_v, rhs_val);
      } else if (rmw_op == llvm::AtomicRMWInst::Sub) {
        *out_val = be_state->builder->CreateSub(old_v, rhs_val);
      } else if (rmw_op == llvm::AtomicRMWInst::FSub) {
        *out_val = be_state->builder->CreateFSub(old_v, rhs_val);
      } else if (rmw_op == llvm::AtomicRMWInst::And) {
        *out_val = be_state->builder->CreateAnd(old_v, rhs_val);
      } else if (rmw_op == llvm::AtomicRMWInst::Or) {
        *out_val = be_state->builder->CreateOr(old_v, rhs_val);
      } else {
        *out_val = be_state->builder->CreateXor(old_v, rhs_val);
      }
      return llvm_gen_be_error_t::ok;
    }
  }

  llvm::Value* result_val = nullptr;
  bool is_complex = loaded_val->getType()->isStructTy();
  bool is_float = loaded_val->getType()->isFPOrFPVectorTy();
  bool is_ptr = loaded_val->getType()->isPointerTy();
  a_type_ptr raw_op1_ty = skip_typerefs(op1->type);
  bool is_signed = raw_op1_ty->kind == tk_integer ? int_kind_is_signed[raw_op1_ty->variant.integer.int_kind] : false;
  if (is_complex && !rhs_val->getType()->isStructTy()) {
      llvm::Type* c_elem_ty = loaded_val->getType()->getStructElementType(0);
      llvm::Value* r_cast = nullptr;
      if (c_elem_ty->isFloatingPointTy()) {
          r_cast = rhs_val->getType()->isFloatingPointTy()
                       ? be_state->builder->CreateFPCast(rhs_val, c_elem_ty)
                       : be_state->builder->CreateSIToFP(rhs_val, c_elem_ty);
      } else {
          r_cast = rhs_val->getType()->isFloatingPointTy()
                       ? be_state->builder->CreateFPToSI(rhs_val, c_elem_ty)
                       : be_state->builder->CreateIntCast(rhs_val, c_elem_ty, true);
      }
      llvm::Value* c_tmp = be_state->builder->CreateInsertValue(
          llvm::UndefValue::get(loaded_val->getType()), r_cast, 0);
      rhs_val = be_state->builder->CreateInsertValue(
          c_tmp, llvm::Constant::getNullValue(c_elem_ty), 1);
  } else if (loaded_val->getType()->isIntOrIntVectorTy() &&
             rhs_val->getType()->isIntOrIntVectorTy() &&
             rhs_val->getType() != loaded_val->getType()) {
      bool is_shift = (expr->variant.operation.kind == eok_shiftl_assign ||
                       expr->variant.operation.kind == eok_shiftr_assign);
      rhs_val = be_state->builder->CreateIntCast(
          rhs_val, loaded_val->getType(), !is_shift && is_signed);
  }
  
  switch (expr->variant.operation.kind) {
      case eok_pre_incr:
      case eok_post_incr:
      case eok_add_assign:
      case eok_padd_assign:
          if (is_complex) {
              llvm::Value* lr = be_state->builder->CreateExtractValue(loaded_val, 0);
              llvm::Value* li = be_state->builder->CreateExtractValue(loaded_val, 1);
              llvm::Value* rr = be_state->builder->CreateExtractValue(rhs_val, 0);
              llvm::Value* ri = be_state->builder->CreateExtractValue(rhs_val, 1);
              bool fp_c = lr->getType()->isFloatingPointTy();
              llvm::Value* r = fp_c ? be_state->builder->CreateFAdd(lr, rr) : be_state->builder->CreateAdd(lr, rr);
              llvm::Value* i = fp_c ? be_state->builder->CreateFAdd(li, ri) : be_state->builder->CreateAdd(li, ri);
              result_val = be_state->builder->CreateInsertValue(
                  be_state->builder->CreateInsertValue(llvm::UndefValue::get(loaded_val->getType()), r, 0),
                  i, 1);
          } else if (is_float) result_val = be_state->builder->CreateFAdd(loaded_val, rhs_val);
          else if (is_ptr) {
              llvm::Type* elem_ty = llvm::Type::getInt8Ty(*be_state->context);
              if (raw_op1_ty->kind == tk_pointer) {
                LLVM_GEN_BE_CHECK(get_llvm_type(raw_op1_ty->variant.pointer.type, &elem_ty));
              }
              bool rhs_signed = true;
              if (op2 && op2->type) {
                a_type_ptr r_ty = skip_typerefs(op2->type);
                if (r_ty->kind == tk_integer) {
                  rhs_signed = (int_kind_is_signed[r_ty->variant.integer.int_kind] != FALSE);
                }
              }
              llvm::Value* idx64 = rhs_val->getType()->isIntegerTy()
                  ? be_state->builder->CreateIntCast(rhs_val, llvm::Type::getInt64Ty(*be_state->context), rhs_signed)
                  : rhs_val;
              result_val = be_state->builder->CreateInBoundsGEP(elem_ty, loaded_val, idx64);
          } else {
              result_val = be_state->builder->CreateAdd(loaded_val, rhs_val);
          }
          break;
      case eok_pre_decr:
      case eok_post_decr:
      case eok_subtract_assign:
      case eok_psubtract_assign:
          if (is_complex) {
              llvm::Value* lr = be_state->builder->CreateExtractValue(loaded_val, 0);
              llvm::Value* li = be_state->builder->CreateExtractValue(loaded_val, 1);
              llvm::Value* rr = be_state->builder->CreateExtractValue(rhs_val, 0);
              llvm::Value* ri = be_state->builder->CreateExtractValue(rhs_val, 1);
              bool fp_c = lr->getType()->isFloatingPointTy();
              llvm::Value* r = fp_c ? be_state->builder->CreateFSub(lr, rr) : be_state->builder->CreateSub(lr, rr);
              llvm::Value* i = fp_c ? be_state->builder->CreateFSub(li, ri) : be_state->builder->CreateSub(li, ri);
              result_val = be_state->builder->CreateInsertValue(
                  be_state->builder->CreateInsertValue(llvm::UndefValue::get(loaded_val->getType()), r, 0),
                  i, 1);
          } else if (is_float) result_val = be_state->builder->CreateFSub(loaded_val, rhs_val);
          else if (is_ptr) {
              llvm::Type* elem_ty = llvm::Type::getInt8Ty(*be_state->context);
              if (raw_op1_ty->kind == tk_pointer) {
                LLVM_GEN_BE_CHECK(get_llvm_type(raw_op1_ty->variant.pointer.type, &elem_ty));
              }
              bool rhs_signed = true;
              if (op2 && op2->type) {
                a_type_ptr r_ty = skip_typerefs(op2->type);
                if (r_ty->kind == tk_integer) {
                  rhs_signed = (int_kind_is_signed[r_ty->variant.integer.int_kind] != FALSE);
                }
              }
              llvm::Value* idx64 = rhs_val->getType()->isIntegerTy()
                  ? be_state->builder->CreateIntCast(rhs_val, llvm::Type::getInt64Ty(*be_state->context), rhs_signed)
                  : rhs_val;
              result_val = be_state->builder->CreateInBoundsGEP(elem_ty, loaded_val, be_state->builder->CreateNeg(idx64));
          } else {
              result_val = be_state->builder->CreateSub(loaded_val, rhs_val);
          }
          break;
      case eok_multiply_assign:
          if (is_complex) {
              LLVM_GEN_BE_CHECK(emit_complex_libcall("__mulsc3", "__muldc3", loaded_val, rhs_val, &result_val));
          } else if (is_float) result_val = be_state->builder->CreateFMul(loaded_val, rhs_val);
          else result_val = be_state->builder->CreateMul(loaded_val, rhs_val);
          break;
      case eok_divide_assign:
          if (is_complex) {
              LLVM_GEN_BE_CHECK(emit_complex_libcall("__divsc3", "__divdc3", loaded_val, rhs_val, &result_val));
          } else if (is_float) result_val = be_state->builder->CreateFDiv(loaded_val, rhs_val);
          else if (is_signed) result_val = be_state->builder->CreateSDiv(loaded_val, rhs_val);
          else result_val = be_state->builder->CreateUDiv(loaded_val, rhs_val);
          break;
      case eok_remainder_assign:
          if (is_float) result_val = be_state->builder->CreateFRem(loaded_val, rhs_val);
          else if (is_signed) result_val = be_state->builder->CreateSRem(loaded_val, rhs_val);
          else result_val = be_state->builder->CreateURem(loaded_val, rhs_val);
          break;
      case eok_and_assign:
          result_val = be_state->builder->CreateAnd(loaded_val, rhs_val);
          break;
      case eok_or_assign:
          result_val = be_state->builder->CreateOr(loaded_val, rhs_val);
          break;
      case eok_xor_assign:
          result_val = be_state->builder->CreateXor(loaded_val, rhs_val);
          break;
      case eok_shiftl_assign:
          result_val = be_state->builder->CreateShl(loaded_val, rhs_val);
          break;
      case eok_shiftr_assign:
          if (is_signed) result_val = be_state->builder->CreateAShr(loaded_val, rhs_val);
          else result_val = be_state->builder->CreateLShr(loaded_val, rhs_val);
          break;
      case eok_bassign: {
          // Block assignment
          uint64_t size = static_cast<uint64_t>(raw_op1_ty->size);
          llvm::Align align = llvm::Align(EDG_ALIGN_OF_TYPE(op1->type));
          be_state->builder->CreateMemCpy(lhs_ptr, align, rhs_val, align, size);
          *out_val = lhs_ptr; // Yield destination pointer for bassign
          return llvm_gen_be_error_t::ok;
      }
      default:
          return llvm_gen_be_error_t::unsupported_expr;
  }

  if (vec_container_ty) {
      llvm::Value* full_vec = be_state->builder->CreateLoad(vec_container_ty, lhs_ptr);
      llvm::Value* updated_vec = be_state->builder->CreateInsertElement(full_vec, result_val, vec_elem_idx);
      be_state->builder->CreateStore(updated_vec, lhs_ptr);
  } else if (bitfield_ptr) {
      LLVM_GEN_BE_CHECK(llvm_emit_bitfield_store(lhs_ptr, result_val, bitfield_ptr, op1->type));
  } else {
      llvm::StoreInst* store = be_state->builder->CreateStore(result_val, lhs_ptr);
      bool is_vol = false;
      LLVM_GEN_BE_CHECK(check_type_is_volatile(op1->type, &is_vol));
      if (is_vol) store->setVolatile(true);
  }

  if (expr->variant.operation.kind == eok_post_incr || expr->variant.operation.kind == eok_post_decr) {
      *out_val = loaded_val;
  } else {
      *out_val = result_val;
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an ISO Embedded C fixed-point binary arithmetic operation using LLVM intrinsics.
 * @details Supports saturating (`llvm.sadd.sat`, `llvm.uadd.sat`, `llvm.ssub.sat`, `llvm.usub.sat`,
 *          `llvm.smul.fix.sat`, `llvm.umul.fix.sat`) and non-saturating (`llvm.smul.fix`, `llvm.umul.fix`,
 *          `llvm.sdiv.fix`, `llvm.udiv.fix`, `add`, `sub`) fixed-point arithmetic.
 * @param[in] op_kind The EDG binary expression operator kind (`eok_add`, `eok_subtract`, `eok_multiply`, `eok_divide`).
 * @param[in] is_signed Whether the fixed-point type is signed.
 * @param[in] is_saturating Whether the fixed-point type is saturating (`_Sat`).
 * @param[in] scale Fractional bit scale factor for fixed-point multiplication/division.
 * @param[in] lhs Lowered left-hand integer representation operand.
 * @param[in] rhs Lowered right-hand integer representation operand.
 * @param[out] out_val Receives the computed fixed-point result value.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_fixed_point_binop(
    an_expr_operator_kind op_kind,
    bool is_signed,
    bool is_saturating,
    unsigned scale,
    llvm::Value* lhs,
    llvm::Value* rhs,
    llvm::Value** out_val) noexcept {
  if (!lhs || !rhs || !out_val) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  llvm::Type* int_ty = lhs->getType();
  llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
  llvm::Value* scale_val = llvm::ConstantInt::get(i32_ty, scale);

  if (op_kind == eok_add) {
    if (is_saturating) {
      llvm::Intrinsic::ID id = is_signed ? llvm::Intrinsic::sadd_sat : llvm::Intrinsic::uadd_sat;
      llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), id, {int_ty});
      *out_val = be_state->builder->CreateCall(fn, {lhs, rhs});
    } else {
      *out_val = be_state->builder->CreateAdd(lhs, rhs);
    }
    return llvm_gen_be_error_t::ok;
  }
  if (op_kind == eok_subtract) {
    if (is_saturating) {
      llvm::Intrinsic::ID id = is_signed ? llvm::Intrinsic::ssub_sat : llvm::Intrinsic::usub_sat;
      llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), id, {int_ty});
      *out_val = be_state->builder->CreateCall(fn, {lhs, rhs});
    } else {
      *out_val = be_state->builder->CreateSub(lhs, rhs);
    }
    return llvm_gen_be_error_t::ok;
  }
  if (op_kind == eok_multiply) {
    llvm::Intrinsic::ID id = is_saturating
                                 ? (is_signed ? llvm::Intrinsic::smul_fix_sat : llvm::Intrinsic::umul_fix_sat)
                                 : (is_signed ? llvm::Intrinsic::smul_fix : llvm::Intrinsic::umul_fix);
    llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), id, {int_ty});
    *out_val = be_state->builder->CreateCall(fn, {lhs, rhs, scale_val});
    return llvm_gen_be_error_t::ok;
  }
  if (op_kind == eok_divide) {
    llvm::Intrinsic::ID id = is_signed ? llvm::Intrinsic::sdiv_fix : llvm::Intrinsic::udiv_fix;
    llvm::Function* fn = EDG_LLVM_GET_INTRINSIC_DECL(be_state->module.get(), id, {int_ty});
    *out_val = be_state->builder->CreateCall(fn, {lhs, rhs, scale_val});
    return llvm_gen_be_error_t::ok;
  }
  return llvm_gen_be_error_t::unsupported_expr;
}

/**
 * @brief Computes a per-byte padding mask (`0x00` = padding byte, `0xFF` = value byte, or bitmask for bitfields) for an EDG type.
 * @details Marks all non-padding field, bitfield, and base-class bytes in @p out_mask so that
 *          `__builtin_clear_padding` zeroes only padding bytes and bits.
 * @param[in] type Pointer to the EDG type to inspect.
 * @param[out] out_mask Receives a byte vector of length `type->size` where `1` bits represent active value bits and `0` bits represent padding.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_mask` is null.
 */
llvm_gen_be_error_t llvm_compute_type_padding_mask(
    a_type_ptr type,
    std::vector<uint8_t>* out_mask) noexcept {
  if (!out_mask) return llvm_gen_be_error_t::invalid_argument;
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  out_mask->clear();
  if (!type) return llvm_gen_be_error_t::ok;
  a_type_ptr ut = skip_typerefs(type);
  uint64_t sz = static_cast<uint64_t>(ut->size);
  if (sz == 0) return llvm_gen_be_error_t::ok;

  if (ut->kind != tk_struct && ut->kind != tk_class) {
    out_mask->assign(sz, 0xFFu);
    return llvm_gen_be_error_t::ok;
  }

  out_mask->assign(sz, 0x00u);
  if (ut->variant.class_struct_union.extra_info) {
    for (a_base_class_ptr bc = ut->variant.class_struct_union.extra_info->base_classes;
         bc != nullptr; bc = bc->next) {
      if (bc->type) {
        std::vector<uint8_t> base_mask;
        LLVM_GEN_BE_CHECK(llvm_compute_type_padding_mask(bc->type, &base_mask));
        uint64_t b_off = static_cast<uint64_t>(bc->offset);
        for (size_t i = 0; i < base_mask.size() && (b_off + i) < sz; ++i) {
          (*out_mask)[b_off + i] |= base_mask[i];
        }
      }
    }
  }
  for (a_field_ptr f = ut->variant.class_struct_union.field_list; f != nullptr; f = f->next) {
    uint64_t f_off = static_cast<uint64_t>(f->offset);
    if (f->is_bit_field) {
      unsigned bit_rem = static_cast<unsigned>(f->offset_bit_remainder);
      unsigned bit_sz = static_cast<unsigned>(f->bit_size);
      for (unsigned b = 0; b < bit_sz; ++b) {
        unsigned total_bit = bit_rem + b;
        uint64_t byte_idx = f_off + (total_bit / 8);
        unsigned bit_in_byte = total_bit % 8;
        if (byte_idx < sz) {
          (*out_mask)[byte_idx] |= static_cast<uint8_t>(1u << bit_in_byte);
        }
      }
    } else if (f->type) {
      std::vector<uint8_t> fld_mask;
      LLVM_GEN_BE_CHECK(llvm_compute_type_padding_mask(f->type, &fld_mask));
      for (size_t i = 0; i < fld_mask.size() && (f_off + i) < sz; ++i) {
        (*out_mask)[f_off + i] |= fld_mask[i];
      }
    }
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a C++11 `sizeof...(pack)` expression (`enk_sizeof_pack`).
 * @details Extracts the resolved pack element count from the EDG expression node and returns an integer constant.
 * @param[in] expr Pointer to the EDG `enk_sizeof_pack` expression node.
 * @param[out] out_val Receives the `llvm::ConstantInt*` pack count.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_sizeof_pack(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  uint64_t pack_count = 0;
  for (an_expr_node_ptr cur = expr->next; cur != nullptr; cur = cur->next) {
    pack_count++;
  }
  *out_val = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), pack_count);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a C++17 fold expression (`enk_fold`).
 * @details Evaluates unary and binary left/right folds across arithmetic, bitwise, shift, relational,
 *          short-circuit logical (`&&`, `||`), assignment, pointer-to-member, and comma operators.
 * @param[in] expr Pointer to the EDG `enk_fold` expression node.
 * @param[out] out_val Receives the folded `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_fold_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  an_expr_node_ptr f_op1 = expr->variant.fold.operands;
  if (!f_op1) {
    return llvm_gen_be_error_t::unsupported_expr;
  }
  a_token_kind f_tok = expr->variant.fold.operator_token;
  llvm::Value* acc = nullptr;
  LLVM_GEN_BE_CHECK(llvm_lower_expression(f_op1, &acc));

  for (an_expr_node_ptr curr = f_op1->next; curr != nullptr; curr = curr->next) {
    if (f_tok == tok_and_and || f_tok == tok_or_or) {
      bool is_and = (f_tok == tok_and_and);
      llvm::Function* func = be_state->builder->GetInsertBlock()->getParent();
      llvm::BasicBlock* rhs_bb = llvm::BasicBlock::Create(
          *be_state->context, is_and ? "fold.land.rhs" : "fold.lor.rhs", func);
      llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(
          *be_state->context, is_and ? "fold.land.end" : "fold.lor.end");
      llvm::Value* lhs_cond = be_state->builder->CreateICmpNE(
          acc, llvm::Constant::getNullValue(acc->getType()));
      llvm::BasicBlock* lhs_end_bb = be_state->builder->GetInsertBlock();
      if (is_and) {
        be_state->builder->CreateCondBr(lhs_cond, rhs_bb, end_bb);
      } else {
        be_state->builder->CreateCondBr(lhs_cond, end_bb, rhs_bb);
      }

      be_state->builder->SetInsertPoint(rhs_bb);
      llvm::Value* rhs = nullptr;
      LLVM_GEN_BE_CHECK(llvm_lower_expression(curr, &rhs));
      llvm::Value* rhs_cond = be_state->builder->CreateICmpNE(
          rhs, llvm::Constant::getNullValue(rhs->getType()));
      llvm::BasicBlock* rhs_end_bb = be_state->builder->GetInsertBlock();
      be_state->builder->CreateBr(end_bb);

      func->insert(func->end(), end_bb);
      be_state->builder->SetInsertPoint(end_bb);
      llvm::PHINode* phi = be_state->builder->CreatePHI(
          be_state->builder->getInt1Ty(), 2, is_and ? "fold.land" : "fold.lor");
      phi->addIncoming(
          is_and ? llvm::ConstantInt::getFalse(*be_state->context)
                 : llvm::ConstantInt::getTrue(*be_state->context),
          lhs_end_bb);
      phi->addIncoming(rhs_cond, rhs_end_bb);
      acc = acc->getType()->isIntegerTy()
                ? be_state->builder->CreateZExtOrTrunc(phi, acc->getType())
                : static_cast<llvm::Value*>(phi);
      continue;
    }

    llvm::Value* rhs = nullptr;
    LLVM_GEN_BE_CHECK(llvm_lower_expression(curr, &rhs));
    bool is_fp = acc->getType()->isFloatingPointTy();
    bool is_signed = (f_op1->type->kind == tk_integer)
                         ? static_cast<bool>(int_kind_is_signed[f_op1->type->variant.integer.int_kind])
                         : true;

    switch (f_tok) {
      case tok_plus:
      case tok_plus_assign:
        acc = is_fp ? be_state->builder->CreateFAdd(acc, rhs)
                    : be_state->builder->CreateAdd(acc, rhs);
        break;
      case tok_minus:
      case tok_minus_assign:
        acc = is_fp ? be_state->builder->CreateFSub(acc, rhs)
                    : be_state->builder->CreateSub(acc, rhs);
        break;
      case tok_star:
      case tok_times_assign:
        acc = is_fp ? be_state->builder->CreateFMul(acc, rhs)
                    : be_state->builder->CreateMul(acc, rhs);
        break;
      case tok_divide:
      case tok_divide_assign:
        acc = is_fp ? be_state->builder->CreateFDiv(acc, rhs)
                    : (is_signed ? be_state->builder->CreateSDiv(acc, rhs)
                                 : be_state->builder->CreateUDiv(acc, rhs));
        break;
      case tok_remainder:
      case tok_remainder_assign:
        acc = is_fp ? be_state->builder->CreateFRem(acc, rhs)
                    : (is_signed ? be_state->builder->CreateSRem(acc, rhs)
                                 : be_state->builder->CreateURem(acc, rhs));
        break;
      case tok_ampersand:
      case tok_and_assign:
        acc = be_state->builder->CreateAnd(acc, rhs);
        break;
      case tok_or:
      case tok_or_assign:
        acc = be_state->builder->CreateOr(acc, rhs);
        break;
      case tok_excl_or:
      case tok_excl_or_assign:
        acc = be_state->builder->CreateXor(acc, rhs);
        break;
      case tok_shift_left:
      case tok_shift_left_assign:
        acc = be_state->builder->CreateShl(acc, rhs);
        break;
      case tok_shift_right:
      case tok_shift_right_assign:
        acc = is_signed ? be_state->builder->CreateAShr(acc, rhs)
                        : be_state->builder->CreateLShr(acc, rhs);
        break;
      case tok_eq:
      case tok_ne:
      case tok_lt:
      case tok_le:
      case tok_gt:
      case tok_ge: {
        llvm::Value* cmp = nullptr;
        if (f_tok == tok_eq) cmp = is_fp ? be_state->builder->CreateFCmpOEQ(acc, rhs) : be_state->builder->CreateICmpEQ(acc, rhs);
        else if (f_tok == tok_ne) cmp = is_fp ? be_state->builder->CreateFCmpONE(acc, rhs) : be_state->builder->CreateICmpNE(acc, rhs);
        else if (f_tok == tok_lt) cmp = is_fp ? be_state->builder->CreateFCmpOLT(acc, rhs) : (is_signed ? be_state->builder->CreateICmpSLT(acc, rhs) : be_state->builder->CreateICmpULT(acc, rhs));
        else if (f_tok == tok_le) cmp = is_fp ? be_state->builder->CreateFCmpOLE(acc, rhs) : (is_signed ? be_state->builder->CreateICmpSLE(acc, rhs) : be_state->builder->CreateICmpULE(acc, rhs));
        else if (f_tok == tok_gt) cmp = is_fp ? be_state->builder->CreateFCmpOGT(acc, rhs) : (is_signed ? be_state->builder->CreateICmpSGT(acc, rhs) : be_state->builder->CreateICmpUGT(acc, rhs));
        else cmp = is_fp ? be_state->builder->CreateFCmpOGE(acc, rhs) : (is_signed ? be_state->builder->CreateICmpSGE(acc, rhs) : be_state->builder->CreateICmpUGE(acc, rhs));
        acc = acc->getType()->isIntegerTy()
                  ? be_state->builder->CreateZExtOrTrunc(cmp, acc->getType())
                  : cmp;
        break;
      }
      case tok_comma:
      case tok_assign:
      case tok_period_star:
      case tok_arrow_star:
        acc = rhs;
        break;
      default:
        acc = be_state->builder->CreateAdd(acc, rhs);
        break;
    }
  }
  *out_val = acc;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a C++ `new` or `new[]` expression (`enk_new_delete` or unlowered `eok_new`).
 * @details Computes allocation size (including array cookies and overflow checks), invokes `operator new`,
 *          initializes the storage via `llvm_emit_dynamic_init`, and registers exception cleanup.
 * @param[in] expr Pointer to the EDG `new` expression node.
 * @param[out] out_val Receives the allocated and initialized pointer `llvm::Value*`.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_new_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  if (expr->kind != enk_new_delete) {
    an_expr_node_ptr op1 = expr->variant.operation.operands;
    llvm::Type* size_t_ty = llvm::Type::getInt64Ty(*be_state->context);
    llvm::Value* size_val = nullptr;
    if (op1) {
      LLVM_GEN_BE_CHECK(llvm_lower_expression(op1, &size_val));
      size_val = be_state->builder->CreateIntCast(size_val, size_t_ty, false);
    } else {
      uint64_t alloc_sz = 1;
      uint64_t alloc_align = 8;
      if (expr->type && expr->type->kind == tk_pointer && expr->type->variant.pointer.type) {
        uint64_t ptee_sz = static_cast<uint64_t>(expr->type->variant.pointer.type->size);
        if (ptee_sz > 0) alloc_sz = ptee_sz;
        if (expr->type->variant.pointer.type->alignment > 16) {
          alloc_align = static_cast<uint64_t>(expr->type->variant.pointer.type->alignment);
        }
      }
      size_val = llvm::ConstantInt::get(size_t_ty, alloc_sz);
      if (alloc_align > 16) {
        llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
        llvm::FunctionType* aligned_new_fty = llvm::FunctionType::get(
            ptr_ty, {size_t_ty, size_t_ty}, false);
        llvm::FunctionCallee aligned_new_fn = be_state->module->getOrInsertFunction(
            "_ZnwmSt11align_val_t", aligned_new_fty);
        *out_val = be_state->builder->CreateCall(
            aligned_new_fn, {size_val, llvm::ConstantInt::get(size_t_ty, alloc_align)});
        return llvm_gen_be_error_t::ok;
      }
    }
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::FunctionType* new_fty = llvm::FunctionType::get(ptr_ty, {size_t_ty}, false);
    llvm::FunctionCallee new_fn = be_state->module->getOrInsertFunction("_Znwm", new_fty);
    *out_val = be_state->builder->CreateCall(new_fn, {size_val});
    return llvm_gen_be_error_t::ok;
  }

  a_new_delete_supplement_ptr nd = expr->variant.new_delete;
  if (!nd) return llvm_gen_be_error_t::invalid_argument;
  a_routine_ptr routine = nd->routine;
  if (!routine || !routine->source_corresp.name) return llvm_gen_be_error_t::unsupported_expr;

  llvm::Type* alloc_type = nullptr;
  llvm_gen_be_error_t err = get_llvm_type(nd->type, &alloc_type);
  if (err != llvm_gen_be_error_t::ok) return err;

  std::vector<llvm::Value*> alloc_args;
  llvm::Type* size_t_ty = llvm::Type::getInt64Ty(*be_state->context);
  llvm::Value* size_val = nullptr;
  llvm::Value* num_elems = nullptr;

  if (!nd->placement_new || llvm::StringRef(routine->source_corresp.name).starts_with("operator new")) {
    if (nd->number_of_elements) {
      err = llvm_lower_expression(nd->number_of_elements, &num_elems);
      if (err != llvm_gen_be_error_t::ok) return err;

      uint64_t type_size = be_state->module->getDataLayout().getTypeAllocSize(alloc_type);
      llvm::Value* type_size_val = llvm::ConstantInt::get(size_t_ty, type_size);

      num_elems = be_state->builder->CreateIntCast(num_elems, size_t_ty, false);
      size_val = be_state->builder->CreateMul(num_elems, type_size_val);

      if (type_has_nontrivial_destructor(nd->type)) {
        uint64_t cookie_sz = (nd->type->alignment > 8) ? static_cast<uint64_t>(nd->type->alignment) : 8ULL;
        llvm::Value* cookie_size_val = llvm::ConstantInt::get(size_t_ty, cookie_sz);
        size_val = be_state->builder->CreateAdd(size_val, cookie_size_val);
      }
    } else {
      uint64_t type_size = be_state->module->getDataLayout().getTypeAllocSize(alloc_type);
      size_val = llvm::ConstantInt::get(size_t_ty, type_size);
    }

    alloc_args.push_back(size_val);
    if (nd->type->alignment > 16 &&
        llvm::StringRef(routine->source_corresp.name).contains("St11align_val_t")) {
      alloc_args.push_back(
          llvm::ConstantInt::get(size_t_ty, static_cast<uint64_t>(nd->type->alignment)));
    }
  }

  for (an_expr_node_ptr arg = nd->arg; arg; arg = arg->next) {
    llvm::Value* arg_val = nullptr;
    err = llvm_lower_expression(arg, &arg_val);
    if (err != llvm_gen_be_error_t::ok) return err;
    alloc_args.push_back(arg_val);
  }

  llvm::Function* func = be_state->module->getFunction(routine->source_corresp.name);
  if (!func) {
    std::vector<llvm::Type*> alloc_arg_tys;
    for (auto* a : alloc_args) alloc_arg_tys.push_back(a->getType());
    llvm::FunctionType* expected_func_ty = llvm::FunctionType::get(
        llvm::PointerType::getUnqual(*be_state->context), alloc_arg_tys, false);
    func = llvm::Function::Create(
        expected_func_ty,
        llvm::GlobalValue::ExternalLinkage,
        routine->source_corresp.name,
        be_state->module.get());
  }

  llvm::Value* allocated_mem = nullptr;
  llvm::BasicBlock* alloc_unwind_bb = nullptr;
  if (!func->doesNotThrow()) {
    LLVM_GEN_BE_CHECK(llvm_get_or_create_current_unwind_dest(&alloc_unwind_bb));
  }
  if (alloc_unwind_bb) {
    llvm::Function* parent_fn = be_state->builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* alloc_cont_bb = llvm::BasicBlock::Create(*be_state->context, "new.alloc.cont", parent_fn);
    allocated_mem = be_state->builder->CreateInvoke(func, alloc_cont_bb, alloc_unwind_bb, alloc_args);
    be_state->builder->SetInsertPoint(alloc_cont_bb);
  } else {
    allocated_mem = be_state->builder->CreateCall(func, alloc_args);
  }
  llvm::Value* user_mem = allocated_mem;

  if (nd->number_of_elements && type_has_nontrivial_destructor(nd->type)) {
    llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
    uint64_t cookie_sz = (nd->type->alignment > 8) ? static_cast<uint64_t>(nd->type->alignment) : 8ULL;
    llvm::Value* cookie_size_val = llvm::ConstantInt::get(size_t_ty, cookie_sz);
    llvm::Value* num_elems_cast = be_state->builder->CreateIntCast(num_elems, size_t_ty, false);
    user_mem = be_state->builder->CreateInBoundsGEP(i8_ty, allocated_mem, cookie_size_val);
    llvm::Value* count_slot = be_state->builder->CreateInBoundsGEP(
        i8_ty, user_mem, llvm::ConstantInt::getSigned(size_t_ty, -8), "cookie.count.slot");
    be_state->builder->CreateStore(num_elems_cast, count_slot);
  }

  llvm::Type* expr_llvm_ty = nullptr;
  err = get_llvm_type(expr->type, &expr_llvm_ty);
  if (err != llvm_gen_be_error_t::ok) return err;

  llvm::Value* casted_mem = user_mem;

  if (nd->dynamic_init) {
    if (nd->number_of_elements && nd->dynamic_init->kind == dik_constructor &&
        nd->dynamic_init->variant.constructor.ptr != nullptr) {
      llvm::Function* parent_fn = be_state->builder->GetInsertBlock()->getParent();
      llvm::AllocaInst* idx_slot = be_state->builder->CreateAlloca(size_t_ty, nullptr, "new.arr.idx");
      be_state->builder->CreateStore(llvm::ConstantInt::get(size_t_ty, 0), idx_slot);
      llvm::BasicBlock* cond_bb = llvm::BasicBlock::Create(*be_state->context, "new.arr.cond", parent_fn);
      llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*be_state->context, "new.arr.body", parent_fn);
      llvm::BasicBlock* done_bb = llvm::BasicBlock::Create(*be_state->context, "new.arr.done", parent_fn);
      llvm::BasicBlock* unwind_bb = nullptr;
      bool need_arr_eh = (nd->freeing_of_storage_on_exception != nullptr) ||
                         (nd->dynamic_init->destructor != nullptr);
      if (need_arr_eh) {
        unwind_bb = llvm::BasicBlock::Create(*be_state->context, "new.arr.unwind", parent_fn);
      }
      be_state->builder->CreateBr(cond_bb);

      be_state->builder->SetInsertPoint(cond_bb);
      llvm::Value* cur_idx = be_state->builder->CreateLoad(size_t_ty, idx_slot, "new.idx");
      llvm::Value* more = be_state->builder->CreateICmpULT(cur_idx, num_elems);
      be_state->builder->CreateCondBr(more, body_bb, done_bb);

      be_state->builder->SetInsertPoint(body_bb);
      llvm::Value* elem_ptr = be_state->builder->CreateInBoundsGEP(alloc_type, casted_mem, cur_idx);
      if (unwind_bb) be_state->current_landing_pads.push_back(unwind_bb);
      err = llvm_emit_dynamic_init(nd->dynamic_init, elem_ptr, alloc_type);
      if (unwind_bb) be_state->current_landing_pads.pop_back();
      if (err != llvm_gen_be_error_t::ok) return err;
      llvm::Value* next_idx = be_state->builder->CreateAdd(cur_idx, llvm::ConstantInt::get(size_t_ty, 1));
      be_state->builder->CreateStore(next_idx, idx_slot);
      be_state->builder->CreateBr(cond_bb);

      if (unwind_bb) {
        be_state->builder->SetInsertPoint(unwind_bb);
        llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
        llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
        if (!parent_fn->hasPersonalityFn()) {
          llvm::FunctionCallee pers_fn = be_state->module->getOrInsertFunction(
              "__gxx_personality_v0", llvm::FunctionType::get(i32_ty, true));
          parent_fn->setPersonalityFn(llvm::cast<llvm::Constant>(pers_fn.getCallee()));
        }
        llvm::StructType* lpad_ty = llvm::StructType::get(*be_state->context, {ptr_ty, i32_ty});
        llvm::LandingPadInst* lpad = be_state->builder->CreateLandingPad(lpad_ty, 0);
        lpad->setCleanup(true);
        if (nd->dynamic_init->destructor != nullptr) {
          llvm::Function* dtor_fn = nullptr;
          LLVM_GEN_BE_CHECK(get_or_create_void_ptr_fn(nd->dynamic_init->destructor, &dtor_fn));
          llvm::BasicBlock* dtor_cond_bb = llvm::BasicBlock::Create(*be_state->context, "new.eh.dtor.cond", parent_fn);
          llvm::BasicBlock* dtor_body_bb = llvm::BasicBlock::Create(*be_state->context, "new.eh.dtor.body", parent_fn);
          llvm::BasicBlock* dtor_end_bb = llvm::BasicBlock::Create(*be_state->context, "new.eh.dtor.end", parent_fn);
          be_state->builder->CreateBr(dtor_cond_bb);

          be_state->builder->SetInsertPoint(dtor_cond_bb);
          llvm::Value* rem_idx = be_state->builder->CreateLoad(size_t_ty, idx_slot);
          llvm::Value* has_built = be_state->builder->CreateICmpUGT(rem_idx, llvm::ConstantInt::get(size_t_ty, 0));
          be_state->builder->CreateCondBr(has_built, dtor_body_bb, dtor_end_bb);

          be_state->builder->SetInsertPoint(dtor_body_bb);
          llvm::Value* destroy_idx = be_state->builder->CreateSub(rem_idx, llvm::ConstantInt::get(size_t_ty, 1));
          be_state->builder->CreateStore(destroy_idx, idx_slot);
          llvm::Value* destroy_ptr = be_state->builder->CreateInBoundsGEP(alloc_type, casted_mem, destroy_idx);
          be_state->builder->CreateCall(dtor_fn, {destroy_ptr});
          be_state->builder->CreateBr(dtor_cond_bb);

          be_state->builder->SetInsertPoint(dtor_end_bb);
        }
        if (nd->freeing_of_storage_on_exception && nd->freeing_of_storage_on_exception->destructor) {
          llvm::Function* cleanup_func = nullptr;
          LLVM_GEN_BE_CHECK(get_or_create_void_ptr_fn(
              nd->freeing_of_storage_on_exception->destructor, &cleanup_func));
          be_state->builder->CreateCall(cleanup_func, {allocated_mem});
        }
        be_state->builder->CreateResume(lpad);
      }
      be_state->builder->SetInsertPoint(done_bb);
    } else if (nd->freeing_of_storage_on_exception) {
      llvm::Function* parent_fn = be_state->builder->GetInsertBlock()->getParent();
      llvm::BasicBlock* normal_dest = llvm::BasicBlock::Create(*be_state->context, "new.init.cont", parent_fn);
      llvm::BasicBlock* unwind_dest = llvm::BasicBlock::Create(*be_state->context, "new.init.unwind", parent_fn);
      be_state->current_landing_pads.push_back(unwind_dest);
      err = llvm_emit_dynamic_init(nd->dynamic_init, casted_mem, alloc_type);
      be_state->current_landing_pads.pop_back();
      if (err != llvm_gen_be_error_t::ok) return err;
      be_state->builder->CreateBr(normal_dest);
      be_state->builder->SetInsertPoint(unwind_dest);
      llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
      llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
      if (!parent_fn->hasPersonalityFn()) {
        llvm::FunctionCallee pers_fn = be_state->module->getOrInsertFunction(
            "__gxx_personality_v0", llvm::FunctionType::get(i32_ty, true));
        parent_fn->setPersonalityFn(llvm::cast<llvm::Constant>(pers_fn.getCallee()));
      }
      llvm::StructType* lpad_ty = llvm::StructType::get(*be_state->context, {ptr_ty, i32_ty});
      llvm::LandingPadInst* lpad = be_state->builder->CreateLandingPad(lpad_ty, 0);
      lpad->setCleanup(true);
      llvm::Function* cleanup_func = nullptr;
      LLVM_GEN_BE_CHECK(get_or_create_void_ptr_fn(
          nd->freeing_of_storage_on_exception->destructor, &cleanup_func));
      be_state->builder->CreateCall(cleanup_func, {allocated_mem});
      be_state->builder->CreateResume(lpad);
      be_state->builder->SetInsertPoint(normal_dest);
    } else {
      err = llvm_emit_dynamic_init(nd->dynamic_init, casted_mem, alloc_type);
      if (err != llvm_gen_be_error_t::ok) return err;
    }
  }

  *out_val = casted_mem;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a C++ `delete` or `delete[]` expression (`enk_new_delete` or unlowered `eok_delete`).
 * @details Emits a null check, runs scalar or reverse-order array element destructors (adjusting for
 *          array cookies), and invokes `operator delete`.
 * @param[in] expr Pointer to the EDG `delete` expression node.
 * @param[out] out_val Receives a null pointer `llvm::Value*` representing the `void` result.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_lower_delete_expression(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;
  if (expr->kind != enk_new_delete) {
    an_expr_node_ptr op1 = expr->variant.operation.operands;
    if (!op1) return llvm_gen_be_error_t::invalid_argument;
    llvm::Value* ptr_to_del = nullptr;
    LLVM_GEN_BE_CHECK(llvm_lower_expression(op1, &ptr_to_del));
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::FunctionType* del_fty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*be_state->context), {ptr_ty}, false);
    llvm::FunctionCallee del_fn = be_state->module->getOrInsertFunction("_ZdlPv", del_fty);
    be_state->builder->CreateCall(del_fn, {ptr_to_del});
    *out_val = nullptr;
    return llvm_gen_be_error_t::ok;
  }

  a_new_delete_supplement_ptr nd = expr->variant.new_delete;
  if (!nd) return llvm_gen_be_error_t::invalid_argument;

  llvm::Value* ptr_to_delete = nullptr;
  llvm_gen_be_error_t err = llvm_lower_expression(nd->arg, &ptr_to_delete);
  if (err != llvm_gen_be_error_t::ok) return err;

  llvm::Value* base_ptr = ptr_to_delete;
  llvm::Value* array_total_bytes = nullptr;

  if (nd->array_delete && type_has_nontrivial_destructor(nd->type)) {
    llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
    llvm::Type* size_t_ty = llvm::Type::getInt64Ty(*be_state->context);
    int64_t cookie_sz = (nd->type->alignment > 8) ? static_cast<int64_t>(nd->type->alignment) : 8LL;
    llvm::Value* cookie_size_val = llvm::ConstantInt::getSigned(size_t_ty, -cookie_sz);
    base_ptr = be_state->builder->CreateInBoundsGEP(i8_ty, ptr_to_delete, cookie_size_val);
    llvm::Value* count_ptr = be_state->builder->CreateInBoundsGEP(
        i8_ty, ptr_to_delete, llvm::ConstantInt::getSigned(size_t_ty, -8), "cookie.count.ptr");
    llvm::Value* count_val = be_state->builder->CreateLoad(size_t_ty, count_ptr, "cookie.count");
    llvm::Type* elem_llvm_ty = nullptr;
    LLVM_GEN_BE_CHECK(get_llvm_type(nd->type, &elem_llvm_ty));
    uint64_t elem_alloc_sz = be_state->module->getDataLayout().getTypeAllocSize(elem_llvm_ty);
    array_total_bytes = be_state->builder->CreateAdd(
        be_state->builder->CreateMul(count_val, llvm::ConstantInt::get(size_t_ty, elem_alloc_sz)),
        llvm::ConstantInt::get(size_t_ty, static_cast<uint64_t>(cookie_sz)));
    if (nd->dynamic_init && nd->dynamic_init->destructor) {
      llvm::Function* dtor_fn = nullptr;
      LLVM_GEN_BE_CHECK(get_or_create_void_ptr_fn(nd->dynamic_init->destructor, &dtor_fn));
      llvm::Function* parent_fn = be_state->builder->GetInsertBlock()->getParent();
      llvm::BasicBlock* pre_bb = be_state->builder->GetInsertBlock();
      llvm::BasicBlock* loop_cond_bb = llvm::BasicBlock::Create(*be_state->context, "del.arr.cond", parent_fn);
      llvm::BasicBlock* loop_body_bb = llvm::BasicBlock::Create(*be_state->context, "del.arr.body", parent_fn);
      llvm::BasicBlock* loop_end_bb = llvm::BasicBlock::Create(*be_state->context, "del.arr.end", parent_fn);
      be_state->builder->CreateBr(loop_cond_bb);

      be_state->builder->SetInsertPoint(loop_cond_bb);
      llvm::PHINode* idx_phi = be_state->builder->CreatePHI(size_t_ty, 2, "del.idx");
      idx_phi->addIncoming(count_val, pre_bb);
      llvm::Value* has_more = be_state->builder->CreateICmpUGT(
          idx_phi, llvm::ConstantInt::get(size_t_ty, 0));
      be_state->builder->CreateCondBr(has_more, loop_body_bb, loop_end_bb);

      be_state->builder->SetInsertPoint(loop_body_bb);
      llvm::Value* prev_idx = be_state->builder->CreateSub(
          idx_phi, llvm::ConstantInt::get(size_t_ty, 1), "del.prev.idx");
      idx_phi->addIncoming(prev_idx, loop_body_bb);
      llvm::Value* elem_ptr = be_state->builder->CreateInBoundsGEP(
          elem_llvm_ty, ptr_to_delete, prev_idx);
      be_state->builder->CreateCall(dtor_fn, {elem_ptr});
      be_state->builder->CreateBr(loop_cond_bb);

      be_state->builder->SetInsertPoint(loop_end_bb);
    }
  } else if (nd->dynamic_init && nd->dynamic_init->destructor) {
    llvm::Function* dtor_fn = nullptr;
    LLVM_GEN_BE_CHECK(get_or_create_void_ptr_fn(nd->dynamic_init->destructor, &dtor_fn));
    be_state->builder->CreateCall(dtor_fn, {ptr_to_delete});
  }

  a_routine_ptr routine = nd->routine;
  std::vector<llvm::Value*> dealloc_args;
  dealloc_args.push_back(base_ptr);

  for (an_expr_node_ptr extra_arg = nd->arg->next; extra_arg; extra_arg = extra_arg->next) {
    llvm::Value* extra_arg_val = nullptr;
    err = llvm_lower_expression(extra_arg, &extra_arg_val);
    if (err != llvm_gen_be_error_t::ok) return err;
    dealloc_args.push_back(extra_arg_val);
  }
  llvm::StringRef del_rname = llvm::StringRef(routine->source_corresp.name);
  if (dealloc_args.size() == 1) {
    llvm::Type* size_t_ty = llvm::Type::getInt64Ty(*be_state->context);
    if (array_total_bytes && (del_rname == "_ZdaPvm" || del_rname == "_ZdaPvmSt11align_val_t")) {
      dealloc_args.push_back(array_total_bytes);
    } else if (!nd->array_delete &&
               (del_rname == "_ZdlPvm" || del_rname == "_ZdlPvmSt11align_val_t")) {
      dealloc_args.push_back(
          llvm::ConstantInt::get(size_t_ty, static_cast<uint64_t>(nd->type->size)));
    }
    if (nd->type->alignment > 16 && del_rname.contains("St11align_val_t")) {
      dealloc_args.push_back(
          llvm::ConstantInt::get(size_t_ty, static_cast<uint64_t>(nd->type->alignment)));
    }
  }

  llvm::Function* func = be_state->module->getFunction(routine->source_corresp.name);
  if (!func) {
    std::vector<llvm::Type*> dealloc_arg_tys;
    for (auto* a : dealloc_args) dealloc_arg_tys.push_back(a->getType());
    llvm::FunctionType* expected_func_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*be_state->context), dealloc_arg_tys, false);
    func = llvm::Function::Create(
        expected_func_ty,
        llvm::GlobalValue::ExternalLinkage,
        routine->source_corresp.name,
        be_state->module.get());
  }
  be_state->builder->CreateCall(func, dealloc_args);

  *out_val = nullptr;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Resolves the `libgcc` mode suffix (`"sd"`, `"dd"`, `"td"`) and prefix (`"__bid_"` or `"__dpd_"`) for decimal floats.
 * @details Uses `"__dpd_"` on PowerPC and SystemZ targets and `"__bid_"` on x86/ARM/other targets.
 * @param[in] byte_width Decimal float byte width (`4`, `8`, or `16`).
 * @param[out] out_prefix Receives `"__bid_"` or `"__dpd_"`.
 * @param[out] out_mode Receives `"sd"`, `"dd"`, or `"td"`.
 * @return `llvm_gen_be_error_t::ok` on success, or `unsupported_type` if `byte_width` is invalid.
 */
static llvm_gen_be_error_t resolve_decimal_float_libgcc_mode(
    unsigned byte_width,
    const char** out_prefix,
    const char** out_mode) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
  *out_prefix = (triple.isPPC() || triple.isSystemZ()) ? "__dpd_" : "__bid_";
  if (byte_width == 4) {
    *out_mode = "sd";
    return llvm_gen_be_error_t::ok;
  }
  if (byte_width == 8) {
    *out_mode = "dd";
    return llvm_gen_be_error_t::ok;
  }
  if (byte_width == 16) {
    *out_mode = "td";
    return llvm_gen_be_error_t::ok;
  }
  return llvm_gen_be_error_t::unsupported_type;
}

/**
 * @brief Lowers an IEEE-754 decimal floating-point binary arithmetic operation (`+`, `-`, `*`, `/`) via `libgcc` `__bid_*` / `__dpd_*` runtime calls.
 * @details Selects `__bid_*` (x86/ARM) or `__dpd_*` (PowerPC/SystemZ) runtime helper for `_Decimal32` (`sd`), `_Decimal64` (`dd`), or `_Decimal128` (`td`).
 * @param[in] byte_width Byte width of the decimal float (`4`, `8`, or `16`).
 * @param[in] op_kind Binary operator kind (`eok_add`, `eok_subtract`, `eok_multiply`, `eok_divide`).
 * @param[in] lhs Lowered left-hand carrier operand (`i32`, `i64`, or `i128`).
 * @param[in] rhs Lowered right-hand carrier operand (`i32`, `i64`, or `i128`).
 * @param[out] out_val Receives the resulting decimal float carrier value.
 * @return `llvm_gen_be_error_t::ok` on success, or an error code on invalid arguments/operator.
 */
llvm_gen_be_error_t llvm_lower_decimal_float_binop(
    unsigned byte_width,
    an_expr_operator_kind op_kind,
    llvm::Value* lhs,
    llvm::Value* rhs,
    llvm::Value** out_val) noexcept {
  if (!lhs || !rhs || !out_val) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  const char* prefix = nullptr;
  const char* mode = nullptr;
  LLVM_GEN_BE_CHECK(resolve_decimal_float_libgcc_mode(byte_width, &prefix, &mode));
  const char* op_name = nullptr;
  switch (op_kind) {
    case eok_add: op_name = "add"; break;
    case eok_subtract: op_name = "sub"; break;
    case eok_multiply: op_name = "mul"; break;
    case eok_divide: op_name = "div"; break;
    default: return llvm_gen_be_error_t::unsupported_expr;
  }
  llvm::Type* carrier_ty = nullptr;
  LLVM_GEN_BE_CHECK(llvm_lower_decimal_float_type(byte_width, &carrier_ty));
  std::string fn_name = std::string(prefix) + op_name + mode + "3";
  llvm::FunctionType* fn_ty = llvm::FunctionType::get(carrier_ty, {carrier_ty, carrier_ty}, false);
  llvm::FunctionCallee fn = be_state->module->getOrInsertFunction(fn_name, fn_ty);
  *out_val = be_state->builder->CreateCall(fn, {lhs, rhs});
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers an IEEE-754 decimal floating-point comparison (`==`, `!=`, `<`, `<=`, `>`, `>=`) via `libgcc` `__bid_*` / `__dpd_*` runtime calls.
 * @details Calls `__bid_eqsd2` / `__dpd_eqsd2` (etc.), compares the `i32` status result against `0`, and zero-extends to @p result_ty.
 * @param[in] byte_width Byte width of the decimal float (`4`, `8`, or `16`).
 * @param[in] op_kind Relational operator kind (`eok_eq`, `eok_ne`, `eok_lt`, `eok_le`, `eok_gt`, `eok_ge`).
 * @param[in] lhs Lowered left-hand carrier operand.
 * @param[in] rhs Lowered right-hand carrier operand.
 * @param[in] result_ty Target boolean/integer result LLVM type (e.g. `i8` or `i32`).
 * @param[out] out_val Receives the comparison result value.
 * @return `llvm_gen_be_error_t::ok` on success, or an error code on invalid arguments/operator.
 */
llvm_gen_be_error_t llvm_lower_decimal_float_cmp(
    unsigned byte_width,
    an_expr_operator_kind op_kind,
    llvm::Value* lhs,
    llvm::Value* rhs,
    llvm::Type* result_ty,
    llvm::Value** out_val) noexcept {
  if (!lhs || !rhs || !result_ty || !out_val) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  const char* prefix = nullptr;
  const char* mode = nullptr;
  LLVM_GEN_BE_CHECK(resolve_decimal_float_libgcc_mode(byte_width, &prefix, &mode));
  const char* cmp_name = nullptr;
  llvm::CmpInst::Predicate pred = llvm::CmpInst::ICMP_EQ;
  switch (op_kind) {
    case eok_eq: cmp_name = "eq"; pred = llvm::CmpInst::ICMP_EQ; break;
    case eok_ne: cmp_name = "ne"; pred = llvm::CmpInst::ICMP_NE; break;
    case eok_lt: cmp_name = "lt"; pred = llvm::CmpInst::ICMP_SLT; break;
    case eok_le: cmp_name = "le"; pred = llvm::CmpInst::ICMP_SLE; break;
    case eok_gt: cmp_name = "gt"; pred = llvm::CmpInst::ICMP_SGT; break;
    case eok_ge: cmp_name = "ge"; pred = llvm::CmpInst::ICMP_SGE; break;
    default: return llvm_gen_be_error_t::unsupported_expr;
  }
  llvm::Type* carrier_ty = nullptr;
  LLVM_GEN_BE_CHECK(llvm_lower_decimal_float_type(byte_width, &carrier_ty));
  llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
  std::string fn_name = std::string(prefix) + cmp_name + mode + "2";
  llvm::FunctionType* fn_ty = llvm::FunctionType::get(i32_ty, {carrier_ty, carrier_ty}, false);
  llvm::FunctionCallee fn = be_state->module->getOrInsertFunction(fn_name, fn_ty);
  llvm::Value* call_res = be_state->builder->CreateCall(fn, {lhs, rhs});
  llvm::Value* cmp_i1 = be_state->builder->CreateICmp(pred, call_res, llvm::ConstantInt::get(i32_ty, 0));
  *out_val = result_ty->isIntegerTy(1) ? cmp_i1 : be_state->builder->CreateZExt(cmp_i1, result_ty);
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers conversions between IEEE-754 decimal floating-point types and decimal/binary-float/integer types via `libgcc` `__bid_*` / `__dpd_*` runtime calls.
 * @details Supports widening/narrowing between `_Decimal32`/`_Decimal64`/`_Decimal128`, conversions to/from `float`/`double`/`x86_fp80`/`fp128`, and signed/unsigned integer conversions.
 * @param[in] src_dec_bytes Source decimal float byte width (`4`, `8`, `16`, or `0` if source is binary float / integer).
 * @param[in] dst_dec_bytes Destination decimal float byte width (`4`, `8`, `16`, or `0` if destination is binary float / integer).
 * @param[in] int_is_unsigned True when converting from/to an unsigned integer type.
 * @param[in] src_val Lowered source operand value.
 * @param[in] dest_ty Target LLVM type.
 * @param[out] out_val Receives the converted LLVM value.
 * @return `llvm_gen_be_error_t::ok` on success, or an error code on invalid conversion widths/types.
 */
llvm_gen_be_error_t llvm_lower_decimal_float_cast(
    unsigned src_dec_bytes,
    unsigned dst_dec_bytes,
    bool int_is_unsigned,
    llvm::Value* src_val,
    llvm::Type* dest_ty,
    llvm::Value** out_val) noexcept {
  if (!src_val || !dest_ty || !out_val || (src_dec_bytes == 0 && dst_dec_bytes == 0)) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  if (src_dec_bytes != 0 && src_dec_bytes == dst_dec_bytes) {
    *out_val = src_val;
    return llvm_gen_be_error_t::ok;
  }
  const char* prefix = nullptr;
  const char* dummy_mode = nullptr;
  unsigned active_dec = (src_dec_bytes != 0) ? src_dec_bytes : dst_dec_bytes;
  LLVM_GEN_BE_CHECK(resolve_decimal_float_libgcc_mode(active_dec, &prefix, &dummy_mode));

  if (src_dec_bytes != 0 && dst_dec_bytes != 0) {
    const char* src_m = nullptr;
    const char* dst_m = nullptr;
    LLVM_GEN_BE_CHECK(resolve_decimal_float_libgcc_mode(src_dec_bytes, &prefix, &src_m));
    LLVM_GEN_BE_CHECK(resolve_decimal_float_libgcc_mode(dst_dec_bytes, &prefix, &dst_m));
    const char* verb = (dst_dec_bytes > src_dec_bytes) ? "extend" : "trunc";
    std::string fn_name = std::string(prefix) + verb + src_m + dst_m + "2";
    llvm::FunctionType* fn_ty = llvm::FunctionType::get(dest_ty, {src_val->getType()}, false);
    llvm::FunctionCallee fn = be_state->module->getOrInsertFunction(fn_name, fn_ty);
    *out_val = be_state->builder->CreateCall(fn, {src_val});
    return llvm_gen_be_error_t::ok;
  }

  llvm::Type* other_ty = (src_dec_bytes == 0) ? src_val->getType() : dest_ty;
  if (other_ty->isFloatingPointTy()) {
    const char* bin_m = "df";
    unsigned bin_bytes = 8;
    if (other_ty->isFloatTy() || other_ty->isHalfTy() || other_ty->isBFloatTy()) {
      bin_m = "sf";
      bin_bytes = 4;
    } else if (other_ty->isX86_FP80Ty()) {
      bin_m = "xf";
      bin_bytes = 12;
    } else if (other_ty->isFP128Ty()) {
      bin_m = "tf";
      bin_bytes = 16;
    }
    if (src_dec_bytes != 0) {
      const char* src_m = nullptr;
      LLVM_GEN_BE_CHECK(resolve_decimal_float_libgcc_mode(src_dec_bytes, &prefix, &src_m));
      const char* verb = (bin_bytes > src_dec_bytes) ? "extend" : "trunc";
      llvm::Type* call_ret_ty = (other_ty->isHalfTy() || other_ty->isBFloatTy())
                                    ? llvm::Type::getFloatTy(*be_state->context)
                                    : dest_ty;
      std::string fn_name = std::string(prefix) + verb + src_m + bin_m;
      llvm::FunctionType* fn_ty = llvm::FunctionType::get(call_ret_ty, {src_val->getType()}, false);
      llvm::FunctionCallee fn = be_state->module->getOrInsertFunction(fn_name, fn_ty);
      llvm::Value* res = be_state->builder->CreateCall(fn, {src_val});
      *out_val = (call_ret_ty != dest_ty) ? be_state->builder->CreateFPTrunc(res, dest_ty) : res;
      return llvm_gen_be_error_t::ok;
    }
    const char* dst_m = nullptr;
    LLVM_GEN_BE_CHECK(resolve_decimal_float_libgcc_mode(dst_dec_bytes, &prefix, &dst_m));
    const char* verb = (dst_dec_bytes >= bin_bytes) ? "extend" : "trunc";
    llvm::Value* call_arg = (other_ty->isHalfTy() || other_ty->isBFloatTy())
                                ? be_state->builder->CreateFPExt(src_val, llvm::Type::getFloatTy(*be_state->context))
                                : src_val;
    std::string fn_name = std::string(prefix) + verb + bin_m + dst_m;
    llvm::FunctionType* fn_ty = llvm::FunctionType::get(dest_ty, {call_arg->getType()}, false);
    llvm::FunctionCallee fn = be_state->module->getOrInsertFunction(fn_name, fn_ty);
    *out_val = be_state->builder->CreateCall(fn, {call_arg});
    return llvm_gen_be_error_t::ok;
  }

  if (other_ty->isIntegerTy()) {
    unsigned int_bw = other_ty->getIntegerBitWidth();
    bool use_64 = (int_bw > 32);
    llvm::Type* abi_int_ty = use_64 ? llvm::Type::getInt64Ty(*be_state->context)
                                    : llvm::Type::getInt32Ty(*be_state->context);
    const char* int_m = use_64 ? "di" : "si";
    if (src_dec_bytes != 0) {
      if (dest_ty->isIntegerTy(1)) {
        return llvm_lower_decimal_float_cmp(
            src_dec_bytes, eok_ne, src_val,
            llvm::ConstantInt::get(src_val->getType(), 0), dest_ty, out_val);
      }
      const char* src_m = nullptr;
      LLVM_GEN_BE_CHECK(resolve_decimal_float_libgcc_mode(src_dec_bytes, &prefix, &src_m));
      const char* verb = int_is_unsigned ? "fixuns" : "fix";
      std::string fn_name = std::string(prefix) + verb + src_m + int_m;
      llvm::FunctionType* fn_ty = llvm::FunctionType::get(abi_int_ty, {src_val->getType()}, false);
      llvm::FunctionCallee fn = be_state->module->getOrInsertFunction(fn_name, fn_ty);
      llvm::Value* res = be_state->builder->CreateCall(fn, {src_val});
      *out_val = (abi_int_ty != dest_ty)
                     ? be_state->builder->CreateIntCast(res, dest_ty, !int_is_unsigned)
                     : res;
      return llvm_gen_be_error_t::ok;
    }
    const char* dst_m = nullptr;
    LLVM_GEN_BE_CHECK(resolve_decimal_float_libgcc_mode(dst_dec_bytes, &prefix, &dst_m));
    const char* verb = int_is_unsigned ? "floatuns" : "float";
    llvm::Value* coerced_int = (src_val->getType() != abi_int_ty)
                                   ? be_state->builder->CreateIntCast(src_val, abi_int_ty, !int_is_unsigned)
                                   : src_val;
    std::string fn_name = std::string(prefix) + verb + int_m + dst_m;
    llvm::FunctionType* fn_ty = llvm::FunctionType::get(dest_ty, {abi_int_ty}, false);
    llvm::FunctionCallee fn = be_state->module->getOrInsertFunction(fn_name, fn_ty);
    *out_val = be_state->builder->CreateCall(fn, {coerced_int});
    return llvm_gen_be_error_t::ok;
  }

  return llvm_gen_be_error_t::unsupported_type;
}

/**
 * @brief Materializes a valid null `llvm::Constant*` for a lowered expression type.
 * @details Guards against `llvm::Constant::getNullValue` undefined behavior (`llvm_unreachable`)
 *          when synthetic or unannotated IL expression nodes lower to `void` or a raw `FunctionType`,
 *          substituting @p fallback_ty in those non-first-class value cases.
 * @param[in] expr_llvm_ty Lowered LLVM type of the expression node.
 * @param[in] fallback_ty First-class fallback LLVM type used when @p expr_llvm_ty cannot form a null constant.
 * @param[out] out_val Receives the materialized null `llvm::Constant*` as an `llvm::Value*`.
 * @return `llvm_gen_be_error_t::ok` on success, or `invalid_argument` if any pointer argument is null.
 */
llvm_gen_be_error_t llvm_materialize_null_constant(
    llvm::Type* expr_llvm_ty,
    llvm::Type* fallback_ty,
    llvm::Value** out_val) noexcept {
  if (!expr_llvm_ty || !fallback_ty || !out_val) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  llvm::Type* target_ty =
      (expr_llvm_ty->isVoidTy() || expr_llvm_ty->isFunctionTy()) ? fallback_ty : expr_llvm_ty;
  *out_val = llvm::Constant::getNullValue(target_ty);
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
