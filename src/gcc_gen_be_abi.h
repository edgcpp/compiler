/**
 * @file gcc_gen_be_abi.h
 * @brief ABI, Calling Convention, and Attribute generation for the GCC backend.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef GCC_GEN_BE_ABI_H
#define GCC_GEN_BE_ABI_H

#include "gcc_gen_be_error.h"
#include <libgccjit.h>
#include "fe_common.h"

BEGIN_EDG_NAMESPACE

/**
 * @brief Applies ABI attributes to a JIT function.
 *
 * @param rout The front-end routine entity containing attributes.
 * @param func The created libgccjit function.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_apply_function_attributes(a_routine_ptr rout, gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Applies struct packing and alignment attributes.
 *
 * @param class_type The class/struct/union type.
 * @param struct_type The corresponding libgccjit struct.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_apply_struct_attributes(a_type_ptr class_type, gcc_jit_struct *struct_type) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Applies visibility and linkage attributes to a global variable.
 *
 * @param var The front-end variable entity.
 * @param lval The corresponding libgccjit global lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_apply_variable_attributes(a_variable_ptr var, gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Represents the System V AMD64 ABI eightbyte classification classes.
 * @details Used to determine register versus memory passing conventions.
 */
typedef enum {
    GCC_GEN_BE_ABI_CLASS_INTEGER,      /**< @brief Passed in general purpose registers. */
    GCC_GEN_BE_ABI_CLASS_SSE,          /**< @brief Passed in SSE/vector registers. */
    GCC_GEN_BE_ABI_CLASS_SSEUP,        /**< @brief Passed in upper halves of SSE registers. */
    GCC_GEN_BE_ABI_CLASS_X87,          /**< @brief Passed in x87 FPU registers. */
    GCC_GEN_BE_ABI_CLASS_X87UP,        /**< @brief Passed in upper halves of x87 FPU registers. */
    GCC_GEN_BE_ABI_CLASS_COMPLEX_X87,  /**< @brief Passed as complex x87 values. */
    GCC_GEN_BE_ABI_CLASS_NO_CLASS,     /**< @brief Ignored or empty class. */
    GCC_GEN_BE_ABI_CLASS_MEMORY        /**< @brief Passed in memory (stack). */
} gcc_gen_be_abi_class_t;

/** @brief Target architecture ABI kinds. */
typedef enum {
    /** @brief System V AMD64 ABI (Linux/macOS/BSD x86_64). */
    GCC_GEN_BE_TARGET_ABI_SYSV_AMD64 = 0,
    /** @brief ARM64 AAPCS64 ABI (Linux AArch64). */
    GCC_GEN_BE_TARGET_ABI_AAPCS64,
    /** @brief Apple Darwin ARM64 ABI (macOS/iOS AArch64). */
    GCC_GEN_BE_TARGET_ABI_DARWIN_ARM64,
    /** @brief Microsoft Windows x64 ABI. */
    GCC_GEN_BE_TARGET_ABI_WIN64,
    /** @brief RISC-V LP64D ABI (64-bit integer + 64-bit hardware float). */
    GCC_GEN_BE_TARGET_ABI_RISCV64_LP64D,
    /** @brief 32-bit x86 System V ABI (i386). */
    GCC_GEN_BE_TARGET_ABI_SYSV_I386,
    /** @brief PowerPC64 ELFv2 little-endian ABI (ppc64le). */
    GCC_GEN_BE_TARGET_ABI_PPC64LE_ELFV2,
    /** @brief IBM z/Architecture 64-bit ELF ABI (s390x). */
    GCC_GEN_BE_TARGET_ABI_S390X,
    /** @brief ARM 32-bit AAPCS / AAPCS-VFP ABI. */
    GCC_GEN_BE_TARGET_ABI_AAPCS32
} gcc_gen_be_target_abi_t;

/**
 * @brief Tracks cumulative register usage across a function signature or call site.
 * @details Maintains GP and FP/SIMD register counters and limits for the active target ABI
 * so parameters that exhaust available hardware registers demote deterministically to stack/memory.
 */
typedef struct {
    gcc_gen_be_target_abi_t target_abi; /**< @brief Active target ABI kind. */
    int gp_used;                        /**< @brief Number of general-purpose argument registers consumed. */
    int fp_used;                        /**< @brief Number of floating-point/vector argument registers consumed. */
    int gp_limit;                       /**< @brief Maximum general-purpose argument registers available. */
    int fp_limit;                       /**< @brief Maximum floating-point/vector argument registers available. */
} gcc_gen_be_abi_reg_state_t;

