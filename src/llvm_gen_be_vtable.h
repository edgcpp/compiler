/**
 * @defgroup llvm_gen_be_cxx_abi C++ ABI, VTables, RTTI & Pointers-to-Members
 * @ingroup llvm_gen_be
 * @brief Itanium and MSVC C++ ABI vtables, VTTs, RTTI, and pointer-to-member lowering.
 */

/**
 * @file llvm_gen_be_vtable.h
 * @ingroup llvm_gen_be_cxx_abi
 * @brief Virtual Table (vtable) and dispatch generation for the EDG LLVM backend.
 * @details Generates vtables, VTTs, construction vtables, adjustment thunks, and virtual
 * function dispatch for both the Itanium and Microsoft MSVC C++ ABIs.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#ifndef LLVM_GEN_BE_VTABLE_H
#define LLVM_GEN_BE_VTABLE_H 1

#include "basic_hdrs.h"
#include "llvm_gen_be_error.h"
#include <llvm/IR/Constant.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>
#include <vector>
#include <string>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @struct llvm_thunk_info_t
 * @brief Represents an adjustment thunk for virtual function calls.
 * @details Encapsulates the adjustment offsets and target function for non-virtual
 * or virtual base adjustments under the Itanium and MSVC C++ ABIs.
 */
struct llvm_thunk_info_t {
  int64_t this_offset;            ///< Non-virtual offset adjustment for 'this' pointer (delta).
  int64_t vcall_offset_index;     ///< VCall offset index in vtable if adjusting through virtual base (-1 if non-virtual).
  int64_t return_offset;          ///< Covariant return type adjustment offset.
  int64_t return_vbase_offset_index; ///< VBase offset index for covariant return adjustment (-1 if non-virtual).
  a_routine_ptr target_routine;   ///< Target method routine to branch or tail-call to.
  bool is_virtual;                ///< True if this thunk requires vcall offset loading from vtable.
};

/**
 * @struct llvm_vtable_builder_t
 * @brief Engine for computing Itanium and MSVC C++ ABI virtual method table layouts.
 * @details Computes offset-to-top, RTTI descriptors, virtual base offsets, vcall offsets,
 * and method slot tables for primary, secondary, and construction vtables.
 */
struct llvm_vtable_builder_t {
  a_type_ptr class_type;                      ///< EDG class AST type pointer being laid out.
  int64_t offset_to_top;                      ///< Offset from subobject to complete object top.
  bool is_primary;                            ///< Whether this is the primary vtable for the complete class.
  std::vector<int64_t> vcall_offsets;         ///< VCall offsets for virtual base functions.
  std::vector<int64_t> vbase_offsets;         ///< Virtual base offsets preceding the address point.
  std::vector<llvm::Constant*> method_slots;  ///< Function pointers or adjustment thunks in the vtable.
  std::vector<llvm_thunk_info_t> thunks;      ///< Generated adjustment thunks for overridden methods.
};

/**
 * @brief Initializes a virtual table builder structure.
 * @details Sets up the builder with initial class type, offset-to-top, and primary flag.
 * @param[out] builder Pointer to the vtable builder instance to initialize.
 * @param[in] class_type Pointer to the EDG class AST type.
 * @param[in] offset_to_top Offset from the subobject to the top of the complete class.
 * @param[in] is_primary True if this is the primary vtable for the class.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if pointers are null.
 */
llvm_gen_be_error_t llvm_vtable_builder_init(
    llvm_vtable_builder_t* builder,
    a_type_ptr class_type,
    int64_t offset_to_top,
    bool is_primary) noexcept;

/**
 * @brief Computes the virtual method table layout for a class.
 * @details Populates the vcall offsets, vbase offsets, and method slots for the class.
 * @param[in,out] builder Pointer to the initialized vtable builder instance.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
llvm_gen_be_error_t llvm_vtable_builder_compute(
    llvm_vtable_builder_t* builder) noexcept;

/**
 * @brief Emits an adjustment thunk function.
 * @details Generates an LLVM IR function that adjusts the 'this' pointer and tail-calls the target routine.
 * @param[in] thunk Pointer to the thunk metadata structure.
 * @param[out] out_thunk_fn Receives the generated LLVM Function pointer on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_emit_thunk(
    const llvm_thunk_info_t* thunk,
    llvm::Function** out_thunk_fn) noexcept;

/**
 * @brief Emits a virtual or covariant return adjustment thunk for a target routine and offset parameters.
 * @details Populates an `llvm_thunk_info_t` descriptor from @p target_routine, @p this_nv_offset,
 *          @p vcall_offset, @p ret_nv_offset, and @p vbase_ret_offset, and delegates to `llvm_emit_thunk`.
 * @param[in] target_routine Target overriding virtual method routine.
 * @param[in] this_nv_offset Non-virtual `this` pointer byte offset adjustment.
 * @param[in] vcall_offset Virtual base `vcall` offset slot index (`-1` if non-virtual).
 * @param[in] ret_nv_offset Non-virtual covariant return pointer byte offset adjustment.
 * @param[in] vbase_ret_offset Virtual base covariant return offset slot index (`-1` if non-virtual).
 * @param[out] out_thunk_fn Receives the generated or cached `llvm::Function*` for the thunk.
 * @return llvm_gen_be_error_t::ok on success, or an error code from `llvm_emit_thunk`.
 */
