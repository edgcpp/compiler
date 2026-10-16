/**
 * @file llvm_gen_be_rtti.cpp
 * @brief RTTI generation subsystem for the EDG LLVM backend.
 * @details Implements type_info descriptor emission for Itanium and MSVC ABIs,
 * dynamic_cast runtime dispatch with offset hints, and typeid lowering.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_rtti.h"
#include "llvm_gen_be_expr.h"
#include "lower_name.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instructions.h>
#include <llvm/TargetParser/Triple.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Generates or retrieves the LLVM global variable for a given type's RTTI type_info.
 * @details Generates `type_info` structures for fundamental types, pointers, classes, and derived classes,
 *          including Itanium ABI `__si_class_type_info` and `__vmi_class_type_info` descriptors,
 *          as well as MSVC TypeDescriptors.
 * @param[in] type The EDG AST type pointer.
 * @param[out] out_const Receives the resulting LLVM Constant pointer on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t get_typeinfo_global(a_type_ptr type, llvm::Constant** out_const) noexcept {
  if (!out_const) return llvm_gen_be_error_t::invalid_argument;
  if (!type) {
    *out_const = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
    return llvm_gen_be_error_t::ok;
  }

  type = skip_typerefs(type);

  char* mangled = mangled_typeinfo_name(type);
  std::string sym_name = mangled ? mangled : "_ZTI_unknown";
  llvm::StringRef name_ref(sym_name);

  llvm::GlobalVariable* gv = be_state->module->getNamedGlobal(name_ref);
  if (gv) {
    *out_const = gv;
    return llvm_gen_be_error_t::ok;
  }

  llvm::Type* i8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  std::string vtable_name;
  llvm::Triple triple(be_state->module->getTargetTriple());
  if (triple.isWindowsMSVCEnvironment()) {
    vtable_name = "??_7type_info@@6B@";
  }

  std::vector<llvm::Constant*> init_elements;

  llvm::Constant* name_str = llvm::ConstantDataArray::getString(*be_state->context, sym_name);
  llvm::GlobalVariable* name_gv = new llvm::GlobalVariable(
      *be_state->module, name_str->getType(), true,
      llvm::GlobalValue::PrivateLinkage, name_str, sym_name + "_name");
  llvm::Constant* name_ptr = name_gv;

  if (type->kind == tk_class || type->kind == tk_struct) {
    a_class_type_supplement_ptr extra = type->variant.class_struct_union.extra_info;
    int base_count = 0;
    a_base_class_ptr base = extra ? extra->direct_base_classes : nullptr;
    while (base) { base_count++; base = base->next_direct; }

    if (base_count == 0) {
      vtable_name = "_ZTVN10__cxxabiv117__class_type_infoE";
      init_elements.push_back(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8_ptr_ty)));
      init_elements.push_back(name_ptr);
    } else if (base_count == 1 && !(extra->direct_base_classes->is_virtual)) {
      vtable_name = "_ZTVN10__cxxabiv120__si_class_type_infoE";
      init_elements.push_back(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8_ptr_ty)));
      init_elements.push_back(name_ptr);

      llvm::Constant* base_ti = nullptr;
      llvm_gen_be_error_t err = get_typeinfo_global(extra->direct_base_classes->type, &base_ti);
      if (err != llvm_gen_be_error_t::ok) return err;
      init_elements.push_back(base_ti);
    } else {
      vtable_name = "_ZTVN10__cxxabiv121__vmi_class_type_infoE";
      init_elements.push_back(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8_ptr_ty)));
      init_elements.push_back(name_ptr);

      llvm::Type* int32_ty = llvm::Type::getInt32Ty(*be_state->context);

      long vmi_flags = 0;
      base = extra ? extra->direct_base_classes : nullptr;
      while (base) {
        if (base->has_public_derivation) vmi_flags |= 2;
        base = base->next_direct;
      }
      if (type->variant.class_struct_union.any_virtual_base_classes) vmi_flags |= 1;

      init_elements.push_back(llvm::ConstantInt::get(int32_ty, vmi_flags));
      init_elements.push_back(llvm::ConstantInt::get(int32_ty, base_count));

      base = extra ? extra->direct_base_classes : nullptr;
      while (base) {
        llvm::Constant* base_ti = nullptr;
        llvm_gen_be_error_t err = get_typeinfo_global(base->type, &base_ti);
        if (err != llvm_gen_be_error_t::ok) return err;
        init_elements.push_back(base_ti);

        long offset_flags = base->offset << 8;
        if (base->is_virtual) offset_flags |= 1;
        if (base->has_public_derivation) offset_flags |= 2;
        init_elements.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), offset_flags));

        base = base->next_direct;
      }
    }
  } else if (type->kind == tk_pointer) {
    vtable_name = "_ZTVN10__cxxabiv119__pointer_type_infoE";
    init_elements.push_back(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8_ptr_ty)));
    init_elements.push_back(name_ptr);

    llvm::Constant* pointee_ti = nullptr;
    llvm_gen_be_error_t err = get_typeinfo_global(type->variant.pointer.type, &pointee_ti);
    if (err != llvm_gen_be_error_t::ok) return err;

    init_elements.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0));
    init_elements.push_back(pointee_ti);
  } else if (type->kind == tk_ptr_to_member) {
    vtable_name = "_ZTVN10__cxxabiv129__pointer_to_member_type_infoE";
    init_elements.push_back(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8_ptr_ty)));
    init_elements.push_back(name_ptr);

    llvm::Constant* pointee_ti = nullptr;
    llvm_gen_be_error_t err = get_typeinfo_global(type->variant.ptr_to_member.type, &pointee_ti);
    if (err != llvm_gen_be_error_t::ok) return err;

    init_elements.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0));
    init_elements.push_back(pointee_ti);

    llvm::Constant* class_ti = nullptr;
    err = get_typeinfo_global(type->variant.ptr_to_member.class_of_which_a_member, &class_ti);
    if (err != llvm_gen_be_error_t::ok) return err;
    init_elements.push_back(class_ti);
  } else if (type->kind == tk_routine) {
    vtable_name = "_ZTVN10__cxxabiv120__function_type_infoE";
    init_elements.push_back(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8_ptr_ty)));
    init_elements.push_back(name_ptr);
  } else if (type->kind == tk_enum) {
    vtable_name = "_ZTVN10__cxxabiv116__enum_type_infoE";
    init_elements.push_back(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8_ptr_ty)));
    init_elements.push_back(name_ptr);
  } else {
    vtable_name = "_ZTVN10__cxxabiv123__fundamental_type_infoE";
    init_elements.push_back(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8_ptr_ty)));
    init_elements.push_back(name_ptr);
  }

  llvm::GlobalVariable* abi_vtable = be_state->module->getNamedGlobal(vtable_name);
  if (!abi_vtable) {
    abi_vtable = new llvm::GlobalVariable(
        *be_state->module, i8_ptr_ty, true,
        llvm::GlobalValue::ExternalLinkage, nullptr, vtable_name);
  }

  init_elements[0] = abi_vtable;

  std::vector<llvm::Type*> element_types;
  for (auto* el : init_elements) {
    element_types.push_back(el->getType());
  }
  llvm::StructType* typeinfo_ty = llvm::StructType::get(*be_state->context, element_types);
  llvm::Constant* typeinfo_init = llvm::ConstantStruct::get(typeinfo_ty, init_elements);

  gv = new llvm::GlobalVariable(
      *be_state->module,
      typeinfo_ty,
      true,
      llvm::GlobalValue::LinkOnceODRLinkage,
      typeinfo_init,
      name_ref);

  *out_const = gv;
  return llvm_gen_be_error_t::ok;
}


static std::string msvc_mangle_number(int64_t n) {
    if (n == 0) return "A@";
    std::string res = "";
    if (n < 0) {
        res += "?";
        n = -n;
    }
    if (n >= 1 && n <= 10) {
        res += std::to_string(n - 1);
        return res;
    }
    
    std::string hex_digits = "";
    while (n > 0) {
        hex_digits = (char)('A' + (n % 16)) + hex_digits;
        n /= 16;
    }
    res += hex_digits + "@";
    return res;
}

static llvm_gen_be_error_t llvm_emit_msvc_rtti_base_class_descriptor(
    a_type_ptr class_type,
    a_base_class_ptr base,
    llvm::Constant** out_const) noexcept {
    if (!class_type || !out_const) return llvm_gen_be_error_t::invalid_argument;

    int64_t mdisp = base ? base->offset : 0;
    int64_t pdisp = -1; // Not yet handling virtual bases
    int64_t vdisp = 0;
    int64_t attributes = 0; // Or other attributes, typically 0 for direct base
    if (base == nullptr) attributes = 0;

    std::string bcd_name = "??_R1";
    bcd_name += msvc_mangle_number(mdisp);
    bcd_name += msvc_mangle_number(pdisp);
    bcd_name += msvc_mangle_number(vdisp);
    bcd_name += msvc_mangle_number(attributes);
    
    char* mangled = mangled_typeinfo_name(base ? base->type : class_type);
    if (mangled) {
        // mangled_typeinfo_name usually returns .?AU...@@ for MSVC. 
        // BCD name requires the name without .?
        if (mangled[0] == '.' && mangled[1] == '?') bcd_name += (mangled + 2);
        else bcd_name += mangled;
    }
    bcd_name += "8";

    llvm::GlobalVariable* gv = be_state->module->getNamedGlobal(bcd_name);
    if (gv) {
        *out_const = gv;
        return llvm_gen_be_error_t::ok;
    }

    llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);

    llvm::Constant* ti = nullptr;
    llvm_gen_be_error_t err = get_typeinfo_global(base ? base->type : class_type, &ti);
    if (err != llvm_gen_be_error_t::ok) return err;

    std::vector<llvm::Constant*> fields;
    fields.push_back(ti ? ti : llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty))); // TypeDescriptor
    
    // num bases
    int num_bases = 0;
    a_type_ptr tgt_type = base ? base->type : class_type;
    if (tgt_type->variant.class_struct_union.extra_info) {
        a_base_class_ptr b = tgt_type->variant.class_struct_union.extra_info->direct_base_classes;
        while(b) { num_bases++; b = b->next_direct; }
    }
    fields.push_back(llvm::ConstantInt::get(i32_ty, num_bases));
    
    // PMD
    fields.push_back(llvm::ConstantInt::get(i32_ty, base ? base->offset : 0)); // mdisp
    fields.push_back(llvm::ConstantInt::get(i32_ty, -1)); // pdisp
    fields.push_back(llvm::ConstantInt::get(i32_ty, 0));  // vdisp
    
    // attributes
    fields.push_back(llvm::ConstantInt::get(i32_ty, 0));

    std::vector<llvm::Type*> field_types;
    for (auto* f : fields) field_types.push_back(f->getType());

    llvm::StructType* bcd_ty = llvm::StructType::get(*be_state->context, field_types);
    llvm::Constant* init = llvm::ConstantStruct::get(bcd_ty, fields);

    gv = new llvm::GlobalVariable(
        *be_state->module, bcd_ty, true,
        llvm::GlobalValue::LinkOnceODRLinkage, init, bcd_name);
    *out_const = gv;
    return llvm_gen_be_error_t::ok;
}

static llvm_gen_be_error_t llvm_emit_msvc_rtti_class_hierarchy_descriptor(
    a_type_ptr type,
    llvm::Constant** out_const) noexcept {
    if (!type || !out_const) return llvm_gen_be_error_t::invalid_argument;

    std::string chd_name = "??_R3";
    char* mangled = mangled_typeinfo_name(type);
    chd_name += (mangled ? mangled : "class");
    chd_name += "8";

    llvm::GlobalVariable* gv = be_state->module->getNamedGlobal(chd_name);
    if (gv) {
        *out_const = gv;
        return llvm_gen_be_error_t::ok;
    }

    llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);

    // Collect all base classes (including the class itself)
    std::vector<a_base_class_ptr> all_bases;
    a_class_type_supplement_ptr extra = type->variant.class_struct_union.extra_info;
    a_base_class_ptr b = extra ? extra->base_classes : nullptr;
    while(b) { all_bases.push_back(b); b = b->next; }

    std::vector<llvm::Constant*> bca_elements;
    
    // BaseClassDescriptor for the class itself
    llvm::Constant* bcd_self = nullptr;
    llvm_gen_be_error_t err = llvm_emit_msvc_rtti_base_class_descriptor(type, nullptr, &bcd_self);
    if (err != llvm_gen_be_error_t::ok) return err;
    bca_elements.push_back(bcd_self);

    for (auto base : all_bases) {
        llvm::Constant* bcd = nullptr;
        err = llvm_emit_msvc_rtti_base_class_descriptor(type, base, &bcd);
        if (err != llvm_gen_be_error_t::ok) return err;
        bca_elements.push_back(bcd);
    }

    llvm::ArrayType* bca_ty = llvm::ArrayType::get(ptr_ty, bca_elements.size());
    llvm::Constant* bca_init = llvm::ConstantArray::get(bca_ty, bca_elements);
    
    std::string bca_name = "??_R2";
    bca_name += (mangled ? mangled : "class");
    bca_name += "8";
    
    llvm::GlobalVariable* bca_gv = new llvm::GlobalVariable(
        *be_state->module, bca_ty, true,
        llvm::GlobalValue::LinkOnceODRLinkage, bca_init, bca_name);

    std::vector<llvm::Constant*> fields;
    fields.push_back(llvm::ConstantInt::get(i32_ty, 0)); // signature
    fields.push_back(llvm::ConstantInt::get(i32_ty, 0)); // attributes
    fields.push_back(llvm::ConstantInt::get(i32_ty, bca_elements.size())); // numBaseClasses
    fields.push_back(bca_gv); // baseClassArray

    std::vector<llvm::Type*> field_types;
    for (auto* f : fields) field_types.push_back(f->getType());

    llvm::StructType* chd_ty = llvm::StructType::get(*be_state->context, field_types);
    llvm::Constant* init = llvm::ConstantStruct::get(chd_ty, fields);

    gv = new llvm::GlobalVariable(
        *be_state->module, chd_ty, true,
        llvm::GlobalValue::LinkOnceODRLinkage, init, chd_name);

    *out_const = gv;
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits MSVC RTTI Complete Object Locator and hierarchy descriptors.
 * @details Generates `_s__RTTICompleteObjectLocator` and associated base class arrays for MSVC.
 * @param[in] type The EDG class AST type pointer.
 * @param[out] out_const Receives the LLVM Constant pointer for the CompleteObjectLocator global.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_emit_msvc_rtti_complete_object_locator(
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
  std::string col_name = "??_R4";
  col_name += (mangled ? mangled : "class");
  col_name += "6B@";

  llvm::StringRef name_ref(col_name);
  llvm::GlobalVariable* gv = be_state->module->getNamedGlobal(name_ref);
  if (gv) {
    *out_const = gv;
    return llvm_gen_be_error_t::ok;
  }

  llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);
  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);

  llvm::Constant* ti = nullptr;
  llvm_gen_be_error_t err = get_typeinfo_global(type, &ti);
  if (err != llvm_gen_be_error_t::ok) return err;
  
  llvm::Constant* chd = nullptr;
  err = llvm_emit_msvc_rtti_class_hierarchy_descriptor(type, &chd);
  if (err != llvm_gen_be_error_t::ok) return err;

  // Layout: { i32 signature, i32 offset, i32 cdOffset, ptr typeDescriptor, ptr classDescriptor }
  std::vector<llvm::Constant*> fields;
  fields.push_back(llvm::ConstantInt::get(i32_ty, 0)); // signature
  fields.push_back(llvm::ConstantInt::get(i32_ty, 0)); // offset
  fields.push_back(llvm::ConstantInt::get(i32_ty, 0)); // cdOffset
  fields.push_back(ti ? ti : llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty)));
  fields.push_back(chd); // classDescriptor

  std::vector<llvm::Type*> field_types;
  for (auto* f : fields) field_types.push_back(f->getType());

  llvm::StructType* col_ty = llvm::StructType::get(*be_state->context, field_types);
  llvm::Constant* init = llvm::ConstantStruct::get(col_ty, fields);

  gv = new llvm::GlobalVariable(
      *be_state->module,
      col_ty,
      true,
      llvm::GlobalValue::LinkOnceODRLinkage,
      init,
      name_ref);

  *out_const = gv;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a C++ dynamic_cast expression to LLVM IR.
 * @details Translates pointer and reference dynamic casts to `__dynamic_cast` runtime calls
 *          with offset hints and bad_cast branch checks.
 * @param[in] expr The EDG AST expression node representing the dynamic_cast.
 * @param[out] out_val Receives the computed LLVM Value pointer on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_dynamic_cast(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;

  an_expr_node_ptr sub_expr = expr->variant.operation.operands;
  if (!sub_expr) return llvm_gen_be_error_t::invalid_argument;

  llvm::Value* sub_val = nullptr;
  llvm_gen_be_error_t err = llvm_lower_expression(sub_expr, &sub_val);
  if (err != llvm_gen_be_error_t::ok) return err;

  a_type_ptr src_type = sub_expr->type;
  if (src_type && (src_type->kind == tk_pointer || is_reference_type(src_type))) {
    src_type = src_type->variant.pointer.type;
  }

  a_type_ptr dst_type = expr->type;
  if (dst_type && (dst_type->kind == tk_pointer || is_reference_type(dst_type))) {
    dst_type = dst_type->variant.pointer.type;
  }

  llvm::Constant* src_ti = nullptr;
  err = get_typeinfo_global(src_type, &src_ti);
  if (err != llvm_gen_be_error_t::ok) return err;

  llvm::Constant* dst_ti = nullptr;
  err = get_typeinfo_global(dst_type, &dst_ti);
  if (err != llvm_gen_be_error_t::ok) return err;

  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);

  llvm::Triple triple(be_state->module->getTargetTriple());
  llvm::Value* cast_result = nullptr;

  if (triple.isWindowsMSVCEnvironment()) {
    // MSVC __RTDynamicCast
    llvm::FunctionType* rtdyn_ty = llvm::FunctionType::get(
        ptr_ty,
        {ptr_ty, llvm::Type::getInt32Ty(*be_state->context), ptr_ty, ptr_ty, llvm::Type::getInt32Ty(*be_state->context)},
        false);
    llvm::FunctionCallee rtdyn_fn = be_state->module->getOrInsertFunction("__RTDynamicCast", rtdyn_ty);
    int is_ref = (expr->variant.operation.kind == eok_ref_dynamic_cast) ? 1 : 0;
    cast_result = be_state->builder->CreateCall(
        rtdyn_fn,
        {sub_val, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0), src_ti, dst_ti,
         llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), is_ref)},
        "dyn_cast_msvc");
  } else {
    // Itanium ABI __dynamic_cast(sub, src_ti, dst_ti, offset_hint)
    llvm::FunctionType* dyn_ty = llvm::FunctionType::get(
        ptr_ty, {ptr_ty, ptr_ty, ptr_ty, i64_ty}, false);
    llvm::FunctionCallee dyn_fn = be_state->module->getOrInsertFunction("__dynamic_cast", dyn_ty);

    int64_t offset_hint = -1; // -1: no hint
    cast_result = be_state->builder->CreateCall(
        dyn_fn,
        {sub_val, src_ti, dst_ti, llvm::ConstantInt::get(i64_ty, offset_hint)},
        "dyn_cast");

    // If reference dynamic cast, check for null and branch to __cxa_bad_cast
    if (expr->variant.operation.kind == eok_ref_dynamic_cast) {
      llvm::Function* parent_func = be_state->builder->GetInsertBlock()->getParent();
      llvm::BasicBlock* ok_bb = llvm::BasicBlock::Create(*be_state->context, "dyn_cast.ok", parent_func);
      llvm::BasicBlock* bad_bb = llvm::BasicBlock::Create(*be_state->context, "dyn_cast.bad", parent_func);

      llvm::Value* is_null = be_state->builder->CreateIsNull(cast_result, "is_bad_cast");
      be_state->builder->CreateCondBr(is_null, bad_bb, ok_bb);

      be_state->builder->SetInsertPoint(bad_bb);
      llvm::FunctionType* bad_cast_ty = llvm::FunctionType::get(
          llvm::Type::getVoidTy(*be_state->context), false);
      llvm::FunctionCallee bad_cast_fn = be_state->module->getOrInsertFunction("__cxa_bad_cast", bad_cast_ty);
      be_state->builder->CreateCall(bad_cast_fn, {});
      be_state->builder->CreateUnreachable();

      be_state->builder->SetInsertPoint(ok_bb);
    }
  }

  llvm::Type* dest_llvm_ty = nullptr;
  err = get_llvm_type(expr->type, &dest_llvm_ty);
  if (err == llvm_gen_be_error_t::ok && dest_llvm_ty && dest_llvm_ty->isPointerTy()) {
    *out_val = cast_result;
  } else {
    *out_val = cast_result;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a C++ typeid expression to LLVM IR.
 * @details Evaluates static typeid expressions to global typeinfo pointers, or polymorphic
 *          typeid expressions by loading from the object's virtual table with bad_typeid checks.
 * @param[in] expr The EDG AST expression node representing the typeid expression.
 * @param[out] out_val Receives the computed LLVM Value pointer representing the type_info on success.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t llvm_lower_typeid(
    an_expr_node_ptr expr,
    llvm::Value** out_val) noexcept {
  if (!expr || !out_val) return llvm_gen_be_error_t::invalid_argument;

  an_expr_node_ptr type_with_expr = expr->variant.typeid_info.type_with_opt_expr;
  if (!type_with_expr) return llvm_gen_be_error_t::invalid_argument;

  a_type_ptr static_ty = type_with_expr->variant.type_operand.type;

  if (expr->variant.typeid_info.is_dynamic && type_with_expr->next != nullptr) {
    // Dynamic polymorphic typeid(expr): load vptr, load vtable[-1]
    an_expr_node_ptr obj_expr = type_with_expr->next;
    llvm::Value* obj_val = nullptr;
    llvm_gen_be_error_t err = llvm_lower_expression(obj_expr, &obj_val);
    if (err != llvm_gen_be_error_t::ok) return err;

    llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
    llvm::Triple triple(be_state->module->getTargetTriple());

    // Null check: if obj is null, call __cxa_bad_typeid() or __RTtypeid throws internally
    if (triple.isWindowsMSVCEnvironment()) {
      // MSVC __RTtypeid(obj) throws __non_rtti_object if RTTI is corrupt
      // and std::bad_typeid if obj is null.
      llvm::FunctionType* rttypeid_ty = llvm::FunctionType::get(ptr_ty, {ptr_ty}, false);
      llvm::FunctionCallee rttypeid_fn = be_state->module->getOrInsertFunction("__RTtypeid", rttypeid_ty);
      *out_val = be_state->builder->CreateCall(rttypeid_fn, {obj_val}, "rt_typeid");
      return llvm_gen_be_error_t::ok;
    }

    llvm::Function* parent_func = be_state->builder->GetInsertBlock()->getParent();
    llvm::BasicBlock* valid_bb = llvm::BasicBlock::Create(*be_state->context, "typeid.valid", parent_func);
    llvm::BasicBlock* bad_bb = llvm::BasicBlock::Create(*be_state->context, "typeid.bad", parent_func);

    llvm::Value* is_null = be_state->builder->CreateIsNull(obj_val, "obj_is_null");
    be_state->builder->CreateCondBr(is_null, bad_bb, valid_bb);

    be_state->builder->SetInsertPoint(bad_bb);
    llvm::FunctionType* bad_typeid_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*be_state->context), false);
    llvm::FunctionCallee bad_typeid_fn = be_state->module->getOrInsertFunction("__cxa_bad_typeid", bad_typeid_ty);
    be_state->builder->CreateCall(bad_typeid_fn, {});
    be_state->builder->CreateUnreachable();

    be_state->builder->SetInsertPoint(valid_bb);
    llvm::Value* vptr = be_state->builder->CreateLoad(ptr_ty, obj_val, "vptr");
    llvm::Value* minus_one = llvm::ConstantInt::get(
        llvm::Type::getInt64Ty(*be_state->context), -1);
    llvm::Value* ti_slot = be_state->builder->CreateInBoundsGEP(ptr_ty, vptr, minus_one, "ti_slot");
    *out_val = be_state->builder->CreateLoad(ptr_ty, ti_slot, "typeinfo_ptr");
    return llvm_gen_be_error_t::ok;
  }

  // Non-polymorphic / static typeid(T)
  llvm::Constant* ti = nullptr;
  llvm_gen_be_error_t err = get_typeinfo_global(static_ty, &ti);
  if (err != llvm_gen_be_error_t::ok) return err;

  *out_val = ti;
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
