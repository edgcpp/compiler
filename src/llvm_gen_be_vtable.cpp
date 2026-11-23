/**
 * @file llvm_gen_be_vtable.cpp
 * @ingroup llvm_gen_be_cxx_abi
 * @brief Virtual Table (vtable) and dispatch generation for the EDG LLVM backend.
 * @details Implements class hierarchy traversal, Itanium and MSVC ABI virtual method tables,
 * VTT arrays, construction vtables, adjustment thunks, and virtual method call dispatch.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_vtable.h"
#include "llvm_gen_be_rtti.h"
#include "lower_name.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/Instructions.h>
#include <llvm/TargetParser/Triple.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

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
    bool is_primary) noexcept {
  if (!builder || !class_type) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  builder->class_type = class_type;
  builder->offset_to_top = offset_to_top;
  builder->is_primary = is_primary;
  builder->vcall_offsets.clear();
  builder->vbase_offsets.clear();
  builder->method_slots.clear();
  builder->thunks.clear();
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Recursively gathers virtual function declarations from base classes and the current class.
 * @details Traverses direct base classes in declaration order followed by the class's own
 *          associated scope routines to populate virtual function slots with lowered routine signatures.
 * @param[in,out] builder Pointer to the active vtable builder instance.
 * @param[in] cls_type The EDG class type being inspected.
 * @param[in] num_vfuncs Total number of virtual function slots in the vtable.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
static llvm_gen_be_error_t gather_vfuncs_for_class(
    llvm_vtable_builder_t* builder,
    a_type_ptr cls_type,
    int num_vfuncs) noexcept {
  if (!cls_type) {
    return llvm_gen_be_error_t::ok;
  }
  cls_type = skip_typerefs(cls_type);
  a_class_type_supplement_ptr s = cls_type->variant.class_struct_union.extra_info;
  if (!s) {
    return llvm_gen_be_error_t::ok;
  }

  for (a_base_class_ptr b = s->direct_base_classes; b != nullptr; b = b->next_direct) {
    llvm_gen_be_error_t base_err = gather_vfuncs_for_class(builder, b->type, num_vfuncs);
    if (base_err != llvm_gen_be_error_t::ok) {
      return base_err;
    }
  }

  if (s->assoc_scope && s->assoc_scope->routines) {
    for (a_routine_ptr r = s->assoc_scope->routines; r != nullptr; r = r->next) {
      if (r->is_virtual) {
        int vnum = r->number.virtual_function;
        if (vnum > 0 && vnum <= num_vfuncs) {
#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN && IA64_ABI
          if (r->overriding_function_for_wrapper != nullptr ||
              r->delta != 0 || r->vcall_index != 0 ||
              r->return_delta != 0 || r->vbase_index != 0) {
            llvm_thunk_info_t thunk_info = {};
            thunk_info.this_offset = static_cast<int64_t>(r->delta);
            thunk_info.vcall_offset_index = (r->vcall_index != 0) ? static_cast<int64_t>(r->vcall_index) : -1;
            thunk_info.return_offset = static_cast<int64_t>(r->return_delta);
            thunk_info.return_vbase_offset_index = (r->vbase_index != 0) ? static_cast<int64_t>(r->vbase_index) : -1;
            thunk_info.target_routine = r->overriding_function_for_wrapper ? r->overriding_function_for_wrapper : r;
            thunk_info.is_virtual = (r->vcall_index != 0);
            builder->thunks.push_back(thunk_info);

            llvm::Function* thunk_fn = nullptr;
            LLVM_GEN_BE_CHECK(llvm_emit_virtual_thunk(
                thunk_info.target_routine,
                thunk_info.this_offset,
                thunk_info.vcall_offset_index,
                thunk_info.return_offset,
                thunk_info.return_vbase_offset_index,
                &thunk_fn));
            builder->method_slots[static_cast<size_t>(vnum - 1)] = thunk_fn;
            continue;
          }
#endif
          char* mangled = nullptr;
#if BACK_END_IS_LLVM_GEN_BE
          mangled = get_mangled_function_name(r);
#endif
          llvm::StringRef name_ref = mangled ? mangled : (r->source_corresp.name ? r->source_corresp.name : "vfunc");

          llvm::FunctionType* fty = nullptr;
          if (r->type) {
            llvm::Type* r_llvm_ty = nullptr;
            llvm_gen_be_error_t ty_err = get_llvm_type(r->type, &r_llvm_ty);
            if (ty_err != llvm_gen_be_error_t::ok) {
              return ty_err;
            }
            fty = llvm::dyn_cast_or_null<llvm::FunctionType>(r_llvm_ty);
          }
          if (!fty) {
            fty = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false);
          }

          llvm::FunctionCallee callee = be_state->module->getOrInsertFunction(name_ref, fty);
          builder->method_slots[static_cast<size_t>(vnum - 1)] = llvm::cast<llvm::Constant>(callee.getCallee());
        }
      }
    }
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Computes the virtual method table layout for a class.
 * @details Populates the vcall offsets, vbase offsets, and method slots for the class.
 * @param[in,out] builder Pointer to the initialized vtable builder instance.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
llvm_gen_be_error_t llvm_vtable_builder_compute(
    llvm_vtable_builder_t* builder) noexcept {
  if (!builder || !builder->class_type) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  a_type_ptr type = skip_typerefs(builder->class_type);
  if (type->kind != tk_class && type->kind != tk_struct) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  a_class_type_supplement_ptr supp = type->variant.class_struct_union.extra_info;
  int num_vfuncs = supp ? supp->highest_virtual_function_number : 0;
  llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);

  // Compute virtual base offsets if this class has virtual bases (both direct and indirect)
  if (supp) {
    std::vector<a_type_ptr> seen_vbases;
    if (supp->direct_base_classes) {
      for (a_base_class_ptr b = supp->direct_base_classes; b != nullptr; b = b->next_direct) {
        if (b->is_virtual) {
          seen_vbases.push_back(b->type ? skip_typerefs(b->type) : nullptr);
          builder->vbase_offsets.push_back(static_cast<int64_t>(b->offset));
          builder->vcall_offsets.push_back(-static_cast<int64_t>(b->offset));
        }
      }
    }
    for (a_base_class_ptr b = supp->base_classes; b != nullptr; b = b->next) {
      if (b->is_virtual) {
        a_type_ptr b_ty = skip_typerefs(b->type);
        bool already_seen = false;
        for (a_type_ptr sv : seen_vbases) {
          if (sv == b_ty) {
            already_seen = true;
            break;
          }
        }
        if (!already_seen) {
          seen_vbases.push_back(b_ty);
          builder->vbase_offsets.push_back(static_cast<int64_t>(b->offset));
          builder->vcall_offsets.push_back(-static_cast<int64_t>(b->offset));
        }
      }
    }
  }

  // Populate method slots (placeholders or real virtual routines)
  builder->method_slots.clear();
  builder->method_slots.resize(static_cast<size_t>(num_vfuncs), llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty)));

  return gather_vfuncs_for_class(builder, type, num_vfuncs);
}

/**
 * @brief Emits an adjustment thunk function.
 * @details Generates an LLVM IR function that adjusts the 'this' pointer and tail-calls the target routine.
 * @param[in] thunk Pointer to the thunk metadata structure.
 * @param[out] out_thunk_fn Receives the generated LLVM Function pointer on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_emit_thunk(
    const llvm_thunk_info_t* thunk,
    llvm::Function** out_thunk_fn) noexcept {
  if (!thunk || !out_thunk_fn) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());

  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  std::string thunk_name = "_Z";
  if (thunk->return_offset != 0 || thunk->return_vbase_offset_index != -1) {
      thunk_name += "Tc";
  } else {
      thunk_name += "T";
  }
  
  if (thunk->is_virtual && thunk->vcall_offset_index != -1) {
      thunk_name += "v";
  } else {
      thunk_name += "h";
      if (thunk->this_offset < 0) thunk_name += "n";
  }
  thunk_name += std::to_string(std::abs(thunk->this_offset)) + "_";
  
  if (thunk->is_virtual && thunk->vcall_offset_index != -1) {
      thunk_name += std::to_string(thunk->vcall_offset_index * 8) + "_";
  }
  
  if (thunk->return_offset != 0 || thunk->return_vbase_offset_index != -1) {
      if (thunk->return_vbase_offset_index != -1) {
          thunk_name += "v";
      } else {
          thunk_name += "h";
      }
      if (thunk->return_offset < 0) thunk_name += "n";
      thunk_name += std::to_string(std::abs(thunk->return_offset)) + "_";
      if (thunk->return_vbase_offset_index != -1) {
          thunk_name += std::to_string(thunk->return_vbase_offset_index * 8) + "_";
      }
  }
  
  if (thunk->target_routine && thunk->target_routine->source_corresp.name) {
      thunk_name += thunk->target_routine->source_corresp.name;
  } else {
      thunk_name += "routine";
  }

  if (llvm::Function* existing_fn = be_state->module->getFunction(thunk_name)) {
    *out_thunk_fn = existing_fn;
    return llvm_gen_be_error_t::ok;
  }

  llvm::FunctionType* thunk_ty = nullptr;
  llvm::AttributeList thunk_attrs;
  if (thunk->target_routine && thunk->target_routine->type) {
    std::vector<a_type_ptr> empty_args;
    llvm_gen_be_error_t ty_err = llvm_build_target_abi_function_type(
        thunk->target_routine->type, false, 0, empty_args, &thunk_ty, &thunk_attrs);
    if (ty_err != llvm_gen_be_error_t::ok) {
      thunk_ty = nullptr;
      thunk_attrs = llvm::AttributeList();
    }
  }
  bool has_sret_param = thunk_ty && thunk_attrs.hasParamAttr(0, llvm::Attribute::StructRet);
  unsigned this_param_idx = has_sret_param ? 1U : 0U;
  if (!thunk_ty || thunk_ty->getNumParams() <= this_param_idx ||
      !thunk_ty->getParamType(this_param_idx)->isPointerTy()) {
    thunk_ty = llvm::FunctionType::get(ptr_ty, {ptr_ty}, false);
    thunk_attrs = llvm::AttributeList();
    this_param_idx = 0U;
  }
  llvm::Function* thunk_fn = llvm::Function::Create(
      thunk_ty,
      llvm::GlobalValue::LinkOnceODRLinkage,
      thunk_name,
      be_state->module.get());
  if (!thunk_attrs.isEmpty()) {
    thunk_fn->setAttributes(thunk_attrs);
  }

  llvm::BasicBlock* bb = llvm::BasicBlock::Create(*be_state->context, "entry", thunk_fn);
  llvm::IRBuilder<> builder{bb};

  llvm::Value* this_arg = thunk_fn->getArg(this_param_idx);
  llvm::Value* adjusted_this = this_arg;

  if (thunk->is_virtual && thunk->vcall_offset_index != -1) {
    // Virtual base adjustment: load vtable, load vcall offset from negative index, and adjust
    llvm::Value* vptr = builder.CreateLoad(ptr_ty, this_arg, "vptr");
    llvm::Value* idx = llvm::ConstantInt::getSigned(
        llvm::Type::getInt64Ty(*be_state->context), thunk->vcall_offset_index);
    llvm::Value* vcall_slot = builder.CreateInBoundsGEP(ptr_ty, vptr, idx, "vcall_slot");
    llvm::Value* vcall_offset = builder.CreateLoad(
        llvm::Type::getInt64Ty(*be_state->context), vcall_slot, "vcall_offset");
    adjusted_this = builder.CreateInBoundsGEP(
        llvm::Type::getInt8Ty(*be_state->context), this_arg, vcall_offset, "adj_this_vbase");
  }

  if (thunk->this_offset != 0) {
    llvm::Value* delta = llvm::ConstantInt::getSigned(
        llvm::Type::getInt64Ty(*be_state->context), thunk->this_offset);
    adjusted_this = builder.CreateInBoundsGEP(
        llvm::Type::getInt8Ty(*be_state->context), adjusted_this, delta, "adj_this");
  }

  // Find or create target function declaration
  llvm::FunctionCallee target_callee = be_state->module->getOrInsertFunction(
      (thunk->target_routine && thunk->target_routine->source_corresp.name) ?
          thunk->target_routine->source_corresp.name : "target_func",
      thunk_ty,
      thunk_attrs);

  std::vector<llvm::Value*> call_args;
  for (unsigned idx = 0; idx < thunk_fn->arg_size(); ++idx) {
    call_args.push_back((idx == this_param_idx) ? adjusted_this : static_cast<llvm::Value*>(thunk_fn->getArg(idx)));
  }

  llvm::CallInst* call = builder.CreateCall(target_callee, call_args);
  if (!thunk_attrs.isEmpty()) {
    call->setAttributes(thunk_attrs);
  }
  bool needs_covariant_adj =
      (thunk->return_offset != 0 || thunk->return_vbase_offset_index != -1) &&
      call->getType()->isPointerTy();
  if (thunk_ty->isVarArg() && !needs_covariant_adj) {
    call->setTailCallKind(llvm::CallInst::TCK_MustTail);
  } else {
    call->setTailCallKind(llvm::CallInst::TCK_Tail);
  }

  llvm::Value* adj_ret = call;
  if (needs_covariant_adj) {
      llvm::BasicBlock* orig_bb = builder.GetInsertBlock();
      llvm::BasicBlock* nonnull_bb = llvm::BasicBlock::Create(*be_state->context, "cov.nonnull", thunk_fn);
      llvm::BasicBlock* done_bb = llvm::BasicBlock::Create(*be_state->context, "cov.done", thunk_fn);
      llvm::Value* is_nonnull = builder.CreateIsNotNull(call, "cov.is_nonnull");
      builder.CreateCondBr(is_nonnull, nonnull_bb, done_bb);

      builder.SetInsertPoint(nonnull_bb);
      llvm::Value* nonnull_adj = call;
      if (thunk->return_vbase_offset_index != -1) {
          llvm::Value* ret_vptr = builder.CreateLoad(ptr_ty, nonnull_adj, "ret_vptr");
          llvm::Value* idx = llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(*be_state->context), thunk->return_vbase_offset_index);
          llvm::Value* vbase_slot = builder.CreateInBoundsGEP(ptr_ty, ret_vptr, idx, "ret_vbase_slot");
          llvm::Value* vbase_offset = builder.CreateLoad(llvm::Type::getInt64Ty(*be_state->context), vbase_slot, "ret_vbase_offset");
          nonnull_adj = builder.CreateInBoundsGEP(llvm::Type::getInt8Ty(*be_state->context), nonnull_adj, vbase_offset, "adj_ret_vbase");
      }
      if (thunk->return_offset != 0) {
          llvm::Value* ret_delta = llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(*be_state->context), thunk->return_offset);
          nonnull_adj = builder.CreateInBoundsGEP(llvm::Type::getInt8Ty(*be_state->context), nonnull_adj, ret_delta, "adj_ret");
      }
      builder.CreateBr(done_bb);

      builder.SetInsertPoint(done_bb);
      llvm::PHINode* cov_phi = builder.CreatePHI(ptr_ty, 2, "cov.ret");
      cov_phi->addIncoming(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty)), orig_bb);
      cov_phi->addIncoming(nonnull_adj, nonnull_bb);
      adj_ret = cov_phi;
  }
  if (thunk_ty->getReturnType()->isVoidTy()) {
      builder.CreateRetVoid();
  } else {
      builder.CreateRet(adj_ret);
  }

  *out_thunk_fn = thunk_fn;
  return llvm_gen_be_error_t::ok;
}

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
    llvm::Function** out_thunk_fn) noexcept {
  llvm_thunk_info_t info = {};
  info.this_offset = this_nv_offset;
  info.vcall_offset_index = vcall_offset;
  info.return_offset = ret_nv_offset;
  info.return_vbase_offset_index = vbase_ret_offset;
  info.target_routine = target_routine;
  info.is_virtual = (vcall_offset != -1);
  return llvm_emit_thunk(&info, out_thunk_fn);
}

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
    llvm::Function** out_thunk_fn) noexcept {
  if (!target_routine || !out_thunk_fn) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  const char* base_name = target_routine->source_corresp.name
                              ? target_routine->source_corresp.name
                              : "vtordisp_target";
  std::string thunk_name = "?vtordisp$" + std::to_string(vtordisp_offset) + "$" +
                           std::to_string(this_nv_offset) + "$" + base_name;
  if (llvm::Function* existing = be_state->module->getFunction(thunk_name)) {
    *out_thunk_fn = existing;
    return llvm_gen_be_error_t::ok;
  }

  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
  llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
  llvm::FunctionType* fn_ty = llvm::FunctionType::get(ptr_ty, {ptr_ty}, false);
  llvm::Function* thunk_fn = llvm::Function::Create(
      fn_ty, llvm::GlobalValue::LinkOnceODRLinkage, thunk_name, be_state->module.get());
  llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(*be_state->context, "entry", thunk_fn);
  llvm::IRBuilder<> bld(entry_bb);

  llvm::Value* this_ptr = thunk_fn->getArg(0);
  llvm::Value* vtordisp_slot = bld.CreateInBoundsGEP(
      i8_ty, this_ptr, llvm::ConstantInt::getSigned(i32_ty, -4), "vtordisp.slot");
  llvm::Value* vtordisp_val = bld.CreateLoad(i32_ty, vtordisp_slot, "vtordisp.val");
  llvm::Value* neg_vtordisp = bld.CreateSub(
      llvm::ConstantInt::getSigned(i32_ty, -this_nv_offset), vtordisp_val, "vtordisp.adj");
  llvm::Value* adj_this = bld.CreateInBoundsGEP(i8_ty, this_ptr, neg_vtordisp, "adj.this");

  llvm::FunctionCallee callee = be_state->module->getOrInsertFunction(base_name, fn_ty);
  llvm::CallInst* call = bld.CreateCall(callee, {adj_this});
  call->setTailCallKind(llvm::CallInst::TCK_Tail);
  bld.CreateRet(call);

  *out_thunk_fn = thunk_fn;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits a Microsoft MSVC virtual base table for a primary class or base subobject.
 * @details Generates a global constant array of 32-bit offsets from the vbptr to each virtual base.
 * @param[in] vbtable_prefix The mangled vbtable prefix for the complete class.
 * @param[in] complete_supp Pointer to the complete class's extra supplement info.
 * @param[in] subobj_type Pointer to the subobject class type (or nullptr for the primary vbtable).
 * @param[in] subobj_offset Byte offset of the subobject within the complete class.
 * @param[out] out_gv Receives the generated or existing LLVM Constant for the vbtable.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
static llvm_gen_be_error_t emit_sub_vbtable(
    const std::string& vbtable_prefix,
    a_class_type_supplement_ptr complete_supp,
    a_type_ptr subobj_type,
    int32_t subobj_offset,
    llvm::Constant** out_gv) noexcept {
  std::string sub_name = vbtable_prefix;
  if (!subobj_type) {
    sub_name += "7B@";
  } else {
    char* sub_mangled = mangled_typeinfo_name(subobj_type);
    sub_name += "7B";
    sub_name += (sub_mangled ? sub_mangled : "sub");
    sub_name += "@";
  }

  llvm::StringRef name_ref = llvm::StringRef(sub_name);
  llvm::GlobalVariable* existing_gv = be_state->module->getNamedGlobal(name_ref);
  if (existing_gv) {
    *out_gv = existing_gv;
    return llvm_gen_be_error_t::ok;
  }

  llvm::Type* int32_ty = llvm::Type::getInt32Ty(*be_state->context);
  std::vector<llvm::Constant*> entries;
  entries.push_back(llvm::ConstantInt::get(int32_ty, 0));

  a_class_type_supplement_ptr target_supp =
      subobj_type ? subobj_type->variant.class_struct_union.extra_info : complete_supp;
  if (target_supp) {
    for (a_base_class_ptr vb = target_supp->base_classes; vb != nullptr; vb = vb->next) {
      if (vb->is_virtual) {
        int32_t vb_offset_in_complete = 0;
        for (a_base_class_ptr d_vb = complete_supp->base_classes; d_vb != nullptr; d_vb = d_vb->next) {
          if (d_vb->is_virtual && d_vb->type == vb->type) {
            vb_offset_in_complete = static_cast<int32_t>(d_vb->offset);
            break;
          }
        }
        entries.push_back(llvm::ConstantInt::getSigned(int32_ty, vb_offset_in_complete - subobj_offset));
      }
    }
  }

  llvm::ArrayType* arr_ty = llvm::ArrayType::get(int32_ty, entries.size());
  llvm::Constant* init = llvm::ConstantArray::get(arr_ty, entries);

  llvm::GlobalVariable* created_vtable_gv = nullptr;
  LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), arr_ty, true, llvm::GlobalValue::LinkOnceODRLinkage, init, name_ref, &created_vtable_gv));
  *out_gv = created_vtable_gv;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits a Microsoft MSVC virtual base table (vbtable).
 * @details Generates an array of 32-bit offsets to virtual base subobjects.
 * @param[in] type The EDG AST type pointer for the class.
 * @param[out] out_const Receives the LLVM Constant pointer for the vbtable global.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_emit_msvc_vbtable(
    a_type_ptr type,
    llvm::Constant** out_const) noexcept {
  if (!type || !out_const) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  type = skip_typerefs(type);
  if (type->kind != tk_class && type->kind != tk_struct) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  char* mangled = mangled_typeinfo_name(type);
  std::string vbtable_name = "??_8";
  vbtable_name += (mangled ? mangled : "class");

  a_class_type_supplement_ptr supp = type->variant.class_struct_union.extra_info;

  llvm::Constant* primary_vbtable = nullptr;
  LLVM_GEN_BE_CHECK(emit_sub_vbtable(vbtable_name, supp, nullptr, 0, &primary_vbtable));
  *out_const = primary_vbtable;

  // Emit subobject vbtables
  if (supp) {
    for (a_base_class_ptr b = supp->base_classes; b != nullptr; b = b->next) {
      if (!b->is_virtual && b->type->variant.class_struct_union.any_virtual_base_classes) {
        llvm::Constant* sub_vbtable = nullptr;
        LLVM_GEN_BE_CHECK(emit_sub_vbtable(vbtable_name, supp, b->type, static_cast<int32_t>(b->offset), &sub_vbtable));
      }
    }
  }

  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits the virtual table for a given class.
 * @details Iterates over the class vtable entries and creates an LLVM GlobalVariable holding the vtable data.
 * @param[in] type The EDG AST type pointer for the class.
 * @param[out] out_const The resulting LLVM Constant pointer for the vtable global.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_emit_vtable(a_type_ptr type, llvm::Constant** out_const) noexcept {
  if (!type || !out_const) return llvm_gen_be_error_t::invalid_argument;
  type = skip_typerefs(type);
  if (type->kind != tk_class && type->kind != tk_struct) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
  if (triple.isWindowsMSVCEnvironment()) {
    // MSVC vftable emission
    std::string vtable_name = "??_7";
    char* mangled_class = mangled_typeinfo_name(type);
    vtable_name += (mangled_class ? mangled_class : "class");
    vtable_name += "6B@";

    llvm::StringRef name_ref = llvm::StringRef(vtable_name);
    if (llvm::GlobalValue* existing_val = be_state->module->getNamedValue(name_ref)) {
      *out_const = existing_val;
      return llvm_gen_be_error_t::ok;
    }

    llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm_vtable_builder_t msvc_builder;
    LLVM_GEN_BE_CHECK(llvm_vtable_builder_init(&msvc_builder, type, 0, true));
    llvm_gen_be_error_t bld_err = llvm_vtable_builder_compute(&msvc_builder);
    if (bld_err != llvm_gen_be_error_t::ok) return bld_err;

    llvm::Constant* col_const = nullptr;
    LLVM_GEN_BE_CHECK(llvm_emit_msvc_rtti_complete_object_locator(type, &col_const));

    std::vector<llvm::Constant*> init_elements;
    init_elements.push_back(col_const);
    for (llvm::Constant* vf : msvc_builder.method_slots) {
      init_elements.push_back(vf);
    }

    llvm::ArrayType* arr_ty = llvm::ArrayType::get(int8_ptr_ty, init_elements.size());
    llvm::Constant* vtable_init = llvm::ConstantArray::get(arr_ty, init_elements);

    a_boolean has_unknown_key_func = FALSE;
    a_routine_ptr key_func = vtbl_decider_function_for_class(type, &has_unknown_key_func);
    llvm::GlobalValue::LinkageTypes linkage = llvm::GlobalValue::LinkOnceODRLinkage;
    if (key_func && key_func->function_def_number) {
        linkage = llvm::GlobalValue::ExternalLinkage;
    }

    std::string storage_name = vtable_name + ".rtti_storage";
    llvm::GlobalVariable* storage_gv = nullptr;
    LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), arr_ty, true, llvm::GlobalValue::PrivateLinkage, vtable_init, storage_name, &storage_gv));

    llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
    llvm::Constant* zero_idx = llvm::ConstantInt::get(i32_ty, 0);
    llvm::Constant* one_idx = llvm::ConstantInt::get(i32_ty, 1);
    llvm::Constant* vftable_addr = llvm::ConstantExpr::getInBoundsGetElementPtr(
        arr_ty, storage_gv, llvm::ArrayRef<llvm::Constant*>({zero_idx, one_idx}));
    llvm::GlobalAlias* vftable_alias = llvm::GlobalAlias::create(
        int8_ptr_ty, 0, linkage, name_ref, vftable_addr, be_state->module.get());

    // Emit secondary ??_7...6B<Base>@@ vftables for multiple inheritance
    a_class_type_supplement_ptr msvc_supp = type->variant.class_struct_union.extra_info;
    if (msvc_supp) {
      for (a_base_class_ptr b = msvc_supp->base_classes; b != nullptr; b = b->next) {
        if (!b->is_virtual && b->offset > 0) {
          char* base_mangled = mangled_typeinfo_name(b->type);
          std::string sec_vft_name = "??_7";
          sec_vft_name += (mangled_class ? mangled_class : "class");
          sec_vft_name += "6B";
          sec_vft_name += (base_mangled ? base_mangled : "base");
          sec_vft_name += "@";
          if (!be_state->module->getNamedValue(sec_vft_name)) {
            LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), arr_ty, true, linkage, vtable_init, sec_vft_name));
          }
        }
      }
    }

    // Also emit vbtable if class has virtual bases
    if (type->variant.class_struct_union.any_virtual_base_classes) {
      llvm::Constant* vbtable_const = nullptr;
      LLVM_GEN_BE_CHECK(llvm_emit_msvc_vbtable(type, &vbtable_const));
    }

    *out_const = vftable_alias;
    return llvm_gen_be_error_t::ok;
  }

  // Itanium ABI vtable emission
  char* m_vtbl = mangled_vtbl_name(type, nullptr, nullptr);
  std::string vtable_name = m_vtbl ? m_vtbl : "_ZTV_unknown";

  llvm::StringRef name_ref = llvm::StringRef(vtable_name);
  llvm::GlobalVariable* gv = be_state->module->getNamedGlobal(name_ref);
  if (gv) {
    *out_const = gv;
    return llvm_gen_be_error_t::ok;
  }

  llvm_vtable_builder_t builder;
  LLVM_GEN_BE_CHECK(llvm_vtable_builder_init(&builder, type, 0, true));
  llvm_gen_be_error_t bld_err = llvm_vtable_builder_compute(&builder);
  if (bld_err != llvm_gen_be_error_t::ok) return bld_err;

  std::vector<llvm::Constant*> init_elements;
  llvm::Type* ptrdiff_ty = llvm::Type::getInt64Ty(*be_state->context);

  // VBase offsets preceding address point
  for (int64_t vbase_off : builder.vbase_offsets) {
    init_elements.push_back(llvm::ConstantInt::getSigned(ptrdiff_ty, vbase_off));
  }

  // VCall offsets for virtual bases
  for (int64_t vcall_off : builder.vcall_offsets) {
    init_elements.push_back(llvm::ConstantInt::getSigned(ptrdiff_ty, vcall_off));
  }

  // 1. Primary VTable
  // offset-to-top (0 for primary)
  init_elements.push_back(llvm::ConstantInt::getSigned(ptrdiff_ty, builder.offset_to_top));

  // RTTI descriptor pointer
  llvm::Constant* ti = nullptr;
  LLVM_GEN_BE_CHECK(get_typeinfo_global(type, &ti));
  init_elements.push_back(ti);

  // Virtual function pointers
  for (llvm::Constant* vf : builder.method_slots) {
    init_elements.push_back(vf);
  }

  // 2. Secondary VTables for multiple inheritance
  a_class_type_supplement_ptr supp = type->variant.class_struct_union.extra_info;
  if (supp) {
    for (a_base_class_ptr b = supp->base_classes; b != nullptr; b = b->next) {
      bool is_primary = (supp->virtual_function_info_base_class == b);
      if (!is_primary && b->offset > 0) {
        if (b->is_virtual) {
          // VCall offset for virtual base relative to complete object
          init_elements.push_back(llvm::ConstantInt::getSigned(ptrdiff_ty, -static_cast<int64_t>(b->offset)));
        }
        // Secondary vtable offset-to-top (negative of base offset)
        init_elements.push_back(llvm::ConstantInt::getSigned(ptrdiff_ty, -static_cast<int64_t>(b->offset)));
        init_elements.push_back(ti);

        llvm_vtable_builder_t sec_builder;
        if (b->type) {
          LLVM_GEN_BE_CHECK(llvm_vtable_builder_init(&sec_builder, b->type, -static_cast<int64_t>(b->offset), false));
          if (llvm_vtable_builder_compute(&sec_builder) == llvm_gen_be_error_t::ok) {
            for (size_t s_idx = 0; s_idx < sec_builder.method_slots.size(); ++s_idx) {
              llvm::Constant* slot_fn = sec_builder.method_slots[s_idx];
              if (s_idx < builder.method_slots.size() &&
                  !llvm::isa<llvm::ConstantPointerNull>(builder.method_slots[s_idx])) {
                slot_fn = builder.method_slots[s_idx];
              }
              init_elements.push_back(slot_fn);
            }
          }
        }
      }
    }
  }

  std::vector<llvm::Type*> element_types;
  for (auto* el : init_elements) {
    element_types.push_back(el->getType());
  }

  llvm::StructType* vtable_ty = llvm::StructType::get(*be_state->context, element_types);
  llvm::Constant* vtable_init = llvm::ConstantStruct::get(vtable_ty, init_elements);

  a_boolean has_unknown_key_func = FALSE;
  a_routine_ptr key_func = vtbl_decider_function_for_class(type, &has_unknown_key_func);
  
  llvm::GlobalValue::LinkageTypes linkage = llvm::GlobalValue::LinkOnceODRLinkage;
  if (key_func && key_func->function_def_number) {
    linkage = llvm::GlobalValue::ExternalLinkage;
  }

  LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), vtable_ty, true, linkage, vtable_init, name_ref, &gv));

#if DO_IL_LOWERING
  llvm::Triple vtbl_triple = llvm::Triple(be_state->module->getTargetTriple());
  if (!vtbl_triple.isOSBinFormatMachO()) {
    llvm::Comdat* comdat = be_state->module->getOrInsertComdat(name_ref);
    comdat->setSelectionKind(vtbl_triple.isOSBinFormatCOFF() ? llvm::Comdat::Largest : llvm::Comdat::Any);
    gv->setComdat(comdat);
  } else {
    gv->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
  }
#endif

  *out_const = gv;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Computes the Itanium ABI vtable address point constant for a given vtable global and class type.
 * @details Offsets past any virtual base and vcall offset entries (`2` slots per virtual base) plus
 *          the `offset-to-top` and `RTTI` slots (`2` slots) to point at the first virtual method slot.
 * @param[in] vtbl_const The vtable global variable constant.
 * @param[in] cls_ty The EDG class type whose virtual base count determines the address point index.
 * @param[out] out_addr_pt Receives the computed `llvm::Constant*` address point.
 * @return llvm_gen_be_error_t::ok on success.
 */
