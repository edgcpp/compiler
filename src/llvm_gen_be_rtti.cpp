/**
 * @file llvm_gen_be_rtti.cpp
 * @ingroup llvm_gen_be_cxx_abi
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
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  if (!type) {
    *out_const = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
    return llvm_gen_be_error_t::ok;
  }

  type = skip_typerefs(type);
  if (type->kind == tk_pointer && type->variant.pointer.is_reference) {
    type = type->variant.pointer.type ? skip_typerefs(type->variant.pointer.type) : nullptr;
  }
  if (!type) {
    *out_const = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
    return llvm_gen_be_error_t::ok;
  }

#if DO_IL_LOWERING
  if (type->typeinfo_var && type->typeinfo_var->source_corresp.name) {
    LLVM_GEN_BE_CHECK(llvm_lower_global_variable(type->typeinfo_var));
    if (llvm::GlobalVariable* lowered_ti =
            be_state->module->getNamedGlobal(type->typeinfo_var->source_corresp.name)) {
      *out_const = lowered_ti;
      return llvm_gen_be_error_t::ok;
    }
  }
#endif

  char* mangled = mangled_typeinfo_name(type);
  std::string sym_name = mangled ? mangled : "_ZTI_unknown";
  llvm::StringRef name_ref = llvm::StringRef(sym_name);

  llvm::GlobalVariable* gv = be_state->module->getNamedGlobal(name_ref);
  if (gv) {
    *out_const = gv;
    return llvm_gen_be_error_t::ok;
  }

  llvm::Type* i8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  std::string vtable_name;
  llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
  if (triple.isWindowsMSVCEnvironment()) {
    vtable_name = "??_7type_info@@6B@";
  }

  std::vector<llvm::Constant*> init_elements;

  std::string raw_zts_text =
      (sym_name.rfind("_ZTI", 0) == 0 && sym_name.size() > 4)
          ? sym_name.substr(4)
          : sym_name;
  llvm::Constant* name_str = llvm::ConstantDataArray::getString(*be_state->context, raw_zts_text);
  llvm::GlobalVariable* name_gv = nullptr;
  LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), name_str->getType(), true, llvm::GlobalValue::PrivateLinkage, name_str, sym_name + "_name", &name_gv));
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
      LLVM_GEN_BE_CHECK(get_typeinfo_global(extra->direct_base_classes->type, &base_ti));
      init_elements.push_back(base_ti);
    } else {
      vtable_name = "_ZTVN10__cxxabiv121__vmi_class_type_infoE";
      init_elements.push_back(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8_ptr_ty)));
      init_elements.push_back(name_ptr);

      llvm::Type* int32_ty = llvm::Type::getInt32Ty(*be_state->context);

      long vmi_flags = 0;
      base = extra->direct_base_classes;
      while (base) {
        if (base->has_public_derivation) vmi_flags |= 2;
        base = base->next_direct;
      }
      if (type->variant.class_struct_union.any_virtual_base_classes) vmi_flags |= 1;

      init_elements.push_back(llvm::ConstantInt::get(int32_ty, static_cast<uint64_t>(vmi_flags)));
      init_elements.push_back(llvm::ConstantInt::get(int32_ty, static_cast<uint64_t>(base_count)));

      base = extra->direct_base_classes;
      while (base) {
        llvm::Constant* base_ti = nullptr;
        LLVM_GEN_BE_CHECK(get_typeinfo_global(base->type, &base_ti));
        init_elements.push_back(base_ti);

        long offset_flags = static_cast<long>(base->offset) << 8;
        if (base->is_virtual) offset_flags |= 1;
        if (base->has_public_derivation) offset_flags |= 2;
        init_elements.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), static_cast<uint64_t>(offset_flags)));

        base = base->next_direct;
      }
    }
  } else if (type->kind == tk_pointer) {
    vtable_name = "_ZTVN10__cxxabiv119__pointer_type_infoE";
    init_elements.push_back(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8_ptr_ty)));
    init_elements.push_back(name_ptr);

    llvm::Constant* pointee_ti = nullptr;
    LLVM_GEN_BE_CHECK(get_typeinfo_global(type->variant.pointer.type, &pointee_ti));

    init_elements.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0));
    init_elements.push_back(pointee_ti);
  } else if (type->kind == tk_ptr_to_member) {
    vtable_name = "_ZTVN10__cxxabiv129__pointer_to_member_type_infoE";
    init_elements.push_back(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8_ptr_ty)));
    init_elements.push_back(name_ptr);

    llvm::Constant* pointee_ti = nullptr;
    LLVM_GEN_BE_CHECK(get_typeinfo_global(type->variant.ptr_to_member.type, &pointee_ti));

    init_elements.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0));
    init_elements.push_back(pointee_ti);

    llvm::Constant* class_ti = nullptr;
    LLVM_GEN_BE_CHECK(get_typeinfo_global(type->variant.ptr_to_member.class_of_which_a_member, &class_ti));
    init_elements.push_back(class_ti);
  } else if (type->kind == tk_routine) {
    vtable_name = "_ZTVN10__cxxabiv120__function_type_infoE";
    init_elements.push_back(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8_ptr_ty)));
    init_elements.push_back(name_ptr);
  } else if (type->kind == tk_array) {
    vtable_name = "_ZTVN10__cxxabiv117__array_type_infoE";
    init_elements.push_back(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8_ptr_ty)));
    init_elements.push_back(name_ptr);
  } else if (type->kind == tk_integer && type->variant.integer.enum_type) {
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
    LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), i8_ptr_ty, true, llvm::GlobalValue::ExternalLinkage, nullptr, vtable_name, &abi_vtable));
  }

  if (triple.isWindowsMSVCEnvironment()) {
    init_elements[0] = abi_vtable;
  } else {
    llvm::Type* i8_ty = llvm::Type::getInt8Ty(*be_state->context);
    llvm::Constant* off16 = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), 16);
    init_elements[0] = llvm::ConstantExpr::getInBoundsGetElementPtr(i8_ty, abi_vtable, off16);
  }

  std::vector<llvm::Type*> element_types;
  for (auto* el : init_elements) {
    element_types.push_back(el->getType());
  }
  llvm::StructType* typeinfo_ty = llvm::StructType::get(*be_state->context, element_types);
  llvm::Constant* typeinfo_init = llvm::ConstantStruct::get(typeinfo_ty, init_elements);

  LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), typeinfo_ty, true, llvm::GlobalValue::LinkOnceODRLinkage, typeinfo_init, name_ref, &gv));

#if DO_IL_LOWERING
  llvm::Triple rtti_triple = llvm::Triple(be_state->module->getTargetTriple());
  if (!rtti_triple.isOSBinFormatMachO()) {
    llvm::Comdat* comdat = be_state->module->getOrInsertComdat(name_ref);
    comdat->setSelectionKind(rtti_triple.isOSBinFormatCOFF() ? llvm::Comdat::Largest : llvm::Comdat::Any);
    gv->setComdat(comdat);
  } else {
    gv->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Global);
  }
#endif

  *out_const = gv;
  return llvm_gen_be_error_t::ok;
}


/**
 * @brief Mangles a signed 64-bit integer according to Microsoft Visual C++ ABI rules.
 * @details Encodes 0 as "A@", 1..10 as single digits '0'..'9', and larger values as
 *          hexadecimal 'A'..'P' digits terminated by '@', prefixed with '?' if negative.
 * @param[in] n The signed 64-bit integer value to mangle.
 * @param[out] out_mangled Receives the mangled string representation.
 * @return llvm_gen_be_error_t::ok on success, or llvm_gen_be_error_t::invalid_argument if out_mangled is null.
 */
