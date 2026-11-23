/**
 * @file llvm_gen_be_debug_types.cpp
 * @ingroup llvm_gen_be_debug
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
#include <llvm/ADT/APSInt.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/IntrinsicInst.h>

#if BACK_END_IS_LLVM_GEN_BE
BEGIN_EDG_NAMESPACE

/**
 * @brief Retrieves or creates an LLVM `DIType` metadata node for an EDG type.
 * @details Translates fundamental integer, floating-point, enumeration (`tk_enum` or
 *          `integer.enum_type`), pointer, array, routine (`tk_routine`), void (`tk_void`),
 *          and composite (`tk_class`, `tk_struct`, `tk_union`) types into DWARF/CodeView debug types.
 * @param[in,out] dbg_state The debug state containing the `DIBuilder` and type cache.
 * @param[in] ty The EDG type to translate.
 * @param[out] out_di_type Receives the resulting `llvm::DIType*`.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t get_or_create_di_type(
    llvm_gen_be_debug_state_t* dbg_state,
    a_type_ptr ty,
    llvm::DIType** out_di_type) noexcept {
    
    if (!dbg_state || !ty || !out_di_type) return llvm_gen_be_error_t::invalid_argument;
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());

    a_type_ptr orig_ty = ty;
    a_type_qualifier_set quals = 0;
    // Resolve typerefs while accumulating CVR/atomic qualifiers
    while (ty->kind == tk_typeref) {
        quals |= ty->variant.typeref.qualifiers;
        ty = ty->variant.typeref.type;
    }

    llvm::DIType* di_ty = nullptr;
    auto it = dbg_state->di_types_map.find(ty);
    if (it != dbg_state->di_types_map.end()) {
        di_ty = it->second;
    } else {
        switch (ty->kind) {
        case tk_void: {
            di_ty = dbg_state->builder->createUnspecifiedType("void");
        } break;
        case tk_nullptr: {
            di_ty = dbg_state->builder->createNullPtrType();
        } break;
        case tk_integer: {
            uint64_t size_in_bits = static_cast<uint64_t>((ty->size > 0 ? ty->size : 4) * 8);
            uint32_t align_in_bits = static_cast<uint32_t>((ty->alignment > 0 ? ty->alignment : 4) * 8);
            uint32_t encoding = llvm::dwarf::DW_ATE_signed;
            an_integer_kind ik = ty->variant.integer.int_kind;
            if (is_bool_type(ty)) {
                encoding = llvm::dwarf::DW_ATE_boolean;
            } else if (ik == ik_char || ik == ik_signed_char) {
                encoding = llvm::dwarf::DW_ATE_signed_char;
            } else if (ik == ik_unsigned_char) {
                encoding = llvm::dwarf::DW_ATE_unsigned_char;
            } else if (ty->variant.integer.char8_t_type ||
                       ty->variant.integer.char16_t_type ||
                       ty->variant.integer.char32_t_type) {
                encoding = llvm::dwarf::DW_ATE_UTF;
            } else if (!int_type_is_signed(ty)) {
                encoding = llvm::dwarf::DW_ATE_unsigned;
            }
            const char* int_name = ty->source_corresp.name ? ty->source_corresp.name : "int";
            llvm::DIType* basic_int_ty = dbg_state->builder->createBasicType(int_name, size_in_bits, encoding);
            if (ty->variant.integer.enum_type || ty->variant.integer.enum_info.constant_list != nullptr) {
                std::vector<llvm::Metadata*> enum_elts;
                a_constant_ptr c_list = nullptr;
                if (ty->variant.integer.is_scoped_enum) {
                    if (ty->variant.integer.enum_info.assoc_scope) {
                        c_list = ty->variant.integer.enum_info.assoc_scope->constants;
                    }
                } else {
                    c_list = ty->variant.integer.enum_info.constant_list;
                }
                for (a_constant_ptr cp = c_list; cp != nullptr; cp = cp->next) {
                    const char* e_name = cp->source_corresp.name ? cp->source_corresp.name : "enumerator";
                    uint64_t enum_uval = EDG_INT_VAL_LOW64(cp->variant.integer_value);
                    enum_elts.push_back(dbg_state->builder->createEnumerator(
                        e_name, llvm::APSInt(llvm::APInt(64, enum_uval), !int_type_is_signed(ty))));
                }
                llvm::DINodeArray elts_arr = dbg_state->builder->getOrCreateArray(enum_elts);
                di_ty = dbg_state->builder->createEnumerationType(
                    dbg_state->compile_unit,
                    ty->source_corresp.name ? ty->source_corresp.name : "",
                    dbg_state->compile_unit->getFile(),
                    0,
                    size_in_bits,
                    align_in_bits,
                    elts_arr,
                    basic_int_ty,
#if LLVM_VERSION_MAJOR >= 18
                    0,
#endif
                    "",
                    ty->variant.integer.is_scoped_enum != 0);
            } else {
                di_ty = basic_int_ty;
            }
        } break;
        case tk_routine: {
            std::vector<llvm::Metadata*> param_types;
            if (ty->variant.routine.return_type && ty->variant.routine.return_type->kind != tk_void) {
                llvm::DIType* ret_di_ty = nullptr;
                LLVM_GEN_BE_CHECK(get_or_create_di_type(dbg_state, ty->variant.routine.return_type, &ret_di_ty));
                param_types.push_back(ret_di_ty);
            } else {
                param_types.push_back(nullptr);
            }
            if (ty->variant.routine.extra_info) {
                for (a_param_type_ptr pt = ty->variant.routine.extra_info->param_type_list; pt != nullptr; pt = pt->next) {
                    if (pt->type) {
                        llvm::DIType* p_di_ty = nullptr;
                        LLVM_GEN_BE_CHECK(get_or_create_di_type(dbg_state, pt->type, &p_di_ty));
                        param_types.push_back(p_di_ty);
                    }
                }
            }
            di_ty = dbg_state->builder->createSubroutineType(
                dbg_state->builder->getOrCreateTypeArray(param_types));
        } break;
        case tk_float: {
            uint64_t size_in_bits = static_cast<uint64_t>(ty->size) * 8;
            di_ty = dbg_state->builder->createBasicType("float", size_in_bits, llvm::dwarf::DW_ATE_float);
        } break;
        case tk_complex: {
            uint64_t size_in_bits = static_cast<uint64_t>(ty->size > 0 ? ty->size : 8) * 8;
            di_ty = dbg_state->builder->createBasicType("_Complex", size_in_bits, llvm::dwarf::DW_ATE_complex_float);
        } break;
#if FIXED_POINT_ALLOWED
        case tk_fixed_point: {
            uint64_t size_in_bits = static_cast<uint64_t>(ty->size > 0 ? ty->size : 4) * 8;
            uint32_t fp_enc = ty->variant.fixed_point.is_unsigned
                                  ? llvm::dwarf::DW_ATE_unsigned_fixed
                                  : llvm::dwarf::DW_ATE_signed_fixed;
            di_ty = dbg_state->builder->createBasicType("_Fract", size_in_bits, fp_enc);
        } break;
#endif
        case tk_pointer: {
            llvm::DIType* pointee = nullptr;
            LLVM_GEN_BE_CHECK(get_or_create_di_type(dbg_state, ty->variant.pointer.type, &pointee));
            uint64_t ptr_size = be_state->module->getDataLayout().getPointerSizeInBits();
            if (ty->variant.pointer.is_reference) {
                uint32_t ref_tag = ty->variant.pointer.is_rvalue_reference
                                       ? llvm::dwarf::DW_TAG_rvalue_reference_type
                                       : llvm::dwarf::DW_TAG_reference_type;
                di_ty = dbg_state->builder->createReferenceType(ref_tag, pointee, ptr_size);
            } else {
                di_ty = dbg_state->builder->createPointerType(pointee, ptr_size);
            }
        } break;
        case tk_ptr_to_member: {
            llvm::DIType* pointee = nullptr;
            LLVM_GEN_BE_CHECK(get_or_create_di_type(dbg_state, ty->variant.ptr_to_member.type, &pointee));
            llvm::DIType* cls_di_ty = nullptr;
            if (ty->variant.ptr_to_member.class_of_which_a_member) {
                LLVM_GEN_BE_CHECK(get_or_create_di_type(dbg_state, ty->variant.ptr_to_member.class_of_which_a_member, &cls_di_ty));
            }
            uint64_t ptr_size = be_state->module->getDataLayout().getPointerSizeInBits();
            if (cls_di_ty) {
                di_ty = dbg_state->builder->createMemberPointerType(pointee, cls_di_ty, ptr_size);
            } else {
                di_ty = dbg_state->builder->createPointerType(pointee, ptr_size);
            }
        } break;
        case tk_array: {
            a_type_ptr elem_edg_ty = ty->variant.array.element_type;
            llvm::SmallVector<llvm::Metadata*, 2> subscripts;
            subscripts.push_back(dbg_state->builder->getOrCreateSubrange(0, static_cast<int64_t>(ty->variant.array.variant.number_of_elements)));
            while (elem_edg_ty->kind == tk_array) {
                subscripts.push_back(dbg_state->builder->getOrCreateSubrange(0, static_cast<int64_t>(elem_edg_ty->variant.array.variant.number_of_elements)));
                elem_edg_ty = elem_edg_ty->variant.array.element_type;
            }
            llvm::DIType* elem = nullptr;
            LLVM_GEN_BE_CHECK(get_or_create_di_type(dbg_state, elem_edg_ty, &elem));
            llvm::DINodeArray subs_array = dbg_state->builder->getOrCreateArray(subscripts);
            di_ty = dbg_state->builder->createArrayType(static_cast<uint64_t>(ty->size) * 8, 0, elem, subs_array);
        } break;
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
                // Bases (including virtual and access flags)
                for (a_base_class_ptr b = supp->direct_base_classes; b != nullptr; b = b->next_direct) {
                    llvm::DIType* base_ty = nullptr;
                    LLVM_GEN_BE_CHECK(get_or_create_di_type(dbg_state, b->type, &base_ty));
                    llvm::DINode::DIFlags flags = llvm::DINode::FlagZero;
                    if (b->is_virtual) flags |= llvm::DINode::FlagVirtual;
                    if (b->derivation) {
                        if (b->derivation->access == as_public) flags |= llvm::DINode::FlagPublic;
                        else if (b->derivation->access == as_protected) flags |= llvm::DINode::FlagProtected;
                        else if (b->derivation->access == as_private) flags |= llvm::DINode::FlagPrivate;
                    } else if (b->has_public_derivation) {
                        flags |= llvm::DINode::FlagPublic;
                    }
                    llvm::DIDerivedType* dt = dbg_state->builder->createInheritance(
                        fwd_decl, base_ty, static_cast<uint64_t>(b->offset) * 8, 0, flags);
                    elements.push_back(dt);
                }
                // Member function declarations in associated scope
                if (supp->assoc_scope) {
                    for (a_routine_ptr r = supp->assoc_scope->routines; r != nullptr; r = r->next) {
                        const char* r_name = r->source_corresp.name ? r->source_corresp.name : "method";
                        llvm::DIType* r_di_ty = nullptr;
                        if (r->type) {
                            LLVM_GEN_BE_CHECK(get_or_create_di_type(dbg_state, r->type, &r_di_ty));
                        }
                        llvm::DISubroutineType* sub_ty = llvm::dyn_cast_or_null<llvm::DISubroutineType>(r_di_ty);
                        if (!sub_ty) {
                            sub_ty = dbg_state->builder->createSubroutineType(
                                dbg_state->builder->getOrCreateTypeArray(llvm::ArrayRef<llvm::Metadata*>()));
                        }
                        llvm::DISubprogram::DISPFlags sp_flags = llvm::DISubprogram::SPFlagZero;
                        unsigned vindex = 0;
                        llvm::DIType* vholder = nullptr;
                        if (r->is_virtual) {
                            sp_flags |= r->pure_virtual ? llvm::DISubprogram::SPFlagPureVirtual
                                                        : llvm::DISubprogram::SPFlagVirtual;
                            vindex = (r->number.virtual_function > 0)
                                         ? static_cast<unsigned>(r->number.virtual_function)
                                         : 0u;
                            vholder = fwd_decl;
                        }
                        llvm::DISubprogram* method_sp = dbg_state->builder->createMethod(
                            dbg_state->compile_unit, r_name, r_name, dbg_state->compile_unit->getFile(), 0,
                            sub_ty, vindex, 0, vholder, llvm::DINode::FlagPrototyped, sp_flags);
                        elements.push_back(method_sp);
                    }
                }
            }
            
            for (a_field_ptr f = ty->variant.class_struct_union.field_list; f != nullptr; f = f->next) {
                llvm::DIType* field_ty = nullptr;
                LLVM_GEN_BE_CHECK(get_or_create_di_type(dbg_state, f->type, &field_ty));
                uint64_t size = f->is_bit_field ? static_cast<uint64_t>(f->bit_size) : static_cast<uint64_t>(skip_typerefs(f->type)->size) * 8;
                uint32_t align = static_cast<uint32_t>(be_state->module->getDataLayout().getABITypeAlign(llvm::Type::getInt8Ty(*be_state->context)).value() * 8);
                uint64_t offset = static_cast<uint64_t>(f->offset) * 8 + (f->is_bit_field ? static_cast<uint64_t>(f->offset_bit_remainder) : 0ULL);
                llvm::DIDerivedType* dt = nullptr;
                if (f->is_bit_field) {
                    dt = dbg_state->builder->createBitFieldMemberType(
                        dbg_state->compile_unit,
                        f->source_corresp.name ? f->source_corresp.name : "",
                        dbg_state->compile_unit->getFile(),
                        0,
                        size,
                        offset,
                        static_cast<uint64_t>(f->offset) * 8,
                        llvm::DINode::FlagBitField,
                        field_ty
                    );
                } else {
                    dt = dbg_state->builder->createMemberType(
                        dbg_state->compile_unit,
                        f->source_corresp.name ? f->source_corresp.name : "",
                        dbg_state->compile_unit->getFile(),
                        0,
                        size,
                        align,
                        offset,
                        llvm::DINode::FlagZero,
                        field_ty
                    );
                }
                elements.push_back(dt);
            }
            
            llvm::DINodeArray elements_array = dbg_state->builder->getOrCreateArray(elements);
            llvm::DINodeArray tpl_params_array;
            if (supp && supp->template_arg_list) {
                std::vector<llvm::Metadata*> tpl_params;
                for (a_template_arg_ptr ta = supp->template_arg_list; ta != nullptr; ta = ta->next) {
                    if (ta->kind == tak_type && ta->variant.type) {
                        llvm::DIType* ta_di_ty = nullptr;
                        LLVM_GEN_BE_CHECK(get_or_create_di_type(dbg_state, ta->variant.type, &ta_di_ty));
                        const char* tp_name = ta->variant.type->source_corresp.name
                                                  ? ta->variant.type->source_corresp.name
                                                  : "T";
                        tpl_params.push_back(dbg_state->builder->createTemplateTypeParameter(
                            dbg_state->compile_unit, tp_name, ta_di_ty, false));
                    } else if (ta->kind == tak_nontype && ta->variant.constant) {
                        a_constant_ptr tc = ta->variant.constant;
                        llvm::DIType* tc_di_ty = nullptr;
                        if (tc->type) {
                            LLVM_GEN_BE_CHECK(get_or_create_di_type(dbg_state, tc->type, &tc_di_ty));
                        }
                        const char* vp_name = tc->source_corresp.name
                                                  ? tc->source_corresp.name
                                                  : "V";
                        llvm::Constant* val_c = llvm::ConstantInt::get(
                            llvm::Type::getInt64Ty(*be_state->context),
                            EDG_INT_VAL_LOW64(tc->variant.integer_value));
                        tpl_params.push_back(dbg_state->builder->createTemplateValueParameter(
                            dbg_state->compile_unit, vp_name, tc_di_ty, false, val_c));
                    } else if (ta->kind == tak_template) {
                        tpl_params.push_back(dbg_state->builder->createTemplateTemplateParameter(
                            dbg_state->compile_unit, "TT", nullptr, "TemplateArg", false));
                    } else if (ta->kind == tak_start_of_pack_expansion) {
                        tpl_params.push_back(dbg_state->builder->createTemplateParameterPack(
                            dbg_state->compile_unit, "Pack", nullptr, llvm::DINodeArray()));
                    }
                }
                tpl_params_array = dbg_state->builder->getOrCreateArray(tpl_params);
            }
            
            if (ty->kind == tk_union) {
                di_ty = dbg_state->builder->createUnionType(
                    dbg_state->compile_unit,
                    ty->source_corresp.name ? ty->source_corresp.name : "",
                    dbg_state->compile_unit->getFile(),
                    0,
                    static_cast<uint64_t>(ty->size) * 8,
                    static_cast<uint32_t>(ty->alignment) * 8,
                    llvm::DINode::FlagZero,
                    elements_array
                );
            } else {
                di_ty = dbg_state->builder->createClassType(
                    dbg_state->compile_unit,
                    ty->source_corresp.name ? ty->source_corresp.name : "",
                    dbg_state->compile_unit->getFile(),
                    0,
                    static_cast<uint64_t>(ty->size) * 8,
                    static_cast<uint32_t>(ty->alignment) * 8,
                    0,
                    llvm::DINode::FlagZero,
                    nullptr,
                    elements_array,
#if LLVM_VERSION_MAJOR >= 18
                    0,
#endif
                    nullptr,
                    tpl_params_array.get()
                );
            }
            
            dbg_state->builder->replaceTemporary(llvm::TempDINode(fwd_decl), di_ty);
        } break;
        default: {
            di_ty = dbg_state->builder->createBasicType("unknown", static_cast<uint64_t>(ty->size) * 8, llvm::dwarf::DW_ATE_unsigned);
        } break;
        }

        dbg_state->di_types_map[ty] = di_ty;
    }
    if ((quals & TQ_CONST) != 0) {
        di_ty = dbg_state->builder->createQualifiedType(llvm::dwarf::DW_TAG_const_type, di_ty);
    }
    if ((quals & TQ_VOLATILE) != 0) {
        di_ty = dbg_state->builder->createQualifiedType(llvm::dwarf::DW_TAG_volatile_type, di_ty);
    }
    if ((quals & TQ_RESTRICT) != 0) {
        di_ty = dbg_state->builder->createQualifiedType(llvm::dwarf::DW_TAG_restrict_type, di_ty);
    }
    if ((quals & TQ_C11_ATOMIC) != 0) {
        di_ty = dbg_state->builder->createQualifiedType(llvm::dwarf::DW_TAG_atomic_type, di_ty);
    }
    if (orig_ty != ty && orig_ty->source_corresp.name != nullptr) {
        di_ty = dbg_state->builder->createTypedef(
            di_ty,
            orig_ty->source_corresp.name,
            dbg_state->compile_unit->getFile(),
            0,
            dbg_state->compile_unit);
    }
    *out_di_type = di_ty;
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits an `llvm.dbg.declare` intrinsic for a local variable or parameter.
 * @details Creates a `DILocalVariable` in the current lexical scope and attaches `llvm.dbg.declare`
 *          to the variable's `AllocaInst`.
 * @param[in,out] dbg_state The debug state.
 * @param[in] var The EDG variable entry.
 * @param[in] alloca_inst The LLVM alloca instruction representing the variable's storage.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
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
        LLVM_GEN_BE_CHECK(get_or_create_di_file(dbg_state, path, &di_file));
    }

    llvm::DIScope* scope = dbg_state->compile_unit;
    if (!dbg_state->scope_stack.empty()) {
        scope = dbg_state->scope_stack.back();
    }

    if (var->is_parameter) {
        di_var = dbg_state->builder->createParameterVariable(
            scope, name, dbg_state->next_param_arg_no++, di_file, line, di_type
        );
    } else {
        di_var = dbg_state->builder->createAutoVariable(
            scope, name, di_file, line, di_type
        );
    }

    llvm::DILocation* loc = llvm::DILocation::get(
        *be_state->context, line, var->source_corresp.decl_position.column, scope);

    dbg_state->builder->insertDeclare(
        alloca_inst,
        di_var,
        dbg_state->builder->createExpression(),
        loc,
        alloca_inst->getParent()
    );

    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Emits an `llvm.dbg.value` intrinsic for an SSA variable update.
 * @details Creates a `DILocalVariable` and inserts `llvm.dbg.value` with an optional DWARF expression.
 * @param[in,out] dbg_state The debug state.
 * @param[in] var The EDG variable entry.
 * @param[in] value The LLVM value representing the variable's new SSA value.
 * @param[in] insert_before The instruction before which to insert the `dbg.value` intrinsic.
 * @param[in] insert_at_end The basic block where the `dbg.value` intrinsic should be inserted at the end.
 * @param[in] expr_ops Optional array of DWARF expression operators.
 * @param[in] num_expr_ops Number of expression operators.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
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
        LLVM_GEN_BE_CHECK(get_or_create_di_file(dbg_state, path, &di_file));
    }

    llvm::DIScope* scope = dbg_state->compile_unit;
    if (!dbg_state->scope_stack.empty()) {
        scope = dbg_state->scope_stack.back();
    }

    if (var->is_parameter) {
        di_var = dbg_state->builder->createParameterVariable(
            scope, name, dbg_state->next_param_arg_no++, di_file, line, di_type
        );
    } else {
        di_var = dbg_state->builder->createAutoVariable(
            scope, name, di_file, line, di_type
        );
    }

    llvm::DILocation* loc = llvm::DILocation::get(
        *be_state->context, line, var->source_corresp.decl_position.column, scope);
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
#if LLVM_VERSION_MAJOR >= 19
            insert_before->getIterator()
#else
            insert_before
#endif
        );
    } else {
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

/**
 * @brief Emits an `llvm.dbg.assign` intrinsic for optimized value tracking (assignment tracking).
 * @details Attaches a `DIAssignID` metadata node to the destination instruction and inserts `llvm.dbg.assign`.
 * @param[in,out] dbg_state The debug state.
 * @param[in] var The EDG variable entry.
 * @param[in] value The LLVM value being assigned.
 * @param[in] dest The destination address or instruction receiving the assignment.
 * @param[in] insert_before The instruction before which to insert the `dbg.assign` intrinsic.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
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
        LLVM_GEN_BE_CHECK(get_or_create_di_file(dbg_state, path, &di_file));
    }

    llvm::DIScope* scope = dbg_state->compile_unit;
    if (!dbg_state->scope_stack.empty()) {
        scope = dbg_state->scope_stack.back();
    }

    di_var = dbg_state->builder->createAutoVariable(
        scope, name, di_file, line, di_type
    );

    llvm::DILocation* loc = llvm::DILocation::get(
        *be_state->context, line, var->source_corresp.decl_position.column, scope);
    
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

/**
 * @brief Emits a `DIGlobalVariableExpression` and attaches `!dbg` metadata to a global variable.
 * @details Creates a `DIGlobalVariableExpression` for @p var in @p dbg_state and attaches it
 *          to @p gv via `llvm::GlobalVariable::addDebugInfo`.
 * @param[in,out] dbg_state The active debug state.
 * @param[in] var The EDG variable descriptor.
 * @param[in,out] gv The LLVM `GlobalVariable` to annotate with debug metadata.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t emit_dbg_global_variable(
    llvm_gen_be_debug_state_t* dbg_state,
    a_variable_ptr var,
    llvm::GlobalVariable* gv) noexcept {
    if (!dbg_state || !var || !gv) return llvm_gen_be_error_t::invalid_argument;

    llvm::DIType* di_type = nullptr;
    llvm_gen_be_error_t err = get_or_create_di_type(dbg_state, var->type, &di_type);
    if (err != llvm_gen_be_error_t::ok) return err;

    std::string name = var->source_corresp.name ? var->source_corresp.name : "";
    llvm::DIFile* di_file = dbg_state->compile_unit->getFile();
    a_line_number line = 0;
    if (var->source_corresp.decl_position.seq != 0) {
        a_const_char* file_name = nullptr;
        a_const_char* full_name = nullptr;
        a_boolean at_end_of_source = FALSE;
        conv_seq_to_file_and_line(var->source_corresp.decl_position.seq, &file_name, &full_name, &line, &at_end_of_source);
        const char* path = full_name ? full_name : (file_name ? file_name : "");
        if (path[0] != '\0') {
            LLVM_GEN_BE_CHECK(get_or_create_di_file(dbg_state, path, &di_file));
        }
    }

    bool is_local_to_unit = (var->storage_class == sc_static || var->source_corresp.name_linkage == nlk_internal);
    llvm::DIGlobalVariableExpression* gve = dbg_state->builder->createGlobalVariableExpression(
        dbg_state->compile_unit, name, gv->getName(), di_file, line, di_type, is_local_to_unit);
    gv->addDebugInfo(gve);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Creates DWARF metadata for class inheritance.
 * @details Wraps `DIBuilder::createInheritance` to describe base class offsets and flags.
 * @param[in,out] dbg_state The debug state.
 * @param[in] derived_type The derived class type.
 * @param[in] base_type The base class type.
 * @param[in] base_offset Offset of the base class in bits.
 * @param[in] vb_ptr_offset Offset of the virtual base pointer.
 * @param[in] flags DWARF flags.
 * @param[out] out_inheritance Pointer to the resulting `DIDerivedType`.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
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
    
    *out_inheritance = dbg_state->builder->createInheritance(
        derived_type, base_type, base_offset, vb_ptr_offset, flags);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Creates DWARF metadata for virtual and non-virtual member functions.
 * @details Wraps `DIBuilder::createMethod` to attach a method subprogram to a class scope.
 * @param[in,out] dbg_state The debug state.
 * @param[in] scope The DIScope representing the class.
 * @param[in] name The method name.
 * @param[in] linkage_name The mangled name.
 * @param[in] file The file where the method is declared.
 * @param[in] line_no The line number.
 * @param[in] type The subroutine type.
 * @param[in] vindex Virtual function index.
 * @param[in] this_adjustment The 'this' pointer adjustment.
 * @param[in] vtable_holder The type that holds the vtable.
 * @param[in] flags DWARF flags.
 * @param[in] sp_flags Subprogram flags.
 * @param[out] out_method Pointer to the resulting `DISubprogram`.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
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
    
    *out_method = dbg_state->builder->createMethod(
        scope, name, linkage_name ? linkage_name : "", file, line_no, type,
        vindex, this_adjustment, vtable_holder, flags, sp_flags);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Creates DWARF metadata for C++ namespaces.
 * @details Wraps `DIBuilder::createNameSpace`.
 * @param[in,out] dbg_state The debug state.
 * @param[in] scope The parent scope.
 * @param[in] name The namespace name.
 * @param[in] export_symbols True if symbols are exported (inline namespace).
 * @param[out] out_namespace Pointer to the resulting `DINamespace`.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t createNameSpace(
    llvm_gen_be_debug_state_t* dbg_state,
    llvm::DIScope* scope,
    const char* name,
    bool export_symbols,
    llvm::DINamespace** out_namespace) noexcept {
    
    if (!dbg_state || !scope || !name || !out_namespace) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
    
    *out_namespace = dbg_state->builder->createNameSpace(scope, name, export_symbols);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Creates DWARF metadata for template type parameters.
 * @details Wraps `DIBuilder::createTemplateTypeParameter`.
 * @param[in,out] dbg_state The debug state.
 * @param[in] scope The context scope.
 * @param[in] name The parameter name.
 * @param[in] type The parameter type.
 * @param[in] is_default True if default parameter.
 * @param[out] out_param Pointer to the resulting `DITemplateTypeParameter`.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
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
    
    *out_param = dbg_state->builder->createTemplateTypeParameter(
        scope, name, type, is_default);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Creates DWARF metadata for template value parameters.
 * @details Wraps `DIBuilder::createTemplateValueParameter`.
 * @param[in,out] dbg_state The debug state.
 * @param[in] scope The context scope.
 * @param[in] name The parameter name.
 * @param[in] type The parameter type.
 * @param[in] is_default True if default parameter.
 * @param[in] value The value metadata.
 * @param[out] out_param Pointer to the resulting `DITemplateValueParameter`.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
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
    
    *out_param = dbg_state->builder->createTemplateValueParameter(
        scope, name, type, is_default, llvm::cast<llvm::ConstantAsMetadata>(value)->getValue());
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Creates DWARF metadata for enumeration types.
 * @details Wraps `DIBuilder::createEnumerationType`.
 * @param[in,out] dbg_state The debug state.
 * @param[in] scope The parent scope.
 * @param[in] name The enum name.
 * @param[in] file The declaring file.
 * @param[in] line_no The line number.
 * @param[in] size_in_bits The size in bits.
 * @param[in] align_in_bits The alignment in bits.
 * @param[in] elements The enumerators.
 * @param[in] underlying_type The underlying integral type.
 * @param[in] unique_identifier The unique identifier.
 * @param[in] is_scoped True if enum class.
 * @param[out] out_enum Pointer to the resulting `DICompositeType`.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
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
    
    *out_enum = dbg_state->builder->createEnumerationType(
        scope, name, file, line_no, size_in_bits, align_in_bits, elements,
        underlying_type,
#if LLVM_VERSION_MAJOR >= 18
        0 /* RunTimeLang */,
