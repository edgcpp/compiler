/**
 * @defgroup llvm_gen_be_types Type & Constant Lowering
 * @ingroup llvm_gen_be
 * @brief EDG type and compile-time constant lowering into LLVM IR types and constants.
 */

/**
 * @file llvm_gen_be_type.h
 * @ingroup llvm_gen_be_types
 * @brief Type generation and lowering subsystem for the EDG LLVM backend.
 * @details Translates EDG front-end type structures (a_type_ptr) into LLVM IR
 * types (llvm::Type*). All functions return llvm_gen_be_error_t and output
 * constructed types via output pointers.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_TYPE_H
#define LLVM_GEN_BE_TYPE_H 1

#include "basic_hdrs.h"
#include "fe_common.h"
#include "error.h"
#include "il.h"
#include "types.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/Attributes.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Type.h>
#include <vector>

BEGIN_EDG_NAMESPACE

/**
 * @brief Translates an EDG type representation into a corresponding LLVM type.
 * @details Traverses the EDG type structure, looks up or populates the type cache,
 * and creates LLVM primitive, pointer, array, aggregate, or routine types.
 * @param[in] edg_type Pointer to the EDG type description structure. If null, produces void.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, llvm_gen_be_error_t::invalid_argument if
 *         out_type is null, or an error code indicating the failure reason.
 */