static llvm_gen_be_error_t compute_vtable_address_point(
    llvm::Constant* vtbl_const,
    a_type_ptr cls_ty,
    llvm::Constant** out_addr_pt) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  auto* vtbl_gv = llvm::dyn_cast<llvm::GlobalVariable>(vtbl_const);
  if (!vtbl_gv) {
    *out_addr_pt = vtbl_const;
    return llvm_gen_be_error_t::ok;
  }
  llvm::Type* gv_val_ty = vtbl_gv->getValueType();
  uint64_t total_elems = 0;
  if (auto* st_ty = llvm::dyn_cast<llvm::StructType>(gv_val_ty)) {
    total_elems = st_ty->getNumElements();
  } else if (auto* arr_ty = llvm::dyn_cast<llvm::ArrayType>(gv_val_ty)) {
    total_elems = arr_ty->getNumElements();
  }
  unsigned addr_idx = 2;
  a_type_ptr resolved = skip_typerefs(cls_ty);
  a_class_type_supplement_ptr c_supp = resolved->variant.class_struct_union.extra_info;
  if (c_supp) {
    std::vector<a_type_ptr> seen_vbases;
    for (a_base_class_ptr vb = c_supp->direct_base_classes; vb != nullptr; vb = vb->next_direct) {
      if (vb->is_virtual) {
        seen_vbases.push_back(vb->type ? skip_typerefs(vb->type) : nullptr);
        addr_idx += 2; // vbase_offset + vcall_offset
      }
    }
    for (a_base_class_ptr vb = c_supp->base_classes; vb != nullptr; vb = vb->next) {
      if (vb->is_virtual) {
        a_type_ptr b_ty = skip_typerefs(vb->type);
        bool already_seen = false;
        for (a_type_ptr sv : seen_vbases) {
          if (sv == b_ty) {
            already_seen = true;
            break;
          }
        }
        if (!already_seen) {
          seen_vbases.push_back(b_ty);
          addr_idx += 2;
        }
      }
    }
  }
  if (addr_idx < total_elems) {
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
    *out_addr_pt = llvm::ConstantExpr::getInBoundsGetElementPtr(
        gv_val_ty,
        vtbl_gv,
        llvm::ArrayRef<llvm::Constant*>({
            llvm::ConstantInt::get(i32_ty, 0),
            llvm::ConstantInt::get(i32_ty, addr_idx)}));
    return llvm_gen_be_error_t::ok;
  }
  *out_addr_pt = vtbl_const;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits the Virtual Table Table (VTT) for a given class.
 * @details Generates the secondary and construction vtable pointer array for classes with virtual bases.
 * @param[in] type The EDG AST type pointer for the class.
 * @param[out] out_const The resulting LLVM Constant pointer for the VTT global.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_emit_vtt(a_type_ptr type, llvm::Constant** out_const) noexcept {
  if (!type || !out_const) return llvm_gen_be_error_t::invalid_argument;
  type = skip_typerefs(type);
  if (type->kind != tk_class && type->kind != tk_struct) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  char* m_vtt = mangled_virtual_table_table_name(type);
  std::string vtt_name = m_vtt ? m_vtt : "_ZTT_unknown";

  llvm::StringRef name_ref = llvm::StringRef(vtt_name);
  llvm::GlobalVariable* gv = be_state->module->getNamedGlobal(name_ref);
  if (gv) {
    *out_const = gv;
    return llvm_gen_be_error_t::ok;
  }

  std::vector<llvm::Constant*> init_elements;

  // VTTs contain pointers to VTables (primary, secondary, and construction vtables)
  llvm::Constant* primary_vtable = nullptr;
  llvm_gen_be_error_t err = llvm_emit_vtable(type, &primary_vtable);
  if (err != llvm_gen_be_error_t::ok) {
    return llvm_gen_be_error_t::vtable_generation_failure;
  }

  llvm::Constant* primary_addr_pt = nullptr;
  LLVM_GEN_BE_CHECK(compute_vtable_address_point(primary_vtable, type, &primary_addr_pt));
  init_elements.push_back(primary_addr_pt);

  // Populate secondary and construction vtable references in Itanium ABI §2.6.2 preorder:
  // 1. Primary virtual pointer for D (already added above).
  // 2. Secondary VTTs for each direct non-virtual and virtual base B in inheritance graph preorder (using _ZTC...).
  // 3. Secondary virtual pointers for D.
  // 4. Virtual VTTs for virtual bases in inheritance graph preorder.
  a_class_type_supplement_ptr supp = type->variant.class_struct_union.extra_info;
  if (supp) {
    for (a_base_class_ptr b = supp->direct_base_classes; b != nullptr; b = b->next_direct) {
      bool is_primary = (supp->virtual_function_info_base_class == b);
      bool is_poly_sub = (!is_primary && b->type && b->type->variant.class_struct_union.any_virtual_functions_including_in_base_classes);
      if (b->type && (b->is_virtual || is_poly_sub)) {
        llvm::Constant* c_vtable = nullptr;
        llvm_gen_be_error_t c_err = llvm_emit_construction_vtable(type, b, &c_vtable);
        if (c_err == llvm_gen_be_error_t::ok) {
          llvm::Constant* direct_addr_pt = nullptr;
          LLVM_GEN_BE_CHECK(compute_vtable_address_point(c_vtable, b->type, &direct_addr_pt));
          init_elements.push_back(direct_addr_pt);
        }
      }
    }
    for (a_base_class_ptr b = supp->base_classes; b != nullptr; b = b->next) {
      bool is_primary = (supp->virtual_function_info_base_class == b);
      bool is_poly_sub = (!is_primary && b->type->variant.class_struct_union.any_virtual_functions_including_in_base_classes);
      if (b->is_virtual || is_poly_sub) {
        llvm::Constant* c_vtable = nullptr;
        llvm_gen_be_error_t c_err = llvm_emit_construction_vtable(type, b, &c_vtable);
        if (c_err == llvm_gen_be_error_t::ok) {
          llvm::Constant* c_addr_pt = nullptr;
          LLVM_GEN_BE_CHECK(compute_vtable_address_point(c_vtable, b->type, &c_addr_pt));
          bool already_added = false;
          for (llvm::Constant* existing : init_elements) {
            if (existing == c_addr_pt) {
              already_added = true;
              break;
            }
          }
          if (!already_added) {
            init_elements.push_back(c_addr_pt);
          }
        }
      }
    }
  }

  std::vector<llvm::Type*> element_types;
  for (auto* el : init_elements) {
    element_types.push_back(el->getType());
  }

  llvm::StructType* vtt_ty = llvm::StructType::get(*be_state->context, element_types);
  llvm::Constant* vtt_init = llvm::ConstantStruct::get(vtt_ty, init_elements);

  LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), vtt_ty, true, llvm::GlobalValue::LinkOnceODRLinkage, vtt_init, name_ref, &gv));

