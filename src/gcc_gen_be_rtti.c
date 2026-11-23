/**
 * @file gcc_gen_be_rtti.c
 * @brief Implementation of Itanium C++ ABI RTTI and VTable generation.
 * @details Implements Itanium C++ ABI VTable layout (`_ZTV`), construction vtables (`_ZTC`),
 * virtual table tables (`_ZTT`), `this`-adjusting and covariant-return thunks, RTTI descriptors
 * (`_ZTI`/`_ZTS`), `dynamic_cast` / `typeid` lowering, and canonical/dynamic symbol mangling.
 * @ingroup gcc_gen_be
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
#include "cmd_line.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

/**
 * @brief Dynamically resizing heap string buffer for unbounded Itanium C++ ABI mangling.
 * @details Eliminates fixed 128/256-byte stack buffer limits during nested type/symbol mangling.
 */
typedef struct gcc_gen_be_dynbuf {
    /** @brief Heap-allocated null-terminated character array. */
    char *data;
    /** @brief Active string length excluding null terminator. */
    size_t len;
    /** @brief Total allocated byte capacity of `data`. */
    size_t cap;
} gcc_gen_be_dynbuf_t;

/**
 * @brief Initializes a dynamic mangling buffer with an empty null-terminated heap string.
 * @details Allocates an initial 64-byte heap buffer and zeroes `data[0]`.
 * @param[out] buf Pointer to the `gcc_gen_be_dynbuf_t` to initialize.
 * @return `GCC_GEN_BE_SUCCESS` on success, `GCC_GEN_BE_ERROR_OOM` on allocation failure, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t dynbuf_init(
    gcc_gen_be_dynbuf_t *buf) GCC_GEN_BE_NOEXCEPT {
    buf->cap = 64;
    buf->len = 0;
    buf->data = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_alloc(buf->cap, (void **)&buf->data));
    buf->data[0] = '\0';
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Appends a null-terminated string onto a dynamic mangling buffer, growing heap capacity as needed.
 * @details Doubles capacity via `gcc_gen_be_realloc_buf` until `buf->len + strlen(str) + 1 <= buf->cap`.
 * @param[in,out] buf Non-null initialized dynamic mangling buffer.
 * @param[in] str Non-null null-terminated string to append.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM` on allocation failure.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t dynbuf_append(
    gcc_gen_be_dynbuf_t *buf,
    const char *str) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    size_t slen = strlen(str);
    if (slen == 0) return GCC_GEN_BE_SUCCESS;
    if (buf->len + slen + 1 > buf->cap) {
        size_t new_cap = buf->cap;
        while (new_cap < buf->len + slen + 1) {
            new_cap *= 2;
        }
        void *new_data = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_realloc_buf(buf->data, new_cap, &new_data));
        buf->data = (char *)new_data;
        buf->cap = new_cap;
    }
    memcpy(buf->data + buf->len, str, slen + 1);
    buf->len += slen;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Appends an Itanium C++ ABI `<length><identifier>` source-name pair onto `buf`.
 * @details Formats the decimal length prefix followed by `ident` without fixed identifier buffer limits.
 * @param[in,out] buf Non-null target dynamic mangling buffer.
 * @param[in] prefix Optional literal prefix before the length (may be NULL).
 * @param[in] ident Non-null identifier string whose length and characters are appended.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t dynbuf_append_len_ident(
    gcc_gen_be_dynbuf_t *buf,
    const char *prefix,
    const char *ident) GCC_GEN_BE_NOEXCEPT {
    if (prefix) {
        GCC_GEN_BE_CHECK(dynbuf_append(buf, prefix));
    }
    char num_buf[32];
    snprintf(num_buf, sizeof(num_buf), "%lu", (unsigned long)strlen(ident));
    GCC_GEN_BE_CHECK(dynbuf_append(buf, num_buf));
    return dynbuf_append(buf, ident);
}

/**
 * @brief Copies a finished `gcc_gen_be_dynbuf_t` into a caller-supplied fixed buffer and frees `buf`.
 * @details Returns `GCC_GEN_BE_ERROR_MANGLING_BUFFER_OVERFLOW` if `buf->len >= size`.
 * @param[in,out] buf Non-null source dynamic buffer (always freed before returning).
 * @param[out] buffer Non-null destination caller buffer.
 * @param[in] size Positive capacity of `buffer` in bytes.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_MANGLING_BUFFER_OVERFLOW`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t dynbuf_finish_into(
    gcc_gen_be_dynbuf_t *buf,
    char *buffer,
    size_t size) GCC_GEN_BE_NOEXCEPT {
    if (buf->len >= size) {
        free(buf->data);
        buf->data = NULL;
        return GCC_GEN_BE_ERROR_MANGLING_BUFFER_OVERFLOW;
    }
    memcpy(buffer, buf->data, buf->len + 1);
    free(buf->data);
    buf->data = NULL;
    return GCC_GEN_BE_SUCCESS;
}

/** @brief Context-backed active substitution table for the current mangling invocation. */
#define active_subst_table (gcc_gen_be_singleton_state.active_subst_table)

