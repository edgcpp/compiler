
/**
 * @file llvm_gen_be_decl.cpp
 * @brief Declaration lowering subsystem for the EDG LLVM backend.
 * @details Translates EDG front-end global variables, function prototypes, and
 * function bodies into LLVM IR globals and functions.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include <type_traits>
#include <vector>

#include "basic_hdrs.h"
#include "llvm_gen_be_internal.h"
#include "llvm_gen_be_abi_sysv_x86_64.h"
#include "llvm_gen_be_abi_aapcs64.h"
#include "llvm_gen_be_abi_win64.h"
#include "il_read.h"
#include "llvm_gen_be_debug.h"

#include <llvm/IR/Verifier.h>
#include "target.h"

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Applies EDG attributes to an LLVM GlobalObject.
 * @param[in] global The LLVM GlobalObject to apply attributes to.
 * @param[in] attributes The EDG attributes list.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
static llvm_gen_be_error_t apply_llvm_attributes(llvm::GlobalObject* global, an_attribute_ptr attributes) noexcept {
    if (!global) return llvm_gen_be_error_t::invalid_argument;
    for (an_attribute_ptr attr = attributes; attr != nullptr; attr = attr->next) {
        if (attr->kind == ak_section) {
            if (attr->arguments && attr->arguments->kind == aak_constant) {
                a_constant_ptr const_ptr = attr->arguments->variant.constant;
                if (const_ptr && const_ptr->kind == ck_string) {
                    std::string sec_name(const_ptr->variant.string.value, const_ptr->variant.string.length - 1);
                    global->setSection(sec_name);
                }
            } else if (attr->arguments && (attr->arguments->kind == aak_token || attr->arguments->kind == aak_raw_token)) {
                if (attr->arguments->variant.token) {
                     std::string sec_name = attr->arguments->variant.token;
                     if (sec_name.size() >= 2 && sec_name.front() == '"' && sec_name.back() == '"') {
                         sec_name = sec_name.substr(1, sec_name.size() - 2);
                     }
                     global->setSection(sec_name);
                }
            }
        }
#if GNU_VISIBILITY_ATTRIBUTE_ALLOWED
        else if (attr->kind == ak_visibility) {
            std::string vis_str;
            if (attr->arguments && attr->arguments->kind == aak_constant) {
                a_constant_ptr const_ptr = attr->arguments->variant.constant;
                if (const_ptr && const_ptr->kind == ck_string) {
                    vis_str = std::string(const_ptr->variant.string.value, const_ptr->variant.string.length - 1);
                }
            } else if (attr->arguments && (attr->arguments->kind == aak_token || attr->arguments->kind == aak_raw_token)) {
                if (attr->arguments->variant.token) {
                     vis_str = attr->arguments->variant.token;
                     if (vis_str.size() >= 2 && vis_str.front() == '"' && vis_str.back() == '"') {
                         vis_str = vis_str.substr(1, vis_str.size() - 2);
                     }
                }
            }
            if (vis_str == "hidden") {
                global->setVisibility(llvm::GlobalValue::HiddenVisibility);
            } else if (vis_str == "protected") {
                global->setVisibility(llvm::GlobalValue::ProtectedVisibility);
            } else if (vis_str == "default") {
                global->setVisibility(llvm::GlobalValue::DefaultVisibility);
            }
        }
#endif
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a single EDG global variable into an LLVM GlobalVariable.
 * @param[in] var The EDG variable.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
llvm_gen_be_error_t llvm_lower_global_variable(a_variable_ptr var) noexcept {
    if (!var || !var->source_corresp.name) return llvm_gen_be_error_t::ok; // Skip unnamed
    
    llvm::Type* llvm_ty = nullptr;
    llvm_gen_be_error_t err = get_llvm_type(var->type, &llvm_ty);
    if (err != llvm_gen_be_error_t::ok) return err;
    
    llvm::GlobalValue::LinkageTypes linkage = llvm::GlobalValue::ExternalLinkage;
    if (var->storage_class == sc_static) {
      linkage = llvm::GlobalValue::InternalLinkage;
    }
    if (var->is_inline) {
      linkage = llvm::GlobalValue::LinkOnceODRLinkage;
    }
#if GNU_EXTENSIONS_ALLOWED
    if (var->is_weak || var->is_weakref) {
      linkage = llvm::GlobalValue::WeakAnyLinkage;
    }
#endif

    an_init_kind init_kind;
    an_initializer_ptr initializer_ptr;
    get_variable_initializer(var, il_header.primary_scope, &init_kind, &initializer_ptr);

    llvm::Constant* llvm_init = nullptr;
    if (init_kind == initk_static) {
       llvm_gen_be_error_t err = evaluate_constant(initializer_ptr->constant, llvm_ty, &llvm_init);
       if (err != llvm_gen_be_error_t::ok) return err;
    } else if (init_kind == initk_dynamic) {
       a_dynamic_init_ptr dip = initializer_ptr->dynamic;
       if (dip && dip->kind == dik_constant && !dip->follows_an_exec_statement) {
         llvm_init = llvm::Constant::getNullValue(llvm_ty);
       }
    }

    if (!llvm_init && var->storage_class != sc_extern) {
       // Default to zero-initialized for static/global without explicit init
       llvm_init = llvm::Constant::getNullValue(llvm_ty);
    }
    
    if (!llvm_init) {
       linkage = llvm::GlobalValue::ExternalLinkage;
    }

    be_state->module->getOrInsertGlobal(var->source_corresp.name, llvm_ty);
    llvm::GlobalVariable* gvar = be_state->module->getNamedGlobal(var->source_corresp.name);
    if (gvar) {
      gvar->setLinkage(linkage);
      err = apply_llvm_attributes(gvar, var->source_corresp.attributes);
      if (err != llvm_gen_be_error_t::ok) return err;
      if (var->is_thread_local) {
        gvar->setThreadLocalMode(llvm::GlobalValue::GeneralDynamicTLSModel);
      }
#if DO_IL_LOWERING
      if (var->comdat_group) {
        llvm::Comdat *comdat = be_state->module->getOrInsertComdat(var->comdat_group);
        gvar->setComdat(comdat);
      }
#endif
      if (llvm_init) {
        gvar->setInitializer(llvm_init);
      }
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a single EDG global variable into an LLVM GlobalVariable.
 * @param[in] var The EDG variable.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
/**
 * @brief Lowers all EDG global variables in the translation unit.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
  * @param[in] _p
 */