#if DO_IL_LOWERING
  if (!llvm::Triple(be_state->module->getTargetTriple()).isOSBinFormatMachO()) {
    llvm::Comdat* comdat = be_state->module->getOrInsertComdat(name_ref);
    comdat->setSelectionKind(llvm::Comdat::Any);
    gv->setComdat(comdat);
  } else {
    gv->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
  }
#endif

  *out_const = gv;
  return llvm_gen_be_error_t::ok;
}

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
    llvm::Constant** out_ctbl) noexcept {
  if (!out_ctbl || !most_derived_class || !subobject_base || !subobject_base->type) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  most_derived_class = skip_typerefs(most_derived_class);
  a_type_ptr base_type = skip_typerefs(subobject_base->type);
  int64_t base_offset = static_cast<int64_t>(subobject_base->offset);

  char* edg_ctbl_name = mangled_vtbl_name(most_derived_class, subobject_base, subobject_base);
  std::string name;
  if (edg_ctbl_name && std::strncmp(edg_ctbl_name, "_ZTC", 4) == 0 &&
      std::strcmp(edg_ctbl_name, "_ZTCmock") != 0) {
    name = edg_ctbl_name;
  } else {
    auto strip_zti = [](const char* s, const char* fallback) noexcept -> const char* {
      if (!s || s[0] == '\0') return fallback;
      if (std::strncmp(s, "_ZTI", 4) == 0 && s[4] != '\0') return s + 4;
      return s;
    };
    name = "_ZTC";
    char* m_der = mangled_typeinfo_name(most_derived_class);
    char* m_base = mangled_typeinfo_name(base_type);
    name += strip_zti(m_der, "der");
    name += std::to_string(base_offset) + "_";
    name += strip_zti(m_base, "base");
  }

  llvm::StringRef name_ref = llvm::StringRef(name);
  llvm::GlobalVariable* gv = be_state->module->getNamedGlobal(name_ref);
  if (gv) {
    *out_ctbl = gv;
    return llvm_gen_be_error_t::ok;
  }

  llvm_vtable_builder_t ctor_builder;
  LLVM_GEN_BE_CHECK(llvm_vtable_builder_init(&ctor_builder, base_type, -base_offset, false));
  llvm_gen_be_error_t bld_err = llvm_vtable_builder_compute(&ctor_builder);
  if (bld_err != llvm_gen_be_error_t::ok) return bld_err;

  // Adjust vbase_offsets and vcall_offsets relative to subobject_base within most_derived_class layout
  a_class_type_supplement_ptr der_supp = most_derived_class->variant.class_struct_union.extra_info;
  a_class_type_supplement_ptr base_supp = base_type->variant.class_struct_union.extra_info;
  if (der_supp && base_supp) {
    size_t vb_idx = 0;
    for (a_base_class_ptr b = base_supp->direct_base_classes; b != nullptr; b = b->next_direct) {
      if (b->is_virtual) {
        for (a_base_class_ptr d_vb = der_supp->base_classes; d_vb != nullptr; d_vb = d_vb->next) {
          if (d_vb->is_virtual && skip_typerefs(d_vb->type) == skip_typerefs(b->type)) {
            int64_t rel_off = static_cast<int64_t>(d_vb->offset) - base_offset;
            ctor_builder.vbase_offsets[vb_idx] = rel_off;
            ctor_builder.vcall_offsets[vb_idx] = -rel_off;
            break;
          }
        }
        ++vb_idx;
      }
    }
  }

  std::vector<llvm::Constant*> init_elements;
  llvm::Type* ptrdiff_ty = llvm::Type::getInt64Ty(*be_state->context);

  for (int64_t vbase_off : ctor_builder.vbase_offsets) {
    init_elements.push_back(llvm::ConstantInt::getSigned(ptrdiff_ty, vbase_off));
  }
  for (int64_t vcall_off : ctor_builder.vcall_offsets) {
    init_elements.push_back(llvm::ConstantInt::getSigned(ptrdiff_ty, vcall_off));
  }
  init_elements.push_back(llvm::ConstantInt::getSigned(ptrdiff_ty, ctor_builder.offset_to_top));

  llvm::Constant* base_ti = nullptr;
  LLVM_GEN_BE_CHECK(get_typeinfo_global(base_type, &base_ti));
  init_elements.push_back(base_ti);

  for (llvm::Constant* vf : ctor_builder.method_slots) {
    init_elements.push_back(vf);
  }

  std::vector<llvm::Type*> element_types;
  for (auto* el : init_elements) {
    element_types.push_back(el->getType());
  }
  llvm::StructType* ctor_vtbl_ty = llvm::StructType::get(*be_state->context, element_types);
  llvm::Constant* ctor_vtbl_init = llvm::ConstantStruct::get(ctor_vtbl_ty, init_elements);

  LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), ctor_vtbl_ty, true, llvm::GlobalValue::LinkOnceODRLinkage, ctor_vtbl_init, name_ref, &gv));