static llvm_gen_be_error_t msvc_mangle_number(int64_t n, std::string* out_mangled) noexcept {
  LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
  if (n == 0) {
    *out_mangled = "A@";
    return llvm_gen_be_error_t::ok;
  }
  std::string res = "";
  uint64_t abs_n = 0;
  if (n < 0) {
    res += "?";
    abs_n = static_cast<uint64_t>(-(n + 1)) + 1ULL;
  } else {
    abs_n = static_cast<uint64_t>(n);
  }
  if (abs_n <= 10) {
    res += std::to_string(abs_n - 1);
    *out_mangled = res;
    return llvm_gen_be_error_t::ok;
  }

  std::string hex_digits = "";
  while (abs_n > 0) {
    hex_digits = static_cast<char>('A' + (abs_n % 16)) + hex_digits;
    abs_n /= 16;
  }
  res += hex_digits + "@";
  *out_mangled = res;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits an MSVC RTTI Base Class Descriptor (`_RTTIBaseClassDescriptor`).
 * @details Generates the global descriptor encoding the base class `TypeDescriptor`,
 *          contained base count, pointer-to-member displacement (`mdisp`, `pdisp`, `vdisp`),
 *          and hierarchy attributes (including virtual base displacement calculation).
 * @param[in] class_type The complete derived EDG class type.
 * @param[in] base Pointer to the EDG base class entry (or nullptr for the self descriptor).
 * @param[out] out_const Receives the generated LLVM Constant global variable.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
static llvm_gen_be_error_t llvm_emit_msvc_rtti_base_class_descriptor(
    a_type_ptr class_type,
    a_base_class_ptr base,
    llvm::Constant** out_const) noexcept {
  int64_t mdisp = 0;
  int64_t pdisp = -1;
  int64_t vdisp = 0;
  int64_t attributes = 0x40; // BCD_HASPCHD

  if (base) {
    if (base->is_virtual) {
      mdisp = 0;
      pdisp = 0;
      a_class_type_supplement_ptr extra = class_type->variant.class_struct_union.extra_info;
      int32_t vb_idx = 1;
      for (a_base_class_ptr b = extra->base_classes; b != base; b = b->next) {
        if (b->is_virtual) {
          vb_idx++;
        }
      }
      vdisp = static_cast<int64_t>(vb_idx) * 4;
      attributes |= 0x04; // BCD_VBOFCONTOBJ
    } else {
      mdisp = static_cast<int64_t>(base->offset);
      pdisp = -1;
      vdisp = 0;
    }
    if (!base->has_public_derivation) {
      attributes |= 0x01; // BCD_NOTVISIBLE
    }
  }

  std::string m_mdisp;
  std::string m_pdisp;
  std::string m_vdisp;
  std::string m_attr;
  LLVM_GEN_BE_CHECK(msvc_mangle_number(mdisp, &m_mdisp));
  LLVM_GEN_BE_CHECK(msvc_mangle_number(pdisp, &m_pdisp));
  LLVM_GEN_BE_CHECK(msvc_mangle_number(vdisp, &m_vdisp));
  LLVM_GEN_BE_CHECK(msvc_mangle_number(attributes, &m_attr));

  std::string bcd_name = "??_R1" + m_mdisp + m_pdisp + m_vdisp + m_attr;

  char* mangled = mangled_typeinfo_name(base ? base->type : class_type);
  if (mangled) {
    if (mangled[0] == '.' && mangled[1] == '?') {
      bcd_name += (mangled + 2);
    } else {
      bcd_name += mangled;
    }
  }
  bcd_name += "8";

  llvm::GlobalVariable* gv = be_state->module->getNamedGlobal(bcd_name);
  if (gv) {
    *out_const = gv;
    return llvm_gen_be_error_t::ok;
  }

  llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);

  llvm::Constant* ti = nullptr;
  LLVM_GEN_BE_CHECK(get_typeinfo_global(base ? base->type : class_type, &ti));

  std::vector<llvm::Constant*> fields;
  fields.push_back(ti);

  int num_bases = 0;
  a_type_ptr tgt_type = base ? base->type : class_type;
  if (tgt_type->variant.class_struct_union.extra_info) {
    a_base_class_ptr b = tgt_type->variant.class_struct_union.extra_info->direct_base_classes;
    while (b) {
      num_bases++;
      b = b->next_direct;
    }
  }
  fields.push_back(llvm::ConstantInt::get(i32_ty, static_cast<uint64_t>(num_bases)));

  // PMD: mdisp, pdisp, vdisp
  fields.push_back(llvm::ConstantInt::get(i32_ty, static_cast<uint64_t>(mdisp)));
  fields.push_back(llvm::ConstantInt::get(i32_ty, static_cast<uint64_t>(pdisp)));
  fields.push_back(llvm::ConstantInt::get(i32_ty, static_cast<uint64_t>(vdisp)));

  // attributes
  fields.push_back(llvm::ConstantInt::get(i32_ty, static_cast<uint64_t>(attributes)));

  std::vector<llvm::Type*> field_types;
  for (auto* f : fields) field_types.push_back(f->getType());

  llvm::StructType* bcd_ty = llvm::StructType::get(*be_state->context, field_types);
  llvm::Constant* init = llvm::ConstantStruct::get(bcd_ty, fields);

  LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), bcd_ty, true, llvm::GlobalValue::LinkOnceODRLinkage, init, bcd_name, &gv));
  *out_const = gv;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits an MSVC RTTI Class Hierarchy Descriptor (`_RTTIClassHierarchyDescriptor`).
 * @details Generates the base class array (`??_R2`) and class hierarchy descriptor (`??_R3`)
 *          for the specified class type.
 * @param[in] type The EDG class AST type pointer.
 * @param[out] out_const Receives the generated LLVM Constant global variable.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
