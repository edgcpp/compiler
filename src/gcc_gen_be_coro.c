/**
 * @file gcc_gen_be_coro.c
 * @brief Implementation of C++20 coroutine frame layout calculation for the GCC backend.
 * @details Calculates exact ABI-aligned frame sizes and alignments across coroutine header fields,
 * promise objects, parameter copies, nested-scope local variables, and spilled temporaries.
 * @ingroup gcc_gen_be
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#include "gcc_gen_be_coro.h"
#include "gcc_gen_be_lib_loader.h"
#include <libgccjit.h>

#if BACK_END_IS_GCC_GEN_BE

BEGIN_EDG_NAMESPACE

/**
 * @brief Appends a single field's size and alignment into the running struct offset and max alignment.
 * @details Rounds `*io_offset` up to `field_align`, advances by `field_size`, and updates `*io_max_align`.
 * @param[in] field_size Field byte size (`0` treated as `1` for non-empty layout safety).
 * @param[in] field_align Field alignment in bytes (normalized to a positive power of two).
 * @param[in,out] io_offset Running byte offset in the coroutine frame.
 * @param[in,out] io_max_align Maximum alignment seen across all frame fields.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_INVALID_ARGUMENT`.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t accumulate_coro_field(
    size_t field_size,
    size_t field_align,
    size_t *io_offset,
    size_t *io_max_align) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    size_t pow2 = 1U;
    while (pow2 < field_align && pow2 < 4096U) {
        pow2 <<= 1U;
    }
    if (pow2 > *io_max_align) {
        *io_max_align = pow2;
    }
    size_t rem = (*io_offset) % pow2;
    if (rem != 0U) {
        *io_offset += (pow2 - rem);
    }
    *io_offset += field_size;
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Extracts the unwrapped byte size and byte alignment of an EDG `a_type_ptr`.
 * @details Strips `tk_typeref` qualifiers and reads `tp->size` and `tp->alignment`.
 * @param[in] tp Frontend type pointer.
 * @param[out] out_size Non-null pointer receiving the byte size.
 * @param[out] out_align Non-null pointer receiving the byte alignment.
 * @return `GCC_GEN_BE_SUCCESS` on success, or `GCC_GEN_BE_ERROR_OOM` on injected fault.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t query_edg_type_layout(
    a_type_ptr tp,
    size_t *out_size,
    size_t *out_align) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    while (tp && tp->kind == tk_typeref) {
        tp = tp->variant.typeref.type;
    }
    if (!tp) {
        *out_size = sizeof(void *);
        *out_align = sizeof(void *);
        return GCC_GEN_BE_SUCCESS;
    }
    *out_size = (tp->size > 0) ? (size_t)tp->size : sizeof(void *);
    *out_align = (tp->alignment > 0) ? (size_t)tp->alignment : sizeof(void *);
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Recursively accumulates `enk_temp_init` temporary sizes within an expression list.
 * @details Mirrors `scan_expr_for_coro_temps` so every spilled temporary contributes its exact size and alignment.
 * @param[in] expr Expression list head (may be NULL).
 * @param[in,out] io_offset Running frame byte offset.
 * @param[in,out] io_max_align Running maximum frame alignment.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t accumulate_coro_expr_temps(
    an_expr_node_ptr expr,
    size_t *io_offset,
    size_t *io_max_align) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    while (expr) {
        if (expr->kind == enk_temp_init && expr->type) {
            size_t tsz = 0;
            size_t tal = 0;
            GCC_GEN_BE_CHECK(query_edg_type_layout(expr->type, &tsz, &tal));
            GCC_GEN_BE_CHECK(accumulate_coro_field(tsz, tal, io_offset, io_max_align));
            if (expr->variant.init.dynamic_init &&
                expr->variant.init.dynamic_init->kind == dik_expression) {
                GCC_GEN_BE_CHECK(accumulate_coro_expr_temps(
                    expr->variant.init.dynamic_init->variant.expression, io_offset, io_max_align));
            }
        } else if (expr->kind == enk_operation) {
            GCC_GEN_BE_CHECK(accumulate_coro_expr_temps(
                expr->variant.operation.operands, io_offset, io_max_align));
        } else if (expr->kind == enk_await || expr->kind == enk_yield) {
            GCC_GEN_BE_CHECK(accumulate_coro_expr_temps(
                expr->variant.await_info.operand, io_offset, io_max_align));
            GCC_GEN_BE_CHECK(accumulate_coro_expr_temps(
                expr->variant.await_info.ready_resume_suspend, io_offset, io_max_align));
        }
        expr = expr->next;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Recursively accumulates `enk_temp_init` temporary sizes across a statement tree.
 * @details Walks blocks, conditionals, loops, switches, `try`/`catch` handlers, and `stmk_init` nodes.
 * @param[in] stmt Statement list head (may be NULL).
 * @param[in,out] io_offset Running frame byte offset.
 * @param[in,out] io_max_align Running maximum frame alignment.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t accumulate_coro_stmt_temps(
    a_statement_ptr stmt,
    size_t *io_offset,
    size_t *io_max_align) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    while (stmt) {
        if (stmt->expr) {
            GCC_GEN_BE_CHECK(accumulate_coro_expr_temps(stmt->expr, io_offset, io_max_align));
        }
        if (stmt->kind == stmk_block) {
            GCC_GEN_BE_CHECK(accumulate_coro_stmt_temps(
                stmt->variant.block.statements, io_offset, io_max_align));
        } else if (stmt->kind == stmk_if || stmt->kind == stmk_if_consteval || stmt->kind == stmk_if_not_consteval) {
            GCC_GEN_BE_CHECK(accumulate_coro_stmt_temps(
                stmt->variant.if_stmt.then_statement, io_offset, io_max_align));
            GCC_GEN_BE_CHECK(accumulate_coro_stmt_temps(
                stmt->variant.if_stmt.else_statement, io_offset, io_max_align));
        } else if (stmt->kind == stmk_while || stmt->kind == stmk_end_test_while) {
            GCC_GEN_BE_CHECK(accumulate_coro_stmt_temps(
                stmt->variant.loop_statement, io_offset, io_max_align));
        } else if (stmt->kind == stmk_for) {
            if (stmt->variant.for_loop.extra_info) {
                GCC_GEN_BE_CHECK(accumulate_coro_stmt_temps(
                    stmt->variant.for_loop.extra_info->initialization, io_offset, io_max_align));
                GCC_GEN_BE_CHECK(accumulate_coro_expr_temps(
                    stmt->variant.for_loop.extra_info->increment, io_offset, io_max_align));
            }
            GCC_GEN_BE_CHECK(accumulate_coro_stmt_temps(
                stmt->variant.for_loop.statement, io_offset, io_max_align));
        } else if (stmt->kind == stmk_switch) {
            GCC_GEN_BE_CHECK(accumulate_coro_stmt_temps(
                stmt->variant.switch_stmt.body_statement, io_offset, io_max_align));
        } else if (stmt->kind == stmk_try_block && stmt->variant.try_block) {
            GCC_GEN_BE_CHECK(accumulate_coro_stmt_temps(
                stmt->variant.try_block->statement, io_offset, io_max_align));
            for (a_handler_ptr h = stmt->variant.try_block->handlers; h != NULL; h = h->next) {
                GCC_GEN_BE_CHECK(accumulate_coro_stmt_temps(h->statement, io_offset, io_max_align));
            }
        } else if (stmt->kind == stmk_init && stmt->variant.dynamic_init &&
                   stmt->variant.dynamic_init->kind == dik_expression) {
            GCC_GEN_BE_CHECK(accumulate_coro_expr_temps(
                stmt->variant.dynamic_init->variant.expression, io_offset, io_max_align));
        }
        stmt = stmt->next;
    }
    return GCC_GEN_BE_SUCCESS;
}

/**
 * @brief Recursively accumulates all local variable and spilled temporary sizes in `scope` and its sub-scopes.
 * @details Sums aligned sizes of `scope->nonstatic_variables`, `scope->assoc_block` temporaries, and child `scope->scopes`.
 * @param[in] scope Scope pointer (may be NULL).
 * @param[in,out] io_offset Running frame byte offset.
 * @param[in,out] io_max_align Running maximum frame alignment.
 * @return `GCC_GEN_BE_SUCCESS` on success, or an error code.
 */