#if DO_IL_LOWERING
  if (!llvm::Triple(be_state->module->getTargetTriple()).isOSBinFormatMachO()) {
    llvm::Comdat* comdat = be_state->module->getOrInsertComdat(name_ref);
    comdat->setSelectionKind(llvm::Comdat::Any);
    gv->setComdat(comdat);
  } else {
    gv->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
  }
#endif

  *out_ctbl = gv;
  return llvm_gen_be_error_t::ok;
}

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
    llvm::Constant** out_const) noexcept {
  if (!out_const || !derived_type || !base_type) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  derived_type = skip_typerefs(derived_type);
  base_type = skip_typerefs(base_type);

  a_base_class synth_base = {};
  synth_base.type = base_type;
  synth_base.offset = 0;
  a_class_type_supplement_ptr der_supp = derived_type->variant.class_struct_union.extra_info;
  if (der_supp) {
    for (a_base_class_ptr b = der_supp->base_classes; b != nullptr; b = b->next) {
      if (skip_typerefs(b->type) == base_type) {
        return llvm_emit_construction_vtable(derived_type, b, out_const);
      }
    }
    for (a_base_class_ptr b = der_supp->direct_base_classes; b != nullptr; b = b->next_direct) {
      if (skip_typerefs(b->type) == base_type) {
        return llvm_emit_construction_vtable(derived_type, b, out_const);
      }
    }
  }
  return llvm_emit_construction_vtable(derived_type, &synth_base, out_const);
}

