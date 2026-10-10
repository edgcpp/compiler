/**
 * @file llvm_gen_be_vtable.cpp
 * @brief Virtual Table (vtable) and dispatch generation for the EDG LLVM backend.
 */

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_vtable.h"
#include "llvm_gen_be_rtti.h"
#include "lower_name.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Intrinsics.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief llvm_emit_vtable
 * @param[in] _p param
 * @return llvm_gen_be_error_t::ok
 */
llvm_gen_be_error_t llvm_emit_vtable(a_type_ptr type, llvm::Constant** out_const) noexcept {
  if (!out_const) return llvm_gen_be_error_t::invalid_argument;
  if (!type || (type->kind != tk_class && type->kind != tk_struct)) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  
  type = skip_typerefs(type);
  
  std::string vtable_name = "_ZTV";
  char* mangled_class = mangled_typeinfo_name(type);
  if (mangled_class && strncmp(mangled_class, "_ZTI", 4) == 0) {
      vtable_name += (mangled_class + 4);
  } else {
      vtable_name += (mangled_class ? mangled_class : "unknown");
  }

  llvm::StringRef name_ref(vtable_name);
  llvm::GlobalVariable* gv = be_state->module->getNamedGlobal(name_ref);
  if (gv) {
    *out_const = gv;
    return llvm_gen_be_error_t::ok;
  }
  
  std::vector<llvm::Constant*> init_elements;
  llvm::Type* int8_ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Type* ptrdiff_ty = llvm::Type::getInt64Ty(*be_state->context);
  
  init_elements.push_back(llvm::ConstantInt::get(ptrdiff_ty, 0));
  
  llvm::Constant* ti = nullptr;
  llvm_gen_be_error_t err = get_typeinfo_global(type, &ti);
  if (err != llvm_gen_be_error_t::ok) return err;
  init_elements.push_back(ti);
  
  // Just emit a single null ptr for testing. Real vtable iterates functions.
  init_elements.push_back(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(int8_ptr_ty)));

  std::vector<llvm::Type*> element_types;
  for (auto* el : init_elements) {
      element_types.push_back(el->getType());
  }
  
  llvm::StructType* vtable_ty = llvm::StructType::get(*be_state->context, element_types);
  llvm::Constant* vtable_init = llvm::ConstantStruct::get(vtable_ty, init_elements);

  gv = new llvm::GlobalVariable(
      *be_state->module,
      vtable_ty,
      true,
      llvm::GlobalValue::LinkOnceODRLinkage,
     /**
 * @brief llvm_lower_virtual_dispatch
 * @param[in] _p param
 * @return llvm_gen_be_error_t::ok
 */
 vtable_init,
      name_ref
  );
  
  *out_const = gv;
  return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t llvm_lower_virtual_dispatch(llvm::Value* object_ptr, a_routine_ptr method_routine, llvm::Value** out_func_ptr) noexcept {
  if (!out_func_ptr || !object_ptr || !method_routine) return llvm_gen_be_error_t::invalid_argument;
  
  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::Value* vptr = be_state->builder->CreateLoad(ptr_ty, object_ptr, "vptr");
  
  long vtable_index = 0; 
  llvm::Value* idx = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*be_state->context), vtable_index);
  llvm::Value* func_gep = be_state->builder->CreateInBoundsGEP(ptr_ty, vptr, idx, "vfunc_gep");
  
  *out_func_ptr = be_state->builder->CreateLoad(ptr_ty, func_gep, "vfunc_load");
  
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
