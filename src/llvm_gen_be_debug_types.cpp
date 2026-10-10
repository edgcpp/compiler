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
            di_ty = dbg_state->builder->createPointerType(pointee, 64);
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
            di_ty = dbg_state->builder->createStructType(
                dbg_state->compile_unit,
                "struct",
                dbg_state->compile_unit->getFile(),
                0,
                ty->size * 8,
                0,
                llvm::DINode::FlagZero,
                nullptr,
                dbg_state->builder->getOrCreateArray(llvm::ArrayRef<llvm::Metadata*>())
            );
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

END_EDG_NAMESPACE
#endif // BACK_END_IS_LLVM_GEN_BE