/**
 * @brief Clears all heap-allocated entries in `active_subst_table` for a fresh mangling pass.
 * @details Frees each stored candidate string while retaining the outer `entries` pointer array for reuse.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t reset_subst_table(void) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (active_subst_table.entries) {
        for (int i = 0; i < active_subst_table.count; ++i) {
            free(active_subst_table.entries[i]);
            active_subst_table.entries[i] = NULL;
        }
    }
    active_subst_table.count = 0;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Formats a 0-based Itanium ABI substitution `seq_id` into base-36 `S_`, `S0_`, ..., `S10_`, etc.
 * @details Supports arbitrarily large `seq_id` values without fixed 64-entry limits.
 * @param[in] index Non-negative zero-based index in `active_subst_table` (`0 -> S_`, `1 -> S0_`, ...).
 * @param[out] out_subst Non-null buffer of 16 bytes receiving the token.
 * @param[in] subst_size Size of `out_subst` in bytes (>= 16).
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t format_subst_seq_token(
    int index,
    char *out_subst,
    size_t subst_size) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (index == 0) {
        snprintf(out_subst, subst_size, "S_");
        return GCC_GEN_BE_SUCCESS;
    }
    const char *digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    unsigned int seq = (unsigned int)(index - 1);
    char rev[16];
    int rlen = 0;
    do {
        rev[rlen++] = digits[seq % 36U];
        seq /= 36U;
    } while (seq > 0U);
    size_t pos = 0;
    out_subst[pos++] = 'S';
    while (rlen > 0) {
        out_subst[pos++] = rev[--rlen];
    }
    out_subst[pos++] = '_';
    out_subst[pos] = '\0';
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Looks up or registers a mangled component in the dynamic Itanium ABI substitution table.
 * @details Searches `active_subst_table.entries`; if present, writes `S..._` into `out_subst`; otherwise
 * dynamically resizes `active_subst_table.entries` and records a heap copy of `candidate`.
 * @param[in] candidate Non-null mangled component string to check.
 * @param[out] out_found Non-null pointer receiving `TRUE` if a substitution matched, `FALSE` otherwise.
 * @param[out] out_subst Non-null 16-byte buffer receiving the `S..._` token when found.
 * @param[in] subst_size Size of `out_subst` (>= 16).
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM` on allocation failure.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t find_or_record_substitution(
    const char *candidate,
    a_boolean *out_found,
    char *out_subst,
    size_t subst_size) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_found = FALSE;
    out_subst[0] = '\0';
    size_t clen = strlen(candidate);
    for (int i = 0; i < active_subst_table.count; ++i) {
        if (strcmp(active_subst_table.entries[i], candidate) == 0) {
            *out_found = TRUE;
            return format_subst_seq_token(i, out_subst, subst_size);
        }
    }
    if (active_subst_table.count >= active_subst_table.capacity) {
        int new_cap = (active_subst_table.capacity == 0) ? 32 : (active_subst_table.capacity * 2);
        void *new_entries = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_realloc_buf(
            active_subst_table.entries, (size_t)new_cap * sizeof(char *), &new_entries));
        active_subst_table.entries = (char **)new_entries;
        active_subst_table.capacity = new_cap;
    }
    char *copy = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_alloc(clen + 1, (void **)&copy));
    memcpy(copy, candidate, clen + 1);
    active_subst_table.entries[active_subst_table.count++] = copy;
    return GCC_GEN_BE_SUCCESS;
}

static gcc_gen_be_error_t mangle_type_inner_dyn(
    a_type_ptr type,
    gcc_gen_be_dynbuf_t *out_buf) GCC_GEN_BE_NOEXCEPT;

/**
 * @brief Recursively mangles enclosing scopes (namespaces, parent classes, and functions) into `buf`.
 * @details Traverses `parent` upward and emits `<len><name>`, standard `St` substitution for `::std`,
 * `12_GLOBAL__N_1` for unnamed namespaces, or `Z<func>E` for function-local scopes in outer-to-inner order.
 * @param[in] scope Non-null parent scope to mangle.
 * @param[in,out] buf Non-null destination dynamic mangling buffer.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t mangle_enclosing_scopes_dyn(
    a_scope_ptr scope,
    gcc_gen_be_dynbuf_t *buf) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (scope->kind == sck_file) return GCC_GEN_BE_SUCCESS;

    if (scope->parent) {
        GCC_GEN_BE_CHECK(mangle_enclosing_scopes_dyn(scope->parent, buf));
    }

    if (scope->kind == sck_namespace && scope->variant.assoc_namespace) {
        a_const_char *ns_name = scope->variant.assoc_namespace->source_corresp.name;
        if (ns_name && strcmp(ns_name, "std") == 0 && (!scope->parent || scope->parent->kind == sck_file)) {
            GCC_GEN_BE_CHECK(dynbuf_append(buf, "St"));
        } else if (ns_name && ns_name[0] != '\0') {
            GCC_GEN_BE_CHECK(dynbuf_append_len_ident(buf, NULL, ns_name));
        } else {
            GCC_GEN_BE_CHECK(dynbuf_append(buf, "12_GLOBAL__N_1"));
        }
    } else if (scope->kind == sck_class_struct_union && scope->variant.assoc_type) {
        a_const_char *cls_name = scope->variant.assoc_type->source_corresp.name;
        if (cls_name && cls_name[0] != '\0') {
            GCC_GEN_BE_CHECK(dynbuf_append_len_ident(buf, NULL, cls_name));
        }
    } else if (scope->kind == sck_function && scope->variant.routine.ptr) {
        a_const_char *fn_name = scope->variant.routine.ptr->source_corresp.name;
        if (fn_name && fn_name[0] != '\0') {
            GCC_GEN_BE_CHECK(dynbuf_append_len_ident(buf, "Z", fn_name));
            GCC_GEN_BE_CHECK(dynbuf_append(buf, "vE"));
        }
    } else if (scope->kind == sck_block && scope->number > 1) {
        char disc[32];
        snprintf(disc, sizeof(disc), "_%lu", (unsigned long)(scope->number - 2));
        GCC_GEN_BE_CHECK(dynbuf_append(buf, disc));
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Mangles an EDG expression node into a dynamic buffer `buf`.
 * @details Encodes constants (`L...E`), `sizeof(T)` (`st...`), unary/binary operators, and `DT...E` fallbacks.
 * @param[in] expr Non-null frontend expression node to mangle.
 * @param[in,out] buf Non-null destination dynamic mangling buffer.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t mangle_expr_dyn(
    an_expr_node_ptr expr,
    gcc_gen_be_dynbuf_t *buf) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (expr->kind == enk_constant && expr->variant.constant.ptr) {
        a_constant_ptr con = expr->variant.constant.ptr;
        gcc_gen_be_dynbuf_t tcode;
        GCC_GEN_BE_CHECK(dynbuf_init(&tcode));
        if (con->type) {
            gcc_gen_be_error_t terr = mangle_type_inner_dyn(con->type, &tcode);
            if (terr != GCC_GEN_BE_SUCCESS) {
                free(tcode.data);
                return terr;
            }
        }
        const char *tc = (tcode.len > 0) ? tcode.data : "i";
        a_boolean ovf = FALSE;
        long val = (con->kind == ck_integer) ? (long)value_of_integer_constant(con, &ovf) : 0L;
        char num_buf[48];
        if (val < 0) {
            snprintf(num_buf, sizeof(num_buf), "n%ldE", -val);
        } else {
            snprintf(num_buf, sizeof(num_buf), "%ldE", val);
        }
        gcc_gen_be_error_t err = dynbuf_append(buf, "L");
        if (err == GCC_GEN_BE_SUCCESS) err = dynbuf_append(buf, tc);
        if (err == GCC_GEN_BE_SUCCESS) err = dynbuf_append(buf, num_buf);
        free(tcode.data);
        return err;
    }
    if (expr->kind == enk_type_operand && expr->variant.type_operand.type) {
        GCC_GEN_BE_CHECK(dynbuf_append(buf, "st"));
        return mangle_type_inner_dyn(expr->variant.type_operand.type, buf);
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
        GCC_GEN_BE_CHECK(dynbuf_append(buf, op_str));
        if (op1) {
            GCC_GEN_BE_CHECK(mangle_expr_dyn(op1, buf));
        }
        if (!is_unary) {
            GCC_GEN_BE_CHECK(mangle_expr_dyn(op2, buf));
        }
        return GCC_GEN_BE_SUCCESS;
    }
    GCC_GEN_BE_CHECK(dynbuf_append(buf, "DT"));
    if (expr->type) {
        GCC_GEN_BE_CHECK(mangle_type_inner_dyn(expr->type, buf));
    } else {
        GCC_GEN_BE_CHECK(dynbuf_append(buf, "i"));
    }
    return dynbuf_append(buf, "E");
}

gcc_gen_be_error_t gcc_gen_be_mangle_expr(
    an_expr_node_ptr expr,
    char *buffer,
    size_t size) GCC_GEN_BE_NOEXCEPT {
    if (!expr || !buffer || size == 0) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    buffer[0] = '\0';
    gcc_gen_be_dynbuf_t dbuf;
    GCC_GEN_BE_CHECK(dynbuf_init(&dbuf));
    gcc_gen_be_error_t err = mangle_expr_dyn(expr, &dbuf);
    if (err != GCC_GEN_BE_SUCCESS) {
        free(dbuf.data);
        return err;
    }
    return dynbuf_finish_into(&dbuf, buffer, size);
}

/**
 * @brief Appends mangled template arguments (`I...E`) onto a dynamic mangling buffer.
 * @details Encodes type template arguments (`tak_type`), integral/enum/bool/float/pointer non-type
 * template arguments (`tak_nontype`), template-template arguments (`tak_template`), and parameter packs (`J...E`).
 * @param[in] targ_list Non-null linked list of template arguments.
 * @param[in,out] buf Non-null destination dynamic mangling buffer.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t mangle_template_args_dyn(
    a_template_arg_ptr targ_list,
    gcc_gen_be_dynbuf_t *buf) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(dynbuf_append(buf, "I"));
    for (a_template_arg_ptr ta = targ_list; ta != NULL; ta = ta->next) {
        if (ta->kind == tak_start_of_pack_expansion) {
            GCC_GEN_BE_CHECK(dynbuf_append(buf, "J"));
            continue;
        }
        if (ta->kind == tak_type && ta->variant.type) {
            GCC_GEN_BE_CHECK(mangle_type_inner_dyn(ta->variant.type, buf));
        } else if (ta->kind == tak_nontype) {
            a_boolean handled_addr = FALSE;
            long val = 0;
            gcc_gen_be_dynbuf_t tcode;
            GCC_GEN_BE_CHECK(dynbuf_init(&tcode));
            if (ta->is_array_bound_of_unknown_type) {
                val = (long)ta->variant.integer_value;
            } else if (ta->variant.constant) {
                a_constant_ptr con = ta->variant.constant;
                if (con->type) {
                    gcc_gen_be_error_t terr = mangle_type_inner_dyn(con->type, &tcode);
                    if (terr != GCC_GEN_BE_SUCCESS) {
                        free(tcode.data);
                        return terr;
                    }
                }
                if (con->kind == ck_integer) {
                    a_boolean ovf = FALSE;
                    val = (long)value_of_integer_constant(con, &ovf);
                } else if (con->kind == ck_address && con->variant.address.kind == abk_routine &&
                           con->variant.address.variant.routine &&
                           con->variant.address.variant.routine->source_corresp.name) {
                    a_const_char *rname = con->variant.address.variant.routine->source_corresp.name;
                    gcc_gen_be_error_t aerr = dynbuf_append_len_ident(buf, "XadL_Z", rname);
                    if (aerr == GCC_GEN_BE_SUCCESS) aerr = dynbuf_append(buf, "vEE");
                    free(tcode.data);
                    GCC_GEN_BE_CHECK(aerr);
                    handled_addr = TRUE;
                } else if (con->kind == ck_address && con->variant.address.kind == abk_variable &&
                           con->variant.address.variant.variable &&
                           con->variant.address.variant.variable->source_corresp.name) {
                    a_const_char *vname = con->variant.address.variant.variable->source_corresp.name;
                    gcc_gen_be_error_t aerr = dynbuf_append_len_ident(buf, "XadL_Z", vname);
                    if (aerr == GCC_GEN_BE_SUCCESS) aerr = dynbuf_append(buf, "EE");
                    free(tcode.data);
                    GCC_GEN_BE_CHECK(aerr);
                    handled_addr = TRUE;
                }
            }
            if (!handled_addr) {
                const char *tc = (tcode.len > 0) ? tcode.data : "i";
                char num_buf[48];
                if (val < 0) {
                    snprintf(num_buf, sizeof(num_buf), "n%ldE", -val);
                } else {
                    snprintf(num_buf, sizeof(num_buf), "%ldE", val);
                }
                gcc_gen_be_error_t lerr = dynbuf_append(buf, "L");
                if (lerr == GCC_GEN_BE_SUCCESS) lerr = dynbuf_append(buf, tc);
                if (lerr == GCC_GEN_BE_SUCCESS) lerr = dynbuf_append(buf, num_buf);
                free(tcode.data);
                GCC_GEN_BE_CHECK(lerr);
            }
        } else if (ta->kind == tak_template && ta->variant.templ.ptr &&
                   ta->variant.templ.ptr->source_corresp.name) {
            a_const_char *tname = ta->variant.templ.ptr->source_corresp.name;
            GCC_GEN_BE_CHECK(dynbuf_append_len_ident(buf, NULL, tname));
        }
        if (ta->is_pack_element && (!ta->next || !ta->next->is_pack_element)) {
            GCC_GEN_BE_CHECK(dynbuf_append(buf, "E"));
        }
    }
    return dynbuf_append(buf, "E");
}

/**
 * @brief Internal recursive helper encoding an EDG type into `out_buf` using `active_subst_table`.
 * @details Encodes types according to the Itanium C++ ABI specification without fixed buffer limits.
 * @param[in] type The frontend type to mangle.
 * @param[in,out] out_buf Non-null destination dynamic mangling buffer.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
static gcc_gen_be_error_t mangle_type_inner_dyn(
    a_type_ptr type,
    gcc_gen_be_dynbuf_t *out_buf) GCC_GEN_BE_NOEXCEPT {
    if (!type) {
        return dynbuf_append(out_buf, "v");
    }

    a_type_qualifier_set quals = TQ_NONE;
    a_type_ptr curr = type;
    while (curr && curr->kind == tk_typeref) {
        if (curr->source_corresp.name && strcmp(curr->source_corresp.name, "nullptr_t") == 0) {
            return dynbuf_append(out_buf, "Dn");
        }
        quals |= curr->variant.typeref.qualifiers;
        curr = curr->variant.typeref.type;
    }
    if (quals & TQ_RESTRICT) GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "r"));
    if (quals & TQ_VOLATILE) GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "V"));
    if (quals & TQ_CONST)    GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "K"));

    if (!curr) {
        return dynbuf_append(out_buf, "v");
    }

    switch (curr->kind) {
        case tk_void:
            GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "v"));
            break;
        case tk_nullptr:
            GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "Dn"));
            break;
        case tk_integer:
            if (curr->variant.integer.bool_type) {
                GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "b"));
            } else if (curr->variant.integer.enum_type) {
                a_scope_ptr pscope = curr->source_corresp.parent_scope;
                a_boolean is_nested = (pscope && pscope->kind != sck_file) ? TRUE : FALSE;
                if (is_nested) {
                    GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "N"));
                    GCC_GEN_BE_CHECK(mangle_enclosing_scopes_dyn(pscope, out_buf));
                }
                a_const_char *ename = curr->source_corresp.name;
                if (ename && ename[0] != '\0') {
                    GCC_GEN_BE_CHECK(dynbuf_append_len_ident(out_buf, NULL, ename));
                } else {
                    GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "4enum"));
                }
                if (is_nested) {
                    GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "E"));
                }
            } else {
                char bp_buf[48];
                const char *icode = "i";
                switch (curr->variant.integer.int_kind) {
                    case ik_char:
                        icode = "c";
                        break;
                    case ik_signed_char:
                        icode = "a";
                        break;
                    case ik_unsigned_char:
                        icode = "h";
                        break;
                    case ik_short:
                        icode = "s";
                        break;
                    case ik_unsigned_short:
                        icode = "t";
                        break;
                    case ik_int:
                        icode = "i";
                        break;
                    case ik_unsigned_int:
                        icode = "j";
                        break;
                    case ik_long:
                        icode = "l";
                        break;
                    case ik_unsigned_long:
                        icode = "m";
                        break;
                    case ik_long_long:
                        icode = "x";
                        break;
                    case ik_unsigned_long_long:
                        icode = "y";
                        break;
                    case ik_bit_precise:
                    case ik_unsigned_bit_precise:
                        {
                            unsigned long bits = (curr->variant.integer.extra_info && curr->variant.integer.extra_info->bit_width > 0)
                                ? (unsigned long)curr->variant.integer.extra_info->bit_width
                                : ((unsigned long)curr->size * 8UL);
                            snprintf(bp_buf, sizeof(bp_buf),
                                     curr->variant.integer.int_kind == ik_unsigned_bit_precise ? "DU%lu_" : "DB%lu_",
                                     bits);
                            icode = bp_buf;
                        }
                        break;
                    default:
#if INT128_EXTENSIONS_ALLOWED
                        if (curr->variant.integer.int_kind == ik_int128) {
                            icode = "n";
                        } else if (curr->variant.integer.int_kind == ik_unsigned_int128) {
                            icode = "o";
                        }
#endif
                        break;
                }
                GCC_GEN_BE_CHECK(dynbuf_append(out_buf, icode));
            }
            break;
        case tk_complex:
            {
                const char *ccode = "Cd";
                switch (curr->variant.float_kind) {
                    case fk_float:
                        ccode = "Cf";
                        break;
                    case fk_double:
                        ccode = "Cd";
                        break;
                    case fk_long_double:
                    case fk_float80:
                        ccode = "Ce";
                        break;
                    case fk_float128:
                    case fk_std_float128:
                        ccode = "Cg";
                        break;
                    case fk_float16:
                    case fk_std_float16:
                        ccode = "CDF16_";
                        break;
                    default:
                        ccode = "Cd";
                        break;
                }
                GCC_GEN_BE_CHECK(dynbuf_append(out_buf, ccode));
            }
            break;
        case tk_float:
            {
#ifdef EDG_GCC_BE_ISOLATED_TEST
                if (curr->variant.float_supplement.is_complex) {
                    GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "C"));
                }
#endif
                const char *fcode = "d";
                switch (curr->variant.float_kind) {
                    case fk_float:
                        fcode = "f";
                        break;
                    case fk_double:
                        fcode = "d";
                        break;
                    case fk_long_double:
                    case fk_float80:
                        fcode = "e";
                        break;
                    case fk_float128:
                    case fk_std_float128:
                        fcode = "g";
                        break;
                    case fk_float16:
                    case fk_std_float16:
                        fcode = "DF16_";
                        break;
                    case fk_std_bfloat16:
                        fcode = "DF16b";
                        break;
                    default:
                        fcode = "d";
                        break;
                }
                GCC_GEN_BE_CHECK(dynbuf_append(out_buf, fcode));
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
                GCC_GEN_BE_CHECK(dynbuf_append(out_buf, vbuf));
                GCC_GEN_BE_CHECK(mangle_type_inner_dyn(elem_tp, out_buf));
            }
            break;
        case tk_pointer:
            if (curr->variant.pointer.is_reference) {
                GCC_GEN_BE_CHECK(dynbuf_append(out_buf, curr->variant.pointer.is_rvalue_reference ? "O" : "R"));
            } else {
                GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "P"));
            }
            GCC_GEN_BE_CHECK(mangle_type_inner_dyn(curr->variant.pointer.type, out_buf));
            break;
        case tk_array:
            {
                char abuf[64];
                snprintf(abuf, sizeof(abuf), "A%lu_", (unsigned long)curr->variant.array.variant.number_of_elements);
                GCC_GEN_BE_CHECK(dynbuf_append(out_buf, abuf));
                GCC_GEN_BE_CHECK(mangle_type_inner_dyn(curr->variant.array.element_type, out_buf));
            }
            break;
        case tk_routine:
            {
                a_routine_type_supplement_ptr rtsp = curr->variant.routine.extra_info;
                if (rtsp) {
                    if (rtsp->qualifiers & TQ_RESTRICT) GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "r"));
                    if (rtsp->qualifiers & TQ_VOLATILE) GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "V"));
                    if (rtsp->qualifiers & TQ_CONST)    GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "K"));
                    if (rtsp->ref_qualifiers != rqk_default) {
                        GCC_GEN_BE_CHECK(dynbuf_append(out_buf, rtsp->ref_qualifiers == rqk_rvalue ? "O" : "R"));
                    }
                    if (rtsp->exception_specification &&
                        rtsp->exception_specification->is_noexcept &&
                        !rtsp->exception_specification->throw_any) {
                        GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "Do"));
                    }
                }
                GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "F"));
                GCC_GEN_BE_CHECK(mangle_type_inner_dyn(curr->variant.routine.return_type, out_buf));
                a_param_type_ptr ptp = rtsp ? rtsp->param_type_list : NULL;
                if (!ptp) {
                    GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "v"));
                } else {
                    for (; ptp != NULL; ptp = ptp->next) {
                        GCC_GEN_BE_CHECK(mangle_type_inner_dyn(ptp->type, out_buf));
                    }
                    if (rtsp->has_ellipsis) {
                        GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "z"));
                    }
                }
                GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "E"));
            }
            break;
        case tk_ptr_to_member:
            GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "M"));
            GCC_GEN_BE_CHECK(mangle_type_inner_dyn(curr->variant.ptr_to_member.class_of_which_a_member, out_buf));
            GCC_GEN_BE_CHECK(mangle_type_inner_dyn(curr->variant.ptr_to_member.type, out_buf));
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

                if (pscope && pscope->kind == sck_namespace && pscope->variant.assoc_namespace &&
                    pscope->variant.assoc_namespace->source_corresp.name &&
                    strcmp(pscope->variant.assoc_namespace->source_corresp.name, "std") == 0 &&
                    (!pscope->parent || pscope->parent->kind == sck_file) && cname && !targs) {
                    if (strcmp(cname, "allocator") == 0) {
                        GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "Sa"));
                        break;
                    }
                    if (strcmp(cname, "basic_string") == 0) {
                        GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "Sb"));
                        break;
                    }
                    if (strcmp(cname, "string") == 0) {
                        GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "Ss"));
                        break;
                    }
                    if (strcmp(cname, "istream") == 0) {
                        GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "Si"));
                        break;
                    }
                    if (strcmp(cname, "ostream") == 0) {
                        GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "So"));
                        break;
                    }
                    if (strcmp(cname, "iostream") == 0) {
                        GCC_GEN_BE_CHECK(dynbuf_append(out_buf, "Sd"));
                        break;
                    }
                }

                gcc_gen_be_dynbuf_t cls_buf;
                GCC_GEN_BE_CHECK(dynbuf_init(&cls_buf));
                gcc_gen_be_error_t cerr = GCC_GEN_BE_SUCCESS;
                if (is_nested) {
                    cerr = dynbuf_append(&cls_buf, "N");
                    if (cerr == GCC_GEN_BE_SUCCESS) {
                        cerr = mangle_enclosing_scopes_dyn(pscope, &cls_buf);
                    }
                }
#if NEED_NAME_MANGLING
                if (curr->source_corresp.unmangled_name_or_mangled_encoding &&
                    curr->source_corresp.unmangled_name_or_mangled_encoding[0] != '\0') {
                    cname = curr->source_corresp.unmangled_name_or_mangled_encoding;
                }
#endif
                if (cerr == GCC_GEN_BE_SUCCESS) {
                    if (ctsp && ctsp->is_lambda_closure_class) {
                        cerr = dynbuf_append(&cls_buf, "Ul");
                        a_boolean emitted_lambda_params = FALSE;
                        if (cerr == GCC_GEN_BE_SUCCESS && ctsp->assoc_scope && ctsp->assoc_scope->routines) {
                            for (a_routine_ptr lrp = ctsp->assoc_scope->routines; lrp != NULL; lrp = lrp->next) {
                                if (lrp->special_kind == sfk_operator && lrp->type && lrp->type->kind == tk_routine &&
                                    lrp->type->variant.routine.extra_info) {
                                    a_param_type_ptr lptp = lrp->type->variant.routine.extra_info->param_type_list;
                                    if (lptp && lrp->storage_class != sc_static) {
                                        lptp = lptp->next;
                                    }
                                    if (lptp) {
                                        for (; lptp != NULL && cerr == GCC_GEN_BE_SUCCESS; lptp = lptp->next) {
                                            cerr = mangle_type_inner_dyn(lptp->type, &cls_buf);
                                        }
                                        emitted_lambda_params = TRUE;
                                    }
                                    break;
                                }
                            }
                        }
                        if (cerr == GCC_GEN_BE_SUCCESS && !emitted_lambda_params) {
                            cerr = dynbuf_append(&cls_buf, "v");
                        }
                        if (cerr == GCC_GEN_BE_SUCCESS) {
                            cerr = dynbuf_append(&cls_buf, "E_");
                        }
                    } else if (cname && cname[0] != '\0') {
                        cerr = dynbuf_append_len_ident(&cls_buf, NULL, cname);
                    } else if (is_nested) {
                        cerr = dynbuf_append(&cls_buf, "Ut_");
                    } else {
                        cerr = dynbuf_append(&cls_buf, "11unnamed_cls");
                    }
                }
                if (cerr == GCC_GEN_BE_SUCCESS && targs) {
                    cerr = mangle_template_args_dyn(targs, &cls_buf);
                }
                if (cerr == GCC_GEN_BE_SUCCESS && is_nested) {
                    cerr = dynbuf_append(&cls_buf, "E");
                }
                if (cerr != GCC_GEN_BE_SUCCESS) {
                    free(cls_buf.data);
                    return cerr;
                }

                a_boolean subst_found = FALSE;
                char subst_tok[16];
                cerr = find_or_record_substitution(cls_buf.data, &subst_found, subst_tok, sizeof(subst_tok));
                if (cerr == GCC_GEN_BE_SUCCESS) {
                    cerr = dynbuf_append(out_buf, subst_found ? subst_tok : cls_buf.data);
                }
                free(cls_buf.data);
                GCC_GEN_BE_CHECK(cerr);
            }
            break;
        default:
            {
                const char *vdef = "v";
                GCC_GEN_BE_CHECK(dynbuf_append(out_buf, vdef));
            }
            break;
    }

    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_mangle_type_name_alloc(
    a_type_ptr tp,
    char **out_str) GCC_GEN_BE_NOEXCEPT {
    if (!out_str) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_str = NULL;

    if (tp && gcc_gen_be_singleton_state.canonical_mangle_typeinfo) {
        char *canon_ti = gcc_gen_be_singleton_state.canonical_mangle_typeinfo(tp);
        if (canon_ti && strncmp(canon_ti, "_ZTI", 4) == 0 && canon_ti[4] != '\0') {
            size_t len = strlen(canon_ti + 4);
            char *res = NULL;
            GCC_GEN_BE_CHECK(gcc_gen_be_alloc(len + 1, (void **)&res));
            memcpy(res, canon_ti + 4, len + 1);
            *out_str = res;
            return GCC_GEN_BE_SUCCESS;
        }
    }

    GCC_GEN_BE_CHECK(reset_subst_table());
    gcc_gen_be_dynbuf_t dbuf;
    GCC_GEN_BE_CHECK(dynbuf_init(&dbuf));
    gcc_gen_be_error_t err = mangle_type_inner_dyn(tp, &dbuf);
    if (err != GCC_GEN_BE_SUCCESS) {
        free(dbuf.data);
        return err;
    }
    *out_str = dbuf.data;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_mangle_type_name(a_type_ptr tp, char *buffer, size_t size) GCC_GEN_BE_NOEXCEPT {
    if (!buffer || size == 0) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    char *heap_str = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name_alloc(tp, &heap_str));
    size_t len = strlen(heap_str);
    if (len >= size) {
        free(heap_str);
        return GCC_GEN_BE_ERROR_MANGLING_BUFFER_OVERFLOW;
    }
    memcpy(buffer, heap_str, len + 1);
    free(heap_str);
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_mangle_with_substitutions(
    a_type_ptr tp,
    a_boolean reset_table,
    char *buffer,
    size_t size) GCC_GEN_BE_NOEXCEPT {
    if (!buffer || size == 0) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    if (reset_table) {
        GCC_GEN_BE_CHECK(reset_subst_table());
    }
    gcc_gen_be_dynbuf_t dbuf;
    GCC_GEN_BE_CHECK(dynbuf_init(&dbuf));
    gcc_gen_be_error_t err = mangle_type_inner_dyn(tp, &dbuf);
    if (err != GCC_GEN_BE_SUCCESS) {
        free(dbuf.data);
        return err;
    }
    return dynbuf_finish_into(&dbuf, buffer, size);
}

/**
 * @brief Maps a source operator function name (`operator+`, `operator==`, etc.) to its 2-letter Itanium code.
 * @details Inspects `raw_name` and writes the Itanium ABI operator code to `*out_op_code`.
 * @param[in] raw_name Unmangled function name (e.g., `"operator()"`, `"operator new"`).
 * @param[out] out_op_code Non-null pointer receiving the static 2-letter code, or NULL if not an operator.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM`.
 */