static llvm_gen_be_error_t llvm_emit_msvc_rtti_class_hierarchy_descriptor(
    a_type_ptr type,
    llvm::Constant** out_const) noexcept {
    std::string chd_name = "??_R3";
    char* mangled = mangled_typeinfo_name(type);
    chd_name += (mangled ? mangled : "class");
    chd_name += "8";

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
    LLVM_GEN_BE_CHECK(llvm_emit_msvc_rtti_base_class_descriptor(type, nullptr, &bcd_self));
    bca_elements.push_back(bcd_self);

    for (auto base : all_bases) {
        llvm::Constant* bcd = nullptr;
        LLVM_GEN_BE_CHECK(llvm_emit_msvc_rtti_base_class_descriptor(type, base, &bcd));
        bca_elements.push_back(bcd);
    }

    llvm::ArrayType* bca_ty = llvm::ArrayType::get(ptr_ty, bca_elements.size());
    llvm::Constant* bca_init = llvm::ConstantArray::get(bca_ty, bca_elements);
    
    std::string bca_name = "??_R2";
    bca_name += (mangled ? mangled : "class");
    bca_name += "8";
    
    llvm::GlobalVariable* bca_gv = nullptr;
    LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), bca_ty, true, llvm::GlobalValue::LinkOnceODRLinkage, bca_init, bca_name, &bca_gv));

    std::vector<llvm::Constant*> fields;
    fields.push_back(llvm::ConstantInt::get(i32_ty, 0)); // signature
    fields.push_back(llvm::ConstantInt::get(i32_ty, 0)); // attributes
    fields.push_back(llvm::ConstantInt::get(i32_ty, bca_elements.size())); // numBaseClasses
    fields.push_back(bca_gv); // baseClassArray

    std::vector<llvm::Type*> field_types;
    for (auto* f : fields) field_types.push_back(f->getType());

    llvm::StructType* chd_ty = llvm::StructType::get(*be_state->context, field_types);
    llvm::Constant* init = llvm::ConstantStruct::get(chd_ty, fields);

    llvm::GlobalVariable* gv = nullptr;
    LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), chd_ty, true, llvm::GlobalValue::LinkOnceODRLinkage, init, chd_name, &gv));

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

  llvm::StringRef name_ref = llvm::StringRef(col_name);
  llvm::GlobalVariable* gv = be_state->module->getNamedGlobal(name_ref);
  if (gv) {
    *out_const = gv;
    return llvm_gen_be_error_t::ok;
  }

  llvm::Type* i32_ty = llvm::Type::getInt32Ty(*be_state->context);

  llvm::Constant* ti = nullptr;
  LLVM_GEN_BE_CHECK(get_typeinfo_global(type, &ti));
  
  llvm::Constant* chd = nullptr;
  LLVM_GEN_BE_CHECK(llvm_emit_msvc_rtti_class_hierarchy_descriptor(type, &chd));

  // Layout: { i32 signature, i32 offset, i32 cdOffset, ptr typeDescriptor, ptr classDescriptor }
  std::vector<llvm::Constant*> fields;
  fields.push_back(llvm::ConstantInt::get(i32_ty, 0)); // signature
  fields.push_back(llvm::ConstantInt::get(i32_ty, 0)); // offset
  fields.push_back(llvm::ConstantInt::get(i32_ty, 0)); // cdOffset
  fields.push_back(ti);
  fields.push_back(chd); // classDescriptor

  std::vector<llvm::Type*> field_types;
  for (auto* f : fields) field_types.push_back(f->getType());

  llvm::StructType* col_ty = llvm::StructType::get(*be_state->context, field_types);
  llvm::Constant* init = llvm::ConstantStruct::get(col_ty, fields);

  LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), col_ty, true, llvm::GlobalValue::LinkOnceODRLinkage, init, name_ref, &gv));

  *out_const = gv;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits an MSVC RTTI Type Descriptor (`_TypeDescriptor`, `??_R0?AV...`).
 * @details Generates the global `_TypeDescriptor` structure containing the `??_7type_info@@6B@`
 *          vftable pointer, spare runtime data pointer, and decorated type name string.
 * @param[in] class_ty The EDG class or fundamental AST type pointer.
 * @param[out] out_r0 Receives the resulting `llvm::GlobalVariable*` for the `_R0` descriptor.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if pointers are null.
 */
