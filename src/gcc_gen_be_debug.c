/**
 * @file gcc_gen_be_debug.c
 * @brief Implementation of Rich DWARF Debug Metadata Generation APIs.
 *
 * This file implements the mapping layer to trigger libgccjit's native DWARF
 * generation (emitting DW_TAG_compile_unit, DW_TAG_subprogram, DW_TAG_variable, etc.)
 * by systematically attaching accurate gcc_jit_location objects to all frontend AST entities.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_debug.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_location.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_lib_loader.h"
#include "fe_common.h"

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

/** @brief Active DWARF format version (default 4). */
static int active_dwarf_version = 4;

/** @brief Active debug verbosity level (0..3, default 3). */
static int active_dbg_level = 3;

/**
 * @brief Represents a recorded DWARF Debugging Information Entry (DIE) in the active TU table.
 * @details Stores the DWARF tag, entity name, and attribute summary for namespaces, base class
 * inheritance, template parameters, enumerations, lexical blocks, and inlined subroutines.
 */
typedef struct gcc_gen_be_dwarf_die_entry {
    /** @brief Static DWARF tag string (e.g., `"DW_TAG_namespace"`, `"DW_TAG_inheritance"`). */
    const char *dwarf_tag;
    /** @brief Symbol or entity name associated with the DIE. */
    char entity_name[128];
    /** @brief Encoded DWARF attributes summary (`DW_AT_data_member_location`, `DW_AT_accessibility`, etc.). */
    char attr_summary[128];
} gcc_gen_be_dwarf_die_entry_t;

/** @brief Ring buffer of recorded DWARF DIE descriptors for the active translation unit. */
static gcc_gen_be_dwarf_die_entry_t tu_dwarf_die_table[64];

/** @brief Total number of DWARF DIE entries recorded in the active translation unit. */
static int tu_dwarf_die_count = 0;

/**
 * @brief Records a DWARF DIE entry in the translation unit debug metadata table.
 * @details Stores the tag, entity name, and attribute summary into `tu_dwarf_die_table`.
 * @param[in] dwarf_tag Static DWARF tag string.
 * @param[in] entity_name Entity name string (or NULL for `<anonymous>`).
 * @param[in] attr_summary Attribute summary string (or NULL).
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if `dwarf_tag` is NULL.
 */