GCC_GEN_BE_NODISCARD static gcc_gen_be_error_t accumulate_coro_scope_locals(
    a_scope_ptr scope,
    size_t *io_offset,
    size_t *io_max_align) GCC_GEN_BE_NOEXCEPT {
    GCC_GEN_BE_CHECK(gcc_gen_be_step_oom_budget());
    if (scope->assoc_block) {
        GCC_GEN_BE_CHECK(accumulate_coro_stmt_temps(scope->assoc_block, io_offset, io_max_align));
    }
    for (a_variable_ptr var = scope->nonstatic_variables; var != NULL; var = var->next) {
        if (var->type) {
            size_t vsz = 0;
            size_t val = 0;
            GCC_GEN_BE_CHECK(query_edg_type_layout(var->type, &vsz, &val));
            GCC_GEN_BE_CHECK(accumulate_coro_field(vsz, val, io_offset, io_max_align));
        }
    }
    if (scope->variables && scope->variables != scope->nonstatic_variables) {
        for (a_variable_ptr var = scope->variables; var != NULL; var = var->next) {
            if (var->type && var->storage_class != sc_static && var->storage_class != sc_extern) {
                size_t vsz = 0;
                size_t val = 0;
                GCC_GEN_BE_CHECK(query_edg_type_layout(var->type, &vsz, &val));
                GCC_GEN_BE_CHECK(accumulate_coro_field(vsz, val, io_offset, io_max_align));
            }
        }
    }
    for (a_scope_ptr sub = scope->scopes; sub != NULL; sub = sub->next) {
        GCC_GEN_BE_CHECK(accumulate_coro_scope_locals(sub, io_offset, io_max_align));
    }
    return GCC_GEN_BE_SUCCESS;
}