/**
 * @brief Queries the active target ABI kind.
 *
 * @details Determines the active target ABI from host/target configuration or explicit override.
 *
 * @param[out] out_abi Pointer to receive the active target ABI kind.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT if out_abi is NULL.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_target_abi(gcc_gen_be_target_abi_t *out_abi) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Sets the active target ABI kind.
 *
 * @details Overrides the active target ABI for cross-compilation or target-specific testing.
 *
 * @param[in] abi The target ABI kind to activate.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT if abi is invalid.
 */
extern gcc_gen_be_error_t gcc_gen_be_set_target_abi(gcc_gen_be_target_abi_t abi) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Classifies a type according to System V AMD64 ABI.
 * @details Implements the recursive eightbyte classification algorithm.
 *          Aggregate types up to 16 bytes are classified into up to 2 eightbytes.
 * @param[in] type The frontend type to classify.
 * @param[out] classes An array of 2 classes receiving the eightbyte classifications.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code on failure.
 */
extern gcc_gen_be_error_t gcc_gen_be_classify_sysv_amd64(a_type_ptr type, gcc_gen_be_abi_class_t classes[2]) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Classifies a type according to AAPCS64 (ARM64) ABI.
 * @details Determines if a type is a Homogeneous Floating-Point Aggregate (HFA)
 *          or Homogeneous Short-Vector Aggregate (HVA).
 * @param[in] type The frontend type to classify.
 * @param[out] is_hfa_hva Set to TRUE if the type is HFA/HVA, FALSE otherwise.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code on failure.
 */
extern gcc_gen_be_error_t gcc_gen_be_classify_aapcs64(a_type_ptr type, a_boolean *is_hfa_hva) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Classifies a type according to Windows x64 ABI.
 * @details Determines if a type must be passed by reference (e.g. non-power-of-two size or > 8 bytes).
 * @param[in] type The frontend type to classify.
 * @param[out] out_is_ref Set to TRUE if the type must be passed by reference, FALSE otherwise.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code on failure.
 */