/**
 * @brief Lowers an Itanium ABI virtual method call.
 * @details Emits code to load the vptr (and adjust `object_ptr` via `vbase_offset` when
 *          `method_routine->vcall_index != 0`), calculate the GEP for the offset, and load the function pointer.
 * @param[in] object_ptr The LLVM value pointing to the object instance.
 * @param[in] method_routine The EDG routine pointer for the virtual method.
 * @param[out] out_func_ptr The loaded virtual function pointer.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_virtual_dispatch(
    llvm::Value* object_ptr,
    a_routine_ptr method_routine,
    llvm::Value** out_func_ptr) noexcept {
  if (!out_func_ptr || !object_ptr || !method_routine) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
  llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);
  llvm::Value* vptr = be_state->builder->CreateLoad(ptr_ty, object_ptr, "vptr");

#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
  if (method_routine->vcall_index != 0) {
    llvm::Value* vbase_idx = llvm::ConstantInt::get(i64_ty, static_cast<uint64_t>(method_routine->vcall_index));
    llvm::Value* vbase_slot = be_state->builder->CreateInBoundsGEP(ptr_ty, vptr, vbase_idx, "vbase_slot");
    llvm::Value* vbase_off = be_state->builder->CreateLoad(i64_ty, vbase_slot, "vbase_off");
    llvm::Value* vbase_obj = be_state->builder->CreateInBoundsGEP(i8_ty, object_ptr, vbase_off, "vbase_obj");
    vptr = be_state->builder->CreateLoad(ptr_ty, vbase_obj, "vbase_vptr");
  }
#endif

  int vfunc_number = method_routine->number.virtual_function;
  uint64_t vtable_index = vfunc_number > 0 ? static_cast<uint64_t>(vfunc_number - 1) : static_cast<uint64_t>(0U);
  llvm::Value* idx = llvm::ConstantInt::get(i64_ty, vtable_index);
  llvm::Value* func_gep = be_state->builder->CreateInBoundsGEP(ptr_ty, vptr, idx, "vfunc_gep");

  llvm::LoadInst* loaded_func = be_state->builder->CreateLoad(ptr_ty, func_gep, "vfunc_load");

  // Attach devirtualization metadata for LLVM optimization passes
  llvm::MDNode* md_node = llvm::MDNode::get(
      *be_state->context,
      llvm::ConstantAsMetadata::get(
          llvm::ConstantInt::get(i64_ty, 0)));
  loaded_func->setMetadata(llvm::LLVMContext::MD_vcall_visibility, md_node);

  *out_func_ptr = loaded_func;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a Microsoft MSVC ABI virtual method call.
 * @details Emits code to load the vfptr (adjusting via `vbtable` when `method_routine->vcall_index != 0`),
 *          calculate the method index GEP, and load the function pointer.
 * @param[in] object_ptr The LLVM value pointing to the object instance.
 * @param[in] method_routine The EDG routine pointer for the virtual method.
 * @param[out] out_func_ptr The loaded virtual function pointer.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_msvc_virtual_dispatch(
    llvm::Value* object_ptr,
    a_routine_ptr method_routine,
    llvm::Value** out_func_ptr) noexcept {
  if (!out_func_ptr || !object_ptr || !method_routine) {
    return llvm_gen_be_error_t::invalid_argument;
  }

  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
  llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
  llvm::Value* target_obj = object_ptr;

#if DO_IL_LOWERING && ABI_CHANGES_FOR_COVARIANT_VIRTUAL_FUNC_RETURN
  if (method_routine->vcall_index != 0) {
    llvm::Value* vbptr = be_state->builder->CreateLoad(ptr_ty, object_ptr, "vbptr");
    llvm::Value* vb_idx = llvm::ConstantInt::get(i32_ty, static_cast<uint64_t>(method_routine->vcall_index));
    llvm::Value* vb_slot = be_state->builder->CreateInBoundsGEP(i32_ty, vbptr, vb_idx, "vbtable_slot");
    llvm::Value* vb_off = be_state->builder->CreateLoad(i32_ty, vb_slot, "vbase_disp");
    target_obj = be_state->builder->CreateInBoundsGEP(i8_ty, object_ptr, vb_off, "vbase_this");
  }
#endif

  llvm::Value* vfptr = be_state->builder->CreateLoad(ptr_ty, target_obj, "vfptr");

  int vfunc_number = method_routine->number.virtual_function;
  uint64_t slot_idx = vfunc_number > 0 ? static_cast<uint64_t>(vfunc_number - 1) : static_cast<uint64_t>(0U);
  llvm::Value* idx = llvm::ConstantInt::get(i32_ty, slot_idx);
  llvm::Value* slot_gep = be_state->builder->CreateInBoundsGEP(ptr_ty, vfptr, idx, "vfptr_slot");

  *out_func_ptr = be_state->builder->CreateLoad(ptr_ty, slot_gep, "vcall_thunk");
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