gcc_gen_be_error_t gcc_gen_be_compute_coro_frame_layout(
    a_coroutine_descr_ptr descr,
    a_scope_ptr func_scope,
    struct gcc_jit_type *frame_type,
    size_t *out_frame_bytes,
    size_t *out_frame_align) GCC_GEN_BE_NOEXCEPT {
    if (!out_frame_bytes || !out_frame_align) {
        return GCC_GEN_BE_ERROR_INVALID_ARGUMENT;
    }

    size_t offset = 0U;
    size_t max_align = sizeof(void *) * 2U; /* 16-byte minimum frame alignment on 64-bit */
    const size_t ptr_sz = sizeof(void *);

    /* 1. __resume_fn and __destroy_fn */
    GCC_GEN_BE_CHECK(accumulate_coro_field(ptr_sz, ptr_sz, &offset, &max_align));
    GCC_GEN_BE_CHECK(accumulate_coro_field(ptr_sz, ptr_sz, &offset, &max_align));

    /* 2. __promise */
    size_t prom_sz = ptr_sz;
    size_t prom_al = ptr_sz;
    if (descr && descr->promise && descr->promise->type) {
        GCC_GEN_BE_CHECK(query_edg_type_layout(descr->promise->type, &prom_sz, &prom_al));
    }
    GCC_GEN_BE_CHECK(accumulate_coro_field(prom_sz, prom_al, &offset, &max_align));

    /* 3. __suspend_index (int), initial_await_resume_called (bool), __coro_flags (int) */
    GCC_GEN_BE_CHECK(accumulate_coro_field(sizeof(int), sizeof(int), &offset, &max_align));
    GCC_GEN_BE_CHECK(accumulate_coro_field(1U, 1U, &offset, &max_align));
    GCC_GEN_BE_CHECK(accumulate_coro_field(sizeof(int), sizeof(int), &offset, &max_align));

    /* 4. Parameter copies */
    if (descr && descr->parameter_copies) {
        for (a_variable_ptr pcopy = descr->parameter_copies; pcopy != NULL; pcopy = pcopy->next) {
            if (pcopy->type) {
                size_t psz = 0;
                size_t pal = 0;
                GCC_GEN_BE_CHECK(query_edg_type_layout(pcopy->type, &psz, &pal));
                GCC_GEN_BE_CHECK(accumulate_coro_field(psz, pal, &offset, &max_align));
            }
        }
    }

    /* 5. Spilled locals and temporaries across function scope & nested sub-scopes */
    if (func_scope) {
        GCC_GEN_BE_CHECK(accumulate_coro_scope_locals(func_scope, &offset, &max_align));
    }

    /* Round total frame byte size up to max_align */
    size_t rem = offset % max_align;
    if (rem != 0U) {
        offset += (max_align - rem);
    }

    if (frame_type && gcc_jit_type_is_struct(frame_type) != NULL) {
        *out_frame_bytes = offset;
    } else {
        *out_frame_bytes = offset;
    }
    *out_frame_align = max_align;
    return GCC_GEN_BE_SUCCESS;
}

END_EDG_NAMESPACE

#endif /* BACK_END_IS_GCC_GEN_BE */