#endif
        unique_identifier ? unique_identifier : "", is_scoped);
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Creates DWARF `DW_TAG_imported_declaration` metadata for a C++ `using` declaration.
 * @details Wraps `DIBuilder::createImportedDeclaration` to emit debug info for imported symbols.
 * @param[in,out] dbg_state The debug state.
 * @param[in] scope The lexical scope containing the `using` declaration.
 * @param[in] decl The imported declaration node (`DINode*`).
 * @param[in] file The source file containing the `using` declaration.
 * @param[in] line_no The source line number.
 * @param[in] name Optional alias name for the imported entity.
 * @param[out] out_import Receives the resulting `DIImportedEntity*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if required pointers are null.
 */
llvm_gen_be_error_t createImportedDeclaration(
    llvm_gen_be_debug_state_t* dbg_state,
    llvm::DIScope* scope,
    llvm::DINode* decl,
    llvm::DIFile* file,
    unsigned line_no,
    const char* name,
    llvm::DIImportedEntity** out_import) noexcept {
    if (!dbg_state || !scope || !decl || !out_import) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    *out_import = dbg_state->builder->createImportedDeclaration(
        scope, decl, file, line_no, name ? name : "");
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Creates DWARF `DW_TAG_imported_module` metadata for a C++ `using namespace` directive.
 * @details Wraps `DIBuilder::createImportedModule` to emit debug info for imported namespaces.
 * @param[in,out] dbg_state The debug state.
 * @param[in] scope The lexical scope containing the `using namespace` directive.
 * @param[in] ns The imported `DINamespace*` metadata node.
 * @param[in] file The source file containing the directive.
 * @param[in] line_no The source line number.
 * @param[out] out_import Receives the resulting `DIImportedEntity*`.
 * @return llvm_gen_be_error_t::ok on success, or `invalid_argument` if required pointers are null.
 */
llvm_gen_be_error_t createImportedModule(
    llvm_gen_be_debug_state_t* dbg_state,
    llvm::DIScope* scope,
    llvm::DINamespace* ns,
    llvm::DIFile* file,
    unsigned line_no,
    llvm::DIImportedEntity** out_import) noexcept {
    if (!dbg_state || !scope || !ns || !out_import) {
        return llvm_gen_be_error_t::invalid_argument;
    }
    LLVM_GEN_BE_CHECK(llvm_gen_be_step_oom_budget());
#if LLVM_VERSION_MAJOR >= 21
    *out_import = dbg_state->builder->createImportedModule(scope, ns, file, line_no);
#else
    *out_import = dbg_state->builder->createImportedModule(scope, ns, file, line_no, llvm::DINodeArray());
#endif
    return llvm_gen_be_error_t::ok;
}

/**
 * @brief Generates DWARF 5 variable fragments and CodeView PDB type records for non-contiguous locations.
 * @details Translates complex struct fields scattered across registers and stack memory into DW_OP_LLVM_fragment
 * composite location descriptions.
 * @param[in] type Pointer to the EDG type.
 * @param[in] offset Target fragment offset in bits.
 * @param[in] size Target fragment size in bits.
 * @param[out] out_expr Pointer to store the resulting DIExpression.
 * @return llvm_gen_be_error_t::ok on success, or an appropriate error code.
 */
llvm_gen_be_error_t build_dwarf_variable_fragment(
    a_type_ptr type,
    uint64_t offset,
    uint64_t size,
    llvm::DIExpression** out_expr) noexcept {
  if (type == nullptr || out_expr == nullptr) {
    return llvm_gen_be_error_t::invalid_argument;
  }
  
  if (!be_state || !be_state->dbg_state) {
    return llvm_gen_be_error_t::di_metadata_failure;
  }

  *out_expr = be_state->dbg_state->builder->createExpression({llvm::dwarf::DW_OP_LLVM_fragment, offset, size});
  return llvm_gen_be_error_t::ok;
}
END_EDG_NAMESPACE

#endif // BACK_END_IS_LLVM_GEN_BE