static gcc_gen_be_error_t map_operator_name_code(const char *raw_name, const char **out_op_code) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
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

/**
 * @brief Rewrites the constructor/destructor variant token (`C1`/`D1` -> `variant_code`) in a canonical `_ZN...` name.
 * @details Scans for the innermost `C1E`, `C2E`, `D1E`, `D2E`, or `D0E` (or `C1I`/`D1I` on template ctors)
 * and substitutes the two-character `variant_code`.
 * @param[in] canon_name Non-null canonical mangled function name starting with `_Z`.
 * @param[in] variant_code Optional 2-character variant code (`"C1"`, `"C2"`, `"D0"`, `"D1"`, `"D2"`).
 * @param[out] out_str Non-null pointer receiving the newly heap-allocated symbol string.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t clone_canonical_fn_with_variant(
    const char *canon_name,
    const char *variant_code,
    char **out_str) GCC_GEN_BE_NOEXCEPT {
    size_t len = strlen(canon_name);
    char *copy = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_alloc(len + 1, (void **)&copy));
    memcpy(copy, canon_name, len + 1);
    if (variant_code && strlen(variant_code) == 2) {
        char *match_pos = NULL;
        for (size_t i = 2; i + 2 < len; ++i) {
            if ((copy[i] == 'C' || copy[i] == 'D') &&
                (copy[i + 1] == '0' || copy[i + 1] == '1' || copy[i + 1] == '2') &&
                (copy[i + 2] == 'E' || copy[i + 2] == 'I' || copy[i + 2] == 'B')) {
                match_pos = copy + i;
            }
        }
        if (match_pos) {
            match_pos[0] = variant_code[0];
            match_pos[1] = variant_code[1];
        }
    }
    *out_str = copy;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_mangle_function_name_alloc(
    a_routine_ptr rout,
    const char *variant_code,
    char **out_str) GCC_GEN_BE_NOEXCEPT {
    if (!rout || !out_str) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_str = NULL;

    if (gcc_gen_be_singleton_state.canonical_mangle_func) {
        char *canon_fn = gcc_gen_be_singleton_state.canonical_mangle_func(rout);
        if (canon_fn && canon_fn[0] != '\0') {
            return clone_canonical_fn_with_variant(canon_fn, variant_code, out_str);
        }
    }

#if NEED_NAME_MANGLING
    if (rout->source_corresp.unmangled_name_or_mangled_encoding &&
        strncmp(rout->source_corresp.unmangled_name_or_mangled_encoding, "_Z", 2) == 0) {
        return clone_canonical_fn_with_variant(
            rout->source_corresp.unmangled_name_or_mangled_encoding, variant_code, out_str);
    }
#endif

    a_const_char *raw_name = rout->source_corresp.name;
    if (raw_name && strncmp(raw_name, "_Z", 2) == 0) {
        return clone_canonical_fn_with_variant(raw_name, variant_code, out_str);
    }

    GCC_GEN_BE_CHECK(reset_subst_table());
    gcc_gen_be_dynbuf_t dbuf;
    GCC_GEN_BE_CHECK(dynbuf_init(&dbuf));

    gcc_gen_be_error_t err = dynbuf_append(&dbuf, "_Z");
    a_scope_ptr pscope = rout->source_corresp.parent_scope;
    a_boolean is_nested = (pscope && pscope->kind != sck_file) ? TRUE : FALSE;
    a_routine_type_supplement_ptr rtsp =
        (rout->type && rout->type->kind == tk_routine) ? rout->type->variant.routine.extra_info : NULL;

    if (err == GCC_GEN_BE_SUCCESS && is_nested) {
        err = dynbuf_append(&dbuf, "N");
        if (err == GCC_GEN_BE_SUCCESS && rtsp) {
            if (rtsp->qualifiers & TQ_RESTRICT) err = dynbuf_append(&dbuf, "r");
            if (err == GCC_GEN_BE_SUCCESS && (rtsp->qualifiers & TQ_VOLATILE)) err = dynbuf_append(&dbuf, "V");
            if (err == GCC_GEN_BE_SUCCESS && (rtsp->qualifiers & TQ_CONST))    err = dynbuf_append(&dbuf, "K");
            if (err == GCC_GEN_BE_SUCCESS && rtsp->ref_qualifiers != rqk_default) {
                err = dynbuf_append(&dbuf, rtsp->ref_qualifiers == rqk_rvalue ? "O" : "R");
            }
        }
        if (err == GCC_GEN_BE_SUCCESS) {
            err = mangle_enclosing_scopes_dyn(pscope, &dbuf);
        }
    }

    const char *op_code = NULL;
    if (err == GCC_GEN_BE_SUCCESS) {
        err = map_operator_name_code(raw_name, &op_code);
    }

    if (err == GCC_GEN_BE_SUCCESS) {
        if (variant_code && variant_code[0] != '\0') {
            err = dynbuf_append(&dbuf, variant_code);
        } else if (rout->special_kind == sfk_constructor) {
            err = dynbuf_append(&dbuf, "C1");
        } else if (rout->special_kind == sfk_destructor) {
            err = dynbuf_append(&dbuf, "D1");
        } else if (rout->special_kind == sfk_conversion) {
            err = dynbuf_append(&dbuf, "cv");
            if (err == GCC_GEN_BE_SUCCESS) {
                a_type_ptr ret_tp = (rout->type && rout->type->kind == tk_routine)
                    ? rout->type->variant.routine.return_type : NULL;
                err = mangle_type_inner_dyn(ret_tp, &dbuf);
            }
        } else if (raw_name && strncmp(raw_name, "operator\"\"", 10) == 0) {
            const char *lit_suf = raw_name + 10;
            while (*lit_suf == ' ') lit_suf++;
            err = dynbuf_append_len_ident(&dbuf, "li", lit_suf);
        } else if (op_code) {
            err = dynbuf_append(&dbuf, op_code);
        } else if (raw_name && raw_name[0] != '\0') {
            if (rout->source_corresp.module_info &&
                !rout->source_corresp.is_exported &&
                rout->source_corresp.module_info->kind == mk_unit &&
                rout->source_corresp.module_info->variant.unit.name &&
                rout->source_corresp.module_info->variant.unit.name[0] != '\0') {
                err = dynbuf_append_len_ident(&dbuf, "W", rout->source_corresp.module_info->variant.unit.name);
            }
            if (err == GCC_GEN_BE_SUCCESS) {
                err = dynbuf_append_len_ident(&dbuf, NULL, raw_name);
            }
        } else {
            err = dynbuf_append(&dbuf, "6anonfn");
        }
    }

    if (err == GCC_GEN_BE_SUCCESS && rout->is_template_function && rout->template_arg_list) {
        err = mangle_template_args_dyn(rout->template_arg_list, &dbuf);
    }

    if (err == GCC_GEN_BE_SUCCESS && is_nested) {
        err = dynbuf_append(&dbuf, "E");
    }

    if (err == GCC_GEN_BE_SUCCESS) {
        a_param_type_ptr ptp = rtsp ? rtsp->param_type_list : NULL;
        if (is_nested && pscope->kind == sck_class_struct_union && rout->storage_class != sc_static && ptp) {
            ptp = ptp->next;
        }
        if (!ptp) {
            err = dynbuf_append(&dbuf, (!rtsp || !rtsp->has_ellipsis) ? "v" : "z");
        } else {
            for (; ptp != NULL && err == GCC_GEN_BE_SUCCESS; ptp = ptp->next) {
                err = mangle_type_inner_dyn(ptp->type, &dbuf);
            }
            if (err == GCC_GEN_BE_SUCCESS && rtsp->has_ellipsis) {
                err = dynbuf_append(&dbuf, "z");
            }
        }
    }

    if (err != GCC_GEN_BE_SUCCESS) {
        free(dbuf.data);
        return err;
    }
    *out_str = dbuf.data;
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_mangle_function_name(
    a_routine_ptr rout,
    const char *variant_code,
    char *buffer,
    size_t size) GCC_GEN_BE_NOEXCEPT {
    if (!rout || !buffer || size == 0) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    buffer[0] = '\0';
    char *heap_str = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name_alloc(rout, variant_code, &heap_str));
    size_t len = strlen(heap_str);
    if (len >= size) {
        free(heap_str);
        return GCC_GEN_BE_ERROR_MANGLING_BUFFER_OVERFLOW;
    }
    memcpy(buffer, heap_str, len + 1);
    free(heap_str);
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
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

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

    char *mangled_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name_alloc(class_type, &mangled_type));

    gcc_gen_be_error_t err = GCC_GEN_BE_SUCCESS;
    char *vtable_name = NULL;
    if (gcc_gen_be_singleton_state.canonical_mangle_vtbl) {
        char *canon_vt = gcc_gen_be_singleton_state.canonical_mangle_vtbl(class_type, NULL, NULL);
        if (canon_vt && canon_vt[0] != '\0') {
            size_t clen = strlen(canon_vt);
            err = gcc_gen_be_alloc(clen + 1, (void **)&vtable_name);
            if (err != GCC_GEN_BE_SUCCESS) {
                free(mangled_type);
                return err;
            }
            memcpy(vtable_name, canon_vt, clen + 1);
        }
    }
    if (!vtable_name) {
        size_t mlen = strlen(mangled_type);
        err = gcc_gen_be_alloc(mlen + 8, (void **)&vtable_name);
        if (err != GCC_GEN_BE_SUCCESS) {
            free(mangled_type);
            return err;
        }
        snprintf(vtable_name, mlen + 8, "_ZTV%s", mangled_type);
    }

    void *cached_vtbl = NULL;
    err = cache_lookup(GCC_GEN_BE_CACHE_VAR, class_type, &cached_vtbl);
    if (err != GCC_GEN_BE_SUCCESS) { free(vtable_name); free(mangled_type); return err; }
    if (cached_vtbl) {
        free(vtable_name);
        free(mangled_type);
        *out_vtable = (gcc_jit_lvalue *)cached_vtbl;
        return GCC_GEN_BE_SUCCESS;
    }
    err = cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)vtable_name, &cached_vtbl);
    if (err != GCC_GEN_BE_SUCCESS) { free(vtable_name); free(mangled_type); return err; }
    if (cached_vtbl) {
        void *var_lval = NULL;
        err = cache_lookup(GCC_GEN_BE_CACHE_VAR, cached_vtbl, &var_lval);
        if (err != GCC_GEN_BE_SUCCESS) { free(vtable_name); free(mangled_type); return err; }
        if (var_lval) {
            cached_vtbl = var_lval;
        }
        err = cache_insert(GCC_GEN_BE_CACHE_VAR, class_type, cached_vtbl);
        free(vtable_name);
        free(mangled_type);
        if (err != GCC_GEN_BE_SUCCESS) return err;
        *out_vtable = (gcc_jit_lvalue *)cached_vtbl;
        return GCC_GEN_BE_SUCCESS;
    }

    a_boolean is_anon_cls = FALSE;
    err = gcc_gen_be_is_scope_anonymous(class_type->source_corresp.parent_scope, &is_anon_cls);
    if (err != GCC_GEN_BE_SUCCESS) {
        free(vtable_name);
        free(mangled_type);
        return err;
    }
    enum gcc_jit_global_kind vt_linkage = is_anon_cls ? GCC_JIT_GLOBAL_INTERNAL : GCC_JIT_GLOBAL_EXPORTED;

    gcc_jit_lvalue *vtable = gcc_jit_context_new_global(ctx, NULL, vt_linkage, array_type, vtable_name);
    if (!is_anon_cls) {
        err = gcc_gen_be_mark_global_vague_linkage(ctx, vtable, vtable_name);
        if (err != GCC_GEN_BE_SUCCESS) {
            free(vtable_name);
            free(mangled_type);
            return err;
        }
    }
    err = cache_insert(GCC_GEN_BE_CACHE_VAR, class_type, vtable);
    if (err == GCC_GEN_BE_SUCCESS) {
        err = cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)vtable_name, vtable);
    }
    free(vtable_name);
    if (err != GCC_GEN_BE_SUCCESS) {
        free(mangled_type);
        return err;
    }

    gcc_jit_rvalue **vtable_entries = NULL;
    err = gcc_gen_be_alloc(num_entries * sizeof(gcc_jit_rvalue *), (void **)&vtable_entries);
    if (err != GCC_GEN_BE_SUCCESS) {
        free(mangled_type);
        return err;
    }

    int idx = 0;
    for (int i = 0; i < num_vcall_offsets; ++i) {
        long vcall_off = 0L;
        if (ctsp->assoc_scope) {
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
        free(mangled_type);
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
                err = gcc_gen_be_emit_thunk(rp, &thunk_fn);
                if (err != GCC_GEN_BE_SUCCESS) { free(vtable_entries); free(mangled_type); return err; }
                func_addr = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(thunk_fn, NULL), void_ptr_type);
            } else {
                void *cached = NULL;
                err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, rp, &cached);
                if (err != GCC_GEN_BE_SUCCESS) { free(vtable_entries); free(mangled_type); return err; }
                if (cached) {
                    gcc_jit_function *fn = (gcc_jit_function *)cached;
                    void *abi_fn = NULL;
                    err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, fn, &abi_fn);
                    if (err != GCC_GEN_BE_SUCCESS) { free(vtable_entries); free(mangled_type); return err; }
                    if (abi_fn) fn = (gcc_jit_function *)abi_fn;
                    func_addr = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(fn, NULL), void_ptr_type);
                } else {
                    extern gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT;
                    gcc_jit_function *fn = NULL;
                    err = gcc_gen_be_lower_function_decl(rp, &fn);
                    if (err != GCC_GEN_BE_SUCCESS) { free(vtable_entries); free(mangled_type); return err; }
                    if (fn) {
                        void *abi_fn = NULL;
                        err = cache_lookup(GCC_GEN_BE_CACHE_FUNC, fn, &abi_fn);
                        if (err != GCC_GEN_BE_SUCCESS) { free(vtable_entries); free(mangled_type); return err; }
                        if (abi_fn) fn = (gcc_jit_function *)abi_fn;
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

    b = use_all_bases ? ctsp->base_classes : ctsp->direct_base_classes;
    int secondary_idx = 0;
    while (b) {
        if (!b->is_virtual && !b->direct) {
            b = use_all_bases ? b->next : b->next_direct;
            continue;
        }
        char *mangled_base = NULL;
        err = gcc_gen_be_mangle_type_name_alloc(b->type, &mangled_base);
        if (err != GCC_GEN_BE_SUCCESS) {
            free(vtable_entries);
            free(mangled_type);
            return err;
        }

        gcc_jit_rvalue **sec_entries = NULL;
        err = gcc_gen_be_alloc(num_entries * sizeof(gcc_jit_rvalue *), (void **)&sec_entries);
        if (err != GCC_GEN_BE_SUCCESS) {
            free(mangled_base);
            free(vtable_entries);
            free(mangled_type);
            return err;
        }
        for (size_t k = 0; k < num_entries; ++k) {
            sec_entries[k] = vtable_entries[k];
        }
        long sec_off_to_top = -((long)b->offset);
        gcc_jit_rvalue *off_to_top_ptr = (sec_off_to_top != 0L)
            ? gcc_jit_context_new_rvalue_from_ptr(ctx, void_ptr_type, (void *)(uintptr_t)sec_off_to_top)
            : gcc_jit_context_null(ctx, void_ptr_type);
        for (int vc = 0; vc < num_vcall_offsets; ++vc) {
            sec_entries[vc] = off_to_top_ptr;
        }
        int vb_slot = num_vcall_offsets;
        for (a_base_class_ptr vb = use_all_bases ? ctsp->base_classes : ctsp->direct_base_classes;
             vb != NULL;
             vb = use_all_bases ? vb->next : vb->next_direct) {
            if (vb->is_virtual) {
                long rel_vbase_off = (long)vb->offset - (long)b->offset;
                sec_entries[vb_slot++] = (rel_vbase_off != 0L)
                    ? gcc_jit_context_new_rvalue_from_ptr(ctx, void_ptr_type, (void *)(uintptr_t)rel_vbase_off)
                    : gcc_jit_context_null(ctx, void_ptr_type);
            }
        }
        sec_entries[num_vcall_offsets + num_vbase_offsets] = off_to_top_ptr;

        gcc_gen_be_error_t sec_err = GCC_GEN_BE_SUCCESS;
        if (b->offset != 0 && ctsp->assoc_scope) {
            for (a_routine_ptr srp = ctsp->assoc_scope->routines;
                 srp != NULL && sec_err == GCC_GEN_BE_SUCCESS;
                 srp = srp->next) {
                if (srp->number.virtual_function != VIRTUAL_FUNCTION_NUMBER_NONE &&
                    srp->number.virtual_function < num_funcs &&
                    !srp->pure_virtual && !srp->is_deleted &&
                    srp->overriding_function_for_wrapper) {
                    gcc_jit_function *sec_thunk = NULL;
                    if (srp->return_delta != 0 || srp->vbase_index != 0) {
                        sec_err = gcc_gen_be_emit_covariant_return_thunk(
                            srp, (long)srp->delta,
                            srp->vbase_index != 0 ? (long)srp->vbase_index : (long)srp->return_delta,
                            srp->vbase_index != 0 ? TRUE : FALSE, &sec_thunk);
                    } else if (b->is_virtual || srp->vcall_index != 0) {
                        sec_err = gcc_gen_be_emit_vcall_thunk(class_type, b, srp, (long)srp->vcall_index, &sec_thunk);
                    } else {
                        sec_err = gcc_gen_be_emit_thunk(srp, &sec_thunk);
                    }
                    if (sec_err == GCC_GEN_BE_SUCCESS) {
                        sec_entries[func_start_idx + srp->number.virtual_function] =
                            gcc_jit_context_new_cast(ctx, NULL, gcc_jit_function_get_address(sec_thunk, NULL), void_ptr_type);
                    }
                }
            }
        }

        if (sec_err == GCC_GEN_BE_SUCCESS) {
            gcc_jit_rvalue *sec_init = gcc_jit_context_new_array_constructor(ctx, NULL, array_type, num_entries, sec_entries);
            size_t sec_cap = strlen(mangled_type) + 32;
            char *sec_name = NULL;
            sec_err = gcc_gen_be_alloc(sec_cap, (void **)&sec_name);
            if (sec_err == GCC_GEN_BE_SUCCESS) {
                snprintf(sec_name, sec_cap, "_ZTV_sec_%s_%d", mangled_type, secondary_idx++);
                gcc_jit_lvalue *sec_vtable = gcc_jit_context_new_global(
                    ctx, NULL, GCC_JIT_GLOBAL_INTERNAL, array_type, sec_name);
                gcc_jit_global_set_initializer_rvalue(sec_vtable, sec_init);
                free(sec_name);
            }

            if (sec_err == GCC_GEN_BE_SUCCESS) {
                char *ctc_name = NULL;
                if (gcc_gen_be_singleton_state.canonical_mangle_vtbl) {
                    char *canon_ctc = gcc_gen_be_singleton_state.canonical_mangle_vtbl(class_type, b, b);
                    if (canon_ctc && canon_ctc[0] != '\0') {
                        size_t clen = strlen(canon_ctc);
                        sec_err = gcc_gen_be_alloc(clen + 1, (void **)&ctc_name);
                        if (sec_err == GCC_GEN_BE_SUCCESS) memcpy(ctc_name, canon_ctc, clen + 1);
                    }
                }
                if (sec_err == GCC_GEN_BE_SUCCESS && !ctc_name) {
                    size_t ctc_cap = strlen(mangled_type) + strlen(mangled_base) + 40;
                    sec_err = gcc_gen_be_alloc(ctc_cap, (void **)&ctc_name);
                    if (sec_err == GCC_GEN_BE_SUCCESS) {
                        snprintf(ctc_name, ctc_cap, "_ZTC%s%lu_%s",
                                 mangled_type, (unsigned long)b->offset, mangled_base);
                    }
                }
                if (sec_err == GCC_GEN_BE_SUCCESS) {
                    void *existing_ctc = NULL;
                    gcc_jit_lvalue *ctc_vtable = NULL;
                    sec_err = cache_lookup(GCC_GEN_BE_CACHE_STRING, ctc_name, &existing_ctc);
                    if (sec_err == GCC_GEN_BE_SUCCESS) {
                        if (existing_ctc) {
                            ctc_vtable = (gcc_jit_lvalue *)existing_ctc;
                        } else {
                            ctc_vtable = gcc_jit_context_new_global(ctx, NULL, vt_linkage, array_type, ctc_name);
                            if (!is_anon_cls) {
                                sec_err = gcc_gen_be_mark_global_vague_linkage(ctx, ctc_vtable, ctc_name);
                            }
                            if (sec_err == GCC_GEN_BE_SUCCESS) {
                                gcc_jit_global_set_initializer_rvalue(ctc_vtable, sec_init);
                                sec_err = cache_insert(GCC_GEN_BE_CACHE_STRING, ctc_name, ctc_vtable);
                            }
                        }
                    }
                    if (sec_err == GCC_GEN_BE_SUCCESS) {
                        sec_err = cache_insert(GCC_GEN_BE_CACHE_VAR, b, ctc_vtable);
                    }
                    free(ctc_name);
                }
            }
        }

        free(mangled_base);
        free(sec_entries);
        if (sec_err != GCC_GEN_BE_SUCCESS) {
            free(vtable_entries);
            free(mangled_type);
            return sec_err;
        }
        b = use_all_bases ? b->next : b->next_direct;
    }

    gcc_jit_rvalue *init_array = gcc_jit_context_new_array_constructor(ctx, NULL, array_type, num_entries, vtable_entries);
    gcc_jit_global_set_initializer_rvalue(vtable, init_array);

    free(vtable_entries);
    free(mangled_type);
    
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
    while (class_type && class_type->kind == tk_typeref) {
        class_type = class_type->variant.typeref.type;
    }
    if (!class_type || (class_type->kind != tk_class && class_type->kind != tk_struct)) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    a_class_type_supplement_ptr ctsp = class_type->variant.class_struct_union.extra_info;
    if (!ctsp) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));

    int num_secondary_vtts = 0;
    int num_vbase_offsets = 0;
    int num_funcs = 0;
    if (ctsp->highest_virtual_function_number != VIRTUAL_FUNCTION_NUMBER_NONE) {
        num_funcs = ctsp->highest_virtual_function_number + 1;
    }
    a_boolean use_all_bases = (ctsp->base_classes != NULL) ? TRUE : FALSE;
    a_base_class_ptr b = use_all_bases ? ctsp->base_classes : ctsp->direct_base_classes;
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

    char *mangled_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name_alloc(class_type, &mangled_type));
    if (strstr(mangled_type, "__SO__") != NULL) {
        free(mangled_type);
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_gen_be_error_t err = GCC_GEN_BE_SUCCESS;
    char *vtt_name = NULL;
    if (gcc_gen_be_singleton_state.canonical_mangle_vtt) {
        char *canon_vtt = gcc_gen_be_singleton_state.canonical_mangle_vtt(class_type);
        if (canon_vtt && canon_vtt[0] != '\0') {
            size_t clen = strlen(canon_vtt);
            err = gcc_gen_be_alloc(clen + 1, (void **)&vtt_name);
            if (err != GCC_GEN_BE_SUCCESS) {
                free(mangled_type);
                return err;
            }
            memcpy(vtt_name, canon_vtt, clen + 1);
        }
    }
    if (!vtt_name) {
        size_t mlen = strlen(mangled_type);
        err = gcc_gen_be_alloc(mlen + 8, (void **)&vtt_name);
        if (err != GCC_GEN_BE_SUCCESS) {
            free(mangled_type);
            return err;
        }
        snprintf(vtt_name, mlen + 8, "_ZTT%s", mangled_type);
    }
    free(mangled_type);

    void *cached_vtt = NULL;
    err = cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)vtt_name, &cached_vtt);
    if (err != GCC_GEN_BE_SUCCESS) { free(vtt_name); return err; }
    if (cached_vtt) {
        void *var_lval = NULL;
        err = cache_lookup(GCC_GEN_BE_CACHE_VAR, cached_vtt, &var_lval);
        if (err != GCC_GEN_BE_SUCCESS) { free(vtt_name); return err; }
        if (var_lval) {
            cached_vtt = var_lval;
        }
        free(vtt_name);
        *out_vtt = (gcc_jit_lvalue *)cached_vtt;
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_lvalue *vtable = NULL;
    err = gcc_gen_be_emit_vtable(class_type, &vtable);
    if (err != GCC_GEN_BE_SUCCESS) {
        free(vtt_name);
        return err;
    }

    a_boolean is_anon_cls = FALSE;
    err = gcc_gen_be_is_scope_anonymous(class_type->source_corresp.parent_scope, &is_anon_cls);
    if (err != GCC_GEN_BE_SUCCESS) {
        free(vtt_name);
        return err;
    }
    enum gcc_jit_global_kind vtt_linkage = is_anon_cls ? GCC_JIT_GLOBAL_INTERNAL : GCC_JIT_GLOBAL_EXPORTED;

    gcc_jit_lvalue *vtt = gcc_jit_context_new_global(ctx, NULL, vtt_linkage, array_type, vtt_name);
    if (!is_anon_cls) {
        err = gcc_gen_be_mark_global_vague_linkage(ctx, vtt, vtt_name);
        if (err != GCC_GEN_BE_SUCCESS) {
            free(vtt_name);
            return err;
        }
    }
    err = cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)vtt_name, vtt);
    free(vtt_name);
    if (err != GCC_GEN_BE_SUCCESS) return err;

    gcc_jit_rvalue **vtt_entries = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_alloc(num_vtt_entries * sizeof(gcc_jit_rvalue *), (void **)&vtt_entries));

    gcc_jit_type *int_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT);
    gcc_jit_rvalue *ap_idx_rval = gcc_jit_context_new_rvalue_from_int(ctx, int_type, addr_point_idx);
    gcc_jit_rvalue *vtable_addr = gcc_jit_lvalue_get_address(
        gcc_jit_context_new_array_access(ctx, NULL, gcc_jit_lvalue_as_rvalue(vtable), ap_idx_rval), NULL);
    vtt_entries[0] = gcc_jit_context_new_cast(ctx, NULL, vtable_addr, void_ptr_type);

    b = use_all_bases ? ctsp->base_classes : ctsp->direct_base_classes;
    for (int i = 0; i < num_secondary_vtts; ++i) {
        while (!b->is_virtual && !b->direct) {
            b = use_all_bases ? b->next : b->next_direct;
        }
        void *cached_ctc = NULL;
        err = cache_lookup(GCC_GEN_BE_CACHE_VAR, b, &cached_ctc);
        if (err != GCC_GEN_BE_SUCCESS) {
            free(vtt_entries);
            return err;
        }
        b = use_all_bases ? b->next : b->next_direct;
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
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

    if (!thunk_rp->type || thunk_rp->type->kind != tk_routine ||
        !thunk_rp->type->variant.routine.extra_info ||
        !thunk_rp->type->variant.routine.extra_info->param_type_list) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }

    a_routine_ptr target_rp = thunk_rp->overriding_function_for_wrapper
        ? thunk_rp->overriding_function_for_wrapper
        : thunk_rp;
    char *mangled_target = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_function_name_alloc(target_rp, NULL, &mangled_target));
    const char *target_encoding = mangled_target;
    if (strncmp(target_encoding, "_Z", 2) == 0 && target_encoding[2] != '\0') {
        target_encoding += 2;
    }

    long delta_abs = thunk_rp->delta < 0 ? -((long)thunk_rp->delta) : (long)thunk_rp->delta;
    long rdelta_abs = thunk_rp->return_delta < 0 ? -((long)thunk_rp->return_delta) : (long)thunk_rp->return_delta;
    long vcall_abs = thunk_rp->vcall_index < 0 ? -((long)thunk_rp->vcall_index) : (long)thunk_rp->vcall_index;
    long vbase_abs = thunk_rp->vbase_index < 0 ? -((long)thunk_rp->vbase_index) : (long)thunk_rp->vbase_index;

    size_t tname_cap = strlen(target_encoding) + 96;
    char *thunk_name = NULL;
    gcc_gen_be_error_t err = gcc_gen_be_alloc(tname_cap, (void **)&thunk_name);
    if (err != GCC_GEN_BE_SUCCESS) {
        free(mangled_target);
        return err;
    }
    if (thunk_rp->return_delta != 0 || thunk_rp->vbase_index != 0) {
        if (thunk_rp->vcall_index != 0 || thunk_rp->vbase_index != 0) {
            snprintf(thunk_name, tname_cap, "_ZTcv0_n%ld_v0_n%ld_%s",
                     vcall_abs != 0 ? vcall_abs : delta_abs,
                     vbase_abs != 0 ? vbase_abs : rdelta_abs,
                     target_encoding);
        } else {
            snprintf(thunk_name, tname_cap, "_ZTch%s%ld_%s%ld_%s",
                     thunk_rp->delta < 0 ? "n" : "", delta_abs,
                     thunk_rp->return_delta < 0 ? "n" : "", rdelta_abs,
                     target_encoding);
        }
    } else if (thunk_rp->vcall_index != 0) {
        snprintf(thunk_name, tname_cap, "_ZTv0_n%ld_%s", vcall_abs, target_encoding);
    } else {
        snprintf(thunk_name, tname_cap, "_ZTh%s%ld_%s",
                 thunk_rp->delta < 0 ? "n" : "", delta_abs, target_encoding);
    }
    free(mangled_target);

    void *cached_thunk = NULL;
    err = cache_lookup(GCC_GEN_BE_CACHE_STRING, thunk_name, &cached_thunk);
    if (err != GCC_GEN_BE_SUCCESS) {
        free(thunk_name);
        return err;
    }
    if (cached_thunk) {
        free(thunk_name);
        *out_func = (gcc_jit_function *)cached_thunk;
        return GCC_GEN_BE_SUCCESS;
    }

    gcc_jit_type *ret_type = NULL;
    err = gcc_gen_be_lower_type(thunk_rp->type->variant.routine.return_type, &ret_type);
    if (err != GCC_GEN_BE_SUCCESS) {
        free(thunk_name);
        return err;
    }

    extern gcc_gen_be_error_t gcc_gen_be_is_passed_by_invisible_reference(a_type_ptr type, a_boolean *out_is_ref) GCC_GEN_BE_NOEXCEPT;
    a_type_ptr orig_ret_type = thunk_rp->type->variant.routine.return_type;
    while (orig_ret_type && orig_ret_type->kind == tk_typeref) orig_ret_type = orig_ret_type->variant.typeref.type;
    a_boolean uses_sret = FALSE;
    err = gcc_gen_be_is_passed_by_invisible_reference(orig_ret_type, &uses_sret);
    if (err != GCC_GEN_BE_SUCCESS) {
        free(thunk_name);
        return err;
    }
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
    err = gcc_gen_be_alloc((size_t)num_params * sizeof(gcc_jit_param *), (void **)&params);
    if (err != GCC_GEN_BE_SUCCESS) {
        free(thunk_name);
        return err;
    }
    int p_idx = 0;
    if (uses_sret) {
        params[p_idx++] = gcc_jit_context_new_param(ctx, NULL, sret_ptr_type, "sret_buffer");
    }
    for (ptp = thunk_rp->type->variant.routine.extra_info->param_type_list; ptp != NULL; ptp = ptp->next) {
        gcc_jit_type *ptype = NULL;
        err = gcc_gen_be_lower_type(ptp->type, &ptype);
        if (err != GCC_GEN_BE_SUCCESS) { free(params); free(thunk_name); return err; }

        a_type_ptr p_orig = ptp->type;
        while (p_orig && p_orig->kind == tk_typeref) p_orig = p_orig->variant.typeref.type;
        a_boolean is_p_invis_ref = FALSE;
        err = gcc_gen_be_is_passed_by_invisible_reference(p_orig, &is_p_invis_ref);
        if (err != GCC_GEN_BE_SUCCESS) { free(params); free(thunk_name); return err; }
        if (is_p_invis_ref) {
            ptype = gcc_jit_type_get_pointer(ptype);
        }

        char pname[32];
        snprintf(pname, sizeof(pname), "p%d", p_idx);
        params[p_idx] = gcc_jit_context_new_param(ctx, NULL, ptype, pname);
        p_idx++;
    }

    int is_variadic = thunk_rp->type->variant.routine.extra_info->has_ellipsis ? 1 : 0;
    a_boolean is_anon_thunk = FALSE;
    err = gcc_gen_be_is_scope_anonymous(target_rp->source_corresp.parent_scope, &is_anon_thunk);
    if (err != GCC_GEN_BE_SUCCESS) {
        free(params);
        free(thunk_name);
        return err;
    }
    enum gcc_jit_function_kind thunk_linkage = is_anon_thunk ? GCC_JIT_FUNCTION_INTERNAL : GCC_JIT_FUNCTION_EXPORTED;
    gcc_jit_function *thunk_fn = gcc_jit_context_new_function(
        ctx, NULL, thunk_linkage, ret_type, thunk_name, num_params, params, is_variadic);
    if (!is_anon_thunk) {
        err = gcc_gen_be_mark_function_vague_linkage(ctx, thunk_fn, thunk_name);
        if (err != GCC_GEN_BE_SUCCESS) {
            free(params);
            free(thunk_name);
            return err;
        }
    }
    err = cache_insert(GCC_GEN_BE_CACHE_STRING, thunk_name, thunk_fn);
    free(thunk_name);
    if (err != GCC_GEN_BE_SUCCESS) {
        free(params);
        return err;
    }

    extern gcc_gen_be_error_t gcc_gen_be_lower_function_decl(a_routine_ptr rout, struct gcc_jit_function **out_func) GCC_GEN_BE_NOEXCEPT;
    gcc_jit_function *target_fn = NULL;
    err = gcc_gen_be_lower_function_decl(target_rp, &target_fn);
    if (err != GCC_GEN_BE_SUCCESS) {
        free(params);
        return err;
    }

    gcc_jit_rvalue **call_args = NULL;
    err = gcc_gen_be_alloc((size_t)num_params * sizeof(gcc_jit_rvalue *), (void **)&call_args);
    if (err != GCC_GEN_BE_SUCCESS) {
        free(params);
        return err;
    }

    gcc_jit_block *block = gcc_jit_function_new_block(thunk_fn, "entry");
    int this_param_idx = uses_sret ? 1 : 0;

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

    for (int i = 0; i < num_params; i++) {
        call_args[i] = (i == this_param_idx) ? adjusted_this : gcc_jit_param_as_rvalue(params[i]);
    }

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

    free(params);
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
 * @param[out] out_hint Non-null pointer to receive the computed `ptrdiff_t` hint (`>= 0`, `-1`, or `-2`).
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM`.
 */