llvm_gen_be_error_t llvm_emit_virtual_thunk(
    a_routine_ptr target_routine,
    int64_t this_nv_offset,
    int64_t vcall_offset,
    int64_t ret_nv_offset,
    int64_t vbase_ret_offset,
    llvm::Function** out_thunk_fn) noexcept;

/**
 * @brief Emits a Microsoft MSVC `vtordisp` virtual base adjustment thunk.
 * @details Loads the 32-bit `vtordisp` displacement stored at `[this - 4]`, subtracts both
 *          `vtordisp` and @p this_nv_offset from `this`, and tail-calls @p target_routine.
 * @param[in] target_routine Target overriding virtual method routine.
 * @param[in] vtordisp_offset Virtual base `vtordisp` slot offset.
 * @param[in] this_nv_offset Additional non-virtual `this` byte adjustment.
 * @param[out] out_thunk_fn Receives the generated or cached `llvm::Function*` for the `vtordisp` thunk.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if pointers are null.
 */
llvm_gen_be_error_t llvm_emit_msvc_vtordisp_thunk(
    a_routine_ptr target_routine,
    int32_t vtordisp_offset,
    int32_t this_nv_offset,
    llvm::Function** out_thunk_fn) noexcept;

/**
 * @brief Emits the virtual table for a given class.
 * @details Iterates over the class vtable entries and creates an LLVM GlobalVariable holding the vtable data.
 * @param[in] type The EDG AST type pointer for the class.
 * @param[out] out_const The resulting LLVM Constant pointer for the vtable global.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_emit_vtable(
    a_type_ptr type,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Emits the Virtual Table Table (VTT) for a given class.
 * @details Generates the secondary and construction vtable pointer array for classes with virtual bases.
 * @param[in] type The EDG AST type pointer for the class.
 * @param[out] out_const The resulting LLVM Constant pointer for the VTT global.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_emit_vtt(
    a_type_ptr type,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Emits a construction virtual table for a base subobject descriptor.
 * @details Mangles `_ZTC<derived><offset>_<base>`, computes `offset-to-top`, `vbase_offset`, and
 *          `vcall_offset` entries relative to @p subobject_base within @p most_derived_class, and
 *          populates virtual function pointers overridden within @p subobject_base's inheritance subgraph.
 * @param[in] most_derived_class The complete derived class type.
 * @param[in] subobject_base The base class subobject descriptor (`a_base_class_ptr`).
 * @param[out] out_ctbl Receives the resulting LLVM Constant pointer for the construction vtable global.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_emit_construction_vtable(
    a_type_ptr most_derived_class,
    a_base_class_ptr subobject_base,
    llvm::Constant** out_ctbl) noexcept;

/**
 * @brief Emits a construction virtual table for a subobject.
 * @details Creates a temporary vtable used during construction or destruction of base subobjects.
 * @param[in] derived_type The complete derived class type.
 * @param[in] base_type The base class subobject type being constructed.
 * @param[out] out_const The resulting LLVM Constant pointer for the construction vtable global.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_emit_construction_vtable(
    a_type_ptr derived_type,
    a_type_ptr base_type,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Emits a Microsoft MSVC virtual base table (vbtable).
 * @details Generates an array of 32-bit offsets to virtual base subobjects.
 * @param[in] type The EDG AST type pointer for the class.
 * @param[out] out_const Receives the LLVM Constant pointer for the vbtable global.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_emit_msvc_vbtable(
    a_type_ptr type,
    llvm::Constant** out_const) noexcept;

/**
 * @brief Lowers an Itanium ABI virtual method call.
 * @details Emits code to load the vptr, calculate the GEP for the offset, and load the function pointer.
 * @param[in] object_ptr The LLVM value pointing to the object instance.
 * @param[in] method_routine The EDG routine pointer for the virtual method.
 * @param[out] out_func_ptr The loaded virtual function pointer.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_virtual_dispatch(
    llvm::Value* object_ptr,
    a_routine_ptr method_routine,
    llvm::Value** out_func_ptr) noexcept;

/**
 * @brief Lowers a Microsoft MSVC ABI virtual method call.
 * @details Emits code to load the vfptr, calculate the method index GEP, and load the function pointer.
 * @param[in] object_ptr The LLVM value pointing to the object instance.
 * @param[in] method_routine The EDG routine pointer for the virtual method.
 * @param[out] out_func_ptr The loaded virtual function pointer.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_msvc_virtual_dispatch(
    llvm::Value* object_ptr,
    a_routine_ptr method_routine,
    llvm::Value** out_func_ptr) noexcept;

END_EDG_NAMESPACE
#endif

#endif /* LLVM_GEN_BE_VTABLE_H */
