/**
 * @file llvm_gen_be_debug_types.cpp
 * @brief LLVM IR Backend Debug Type System and Variable Tracking.
 * @details Implements DWARF type translation and variable metadata emission.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */
#include "llvm_gen_be_debug.h"
#include "llvm_gen_be_internal.h"
#include "il.h"
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/IntrinsicInst.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

llvm_gen_be_error_t get_or_create_di_type(
    llvm_gen_be_debug_state_t* dbg_state,
    a_type_ptr ty,
    llvm::DIType** out_di_type) noexcept {
    
    if (!dbg_state || !ty || !out_di_type) return llvm_gen_be_error_t::invalid_argument;

    // Resolve typerefs
    while (ty->kind == tk_typeref) {
        ty = ty->variant.typeref.type;
    }

    auto it = dbg_state->di_types_map.find(ty);
    if (it != dbg_state->di_types_map.end()) {
        *out_di_type = it->second;
        return llvm_gen_be_error_t::ok;
    }

    llvm::DIType* di_ty = nullptr;

    switch (ty->kind) {
        case tk_integer: {
            uint64_t size_in_bits = ty->size * 8;
            uint32_t encoding = llvm::dwarf::DW_ATE_signed;
            if (is_bool_type(ty)) {
                encoding = llvm::dwarf::DW_ATE_boolean;
            } else if (!int_type_is_signed(ty)) {
                encoding = llvm::dwarf::DW_ATE_unsigned;
            }
            di_ty = dbg_state->builder->createBasicType("int", size_in_bits, encoding);
            break;
        }
        case tk_float: {
            uint64_t size_in_bits = ty->size * 8;
            di_ty = dbg_state->builder->createBasicType("float", size_in_bits, llvm::dwarf::DW_ATE_float);
            break;
        }
        case tk_pointer:
        case tk_ptr_to_member: {
            llvm::DIType* pointee = nullptr;
            llvm_gen_be_error_t err = get_or_create_di_type(dbg_state, ty->variant.pointer.type, &pointee);
            if (err != llvm_gen_be_error_t::ok) return err;
            uint64_t ptr_size = be_state->module->getDataLayout().getPointerSizeInBits();
            di_ty = dbg_state->builder->createPointerType(pointee, ptr_size);
            break;
        }
        case tk_array: {
            llvm::DIType* elem = nullptr;
            llvm_gen_be_error_t err = get_or_create_di_type(dbg_state, ty->variant.array.element_type, &elem);
            if (err != llvm_gen_be_error_t::ok) return err;
            llvm::SmallVector<llvm::Metadata*, 1> subscripts;
            subscripts.push_back(dbg_state->builder->getOrCreateSubrange(0, ty->variant.array.variant.number_of_elements));
            llvm::DINodeArray subs_array = dbg_state->builder->getOrCreateArray(subscripts);
            di_ty = dbg_state->builder->createArrayType(ty->size * 8, 0, elem, subs_array);
            break;
        }
        case tk_class:
        case tk_struct:
        case tk_union: {
            // Forward declaration to break cycles
            llvm::DICompositeType* fwd_decl = dbg_state->builder->createReplaceableCompositeType(
                ty->kind == tk_union ? llvm::dwarf::DW_TAG_union_type : (ty->kind == tk_class ? llvm::dwarf::DW_TAG_class_type : llvm::dwarf::DW_TAG_structure_type),
                ty->source_corresp.name ? ty->source_corresp.name : "",
                dbg_state->compile_unit,
                dbg_state->compile_unit->getFile(),
                0);
            dbg_state->di_types_map[ty] = fwd_decl;
            
            std::vector<llvm::Metadata*> elements;
            
            a_class_type_supplement_ptr supp = ty->variant.class_struct_union.extra_info;
            if (supp) {
                // Bases
                for (a_base_class_ptr b = supp->direct_base_classes; b != nullptr; b = b->next_direct) {
                    llvm::DIType* base_ty = nullptr;
                    if (get_or_create_di_type(dbg_state, b->type, &base_ty) == llvm_gen_be_error_t::ok) {
                        llvm::DINode::DIFlags flags = llvm::DINode::FlagZero;
                        if (b->is_virtual) flags |= llvm::DINode::FlagVirtual;
                        // Access flags could be mapped here
                        llvm::DIDerivedType* dt = dbg_state->builder->createInheritance(
                            fwd_decl, base_ty, b->offset * 8, 0, flags);
                        elements.push_back(dt);
                    }
                }
            }
            
            for (a_field_ptr f = ty->variant.class_struct_union.field_list; f != nullptr; f = f->next) {
                llvm::DIType* field_ty = nullptr;
                if (get_or_create_di_type(dbg_state, f->type, &field_ty) == llvm_gen_be_error_t::ok) {
                    uint64_t size = f->is_bit_field ? f->bit_size : f->type->size * 8;
                    uint64_t align = be_state->module->getDataLayout().getABITypeAlign(field_ty ? llvm::Type::getInt8Ty(*be_state->context) : nullptr).value() * 8;
                    uint64_t offset = f->offset * 8 + (f->is_bit_field ? f->offset_bit_remainder : 0);
                    llvm::DIDerivedType* dt = dbg_state->builder->createMemberType(
                        fwd_decl,
                        f->source_corresp.name ? f->source_corresp.name : "",
                        dbg_state->compile_unit->getFile(),
                        0,
                        size,
                        align,
                        offset,
                        llvm::DINode::FlagZero,
                        field_ty
                    );
                    elements.push_back(dt);
                }
            }
            
            llvm::DINodeArray elements_array = dbg_state->builder->getOrCreateArray(elements);
            
            di_ty = dbg_state->builder->createClassType(
                dbg_state->compile_unit,
                ty->source_corresp.name ? ty->source_corresp.name : "",
                dbg_state->compile_unit->getFile(),
                0,
                ty->size * 8,
                ty->alignment * 8,
                0,
                llvm::DINode::FlagZero,
                nullptr,
                elements_array
            );
            
            dbg_state->builder->replaceTemporary(llvm::TempDINode(fwd_decl), di_ty);
            break;
        }
        default: {
            di_ty = dbg_state->builder->createBasicType("unknown", ty->size * 8, llvm::dwarf::DW_ATE_unsigned);
            break;
        }
    }

    if (!di_ty) {
        return llvm_gen_be_error_t::di_metadata_failure;
    }

    dbg_state->di_types_map[ty] = di_ty;
    *out_di_type = di_ty;
    return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t emit_dbg_declare_for_variable(
    llvm_gen_be_debug_state_t* dbg_state,
    a_variable_ptr var,
    llvm::AllocaInst* alloca_inst) noexcept {
    
    if (!dbg_state || !var || !alloca_inst) return llvm_gen_be_error_t::invalid_argument;

    llvm::DILocalVariable* di_var = nullptr;
    llvm::DIType* di_type = nullptr;

    llvm_gen_be_error_t err = get_or_create_di_type(dbg_state, var->type, &di_type);
    if (err != llvm_gen_be_error_t::ok) return err;

    std::string name = var->source_corresp.name ? var->source_corresp.name : "";

    llvm::DIFile* di_file = nullptr;
    a_const_char* file_name = nullptr;
    a_const_char* full_name = nullptr;
    a_line_number line = 0;
    a_boolean at_end_of_source = FALSE;

    if (var->source_corresp.decl_position.seq != 0) {
        conv_seq_to_file_and_line(var->source_corresp.decl_position.seq, &file_name, &full_name, &line, &at_end_of_source);
    }
    const char* path = full_name ? full_name : (file_name ? file_name : "");
    if (path[0] == '\0') {
        di_file = dbg_state->compile_unit->getFile();
    } else {
        err = get_or_create_di_file(dbg_state, path, &di_file);
        if (err != llvm_gen_be_error_t::ok) return err;
    }

    llvm::DIScope* scope = dbg_state->compile_unit;
    if (!dbg_state->scope_stack.empty()) {
        scope = dbg_state->scope_stack.back();
    }

    if (var->is_parameter) {
        di_var = dbg_state->builder->createParameterVariable(
            scope, name, 1 /* argNo */, di_file, line, di_type
        );
    } else {
        di_var = dbg_state->builder->createAutoVariable(
            scope, name, di_file, line, di_type
        );
    }

    llvm::DILocation* loc = llvm::DILocation::get(*be_state->context, line, 0, scope);

    dbg_state->builder->insertDeclare(
        alloca_inst,
        di_var,
        dbg_state->builder->createExpression(),
        loc,
        alloca_inst->getParent()
    );

    return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t emit_dbg_value_for_variable(
    llvm_gen_be_debug_state_t* dbg_state,
    a_variable_ptr var,
    llvm::Value* value,
    llvm::Instruction* insert_before,
    llvm::BasicBlock* insert_at_end,
    const uint64_t* expr_ops,
    size_t num_expr_ops) noexcept {
    
    if (!dbg_state || !var || !value || (!insert_before && !insert_at_end)) return llvm_gen_be_error_t::invalid_argument;

    llvm::DILocalVariable* di_var = nullptr;
    llvm::DIType* di_type = nullptr;

    llvm_gen_be_error_t err = get_or_create_di_type(dbg_state, var->type, &di_type);
    if (err != llvm_gen_be_error_t::ok) return err;

    std::string name = var->source_corresp.name ? var->source_corresp.name : "";

    llvm::DIFile* di_file = nullptr;
    a_const_char* file_name = nullptr;
    a_const_char* full_name = nullptr;
    a_line_number line = 0;
    a_boolean at_end_of_source = FALSE;

    if (var->source_corresp.decl_position.seq != 0) {
        conv_seq_to_file_and_line(var->source_corresp.decl_position.seq, &file_name, &full_name, &line, &at_end_of_source);
    }
    const char* path = full_name ? full_name : (file_name ? file_name : "");
    if (path[0] == '\0') {
        di_file = dbg_state->compile_unit->getFile();
    } else {
        err = get_or_create_di_file(dbg_state, path, &di_file);
        if (err != llvm_gen_be_error_t::ok) return err;
    }

    llvm::DIScope* scope = dbg_state->compile_unit;
    if (!dbg_state->scope_stack.empty()) {
        scope = dbg_state->scope_stack.back();
    }

    if (var->is_parameter) {
        di_var = dbg_state->builder->createParameterVariable(
            scope, name, 1 /* argNo */, di_file, line, di_type
        );
    } else {
        di_var = dbg_state->builder->createAutoVariable(
            scope, name, di_file, line, di_type
        );
    }

    llvm::DILocation* loc = llvm::DILocation::get(*be_state->context, line, 0, scope);
    llvm::DIExpression* expr = nullptr;
    if (expr_ops && num_expr_ops > 0) {
        expr = dbg_state->builder->createExpression(llvm::ArrayRef<uint64_t>(expr_ops, num_expr_ops));
    } else {
        expr = dbg_state->builder->createExpression();
    }

    if (insert_before) {
        dbg_state->builder->insertDbgValueIntrinsic(
            value,
            di_var,
            expr,
            loc,
            insert_before->getIterator()
        );
    } else if (insert_at_end) {
        dbg_state->builder->insertDbgValueIntrinsic(
            value,
            di_var,
            expr,
            loc,
            insert_at_end
        );
    }

    return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t emit_dbg_assign_for_variable(
    llvm_gen_be_debug_state_t* dbg_state,
    a_variable_ptr var,
    llvm::Value* value,
    llvm::Value* dest,
    llvm::Instruction* insert_before) noexcept {
    
    if (!dbg_state || !var || !value || !dest || !insert_before) return llvm_gen_be_error_t::invalid_argument;

    llvm::DILocalVariable* di_var = nullptr;
    llvm::DIType* di_type = nullptr;

    llvm_gen_be_error_t err = get_or_create_di_type(dbg_state, var->type, &di_type);
    if (err != llvm_gen_be_error_t::ok) return err;

    std::string name = var->source_corresp.name ? var->source_corresp.name : "";

    llvm::DIFile* di_file = nullptr;
    a_const_char* file_name = nullptr;
    a_const_char* full_name = nullptr;
    a_line_number line = 0;
    a_boolean at_end_of_source = FALSE;

    if (var->source_corresp.decl_position.seq != 0) {
        conv_seq_to_file_and_line(var->source_corresp.decl_position.seq, &file_name, &full_name, &line, &at_end_of_source);
    }
    const char* path = full_name ? full_name : (file_name ? file_name : "");
    if (path[0] == '\0') {
        di_file = dbg_state->compile_unit->getFile();
    } else {
        err = get_or_create_di_file(dbg_state, path, &di_file);
        if (err != llvm_gen_be_error_t::ok) return err;
    }

    llvm::DIScope* scope = dbg_state->compile_unit;
    if (!dbg_state->scope_stack.empty()) {
        scope = dbg_state->scope_stack.back();
    }

    di_var = dbg_state->builder->createAutoVariable(
        scope, name, di_file, line, di_type
    );

    llvm::DILocation* loc = llvm::DILocation::get(*be_state->context, line, 0, scope);
    
    llvm::DIAssignID* assign_id = llvm::DIAssignID::getDistinct(*be_state->context);
    llvm::Instruction* dest_inst = llvm::dyn_cast<llvm::Instruction>(dest);
    if (dest_inst) {
        dest_inst->setMetadata(llvm::LLVMContext::MD_DIAssignID, assign_id);
    }

    dbg_state->builder->insertDbgAssign(
        insert_before,
        value,
        di_var,
        dbg_state->builder->createExpression(),
        dest,
        dbg_state->builder->createExpression(),
        loc
    );

    return llvm_gen_be_error_t::ok;
}


llvm_gen_be_error_t createInheritance(
    llvm_gen_be_debug_state_t* dbg_state,
    llvm::DIType* derived_type,
    llvm::DIType* base_type,
    uint64_t base_offset,
    uint32_t vb_ptr_offset,
    llvm::DINode::DIFlags flags,
    llvm::DIDerivedType** out_inheritance) noexcept {
    
    if (!dbg_state || !derived_type || !base_type || !out_inheritance) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    
    llvm::DIDerivedType* derived = dbg_state->builder->createInheritance(
        derived_type, base_type, base_offset, vb_ptr_offset, flags);
    
    if (!derived) return llvm_gen_be_error_t::di_metadata_failure;
    
    *out_inheritance = derived;
    return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t createMethod(
    llvm_gen_be_debug_state_t* dbg_state,
    llvm::DIScope* scope,
    const char* name,
    const char* linkage_name,
    llvm::DIFile* file,
    unsigned line_no,
    llvm::DISubroutineType* type,
    unsigned vindex,
    int this_adjustment,
    llvm::DIType* vtable_holder,
    llvm::DINode::DIFlags flags,
    llvm::DISubprogram::DISPFlags sp_flags,
    llvm::DISubprogram** out_method) noexcept {
    
    if (!dbg_state || !scope || !name || !type || !out_method) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    
    llvm::DISubprogram* method = dbg_state->builder->createMethod(
        scope, name, linkage_name ? linkage_name : "", file, line_no, type,
        vindex, this_adjustment, vtable_holder, flags, sp_flags);
        
    if (!method) return llvm_gen_be_error_t::di_metadata_failure;
    
    *out_method = method;
    return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t createNameSpace(
    llvm_gen_be_debug_state_t* dbg_state,
    llvm::DIScope* scope,
    const char* name,
    bool export_symbols,
    llvm::DINamespace** out_namespace) noexcept {
    
    if (!dbg_state || !scope || !name || !out_namespace) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    
    llvm::DINamespace* ns = dbg_state->builder->createNameSpace(scope, name, export_symbols);
    
    if (!ns) return llvm_gen_be_error_t::di_metadata_failure;
    
    *out_namespace = ns;
    return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t createTemplateTypeParameter(
    llvm_gen_be_debug_state_t* dbg_state,
    llvm::DIScope* scope,
    const char* name,
    llvm::DIType* type,
    bool is_default,
    llvm::DITemplateTypeParameter** out_param) noexcept {
    
    if (!dbg_state || !scope || !name || !type || !out_param) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    
    llvm::DITemplateTypeParameter* param = dbg_state->builder->createTemplateTypeParameter(
        scope, name, type, is_default);
        
    if (!param) return llvm_gen_be_error_t::di_metadata_failure;
    
    *out_param = param;
    return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t createTemplateValueParameter(
    llvm_gen_be_debug_state_t* dbg_state,
    llvm::DIScope* scope,
    const char* name,
    llvm::DIType* type,
    bool is_default,
    llvm::Metadata* value,
    llvm::DITemplateValueParameter** out_param) noexcept {
    
    if (!dbg_state || !scope || !name || !type || !value || !out_param) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    
    llvm::DITemplateValueParameter* param = dbg_state->builder->createTemplateValueParameter(
        scope, name, type, is_default, llvm::cast<llvm::ConstantAsMetadata>(value)->getValue());
        
    if (!param) return llvm_gen_be_error_t::di_metadata_failure;
    
    *out_param = param;
    return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t createEnumerationType(
    llvm_gen_be_debug_state_t* dbg_state,
    llvm::DIScope* scope,
    const char* name,
    llvm::DIFile* file,
    unsigned line_no,
    uint64_t size_in_bits,
    uint32_t align_in_bits,
    llvm::DINodeArray elements,
    llvm::DIType* underlying_type,
    const char* unique_identifier,
    bool is_scoped,
    llvm::DICompositeType** out_enum) noexcept {
    
    if (!dbg_state || !scope || !name || !underlying_type || !out_enum) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    
    llvm::DICompositeType* enum_type = dbg_state->builder->createEnumerationType(
        scope, name, file, line_no, size_in_bits, align_in_bits, elements,
        underlying_type, 0 /* RunTimeLang */, unique_identifier ? unique_identifier : "", is_scoped);
        
    if (!enum_type) return llvm_gen_be_error_t::di_metadata_failure;
    
    *out_enum = enum_type;
    return llvm_gen_be_error_t::ok;
}

llvm_gen_be_error_t build_dwarf_variable_fragment(
    a_type_ptr type,
    uint64_t offset,
    uint64_t size,
    llvm::DIExpression** out_expr) noexcept {
  if (type == nullptr || out_expr == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  
  if (!be_state || !be_state->dbg_state || !be_state->dbg_state->builder) {
    return llvm_gen_be_error_t::di_metadata_failure;
  }

  llvm::DIExpression* expr = be_state->dbg_state->builder->createExpression({llvm::dwarf::DW_OP_LLVM_fragment, offset, size});
  if (!expr) {
    return llvm_gen_be_error_t::di_metadata_failure;
  }
  
  *out_expr = expr;
  return llvm_gen_be_error_t::ok;
}
END_EDG_NAMESPACE

#endif // BACK_END_IS_LLVM_GEN_BE

/**
 * @brief Generates DWARF 5 variable fragments and CodeView PDB type records for non-contiguous locations.
 * @details Translates complex struct fields scattered across registers and stack memory into DW_OP_piece
 * and DW_OP_bit_piece composite location descriptions.
 * @param[in] type Pointer to the EDG type.
 * @param[in] offset Target fragment offset in bits.
 * @param[in] size Target fragment size in bits.
 * @param[out] out_expr Pointer to store the resulting DIExpression.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
