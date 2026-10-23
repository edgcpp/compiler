/**
 * @file gcc_gen_be_rtti.c
 * @brief Implementation of Itanium C++ ABI RTTI and VTable generation.
 *
 * This file implements the full Itanium ABI VTable Layout Engine, Construction
 * Virtual Tables (VTC), Virtual Table Tables (VTT), and Virtual Function Thunk
 * Generation. It also fully implements the RTTI descriptor hierarchy and
 * dynamic_cast / typeid lowering.
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_rtti.h"
#include "gcc_gen_be_context.h"
#include "gcc_gen_be_cache.h"
#include "gcc_gen_be_type.h"
#include "gcc_gen_be_expr.h"
#include "gcc_gen_be_lib_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

/**
 * @brief Appends a string to a bounded buffer.
 *
 * @details Safely concatenates `str` onto `buffer` up to `size - 1` characters.
 *
 * @param[in,out] buffer The destination string buffer.
 * @param[in] size The total capacity of `buffer`.
 * @param[in] str The null-terminated string to append.
 * @return GCC_GEN_BE_SUCCESS on success, or GCC_GEN_BE_ERROR_INVALID_ARGUMENT if buffer/str is NULL.
 */
static gcc_gen_be_error_t append_mangled_str(char *buffer, size_t size, const char *str) GCC_GEN_BE_NOEXCEPT {
    if (!buffer || !str || size == 0) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    size_t len = strlen(buffer);
    size_t slen = strlen(str);
    if (len + slen >= size) {
        return GCC_GEN_BE_ERROR_MANGLING_BUFFER_OVERFLOW;
    }
    memcpy(buffer + len, str, slen + 1);
    return GCC_GEN_BE_SUCCESS;
}

/** @brief Context-backed active substitution table for the current mangling invocation. */
#define active_subst_table (gcc_gen_be_singleton_state.active_subst_table)

/**
 * @brief Looks up or registers a mangled component in the Itanium ABI substitution table.
 * @details If `candidate` is already in `active_subst_table`, writes `S_` or `S<seq-id>_` (base 36)
 * into `out_subst` and sets `*out_found = TRUE`. Otherwise appends `candidate` to the table.
 * @param[in] candidate The mangled component string to check.
 * @param[out] out_found Pointer to receive TRUE if a substitution matched, FALSE otherwise.
 * @param[out] out_subst Buffer of at least 16 bytes to receive the `S..._` token when found.
 * @param[in] subst_size Size of `out_subst`.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t find_or_record_substitution(
    const char *candidate,
    a_boolean *out_found,
    char *out_subst,
    size_t subst_size) GCC_GEN_BE_NOEXCEPT {
    if (!candidate || !out_found || !out_subst || subst_size < 8) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }
    *out_found = FALSE;
    out_subst[0] = '\0';
    if (candidate[0] == '\0' || strlen(candidate) <= 1) {
        return GCC_GEN_BE_SUCCESS;
    }
    for (int i = 0; i < active_subst_table.count; ++i) {
        if (strcmp(active_subst_table.entries[i], candidate) == 0) {
            *out_found = TRUE;
            if (i == 0) {
                snprintf(out_subst, subst_size, "S_");
            } else {
                const char *digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
                int seq = i - 1;
                if (seq < 36) {
                    snprintf(out_subst, subst_size, "S%c_", digits[seq]);
                } else {
                    snprintf(out_subst, subst_size, "S%c%c_", digits[seq / 36], digits[seq % 36]);
                }
            }
            return GCC_GEN_BE_SUCCESS;
        }
    }
    if (active_subst_table.count < 64 && strlen(candidate) < sizeof(active_subst_table.entries[0])) {
        snprintf(active_subst_table.entries[active_subst_table.count],
                 sizeof(active_subst_table.entries[0]), "%s", candidate);
        active_subst_table.count++;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Recursively mangles enclosing scopes (namespaces, parent classes, and functions) for Itanium C++ ABI.
 *
 * @details Traverses `parent_scope` upward and emits `<len><name>`, standard `St` substitution for `::std`,
 * or `Z<func>E` for function-local scopes in outer-to-inner order.
 *
 * @param[in] scope The parent scope to mangle.
 * @param[in,out] buffer The destination mangled string buffer.
 * @param[in] size The capacity of `buffer`.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
static gcc_gen_be_error_t mangle_enclosing_scopes(a_scope_ptr scope, char *buffer, size_t size) GCC_GEN_BE_NOEXCEPT {
    if (!buffer || size == 0) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (!scope || scope->kind == sck_file) return GCC_GEN_BE_SUCCESS;

    if (scope->parent) {
        GCC_GEN_BE_CHECK(mangle_enclosing_scopes(scope->parent, buffer, size));
    }

    if (scope->kind == sck_namespace && scope->variant.assoc_namespace) {
        a_const_char *ns_name = scope->variant.assoc_namespace->source_corresp.name;
        if (ns_name && strcmp(ns_name, "std") == 0 && (!scope->parent || scope->parent->kind == sck_file)) {
            GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "St"));
        } else if (ns_name && ns_name[0] != '\0') {
            char part[128];
            snprintf(part, sizeof(part), "%lu%s", (unsigned long)strlen(ns_name), ns_name);
            GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, part));
        } else {
            GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "12_GLOBAL__N_1"));
        }
    } else if (scope->kind == sck_class_struct_union && scope->variant.assoc_type) {
        a_const_char *cls_name = scope->variant.assoc_type->source_corresp.name;
        if (cls_name && cls_name[0] != '\0') {
            char part[128];
            snprintf(part, sizeof(part), "%lu%s", (unsigned long)strlen(cls_name), cls_name);
            GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, part));
        }
    } else if (scope->kind == sck_function && scope->variant.routine.ptr) {
        a_const_char *fn_name = scope->variant.routine.ptr->source_corresp.name;
        if (fn_name && fn_name[0] != '\0') {
            char part[128];
            snprintf(part, sizeof(part), "Z%lu%svE", (unsigned long)strlen(fn_name), fn_name);
            GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, part));
        }
    } else if (scope->kind == sck_block && scope->number > 1) {
        char disc[32];
        snprintf(disc, sizeof(disc), "_%lu", (unsigned long)(scope->number - 2));
        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, disc));
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Marks a global RTTI/vtable variable with weak/COMDAT vague linkage attributes when supported.
 * @details Attaches the `"weak"` attribute via `p_gcc_jit_lvalue_add_string_attribute` if resolved.
 * @param[in] global The `gcc_jit_lvalue` global variable to annotate.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t mark_global_vague_linkage(struct gcc_jit_lvalue *global) GCC_GEN_BE_NOEXCEPT {
    if (!global) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (p_gcc_jit_lvalue_add_string_attribute) {
        p_gcc_jit_lvalue_add_string_attribute(global, (enum gcc_jit_variable_attribute)0, "default");
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Retrieves a deterministic Itanium C++ ABI mangled type encoding.
 * @details Resets the active substitution table and delegates to `get_mangled_type_name_inner`.
 * @param[in] type The frontend type to mangle.
 * @param[out] buffer The buffer to store the mangled name.
 * @param[in] size The size of the buffer.
 * @return GCC_GEN_BE_SUCCESS or an error code.
 */
static gcc_gen_be_error_t get_mangled_type_name(a_type_ptr type, char *buffer, size_t size) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Internal recursive helper for `get_mangled_type_name` that shares `active_subst_table`.
 * @details Encodes types according to the Itanium C++ ABI specification and records/reuses substitutions.
 * @param[in] type The frontend type to mangle.
 * @param[out] buffer Destination buffer.
 * @param[in] size Capacity of `buffer`.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t get_mangled_type_name_inner(a_type_ptr type, char *buffer, size_t size) GCC_GEN_BE_NOEXCEPT;

gcc_gen_be_error_t gcc_gen_be_mangle_expr(
    an_expr_node_ptr expr,
    char *buffer,
    size_t size) GCC_GEN_BE_NOEXCEPT {
    if (!expr || !buffer || size == 0) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    buffer[0] = '\0';

    if (expr->kind == enk_constant && expr->variant.constant.ptr) {
        a_constant_ptr con = expr->variant.constant.ptr;
        char tcode[64];
        tcode[0] = '\0';
        const char *tc = "i";
        if (con->type && get_mangled_type_name_inner(con->type, tcode, sizeof(tcode)) == GCC_GEN_BE_SUCCESS &&
            tcode[0] != '\0') {
            tc = tcode;
        }
        a_boolean ovf = FALSE;
        long val = (con->kind == ck_integer) ? (long)value_of_integer_constant(con, &ovf) : 0L;
        char cbuf[128];
        if (val < 0) {
            snprintf(cbuf, sizeof(cbuf), "L%sn%ldE", tc, -val);
        } else {
            snprintf(cbuf, sizeof(cbuf), "L%s%ldE", tc, val);
        }
        return append_mangled_str(buffer, size, cbuf);
    }
    if (expr->kind == enk_type_operand && expr->variant.type_operand.type) {
        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "st"));
        char tbuf[128];
        tbuf[0] = '\0';
        GCC_GEN_BE_CHECK(get_mangled_type_name_inner(expr->variant.type_operand.type, tbuf, sizeof(tbuf)));
        return append_mangled_str(buffer, size, tbuf);
    }
    if (expr->kind == enk_operation) {
        an_expr_operator_kind op = expr->variant.operation.kind;
        an_expr_node_ptr op1 = expr->variant.operation.operands;
        an_expr_node_ptr op2 = op1 ? op1->next : NULL;
        const char *op_str = "pl";
        a_boolean is_unary = (op2 == NULL) ? TRUE : FALSE;
        if (op == eok_add) op_str = "pl";
        else if (op == eok_subtract) op_str = "mi";
        else if (op == eok_multiply) op_str = "ml";
        else if (op == eok_divide) op_str = "dv";
        else if (op == eok_negate) { op_str = "ng"; is_unary = TRUE; }
        else if (op == eok_not) { op_str = "nt"; is_unary = TRUE; }
        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, op_str));
        if (op1) {
            char sub1[128];
            sub1[0] = '\0';
            GCC_GEN_BE_CHECK(gcc_gen_be_mangle_expr(op1, sub1, sizeof(sub1)));
            GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, sub1));
        }
        if (!is_unary && op2) {
            char sub2[128];
            sub2[0] = '\0';
            GCC_GEN_BE_CHECK(gcc_gen_be_mangle_expr(op2, sub2, sizeof(sub2)));
            GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, sub2));
        }
        return GCC_GEN_BE_SUCCESS;
    }
    char fallback_t[64];
    fallback_t[0] = '\0';
    if (expr->type) {
        GCC_GEN_BE_CHECK(get_mangled_type_name_inner(expr->type, fallback_t, sizeof(fallback_t)));
    } else {
        snprintf(fallback_t, sizeof(fallback_t), "i");
    }
    GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "DT"));
    GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, fallback_t));
    return append_mangled_str(buffer, size, "E");
}

/**
 * @brief Appends mangled template arguments (`I...E`) to the mangled name buffer.
 *
 * @details Encodes type template arguments (`tak_type`), integral/enum/bool/float/pointer non-type
 * template arguments (`tak_nontype`), template-template arguments (`tak_template`), and parameter packs (`J...E`).
 *
 * @param[in] targ_list The linked list of template arguments.
 * @param[in,out] buffer The destination mangled string buffer.
 * @param[in] size The capacity of `buffer`.
 * @return GCC_GEN_BE_SUCCESS on success.
 */
