/**
 * @file llvm_gen_be_rtti.cpp
 * @brief RTTI generation subsystem for the EDG LLVM backend.
 */

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_rtti.h"
#include "lower_name.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief get_typeinfo_global
 * @param[in] _p param
 * @return llvm_gen_be_error_t::ok
 */
llvm_gen_be_error_t get_typeinfo_global(a_type_ptr type, llvm::Constant** out_const) noexcept {
  if (!out_const) return llvm_gen_be_error_t::invalid_argument;
  if (!type) {
    *out_const = llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(*be_state->context));
    return llvm_gen_be_error_t::ok;
  }
  
  type = skip_typerefs(type);
  
  char* mangled = mangled_typeinfo_name(type);
  llvm::StringRef name_ref(mangled);
  
  llvm::GlobalVariable* gv = be_state->module->getNamedGlobal(name_ref);
  if (gv) {
    *out_const = gv;
    return llvm_gen_be_error_t::ok;
  }
  
  llvm::Type* i8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  std::string vtable_name;
  std::vector<llvm::Constant*> init_elements;
  
  llvm::Constant* name_str = llvm::ConstantDataArray::getString(*be_state->context, mangled);
  llvm::GlobalVariable* name_gv = new llvm::GlobalVariable(
      *be_state->module, name_str->getType(), true,
      llvm::GlobalValue::PrivateLinkage, name_str, name_ref.str() + "_name");
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
          init_elements.push_back(llvm::ConstantInt::get(int32_ty, 0));
          init_elements.push_back(llvm::ConstantInt::get(int32_ty, base_count));
          
          base = extra->direct_base_classes;
          while (base) {
              llvm::Constant* base_ti = nullptr;
              llvm_gen_be_error_t err = get_typeinfo_global(base->type, &base_ti);
              if (err != llvm_gen_be_error_t::ok) return err;
              init_elements.push_back(base_ti);
              
              long offset_flags = base->offset;
              if (base->is_virtual) offset_flags = 1;
              if (base->has_public_derivation) offset_flags |= 2;
              init_elements.push_back(llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), offset_flags));
              
              base = base->next_direct;
          }
      }
  } else if (type->kind == tk_pointer || type->kind == tk_ptr_to_member) {
      vtable_name = "_ZTVN10__cxxabiv119__pointer_type_infoE";
      init_elements.push_back(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(i8_ptr_ty)));
      init_elements.push_back(name_ptr);
      
      llvm::Constant* pointee_ti = nullptr;
      a_type_ptr pointee = (type->kind == tk_pointer) ? type->variant.pointer.type : type->variant.ptr_to_member.type;
      llvm_gen_be_error_t err = get_typeinfo_global(pointee, &pointee_ti);
      if (err != llvm_gen_be_error_t::ok) return err;
      
      init_elements.push_back(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*be_state->context), 0));
      init_elements.push_back(pointee_ti);
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
      name_ref
  );
  
  *out_const = gv;
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