llvm_gen_be_error_t llvm_emit_msvc_rtti_type_descriptor(
    a_type_ptr class_ty,
    llvm::GlobalVariable** out_r0) noexcept {
  if (!class_ty || !out_r0) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  a_type_ptr resolved = skip_typerefs(class_ty);
  char* mangled = mangled_typeinfo_name(resolved);
  std::string r0_name = "??_R0";
  if (mangled && mangled[0] == '.') {
    r0_name += (mangled + 1);
  } else {
    r0_name += "?AV";
    r0_name += (mangled ? mangled : "class");
    r0_name += "@@";
  }
  r0_name += "@8";

  if (llvm::GlobalVariable* existing = be_state->module->getNamedGlobal(r0_name)) {
    *out_r0 = existing;
    return llvm_gen_be_error_t::ok;
  }

  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::GlobalVariable* ti_vtbl = be_state->module->getNamedGlobal("??_7type_info@@6B@");
  if (!ti_vtbl) {
    LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), ptr_ty, true, llvm::GlobalValue::ExternalLinkage, nullptr, "??_7type_info@@6B@", &ti_vtbl));
  }
  llvm::Constant* spare_null = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
  llvm::Constant* decorated_str = llvm::ConstantDataArray::getString(
      *be_state->context, mangled ? mangled : ".?AVclass@@");

  llvm::StructType* r0_ty = llvm::StructType::get(
      *be_state->context, {ptr_ty, ptr_ty, decorated_str->getType()});
  llvm::Constant* r0_init = llvm::ConstantStruct::get(
      r0_ty, {ti_vtbl, spare_null, decorated_str});

  LLVM_GEN_BE_CHECK(llvm_create_global_variable(be_state->module.get(), r0_ty, false, llvm::GlobalValue::LinkOnceODRLinkage, r0_init, r0_name, out_r0));
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
  LLVM_GEN_BE_CHECK(get_typeinfo_global(src_type, &src_ti));

  llvm::Constant* dst_ti = nullptr;
  LLVM_GEN_BE_CHECK(get_typeinfo_global(dst_type, &dst_ti));

  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Type* i64_ty = llvm::Type::getInt64Ty(*be_state->context);

  llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());
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
         llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), static_cast<uint64_t>(is_ref))},
        "dyn_cast_msvc");
  } else if (dst_type && skip_typerefs(dst_type)->kind == tk_void) {
    // Itanium ABI dynamic_cast<void*>(sub_val): load offset_to_top at vptr[-2]
    llvm::Value* vptr = be_state->builder->CreateLoad(ptr_ty, sub_val, "vptr.void_cast");
    llvm::Value* minus_two = llvm::ConstantInt::getSigned(i64_ty, -2);
    llvm::Value* top_slot = be_state->builder->CreateInBoundsGEP(i64_ty, vptr, minus_two, "off_to_top.slot");
    llvm::Value* off_to_top = be_state->builder->CreateLoad(i64_ty, top_slot, "off_to_top");
    cast_result = be_state->builder->CreateInBoundsGEP(
        llvm::Type::getInt8Ty(*be_state->context), sub_val, off_to_top, "dyn_cast.void");
  } else {
    // Itanium ABI __dynamic_cast(sub, src_ti, dst_ti, offset_hint)
    llvm::FunctionType* dyn_ty = llvm::FunctionType::get(
        ptr_ty, {ptr_ty, ptr_ty, ptr_ty, i64_ty}, false);
    llvm::FunctionCallee dyn_fn = be_state->module->getOrInsertFunction("__dynamic_cast", dyn_ty);

    int64_t offset_hint = -1; // -1: no hint / virtual base
    if (src_type && dst_type) {
      a_type_ptr s_cls = skip_typerefs(src_type);
      a_type_ptr d_cls = skip_typerefs(dst_type);
      if ((s_cls->kind == tk_class || s_cls->kind == tk_struct) &&
          (d_cls->kind == tk_class || d_cls->kind == tk_struct)) {
        a_class_type_supplement_ptr d_supp = d_cls->variant.class_struct_union.extra_info;
        a_base_class_ptr list = d_supp ? (d_supp->base_classes ? d_supp->base_classes : d_supp->direct_base_classes) : nullptr;
        if (list) {
          int pub_nonvirt_matches = 0;
          int priv_matches = 0;
          int virt_matches = 0;
          int64_t pub_off = 0;
          for (a_base_class_ptr b = list; b != nullptr; b = (d_supp->base_classes ? b->next : b->next_direct)) {
            if (skip_typerefs(b->type) == s_cls) {
              if (b->is_virtual) {
                virt_matches++;
              } else if (b->has_public_derivation) {
                pub_nonvirt_matches++;
                pub_off = static_cast<int64_t>(b->offset);
              } else {
                priv_matches++;
              }
            }
          }
          if (virt_matches > 0) {
            offset_hint = -1;
          } else if (pub_nonvirt_matches == 1 && priv_matches == 0) {
            offset_hint = pub_off;
          } else if (pub_nonvirt_matches > 1) {
            offset_hint = -3;
          } else if (priv_matches > 0) {
            offset_hint = -2;
          }
        }
      }
    }
    cast_result = be_state->builder->CreateCall(
        dyn_fn,
        {sub_val, src_ti, dst_ti, llvm::ConstantInt::get(i64_ty, static_cast<uint64_t>(offset_hint))},
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

  *out_val = cast_result;
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
    llvm::Triple triple = llvm::Triple(be_state->module->getTargetTriple());

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
    llvm::Value* minus_one = llvm::ConstantInt::getSigned(
        llvm::Type::getInt64Ty(*be_state->context), -1);
    llvm::Value* ti_slot = be_state->builder->CreateInBoundsGEP(ptr_ty, vptr, minus_one, "ti_slot");
    *out_val = be_state->builder->CreateLoad(ptr_ty, ti_slot, "typeinfo_ptr");
    return llvm_gen_be_error_t::ok;
  }

  // Non-polymorphic / static typeid(T)
  llvm::Constant* ti = nullptr;
  LLVM_GEN_BE_CHECK(get_typeinfo_global(static_ty, &ti));

  *out_val = ti;
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