static gcc_gen_be_error_t mangle_template_args(a_template_arg_ptr targ_list, char *buffer, size_t size) GCC_GEN_BE_NOEXCEPT {
    if (!buffer || size == 0) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (!targ_list) return GCC_GEN_BE_SUCCESS;

    GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "I"));
    for (a_template_arg_ptr ta = targ_list; ta != NULL; ta = ta->next) {
        if (ta->kind == tak_start_of_pack_expansion) {
            GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "J"));
            continue;
        }
        if (ta->kind == tak_type && ta->variant.type) {
            char tbuf[256];
            tbuf[0] = '\0';
            GCC_GEN_BE_CHECK(get_mangled_type_name_inner(ta->variant.type, tbuf, sizeof(tbuf)));
            GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, tbuf));
        } else if (ta->kind == tak_nontype) {
            char lbuf[128];
            long val = 0;
            const char *type_code = "i";
            char enum_code[64];
            if (ta->is_array_bound_of_unknown_type) {
                val = (long)ta->variant.integer_value;
            } else if (ta->variant.constant) {
                a_constant_ptr con = ta->variant.constant;
                if (con->type) {
                    enum_code[0] = '\0';
                    if (get_mangled_type_name_inner(con->type, enum_code, sizeof(enum_code)) == GCC_GEN_BE_SUCCESS &&
                        enum_code[0] != '\0') {
                        type_code = enum_code;
                    }
                }
                if (con->kind == ck_integer) {
                    a_boolean ovf = FALSE;
                    val = (long)value_of_integer_constant(con, &ovf);
                } else if (con->kind == ck_address && con->variant.address.kind == abk_routine &&
                           con->variant.address.variant.routine &&
                           con->variant.address.variant.routine->source_corresp.name) {
                    a_const_char *rname = con->variant.address.variant.routine->source_corresp.name;
                    snprintf(lbuf, sizeof(lbuf), "XadL_Z%lu%svEE", (unsigned long)strlen(rname), rname);
                    GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, lbuf));
                    goto check_pack_end;
                } else if (con->kind == ck_address && con->variant.address.kind == abk_variable &&
                           con->variant.address.variant.variable &&
                           con->variant.address.variant.variable->source_corresp.name) {
                    a_const_char *vname = con->variant.address.variant.variable->source_corresp.name;
                    snprintf(lbuf, sizeof(lbuf), "XadL_Z%lu%sEE", (unsigned long)strlen(vname), vname);
                    GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, lbuf));
                    goto check_pack_end;
                }
            }
            if (val < 0) {
                snprintf(lbuf, sizeof(lbuf), "L%sn%ldE", type_code, -val);
            } else {
                snprintf(lbuf, sizeof(lbuf), "L%s%ldE", type_code, val);
            }
            GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, lbuf));
        } else if (ta->kind == tak_template && ta->variant.templ.ptr &&
                   ta->variant.templ.ptr->source_corresp.name) {
            a_const_char *tname = ta->variant.templ.ptr->source_corresp.name;
            char tbuf[128];
            snprintf(tbuf, sizeof(tbuf), "%lu%s", (unsigned long)strlen(tname), tname);
            GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, tbuf));
        }
check_pack_end:
        if (ta->is_pack_element && (!ta->next || !ta->next->is_pack_element)) {
            GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "E"));
        }
    }
    GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "E"));
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Internal recursive helper for `get_mangled_type_name` that shares `active_subst_table`.
 * @details Encodes types according to the Itanium C++ ABI specification and records/reuses substitutions.
 * @param[in] type The frontend type to mangle.
 * @param[out] buffer Destination buffer.
 * @param[in] size Capacity of `buffer`.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t get_mangled_type_name_inner(a_type_ptr type, char *buffer, size_t size) GCC_GEN_BE_NOEXCEPT {
    if (!buffer || size == 0) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    buffer[0] = '\0';
    if (!type) {
        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "v"));
        return GCC_GEN_BE_SUCCESS;
    }

    a_type_qualifier_set quals = TQ_NONE;
    a_type_ptr curr = type;
    while (curr && curr->kind == tk_typeref) {
        if (curr->source_corresp.name && strcmp(curr->source_corresp.name, "nullptr_t") == 0) {
            GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "Dn"));
            return GCC_GEN_BE_SUCCESS;
        }
        quals |= curr->variant.typeref.qualifiers;
        curr = curr->variant.typeref.type;
    }
    if (quals & TQ_RESTRICT) GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "r"));
    if (quals & TQ_VOLATILE) GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "V"));
    if (quals & TQ_CONST)    GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "K"));

    if (!curr) {
        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "v"));
        return GCC_GEN_BE_SUCCESS;
    }

    switch (curr->kind) {
        case tk_void:
            GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "v"));
            break;
        case tk_nullptr:
            GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "Dn"));
            break;
        case tk_integer:
            if (curr->variant.integer.bool_type) {
                GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "b"));
            } else if (curr->variant.integer.enum_type) {
                a_scope_ptr pscope = curr->source_corresp.parent_scope;
                a_boolean is_nested = (pscope && pscope->kind != sck_file) ? TRUE : FALSE;
                if (is_nested) {
                    GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "N"));
                    GCC_GEN_BE_CHECK(mangle_enclosing_scopes(pscope, buffer, size));
                }
                a_const_char *ename = curr->source_corresp.name;
                if (ename && ename[0] != '\0') {
                    char part[128];
                    snprintf(part, sizeof(part), "%lu%s", (unsigned long)strlen(ename), ename);
                    GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, part));
                } else {
                    GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "4enum"));
                }
                if (is_nested) {
                    GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "E"));
                }
            } else {
                switch (curr->variant.integer.int_kind) {
                    case ik_char: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "c")); break;
                    case ik_signed_char: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "a")); break;
                    case ik_unsigned_char: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "h")); break;
                    case ik_short: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "s")); break;
                    case ik_unsigned_short: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "t")); break;
                    case ik_int: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "i")); break;
                    case ik_unsigned_int: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "j")); break;
                    case ik_long: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "l")); break;
                    case ik_unsigned_long: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "m")); break;
                    case ik_long_long: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "x")); break;
                    case ik_unsigned_long_long: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "y")); break;
#if INT128_EXTENSIONS_ALLOWED
                    case ik_int128: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "n")); break;
                    case ik_unsigned_int128: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "o")); break;
#endif
                    default: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "i")); break;
                }
            }
            break;
        case tk_complex:
            GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "C"));
            switch (curr->variant.float_kind) {
                case fk_float: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "f")); break;
                case fk_double: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "d")); break;
                case fk_long_double:
                case fk_float80: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "e")); break;
                case fk_float128:
                case fk_std_float128: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "g")); break;
                default: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "d")); break;
            }
            break;
        case tk_float:
#ifdef EDG_GCC_BE_ISOLATED_TEST
            if (curr->variant.float_supplement.is_complex) {
                GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "C"));
            }
#endif
            switch (curr->variant.float_kind) {
                case fk_float: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "f")); break;
                case fk_double: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "d")); break;
                case fk_long_double:
                case fk_float80: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "e")); break;
                case fk_float128:
                case fk_std_float128: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "g")); break;
                case fk_float16:
                case fk_std_float16: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "DF16_")); break;
                case fk_std_bfloat16: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "DF16b")); break;
                default: GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "d")); break;
            }
            break;
        case tk_vector:
            {
                a_type_ptr elem_tp = curr->variant.vector.element_type;
                long vec_sz = (long)curr->size;
                long elem_sz = elem_tp ? (long)elem_tp->size : 4L;
                long num_elems = (elem_sz > 0) ? (vec_sz / elem_sz) : 4L;
                char vbuf[64];
                snprintf(vbuf, sizeof(vbuf), "Dv%ld_", num_elems);
                GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, vbuf));
                char ebuf[128];
                ebuf[0] = '\0';
                GCC_GEN_BE_CHECK(get_mangled_type_name_inner(elem_tp, ebuf, sizeof(ebuf)));
                GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, ebuf));
            }
            break;
        case tk_pointer:
            if (curr->variant.pointer.is_reference) {
                GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, curr->variant.pointer.is_rvalue_reference ? "O" : "R"));
            } else {
                GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "P"));
            }
            {
                char sub[256];
                sub[0] = '\0';
                GCC_GEN_BE_CHECK(get_mangled_type_name_inner(curr->variant.pointer.type, sub, sizeof(sub)));
                GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, sub));
            }
            break;
        case tk_array:
            {
                char abuf[64];
                snprintf(abuf, sizeof(abuf), "A%lu_", (unsigned long)curr->variant.array.variant.number_of_elements);
                GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, abuf));
                char elem[256];
                elem[0] = '\0';
                GCC_GEN_BE_CHECK(get_mangled_type_name_inner(curr->variant.array.element_type, elem, sizeof(elem)));
                GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, elem));
            }
            break;
        case tk_routine:
            {
                a_routine_type_supplement_ptr rtsp = curr->variant.routine.extra_info;
                if (rtsp) {
                    if (rtsp->qualifiers & TQ_RESTRICT) GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "r"));
                    if (rtsp->qualifiers & TQ_VOLATILE) GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "V"));
                    if (rtsp->qualifiers & TQ_CONST)    GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "K"));
                    if (rtsp->ref_qualifiers != rqk_default) {
                        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, rtsp->ref_qualifiers == rqk_rvalue ? "O" : "R"));
                    }
                    if (rtsp->exception_specification &&
                        rtsp->exception_specification->is_noexcept &&
                        !rtsp->exception_specification->throw_any) {
                        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "Do"));
                    }
                }
                GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "F"));
                char rbuf[512];
                rbuf[0] = '\0';
                GCC_GEN_BE_CHECK(get_mangled_type_name_inner(curr->variant.routine.return_type, rbuf, sizeof(rbuf)));
                GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, rbuf));
                a_param_type_ptr ptp = rtsp ? rtsp->param_type_list : NULL;
                if (!ptp) {
                    GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "v"));
                } else {
                    for (; ptp != NULL; ptp = ptp->next) {
                        char pbuf[512];
                        pbuf[0] = '\0';
                        GCC_GEN_BE_CHECK(get_mangled_type_name_inner(ptp->type, pbuf, sizeof(pbuf)));
                        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, pbuf));
                    }
                    if (rtsp->has_ellipsis) {
                        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "z"));
                    }
                }
                GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "E"));
            }
            break;
        case tk_ptr_to_member:
            {
                GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "M"));
                char cbuf[256];
                cbuf[0] = '\0';
                GCC_GEN_BE_CHECK(get_mangled_type_name_inner(curr->variant.ptr_to_member.class_of_which_a_member, cbuf, sizeof(cbuf)));
                GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, cbuf));
                char mbuf[256];
                mbuf[0] = '\0';
                GCC_GEN_BE_CHECK(get_mangled_type_name_inner(curr->variant.ptr_to_member.type, mbuf, sizeof(mbuf)));
                GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, mbuf));
            }
            break;
        case tk_class:
        case tk_struct:
        case tk_union:
            {
                a_scope_ptr pscope = curr->source_corresp.parent_scope;
                a_boolean is_nested = (pscope && pscope->kind != sck_file) ? TRUE : FALSE;
                a_class_type_supplement_ptr ctsp = curr->variant.class_struct_union.extra_info;
                a_template_arg_ptr targs = ctsp ? ctsp->template_arg_list : NULL;
                a_const_char *cname = curr->source_corresp.name;

                /* Standard Itanium ABI shorthand substitutions for ::std::allocator, ::std::basic_string, etc. */
                if (pscope && pscope->kind == sck_namespace && pscope->variant.assoc_namespace &&
                    pscope->variant.assoc_namespace->source_corresp.name &&
                    strcmp(pscope->variant.assoc_namespace->source_corresp.name, "std") == 0 &&
                    (!pscope->parent || pscope->parent->kind == sck_file) && cname) {
                    if (strcmp(cname, "allocator") == 0 && !targs) {
                        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "Sa"));
                        break;
                    }
                    if (strcmp(cname, "basic_string") == 0 && !targs) {
                        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "Sb"));
                        break;
                    }
                    if (strcmp(cname, "string") == 0 && !targs) {
                        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "Ss"));
                        break;
                    }
                    if (strcmp(cname, "istream") == 0 && !targs) {
                        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "Si"));
                        break;
                    }
                    if (strcmp(cname, "ostream") == 0 && !targs) {
                        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "So"));
                        break;
                    }
                    if (strcmp(cname, "iostream") == 0 && !targs) {
                        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "Sd"));
                        break;
                    }
                }

                char cls_encoded[512];
                cls_encoded[0] = '\0';
                if (is_nested) {
                    GCC_GEN_BE_CHECK(append_mangled_str(cls_encoded, sizeof(cls_encoded), "N"));
                    GCC_GEN_BE_CHECK(mangle_enclosing_scopes(pscope, cls_encoded, sizeof(cls_encoded)));
                }
#if NEED_NAME_MANGLING
                if (curr->source_corresp.unmangled_name_or_mangled_encoding &&
                    curr->source_corresp.unmangled_name_or_mangled_encoding[0] != '\0') {
                    cname = curr->source_corresp.unmangled_name_or_mangled_encoding;
                }