llvm_gen_be_error_t llvm_lower_global_variables(void) noexcept {
  if (!il_header.primary_scope) return llvm_gen_be_error_t::ok;
  for (a_variable_ptr var = il_header.primary_scope->variables; var != nullptr; var = var->next) {
     llvm_gen_be_error_t err = llvm_lower_global_variable(var);
     if (err != llvm_gen_be_error_t::ok) return err;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a single EDG routine prototype into an LLVM Function.
 * @param[in] routine The EDG routine.
 * @return llvm_gen_be_error_t::ok on success, or a/**
 * @brief llvm_lower_function_prototype
 * @param[in] _p param
 * @return llvm_gen_be_error_t::ok
 */
n error code.
 */
llvm_gen_be_error_t llvm_lower_function_prototype(a_routine_ptr routine) noexcept {
    if (!routine || !routine->source_corresp.name) return llvm_gen_be_error_t::ok;
    
    llvm::FunctionType* func_ty = nullptr;
    llvm::AttributeList attrs;
    llvm_gen_be_error_t err = llvm_gen_be_error_t::ok;
    
    llvm::Triple triple(be_state->module->getTargetTriple());
    if (triple.isAArch64()) {
      err = build_aapcs64_function_type(routine->type, &func_ty, &attrs);
    } else {
      err = build_sysv_function_type(routine->type, &func_ty, &attrs);
    }
    if (err != llvm_gen_be_error_t::ok) return err;
    if (!func_ty) return llvm_gen_be_error_t::ok;

    llvm::GlobalValue::LinkageTypes linkage = llvm::GlobalValue::ExternalLinkage;
    if (routine->function_def_number != 0) {
       if (routine->storage_class == sc_static) {
         linkage = llvm::GlobalValue::InternalLinkage;
       } else if (routine->is_inline) {
         linkage = llvm::GlobalValue::LinkOnceODRLinkage;
       }
    }

    llvm::Function* func = llvm::Function::Create(
        func_ty, linkage, routine->source_corresp.name, be_state->module.get());
    func->setAttributes(attrs);
    err = apply_llvm_attributes(func, routine->source_corresp.attributes);
    if (err != llvm_gen_be_error_t::ok) return err;
        
#if GNU_EXTENSIONS_ALLOWED || MICROSOFT_EXTENSIONS_ALLOWED
    if (routine->always_inline) {
       func->addFnAttr(llvm::Attribute::AlwaysInline);
    }
    if (routine->never_inline) {
       func->addFnAttr(llvm::Attribute::NoInline);
    }
#endif
    
    // Basic calling convention mapping
    if (routine->type && routine->type->kind == tk_routine) {
        a_routine_type_supplement_ptr supp = routine->type->variant.routine.extra_info;
        if (supp) {
#if MICROSOFT_EXTENSIONS_ALLOWED
            if (supp->calling_convention == cc_stdcall) func->setCallingConv(llvm::CallingConv::X86_StdCall);
            else if (supp->calling_convention == cc_fastcall) func->setCallingConv(llvm::CallingConv::X86_FastCall);
            else if (supp->calling_convention == cc_thiscall) func->setCallingConv(llvm::CallingConv::X86_ThisCall);
            else if (supp->calling_convention == cc_vectorcall) func->setCallingConv(llvm::CallingConv::X86_VectorCall);
#endif
        }
    }

    if (routine->type && routine->type->kind == tk_routine && routine->type->variant.routine.extra_info && 
        routine->type->variant.routine.extra_info->does_not_return) {
       func->addFnAttr(llvm::Attribute::NoReturn);
    }

    if (routine->type && routine->type->kind == tk_routine && routine->type->variant.routine.extra_info) {
      unsigned param_idx = 0;
      for (a_param_type_ptr param = routine->type->variant.routine.extra_info->param_type_list;
           param != nullptr; param = param->next, ++param_idx) {
        if (param->type && (get_type_qualifiers(param->type) & TQ_RESTRICT) != 0) {
          func->addParamAttr(param_idx, llvm::Attribute::NoAlias);
        }
      }
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers/**
 * @brief llvm_lower_function_declarations
 * @param[in] _p param
 * @return llvm_gen_be_error_t::ok
 */
 all EDG routine declarations in the translation unit.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
llvm_gen_be_error_t llvm_lower_function_declarations(void) noexcept {
  if (!il_header.primary_scope) return llvm_gen_be_error_t::ok;
  for (a_routine_ptr routine = il_header.primary_scope->routines; routine != nullptr; routine = routine->next) {
    llvm_gen_be_error_t err = llvm_lower_function_prototype(routine);
    if (err != llvm_gen_be_error_t::ok) return err;
  }
  return llvm_gen_be_error_t::ok;
}/**
 * @brief get_scope_for_routine_definition
 * @param[in] _p param
 * @return llvm_gen_be_error_t::ok
 */


/**
 * @brief Gets the lexical scope for a routine definition.
 * @param[in] rout The EDG routine.
 * @param[out] out_scope Pointer to store the resulting scope.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
static llvm_gen_be_error_t get_scope_for_routine_definition(a_routine_ptr rout, a_scope_ptr* out_scope) noexcept {
  if (!rout || !out_scope) return llvm_gen_be_error_t::invalid_argument;
  a_memory_region_number region_number = mem_region_for_routine(rout);
#if IL_SHOULD_BE_WRITTEN_TO_FILE
  if (!skip_il_read && mem_region_table[region_number] == NULL) {
    read_memory_region(region_number);
  }
#endif
  a_scope_ptr res = scope_for_routine(rout);
  if (!res) {
     a_func/**
 * @brief llvm_lower_function_body
 * @param[in] _p param
 * @return llvm_gen_be_error_t::ok
 */
tion_def_descr def_descr = il_header.function_def_table[rout->function_def_number];
     res = def_descr.scope;
  }
  *out_scope = res;
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers a single EDG routine body into LLVM IR.
 * @param[in] routine The EDG routine.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
llvm_gen_be_error_t llvm_lower_function_body(a_routine_ptr routine) noexcept {
    if (!routine || !routine->source_corresp.name) return llvm_gen_be_error_t::ok;
    if (!routine->function_def_number) return llvm_gen_be_error_t::ok; // No body

    llvm::Function* func = be_state->module->getFunction(routine->source_corresp.name);
    if (!func) return llvm_gen_be_error_t::ok;

    llvm::BasicBlock* entry_bb = llvm::BasicBlock::Create(*be_state->context, "entry", func);
    be_state->builder->SetInsertPoint(entry_bb);

    // Look up the function definition
    a_scope_ptr func_scope = nullptr;
    llvm_gen_be_error_t err = get_scope_for_routine_definition(routine, &func_scope);
    if (err != llvm_gen_be_error_t::ok) return err;
    if (!func_scope) return llvm_gen_be_error_t::ok;

    llvm::DISubprogram* di_subprogram = nullptr;
    if (be_state->dbg_state) {
        err = create_di_subprogram(be_state->dbg_state, routine, func, &di_subprogram);
        if (err != llvm_gen_be_error_t::ok) return err;
        be_state->dbg_state->scope_stack.push_back(di_subprogram);
    }

    // Clear local variables and labels for the new function scope
    be_state->local_vars.clear();
    be_state->label_blocks.clear();

    for (a_label_ptr label = func_scope->labels; label != nullptr; label = label->next) {
      be_state->label_blocks[label] = llvm::BasicBlock::Create(*be_state->context, "label", func);
    }

    // Check if we have sret
    bool sret = false;
    err = llvm_gen_be_error_t::ok;
    llvm::Triple triple(be_state->module->getTargetTriple());

    if (triple.isAArch64()) {
       aapcs64_arg_info_t ret_info;
       err = compute_aapcs64_return_info(routine->type->variant.routine.return_type, &ret_info, &sret);
    } else {
       x86_64_abi_arg_info_t ret_info;
       err = compute_sysv_return_info(routine->type->variant.routine.return_type, &ret_info, &sret);
    }
    if (err != llvm_gen_be_error_t::ok) return err;

    unsigned arg_idx = sret ? 1 : 0;
    if (sret && func->arg_size() > 0) {
       func->getArg(0)->setName("sret_ptr");
    }

    // Allocate parameters and bind arguments
    for (a_variable_ptr param = func_scope->variant.routine.parameters; param != nullptr; param = param->next) {
      if (arg_idx < func->arg_size()) {
        llvm::Argument* arg = func->getArg(arg_idx++);
        if (param->source_corresp.name) {
          arg->setName(param->source_corresp.name);
        }

        llvm::Type* param_ty = nullptr;
        err = get_llvm_type(param->type, &param_ty);
        if (err != llvm_gen_be_error_t::ok) return err;
        llvm::AllocaInst* alloca = be_state->builder->CreateAlloca(param_ty, nullptr, param->source_corresp.name ? std::string(param->source_corresp.name) + ".addr" : "");
        
        // Unpacking logic for 1.3.4
        // If the LLVM arg type differs from the EDG param type (due to register packing or byval ptr)
        // we must cast or load it.
        if (arg->hasByValAttr()) {
           // For byval arguments, LLVM passes a pointer to the caller's stack copy.
           // We can just use this pointer as the local variable or copy it.
           // SROA handles pointer casts.
           llvm::Value* cast_arg = be_state->builder->CreatePointerCast(arg, llvm::PointerType::getUnqual(*be_state->context));
           llvm::Value* val = be_state->builder->CreateLoad(param_ty, cast_arg);
           be_state->builder->CreateStore(val, alloca);
        } else if (arg->getType() != param_ty) {
           // Direct register packed struct
           llvm::AllocaInst* temp_alloca = be_state->builder->CreateAlloca(arg->getType());
           be_state->builder->CreateStore(arg, temp_alloca);
           llvm::Value* cast_ptr = be_state->builder->CreatePointerCast(temp_alloca, llvm::PointerType::getUnqual(*be_state->context));
           llvm::Value* unpacked_val = be_state->builder->CreateLoad(param_ty, cast_ptr);
           be_state->builder->CreateStore(unpacked_val, alloca);
        } else {
           be_state->builder->CreateStore(arg, alloca);
        }
        
        be_state->local_vars[param] = alloca;
        
        if (be_state->dbg_state) {
            err = emit_dbg_declare_for_variable(be_state->dbg_state, param, alloca);
            if (err != llvm_gen_be_error_t::ok) return err;
        }
      }
    }

    // Allocate local variables
    for (a_variable_ptr lvar = func_scope->nonstatic_variables; lvar != nullptr; lvar = lvar->next) {
      llvm::Type* lvar_ty = nullptr;
      err = get_llvm_type(lvar->type, &lvar_ty);
      if (err != llvm_gen_be_error_t::ok) return err;
      llvm::AllocaInst* alloca = be_state->builder->CreateAlloca(lvar_ty, nullptr, lvar->source_corresp.name ? lvar->source_corresp.name : "");
      be_state->local_vars[lvar] = alloca;
      
      if (be_state->dbg_state) {
          err = emit_dbg_declare_for_variable(be_state->dbg_state, lvar, alloca);
          if (err != llvm_gen_be_error_t::ok) return err;
      }
    }

    if (func_scope->assoc_block) {
      llvm_gen_be_error_t err = llvm_lower_statement(func_scope->assoc_block);
      if (err != llvm_gen_be_error_t::ok) return err;
    }

    // Ensure all blocks have a terminator
    for (llvm::BasicBlock& bb : *func) {
      if (!bb.getTerminatorOrNull()) {
        be_state->builder->SetInsertPoint(&bb);
        if (func->getReturnType()->isVoidTy()) {
          be_state->builder->CreateRetVoid();
        } else {
          be_state->builder->CreateUnreachable();
        }
      }
    }
    
    std::string err_str;
    llvm::raw_string_ostream os(err_str);
    if (llvm::verifyFunction(*func, &os)) {
      if (be_state->dbg_state) {
        llvm_gen_be_error_t err = pop_lexical_block(be_state->dbg_state);
        if (err != llvm_gen_/**
 * @brief llvm_lower_function_definitions
 * @param[in] _p param
 * @return llvm_gen_be_error_t::ok
 */
be_error_t::ok) return err;
      }
      return llvm_gen_be_error_t::verification_failure;
    }
    if (be_state->dbg_state) {
      llvm_gen_be_error_t err = pop_lexical_block(be_state->dbg_state);
      if (err != llvm_gen_be_error_t::ok) return err;
    }
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers all EDG routine definitions in the translation unit.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
  * @param[in] _p
 */
llvm_gen_be_error_t llvm_lower_function_definitions(void) noexcept {
  if (!il_header.primary_scope) return llvm_gen_be_error_t::ok;

  for (a_routine_ptr routine = il_header.primary_scope->routines; routine != nullptr; routine = routine->next) {
    llvm_gen_be_error_t err = llvm_lower_function_body(routine);
    if (err != llvm_gen_be_error_t::ok) return err;
  }
  return llvm_gen_be_error_t::ok;
}

/**
 * @brief Lowers global constructors and destructors into llvm.global_ctors/dtors.
 * @return llvm_gen_be_error_t::ok on success, or an error code.
 */
llvm_gen_be_error_t llvm_lower_global_ctors_and_dtors(void) noexcept {
  std::vector<llvm::Constant*> ctors;
  std::vector<llvm::Constant*> dtors;
  llvm::Type* int32_ty = llvm::Type::getInt32Ty(*be_state->context);
  llvm::Type* void_fn_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(*be_state->context), false);
  llvm::Type* ptr_ty = llvm::PointerType::getUnqual(*be_state->context);
  llvm::StructType* ctor_struct_ty = llvm::StructType::get(
      int32_ty, ptr_ty, ptr_ty); // { i32, void ()*, i8* }

  if (il_header.primary_scope) {
    for (a_routine_ptr routine = il_header.primary_scope->routines; routine != nullptr; routine = routine->next) {
      if (!routine->source_corresp.name) continue;
      
      bool is_ctor = routine->is_initialization_routine;
      bool is_dtor = routine->is_finalization_routine;
      int ctor_priority_val = 65535;
      int dtor_priority_val = 65535;

#if GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED
#if GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
      if (routine->has_ctor_priority) {
        is_ctor = true;
        ctor_priority_val = gnu_routine_supp(routine)->ctor_priority;
      }
      if (routine->has_dtor_priority) {
        is_dtor = true;
        dtor_priority_val = gnu_routine_supp(routine)->dtor_priority;
      }
#if DO_IL_LOWERING
      if (routine->is_initialization_routine && gnu_routine_supp_or_null(routine) && gnu_routine_supp(routine)->init_priority != 0) {
         ctor_priority_val = gnu_routine_supp(routine)->init_priority;
      }
      if (routine->is_finalization_routine && gnu_routine_supp_or_null(routine) && gnu_routine_supp(routine)->init_priority != 0) {
         dtor_priority_val = gnu_routine_supp(routine)->init_priority;
      }
#endif // DO_IL_LOWERING
#endif // GNU_INIT_PRIORITY_ATTRIBUTE_ALLOWED
#endif // GNU_EXTENSIONS_ALLOWED || REDEFINE_EXTNAME_PRAGMA_ENABLED

      if (is_ctor || is_dtor) {
        llvm::Function* func = be_state->module->getFunction(routine->source_corresp.name);
        if (func) {
          llvm::Constant* fn_ptr = func;
          llvm::Constant* null_ptr = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptr_ty));
          
          if (is_ctor) {
            llvm::Constant* priority = llvm::ConstantInt::get(int32_ty, ctor_priority_val);
            ctors.push_back(llvm::ConstantStruct::get(ctor_struct_ty, {priority, fn_ptr, null_ptr}));
          }
          if (is_dtor) {
            llvm::Constant* priority = llvm::ConstantInt::get(int32_ty, dtor_priority_val);
            dtors.push_back(llvm::ConstantStruct::get(ctor_struct_ty, {priority, fn_ptr, null_ptr}));
          }
        }
      }
    }
  }

  if (!ctors.empty()) {
    llvm::ArrayType* ctors_array_ty = llvm::ArrayType::get(ctor_struct_ty, ctors.size());
    llvm::Constant* ctors_array = llvm::ConstantArray::get(ctors_array_ty, ctors);
    new llvm::GlobalVariable(
        *be_state->module, ctors_array_ty, false,
        llvm::GlobalValue::AppendingLinkage, ctors_array, "llvm.global_ctors");
  }
  
  if (!dtors.empty()) {
    llvm::ArrayType* dtors_array_ty = llvm::ArrayType::get(ctor_struct_ty, dtors.size());
    llvm::Constant* dtors_array = llvm::ConstantArray::get(dtors_array_ty, dtors);
    new llvm::GlobalVariable(
        *be_state->module, dtors_array_ty, false,
        llvm::GlobalValue::AppendingLinkage, dtors_array, "llvm.global_dtors");
  }
  return llvm_gen_be_error_t::ok;
}

END_EDG_NAMESPACE
#endif