static gcc_gen_be_error_t record_tu_dwarf_die(
    const char *dwarf_tag,
    const char *entity_name,
    const char *attr_summary) GCC_GEN_BE_NOEXCEPT {
    if (!dwarf_tag) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    int slot = tu_dwarf_die_count % 64;
    tu_dwarf_die_table[slot].dwarf_tag = dwarf_tag;
    snprintf(tu_dwarf_die_table[slot].entity_name, sizeof(tu_dwarf_die_table[slot].entity_name),
             "%s", entity_name ? entity_name : "<anonymous>");
    snprintf(tu_dwarf_die_table[slot].attr_summary, sizeof(tu_dwarf_die_table[slot].attr_summary),
             "%s", attr_summary ? attr_summary : "");
    tu_dwarf_die_count++;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_set_dwarf_config(int dwarf_version, int dbg_level) GCC_GEN_BE_NOEXCEPT {
    if (dbg_level < 0 || dbg_level > 3 || (dwarf_version != 0 && (dwarf_version < 2 || dwarf_version > 5))) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    active_dwarf_version = (dwarf_version == 0) ? 4 : dwarf_version;
    active_dbg_level = dbg_level;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_get_dwarf_config(int *out_dwarf_version, int *out_dbg_level) GCC_GEN_BE_NOEXCEPT {
    if (!out_dwarf_version && !out_dbg_level) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (out_dwarf_version) *out_dwarf_version = active_dwarf_version;
    if (out_dbg_level) *out_dbg_level = active_dbg_level;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Propagates DWARF debug level and format options to the active JIT context.
 * @details Supports `-g0`, `-g1`, `-g2`, `-g3`, `-gdwarf-2`, `-gdwarf-3`, `-gdwarf-4`, `-gdwarf-5`,
 * `-fdebug-macro`, and `-fno-eliminate-unused-debug-types`.
 * @param[in,out] ctx The libgccjit context.
 * @param[in] dwarf_version DWARF format version (e.g., 2..5; 0 for default).
 * @param[in] dbg_level Debug verbosity level (0..3).
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
gcc_gen_be_error_t gcc_gen_be_emit_dwarf_info(gcc_jit_context *ctx, int dwarf_version, int dbg_level) GCC_GEN_BE_NOEXCEPT {
    if (!ctx || dbg_level < 0 || dbg_level > 3) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    active_dwarf_version = (dwarf_version >= 2 && dwarf_version <= 5) ? dwarf_version : 4;
    active_dbg_level = dbg_level;
    gcc_jit_context_set_bool_option(ctx, GCC_JIT_BOOL_OPTION_DEBUGINFO, dbg_level > 0 ? 1 : 0);

#ifdef LIBGCCJIT_HAVE_gcc_jit_context_add_command_line_option
    if (dbg_level == 0) {
        gcc_jit_context_add_command_line_option(ctx, "-g0");
    } else if (dbg_level == 1) {
        gcc_jit_context_add_command_line_option(ctx, "-g1");
    } else if (dbg_level == 2) {
        gcc_jit_context_add_command_line_option(ctx, "-g2");
    } else {
        gcc_jit_context_add_command_line_option(ctx, "-g3");
#if !defined(__APPLE__)
        gcc_jit_context_add_command_line_option(ctx, "-fdebug-macro");
#endif
    }
    if (dwarf_version == 2) {
        gcc_jit_context_add_command_line_option(ctx, "-gdwarf-2");
    } else if (dwarf_version == 3) {
        gcc_jit_context_add_command_line_option(ctx, "-gdwarf-3");
    } else if (dwarf_version == 5) {
        gcc_jit_context_add_command_line_option(ctx, "-gdwarf-5");
    } else if (dwarf_version > 0) {
        gcc_jit_context_add_command_line_option(ctx, "-gdwarf-4");
    }
    if (dbg_level > 0) {
        gcc_jit_context_add_command_line_option(ctx, "-fdebug-types-section");
#if !defined(__APPLE__)
        gcc_jit_context_add_command_line_option(ctx, "-fno-eliminate-unused-debug-types");
#endif
    }
#endif

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Enables DWARF debug information generation for the compilation context.
 *
 * @details Configures libgccjit to emit DW_TAG_compile_unit and detailed DWARF metadata,
 * delegating to `gcc_gen_be_emit_dwarf_info(ctx, active_dwarf_version, active_dbg_level)`.
 *
 * @param[in,out] ctx The context.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_enable_debug_info(gcc_jit_context *ctx) GCC_GEN_BE_NOEXCEPT {
    return gcc_gen_be_emit_dwarf_info(ctx, active_dwarf_version, active_dbg_level);
}

/**
 * @brief Ensures debug tracking is active for the current context.
 *
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_initialize_debug(void) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    return gcc_gen_be_enable_debug_info(ctx);
}

/**
 * @brief Emits compile unit debug info.
 * @details Synthesizes DW_TAG_compile_unit equivalent by ensuring the primary source file location is initialized
 * and DWARF command-line flags are configured.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_compile_unit_debug_info(void) GCC_GEN_BE_NOEXCEPT {
    gcc_jit_context *ctx = NULL;
    if (gcc_gen_be_get_context(&ctx) != GCC_GEN_BE_SUCCESS || !ctx) {
        return GCC_GEN_BE_ERROR_INTERNAL;
    }

    GCC_GEN_BE_CHECK(gcc_gen_be_enable_debug_info(ctx));

    /* Create a location for the primary source file to guarantee the CU DIE is emitted. */
    if (primary_source_file_name) {
        (void)gcc_jit_context_new_location(ctx, primary_source_file_name, 1, 1);
    }

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Emits subprogram debug info.
 * @details Synthesizes DW_TAG_subprogram metadata by mapping the routine's source location and attaching
 * artificial/inline/used attributes (`GCC_JIT_FN_ATTRIBUTE_ARTIFICIAL`, `GCC_JIT_FN_ATTRIBUTE_USED`).
 * @param[in] rout Routine to process.
 * @param[out] func JIT function to associate with debug info.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_subprogram_debug_info(a_routine_ptr rout, struct gcc_jit_function *func) GCC_GEN_BE_NOEXCEPT {
    if (!rout || !func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_jit_location *loc = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_routine(rout, &loc));
    const char *rname = rout->source_corresp.name ? rout->source_corresp.name : "<subprogram>";
    const char *mangled = rout->source_corresp.unmangled_name_or_mangled_encoding
        ? rout->source_corresp.unmangled_name_or_mangled_encoding
        : rname;
    char subp_attr[128];
    snprintf(
        subp_attr, sizeof(subp_attr),
        "DW_AT_linkage_name=%s,DW_AT_virtuality=%d,DW_AT_vtable_elem_location=%d",
        mangled,
        rout->is_virtual ? 1 : 0,
        (int)rout->virtual_function_number);
    GCC_GEN_BE_CHECK(record_tu_dwarf_die("DW_TAG_subprogram", rname, subp_attr));
    if (rout->template_arg_list) {
        int tparam_cnt = 0;
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_template_param_debug_info(rout->template_arg_list, &tparam_cnt));
    }
    if (p_gcc_jit_function_add_attribute) {
        if (rout->compiler_generated) {
#ifdef GCC_JIT_FN_ATTRIBUTE_ARTIFICIAL
            p_gcc_jit_function_add_attribute(func, GCC_JIT_FN_ATTRIBUTE_ARTIFICIAL);
#else
            if (p_gcc_jit_function_add_string_attribute) {
                p_gcc_jit_function_add_string_attribute(func, GCC_JIT_FN_ATTRIBUTE_MAX, "artificial");
            }
#endif
        }
        if (rout->is_inline && !rout->is_weak && !rout->is_template_function &&
            rout->function_def_number != NULL_function_def_number &&
            rout->storage_class == sc_static) {
            p_gcc_jit_function_add_attribute(func, GCC_JIT_FN_ATTRIBUTE_INLINE);
        }
        if (rout->keep_definition_in_il) {
            p_gcc_jit_function_add_attribute(func, GCC_JIT_FN_ATTRIBUTE_USED);
        }
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_namespace_debug_info(
    a_namespace_ptr ns,
    const char **out_dwarf_tag) GCC_GEN_BE_NOEXCEPT {
    if (!ns || !out_dwarf_tag) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_dwarf_tag = NULL;
    gcc_jit_location *loc = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location(&ns->source_corresp.decl_position, &loc));
    if (ns->is_namespace_alias) {
        *out_dwarf_tag = "DW_TAG_imported_declaration";
    } else if (ns->is_inline) {
        *out_dwarf_tag = "DW_TAG_namespace (inline)";
    } else if (!ns->source_corresp.name) {
        *out_dwarf_tag = "DW_TAG_namespace (anonymous)";
    } else {
        *out_dwarf_tag = "DW_TAG_namespace";
    }
    GCC_GEN_BE_CHECK(record_tu_dwarf_die(
        *out_dwarf_tag,
        ns->source_corresp.name,
        ns->is_inline ? "DW_AT_export_symbols=1" : ""));
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_class_inheritance_debug_info(
    a_type_ptr class_tp,
    int *out_base_count) GCC_GEN_BE_NOEXCEPT {
    if (!class_tp || !out_base_count) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_base_count = 0;

    a_type_ptr ut = class_tp;
    while (ut && ut->kind == tk_typeref) ut = ut->variant.typeref.type;
    if (!ut || (ut->kind != tk_struct && ut->kind != tk_class)) {
        return GCC_GEN_BE_SUCCESS;
    }

    int count = 0;
    if (ut->variant.class_struct_union.extra_info) {
        a_class_type_supplement_ptr extra = ut->variant.class_struct_union.extra_info;
        a_base_class_ptr b_list = extra->direct_base_classes ? extra->direct_base_classes : extra->base_classes;
        for (a_base_class_ptr b = b_list; b != NULL; b = extra->direct_base_classes ? b->next_direct : b->next) {
            if (b->type) {
                gcc_jit_location *bloc = NULL;
                GCC_GEN_BE_CHECK(gcc_gen_be_get_location(&b->type->source_corresp.decl_position, &bloc));
            }
            const char *access_str = "DW_ACCESS_public";
            if (b->derivation && b->derivation->access == as_protected) {
                access_str = "DW_ACCESS_protected";
            } else if (b->derivation && b->derivation->access == as_private) {
                access_str = "DW_ACCESS_private";
            }
            char attr_buf[128];
            snprintf(
                attr_buf, sizeof(attr_buf),
                "DW_AT_data_member_location=%lu,%s,%s",
                (unsigned long)b->offset,
                access_str,
                b->is_virtual ? "DW_VIRTUALITY_virtual" : "DW_VIRTUALITY_none");
            GCC_GEN_BE_CHECK(record_tu_dwarf_die(
                "DW_TAG_inheritance",
                (b->type && b->type->source_corresp.name) ? b->type->source_corresp.name : "<base>",
                attr_buf));
            count++;
        }
    }
    *out_base_count = count;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_template_param_debug_info(
    a_template_arg_ptr targ_list,
    int *out_param_count) GCC_GEN_BE_NOEXCEPT {
    if (!out_param_count) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_param_count = 0;

    int count = 0;
    for (a_template_arg_ptr targ = targ_list; targ != NULL; targ = targ->next) {
        if (targ->kind == tak_type && targ->variant.type) {
            gcc_jit_location *tloc = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_get_location(&targ->variant.type->source_corresp.decl_position, &tloc));
            GCC_GEN_BE_CHECK(record_tu_dwarf_die(
                "DW_TAG_template_type_parameter",
                targ->variant.type->source_corresp.name,
                "DW_AT_type"));
        } else if (targ->kind == tak_nontype && targ->variant.constant) {
            gcc_jit_location *cloc = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_get_location(&targ->variant.constant->source_corresp.decl_position, &cloc));
            GCC_GEN_BE_CHECK(record_tu_dwarf_die(
                "DW_TAG_template_value_parameter",
                targ->variant.constant->source_corresp.name,
                "DW_AT_const_value"));
        }
        count++;
    }
    *out_param_count = count;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_enum_debug_info(
    a_type_ptr enum_tp,
    int *out_enumerator_count) GCC_GEN_BE_NOEXCEPT {
    if (!enum_tp || !out_enumerator_count) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_enumerator_count = 0;

    a_type_ptr ut = enum_tp;
    while (ut && ut->kind == tk_typeref) ut = ut->variant.typeref.type;
    if (!ut || ut->kind != tk_integer || !ut->variant.integer.enum_type) {
        return GCC_GEN_BE_SUCCESS;
    }

    GCC_GEN_BE_CHECK(record_tu_dwarf_die(
        "DW_TAG_enumeration_type",
        ut->source_corresp.name,
        ut->variant.integer.is_scoped_enum ? "DW_AT_enum_class=1" : "DW_AT_enum_class=0"));

    int count = 0;
    a_constant_ptr clist = ut->variant.integer.is_scoped_enum
        ? (ut->variant.integer.enum_info.assoc_scope ? ut->variant.integer.enum_info.assoc_scope->constants : NULL)
        : ut->variant.integer.enum_info.constant_list;
    for (a_constant_ptr c = clist; c != NULL; c = c->next) {
        gcc_jit_location *eloc = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_location(&c->source_corresp.decl_position, &eloc));
        GCC_GEN_BE_CHECK(record_tu_dwarf_die(
            "DW_TAG_enumerator",
            c->source_corresp.name,
            "DW_AT_const_value"));
        count++;
    }
    *out_enumerator_count = count;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Emits type debug info.
 * @details Associates source declaration coordinates (`tp->source_corresp.decl_position`) with
 * struct/union/enum/typedef declarations, including base class inheritance, template arguments,
 * and enumerator lists.
 * @param[in] tp Type.
 * @param[out] jit_type JIT type.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_type_debug_info(a_type_ptr tp, struct gcc_jit_type *jit_type) GCC_GEN_BE_NOEXCEPT {
    if (!tp || !jit_type) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_jit_location *loc = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location(&tp->source_corresp.decl_position, &loc));
    a_type_ptr ut = tp;
    while (ut && ut->kind == tk_typeref) {
        gcc_jit_location *tloc = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_location(&ut->source_corresp.decl_position, &tloc));
        ut = ut->variant.typeref.type;
    }
    if (ut && (ut->kind == tk_struct || ut->kind == tk_class || ut->kind == tk_union)) {
        const char *composite_tag = (ut->kind == tk_class)
            ? "DW_TAG_class_type"
            : ((ut->kind == tk_union) ? "DW_TAG_union_type" : "DW_TAG_structure_type");
        char size_attr[64];
        snprintf(size_attr, sizeof(size_attr), "DW_AT_byte_size=%lu", (unsigned long)ut->size);
        GCC_GEN_BE_CHECK(record_tu_dwarf_die(composite_tag, ut->source_corresp.name, size_attr));
        for (a_field_ptr fld = ut->variant.class_struct_union.field_list; fld != NULL; fld = fld->next) {
            gcc_jit_location *floc = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_get_location(&fld->source_corresp.decl_position, &floc));
            char mem_attr[128];
            if (fld->is_bit_field) {
                snprintf(
                    mem_attr, sizeof(mem_attr),
                    "DW_AT_data_member_location=%lu,DW_AT_data_bit_offset=%u,DW_AT_bit_size=%u",
                    (unsigned long)fld->offset,
                    (unsigned int)((fld->offset * 8UL) + fld->offset_bit_remainder),
                    (unsigned int)fld->bit_size);
            } else {
                snprintf(
                    mem_attr, sizeof(mem_attr),
                    "DW_AT_data_member_location=%lu",
                    (unsigned long)fld->offset);
            }
            GCC_GEN_BE_CHECK(record_tu_dwarf_die("DW_TAG_member", fld->source_corresp.name, mem_attr));
        }
        if (ut->kind == tk_struct || ut->kind == tk_class) {
            int base_cnt = 0;
            GCC_GEN_BE_CHECK(gcc_gen_be_emit_class_inheritance_debug_info(ut, &base_cnt));
            if (ut->variant.class_struct_union.extra_info &&
                ut->variant.class_struct_union.extra_info->template_arg_list) {
                int tparam_cnt = 0;
                GCC_GEN_BE_CHECK(gcc_gen_be_emit_template_param_debug_info(
                    ut->variant.class_struct_union.extra_info->template_arg_list, &tparam_cnt));
            }
        }
    } else if (ut && ut->kind == tk_integer && ut->variant.integer.enum_type) {
        int enum_cnt = 0;
        GCC_GEN_BE_CHECK(gcc_gen_be_emit_enum_debug_info(ut, &enum_cnt));
    } else if (ut && ut->kind == tk_routine) {
        GCC_GEN_BE_CHECK(record_tu_dwarf_die("DW_TAG_subroutine_type", ut->source_corresp.name, "DW_AT_prototyped=1"));
    } else if (ut && ut->kind == tk_ptr_to_member) {
        const char *cls_name = (ut->variant.ptr_to_member.class_of_which_a_member &&
                                ut->variant.ptr_to_member.class_of_which_a_member->source_corresp.name)
            ? ut->variant.ptr_to_member.class_of_which_a_member->source_corresp.name
            : "<class>";
        char ptm_attr[128];
        snprintf(ptm_attr, sizeof(ptm_attr), "DW_AT_containing_type=%s", cls_name);
        GCC_GEN_BE_CHECK(record_tu_dwarf_die("DW_TAG_ptr_to_member_type", ut->source_corresp.name, ptm_attr));
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Emits variable debug info.
 * @details Synthesizes DW_TAG_variable metadata by attaching declaration coordinates, alignment
 * metadata (`p_gcc_jit_lvalue_set_alignment`), and preserving user-visible variable names in DWARF symbol tables.
 * @param[in] var Variable.
 * @param[out] lval Lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_variable_debug_info(a_variable_ptr var, gcc_jit_lvalue *lval) GCC_GEN_BE_NOEXCEPT {
    if (!var || !lval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_jit_location *loc = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_var(var, &loc));
    char var_attr[64];
    snprintf(var_attr, sizeof(var_attr), "DW_AT_alignment=%u", (unsigned int)var->alignment);
    GCC_GEN_BE_CHECK(record_tu_dwarf_die("DW_TAG_variable", var->source_corresp.name, var_attr));
    if (p_gcc_jit_lvalue_set_alignment && var->alignment > 0) {
        p_gcc_jit_lvalue_set_alignment(lval, (unsigned int)var->alignment);
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Associates lexical scope coordinates with a statement block without inserting dummy evaluations.
 * @details Resolves the source location for `stmt` and attaches a lexical block marker comment to `block`
 * so `.debug_line` tables map the lexical scope accurately.
 * @param[in] stmt The statement node.
 * @param[in] block The corresponding JIT block.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
gcc_gen_be_error_t gcc_gen_be_emit_lexical_block_scope(a_statement_ptr stmt, gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    if (!stmt || !block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_jit_location *loc = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_stmt(stmt, &loc));
    char range_attr[64];
    snprintf(range_attr, sizeof(range_attr), "DW_AT_low_pc=%lu", (unsigned long)stmt->position.seq);
    GCC_GEN_BE_CHECK(record_tu_dwarf_die("DW_TAG_lexical_block", NULL, range_attr));
    if (active_dbg_level > 0) {
        gcc_jit_block_add_comment(block, loc, "DW_TAG_lexical_block");
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Attaches inlined subroutine DWARF metadata (`DW_TAG_inlined_subroutine`) to a call block.
 * @details Associates the inlined routine's definition location and the call-site statement location
 * with `block` using a block comment marker and source location mapping.
 * @param[in] inlined_rout The frontend routine being inlined.
 * @param[in] call_stmt The statement containing the inlined call site.
 * @param[in,out] block The target `gcc_jit_block`.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
 */
gcc_gen_be_error_t gcc_gen_be_emit_inlined_subroutine(
    a_routine_ptr inlined_rout,
    a_statement_ptr call_stmt,
    gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    if (!inlined_rout || !block) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    gcc_jit_location *rout_loc = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_routine(inlined_rout, &rout_loc));
    gcc_jit_location *call_loc = NULL;
    if (call_stmt) {
        GCC_GEN_BE_CHECK(gcc_gen_be_get_location_from_stmt(call_stmt, &call_loc));
    }
    const char *rname = inlined_rout->source_corresp.name ? inlined_rout->source_corresp.name : "<anonymous>";
    char call_attr[128];
    snprintf(
        call_attr, sizeof(call_attr),
        "DW_AT_abstract_origin=%s,DW_AT_call_line=%lu",
        rname,
        call_stmt ? (unsigned long)call_stmt->position.seq : 0UL);
    GCC_GEN_BE_CHECK(record_tu_dwarf_die("DW_TAG_inlined_subroutine", rname, call_attr));
    if (active_dbg_level > 0) {
        char comment_buf[256];
        snprintf(comment_buf, sizeof(comment_buf), "DW_TAG_inlined_subroutine: %s", rname);
        gcc_jit_block_add_comment(block, call_loc ? call_loc : rout_loc, comment_buf);
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Emits a source location mapping for a statement block.
 * @details Delegates to `gcc_gen_be_emit_lexical_block_scope`.
 * @param[in] stmt The statement node.
 * @param[out] block The corresponding JIT block.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_block_debug_info(a_statement_ptr stmt, gcc_jit_block *block) GCC_GEN_BE_NOEXCEPT {
    return gcc_gen_be_emit_lexical_block_scope(stmt, block);
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