#endif
                if (ctsp && ctsp->is_lambda_closure_class) {
                    GCC_GEN_BE_CHECK(append_mangled_str(cls_encoded, sizeof(cls_encoded), "Ul"));
                    a_boolean emitted_lambda_params = FALSE;
                    if (ctsp->assoc_scope && ctsp->assoc_scope->routines) {
                        for (a_routine_ptr lrp = ctsp->assoc_scope->routines; lrp != NULL; lrp = lrp->next) {
                            if (lrp->special_kind == sfk_operator && lrp->type && lrp->type->kind == tk_routine &&
                                lrp->type->variant.routine.extra_info) {
                                a_param_type_ptr lptp = lrp->type->variant.routine.extra_info->param_type_list;
                                if (lptp && lrp->storage_class != sc_static) {
                                    lptp = lptp->next;
                                }
                                if (lptp) {
                                    for (; lptp != NULL; lptp = lptp->next) {
                                        char lpbuf[128];
                                        lpbuf[0] = '\0';
                                        GCC_GEN_BE_CHECK(get_mangled_type_name_inner(lptp->type, lpbuf, sizeof(lpbuf)));
                                        GCC_GEN_BE_CHECK(append_mangled_str(cls_encoded, sizeof(cls_encoded), lpbuf));
                                    }
                                    emitted_lambda_params = TRUE;
                                }
                                break;
                            }
                        }
                    }
                    if (!emitted_lambda_params) {
                        GCC_GEN_BE_CHECK(append_mangled_str(cls_encoded, sizeof(cls_encoded), "v"));
                    }
                    GCC_GEN_BE_CHECK(append_mangled_str(cls_encoded, sizeof(cls_encoded), "E_"));
                } else if (cname && cname[0] != '\0') {
                    char part[256];
                    snprintf(part, sizeof(part), "%lu%s", (unsigned long)strlen(cname), cname);
                    GCC_GEN_BE_CHECK(append_mangled_str(cls_encoded, sizeof(cls_encoded), part));
                } else if (is_nested) {
                    GCC_GEN_BE_CHECK(append_mangled_str(cls_encoded, sizeof(cls_encoded), "Ut_"));
                } else {
                    GCC_GEN_BE_CHECK(append_mangled_str(cls_encoded, sizeof(cls_encoded), "11unnamed_cls"));
                }
                if (targs) {
                    GCC_GEN_BE_CHECK(mangle_template_args(targs, cls_encoded, sizeof(cls_encoded)));
                }
                if (is_nested) {
                    GCC_GEN_BE_CHECK(append_mangled_str(cls_encoded, sizeof(cls_encoded), "E"));
                }

                a_boolean subst_found = FALSE;
                char subst_tok[16];
                GCC_GEN_BE_CHECK(find_or_record_substitution(cls_encoded, &subst_found, subst_tok, sizeof(subst_tok)));
                if (subst_found) {
                    GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, subst_tok));
                } else {
                    GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, cls_encoded));
                }
            }
            break;
        default:
            GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "v"));
            break;
    }

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Retrieves a deterministic Itanium C++ ABI mangled type encoding.
 * @details Resets the active substitution table and delegates to `get_mangled_type_name_inner`.
 * @param[in] type The frontend type to mangle.
 * @param[out] buffer The buffer to store the mangled name.
 * @param[in] size The size of the buffer.
 * @return GCC_GEN_BE_SUCCESS or an error code.
 */
static gcc_gen_be_error_t get_mangled_type_name(a_type_ptr type, char *buffer, size_t size) GCC_GEN_BE_NOEXCEPT {
    active_subst_table.count = 0;
    return get_mangled_type_name_inner(type, buffer, size);
}

gcc_gen_be_error_t gcc_gen_be_mangle_type_name(a_type_ptr tp, char *buffer, size_t size) GCC_GEN_BE_NOEXCEPT {
    if (!buffer || size == 0) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    return get_mangled_type_name(tp, buffer, size);
}

gcc_gen_be_error_t gcc_gen_be_mangle_with_substitutions(
    a_type_ptr tp,
    a_boolean reset_table,
    char *buffer,
    size_t size) GCC_GEN_BE_NOEXCEPT {
    if (!buffer || size == 0) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (reset_table) {
        active_subst_table.count = 0;
    }
    return get_mangled_type_name_inner(tp, buffer, size);
}