static gcc_gen_be_error_t compute_dynamic_cast_hint(
    a_type_ptr src_type,
    a_type_ptr dst_type,
    long *out_hint) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    *out_hint = -1L;

    a_type_ptr s = src_type;
    while (s->kind == tk_typeref) s = s->variant.typeref.type;
    if (s->kind == tk_pointer) {
        s = s->variant.pointer.type;
        while (s && s->kind == tk_typeref) s = s->variant.typeref.type;
    }

    a_type_ptr d = dst_type;
    while (d->kind == tk_typeref) d = d->variant.typeref.type;
    if (d->kind == tk_pointer) {
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
 * @param expr The dynamic_cast expression node.
 * @param out_rval Pointer to receive the generated JIT rvalue.
 * @return GCC_GEN_BE_SUCCESS on success, or an error code.
 */
gcc_gen_be_error_t gcc_gen_be_lower_dynamic_cast(an_expr_node_ptr expr, struct gcc_jit_rvalue **out_rval) GCC_GEN_BE_NOEXCEPT {
    if (!expr || !out_rval) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    *out_rval = NULL;
    if (!expr->type || !expr->variant.operation.operands) return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;

    gcc_jit_context *ctx = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

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
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(expr->variant.operation.operands, &src_ptr));
    src_ptr = gcc_jit_context_new_cast(ctx, NULL, src_ptr, void_ptr_type);

    if (is_cast_to_void) {
        /* dynamic_cast<void*> retrieves the offset-to-top from the vtable */
        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        if (!cblock) return GCC_GEN_BE_ERROR_INVALID_STATE;
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
    GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__dynamic_cast", &cached));
    if (cached) {
        dyn_cast_fn = (gcc_jit_function *)cached;
    } else {
        gcc_jit_type *long_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG);
        gcc_jit_param *p1 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "src_ptr");
        gcc_jit_param *p2 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "src_type");
        gcc_jit_param *p3 = gcc_jit_context_new_param(ctx, NULL, void_ptr_type, "dst_type");
        gcc_jit_param *p4 = gcc_jit_context_new_param(ctx, NULL, long_type, "src2dst");
        gcc_jit_param *params[] = {p1, p2, p3, p4};
        dyn_cast_fn = gcc_jit_context_new_function(ctx, NULL, GCC_JIT_FUNCTION_IMPORTED, void_ptr_type, "__dynamic_cast", 4, params, 0);
        GCC_GEN_BE_CHECK(cache_insert(GCC_GEN_BE_CACHE_FUNC, (void *)"__dynamic_cast", dyn_cast_fn));
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
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_typeid(&src_typeid_expr, &src_type_info));
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
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_typeid(&dst_typeid_expr, &dst_type_info));
    dst_type_info = gcc_jit_context_new_cast(ctx, NULL, dst_type_info, void_ptr_type);

    long src2dst_hint = -1L;
    GCC_GEN_BE_CHECK(compute_dynamic_cast_hint(
        expr->variant.operation.operands->type,
        expr->type,
        &src2dst_hint));
    gcc_jit_rvalue *offset = gcc_jit_context_new_rvalue_from_long(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_LONG), src2dst_hint);

    gcc_jit_rvalue *args[] = {src_ptr, src_type_info, dst_type_info, offset};
    gcc_jit_rvalue *call = gcc_jit_context_new_call(ctx, NULL, dyn_cast_fn, 4, args);

    gcc_jit_type *res_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_lower_type(expr->type, &res_type));

    if (expr->type->kind == tk_pointer && expr->type->variant.pointer.is_reference) {
        gcc_jit_block *cblock = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_get_current_block(&cblock));
        if (!cblock) return GCC_GEN_BE_ERROR_INVALID_STATE;
        gcc_jit_function *func = gcc_jit_block_get_function(cblock);

        gcc_jit_rvalue *null_ptr = gcc_jit_context_null(ctx, void_ptr_type);
        gcc_jit_rvalue *is_null = gcc_jit_context_new_comparison(ctx, NULL, GCC_JIT_COMPARISON_EQ, call, null_ptr);

        gcc_jit_block *throw_block = gcc_jit_function_new_block(func, "dynamic_cast_throw");
        gcc_jit_block *continue_block = gcc_jit_function_new_block(func, "dynamic_cast_continue");

        gcc_jit_block_end_with_conditional(cblock, NULL, is_null, throw_block, continue_block);

        void *cached_bad_cast = NULL;
        gcc_jit_function *bad_cast_fn = NULL;
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_bad_cast", &cached_bad_cast));
        if (cached_bad_cast) {
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
    GCC_GEN_BE_CHECK(gcc_gen_be_get_context(&ctx));
    if (!ctx) return GCC_GEN_BE_ERROR_INVALID_STATE;

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
        if (!cblock) return GCC_GEN_BE_ERROR_INVALID_STATE;

        gcc_jit_function *func = gcc_jit_block_get_function(cblock);

        gcc_jit_lvalue *lval = NULL;
        GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_lvalue(operand_expr, &lval));

        gcc_jit_rvalue *obj_ptr = NULL;
        if (lval) {
            obj_ptr = gcc_jit_lvalue_get_address(lval, NULL);
        } else {
            GCC_GEN_BE_CHECK(gcc_gen_be_lower_expr_rvalue(operand_expr, &obj_ptr));
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
        GCC_GEN_BE_CHECK(cache_lookup(GCC_GEN_BE_CACHE_FUNC, (void *)"__cxa_bad_typeid", &cached));
        if (cached) {
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

    gcc_jit_lvalue *type_info_lval = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_emit_type_descriptor(tp, &type_info_lval));
    *out_rval = gcc_jit_context_new_cast(
        ctx, NULL, gcc_jit_lvalue_get_address(type_info_lval, NULL), type_info_type);

    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Computes the Itanium C++ ABI `__vmi_class_type_info` `__flags` word (`0`, `1`, `2`, or `3`).
 * @details Sets `0x1` (`__non_diamond_repeat_mask`) when a non-virtual base class appears multiple times
 * in `ctsp->base_classes`, and `0x2` (`__diamond_shaped_mask`) when virtual base classes exist.
 * @param[in] stripped Non-null unwrapped `tk_class` / `tk_struct` type with non-null `extra_info`.
 * @param[out] out_flags Non-null pointer to receive the computed flags (`0..3`).
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t compute_vmi_type_info_flags(
    a_type_ptr stripped,
    long *out_flags) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    long flags = 0L;
    if (stripped->variant.class_struct_union.any_virtual_base_classes) {
        flags |= 0x2L;
    }
    a_class_type_supplement_ptr ctsp = stripped->variant.class_struct_union.extra_info;
    a_boolean use_all = (ctsp->base_classes != NULL) ? TRUE : FALSE;
    for (a_base_class_ptr b1 = use_all ? ctsp->base_classes : ctsp->direct_base_classes;
         b1 != NULL;
         b1 = use_all ? b1->next : b1->next_direct) {
        if (b1->is_virtual) {
            flags |= 0x2L;
            continue;
        }
        for (a_base_class_ptr b2 = use_all ? b1->next : b1->next_direct;
             b2 != NULL;
             b2 = use_all ? b2->next : b2->next_direct) {
            if (!b2->is_virtual && b1->type && b1->type == b2->type) {
                flags |= 0x1L;
                break;
            }
        }
    }
    *out_flags = flags;
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

    char *mangled_type = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_mangle_type_name_alloc(tp, &mangled_type));

    gcc_gen_be_error_t aerr = GCC_GEN_BE_SUCCESS;
    char *zts_name = NULL;
    if (gcc_gen_be_singleton_state.canonical_mangle_typeinfo_str) {
        char *canon_zts = gcc_gen_be_singleton_state.canonical_mangle_typeinfo_str(tp);
        if (canon_zts && canon_zts[0] != '\0') {
            size_t clen = strlen(canon_zts);
            aerr = gcc_gen_be_alloc(clen + 1, (void **)&zts_name);
            if (aerr != GCC_GEN_BE_SUCCESS) {
                free(mangled_type);
                return aerr;
            }
            memcpy(zts_name, canon_zts, clen + 1);
        }
    }
    if (!zts_name) {
        size_t mlen = strlen(mangled_type);
        aerr = gcc_gen_be_alloc(mlen + 8, (void **)&zts_name);
        if (aerr != GCC_GEN_BE_SUCCESS) {
            free(mangled_type);
            return aerr;
        }
        snprintf(zts_name, mlen + 8, "_ZTS%s", mangled_type);
    }

    char *zti_name = NULL;
    if (gcc_gen_be_singleton_state.canonical_mangle_typeinfo) {
        char *canon_zti = gcc_gen_be_singleton_state.canonical_mangle_typeinfo(tp);
        if (canon_zti && canon_zti[0] != '\0') {
            size_t clen = strlen(canon_zti);
            aerr = gcc_gen_be_alloc(clen + 1, (void **)&zti_name);
            if (aerr != GCC_GEN_BE_SUCCESS) {
                free(zts_name);
                free(mangled_type);
                return aerr;
            }
            memcpy(zti_name, canon_zti, clen + 1);
        }
    }
    if (!zti_name) {
        size_t mlen = strlen(mangled_type);
        aerr = gcc_gen_be_alloc(mlen + 8, (void **)&zti_name);
        if (aerr != GCC_GEN_BE_SUCCESS) {
            free(zts_name);
            free(mangled_type);
            return aerr;
        }
        snprintf(zti_name, mlen + 8, "_ZTI%s", mangled_type);
    }

    void *cached_zti = NULL;
    aerr = cache_lookup(GCC_GEN_BE_CACHE_GUARD, tp, &cached_zti);
    if (aerr != GCC_GEN_BE_SUCCESS) {
        free(zti_name);
        free(zts_name);
        free(mangled_type);
        return aerr;
    }
    if (cached_zti) {
        free(zti_name);
        free(zts_name);
        free(mangled_type);
        *out_descriptor = (gcc_jit_lvalue *)cached_zti;
        return GCC_GEN_BE_SUCCESS;
    }
    aerr = cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)zti_name, &cached_zti);
    if (aerr != GCC_GEN_BE_SUCCESS) {
        free(zti_name);
        free(zts_name);
        free(mangled_type);
        return aerr;
    }
    if (cached_zti) {
        void *var_lval = NULL;
        aerr = cache_lookup(GCC_GEN_BE_CACHE_VAR, cached_zti, &var_lval);
        if (aerr != GCC_GEN_BE_SUCCESS) {
            free(zti_name);
            free(zts_name);
            free(mangled_type);
            return aerr;
        }
        if (var_lval) {
            cached_zti = var_lval;
        }
        aerr = cache_insert(GCC_GEN_BE_CACHE_GUARD, tp, cached_zti);
        free(zti_name);
        free(zts_name);
        free(mangled_type);
        if (aerr != GCC_GEN_BE_SUCCESS) return aerr;
        *out_descriptor = (gcc_jit_lvalue *)cached_zti;
        return GCC_GEN_BE_SUCCESS;
    }

    a_boolean is_anon_tp = FALSE;
    aerr = gcc_gen_be_is_scope_anonymous(tp->source_corresp.parent_scope, &is_anon_tp);
    if (aerr != GCC_GEN_BE_SUCCESS) {
        free(zti_name);
        free(zts_name);
        free(mangled_type);
        return aerr;
    }
    enum gcc_jit_global_kind rtti_linkage = is_anon_tp ? GCC_JIT_GLOBAL_INTERNAL : GCC_JIT_GLOBAL_EXPORTED;

    /* Emit or reuse _ZTS<mangled> read-only character array */
    gcc_jit_lvalue *zts_global = NULL;
    void *cached_zts = NULL;
    aerr = cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)zts_name, &cached_zts);
    if (aerr != GCC_GEN_BE_SUCCESS) {
        free(zti_name);
        free(zts_name);
        free(mangled_type);
        return aerr;
    }
    if (cached_zts) {
        void *zts_var_lval = NULL;
        aerr = cache_lookup(GCC_GEN_BE_CACHE_VAR, cached_zts, &zts_var_lval);
        if (aerr != GCC_GEN_BE_SUCCESS) {
            free(zti_name);
            free(zts_name);
            free(mangled_type);
            return aerr;
        }
        if (zts_var_lval) {
            cached_zts = zts_var_lval;
        }
        zts_global = (gcc_jit_lvalue *)cached_zts;
    } else {
        gcc_jit_type *char_type = gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_CHAR);
        const char *zts_payload = (strncmp(zts_name, "_ZTS", 4) == 0 && zts_name[4] != '\0')
            ? (zts_name + 4)
            : mangled_type;
        size_t name_len = strlen(zts_payload) + 1;
        gcc_jit_type *zts_arr_type = gcc_jit_context_new_array_type(ctx, NULL, char_type, (int)name_len);
        zts_global = gcc_jit_context_new_global(ctx, NULL, rtti_linkage, zts_arr_type, zts_name);
        if (!is_anon_tp) {
            aerr = gcc_gen_be_mark_global_vague_linkage(ctx, zts_global, zts_name);
            if (aerr != GCC_GEN_BE_SUCCESS) {
                free(zti_name);
                free(zts_name);
                free(mangled_type);
                return aerr;
            }
        }
        aerr = cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)zts_name, zts_global);
        if (aerr != GCC_GEN_BE_SUCCESS) {
            free(zti_name);
            free(zts_name);
            free(mangled_type);
            return aerr;
        }
        gcc_jit_rvalue **char_vals = NULL;
        aerr = gcc_gen_be_alloc(name_len * sizeof(gcc_jit_rvalue *), (void **)&char_vals);
        if (aerr != GCC_GEN_BE_SUCCESS) {
            free(zti_name);
            free(zts_name);
            free(mangled_type);
            return aerr;
        }
        for (size_t i = 0; i < name_len; ++i) {
            char_vals[i] = gcc_jit_context_new_rvalue_from_int(ctx, char_type, (int)(unsigned char)zts_payload[i]);
        }
        gcc_jit_global_set_initializer_rvalue(
            zts_global, gcc_jit_context_new_array_constructor(ctx, NULL, zts_arr_type, name_len, char_vals));
        free(char_vals);
    }
    free(zts_name);
    free(mangled_type);

    /* Determine Itanium ABI __cxxabiv1::*type_info vtable symbol */
    const char *tv_sym = "_ZTVN10__cxxabiv123__fundamental_type_infoE";
    a_type_ptr single_base_tp = NULL;
    a_boolean is_vmi_class = FALSE;
    a_boolean is_ptr_type = FALSE;
    a_boolean is_ptm_type = FALSE;
    int vmi_base_count = 0;
    switch (tp->kind) {
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
            if (tp->variant.integer.enum_type) {
                tv_sym = "_ZTVN10__cxxabiv116__enum_type_infoE";
            }
            break;
        case tk_class:
        case tk_struct:
        case tk_union:
            {
                a_class_type_supplement_ptr ctsp = tp->variant.class_struct_union.extra_info;
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

    gcc_jit_type *void_ptr_type = gcc_jit_type_get_pointer(gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_VOID));
    gcc_jit_type *tv_arr_type = gcc_jit_context_new_array_type(ctx, NULL, void_ptr_type, 8);
    void *cached_tv = NULL;
    gcc_jit_lvalue *tv_global = NULL;
    aerr = cache_lookup(GCC_GEN_BE_CACHE_VAR, (void *)tv_sym, &cached_tv);
    if (aerr != GCC_GEN_BE_SUCCESS) {
        free(zti_name);
        return aerr;
    }
    if (cached_tv) {
        tv_global = (gcc_jit_lvalue *)cached_tv;
    } else {
        void *existing_tv_var = NULL;
        aerr = cache_lookup(GCC_GEN_BE_CACHE_STRING, (void *)tv_sym, &existing_tv_var);
        if (aerr != GCC_GEN_BE_SUCCESS) {
            free(zti_name);
            return aerr;
        }
        if (existing_tv_var) {
            void *existing_tv_lval = NULL;
            aerr = cache_lookup(GCC_GEN_BE_CACHE_VAR, existing_tv_var, &existing_tv_lval);
            if (aerr != GCC_GEN_BE_SUCCESS) {
                free(zti_name);
                return aerr;
            }
            if (existing_tv_lval) {
                tv_global = (gcc_jit_lvalue *)existing_tv_lval;
            }
        }
        if (!tv_global) {
            tv_global = gcc_jit_context_new_global(ctx, NULL, GCC_JIT_GLOBAL_IMPORTED, tv_arr_type, tv_sym);
        }
        aerr = cache_insert(GCC_GEN_BE_CACHE_VAR, (void *)tv_sym, tv_global);
        if (aerr != GCC_GEN_BE_SUCCESS) {
            free(zti_name);
            return aerr;
        }
    }
    gcc_jit_rvalue *two_idx = gcc_jit_context_new_rvalue_from_int(ctx, gcc_jit_context_get_type(ctx, GCC_JIT_TYPE_INT), 2);
    gcc_jit_rvalue *tv_addr_pt = gcc_jit_context_new_cast(
        ctx, NULL,
        gcc_jit_lvalue_get_address(gcc_jit_context_new_array_access(ctx, NULL, gcc_jit_lvalue_as_rvalue(tv_global), two_idx), NULL),
        void_ptr_type);
    gcc_jit_rvalue *zts_ptr = gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(zts_global, NULL), void_ptr_type);