extern gcc_gen_be_error_t gcc_gen_be_classify_win64(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Classifies a type according to the RISC-V LP64D ABI.
 * @details Determines whether an aggregate is passed by reference (`> 2 * XLEN` = 16 bytes),
 * in hardware floating-point registers (`fp+fp` or `fp+int` for 1-2 field structs `<= 16` bytes),
 * or in general-purpose integer registers (`<= 16` bytes).
 * @param[in] type The frontend type to classify.
 * @param[out] out_is_ref Set to TRUE if passed by reference (`> 16` bytes), FALSE otherwise.
 * @param[out] out_uses_fp_regs Optional pointer set to TRUE if passed in `fa0..fa7` FP registers.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_classify_riscv64(
    a_type_ptr type,
    a_boolean *out_is_ref,
    a_boolean *out_uses_fp_regs) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Classifies a type according to the 32-bit x86 (`i386` System V) ABI.
 * @details Determines if an aggregate must be returned via hidden `sret` pointer on the stack.
 * @param[in] type The frontend type to classify.
 * @param[out] out_is_sret Set to TRUE if returned via hidden stack pointer, FALSE otherwise.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_classify_i386(
    a_type_ptr type,
    a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Classifies a type according to the PowerPC64 ELFv2 (`ppc64le`) ABI.
 * @details Determines if a type is a homogeneous float/vector aggregate (up to 8 elements)
 * or fits in general-purpose registers (`<= 16` bytes for return, `<= 64` bytes for parameters).
 * @param[in] type The frontend type to classify.
 * @param[out] out_is_hfa Set to TRUE if homogeneous float/vector aggregate.
 * @param[out] out_is_sret Set to TRUE if returned via hidden `r3` memory buffer (`> 16` bytes and non-HFA).
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_classify_ppc64le(
    a_type_ptr type,
    a_boolean *out_is_hfa,
    a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Alias classifier for the PowerPC64 ELFv2 ABI (`gcc_gen_be_classify_ppc64_elfv2`).
 * @details Delegates to `gcc_gen_be_classify_ppc64le`.
 * @param[in] type The frontend type to classify.
 * @param[out] out_is_hfa Set to TRUE if homogeneous float/vector aggregate.
 * @param[out] out_is_sret Set to TRUE if returned via hidden `r3` memory buffer (`> 16` bytes and non-HFA).
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_classify_ppc64_elfv2(
    a_type_ptr type,
    a_boolean *out_is_hfa,
    a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Classifies a type according to the IBM z/Architecture 64-bit ELF ABI (`s390x`).
 * @details Determines whether an aggregate is passed by hidden reference (`> 8` bytes or non-power-of-two size)
 * or in a single floating-point register (1-field float/double struct).
 * @param[in] type The frontend type to classify.
 * @param[out] out_is_ref Set to TRUE if passed/returned by hidden reference, FALSE otherwise.
 * @param[out] out_is_fp_reg Optional pointer set to TRUE if passed in a floating-point register.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_classify_s390x(
    a_type_ptr type,
    a_boolean *out_is_ref,
    a_boolean *out_is_fp_reg) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Determines if a type must be passed by invisible reference.
 * @details Checks if the type is non-trivially copyable.
 * @param[in] type The frontend type.
 * @param[out] out_is_ref Set to TRUE if it must be passed by invisible reference, FALSE otherwise.
 * @return gcc_gen_be_error_t GCC_GEN_BE_SUCCESS on success.
 */
extern gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT;
/**
 * @brief Determines if a type must be returned via sret.
 * @details Checks if the type is non-trivially copyable.
 * @param[in] type The frontend type.
 * @return TRUE if it must be returned via sret, FALSE otherwise.
 */
extern gcc_gen_be_error_t gcc_gen_be_is_returned_via_sret(a_type_ptr type, a_boolean *out_is_sret) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Classifies Darwin ARM64 ABI rules.
 * @details Implements Darwin variadic argument and bitfield calling conventions.
 * @param[in] type Type.
 * @param[out] out_is_stack TRUE if passed on stack (e.g. variadic).
 * @return GCC_GEN_BE_SUCCESS
 */
extern gcc_gen_be_error_t gcc_gen_be_classify_darwin_arm64(a_type_ptr type, a_boolean *out_is_stack) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Classifies a variadic (`va_arg`) argument type for the active target ABI.
 * @details Computes whether the argument is passed in the stack overflow area (`out_is_stack`),
 * and for register-save-area targets (System V AMD64, Linux AAPCS64), computes the number of
 * general-purpose registers (`out_num_gp`, 0..2) and floating-point/vector registers (`out_num_fp`, 0..4).
 * @param[in] type The frontend argument type.
 * @param[out] out_is_stack Pointer to receive TRUE if passed exclusively on the stack/overflow area.
 * @param[out] out_num_gp Optional pointer to receive the count of GP registers required.
 * @param[out] out_num_fp Optional pointer to receive the count of FP/SIMD registers required.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_classify_va_arg(
    a_type_ptr type,
    a_boolean *out_is_stack,
    int *out_num_gp,
    int *out_num_fp) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Computes the coerced register-transfer ABI type for System V AMD64 or the active target ABI.
 * @details For aggregates `<= 16` bytes classified as `[INTEGER, INTEGER]`, `[INTEGER, SSE]`,
 * `[SSE, INTEGER]`, or `[SSE, SSE]`, or vector/x87 types (`SSEUP`, `X87`, `COMPLEX_X87`),
 * synthesizes the coerced register-transfer representation when required by the target ABI.
 * @param[in] type The frontend type to inspect.
 * @param[out] out_coerced_type Pointer to receive the coerced `gcc_jit_type` (or the lowered type if no coercion needed).
 * @param[out] out_needs_coercion Pointer to receive TRUE if explicit register coercion is required, FALSE otherwise.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_get_coerced_abi_type(
    a_type_ptr type,
    gcc_jit_type **out_coerced_type,
    a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Computes the coerced register-transfer ABI type for AAPCS64 (ARM64).
 * @details Handles Homogeneous Floating-Point Aggregates (HFA) and Homogeneous Short-Vector Aggregates (HVA)
 * of up to 4 elements, and non-HFA composites `<= 16` bytes coerced into `uint64_t` general-purpose register pairs.
 * @param[in] type The frontend type to inspect.
 * @param[out] out_coerced_type Pointer to receive the coerced `gcc_jit_type`.
 * @param[out] out_needs_coercion Pointer to receive TRUE if explicit register coercion is required, FALSE otherwise.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_coerced_aapcs64_type(
    a_type_ptr type,
    gcc_jit_type **out_coerced_type,
    a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Computes the coerced register-transfer ABI type for RISC-V LP64D (`fa0`-`fa7` / `a0`-`a7`).
 * @details Flattens structs up to 16 bytes containing one or two floating-point/integer leaf fields
 * into a 1- or 2-field coerced struct (`fp`, `fp+fp`, `fp+gp`, or `gp+fp`), or into `uint64_t` integer
 * register pairs for non-FP composites `<= 16` bytes.
 * @param[in] type The frontend type to inspect.
 * @param[out] out_coerced_type Pointer to receive the coerced `gcc_jit_type`.
 * @param[out] out_needs_coercion Pointer to receive TRUE if explicit register coercion is required, FALSE otherwise.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_coerced_riscv64_type(
    a_type_ptr type,
    gcc_jit_type **out_coerced_type,
    a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Computes the coerced register-transfer ABI type for PowerPC64 ELFv2 (`ppc64le`).
 * @details Coerces Homogeneous Floating-Point and Vector Aggregates (HFA/HVA) of up to 8 elements
 * into a homogeneous lane struct, and non-HFA composites `<= 16` bytes into `uint64_t` register pairs.
 * @param[in] type The frontend type to inspect.
 * @param[out] out_coerced_type Pointer to receive the coerced `gcc_jit_type`.
 * @param[out] out_needs_coercion Pointer to receive TRUE if explicit register coercion is required, FALSE otherwise.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_coerced_ppc64le_type(
    a_type_ptr type,
    gcc_jit_type **out_coerced_type,
    a_boolean *out_needs_coercion) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Coerces an rvalue between a frontend aggregate type and its coerced register-transfer ABI type.
 * @details Uses `p_gcc_jit_context_new_bitcast` when sizes match and `libgccjit` supports bitcast,
 * or materializes the source rvalue into a temporary stack local in `block` and reinterprets its address
 * as `dst_type *`.
 * @param[in] func The active JIT function owning temporary locals.
 * @param[in] block The active basic block for temporary assignments.
 * @param[in] src_rval The source rvalue to coerce.
 * @param[in] dst_type The target `gcc_jit_type` (either coerced ABI type or unpacked frontend type).
 * @param[out] out_rval Pointer to receive the coerced `gcc_jit_rvalue`.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_coerce_rvalue(
    gcc_jit_function *func,
    gcc_jit_block *block,
    gcc_jit_rvalue *src_rval,
    gcc_jit_type *dst_type,
    gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Packs an aggregate argument rvalue into its coerced register representation for a call site.
 * @details Inspects `arg_type` using `gcc_gen_be_get_coerced_abi_type` and coerces `src_rval` via
 * `gcc_gen_be_coerce_rvalue` when register coercion applies.
 * @param[in] func The active JIT function owning temporary locals.
 * @param[in] block The active basic block for temporary assignments.
 * @param[in] arg_type The frontend parameter/argument type.
 * @param[in] src_rval The source argument rvalue.
 * @param[out] out_rval Pointer to receive the coerced argument rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_coerce_Arg_to_registers(
    gcc_jit_function *func,
    gcc_jit_block *block,
    a_type_ptr arg_type,
    gcc_jit_rvalue *src_rval,
    gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Unpacks a coerced register parameter into a local aggregate `gcc_jit_lvalue` in the function prologue.
 * @details Materializes a stack local of the lowered frontend aggregate type and copies the coerced
 * register parameter value into it.
 * @param[in] func The active JIT function.
 * @param[in] block The function entry block.
 * @param[in] param_type The frontend parameter type.
 * @param[in] param_rval The incoming coerced register parameter rvalue.
 * @param[out] out_lval Pointer to receive the reconstructed local aggregate lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_unpack_coerced_Arg(
    gcc_jit_function *func,
    gcc_jit_block *block,
    a_type_ptr param_type,
    gcc_jit_rvalue *param_rval,
    gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Packs an aggregate argument into its target-coerced register representation at a call site.
 * @details Delegates to `gcc_gen_be_coerce_Arg_to_registers`.
 * @param[in] func The active JIT function owning temporary locals.
 * @param[in] block The active basic block for temporary assignments.
 * @param[in] arg_type The frontend argument type.
 * @param[in] src_rval The source argument rvalue.
 * @param[out] out_rval Pointer to receive the packed/coerced argument rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_pack_coerced_argument(
    gcc_jit_function *func,
    gcc_jit_block *block,
    a_type_ptr arg_type,
    gcc_jit_rvalue *src_rval,
    gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Unpacks a target-coerced register parameter into a local aggregate lvalue in the callee prologue.
 * @details Delegates to `gcc_gen_be_unpack_coerced_Arg`.
 * @param[in] func The active JIT function.
 * @param[in] block The function entry basic block.
 * @param[in] param_type The frontend parameter type.
 * @param[in] param_rval The incoming coerced register parameter rvalue.
 * @param[out] out_lval Pointer to receive the unpacked local aggregate lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_unpack_coerced_parameter(
    gcc_jit_function *func,
    gcc_jit_block *block,
    a_type_ptr param_type,
    gcc_jit_rvalue *param_rval,
    gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Packs a function return value into its ABI-coerced register carrier type.
 *
 * @details When `ret_type` requires register coercion (e.g. small structs/unions returned in
 * integer/SSE/FP register pairs), packs `ret_rval` into a carrier rvalue matching the coerced
 * function return type.
 *
 * @param[in] func The active `gcc_jit_function`.
 * @param[in] block The active epilogue/return `gcc_jit_block`.
 * @param[in] ret_type The frontend return `a_type_ptr`.
 * @param[in] ret_rval The uncoerced return value `gcc_jit_rvalue`.
 * @param[out] out_coerced_rval Pointer to receive the coerced return `gcc_jit_rvalue`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_pack_coerced_return(
    gcc_jit_function *func,
    gcc_jit_block *block,
    a_type_ptr ret_type,
    gcc_jit_rvalue *ret_rval,
    gcc_jit_rvalue **out_coerced_rval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Unpacks a coerced register-carrier return value into a local aggregate lvalue at a call site.
 *
 * @details Reconstructs the frontend aggregate `ret_type` in a temporary stack slot from the
 * register-carrier `coerced_rval` returned by a call expression.
 *
 * @param[in] func The caller `gcc_jit_function`.
 * @param[in] block The active call-site `gcc_jit_block`.
 * @param[in] ret_type The frontend return `a_type_ptr`.
 * @param[in] coerced_rval The coerced carrier `gcc_jit_rvalue` returned by the call.
 * @param[out] out_lval Pointer to receive the unpacked aggregate `gcc_jit_lvalue`.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_unpack_coerced_return(
    gcc_jit_function *func,
    gcc_jit_block *block,
    a_type_ptr ret_type,
    gcc_jit_rvalue *coerced_rval,
    gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Classifies a type according to the ARM 32-bit AAPCS / AAPCS-VFP ABI.
 * @details Composites `<= 4` bytes are returned in `r0` (`*out_is_sret = FALSE`),
 * composites `> 4` bytes are returned via `sret` in `r0` (`*out_is_sret = TRUE`),
 * and 8-byte aligned arguments require even-odd register pairs (`r0:r1`, `r2:r3`).
 * @param[in] type The frontend type to classify.
 * @param[out] out_is_sret Set to `TRUE` if returned via hidden `sret` pointer in `r0`.
 * @param[out] out_even_odd_pair Optional pointer set to `TRUE` if double-word even-odd register alignment applies.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
extern gcc_gen_be_error_t gcc_gen_be_classify_aapcs32(
    a_type_ptr type,
    a_boolean *out_is_sret,
    a_boolean *out_even_odd_pair) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Initializes cumulative register-tracking state for a function call or signature.
 * @details Configures `gp_limit`, `fp_limit`, and initial `gp_used` (accounting for `sret` when the ABI
 * passes `sret` in a general-purpose argument register rather than a dedicated register like AArch64 `x8`).
 * @param[in] abi The target ABI kind.
 * @param[in] has_sret `TRUE` if the function returns an aggregate via hidden `sret` pointer.
 * @param[out] out_state Pointer to receive the initialized register state.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
extern gcc_gen_be_error_t gcc_gen_be_init_abi_reg_state(
    gcc_gen_be_target_abi_t abi,
    a_boolean has_sret,
    gcc_gen_be_abi_reg_state_t *out_state) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Allocates argument registers for a single parameter from a cumulative `gcc_gen_be_abi_reg_state_t`.
 * @details Evaluates `param_type` against remaining GP and FP/SIMD registers in `inout_state`,
 * updates `gp_used` and `fp_used` if passed in registers, or demotes the argument to stack/memory
 * (`*out_in_registers = FALSE`) when registers are exhausted.
 * @param[in,out] inout_state Cumulative register state for the current signature/call site.
 * @param[in] param_type The frontend parameter type.
 * @param[out] out_in_registers Pointer to receive `TRUE` if passed in registers, `FALSE` if passed in memory.
 * @param[out] out_by_ref Pointer to receive `TRUE` if passed by indirect pointer reference.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
extern gcc_gen_be_error_t gcc_gen_be_allocate_abi_param_regs(
    gcc_gen_be_abi_reg_state_t *inout_state,
    a_type_ptr param_type,
    a_boolean *out_in_registers,
    a_boolean *out_by_ref) GCC_GEN_BE_NOEXCEPT;

END_EDG_NAMESPACE

#endif /* GCC_GEN_BE_ABI_H */