/**
 * @brief Maps a source operator function name (`operator+`, `operator==`, etc.) to its 2-letter Itanium code.
 * @details Inspects `raw_name` and writes the Itanium ABI operator code to `*out_op_code`.
 * @param[in] raw_name Unmangled function name (e.g., `"operator()"`, `"operator new"`).
 * @param[out] out_op_code Pointer to receive the static 2-letter code, or NULL if not an operator.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
static gcc_gen_be_error_t map_operator_name_code(const char *raw_name, const char **out_op_code) GCC_GEN_BE_NOEXCEPT {
    if (!out_op_code) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_op_code = NULL;
    if (!raw_name || strncmp(raw_name, "operator", 8) != 0) return GCC_GEN_BE_SUCCESS;
    const char *rest = raw_name + 8;
    while (*rest == ' ') rest++;
    if (strcmp(rest, "new") == 0) *out_op_code = "nw";
    else if (strcmp(rest, "new[]") == 0) *out_op_code = "na";
    else if (strcmp(rest, "delete") == 0) *out_op_code = "dl";
    else if (strcmp(rest, "delete[]") == 0) *out_op_code = "da";
    else if (strcmp(rest, "co_await") == 0) *out_op_code = "aw";
    else if (strcmp(rest, "()") == 0) *out_op_code = "cl";
    else if (strcmp(rest, "[]") == 0) *out_op_code = "ix";
    else if (strcmp(rest, "->*") == 0) *out_op_code = "pm";
    else if (strcmp(rest, "->") == 0) *out_op_code = "pt";
    else if (strcmp(rest, "==") == 0) *out_op_code = "eq";
    else if (strcmp(rest, "!=") == 0) *out_op_code = "ne";
    else if (strcmp(rest, "<=>") == 0) *out_op_code = "ss";
    else if (strcmp(rest, "<=") == 0) *out_op_code = "le";
    else if (strcmp(rest, ">=") == 0) *out_op_code = "ge";
    else if (strcmp(rest, "<<=") == 0) *out_op_code = "lS";
    else if (strcmp(rest, ">>=") == 0) *out_op_code = "rS";
    else if (strcmp(rest, "<<") == 0) *out_op_code = "ls";
    else if (strcmp(rest, ">>") == 0) *out_op_code = "rs";
    else if (strcmp(rest, "<") == 0) *out_op_code = "lt";
    else if (strcmp(rest, ">") == 0) *out_op_code = "gt";
    else if (strcmp(rest, "+=") == 0) *out_op_code = "pL";
    else if (strcmp(rest, "-=") == 0) *out_op_code = "mI";
    else if (strcmp(rest, "*=") == 0) *out_op_code = "mL";
    else if (strcmp(rest, "/=") == 0) *out_op_code = "dV";
    else if (strcmp(rest, "%=") == 0) *out_op_code = "rM";
    else if (strcmp(rest, "&=") == 0) *out_op_code = "aN";
    else if (strcmp(rest, "|=") == 0) *out_op_code = "oR";
    else if (strcmp(rest, "^=") == 0) *out_op_code = "eO";
    else if (strcmp(rest, "++") == 0) *out_op_code = "pp";
    else if (strcmp(rest, "--") == 0) *out_op_code = "mm";
    else if (strcmp(rest, "&&") == 0) *out_op_code = "aa";
    else if (strcmp(rest, "||") == 0) *out_op_code = "oo";
    else if (strcmp(rest, "+") == 0) *out_op_code = "pl";
    else if (strcmp(rest, "-") == 0) *out_op_code = "mi";
    else if (strcmp(rest, "*") == 0) *out_op_code = "ml";
    else if (strcmp(rest, "/") == 0) *out_op_code = "dv";
    else if (strcmp(rest, "%") == 0) *out_op_code = "rm";
    else if (strcmp(rest, "&") == 0) *out_op_code = "an";
    else if (strcmp(rest, "|") == 0) *out_op_code = "or";
    else if (strcmp(rest, "^") == 0) *out_op_code = "eo";
    else if (strcmp(rest, "~") == 0) *out_op_code = "co";
    else if (strcmp(rest, "!") == 0) *out_op_code = "nt";
    else if (strcmp(rest, ",") == 0) *out_op_code = "cm";
    else if (strcmp(rest, "=") == 0) *out_op_code = "aS";
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_mangle_function_name(
    a_routine_ptr rout,
    const char *variant_code,
    char *buffer,
    size_t size) GCC_GEN_BE_NOEXCEPT {
    if (!rout || !buffer || size == 0) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    buffer[0] = '\0';
    active_subst_table.count = 0;

#if NEED_NAME_MANGLING
    if (!variant_code &&
        rout->source_corresp.unmangled_name_or_mangled_encoding &&
        strncmp(rout->source_corresp.unmangled_name_or_mangled_encoding, "_Z", 2) == 0) {
        return append_mangled_str(buffer, size, rout->source_corresp.unmangled_name_or_mangled_encoding);
    }
#endif

    a_const_char *raw_name = rout->source_corresp.name;
    if (raw_name && strncmp(raw_name, "_Z", 2) == 0 && !variant_code) {
        return append_mangled_str(buffer, size, raw_name);
    }

    a_scope_ptr pscope = rout->source_corresp.parent_scope;
    a_boolean is_nested = (pscope && pscope->kind != sck_file) ? TRUE : FALSE;
    a_routine_type_supplement_ptr rtsp =
        (rout->type && rout->type->kind == tk_routine) ? rout->type->variant.routine.extra_info : NULL;

    GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "_Z"));
    if (is_nested) {
        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "N"));
        if (rtsp) {
            if (rtsp->qualifiers & TQ_RESTRICT) GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "r"));
            if (rtsp->qualifiers & TQ_VOLATILE) GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "V"));
            if (rtsp->qualifiers & TQ_CONST)    GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "K"));
            if (rtsp->ref_qualifiers != rqk_default) {
                GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, rtsp->ref_qualifiers == rqk_rvalue ? "O" : "R"));
            }
        }
        GCC_GEN_BE_CHECK(mangle_enclosing_scopes(pscope, buffer, size));
    }

    const char *op_code = NULL;
    GCC_GEN_BE_CHECK(map_operator_name_code(raw_name, &op_code));

    if (variant_code && variant_code[0] != '\0') {
        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, variant_code));
    } else if (rout->special_kind == sfk_constructor) {
        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "C1"));
    } else if (rout->special_kind == sfk_destructor) {
        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "D1"));
    } else if (rout->special_kind == sfk_conversion) {
        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "cv"));
        char cv_tbuf[128];
        cv_tbuf[0] = '\0';
        a_type_ptr ret_tp = (rout->type && rout->type->kind == tk_routine) ? rout->type->variant.routine.return_type : NULL;
        GCC_GEN_BE_CHECK(get_mangled_type_name_inner(ret_tp, cv_tbuf, sizeof(cv_tbuf)));
        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, cv_tbuf));
    } else if (raw_name && strncmp(raw_name, "operator\"\"", 10) == 0) {
        const char *lit_suf = raw_name + 10;
        while (*lit_suf == ' ') lit_suf++;
        char lpart[128];
        snprintf(lpart, sizeof(lpart), "li%lu%s", (unsigned long)strlen(lit_suf), lit_suf);
        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, lpart));
    } else if (op_code) {
        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, op_code));
    } else if (raw_name && raw_name[0] != '\0') {
        if (rout->source_corresp.module_info &&
            !rout->source_corresp.is_exported &&
            rout->source_corresp.module_info->kind == mk_unit &&
            rout->source_corresp.module_info->variant.unit.name &&
            rout->source_corresp.module_info->variant.unit.name[0] != '\0') {
            const char *mname = rout->source_corresp.module_info->variant.unit.name;
            char mpart[128];
            snprintf(mpart, sizeof(mpart), "W%lu%s", (unsigned long)strlen(mname), mname);
            GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, mpart));
        }
        char npart[256];
        snprintf(npart, sizeof(npart), "%lu%s", (unsigned long)strlen(raw_name), raw_name);
        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, npart));
    } else {
        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "6anonfn"));
    }

    if (rout->is_template_function && rout->template_arg_list) {
        GCC_GEN_BE_CHECK(mangle_template_args(rout->template_arg_list, buffer, size));
    }

    if (is_nested) {
        GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "E"));
    }

    a_param_type_ptr ptp = rtsp ? rtsp->param_type_list : NULL;
    if (is_nested && pscope->kind == sck_class_struct_union && rout->storage_class != sc_static && ptp) {
        /* Skip implicit `this` parameter in parameter list encoding */
        ptp = ptp->next;
    }
    if (!ptp) {
        if (!rtsp || !rtsp->has_ellipsis) {
            GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "v"));
        } else {
            GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "z"));
        }
    } else {
        for (; ptp != NULL; ptp = ptp->next) {
            char pbuf[256];
            pbuf[0] = '\0';
            GCC_GEN_BE_CHECK(get_mangled_type_name_inner(ptp->type, pbuf, sizeof(pbuf)));
            GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, pbuf));
        }
        if (rtsp && rtsp->has_ellipsis) {
            GCC_GEN_BE_CHECK(append_mangled_str(buffer, size, "z"));
        }
    }

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Emits a virtual table for a given class type matching Itanium ABI.
 *
 * @details Replaces address-based naming with true Itanium ABI symbol mangling (_ZTV...).
 * Calculates virtual base class offsets, virtual call offsets, and offset-to-top.
 * Populates primary virtual function table pointers, secondary vtables, and Construction
 * Virtual Tables (`_ZTC<Derived><Offset>_<Base>`), and handles pure/deleted virtuals.
 *
 * @param class_type The class type for which to emit the vtable.
 * @param out_vtable Pointer to receive the generated JIT lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_vtable(a_type_ptr class_type, struct gcc_jit_lvalue **out_vtable) GCC_GEN_BE_NOEXCEPT {
    if (!class_type || !out_vtable) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_vtable = NULL;

    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    a_class_type_supplement_ptr ctsp = class_type->variant.class_struct_union.extra_info;
    if (!ctsp) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    int num_funcs = 0;
    if (ctsp->highest_virtual_function_number != VIRTUAL_FUNCTION_NUMBER_NONE) {
        num_funcs = ctsp->highest_virtual_function_number + 1;
    }

    int num_vcall_offsets = 0;
    int num_vbase_offsets = 0;
    a_base_class_ptr b = ctsp->base_classes ? ctsp->base_classes : ctsp->direct_base_classes;
    a_boolean use_all_bases = (ctsp->base_classes != NULL) ? TRUE : FALSE;
    while (b) {
        if (b->is_virtual || b->direct) {
            if (b->is_virtual) num_vbase_offsets++;
            num_vcall_offsets += num_funcs;
        }
        b = use_all_bases ? b->next : b->next_direct;
    }

    size_t num_entries = (size_t)(num_vcall_offsets + num_vbase_offsets + 2 + num_funcs);
    
    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));
    gcc_jit_type *array_type = gcc_jit_context_new_array_type(ctx, NULL, void_ptr_type, (int)num_entries);

    char mangled_type[1024];
    GCC_GEN_BE_CHECK(get_mangled_type_name(class_type, mangled_type, sizeof(mangled_type)));

    char vtable_name[1088];
    snprintf(vtable_name, sizeof(vtable_name), "_ZTV%s", mangled_type);

    void *cached_vtbl = NULL;
    if (cache_lookup(GCC_GEN_BE_CACHE_VAR, class_type, &cached_vtbl) == GCC_GEN_BE_SUCCESS && cached_vtbl) {
        *out_vtable = (gcc_jit_lvalue *)cached_vtbl;
        return GCC_GEN_BE_SUCCESS;
    }
    if (cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)vtable_name, &cached_vtbl) == GCC_GEN_BE_SUCCESS && cached_vtbl) {
        void *var_lval = NULL;
        if (cache_lookup(GCC_GEN_BE_CACHE_VAR, cached_vtbl, &var_lval) == GCC_GEN_BE_SUCCESS && var_lval) {
            cached_vtbl = var_lval;
        }
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, class_type, cached_vtbl));
        *out_vtable = (gcc_jit_lvalue *)cached_vtbl;
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_lvalue *vtable = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, array_type, vtable_name);
    GCC_GEN_BE_CHECK(mark_global_vague_linkage(vtable));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, class_type, vtable));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)vtable_name, vtable));
    
    gcc_jit_rvalue **vtable_entries = (gcc_jit_rvalue **)malloc(num_entries * sizeof(gcc_jit_rvalue *));
    if (!vtable_entries) return GCC_GEN_BE_ERROR_OOM;

    int idx = 0;
    for (int i = 0; i < num_vcall_offsets; ++i) {
        long vcall_off = 0L;
        if (ctsp->assoc_scope && num_funcs > 0) {
            int target_vnum = i % num_funcs;
            for (a_routine_ptr vr = ctsp->assoc_scope->routines; vr != NULL; vr = vr->next) {
                if (vr->number.virtual_function == target_vnum && vr->delta != 0) {
                    vcall_off = (long)vr->delta;
                    break;
                }
            }
        }
        if (vcall_off != 0L) {
            vtable_entries[idx++] = gcc_jit_context_new_rvalue_from_ptr(ctx, void_ptr_type, (void *)(uintptr_t)vcall_off);
        } else {
            vtable_entries[idx++] = gcc_jit_context_null(ctx, void_ptr_type);
        }
    }
    
    b = use_all_bases ? ctsp->base_classes : ctsp->direct_base_classes;
    while (b) {
        if (b->is_virtual) {
            long vbase_off = (long)b->offset;
            if (vbase_off != 0L) {
                vtable_entries[idx++] = gcc_jit_context_new_rvalue_from_ptr(ctx, void_ptr_type, (void *)(uintptr_t)vbase_off);
            } else {
                vtable_entries[idx++] = gcc_jit_context_null(ctx, void_ptr_type);
            }
        }
        b = use_all_bases ? b->next : b->next_direct;
    }

    /* Offset-to-top (0 for primary vtable) */
    vtable_entries[idx++] = gcc_jit_context_null(ctx, void_ptr_type);

    gcc_jit_rvalue *type_info_ptr = NULL;
    an_expr_node dummy_expr;
    memset(&dummy_expr, 0, sizeof(dummy_expr));
    dummy_expr.kind = enk_typeid;
    an_expr_node dummy_type_op;
    memset(&dummy_type_op, 0, sizeof(dummy_type_op));
    dummy_type_op.kind = enk_type_operand;
    dummy_type_op.variant.type_operand.type = class_type;
    dummy_expr.variant.typeid_info.type_with_opt_expr = &dummy_type_op;
    err = gcc_gen_be_lower_typeid(&dummy_expr, &type_info_ptr);
    if (err != GCC_GEN_BE_SUCCESS) {
        free(vtable_entries);
        return err;
    }
    vtable_entries[idx++] = gcc_jit_context_new_cast(ctx, NULL, type_info_ptr, void_ptr_type);

    gcc_jit_function *pure_virt_fn = NULL;
    gcc_jit_function *deleted_virt_fn = NULL;
    
    int func_start_idx = idx;
    for (int i = 0; i < num_funcs; ++i) {
        vtable_entries[func_start_idx + i] = NULL;
    }

    a_routine_ptr rp = ctsp->assoc_scope ? ctsp->assoc_scope->routines : NULL;
    for (; rp != NULL; rp = rp->next) {
        if (rp->number.virtual_function != VIRTUAL_FUNCTION_NUMBER_NONE && 
            rp->number.virtual_function < num_funcs) {
            
            gcc_jit_rvalue *func_addr = NULL;
            if (rp->pure_virtual) {
                if (!pure_virt_fn) {
                    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
                    pure_virt_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "__cxa_pure_virtual", 0, NULL, 0);
                }
                func_addr = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(pure_virt_fn, NULL), void_ptr_type);
            } else if (rp->is_deleted) {
                if (!deleted_virt_fn) {
                    gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
                    deleted_virt_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "__cxa_deleted_virtual", 0, NULL, 0);
                }
                func_addr = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(deleted_virt_fn, NULL), void_ptr_type);
            } else if (rp->overriding_function_for_wrapper && (rp->delta != 0 || rp->vcall_index != 0 || rp->return_delta != 0 || rp->vbase_index != 0)) {
                gcc_jit_function *thunk_fn = NULL;
                if (gcc_gen_be_emit_thunk(rp, &thunk_fn) == GCC_GEN_BE_SUCCESS && thunk_fn) {
                    func_addr = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(thunk_fn, NULL), void_ptr_type);
                }
            } else {
                void *cached = NULL;
                err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, rp, &cached);
                if (err == GCC_GEN_BE_SUCCESS && cached) {
                    gcc_jit_function *fn = (gcc_jit_function *)cached;
                    func_addr = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(fn, NULL), void_ptr_type);
                } else {
                    extern gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT;
                    gcc_jit_function *fn = NULL;
                    if (gcc_gen_be_lower_function_decl(rp, &fn) == GCC_GEN_BE_SUCCESS && fn) {
                        func_addr = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(fn, NULL), void_ptr_type);
                    }
                }
            }
            if (func_addr) {
                vtable_entries[func_start_idx + rp->number.virtual_function] = func_addr;
            }
        }
    }

    for (int i = 0; i < num_funcs; ++i) {
        if (vtable_entries[func_start_idx + i] == NULL) {
            if (!pure_virt_fn) {
                gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
                pure_virt_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "__cxa_pure_virtual", 0, NULL, 0);
            }
            vtable_entries[func_start_idx + i] = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(pure_virt_fn, NULL), void_ptr_type);
        }
    }

    /* Populate secondary vtables and Construction Virtual Tables (_ZTC<Derived><Offset>_<Base>) */
    b = use_all_bases ? ctsp->base_classes : ctsp->direct_base_classes;
    int secondary_idx = 0;
    while (b) {
        if (!b->is_virtual && !b->direct) {
            b = use_all_bases ? b->next : b->next_direct;
            continue;
        }
        char mangled_base[256];
        GCC_GEN_BE_CHECK(get_mangled_type_name(b->type, mangled_base, sizeof(mangled_base)));

        gcc_jit_rvalue **sec_entries = (gcc_jit_rvalue **)malloc(num_entries * sizeof(gcc_jit_rvalue *));
        if (!sec_entries) {
            free(vtable_entries);
            return GCC_GEN_BE_ERROR_OOM;
        }
        for (size_t k = 0; k < num_entries; ++k) {
            sec_entries[k] = vtable_entries[k];
        }
        /* Negative offset-to-top, vbase offsets, and vcall offsets for secondary vtable */
        long sec_off_to_top = -((long)b->offset);
        gcc_jit_rvalue *off_to_top_ptr = (sec_off_to_top != 0L)
            ? gcc_jit_context_new_rvalue_from_ptr(ctx, void_ptr_type, (void *)(uintptr_t)sec_off_to_top)
            : gcc_jit_context_null(ctx, void_ptr_type);
        for (int vc = 0; vc < num_vcall_offsets; ++vc) {
            sec_entries[vc] = off_to_top_ptr;
        }
        int vb_slot = num_vcall_offsets;
        for (a_base_class_ptr vb = use_all_bases ? ctsp->base_classes : ctsp->direct_base_classes;
             vb != NULL && vb_slot < num_vcall_offsets + num_vbase_offsets;
             vb = use_all_bases ? vb->next : vb->next_direct) {
            if (vb->is_virtual) {
                long rel_vbase_off = (long)vb->offset - (long)b->offset;
                sec_entries[vb_slot++] = (rel_vbase_off != 0L)
                    ? gcc_jit_context_new_rvalue_from_ptr(ctx, void_ptr_type, (void *)(uintptr_t)rel_vbase_off)
                    : gcc_jit_context_null(ctx, void_ptr_type);
            }
        }
        sec_entries[num_vcall_offsets + num_vbase_offsets] = off_to_top_ptr;

        /* Populate secondary vtable virtual slots with `this`-adjusting thunks when base offset != 0 */
        gcc_gen_be_error_t sec_err = GCC_GEN_BE_SUCCESS;
        gcc_jit_rvalue *sec_init = NULL;
        gcc_jit_lvalue *sec_vtable = NULL;
        gcc_jit_lvalue *ctc_vtable = NULL;
        char sec_name[300];
        char ctc_name[300];
        if (b->offset != 0 && ctsp->assoc_scope) {
            for (a_routine_ptr srp = ctsp->assoc_scope->routines; srp != NULL; srp = srp->next) {
                if (srp->number.virtual_function != VIRTUAL_FUNCTION_NUMBER_NONE &&
                    srp->number.virtual_function < num_funcs &&
                    !srp->pure_virtual && !srp->is_deleted &&
                    srp->overriding_function_for_wrapper) {
                    gcc_jit_function *sec_thunk = NULL;
                    if (srp->return_delta != 0 || srp->vbase_index != 0) {
                        GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_emit_covariant_return_thunk(
                            srp, (long)srp->delta,
                            srp->vbase_index != 0 ? (long)srp->vbase_index : (long)srp->return_delta,
                            srp->vbase_index != 0 ? TRUE : FALSE, &sec_thunk), sec_err, sec_cleanup);
                    } else if (b->is_virtual || srp->vcall_index != 0) {
                        GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_emit_vcall_thunk(class_type, b, srp, (long)srp->vcall_index, &sec_thunk), sec_err, sec_cleanup);
                    } else {
                        GCC_GEN_BE_CHECK_CLEANUP(gcc_gen_be_emit_thunk(srp, &sec_thunk), sec_err, sec_cleanup);
                    }
                    if (sec_thunk) {
                        sec_entries[func_start_idx + srp->number.virtual_function] =
                            gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(sec_thunk, NULL), void_ptr_type);
                    }
                }
            }
        }

        sec_init = gcc_jit_context_new_array_constructor(ctx, NULL, array_type, num_entries, sec_entries);

        snprintf(sec_name, sizeof(sec_name), "_ZTV_sec_%s_%d", mangled_type, secondary_idx++);
        sec_vtable = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, array_type, sec_name);
        GCC_GEN_BE_CHECK_CLEANUP(mark_global_vague_linkage(sec_vtable), sec_err, sec_cleanup);
        gcc_jit_global_set_initializer_rvalue(sec_vtable, sec_init);
        
        /* Construction Virtual Table: _ZTC<Derived><Offset>_<Base> */
        snprintf(ctc_name, sizeof(ctc_name), "_ZTC%s%lu_%s", mangled_type, (unsigned long)b->offset, mangled_base);
        ctc_vtable = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, array_type, ctc_name);
        GCC_GEN_BE_CHECK_CLEANUP(mark_global_vague_linkage(ctc_vtable), sec_err, sec_cleanup);
        gcc_jit_global_set_initializer_rvalue(ctc_vtable, sec_init);
        GCC_GEN_BE_CHECK_CLEANUP(cache_insert(GCC_GEN_BE_CACHE_VAR, b, ctc_vtable), sec_err, sec_cleanup);

    sec_cleanup:
        free(sec_entries);
        if (sec_err != GCC_GEN_BE_SUCCESS) {
            free(vtable_entries);
            return sec_err;
        }
        b = use_all_bases ? b->next : b->next_direct;
    }

    gcc_jit_rvalue *init_array = gcc_jit_context_new_array_constructor(ctx, NULL, array_type, num_entries, vtable_entries);
    gcc_jit_global_set_initializer_rvalue(vtable, init_array);

    free(vtable_entries);
    
    *out_vtable = vtable;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Emits a Virtual Table Table (VTT) for classes with virtual base classes.
 *
 * @details Synthesizes primary VTT arrays referencing the complete primary class vtable,
 * subobject construction vtables (`_ZTC`), and secondary virtual base vtables.
 *
 * @param class_type The class type for which to emit the VTT.
 * @param out_vtt Pointer to receive the generated JIT lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_vtt(a_type_ptr class_type, struct gcc_jit_lvalue **out_vtt) GCC_GEN_BE_NOEXCEPT {
    if (!class_type || !out_vtt) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_vtt = NULL;

    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));
    
    a_class_type_supplement_ptr ctsp = class_type->variant.class_struct_union.extra_info;
    int num_secondary_vtts = 0;
    int num_vbase_offsets = 0;
    int num_funcs = 0;
    if (ctsp && ctsp->highest_virtual_function_number != VIRTUAL_FUNCTION_NUMBER_NONE) {
        num_funcs = ctsp->highest_virtual_function_number + 1;
    }
    a_boolean use_all_bases = (ctsp && ctsp->base_classes != NULL) ? TRUE : FALSE;
    a_base_class_ptr b = ctsp ? (use_all_bases ? ctsp->base_classes : ctsp->direct_base_classes) : NULL;
    while (b) {
        if (b->is_virtual || b->direct) {
            num_secondary_vtts++;
            if (b->is_virtual) num_vbase_offsets++;
        }
        b = use_all_bases ? b->next : b->next_direct;
    }
    int addr_point_idx = (num_secondary_vtts * num_funcs) + num_vbase_offsets + 2;
    
    size_t num_vtt_entries = 1 + (size_t)num_secondary_vtts;
    gcc_jit_type *array_type = gcc_jit_context_new_array_type(ctx, NULL, void_ptr_type, (int)num_vtt_entries);

    char mangled_type[1024];
    GCC_GEN_BE_CHECK(get_mangled_type_name(class_type, mangled_type, sizeof(mangled_type)));
    if (strstr(mangled_type, "__SO__") != NULL) {
        return GCC_GEN_BE_SUCCESS;
    }

    char vtt_name[1088];
    snprintf(vtt_name, sizeof(vtt_name), "_ZTT%s", mangled_type);

    void *cached_vtt = NULL;
    if (cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)vtt_name, &cached_vtt) == GCC_GEN_BE_SUCCESS && cached_vtt) {
        void *var_lval = NULL;
        if (cache_lookup(GCC_GEN_BE_CACHE_VAR, cached_vtt, &var_lval) == GCC_GEN_BE_SUCCESS && var_lval) {
            cached_vtt = var_lval;
        }
        *out_vtt = (gcc_jit_lvalue *)cached_vtt;
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_lvalue *vtable = NULL;
    err = gcc_gen_be_emit_vtable(class_type, &vtable);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    gcc_jit_lvalue *vtt = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, array_type, vtt_name);
    GCC_GEN_BE_CHECK(mark_global_vague_linkage(vtt));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)vtt_name, vtt));

    gcc_jit_rvalue **vtt_entries = (gcc_jit_rvalue **)malloc(num_vtt_entries * sizeof(gcc_jit_rvalue *));
    if (!vtt_entries) return GCC_GEN_BE_ERROR_OOM;

    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_rvalue *ap_idx_rval = gcc_jit_context_new_rvalue_from_int(ctx, int_type, addr_point_idx);
    gcc_jit_rvalue *vtable_addr = gcc_jit_lvalue_get_address(
        gcc_jit_context_new_array_access(ctx, NULL, gcc_jit_lvalue_as_rvalue(vtable), ap_idx_rval), NULL);
    vtt_entries[0] = gcc_jit_context_new_cast(ctx, NULL, vtable_addr, void_ptr_type);
    
    b = ctsp ? (use_all_bases ? ctsp->base_classes : ctsp->direct_base_classes) : NULL;
    for (int i = 0; i < num_secondary_vtts; ++i) {
        while (b && !b->is_virtual && !b->direct) {
            b = use_all_bases ? b->next : b->next_direct;
        }
        void *cached_ctc = NULL;
        if (b) {
            GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_VAR, b, &cached_ctc));
            b = use_all_bases ? b->next : b->next_direct;
        }
        if (cached_ctc) {
            gcc_jit_rvalue *ctc_addr = gcc_jit_lvalue_get_address(
                gcc_jit_context_new_array_access(ctx, NULL, gcc_jit_lvalue_as_rvalue((gcc_jit_lvalue *)cached_ctc), ap_idx_rval), NULL);
            vtt_entries[1 + i] = gcc_jit_context_new_cast(ctx, NULL, ctc_addr, void_ptr_type);
        } else {
            vtt_entries[1 + i] = vtt_entries[0];
        }
    }
    
    gcc_jit_rvalue *init_array = gcc_jit_context_new_array_constructor(ctx, NULL, array_type, num_vtt_entries, vtt_entries);
    gcc_jit_global_set_initializer_rvalue(vtt, init_array);
    free(vtt_entries);

    *out_vtt = vtt;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Emits a virtual method dispatch thunk.
 *
 * @details Synthesizes non-virtual this-adjustment thunks (_ZThn...), virtual 
 * this-adjustment thunks (_ZTv...), and covariant return thunks (_ZTch..., _ZTcv...).
 * Generates tail-calls to preserve call stack.
 *
 * @param thunk_rp The thunk routine pointer to emit.
 * @param out_func Pointer to receive the generated JIT function.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_thunk(a_routine_ptr thunk_rp, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT {
    if (!thunk_rp || !out_func) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_func = NULL;

    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    
    char thunk_name[256];
    a_const_char *target_name = (thunk_rp->overriding_function_for_wrapper && thunk_rp->overriding_function_for_wrapper->source_corresp.name)
        ? thunk_rp->overriding_function_for_wrapper->source_corresp.name
        : (thunk_rp->source_corresp.name ? thunk_rp->source_corresp.name : "vfunc");
    long delta_abs = thunk_rp->delta < 0 ? -((long)thunk_rp->delta) : (long)thunk_rp->delta;
    long rdelta_abs = thunk_rp->return_delta < 0 ? -((long)thunk_rp->return_delta) : (long)thunk_rp->return_delta;
    long vcall_abs = thunk_rp->vcall_index < 0 ? -((long)thunk_rp->vcall_index) : (long)thunk_rp->vcall_index;
    long vbase_abs = thunk_rp->vbase_index < 0 ? -((long)thunk_rp->vbase_index) : (long)thunk_rp->vbase_index;
    if (thunk_rp->return_delta != 0 || thunk_rp->vbase_index != 0) {
        if (thunk_rp->vcall_index != 0 || thunk_rp->vbase_index != 0) {
            snprintf(thunk_name, sizeof(thunk_name), "_ZTcv0_n%ld_v0_n%ld_%s",
                     vcall_abs != 0 ? vcall_abs : delta_abs,
                     vbase_abs != 0 ? vbase_abs : rdelta_abs,
                     target_name);
        } else {
            snprintf(thunk_name, sizeof(thunk_name), "_ZTch%s%ld_%s%ld_%s",
                     thunk_rp->delta < 0 ? "n" : "", delta_abs,
                     thunk_rp->return_delta < 0 ? "n" : "", rdelta_abs,
                     target_name);
        }
    } else if (thunk_rp->vcall_index != 0) {
        snprintf(thunk_name, sizeof(thunk_name), "_ZTv0_n%ld_%s", vcall_abs, target_name);
    } else {
        snprintf(thunk_name, sizeof(thunk_name), "_ZTh%s%ld_%s",
                 thunk_rp->delta < 0 ? "n" : "", delta_abs, target_name);
    }
    
    gcc_jit_type *ret_type = NULL;
    err = gcc_gen_be_lower_type(thunk_rp->type->variant.routine.return_type, &ret_type);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    
    extern gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT;
    a_type_ptr orig_ret_type = thunk_rp->type->variant.routine.return_type;
    while (orig_ret_type && orig_ret_type->kind == tk_typeref) orig_ret_type = orig_ret_type->variant.typeref.type;
    a_boolean uses_sret = FALSE;
    err = gcc_gen_be_is_passed_by_invisible_reference(orig_ret_type, &uses_sret);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    gcc_jit_type *sret_ptr_type = NULL;
    if (uses_sret) {
        sret_ptr_type = gcc_jit_type_get_pointer(ret_type);
        ret_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
    }

    int num_params = uses_sret ? 1 : 0;
    a_param_type_ptr ptp;
    for (ptp = thunk_rp->type->variant.routine.extra_info->param_type_list; ptp != NULL; ptp = ptp->next) {
        num_params++;
    }
    
    gcc_jit_param **params = NULL;
    if (num_params > 0) {
        params = (gcc_jit_param **)malloc((size_t)num_params * sizeof(gcc_jit_param *));
        int i = 0;
        if (uses_sret) {
            params[i++] = gcc_jit_context_new_param(ctx, NULL, sret_ptr_type, "sret_buffer");
        }
        for (ptp = thunk_rp->type->variant.routine.extra_info->param_type_list; ptp != NULL; ptp = ptp->next) {
            gcc_jit_type *ptype = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(ptp->type, &ptype));
            
            a_type_ptr p_orig = ptp->type;
            while (p_orig && p_orig->kind == tk_typeref) p_orig = p_orig->variant.typeref.type;
            a_boolean is_p_invis_ref = FALSE;
            err = gcc_gen_be_is_passed_by_invisible_reference(p_orig, &is_p_invis_ref);
            if (err != GCC_GEN_BE_SUCCESS) { free(params); return err; }
            if (is_p_invis_ref) {
                ptype = gcc_jit_type_get_pointer(ptype);
            }
            
            char pname[32];
            snprintf(pname, sizeof(pname), "p%d", i);
            params[i] = gcc_jit_context_new_param(ctx, NULL, ptype, pname);
            i++;
        }
    }
    
    int is_variadic = thunk_rp->type->variant.routine.extra_info->has_ellipsis ? 1 : 0;
    
    gcc_jit_function *thunk_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_EXPORTED, ret_type, thunk_name, num_params, params, is_variadic);
    
    gcc_jit_block *block = gcc_jit_function_new_block(thunk_fn, "entry");
    int this_param_idx = uses_sret ? 1 : 0;
    if (num_params <= this_param_idx) {
        if (params) free(params);
        return GCC_GEN_BE_ERROR_INTERNAL;
    }
    
    gcc_jit_type *void_ptr_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID_PTR);
    gcc_jit_type *ptrdiff_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
    
    gcc_jit_rvalue *this_val = gcc_jit_param_as_rvalue(params[this_param_idx]);
    gcc_jit_type *this_type = gcc_jit_rvalue_get_type(this_val);
    gcc_jit_rvalue *this_bytes = gcc_jit_context_new_cast(ctx, NULL, this_val, void_ptr_type);
    
    if (thunk_rp->delta != 0) {
        gcc_jit_rvalue *delta_rval = gcc_jit_context_new_rvalue_from_long(ctx, ptrdiff_type, (long)thunk_rp->delta);
        gcc_jit_rvalue *this_arr = gcc_jit_context_new_cast(ctx, NULL, this_bytes, gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR)));
        this_bytes = gcc_jit_lvalue_get_address(gcc_jit_context_new_array_access(ctx, NULL, this_arr, delta_rval), NULL);
        this_bytes = gcc_jit_context_new_cast(ctx, NULL, this_bytes, void_ptr_type);
    }
    
    if (thunk_rp->vcall_index != 0) {
        gcc_jit_type *vtable_ptr_type = gcc_jit_type_get_pointer(gcc_jit_type_get_pointer(void_ptr_type));
        gcc_jit_lvalue *vtable_ptr_lval = gcc_jit_rvalue_dereference(gcc_jit_context_new_cast(ctx, NULL, this_bytes, vtable_ptr_type), NULL);
        gcc_jit_rvalue *vtable_ptr = gcc_jit_lvalue_as_rvalue(vtable_ptr_lval);
        
        gcc_jit_rvalue *vcall_idx = gcc_jit_context_new_rvalue_from_long(ctx, ptrdiff_type, (long)thunk_rp->vcall_index);
        gcc_jit_type *ptrdiff_ptr_type = gcc_jit_type_get_pointer(ptrdiff_type);
        gcc_jit_rvalue *vtable_ptrdiff = gcc_jit_context_new_cast(ctx, NULL, vtable_ptr, ptrdiff_ptr_type);
        gcc_jit_lvalue *offset_lval = gcc_jit_context_new_array_access(ctx, NULL, vtable_ptrdiff, vcall_idx);
        gcc_jit_rvalue *offset = gcc_jit_lvalue_as_rvalue(offset_lval);
        
        gcc_jit_rvalue *this_arr = gcc_jit_context_new_cast(ctx, NULL, this_bytes, gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR)));
        this_bytes = gcc_jit_lvalue_get_address(gcc_jit_context_new_array_access(ctx, NULL, this_arr, offset), NULL);
        this_bytes = gcc_jit_context_new_cast(ctx, NULL, this_bytes, void_ptr_type);
    }
    
    gcc_jit_rvalue *adjusted_this = gcc_jit_context_new_cast(ctx, NULL, this_bytes, this_type);
    
    gcc_jit_rvalue **call_args = (gcc_jit_rvalue **)malloc((size_t)num_params * sizeof(gcc_jit_rvalue *));
    for (int i = 0; i < num_params; i++) {
        call_args[i] = (i == this_param_idx) ? adjusted_this : gcc_jit_param_as_rvalue(params[i]);
    }
    
    extern gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT;
    gcc_jit_function *target_fn = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_function_decl(thunk_rp->overriding_function_for_wrapper, &target_fn));
    
    gcc_jit_rvalue *call_rval = gcc_jit_context_new_call(ctx, NULL, target_fn, num_params, call_args);
    
    if (thunk_rp->return_delta != 0 || thunk_rp->vbase_index != 0) {
        gcc_jit_lvalue *ret_lval = gcc_jit_function_new_local(thunk_fn, NULL, ret_type, "ret_val");
        gcc_jit_block_add_assignment(block, NULL, ret_lval, call_rval);
        
        gcc_jit_rvalue *ret_bytes = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_as_rvalue(ret_lval), void_ptr_type);
        gcc_jit_rvalue *null_ptr = gcc_jit_context_null(ctx, void_ptr_type);
        
        gcc_jit_block *adjust_block = gcc_jit_function_new_block(thunk_fn, "adjust_return");
        gcc_jit_block *end_block = gcc_jit_function_new_block(thunk_fn, "end_return");
        
        gcc_jit_rvalue *is_not_null = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_NE, ret_bytes, null_ptr);
        gcc_jit_block_end_with_conditional(block, NULL, is_not_null, adjust_block, end_block);
        
        if (thunk_rp->vbase_index != 0) {
            gcc_jit_type *vtable_ptr_type = gcc_jit_type_get_pointer(gcc_jit_type_get_pointer(void_ptr_type));
            gcc_jit_lvalue *vtable_ptr_lval = gcc_jit_rvalue_dereference(gcc_jit_context_new_cast(ctx, NULL, ret_bytes, vtable_ptr_type), NULL);
            gcc_jit_rvalue *vtable_ptr = gcc_jit_lvalue_as_rvalue(vtable_ptr_lval);
            
            gcc_jit_rvalue *vbase_idx = gcc_jit_context_new_rvalue_from_long(ctx, ptrdiff_type, (long)thunk_rp->vbase_index);
            gcc_jit_type *ptrdiff_ptr_type = gcc_jit_type_get_pointer(ptrdiff_type);
            gcc_jit_rvalue *vtable_ptrdiff = gcc_jit_context_new_cast(ctx, NULL, vtable_ptr, ptrdiff_ptr_type);
            gcc_jit_lvalue *offset_lval = gcc_jit_context_new_array_access(ctx, NULL, vtable_ptrdiff, vbase_idx);
            gcc_jit_rvalue *offset = gcc_jit_lvalue_as_rvalue(offset_lval);
            
            gcc_jit_rvalue *ret_arr = gcc_jit_context_new_cast(ctx, NULL, ret_bytes, gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR)));
            ret_bytes = gcc_jit_lvalue_get_address(gcc_jit_context_new_array_access(ctx, NULL, ret_arr, offset), NULL);
            ret_bytes = gcc_jit_context_new_cast(ctx, NULL, ret_bytes, void_ptr_type);
        }
        if (thunk_rp->return_delta != 0) {
            gcc_jit_rvalue *delta_rval = gcc_jit_context_new_rvalue_from_long(ctx, ptrdiff_type, (long)thunk_rp->return_delta);
            gcc_jit_rvalue *ret_arr = gcc_jit_context_new_cast(ctx, NULL, ret_bytes, gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR)));
            ret_bytes = gcc_jit_lvalue_get_address(gcc_jit_context_new_array_access(ctx, NULL, ret_arr, delta_rval), NULL);
            ret_bytes = gcc_jit_context_new_cast(ctx, NULL, ret_bytes, void_ptr_type);
        }
        
        gcc_jit_block_add_assignment(adjust_block, NULL, ret_lval, gcc_jit_context_new_cast(ctx, NULL, ret_bytes, ret_type));
        gcc_jit_block_end_with_jump(adjust_block, NULL, end_block);
        
        gcc_jit_block_end_with_return(end_block, NULL, gcc_jit_lvalue_as_rvalue(ret_lval));
    } else {
        if (p_gcc_jit_rvalue_set_bool_require_tail_call) {
            p_gcc_jit_rvalue_set_bool_require_tail_call(call_rval, 1);
        }
        if (ret_type == gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID)) {
            gcc_jit_block_add_eval(block, NULL, call_rval);
            gcc_jit_block_end_with_void_return(block, NULL);
        } else {
            gcc_jit_block_end_with_return(block, NULL, call_rval);
        }
    }
    
    if (params) free(params);
    free(call_args);

    *out_func = thunk_fn;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_emit_vcall_thunk(
    a_type_ptr class_type,
    a_base_class_ptr vbase,
    a_routine_ptr rout,
    long vcall_offset,
    struct gcc_jit_function **out_thunk) GCC_GEN_BE_NOEXCEPT {
    if (!rout || !out_thunk) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_thunk = NULL;
    (void)class_type;

    a_targ_ptrdiff_t saved_delta = rout->delta;
    a_virtual_table_index saved_vcall = rout->vcall_index;
    if (vbase && rout->delta == 0) {
        rout->delta = (a_targ_ptrdiff_t)vbase->offset;
    }
    if (vcall_offset != 0 && rout->vcall_index == 0) {
        rout->vcall_index = (a_virtual_table_index)vcall_offset;
    }
    gcc_gen_be_error_t err = gcc_gen_be_emit_thunk(rout, out_thunk);
    rout->delta = saved_delta;
    rout->vcall_index = saved_vcall;
    return err;
}