#if UINTPTR_MAX > 0xffffffffUL
    a_boolean is_lp64_vmi = !gcc_be_m32;
#else
    a_boolean is_lp64_vmi = FALSE;
#endif
    int num_zti_fields = 2;
    if (single_base_tp) {
        num_zti_fields = 3;
    } else if (is_ptr_type) {
        num_zti_fields = 4; /* vptr, name, __flags, __pointee */
    } else if (is_ptm_type) {
        num_zti_fields = 5; /* vptr, name, __flags, __pointee, __context */
    } else if (is_vmi_class) {
        num_zti_fields = (is_lp64_vmi ? 3 : 4) + (vmi_base_count * 2);
    }

    gcc_jit_type *zti_arr_type = gcc_jit_context_new_array_type(ctx, NULL, void_ptr_type, num_zti_fields);
    gcc_jit_lvalue *zti_global = gcc_jit_context_new_global(ctx, NULL, rtti_linkage, zti_arr_type, zti_name);
    if (!is_anon_tp) {
        aerr = gcc_gen_be_mark_global_vague_linkage(ctx, zti_global, zti_name);
        if (aerr != GCC_GEN_BE_SUCCESS) {
            free(zti_name);
            return aerr;
        }
    }
    aerr = cache_insert(GCC_GEN_BE_CACHE_GUARD, tp, zti_global);
    if (aerr == GCC_GEN_BE_SUCCESS) {
        aerr = cache_insert(GCC_GEN_BE_CACHE_STRING, (void *)zti_name, zti_global);
    }
    free(zti_name);
    if (aerr != GCC_GEN_BE_SUCCESS) return aerr;

    gcc_jit_rvalue **zti_elems = NULL;
    GCC_GEN_BE_CHECK(gcc_gen_be_alloc((size_t)num_zti_fields * sizeof(gcc_jit_rvalue *), (void **)&zti_elems));
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
        a_type_ptr pt = tp->variant.pointer.type;
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
        if (tp->variant.pointer.type) {
            gcc_gen_be_error_t perr = gcc_gen_be_emit_type_descriptor(tp->variant.pointer.type, &pointee_desc);
            if (perr != GCC_GEN_BE_SUCCESS) { free(zti_elems); return perr; }
        }
        zti_elems[3] = pointee_desc
            ? gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(pointee_desc, NULL), void_ptr_type)
            : gcc_jit_context_null(ctx, void_ptr_type);
    } else if (is_ptm_type) {
        zti_elems[2] = gcc_jit_context_null(ctx, void_ptr_type);
        gcc_jit_lvalue *member_desc = NULL;
        if (tp->variant.ptr_to_member.type) {
            gcc_gen_be_error_t merr = gcc_gen_be_emit_type_descriptor(tp->variant.ptr_to_member.type, &member_desc);
            if (merr != GCC_GEN_BE_SUCCESS) { free(zti_elems); return merr; }
        }
        zti_elems[3] = member_desc
            ? gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(member_desc, NULL), void_ptr_type)
            : gcc_jit_context_null(ctx, void_ptr_type);
        gcc_jit_lvalue *ctx_desc = NULL;
        if (tp->variant.ptr_to_member.class_of_which_a_member) {
            gcc_gen_be_error_t cerr = gcc_gen_be_emit_type_descriptor(tp->variant.ptr_to_member.class_of_which_a_member, &ctx_desc);
            if (cerr != GCC_GEN_BE_SUCCESS) { free(zti_elems); return cerr; }
        }
        zti_elems[4] = ctx_desc
            ? gcc_jit_context_new_cast(ctx, NULL, gcc_jit_lvalue_get_address(ctx_desc, NULL), void_ptr_type)
            : gcc_jit_context_null(ctx, void_ptr_type);
    } else if (is_vmi_class) {
        a_class_type_supplement_ptr ctsp = tp->variant.class_struct_union.extra_info;
        long vmi_flags = 0L;
        aerr = compute_vmi_type_info_flags(tp, &vmi_flags);
        if (aerr != GCC_GEN_BE_SUCCESS) { free(zti_elems); return aerr; }
        int f_idx = 3;
        if (is_lp64_vmi) {
            unsigned long long packed_hdr =
                (((unsigned long long)(unsigned int)vmi_base_count) << 32) |
                (unsigned long long)(unsigned int)vmi_flags;
            zti_elems[2] = gcc_jit_context_new_rvalue_from_ptr(ctx, void_ptr_type, (void *)(uintptr_t)packed_hdr);
            f_idx = 3;
        } else {
            zti_elems[2] = (vmi_flags != 0L)
                ? gcc_jit_context_new_rvalue_from_ptr(ctx, void_ptr_type, (void *)(uintptr_t)vmi_flags)
                : gcc_jit_context_null(ctx, void_ptr_type);
            zti_elems[3] = gcc_jit_context_new_rvalue_from_ptr(ctx, void_ptr_type, (void *)(uintptr_t)vmi_base_count);
            f_idx = 4;
        }
        for (a_base_class_ptr vb = ctsp->direct_base_classes; vb != NULL; vb = vb->next_direct) {
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