llvm_gen_be_error_t llvm_type_from_edg_type(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Lowers an EDG integer type into a matching LLVM IntegerType.
 * @details Maps integer kinds (char, short, int, long, long long, __int128, bool)
 * into appropriate bit-width LLVM integer types.
 * @param[in] edg_type Pointer to the EDG integer type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_integer(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Computes the fractional bit scale, signedness, and saturation flags for an EDG fixed-point type.
 * @details Maps ISO/IEC TR 18037 `_Fract` and `_Accum` (`fpp_short`, `fpp_default`, `fpp_long`,
 *          signed and unsigned, saturating and non-saturating) to their exact fractional bit count.
 * @param[in] tp Pointer to the EDG `tk_fixed_point` type structure.
 * @param[out] out_frac_bits Receives the number of fractional bits (`F`).
 * @param[out] out_is_signed Receives `true` if the fixed-point type is signed.
 * @param[out] out_is_saturating Receives `true` if the fixed-point type is saturating (`_Sat`).
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if inputs are null or not `tk_fixed_point`.
 */
llvm_gen_be_error_t llvm_get_fixed_point_scale(
    a_type_ptr tp,
    unsigned* out_frac_bits,
    bool* out_is_signed,
    bool* out_is_saturating) noexcept;

/**
 * @brief Lowers an EDG floating-point type into a matching LLVM floating-point Type.
 * @details Handles half/fp16, bfloat16, float, double, x86_fp80, and fp128.
 * @param[in] edg_type Pointer to the EDG floating-point type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_float(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Lowers an EDG pointer or reference type into an LLVM opaque PointerType.
 * @details Constructs an opaque pointer using LLVM's unadorned pointer representation.
 * Supports non-default address spaces (e.g. __attribute__((address_space(N)))).
 * @param[in] edg_type Pointer to the EDG pointer type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_pointer(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Lowers an EDG array type into an LLVM ArrayType or PointerType.
 * @details Maps fixed-size arrays to llvm::ArrayType, zero-bound arrays to 0-length arrays,
 * and variable-length or incomplete arrays to opaque pointers.
 * @param[in] edg_type Pointer to the EDG array type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_array(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Lowers an EDG vector type into an LLVM VectorType.
 * @details Translates GNU/Clang vector types into llvm::FixedVectorType.
 * @param[in] edg_type Pointer to the EDG vector type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_vector(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Lowers an EDG struct, union, or class type into an LLVM StructType.
 * @details Generates packed structures matching EDG field byte offsets and tail padding.
 * Cyclic struct references are resolved using forward-declared opaque StructTypes.
 * @param[in] edg_type Pointer to the EDG struct, union, or class type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_struct(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Lowers an EDG routine (function) type into an LLVM FunctionType.
 * @details Gathers return type, parameter types, and variadic ellipsis flag.
 * Dispatches to target-specific ABI handlers to perform sret and pass-by-value adjustments.
 * @param[in] edg_type Pointer to the EDG routine type structure.
 * @param[out] out_fn_type Pointer to the variable where the resulting FunctionType* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_type_from_routine(
    a_type_ptr edg_type,
    llvm::FunctionType** out_fn_type) noexcept;

/**
 * @brief Maps an EDG routine type's calling convention to an LLVM CallingConv::ID.
 * @details Examines the routine type supplement and maps conventions like cdecl, fastcall, stdcall, etc.
 * @param[in] edg_type Pointer to the EDG routine type structure.
 * @param[out] out_cc Pointer to the variable where the resulting LLVM calling convention ID is stored.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_calling_conv_from_routine(
    a_type_ptr edg_type,
    unsigned* out_cc) noexcept;

/**
 * @brief Legacy compatibility helper redirecting to llvm_type_from_edg_type.
 * @details Convenience wrapper returning llvm::Type* while inspecting error codes internally.
 * @param[in] edg_type Pointer to the EDG type structure.
 * @param[out] out_type Pointer to the variable where the resulting llvm::Type* is stored.
 * @return llvm_gen_be_error_t::ok on success, or an error code on failure.
 */
llvm_gen_be_error_t get_llvm_type(
    a_type_ptr edg_type,
    llvm::Type** out_type) noexcept;

/**
 * @brief Constructs or retrieves the LLVM TBAA type descriptor `MDNode` for an EDG type.
 * @details Creates the `"Simple C/C++ TBAA"` root, `"omnipotent char"` node, scalar type nodes,
 *          and struct path descriptor nodes using `llvm::MDBuilder`.
 * @param[in] type Pointer to the EDG type structure.
 * @param[out] out_node Receives the constructed `llvm::MDNode*` type descriptor.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_node` is null.
 */
llvm_gen_be_error_t llvm_get_tbaa_type_node(
    a_type_ptr type,
    llvm::MDNode** out_node) noexcept;

/**
 * @brief Constructs an LLVM `!tbaa` access tag `MDNode` for a memory load or store.
 * @details Combines the base type node, scalar access type node, and byte offset via `MDBuilder::createTBAAAccessTag`.
 * @param[in] base_type Pointer to the base EDG type (struct or scalar).
 * @param[in] access_type Pointer to the accessed scalar/pointer EDG type.
 * @param[in] offset Byte offset of the access within `base_type`.
 * @param[out] out_tag Receives the constructed `!tbaa` access tag `llvm::MDNode*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_tag` is null.
 */
llvm_gen_be_error_t llvm_get_tbaa_access_tag(
    a_type_ptr base_type,
    a_type_ptr access_type,
    uint64_t offset,
    llvm::MDNode** out_tag) noexcept;

/**
 * @brief Constructs an LLVM `!tbaa.struct` metadata node for aggregate memory copy operations.
 * @details Builds field offset/size/tag triples for all non-bitfield fields of `struct_type`.
 * @param[in] struct_type Pointer to the EDG struct/class type.
 * @param[out] out_node Receives the constructed `!tbaa.struct` `llvm::MDNode*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_node` is null.
 */
llvm_gen_be_error_t llvm_get_tbaa_struct_node(
    a_type_ptr struct_type,
    llvm::MDNode** out_node) noexcept;

/**
 * @brief Resolves a symbolic GPU/target address-space name (`shared`, `constant`, `global`, `local`, `generic`) to an LLVM address space ID.
 * @details Maps address-space names for `nvptx64`, `amdgcn`, and `spirv64` target triples (or generic targets).
 * @param[in] space_name Null-terminated address-space name (`"shared"`, `"constant"`, `"global"`, `"local"`, `"generic"`).
 * @param[out] out_addr_space Receives the target-specific LLVM address space integer.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if `out_addr_space` is null.
 */
llvm_gen_be_error_t llvm_resolve_target_address_space(
    const char* space_name,
    unsigned* out_addr_space) noexcept;

/**
 * @brief Determines whether an EDG type represents an IEEE-754 decimal floating-point type (`_Decimal32`, `_Decimal64`, `_Decimal128`).
 * @details Inspects typedef chains (`tk_typeref`) for canonical decimal floating-point type names and returns the byte width (`4`, `8`, `16`, or `0` if not a decimal float).
 * @param[in] edg_type Pointer to the EDG type to inspect (may be `nullptr`).
 * @param[out] out_byte_width Receives `4`, `8`, or `16` for decimal floating-point types, or `0` otherwise.
 * @return `llvm_gen_be_error_t::ok` on success, or `invalid_argument` if `out_byte_width` is null.
 */
llvm_gen_be_error_t llvm_get_decimal_float_byte_width(
    a_type_ptr edg_type,
    unsigned* out_byte_width) noexcept;

/**
 * @brief Lowers an IEEE-754 decimal floating-point byte width (`4`, `8`, or `16`) to its BID/DPD integer carrier `llvm::Type*`.
 * @details Maps `_Decimal32` (4 bytes) to `i32`, `_Decimal64` (8 bytes) to `i64`, and `_Decimal128` (16 bytes) to `i128`.
 * @param[in] byte_width Byte width of the decimal floating-point type (`4`, `8`, or `16`).
 * @param[out] out_type Receives the corresponding LLVM integer carrier type.
 * @return `llvm_gen_be_error_t::ok` on success, `invalid_argument` if `out_type` is null, or `unsupported_type` if `byte_width` is invalid.
 */
llvm_gen_be_error_t llvm_lower_decimal_float_type(
    unsigned byte_width,
    llvm::Type** out_type) noexcept;

/**
 * @brief Builds the target-ABI coerced `llvm::FunctionType*` and `llvm::AttributeList` for an EDG routine type.
 * @details Unifies ABI coercion (`sret`, `byval`, `inreg`, `zext`/`sext` on narrow `i8`/`i16`/`bool` integers,
 *          HFA/eightbyte packing, and variadic parameter expansion) across function definitions, direct calls,
 *          indirect function-pointer calls, and virtual dispatch/thunks.
 * @param[in] routine_edg_type Pointer to the EDG `tk_routine` type descriptor.
 * @param[in] is_variadic_call True when building an expanded call-site signature for a variadic (`...`) call.
 * @param[in] num_fixed_args Number of fixed (non-variadic) formal parameters.
 * @param[in] call_arg_types EDG types of all actual call-site arguments (used for variadic `...` classification).
 * @param[out] out_fn_ty Receives the target-ABI lowered `llvm::FunctionType*`.
 * @param[out] out_attrs Receives the target-ABI `llvm::AttributeList`.
 * @return `llvm_gen_be_error_t::ok` on success, or an error code on invalid/unsupported types.
 */
llvm_gen_be_error_t llvm_build_target_abi_function_type(
    a_type_ptr routine_edg_type,
    bool is_variadic_call,
    unsigned num_fixed_args,
    const std::vector<a_type_ptr>& call_arg_types,
    llvm::FunctionType** out_fn_ty,
    llvm::AttributeList* out_attrs) noexcept;

/**
 * @brief Computes both the callee declaration `FunctionType*` and the full call-site coerced parameter/attribute schedule.
 * @details For non-variadic calls, `out_call_param_tys` matches `(*out_callee_fn_ty)->params()`. For variadic (`...`)
 *          calls with extra arguments beyond @p num_fixed_args, `*out_callee_fn_ty` retains the fixed variadic prototype
 *          while `out_call_param_tys` and `out_call_attrs` include target-ABI coerced types and `byval`/`zext`/`sext`
 *          attributes for every variadic argument.
 * @param[in] routine_edg_type Pointer to the EDG `tk_routine` type descriptor.
 * @param[in] is_variadic_call True when the routine has an ellipsis (`...`).
 * @param[in] num_fixed_args Number of fixed formal parameters before `...`.
 * @param[in] call_arg_types EDG types of all actual call-site arguments.
 * @param[out] out_callee_fn_ty Receives the callee `llvm::FunctionType*`.
 * @param[out] out_call_param_tys Receives the full list of expected LLVM parameter types for all call-site arguments.
 * @param[out] out_call_attrs Receives the combined call-site `llvm::AttributeList`.
 * @return `llvm_gen_be_error_t::ok` on success, or an error code on failure.
 */
llvm_gen_be_error_t llvm_build_target_abi_call_schedule(
    a_type_ptr routine_edg_type,
    bool is_variadic_call,
    unsigned num_fixed_args,
    const std::vector<a_type_ptr>& call_arg_types,
    llvm::FunctionType** out_callee_fn_ty,
    std::vector<llvm::Type*>* out_call_param_tys,
    llvm::AttributeList* out_call_attrs) noexcept;

END_EDG_NAMESPACE

#endif /* LLVM_GEN_BE_TYPE_H */