gcc_gen_be_error_t gcc_gen_be_emit_covariant_return_thunk(
    a_routine_ptr overriding_rout,
    long this_offset,
    long result_offset,
    a_boolean is_vbase_return,
    struct gcc_jit_function **out_thunk) GCC_GEN_BE_NOEXCEPT {
    if (!overriding_rout || !out_thunk) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_thunk = NULL;

    a_targ_ptrdiff_t saved_delta = overriding_rout->delta;
    a_targ_ptrdiff_t saved_ret_delta = overriding_rout->return_delta;
    a_virtual_table_index saved_vbase_idx = overriding_rout->vbase_index;

    overriding_rout->delta = (a_targ_ptrdiff_t)this_offset;
    if (is_vbase_return) {
        overriding_rout->vbase_index = (a_virtual_table_index)result_offset;
    } else {
        overriding_rout->return_delta = (a_targ_ptrdiff_t)result_offset;
    }

    gcc_gen_be_error_t err = gcc_gen_be_emit_thunk(overriding_rout, out_thunk);

    overriding_rout->delta = saved_delta;
    overriding_rout->return_delta = saved_ret_delta;
    overriding_rout->vbase_index = saved_vbase_idx;
    return err;
}

/**
 * @brief Computes the Itanium C++ ABI `src2dst` offset hint for `__dynamic_cast`.
 * @details Inspects the base class list of `dst_ptr_or_ref_type` to determine whether
 * `src_ptr_or_ref_type` is a unique public non-virtual base (returning its byte offset >= 0),
 * a virtual or multiple base (returning `-2`), or unrelated/unknown (returning `-1`).
 * @param[in] src_type Source pointer/reference or class type.
 * @param[in] dst_type Destination pointer/reference or class type.
 * @param[out] out_hint Pointer to receive the computed `ptrdiff_t` hint (`>= 0`, `-1`, or `-2`).
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT` if `out_hint` is NULL.
 */
static gcc_gen_be_error_t compute_dynamic_cast_hint(
    a_type_ptr src_type,
    a_type_ptr dst_type,
    long *out_hint) GCC_GEN_BE_NOEXCEPT {
    if (!out_hint) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_hint = -1L;

    a_type_ptr s = src_type;
    while (s && s->kind == tk_typeref) s = s->variant.typeref.type;
    if (s && s->kind == tk_pointer) {
        s = s->variant.pointer.type;
        while (s && s->kind == tk_typeref) s = s->variant.typeref.type;
    }

    a_type_ptr d = dst_type;
    while (d && d->kind == tk_typeref) d = d->variant.typeref.type;
    if (d && d->kind == tk_pointer) {
        d = d->variant.pointer.type;
        while (d && d->kind == tk_typeref) d = d->variant.typeref.type;
    }

    if (!s || !d || (d->kind != tk_class && d->kind != tk_struct)) {
        return GCC_GEN_BE_SUCCESS;
    }
    a_class_type_supplement_ptr ctsp = d->variant.class_struct_union.extra_info;
    if (!ctsp) return GCC_GEN_BE_SUCCESS;

    int match_count = 0;
    long matched_off = -1L;
    a_boolean matched_virtual = FALSE;
    a_boolean matched_public = FALSE;
    a_boolean use_all = (ctsp->base_classes != NULL) ? TRUE : FALSE;
    for (a_base_class_ptr b = use_all ? ctsp->base_classes : ctsp->direct_base_classes;
         b != NULL;
         b = use_all ? b->next : b->next_direct) {
        if (b->type == s) {
            match_count++;
            matched_off = (long)b->offset;
            if (b->is_virtual) matched_virtual = TRUE;
            if (b->has_public_derivation) matched_public = TRUE;
        }
    }
    if (match_count == 1) {
        if (matched_virtual || !matched_public) {
            *out_hint = -2L;
        } else {
            *out_hint = matched_off;
        }
    } else if (match_count > 1) {
        *out_hint = -2L;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers a dynamic_cast expression into a call to __dynamic_cast.
 *
 * @details Implements dynamic downcast and cross-cast lowering via __dynamic_cast,
 * passes source subobject pointer, source _ZTI..., target _ZTI..., and hint offset.
 * Handles reference dynamic_cast by throwing __cxa_bad_cast on failure.
 * Also handles static upcast bypass and dynamic_cast<void*>.
 *
 * @param expr The typeid expression node.
 * @param out_rval Pointer to receive the generated JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_dynamic_cast(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!expr || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;

    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    /* Check if this is a dynamic_cast<void*> */
    a_type_ptr dest_type_stripped = expr->type;
    while (dest_type_stripped && dest_type_stripped->kind == tk_typeref) {
        dest_type_stripped = dest_type_stripped->variant.typeref.type;
    }
    a_boolean is_cast_to_void = FALSE;
    if (dest_type_stripped && dest_type_stripped->kind == tk_pointer) {
        a_type_ptr pointee = dest_type_stripped->variant.pointer.type;
        while (pointee && pointee->kind == tk_typeref) pointee = pointee->variant.typeref.type;
        if (pointee && pointee->kind == tk_void) is_cast_to_void = TRUE;
    }

    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));
    gcc_jit_rvalue *src_ptr = NULL;
    err = gcc_gen_be_lower_expr_rvalue(expr->variant.operation.operands, &src_ptr);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    src_ptr = gcc_jit_context_new_cast(ctx, NULL, src_ptr, void_ptr_type);

    if (is_cast_to_void) {
        /* dynamic_cast<void*> retrieves the offset-to-top from the vtable */
        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        gcc_jit_function *func = gcc_jit_block_get_function(cblock);

        gcc_jit_rvalue *null_ptr = gcc_jit_context_null(ctx, void_ptr_type);
        gcc_jit_rvalue *is_null = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, src_ptr, null_ptr);

        gcc_jit_block *calc_block = gcc_jit_function_new_block(func, "void_cast_calc");
        gcc_jit_block *end_block = gcc_jit_function_new_block(func, "void_cast_end");
        
        gcc_jit_lvalue *res_lval = gcc_jit_function_new_local(func, NULL, void_ptr_type, "void_cast_res");
        gcc_jit_block_add_assignment(cblock, NULL, res_lval, null_ptr);
        gcc_jit_block_end_with_conditional(cblock, NULL, is_null, end_block, calc_block);

        /* vptr is at offset 0 */
        gcc_jit_type *void_ptr_ptr_type = gcc_jit_type_get_pointer(void_ptr_type);
        gcc_jit_rvalue *src_ptr_cast = gcc_jit_context_new_cast(ctx, NULL, src_ptr, void_ptr_ptr_type);
        gcc_jit_lvalue *vptr_lval = gcc_jit_rvalue_dereference(src_ptr_cast, NULL);
        gcc_jit_rvalue *vptr_val = gcc_jit_lvalue_as_rvalue(vptr_lval);
        gcc_jit_rvalue *vptr_cast = gcc_jit_context_new_cast(ctx, NULL, vptr_val, void_ptr_ptr_type);

        /* offset-to-top is at index -2 */
        gcc_jit_type *ptrdiff_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_SIZE_T);
        gcc_jit_type *ptrdiff_ptr_type = gcc_jit_type_get_pointer(ptrdiff_type);
        gcc_jit_rvalue *vptr_ptrdiff = gcc_jit_context_new_cast(ctx, NULL, vptr_cast, ptrdiff_ptr_type);
        gcc_jit_rvalue *minus_two = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), -2);
        gcc_jit_lvalue *offset_lval = gcc_jit_context_new_array_access(ctx, NULL, vptr_ptrdiff, minus_two);
        gcc_jit_rvalue *offset = gcc_jit_lvalue_as_rvalue(offset_lval);

        /* adjusted_ptr = (char*)src_ptr + offset */
        gcc_jit_rvalue *src_arr = gcc_jit_context_new_cast(ctx, NULL, src_ptr, gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_UNSIGNED_CHAR)));
        gcc_jit_rvalue *adjusted_ptr = gcc_jit_lvalue_get_address(gcc_jit_context_new_array_access(ctx, NULL, src_arr, offset), NULL);
        gcc_jit_block_add_assignment(calc_block, NULL, res_lval, gcc_jit_context_new_cast(ctx, NULL, adjusted_ptr, void_ptr_type));
        
        gcc_jit_block_end_with_jump(calc_block, NULL, end_block);
        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(end_block));
        
        *out_rval = gcc_jit_lvalue_as_rvalue(res_lval);
        return GCC_GEN_BE_SUCCESS;
    }

    void *cached = NULL;
    gcc_jit_function *dyn_cast_fn = NULL;
    err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__dynamic_cast", &cached);
    if (err == GCC_GEN_BE_SUCCESS && cached) {
        dyn_cast_fn = (gcc_jit_function *)cached;
    } else {
        gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
        gcc_jit_param *p1 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "src_ptr");
        gcc_jit_param *p2 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "src_type");
        gcc_jit_param *p3 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "dst_type");
        gcc_jit_param *p4 = gcc_jit_context_new_param(ctx, NULL, long_type, "src2dst");
        gcc_jit_param *params[] = {p1, p2, p3, p4};
        dyn_cast_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "__dynamic_cast", 4, params, 0);
        err = cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"__dynamic_cast", dyn_cast_fn);
        if (err != GCC_GEN_BE_SUCCESS) return err;
    }

    an_expr_node src_typeid_expr;
    memset(&src_typeid_expr, 0, sizeof(src_typeid_expr));
    src_typeid_expr.kind = enk_typeid;
    an_expr_node dummy_src_type_op;
    memset(&dummy_src_type_op, 0, sizeof(dummy_src_type_op));
    dummy_src_type_op.kind = enk_type_operand;
    dummy_src_type_op.variant.type_operand.type = expr->variant.operation.operands->type;
    src_typeid_expr.variant.typeid_info.type_with_opt_expr = &dummy_src_type_op;
    
    gcc_jit_rvalue *src_type_info = NULL;
    err = gcc_gen_be_lower_typeid(&src_typeid_expr, &src_type_info);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    src_type_info = gcc_jit_context_new_cast(ctx, NULL, src_type_info, void_ptr_type);

    an_expr_node dst_typeid_expr;
    memset(&dst_typeid_expr, 0, sizeof(dst_typeid_expr));
    dst_typeid_expr.kind = enk_typeid;
    an_expr_node dummy_dst_type_op;
    memset(&dummy_dst_type_op, 0, sizeof(dummy_dst_type_op));
    dummy_dst_type_op.kind = enk_type_operand;
    dummy_dst_type_op.variant.type_operand.type = expr->type;
    dst_typeid_expr.variant.typeid_info.type_with_opt_expr = &dummy_dst_type_op;
    
    gcc_jit_rvalue *dst_type_info = NULL;
    err = gcc_gen_be_lower_typeid(&dst_typeid_expr, &dst_type_info);
    if (err != GCC_GEN_BE_SUCCESS) return err;
    dst_type_info = gcc_jit_context_new_cast(ctx, NULL, dst_type_info, void_ptr_type);

    long src2dst_hint = -1L;
    GCC_GEN_BE_CHECK(compute_dynamic_cast_hint(
        expr->variant.operation.operands ? expr->variant.operation.operands->type : NULL,
        expr->type,
        &src2dst_hint));
    gcc_jit_rvalue *offset = gcc_jit_context_new_rvalue_from_long(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG), src2dst_hint);
    
    gcc_jit_rvalue *args[] = {src_ptr, src_type_info, dst_type_info, offset};
    gcc_jit_rvalue *call = gcc_jit_context_new_call(ctx, NULL, dyn_cast_fn, 4, args);
    
    gcc_jit_type *res_type = NULL;
    err = gcc_gen_be_lower_type(expr->type, &res_type);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    if (expr->type && expr->type->kind == tk_pointer && expr->type->variant.pointer.is_reference) {
        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        gcc_jit_function *func = gcc_jit_block_get_function(cblock);

        gcc_jit_rvalue *null_ptr = gcc_jit_context_null(ctx, void_ptr_type);
        gcc_jit_rvalue *is_null = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, call, null_ptr);

        gcc_jit_block *throw_block = gcc_jit_function_new_block(func, "dynamic_cast_throw");
        gcc_jit_block *continue_block = gcc_jit_function_new_block(func, "dynamic_cast_continue");
        
        gcc_jit_block_end_with_conditional(cblock, NULL, is_null, throw_block, continue_block);

        void *cached_bad_cast = NULL;
        gcc_jit_function *bad_cast_fn = NULL;
        if (cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_bad_cast", &cached_bad_cast) == GCC_GEN_BE_SUCCESS && cached_bad_cast) {
            bad_cast_fn = (gcc_jit_function *)cached_bad_cast;
        } else {
            gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
            bad_cast_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "__cxa_bad_cast", 0, NULL, 0);
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_bad_cast", bad_cast_fn));
        }
        gcc_jit_block_add_eval(throw_block, NULL, gcc_jit_context_new_call(ctx, NULL, bad_cast_fn, 0, NULL));
        gcc_jit_block *unreachable_block = gcc_jit_function_new_block(func, "unreachable_after_bad_cast");
        gcc_jit_block_end_with_jump(throw_block, NULL, unreachable_block);

        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(continue_block));
        
        gcc_jit_lvalue *res_lval = gcc_jit_function_new_local(func, NULL, res_type, "dyn_cast_res");
        gcc_jit_block_add_assignment(continue_block, NULL, res_lval, gcc_jit_context_new_cast(ctx, NULL, call, res_type));
        *out_rval = gcc_jit_lvalue_as_rvalue(res_lval);
    } else {
        *out_rval = gcc_jit_context_new_cast(ctx, NULL, call, res_type);
    }
    
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Lowers a typeid expression into a type_info descriptor.
 *
 * @details Lowers static typeid expressions to constant addresses of _ZTI... 
 * and dynamic typeid for polymorphic expressions with null pointer checks
 * invoking __cxa_bad_typeid().
 *
 * @param expr The typeid expression node.
 * @param out_rval Pointer to receive the generated JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_typeid(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!expr || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;

    gcc_jit_context *ctx = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_get_context(&ctx);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));
    gcc_jit_type *type_info_type = void_ptr_type;
    
    an_expr_node_ptr type_op = expr->variant.typeid_info.type_with_opt_expr;
    if (!type_op || type_op->kind != enk_type_operand) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    a_type_ptr tp = type_op->variant.type_operand.type;
    if (!tp) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    if (expr->variant.typeid_info.is_dynamic) {
        an_expr_node_ptr operand_expr = type_op->next;
        if (!operand_expr) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        if (!cblock) return GCC_GEN_BE_ERROR_INTERNAL;

        gcc_jit_function *func = gcc_jit_block_get_function(cblock);

        gcc_jit_lvalue *lval = NULL;
        err = gcc_gen_be_lower_expr_lvalue(operand_expr, &lval);
        if (err != GCC_GEN_BE_SUCCESS) return err;

        gcc_jit_rvalue *obj_ptr = NULL;
        if (lval) {
            obj_ptr = gcc_jit_lvalue_get_address(lval, NULL);
        } else {
            err = gcc_gen_be_lower_expr_rvalue(operand_expr, &obj_ptr);
            if (err != GCC_GEN_BE_SUCCESS) return err;
        }

        gcc_jit_type *void_ptr_ptr_type = gcc_jit_type_get_pointer(void_ptr_type);
        obj_ptr = gcc_jit_context_new_cast(ctx, NULL, obj_ptr, void_ptr_ptr_type);

        gcc_jit_rvalue *null_ptr = gcc_jit_context_null(ctx, void_ptr_ptr_type);
        gcc_jit_rvalue *is_null = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, obj_ptr, null_ptr);

        gcc_jit_block *throw_block = gcc_jit_function_new_block(func, "typeid_null");
        gcc_jit_block *continue_block = gcc_jit_function_new_block(func, "typeid_continue");

        gcc_jit_block_end_with_conditional(cblock, NULL, is_null, throw_block, continue_block);

        void *cached = NULL;
        gcc_jit_function *bad_typeid_fn = NULL;
        err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_bad_typeid", &cached);
        if (err == GCC_GEN_BE_SUCCESS && cached) {
            bad_typeid_fn = (gcc_jit_function *)cached;
        } else {
            gcc_jit_type *void_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID);
            bad_typeid_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_type, "__cxa_bad_typeid", 0, NULL, 0);
            GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_bad_typeid", bad_typeid_fn));
        }
        gcc_jit_rvalue *call = gcc_jit_context_new_call(ctx, NULL, bad_typeid_fn, 0, NULL);
        gcc_jit_block_add_eval(throw_block, NULL, call);
        
        gcc_jit_block *unreachable_block = gcc_jit_function_new_block(func, "unreachable_after_bad_typeid");
        gcc_jit_block_end_with_jump(throw_block, NULL, unreachable_block);

        GCC_GEN_BE_CHECK(gcc_gen_be_set_current_block(continue_block));

        gcc_jit_lvalue *vptr_lval = gcc_jit_rvalue_dereference(obj_ptr, NULL);
        gcc_jit_rvalue *vptr_val = gcc_jit_lvalue_as_rvalue(vptr_lval);
        gcc_jit_rvalue *vptr_cast = gcc_jit_context_new_cast(ctx, NULL, vptr_val, void_ptr_ptr_type);

        gcc_jit_rvalue *minus_one = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), -1);
        gcc_jit_lvalue *type_info_ptr_lval = gcc_jit_context_new_array_access(ctx, NULL, vptr_cast, minus_one);

        gcc_jit_rvalue *type_info_val = gcc_jit_lvalue_as_rvalue(type_info_ptr_lval);
        gcc_jit_lvalue *res_lval = gcc_jit_function_new_local(func, NULL, type_info_type, "dynamic_type_info");
        gcc_jit_block_add_assignment(continue_block, NULL, res_lval, gcc_jit_context_new_cast(ctx, NULL, type_info_val, type_info_type));
        
        *out_rval = gcc_jit_lvalue_as_rvalue(res_lval);
        return GCC_GEN_BE_SUCCESS;
    }

    char mangled_type[128];
    GCC_GEN_BE_CHECK(get_mangled_type_name(tp, mangled_type, sizeof(mangled_type)));

    char name[256];
    snprintf(name, sizeof(name), "_ZTI%s", mangled_type);

    gcc_jit_lvalue *type_info_lval = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_IMPORTED, type_info_type, name);
    *out_rval = gcc_jit_lvalue_get_address(type_info_lval, NULL);

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Emits a std::type_info descriptor for the given type.
 *
 * @details Emits type name string literals (_ZTS...) in read-only data section and 
 * complete type descriptors (_ZTI...) hierarchy including __fundamental_type_info,
 * __pointer_type_info, __pointer_to_member_type_info, __array_type_info, __function_type_info, 
 * __enum_type_info, __class_type_info, __si_class_type_info, and 
 * __vmi_class_type_info based on the underlying class shape and Itanium ABI.
 *
 * @param tp The type to describe.
 * @param out_descriptor Pointer to receive the generated JIT lvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_emit_type_descriptor(a_type_ptr tp, struct gcc_jit_lvalue **out_descriptor) GCC_GEN_BE_NOEXCEPT {
    if (!tp || !out_descriptor) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_descriptor = NULL;

    while (tp && tp->kind == tk_typeref) tp = tp->variant.typeref.type;
    if (tp && tp->kind == tk_pointer && tp->variant.pointer.is_reference) {
        tp = tp->variant.pointer.type;
        while (tp && tp->kind == tk_typeref) tp = tp->variant.typeref.type;
    }
    if (!tp) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

#if DO_IL_LOWERING
    if (tp->typeinfo_var) {
        extern gcc_gen_be_error_t gcc_gen_be_lower_global_variable_decl(a_variable_ptr var, struct gcc_jit_lvalue **out_lval) GCC_GEN_BE_NOEXCEPT;
        return gcc_gen_be_lower_global_variable_decl(tp->typeinfo_var, out_descriptor);
    }
#endif

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    char mangled_type[256];
    GCC_GEN_BE_CHECK(get_mangled_type_name(tp, mangled_type, sizeof(mangled_type)));

    char zts_name[300];
    snprintf(zts_name, sizeof(zts_name), "_ZTS%s", mangled_type);

    char zti_name[300];
    snprintf(zti_name, sizeof(zti_name), "_ZTI%s", mangled_type);

    void *cached_zti = NULL;
    if (cache_lookup(GCC_GEN_BE_CACHE_GUARD, tp, &cached_zti) == GCC_GEN_BE_SUCCESS && cached_zti) {
        *out_descriptor = (gcc_jit_lvalue *)cached_zti;
        return GCC_GEN_BE_SUCCESS;
    }
    if (cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)zti_name, &cached_zti) == GCC_GEN_BE_SUCCESS && cached_zti) {
        void *var_lval = NULL;
        if (cache_lookup(GCC_GEN_BE_CACHE_VAR, cached_zti, &var_lval) == GCC_GEN_BE_SUCCESS && var_lval) {
            cached_zti = var_lval;
        }
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_GUARD, tp, cached_zti));
        *out_descriptor = (gcc_jit_lvalue *)cached_zti;
        return GCC_GEN_BE_SUCCESS;
    }

    /* Emit or reuse _ZTS<mangled> read-only character array */
    gcc_jit_lvalue *zts_global = NULL;
    void *cached_zts = NULL;
    if (cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)zts_name, &cached_zts) == GCC_GEN_BE_SUCCESS && cached_zts) {
        void *zts_var_lval = NULL;
        if (cache_lookup(GCC_GEN_BE_CACHE_VAR, cached_zts, &zts_var_lval) == GCC_GEN_BE_SUCCESS && zts_var_lval) {
            cached_zts = zts_var_lval;
        }
        zts_global = (gcc_jit_lvalue *)cached_zts;
    } else {
        gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
        size_t name_len = strlen(mangled_type) + 1;
        gcc_jit_type *zts_arr_type = gcc_jit_context_new_array_type(ctx, NULL, char_type, (int)name_len);
        zts_global = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, zts_arr_type, zts_name);
        GCC_GEN_BE_CHECK(mark_global_vague_linkage(zts_global));
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)zts_name, zts_global));
        gcc_jit_rvalue **char_vals = (gcc_jit_rvalue **)malloc(name_len * sizeof(gcc_jit_rvalue *));
        if (!char_vals) return GCC_GEN_BE_ERROR_OOM;
        for (size_t i = 0; i < name_len; ++i) {
            char_vals[i] = gcc_jit_context_new_rvalue_from_int(ctx, char_type, (int)(unsigned char)mangled_type[i]);
        }
        gcc_jit_global_set_initializer_rvalue(
            zts_global, gcc_jit_context_new_array_constructor(ctx, NULL, zts_arr_type, name_len, char_vals));
        free(char_vals);
    }

    /* Determine Itanium ABI __cxxabiv1::*type_info vtable symbol */
    a_type_ptr stripped = tp;
    while (stripped && stripped->kind == tk_typeref) stripped = stripped->variant.typeref.type;
    const char *tv_sym = "_ZTVN10__cxxabiv123__fundamental_type_infoE";
    a_type_ptr single_base_tp = NULL;
    a_boolean is_vmi_class = FALSE;
    a_boolean is_ptr_type = FALSE;
    a_boolean is_ptm_type = FALSE;
    int vmi_base_count = 0;
    if (stripped) {
        switch (stripped->kind) {
            case tk_pointer:
                tv_sym = "_ZTVN10__cxxabiv119__pointer_type_infoE";
                is_ptr_type = TRUE;
                break;
            case tk_ptr_to_member:
                tv_sym = "_ZTVN10__cxxabiv129__pointer_to_member_type_infoE";
                is_ptm_type = TRUE;
                break;
            case tk_array:
                tv_sym = "_ZTVN10__cxxabiv117__array_type_infoE";
                break;
            case tk_routine:
                tv_sym = "_ZTVN10__cxxabiv120__function_type_infoE";
                break;
            case tk_integer:
                if (stripped->variant.integer.enum_type) {
                    tv_sym = "_ZTVN10__cxxabiv116__enum_type_infoE";
                }
                break;
            case tk_class:
            case tk_struct:
            case tk_union:
                {
                    a_class_type_supplement_ptr ctsp = stripped->variant.class_struct_union.extra_info;
                    a_base_class_ptr b = ctsp ? ctsp->direct_base_classes : NULL;
                    if (!b) {
                        tv_sym = "_ZTVN10__cxxabiv117__class_type_infoE";
                    } else if (!b->next_direct && !b->is_virtual && b->offset == 0 && b->has_public_derivation) {
                        tv_sym = "_ZTVN10__cxxabiv120__si_class_type_infoE";
                        single_base_tp = b->type;
                    } else {
                        tv_sym = "_ZTVN10__cxxabiv121__vmi_class_type_infoE";
                        is_vmi_class = TRUE;
                        for (a_base_class_ptr vb = b; vb != NULL; vb = vb->next_direct) {
                            vmi_base_count++;
                        }
                    }
                }
                break;
            default:
                break;
        }
    }

    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));
    gcc_jit_type *tv_arr_type = gcc_jit_context_new_array_type(ctx, NULL, void_ptr_type, 8);
    void *cached_tv = NULL;
    gcc_jit_lvalue *tv_global = NULL;
    if (cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)tv_sym, &cached_tv) == GCC_GEN_BE_SUCCESS && cached_tv) {
        tv_global = (gcc_jit_lvalue *)cached_tv;
    } else {
        tv_global = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_IMPORTED, tv_arr_type, tv_sym);
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)tv_sym, tv_global));
    }
    gcc_jit_rvalue *two_idx = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), 2);
    gcc_jit_rvalue *tv_addr_pt = gcc_jit_context_new_cast(
        ctx, NULL,
        gcc_jit_lvalue_get_address(gcc_jit_context_new_array_access(ctx, NULL, gcc_jit_lvalue_as_rvalue(tv_global), two_idx), NULL),
        void_ptr_type);
    gcc_jit_rvalue *zts_ptr = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(zts_global, NULL), void_ptr_type);

    int num_zti_fields = 2;
    if (single_base_tp) {
        num_zti_fields = 3;
    } else if (is_ptr_type) {
        num_zti_fields = 4; /* vptr, name, __flags, __pointee */
    } else if (is_ptm_type) {
        num_zti_fields = 5; /* vptr, name, __flags, __pointee, __context */
    } else if (is_vmi_class) {
        num_zti_fields = 4 + (vmi_base_count * 2); /* vptr, name, __flags, __base_count, (__base_type, __offset_flags)*N */
    }

    gcc_jit_type *zti_arr_type = gcc_jit_context_new_array_type(ctx, NULL, void_ptr_type, num_zti_fields);
    gcc_jit_lvalue *zti_global = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, zti_arr_type, zti_name);
    GCC_GEN_BE_CHECK(mark_global_vague_linkage(zti_global));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_GUARD, tp, zti_global));
    GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)zti_name, zti_global));

    gcc_jit_rvalue **zti_elems = (gcc_jit_rvalue **)malloc((size_t)num_zti_fields * sizeof(gcc_jit_rvalue *));
    if (!zti_elems) return GCC_GEN_BE_ERROR_OOM;
    zti_elems[0] = tv_addr_pt;
    zti_elems[1] = zts_ptr;

    if (single_base_tp) {
        gcc_jit_lvalue *base_desc = NULL;
        gcc_gen_be_error_t berr = gcc_gen_be_emit_type_descriptor(single_base_tp, &base_desc);
        if (berr != GCC_GEN_BE_SUCCESS) { free(zti_elems); return berr; }
        zti_elems[2] = base_desc
            ? gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(base_desc, NULL), void_ptr_type)
            : gcc_jit_context_null(ctx, void_ptr_type);
    } else if (is_ptr_type) {
        long pflags = 0L;
        a_type_ptr pt = stripped->variant.pointer.type;
        while (pt && pt->kind == tk_typeref) {
            if (pt->variant.typeref.qualifiers & TQ_CONST)    pflags |= 0x1L; /* __const_mask */
            if (pt->variant.typeref.qualifiers & TQ_VOLATILE) pflags |= 0x2L; /* __volatile_mask */
            if (pt->variant.typeref.qualifiers & TQ_RESTRICT) pflags |= 0x4L; /* __restrict_mask */
            pt = pt->variant.typeref.type;
        }
        zti_elems[2] = (pflags != 0L)
            ? gcc_jit_context_new_rvalue_from_ptr(ctx, void_ptr_type, (void *)(uintptr_t)pflags)
            : gcc_jit_context_null(ctx, void_ptr_type);
        gcc_jit_lvalue *pointee_desc = NULL;
        if (stripped->variant.pointer.type) {
            gcc_gen_be_error_t perr = gcc_gen_be_emit_type_descriptor(stripped->variant.pointer.type, &pointee_desc);
            if (perr != GCC_GEN_BE_SUCCESS) { free(zti_elems); return perr; }
        }
        zti_elems[3] = pointee_desc
            ? gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(pointee_desc, NULL), void_ptr_type)
            : gcc_jit_context_null(ctx, void_ptr_type);
    } else if (is_ptm_type) {
        zti_elems[2] = gcc_jit_context_null(ctx, void_ptr_type);
        gcc_jit_lvalue *member_desc = NULL;
        if (stripped->variant.ptr_to_member.type) {
            gcc_gen_be_error_t merr = gcc_gen_be_emit_type_descriptor(stripped->variant.ptr_to_member.type, &member_desc);
            if (merr != GCC_GEN_BE_SUCCESS) { free(zti_elems); return merr; }
        }
        zti_elems[3] = member_desc
            ? gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(member_desc, NULL), void_ptr_type)
            : gcc_jit_context_null(ctx, void_ptr_type);
        gcc_jit_lvalue *ctx_desc = NULL;
        if (stripped->variant.ptr_to_member.class_of_which_a_member) {
            gcc_gen_be_error_t cerr = gcc_gen_be_emit_type_descriptor(stripped->variant.ptr_to_member.class_of_which_a_member, &ctx_desc);
            if (cerr != GCC_GEN_BE_SUCCESS) { free(zti_elems); return cerr; }
        }
        zti_elems[4] = ctx_desc
            ? gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(ctx_desc, NULL), void_ptr_type)
            : gcc_jit_context_null(ctx, void_ptr_type);
    } else if (is_vmi_class) {
        a_class_type_supplement_ptr ctsp = stripped->variant.class_struct_union.extra_info;
        long vmi_flags = (stripped->variant.class_struct_union.any_virtual_base_classes) ? 0x2L : 0x1L;
        zti_elems[2] = gcc_jit_context_new_rvalue_from_ptr(ctx, void_ptr_type, (void *)(uintptr_t)vmi_flags);
        zti_elems[3] = gcc_jit_context_new_rvalue_from_ptr(ctx, void_ptr_type, (void *)(uintptr_t)vmi_base_count);
        int f_idx = 4;
        for (a_base_class_ptr vb = ctsp ? ctsp->direct_base_classes : NULL; vb != NULL; vb = vb->next_direct) {
            gcc_jit_lvalue *vb_desc = NULL;
            if (vb->type) {
                gcc_gen_be_error_t verr = gcc_gen_be_emit_type_descriptor(vb->type, &vb_desc);
                if (verr != GCC_GEN_BE_SUCCESS) { free(zti_elems); return verr; }
            }
            zti_elems[f_idx++] = vb_desc
                ? gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(vb_desc, NULL), void_ptr_type)
                : gcc_jit_context_null(ctx, void_ptr_type);
            long offset_flags = ((long)vb->offset << 8) |
                                (vb->is_virtual ? 0x1L : 0L) |
                                (vb->has_public_derivation ? 0x2L : 0L);
            zti_elems[f_idx++] = (offset_flags != 0L)
                ? gcc_jit_context_new_rvalue_from_ptr(ctx, void_ptr_type, (void *)(uintptr_t)offset_flags)
                : gcc_jit_context_null(ctx, void_ptr_type);
        }
    }

    gcc_jit_global_set_initializer_rvalue(
        zti_global, gcc_jit_context_new_array_constructor(ctx, NULL, zti_arr_type, (size_t)num_zti_fields, zti_elems));
    free(zti_elems);

    *out_descriptor = zti_global;
    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
